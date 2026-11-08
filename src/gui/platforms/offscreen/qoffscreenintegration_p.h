// MIT License - see LICENSE
#pragma once

#include "qpa/qplatformbackingstore_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformscreen_p.h"
#include "qpa/qplatformwindow_p.h"

#include <QtGui/qimage.h>

// Headless platform: windows are not shown anywhere, and their contents live in a QImage. Used
// by the tests and wherever there is no display.
class QOffscreenScreen : public QPlatformScreen {
public:
    QOffscreenScreen();

    QRect geometry() const override { return QRect(0, 0, 800, 600); }
    QString name() const override { return QString::fromLatin1("offscreen"); }
    // 1, or QT_SCALE_FACTOR if that is set.
    qreal devicePixelRatio() const override { return m_ratio; }

private:
    qreal m_ratio = 1.0;
};

class QOffscreenWindow : public QPlatformWindow {
public:
    explicit QOffscreenWindow(QWindow *window) : QPlatformWindow(window) {}

    void setGeometry(const QRect &rect) override;
    void setVisible(bool visible) override;
    quintptr winId() const override { return m_id; }

private:
    bool m_visible = false;
    quintptr m_id = nextId();
    static quintptr nextId();
};

class QOffscreenBackingStore : public QPlatformBackingStore {
public:
    explicit QOffscreenBackingStore(QWindow *window);
    ~QOffscreenBackingStore() override;

    // The pixels that `window` painted, for tests; null if the window has no backing store.
    static QImage imageOf(const QWindow *window);

    QPaintDevice *paintDevice() override { return &m_image; }
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    QImage toImage() const override { return m_image; }

private:
    QImage m_image;
};

class QOffscreenIntegration : public QPlatformIntegration {
public:
    QOffscreenIntegration();
    ~QOffscreenIntegration() override;

    void initialize() override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;

private:
    QOffscreenScreen *m_screen;
};
