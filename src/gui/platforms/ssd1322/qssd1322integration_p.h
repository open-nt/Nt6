// MIT License - see LICENSE
#pragma once

#include "platforms/offscreen/qoffscreenintegration_p.h"

#include <QtGpio/qssd1322display.h>

#include <memory>

class QSsd1322Integration;

// The panel of an SSD1322 OLED as the one screen: 4 bits of gray, usually 256x64.
class QSsd1322Screen : public QPlatformScreen {
public:
    explicit QSsd1322Screen(const QSize &size) : m_size(size) {}

    QRect geometry() const override { return QRect(QPoint(0, 0), m_size); }
    int depth() const override { return 4; }
    QString name() const override { return QString::fromLatin1("ssd1322"); }

private:
    QSize m_size;
};

// A window as on offscreen, except that a top-level window fills the panel when it is shown: there
// is no window manager, and the panel is too small for anything else.
class QSsd1322Window : public QOffscreenWindow {
public:
    explicit QSsd1322Window(QWindow *window) : QOffscreenWindow(window) {}

    void setVisible(bool visible) override;
};

// The pixels of a window in memory, as on offscreen; flush() converts the flushed region to gray
// and sends it to the panel.
class QSsd1322BackingStore : public QOffscreenBackingStore {
public:
    QSsd1322BackingStore(QWindow *window, const QSsd1322Integration *integration)
        : QOffscreenBackingStore(window), m_integration(integration) {}

    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;

private:
    const QSsd1322Integration *m_integration;
};

class QSsd1322Integration : public QPlatformIntegration {
public:
    // Opens the display described by `options` (key=value pairs separated by colons, see doc/gui.h);
    // returns null and prints why if it cannot.
    static QSsd1322Integration *create(const QString &options);
    ~QSsd1322Integration() override;

    void initialize() override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;

    // Copies `rect` (in screen coordinates) of `image`, whose top left is at `origin` on the
    // screen, into the frame as gray, and sends it to the panel.
    void present(const QImage &image, const QPoint &origin, const QRegion &region) const;
    // The gray frame as last sent, for tests.
    QImage frame() const;

private:
    explicit QSsd1322Integration(std::unique_ptr<QSsd1322Display> display);

    std::unique_ptr<QSsd1322Display> m_display;
    QSsd1322Screen *m_screen;
    mutable QImage m_frame; // Format_Grayscale8, the size of the panel
};
