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
    void setCursor(Qt::CursorShape shape) override { m_cursor = shape; }
    // The cursor that `window` shows, for tests: its own, or the override cursor of the application.
    static Qt::CursorShape cursorOf(const QWindow *window);
    void setWindowFlags(Qt::WindowFlags flags) override { m_flags = flags; }
    // A maximized or full screen window takes the geometry of its screen, and gets its own back when it is normal.
    void setWindowStates(Qt::WindowStates states) override;
    void setParent(const QPlatformWindow *parent) override { m_parent = parent; }
    // The platform window of the parent window, for tests; null for a top-level window.
    const QPlatformWindow *parent() const { return m_parent; }
    void raise() override { m_stacking = 1; }
    void lower() override { m_stacking = -1; }
    // What reached the platform, for tests: the flags, and 1 or -1 after the last raise() or lower().
    Qt::WindowFlags flags() const { return m_flags; }
    int stacking() const { return m_stacking; }

private:
    Qt::CursorShape m_cursor = Qt::ArrowCursor;
    Qt::WindowFlags m_flags = Qt::Window;
    int m_stacking = 0;
    const QPlatformWindow *m_parent = nullptr;
    QRect m_normalGeometry;
    bool m_filling = false; // the window has the geometry of its screen
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
    QEglNativePlatform eglPlatform() const override;

    // For tests of what the widgets tell a platform with an on-screen keyboard: made present, this one records it.
    struct InputPanel {
        bool present = false;
        bool visible = false;
        int requests = 0; // to show it
        Qt::InputMethodHints hints;
        QWindow *window = nullptr;
        int minimumHeight = 0;
        QRect item;
    };
    InputPanel inputPanel;
    bool hasInputPanel() const override { return inputPanel.present; }
    void setInputPanelVisible(bool visible) override {
        inputPanel.visible = visible;
        inputPanel.requests += visible ? 1 : 0;
    }
    void setInputMethodHints(Qt::InputMethodHints hints) override { inputPanel.hints = hints; }
    void setInputItem(QWindow *window, int minimumHeight, const QRect &item) override {
        inputPanel.window = window;
        inputPanel.minimumHeight = minimumHeight;
        inputPanel.item = item;
    }

private:
    QOffscreenScreen *m_screen;
};
