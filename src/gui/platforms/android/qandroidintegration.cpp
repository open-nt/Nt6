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
    const ARect insets = Glue::instance().systemInsets();
    const QRect free = QRect(QPoint(0, 0), pixels).adjusted(insets.left, insets.top, -insets.right, -insets.bottom);
    if (!free.isEmpty())
        available = QRect(qRound(free.x() / m_ratio), qRound(free.y() / m_ratio), qRound(free.width() / m_ratio),
                          qRound(free.height() / m_ratio));
    const bool changed = pixels != m_pixels || geometry != m_geometry || available != m_available;
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

void QAndroidWindow::setGeometry(const QRect &rect) {
    if (!m_topLevel) {
        QPlatformWindow::setGeometry(rect);
        return;
    }
    const QRect screenRect = m_screen->availableGeometry();
    QPlatformWindow::setGeometry(screenRect);
    if (rect != screenRect)
        QWindowSystemInterface::handleGeometryChange(window(), screenRect);
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
        setGeometry(m_screen->availableGeometry());
        QWindowSystemInterface::handleExposeEvent(window(), QRegion(QRect(QPoint(0, 0), window()->size())));
        QWindowSystemInterface::handleWindowActivated(window());
    } else {
        QWindowSystemInterface::handleExposeEvent(window(), QRegion());
        if (QGuiApplication::focusWindow() == window())
            QWindowSystemInterface::handleWindowActivated(nullptr);
        // The window below shows again.
        if (QAndroidWindow *below = top()) {
            QWindowSystemInterface::handleExposeEvent(below->window(),
                                                      QRegion(QRect(QPoint(0, 0), below->window()->size())));
            QWindowSystemInterface::handleWindowActivated(below->window());
        }
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
    : QPlatformBackingStore(window), m_screen(screen) {}

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

// Copies the image into the buffer of the native window. Only the window on top shows; the others paint into their
// own images until they are raised.
void QAndroidBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset) {
    (void)region;
    (void)offset;
    QAndroidWindow *top = QAndroidWindow::top();
    if (!top || top->window() != window || m_image.isNull())
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
    // The window lies inside the system bars.
    const QPoint origin = m_screen->pixelOrigin();
    const int width = std::min(buffer.width - origin.x(), m_image.width());
    const int height = std::min(buffer.height - origin.y(), m_image.height());
    auto *destination = static_cast<std::uint8_t *>(buffer.bits) +
                        (static_cast<std::ptrdiff_t>(origin.y()) * buffer.stride + origin.x()) * 4;
    // The rest of the screen, under the system bars, is black. It is cleared every time: the buffers take turns, and
    // one may still hold a frame that was painted before the sizes of the bars were known.
    auto *pixels = static_cast<std::uint32_t *>(buffer.bits);
    const int left = std::clamp(origin.x(), 0, buffer.width);
    const int right = std::clamp(origin.x() + std::max(width, 0), left, buffer.width);
    for (int y = 0; y < buffer.height; ++y) {
        std::uint32_t *line = pixels + static_cast<std::ptrdiff_t>(y) * buffer.stride;
        if (y < origin.y() || y >= origin.y() + height) {
            std::fill_n(line, buffer.width, 0xff000000u);
        } else {
            std::fill_n(line, left, 0xff000000u);
            std::fill_n(line + right, buffer.width - right, 0xff000000u);
        }
    }
    for (int y = 0; y < height; ++y) {
        const auto *source = reinterpret_cast<const std::uint32_t *>(m_image.constScanLine(y));
        auto *row = reinterpret_cast<std::uint32_t *>(destination + static_cast<std::ptrdiff_t>(y) * buffer.stride * 4);
        for (int x = 0; x < width; ++x) {
            const std::uint32_t p = source[x]; // 0xAARRGGBB, which memory holds as B, G, R, A
            row[x] = 0xff000000u | (p & 0x0000ff00u) | ((p >> 16) & 0xffu) | ((p & 0xffu) << 16);
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
        case Event::Resume:
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
        QWindowSystemInterface::handleGeometryChange(w->window(), m_screen->availableGeometry());
        QWindowSystemInterface::handleExposeEvent(w->window(),
                                                  QRegion(QRect(QPoint(0, 0), m_screen->availableGeometry().size())));
    }
}

// Every finger goes to the window on top as a touch event; a window that does not take touch events gets the first
// finger as the mouse.
void QAndroidIntegration::touch(const Event &event) {
    QAndroidWindow *top = QAndroidWindow::top();
    if (!top)
        return;
    QWindow *window = top->window();
    if (event.action == AMOTION_EVENT_ACTION_CANCEL) {
        QWindowSystemInterface::handleTouchCancel(window);
        return;
    }
    QList<QEventPoint> points;
    for (const Event::TouchPoint &p : event.points) {
        const QPointF global(static_cast<qreal>(p.x) / m_screen->ratio(), static_cast<qreal>(p.y) / m_screen->ratio());
        static const QEventPoint::State states[] = {QEventPoint::Stationary, QEventPoint::Pressed, QEventPoint::Updated,
                                                    QEventPoint::Released};
        points.append(QEventPoint(p.id, states[p.state & 3], global - QPointF(window->geometry().topLeft()), global));
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
