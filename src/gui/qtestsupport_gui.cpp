// MIT License - see LICENSE
#include <QtGui/qtestsupport_gui.h>

#include "qpa/qwindowsysteminterface_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qeventloop.h>
#include <QtGui/qwindow.h>

#include <chrono>

namespace {

// Processes events until `done` returns true or `timeout` milliseconds pass; returns the last value of `done`.
template <typename Predicate> bool waitFor(Predicate done, int timeout) {
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout);
    while (!done()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        if (left.count() <= 0)
            return done();
        QCoreApplication::processEvents(QEventLoop::AllEvents, left.count() < 10 ? static_cast<int>(left.count()) : 10);
    }
    return true;
}

} // namespace

namespace QTest {

bool qWaitForWindowExposed(QWindow *window, int timeout) {
    return window && waitFor([window] { return window->isExposed(); }, timeout);
}

bool qWaitForWindowActive(QWindow *window, int timeout) {
    return window && waitFor([window] { return window->isActive(); }, timeout);
}

} // namespace QTest

namespace QTestPrivate {

void sendMouseEvent(QWindow *window, QEvent::Type type, const QPointF &local, Qt::MouseButton button,
                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers) {
    const QPointF global = local + QPointF(window->position());
    QWindowSystemInterface::handleMouseEvent(window, local, global, buttons, button, type, modifiers);
}

void sendWheelEvent(QWindow *window, const QPointF &local, const QPoint &angleDelta, const QPoint &pixelDelta,
                    Qt::KeyboardModifiers modifiers) {
    const QPointF global = local + QPointF(window->position());
    QWindowSystemInterface::handleWheelEvent(window, local, global, pixelDelta, angleDelta, modifiers);
}

void sendKeyEvent(QWindow *window, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers, const QString &text) {
    QWindowSystemInterface::handleKeyEvent(window, type, key, modifiers, text, false);
}

} // namespace QTestPrivate
