// MIT License - see LICENSE
#include "qpa/qplatformbackingstore_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformscreen_p.h"
#include "qpa/qplatformwindow_p.h"

#include "platforms/offscreen/qoffscreenintegration_p.h"
#ifdef NT_HAVE_XCB
#include "platforms/xcb/qxcbintegration_p.h"
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

namespace QtPrivate {

QStringList availablePlatforms() {
    QStringList names;
#ifdef NT_HAVE_XCB
    names.append(QString::fromLatin1("xcb"));
#endif
    names.append(QString::fromLatin1("offscreen"));
    return names;
}

QPlatformIntegration *createPlatformIntegration(const QString &name, int &argc, char **argv) {
    (void)argc;
    (void)argv;
    const QString key = name.toLower();
    if (key == QString::fromLatin1("offscreen"))
        return new QOffscreenIntegration;
#ifdef NT_HAVE_XCB
    if (key == QString::fromLatin1("xcb"))
        return QXcbIntegration::create();
#endif
    return nullptr;
}

} // namespace QtPrivate
