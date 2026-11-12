// MIT License - see LICENSE
#include "qoffscreenintegration_p.h"

#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"

#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>

#include <atomic>
#include <cstdlib>
#include <map>
#include <mutex>

quintptr QOffscreenWindow::nextId() {
    static std::atomic<quintptr> counter{1};
    return counter.fetch_add(1);
}

QOffscreenScreen::QOffscreenScreen() {
    if (const char *env = std::getenv("QT_SCALE_FACTOR")) {
        const double factor = std::atof(env);
        if (factor > 0.0)
            m_ratio = factor;
    }
}

void QOffscreenWindow::setGeometry(const QRect &rect) {
    QPlatformWindow::setGeometry(rect);
    if (m_visible)
        QWindowSystemInterface::handleExposeEvent(window(), QRegion(QRect(QPoint(0, 0), rect.size())));
}

Qt::CursorShape QOffscreenWindow::cursorOf(const QWindow *window) {
    const auto *platform = static_cast<const QOffscreenWindow *>(QtPrivate::GuiAccess::platformWindow(window));
    return platform ? platform->m_cursor : Qt::ArrowCursor;
}

void QOffscreenWindow::setVisible(bool visible) {
    m_visible = visible;
    const QRegion region = visible ? QRegion(QRect(QPoint(0, 0), window()->size())) : QRegion();
    QWindowSystemInterface::handleExposeEvent(window(), region);
    if (visible)
        QWindowSystemInterface::handleWindowActivated(window());
    else if (QGuiApplication::focusWindow() == window())
        QWindowSystemInterface::handleWindowActivated(nullptr);
}

namespace {

std::mutex g_storeMutex;

std::map<const QWindow *, QOffscreenBackingStore *> &stores() {
    static auto *map = new std::map<const QWindow *, QOffscreenBackingStore *>;
    return *map;
}

} // namespace

QOffscreenBackingStore::QOffscreenBackingStore(QWindow *window) : QPlatformBackingStore(window) {
    std::lock_guard<std::mutex> lock(g_storeMutex);
    stores()[window] = this;
}

QOffscreenBackingStore::~QOffscreenBackingStore() {
    std::lock_guard<std::mutex> lock(g_storeMutex);
    stores().erase(window());
}

QImage QOffscreenBackingStore::imageOf(const QWindow *window) {
    std::lock_guard<std::mutex> lock(g_storeMutex);
    const auto it = stores().find(window);
    return it == stores().end() ? QImage() : it->second->toImage();
}

void QOffscreenBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset) {
    (void)window;
    (void)region;
    (void)offset;
}

void QOffscreenBackingStore::resize(const QSize &size, const QRegion &staticContents) {
    (void)staticContents;
    const qreal ratio = window()->devicePixelRatio();
    const QSize native(qRound(size.width() * ratio), qRound(size.height() * ratio));
    if (m_image.size() == native && m_image.devicePixelRatio() == ratio)
        return;
    QImage image(native, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::white);
    m_image = image;
}

QOffscreenIntegration::QOffscreenIntegration() : m_screen(new QOffscreenScreen) {}

QOffscreenIntegration::~QOffscreenIntegration() { delete m_screen; }

void QOffscreenIntegration::initialize() { QWindowSystemInterface::handleScreenAdded(m_screen); }

QPlatformWindow *QOffscreenIntegration::createPlatformWindow(QWindow *window) const {
    return new QOffscreenWindow(window);
}

QPlatformBackingStore *QOffscreenIntegration::createPlatformBackingStore(QWindow *window) const {
    return new QOffscreenBackingStore(window);
}

// OpenGL without a window system: a display without surfaces (or of the first device), where each window renders
// into a framebuffer object.
QEglNativePlatform QOffscreenIntegration::eglPlatform() const {
    QEglNativePlatform native;
    native.platform = 0x31DD; // EGL_PLATFORM_SURFACELESS_MESA
    return native;
}
