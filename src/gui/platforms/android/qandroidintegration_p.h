// MIT License - see LICENSE
#pragma once

#include "qandroidglue_p.h"
#include "qpa/qplatformbackingstore_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformscreen_p.h"
#include "qpa/qplatformwindow_p.h"

#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtGui/qimage.h>

#include <memory>
#include <vector>

// The Android platform: one screen, which is the native window of the activity, and software rendering into it. The
// windows of the application share that surface; the one shown last is on top.
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

    // Every visible top-level window, the topmost last.
    static std::vector<QAndroidWindow *> &stack();
    static QAndroidWindow *top();

private:
    QAndroidScreen *m_screen;
    quintptr m_id;
    bool m_topLevel;
};

class QAndroidBackingStore : public QPlatformBackingStore {
public:
    QAndroidBackingStore(QWindow *window, QAndroidScreen *screen);

    QPaintDevice *paintDevice() override { return &m_image; }
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    QImage toImage() const override { return m_image; }

private:
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

private:
    explicit QAndroidIntegration(ANativeWindow *window);
    void processEvents();
    void touch(const QtAndroid::Event &event);
    void key(const QtAndroid::Event &event);
    void windowChanged();

    std::unique_ptr<QAndroidScreen> m_screen;
    std::unique_ptr<QSocketNotifier> m_notifier;
    int m_touchPointer = -1;
    QPointF m_touchPosition;
    Qt::MouseButtons m_buttons;
    bool m_surfaceLost = false;
};
