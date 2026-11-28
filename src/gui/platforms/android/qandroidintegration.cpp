// MIT License - see LICENSE
#include "qandroidintegration_p.h"

#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qmimedata.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>

#if QT_CONFIG(vulkan)
#include "qvulkan_p.h"

#include <vulkan/vulkan_android.h>
#endif

#include <android/input.h>
#include <android/keycodes.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

using QtAndroid::Event;
using QtAndroid::Glue;

QAndroidScreen::QAndroidScreen() = default;

bool QAndroidScreen::update(ANativeWindow *window) {
    if (!window)
        return false;
    m_dpi = Glue::instance().density();
    m_ratio = static_cast<qreal>(m_dpi) / 160.0;
    const QSize pixels(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
    const QRect geometry(0, 0, qRound(pixels.width() / m_ratio), qRound(pixels.height() / m_ratio));
    // The window is what the system bars leave free.
    QRect available = geometry;
    int barsBottom = 0;
    const ARect insets = Glue::instance().systemInsets(&barsBottom);
    const QRect free = QRect(QPoint(0, 0), pixels).adjusted(insets.left, insets.top, -insets.right, -insets.bottom);
    if (!free.isEmpty())
        available = QRect(qRound(free.x() / m_ratio), qRound(free.y() / m_ratio), qRound(free.width() / m_ratio),
                          qRound(free.height() / m_ratio));
    QRect unobstructed = available;
    if (!free.isEmpty())
        unobstructed.setHeight(
            std::max(available.height(), qRound((pixels.height() - insets.top - barsBottom) / m_ratio)));
    const bool changed =
        pixels != m_pixels || geometry != m_geometry || available != m_available || unobstructed != m_unobstructed;
    m_unobstructed = unobstructed;
    m_pixels = pixels;
    m_geometry = geometry;
    m_available = available;
    return changed;
}

QSizeF QAndroidScreen::physicalSize() const {
    return QSizeF(m_pixels.width() * 25.4 / m_dpi, m_pixels.height() * 25.4 / m_dpi);
}

std::vector<QAndroidWindow *> &QAndroidWindow::stack() {
    static std::vector<QAndroidWindow *> windows;
    return windows;
}

QAndroidWindow *QAndroidWindow::top() { return stack().empty() ? nullptr : stack().back(); }

QAndroidWindow::QAndroidWindow(QWindow *window, QAndroidScreen *screen)
    : QPlatformWindow(window), m_screen(screen), m_topLevel(window->parent() == nullptr) {
    static std::atomic<quintptr> counter{1};
    m_id = counter.fetch_add(1);
    // A top-level window fills the screen, as every window does on Android.
    if (m_topLevel)
        QPlatformWindow::setGeometry(screen->availableGeometry());
    else
        QPlatformWindow::setGeometry(window->geometry());
}

QAndroidWindow::~QAndroidWindow() {
    auto &s = stack();
    s.erase(std::remove(s.begin(), s.end(), this), s.end());
}

bool QAndroidWindow::isDialog() const {
    const auto type = window()->flags() & Qt::WindowType_Mask;
    return m_topLevel && (type == Qt::Dialog || type == Qt::Sheet) &&
           !window()->windowStates().testFlag(Qt::WindowFullScreen);
}

bool QAndroidWindow::isOverlay() const {
    if (!isDialog())
        return false;
    // Something has to show behind it: a window below that fills the screen. (Not yet in the stack, it is on top.)
    for (const QAndroidWindow *other : stack()) {
        if (other == this)
            break;
        if (!other->isDialog())
            return true;
    }
    return false;
}

QRect QAndroidWindow::dialogGeometry(const QRect &wanted) const {
    // The widths of a Material dialog are 280 to 560 dp, and it keeps clear of the edges.
    const QRect screen = m_screen->availableGeometry();
    // A sheet lies against an edge (QMaterialStyle says which).
    const QString sheet = window()->property("_nt_sheet").toString();
    // While it slides in, it is lower, or further left, than where it rests, by as much as the style says.
    const int away = std::max(0, window()->property("_nt_sheetOffset").toInt());
    if (sheet == QLatin1String("bottom")) {
        const int width = std::max(1, std::min(640, screen.width()));
        const int height = std::clamp(wanted.height(), 1, std::max(1, screen.height() - 72));
        return QRect(screen.x() + (screen.width() - width) / 2,
                     screen.y() + screen.height() - height + std::min(away, height), width, height);
    }
    if (sheet == QLatin1String("drawer")) {
        const int width = std::max(1, std::min(360, screen.width() - 56));
        return QRect(screen.x() - std::min(away, width), screen.y(), width, screen.height());
    }
    const int margin = 24;
    const int widest = std::max(1, std::min(560, screen.width() - 2 * margin));
    const int width = std::clamp(wanted.width(), std::min(280, widest), widest);
    const int height = std::clamp(wanted.height(), 1, std::max(1, screen.height() - 2 * margin));
    return QRect(screen.x() + (screen.width() - width) / 2, screen.y() + (screen.height() - height) / 2, width, height);
}

QRect QAndroidWindow::baseGeometry() const {
    QRect area = m_screen->availableGeometry();
    area.setHeight(
        std::clamp(m_minimumHeight, area.height(), std::max(area.height(), m_screen->unobstructedGeometry().height())));
    return area;
}

bool QAndroidWindow::setInput(int minimumHeight, const QRect &item) {
    const bool other = minimumHeight != m_minimumHeight;
    m_minimumHeight = minimumHeight;
    m_inputItem = item;
    return other && m_topLevel && !isOverlay();
}

int QAndroidWindow::pan() const {
    if (!m_topLevel || m_inputItem.isNull() || isOverlay())
        return 0;
    const int shown = m_screen->availableGeometry().height();
    const int excess = geometry().height() - shown;
    if (excess <= 0)
        return 0;
    constexpr int margin = 12;
    // Far enough for the field to clear the keyboard, but not so far that its top goes out of sight.
    const int wanted = std::min(m_inputItem.y() + m_inputItem.height() + margin - shown, m_inputItem.y() - margin);
    return std::clamp(wanted, 0, excess);
}

void QAndroidWindow::setGeometry(const QRect &rect) {
    if (!m_topLevel) {
        QPlatformWindow::setGeometry(rect);
        return;
    }
    const QRect screenRect = isOverlay() ? dialogGeometry(rect) : baseGeometry();
    const QPoint before = geometry().topLeft();
    QPlatformWindow::setGeometry(screenRect);
    if (rect != screenRect)
        QWindowSystemInterface::handleGeometryChange(window(), screenRect);
    // A dialog that moved, as a sheet that slides in, is put on the screen again where it now is.
    const auto &s = stack();
    if (isOverlay() && screenRect.topLeft() != before && std::find(s.begin(), s.end(), this) != s.end())
        QWindowSystemInterface::handleExposeEvent(window(), QRegion(QRect(QPoint(0, 0), screenRect.size())));
}

void QAndroidWindow::setVisible(bool visible) {
    auto &s = stack();
    if (!m_topLevel) {
        QWindowSystemInterface::handleExposeEvent(window(),
                                                  visible ? QRegion(QRect(QPoint(0, 0), window()->size())) : QRegion());
        return;
    }
    s.erase(std::remove(s.begin(), s.end(), this), s.end());
    if (visible) {
        s.push_back(this);
        // A dialog keeps the size that it asks for; other windows fill the screen.
        setGeometry(window()->geometry());
        QWindowSystemInterface::handleExposeEvent(window(), QRegion(QRect(QPoint(0, 0), window()->size())));
        QWindowSystemInterface::handleWindowActivated(window());
    } else {
        QWindowSystemInterface::handleExposeEvent(window(), QRegion());
        if (QGuiApplication::focusWindow() == window())
            QWindowSystemInterface::handleWindowActivated(nullptr);
        // The windows below show again, from the one that fills the screen up.
        std::size_t first = s.size();
        while (first > 0 && s[--first]->isOverlay()) {
        }
        for (std::size_t i = first; i < s.size(); ++i)
            QWindowSystemInterface::handleExposeEvent(s[i]->window(),
                                                      QRegion(QRect(QPoint(0, 0), s[i]->window()->size())));
        if (QAndroidWindow *below = top())
            QWindowSystemInterface::handleWindowActivated(below->window());
    }
}

void QAndroidWindow::requestActivateWindow() {
    if (m_topLevel)
        QWindowSystemInterface::handleWindowActivated(window());
}

void QAndroidWindow::raise() {
    auto &s = stack();
    const auto it = std::find(s.begin(), s.end(), this);
    if (it == s.end())
        return;
    s.erase(it);
    s.push_back(this);
    QWindowSystemInterface::handleExposeEvent(window(), QRegion(QRect(QPoint(0, 0), window()->size())));
}

void *QAndroidWindow::eglNativeWindow() const { return Glue::instance().window(); }

QAndroidBackingStore::QAndroidBackingStore(QWindow *window, QAndroidScreen *screen)
    : QPlatformBackingStore(window), m_screen(screen) {
    stores().push_back(this);
}

QAndroidBackingStore::~QAndroidBackingStore() {
    auto &all = stores();
    all.erase(std::remove(all.begin(), all.end(), this), all.end());
}

std::vector<QAndroidBackingStore *> &QAndroidBackingStore::stores() {
    static std::vector<QAndroidBackingStore *> all;
    return all;
}

const QAndroidBackingStore *QAndroidBackingStore::storeOf(const QWindow *window) {
    for (const QAndroidBackingStore *store : stores()) {
        if (store->window() == window)
            return store;
    }
    return nullptr;
}

namespace {

// A pixel of an image, 0xAARRGGBB, which memory holds as B, G, R, A, as an opaque pixel of the screen.
std::uint32_t screenPixel(std::uint32_t p) {
    return 0xff000000u | (p & 0x0000ff00u) | ((p >> 16) & 0xffu) | ((p & 0xffu) << 16);
}

// Mixes two screen pixels: `amount` of 256 of the second.
std::uint32_t mixPixels(std::uint32_t below, std::uint32_t above, std::uint32_t amount) {
    const std::uint32_t rest = 256 - amount;
    const std::uint32_t outer = (((below & 0x00ff00ffu) * rest + (above & 0x00ff00ffu) * amount) >> 8) & 0x00ff00ffu;
    const std::uint32_t green = (((below & 0x0000ff00u) * rest + (above & 0x0000ff00u) * amount) >> 8) & 0x0000ff00u;
    return 0xff000000u | outer | green;
}

// A run of pixels of an image as pixels of the screen. A plain loop, so that the compiler does several at a time.
void copyPixels(const std::uint32_t *source, std::uint32_t *row, int count) {
    for (int i = 0; i < count; ++i)
        row[i] = screenPixel(source[i]);
}

} // namespace

void QAndroidBackingStore::resize(const QSize &size, const QRegion &staticContents) {
    (void)staticContents;
    const qreal ratio = m_screen->ratio();
    const QSize native(qRound(size.width() * ratio), qRound(size.height() * ratio));
    if (m_image.size() == native && m_image.devicePixelRatio() == ratio)
        return;
    QImage image(native, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::white);
    m_image = image;
}

// Makes the frame in the buffer of the native window: the window that fills the screen, and on it the dialogs, each
// with what is below it dimmed. Windows under the one that fills the screen paint into their own images only.
void QAndroidBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset) {
    (void)region;
    (void)offset;
    const auto &windows = QAndroidWindow::stack();
    if (windows.empty())
        return;
    std::size_t base = windows.size() - 1;
    while (base > 0 && windows[base]->isOverlay())
        --base;
    if (std::none_of(windows.begin() + static_cast<std::ptrdiff_t>(base), windows.end(),
                     [window](const QAndroidWindow *w) { return w->window() == window; }))
        return;
    ANativeWindow *native = Glue::instance().window();
    if (!native)
        return;
    // Opaque 32-bit pixels, in the size of the window; the screen is not scaled by the compositor. The window itself is
    // asked every time: after the application was in the background, a new window can have the address of the old
    // one, and it starts with 16-bit pixels.
    if (ANativeWindow_getFormat(native) != WINDOW_FORMAT_RGBX_8888)
        ANativeWindow_setBuffersGeometry(native, 0, 0, WINDOW_FORMAT_RGBX_8888);
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(native, &buffer, nullptr) != 0)
        return;
    if (buffer.format != WINDOW_FORMAT_RGBX_8888 && buffer.format != WINDOW_FORMAT_RGBA_8888) {
        // Four bytes a pixel would not fit; leave the buffer alone.
        ANativeWindow_unlockAndPost(native);
        return;
    }
    auto *pixels = static_cast<std::uint32_t *>(buffer.bits);
    const auto line = [&](int y) { return pixels + static_cast<std::ptrdiff_t>(y) * buffer.stride; };
    // The windows lie inside the system bars.
    const QPoint origin = m_screen->pixelOrigin();
    const QRect inside(origin, QSize(buffer.width - origin.x(), buffer.height - origin.y()));
    const qreal ratio = m_screen->ratio();
    // Black where the window that fills the screen is not: under the system bars. Every time: the buffers take turns,
    // and one may still hold a frame that was painted before the sizes of the bars were known. The whole frame is made
    // each time: to keep what has not changed, the system would copy it from the frame before, which takes longer.
    const QAndroidBackingStore *bottom = storeOf(windows[base]->window());
    const int slid = qRound(windows[base]->pan() * ratio); // in pixels
    const QRect under =
        bottom && !bottom->m_image.isNull() ? QRect(origin, bottom->m_image.size() - QSize(0, slid)) : QRect();
    for (int y = 0; y < buffer.height; ++y) {
        const bool beside = y >= under.top() && y <= under.bottom();
        const int before = beside ? std::clamp(under.left(), 0, buffer.width) : buffer.width;
        const int after = beside ? std::clamp(under.right() + 1, 0, buffer.width) : buffer.width;
        std::fill(line(y), line(y) + before, 0xff000000u);
        std::fill(line(y) + after, line(y) + buffer.width, 0xff000000u);
    }
    for (std::size_t i = base; i < windows.size(); ++i) {
        const QAndroidBackingStore *store = storeOf(windows[i]->window());
        if (!store || store->m_image.isNull())
            continue;
        const QImage &image = store->m_image;
        QPoint at = origin;
        double radius = 0;
        QString sheet;
        if (i == base)
            at.ry() -= slid;
        if (i > base) {
            // The scrim of Material: black at 32 percent over what is below the dialog.
            for (int y = std::max(0, inside.top()); y < buffer.height; ++y) {
                std::uint32_t *row = line(y);
                for (int x = std::max(0, inside.left()); x < buffer.width; ++x)
                    row[x] = mixPixels(row[x], 0xff000000u, 82);
            }
            const QPoint corner = windows[i]->geometry().topLeft() - m_screen->availableGeometry().topLeft();
            at += QPoint(qRound(corner.x() * ratio), qRound(corner.y() * ratio));
            // A style may ask for rounded corners, in dp (QMaterialStyle does for dialogs).
            radius = std::min(windows[i]->window()->property("_nt_cornerRadius").toDouble() * ratio,
                              std::min(image.width(), image.height()) / 2.0);
            sheet = windows[i]->window()->property("_nt_sheet").toString();
        }
        const int left = std::max(0, -at.x());
        const int right = std::min(image.width(), buffer.width - at.x());
        // What is slid up does not go over the bar at the top.
        const int first = std::max({0, -at.y(), i == base ? origin.y() - at.y() : 0});
        for (int y = first; y < std::min(image.height(), buffer.height - at.y()); ++y) {
            const auto *source = reinterpret_cast<const std::uint32_t *>(image.constScanLine(y));
            std::uint32_t *row = line(at.y() + y) + at.x();
            // How far into a rounded corner this row and a column are.
            // The corners of a sheet that touch its edge of the screen are square.
            const bool squareBottom = sheet == QLatin1String("bottom");
            const bool squareLeft = sheet == QLatin1String("drawer");
            const double dy = radius - (squareBottom ? y + 0.5 : std::min(y + 0.5, image.height() - y - 0.5));
            if (dy <= 0) {
                // Not at a corner: the whole row is copied.
                if (right > left)
                    copyPixels(source + left, row + left, right - left);
                continue;
            }
            for (int x = left; x < right; ++x) {
                const double fromRight = image.width() - x - 0.5;
                const double dx = radius - (squareLeft ? fromRight : std::min(x + 0.5, fromRight));
                if (dx <= 0) {
                    row[x] = screenPixel(source[x]);
                    continue;
                }
                const double covered = std::clamp(radius - std::hypot(dx, dy) + 0.5, 0.0, 1.0);
                row[x] = mixPixels(row[x], screenPixel(source[x]), static_cast<std::uint32_t>(covered * 256));
            }
        }
    }
    ANativeWindow_unlockAndPost(native);
}

QAndroidIntegration *QAndroidIntegration::create() {
    Glue &glue = Glue::instance();
    if (!glue.activity()) {
        std::fprintf(stderr, "Nt GUI (android): the program was not started by an activity\n");
        return nullptr;
    }
    ANativeWindow *window = glue.waitForWindow(15000);
    if (!window) {
        std::fprintf(stderr, "Nt GUI (android): the activity has no window\n");
        return nullptr;
    }
    return new QAndroidIntegration(window);
}

QAndroidIntegration::QAndroidIntegration(ANativeWindow *window) : m_screen(std::make_unique<QAndroidScreen>()) {
    m_screen->update(window);
}

QAndroidIntegration::~QAndroidIntegration() = default;

void QAndroidIntegration::initialize() {
    QWindowSystemInterface::handleScreenAdded(m_screen.get());
    Glue &glue = Glue::instance();
    m_notifier = std::make_unique<QSocketNotifier>(glue.eventDescriptor(), QSocketNotifier::Read);
    QObject::connect(m_notifier.get(), &QSocketNotifier::activated, [this] { processEvents(); });
}

QPlatformWindow *QAndroidIntegration::createPlatformWindow(QWindow *window) const {
    return new QAndroidWindow(window, m_screen.get());
}

QPlatformBackingStore *QAndroidIntegration::createPlatformBackingStore(QWindow *window) const {
    return new QAndroidBackingStore(window, m_screen.get());
}

namespace {

// The clipboard of Android: plain text, kept by the system. The data of a mode other than Clipboard stays in the
// process.
class QAndroidClipboard : public QPlatformClipboard {
public:
    QMimeData *mimeData(QClipboard::Mode mode) override {
        if (mode == QClipboard::Clipboard) {
            const QString text = QString::fromUtf8(Glue::instance().clipboardText());
            QMimeData *current = m_data[mode].get();
            if (text.isEmpty() && !current)
                return nullptr;
            if (!current || (!text.isEmpty() && current->text() != text)) {
                auto *data = new QMimeData;
                if (!text.isEmpty())
                    data->setText(text);
                m_data[mode].reset(data);
            }
        }
        return m_data[mode].get();
    }
    void setMimeData(QMimeData *data, QClipboard::Mode mode) override {
        m_data[mode].reset(data);
        if (mode == QClipboard::Clipboard && data && data->hasText())
            Glue::instance().setClipboardText(data->text().toStdString());
        emitChanged(mode);
    }
    bool supportsMode(QClipboard::Mode mode) const override { return mode == QClipboard::Clipboard; }
    bool ownsMode(QClipboard::Mode mode) const override { return mode == QClipboard::Clipboard && m_data[mode]; }
};

} // namespace

QPlatformClipboard *QAndroidIntegration::clipboard() const {
    if (!m_clipboard)
        m_clipboard = std::make_unique<QAndroidClipboard>();
    return m_clipboard.get();
}

void QAndroidIntegration::setInputPanelVisible(bool visible) { Glue::instance().setSoftInputVisible(visible); }

void QAndroidIntegration::setInputItem(QWindow *window, int minimumHeight, const QRect &item) {
    const auto &windows = QAndroidWindow::stack();
    bool slid = false;
    QAndroidWindow *resized = nullptr;
    for (QAndroidWindow *each : windows) {
        if (each->window() != window)
            continue;
        const int before = each->pan();
        if (each->setInput(minimumHeight, item))
            resized = each;
        slid = each->pan() != before;
    }
    if (resized) {
        const QRect before = resized->geometry();
        resized->setGeometry(resized->window()->geometry());
        if (resized->geometry() != before) {
            QWindowSystemInterface::handleGeometryChange(resized->window(), resized->geometry());
            slid = true;
        }
    }
    // The frame is made anew, from the window that fills the screen up.
    if (slid) {
        for (QAndroidWindow *each : std::vector<QAndroidWindow *>(windows))
            QWindowSystemInterface::handleExposeEvent(each->window(),
                                                      QRegion(QRect(QPoint(0, 0), each->geometry().size())));
    }
}

// As QAndroidWindow::dialogGeometry() places them.
int QAndroidIntegration::maximumDialogWidth() const {
    return std::max(1, std::min(560, m_screen->availableGeometry().width() - 48));
}

void QAndroidIntegration::setInputMethodHints(Qt::InputMethodHints hints) {
    Glue::instance().setSoftInputHints(static_cast<int>(hints.toInt()));
}

// Vulkan presents to the native window of the activity through VK_KHR_android_surface. The surface belongs to the
// platform window, and goes when the window is destroyed or when Android takes the native window away.
quint64 QAndroidIntegration::createVulkanSurface(QWindow *window, void *instance,
                                                 QVulkanGetInstanceProcAddr getInstanceProcAddr) const {
#if QT_CONFIG(vulkan)
    auto *platformWindow = static_cast<QAndroidWindow *>(QtPrivate::GuiAccess::platformWindow(window));
    ANativeWindow *native = Glue::instance().window();
    if (!platformWindow || !native || !instance || !getInstanceProcAddr)
        return 0;
    if (platformWindow->vulkanSurface)
        return platformWindow->vulkanSurface;
    auto *vkInstance = static_cast<VkInstance>(instance);
    auto getProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(getInstanceProcAddr);
    auto createSurface =
        reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(getProcAddr(vkInstance, "vkCreateAndroidSurfaceKHR"));
    auto destroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(getProcAddr(vkInstance, "vkDestroySurfaceKHR"));
    if (!createSurface || !destroySurface) {
        qWarning("Nt GUI (android): the Vulkan instance has no VK_KHR_android_surface");
        return 0;
    }
    VkAndroidSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    info.window = native;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkResult result = createSurface(vkInstance, &info, nullptr, &surface);
    if (result != VK_SUCCESS) {
        qWarning("Nt GUI (android): vkCreateAndroidSurfaceKHR failed: %d", static_cast<int>(result));
        return 0;
    }
    platformWindow->vulkanSurface = QtPrivate::Vulkan::handleToU64(surface);
    platformWindow->destroyVulkanSurface = [vkInstance, destroySurface, surface] {
        destroySurface(vkInstance, surface, nullptr);
    };
    return platformWindow->vulkanSurface;
#else
    return QPlatformIntegration::createVulkanSurface(window, instance, getInstanceProcAddr);
#endif
}

// OpenGL ES through the EGL library of the system; windows get EGL window surfaces of the activity's native window.
QEglNativePlatform QAndroidIntegration::eglPlatform() const {
    QEglNativePlatform native;
    native.platform = 0x3141; // EGL_PLATFORM_ANDROID_KHR
    native.windowSurfaces = true;
    return native;
}

void QAndroidIntegration::processEvents() {
    Glue &glue = Glue::instance();
    glue.drainDescriptor();
    Event event;
    while (glue.next(&event)) {
        switch (event.kind) {
        case Event::Touch:
            touch(event);
            break;
        case Event::Key:
            key(event);
            break;
        case Event::Text:
            text(event);
            break;
        case Event::WindowCreated:
        case Event::WindowResized:
        case Event::ConfigChanged:
            if (event.kind == Event::ConfigChanged)
                QWindowSystemInterface::handleColorSchemeChange(colorScheme());
            windowChanged();
            break;
        case Event::WindowDestroyed:
            // Nothing may draw into the window from here on; the UI thread waits for this. The EGL surfaces
            // of OpenGL windows are made of the window that is going away.
            for (QAndroidWindow *w : QAndroidWindow::stack()) {
                QWindowSystemInterface::handleExposeEvent(w->window(), QRegion());
                QtPrivate::GuiAccess::surfaceAboutToBeDestroyed(w->window());
#if QT_CONFIG(vulkan)
                QtPrivate::GuiAccess::releaseVulkanWindow(w->window());
#endif
                if (w->destroyVulkanSurface) {
                    w->destroyVulkanSurface();
                    w->destroyVulkanSurface = nullptr;
                    w->vulkanSurface = 0;
                }
            }
            m_surfaceLost = true;
            glue.windowReleased();
            break;
        case Event::Pause:
            QtPrivate::GuiAccess::setApplicationState(Qt::ApplicationInactive);
            break;
        case Event::Resume:
            QtPrivate::GuiAccess::setApplicationState(Qt::ApplicationActive);
            break;
        case Event::Close:
            if (QGuiApplication::instance())
                QGuiApplication::quit();
            break;
        }
    }
}

// The window was made or resized, or the screen turned: every window follows the new size of the screen.
void QAndroidIntegration::windowChanged() {
    ANativeWindow *native = Glue::instance().window();
    if (!native)
        return;
    const bool surfaceReturned = m_surfaceLost;
    m_surfaceLost = false;
    const bool changed = m_screen->update(native);
    if (changed)
        QWindowSystemInterface::handleScreenGeometryChange(m_screen.get());
    if (!changed && !surfaceReturned)
        return;
    for (QAndroidWindow *w : QAndroidWindow::stack()) {
        // A dialog is centred in the new screen; other windows take its size.
        w->setGeometry(w->window()->geometry());
        QWindowSystemInterface::handleGeometryChange(w->window(), w->geometry());
        QWindowSystemInterface::handleExposeEvent(w->window(), QRegion(QRect(QPoint(0, 0), w->geometry().size())));
    }
}

// Every finger goes to the window on top as a touch event; a window that does not take touch events gets the first
// finger as the mouse.
void QAndroidIntegration::touch(const Event &event) {
    QAndroidWindow *top = QAndroidWindow::top();
    if (!top)
        return;
    QWindow *window = top->window();
    // A tap beside a sheet dismisses it, and is not for what is below.
    if (m_dismissingTouch) {
        if (event.action == AMOTION_EVENT_ACTION_UP || event.action == AMOTION_EVENT_ACTION_CANCEL)
            m_dismissingTouch = false;
        return;
    }
    if (event.action == AMOTION_EVENT_ACTION_DOWN && !event.points.empty() && top->isOverlay() &&
        !window->property("_nt_sheet").toString().isEmpty()) {
        const auto &first = event.points.front();
        const QPointF at(static_cast<qreal>(first.x) / m_screen->ratio(),
                         static_cast<qreal>(first.y) / m_screen->ratio());
        if (!QRectF(window->geometry()).contains(at)) {
            m_dismissingTouch = true;
            QWindowSystemInterface::handleCloseEvent(window);
            return;
        }
    }
    if (event.action == AMOTION_EVENT_ACTION_CANCEL) {
        QWindowSystemInterface::handleTouchCancel(window);
        return;
    }
    // A press may move the focus and with it how far the window is slid: a gesture keeps the one it began with.
    if (event.action == AMOTION_EVENT_ACTION_DOWN)
        m_touchPan = top->pan();
    QList<QEventPoint> points;
    for (const Event::TouchPoint &p : event.points) {
        const QPointF global(static_cast<qreal>(p.x) / m_screen->ratio(), static_cast<qreal>(p.y) / m_screen->ratio());
        static const QEventPoint::State states[] = {QEventPoint::Stationary, QEventPoint::Pressed, QEventPoint::Updated,
                                                    QEventPoint::Released};
        points.append(QEventPoint(p.id, states[p.state & 3],
                                  global - QPointF(window->geometry().topLeft()) + QPointF(0, m_touchPan), global));
    }
    QWindowSystemInterface::handleTouchEvent(window, points);
}

namespace {

// The Qt key, and the text it types, for an Android key code.
int qtKey(int code, bool shift, QString *text) {
    if (code >= AKEYCODE_A && code <= AKEYCODE_Z) {
        const int letter = code - AKEYCODE_A;
        *text = QString(QChar(static_cast<char16_t>((shift ? 'A' : 'a') + letter)));
        return Qt::Key_A + letter;
    }
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9) {
        static const char shifted[] = ")!@#$%^&*(";
        const int digit = code - AKEYCODE_0;
        *text = QString(QChar(static_cast<char16_t>(shift ? shifted[digit] : '0' + digit)));
        return Qt::Key_0 + digit;
    }
    switch (code) {
    case AKEYCODE_SPACE:
        *text = QString(QChar(' '));
        return Qt::Key_Space;
    case AKEYCODE_ENTER:
    case AKEYCODE_NUMPAD_ENTER:
        *text = QString(QChar('\r'));
        return Qt::Key_Return;
    case AKEYCODE_TAB:
        *text = QString(QChar('\t'));
        return Qt::Key_Tab;
    case AKEYCODE_DEL:
        return Qt::Key_Backspace;
    case AKEYCODE_FORWARD_DEL:
        return Qt::Key_Delete;
    case AKEYCODE_ESCAPE:
        return Qt::Key_Escape;
    case AKEYCODE_BACK:
        return Qt::Key_Back;
    case AKEYCODE_MENU:
        return Qt::Key_Menu;
    case AKEYCODE_DPAD_LEFT:
        return Qt::Key_Left;
    case AKEYCODE_DPAD_RIGHT:
        return Qt::Key_Right;
    case AKEYCODE_DPAD_UP:
        return Qt::Key_Up;
    case AKEYCODE_DPAD_DOWN:
        return Qt::Key_Down;
    case AKEYCODE_MOVE_HOME:
        return Qt::Key_Home;
    case AKEYCODE_MOVE_END:
        return Qt::Key_End;
    case AKEYCODE_PAGE_UP:
        return Qt::Key_PageUp;
    case AKEYCODE_PAGE_DOWN:
        return Qt::Key_PageDown;
    case AKEYCODE_COMMA:
        *text = QString(QChar(shift ? '<' : ','));
        return shift ? Qt::Key_Less : Qt::Key_Comma;
    case AKEYCODE_PERIOD:
        *text = QString(QChar(shift ? '>' : '.'));
        return shift ? Qt::Key_Greater : Qt::Key_Period;
    case AKEYCODE_MINUS:
        *text = QString(QChar(shift ? '_' : '-'));
        return shift ? Qt::Key_Underscore : Qt::Key_Minus;
    case AKEYCODE_EQUALS:
        *text = QString(QChar(shift ? '+' : '='));
        return shift ? Qt::Key_Plus : Qt::Key_Equal;
    case AKEYCODE_SLASH:
        *text = QString(QChar(shift ? '?' : '/'));
        return shift ? Qt::Key_Question : Qt::Key_Slash;
    case AKEYCODE_SEMICOLON:
        *text = QString(QChar(shift ? ':' : ';'));
        return shift ? Qt::Key_Colon : Qt::Key_Semicolon;
    case AKEYCODE_APOSTROPHE:
        *text = QString(QChar(shift ? '"' : '\''));
        return shift ? Qt::Key_QuoteDbl : Qt::Key_Apostrophe;
    case AKEYCODE_AT:
        *text = QString(QChar('@'));
        return Qt::Key_At;
    case AKEYCODE_PLUS:
        *text = QString(QChar('+'));
        return Qt::Key_Plus;
    default:
        return Qt::Key_unknown;
    }
}

} // namespace

// Text from the on-screen keyboard goes to the window that has the focus, as typed characters.
void QAndroidIntegration::text(const Event &event) {
    QAndroidWindow *top = QAndroidWindow::top();
    if (!top)
        return;
    QWindow *window = QGuiApplication::focusWindow() ? QGuiApplication::focusWindow() : top->window();
    const QString text = QString::fromUtf8(event.text.c_str());
    if (text.isEmpty())
        return;
    QWindowSystemInterface::handleKeyEvent(window, QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, text);
    QWindowSystemInterface::handleKeyEvent(window, QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, text);
}

void QAndroidIntegration::key(const Event &event) {
    QAndroidWindow *top = QAndroidWindow::top();
    if (!top)
        return;
    QWindow *window = QGuiApplication::focusWindow() ? QGuiApplication::focusWindow() : top->window();
    Qt::KeyboardModifiers modifiers;
    if (event.meta & AMETA_SHIFT_ON)
        modifiers |= Qt::ShiftModifier;
    if (event.meta & AMETA_CTRL_ON)
        modifiers |= Qt::ControlModifier;
    if (event.meta & AMETA_ALT_ON)
        modifiers |= Qt::AltModifier;
    QString text;
    const int key = qtKey(event.keyCode, modifiers.testFlag(Qt::ShiftModifier), &text);
    if (key == Qt::Key_unknown)
        return;
    const bool press = event.action == AKEY_EVENT_ACTION_DOWN;
    QWindowSystemInterface::handleKeyEvent(window, press ? QEvent::KeyPress : QEvent::KeyRelease, key, modifiers, text);
    // Back closes the window on top, as it does in a Qt application whose window does not take the key.
    if (key == Qt::Key_Back && !press)
        QWindowSystemInterface::handleCloseEvent(top->window());
}
