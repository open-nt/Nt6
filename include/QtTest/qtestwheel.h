// MIT License - see LICENSE
#pragma once

#include <QtCore/qlogging.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtGui/qtestsupport_gui.h>
#include <QtGui/qwindow.h>

#ifdef QT_WIDGETS_LIB
#include <QtWidgets/qwidget.h>
#endif

namespace QTest {

/*!
    Simulates turning the mouse wheel at \a pos in \a window by \a angleDelta, in eighths of a degree (120 for one
    notch), and \a pixelDelta pixels, with the modifiers \a stateKey held.

    \since 6.0
*/
inline void wheelEvent(QWindow *window, QPointF pos, QPoint angleDelta, QPoint pixelDelta = QPoint(),
                       Qt::KeyboardModifiers stateKey = Qt::NoModifier) {
    if (!window) {
        qWarning("QTest::wheelEvent(): the window is null");
        return;
    }
    QTestPrivate::sendWheelEvent(window, pos, angleDelta, pixelDelta, stateKey);
}

#ifdef QT_WIDGETS_LIB

/*!
    \overload

    Simulates turning the mouse wheel at \a pos in \a widget (its center if null) by \a angleDelta and
    \a pixelDelta, with the modifiers \a stateKey held.
*/
inline void wheelEvent(QWidget *widget, QPoint pos, QPoint angleDelta, QPoint pixelDelta = QPoint(),
                       Qt::KeyboardModifiers stateKey = Qt::NoModifier) {
    if (!widget) {
        qWarning("QTest::wheelEvent(): the widget is null");
        return;
    }
    QWidget *top = widget->window();
    QWindow *window = top->windowHandle();
    if (!window) {
        qWarning("QTest::wheelEvent(): the widget has no window; show it first");
        return;
    }
    if (pos.isNull())
        pos = widget->rect().center();
    wheelEvent(window, QPointF(widget->mapTo(top, pos)), angleDelta, pixelDelta, stateKey);
}

#endif // QT_WIDGETS_LIB

} // namespace QTest
