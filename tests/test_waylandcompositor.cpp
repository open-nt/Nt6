// MIT License - see LICENSE
//
// Nt Wayland Compositor against the reference client library: libwayland-client, with the xdg-shell client code
// that wayland-scanner generates, runs a client on a worker thread while the compositor serves it from the main
// thread's event loop. What the client sees is checked against what the compositor did, and the other way round.
#include <QtCore>
#include <QtGui/qimage.h>
#include <QtTest/qtest.h>
#include <QtWaylandCompositor/QtWaylandCompositor>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 48;
constexpr std::uint32_t kPixel = 0xff2040c0u; // premultiplied ARGB

// What the client thread learns, read by the test with the mutex held.
struct ClientState {
    std::mutex mutex;
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_compositor *compositor = nullptr;
    wl_shm *shm = nullptr;
    wl_seat *seat = nullptr;
    wl_output *output = nullptr;
    xdg_wm_base *wmBase = nullptr;
    wl_surface *surface = nullptr;
    wl_buffer *buffer = nullptr;
    xdg_surface *xdgSurface = nullptr;
    xdg_toplevel *toplevel = nullptr;
    wl_pointer *pointer = nullptr;
    wl_keyboard *keyboard = nullptr;
    std::vector<std::uint32_t> shmFormats;
    std::uint32_t seatCapabilities = 0;
    QString seatName;
    QSize outputMode;
    int outputRefresh = 0;
    QPoint outputPosition;
    int outputScale = 0;
    bool outputDone = false;
    std::atomic<int> frameCallbacks{0};
    std::atomic<int> bufferReleases{0};
    std::uint32_t lastConfigureSerial = 0;
    QSize configuredSize;
    std::vector<std::uint32_t> configuredStates;
    std::atomic<int> configures{0};
    std::atomic<int> pings{0};
    std::atomic<bool> keyboardEntered{false};
    std::vector<std::pair<std::uint32_t, std::uint32_t>> keys; // code, state
    std::uint32_t modsDepressed = 0;
    std::uint32_t keymapFormat = 0xffff;
    std::uint32_t keymapSize = 0;
    std::atomic<bool> pointerEntered{false};
    QPointF pointerPosition;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> buttons; // code, state
    std::atomic<int> axisEvents{0};
    std::atomic<bool> closeReceived{false};
    QString error;
    // Steps the test asks the client thread to take, in order.
    std::atomic<int> step{0};
    std::atomic<bool> finished{false};
};

ClientState g_client;

void registryGlobal(void *, wl_registry *registry, std::uint32_t name, const char *interface, std::uint32_t version) {
    ClientState &c = g_client;
    const std::lock_guard<std::mutex> lock(c.mutex);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0)
        c.compositor = static_cast<wl_compositor *>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
    else if (std::strcmp(interface, wl_shm_interface.name) == 0)
        c.shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    else if (std::strcmp(interface, wl_seat_interface.name) == 0)
        c.seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
    else if (std::strcmp(interface, wl_output_interface.name) == 0)
        c.output =
            static_cast<wl_output *>(wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 3u)));
    else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0)
        c.wmBase =
            static_cast<xdg_wm_base *>(wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 3u)));
}
void registryGlobalRemove(void *, wl_registry *, std::uint32_t) {}
const wl_registry_listener kRegistryListener = {registryGlobal, registryGlobalRemove};

void shmFormat(void *, wl_shm *, std::uint32_t format) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.shmFormats.push_back(format);
}
const wl_shm_listener kShmListener = {shmFormat};

void outputGeometry(void *, wl_output *, std::int32_t x, std::int32_t y, std::int32_t, std::int32_t, std::int32_t,
                    const char *, const char *, std::int32_t) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.outputPosition = QPoint(x, y);
}
void outputMode(void *, wl_output *, std::uint32_t flags, std::int32_t w, std::int32_t h, std::int32_t refresh) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        g_client.outputMode = QSize(w, h);
        g_client.outputRefresh = refresh;
    }
}
void outputDone(void *, wl_output *) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.outputDone = true;
}
void outputScale(void *, wl_output *, std::int32_t factor) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.outputScale = factor;
}
const wl_output_listener kOutputListener = {outputGeometry, outputMode, outputDone, outputScale, nullptr, nullptr};

void seatCapabilities(void *, wl_seat *seat, std::uint32_t capabilities) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.seatCapabilities = capabilities;
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !g_client.pointer)
        g_client.pointer = wl_seat_get_pointer(seat);
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !g_client.keyboard)
        g_client.keyboard = wl_seat_get_keyboard(seat);
}
void seatName(void *, wl_seat *, const char *name) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.seatName = QString::fromUtf8(name);
}
const wl_seat_listener kSeatListener = {seatCapabilities, seatName};

void frameDone(void *, wl_callback *callback, std::uint32_t) {
    wl_callback_destroy(callback);
    ++g_client.frameCallbacks;
}
const wl_callback_listener kFrameListener = {frameDone};

void bufferRelease(void *, wl_buffer *) { ++g_client.bufferReleases; }
const wl_buffer_listener kBufferListener = {bufferRelease};

void wmBasePing(void *, xdg_wm_base *base, std::uint32_t serial) {
    xdg_wm_base_pong(base, serial);
    ++g_client.pings;
}
const xdg_wm_base_listener kWmBaseListener = {wmBasePing};

void xdgSurfaceConfigure(void *, xdg_surface *surface, std::uint32_t serial) {
    {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        g_client.lastConfigureSerial = serial;
    }
    xdg_surface_ack_configure(surface, serial);
    ++g_client.configures;
}
const xdg_surface_listener kXdgSurfaceListener = {xdgSurfaceConfigure};

void toplevelConfigure(void *, xdg_toplevel *, std::int32_t w, std::int32_t h, wl_array *states) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.configuredSize = QSize(w, h);
    g_client.configuredStates.clear();
    const auto *p = static_cast<const std::uint32_t *>(states->data);
    for (std::size_t i = 0; i < states->size / sizeof(std::uint32_t); ++i)
        g_client.configuredStates.push_back(p[i]);
}
void toplevelClose(void *, xdg_toplevel *) { g_client.closeReceived = true; }
void toplevelConfigureBounds(void *, xdg_toplevel *, std::int32_t, std::int32_t) {}
void toplevelWmCapabilities(void *, xdg_toplevel *, wl_array *) {}
const xdg_toplevel_listener kToplevelListener = {toplevelConfigure, toplevelClose, toplevelConfigureBounds,
                                                 toplevelWmCapabilities};

void keyboardKeymap(void *, wl_keyboard *, std::uint32_t format, int fd, std::uint32_t size) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.keymapFormat = format;
    g_client.keymapSize = size;
    ::close(fd);
}
void keyboardEnter(void *, wl_keyboard *, std::uint32_t, wl_surface *, wl_array *) { g_client.keyboardEntered = true; }
void keyboardLeave(void *, wl_keyboard *, std::uint32_t, wl_surface *) { g_client.keyboardEntered = false; }
void keyboardKey(void *, wl_keyboard *, std::uint32_t, std::uint32_t, std::uint32_t key, std::uint32_t state) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.keys.emplace_back(key, state);
}
void keyboardModifiers(void *, wl_keyboard *, std::uint32_t, std::uint32_t depressed, std::uint32_t, std::uint32_t,
                       std::uint32_t) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.modsDepressed = depressed;
}
void keyboardRepeatInfo(void *, wl_keyboard *, std::int32_t, std::int32_t) {}
const wl_keyboard_listener kKeyboardListener = {keyboardKeymap, keyboardEnter,     keyboardLeave,
                                                keyboardKey,    keyboardModifiers, keyboardRepeatInfo};

void pointerEnter(void *, wl_pointer *, std::uint32_t, wl_surface *, wl_fixed_t x, wl_fixed_t y) {
    {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        g_client.pointerPosition = QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y));
    }
    g_client.pointerEntered = true;
}
void pointerLeave(void *, wl_pointer *, std::uint32_t, wl_surface *) { g_client.pointerEntered = false; }
void pointerMotion(void *, wl_pointer *, std::uint32_t, wl_fixed_t x, wl_fixed_t y) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.pointerPosition = QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y));
}
void pointerButton(void *, wl_pointer *, std::uint32_t, std::uint32_t, std::uint32_t button, std::uint32_t state) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.buttons.emplace_back(button, state);
}
void pointerAxis(void *, wl_pointer *, std::uint32_t, std::uint32_t, wl_fixed_t) { ++g_client.axisEvents; }
void pointerFrame(void *, wl_pointer *) {}
void pointerAxisSource(void *, wl_pointer *, std::uint32_t) {}
void pointerAxisStop(void *, wl_pointer *, std::uint32_t, std::uint32_t) {}
void pointerAxisDiscrete(void *, wl_pointer *, std::uint32_t, std::int32_t) {}
const wl_pointer_listener kPointerListener = {
    pointerEnter,      pointerLeave,    pointerMotion,       pointerButton, pointerAxis, pointerFrame,
    pointerAxisSource, pointerAxisStop, pointerAxisDiscrete, nullptr,       nullptr};

// A shared-memory buffer filled with kPixel.
wl_buffer *makeBuffer(wl_shm *shm) {
    const int stride = kWidth * 4;
    const int size = stride * kHeight;
    const int fd = ::memfd_create("nt-wayland-test", MFD_CLOEXEC);
    if (fd < 0 || ::ftruncate(fd, size) < 0)
        return nullptr;
    void *data = ::mmap(nullptr, static_cast<std::size_t>(size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
        return nullptr;
    auto *pixels = static_cast<std::uint32_t *>(data);
    for (int i = 0; i < kWidth * kHeight; ++i)
        pixels[i] = kPixel;
    ::munmap(data, static_cast<std::size_t>(size));
    wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, kWidth, kHeight, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    ::close(fd);
    return buffer;
}

void waitForStep(int step) {
    // Dispatch events while the test decides what comes next.
    while (g_client.step < step) {
        wl_display_dispatch_pending(g_client.display);
        wl_display_flush(g_client.display);
        pollfd pfd{wl_display_get_fd(g_client.display), POLLIN, 0};
        if (wl_display_prepare_read(g_client.display) == 0) {
            if (::poll(&pfd, 1, 10) > 0)
                wl_display_read_events(g_client.display);
            else
                wl_display_cancel_read(g_client.display);
        }
        wl_display_dispatch_pending(g_client.display);
    }
}

void fail(const char *what) {
    const std::lock_guard<std::mutex> lock(g_client.mutex);
    g_client.error = QString::fromLatin1(what);
}

// The client scenario, run on its own thread.
void runClient(const QByteArray &socketName) {
    ClientState &c = g_client;
    c.display = wl_display_connect(socketName.constData());
    if (!c.display) {
        fail("connect");
        c.finished = true;
        return;
    }
    c.registry = wl_display_get_registry(c.display);
    wl_registry_add_listener(c.registry, &kRegistryListener, nullptr);
    wl_display_roundtrip(c.display);
    if (!c.compositor || !c.shm || !c.seat || !c.output || !c.wmBase) {
        fail("globals");
        c.finished = true;
        return;
    }
    wl_shm_add_listener(c.shm, &kShmListener, nullptr);
    wl_output_add_listener(c.output, &kOutputListener, nullptr);
    wl_seat_add_listener(c.seat, &kSeatListener, nullptr);
    xdg_wm_base_add_listener(c.wmBase, &kWmBaseListener, nullptr);
    wl_display_roundtrip(c.display);
    {
        const std::lock_guard<std::mutex> lock(c.mutex);
        if (c.pointer)
            wl_pointer_add_listener(c.pointer, &kPointerListener, nullptr);
        if (c.keyboard)
            wl_keyboard_add_listener(c.keyboard, &kKeyboardListener, nullptr);
    }
    wl_display_roundtrip(c.display);

    // A toplevel window with a buffer.
    c.surface = wl_compositor_create_surface(c.compositor);
    c.xdgSurface = xdg_wm_base_get_xdg_surface(c.wmBase, c.surface);
    xdg_surface_add_listener(c.xdgSurface, &kXdgSurfaceListener, nullptr);
    c.toplevel = xdg_surface_get_toplevel(c.xdgSurface);
    xdg_toplevel_add_listener(c.toplevel, &kToplevelListener, nullptr);
    xdg_toplevel_set_title(c.toplevel, "Nt test window");
    xdg_toplevel_set_app_id(c.toplevel, "org.open-nt.test");
    xdg_toplevel_set_min_size(c.toplevel, 32, 24);
    wl_surface_commit(c.surface);
    wl_display_roundtrip(c.display);

    waitForStep(1); // the compositor configures; the listener acknowledged
    c.buffer = makeBuffer(c.shm);
    if (!c.buffer) {
        fail("buffer");
        c.finished = true;
        return;
    }
    wl_buffer_add_listener(c.buffer, &kBufferListener, nullptr);
    wl_surface_attach(c.surface, c.buffer, 0, 0);
    wl_surface_damage(c.surface, 0, 0, kWidth, kHeight);
    wl_callback *frame = wl_surface_frame(c.surface);
    wl_callback_add_listener(frame, &kFrameListener, nullptr);
    wl_surface_commit(c.surface);
    wl_display_roundtrip(c.display);

    waitForStep(2); // input happened
    // A second buffer commit releases the first.
    wl_buffer *second = makeBuffer(c.shm);
    wl_surface_attach(c.surface, second, 0, 0);
    wl_surface_damage_buffer(c.surface, 0, 0, 8, 8);
    wl_surface_commit(c.surface);
    wl_display_roundtrip(c.display);

    waitForStep(3); // tear down
    xdg_toplevel_destroy(c.toplevel);
    xdg_surface_destroy(c.xdgSurface);
    wl_surface_destroy(c.surface);
    wl_buffer_destroy(second);
    wl_buffer_destroy(c.buffer);
    wl_display_roundtrip(c.display);
    waitForStep(4);
    wl_display_disconnect(c.display);
    c.finished = true;
}

template <typename Predicate> bool waitFor(Predicate p, int timeout = 5000) { return QTest::qWaitFor(p, timeout); }

} // namespace

class TestWaylandCompositor : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void clientScenario();
    void outputModes();
    void popupPosition();

private:
    QTemporaryDir m_runtimeDir;
};

void TestWaylandCompositor::initTestCase() {
    QVERIFY(m_runtimeDir.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtimeDir.path().toUtf8());
}

void TestWaylandCompositor::clientScenario() {
    QWaylandCompositor compositor;
    compositor.setSocketName("nt-test-0");
    QWaylandOutput output(&compositor, nullptr);
    output.setPosition(QPoint(10, 20));
    output.addMode(QWaylandOutputMode(QSize(1280, 720), 60000), true);
    output.setScaleFactor(2);
    QWaylandXdgShell shell(&compositor);

    QList<QWaylandSurface *> surfaces;
    QObject::connect(&compositor, &QWaylandCompositor::surfaceCreated, [&](QWaylandSurface *s) { surfaces.append(s); });
    QWaylandXdgToplevel *toplevel = nullptr;
    QWaylandXdgSurface *xdgSurface = nullptr;
    QObject::connect(&shell, &QWaylandXdgShell::toplevelCreated, [&](QWaylandXdgToplevel *t, QWaylandXdgSurface *x) {
        toplevel = t;
        xdgSurface = x;
    });
    int pongs = 0;
    QObject::connect(&shell, &QWaylandXdgShell::pong, [&](uint) { ++pongs; });
    int damaged = 0;
    QRegion lastDamage;

    compositor.create();
    QVERIFY(compositor.isCreated());
    QCOMPARE(compositor.socketName(), QByteArray("nt-test-0"));
    QVERIFY(QFile::exists(m_runtimeDir.path() + QLatin1String("/nt-test-0")));
    QCOMPARE(compositor.defaultOutput(), &output);
    QVERIFY(compositor.defaultSeat() != nullptr);
    QCOMPARE(output.geometry(), QRect(10, 20, 640, 360));

    QThread *client = QThread::create([] { runClient("nt-test-0"); });
    client->start();

    // The client connected, bound the globals and made a toplevel.
    QVERIFY(waitFor([&] { return toplevel != nullptr || !g_client.error.isEmpty(); }));
    QVERIFY2(g_client.error.isEmpty(), qPrintable(g_client.error));
    QCOMPARE(compositor.clients().size(), 1);
    QVERIFY(compositor.clients().first()->processId() == ::getpid());
    QCOMPARE(surfaces.size(), 1);
    QWaylandSurface *surface = surfaces.first();
    QCOMPARE(surface->client(), compositor.clients().first());
    QCOMPARE(xdgSurface->surface(), surface);
    QCOMPARE(xdgSurface->toplevel(), toplevel);
    QCOMPARE(surface->role(), QWaylandXdgToplevel::role());
    QVERIFY(waitFor([&] { return toplevel->title() == QLatin1String("Nt test window"); }));
    QCOMPARE(toplevel->appId(), QString::fromLatin1("org.open-nt.test"));
    QCOMPARE(toplevel->minSize(), QSize(32, 24));
    QVERIFY(!surface->hasContent());
    QObject::connect(surface, &QWaylandSurface::damaged, [&](const QRegion &r) {
        ++damaged;
        lastDamage = r;
    });
    {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        QVERIFY(g_client.shmFormats.size() >= 2);
        QCOMPARE(g_client.outputMode, QSize(1280, 720));
        QCOMPARE(g_client.outputRefresh, 60000);
        QCOMPARE(g_client.outputPosition, QPoint(10, 20));
        QCOMPARE(g_client.outputScale, 2);
        QVERIFY(g_client.outputDone);
        QCOMPARE(g_client.seatCapabilities, 7u);
        QCOMPARE(g_client.seatName, QString::fromLatin1("seat0"));
    }

    // Configure: the client acknowledges, and the states take effect.
    const uint serial = toplevel->sendConfigure(QSize(400, 300),
                                                QList<QWaylandXdgToplevel::State>{QWaylandXdgToplevel::ActivatedState});
    QVERIFY(serial != 0);
    QVERIFY(waitFor([&] { return toplevel->activated(); }));
    {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        QCOMPARE(g_client.configuredSize, QSize(400, 300));
        QCOMPARE(g_client.configuredStates, std::vector<std::uint32_t>{XDG_TOPLEVEL_STATE_ACTIVATED});
        QCOMPARE(g_client.lastConfigureSerial, serial);
    }
    QCOMPARE(toplevel->states(), QList<QWaylandXdgToplevel::State>{QWaylandXdgToplevel::ActivatedState});
    g_client.step = 1;

    // The first buffer arrives.
    QVERIFY(waitFor([&] { return surface->hasContent(); }));
    QCOMPARE(surface->bufferSize(), QSize(kWidth, kHeight));
    QCOMPARE(surface->destinationSize(), QSize(kWidth, kHeight));
    QCOMPARE(xdgSurface->windowGeometry(), QRect(0, 0, kWidth, kHeight));
    QCOMPARE(damaged, 1);
    QCOMPARE(lastDamage, QRegion(QRect(0, 0, kWidth, kHeight)));
    const QWaylandBufferRef buffer = surface->currentBuffer();
    QVERIFY(buffer.hasBuffer());
    QVERIFY(buffer.isSharedMemory());
    QCOMPARE(buffer.size(), QSize(kWidth, kHeight));
    const QImage image = buffer.image();
    QCOMPARE(image.format(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(image.pixel(5, 7), QRgb(kPixel));
    QCOMPARE(image.pixel(kWidth - 1, kHeight - 1), QRgb(kPixel));
    QVERIFY(surface->inputRegionContains(QPoint(1, 1)));
    QVERIFY(!surface->inputRegionContains(QPoint(kWidth + 5, 1)));

    // A view takes the buffer; frame callbacks are answered on request.
    QWaylandView view;
    view.setSurface(surface);
    QVERIFY(view.advance());
    QCOMPARE(view.currentBuffer(), buffer);
    QVERIFY(!view.advance());
    QCOMPARE(g_client.frameCallbacks.load(), 0);
    output.sendFrameCallbacks();
    QVERIFY(waitFor([&] { return g_client.frameCallbacks.load() == 1; }));

    // Keyboard: focus, a key, a modifier.
    QWaylandSeat *seat = compositor.defaultSeat();
    QVERIFY(seat->setKeyboardFocus(surface));
    QCOMPARE(seat->keyboardFocus(), surface);
    QVERIFY(waitFor([&] { return g_client.keyboardEntered.load(); }));
    seat->sendKeyEvent(Qt::Key_A, true);
    seat->sendKeyEvent(Qt::Key_A, false);
    seat->sendKeyEvent(Qt::Key_Shift, true);
    QVERIFY(waitFor([&] {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        return g_client.keys.size() == 3;
    }));
    {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        QCOMPARE(g_client.keys[0], (std::pair<std::uint32_t, std::uint32_t>(30, 1)));
        QCOMPARE(g_client.keys[1], (std::pair<std::uint32_t, std::uint32_t>(30, 0)));
        QCOMPARE(g_client.keys[2].first, 42u);
        QVERIFY(g_client.keymapFormat == 0 || g_client.keymapFormat == 1);
        if (g_client.keymapFormat == 1)
            QVERIFY(g_client.keymapSize > 100);
    }
    QVERIFY(waitFor([&] {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        return (g_client.modsDepressed & 1u) != 0;
    }));
    seat->sendKeyEvent(Qt::Key_Shift, false);

    // Pointer: enter with a move, a click, a wheel notch.
    seat->sendMouseMoveEvent(&view, QPointF(10.5, 20.25));
    QCOMPARE(seat->mouseFocus(), &view);
    QVERIFY(waitFor([&] { return g_client.pointerEntered.load(); }));
    seat->sendMouseMoveEvent(&view, QPointF(12, 22));
    seat->sendMousePressEvent(Qt::LeftButton);
    QVERIFY(seat->pointer()->isButtonPressed());
    seat->sendMouseReleaseEvent(Qt::LeftButton);
    seat->sendMouseWheelEvent(Qt::Vertical, 120);
    QVERIFY(waitFor([&] {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        return g_client.buttons.size() == 2 && g_client.axisEvents.load() == 1;
    }));
    {
        const std::lock_guard<std::mutex> lock(g_client.mutex);
        QCOMPARE(g_client.pointerPosition, QPointF(12, 22));
        QCOMPARE(g_client.buttons[0], (std::pair<std::uint32_t, std::uint32_t>(0x110, 1)));
        QCOMPARE(g_client.buttons[1], (std::pair<std::uint32_t, std::uint32_t>(0x110, 0)));
    }

    // Ping and pong.
    QVERIFY(shell.ping(compositor.clients().first()) != 0);
    QVERIFY(waitFor([&] { return pongs == 1; }));
    QCOMPARE(g_client.pings.load(), 1);
    g_client.step = 2;

    // The second buffer replaces and releases the first; the damage is the buffer damage, scaled.
    QVERIFY(waitFor([&] { return damaged == 2; }));
    QCOMPARE(lastDamage, QRegion(QRect(0, 0, 8, 8)));
    QVERIFY(waitFor([&] { return g_client.bufferReleases.load() == 1; }));
    QVERIFY(surface->currentBuffer() != buffer);
    QVERIFY(view.advance());

    // Close goes through; then the client tears down and leaves.
    toplevel->sendClose();
    QVERIFY(waitFor([&] { return g_client.closeReceived.load(); }));
    QPointer<QWaylandXdgToplevel> toplevelGuard(toplevel);
    QPointer<QWaylandSurface> surfaceGuard(surface);
    bool surfaceDestroyed = false;
    QObject::connect(surface, &QWaylandSurface::surfaceDestroyed, [&] { surfaceDestroyed = true; });
    g_client.step = 3;
    QVERIFY(waitFor([&] { return surfaceDestroyed; }));
    QVERIFY(waitFor([&] { return toplevelGuard.isNull() && surfaceGuard.isNull(); }));
    QVERIFY(compositor.surfaces().isEmpty());
    g_client.step = 4;
    QVERIFY(waitFor([&] { return g_client.finished.load(); }));
    QVERIFY(waitFor([&] { return compositor.clients().isEmpty(); }));
    client->wait();
    delete client;
    QVERIFY2(g_client.error.isEmpty(), qPrintable(g_client.error));
}

void TestWaylandCompositor::outputModes() {
    QWaylandCompositor compositor;
    compositor.setSocketName("nt-test-1");
    QWaylandOutput output;
    QCOMPARE(output.compositor(), nullptr);
    output.setCompositor(&compositor);
    QCOMPARE(compositor.outputs().size(), 1);
    output.addMode(QWaylandOutputMode(QSize(800, 600), 60000));
    output.addMode(QWaylandOutputMode(QSize(1024, 768), 50000), true);
    QCOMPARE(output.currentMode(), QWaylandOutputMode(QSize(800, 600), 60000));
    output.setCurrentMode(QWaylandOutputMode(QSize(1024, 768), 50000));
    QCOMPARE(output.geometry(), QRect(0, 0, 1024, 768));
    QCOMPARE(output.availableGeometry(), output.geometry());
    output.setAvailableGeometry(QRect(0, 0, 1024, 700));
    QCOMPARE(output.availableGeometry(), QRect(0, 0, 1024, 700));
    output.setPhysicalSizeFollowsSize(true);
    QCOMPARE(output.physicalSize(), QSize(271, 203));
    compositor.create();
    QVERIFY(compositor.isCreated());
    // A second compositor cannot take the same socket name.
    QWaylandCompositor other;
    other.setSocketName("nt-test-1");
    other.create();
    QVERIFY(!other.isCreated());
}

void TestWaylandCompositor::popupPosition() {
    // The arithmetic of the positioner, without a client: anchored at the bottom-left of a rectangle, extending
    // down and to the right.
    QWaylandCompositor compositor;
    compositor.setSocketName("nt-test-2");
    QWaylandXdgShell shell(&compositor);
    QCOMPARE(QWaylandXdgShell::interfaceName(), QByteArray("xdg_wm_base"));
    QCOMPARE(QWaylandXdgShell::findIn(&compositor), &shell);
    QCOMPARE(shell.extensionContainer(), &compositor);
    QCOMPARE(QWaylandXdgSurface::role()->name(), QByteArray("xdg_surface"));
    QCOMPARE(QWaylandXdgPopup::role()->name(), QByteArray("xdg_popup"));
}

QTEST_GUILESS_MAIN(TestWaylandCompositor)
#include "test_waylandcompositor.moc"
