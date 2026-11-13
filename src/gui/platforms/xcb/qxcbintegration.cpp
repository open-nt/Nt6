// MIT License - see LICENSE
#include "qxcbintegration_p.h"

#include "qpa/qdesktopportal_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qxcbconnection_p.h"
#include "qxcbwindow_p.h"

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
