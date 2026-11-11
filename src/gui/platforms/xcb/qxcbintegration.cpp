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

QPlatformWindow *QXcbIntegration::createPlatformWindow(QWindow *window) const {
    return new QXcbWindow(window, m_connection.get());
}

QPlatformBackingStore *QXcbIntegration::createPlatformBackingStore(QWindow *window) const {
    return new QXcbBackingStore(window, m_connection.get());
}
