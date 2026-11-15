// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qstring.h>

class QWindow;

namespace QTest {

/*!
    Waits until \a window is exposed (see QWindow::isExposed()), processing events meanwhile, for at
    most \a timeout milliseconds. Returns \c true if the window became exposed in time.

    \since 6.0
    \sa qWaitForWindowActive()
*/
bool qWaitForWindowExposed(QWindow *window, int timeout = 5000);

/*!
    Waits until \a window is the active window (see QWindow::isActive()), processing events meanwhile,
    for at most \a timeout milliseconds. Returns \c true if the window became active in time.

    \since 6.0
    \sa qWaitForWindowExposed()
*/
bool qWaitForWindowActive(QWindow *window, int timeout = 5000);

} // namespace QTest

namespace QTestPrivate {

// Delivers a mouse event of `type` at `local` in `window` as the window system would, with `button` the button that
// changed, `buttons` the buttons that are down afterwards and `modifiers` the keyboard modifiers. Used by the
// QTest::mouseClick() family of Nt Test.
void sendMouseEvent(QWindow *window, QEvent::Type type, const QPointF &local, Qt::MouseButton button,
                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);

// Delivers a wheel event at `local` in `window` with `angleDelta`, `pixelDelta` and `modifiers`, as the window
// system would. Used by QTest::wheelEvent() of Nt Test.
void sendWheelEvent(QWindow *window, const QPointF &local, const QPoint &angleDelta, const QPoint &pixelDelta,
                    Qt::KeyboardModifiers modifiers);

// Delivers a key event of `type` for `key` with `modifiers` and `text` to `window`, as the window system would. Used
// by the QTest::keyClick() family of Nt Test.
void sendKeyEvent(QWindow *window, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers, const QString &text);

} // namespace QTestPrivate
