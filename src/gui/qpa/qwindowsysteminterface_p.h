// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qregion.h>

class QPlatformScreen;
class QWindow;

// How a platform plugin passes what happens in the window system to Nt GUI. The handlers run in
// the thread of the application object and deliver their events before they return.
class QWindowSystemInterface {
public:
    static void handleScreenAdded(QPlatformScreen *screen);
    static void handleColorSchemeChange(Qt::ColorScheme scheme);
    static void handleScreenRemoved(QPlatformScreen *screen);
    static void handleScreenGeometryChange(QPlatformScreen *screen);

    static void handleExposeEvent(QWindow *window, const QRegion &region);
    static void handleGeometryChange(QWindow *window, const QRect &newRect);
    static void handleCloseEvent(QWindow *window);
    static void handleWindowActivated(QWindow *window);
    static void handleEnterEvent(QWindow *window, const QPointF &local, const QPointF &global);
    static void handleLeaveEvent(QWindow *window);
    static void handleMouseEvent(QWindow *window, const QPointF &local, const QPointF &global, Qt::MouseButtons buttons,
                                 Qt::MouseButton button, QEvent::Type type, Qt::KeyboardModifiers modifiers);
    static void handleWheelEvent(QWindow *window, const QPointF &local, const QPointF &global, const QPoint &pixelDelta,
                                 const QPoint &angleDelta, Qt::KeyboardModifiers modifiers);
    static void handleKeyEvent(QWindow *window, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                               const QString &text = QString(), bool autorep = false);
};
