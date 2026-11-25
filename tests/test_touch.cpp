// MIT License - see LICENSE
//
// Tests for touch input on the offscreen platform: QTouchEvent sequences that reach a window or a widget, and the first
// finger that is delivered as the mouse when nothing takes the touch events.
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>

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

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    testWidgetTakesTouch();
    testIgnoredTouchBecomesMouse();
    testWidgetThatIgnoresTouchGetsMouse();
    testTouchOnAPlainWindow();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
