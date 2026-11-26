// MIT License - see LICENSE
#pragma once

#include "qandroidglue_p.h"
#include "qpa/qplatformbackingstore_p.h"
#include "qpa/qplatformclipboard_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformscreen_p.h"
#include "qpa/qplatformwindow_p.h"

#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtGui/qimage.h>

#include <memory>
#include <vector>

// The Android platform: one screen, which is the native window of the activity, and software rendering into it. The
// windows of the application share that surface. A window fills it, except a dialog over another window, which keeps
// its size, centred, with the windows below dimmed.
class QAndroidScreen : public QPlatformScreen {
public:
    QAndroidScreen();

    QRect geometry() const override { return m_geometry; }
    // Without the system bars; where the windows are.
    QRect availableGeometry() const override { return m_available; }
    QString name() const override { return QString::fromLatin1("android"); }
    qreal logicalDpiX() const override { return 120.0; }
    qreal logicalDpiY() const override { return 120.0; }
    qreal devicePixelRatio() const override { return m_ratio; }
    QSizeF physicalSize() const override;

    // Reads the size of the native window again; returns whether it changed.
    bool update(ANativeWindow *window);
    QSize pixelSize() const { return m_pixels; }
    // Where the window of the screen begins, in pixels.
    QPoint pixelOrigin() const { return QPoint(qRound(m_available.x() * m_ratio), qRound(m_available.y() * m_ratio)); }
    qreal ratio() const { return m_ratio; }

private:
    QSize m_pixels;
    QRect m_geometry;
    QRect m_available;
    qreal m_ratio = 1.0;
    int m_dpi = 160;
};

class QAndroidWindow : public QPlatformWindow {
public:
    QAndroidWindow(QWindow *window, QAndroidScreen *screen);
    ~QAndroidWindow() override;

    void setGeometry(const QRect &rect) override;
    void setVisible(bool visible) override;
    void requestActivateWindow() override;
    void raise() override;
    quintptr winId() const override { return m_id; }
    void *eglNativeWindow() const override;
    bool isTopLevel() const { return m_topLevel; }
    // Whether the window is a dialog that is shown over a window that fills the screen.
    bool isOverlay() const;

    // Every visible top-level window, the topmost last.
    static std::vector<QAndroidWindow *> &stack();
    static QAndroidWindow *top();

private:
    bool isDialog() const;
    // Where a dialog of the size that is asked for goes: centred, inside the screen with a margin.
    QRect dialogGeometry(const QRect &wanted) const;

    QAndroidScreen *m_screen;
    quintptr m_id;
    bool m_topLevel;
};

class QAndroidBackingStore : public QPlatformBackingStore {
public:
    QAndroidBackingStore(QWindow *window, QAndroidScreen *screen);
    ~QAndroidBackingStore() override;

    QPaintDevice *paintDevice() override { return &m_image; }
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    QImage toImage() const override { return m_image; }

private:
    // The backing store of each window, for the frame is made of those of the windows that show.
    static std::vector<QAndroidBackingStore *> &stores();
    static const QAndroidBackingStore *storeOf(const QWindow *window);

    QAndroidScreen *m_screen;
    QImage m_image;
};

class QAndroidIntegration : public QPlatformIntegration {
public:
    // Waits for the activity's window; returns null if there is no activity (the program was not started by one).
    static QAndroidIntegration *create();
    ~QAndroidIntegration() override;

    void initialize() override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QEglNativePlatform eglPlatform() const override;
    QPlatformClipboard *clipboard() const override;
    bool hasInputPanel() const override { return true; }
    Qt::ColorScheme colorScheme() const override {
        return QtAndroid::Glue::instance().nightMode() ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
    }
    void setInputPanelVisible(bool visible) override;
    const char *vulkanSurfaceExtension() const override { return "VK_KHR_android_surface"; }
    quint64 createVulkanSurface(QWindow *window, void *instance,
                                QVulkanGetInstanceProcAddr getInstanceProcAddr) const override;

private:
    explicit QAndroidIntegration(ANativeWindow *window);
    void processEvents();
    void touch(const QtAndroid::Event &event);
    void key(const QtAndroid::Event &event);
    void text(const QtAndroid::Event &event);
    void windowChanged();

    std::unique_ptr<QAndroidScreen> m_screen;
    mutable std::unique_ptr<QPlatformClipboard> m_clipboard;
    std::unique_ptr<QSocketNotifier> m_notifier;
    bool m_surfaceLost = false;
};
