// MIT License - see LICENSE
#include "qpa/qwindowsysteminterface_p.h"

#include "qwindow_p.h"
#include "qshortcut_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>

void QWindowSystemInterface::handleScreenAdded(QPlatformScreen *screen) { QtPrivate::GuiAccess::addScreen(screen); }

void QWindowSystemInterface::handleColorSchemeChange(Qt::ColorScheme scheme) {
    QtPrivate::GuiAccess::setPlatformColorScheme(scheme);
}

void QWindowSystemInterface::handleScreenRemoved(QPlatformScreen *screen) {
    QtPrivate::GuiAccess::removeScreen(screen);
}

void QWindowSystemInterface::handleScreenGeometryChange(QPlatformScreen *screen) {
    QtPrivate::GuiAccess::screenGeometryChanged(screen);
}

void QWindowSystemInterface::handleExposeEvent(QWindow *window, const QRegion &region) {
    QtPrivate::GuiAccess::setExposed(window, window->isVisible() && !region.isEmpty());
    QExposeEvent event(region);
    QCoreApplication::sendEvent(window, &event);
}

void QWindowSystemInterface::handleGeometryChange(QWindow *window, const QRect &newRect) {
    QtPrivate::GuiAccess::applyGeometry(window, newRect);
}

void QWindowSystemInterface::handleCloseEvent(QWindow *window) { window->close(); }

void QWindowSystemInterface::handleWindowActivated(QWindow *window) {
    if (!QtPrivate::GuiAccess::inputWindowActivated(window))
        return;
}

namespace {

// The touch sequence that is going on: the fingers that are down, whether the window took it, and the finger that is
// sent as the mouse when it did not.
struct TouchSequence {
    QList<int> down;
    QMap<int, QPointF> pressPositions;
    QPointer<QWindow> window;
    bool accepted = false;
    int mouseFinger = -1;
    bool mouseUsed = false;
    QPointF lastLocal;
    QPointF lastGlobal;
};

TouchSequence g_touch;

} // namespace

void QWindowSystemInterface::handleTouchEvent(QWindow *window, const QList<QEventPoint> &points,
                                              Qt::KeyboardModifiers modifiers) {
    TouchSequence &s = g_touch;
    const bool begin = s.down.isEmpty();
    if (begin) {
        bool anyPressed = false;
        for (const QEventPoint &p : points)
            anyPressed = anyPressed || p.state() == QEventPoint::Pressed;
        if (!anyPressed)
            return; // the rest of a sequence that was cancelled
        s = TouchSequence();
        s.window = window;
    }
    QList<QEventPoint> list;
    for (const QEventPoint &source : points) {
        QEventPoint p = source;
        p.setPosition(p.scenePosition());
        if (p.state() == QEventPoint::Pressed) {
            if (!s.down.contains(p.id()))
                s.down.append(p.id());
            s.pressPositions[p.id()] = p.scenePosition();
        } else if (p.state() == QEventPoint::Released) {
            s.down.removeAll(p.id());
        }
        p.setPressPosition(s.pressPositions.value(p.id(), p.scenePosition()));
        list.append(p);
    }
    const bool end = s.down.isEmpty();
    const QEvent::Type type = begin ? QEvent::TouchBegin : end ? QEvent::TouchEnd : QEvent::TouchUpdate;
    if (begin || s.accepted) {
        QTouchEvent event(type, modifiers, list);
        QCoreApplication::sendEvent(window, &event);
        if (begin)
            s.accepted = event.isAccepted();
    }
    if (!s.accepted) {
        // Nobody took the touch events: the first finger is the mouse.
        for (const QEventPoint &p : list) {
            if (begin && p.state() == QEventPoint::Pressed && s.mouseFinger < 0 && !s.mouseUsed) {
                s.mouseFinger = p.id();
                s.mouseUsed = true;
                handleEnterEvent(window, p.scenePosition(), p.globalPosition());
                handleMouseEvent(window, p.scenePosition(), p.globalPosition(), Qt::LeftButton, Qt::LeftButton,
                                 QEvent::MouseButtonPress, modifiers);
            } else if (p.id() == s.mouseFinger) {
                if (p.state() == QEventPoint::Released) {
                    s.mouseFinger = -1;
                    handleMouseEvent(window, p.scenePosition(), p.globalPosition(), Qt::NoButton, Qt::LeftButton,
                                     QEvent::MouseButtonRelease, modifiers);
                    handleLeaveEvent(window);
                } else if (p.state() == QEventPoint::Updated) {
                    handleMouseEvent(window, p.scenePosition(), p.globalPosition(), Qt::LeftButton, Qt::NoButton,
                                     QEvent::MouseMove, modifiers);
                }
                s.lastLocal = p.scenePosition();
                s.lastGlobal = p.globalPosition();
            }
        }
    }
    if (end)
        s.mouseUsed = false;
}

void QWindowSystemInterface::handleTouchCancel(QWindow *window) {
    TouchSequence &s = g_touch;
    if (s.down.isEmpty())
        return;
    if (s.accepted) {
        QTouchEvent event(QEvent::TouchCancel, Qt::NoModifier, QList<QEventPoint>());
        QCoreApplication::sendEvent(window, &event);
    } else if (s.mouseFinger >= 0) {
        handleMouseEvent(window, s.lastLocal, s.lastGlobal, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease,
                         Qt::NoModifier);
        handleLeaveEvent(window);
    }
    s = TouchSequence();
}

void QWindowSystemInterface::handleEnterEvent(QWindow *window, const QPointF &local, const QPointF &global) {
    QtPrivate::GuiAccess::setCursorPosition(global.toPoint());
    QEnterEvent event(local, global);
    QCoreApplication::sendEvent(window, &event);
}

void QWindowSystemInterface::handleLeaveEvent(QWindow *window) {
    QEvent event(QEvent::Leave);
    QCoreApplication::sendEvent(window, &event);
}

void QWindowSystemInterface::handleMouseEvent(QWindow *window, const QPointF &local, const QPointF &global,
                                              Qt::MouseButtons buttons, Qt::MouseButton button, QEvent::Type type,
                                              Qt::KeyboardModifiers modifiers) {
    QtPrivate::GuiAccess::setInput(buttons, modifiers);
    QtPrivate::GuiAccess::setCursorPosition(global.toPoint());
    QMouseEvent event(type, local, global, button, buttons, modifiers);
    QCoreApplication::sendEvent(window, &event);
}

void QWindowSystemInterface::handleWheelEvent(QWindow *window, const QPointF &local, const QPointF &global,
                                              const QPoint &pixelDelta, const QPoint &angleDelta,
                                              Qt::KeyboardModifiers modifiers) {
    QtPrivate::GuiAccess::setInput(QGuiApplication::mouseButtons(), modifiers);
    QWheelEvent event(local, global, pixelDelta, angleDelta, QGuiApplication::mouseButtons(), modifiers);
    QCoreApplication::sendEvent(window, &event);
}

bool QWindowSystemInterface::handleKeyEvent(QWindow *window, QEvent::Type type, int key,
                                            Qt::KeyboardModifiers modifiers, const QString &text, bool autorep) {
    return handleExtendedKeyEvent(window, type, key, modifiers, 0, 0, 0, text, autorep);
}

bool QWindowSystemInterface::handleExtendedKeyEvent(QWindow *window, QEvent::Type type, int key,
                                                    Qt::KeyboardModifiers modifiers, quint32 nativeScanCode,
                                                    quint32 nativeVirtualKey, quint32 nativeModifiers,
                                                    const QString &text, bool autorep) {
    QtPrivate::GuiAccess::setInput(QGuiApplication::mouseButtons(), modifiers);
    // A QShortcut takes the key press before the window sees it.
    if (type == QEvent::KeyPress && QtPrivate::tryShortcut(window, key, modifiers, autorep))
        return true;
    QKeyEvent event(type, key, modifiers, nativeScanCode, nativeVirtualKey, nativeModifiers, text, autorep);
    // An event filter that keeps the key took it; otherwise the window says whether a widget accepted it.
    const bool delivered = QCoreApplication::sendEvent(window, &event);
    return delivered && event.isAccepted();
}
