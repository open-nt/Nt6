// MIT License - see LICENSE
//
// Tests for touch input on the offscreen platform: QTouchEvent sequences that reach a window or a widget, and the first
// finger that is delivered as the mouse when nothing takes the touch events.
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>
#include <QtTest/qsignalspy.h>

#include "qpa/qwindowsysteminterface_p.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

QEventPoint point(int id, QEventPoint::State state, const QPointF &local) {
    return QEventPoint(id, state, local, local);
}

// A widget that takes touch events and keeps what it got.
class TouchWidget : public QWidget {
public:
    explicit TouchWidget(bool accept, QWidget *parent = nullptr) : QWidget(parent), m_accept(accept) {
        setAttribute(Qt::WA_AcceptTouchEvents);
    }
    struct Seen {
        QEvent::Type type;
        QList<QEventPoint> points;
    };
    QList<Seen> seen;
    int mousePresses = 0;

protected:
    bool event(QEvent *e) override {
        switch (e->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            seen.append({e->type(), static_cast<QTouchEvent *>(e)->points()});
            e->setAccepted(m_accept);
            return m_accept;
        default:
            return QWidget::event(e);
        }
    }
    void mousePressEvent(QMouseEvent *) override { ++mousePresses; }

private:
    bool m_accept;
};

QWindow *windowOf(QWidget &w) {
    w.show();
    return w.windowHandle();
}

void testWidgetTakesTouch() {
    QWidget top;
    top.resize(300, 200);
    auto *inner = new TouchWidget(true, &top);
    inner->setGeometry(100, 50, 150, 100);
    QWindow *window = windowOf(top);
    CHECK(window);

    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Pressed, QPointF(120, 70))});
    QWindowSystemInterface::handleTouchEvent(window, {point(1, QEventPoint::Pressed, QPointF(200, 120)),
                                                      point(0, QEventPoint::Stationary, QPointF(120, 70))});
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Updated, QPointF(130, 80))});
    QWindowSystemInterface::handleTouchEvent(window, {point(1, QEventPoint::Released, QPointF(200, 120))});
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Released, QPointF(130, 80))});

    CHECK(inner->seen.size() == 5);
    if (inner->seen.size() == 5) {
        CHECK(inner->seen[0].type == QEvent::TouchBegin);
        CHECK(inner->seen[1].type == QEvent::TouchUpdate);
        CHECK(inner->seen[2].type == QEvent::TouchUpdate);
        CHECK(inner->seen[3].type == QEvent::TouchUpdate);
        CHECK(inner->seen[4].type == QEvent::TouchEnd);
        CHECK(inner->seen[0].points.size() == 1);
        // Positions are in the coordinates of the widget that gets the event.
        CHECK(inner->seen[0].points[0].position() == QPointF(20, 20));
        CHECK(inner->seen[0].points[0].scenePosition() == QPointF(120, 70));
        CHECK(inner->seen[2].points[0].position() == QPointF(30, 30));
        CHECK(inner->seen[2].points[0].pressPosition() == QPointF(20, 20));
        CHECK(inner->seen[1].points[0].state() == QEventPoint::Pressed);
    }
    CHECK(inner->mousePresses == 0); // accepted: not emulated
}

void testIgnoredTouchBecomesMouse() {
    QWidget top;
    top.resize(300, 200);
    auto *button = new QPushButton(QStringLiteral("Tap"), &top);
    button->setGeometry(20, 20, 120, 60);
    int clicks = 0;
    QObject::connect(button, &QPushButton::clicked, [&] { ++clicks; });
    QWindow *window = windowOf(top);

    QWindowSystemInterface::handleTouchEvent(window, {point(7, QEventPoint::Pressed, QPointF(40, 40))});
    CHECK(button->isDown());
    // A second finger does not become another mouse.
    QWindowSystemInterface::handleTouchEvent(window, {point(8, QEventPoint::Pressed, QPointF(250, 150))});
    QWindowSystemInterface::handleTouchEvent(window, {point(8, QEventPoint::Released, QPointF(250, 150))});
    CHECK(button->isDown());
    QWindowSystemInterface::handleTouchEvent(window, {point(7, QEventPoint::Released, QPointF(40, 40))});
    CHECK(!button->isDown());
    CHECK(clicks == 1);

    // Dragging off the button does not click.
    QWindowSystemInterface::handleTouchEvent(window, {point(1, QEventPoint::Pressed, QPointF(40, 40))});
    QWindowSystemInterface::handleTouchEvent(window, {point(1, QEventPoint::Updated, QPointF(280, 190))});
    QWindowSystemInterface::handleTouchEvent(window, {point(1, QEventPoint::Released, QPointF(280, 190))});
    CHECK(clicks == 1);
}

void testWidgetThatIgnoresTouchGetsMouse() {
    QWidget top;
    top.resize(200, 200);
    auto *inner = new TouchWidget(false, &top);
    inner->setGeometry(0, 0, 200, 200);
    QWindow *window = windowOf(top);
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Pressed, QPointF(10, 10))});
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Released, QPointF(10, 10))});
    CHECK(inner->seen.size() == 1); // only the TouchBegin that it ignored
    CHECK(inner->mousePresses == 1);
}

void testTouchOnAPlainWindow() {
    // A window that accepts the touch events gets the whole sequence, and no mouse events.
    class W : public QWindow {
    public:
        QList<QEvent::Type> types;
        int mice = 0;

    protected:
        void touchEvent(QTouchEvent *e) override {
            types.append(e->type());
            e->accept();
        }
        void mousePressEvent(QMouseEvent *) override { ++mice; }
    } window;
    window.resize(100, 100);
    window.show();
    QWindowSystemInterface::handleTouchEvent(&window, {point(0, QEventPoint::Pressed, QPointF(5, 5))});
    QWindowSystemInterface::handleTouchEvent(&window, {point(0, QEventPoint::Updated, QPointF(6, 6))});
    QWindowSystemInterface::handleTouchCancel(&window);
    CHECK((window.types == QList<QEvent::Type>{QEvent::TouchBegin, QEvent::TouchUpdate, QEvent::TouchCancel}));
    CHECK(window.mice == 0);
    // After a cancel the rest of the sequence is dropped, and a new one begins.
    QWindowSystemInterface::handleTouchEvent(&window, {point(0, QEventPoint::Released, QPointF(6, 6))});
    CHECK(window.types.size() == 3);
    QWindowSystemInterface::handleTouchEvent(&window, {point(0, QEventPoint::Pressed, QPointF(5, 5))});
    CHECK(window.types.size() == 4 && window.types.last() == QEvent::TouchBegin);
    QWindowSystemInterface::handleTouchEvent(&window, {point(0, QEventPoint::Released, QPointF(5, 5))});
    CHECK(window.types.last() == QEvent::TouchEnd);
}

// A finger on a widget of a window: down, to the places given, and up.
void finger(QWidget &top, const QPoint &from, const QList<QPoint> &moves = {}) {
    QWindow *window = top.windowHandle();
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Pressed, QPointF(from))});
    QPoint at = from;
    for (const QPoint &next : moves) {
        at = next;
        QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Updated, QPointF(at))});
    }
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Released, QPointF(at))});
    QCoreApplication::processEvents();
}

// A list that scrolls under a finger with the touch gesture of QScroller: a tap is a click, and a drag is not.
void testScrollerTouchGesture() {
    QWidget top;
    top.resize(240, 240);
    auto *list = new QListWidget(&top);
    list->setGeometry(0, 0, 200, 200);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    for (int i = 0; i < 100; ++i)
        list->addItem(QStringLiteral("Row %1").arg(i));
    top.show();
    QCoreApplication::processEvents();
    QScroller::grabGesture(list->viewport(), QScroller::TouchGesture);
    QSignalSpy pressed(list, &QListWidget::itemPressed);
    QSignalSpy clicked(list, &QListWidget::itemClicked);
    const auto middleOf = [&](int row) {
        return list->viewport()->mapTo(&top, list->visualItemRect(list->item(row)).center());
    };

    finger(top, middleOf(2));
    CHECK(pressed.count() == 1);
    CHECK(clicked.count() == 1);
    CHECK(list->currentRow() == 2);

    list->clearSelection();
    list->setCurrentRow(-1);
    finger(top, QPoint(50, 150), {QPoint(50, 130), QPoint(50, 90), QPoint(50, 50)});
    CHECK(list->verticalScrollBar()->value() > 20);
    CHECK(pressed.count() == 1); // the drag pressed nothing
    CHECK(clicked.count() == 1);
    CHECK(list->selectedItems().isEmpty());
    QScroller::scroller(list->viewport())->stop();

    // A finger that rests is a press, which a drag after it ends without a click.
    const int scrolled = list->verticalScrollBar()->value();
    QWindow *window = top.windowHandle();
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Pressed, QPointF(50, 150))});
    CHECK(pressed.count() == 1); // held back
    QElapsedTimer held;
    held.start();
    while (held.elapsed() < 350)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    CHECK(pressed.count() == 2);
    for (const int y : {110, 70})
        QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Updated, QPointF(50, y))});
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Released, QPointF(50, 70))});
    CHECK(list->verticalScrollBar()->value() > scrolled);
    CHECK(clicked.count() == 1);
    QScroller::scroller(list->viewport())->stop();

    // A touch that is cancelled clicks nothing.
    QWindowSystemInterface::handleTouchEvent(window, {point(0, QEventPoint::Pressed, QPointF(50, 100))});
    QWindowSystemInterface::handleTouchCancel(window);
    QCoreApplication::processEvents();
    CHECK(pressed.count() == 2);
    CHECK(clicked.count() == 1);
}

// In a scroll area that scrolls under the finger, a tap clicks the button that it is on, and a drag does not.
void testScrollerTouchOverButtons() {
    QWidget top;
    top.resize(240, 240);
    auto *area = new QScrollArea(&top);
    area->setGeometry(0, 0, 200, 200);
    auto *content = new QWidget;
    content->resize(180, 600);
    auto *button = new QPushButton(QStringLiteral("In the area"), content);
    button->setGeometry(20, 60, 140, 40);
    area->setWidget(content);
    top.show();
    QCoreApplication::processEvents();
    QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);
    QSignalSpy clicked(button, &QPushButton::clicked);
    const QPoint on = button->mapTo(&top, QPoint(70, 20));
    finger(top, on);
    CHECK(clicked.count() == 1);
    CHECK(!button->isDown());
    finger(top, on, {on - QPoint(0, 30), on - QPoint(0, 50)});
    CHECK(clicked.count() == 1);
    CHECK(area->verticalScrollBar()->value() > 0);
    QScroller::scroller(area->viewport())->stop();
}

// A drag across a page that only scrolls up and down is not a scroll: it moves the slider under the finger. A drag
// up the slider still scrolls the page.
void testScrollerLeavesCrossDrags() {
    QWidget top;
    top.resize(240, 240);
    auto *area = new QScrollArea(&top);
    area->setGeometry(0, 0, 200, 200);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->resize(180, 600);
    auto *slider = new QSlider(Qt::Horizontal, content);
    slider->setGeometry(10, 60, 160, 40);
    slider->setRange(0, 100);
    area->setWidget(content);
    top.show();
    QCoreApplication::processEvents();
    QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);
    const QPoint handle = slider->mapTo(&top, QPoint(8, 20));
    finger(top, handle, {handle + QPoint(30, 2), handle + QPoint(80, 2), handle + QPoint(120, 2)});
    CHECK(slider->value() > 50);
    CHECK(!slider->isSliderDown());
    CHECK(area->verticalScrollBar()->value() == 0);

    const int value = slider->value();
    const QPoint on = slider->mapTo(&top, QPoint(80, 20));
    finger(top, on, {on - QPoint(0, 30), on - QPoint(0, 50)});
    CHECK(area->verticalScrollBar()->value() > 0);
    CHECK(slider->value() == value);
    QScroller::scroller(area->viewport())->stop();
}

// A button with a menu opens it when it is pressed. Releasing is not a click, as in Qt.
void testMenuButtonIsNotClicked() {
    QWidget top;
    top.resize(300, 300);
    auto *button = new QPushButton(QStringLiteral("More"), &top);
    button->setGeometry(20, 20, 100, 40);
    auto *menu = new QMenu(button);
    menu->addAction(QStringLiteral("One"));
    button->setMenu(menu);
    top.show();
    QCoreApplication::processEvents();
    QSignalSpy pressed(button, &QPushButton::pressed);
    QSignalSpy clicked(button, &QPushButton::clicked);

    finger(top, QPoint(70, 40));
    CHECK(menu->isVisible());
    CHECK(pressed.count() == 1);
    CHECK(clicked.count() == 0);
    CHECK(button->isDown()); // it stays down while the menu is open
    menu->hide();
    QCoreApplication::processEvents();
    CHECK(!button->isDown());
    CHECK(clicked.count() == 0);
    // Moving off the button while the menu is open leaves it down.
    QWindow *window = top.windowHandle();
    QWindowSystemInterface::handleMouseEvent(window, QPointF(70, 40), QPointF(70, 40), Qt::LeftButton, Qt::LeftButton,
                                             QEvent::MouseButtonPress, Qt::NoModifier);
    QWindowSystemInterface::handleMouseEvent(window, QPointF(250, 250), QPointF(250, 250), Qt::LeftButton, Qt::NoButton,
                                             QEvent::MouseMove, Qt::NoModifier);
    CHECK(button->isDown());
    QWindowSystemInterface::handleMouseEvent(window, QPointF(250, 250), QPointF(250, 250), Qt::NoButton, Qt::LeftButton,
                                             QEvent::MouseButtonRelease, Qt::NoModifier);
    menu->hide();
    // Without its menu it is a button like any other again.
    button->setMenu(nullptr);
    finger(top, QPoint(70, 40));
    CHECK(clicked.count() == 1);
    button->setMenu(menu);
    clicked.clear();

    pressed.clear();
    clicked.clear();
    button->setFocus();
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
    QApplication::sendEvent(button, &down);
    CHECK(menu->isVisible());
    // The release goes where the focus is: to the menu, or back to the button.
    QKeyEvent up(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
    QApplication::sendEvent(button, &up);
    CHECK(clicked.count() == 0);
    menu->hide();
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    testWidgetTakesTouch();
    testIgnoredTouchBecomesMouse();
    testWidgetThatIgnoresTouchGetsMouse();
    testTouchOnAPlainWindow();
    testScrollerTouchGesture();
    testScrollerTouchOverButtons();
    testScrollerLeavesCrossDrags();
    testMenuButtonIsNotClicked();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
