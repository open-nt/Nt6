// MIT License - see LICENSE
#include "qandroidintegration_p.h"

#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>

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
    static ANativeWindow *formatted = nullptr;
    if (formatted != native) {
        // Opaque pixels, in the size of the window; the screen is not scaled by the compositor.
        ANativeWindow_setBuffersGeometry(native, 0, 0, WINDOW_FORMAT_RGBX_8888);
        formatted = native;
    }
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(native, &buffer, nullptr) != 0)
        return;
    // The window lies inside the system bars.
    const QPoint origin = m_screen->pixelOrigin();
    const int width = std::min(buffer.width - origin.x(), m_image.width());
    const int height = std::min(buffer.height - origin.y(), m_image.height());
    auto *destination = static_cast<std::uint8_t *>(buffer.bits) +
                        (static_cast<std::ptrdiff_t>(origin.y()) * buffer.stride + origin.x()) * 4;
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
        case Event::WindowCreated:
        case Event::WindowResized:
        case Event::ConfigChanged:
            windowChanged();
            break;
        case Event::WindowDestroyed:
            // Nothing may draw into the window from here on; the UI thread waits for this. The EGL surfaces
            // of OpenGL windows are made of the window that is going away.
            for (QAndroidWindow *w : QAndroidWindow::stack())
                QtPrivate::GuiAccess::surfaceAboutToBeDestroyed(w->window());
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

void QAndroidIntegration::touch(const Event &event) {
    QAndroidWindow *top = QAndroidWindow::top();
    if (!top)
        return;
    QWindow *window = top->window();
    const QPointF global(static_cast<qreal>(event.x) / m_screen->ratio(),
                         static_cast<qreal>(event.y) / m_screen->ratio());
    const QPointF position = global - QPointF(window->geometry().topLeft());
    switch (event.action) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        if (m_touchPointer >= 0)
            return; // the first finger is the pointer; others are ignored
        m_touchPointer = event.pointerId;
        m_touchPosition = global;
        m_buttons = Qt::LeftButton;
        QWindowSystemInterface::handleEnterEvent(window, position, global);
        QWindowSystemInterface::handleMouseEvent(window, position, global, m_buttons, Qt::LeftButton,
                                                 QEvent::MouseButtonPress, Qt::NoModifier);
        break;
    case AMOTION_EVENT_ACTION_MOVE:
        if (event.pointerId != m_touchPointer)
            return;
        m_touchPosition = global;
        QWindowSystemInterface::handleMouseEvent(window, position, global, m_buttons, Qt::NoButton, QEvent::MouseMove,
                                                 Qt::NoModifier);
        break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
    case AMOTION_EVENT_ACTION_CANCEL:
        if (event.pointerId != m_touchPointer && event.action != AMOTION_EVENT_ACTION_CANCEL)
            return;
        m_touchPointer = -1;
        m_buttons = Qt::NoButton;
        QWindowSystemInterface::handleMouseEvent(window, position, global, Qt::NoButton, Qt::LeftButton,
                                                 QEvent::MouseButtonRelease, Qt::NoModifier);
        QWindowSystemInterface::handleLeaveEvent(window);
        break;
    default:
        break;
    }
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
