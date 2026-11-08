// MIT License - see LICENSE
//
// Tests of the xcb platform at a device pixel ratio of 2 (QT_SCALE_FACTOR=2): the window keeps its size in
// window coordinates while the X server sees twice as many pixels. The tests are skipped when there is no X
// display.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qscreen.h>

#include "qpa/qplatformintegration_p.h"
#include "xcb_test_helpers.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

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
    QSize lastSize;
    QPointF lastMouse;

protected:
    void paintEvent(QPaintEvent *) override {
        ++paints;
        QPainter p(this);
        p.fillRect(0, 0, width(), height(), QColor(200, 30, 60));
    }
    void resizeEvent(QResizeEvent *e) override {
        QRasterWindow::resizeEvent(e);
        lastSize = e->size();
    }
    void mousePressEvent(QMouseEvent *e) override { lastMouse = e->position(); }
};

} // namespace

using namespace xcbtest;

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "xcb", 1);
    ::setenv("QT_SCALE_FACTOR", "2", 1);
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

    QScreen *screen = QGuiApplication::primaryScreen();
    const xcb_screen_t *root = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    CHECK(screen->devicePixelRatio() == 2.0);
    CHECK(screen->size() == QSize((root->width_in_pixels + 1) / 2, (root->height_in_pixels + 1) / 2));

    TestWindow w;
    w.resize(200, 120);
    w.show();
    CHECK(waitFor([&] { return w.isExposed() && w.paints > 0; }));
    const xcb_window_t id = static_cast<xcb_window_t>(w.winId());
    CHECK(w.size() == QSize(200, 120) && w.devicePixelRatio() == 2.0);

    // The server has twice as many pixels, and they show what the window painted.
    xcb_get_geometry_reply_t *geometry = xcb_get_geometry_reply(c, xcb_get_geometry(c, id), nullptr);
    CHECK(geometry && geometry->width == 400 && geometry->height == 240);
    std::free(geometry);
    bool pixelsOk = false;
    for (int attempt = 0; attempt < 50 && !pixelsOk; ++attempt) {
        xcb_get_image_reply_t *image =
            xcb_get_image_reply(c, xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, id, 0, 0, 400, 240, ~0u), nullptr);
        if (image) {
            const uint8_t *data = xcb_get_image_data(image);
            const auto at = [&](int x, int y) { return data + (y * 400 + x) * 4; };
            pixelsOk = at(0, 0)[2] == 200 && at(399, 239)[2] == 200 && at(399, 239)[1] == 30 && at(200, 120)[0] == 60;
            std::free(image);
        }
        if (!pixelsOk)
            waitFor([] { return false; }, 20);
    }
    CHECK(pixelsOk);

    // A click at pixel (60, 80) of the server is at (30, 40) in the window.
    sendButton(c, id, 1, XCB_BUTTON_PRESS, 10000, 60, 80);
    CHECK(waitFor([&] { return w.lastMouse == QPointF(30, 40); }));

    // Resizing in window coordinates resizes the pixels of the server by twice as much.
    const int paintsBefore = w.paints;
    w.resize(150, 100);
    CHECK(waitFor([&] { return w.lastSize == QSize(150, 100) && w.paints > paintsBefore; }));
    geometry = xcb_get_geometry_reply(c, xcb_get_geometry(c, id), nullptr);
    CHECK(geometry && geometry->width == 300 && geometry->height == 200);
    std::free(geometry);
    CHECK(w.size() == QSize(150, 100));

    xcb_disconnect(c);
    if (g_failures == 0)
        std::puts("all xcb scale tests passed");
    return g_failures == 0 ? 0 : 1;
}
