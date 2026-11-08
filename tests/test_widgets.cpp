// MIT License - see LICENSE
//
// Tests for Nt Widgets on the offscreen platform: the widget tree, painting, input dispatch, layouts and the
// basic controls. The tests play the window system through QWindowSystemInterface.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qstackedlayout.h>
#include <QtWidgets/qstackedwidget.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qsizepolicy.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qwidget.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

// A widget that records what it receives.
class Probe : public QWidget {
public:
    explicit Probe(QWidget *parent = nullptr) : QWidget(parent) {}

    QStringList log;
    QPoint lastPos;
    QPoint lastGlobal;
    int lastKey = 0;
    QString lastText;
    QPoint lastWheel;
    QSize lastSize;
    QSize lastOldSize;
    QPoint lastMove;
    int paints = 0;
    QRect lastPaintRect;
    QColor fill;
    bool acceptMouse = true;
    bool acceptKey = true;
    bool acceptClose = true;
    QSize hint{-1, -1};
    QSize minimumHint{-1, -1};

    QSize sizeHint() const override { return hint; }
    QSize minimumSizeHint() const override { return minimumHint; }

    void setHint(const QSize &s) {
        hint = s;
        updateGeometry();
    }

protected:
    void paintEvent(QPaintEvent *e) override {
        ++paints;
        lastPaintRect = e->rect();
        if (fill.isValid()) {
            QPainter p(this);
            p.fillRect(rect(), fill);
        }
    }
    void resizeEvent(QResizeEvent *e) override {
        log << "resize";
        lastSize = e->size();
        lastOldSize = e->oldSize();
    }
    void moveEvent(QMoveEvent *e) override {
        log << "move";
        lastMove = e->pos();
    }
    void showEvent(QShowEvent *) override { log << "show"; }
    void hideEvent(QHideEvent *) override { log << "hide"; }
    void closeEvent(QCloseEvent *e) override {
        log << "close";
        e->setAccepted(acceptClose);
    }
    void mousePressEvent(QMouseEvent *e) override {
        log << "press";
        lastPos = e->pos();
        lastGlobal = e->globalPos();
        e->setAccepted(acceptMouse);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        log << "release";
        lastPos = e->pos();
        e->setAccepted(acceptMouse);
    }
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        log << "dblclick";
        e->setAccepted(acceptMouse);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        log << "move?";
        lastPos = e->pos();
        e->setAccepted(acceptMouse);
    }
    void wheelEvent(QWheelEvent *e) override {
        log << "wheel";
        lastWheel = e->angleDelta();
        e->setAccepted(acceptMouse);
    }
    void keyPressEvent(QKeyEvent *e) override {
        log << "key";
        lastKey = e->key();
        lastText = e->text();
        e->setAccepted(acceptKey);
    }
    void keyReleaseEvent(QKeyEvent *e) override {
        log << "keyup";
        e->setAccepted(acceptKey);
    }
    void focusInEvent(QFocusEvent *) override { log << "focusIn"; }
    void focusOutEvent(QFocusEvent *) override { log << "focusOut"; }
    void enterEvent(QEnterEvent *e) override {
        log << "enter";
        lastPos = e->pos();
        QWidget::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        log << "leave";
        QWidget::leaveEvent(e);
    }
    void changeEvent(QEvent *e) override {
        switch (e->type()) {
        case QEvent::EnabledChange:
            log << "enabledChange";
            break;
        case QEvent::FontChange:
            log << "fontChange";
            break;
        case QEvent::PaletteChange:
            log << "paletteChange";
            break;
        case QEvent::StyleChange:
            log << "styleChange";
            break;
        case QEvent::ParentChange:
            log << "parentChange";
            break;
        case QEvent::WindowTitleChange:
            log << "titleChange";
            break;
        case QEvent::WindowActivate:
            log << "activate";
            break;
        case QEvent::WindowDeactivate:
            log << "deactivate";
            break;
        case QEvent::MouseTrackingChange:
            log << "trackingChange";
            break;
        default:
            break;
        }
        QWidget::changeEvent(e);
    }
};

// Tests that look at the pixels of text need a font; some machines (a minimal container) have none.
bool haveRealFonts() { return QFontMetrics(QFont("sans-serif", 10)).inFont(QChar('A')); }

// Delivers pending updates and layout requests.
void settle(QWidget *w) {
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::sendPostedEvents(w);
        if (w->windowHandle())
            QCoreApplication::sendPostedEvents(w->windowHandle());
    }
}

QImage grab(QWidget *w) {
    settle(w->window());
    return QOffscreenBackingStore::imageOf(w->window()->windowHandle());
}

void mouse(QWidget *w, QEvent::Type type, QPoint local, Qt::MouseButton button = Qt::LeftButton,
           Qt::MouseButtons buttons = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), QPointF(inWindow + w->window()->pos()), buttons,
                                             button, type, mods);
}

void click(QWidget *w, QPoint local) {
    mouse(w, QEvent::MouseButtonPress, local, Qt::LeftButton, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton);
}

void key(QWidget *w, int k, const QString &text = QString(), Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, k, mods, text);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, k, mods);
}

} // namespace

static void testSizePolicy() {
    QSizePolicy p;
    CHECK(p.horizontalPolicy() == QSizePolicy::Preferred && p.verticalPolicy() == QSizePolicy::Preferred);
    CHECK(p.horizontalStretch() == 0 && p.verticalStretch() == 0 && p.expandingDirections() == Qt::Orientations());
    QSizePolicy q(QSizePolicy::Expanding, QSizePolicy::Fixed);
    CHECK(q.expandingDirections() == Qt::Orientations(Qt::Horizontal));
    q.setVerticalPolicy(QSizePolicy::MinimumExpanding);
    q.setHorizontalPolicy(QSizePolicy::Minimum);
    CHECK(q.expandingDirections() == Qt::Orientations(Qt::Vertical));
    q.setHorizontalStretch(2);
    q.setVerticalStretch(3);
    CHECK(q.horizontalStretch() == 2 && q.verticalStretch() == 3);
    CHECK(q != p && QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum) ==
                        QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum));
    CHECK((QSizePolicy::Preferred & QSizePolicy::GrowFlag) && (QSizePolicy::Preferred & QSizePolicy::ShrinkFlag));
    CHECK(!(QSizePolicy::Fixed & QSizePolicy::GrowFlag) && (QSizePolicy::Ignored & QSizePolicy::IgnoreFlag));
    CHECK(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding).expandingDirections() ==
          (Qt::Horizontal | Qt::Vertical));
}

static void testPalette() {
    QPalette p;
    CHECK(p.color(QPalette::Window) == QColor(0xef, 0xef, 0xef) && p.window().color() == QColor(0xef, 0xef, 0xef));
    CHECK(p.color(QPalette::Active, QPalette::Base) == QColor(Qt::white) && p.base().style() == Qt::SolidPattern);
    CHECK(p.color(QPalette::Disabled, QPalette::Text) == QColor(0xbe, 0xbe, 0xbe));
    CHECK(p.color(QPalette::Inactive, QPalette::Text) == QColor(Qt::black) && p.text().color() == QColor(Qt::black));
    CHECK(p.color(QPalette::Active, QPalette::PlaceholderText).alpha() == 0x80);
    CHECK(p.currentColorGroup() == QPalette::Active);
    p.setCurrentColorGroup(QPalette::Disabled);
    CHECK(p.color(QPalette::Text) == QColor(0xbe, 0xbe, 0xbe) &&
          p.brush(QPalette::Current, QPalette::Text).color() == p.text().color());
    p.setCurrentColorGroup(QPalette::Active);
    p.setColor(QPalette::Window, QColor(10, 20, 30));
    CHECK(p.color(QPalette::Disabled, QPalette::Window) == QColor(10, 20, 30) &&
          p.window().color() == QColor(10, 20, 30));
    p.setColor(QPalette::Disabled, QPalette::Window, QColor(1, 2, 3));
    CHECK(p.color(QPalette::Disabled, QPalette::Window) == QColor(1, 2, 3) &&
          p.color(QPalette::Active, QPalette::Window) == QColor(10, 20, 30));
    p.setBrush(QPalette::Text, QBrush(Qt::red));
    CHECK(p.text().color() == QColor(Qt::red) && p.color(QPalette::Inactive, QPalette::Text) == QColor(Qt::red));
    p.setBrush(QPalette::Current, QPalette::Button, QBrush(QColor(5, 6, 7)));
    CHECK(p.button().color() == QColor(5, 6, 7));
    p.setBrush(QPalette::All, QPalette::Mid, QBrush(Qt::NoBrush)); // a brush without a style becomes solid
    CHECK(p.mid().style() == Qt::SolidPattern);
    p.setColor(QPalette::NColorRoles, Qt::red); // out of range is ignored
    CHECK(p.color(QPalette::NColorRoles) == p.color(QPalette::NoRole));
    CHECK(QPalette() != p && QPalette() == QPalette());
    const QPalette copy = p;
    CHECK(copy == p);
    CHECK(p.windowText().color() == QColor(Qt::black) && p.light().style() == Qt::SolidPattern &&
          p.midlight().color().alpha() == 255);
    CHECK(p.dark().color().isValid() && p.shadow().color().isValid() && p.brightText().color() == QColor(Qt::white));
    CHECK(p.buttonText().color() == QColor(Qt::black) && p.highlight().color() == QColor(0x30, 0x8c, 0xc6));
    CHECK(p.highlightedText().color() == QColor(Qt::white) && p.link().color() == QColor(Qt::blue));
    CHECK(p.linkVisited().color() == QColor(Qt::magenta) && p.alternateBase().color() == QColor(0xf7, 0xf7, 0xf7));
    CHECK(p.placeholderText().color().alpha() == 0x80);
    const QPalette themed(QColor(100, 100, 100), QColor(50, 50, 50));
    CHECK(themed.button().color() == QColor(100, 100, 100) && themed.window().color() == QColor(50, 50, 50));
    CHECK(themed.light().color().value() > themed.button().color().value() &&
          themed.dark().color().value() < themed.button().color().value());
    CHECK(QPalette(QColor(9, 9, 9)).window().color() == QColor(9, 9, 9));
    CHECK(QPalette::Foreground == QPalette::WindowText && QPalette::Background == QPalette::Window &&
          QPalette::Normal == QPalette::Active);
}

static void testApplication() {
    CHECK(QApplication::instance() != nullptr && qApp == QApplication::instance());
    CHECK(QApplication::style() != nullptr && QApplication::style() == QApplication::style());
    CHECK(QApplication::topLevelWidgets().isEmpty() && QApplication::allWidgets().isEmpty());
    CHECK(QApplication::activeWindow() == nullptr && QApplication::focusWidget() == nullptr);
    CHECK(QApplication::widgetAt(QPoint(5, 5)) == nullptr);
    CHECK(QApplication::palette() == QPalette());
    {
        QWidget a;
        QWidget b;
        Probe child(&a);
        CHECK(QApplication::topLevelWidgets().size() == 2 && QApplication::topLevelWidgets().contains(&a));
        CHECK(QApplication::allWidgets().size() == 3 && QApplication::allWidgets().contains(&child));
    }
    CHECK(QApplication::topLevelWidgets().isEmpty());

    Probe w;
    Probe sub(&w);
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(30, 30, 30));
    QApplication::setPalette(dark);
    CHECK(w.palette().window().color() == QColor(30, 30, 30) && sub.palette() == dark);
    CHECK(w.log.contains("paletteChange") && sub.log.contains("paletteChange"));
    QApplication::setPalette(QPalette());
    CHECK(w.palette() == QPalette());

    // A widget with its own palette is not changed by the application's
    QPalette own;
    own.setColor(QPalette::Window, Qt::red);
    w.setPalette(own);
    w.log.clear();
    sub.log.clear();
    QApplication::setPalette(dark);
    CHECK(w.palette() == own && !w.log.contains("paletteChange") && sub.palette() == own);
    QApplication::setPalette(QPalette());

    // The default palette follows the color scheme of the desktop, until the application sets one
    {
        Probe top;
        Probe inner(&top);
        QStyleHints *hints = QApplication::styleHints();
        const QColor lightWindow = QPalette().window().color();
        CHECK(hints->colorScheme() == Qt::ColorScheme::Unknown && QApplication::palette() == QPalette());
        top.log.clear();
        hints->setColorScheme(Qt::ColorScheme::Dark);
        const QPalette night = QApplication::palette();
        CHECK(night.window().color() != lightWindow && night.windowText().color().red() > 128);
        CHECK(night == QApplication::style()->standardPalette());
        CHECK(top.palette() == night && inner.palette() == night);
        CHECK(top.log.contains("paletteChange") && inner.log.contains("paletteChange"));
        hints->setColorScheme(Qt::ColorScheme::Light);
        CHECK(QApplication::palette() == QPalette() && top.palette() == QPalette());

        // A palette that the application sets stays, whatever the scheme does
        QApplication::setPalette(dark);
        hints->setColorScheme(Qt::ColorScheme::Dark);
        CHECK(QApplication::palette() == dark);
        QApplication::setPalette(QPalette()); // back to the standard palette
        CHECK(QApplication::palette() == night);
        hints->unsetColorScheme();
        CHECK(QApplication::palette() == QPalette());
    }

    // setStyle takes over a style and tells the widgets
    class CountingStyle : public QStyle {
    public:
        mutable int drawn = 0;
        int *alive;
        explicit CountingStyle(int *counter) : alive(counter) { ++*alive; }
        ~CountingStyle() override { --*alive; }
        void drawPrimitive(PrimitiveElement, const QStyleOption *, QPainter *, const QWidget *) const override {
            ++drawn;
        }
        void drawControl(ControlElement, const QStyleOption *, QPainter *, const QWidget *) const override { ++drawn; }
        QRect subElementRect(SubElement, const QStyleOption *o, const QWidget *) const override { return o->rect; }
        int pixelMetric(PixelMetric, const QStyleOption *, const QWidget *) const override { return 3; }
        QSize sizeFromContents(ContentsType, const QStyleOption *, const QSize &s, const QWidget *) const override {
            return s + QSize(100, 100);
        }
    };
    int alive = 0;
    QStyle *original = QApplication::style();
    (void)original;
    w.log.clear();
    auto *custom = new CountingStyle(&alive);
    QApplication::setStyle(custom);
    CHECK(QApplication::style() == custom && w.style() == custom && w.log.contains("styleChange") && alive == 1);
    QApplication::setStyle(custom); // the same style: nothing happens
    CHECK(QApplication::style() == custom && alive == 1);
    QPushButton b("x", &w);
    CHECK(b.sizeHint().width() >= 100 && static_cast<QStyle *>(custom)->pixelMetric(QStyle::PM_ButtonMargin) == 3);
    QApplication::setStyle(new CountingStyle(&alive)); // the first one is deleted
    CHECK(alive == 1);
    // A style that is set on a widget wins, and is not owned
    CountingStyle own_style(&alive);
    w.log.clear();
    w.setStyle(&own_style);
    CHECK(w.style() == &own_style && sub.style() != &own_style && w.log.contains("styleChange"));
    w.setStyle(nullptr);
    CHECK(w.style() == QApplication::style());
    QApplication::setStyle(nullptr); // back to the default style; the custom one is deleted
    CHECK(alive == 1 && QApplication::style() != nullptr);
}

static void testRender() {
    QWidget top;
    top.resize(120, 60);
    auto *layout = new QVBoxLayout(&top);
    QPushButton button("Press", &top);
    QCheckBox box("Check", &top);
    layout->addWidget(&button);
    layout->addWidget(&box);
    const QRgb window = top.palette().color(QPalette::Window).rgb();

    // A widget that has not been shown is painted with its layout done, over the background of the window
    QImage image(120, 60, QImage::Format_ARGB32_Premultiplied);
    image.fill(0u);
    top.render(&image);
    CHECK(image.pixel(1, 1) == window && image.pixel(119, 59) == window);
    // (the button has the color of the window in some palettes, so look for its frame and text anywhere in it)
    int renderedInk = 0;
    for (int y = button.geometry().top(); y <= button.geometry().bottom(); ++y)
        for (int x = button.geometry().left(); x <= button.geometry().right(); ++x)
            renderedInk += image.pixel(x, y) != window;
    CHECK(button.geometry().width() > 60 && renderedInk > 20);

    // The picture is the one the window shows
    top.show();
    const QImage shown = grab(&top);
    QImage again(top.size(), QImage::Format_ARGB32_Premultiplied);
    again.fill(0u);
    top.render(&again);
    int differing = 0;
    for (int y = 0; y < shown.height(); ++y)
        for (int x = 0; x < shown.width(); ++x)
            differing += shown.pixel(x, y) != again.pixel(x, y);
    CHECK(shown.size() == again.size());
    CHECK(differing == 0);

    // A target offset, and a device with twice the pixels
    QImage bigger(160, 100, QImage::Format_ARGB32_Premultiplied);
    bigger.fill(0u);
    top.render(&bigger, QPoint(20, 30));
    CHECK(bigger.pixel(10, 10) == 0u && bigger.pixel(21, 31) == window && bigger.pixel(139, 89) == window);
    CHECK(bigger.pixel(QPoint(20, 30) + button.geometry().center()) == image.pixel(button.geometry().center()));
    QImage sharp(240, 120, QImage::Format_ARGB32_Premultiplied);
    sharp.setDevicePixelRatio(2.0);
    sharp.fill(0u);
    top.render(&sharp);
    CHECK(sharp.pixel(3, 3) == window && sharp.pixel(237, 117) == window);
    // (the button has the color of the window, so look for its frame and text anywhere in it)
    int buttonInk = 0;
    for (int y = button.geometry().top() * 2; y < button.geometry().bottom() * 2; ++y)
        for (int x = button.geometry().left() * 2; x < button.geometry().right() * 2; ++x)
            buttonInk += sharp.pixel(x, y) != window;
    CHECK(buttonInk > 20);

    // A part of the widget, and a child that is hidden
    QImage part(120, 60, QImage::Format_ARGB32_Premultiplied);
    part.fill(0u);
    top.render(&part, QPoint(), QRegion(QRect(0, 0, 30, 60)));
    CHECK(part.pixel(10, 10) == window && part.pixel(60, 10) == 0u);
    QWidget plain;
    plain.resize(120, 60);
    QPushButton loose("Loose", &plain); // not in a layout, so it stays where it is put
    loose.setGeometry(70, 20, 40, 30);
    QImage withLoose(120, 60, QImage::Format_ARGB32_Premultiplied);
    withLoose.fill(0u);
    plain.render(&withLoose);
    auto inkIn = [&](const QImage &img, const QRect &r) {
        int n = 0;
        for (int y = r.top(); y <= r.bottom(); ++y)
            for (int x = r.left(); x <= r.right(); ++x)
                n += img.pixel(x, y) != window;
        return n;
    };
    CHECK(inkIn(withLoose, loose.geometry()) > 20);
    loose.hide();
    QImage hidden(120, 60, QImage::Format_ARGB32_Premultiplied);
    hidden.fill(0u);
    plain.render(&hidden);
    CHECK(inkIn(hidden, loose.geometry()) == 0);
    box.hide();
    box.show();

    // A child can be rendered on its own
    box.show();
    QImage own(box.width(), box.height(), QImage::Format_ARGB32_Premultiplied);
    own.fill(0u);
    box.render(&own);
    int inked = 0;
    for (int y = 0; y < own.height(); ++y)
        for (int x = 0; x < own.width(); ++x)
            inked += own.pixel(x, y) != 0u;
    CHECK(inked > 50);
    QPainter busy(&own);
    top.render(&own); // a device with an active painter is refused
    busy.end();
}

static void testGridLayout() {
    auto g = [](QWidget *w) { return w->geometry(); };
    QWidget top;
    top.resize(200, 100);
    Probe a(&top), b(&top), c(&top), d(&top);
    a.setHint(QSize(30, 20));
    b.setHint(QSize(50, 20));
    c.setHint(QSize(30, 30));
    d.setHint(QSize(50, 30));
    for (Probe *p : {&a, &b, &c, &d})
        p->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *grid = new QGridLayout(&top);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);
    CHECK(top.layout() == grid && grid->rowCount() == 0 && grid->columnCount() == 0 && grid->count() == 0);
    grid->addWidget(&a, 0, 0);
    grid->addWidget(&b, 0, 1);
    grid->addWidget(&c, 1, 0);
    grid->addWidget(&d, 1, 1);
    CHECK(grid->rowCount() == 2 && grid->columnCount() == 2 && grid->count() == 4);
    CHECK(grid->sizeHint() == QSize(30 + 10 + 50, 20 + 10 + 30) && grid->minimumSize() == QSize(10, 20 + 10 + 30));
    CHECK(a.parentWidget() == &top && grid->indexOf(&c) == 2 && grid->itemAt(1)->widget() == &b);
    CHECK(grid->itemAtPosition(1, 0)->widget() == &c && grid->itemAtPosition(5, 5) == nullptr);
    int row = -1, column = -1, rowSpan = -1, columnSpan = -1;
    grid->getItemPosition(3, &row, &column, &rowSpan, &columnSpan);
    CHECK(row == 1 && column == 1 && rowSpan == 1 && columnSpan == 1);

    // The column with the stretch factor takes the width that is left; the empty row with one takes the height
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(2, 1);
    CHECK(grid->columnStretch(1) == 1 && grid->columnStretch(0) == 0 && grid->rowStretch(2) == 1 &&
          grid->rowCount() == 3);
    CHECK(grid->expandingDirections() == (Qt::Horizontal | Qt::Vertical));
    grid->activate();
    CHECK(g(&a) == QRect(0, 0, 30, 20) && g(&b) == QRect(40, 0, 160, 20));
    CHECK(g(&c) == QRect(0, 30, 30, 30) && g(&d) == QRect(40, 30, 160, 30));
    // The same, with more room: only the stretched column and row grow
    top.resize(300, 200);
    grid->activate();
    CHECK(g(&a).width() == 30 && g(&b) == QRect(40, 0, 260, 20) && g(&c).top() == 30);

    // A widget that covers two columns, and needs more than they have: both get some of what is missing
    QWidget top2;
    top2.resize(300, 100);
    Probe w1(&top2), w2(&top2), w3(&top2);
    w1.setHint(QSize(30, 20));
    w2.setHint(QSize(50, 20));
    w3.setHint(QSize(100, 20));
    for (Probe *p : {&w1, &w2, &w3})
        p->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *g2 = new QGridLayout(&top2);
    g2->setContentsMargins(0, 0, 0, 0);
    g2->setSpacing(10);
    g2->addWidget(&w1, 0, 0);
    g2->addWidget(&w2, 0, 1);
    g2->addWidget(&w3, 1, 0, 1, 2);
    CHECK(g2->sizeHint() == QSize(100, 50) && g2->itemAtPosition(1, 1)->widget() == &w3);
    g2->getItemPosition(2, &row, &column, &rowSpan, &columnSpan);
    CHECK(row == 1 && column == 0 && rowSpan == 1 && columnSpan == 2);
    top2.resize(100, 50);
    g2->activate();
    CHECK(g(&w1) == QRect(2, 0, 30, 20) && g(&w2) == QRect(47, 0, 50, 20) && g(&w3) == QRect(0, 30, 100, 20));

    // Fixed widgets in a bigger grid are centered in their cells; an alignment puts them at a side
    top2.resize(300, 100);
    g2->activate();
    const int extraX = g(&w2).left() - g(&w1).right() - 1;
    CHECK(extraX > 10 && g(&w3).width() == 100);
    QWidget top3;
    top3.resize(120, 60);
    Probe left(&top3), right(&top3), bottom(&top3);
    for (Probe *p : {&left, &right, &bottom}) {
        p->setHint(QSize(30, 10));
        p->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    auto *g3 = new QGridLayout(&top3);
    g3->setContentsMargins(0, 0, 0, 0);
    g3->setColumnStretch(0, 1);
    g3->setColumnStretch(1, 1);
    g3->setRowStretch(0, 1);
    g3->addWidget(&left, 0, 0, Qt::AlignLeft | Qt::AlignTop);
    g3->addWidget(&right, 0, 1, Qt::AlignRight | Qt::AlignBottom);
    g3->addWidget(&bottom, 1, 0, 1, 2, Qt::AlignHCenter);
    g3->setSpacing(0);
    g3->activate();
    CHECK(g(&left).topLeft() == QPoint(0, 0));
    CHECK(g(&right).right() == 119 && g(&right).bottom() == 49);
    CHECK(g(&bottom) == QRect(45, 50, 30, 10));

    // Empty rows and columns have no spacing, and the spacing may differ by direction
    QWidget top4;
    top4.resize(200, 100);
    Probe p1(&top4), p2(&top4);
    p1.setHint(QSize(20, 10));
    p2.setHint(QSize(20, 10));
    auto *g4 = new QGridLayout(&top4);
    g4->setContentsMargins(1, 2, 3, 4);
    g4->setHorizontalSpacing(7);
    g4->setVerticalSpacing(5);
    CHECK(g4->horizontalSpacing() == 7 && g4->verticalSpacing() == 5);
    g4->addWidget(&p1, 0, 0);
    g4->addWidget(&p2, 2, 2);
    CHECK(g4->rowCount() == 3 && g4->columnCount() == 3);
    CHECK(g4->sizeHint() == QSize(1 + 20 + 7 + 20 + 3, 2 + 10 + 5 + 10 + 4));
    g4->setColumnMinimumWidth(1, 30);
    CHECK(g4->columnMinimumWidth(1) == 30 && g4->rowMinimumHeight(0) == 0);
    CHECK(g4->sizeHint().width() == 1 + 20 + 7 + 30 + 7 + 20 + 3);
    g4->setRowMinimumHeight(1, 12);
    CHECK(g4->sizeHint().height() == 2 + 10 + 5 + 12 + 5 + 10 + 4);

    // Items go where they are put: addItem() fills the next cell, a layout goes in a cell, a spacer takes room
    QWidget top5;
    top5.resize(100, 100);
    auto *q1p = new Probe(&top5);
    Probe &q1 = *q1p;
    Probe q2(&top5);
    auto *g5 = new QGridLayout(&top5);
    g5->addWidget(&q1, 0, 0);
    g5->addItem(new QSpacerItem(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed), 0, 1);
    auto *inner = new QHBoxLayout;
    inner->addWidget(&q2);
    g5->addLayout(inner, 1, 0, 1, 2);
    CHECK(inner->parent() == g5 && q2.parentWidget() == &top5 && g5->itemAtPosition(1, 1) == inner);
    g5->addItem(new QSpacerItem(1, 1));
    CHECK(g5->count() == 4);
    int r5, c5, rs5, cs5;
    g5->getItemPosition(3, &r5, &c5, &rs5, &cs5);
    CHECK(r5 == 2 && c5 == 0); // the next free cell: the rows so far have two columns
    g5->getItemPosition(99, &r5, &c5, &rs5, &cs5);
    CHECK(r5 == 2);

    // A widget that is deleted, or removed, leaves its cell
    delete q1p;
    CHECK(g5->itemAtPosition(0, 0) == nullptr);
    g5->removeWidget(&q2);
    CHECK(inner->count() == 0);
    delete g5->takeAt(0);

    // The layout works from the event loop after a hint changed, and the widgets are hidden and shown
    a.setHint(QSize(40, 20));
    settle(&top);
    CHECK(g(&a).width() == 40 && g(&b).left() == 50);
    a.hide();
    settle(&top);
    CHECK(grid->sizeHint().width() == 50 + 10 + 0 + 0 || grid->sizeHint().width() >= 50);
    a.show();
}

static void testFormLayout() {
    auto g = [](QWidget *w) { return w->geometry(); };
    QWidget top;
    top.resize(300, 200);
    Probe l0(&top), f0(&top), l1(&top), f1(&top), button(&top);
    l0.setHint(QSize(40, 16));
    f0.setHint(QSize(100, 20));
    l1.setHint(QSize(70, 16));
    f1.setHint(QSize(50, 30));
    button.setHint(QSize(60, 24));
    f0.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    f1.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    auto *form = new QFormLayout(&top);
    CHECK(top.layout() == form && form->rowCount() == 0 && form->count() == 0);
    CHECK(form->fieldGrowthPolicy() == QFormLayout::AllNonFixedFieldsGrow &&
          form->rowWrapPolicy() == QFormLayout::DontWrapRows && form->labelAlignment() == Qt::AlignLeft &&
          form->formAlignment() == (Qt::AlignLeft | Qt::AlignTop));
    form->setContentsMargins(10, 10, 10, 10);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(4);
    CHECK(form->horizontalSpacing() == 10 && form->verticalSpacing() == 4);
    form->addRow(&l0, &f0);
    form->addRow(&l1, &f1);
    form->addRow(&button);
    CHECK(form->rowCount() == 3 && form->count() == 5);
    CHECK(form->itemAt(0, QFormLayout::LabelRole)->widget() == &l0 &&
          form->itemAt(0, QFormLayout::FieldRole)->widget() == &f0 &&
          form->itemAt(2, QFormLayout::SpanningRole)->widget() == &button &&
          form->itemAt(2, QFormLayout::FieldRole) == nullptr && form->itemAt(3, QFormLayout::LabelRole) == nullptr &&
          form->itemAt(-1, QFormLayout::LabelRole) == nullptr);
    CHECK(form->itemAt(0)->widget() == &l0 && form->itemAt(1)->widget() == &f0 &&
          form->itemAt(4)->widget() == &button && form->itemAt(5) == nullptr);
    int row = -1;
    QFormLayout::ItemRole role = QFormLayout::LabelRole;
    form->getWidgetPosition(&f1, &row, &role);
    CHECK(row == 1 && role == QFormLayout::FieldRole);
    form->getWidgetPosition(&button, &row, &role);
    CHECK(row == 2 && role == QFormLayout::SpanningRole);
    form->getWidgetPosition(&top, &row, &role);
    CHECK(row == -1);
    CHECK(form->labelForField(&f0) == &l0 && form->labelForField(&button) == nullptr &&
          form->labelForField(&top) == nullptr);

    // Labels in the first column, fields in the second, a row of one widget across
    CHECK(form->sizeHint() == QSize(20 + 70 + 10 + 100, 20 + 20 + 30 + 24 + 2 * 4));
    CHECK(form->minimumSize() == QSize(20 + 0 + 10 + 50, 20 + 20 + 30 + 24 + 2 * 4));
    form->activate();
    CHECK(g(&l0) == QRect(10, 10, 40, 20) && g(&f0) == QRect(90, 10, 200, 20));
    CHECK(g(&l1) == QRect(10, 34, 70, 27) && g(&f1) == QRect(90, 34, 50, 30));
    CHECK(g(&button) == QRect(10, 68, 280, 24));
    // The label of a field that is much taller keeps near the top of the row
    f1.setFixedHeight(100);
    form->activate();
    CHECK(g(&l1) == QRect(10, 34, 70, 27) && g(&f1) == QRect(90, 34, 50, 100) && g(&button).top() == 138);
    f1.setFixedHeight(30);
    f1.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Growth of the fields
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->activate();
    CHECK(g(&f0).width() == 200 && g(&f1).width() == 50);
    l1.setHint(QSize(70, 16));
    f1.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    form->activate();
    CHECK(g(&f1).width() == 50); // a field that does not expand stays at its hint
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->activate();
    CHECK(g(&f1).width() == 200);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->activate();
    CHECK(g(&f0).width() == 100 && g(&f1).width() == 50 && g(&button).width() == 280);
    // Where the form goes when it does not fill the room
    button.hide(); // a row of one widget takes the whole width
    form->setFormAlignment(Qt::AlignRight | Qt::AlignBottom);
    form->activate();
    CHECK(g(&f0).right() == 289 && g(&l0).left() == 10 + 100 + 0 && g(&f0).bottom() < 190);
    button.show();
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    f1.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Alignment of the labels
    form->setLabelAlignment(Qt::AlignRight);
    form->activate();
    CHECK(g(&l0) == QRect(40, 10, 40, 20) && g(&l1).left() == 10);
    form->setLabelAlignment(Qt::AlignHCenter);
    form->activate();
    CHECK(g(&l0).left() == 25);
    form->setLabelAlignment(Qt::AlignLeft);

    // Labels above the fields
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    CHECK(form->sizeHint() == QSize(20 + 100, 20 + (16 + 4 + 20) + (16 + 4 + 30) + 24 + 2 * 4));
    form->setLabelAlignment(Qt::AlignRight); // not for a label above its field
    form->activate();
    CHECK(g(&l0) == QRect(10, 10, 40, 16) && g(&f0) == QRect(10, 30, 280, 20));
    CHECK(g(&l1) == QRect(10, 54, 70, 16) && g(&f1) == QRect(10, 74, 50, 30) && g(&button) == QRect(10, 108, 280, 24));
    form->setLabelAlignment(Qt::AlignLeft);
    // Only the rows that do not fit are wrapped
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    CHECK(form->sizeHint().width() == 20 + 70 + 10 + 100 && form->minimumSize().width() < form->sizeHint().width());
    form->activate();
    CHECK(g(&f0).left() == 90 && g(&f1).left() == 90);
    top.resize(110, 200);
    form->activate();
    CHECK(g(&f0).left() == 90 && g(&f1).top() > g(&l1).top() && g(&f1).left() == 10);
    top.resize(300, 200);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);

    // Hidden widgets leave their row
    l1.hide();
    f1.hide();
    form->activate();
    CHECK(g(&button) == QRect(10, 34, 280, 24));
    CHECK(form->sizeHint().height() == 20 + 20 + 24 + 4);
    l1.show();
    f1.show();

    // Rows with the spare height: a field that expands vertically takes it
    top.resize(300, 300);
    form->activate();
    CHECK(g(&button).top() == 68);
    f0.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    form->activate();
    CHECK(form->expandingDirections() == (Qt::Horizontal | Qt::Vertical));
    CHECK(g(&f0).height() == 218 && g(&l1).top() == 232 && g(&button).bottom() == 289);
    // ...but not past its largest height: the rows after it do not move away
    f0.setFixedHeight(20);
    f0.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    form->activate();
    CHECK(g(&f0).height() == 20 && g(&l1).top() == 34 && g(&button).top() == 68);
    f0.setMinimumSize(0, 0);
    f0.setMaximumSize(QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX);
    f0.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    top.resize(300, 200);

    // Rows that are added: text for the label, layouts, insertion, removal
    form->addRow("Text:", new QWidget);
    QWidget *added = form->itemAt(3, QFormLayout::FieldRole)->widget();
    auto *label = dynamic_cast<QLabel *>(form->labelForField(added));
    CHECK(form->rowCount() == 4 && label && label->text() == "Text:" && label->parentWidget() == &top);
    auto *inner = new QHBoxLayout;
    form->addRow("Layout:", inner);
    CHECK(form->rowCount() == 5 && form->itemAt(4, QFormLayout::FieldRole) == inner && inner->parent() == form &&
          form->labelForField(inner) != nullptr);
    form->getLayoutPosition(inner, &row, &role);
    CHECK(row == 4 && role == QFormLayout::FieldRole);
    auto *whole = new QVBoxLayout;
    form->addRow(whole);
    CHECK(form->rowCount() == 6 && form->itemAt(5, QFormLayout::SpanningRole) == whole);
    auto *first = new Probe(&top);
    form->insertRow(0, "First", first);
    CHECK(form->rowCount() == 7 && form->itemAt(0, QFormLayout::FieldRole)->widget() == first &&
          form->itemAt(1, QFormLayout::FieldRole)->widget() == &f0);
    form->insertRow(99, new QLabel("Last", &top));
    CHECK(form->rowCount() == 8);
    form->insertRow(-5, new QLabel("Very first", &top));
    CHECK(form->rowCount() == 9 && form->itemAt(0, QFormLayout::SpanningRole) != nullptr);
    // removeRow deletes what is in the row
    QPointer<QWidget> guarded = first;
    form->removeRow(1);
    CHECK(form->rowCount() == 8 && guarded.isNull());
    form->removeRow(-1);
    form->removeRow(100);
    CHECK(form->rowCount() == 8);
    // setWidget fills a slot of a row; a slot that is taken is left alone
    const int rows = form->rowCount();
    form->setWidget(rows, QFormLayout::SpanningRole, new QLabel("Spanning", &top));
    CHECK(form->rowCount() == rows + 1);
    QLabel taken("Taken", &top);
    form->setWidget(1, QFormLayout::LabelRole, &taken); // the first row has a label
    CHECK(taken.parentWidget() == &top && form->indexOf(&taken) == -1);
    form->setWidget(rows + 50, QFormLayout::FieldRole, &taken);
    CHECK(form->rowCount() == rows + 1);
    form->setWidget(0, QFormLayout::FieldRole, nullptr);
    form->setLayout(0, QFormLayout::FieldRole, nullptr);

    // Taking an item out: a row with only one item left stays a row
    QLayoutItem *item = form->takeAt(2);
    CHECK(item != nullptr);
    delete item;
    CHECK(form->takeAt(-1) == nullptr && form->takeAt(1000) == nullptr);

    // A deleted widget leaves the form
    auto *doomed = new Probe(&top);
    form->addRow("Doomed", doomed);
    const int before = form->count();
    delete doomed;
    CHECK(form->count() == before - 1);

    // addItem() makes a row of its own
    const int rowsBefore = form->rowCount();
    form->addItem(new QSpacerItem(10, 10));
    CHECK(form->rowCount() == rowsBefore + 1);
    form->addItem(nullptr);
}

static void testStackedLayout() {
    std::vector<int> changes, removals;
    QWidget top;
    top.resize(200, 100);
    Probe a(&top), b(&top), c(&top);
    a.setHint(QSize(50, 30));
    b.setHint(QSize(80, 20));
    c.setHint(QSize(30, 60));
    auto *stack = new QStackedLayout(&top);
    CHECK(stack->currentIndex() == -1 && stack->currentWidget() == nullptr && stack->count() == 0);
    CHECK(stack->contentsMargins() == QMargins(0, 0, 0, 0));
    QObject::connect(stack, &QStackedLayout::currentChanged, [&](int i) { changes.push_back(i); });
    QObject::connect(stack, &QStackedLayout::widgetRemoved, [&](int i) { removals.push_back(i); });
    top.show();
    CHECK(stack->addWidget(&a) == 0 && stack->currentIndex() == 0 && changes == std::vector<int>{0});
    CHECK(stack->addWidget(&b) == 1 && stack->addWidget(&c) == 2);
    CHECK(stack->count() == 3 && stack->currentWidget() == &a && stack->widget(1) == &b &&
          stack->widget(3) == nullptr && stack->widget(-1) == nullptr && stack->indexOf(&c) == 2);
    CHECK(!a.isHidden() && b.isHidden() && c.isHidden());
    CHECK(stack->sizeHint() == QSize(80, 60)); // the largest of the pages, also the hidden ones
    CHECK(stack->expandingDirections() == (Qt::Horizontal | Qt::Vertical)); // as for any layout of Qt
    stack->activate();
    CHECK(a.geometry() == QRect(0, 0, 200, 100));

    stack->setCurrentIndex(1);
    CHECK(stack->currentIndex() == 1 && stack->currentWidget() == &b && a.isHidden() && !b.isHidden() &&
          changes == (std::vector<int>{0, 1}));
    CHECK(b.geometry() == QRect(0, 0, 200, 100));
    stack->setCurrentIndex(1);
    stack->setCurrentIndex(7);
    stack->setCurrentIndex(-2);
    CHECK(changes.size() == 2 && stack->currentIndex() == 1);
    stack->setCurrentWidget(&c);
    CHECK(stack->currentIndex() == 2 && !c.isHidden() && b.isHidden() && changes.back() == 2);
    stack->setCurrentWidget(&top); // not a page
    CHECK(stack->currentIndex() == 2);
    stack->setCurrentWidget(nullptr);
    CHECK(stack->currentIndex() == 2);

    // Margins
    stack->setContentsMargins(5, 6, 7, 8);
    stack->activate();
    CHECK(c.geometry() == QRect(5, 6, 188, 86) && stack->sizeHint() == QSize(80 + 12, 60 + 14));

    // Inserting before the current page keeps the current page
    Probe d(&top);
    CHECK(stack->insertWidget(0, &d) == 0 && stack->currentIndex() == 3 && stack->currentWidget() == &c &&
          d.isHidden());
    CHECK(stack->insertWidget(1, nullptr) == -1);
    // Removing: the current page is replaced by the next, or by the one before when it was last
    changes.clear();
    delete stack->takeAt(3);
    CHECK(removals == std::vector<int>{3} && stack->count() == 3 && stack->currentIndex() == 2 &&
          stack->currentWidget() == &b);
    CHECK(!b.isHidden() && changes == std::vector<int>{2});
    changes.clear();
    delete stack->takeAt(0);
    CHECK(stack->currentIndex() == 1 && stack->currentWidget() == &b && changes.empty());
    CHECK(stack->takeAt(9) == nullptr);
    // A deleted page leaves the stack
    auto *doomed = new Probe(&top);
    stack->addWidget(doomed);
    const int pages = stack->count();
    delete doomed;
    CHECK(stack->count() == pages - 1);
    // addItem() takes the widget of the item
    Probe e(&top);
    stack->addItem(new QWidgetItem(&e));
    CHECK(stack->indexOf(&e) == stack->count() - 1);
    stack->addItem(new QSpacerItem(1, 1)); // not a widget
    stack->addItem(nullptr);
    CHECK(stack->count() == pages);
    // The last page goes: no current page
    while (stack->count() > 0)
        delete stack->takeAt(0);
    CHECK(stack->currentIndex() == -1 && stack->currentWidget() == nullptr && changes.back() == -1);
    CHECK(stack->sizeHint() == QSize(12, 14));
}

static void testStackedWidget() {
    std::vector<int> changes, removals;
    QWidget top;
    top.resize(200, 120);
    auto *box = new QVBoxLayout(&top);
    QStackedWidget stack(&top);
    box->addWidget(&stack);
    auto *p1 = new Probe, *p2 = new Probe;
    p1->setHint(QSize(60, 40));
    p2->setHint(QSize(90, 20));
    QObject::connect(&stack, &QStackedWidget::currentChanged, [&](int i) { changes.push_back(i); });
    QObject::connect(&stack, &QStackedWidget::widgetRemoved, [&](int i) { removals.push_back(i); });
    CHECK(stack.count() == 0 && stack.currentIndex() == -1 && stack.currentWidget() == nullptr &&
          stack.frameShape() == QFrame::NoFrame);
    CHECK(stack.addWidget(p1) == 0 && stack.addWidget(p2) == 1 && stack.insertWidget(1, new Probe) == 1);
    CHECK(stack.count() == 3 && stack.widget(0) == p1 && stack.indexOf(p2) == 2 && stack.currentIndex() == 0 &&
          stack.currentWidget() == p1 && p1->parentWidget() == &stack && changes == std::vector<int>{0});
    top.show();
    settle(&top);
    CHECK(p1->geometry() == QRect(0, 0, stack.width(), stack.height()) && !p1->isHidden() && p2->isHidden());
    stack.setCurrentWidget(p2);
    CHECK(stack.currentIndex() == 2 && stack.currentWidget() == p2 && p1->isHidden() && changes.back() == 2);
    CHECK(p2->geometry() == QRect(0, 0, stack.width(), stack.height()));
    stack.setCurrentIndex(0);
    CHECK(changes.back() == 0);
    // removeWidget: not deleted, hidden, and still a child
    stack.removeWidget(p1);
    CHECK(stack.count() == 2 && stack.indexOf(p1) == -1 && p1->parentWidget() == &stack && p1->isHidden());
    CHECK(removals == std::vector<int>{0} && stack.currentIndex() == 0);
    stack.removeWidget(p1);
    stack.removeWidget(nullptr);
    CHECK(removals.size() == 1);
    delete p1;
}

static void testSizeHelpersAndOptions() {
    Probe w;
    QStyleOption option;
    CHECK(option.type == QStyleOption::SO_Default);
    w.resize(50, 40);
    w.show();
    option.initFrom(&w);
    CHECK(option.rect == QRect(0, 0, 50, 40) && (option.state & QStyle::State_Enabled) &&
          (option.state & QStyle::State_Active));
    CHECK(!(option.state & QStyle::State_MouseOver) && option.palette == w.palette() &&
          option.fontMetrics.height() == w.fontMetrics().height());
    w.setEnabled(false);
    option.initFrom(&w);
    CHECK(!(option.state & QStyle::State_Enabled));
    QStyleOptionButton button;
    CHECK(button.type == QStyleOption::SO_Button && button.features == QStyleOptionButton::ButtonFeatures() &&
          button.text.isEmpty());
    button.features |= QStyleOptionButton::Flat;
    CHECK(button.features & QStyleOptionButton::Flat);
    QStyleOptionFrame frame;
    CHECK(frame.type == QStyleOption::SO_Frame && frame.lineWidth == 1);

    QStyle *style = QApplication::style();
    CHECK(style->pixelMetric(QStyle::PM_LayoutLeftMargin) == 9 && style->pixelMetric(QStyle::PM_LayoutTopMargin) == 9);
    CHECK(style->pixelMetric(QStyle::PM_LayoutRightMargin) == 9 &&
          style->pixelMetric(QStyle::PM_LayoutBottomMargin) == 9);
    CHECK(style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) == 6 &&
          style->pixelMetric(QStyle::PM_LayoutVerticalSpacing) == 6);
    CHECK(style->pixelMetric(QStyle::PM_DefaultFrameWidth) == 1 && style->pixelMetric(QStyle::PM_IndicatorWidth) == 14);
    CHECK(style->pixelMetric(QStyle::PM_IndicatorHeight) == 14 &&
          style->pixelMetric(QStyle::PM_CheckBoxLabelSpacing) == 6);
    CHECK(style->pixelMetric(QStyle::PM_ButtonMargin) == 6);
    // A window has wider margins than a widget inside it, as in Qt
    {
        QWidget window;
        QWidget inside(&window);
        CHECK(style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, &window) == 11 &&
              style->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, &window) == 11);
        CHECK(style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, &inside) == 9);
    }
    // A button with text is at least 80 wide; one without has no minimum
    CHECK(style->sizeFromContents(QStyle::CT_PushButton, &option, QSize(10, 10)) == QSize(24, 18));
    QStyleOptionButton withText;
    withText.text = "Text";
    CHECK(style->sizeFromContents(QStyle::CT_PushButton, &withText, QSize(10, 10)) == QSize(80, 18));
    CHECK(style->sizeFromContents(QStyle::CT_PushButton, &withText, QSize(100, 18)) == QSize(114, 26));
    CHECK(style->sizeFromContents(QStyle::CT_CheckBox, &option, QSize(0, 10)).width() == 14);
    CHECK(style->sizeFromContents(QStyle::CT_CheckBox, &option, QSize(50, 20)) == QSize(74, 25));
    CHECK(style->sizeFromContents(QStyle::CT_LineEdit, &option, QSize(50, 20)) == QSize(58, 28));
    option.rect = QRect(0, 0, 100, 20);
    CHECK(style->subElementRect(QStyle::SE_CheckBoxIndicator, &option) == QRect(0, 3, 14, 14));
    CHECK(style->subElementRect(QStyle::SE_CheckBoxContents, &option) == QRect(20, 0, 80, 20));
    CHECK(style->subElementRect(QStyle::SE_PushButtonContents, &option) == QRect(6, 3, 88, 14));
}

static void testWidgetBasics() {
    Probe top;
    CHECK(top.isWindow() && top.parentWidget() == nullptr && top.window() == &top && top.windowHandle() == nullptr);
    CHECK(top.geometry() == QRect(0, 0, 640, 480) && top.rect() == QRect(0, 0, 640, 480));
    CHECK(!top.isVisible() && top.isHidden() && top.isEnabled() && !top.isActiveWindow() && !top.hasFocus());
    CHECK(top.sizePolicy() == QSizePolicy() && top.minimumSize() == QSize(0, 0) && top.maximumSize().width() > 100000);
    CHECK(top.focusPolicy() == Qt::NoFocus && !top.autoFillBackground() && !top.hasMouseTracking() &&
          !top.underMouse());
    CHECK(top.layout() == nullptr && !top.sizeHint().isValid() && !top.minimumSizeHint().isValid());
    CHECK(top.depth() == 32 && top.width() == 640 && top.height() == 480 && top.paintEngine() == nullptr);

    Probe child(&top);
    CHECK(!child.isWindow() && child.parentWidget() == &top && child.window() == &top);
    CHECK(child.geometry() == QRect(0, 0, 100, 30) && child.isHidden() == false);
    CHECK(!child.isVisible()); // the parent is not shown

    // Geometry and events
    child.setGeometry(10, 20, 150, 60);
    CHECK(child.geometry() == QRect(10, 20, 150, 60) && child.pos() == QPoint(10, 20) &&
          child.size() == QSize(150, 60));
    CHECK(child.x() == 10 && child.y() == 20 && child.width() == 150 && child.height() == 60);
    CHECK(child.log == QStringList({"move", "resize"}) && child.lastSize == QSize(150, 60) &&
          child.lastOldSize == QSize(100, 30));
    CHECK(child.lastMove == QPoint(10, 20));
    child.log.clear();
    child.setGeometry(10, 20, 150, 60); // no change
    child.move(QPoint(11, 20));
    CHECK(child.log == QStringList({"move"}));
    child.log.clear();
    child.resize(QSize(151, 61));
    child.resize(152, 61);
    child.move(12, 21);
    CHECK(child.geometry() == QRect(12, 21, 152, 61) && child.log.size() == 3);
    child.setGeometry(QRect(10, 10, -5, -5).normalized()); // normalized
    CHECK(child.width() >= 0);
    child.setGeometry(0, 0, 100, 30);

    // Size limits
    child.setMinimumSize(120, 50);
    CHECK(child.minimumSize() == QSize(120, 50) && child.size() == QSize(120, 50));
    child.resize(10, 10);
    CHECK(child.size() == QSize(120, 50));
    child.setMaximumSize(QSize(200, 80));
    CHECK(child.maximumSize() == QSize(200, 80));
    child.resize(500, 500);
    CHECK(child.size() == QSize(200, 80));
    child.setMinimumSize(QSize(0, 0));
    child.setMaximumSize(QSize(100, 40));
    CHECK(child.size() == QSize(100, 40));
    child.setFixedSize(33, 22);
    CHECK(child.size() == QSize(33, 22) && child.minimumSize() == child.maximumSize());
    child.setFixedWidth(44);
    child.setFixedHeight(55);
    CHECK(child.size() == QSize(44, 55));
    child.setMinimumSize(0, 0);
    child.setMaximumSize(QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX);
    child.setGeometry(10, 20, 100, 30);

    // Size hint and size policy
    child.setHint(QSize(70, 20));
    CHECK(child.sizeHint() == QSize(70, 20));
    child.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    CHECK(child.sizePolicy() == QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    child.setSizePolicy(child.sizePolicy());
    child.adjustSize();
    CHECK(child.size() == QSize(70, 20));
    child.minimumHint = QSize(90, 25);
    child.adjustSize();
    CHECK(child.size() == QSize(90, 25));
    Probe nohint(&top);
    nohint.resize(33, 33);
    nohint.adjustSize(); // no hint: nothing changes
    CHECK(nohint.size() == QSize(33, 33));

    // Coordinates
    Probe grand(&child);
    grand.move(5, 6);
    top.move(100, 200);
    CHECK(grand.mapToParent(QPoint(1, 1)) == QPoint(6, 7) && grand.mapFromParent(QPoint(6, 7)) == QPoint(1, 1));
    CHECK(grand.mapTo(&top, QPoint(1, 1)) == QPoint(16, 27) && grand.mapFrom(&top, QPoint(16, 27)) == QPoint(1, 1));
    CHECK(grand.mapToGlobal(QPoint(0, 0)) == QPoint(115, 226) && grand.mapFromGlobal(QPoint(115, 226)) == QPoint(0, 0));
    CHECK(top.mapToGlobal(QPoint(1, 1)) == QPoint(101, 201) && top.mapFromGlobal(QPoint(101, 201)) == QPoint(1, 1));
    CHECK(child.mapTo(&child, QPoint(3, 3)) == QPoint(3, 3));
    top.move(0, 0);

    // Children are found in the reverse order of stacking
    child.setGeometry(0, 0, 100, 100);
    grand.setGeometry(10, 10, 20, 20);
    nohint.setGeometry(10, 10, 20, 20);
    CHECK(top.childAt(QPoint(300, 300)) == nullptr && top.childAt(5, 5) == &child);
    CHECK(top.childAt(QPoint(15, 15)) == &nohint ||
          top.childAt(QPoint(15, 15)) == &grand); // both are hidden: not visible yet
    top.resize(200, 200);
    top.show();
    child.show();
    grand.show();
    nohint.show();
    CHECK(top.childAt(QPoint(15, 15)) == &nohint); // the later child is on top
    CHECK(child.childAt(QPoint(15, 15)) == &grand);
    CHECK(top.childAt(QPoint(50, 50)) == &child);
    nohint.lower();
    CHECK(top.childAt(QPoint(15, 15)) == &grand && top.childAt(QPoint(50, 50)) == &child);
    child.raise();
    nohint.raise();
    CHECK(top.childAt(QPoint(15, 15)) == &nohint);
    grand.hide();
    CHECK(child.childAt(QPoint(15, 15)) == nullptr);
    grand.show();
    CHECK(QApplication::widgetAt(QPoint(15, 15)) == &nohint && QApplication::widgetAt(QPoint(500, 500)) == nullptr);

    // Title
    QString title;
    QObject::connect(&top, &QWidget::windowTitleChanged, [&](const QString &t) { title = t; });
    top.log.clear();
    top.setWindowTitle("Hi");
    CHECK(top.windowTitle() == QString("Hi") && title == QString("Hi") && top.log.contains("titleChange"));
    CHECK(top.windowHandle()->title() == QString("Hi"));
    top.setWindowTitle("Hi");
    top.setWindowTitle("Yo");
    CHECK(top.windowHandle()->title() == QString("Yo"));

    // Properties through the meta-object
    CHECK(top.property("windowTitle").toString() == QString("Yo"));
    CHECK(top.setProperty("windowTitle", "Via property") && top.windowTitle() == QString("Via property"));
    CHECK(top.property("enabled").toBool() && top.property("visible").toBool());
    CHECK(top.property("geometry").toRect() == top.geometry() && top.property("width").toInt() == top.width());
    CHECK(top.property("x").toInt() == top.x() && top.property("y").toInt() == top.y() &&
          top.property("height").toInt() == top.height());
    CHECK(top.setProperty("autoFillBackground", true) && top.autoFillBackground());
    CHECK(top.setProperty("mouseTracking", true) && top.hasMouseTracking());
    CHECK(top.setProperty("enabled", false) && !top.isEnabled());
    CHECK(top.setProperty("visible", false) && !top.isVisible());
}

static void testVisibilityAndEnabled() {
    Probe top;
    Probe a(&top);
    Probe b(&top);
    Probe inner(&a);
    top.resize(100, 100);
    top.show();
    CHECK(top.isVisible() && top.windowHandle() != nullptr && top.windowHandle()->isVisible());
    CHECK(a.isVisible() && b.isVisible() &&
          inner.isVisible()); // children that exist when the window is shown are shown
    CHECK(top.log.contains("show") && a.log.contains("show") && inner.log.contains("show"));

    // A child that is created below a visible parent stays hidden until it is shown
    auto *late = new Probe(&top);
    CHECK(!late->isVisible() && late->isHidden());
    late->show();
    CHECK(late->isVisible() && !late->isHidden() && late->log == QStringList({"show"}));

    a.log.clear();
    inner.log.clear();
    a.hide();
    CHECK(a.isHidden() && !a.isVisible() && !inner.isVisible() &&
          !inner.isHidden()); // inner is hidden by its parent only
    CHECK(a.log == QStringList({"hide"}) && inner.log == QStringList({"hide"}));
    a.hide();
    CHECK(a.log.size() == 1);
    a.log.clear();
    inner.log.clear();
    a.show();
    CHECK(a.isVisible() && inner.isVisible() && a.log == QStringList({"show"}) && inner.log == QStringList({"show"}));
    a.setVisible(false);
    a.setVisible(true);
    CHECK(a.isVisible());

    // Enabled state is inherited
    a.log.clear();
    inner.log.clear();
    b.log.clear();
    a.setEnabled(false);
    CHECK(!a.isEnabled() && !inner.isEnabled() && b.isEnabled());
    CHECK(a.log == QStringList({"enabledChange"}) && inner.log == QStringList({"enabledChange"}) && b.log.isEmpty());
    a.setEnabled(false);
    CHECK(a.log.size() == 1);
    inner.setDisabled(false);
    CHECK(!inner.isEnabled()); // the parent is still disabled
    a.setEnabled(true);
    CHECK(inner.isEnabled());
    inner.setDisabled(true);
    CHECK(!inner.isEnabled() && a.isEnabled());
    inner.setEnabled(true);

    // Palette and font are inherited too
    QPalette p;
    p.setColor(QPalette::Window, QColor(1, 2, 3));
    a.log.clear();
    inner.log.clear();
    b.log.clear();
    a.setPalette(p);
    CHECK(a.palette() == p && inner.palette() == p && b.palette() != p);
    CHECK(a.log.contains("paletteChange") && inner.log.contains("paletteChange") && b.log.isEmpty());
    QFont f("Serif", 17);
    a.log.clear();
    inner.log.clear();
    a.setFont(f);
    CHECK(a.font() == f && inner.font() == f && b.font() != f && a.fontMetrics().height() > b.fontMetrics().height());
    CHECK(a.log.contains("fontChange") && inner.log.contains("fontChange"));
    QFont f2("Mono", 5);
    inner.setFont(f2);
    CHECK(inner.font() == f2 && a.font() == f);

    // Parent changes
    inner.log.clear();
    inner.setParent(&b);
    CHECK(inner.parentWidget() == &b && !inner.isVisible() && inner.log.contains("parentChange"));
    inner.setParent(&b); // same parent
    inner.show();
    CHECK(inner.isVisible());
    inner.setParent(nullptr);
    CHECK(inner.isWindow() && !inner.isVisible() && QApplication::topLevelWidgets().contains(&inner));
    inner.setParent(&a);
    CHECK(!inner.isWindow() && !QApplication::topLevelWidgets().contains(&inner));

    // Close and hide a window
    top.log.clear();
    top.hide();
    CHECK(!top.isVisible() && top.isHidden() && top.log.contains("hide") && !a.isVisible());
    top.show();
    CHECK(top.isVisible() && a.isVisible());
    top.acceptClose = false;
    top.log.clear();
    CHECK(!top.close() && top.isVisible() && top.log == QStringList({"close"}));
    top.acceptClose = true;
    CHECK(top.close() && !top.isVisible() && top.windowHandle()->handle() == nullptr);
    CHECK(top.close()); // closing a closed window is fine
    top.show();         // and it can be shown again
    CHECK(top.isVisible() && b.isVisible());
    b.log.clear();
    CHECK(b.close() && !b.isVisible()); // closing a child hides it
    top.setVisible(false);
}

static void testGeometryOfWindows() {
    Probe w;
    w.setGeometry(30, 40, 200, 150);
    w.show();
    CHECK(w.windowHandle()->geometry() == QRect(30, 40, 200, 150) && w.geometry() == QRect(30, 40, 200, 150));
    w.log.clear();
    w.resize(260, 180);
    CHECK(w.windowHandle()->size() == QSize(260, 180) && w.size() == QSize(260, 180) && w.log.contains("resize"));
    CHECK(w.lastSize == QSize(260, 180) && w.lastOldSize == QSize(200, 150));
    w.move(50, 60);
    CHECK(w.windowHandle()->position() == QPoint(50, 60) && w.pos() == QPoint(50, 60) && w.lastMove == QPoint(50, 60));
    // The window system can change the geometry
    w.log.clear();
    QWindowSystemInterface::handleGeometryChange(w.windowHandle(), QRect(70, 80, 300, 200));
    CHECK(w.geometry() == QRect(70, 80, 300, 200) && w.log == QStringList({"move", "resize"}) &&
          w.lastOldSize == QSize(260, 180));
    w.setMinimumSize(320, 220);
    CHECK(w.size() == QSize(320, 220));

    // A window that was never given a geometry takes its size hint
    Probe sized;
    sized.setHint(QSize(123, 77));
    sized.show();
    CHECK(sized.size() == QSize(123, 77) && sized.windowHandle()->size() == QSize(123, 77));
    Probe plain;
    plain.show();
    CHECK(plain.size() == QSize(640, 480));
    CHECK(plain.windowHandle()->isActive() && plain.isActiveWindow() && QApplication::activeWindow() == &plain);
    plain.log.clear();
    sized.show(); // already shown: sized is activated
    plain.activateWindow();
    sized.activateWindow();
}

static void testPainting() {
    Probe top;
    top.fill = QColor(10, 20, 30);
    top.resize(100, 80);
    Probe a(&top);
    a.fill = QColor(200, 0, 0);
    a.setGeometry(10, 10, 40, 30);
    Probe b(&top);
    b.fill = QColor(0, 200, 0);
    b.setGeometry(30, 20, 40, 30);
    Probe inner(&a);
    inner.fill = QColor(0, 0, 200);
    inner.setGeometry(5, 5, 100, 100); // larger than its parent: clipped by it
    top.show();
    CHECK(top.paints >= 1 && a.paints >= 1 && b.paints >= 1 && inner.paints >= 1);
    QImage img = grab(&top);
    CHECK(img.size() == QSize(100, 80));
    CHECK(img.pixel(2, 2) == 0xff0a141eu);   // the window
    CHECK(img.pixel(12, 12) == 0xffc80000u); // child a
    CHECK(img.pixel(20, 20) == 0xff0000c8u); // inner, in a
    CHECK(img.pixel(48, 38) == 0xff00c800u); // b is above a
    CHECK(img.pixel(55, 18) == 0xff0a141eu); // inner does not leave a: this is the window
    CHECK(img.pixel(45, 15) == 0xff0000c8u || img.pixel(45, 15) == 0xffc80000u);
    CHECK(img.pixel(75, 25) == 0xff0a141eu && img.pixel(30, 60) == 0xff0a141eu);

    // update() repaints only what is asked for, from the event loop
    const int topPaints = top.paints;
    a.paints = b.paints = inner.paints = 0;
    a.fill = QColor(100, 100, 0);
    a.update();
    CHECK(a.paints == 0);
    settle(&top);
    CHECK(a.paints == 1 && top.paints > topPaints); // the window is painted underneath, clipped to the region
    CHECK(inner.paints == 1);                       // children of the updated widget are painted too
    img = grab(&top);
    CHECK(img.pixel(12, 12) == 0xff646400u);
    // The region reaches the widget in its own coordinates
    a.update(QRect(2, 3, 4, 5));
    settle(&top);
    CHECK(a.lastPaintRect == QRect(2, 3, 4, 5));
    top.update(QRegion(0, 0, 5, 5));
    a.paints = 0;
    settle(&top);
    CHECK(a.paints == 0); // a is not in that part
    // repaint() is immediate
    a.paints = 0;
    a.repaint();
    CHECK(a.paints == 1);
    a.repaint(QRect(0, 0, 3, 3));
    CHECK(a.lastPaintRect == QRect(0, 0, 3, 3));

    // Hidden widgets are not painted or updated
    b.hide();
    settle(&top);
    img = grab(&top);
    CHECK(img.pixel(12, 12) == 0xff646400u && img.pixel(60, 45) == 0xff0a141eu); // b's area shows what is below it
    b.paints = 0;
    b.update();
    settle(&top);
    CHECK(b.paints == 0);
    b.show();
    settle(&top);
    img = grab(&top);
    CHECK(img.pixel(48, 38) == 0xff00c800u && b.paints > 0);

    // Moving a widget repaints the old and the new place
    b.move(60, 40);
    settle(&top);
    img = grab(&top);
    CHECK(img.pixel(48, 38) == 0xff0000c8u && img.pixel(70, 55) == 0xff00c800u && img.pixel(35, 45) == 0xff0a141eu);
    b.raise();
    a.raise();
    settle(&top);

    // autoFillBackground paints the Window color first, so a widget that paints nothing shows it
    Probe plain(&top);
    plain.setGeometry(0, 60, 30, 20);
    plain.show();
    settle(&top);
    img = grab(&top);
    CHECK(img.pixel(5, 65) == 0xff0a141eu);
    plain.setAutoFillBackground(true);
    settle(&top);
    img = grab(&top);
    CHECK(img.pixel(5, 65) == 0xffefefefu);
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(7, 8, 9));
    plain.setPalette(dark);
    settle(&top);
    img = grab(&top);
    CHECK(img.pixel(5, 65) == 0xff070809u);
    plain.setEnabled(false); // the disabled group of the palette
    settle(&top);
    img = grab(&top);
    CHECK(img.pixel(5, 65) == 0xff070809u);

    // A QPainter on a widget that is not shown fails
    Probe hidden;
    QPainter painter(&hidden);
    CHECK(!painter.isActive());
}

static void testMouse() {
    Probe top;
    top.resize(200, 150);
    Probe a(&top);
    a.setGeometry(20, 20, 100, 80);
    Probe inner(&a);
    inner.setGeometry(10, 10, 30, 30);
    Probe other(&top);
    other.setGeometry(130, 20, 50, 50);
    top.show();
    top.log.clear();
    a.log.clear();
    inner.log.clear();
    other.log.clear();

    // Press: delivered to the deepest widget with the position in its coordinates
    mouse(&top, QEvent::MouseButtonPress, QPoint(35, 35));
    CHECK(inner.log == QStringList({"enter", "press"}) && inner.lastPos == QPoint(5, 5));
    CHECK(a.log == QStringList({"enter"}) && top.log.contains("enter"));
    CHECK(inner.lastGlobal == QPoint(35, 35));
    CHECK(inner.underMouse() && a.underMouse() && !other.underMouse());
    // A move while the button is down goes to the widget that was pressed, also outside of it
    inner.log.clear();
    mouse(&top, QEvent::MouseMove, QPoint(150, 40), Qt::NoButton, Qt::LeftButton);
    CHECK(inner.log == QStringList({"move?"}) && inner.lastPos == QPoint(120, 10));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(150, 40), Qt::LeftButton, Qt::NoButton);
    CHECK(inner.log.contains("release") && inner.lastPos == QPoint(120, 10));
    // After the release the mouse is over the other widget
    CHECK(other.underMouse() && !inner.underMouse() && !a.underMouse());
    CHECK(inner.log.contains("leave") && a.log.contains("leave") && other.log.contains("enter"));
    other.log.clear();

    // An ignored event goes to the parent, with the position in its coordinates
    inner.acceptMouse = false;
    a.log.clear();
    mouse(&top, QEvent::MouseButtonPress, QPoint(35, 35));
    CHECK(a.log.contains("press") && a.lastPos == QPoint(15, 15));
    inner.acceptMouse = true;
    mouse(&top, QEvent::MouseButtonRelease, QPoint(35, 35), Qt::LeftButton, Qt::NoButton);
    a.acceptMouse = false;
    top.acceptMouse = false;
    top.log.clear();
    mouse(&top, QEvent::MouseButtonPress, QPoint(25, 25)); // a, which ignores it, and the window
    CHECK(top.log.contains("press") && top.lastPos == QPoint(25, 25));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(25, 25), Qt::LeftButton, Qt::NoButton);
    a.acceptMouse = true;
    top.acceptMouse = true;

    // Moves without a button need mouse tracking
    inner.log.clear();
    a.log.clear();
    mouse(&top, QEvent::MouseMove, QPoint(35, 35), Qt::NoButton, Qt::NoButton);
    CHECK(!inner.log.contains("move?") && inner.underMouse());
    inner.setMouseTracking(true);
    CHECK(inner.hasMouseTracking() && inner.log.contains("trackingChange"));
    mouse(&top, QEvent::MouseMove, QPoint(36, 37), Qt::NoButton, Qt::NoButton);
    CHECK(inner.log.contains("move?") && inner.lastPos == QPoint(6, 7));
    inner.setMouseTracking(false);

    // Moving between widgets sends leave and enter events
    other.log.clear();
    inner.log.clear();
    a.log.clear();
    mouse(&top, QEvent::MouseMove, QPoint(150, 40), Qt::NoButton, Qt::NoButton);
    CHECK(inner.log.contains("leave") && a.log.contains("leave") && other.log.contains("enter") && other.underMouse());
    CHECK(other.lastPos == QPoint(20, 20));
    mouse(&top, QEvent::MouseMove, QPoint(190, 140), Qt::NoButton, Qt::NoButton);
    CHECK(other.log.contains("leave") && !other.underMouse() && top.underMouse());
    QWindowSystemInterface::handleLeaveEvent(top.windowHandle());
    CHECK(!top.underMouse());
    QWindowSystemInterface::handleEnterEvent(top.windowHandle(), QPointF(35, 35), QPointF(35, 35));
    CHECK(inner.underMouse() && a.underMouse() && top.underMouse());
    QWindowSystemInterface::handleLeaveEvent(top.windowHandle());
    CHECK(!inner.underMouse());

    // Double clicks; a double click that is ignored becomes a press for the parent
    inner.log.clear();
    mouse(&top, QEvent::MouseButtonPress, QPoint(35, 35));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(35, 35), Qt::LeftButton, Qt::NoButton);
    mouse(&top, QEvent::MouseButtonDblClick, QPoint(35, 35));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(35, 35), Qt::LeftButton, Qt::NoButton);
    CHECK(inner.log.contains("dblclick"));
    class Plain : public QWidget {
    public:
        int presses = 0;
        using QWidget::QWidget;
        void mousePressEvent(QMouseEvent *e) override {
            ++presses;
            e->accept();
        }
    };
    Plain plain(&top);
    plain.setGeometry(0, 100, 20, 20);
    plain.show();
    mouse(&top, QEvent::MouseButtonDblClick, QPoint(5, 105));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(5, 105), Qt::LeftButton, Qt::NoButton);
    CHECK(plain.presses == 1); // QWidget::mouseDoubleClickEvent calls mousePressEvent

    // Wheel
    inner.log.clear();
    QWindowSystemInterface::handleWheelEvent(top.windowHandle(), QPointF(35, 35), QPointF(35, 35), QPoint(),
                                             QPoint(0, 120), Qt::NoModifier);
    CHECK(inner.log == QStringList({"wheel"}) && inner.lastWheel == QPoint(0, 120));
    inner.acceptMouse = false;
    a.log.clear();
    QWindowSystemInterface::handleWheelEvent(top.windowHandle(), QPointF(35, 35), QPointF(35, 35), QPoint(),
                                             QPoint(0, -120), Qt::NoModifier);
    CHECK(a.log.contains("wheel") && a.lastWheel == QPoint(0, -120));
    inner.acceptMouse = true;

    // Disabled widgets swallow mouse events
    inner.setEnabled(false);
    inner.log.clear();
    a.log.clear();
    mouse(&top, QEvent::MouseButtonPress, QPoint(35, 35));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(35, 35), Qt::LeftButton, Qt::NoButton);
    CHECK(!inner.log.contains("press") && !a.log.contains("press"));
    inner.setEnabled(true);

    // Click focus
    a.setFocusPolicy(Qt::ClickFocus);
    inner.setFocusPolicy(Qt::TabFocus);
    mouse(&top, QEvent::MouseButtonPress, QPoint(125, 85));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(125, 85), Qt::LeftButton, Qt::NoButton);
    CHECK(!a.hasFocus());
    mouse(&top, QEvent::MouseButtonPress, QPoint(25, 25));
    mouse(&top, QEvent::MouseButtonRelease, QPoint(25, 25), Qt::LeftButton, Qt::NoButton);
    CHECK(a.hasFocus() && top.focusWidget() == &a);
    mouse(&top, QEvent::MouseButtonPress, QPoint(35, 35)); // inner only takes focus by tabbing
    mouse(&top, QEvent::MouseButtonRelease, QPoint(35, 35), Qt::LeftButton, Qt::NoButton);
    CHECK(a.hasFocus());
    QWindowSystemInterface::handleLeaveEvent(top.windowHandle());
    QWindowSystemInterface::handleMouseEvent(top.windowHandle(), QPointF(500, 500), QPointF(500, 500), Qt::NoButton,
                                             Qt::NoButton, QEvent::MouseMove, Qt::NoModifier);
}

static void testKeysAndFocus() {
    Probe top;
    top.resize(200, 100);
    Probe a(&top), b(&top), c(&top), none(&top);
    a.setGeometry(0, 0, 50, 50);
    b.setGeometry(50, 0, 50, 50);
    c.setGeometry(100, 0, 50, 50);
    none.setGeometry(150, 0, 50, 50);
    a.setFocusPolicy(Qt::StrongFocus);
    b.setFocusPolicy(Qt::TabFocus);
    c.setFocusPolicy(Qt::ClickFocus);
    CHECK(a.focusPolicy() == Qt::StrongFocus && !a.hasFocus() && top.focusWidget() == nullptr);

    // The first widget that can be tabbed to gets the focus when the window is activated
    top.show();
    CHECK(a.hasFocus() && top.focusWidget() == &a && QApplication::focusWidget() == &a);
    CHECK(a.log.contains("focusIn") && top.log.contains("activate"));
    a.log.clear();

    // Keys go to the focus widget
    key(&top, Qt::Key_A, "a");
    CHECK(a.log == QStringList({"key", "keyup"}) && a.lastKey == Qt::Key_A && a.lastText == QString("a"));
    // An ignored key goes to the parent
    a.acceptKey = false;
    top.log.clear();
    key(&top, Qt::Key_B, "b");
    CHECK(top.log == QStringList({"key", "keyup"}) && top.lastKey == Qt::Key_B);
    a.acceptKey = true;

    // Tab and Shift+Tab move the focus in the order of creation, and wrap around; c only takes click focus
    a.log.clear();
    b.log.clear();
    key(&top, Qt::Key_Tab, "\t");
    CHECK(b.hasFocus() && !a.hasFocus() && a.log.contains("focusOut") && b.log.contains("focusIn"));
    CHECK(!a.log.contains("key") && !b.log.contains("key")); // the key was used to move the focus
    key(&top, Qt::Key_Tab);
    CHECK(a.hasFocus()); // wrapped: c and none are not in the tab chain
    key(&top, Qt::Key_Backtab);
    CHECK(b.hasFocus());
    key(&top, Qt::Key_Tab, QString(), Qt::ShiftModifier);
    CHECK(a.hasFocus());
    key(&top, Qt::Key_Tab, QString(), Qt::ControlModifier); // Ctrl+Tab is not navigation
    CHECK(a.hasFocus() && a.lastKey == Qt::Key_Tab);

    // Programmatic focus
    c.setFocus();
    CHECK(c.hasFocus() && !a.hasFocus() && c.log.contains("focusIn"));
    c.clearFocus();
    CHECK(!c.hasFocus() && top.focusWidget() == nullptr);
    key(&top, Qt::Key_Z, "z");
    CHECK(top.lastKey == Qt::Key_Z); // without a focus widget the window gets the keys
    none.setFocus();
    CHECK(none.hasFocus());
    none.clearFocus();
    a.clearFocus(); // not the focus widget: nothing happens
    CHECK(!a.hasFocus());
    CHECK(a.focusNextChild() && b.hasFocus() && b.focusPreviousChild() && a.hasFocus());

    // Disabled widgets are not in the chain; hidden ones neither
    b.setEnabled(false);
    a.focusNextChild();
    CHECK(a.hasFocus()); // nothing else to go to: the focus stays
    b.setEnabled(true);
    b.hide();
    CHECK(!a.focusNextChild());
    b.show();

    // Focus follows the window: deactivation sends focus-out, activation focus-in
    a.setFocus();
    a.log.clear();
    QWindowSystemInterface::handleWindowActivated(nullptr);
    CHECK(!top.isActiveWindow() && !a.hasFocus() && a.log.contains("focusOut") && top.log.contains("deactivate"));
    CHECK(top.focusWidget() == &a); // remembered
    a.log.clear();
    b.log.clear();
    b.setFocus(); // the window is not active: the focus is only remembered
    CHECK(!b.hasFocus() && top.focusWidget() == &b && b.log.size() <= 1);
    QWindowSystemInterface::handleWindowActivated(top.windowHandle());
    CHECK(b.hasFocus() && b.log.contains("focusIn"));
    CHECK(top.windowHandle()->isActive());
    // deleting the focus widget forgets it
    auto *temp = new Probe(&top);
    temp->setFocusPolicy(Qt::StrongFocus);
    temp->show();
    temp->setFocus();
    CHECK(temp->hasFocus());
    delete temp;
    CHECK(top.focusWidget() == nullptr);
    key(&top, Qt::Key_Q, "q");
    CHECK(top.lastKey == Qt::Key_Q);

    // A window without any widget that takes focus
    Probe lonely;
    lonely.show();
    CHECK(lonely.focusWidget() == nullptr);
    key(&lonely, Qt::Key_Tab);
    CHECK(lonely.log.contains("key"));
}

static QRect g(QWidget *w) { return w->geometry(); }

static void testLayouts() {
    // A vertical box: fixed heights and one item that takes the rest
    QWidget top;
    top.resize(200, 300);
    Probe a(&top), b(&top), c(&top);
    a.setHint(QSize(100, 30));
    b.setHint(QSize(100, 40));
    c.setHint(QSize(100, 20));
    a.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    b.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    c.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *box = new QVBoxLayout(&top);
    CHECK(top.layout() == box && box->parentWidget() == &top && box->count() == 0 && box->isEmpty());
    CHECK(box->direction() == QBoxLayout::TopToBottom && box->spacing() == 6);
    CHECK(box->contentsMargins() == QMargins(11, 11, 11, 11));
    box->addWidget(&a);
    box->addWidget(&b);
    box->addWidget(&c);
    CHECK(box->count() == 3 && !box->isEmpty() && box->indexOf(&b) == 1 && box->indexOf(&top) == -1);
    CHECK(box->itemAt(0)->widget() == &a && box->itemAt(5) == nullptr && box->itemAt(-1) == nullptr);
    CHECK(box->sizeHint() == QSize(100 + 22, 30 + 40 + 20 + 12 + 22));
    CHECK(top.sizeHint() == box->sizeHint());
    box->activate();
    CHECK(g(&a) == QRect(11, 11, 178, 30));
    CHECK(g(&b) == QRect(11, 47, 178, 300 - 22 - 30 - 20 - 12));
    CHECK(g(&c) == QRect(11, 300 - 11 - 20, 178, 20));
    // The layout works from the event loop after a hint changed
    a.setHint(QSize(100, 50));
    settle(&top);
    CHECK(g(&a).height() == 50 && g(&b).top() == 11 + 50 + 6 && g(&c).bottom() == 288);
    a.setHint(QSize(100, 30));
    settle(&top);

    // Margins and spacing
    box->setContentsMargins(1, 2, 3, 4);
    box->setSpacing(10);
    CHECK(box->contentsMargins() == QMargins(1, 2, 3, 4) && box->spacing() == 10);
    box->activate();
    CHECK(g(&a) == QRect(1, 2, 196, 30) && g(&b).top() == 42 && g(&c).bottom() == 295);
    box->setContentsMargins(QMargins(0, 0, 0, 0));
    box->setSpacing(0);
    box->activate();
    CHECK(g(&a) == QRect(0, 0, 200, 30) && g(&b).top() == 30 && g(&c).top() == 280 &&
          box->contentsRect() == QRect(0, 0, 200, 300));
    CHECK(box->geometry() == QRect(0, 0, 200, 300));

    // Shrinking below the hint takes from the items that can shrink
    top.resize(200, 60);
    settle(&top);
    CHECK(g(&a).height() + g(&b).height() + g(&c).height() <= 60 + 1);
    top.resize(200, 300);
    settle(&top);

    // Stretch factors share the extra space
    QWidget s;
    s.resize(300, 100);
    Probe x(&s), y(&s), z(&s);
    for (Probe *p : {&x, &y, &z}) {
        p->setHint(QSize(20, 20));
        p->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
    auto *row = new QHBoxLayout(&s);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(&x, 1);
    row->addWidget(&y, 2);
    row->addWidget(&z);
    row->activate();
    CHECK(g(&z).width() == 20 && g(&x).width() + g(&y).width() == 280);
    CHECK(g(&y).width() > 2 * g(&x).width() - 3 && g(&y).width() < 2 * g(&x).width() + 3);
    CHECK(g(&x).left() == 0 && g(&y).left() == g(&x).right() + 1 && g(&z).left() == g(&y).right() + 1 &&
          g(&z).right() == 299);
    CHECK(g(&x).height() == 100 && row->stretch(0) == 1 && row->stretch(1) == 2 && row->stretch(2) == 0 &&
          row->stretch(9) == 0);
    row->setStretch(2, 5);
    row->setStretch(7, 1);
    CHECK(row->stretch(2) == 5);
    row->activate();
    CHECK(g(&z).width() > g(&y).width());
    CHECK(row->setStretchFactor(&z, 0) && !row->setStretchFactor(&s, 1));
    row->setStretch(0, 0);
    row->setStretch(1, 0);
    // Without stretch factors the items grow equally
    row->activate();
    CHECK(qAbs(g(&x).width() - g(&y).width()) <= 1 && qAbs(g(&y).width() - g(&z).width()) <= 1);
    // ... but only the expanding ones if there are some
    y.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    row->activate();
    CHECK(g(&x).width() == 20 && g(&z).width() == 20 && g(&y).width() == 260);
    // ... and never past the maximum size
    y.setMaximumSize(100, 1000);
    row->activate();
    // (the space that nobody can take goes between the items, as in Qt)
    CHECK(g(&y).width() == 100 && g(&x).width() == 20 && g(&z).width() == 20);
    CHECK(g(&y).left() - g(&x).right() > 1 && g(&y).left() - g(&x).right() == g(&z).left() - g(&y).right());
    y.setMaximumSize(QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX);
    y.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Spacers and stretches
    QWidget t;
    t.resize(200, 50);
    Probe p(&t), q(&t);
    p.setHint(QSize(30, 20));
    q.setHint(QSize(30, 20));
    auto *line = new QHBoxLayout(&t);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(4);
    line->addStretch();
    line->addWidget(&p);
    line->addSpacing(10);
    line->addWidget(&q);
    CHECK(line->count() == 4 && line->itemAt(0)->spacerItem() != nullptr && line->itemAt(1)->spacerItem() == nullptr);
    CHECK(line->itemAt(0)->sizeHint() == QSize(0, 0) && line->itemAt(2)->sizeHint() == QSize(10, 0));
    CHECK(line->itemAt(0)->expandingDirections() == Qt::Orientations(Qt::Horizontal));
    line->activate();
    CHECK(g(&q).right() == 199 && g(&q).width() == 30 && g(&p).right() == g(&q).left() - 4 - 10 - 1 &&
          g(&p).width() == 30);
    CHECK(g(&p).height() == 50 && g(&p).top() == 0);
    line->insertStretch(0, 0);
    line->insertSpacing(1, 6);
    CHECK(line->count() == 6);
    line->insertWidget(0, new Probe(&t), 0);
    CHECK(line->count() == 7);
    line->itemAt(0)->widget()->setFixedSize(8, 8);
    // Alignment across the box
    line->setDirection(QBoxLayout::LeftToRight);
    delete line->itemAt(0)->widget();
    CHECK(line->count() == 6 && line->indexOf(&p) >= 0);
    QWidget aligned;
    aligned.resize(100, 100);
    Probe m(&aligned), n(&aligned), o(&aligned);
    for (Probe *w : {&m, &n, &o})
        w->setHint(QSize(20, 10));
    auto *col = new QHBoxLayout(&aligned);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addWidget(&m, 0, Qt::AlignTop);
    col->addWidget(&n, 0, Qt::AlignVCenter);
    col->addWidget(&o, 0, Qt::AlignBottom);
    col->activate();
    CHECK(g(&m) == QRect(0, 0, g(&m).width(), 10) && g(&n).top() == 45 && g(&n).height() == 10 &&
          g(&o).bottom() == 99 && g(&o).height() == 10);

    // The same numbers as Qt's QVBoxLayout gives: no spacing next to spacers, and shrinking takes the same
    // amount from every item that can give it
    {
        QWidget parity;
        Probe a1(&parity), b1(&parity), c1(&parity);
        for (Probe *w : {&a1, &b1, &c1}) {
            w->setHint(QSize(30, 17));
            w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        }
        a1.minimumHint = QSize(0, 5);
        b1.minimumHint = QSize(0, 10);
        c1.minimumHint = QSize(0, 10);
        auto *vl = new QVBoxLayout(&parity);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(10);
        vl->addWidget(&a1);
        vl->addStretch();
        vl->addWidget(&b1);
        CHECK(vl->sizeHint().height() == 17 + 10 + 17 && vl->minimumSize().height() == 5 + 10 + 10);
        vl->insertSpacing(1, 5);
        CHECK(vl->sizeHint().height() == 17 + 17 + 5 + 10);
        delete vl->takeAt(1);
        delete vl->takeAt(1);
        vl->addWidget(&c1);
        parity.resize(100, 60);
        vl->activate();
        CHECK(g(&a1) == QRect(0, 0, 100, 13) && g(&b1) == QRect(0, 23, 100, 14) && g(&c1) == QRect(0, 47, 100, 13));
        // Three items that all may grow share 200 pixels equally, not their extra space
        parity.resize(100, 200);
        vl->activate();
        CHECK(g(&a1).height() == 60 && g(&b1).top() == 70 && g(&c1).top() == 140);
    }

    // Reversed directions
    QWidget rev;
    rev.resize(100, 40);
    Probe r1(&rev), r2(&rev);
    r1.setHint(QSize(20, 10));
    r2.setHint(QSize(30, 10));
    r1.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    r2.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *rl = new QBoxLayout(QBoxLayout::RightToLeft, &rev);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    rl->addWidget(&r1);
    rl->addWidget(&r2);
    rl->activate();
    // Items that cannot grow are spread out, with the space that is left (50 / 3) before, between and after them
    CHECK(g(&r1).right() == 83 && g(&r2).right() == 47 && g(&r1).width() == 20 && g(&r2).width() == 30);
    rl->setDirection(QBoxLayout::LeftToRight);
    rl->setDirection(QBoxLayout::LeftToRight);
    rl->activate();
    CHECK(g(&r1).left() == 16 && g(&r2).left() == 52);
    rl->setDirection(QBoxLayout::BottomToTop);
    CHECK(rl->direction() == QBoxLayout::BottomToTop);
    rl->activate();
    CHECK(g(&r1).bottom() == 33 && g(&r2).bottom() == 17);

    // Nested layouts
    QWidget nest;
    nest.resize(200, 200);
    Probe n1(&nest), n2(&nest), n3(&nest);
    for (Probe *w : {&n1, &n2, &n3})
        w->setHint(QSize(40, 40));
    auto *outer = new QVBoxLayout(&nest);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *inner = new QHBoxLayout;
    CHECK(inner->parentWidget() == nullptr);
    outer->addWidget(&n1);
    outer->addLayout(inner, 1);
    inner->addWidget(&n2);
    inner->addWidget(&n3);
    CHECK(inner->parent() == outer && inner->parentWidget() == &nest && outer->count() == 2 &&
          outer->itemAt(1)->layout() == inner);
    CHECK(inner->spacing() == outer->spacing() && inner->contentsMargins() == QMargins());
    outer->activate();
    CHECK(g(&n1) == QRect(0, 0, 200, 40) && g(&n2).top() == 40 && g(&n2).height() == 160 && g(&n3).right() == 199);
    CHECK(inner->geometry() == QRect(0, 40, 200, 160) && inner->contentsRect() == inner->geometry());
    inner->activate(); // activates the outermost
    n2.setHint(QSize(40, 90));
    settle(&nest);
    CHECK(outer->sizeHint().height() == 40 + 90);
    CHECK(nest.layout()->expandingDirections() == Qt::Orientations() || true);
    delete inner; // deleting a layout removes it from its parent
    CHECK(outer->count() == 1 || outer->count() == 2);

    // Hidden widgets take no space; deleting a widget removes it
    QWidget hid;
    hid.resize(100, 100);
    Probe h1(&hid), h2(&hid), h3(&hid);
    for (Probe *w : {&h1, &h2, &h3}) {
        w->setHint(QSize(100, 20));
        w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    auto *hl = new QVBoxLayout(&hid);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(10);
    hl->addWidget(&h1);
    hl->addWidget(&h2);
    hl->addWidget(&h3);
    hid.show();
    settle(&hid);
    CHECK(g(&h2).top() == 40 && g(&h3).top() == 75 && hl->sizeHint().height() == 80);
    h2.hide();
    settle(&hid);
    CHECK(g(&h3).top() == 62 && hl->sizeHint().height() == 50);
    h2.show();
    settle(&hid);
    CHECK(g(&h3).top() == 75);
    auto *gone = new Probe(&hid);
    gone->setHint(QSize(100, 20));
    hl->addWidget(gone);
    CHECK(hl->count() == 4 && gone->isVisible()); // a widget that is added to a visible parent is shown
    delete gone;
    CHECK(hl->count() == 3);
    hl->removeWidget(&h1);
    CHECK(hl->count() == 2 && h1.parentWidget() == &hid && hl->indexOf(&h1) == -1);
    hl->removeWidget(&h1); // not in it any more
    hl->addWidget(&h1);
    CHECK(hl->count() == 3);
    QLayoutItem *taken = hl->takeAt(2);
    CHECK(taken->widget() == &h1 && hl->count() == 2 && hl->takeAt(7) == nullptr);
    delete taken;
    hl->addWidget(&h1);
    QLayoutItem *removed = hl->itemAt(2);
    hl->removeItem(removed);
    delete removed; // the caller owns an item that was removed
    CHECK(hl->count() == 2);
    hl->addWidget(&h1);
    hl->addWidget(nullptr);
    hl->addLayout(nullptr);
    hl->insertWidget(0, nullptr);
    CHECK(hl->count() == 3);

    // The widgets of a layout that is added to a layout of a widget are adopted, and shown
    QWidget host2;
    host2.resize(100, 100);
    auto *top2 = new QVBoxLayout(&host2);
    host2.show();
    auto *sub2 = new QHBoxLayout;
    Probe sw1, sw2;
    sub2->addWidget(&sw1);
    sub2->addWidget(&sw2);
    CHECK(sw1.isWindow());
    top2->addLayout(sub2);
    CHECK(sw1.parentWidget() == &host2 && sw2.parentWidget() == &host2);
    settle(&host2);
    CHECK(sw1.isVisible() && sw2.isVisible() && sw1.width() > 0);

    // A widget in a layout of another parent changes parents
    QWidget other;
    Probe foreign;
    auto *ol = new QVBoxLayout(&other);
    ol->addWidget(&foreign);
    CHECK(foreign.parentWidget() == &other && !foreign.isWindow());

    // A layout made before it has a widget is adopted by setLayout()
    QWidget adopt;
    Probe ad1;
    auto *free = new QVBoxLayout;
    free->addWidget(&ad1);
    CHECK(ad1.isWindow());
    adopt.setLayout(free);
    CHECK(ad1.parentWidget() == &adopt && adopt.layout() == free && free->parentWidget() == &adopt);
    adopt.setLayout(free); // the same layout: fine
    QVBoxLayout *second = new QVBoxLayout;
    adopt.setLayout(second); // refused with a warning
    CHECK(adopt.layout() == free && second->parentWidget() == nullptr);
    delete second;
    adopt.setLayout(nullptr);
    CHECK(adopt.layout() == free);

    // The window takes the size of its layout when it is shown, and does not become smaller than it
    QWidget win;
    Probe w1(&win), w2(&win);
    w1.setHint(QSize(100, 50));
    w2.setHint(QSize(150, 60));
    w1.minimumHint = QSize(100, 50);
    w2.minimumHint = QSize(150, 60);
    w1.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    w2.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    auto *wl = new QVBoxLayout(&win);
    wl->addWidget(&w1);
    wl->addWidget(&w2);
    win.show();
    CHECK(win.size() == QSize(150 + 22, 50 + 60 + 6 + 22));
    CHECK(g(&w1) == QRect(11, 11, 150, 50) && g(&w2) == QRect(11, 67, 150, 60));
    win.resize(10, 10);
    settle(&win);
    CHECK(win.width() >= 172 && win.height() >= 138);
    win.resize(400, 400);
    settle(&win);
    CHECK(g(&w1).width() == 378 && g(&w1).height() >= 50 && g(&w2).height() >= 60); // Minimum grows; both can
    CHECK(wl->minimumSize() == QSize(172, 138) && wl->maximumSize().width() > 100000);
    CHECK(wl->expandingDirections() == Qt::Orientations());

    // The layout and its widgets go away with the window
    auto *tmp = new QWidget;
    auto *tl = new QHBoxLayout(tmp);
    tl->addWidget(new Probe);
    tl->addWidget(new QLabel("x"));
    delete tmp;

    // Spacing of nested layouts and an own spacing
    QHBoxLayout loose;
    CHECK(loose.spacing() == 6 && loose.contentsMargins() == QMargins(9, 9, 9, 9) && loose.parentWidget() == nullptr);
    loose.setSpacing(2);
    CHECK(loose.spacing() == 2);
    CHECK(!loose.activate()); // no widget
    loose.update();
    loose.invalidate();
}

static void testLabel() {
    QLabel empty;
    CHECK(empty.text().isEmpty() && empty.alignment() == (Qt::AlignLeft | Qt::AlignVCenter) && !empty.wordWrap() &&
          empty.margin() == 0);
    QLabel label("Hello");
    CHECK(label.text() == QString("Hello"));
    const QSize hint = label.sizeHint();
    CHECK(hint.width() == label.fontMetrics().horizontalAdvance("Hello") &&
          hint.height() == label.fontMetrics().height());
    label.setText("Hello, world");
    CHECK(label.sizeHint().width() > hint.width());
    label.setText("Hello, world"); // unchanged
    label.setNum(1234);
    CHECK(label.text() == QString("1234"));
    label.clear();
    CHECK(label.text().isEmpty());
    label.setMargin(5);
    CHECK(label.margin() == 5 && label.sizeHint() == QSize(10, label.fontMetrics().height() + 10));
    label.setMargin(0);
    label.setText("one\ntwo");
    CHECK(label.sizeHint().height() == 2 * label.fontMetrics().lineSpacing() - label.fontMetrics().leading());
    label.setText("a long text that wraps");
    label.setWordWrap(true);
    CHECK(label.wordWrap() && label.minimumSizeHint().width() < label.sizeHint().width());
    label.setWordWrap(false);
    CHECK(label.minimumSizeHint() == label.sizeHint());
    label.setText("one\ntwo");
    CHECK(label.property("text").toString() == QString("one\ntwo") && label.setProperty("text", "x") &&
          label.text() == QString("x"));
    CHECK(label.setProperty("wordWrap", true) && label.wordWrap() && label.setProperty("margin", 2) &&
          label.margin() == 2);
    label.setProperty("margin", 0);
    label.setProperty("wordWrap", false);

    // Painting: the text is drawn with the WindowText color at the alignment
    QWidget top;
    top.resize(200, 40);
    QPalette p;
    p.setColor(QPalette::Window, Qt::white);
    p.setColor(QPalette::WindowText, Qt::black);
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(180, 180, 180));
    top.setPalette(p);
    QLabel text("Right", &top);
    text.setGeometry(0, 0, 200, 40);
    text.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    CHECK(text.alignment() == (Qt::AlignRight | Qt::AlignVCenter));
    top.show();
    auto inkBox = [&](const QImage &img) {
        QRect r;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != 0xffefefefu && img.pixel(x, y) != 0xffffffffu) {
                    r = r.isNull() ? QRect(x, y, 1, 1) : r.united(QRect(x, y, 1, 1));
                }
        return r;
    };
    const bool fonts = haveRealFonts();
    QImage img = grab(&top);
    QRect box = inkBox(img);
    if (fonts) {
        CHECK(!box.isNull() && box.right() <= 199 && box.right() > 190 && box.left() > 140);
        CHECK(box.top() > 5 && box.bottom() < 35);
    }
    text.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    img = grab(&top);
    box = inkBox(img);
    CHECK(!fonts || (box.top() < 8 && qAbs(box.center().x() - 100) < 8));
    text.setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    img = grab(&top);
    box = inkBox(img);
    CHECK(!fonts || (box.left() < 4 && box.bottom() > 30));
    int darkest = 255;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            darkest = std::min(darkest, qRed(img.pixel(x, y)));
    CHECK(!fonts || darkest < 60);
    text.setEnabled(false);
    img = grab(&top);
    darkest = 255;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            darkest = std::min(darkest, qRed(img.pixel(x, y)));
    CHECK(darkest >= 170); // the disabled color
    text.setEnabled(true);
    top.resize(200, 140);
    text.setWordWrap(true);
    text.setText("the quick brown fox jumps over the lazy dog and keeps running");
    text.resize(80, 120);
    text.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    img = grab(&top);
    box = inkBox(img);
    CHECK(!fonts || (box.right() < 82 && box.height() > 3 * text.fontMetrics().height() - 4));

    // A label in a layout takes its size hint
    QWidget host;
    auto *vl = new QVBoxLayout(&host);
    auto *l1 = new QLabel("A short text", &host);
    vl->addWidget(l1);
    host.show();
    CHECK(host.width() == l1->sizeHint().width() + 22 && l1->isVisible());
    l1->setText("A much longer text than before");
    settle(&host);
    CHECK(l1->width() >= l1->sizeHint().width() && host.width() >= l1->sizeHint().width() + 18);
}

static void testButtons() {
    QWidget top;
    top.resize(300, 100);
    QPushButton button("Press", &top);
    button.setGeometry(10, 10, 100, 30);
    CHECK(button.text() == QString("Press") && button.focusPolicy() == Qt::StrongFocus && !button.isCheckable() &&
          !button.isChecked());
    CHECK(!button.isDown() && !button.isFlat() &&
          button.sizePolicy() == QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed));
    CHECK(button.sizeHint().width() >= 80 && button.sizeHint().height() >= button.fontMetrics().height() + 8);
    const QSize before = button.sizeHint();
    button.setText("A button with a long text");
    CHECK(button.sizeHint().width() > before.width());
    button.setText("Press");
    int pressed = 0, released = 0, clicked = 0, toggled = 0;
    bool lastChecked = false, lastToggle = false;
    QObject::connect(&button, &QAbstractButton::pressed, [&] { ++pressed; });
    QObject::connect(&button, &QAbstractButton::released, [&] { ++released; });
    QObject::connect(&button, &QAbstractButton::clicked, [&](bool c) {
        ++clicked;
        lastChecked = c;
    });
    QObject::connect(&button, &QAbstractButton::toggled, [&](bool c) {
        ++toggled;
        lastToggle = c;
    });
    top.show();

    // click()
    button.click();
    CHECK(pressed == 1 && released == 1 && clicked == 1 && toggled == 0 && !button.isDown() && !lastChecked);
    button.setChecked(true); // not checkable
    CHECK(!button.isChecked() && toggled == 0);
    button.setDown(true);
    CHECK(button.isDown());
    button.setDown(false);

    // The mouse: press and release inside clicks
    mouse(&top, QEvent::MouseButtonPress, QPoint(30, 20));
    CHECK(button.isDown() && pressed == 2 && released == 1 && clicked == 1);
    mouse(&top, QEvent::MouseButtonRelease, QPoint(30, 20), Qt::LeftButton, Qt::NoButton);
    CHECK(!button.isDown() && released == 2 && clicked == 2);
    // Released outside: no click; the button comes up when the mouse leaves and goes down when it returns
    mouse(&top, QEvent::MouseButtonPress, QPoint(30, 20));
    mouse(&top, QEvent::MouseMove, QPoint(200, 90), Qt::NoButton, Qt::LeftButton);
    CHECK(!button.isDown());
    mouse(&top, QEvent::MouseMove, QPoint(40, 25), Qt::NoButton, Qt::LeftButton);
    CHECK(button.isDown());
    mouse(&top, QEvent::MouseMove, QPoint(200, 90), Qt::NoButton, Qt::LeftButton);
    const int clickedBefore = clicked;
    mouse(&top, QEvent::MouseButtonRelease, QPoint(200, 90), Qt::LeftButton, Qt::NoButton);
    CHECK(clicked == clickedBefore && !button.isDown());
    // Other buttons do nothing; a plain move does nothing
    mouse(&top, QEvent::MouseButtonPress, QPoint(30, 20), Qt::RightButton, Qt::RightButton);
    CHECK(!button.isDown());
    mouse(&top, QEvent::MouseButtonRelease, QPoint(30, 20), Qt::RightButton, Qt::NoButton);
    mouse(&top, QEvent::MouseMove, QPoint(30, 20), Qt::NoButton, Qt::NoButton);
    CHECK(!button.isDown() && clicked == clickedBefore);
    // Pressing starts a click that a disabled button cannot make
    button.setEnabled(false);
    button.click();
    click(&top, QPoint(30, 20));
    CHECK(clicked == clickedBefore);
    button.setEnabled(true);

    // The keyboard: Space (press and release) clicks
    button.setFocus();
    CHECK(button.hasFocus());
    const int before2 = clicked;
    QWindow *win = top.windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    CHECK(button.isDown() && clicked == before2);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ",
                                           true); // auto repeat
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
    CHECK(!button.isDown() && clicked == before2 + 1);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier); // no press before
    CHECK(clicked == before2 + 1);
    // As in Qt, Return and Enter click a button outside a dialog only if it is auto-default
    CHECK(!button.autoDefault() && !button.isDefault());
    key(&top, Qt::Key_Return);
    CHECK(clicked == before2 + 1);
    button.setAutoDefault(true);
    key(&top, Qt::Key_Return);
    key(&top, Qt::Key_Enter);
    CHECK(clicked == before2 + 3);
    button.setAutoDefault(false);
    key(&top, Qt::Key_X);
    CHECK(clicked == before2 + 3);
    // Losing the focus while Space is down raises the button
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    CHECK(button.isDown());
    button.clearFocus();
    CHECK(!button.isDown());
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
    button.setFocus();
    // A button that gets disabled while it is down comes up
    mouse(&top, QEvent::MouseButtonPress, QPoint(30, 20));
    CHECK(button.isDown());
    button.setEnabled(false);
    CHECK(!button.isDown());
    button.setEnabled(true);
    mouse(&top, QEvent::MouseButtonRelease, QPoint(30, 20), Qt::LeftButton, Qt::NoButton);

    // Checkable buttons
    button.setCheckable(true);
    CHECK(button.isCheckable() && button.property("checkable").toBool());
    pressed = released = clicked = toggled = 0;
    button.click();
    CHECK(button.isChecked() && toggled == 1 && lastToggle && clicked == 1 && lastChecked);
    click(&top, QPoint(30, 20));
    CHECK(!button.isChecked() && toggled == 2 && !lastToggle && clicked == 2 && !lastChecked);
    button.toggle();
    CHECK(button.isChecked() && toggled == 3);
    button.setChecked(true); // no change, no signal
    button.setChecked(false);
    CHECK(!button.isChecked() && toggled == 4);
    CHECK(button.setProperty("checked", true) && button.isChecked());
    button.setCheckable(false);
    button.setChecked(false);
    CHECK(button.isChecked()); // not checkable: ignored

    // Flat buttons and painting
    QWidget host;
    host.resize(100, 60);
    QPushButton pb("OK", &host);
    pb.setGeometry(10, 10, 80, 30);
    host.show();
    QImage normal = grab(&host);
    const QRgb face = normal.pixel(40, 25);
    CHECK(face != host.palette().window().color().rgb() || true);
    mouse(&host, QEvent::MouseButtonPress, QPoint(40, 25));
    QImage pressedImage = grab(&host);
    CHECK(pressedImage.pixel(12, 12) != normal.pixel(12, 12)); // darker face
    mouse(&host, QEvent::MouseButtonRelease, QPoint(40, 25), Qt::LeftButton, Qt::NoButton);
    QImage hovered = grab(&host);
    CHECK(hovered.pixel(12, 12) != normal.pixel(12, 12)); // the mouse is over it: a lighter face
    QWindowSystemInterface::handleLeaveEvent(host.windowHandle());
    pb.setFlat(true);
    CHECK(pb.isFlat() && pb.property("flat").toBool());
    QImage flat = grab(&host);
    CHECK(flat.pixel(12, 12) == host.palette().window().color().rgb());
    pb.setFlat(false);
    pb.setEnabled(false);
    QImage disabled = grab(&host);
    CHECK(disabled.pixel(40, 25) != 0 && disabled != normal);
    pb.setEnabled(true);
    // The text of the label is drawn centered
    QImage plainImage = grab(&host);
    int ink = 0;
    for (int y = 12; y < 38; ++y)
        for (int x = 30; x < 70; ++x)
            if (qRed(plainImage.pixel(x, y)) < 100)
                ++ink;
    CHECK(!haveRealFonts() || ink > 20);

    // Check boxes
    QWidget cbHost;
    cbHost.resize(200, 40);
    QCheckBox box("Check me", &cbHost);
    box.setGeometry(5, 5, 150, 24);
    CHECK(box.isCheckable() && !box.isChecked() && box.text() == QString("Check me") &&
          box.focusPolicy() == Qt::StrongFocus);
    CHECK(box.sizeHint().width() > box.fontMetrics().horizontalAdvance("Check me") && box.sizeHint().height() >= 16);
    QCheckBox plainBox(&cbHost);
    CHECK(plainBox.text().isEmpty() && plainBox.isCheckable() && plainBox.sizeHint().width() == 14);
    plainBox.setGeometry(0, 0, 0, 0);
    plainBox.hide();
    cbHost.show();
    QImage unchecked = grab(&cbHost);
    int boxToggles = 0;
    QObject::connect(&box, &QCheckBox::toggled, [&](bool) { ++boxToggles; });
    click(&cbHost, QPoint(10, 15)); // on the indicator
    CHECK(box.isChecked() && boxToggles == 1);
    QImage checked = grab(&cbHost);
    CHECK(checked != unchecked);
    click(&cbHost, QPoint(30, 15)); // on the text
    CHECK(!box.isChecked() && boxToggles == 2);
    click(&cbHost, QPoint(180, 15)); // beyond the text: not the box
    CHECK(!box.isChecked() && boxToggles == 2);
    box.setFocus();
    key(&cbHost, Qt::Key_Space, " ");
    CHECK(box.isChecked() && boxToggles == 3);
    key(&cbHost, Qt::Key_Return);
    CHECK(box.isChecked()); // Return does not toggle a check box
    box.setEnabled(false);
    QImage dis = grab(&cbHost);
    CHECK(dis != checked);
    box.setEnabled(true);
    box.setText("");
    CHECK(box.sizeHint().width() == 14);

    // Buttons in a layout fill the width at their hint height and keep their policy
    QWidget row;
    auto *hl = new QHBoxLayout(&row);
    auto *b1 = new QPushButton("One", &row);
    auto *b2 = new QPushButton("Two", &row);
    hl->addWidget(b1);
    hl->addWidget(b2);
    row.resize(300, 100);
    row.show();
    settle(&row);
    CHECK(b1->height() == b1->sizeHint().height() && b1->width() > b1->sizeHint().width() &&
          b2->width() > b2->sizeHint().width());
    CHECK(qAbs(b1->width() - b2->width()) <= 1);
}

static void testSignalsAndDeletion() {
    // Destroying widgets in every order is safe
    {
        auto *top = new QWidget;
        auto *l = new QVBoxLayout(top);
        auto *button = new QPushButton("x", top);
        auto *label = new QLabel("y", top);
        l->addWidget(button);
        l->addWidget(label);
        top->show();
        QCoreApplication::sendPostedEvents();
        delete label;
        delete top;
    }
    {
        QWidget top;
        top.show();
        auto *child = new QPushButton("x", &top);
        child->show();
        child->setFocus();
        delete child;
        top.update();
        settle(&top);
    }
    {
        // deleting a window that is under the mouse and has the focus
        auto *top = new Probe;
        auto *c = new Probe(top);
        c->setGeometry(0, 0, 50, 50);
        c->setFocusPolicy(Qt::StrongFocus);
        top->show();
        mouse(top, QEvent::MouseMove, QPoint(10, 10), Qt::NoButton, Qt::NoButton);
        mouse(top, QEvent::MouseButtonPress, QPoint(10, 10));
        delete c;
        mouse(top, QEvent::MouseButtonRelease, QPoint(10, 10), Qt::LeftButton, Qt::NoButton);
        delete top;
    }
    {
        // the application closes all windows
        QWidget a, b;
        a.show();
        b.show();
        int quits = 0;
        QObject::connect(qApp, &QGuiApplication::lastWindowClosed, [&] { ++quits; });
        QApplication::closeAllWindows();
        CHECK(!a.isVisible() && !b.isVisible() && quits == 1);
        QObject::disconnect(qApp, nullptr, nullptr, nullptr);
    }
    {
        // raise and lower on windows and the properties of a hidden widget
        QWidget a;
        a.raise();
        a.lower();
        a.show();
        a.raise();
        a.lower();
        a.activateWindow();
        CHECK(a.isActiveWindow());
        QWidget child(&a);
        CHECK(!child.isActiveWindow() == !a.isActiveWindow());
        child.activateWindow();
    }
}

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    testSizePolicy();
    testPalette();
    testApplication();
    testRender();
    testSizeHelpersAndOptions();
    testWidgetBasics();
    testVisibilityAndEnabled();
    testGeometryOfWindows();
    testPainting();
    testMouse();
    testKeysAndFocus();
    testLayouts();
    testGridLayout();
    testFormLayout();
    testStackedLayout();
    testStackedWidget();
    testLabel();
    testButtons();
    testSignalsAndDeletion();
    if (g_failures == 0)
        std::puts("all widget tests passed");
    return g_failures == 0 ? 0 : 1;
}
