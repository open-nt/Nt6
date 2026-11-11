// MIT License - see LICENSE
#include "qpa/qwindowsysteminterface_p.h"

#include "qwindow_p.h"

#include <QtCore/qcoreapplication.h>
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

void QWindowSystemInterface::handleKeyEvent(QWindow *window, QEvent::Type type, int key,
                                            Qt::KeyboardModifiers modifiers, const QString &text, bool autorep) {
    QtPrivate::GuiAccess::setInput(QGuiApplication::mouseButtons(), modifiers);
    QKeyEvent event(type, key, modifiers, text, autorep);
    QCoreApplication::sendEvent(window, &event);
}
