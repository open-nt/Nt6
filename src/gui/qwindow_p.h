// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtGui/qwindow.h>

class QPlatformWindow;
class QPlatformIntegration;
class QPlatformScreen;

namespace QtPrivate {

// Reaches the private parts of QWindow, QScreen and QGuiApplication for the platform layer.
struct GuiAccess {
    static QPlatformWindow *platformWindow(const QWindow *window);
    // Applies a geometry the platform reports and sends the move and resize events for it.
    static void applyGeometry(QWindow *window, const QRect &rect);
    static void setExposed(QWindow *window, bool exposed);
    static void setActive(QWindow *window, bool active);

    static QPlatformIntegration *integration();
    static void registerWindow(QWindow *window);
    static void unregisterWindow(QWindow *window);
    static void maybeQuitOnLastWindowClosed();
    static bool inputWindowActivated(QWindow *window);
    static void addScreen(QPlatformScreen *screen);
    static void removeScreen(QPlatformScreen *screen);
    static void screenGeometryChanged(QPlatformScreen *screen);
    static void setPlatformColorScheme(Qt::ColorScheme scheme);
    static void setFocusWindow(QWindow *window);
    static void setInput(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    // Deletes the platform integration; the application object calls it as it goes away.
    static void shutdown();
};

} // namespace QtPrivate
