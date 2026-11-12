// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtGui/qwindow.h>

class QOpenGLContext;
struct QOpenGLFunctionsPrivate;
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
    // Called when `closed` was hidden: quits if it was the last window that keeps the application running.
    static void maybeQuitOnLastWindowClosed(QWindow *closed);
    // Whether the window keeps the application running while it shows; true unless it is set otherwise.
    static void setQuitOnClose(QWindow *window, bool quit);
    static bool quitOnClose(const QWindow *window);
    static bool inputWindowActivated(QWindow *window);
    static void addScreen(QPlatformScreen *screen);
    static void removeScreen(QPlatformScreen *screen);
    static void screenGeometryChanged(QPlatformScreen *screen);
    static void setPlatformColorScheme(Qt::ColorScheme scheme);
    static void setFocusWindow(QWindow *window);
    static void setInput(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    // The last position of the mouse that the platform reported, or that was set.
    static void setCursorPosition(const QPoint &pos);
    static QPoint cursorPosition();
    // Gives the platform window of `window` the cursor that it has to show now: the override cursor of the
    // application, or its own.
    static void applyCursor(QWindow *window);
    // The format of the buffers of an OpenGL surface, which QWindow::format() returns from then on.
    static void setActualFormat(QWindow *window, const QSurfaceFormat &format);
    // The functions of `context`, resolved the first time they are asked for.
    static QOpenGLFunctionsPrivate *openGLFunctions(QOpenGLContext *context);
    // Called before the platform window of `surface` goes away: drops what OpenGL made for it.
    static void surfaceAboutToBeDestroyed(QSurface *surface);
    // Deletes the platform integration; the application object calls it as it goes away.
    static void shutdown();
};

} // namespace QtPrivate
