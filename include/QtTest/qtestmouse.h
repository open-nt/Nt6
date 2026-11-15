// MIT License - see LICENSE
#pragma once

#include <QtCore/qlogging.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qtenvironmentvariables.h>
#include <QtGui/qtestsupport_gui.h>
#include <QtGui/qwindow.h>
#include <QtTest/qtest.h>

#ifdef QT_WIDGETS_LIB
#include <QtWidgets/qwidget.h>
#endif

namespace QTest {

/*!
    \enum QTest::MouseAction

    What a mouse does in mouseEvent().

    \value MousePress A button goes down.
    \value MouseRelease A button comes up.
    \value MouseDClick A button goes down and comes up twice, the second press a double click.
    \value MouseClick A button goes down and comes up.
    \value MouseMove The mouse moves.
*/
enum MouseAction { MousePress, MouseRelease, MouseClick, MouseDClick, MouseMove };

/*!
    Returns the delay, in milliseconds, before each simulated mouse event when none is given: the value of the
    \c QTEST_MOUSEEVENT_DELAY environment variable, or 0.
*/
inline int defaultMouseDelay() {
    static const int delay = qEnvironmentVariableIntValue("QTEST_MOUSEEVENT_DELAY");
    return delay > 0 ? delay : 0;
}

} // namespace QTest

namespace QTestPrivate {

// The buttons that the simulated mouse holds down.
inline Qt::MouseButtons &simulatedMouseButtons() {
    static Qt::MouseButtons buttons;
    return buttons;
}

// Simulates `action` with `button` at `pos`, a position in `window`, after `delay` milliseconds (the default delay if
// negative), with `modifiers` held.
inline void mouseEventAt(QTest::MouseAction action, QWindow *window, Qt::MouseButton button,
                         Qt::KeyboardModifiers modifiers, const QPoint &pos, int delay) {
    if (!window) {
        qWarning("QTest::mouseEvent(): the window is null");
        return;
    }
    if (delay < 0 || delay < QTest::defaultMouseDelay())
        delay = QTest::defaultMouseDelay();
    if (delay > 0)
        QTest::qWait(delay);
    Qt::MouseButtons &buttons = simulatedMouseButtons();
    const QPointF local(pos);
    const auto press = [&](QEvent::Type type) {
        buttons |= button;
        sendMouseEvent(window, type, local, button, buttons, modifiers);
    };
    const auto release = [&] {
        buttons &= ~Qt::MouseButtons(button);
        sendMouseEvent(window, QEvent::MouseButtonRelease, local, button, buttons, modifiers);
    };
    switch (action) {
    case QTest::MousePress:
        press(QEvent::MouseButtonPress);
        break;
    case QTest::MouseRelease:
        release();
        break;
    case QTest::MouseClick:
        press(QEvent::MouseButtonPress);
        release();
        break;
    case QTest::MouseDClick:
        press(QEvent::MouseButtonPress);
        release();
        press(QEvent::MouseButtonDblClick);
        release();
        break;
    case QTest::MouseMove:
        sendMouseEvent(window, QEvent::MouseMove, local, Qt::NoButton, buttons, modifiers);
        break;
    }
}

} // namespace QTestPrivate

namespace QTest {

/*!
    Simulates \a action of the mouse with \a button in \a window at \a pos, a position in the window, or at the
    center of the window if \a pos is null (and \c{QPoint(0, 0)} is null, as in Qt), with the keyboard modifiers
    \a stateKey held. Waits \a delay
    milliseconds first, or defaultMouseDelay() if \a delay is negative.

    The functions below call this one; use them instead.
*/
inline void mouseEvent(MouseAction action, QWindow *window, Qt::MouseButton button, Qt::KeyboardModifiers stateKey,
                       QPoint pos, int delay = -1) {
    if (window && pos.isNull())
        pos = QRect(QPoint(), window->size()).center();
    QTestPrivate::mouseEventAt(action, window, button, stateKey, pos, delay);
}

/*!
    Simulates pressing \a button in \a window at \a pos (the center of the window if null) with the modifiers
    \a stateKey, after \a delay milliseconds.

    \sa mouseRelease(), mouseClick()
*/
inline void mousePress(QWindow *window, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                       QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MousePress, window, button, stateKey, pos, delay);
}

/*!
    Simulates releasing \a button in \a window at \a pos (the center of the window if null) with the modifiers
    \a stateKey, after \a delay milliseconds.

    \sa mousePress(), mouseClick()
*/
inline void mouseRelease(QWindow *window, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                         QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseRelease, window, button, stateKey, pos, delay);
}

/*!
    Simulates clicking \a button in \a window at \a pos (the center of the window if null) with the modifiers
    \a stateKey, after \a delay milliseconds: a press and a release.

    \sa mouseDClick(), mousePress(), mouseRelease()
*/
inline void mouseClick(QWindow *window, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                       QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseClick, window, button, stateKey, pos, delay);
}

/*!
    Simulates double-clicking \a button in \a window at \a pos (the center of the window if null) with the
    modifiers \a stateKey, after \a delay milliseconds: a click, then a double-click press and a release.

    \sa mouseClick()
*/
inline void mouseDClick(QWindow *window, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                        QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseDClick, window, button, stateKey, pos, delay);
}

/*!
    Simulates moving the mouse to \a pos in \a window (the center of the window if null), after \a delay
    milliseconds, with the buttons that mousePress() left down.

    \sa mousePress()
*/
inline void mouseMove(QWindow *window, QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseMove, window, Qt::NoButton, Qt::NoModifier, pos, delay);
}

#ifdef QT_WIDGETS_LIB

/*!
    \overload

    Simulates \a action with \a button in \a widget at \a pos, a position in the widget, or at its center if
    \a pos is null, with the modifiers \a stateKey, after \a delay milliseconds. The widget's window must be
    shown.
*/
inline void mouseEvent(MouseAction action, QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers stateKey,
                       QPoint pos, int delay = -1) {
    if (!widget) {
        qWarning("QTest::mouseEvent(): the widget is null");
        return;
    }
    QWidget *top = widget->window();
    QWindow *window = top->windowHandle();
    if (!window) {
        qWarning("QTest::mouseEvent(): the widget has no window; show it first");
        return;
    }
    if (pos.isNull())
        pos = widget->rect().center();
    QTestPrivate::mouseEventAt(action, window, button, stateKey, widget->mapTo(top, pos), delay);
}

/*!
    \overload

    Simulates pressing \a button in \a widget at \a pos (its center if null) with the modifiers \a stateKey, after
    \a delay milliseconds.
*/
inline void mousePress(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                       QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MousePress, widget, button, stateKey, pos, delay);
}

/*!
    \overload

    Simulates releasing \a button in \a widget at \a pos (its center if null) with the modifiers \a stateKey,
    after \a delay milliseconds.
*/
inline void mouseRelease(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                         QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseRelease, widget, button, stateKey, pos, delay);
}

/*!
    \overload

    Simulates clicking \a button in \a widget at \a pos (its center if null) with the modifiers \a stateKey, after
    \a delay milliseconds.
*/
inline void mouseClick(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                       QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseClick, widget, button, stateKey, pos, delay);
}

/*!
    \overload

    Simulates double-clicking \a button in \a widget at \a pos (its center if null) with the modifiers
    \a stateKey, after \a delay milliseconds.
*/
inline void mouseDClick(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::NoModifier,
                        QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseDClick, widget, button, stateKey, pos, delay);
}

/*!
    \overload

    Simulates moving the mouse to \a pos in \a widget (its center if null), after \a delay milliseconds.
*/
inline void mouseMove(QWidget *widget, QPoint pos = QPoint(), int delay = -1) {
    mouseEvent(MouseMove, widget, Qt::NoButton, Qt::NoModifier, pos, delay);
}

#endif // QT_WIDGETS_LIB

} // namespace QTest
