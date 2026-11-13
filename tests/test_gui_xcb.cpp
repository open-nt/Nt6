// MIT License - see LICENSE
//
// End-to-end tests of the xcb platform against a real X server (or XWayland). A second XCB
// connection plays the user: it sends key and button events to the window and reads its pixels
// back. The tests are skipped when there is no X display.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qscreen.h>

#include "qpa/qplatformintegration_p.h"
#include "qwindow_p.h"
#include "xcb_test_helpers.h"

#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

class TestWindow : public QRasterWindow {
public:
    int paints = 0;
    int resizes = 0;
    QSize lastSize;
    QStringList log;
    QPoint lastWheel;
    QPoint lastMouse;
    int lastKey = 0;
    QString lastText;
    Qt::KeyboardModifiers lastModifiers;

protected:
    void paintEvent(QPaintEvent *) override {
        ++paints;
        QPainter p(this);
        p.fillRect(0, 0, width(), height(), QColor(200, 30, 60));
    }
    void resizeEvent(QResizeEvent *e) override {
        QRasterWindow::resizeEvent(e);
        ++resizes;
        lastSize = e->size();
    }
    void keyPressEvent(QKeyEvent *e) override {
        log << "key";
        lastKey = e->key();
        lastText = e->text();
        lastModifiers = e->modifiers();
    }
    void mousePressEvent(QMouseEvent *e) override {
        log << "press";
        lastMouse = e->pos();
    }
    void mouseDoubleClickEvent(QMouseEvent *) override { log << "dblclick"; }
    void mouseReleaseEvent(QMouseEvent *) override { log << "release"; }
    void mouseMoveEvent(QMouseEvent *e) override {
        log << "move";
        lastMouse = e->pos();
    }
    void wheelEvent(QWheelEvent *e) override {
        log << "wheel";
        lastWheel = e->angleDelta();
    }
};

} // namespace

using namespace xcbtest;

// Counts the events that the xcb platform passes to the native event filters.
struct XcbEventCounter : QAbstractNativeEventFilter {
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override {
        if (eventType == "xcb_generic_event_t" && message)
            ++events;
        return false;
    }
    int events = 0;
};

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "xcb", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    {
        int n = 0;
        std::unique_ptr<QPlatformIntegration> probe(QtPrivate::createPlatformIntegration("xcb", n, nullptr));
        if (!probe) {
            std::puts("SKIP: no X display");
            return 0;
        }
    }
    QGuiApplication app(argc, argv);
    xcb_connection_t *c = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(c)) {
        std::puts("SKIP: no X display");
        return 0;
    }

    CHECK(QGuiApplication::platformName() == QString("xcb"));
    QScreen *screen = QGuiApplication::primaryScreen();
    CHECK(screen != nullptr);
    const xcb_screen_t *root = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    CHECK(screen->size() == QSize(root->width_in_pixels, root->height_in_pixels));
    CHECK(screen->depth() == root->root_depth);
    CHECK(screen->logicalDotsPerInch() > 0);

    XcbEventCounter counter;
    app.installNativeEventFilter(&counter);

    TestWindow w;
    w.setTitle("Nt xcb test \xc3\xa4");
    w.resize(200, 120);
    w.show();
    CHECK(w.winId() != 0);
    CHECK(waitFor([&] { return w.isExposed() && w.paints > 0; }));
    CHECK(counter.events > 0); // the expose event at least went through the filter
    const xcb_window_t id = static_cast<xcb_window_t>(w.winId());

    // The window has the size we asked for and the pixels we painted.
    CHECK(w.size() == QSize(200, 120));
    xcb_get_geometry_reply_t *geometry = xcb_get_geometry_reply(c, xcb_get_geometry(c, id), nullptr);
    CHECK(geometry && geometry->width == 200 && geometry->height == 120);
    std::free(geometry);
    bool pixelsOk = false;
    for (int attempt = 0; attempt < 50 && !pixelsOk; ++attempt) {
        xcb_get_image_reply_t *image =
            xcb_get_image_reply(c, xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, id, 0, 0, 200, 120, ~0u), nullptr);
        if (image) {
            const uint8_t *data = xcb_get_image_data(image);
            // Little endian BGRX: blue, green, red
            pixelsOk = data[(60 * 200 + 100) * 4 + 0] == 60 && data[(60 * 200 + 100) * 4 + 1] == 30 &&
                       data[(60 * 200 + 100) * 4 + 2] == 200;
            std::free(image);
        }
        if (!pixelsOk)
            waitFor([] { return false; }, 20);
    }
    CHECK(pixelsOk);

    // The title is set as UTF-8 in _NET_WM_NAME.
    {
        const xcb_atom_t netWmName = internAtom(c, "_NET_WM_NAME");
        const xcb_atom_t utf8 = internAtom(c, "UTF8_STRING");
        xcb_get_property_reply_t *r =
            xcb_get_property_reply(c, xcb_get_property(c, 0, id, netWmName, utf8, 0, 100), nullptr);
        CHECK(r != nullptr);
        if (r) {
            CHECK(QByteArray(static_cast<const char *>(xcb_get_property_value(r)), xcb_get_property_value_length(r)) ==
                  QByteArray("Nt xcb test \xc3\xa4"));
            std::free(r);
        }
    }

    // Keys
    const xcb_keycode_t keyA = keycodeFor(c, 'a');
    CHECK(keyA != 0);
    if (keyA != 0) {
        sendKey(c, id, keyA, 0, XCB_KEY_PRESS);
        CHECK(waitFor([&] { return w.log.contains("key"); }));
        CHECK(w.lastKey == Qt::Key_A && w.lastText == QString("a") && w.lastModifiers == Qt::NoModifier);
        sendKey(c, id, keyA, XCB_MOD_MASK_SHIFT, XCB_KEY_PRESS);
        CHECK(waitFor([&] { return w.log.count("key") == 2; }));
        CHECK(w.lastKey == Qt::Key_A && w.lastText == QString("A") && w.lastModifiers == Qt::ShiftModifier);
        sendKey(c, id, keyA, XCB_MOD_MASK_CONTROL, XCB_KEY_PRESS);
        CHECK(waitFor([&] { return w.log.count("key") == 3; }));
        CHECK(w.lastKey == Qt::Key_A && w.lastText.isEmpty() && w.lastModifiers == Qt::ControlModifier);
    }
    const xcb_keycode_t escape = keycodeFor(c, 0xff1b);
    if (escape != 0) {
        sendKey(c, id, escape, 0, XCB_KEY_PRESS);
        CHECK(waitFor([&] { return w.log.count("key") == 4; }));
        CHECK(w.lastKey == Qt::Key_Escape);
    }

    // A key that another user of Nt's connection reads into XCB's queue, as Mesa does while an EGL window
    // renders, is still delivered, though the socket has nothing more to report.
    if (keyA != 0) {
        auto *own = static_cast<xcb_connection_t *>(QtPrivate::GuiAccess::integration()->eglPlatform().display);
        CHECK(own != nullptr);
        if (own) {
            const qsizetype keysBefore = w.log.count("key");
            sendKey(c, id, keyA, 0, XCB_KEY_PRESS);
            std::free(xcb_get_input_focus_reply(c, xcb_get_input_focus(c), nullptr));     // the server has sent it
            std::free(xcb_get_input_focus_reply(own, xcb_get_input_focus(own), nullptr)); // queued by XCB
            CHECK(waitFor([&] { return w.log.count("key") == keysBefore + 1; }, 1000));

            // When a timer that is always due keeps the event loop from sleeping, the queue is not looked at before
            // waiting. OpenGL, which is what reads events into the queue, has the platform look at it after a swap.
            QTimer busy;
            busy.start(0);
            QWindow glWindow;
            glWindow.setSurfaceType(QSurface::OpenGLSurface);
            glWindow.resize(64, 64);
            glWindow.show();
            QOpenGLContext context;
            const bool gl =
                waitFor([&] { return glWindow.isExposed(); }) && context.create() && context.makeCurrent(&glWindow);
            if (gl)
                context.swapBuffers(&glWindow);
            // Sends a key and reads it into XCB's queue; returns whether it is still there after a while.
            auto queueKey = [&] {
                (void)waitFor([] { return false; }, 300); // the events of the window come in first
                const qsizetype before = w.log.count("key");
                sendKey(c, id, keyA, 0, XCB_KEY_PRESS);
                std::free(xcb_get_input_focus_reply(c, xcb_get_input_focus(c), nullptr));
                std::free(xcb_get_input_focus_reply(own, xcb_get_input_focus(own), nullptr));
                (void)waitFor([&] { return w.log.count("key") > before; }, 200);
                return before;
            };
            // The next passes of the event loop handle the key, not whenever other traffic comes by.
            auto handledSoon = [&](qsizetype before) {
                for (int pass = 0; pass < 3 && w.log.count("key") == before; ++pass)
                    QCoreApplication::processEvents();
                return w.log.count("key") == before + 1;
            };
            const qsizetype before = queueKey();
            CHECK(w.log.count("key") == before); // stuck in the queue
            QtPrivate::GuiAccess::integration()->checkQueuedEvents();
            CHECK(handledSoon(before));
            if (gl) {
                const qsizetype beforeSwap = queueKey();
                context.swapBuffers(&glWindow);
                CHECK(handledSoon(beforeSwap));
                context.doneCurrent();
            }
        }
    }

    // Cursors come from the cursor theme when libxcb-cursor is there, and from the X font otherwise; either way
    // the server accepts them and the connection keeps working.
    for (Qt::CursorShape shape : {Qt::ArrowCursor, Qt::PointingHandCursor, Qt::IBeamCursor, Qt::SizeFDiagCursor,
                                  Qt::ForbiddenCursor, Qt::BlankCursor, Qt::ArrowCursor})
        w.setCursor(shape);
    w.unsetCursor();
    if (keyA != 0) {
        const qsizetype keysBefore = w.log.count("key");
        sendKey(c, id, keyA, 0, XCB_KEY_PRESS);
        CHECK(waitFor([&] { return w.log.count("key") == keysBefore + 1; }));
    }

    // Mouse: press, release, a fast second press counts as a double click, wheel and move.
    w.log.clear();
    sendButton(c, id, 1, XCB_BUTTON_PRESS, 10000, 30, 40);
    sendButton(c, id, 1, XCB_BUTTON_RELEASE, 10050, 30, 40);
    sendButton(c, id, 1, XCB_BUTTON_PRESS, 10100, 31, 40);
    sendButton(c, id, 1, XCB_BUTTON_RELEASE, 10150, 31, 40);
    CHECK(waitFor([&] { return w.log.size() >= 4; }));
    CHECK(w.log == QStringList({"press", "release", "dblclick", "release"}));
    CHECK(w.lastMouse == QPoint(30, 40));
    w.log.clear();
    sendButton(c, id, 3, XCB_BUTTON_PRESS, 20000, 1, 1);
    sendButton(c, id, 3, XCB_BUTTON_RELEASE, 20010, 1, 1);
    CHECK(waitFor([&] { return w.log.size() >= 2; }));
    CHECK(w.log == QStringList({"press", "release"}));
    w.log.clear();
    sendButton(c, id, 4, XCB_BUTTON_PRESS, 30000, 5, 5);
    CHECK(waitFor([&] { return w.log.contains("wheel"); }));
    CHECK(w.lastWheel == QPoint(0, 120));
    sendButton(c, id, 5, XCB_BUTTON_PRESS, 30100, 5, 5);
    CHECK(waitFor([&] { return w.log.count("wheel") == 2; }));
    CHECK(w.lastWheel == QPoint(0, -120));
    sendMotion(c, id, 77, 88);
    CHECK(waitFor([&] { return w.log.contains("move"); }));
    CHECK(w.lastMouse == QPoint(77, 88));

    // Resizing from the application reaches the server, and the window repaints at the new size.
    const int paintsBefore = w.paints;
    w.resize(300, 150);
    CHECK(waitFor([&] { return w.lastSize == QSize(300, 150) && w.paints > paintsBefore; }));
    // The server is another process: wait until it has the new size.
    bool resized = false;
    for (int attempt = 0; attempt < 100 && !resized; ++attempt) {
        geometry = xcb_get_geometry_reply(c, xcb_get_geometry(c, id), nullptr);
        resized = geometry && geometry->width == 300 && geometry->height == 150;
        std::free(geometry);
        if (!resized)
            waitFor([] { return false; }, 20);
    }
    CHECK(resized);

    // A move is reported with the position on the screen.
    w.setPosition(120, 90);
    CHECK(waitFor([&] { return w.position() == QPoint(120, 90); }));

    // The window manager's close request closes the window and ends the application.
    const xcb_atom_t wmProtocols = internAtom(c, "WM_PROTOCOLS");
    xcb_client_message_event_t close;
    std::memset(&close, 0, sizeof close);
    close.response_type = XCB_CLIENT_MESSAGE;
    close.format = 32;
    close.window = id;
    close.type = wmProtocols;
    close.data.data32[0] = internAtom(c, "WM_DELETE_WINDOW");
    close.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(c, 0, id, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&close));
    xcb_flush(c);
    CHECK(waitFor([&] { return !w.isVisible(); }));
    CHECK(w.handle() == nullptr);

    xcb_disconnect(c);
    if (g_failures == 0)
        std::puts("all xcb tests passed");
    return g_failures == 0 ? 0 : 1;
}
