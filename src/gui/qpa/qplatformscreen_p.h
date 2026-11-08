// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>

class QScreen;

// What a platform plugin tells Nt about one screen. QGuiApplication wraps it in a QScreen.
class QPlatformScreen {
public:
    virtual ~QPlatformScreen();

    virtual QRect geometry() const = 0;
    virtual QRect availableGeometry() const { return geometry(); }
    virtual int depth() const { return 32; }
    virtual QString name() const { return QString(); }
    virtual QSizeF physicalSize() const;
    virtual qreal logicalDpiX() const { return 96.0; }
    virtual qreal logicalDpiY() const { return 96.0; }
    // Pixels per unit of window coordinates.
    virtual qreal devicePixelRatio() const { return 1.0; }

    QScreen *screen() const { return m_screen; }

private:
    friend class QScreen;
    QScreen *m_screen = nullptr;
};
