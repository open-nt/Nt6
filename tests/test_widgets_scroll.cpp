// MIT License - see LICENSE
//
// Tests for frames, scroll bars, scroll areas, lists, tables and text editors of Nt Widgets on the offscreen platform. The tests play the
// window system through QWindowSystemInterface.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qscrollarea.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtablewidget.h>
#include <QtWidgets/qtextedit.h>
#include <QtWidgets/qtooltip.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qtooltip_p.h"

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

void press(QWidget *w, QPoint local, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    mouse(w, QEvent::MouseButtonPress, local, Qt::LeftButton, Qt::LeftButton, mods);
}
void drag(QWidget *w, QPoint local) { mouse(w, QEvent::MouseMove, local, Qt::NoButton, Qt::LeftButton); }
void release(QWidget *w, QPoint local) { mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton); }
void click(QWidget *w, QPoint local, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    press(w, local, mods);
    release(w, local);
}

void key(QWidget *w, int k, const QString &text = QString(), Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, k, mods, text);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, k, mods);
}

void type(QWidget *w, const QString &text) {
    for (QChar c : text) {
        int code = c.unicode();
        if (code >= 'a' && code <= 'z')
            code -= 32;
        key(w, c == QChar(' ') ? int(Qt::Key_Space) : code, QString(c));
    }
}

void wheel(QWidget *w, QPoint local, int notches) {
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleWheelEvent(w->window()->windowHandle(), QPointF(inWindow), QPointF(inWindow),
                                             QPoint(), QPoint(0, 120 * notches), Qt::NoModifier);
}

QPoint inWindow(QWidget *w, QPoint local = QPoint(0, 0)) { return w->mapTo(w->window(), local); }

void hover(QWidget *w, QPoint local) { mouse(w, QEvent::MouseMove, local, Qt::NoButton, Qt::NoButton); }

// Runs the event loop until `condition` holds or `ms` milliseconds have passed.
template <typename Condition> bool waitFor(Condition condition, int ms = 2000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > ms)
            return false;
        QEventLoop loop;
        QTimer::singleShot(3, [&] { loop.quit(); });
        loop.exec();
    }
    return true;
}

// A scroll area with contents of a given size that records what it is told.
class Area : public QAbstractScrollArea {
public:
    explicit Area(QWidget *parent = nullptr) : QAbstractScrollArea(parent) { setFocusPolicy(Qt::StrongFocus); }

    QSize contents{1000, 800};
    QColor fill{200, 40, 40};
    QPoint scrolled;
    QPoint lastPress;
    int presses = 0;
    int paints = 0;
    int resizes = 0;
    QSize lastResize;

    void updateRanges() {
        horizontalScrollBar()->setRange(0, std::max(0, contents.width() - viewport()->width()));
        verticalScrollBar()->setRange(0, std::max(0, contents.height() - viewport()->height()));
        horizontalScrollBar()->setPageStep(viewport()->width());
        verticalScrollBar()->setPageStep(viewport()->height());
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        ++resizes;
        lastResize = event->size();
        updateRanges();
    }
    void scrollContentsBy(int dx, int dy) override {
        scrolled += QPoint(dx, dy);
        QAbstractScrollArea::scrollContentsBy(dx, dy);
    }
    void paintEvent(QPaintEvent *) override {
        ++paints;
        QPainter p(viewport());
        p.fillRect(viewport()->rect(), fill);
    }
    void mousePressEvent(QMouseEvent *event) override {
        ++presses;
        lastPress = event->pos();
    }
};

} // namespace

static void testFrame() {
    QWidget top;
    top.resize(120, 80);
    QFrame frame(&top);
    frame.setGeometry(10, 10, 100, 60);
    CHECK(frame.frameShape() == QFrame::NoFrame && frame.frameShadow() == QFrame::Plain && frame.frameWidth() == 0);
    CHECK(frame.lineWidth() == 1 && frame.midLineWidth() == 0 && frame.contentsRect() == QRect(0, 0, 100, 60));

    frame.setFrameStyle(QFrame::Box | QFrame::Raised);
    CHECK(frame.frameShape() == QFrame::Box && frame.frameShadow() == QFrame::Raised);
    CHECK(frame.frameStyle() == (QFrame::Box | QFrame::Raised));
    CHECK(frame.frameWidth() == 2); // two lines of one pixel
    frame.setMidLineWidth(2);
    CHECK(frame.frameWidth() == 4 && frame.contentsRect() == QRect(4, 4, 92, 52));
    frame.setFrameShadow(QFrame::Plain);
    CHECK(frame.frameWidth() == 1); // a plain box is one line
    frame.setLineWidth(3);
    CHECK(frame.frameWidth() == 3 && frame.lineWidth() == 3);
    frame.setLineWidth(-4);
    CHECK(frame.lineWidth() == 0);

    // A shadow of 0 stands for Plain
    frame.setFrameStyle(QFrame::Panel);
    CHECK(frame.frameShadow() == QFrame::Plain);
    frame.setLineWidth(2);
    CHECK(frame.frameWidth() == 2);
    frame.setFrameShape(QFrame::StyledPanel);
    frame.setFrameShadow(QFrame::Sunken);
    CHECK(frame.frameStyle() == (QFrame::StyledPanel | QFrame::Sunken));
    CHECK(frame.frameWidth() == frame.style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, &frame));

    // The shape is drawn in the text color for a plain frame, and nothing is drawn inside it
    top.setAutoFillBackground(true);
    frame.setFrameStyle(QFrame::Box | QFrame::Plain);
    frame.setLineWidth(2);
    top.show();
    QImage image = grab(&top);
    const QRgb text = frame.palette().color(QPalette::WindowText).rgb();
    const QRgb window = top.palette().color(QPalette::Window).rgb();
    const QPoint o = inWindow(&frame);
    CHECK(image.pixel(o) == text && image.pixel(o + QPoint(1, 1)) == text && image.pixel(o + QPoint(99, 59)) == text);
    CHECK(image.pixel(o + QPoint(50, 1)) == text && image.pixel(o + QPoint(2, 2)) == window);
    CHECK(image.pixel(o + QPoint(50, 30)) == window && image.pixel(o + QPoint(-1, 30)) == window);

    // Raised: light on the top and left, dark on the bottom and right; sunken the other way
    frame.setFrameStyle(QFrame::Panel | QFrame::Raised);
    frame.setLineWidth(1);
    image = grab(&top);
    const QRgb light = frame.palette().color(QPalette::Light).rgb(), dark = frame.palette().color(QPalette::Dark).rgb();
    CHECK(image.pixel(o + QPoint(50, 0)) == light && image.pixel(o + QPoint(0, 30)) == light);
    CHECK(image.pixel(o + QPoint(50, 59)) == dark && image.pixel(o + QPoint(99, 30)) == dark);
    frame.setFrameShadow(QFrame::Sunken);
    image = grab(&top);
    CHECK(image.pixel(o + QPoint(50, 0)) == dark && image.pixel(o + QPoint(50, 59)) == light);

    // A line is drawn across the middle
    frame.setFrameStyle(QFrame::HLine | QFrame::Plain);
    frame.setLineWidth(2);
    image = grab(&top);
    CHECK(image.pixel(o + QPoint(50, 28)) == text && image.pixel(o + QPoint(50, 29)) == text);
    CHECK(image.pixel(o + QPoint(50, 27)) == window && image.pixel(o + QPoint(50, 30)) == window);
    CHECK(image.pixel(o + QPoint(0, 29)) == text && image.pixel(o + QPoint(99, 29)) == text);
    frame.setFrameStyle(QFrame::VLine | QFrame::Sunken);
    frame.setLineWidth(1);
    image = grab(&top);
    // the middle line of 2 pixels is still set: 1 + 2 + 1
    CHECK(image.pixel(o + QPoint(47, 30)) == dark && image.pixel(o + QPoint(50, 30)) == light);
    CHECK(image.pixel(o + QPoint(48, 30)) == window && image.pixel(o + QPoint(49, 30)) == window);
    CHECK(image.pixel(o + QPoint(40, 30)) == window);

    // A frame with no shape draws nothing
    frame.setFrameStyle(QFrame::NoFrame);
    image = grab(&top);
    CHECK(image.pixel(o) == window && image.pixel(o + QPoint(50, 30)) == window);
}

static void testScrollBar() {
    QWidget top;
    top.resize(300, 200);
    QScrollBar vbar(&top);
    QScrollBar hbar(Qt::Horizontal, &top);
    CHECK(vbar.orientation() == Qt::Vertical && hbar.orientation() == Qt::Horizontal);
    CHECK(vbar.minimum() == 0 && vbar.maximum() == 99 && vbar.value() == 0 && vbar.singleStep() == 1 &&
          vbar.pageStep() == 10);
    CHECK(vbar.sizePolicy().verticalPolicy() == QSizePolicy::Expanding &&
          vbar.sizePolicy().horizontalPolicy() == QSizePolicy::Fixed);
    CHECK(hbar.sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);
    const int extent = vbar.style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, &vbar);
    CHECK(extent > 8 && vbar.sizeHint().width() == extent && hbar.sizeHint().height() == extent);
    CHECK(vbar.sizeHint().height() >= 2 * extent && vbar.focusPolicy() == Qt::NoFocus);

    int values = 0, ranges = 0, pressed = 0, released = 0, moved = 0;
    QObject::connect(&vbar, &QScrollBar::valueChanged, [&](int) { ++values; });
    QObject::connect(&vbar, &QScrollBar::rangeChanged, [&](int, int) { ++ranges; });
    QObject::connect(&vbar, &QScrollBar::sliderPressed, [&] { ++pressed; });
    QObject::connect(&vbar, &QScrollBar::sliderReleased, [&] { ++released; });
    QObject::connect(&vbar, &QScrollBar::sliderMoved, [&](int) { ++moved; });
    vbar.setValue(40);
    CHECK(vbar.value() == 40 && vbar.sliderPosition() == 40 && values == 1);
    vbar.setValue(40);
    CHECK(values == 1);
    vbar.setValue(500);
    CHECK(vbar.value() == 99 && values == 2);
    vbar.setRange(0, 50);
    CHECK(vbar.value() == 50 && values == 3 && ranges == 1);
    vbar.setRange(0, 50);
    CHECK(ranges == 1);
    vbar.setMaximum(20);
    CHECK(vbar.maximum() == 20 && vbar.value() == 20);
    vbar.setMinimum(30);
    CHECK(vbar.minimum() == 30 && vbar.maximum() == 30 && vbar.value() == 30);
    vbar.setRange(10, 0);
    CHECK(vbar.minimum() == 10 && vbar.maximum() == 10);
    vbar.setSingleStep(0);
    vbar.setPageStep(0);
    CHECK(vbar.singleStep() == 1 && vbar.pageStep() == 1);

    // The handle is as long as the page is of the range plus the page
    vbar.setGeometry(280, 0, extent, 100);
    hbar.setGeometry(0, 180, 200, extent);
    vbar.setRange(0, 90);
    vbar.setPageStep(10);
    vbar.setSingleStep(1);
    vbar.setValue(0);
    auto handle = [](QScrollBar &bar) {
        QStyleOptionSlider option;
        option.initFrom(&bar);
        option.orientation = bar.orientation();
        option.minimum = bar.minimum();
        option.maximum = bar.maximum();
        option.sliderPosition = bar.sliderPosition();
        option.pageStep = bar.pageStep();
        return bar.style()->subElementRect(QStyle::SE_ScrollBarSlider, &option, &bar);
    };
    QRect h = handle(vbar);
    const int minLength = vbar.style()->pixelMetric(QStyle::PM_ScrollBarSliderMin, nullptr, &vbar);
    CHECK(h.top() == 0 && h.height() == std::max(minLength, 10) && h.width() < extent);
    vbar.setValue(90);
    h = handle(vbar);
    CHECK(h.bottom() == 99);
    vbar.setPageStep(50);
    vbar.setRange(0, 50);
    vbar.setValue(0);
    CHECK(handle(vbar).height() == 50); // 50 of 100
    vbar.setValue(25);
    CHECK(handle(vbar).top() == 25);
    // Without a range there is nothing to scroll, and clicking does nothing
    hbar.setRange(0, 0);
    CHECK(handle(hbar).width() == hbar.width());

    top.show();
    settle(&top);
    // Click next to the handle: a page
    vbar.setRange(0, 90);
    vbar.setPageStep(10);
    vbar.setValue(0);
    values = 0;
    click(&vbar, QPoint(7, 90));
    CHECK(vbar.value() == 10 && values == 1);
    click(&vbar, QPoint(7, 2));
    CHECK(vbar.value() == 10 || vbar.value() == 0); // the handle is now at the top, 10 of 100: it is under the point
    vbar.setValue(60);
    click(&vbar, QPoint(7, 2));
    CHECK(vbar.value() == 50);
    CHECK(pressed == 0);

    // Dragging the handle
    vbar.setValue(0);
    const QRect grab0 = handle(vbar);
    values = 0;
    press(&vbar, grab0.center());
    CHECK(vbar.isSliderDown() && pressed == 1);
    drag(&vbar, grab0.center() + QPoint(0, 45));
    CHECK(vbar.value() > 40 && vbar.value() < 60 && moved >= 1);
    drag(&vbar, grab0.center() + QPoint(3, 500));
    CHECK(vbar.value() == 90 && vbar.sliderPosition() == 90);
    drag(&vbar, grab0.center() + QPoint(0, -500));
    CHECK(vbar.value() == 0);
    release(&vbar, grab0.center());
    CHECK(!vbar.isSliderDown() && released == 1);
    const int after = vbar.value();
    drag(&vbar, QPoint(7, 50));
    CHECK(vbar.value() == after); // a move without the button does not drag

    // The wheel: turning it away scrolls to the start, three steps for a notch
    vbar.setValue(30);
    wheel(&vbar, QPoint(7, 50), -1);
    CHECK(vbar.value() == 33);
    wheel(&vbar, QPoint(7, 50), 2);
    CHECK(vbar.value() == 27);
    vbar.setSingleStep(5);
    wheel(&vbar, QPoint(7, 50), 1);
    CHECK(vbar.value() == 17); // a notch is at most a page: the page is 10
    wheel(&vbar, QPoint(7, 50), 20);
    CHECK(vbar.value() == 0);

    // Orientation
    hbar.setRange(0, 90);
    hbar.setPageStep(30);
    hbar.setValue(0);
    CHECK(handle(hbar).left() == 0 && handle(hbar).width() > 30);
    hbar.setOrientation(Qt::Vertical);
    CHECK(hbar.orientation() == Qt::Vertical && hbar.sizePolicy().verticalPolicy() == QSizePolicy::Expanding);

    // A bar is drawn: the handle differs from the groove
    QImage image = grab(&top);
    const QRect hv = handle(vbar);
    const QPoint base = inWindow(&vbar);
    CHECK(image.pixel(base + hv.center()) != image.pixel(base + QPoint(7, 95)));
}

static void testScrollArea() {
    QWidget top;
    top.resize(260, 200);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    Area area(&top);
    layout->addWidget(&area);
    top.show();
    settle(&top);

    const int extent = area.style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, &area);
    CHECK(area.frameShape() == QFrame::StyledPanel && area.frameShadow() == QFrame::Sunken && area.frameWidth() == 1);
    CHECK(area.viewport() && area.viewport()->parentWidget() == &area);
    CHECK(area.horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded &&
          area.verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded);
    // The area is 260 by 200; inside the frame 258 by 198, less a scroll bar on the right and one at the bottom
    CHECK(area.size() == QSize(260, 200));
    CHECK(area.viewport()->geometry() == QRect(1, 1, 258 - extent, 198 - extent));
    CHECK(!area.horizontalScrollBar()->isHidden() && !area.verticalScrollBar()->isHidden());
    CHECK(area.verticalScrollBar()->geometry() == QRect(259 - extent, 1, extent, 198 - extent));
    CHECK(area.horizontalScrollBar()->geometry() == QRect(1, 199 - extent, 258 - extent, extent));
    CHECK(area.verticalScrollBar()->maximum() == 800 - (198 - extent));
    CHECK(area.horizontalScrollBar()->maximum() == 1000 - (258 - extent));
    CHECK(area.verticalScrollBar()->pageStep() == 198 - extent);
    CHECK(area.lastResize == area.viewport()->size());

    // The viewport is painted by the area
    QImage image = grab(&top);
    const QPoint v0 = inWindow(&area.viewport()[0]);
    CHECK(image.pixel(v0 + QPoint(5, 5)) == area.fill.rgb());
    CHECK(image.pixel(v0 + QPoint(area.viewport()->width() - 1, area.viewport()->height() - 1)) == area.fill.rgb());
    CHECK(area.paints >= 1);

    // Scroll bars follow the range
    area.contents = QSize(100, 800);
    area.updateRanges();
    CHECK(area.horizontalScrollBar()->isHidden() && !area.verticalScrollBar()->isHidden());
    CHECK(area.viewport()->width() == 258 - extent && area.viewport()->height() == 198);
    CHECK(area.verticalScrollBar()->geometry() == QRect(259 - extent, 1, extent, 198));
    area.contents = QSize(100, 100);
    area.updateRanges();
    CHECK(area.horizontalScrollBar()->isHidden() && area.verticalScrollBar()->isHidden());
    CHECK(area.viewport()->geometry() == QRect(1, 1, 258, 198));
    // A scroll bar that appears makes the other one necessary: the viewport is shorter, and 190 is not enough
    area.contents = QSize(250, 190);
    area.updateRanges();
    CHECK(area.horizontalScrollBar()->isHidden() && area.verticalScrollBar()->isHidden());
    area.contents = QSize(258, 190);
    area.updateRanges();
    CHECK(area.horizontalScrollBar()->isHidden() && area.verticalScrollBar()->isHidden());
    area.contents = QSize(259, 190);
    area.updateRanges();
    CHECK(!area.horizontalScrollBar()->isHidden());
    CHECK(area.verticalScrollBar()->isHidden() == (190 <= 198 - extent));

    // Policies
    area.contents = QSize(100, 100);
    area.updateRanges();
    area.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    CHECK(area.verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn && !area.verticalScrollBar()->isHidden());
    CHECK(area.viewport()->width() == 258 - extent && area.viewport()->height() == 198);
    area.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    CHECK(!area.horizontalScrollBar()->isHidden() && area.viewport()->height() == 198 - extent);
    area.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area.contents = QSize(1000, 800);
    area.updateRanges();
    CHECK(area.horizontalScrollBar()->isHidden() && area.verticalScrollBar()->isHidden());
    CHECK(area.viewport()->geometry() == QRect(1, 1, 258, 198));
    CHECK(area.verticalScrollBar()->maximum() == 800 - 198);
    // A bar that is off can still be moved from the code
    area.verticalScrollBar()->setValue(10);
    CHECK(area.scrolled == QPoint(0, -10));

    // Margins leave room for the widgets of a subclass
    area.setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area.setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area.setViewportMargins(20, 10, 5, 3);
    CHECK(area.viewportMargins() == QMargins(20, 10, 5, 3));
    CHECK(area.viewport()->geometry() == QRect(1 + 20, 1 + 10, 258 - extent - 25, 198 - extent - 13));
    area.setViewportMargins(QMargins());
    CHECK(area.viewport()->geometry() == QRect(1, 1, 258 - extent, 198 - extent));
    area.contents = QSize(1000, 800);
    area.updateRanges();

    // Mouse events of the viewport have positions in the viewport, and focus the area
    area.verticalScrollBar()->setValue(0);
    area.horizontalScrollBar()->setValue(0);
    area.presses = 0;
    click(area.viewport(), QPoint(30, 40));
    CHECK(area.presses == 1 && area.lastPress == QPoint(30, 40));
    CHECK(area.hasFocus());

    // The wheel scrolls, three steps for a notch; keys scroll when the area has the focus
    area.verticalScrollBar()->setSingleStep(10);
    wheel(area.viewport(), QPoint(30, 40), -1);
    CHECK(area.verticalScrollBar()->value() == 30);
    wheel(area.viewport(), QPoint(30, 40), 1);
    CHECK(area.verticalScrollBar()->value() == 0);
    key(&area, Qt::Key_Down);
    CHECK(area.verticalScrollBar()->value() == 10);
    key(&area, Qt::Key_Up);
    CHECK(area.verticalScrollBar()->value() == 0);
    key(&area, Qt::Key_PageDown);
    CHECK(area.verticalScrollBar()->value() == area.verticalScrollBar()->pageStep());
    key(&area, Qt::Key_PageUp);
    CHECK(area.verticalScrollBar()->value() == 0);
    key(&area, Qt::Key_End);
    CHECK(area.verticalScrollBar()->value() == area.verticalScrollBar()->maximum());
    key(&area, Qt::Key_Home);
    CHECK(area.verticalScrollBar()->value() == 0);
    area.horizontalScrollBar()->setSingleStep(7);
    key(&area, Qt::Key_Right);
    CHECK(area.horizontalScrollBar()->value() == 7);
    key(&area, Qt::Key_Left);
    CHECK(area.horizontalScrollBar()->value() == 0);

    // Scrolling tells the subclass how far the contents moved
    area.scrolled = QPoint();
    area.horizontalScrollBar()->setValue(25);
    area.verticalScrollBar()->setValue(40);
    CHECK(area.scrolled == QPoint(-25, -40));
    area.verticalScrollBar()->setValue(30);
    CHECK(area.scrolled == QPoint(-25, -30));

    // Drag the handle of the scroll bar of the area
    area.verticalScrollBar()->setValue(0);
    QScrollBar *bar = area.verticalScrollBar();
    press(bar, QPoint(bar->width() / 2, 10));
    drag(bar, QPoint(bar->width() / 2, 60));
    release(bar, QPoint(bar->width() / 2, 60));
    CHECK(bar->value() > 100);

    // The size hint adds the frame and the margins to the viewport's
    Area hinted;
    CHECK(hinted.sizeHint() == QSize(256 + 2, 192 + 2));
    hinted.setViewportMargins(5, 6, 7, 8);
    CHECK(hinted.sizeHint() == QSize(256 + 2 + 12, 192 + 2 + 14));
    hinted.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    CHECK(hinted.sizeHint().width() == 256 + 2 + 12 + extent);
    CHECK(hinted.minimumSizeHint().width() > 0 && hinted.minimumSizeHint().height() > 0);
    hinted.setFrameStyle(QFrame::NoFrame);
    CHECK(hinted.sizeHint().width() == 256 + 12 + extent);

    // A disabled area gives its scroll bars no events
    area.setEnabled(false);
    area.verticalScrollBar()->setValue(0);
    wheel(area.viewport(), QPoint(30, 40), -1);
    CHECK(area.verticalScrollBar()->value() == 0);
    area.setEnabled(true);
}

static void testScrollAreaWidget() {
    QWidget top;
    top.resize(240, 180);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    QScrollArea area(&top);
    layout->addWidget(&area);
    top.show();
    settle(&top);
    CHECK(area.widget() == nullptr && !area.widgetResizable());
    CHECK(area.verticalScrollBar()->singleStep() == 20 && area.horizontalScrollBar()->singleStep() == 20);
    CHECK(area.sizeHint().width() > 20);
    const int extent = area.style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, &area);

    // A widget keeps its size, and is moved in the viewport
    auto *page = new QWidget;
    page->resize(400, 300);
    area.setWidget(page);
    CHECK(area.widget() == page && page->parentWidget() == area.viewport() && !page->isHidden());
    CHECK(page->size() == QSize(400, 300) && page->pos() == QPoint(0, 0));
    const QSize view = area.viewport()->size();
    CHECK(view == QSize(238 - extent, 178 - extent));
    CHECK(area.horizontalScrollBar()->maximum() == 400 - view.width());
    CHECK(area.verticalScrollBar()->maximum() == 300 - view.height());
    CHECK(area.verticalScrollBar()->pageStep() == view.height());
    area.verticalScrollBar()->setValue(50);
    area.horizontalScrollBar()->setValue(30);
    CHECK(page->pos() == QPoint(-30, -50));
    wheel(area.viewport(), QPoint(10, 10), -1);
    CHECK(area.verticalScrollBar()->value() == 110 || area.verticalScrollBar()->value() == area.verticalScrollBar()->maximum());
    CHECK(page->pos().y() == -area.verticalScrollBar()->value());

    // The scroll bars follow the widget when it is resized
    page->resize(200, 100);
    CHECK(area.horizontalScrollBar()->isHidden() && area.verticalScrollBar()->isHidden());
    CHECK(page->pos() == QPoint(0, 0));
    page->resize(600, 100);
    CHECK(!area.horizontalScrollBar()->isHidden() && area.verticalScrollBar()->isHidden());
    CHECK(area.horizontalScrollBar()->maximum() == 600 - area.viewport()->width());
    page->resize(400, 300);

    // ensureVisible() scrolls just enough, with a margin
    area.horizontalScrollBar()->setValue(0);
    area.verticalScrollBar()->setValue(0);
    area.ensureVisible(10, 10, 5, 5);
    CHECK(area.horizontalScrollBar()->value() == 0 && area.verticalScrollBar()->value() == 0);
    area.ensureVisible(390, 290, 10, 10);
    CHECK(area.horizontalScrollBar()->value() == 400 - area.viewport()->width());
    CHECK(area.verticalScrollBar()->value() == 300 - area.viewport()->height());
    area.ensureVisible(5, 5, 20, 20);
    CHECK(area.horizontalScrollBar()->value() == 0 && area.verticalScrollBar()->value() == 0);

    auto *far = new QPushButton("Far", page);
    far->setGeometry(300, 250, 60, 30);
    area.ensureWidgetVisible(far, 5, 5);
    CHECK(area.verticalScrollBar()->value() > 0 && area.horizontalScrollBar()->value() > 0);
    const QRect seen(area.horizontalScrollBar()->value(), area.verticalScrollBar()->value(), area.viewport()->width(),
                     area.viewport()->height());
    CHECK(seen.contains(far->geometry().center()));
    QPushButton elsewhere("Not in the page", &top);
    const int before = area.verticalScrollBar()->value();
    area.ensureWidgetVisible(&elsewhere);
    area.ensureWidgetVisible(nullptr);
    CHECK(area.verticalScrollBar()->value() == before);

    // takeWidget() gives the widget back
    QWidget *taken = area.takeWidget();
    CHECK(taken == page && page->parentWidget() == nullptr && area.widget() == nullptr);
    CHECK(area.horizontalScrollBar()->isHidden() && area.verticalScrollBar()->isHidden());
    CHECK(area.takeWidget() == nullptr);
    delete taken;

    // A widget with a layout, resizable: it takes the width of the viewport and the height of its layout
    auto *content = new QWidget;
    auto *column = new QVBoxLayout(content);
    std::vector<QLabel *> labels;
    for (int i = 0; i < 12; ++i) {
        auto *label = new QLabel(QString("Line %1").arg(i), content);
        column->addWidget(label);
        labels.push_back(label);
    }
    area.setWidget(content);
    area.setWidgetResizable(true);
    CHECK(area.widgetResizable());
    settle(&top);
    CHECK(content->width() == area.viewport()->width());
    const int least = content->minimumSizeHint().height();
    CHECK(least > 100 && content->height() == std::max(least, area.viewport()->height()));
    CHECK(!area.verticalScrollBar()->isHidden() && area.horizontalScrollBar()->isHidden());
    CHECK(area.verticalScrollBar()->maximum() == content->height() - area.viewport()->height());
    // More room for the area: no scroll bar, and the widget fills the viewport
    top.resize(240, least + 40);
    settle(&top);
    CHECK(area.verticalScrollBar()->isHidden() && content->size() == area.viewport()->size());
    CHECK(content->pos() == QPoint(0, 0));
    top.resize(240, 180);
    settle(&top);
    CHECK(!area.verticalScrollBar()->isHidden());
    // A layout that needs more room is followed, and so is a label with a longer text
    labels[0]->setText("A much longer line of text that does not fit in so small an area");
    QCoreApplication::sendPostedEvents(content);
    settle(&top);
    CHECK(content->width() >= content->minimumSizeHint().width());
    CHECK(content->width() == std::max(area.viewport()->width(), content->minimumSizeHint().width()));
    CHECK(area.sizeHint().width() > 2);
    area.setWidgetResizable(false);
    CHECK(!area.widgetResizable());

    // Replacing the widget deletes the old one
    QPointer<QWidget> old = content;
    area.setWidget(new QWidget);
    CHECK(old.isNull() && area.widget() != nullptr);
    // Never sized: the size hint is used
    auto *sized = new QWidget;
    auto *l2 = new QVBoxLayout(sized);
    l2->addWidget(new QLabel("Sized by its layout", sized));
    area.setWidget(sized);
    CHECK(sized->size() == sized->sizeHint());
    area.setWidget(sized);
    CHECK(area.widget() == sized);
    area.setWidget(nullptr);
    CHECK(area.widget() == nullptr);
}


static void testHeaderView() {
    QWidget top;
    top.resize(300, 120);
    QTableWidget table(4, 3, &top);
    table.setGeometry(0, 0, 300, 120);
    QHeaderView *h = table.horizontalHeader();
    QHeaderView *v = table.verticalHeader();
    CHECK(h->orientation() == Qt::Horizontal && v->orientation() == Qt::Vertical);
    CHECK(h->count() == 3 && v->count() == 4);
    CHECK(h->defaultSectionSize() == 100 && v->defaultSectionSize() == 30);
    CHECK(h->sectionSize(0) == 100 && v->sectionSize(3) == 30 && h->sectionSize(7) == 0 && h->sectionSize(-1) == 0);
    CHECK(h->length() == 300 && v->length() == 120);
    CHECK(h->sectionPosition(0) == 0 && h->sectionPosition(2) == 200 && h->sectionPosition(3) == 300);
    CHECK(h->logicalIndexAt(0) == 0 && h->logicalIndexAt(99) == 0 && h->logicalIndexAt(100) == 1 &&
          h->logicalIndexAt(299) == 2 && h->logicalIndexAt(300) == -1 && h->logicalIndexAt(-1) == -1);
    CHECK(h->minimumSectionSize() == 20 && !h->stretchLastSection() && h->offset() == 0);
    CHECK(h->sectionsClickable() && v->sectionsClickable());

    std::vector<int> resized;
    QObject::connect(h, &QHeaderView::sectionResized, [&](int index, int oldSize, int newSize) {
        resized.push_back(index);
        resized.push_back(oldSize);
        resized.push_back(newSize);
    });
    h->resizeSection(1, 150);
    CHECK(h->sectionSize(1) == 150 && h->length() == 350 && resized == (std::vector<int>{1, 100, 150}));
    h->resizeSection(1, 150);
    CHECK(resized.size() == 3);
    h->resizeSection(1, 5);
    CHECK(h->sectionSize(1) == 20); // not below the minimum
    h->setMinimumSectionSize(8);
    h->resizeSection(1, 5);
    CHECK(h->sectionSize(1) == 8 && h->minimumSectionSize() == 8);
    h->resizeSection(9, 50);
    CHECK(h->count() == 3);
    h->setDefaultSectionSize(60);
    CHECK(h->defaultSectionSize() == 60 && h->sectionSize(0) == 100); // sections that exist keep their size
    h->resizeSection(1, 100);

    // Offset: positions are in the widget, so scrolling moves them
    h->setOffset(150);
    CHECK(h->offset() == 150 && h->logicalIndexAt(0) == 1 && h->logicalIndexAt(49) == 1 && h->logicalIndexAt(50) == 2);
    h->setOffset(0);

    // The last section can take what is left
    top.show();
    settle(&top);
    const int room = h->width();
    CHECK(room > 0);
    h->resizeSection(0, 20);
    h->resizeSection(1, 20);
    h->resizeSection(2, 20);
    h->setStretchLastSection(true);
    CHECK(h->stretchLastSection() && h->sectionSize(2) == room - 40 && h->length() == room);
    CHECK(h->sectionSize(0) == 20);
    h->setStretchLastSection(false);
    CHECK(h->sectionSize(2) == 20);
    h->resizeSection(0, 100);
    h->resizeSection(1, 100);
    h->resizeSection(2, 100);

    // Labels: given by the table; the numbers are the default
    QImage image = grab(&top);
    CHECK(image.width() >= 300);
    table.setHorizontalHeaderLabels({"Name", "Role"});
    CHECK(table.columnCount() == 3 && h->count() == 3);
    table.setHorizontalHeaderLabels({"A", "B", "C", "D"});
    CHECK(table.columnCount() == 4 && h->count() == 4);
    CHECK(h->sizeHint().height() > table.fontMetrics().height() && h->sizeHint().width() == h->length());
    CHECK(v->sizeHint().width() > 8 && v->sizeHint().height() == v->length());
    table.setVerticalHeaderLabels({"First row, with a long label"});
    CHECK(v->sizeHint().width() > table.fontMetrics().horizontalAdvance("First row"));

    // The user drags the edge of a section to resize it
    settle(&top);
    table.setVerticalHeaderLabels({"1", "2", "3", "4"});
    table.setColumnWidth(0, 100);
    const int edge = h->sectionSize(0);
    resized.clear();
    press(h, QPoint(edge, 5));
    drag(h, QPoint(edge + 30, 5));
    CHECK(h->sectionSize(0) == 130);
    drag(h, QPoint(edge - 40, 5));
    CHECK(h->sectionSize(0) == 60);
    drag(h, QPoint(0, 5));
    CHECK(h->sectionSize(0) == 8); // the minimum
    release(h, QPoint(0, 5));
    CHECK(h->sectionSize(0) == 8);
    h->resizeSection(0, 100);
    // A vertical header resizes rows
    const int rowEdge = v->sectionSize(0);
    press(v, QPoint(5, rowEdge));
    drag(v, QPoint(5, rowEdge + 12));
    release(v, QPoint(5, rowEdge + 12));
    CHECK(v->sectionSize(0) == 42 && table.rowHeight(0) == 42);
    table.setRowHeight(0, 30);

    // A click in a section, with the button released in the same one, is a click
    std::vector<int> clicked;
    QObject::connect(h, &QHeaderView::sectionClicked, [&](int index) { clicked.push_back(index); });
    click(h, QPoint(150, 5));
    CHECK(clicked == std::vector<int>{1});
    press(h, QPoint(150, 5));
    release(h, QPoint(250, 5));
    CHECK(clicked.size() == 1);
    // Away from an edge, so that it does not resize
    h->setSectionsClickable(false);
    click(h, QPoint(150, 5));
    CHECK(clicked.size() == 1);
    h->setSectionsClickable(true);

    // A header is drawn: the pressed section is darker than the others
    image = grab(&top);
    const QPoint base = inWindow(h);
    const QRgb plain = image.pixel(base + QPoint(150, 3));
    press(h, QPoint(150, 3));
    image = grab(&top);
    CHECK(image.pixel(base + QPoint(150, 3)) != plain);
    release(h, QPoint(150, 3));
    image = grab(&top);
    CHECK(image.pixel(base + QPoint(150, 3)) == plain);
    // The stretched end has a header look too, after the last section
    table.setColumnCount(1);
    settle(&top);
    image = grab(&top);
    CHECK(image.pixel(base + QPoint(h->width() - 5, 3)) == plain);
}

static void testTableWidget() {
    QWidget top;
    top.resize(330, 220);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    QTableWidget table(20, 3, &top);
    layout->addWidget(&table);
    top.show();
    settle(&top);

    CHECK(table.rowCount() == 20 && table.columnCount() == 3 && table.currentRow() == -1 && table.currentColumn() == -1);
    CHECK(table.currentItem() == nullptr && table.selectedItems().isEmpty());
    CHECK(table.showGrid() && !table.alternatingRowColors() && table.focusPolicy() == Qt::StrongFocus);
    CHECK(table.rowHeight(0) == 30 && table.columnWidth(0) == 100 && table.item(0, 0) == nullptr &&
          table.item(30, 0) == nullptr && table.item(-1, 0) == nullptr);
    CHECK(table.frameShape() == QFrame::StyledPanel);

    // The headers are around the viewport, which holds the cells
    QHeaderView *h = table.horizontalHeader(), *v = table.verticalHeader();
    QWidget *view = table.viewport();
    CHECK(!h->isHidden() && !v->isHidden());
    CHECK(view->x() == 1 + v->width() && view->y() == 1 + h->height());
    CHECK(h->geometry() == QRect(view->x(), 1, view->width(), h->height()));
    CHECK(v->geometry() == QRect(1, view->y(), v->width(), view->height()));
    CHECK(h->height() == h->sizeHint().height() && v->width() == v->sizeHint().width());
    // 20 rows of 30 pixels and 3 columns of 100: the rows do not fit, the columns may
    CHECK(!table.verticalScrollBar()->isHidden());
    CHECK(table.verticalScrollBar()->maximum() == 600 - view->height());
    CHECK(table.horizontalScrollBar()->maximum() == std::max(0, 300 - view->width()));
    CHECK(table.verticalScrollBar()->pageStep() == view->height() && table.verticalScrollBar()->singleStep() == 30);
    CHECK(table.rowAt(0) == 0 && table.rowAt(29) == 0 && table.rowAt(30) == 1 && table.columnAt(0) == 0 &&
          table.columnAt(100) == 1 && table.columnAt(299) == 2 && table.rowAt(-1) == -1);

    // Items
    auto *name = new QTableWidgetItem("Ada");
    table.setItem(1, 0, name);
    CHECK(table.item(1, 0) == name && name->tableWidget() == &table && name->row() == 1 && name->column() == 0);
    CHECK(name->text() == "Ada" && name->flags() == (Qt::ItemIsSelectable | Qt::ItemIsEnabled) &&
          name->textAlignment() == (Qt::AlignLeft | Qt::AlignVCenter) && name->toolTip().isEmpty());
    name->setText("Ada Lovelace");
    CHECK(name->text() == "Ada Lovelace");
    table.setItem(1, 1, new QTableWidgetItem("Engineer"));
    table.setItem(2, 2, new QTableWidgetItem("x"));
    CHECK(table.itemAt(QPoint(10, 40)) == name && table.itemAt(110, 40) == table.item(1, 1) &&
          table.itemAt(10, 10) == nullptr && table.itemAt(1000, 10) == nullptr);
    CHECK(table.visualItemRect(name) == QRect(0, 30, 100, 30));
    QTableWidgetItem stray;
    CHECK(table.visualItemRect(&stray).isNull() && table.visualItemRect(nullptr).isNull());
    CHECK(stray.tableWidget() == nullptr && stray.row() == -1);
    // A new item in the cell replaces the old one
    table.setItem(2, 2, new QTableWidgetItem("y"));
    CHECK(table.item(2, 2)->text() == "y");
    // takeItem hands the item back; deleting an item takes it out
    QTableWidgetItem *taken = table.takeItem(2, 2);
    CHECK(taken && taken->text() == "y" && table.item(2, 2) == nullptr && taken->tableWidget() == nullptr);
    delete taken;
    CHECK(table.takeItem(2, 2) == nullptr);
    auto *gone = new QTableWidgetItem("gone");
    table.setItem(3, 0, gone);
    delete gone;
    CHECK(table.item(3, 0) == nullptr);
    // An item moves from a table to a cell of another, and within one
    auto *mover = new QTableWidgetItem("mover");
    table.setItem(4, 0, mover);
    table.setItem(4, 1, mover);
    CHECK(table.item(4, 0) == nullptr && table.item(4, 1) == mover && mover->column() == 1);
    table.setItem(40, 0, new QTableWidgetItem("outside")); // there is no such cell: it is deleted

    // Painting: background, alternating colors, grid, selected cell
    table.verticalScrollBar()->setValue(0);
    const QPalette pal = table.palette();
    const QRgb baseColor = pal.color(QPalette::Base).rgb();
    const QPoint origin = inWindow(view);
    QImage image = grab(&top);
    CHECK(image.pixel(origin + QPoint(5, 5)) == baseColor && image.pixel(origin + QPoint(5, 35)) == baseColor);
    const QRgb lineColor = QColor(baseColor).darker(125).rgb();
    CHECK(image.pixel(origin + QPoint(99, 10)) == lineColor && image.pixel(origin + QPoint(10, 29)) == lineColor);
    CHECK(image.pixel(origin + QPoint(199, 10)) == lineColor && image.pixel(origin + QPoint(10, 59)) == lineColor);
    table.setShowGrid(false);
    image = grab(&top);
    CHECK(!table.showGrid() && image.pixel(origin + QPoint(99, 10)) == baseColor &&
          image.pixel(origin + QPoint(10, 29)) == baseColor);
    table.setShowGrid(true);
    table.setAlternatingRowColors(true);
    image = grab(&top);
    const QRgb alternate = pal.color(QPalette::AlternateBase).rgb();
    CHECK(table.alternatingRowColors() && alternate != baseColor);
    CHECK(image.pixel(origin + QPoint(5, 5)) == baseColor && image.pixel(origin + QPoint(5, 35)) == alternate &&
          image.pixel(origin + QPoint(205, 65)) == baseColor && image.pixel(origin + QPoint(205, 95)) == alternate);
    table.setAlternatingRowColors(false);
    name->setBackground(QBrush(QColor(250, 200, 60)));
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(3, 33)) == QColor(250, 200, 60).rgb() &&
          image.pixel(origin + QPoint(103, 33)) == baseColor);
    name->setBackground(QBrush());

    // Clicking selects the cell, and says so
    std::vector<int> current;
    int selections = 0, clicks = 0, itemClicks = 0, doubles = 0;
    QObject::connect(&table, &QTableWidget::currentCellChanged,
                     [&](int r, int c, int pr, int pc) { current.insert(current.end(), {r, c, pr, pc}); });
    QObject::connect(&table, &QTableWidget::itemSelectionChanged, [&] { ++selections; });
    QObject::connect(&table, &QTableWidget::cellClicked, [&](int, int) { ++clicks; });
    QObject::connect(&table, &QTableWidget::itemClicked, [&](QTableWidgetItem *) { ++itemClicks; });
    QObject::connect(&table, &QTableWidget::cellDoubleClicked, [&](int, int) { ++doubles; });
    click(view, QPoint(110, 40));
    CHECK(table.currentRow() == 1 && table.currentColumn() == 1 && table.hasFocus());
    CHECK((current == std::vector<int>{1, 1, -1, -1}) && selections == 1 && clicks == 1 && itemClicks == 1);
    CHECK(table.currentItem() == table.item(1, 1) && table.item(1, 1)->isSelected() && !name->isSelected());
    CHECK(table.selectedItems().size() == 1 && table.selectedItems().first() == table.item(1, 1));
    image = grab(&top);
    const QRgb highlight = pal.color(QPalette::Highlight).rgb();
    CHECK(image.pixel(origin + QPoint(103, 33)) == highlight && image.pixel(origin + QPoint(3, 33)) != highlight);
    // An empty cell is selected too, and a click on it is no item click
    click(view, QPoint(210, 100));
    CHECK(table.currentRow() == 3 && table.currentColumn() == 2 && table.currentItem() == nullptr);
    CHECK(clicks == 2 && itemClicks == 1 && table.selectedItems().isEmpty());
    CHECK(current.size() == 8 && current[4] == 3 && current[5] == 2 && current[6] == 1 && current[7] == 1);
    click(view, QPoint(210, 100));
    CHECK(selections == 2 && clicks == 3); // the same cell: not a new selection
    // The selection mode: the same, as in Qt, with one selected cell at most
    CHECK(table.selectionMode() == QAbstractItemView::ExtendedSelection);
    table.setCurrentCell(1, 1);
    CHECK(table.selectedItems().size() == 1);
    const int changes = selections;
    table.setSelectionMode(QAbstractItemView::NoSelection);
    CHECK(selections == changes + 1 && table.selectedItems().isEmpty() && !table.item(1, 1)->isSelected() &&
          table.currentRow() == 1);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(103, 33)) != highlight);
    table.setSelectionMode(QAbstractItemView::SingleSelection);
    CHECK(table.selectedItems().size() == 1 && table.item(1, 1)->isSelected());
    table.setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Press in one cell, release in another: no click
    press(view, QPoint(10, 40));
    release(view, QPoint(110, 40));
    CHECK(clicks == 3);
    release(view, QPoint(110, 40));
    // Double click
    QWindow *win = top.windowHandle();
    const QPoint dbl = inWindow(view, QPoint(10, 10));
    QWindowSystemInterface::handleMouseEvent(win, QPointF(dbl), QPointF(dbl), Qt::LeftButton, Qt::LeftButton,
                                             QEvent::MouseButtonDblClick, Qt::NoModifier);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(dbl), QPointF(dbl), Qt::NoButton, Qt::LeftButton,
                                             QEvent::MouseButtonRelease, Qt::NoModifier);
    CHECK(doubles == 1 && table.currentRow() == 0 && table.currentColumn() == 0);
    // Click outside the cells
    table.setRowCount(2);
    settle(&top);
    const int selBefore = selections;
    click(view, QPoint(10, view->height() - 5));
    CHECK(table.currentRow() == 0 && selections == selBefore);
    table.setRowCount(20);

    // Keys
    table.setCurrentCell(0, 0);
    key(&table, Qt::Key_Right);
    CHECK(table.currentRow() == 0 && table.currentColumn() == 1);
    key(&table, Qt::Key_Down);
    CHECK(table.currentRow() == 1 && table.currentColumn() == 1);
    key(&table, Qt::Key_Left);
    key(&table, Qt::Key_Left);
    CHECK(table.currentColumn() == 0); // the edge stays
    key(&table, Qt::Key_Up);
    key(&table, Qt::Key_Up);
    CHECK(table.currentRow() == 0);
    key(&table, Qt::Key_End);
    CHECK(table.currentRow() == 0 && table.currentColumn() == 2);
    key(&table, Qt::Key_Home);
    CHECK(table.currentColumn() == 0);
    key(&table, Qt::Key_End, QString(), Qt::ControlModifier);
    CHECK(table.currentRow() == 19 && table.currentColumn() == 2);
    key(&table, Qt::Key_Home, QString(), Qt::ControlModifier);
    CHECK(table.currentRow() == 0 && table.currentColumn() == 0);
    key(&table, Qt::Key_PageDown);
    const int page = table.currentRow();
    CHECK(page > 1 && page < 10);
    key(&table, Qt::Key_PageUp);
    CHECK(table.currentRow() == 0);
    key(&table, Qt::Key_A, "a");
    CHECK(table.currentRow() == 0);
    // Without a current cell, the first key selects the first cell
    table.setCurrentCell(5, 1);
    table.clearContents();
    CHECK(table.currentRow() == -1);
    key(&table, Qt::Key_Down);
    CHECK(table.currentRow() == 0 && table.currentColumn() == 0);

    // Moving the current cell scrolls it into view, and the headers follow
    table.setCurrentCell(19, 2);
    QScrollBar *vbar = table.verticalScrollBar();
    CHECK(vbar->value() == vbar->maximum() && v->offset() == vbar->value());
    CHECK(table.visualItemRect(table.item(19, 0)).isNull());
    table.setItem(19, 0, new QTableWidgetItem("last"));
    CHECK(table.visualItemRect(table.item(19, 0)).bottom() == view->height() - 1);
    table.setCurrentCell(0, 0);
    CHECK(vbar->value() == 0 && v->offset() == 0);
    table.scrollToItem(table.item(19, 0));
    CHECK(vbar->value() == vbar->maximum());
    table.scrollToItem(nullptr);
    table.scrollToItem(&stray);
    vbar->setValue(45);
    CHECK(table.rowAt(0) == 1 && table.rowAt(14) == 1 && table.rowAt(15) == 2 && table.itemAt(10, 20) == nullptr);
    CHECK(h->offset() == 0 && v->offset() == 45 && v->logicalIndexAt(0) == 1);
    vbar->setValue(0);
    // The wheel: three rows a notch
    wheel(view, QPoint(10, 10), -1);
    CHECK(vbar->value() == 90 && v->offset() == 90);
    wheel(view, QPoint(10, 10), 1);
    CHECK(vbar->value() == 0);

    // Sizes: a wider column needs the horizontal bar, and the headers keep up
    table.setColumnWidth(0, 400);
    CHECK(table.columnWidth(0) == 400 && !table.horizontalScrollBar()->isHidden());
    CHECK(table.horizontalScrollBar()->maximum() == 600 - view->width());
    CHECK(table.columnAt(350) == 0 && table.columnAt(450) == 1);
    table.setCurrentCell(0, 2);
    CHECK(table.horizontalScrollBar()->value() > 0 && h->offset() == table.horizontalScrollBar()->value());
    const QRect seen = table.visualItemRect(table.item(0, 0));
    CHECK(seen.isNull() || seen.right() < view->width());
    table.setColumnWidth(0, 100);
    table.setRowHeight(0, 60);
    CHECK(table.rowHeight(0) == 60 && table.rowAt(59) == 0 && table.rowAt(60) == 1);
    table.setRowHeight(0, 30);
    table.horizontalScrollBar()->setValue(0);

    // The widest text makes the column
    table.setItem(0, 0, new QTableWidgetItem("A rather long text for a narrow column"));
    table.setHorizontalHeaderLabels({"Name", "Role", "Notes"});
    table.resizeColumnToContents(0);
    CHECK(table.columnWidth(0) >= table.fontMetrics().horizontalAdvance("A rather long text for a narrow column") + 6);
    table.resizeColumnsToContents();
    CHECK(table.columnWidth(2) >= table.fontMetrics().horizontalAdvance("Notes"));
    table.resizeColumnToContents(99);
    table.setColumnWidth(0, 100);
    table.setColumnWidth(1, 100);
    table.setColumnWidth(2, 100);
    table.setItem(0, 0, nullptr);
    CHECK(table.item(0, 0) == nullptr);

    // Items that cannot be selected are skipped, not entered
    auto *locked = new QTableWidgetItem("locked");
    locked->setFlags(Qt::ItemIsEnabled);
    table.setItem(5, 1, locked);
    table.setCurrentCell(5, 0);
    key(&table, Qt::Key_Right);
    CHECK(table.currentRow() == 5 && table.currentColumn() == 2);
    table.setCurrentCell(5, 1);
    CHECK(table.currentColumn() == 2);
    click(view, QPoint(110, 5 * 30 + 10 - vbar->value()));
    CHECK(table.currentColumn() == 2);
    locked->setSelected(true);
    CHECK(!locked->isSelected());
    auto *open = new QTableWidgetItem("open");
    table.setItem(6, 0, open);
    open->setSelected(true);
    CHECK(open->isSelected() && table.currentRow() == 6);
    open->setSelected(false);
    CHECK(!open->isSelected() && table.currentRow() == -1);
    table.setCurrentItem(open);
    CHECK(table.currentItem() == open);
    table.setCurrentItem(nullptr);
    table.setCurrentItem(&stray);
    CHECK(table.currentItem() == open);
    table.setCurrentCell(99, 0);
    table.setCurrentCell(0, 99);
    CHECK(table.currentItem() == open);

    // Structure: rows and columns are inserted and removed with their items
    table.clear();
    table.setRowCount(3);
    table.setColumnCount(2);
    table.setHorizontalHeaderLabels({"X", "Y"});
    table.setVerticalHeaderLabels({"a", "b", "c"});
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 2; ++c)
            table.setItem(r, c, new QTableWidgetItem(QString("%1%2").arg(r).arg(c)));
    table.setCurrentCell(2, 1);
    table.insertRow(1);
    CHECK(table.rowCount() == 4 && table.item(1, 0) == nullptr && table.item(2, 0)->text() == "10" &&
          table.item(2, 0)->row() == 2 && table.currentRow() == 3 && table.currentItem()->text() == "21");
    CHECK(table.verticalHeader()->count() == 4);
    table.removeRow(0);
    CHECK(table.rowCount() == 3 && table.item(1, 0)->text() == "10" && table.currentRow() == 2);
    table.removeRow(9);
    CHECK(table.rowCount() == 3);
    table.insertColumn(0);
    CHECK(table.columnCount() == 3 && table.item(1, 1)->text() == "10" && table.item(1, 0) == nullptr &&
          table.currentColumn() == 2 && table.item(1, 1)->column() == 1);
    table.removeColumn(1);
    CHECK(table.columnCount() == 2 && table.item(1, 1)->text() == "11" && table.currentColumn() == 1);
    table.removeColumn(9);
    // Removing the current row or column leaves no current cell
    table.removeRow(2);
    CHECK(table.currentRow() == -1 && table.currentItem() == nullptr);
    table.setCurrentCell(0, 1);
    table.removeColumn(1);
    CHECK(table.currentColumn() == -1);
    table.setRowCount(1);
    CHECK(table.rowCount() == 1 && table.horizontalHeader()->count() == 1);
    table.setRowCount(0);
    table.setColumnCount(0);
    CHECK(table.rowCount() == 0 && table.columnCount() == 0);
    key(&table, Qt::Key_Down); // nothing to select
    CHECK(table.currentRow() == -1);
    table.insertRow(5);
    CHECK(table.rowCount() == 1);
    table.setRowCount(-4);
    CHECK(table.rowCount() == 0);

    // A disabled table draws in the disabled colors, and takes no keys
    table.setRowCount(3);
    table.setColumnCount(3);
    table.setItem(0, 0, new QTableWidgetItem("Disabled"));
    table.setEnabled(false);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(5, 5)) == table.palette().color(QPalette::Disabled, QPalette::Base).rgb());
    table.setEnabled(true);
}

static void testTableToolTips() {
    QtPrivate::toolTipDelay() = 15;
    QWidget top;
    top.resize(330, 200);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    QTableWidget table(4, 2, &top);
    table.setToolTip("The whole table");
    layout->addWidget(&table);
    auto *tipped = new QTableWidgetItem("Has a tool tip");
    tipped->setToolTip("About this cell");
    table.setItem(0, 0, tipped);
    table.setItem(1, 1, new QTableWidgetItem("None"));
    top.show();
    settle(&top);
    QWidget *view = table.viewport();

    hover(view, QPoint(20, 10));
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    CHECK(QToolTip::text() == QString("About this cell"));
    // Another cell without one shows the one of the table; an empty cell too
    hover(view, QPoint(120, 40));
    CHECK(waitFor([] { return QToolTip::text() == QString("The whole table"); }));
    hover(view, QPoint(20, 100));
    CHECK(waitFor([] { return QToolTip::text() == QString("The whole table"); }));
    hover(view, QPoint(20, 12));
    CHECK(waitFor([] { return QToolTip::text() == QString("About this cell"); }));
    // Without a tool tip on the table, nothing is shown over the cells that have none
    table.setToolTip(QString());
    hover(view, QPoint(120, 40));
    CHECK(!waitFor([] { return QToolTip::isVisible(); }, 100));
    QToolTip::hideText();
}

static void testListWidget() {
    std::vector<int> rowsSeen;
    int selections = 0, clicks = 0, doubles = 0, activations = 0, currentItems = 0;
    QStringList texts;
    QWidget top;
    top.resize(200, 120);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    QListWidget list(&top);
    layout->addWidget(&list);
    QObject::connect(&list, &QListWidget::currentRowChanged, [&](int r) { rowsSeen.push_back(r); });
    QObject::connect(&list, &QListWidget::currentTextChanged, [&](const QString &t) { texts.append(t); });
    QObject::connect(&list, &QListWidget::currentItemChanged,
                     [&](QListWidgetItem *, QListWidgetItem *) { ++currentItems; });
    QObject::connect(&list, &QListWidget::itemSelectionChanged, [&] { ++selections; });
    QObject::connect(&list, &QListWidget::itemClicked, [&](QListWidgetItem *) { ++clicks; });
    QObject::connect(&list, &QListWidget::itemDoubleClicked, [&](QListWidgetItem *) { ++doubles; });
    QObject::connect(&list, &QListWidget::itemActivated, [&](QListWidgetItem *) { ++activations; });
    top.show();
    settle(&top);

    CHECK(list.count() == 0 && list.currentRow() == -1 && list.currentItem() == nullptr &&
          list.selectedItems().isEmpty());
    CHECK(list.selectionMode() == QAbstractItemView::SingleSelection && !list.alternatingRowColors() &&
          list.focusPolicy() == Qt::StrongFocus && list.horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff);
    CHECK(list.item(0) == nullptr && list.takeItem(0) == nullptr && list.row(nullptr) == -1);
    const int h = list.rowHeight();
    CHECK(h == list.fontMetrics().height());

    // The first item becomes the current one, without being selected
    list.addItem("Alpha");
    CHECK(list.count() == 1 && list.currentRow() == 0 && list.currentItem()->text() == "Alpha" &&
          rowsSeen == std::vector<int>{0} && texts == QStringList{"Alpha"} && currentItems == 1 &&
          list.selectedItems().isEmpty());
    list.addItems({"Beta", "Gamma", "Delta"});
    list.addItem(new QListWidgetItem("Epsilon"));
    CHECK(list.count() == 5 && list.item(4)->text() == "Epsilon" && list.item(4)->listWidget() == &list &&
          list.row(list.item(3)) == 3 && rowsSeen.size() == 1);
    list.insertItem(1, "Inserted");
    list.insertItems(0, {"First", "Second"});
    CHECK(list.count() == 8 && list.item(0)->text() == "First" && list.item(1)->text() == "Second" &&
          list.item(2)->text() == "Alpha" && list.item(3)->text() == "Inserted");
    CHECK(list.currentRow() == 2 && list.currentItem()->text() == "Alpha"); // the current item stays where it moved
    list.insertItem(99, "Last");
    list.insertItem(-3, "Zero");
    CHECK(list.count() == 10 && list.item(0)->text() == "Zero" && list.item(9)->text() == "Last" &&
          list.currentRow() == 3);
    list.clear();
    CHECK(list.count() == 0 && list.currentRow() == -1 && rowsSeen.back() == -1 && texts.back().isEmpty());
    list.addItems({"Alpha", "Beta", "Gamma", "Delta", "Epsilon"});
    list.setCurrentRow(-1);
    list.clear();
    list.addItems({"Alpha", "Beta", "Gamma", "Delta", "Epsilon"});
    rowsSeen.clear();
    selections = clicks = 0;

    // Geometry
    QWidget *view = list.viewport();
    CHECK(list.visualItemRect(list.item(2)) == QRect(0, 2 * h, view->width(), h));
    QListWidgetItem stray;
    CHECK(list.visualItemRect(&stray).isNull() && list.visualItemRect(nullptr).isNull());
    CHECK(list.itemAt(QPoint(5, h + 1)) == list.item(1) && list.itemAt(5, 0) == list.item(0) &&
          list.itemAt(5, 5 * h) == nullptr && list.itemAt(-1, 5) == nullptr &&
          list.itemAt(view->width(), 5) == nullptr && list.itemAt(5, -1) == nullptr);
    CHECK(list.verticalScrollBar()->isHidden() && list.verticalScrollBar()->singleStep() == h);

    // Painting: rows, a selected row, alternating colors, a background
    const QPalette pal = list.palette();
    const QPoint origin = inWindow(view);
    QImage image = grab(&top);
    const QRgb base = pal.color(QPalette::Base).rgb();
    CHECK(image.pixel(origin + QPoint(view->width() - 3, 3)) == base);
    list.setAlternatingRowColors(true);
    image = grab(&top);
    CHECK(list.alternatingRowColors() &&
          image.pixel(origin + QPoint(view->width() - 3, h + 3)) == pal.color(QPalette::AlternateBase).rgb() &&
          image.pixel(origin + QPoint(view->width() - 3, 2 * h + 3)) == base);
    list.setAlternatingRowColors(false);
    list.item(3)->setBackground(QBrush(QColor(250, 200, 60)));
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(view->width() - 3, 3 * h + 3)) == QColor(250, 200, 60).rgb());
    list.item(3)->setBackground(QBrush());

    // Clicking selects the item, and says so
    click(view, QPoint(10, h + 3));
    CHECK(list.currentRow() == 1 && list.item(1)->isSelected() && list.selectedItems().size() == 1 && list.hasFocus());
    CHECK(rowsSeen == std::vector<int>{1} && selections == 1 && clicks == 1);
    image = grab(&top);
    const QRgb highlight = pal.color(QPalette::Highlight).rgb();
    CHECK(image.pixel(origin + QPoint(view->width() - 3, h + 3)) == highlight &&
          image.pixel(origin + QPoint(view->width() - 3, 3)) == base);
    click(view, QPoint(10, 3 * h + 3));
    CHECK(list.currentRow() == 3 && !list.item(1)->isSelected() && list.item(3)->isSelected() &&
          list.selectedItems().size() == 1 && selections == 2); // the item that goes and the new one are one change
    click(view, QPoint(10, 3 * h + 3));
    CHECK(selections == 2 && clicks == 3);
    // Press in one row, release in another: no click
    press(view, QPoint(10, 3));
    release(view, QPoint(10, h + 3));
    CHECK(clicks == 3);
    release(view, QPoint(10, 3));
    // Below the items nothing is selected
    click(view, QPoint(10, 5 * h + 2));
    CHECK(list.currentRow() == 0 || list.currentRow() == 3);
    // Double click activates
    QWindow *win = top.windowHandle();
    const QPoint dbl = inWindow(view, QPoint(10, 2 * h + 3));
    QWindowSystemInterface::handleMouseEvent(win, QPointF(dbl), QPointF(dbl), Qt::LeftButton, Qt::LeftButton,
                                             QEvent::MouseButtonDblClick, Qt::NoModifier);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(dbl), QPointF(dbl), Qt::NoButton, Qt::LeftButton,
                                             QEvent::MouseButtonRelease, Qt::NoModifier);
    CHECK(doubles == 1 && activations == 1 && list.currentRow() == 2);

    // Keys
    list.setCurrentRow(0);
    CHECK(list.currentRow() == 0 && list.item(0)->isSelected());
    key(&list, Qt::Key_Down);
    CHECK(list.currentRow() == 1 && list.item(1)->isSelected() && !list.item(0)->isSelected());
    key(&list, Qt::Key_Up);
    key(&list, Qt::Key_Up);
    CHECK(list.currentRow() == 0);
    key(&list, Qt::Key_End);
    CHECK(list.currentRow() == 4);
    key(&list, Qt::Key_Down);
    CHECK(list.currentRow() == 4);
    key(&list, Qt::Key_Home);
    CHECK(list.currentRow() == 0);
    key(&list, Qt::Key_PageDown);
    CHECK(list.currentRow() > 0 && list.currentRow() <= 4);
    key(&list, Qt::Key_PageUp);
    CHECK(list.currentRow() == 0);
    activations = 0;
    key(&list, Qt::Key_Return);
    key(&list, Qt::Key_Enter);
    CHECK(activations == 2);
    // A letter goes to the next item that starts with it, and wraps around
    key(&list, Qt::Key_G, "g");
    CHECK(list.currentRow() == 2 && list.item(2)->isSelected());
    key(&list, Qt::Key_D, "D");
    CHECK(list.currentRow() == 3);
    key(&list, Qt::Key_A, "a");
    CHECK(list.currentRow() == 0);
    key(&list, Qt::Key_Z, "z");
    CHECK(list.currentRow() == 0);
    key(&list, Qt::Key_F1);
    key(&list, Qt::Key_A, "a", Qt::ControlModifier);
    CHECK(list.selectedItems().size() == 1); // Control+A does not select all in this mode

    // Items that are not enabled or not selectable
    list.item(1)->setFlags(Qt::ItemIsSelectable); // not enabled
    list.item(2)->setFlags(Qt::ItemIsEnabled);    // not selectable
    key(&list, Qt::Key_Down);
    CHECK(list.currentRow() == 2 && !list.item(2)->isSelected()); // enabled: current, but not selected
    list.item(1)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    list.item(1)->setFlags(Qt::NoItemFlags);
    list.setCurrentRow(0);
    key(&list, Qt::Key_Down);
    CHECK(list.currentRow() == 2);
    key(&list, Qt::Key_Up);
    CHECK(list.currentRow() == 0);
    click(view, QPoint(10, h + 3));
    CHECK(list.currentRow() == 0);
    list.setCurrentRow(1);
    CHECK(list.currentRow() == 0);
    list.item(1)->setSelected(true);
    CHECK(!list.item(1)->isSelected());
    list.item(1)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    list.item(2)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    // Selection modes
    list.setSelectionMode(QAbstractItemView::NoSelection); // clears the selection
    list.setSelectionMode(QAbstractItemView::MultiSelection);
    click(view, QPoint(10, 3));
    click(view, QPoint(10, 2 * h + 3));
    CHECK(list.item(0)->isSelected() && list.item(2)->isSelected() && list.selectedItems().size() == 2);
    click(view, QPoint(10, 3));
    CHECK(!list.item(0)->isSelected() && list.selectedItems().size() == 1);
    key(&list, Qt::Key_Down); // moves the current item and nothing else
    CHECK(list.currentRow() == 1 && list.selectedItems().size() == 1);
    key(&list, Qt::Key_Space);
    CHECK(list.item(1)->isSelected() && list.selectedItems().size() == 2);
    key(&list, Qt::Key_Space);
    CHECK(!list.item(1)->isSelected());
    key(&list, Qt::Key_A, QString(), Qt::ControlModifier);
    CHECK(list.selectedItems().size() == 5);
    list.item(1)->setSelected(false);
    CHECK(list.selectedItems().size() == 4);
    list.item(1)->setSelected(true);
    list.setSelectionMode(QAbstractItemView::SingleSelection);
    CHECK(list.selectedItems().size() == 1);
    list.setSelectionMode(QAbstractItemView::ExtendedSelection);
    click(view, QPoint(10, 3));
    CHECK(list.selectedItems().size() == 1 && list.item(0)->isSelected());
    click(view, QPoint(10, 3 * h + 3), Qt::ShiftModifier);
    CHECK(list.selectedItems().size() == 4 && list.item(3)->isSelected() && list.currentRow() == 3);
    click(view, QPoint(10, h + 3), Qt::ShiftModifier);
    CHECK(list.selectedItems().size() == 2 && list.item(0)->isSelected() && list.item(1)->isSelected());
    click(view, QPoint(10, 4 * h + 3), Qt::ControlModifier);
    CHECK(list.selectedItems().size() == 3 && list.item(4)->isSelected());
    click(view, QPoint(10, 4 * h + 3), Qt::ControlModifier);
    CHECK(list.selectedItems().size() == 2 && !list.item(4)->isSelected());
    click(view, QPoint(10, 3 * h + 3), Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(list.selectedItems().size() == 4 && list.item(3)->isSelected() &&
          list.item(4)->isSelected()); // the range from the last anchor is added
    click(view, QPoint(10, 2 * h + 3));
    CHECK(list.selectedItems().size() == 1 && list.item(2)->isSelected());
    key(&list, Qt::Key_Down, QString(), Qt::ShiftModifier);
    key(&list, Qt::Key_Down, QString(), Qt::ShiftModifier);
    CHECK(list.selectedItems().size() == 3 && list.currentRow() == 4 && list.item(4)->isSelected());
    key(&list, Qt::Key_Up, QString(), Qt::ShiftModifier);
    CHECK(list.selectedItems().size() == 2 && !list.item(4)->isSelected());
    key(&list, Qt::Key_Up, QString(), Qt::ControlModifier); // the current item moves, the selection stays
    CHECK(list.currentRow() == 2 && list.selectedItems().size() == 2);
    key(&list, Qt::Key_Space, QString(), Qt::ControlModifier);
    CHECK(list.selectedItems().size() == 1);
    key(&list, Qt::Key_Space);
    CHECK(list.item(2)->isSelected() && list.selectedItems().size() == 1);
    click(view, QPoint(10, 5 * h + 2)); // empty space clears the selection
    CHECK(list.selectedItems().isEmpty());
    list.selectAll();
    CHECK(list.selectedItems().size() == 5);
    list.setSelectionMode(QAbstractItemView::NoSelection);
    CHECK(list.selectedItems().isEmpty());
    click(view, QPoint(10, 3));
    CHECK(list.currentRow() == 0 && list.selectedItems().isEmpty());
    list.setCurrentRow(2);
    CHECK(list.currentRow() == 2 && list.selectedItems().isEmpty());
    list.item(2)->setSelected(true);
    CHECK(list.selectedItems().isEmpty());
    list.selectAll();
    CHECK(list.selectedItems().isEmpty());
    list.setSelectionMode(QAbstractItemView::SingleSelection);
    list.setCurrentRow(4);
    CHECK(list.selectedItems().size() == 1);
    list.item(0)->setSelected(true);
    CHECK(list.selectedItems().size() == 1 && list.item(0)->isSelected() && !list.item(4)->isSelected());

    // Items that go or come
    list.setCurrentRow(2);
    rowsSeen.clear();
    selections = 0;
    QListWidgetItem *taken = list.takeItem(1);
    CHECK(taken && taken->text() == "Beta" && taken->listWidget() == nullptr && list.count() == 4 &&
          list.currentRow() == 1 && rowsSeen.empty() && list.currentItem()->text() == "Gamma");
    taken->setSelected(true); // not in a list
    CHECK(!taken->isSelected());
    QListWidgetItem *current = list.takeItem(1); // the current item: its successor takes over
    CHECK(current->text() == "Gamma" && list.currentRow() == 1 && list.currentItem()->text() == "Delta" &&
          rowsSeen == std::vector<int>{1} && selections == 1);
    delete current;
    delete list.takeItem(2);
    CHECK(list.count() == 2 && list.currentRow() == 1);
    delete list.takeItem(1); // the last
    CHECK(list.currentRow() == 0);
    delete list.takeItem(0);
    CHECK(list.count() == 0 && list.currentRow() == -1 && list.takeItem(0) == nullptr);
    // An item that is deleted leaves the list; one that is inserted moves between lists
    list.addItem(taken);
    auto *doomed = new QListWidgetItem("Doomed", &list);
    CHECK(list.count() == 2 && doomed->listWidget() == &list);
    delete doomed;
    CHECK(list.count() == 1);
    QListWidget other;
    other.addItem(taken);
    CHECK(list.count() == 0 && other.count() == 1 && taken->listWidget() == &other);
    other.insertItem(0, taken); // into the same list, at the place it is
    CHECK(other.count() == 1 && other.item(0) == taken);
    other.addItem("B");
    other.addItem("A");
    other.insertItem(0, other.item(2)); // moves "A" to the front
    CHECK(other.count() == 3 && other.item(0)->text() == "A");
    other.insertItem(3, other.item(0)); // and to the end
    CHECK(other.item(2)->text() == "A");
    list.addItem(other.takeItem(1));
    list.addItem(other.takeItem(0));
    list.addItem(other.takeItem(0));

    // Sorting
    list.clear();
    list.addItems({"pear", "apple", "fig", "apple"});
    list.setCurrentRow(2);
    list.sortItems();
    CHECK(list.item(0)->text() == "apple" && list.item(1)->text() == "apple" && list.item(2)->text() == "fig" &&
          list.item(3)->text() == "pear" && list.currentItem()->text() == "fig" && list.currentRow() == 2);
    list.sortItems(Qt::DescendingOrder);
    CHECK(list.item(0)->text() == "pear" && list.item(3)->text() == "apple" && list.currentItem()->text() == "fig");

    // Text: alignment, color, font, a text that is too long
    list.clear();
    list.addItems(
        {"Right", "Colored", "A text that is much too long to fit in the width of this list, so it is elided"});
    list.item(0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    list.item(1)->setForeground(QBrush(QColor(255, 0, 0)));
    QFont bold = list.font();
    bold.setBold(true);
    list.item(1)->setFont(bold);
    CHECK(list.item(1)->font().bold() && !list.item(0)->font().bold() &&
          list.item(0)->textAlignment() == (Qt::AlignRight | Qt::AlignVCenter) &&
          list.item(1)->foreground().color() == QColor(255, 0, 0));
    list.item(0)->setText("Right");
    list.item(0)->setToolTip("About the first");
    settle(&top);
    image = grab(&top);
    CHECK(image.width() > 0);

    // Scrolling: more rows than fit
    list.clear();
    for (int i = 0; i < 40; ++i)
        list.addItem(QString("Item %1").arg(i));
    QScrollBar *bar = list.verticalScrollBar();
    CHECK(!bar->isHidden() && bar->maximum() == 40 * h - view->height() && bar->pageStep() == view->height());
    list.setCurrentRow(39);
    CHECK(bar->value() == bar->maximum() && list.visualItemRect(list.item(39)).bottom() == view->height() - 1);
    list.setCurrentRow(0);
    CHECK(bar->value() == 0);
    key(&list, Qt::Key_End);
    CHECK(list.currentRow() == 39 && bar->value() == bar->maximum());
    key(&list, Qt::Key_Home);
    CHECK(bar->value() == 0);
    key(&list, Qt::Key_PageDown);
    CHECK(list.currentRow() > 3 && list.currentRow() < 20);
    list.scrollToItem(list.item(30));
    CHECK(list.visualItemRect(list.item(30)).bottom() <= view->height() &&
          list.visualItemRect(list.item(30)).top() >= 0);
    list.scrollToItem(nullptr);
    list.scrollToItem(&stray);
    bar->setValue(h + h / 2);
    CHECK(list.itemAt(5, 0) == list.item(1) && list.itemAt(5, h - h / 2 + 1) == list.item(2));
    CHECK(list.visualItemRect(list.item(1)).top() == -h / 2);
    bar->setValue(0);
    wheel(view, QPoint(10, 10), -1);
    CHECK(bar->value() == 3 * h);
    wheel(view, QPoint(10, 10), 1);
    CHECK(bar->value() == 0);
    click(view, QPoint(10, 3));
    wheel(view, QPoint(10, 10), -2);
    click(view, QPoint(10, 3));
    CHECK(list.currentRow() == 6);
    // Removing rows shrinks the range
    for (int i = 0; i < 30; ++i)
        delete list.takeItem(list.count() - 1);
    CHECK(bar->maximum() == std::max(0, 10 * h - view->height()));

    // Tool tips: of the item, then of the list
    QtPrivate::toolTipDelay() = 15;
    list.clear();
    list.addItems({"Has a tip", "None"});
    list.item(0)->setToolTip("About this item");
    list.setToolTip("The whole list");
    hover(view, QPoint(20, 3));
    CHECK(waitFor([] { return QToolTip::isVisible(); }) && QToolTip::text() == QString("About this item"));
    hover(view, QPoint(20, h + 3));
    CHECK(waitFor([] { return QToolTip::text() == QString("The whole list"); }));
    list.setToolTip(QString());
    hover(view, QPoint(20, h + 4));
    CHECK(!waitFor([] { return QToolTip::isVisible(); }, 100));
    QToolTip::hideText();

    // A disabled list draws in the disabled colors
    list.setEnabled(false);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(view->width() - 3, 3)) ==
          list.palette().color(QPalette::Disabled, QPalette::Base).rgb());
    list.setEnabled(true);
}

static void testPlainTextEdit() {
    int changes = 0, cursors = 0, selectionsSeen = 0, blocksSeen = 0;
    std::vector<bool> undoStates, redoStates;
    QWidget top;
    top.resize(300, 180);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    QPlainTextEdit edit(&top);
    QPushButton after("After", &top);
    layout->addWidget(&edit);
    layout->addWidget(&after);
    QObject::connect(&edit, &QPlainTextEdit::textChanged, [&] { ++changes; });
    QObject::connect(&edit, &QPlainTextEdit::cursorPositionChanged, [&] { ++cursors; });
    QObject::connect(&edit, &QPlainTextEdit::selectionChanged, [&] { ++selectionsSeen; });
    QObject::connect(&edit, &QPlainTextEdit::blockCountChanged, [&](int n) { blocksSeen = n; });
    QObject::connect(&edit, &QPlainTextEdit::undoAvailable, [&](bool b) { undoStates.push_back(b); });
    QObject::connect(&edit, &QPlainTextEdit::redoAvailable, [&](bool b) { redoStates.push_back(b); });
    top.show();
    settle(&top);

    CHECK(edit.toPlainText().isEmpty() && edit.blockCount() == 1 && !edit.isReadOnly() &&
          edit.lineWrapMode() == QPlainTextEdit::WidgetWidth && edit.tabStopDistance() == 80 &&
          !edit.tabChangesFocus() && edit.focusPolicy() == Qt::StrongFocus && edit.isUndoRedoEnabled() &&
          edit.placeholderText().isEmpty());
    CHECK(edit.sizePolicy().horizontalPolicy() == QSizePolicy::Expanding &&
          edit.sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    const int lh = edit.fontMetrics().height();
    QWidget *view = edit.viewport();
    CHECK(edit.textCursor().position() == 0 && edit.textCursor().atStart() && edit.textCursor().atEnd() &&
          edit.cursorRect() == QRect(4, 4, 1, lh));

    // Setting the text
    edit.setPlainText("one\ntwo\nthree");
    CHECK(edit.toPlainText() == "one\ntwo\nthree" && edit.blockCount() == 3 && changes == 1 && blocksSeen == 3);
    edit.setPlainText("one\ntwo\nthree");
    CHECK(changes == 1); // the same text
    CHECK(edit.textCursor().position() == 0 && edit.cursorRect().y() == 4);
    edit.clear();
    CHECK(edit.toPlainText().isEmpty() && edit.blockCount() == 1 && changes == 2 && blocksSeen == 1);

    // Typing: a click gives the focus, text goes in at the cursor
    click(view, QPoint(20, 20));
    CHECK(edit.hasFocus());
    type(&edit, "Hello");
    CHECK(edit.toPlainText() == "Hello" && edit.textCursor().position() == 5 && changes == 7);
    key(&edit, Qt::Key_Space, " ");
    type(&edit, "world");
    CHECK(edit.toPlainText() == "Hello world" && edit.textCursor().position() == 11);
    key(&edit, Qt::Key_Backspace);
    CHECK(edit.toPlainText() == "Hello worl" && edit.textCursor().position() == 10);
    key(&edit, Qt::Key_Left);
    key(&edit, Qt::Key_Left);
    key(&edit, Qt::Key_Delete);
    CHECK(edit.toPlainText() == "Hello wol" && edit.textCursor().position() == 8);
    key(&edit, Qt::Key_Return);
    CHECK(edit.toPlainText() == "Hello wo\nl" && edit.blockCount() == 2 && blocksSeen == 2 &&
          edit.textCursor().position() == 9 && edit.textCursor().blockNumber() == 1 &&
          edit.textCursor().columnNumber() == 0 && edit.textCursor().atBlockStart());
    key(&edit, Qt::Key_Tab);
    CHECK(edit.toPlainText() == "Hello wo\n\tl");
    key(&edit, Qt::Key_Backspace);
    key(&edit, Qt::Key_Backspace);
    CHECK(edit.toPlainText() == "Hello wol" && edit.blockCount() == 1);
    // Editing keys that have nothing to do do nothing
    key(&edit, Qt::Key_Home, QString(), Qt::ControlModifier);
    key(&edit, Qt::Key_Backspace);
    CHECK(edit.toPlainText() == "Hello wol");
    key(&edit, Qt::Key_End, QString(), Qt::ControlModifier);
    key(&edit, Qt::Key_Delete);
    CHECK(edit.toPlainText() == "Hello wol");
    key(&edit, Qt::Key_F5);
    key(&edit, Qt::Key_C, "c", Qt::ControlModifier);
    CHECK(edit.toPlainText() == "Hello wol");

    // Moving: by a character, a word, a line, a page, to the ends
    edit.setPlainText("alpha beta gamma\ndelta epsilon\n\nzeta");
    key(&edit, Qt::Key_Right);
    CHECK(edit.textCursor().position() == 1);
    key(&edit, Qt::Key_Right, QString(), Qt::ControlModifier);
    CHECK(edit.textCursor().position() == 6);
    key(&edit, Qt::Key_Right, QString(), Qt::ControlModifier);
    CHECK(edit.textCursor().position() == 11);
    key(&edit, Qt::Key_Left, QString(), Qt::ControlModifier);
    CHECK(edit.textCursor().position() == 6);
    key(&edit, Qt::Key_End);
    CHECK(edit.textCursor().position() == 16 && edit.textCursor().atBlockEnd());
    key(&edit, Qt::Key_Down);
    CHECK(edit.textCursor().blockNumber() == 1 && edit.textCursor().columnNumber() <= 13 &&
          edit.textCursor().columnNumber() > 5);
    key(&edit, Qt::Key_Down);
    CHECK(edit.textCursor().blockNumber() == 2 && edit.textCursor().atBlockStart());
    key(&edit, Qt::Key_Down);
    CHECK(edit.textCursor().blockNumber() == 3);
    key(&edit, Qt::Key_Down);
    CHECK(edit.textCursor().blockNumber() == 3);
    key(&edit, Qt::Key_Up);
    key(&edit, Qt::Key_Up);
    CHECK(edit.textCursor().blockNumber() == 1);
    key(&edit, Qt::Key_Up);
    CHECK(edit.textCursor().blockNumber() == 0 && edit.textCursor().columnNumber() > 5); // the column is kept
    key(&edit, Qt::Key_Home);
    CHECK(edit.textCursor().position() == 0);
    key(&edit, Qt::Key_End, QString(), Qt::ControlModifier);
    CHECK(edit.textCursor().atEnd() && edit.textCursor().position() == static_cast<int>(edit.toPlainText().size()));
    key(&edit, Qt::Key_PageUp);
    CHECK(edit.textCursor().blockNumber() == 0);
    key(&edit, Qt::Key_PageDown);
    CHECK(edit.textCursor().blockNumber() == 3);
    key(&edit, Qt::Key_Left);
    key(&edit, Qt::Key_Left);
    key(&edit, Qt::Key_Home, QString(), Qt::ControlModifier);
    CHECK(edit.textCursor().position() == 0);

    // The column that Up and Down keep survives short lines
    edit.moveCursor(QTextCursor::End);
    edit.moveCursor(QTextCursor::Up);
    edit.moveCursor(QTextCursor::Up);
    edit.moveCursor(QTextCursor::EndOfBlock);
    key(&edit, Qt::Key_Down);
    key(&edit, Qt::Key_Down);
    CHECK(edit.textCursor().blockNumber() == 3);
    key(&edit, Qt::Key_Up);
    key(&edit, Qt::Key_Up);
    CHECK(edit.textCursor().blockNumber() == 1);

    // Selecting: with Shift, with the mouse, a word, everything
    edit.moveCursor(QTextCursor::Start);
    selectionsSeen = 0;
    key(&edit, Qt::Key_Right, QString(), Qt::ShiftModifier);
    key(&edit, Qt::Key_Right, QString(), Qt::ShiftModifier);
    CHECK(edit.textCursor().hasSelection() && edit.textCursor().selectedText() == "al" && selectionsSeen == 2 &&
          edit.textCursor().anchor() == 0 && edit.textCursor().position() == 2);
    key(&edit, Qt::Key_Right, QString(), Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(edit.textCursor().selectedText() == "alpha ");
    key(&edit, Qt::Key_Down, QString(), Qt::ShiftModifier);
    CHECK(edit.textCursor().selectedText().contains("\n"));
    key(&edit, Qt::Key_Left); // collapses to the start
    CHECK(!edit.textCursor().hasSelection() && edit.textCursor().position() == 0);
    key(&edit, Qt::Key_Right, QString(), Qt::ShiftModifier);
    key(&edit, Qt::Key_Right); // collapses to the end
    CHECK(!edit.textCursor().hasSelection() && edit.textCursor().position() == 1);
    // A selection is replaced by what is typed, and removed by Backspace
    edit.setPlainText("one two three");
    key(&edit, Qt::Key_Right, QString(), Qt::ControlModifier | Qt::ShiftModifier);
    type(&edit, "1");
    CHECK(edit.toPlainText() == "1two three" && edit.textCursor().position() == 1);
    key(&edit, Qt::Key_Right, QString(), Qt::ControlModifier | Qt::ShiftModifier);
    key(&edit, Qt::Key_Backspace);
    CHECK(edit.toPlainText() == "1three");
    key(&edit, Qt::Key_Home);
    key(&edit, Qt::Key_Right, QString(), Qt::ControlModifier | Qt::ShiftModifier);
    key(&edit, Qt::Key_Delete);
    CHECK(edit.toPlainText().isEmpty());
    edit.setPlainText("one two three");
    key(&edit, Qt::Key_A, "a", Qt::ControlModifier);
    CHECK(edit.textCursor().selectedText() == "one two three" && edit.textCursor().position() == 13);
    edit.selectAll();
    CHECK(edit.textCursor().selectionStart() == 0 && edit.textCursor().selectionEnd() == 13);
    key(&edit, Qt::Key_Home);
    // Mouse: click places the cursor, drag selects, double click takes a word
    press(view, QPoint(4 + 1, 4 + 2));
    CHECK(edit.textCursor().position() == 0);
    drag(view, QPoint(4 + edit.fontMetrics().horizontalAdvance("one t") + 1, 6));
    CHECK(edit.textCursor().selectedText().startsWith("one") && edit.textCursor().hasSelection());
    release(view, QPoint(60, 6));
    const int wordX = 4 + edit.fontMetrics().horizontalAdvance("one t") + 2;
    QWindow *win = top.windowHandle();
    const QPoint dbl = inWindow(view, QPoint(wordX, 6));
    QWindowSystemInterface::handleMouseEvent(win, QPointF(dbl), QPointF(dbl), Qt::LeftButton, Qt::LeftButton,
                                             QEvent::MouseButtonDblClick, Qt::NoModifier);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(dbl), QPointF(dbl), Qt::NoButton, Qt::LeftButton,
                                             QEvent::MouseButtonRelease, Qt::NoModifier);
    CHECK(edit.textCursor().selectedText() == "two");
    click(view, QPoint(200, 6), Qt::ShiftModifier);
    CHECK(edit.textCursor().selectedText().size() > 3);
    click(view, QPoint(250, 100));
    CHECK(!edit.textCursor().hasSelection() && edit.textCursor().atEnd());

    // Undo and redo: typing is undone together, a change of the program on its own
    edit.setPlainText("start");
    undoStates.clear();
    redoStates.clear();
    key(&edit, Qt::Key_End, QString(), Qt::ControlModifier);
    type(&edit, " now");
    CHECK(edit.toPlainText() == "start now" && undoStates == std::vector<bool>{true});
    key(&edit, Qt::Key_Z, "z", Qt::ControlModifier);
    CHECK(edit.toPlainText() == "start" && undoStates == (std::vector<bool>{true, false}) &&
          redoStates == std::vector<bool>{true} && edit.textCursor().position() == 5);
    key(&edit, Qt::Key_Y, "y", Qt::ControlModifier);
    CHECK(edit.toPlainText() == "start now" && redoStates == (std::vector<bool>{true, false}) &&
          edit.textCursor().position() == 9);
    key(&edit, Qt::Key_Z, "z", Qt::ControlModifier);
    key(&edit, Qt::Key_Z, "Z", Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(edit.toPlainText() == "start now");
    edit.undo();
    edit.undo(); // nothing more to undo
    CHECK(edit.toPlainText() == "start");
    edit.redo();
    edit.redo();
    CHECK(edit.toPlainText() == "start now");
    // Typing after moving is a new change; a change erases what could be redone
    key(&edit, Qt::Key_Left);
    type(&edit, "X");
    type(&edit, "Y");
    key(&edit, Qt::Key_Left);
    type(&edit, "Z");
    CHECK(edit.toPlainText() == "start noXZYw");
    edit.setPlainText("abc");
    CHECK(undoStates.back() == false);
    edit.setUndoRedoEnabled(false);
    type(&edit, "d");
    edit.undo();
    CHECK(!edit.isUndoRedoEnabled() && edit.toPlainText().contains("d"));
    edit.setUndoRedoEnabled(true);

    // Read-only: the cursor moves and selects, the text stays
    edit.setPlainText("fixed text");
    edit.setReadOnly(true);
    CHECK(edit.isReadOnly());
    type(&edit, "x");
    key(&edit, Qt::Key_Return);
    key(&edit, Qt::Key_Backspace);
    key(&edit, Qt::Key_Delete);
    CHECK(edit.hasFocus());
    key(&edit, Qt::Key_Tab); // a read-only editor lets the Tab key move the focus
    CHECK(after.hasFocus());
    edit.setFocus();
    key(&edit, Qt::Key_Z, "z", Qt::ControlModifier);
    CHECK(edit.toPlainText() == "fixed text");
    edit.setFocus();
    key(&edit, Qt::Key_Right);
    key(&edit, Qt::Key_Right, QString(), Qt::ShiftModifier);
    CHECK(edit.textCursor().selectedText() == "i" && edit.textCursor().position() == 2);
    edit.insertPlainText("!"); // the program may
    CHECK(edit.toPlainText() == "f!xed text");
    edit.setReadOnly(false);
    edit.setFocus();

    // Tab: typed, or it moves the focus
    edit.clear();
    key(&edit, Qt::Key_Tab);
    CHECK(edit.toPlainText() == "\t" && edit.hasFocus());
    edit.setTabChangesFocus(true);
    key(&edit, Qt::Key_Tab);
    CHECK(edit.toPlainText() == "\t" && after.hasFocus());
    edit.setTabChangesFocus(false);
    edit.setFocus();

    // Appending and inserting
    edit.clear();
    edit.appendPlainText("first");
    CHECK(edit.toPlainText() == "first" && edit.blockCount() == 1);
    edit.appendPlainText("second");
    edit.appendPlainText("");
    CHECK(edit.toPlainText() == "first\nsecond\n" && edit.blockCount() == 3);
    edit.setPlainText("hello world");
    edit.moveCursor(QTextCursor::Start);
    edit.moveCursor(QTextCursor::NextWord, QTextCursor::KeepAnchor);
    edit.insertPlainText("bye ");
    CHECK(edit.toPlainText() == "bye world" && edit.textCursor().position() == 4 && !edit.textCursor().hasSelection());
    edit.insertPlainText("big ");
    CHECK(edit.toPlainText() == "bye big world");

    // QTextCursor
    edit.setPlainText("The quick brown fox\njumps over\nthe lazy dog");
    QTextCursor c = edit.textCursor();
    CHECK(!c.isNull() && c.position() == 0 && c == edit.textCursor() && QTextCursor().isNull() &&
          !(c != edit.textCursor()));
    CHECK(c.movePosition(QTextCursor::NextWord) && c.position() == 4);
    CHECK(c.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor) && c.selectedText() == "quick" &&
          c.hasSelection() && c.selectionStart() == 4 && c.selectionEnd() == 9 && c.anchor() == 4);
    c.clearSelection();
    CHECK(!c.hasSelection() && c.position() == 9);
    c.select(QTextCursor::WordUnderCursor);
    CHECK(c.selectedText() == "quick");
    c.setPosition(0);
    c.select(QTextCursor::BlockUnderCursor);
    CHECK(c.selectedText() == "The quick brown fox");
    c.select(QTextCursor::LineUnderCursor);
    CHECK(c.selectedText() == "The quick brown fox");
    c.select(QTextCursor::Document);
    CHECK(c.selectedText() == edit.toPlainText());
    c.setPosition(10);
    c.setPosition(14, QTextCursor::KeepAnchor);
    CHECK(c.selectedText() == "brow" && c.anchor() == 10);
    c.setPosition(-5);
    CHECK(c.position() == 0);
    c.setPosition(9999);
    CHECK(c.atEnd() && c.position() == static_cast<int>(edit.toPlainText().size()) &&
          !c.movePosition(QTextCursor::End));
    CHECK(c.movePosition(QTextCursor::StartOfBlock) && c.atBlockStart() && c.blockNumber() == 2 &&
          c.positionInBlock() == 0);
    CHECK(c.movePosition(QTextCursor::PreviousBlock) && c.blockNumber() == 1 && c.position() == 20);
    CHECK(c.movePosition(QTextCursor::NextBlock) && c.blockNumber() == 2);
    CHECK(c.movePosition(QTextCursor::Up) && c.blockNumber() == 1);
    CHECK(c.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, 1) && c.blockNumber() == 2);
    CHECK(c.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 2) && c.position() == 29);
    CHECK(c.movePosition(QTextCursor::WordLeft) && c.movePosition(QTextCursor::Right) &&
          c.movePosition(QTextCursor::NoMove) == false);
    c.movePosition(QTextCursor::Start);
    CHECK(c.atStart() && !c.movePosition(QTextCursor::PreviousCharacter) &&
          c.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, 3) && c.position() == 3 &&
          c.movePosition(QTextCursor::StartOfWord) && c.position() == 0);
    // The cursor changes the text; the editor's cursor follows the text
    c.setPosition(0);
    c.insertText("Oh. ");
    CHECK(edit.toPlainText().startsWith("Oh. The quick") && c.position() == 4);
    c.setPosition(4);
    c.setPosition(8, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    CHECK(edit.toPlainText().startsWith("Oh. quick") && !c.hasSelection());
    c.deleteChar();
    CHECK(edit.toPlainText().startsWith("Oh. uick"));
    c.deletePreviousChar();
    CHECK(edit.toPlainText().startsWith("Oh.uick") && c.position() == 3);
    c.setPosition(0);
    c.insertBlock();
    CHECK(edit.toPlainText().startsWith("\nOh.uick") && edit.blockCount() == 4);
    c.setPosition(static_cast<int>(edit.toPlainText().size()));
    c.deleteChar();
    c.setPosition(0);
    c.deletePreviousChar();
    c.removeSelectedText();
    CHECK(edit.toPlainText().startsWith("\nOh.uick"));
    // The editor's own cursor moves with edits before it
    edit.setPlainText("abcdef");
    edit.moveCursor(QTextCursor::End);
    QTextCursor front = edit.textCursor();
    front.setPosition(0);
    front.insertText("123");
    CHECK(edit.textCursor().position() == 9);
    front.setPosition(1);
    front.setPosition(5, QTextCursor::KeepAnchor);
    front.removeSelectedText();
    CHECK(edit.toPlainText() == "1cdef" && edit.textCursor().position() == 5);
    // setTextCursor puts the editor's cursor where the cursor is
    edit.setPlainText("one two three");
    QTextCursor mover = edit.textCursor();
    mover.setPosition(4);
    mover.setPosition(7, QTextCursor::KeepAnchor);
    cursors = selectionsSeen = 0;
    edit.setTextCursor(mover);
    CHECK(edit.textCursor().selectedText() == "two" && cursors == 1 && selectionsSeen == 1);
    edit.setTextCursor(QTextCursor());
    CHECK(edit.textCursor().selectedText() == "two");
    QTextCursor beyond = mover;
    beyond.setPosition(5000);
    edit.setTextCursor(beyond);
    CHECK(edit.textCursor().atEnd());
    // An edit block is undone together
    edit.setPlainText("base");
    QTextCursor grouped = edit.textCursor();
    grouped.movePosition(QTextCursor::End);
    grouped.beginEditBlock();
    grouped.insertText(" one");
    grouped.beginEditBlock();
    grouped.insertText(" two");
    grouped.endEditBlock();
    grouped.insertText(" three");
    grouped.endEditBlock();
    grouped.endEditBlock(); // too many: ignored
    CHECK(edit.toPlainText() == "base one two three");
    edit.undo();
    CHECK(edit.toPlainText() == "base");
    // A cursor of an editor that is deleted is null
    QTextCursor orphan;
    {
        QPlainTextEdit gone;
        orphan = gone.textCursor();
        CHECK(!orphan.isNull());
    }
    CHECK(orphan.isNull() && !orphan.movePosition(QTextCursor::End) && orphan.selectedText().isEmpty() &&
          orphan.blockNumber() == 0 && !orphan.atEnd() && !orphan.atBlockEnd());
    orphan.setPosition(3);
    orphan.insertText("x");
    orphan.deleteChar();
    orphan.deletePreviousChar();
    orphan.removeSelectedText();
    orphan.beginEditBlock();
    orphan.endEditBlock();
    orphan.select(QTextCursor::Document);

    // Wrapping and scrolling
    edit.clear();
    const QString words =
        "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu nu xi omicron pi rho sigma";
    edit.setPlainText(words);
    settle(&top);
    CHECK(edit.blockCount() == 1 && edit.horizontalScrollBar()->isHidden());
    edit.moveCursor(QTextCursor::End);
    CHECK(edit.cursorRect().y() > 4 + lh); // on a later line
    const int endY = edit.cursorRect().y();
    edit.moveCursor(QTextCursor::Start);
    CHECK(edit.cursorRect().y() == 4);
    key(&edit, Qt::Key_Down);
    CHECK(edit.textCursor().position() > 0 && edit.textCursor().blockNumber() == 0 && edit.cursorRect().y() == 4 + lh);
    key(&edit, Qt::Key_End);
    CHECK(edit.textCursor().position() < static_cast<int>(words.size()) && edit.cursorRect().y() == 4 + lh);
    key(&edit, Qt::Key_Home);
    CHECK(edit.cursorRect().x() == 4 && edit.cursorRect().y() == 4 + lh);
    CHECK(endY > 0);
    // A click on a wrapped line lands on it
    click(view, QPoint(10, 4 + lh + lh / 2));
    CHECK(edit.cursorRect().y() == 4 + lh && edit.textCursor().position() > 5);
    edit.setLineWrapMode(QPlainTextEdit::NoWrap);
    settle(&top);
    CHECK(edit.lineWrapMode() == QPlainTextEdit::NoWrap && !edit.horizontalScrollBar()->isHidden() &&
          edit.cursorRect().y() == 4 - edit.verticalScrollBar()->value());
    edit.moveCursor(QTextCursor::End);
    CHECK(edit.horizontalScrollBar()->value() > 0 && edit.cursorRect().x() < view->width() &&
          edit.cursorRect().x() >= 0);
    edit.moveCursor(QTextCursor::Start);
    CHECK(edit.horizontalScrollBar()->value() == 0);
    edit.setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit.setTabStopDistance(40);
    CHECK(edit.tabStopDistance() == 40);
    edit.setPlainText("a\tb");
    edit.moveCursor(QTextCursor::End);
    CHECK(edit.cursorRect().x() >= 4 + 40 && edit.cursorRect().x() < 4 + 40 + 30);
    edit.setTabStopDistance(80);
    // Many lines: the editor scrolls to the cursor
    QString many;
    for (int i = 0; i < 60; ++i)
        many += QString("line %1\n").arg(i);
    edit.setPlainText(many);
    QScrollBar *bar = edit.verticalScrollBar();
    CHECK(!bar->isHidden() && bar->maximum() > 0 && bar->singleStep() == lh);
    edit.moveCursor(QTextCursor::End);
    CHECK(bar->value() == bar->maximum() && edit.cursorRect().bottom() <= view->height());
    edit.moveCursor(QTextCursor::Start);
    CHECK(bar->value() == 0);
    key(&edit, Qt::Key_PageDown);
    CHECK(edit.textCursor().blockNumber() > 3 && edit.textCursor().blockNumber() < 20);
    bar->setValue(0);
    wheel(view, QPoint(10, 10), -1);
    CHECK(bar->value() == 3 * lh);
    edit.setPlainText("short");
    CHECK(bar->isHidden() && bar->value() == 0);

    // Appending follows the end of a log
    edit.setReadOnly(true);
    edit.setPlainText(many);
    bar->setValue(bar->maximum());
    edit.appendPlainText("a new line");
    CHECK(bar->value() == bar->maximum());
    bar->setValue(0);
    edit.appendPlainText("another");
    CHECK(bar->value() == 0);
    edit.setReadOnly(false);

    // Painting: background, selection, cursor
    edit.setPlainText("Selected text");
    edit.moveCursor(QTextCursor::Start);
    edit.moveCursor(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    edit.setFocus();
    QImage image = grab(&top);
    const QPoint origin = inWindow(view);
    const QPalette pal = edit.palette();
    CHECK(image.pixel(origin + QPoint(view->width() - 3, view->height() - 3)) == pal.color(QPalette::Base).rgb());
    CHECK(image.pixel(origin + QPoint(4 + 1, 4 + 1)) == pal.color(QPalette::Highlight).rgb());
    edit.moveCursor(QTextCursor::Start);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(4 + 1, 4 + 1)) == pal.color(QPalette::Base).rgb());
    const QRect caret = edit.cursorRect();
    CHECK(image.pixel(origin + QPoint(caret.x(), caret.y() + 3)) == pal.color(QPalette::Text).rgb());
    edit.clearFocus();
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(caret.x(), caret.y() + 3)) == pal.color(QPalette::Base).rgb());
    edit.setEnabled(false);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(view->width() - 3, view->height() - 3)) ==
          pal.color(QPalette::Disabled, QPalette::Base).rgb());
    edit.setEnabled(true);
    edit.setPlaceholderText("Write here");
    CHECK(edit.placeholderText() == "Write here");
    edit.clear();
    image = grab(&top);
    CHECK(image.width() > 0);

    // QTextEdit has the other names
    QTextEdit rich("start", &top);
    CHECK(rich.toPlainText() == "start" && rich.acceptRichText());
    rich.setText("replaced");
    rich.append("added");
    CHECK(rich.toPlainText() == "replaced\nadded" && rich.blockCount() == 2);
    rich.setAcceptRichText(false);
    CHECK(!rich.acceptRichText());
    QTextEdit plain;
    CHECK(plain.toPlainText().isEmpty());
}

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    // The tests look for the colors of the light palette, whatever the desktop uses.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    testFrame();
    testScrollBar();
    testScrollArea();
    testScrollAreaWidget();
    testHeaderView();
    testTableWidget();
    testTableToolTips();
    testListWidget();
    testPlainTextEdit();
    if (g_failures == 0)
        std::puts("all scroll tests passed");
    return g_failures == 0 ? 0 : 1;
}
