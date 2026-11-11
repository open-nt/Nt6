// MIT License - see LICENSE
#pragma once

#include "qpa/qplatformintegration_p.h"

#include <memory>

class QDesktopPortalSettings;
class QXcbConnection;

// The X11 platform: talks to an X server, or XWayland, with XCB.
class QXcbIntegration : public QPlatformIntegration {
public:
    // Returns null, after printing the reason, if there is no display to connect to.
    static QXcbIntegration *create();
    ~QXcbIntegration() override;

    void initialize() override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    Qt::ColorScheme colorScheme() const override;
    QPoint cursorPosition() const override;
    void setCursorPosition(const QPoint &pos) override;

private:
    explicit QXcbIntegration(std::unique_ptr<QXcbConnection> connection);
    std::unique_ptr<QXcbConnection> m_connection;
    std::unique_ptr<QDesktopPortalSettings> m_portal;
};
