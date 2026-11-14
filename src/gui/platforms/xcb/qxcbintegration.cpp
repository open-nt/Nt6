// MIT License - see LICENSE
#include "qxcbintegration_p.h"

#include "qpa/qdesktopportal_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qxcbconnection_p.h"
#include "qxcbwindow_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qwindow.h>

#if QT_CONFIG(vulkan)
#include "qvulkan_p.h"

#include <vulkan/vulkan_xcb.h>
#endif

#include <cstdio>

QXcbIntegration *QXcbIntegration::create() {
    QString error;
    std::unique_ptr<QXcbConnection> connection = QXcbConnection::create(&error);
    if (!connection) {
        std::fprintf(stderr, "Nt GUI (xcb): %s\n", error.toUtf8().constData());
        return nullptr;
    }
    return new QXcbIntegration(std::move(connection));
}

QXcbIntegration::QXcbIntegration(std::unique_ptr<QXcbConnection> connection) : m_connection(std::move(connection)) {}

QXcbIntegration::~QXcbIntegration() = default;

void QXcbIntegration::initialize() {
    QWindowSystemInterface::handleScreenAdded(m_connection->platformScreen());
    m_portal = std::make_unique<QDesktopPortalSettings>();
    QObject::connect(m_portal.get(), &QDesktopPortalSettings::colorSchemeChanged,
                     [](Qt::ColorScheme scheme) { QWindowSystemInterface::handleColorSchemeChange(scheme); });
}

Qt::ColorScheme QXcbIntegration::colorScheme() const {
    return m_portal ? m_portal->colorScheme() : Qt::ColorScheme::Unknown;
}

QPoint QXcbIntegration::cursorPosition() const { return m_connection->pointerPosition(); }

void QXcbIntegration::setCursorPosition(const QPoint &pos) { m_connection->warpPointer(pos); }

// OpenGL on X11 through the XCB platform of EGL; windows are created with the root visual.
// Mesa's EGL reads the replies it waits for from the connection, and with them the events in front of them, which
// then sit in XCB's queue: the socket is not readable for them. Without this, a resize could wait until the event
// loop next sleeps, which a fast repaint timer can put off for seconds.
void QXcbIntegration::checkQueuedEvents() { m_connection->checkQueue(); }

QEglNativePlatform QXcbIntegration::eglPlatform() const {
    QEglNativePlatform native;
    native.platform = 0x31DC; // EGL_PLATFORM_XCB_EXT
    native.display = m_connection->xcb();
    native.screen = m_connection->screenNumber();
    native.windowSurfaces = true;
    native.visualId = m_connection->visual();
    return native;
}

QPlatformWindow *QXcbIntegration::createPlatformWindow(QWindow *window) const {
    return new QXcbWindow(window, m_connection.get());
}

QPlatformBackingStore *QXcbIntegration::createPlatformBackingStore(QWindow *window) const {
    return new QXcbBackingStore(window, m_connection.get());
}

// Vulkan presents to X windows through VK_KHR_xcb_surface, with the connection of the platform. The surface is owned by
// the platform window and destroyed before the X window, by QWindow::destroy().
quint64 QXcbIntegration::createVulkanSurface(QWindow *window, void *instance,
                                             QVulkanGetInstanceProcAddr getInstanceProcAddr) const {
#if QT_CONFIG(vulkan)
    auto *platformWindow = static_cast<QXcbWindow *>(window->handle());
    if (!platformWindow || !instance || !getInstanceProcAddr)
        return 0;
    if (platformWindow->vulkanSurface)
        return platformWindow->vulkanSurface;
    auto *vkInstance = static_cast<VkInstance>(instance);
    auto getProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(getInstanceProcAddr);
    auto createSurface = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(getProcAddr(vkInstance, "vkCreateXcbSurfaceKHR"));
    auto destroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(getProcAddr(vkInstance, "vkDestroySurfaceKHR"));
    if (!createSurface || !destroySurface) {
        qWarning("Nt GUI (xcb): the Vulkan instance has no VK_KHR_xcb_surface");
        return 0;
    }
    VkXcbSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    info.connection = m_connection->xcb();
    info.window = platformWindow->id();
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkResult result = createSurface(vkInstance, &info, nullptr, &surface);
    if (result != VK_SUCCESS) {
        qWarning("Nt GUI (xcb): vkCreateXcbSurfaceKHR failed: %d", static_cast<int>(result));
        return 0;
    }
    platformWindow->vulkanSurface = QtPrivate::Vulkan::handleToU64(surface);
    platformWindow->destroyVulkanSurface = [vkInstance, destroySurface, surface] {
        destroySurface(vkInstance, surface, nullptr);
    };
    return platformWindow->vulkanSurface;
#else
    return QPlatformIntegration::createVulkanSurface(window, instance, getInstanceProcAddr);
#endif
}
