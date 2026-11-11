// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QPlatformBackingStore;
class QPlatformWindow;
class QWindow;

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
};

namespace QtPrivate {

// Names of the platforms built into this library.
QStringList availablePlatforms();
// Creates the platform called `name`; returns null if there is none or it cannot start (it
// prints why).
QPlatformIntegration *createPlatformIntegration(const QString &name, int &argc, char **argv);

} // namespace QtPrivate
