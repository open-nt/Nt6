// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qregion.h>

class QEventPoint;
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
    // The fingers that changed or are down, in one call. A point has its id and state, its position in the window
    // as scenePosition() and on the screen as globalPosition(). The first call of a sequence (the first finger
    // down) sends QEvent::TouchBegin to the window; if the window does not accept it, the first finger is sent as
    // the mouse instead.
    static void handleTouchEvent(QWindow *window, const QList<QEventPoint> &points,
                                 Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    // The system took the touch sequence away (a gesture of the system, or the window going away).
    static void handleTouchCancel(QWindow *window);
    static void handleKeyEvent(QWindow *window, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                               const QString &text = QString(), bool autorep = false);
    // The same with the codes of the platform's keyboard, which QKeyEvent::nativeScanCode() and the others return.
    static void handleExtendedKeyEvent(QWindow *window, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                                       quint32 nativeScanCode, quint32 nativeVirtualKey, quint32 nativeModifiers,
                                       const QString &text = QString(), bool autorep = false);
};
