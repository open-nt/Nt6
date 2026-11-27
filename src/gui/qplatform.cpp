// MIT License - see LICENSE
#include "qpa/qplatformbackingstore_p.h"

#include "qwindow_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformscreen_p.h"
#include "qpa/qplatformwindow_p.h"

#include "platforms/offscreen/qoffscreenintegration_p.h"
#ifdef NT_HAVE_XCB
#include "platforms/xcb/qxcbintegration_p.h"
#endif
#ifdef NT_HAVE_SSD1322
#include "platforms/ssd1322/qssd1322integration_p.h"
#endif
#ifdef __ANDROID__
#include "platforms/android/qandroidintegration_p.h"
#endif

#include <QtCore/qdebug.h>

QPlatformScreen::~QPlatformScreen() = default;

QSizeF QPlatformScreen::physicalSize() const {
    const QRect g = geometry();
    return QSizeF(g.width() * 25.4 / logicalDpiX(), g.height() * 25.4 / logicalDpiY());
}

QPlatformWindow::~QPlatformWindow() = default;
QPlatformBackingStore::~QPlatformBackingStore() = default;
QPlatformIntegration::~QPlatformIntegration() = default;

QPoint QPlatformIntegration::cursorPosition() const { return QtPrivate::GuiAccess::cursorPosition(); }

void QPlatformIntegration::setCursorPosition(const QPoint &pos) { QtPrivate::GuiAccess::setCursorPosition(pos); }

quint64 QPlatformIntegration::createVulkanSurface(QWindow *window, void *instance,
                                                  QVulkanGetInstanceProcAddr getInstanceProcAddr) const {
    (void)window;
    (void)instance;
    (void)getInstanceProcAddr;
    return 0;
}

namespace QtPrivate {

QStringList availablePlatforms() {
    QStringList names;
#ifdef NT_HAVE_XCB
    names.append(QString::fromLatin1("xcb"));
#endif
#ifdef __ANDROID__
    names.append(QString::fromLatin1("android"));
#endif
#ifdef NT_HAVE_SSD1322
    names.append(QString::fromLatin1("ssd1322"));
#endif
    names.append(QString::fromLatin1("offscreen"));
    return names;
}

QPlatformIntegration *createPlatformIntegration(const QString &name, int &argc, char **argv, const QString &options) {
    (void)argc;
    (void)argv;
    (void)options;
    const QString key = name.toLower();
    if (key == QString::fromLatin1("offscreen"))
        return new QOffscreenIntegration;
#ifdef NT_HAVE_SSD1322
    if (key == QString::fromLatin1("ssd1322"))
        return QSsd1322Integration::create(options);
#endif
#ifdef __ANDROID__
    if (key == QString::fromLatin1("android"))
        return QAndroidIntegration::create();
#endif
#ifdef NT_HAVE_XCB
    if (key == QString::fromLatin1("xcb"))
        return QXcbIntegration::create();
#endif
    return nullptr;
}

} // namespace QtPrivate
