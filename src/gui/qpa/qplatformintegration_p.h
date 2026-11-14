// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QPlatformBackingStore;

// How OpenGL reaches the window system of a platform through EGL.
struct QEglNativePlatform {
    unsigned platform = 0;       // an EGL_PLATFORM_* value; 0 if the platform has no OpenGL
    void *display = nullptr;     // the native display for eglGetPlatformDisplay()
    int screen = -1;             // the screen attribute of the X11 and XCB platforms, or -1
    bool windowSurfaces = false; // windows get EGL window surfaces; otherwise each renders into a framebuffer object
    unsigned visualId = 0;       // the native visual of the windows, which the EGL config must match; 0 for any
};
class QPlatformWindow;
class QWindow;

// The types of vkGetInstanceProcAddr without the Vulkan headers, which this header does not include: a platform casts
// the pointer back to PFN_vkGetInstanceProcAddr before calling it.
using QVulkanVoidFunction = void (*)();
using QVulkanGetInstanceProcAddr = QVulkanVoidFunction (*)(void *instance, const char *name);

// Entry point of a platform plugin ("offscreen", "xcb"). QGuiApplication creates one and asks it
// for the pieces of the window system.
class QPlatformIntegration {
public:
    virtual ~QPlatformIntegration();

    // Called once the application object is ready. Platforms announce their screens here with
    // QWindowSystemInterface::handleScreenAdded().
    virtual void initialize() {}
    virtual QPlatformWindow *createPlatformWindow(QWindow *window) const = 0;
    virtual QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const = 0;
    // The color scheme of the desktop when the application starts. Later changes go through
    // QWindowSystemInterface::handleColorSchemeChange().
    virtual Qt::ColorScheme colorScheme() const { return Qt::ColorScheme::Unknown; }
    // Where the mouse cursor is on the desktop. The default is where the last mouse event or the last
    // setCursorPosition() put it.
    virtual QPoint cursorPosition() const;
    // Moves the mouse cursor. The default only remembers the position.
    virtual void setCursorPosition(const QPoint &pos);
    // How OpenGL contexts reach the window system. The default has no OpenGL.
    virtual QEglNativePlatform eglPlatform() const { return {}; }
    // Called on the thread of the application object after OpenGL used the connection to the window system, as a
    // swap of buffers does. The EGL library can read events from the connection into the platform's own queue,
    // where no socket notifier sees them; the platform makes sure that they are handled.
    virtual void checkQueuedEvents() {}
    // The instance extension that Vulkan surfaces of the platform's windows need ("VK_KHR_xcb_surface"); null when the
    // platform cannot present with Vulkan.
    virtual const char *vulkanSurfaceExtension() const { return nullptr; }
    // Creates the VkSurfaceKHR of `window` in `instance` (a VkInstance) with the functions that `getInstanceProcAddr`
    // resolves, and stores it in the platform window, which destroys it as it goes away. Returns the handle as an
    // integer (see QtPrivate::Vulkan::handleFromU64()), or 0 when the platform cannot make one.
    virtual quint64 createVulkanSurface(QWindow *window, void *instance,
                                        QVulkanGetInstanceProcAddr getInstanceProcAddr) const;
};

namespace QtPrivate {

// Names of the platforms built into this library.
QStringList availablePlatforms();
// Creates the platform called `name`; returns null if there is none or it cannot start (it
// prints why).
QPlatformIntegration *createPlatformIntegration(const QString &name, int &argc, char **argv);

} // namespace QtPrivate
