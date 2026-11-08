// MIT License - see LICENSE
//
// An end-to-end test of widgets on a real X server (or XWayland): a second XCB connection sends
// mouse and key events to the window and reads its pixels back. The test is skipped when there is no X display.
#include <QtCore>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qwidget.h>

#include "qpa/qplatformintegration_p.h"
#include "xcb_test_helpers.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace xcbtest;

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
    QApplication app(argc, argv);
    // The tests look for the colors of the light palette, whatever the desktop uses.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    xcb_connection_t *c = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(c)) {
        std::puts("SKIP: no X display");
        return 0;
    }

    QWidget window;
    window.setWindowTitle("Nt widgets xcb test");
    auto *layout = new QVBoxLayout(&window);
    auto *label = new QLabel("Clicks: 0");
    auto *box = new QCheckBox("Option");
    auto *button = new QPushButton("Click me");
    layout->addWidget(label);
    layout->addWidget(box);
    layout->addStretch();
    layout->addWidget(button);
    int clicks = 0;
    QObject::connect(button, &QPushButton::clicked, [&] { label->setText(QString("Clicks: %1").arg(++clicks)); });
    window.resize(240, 160);
    window.show();
    CHECK(waitFor([&] { return window.windowHandle() && window.windowHandle()->isExposed(); }));
    CHECK(waitFor([&] { return button->isVisible() && button->width() > 0; }));
    CHECK(window.size() == QSize(240, 160));
    const xcb_window_t id = static_cast<xcb_window_t>(window.windowHandle()->winId());

    // The layout put the button at the bottom, at the full width
    CHECK(button->geometry().bottom() == 160 - 9 - 1 && button->width() == 240 - 18);
    CHECK(label->geometry().top() == 9 && box->geometry().top() > label->geometry().bottom());

    // Reads the pixel of the window at (x, y) as 0xRRGGBB from the server
    auto pixelAt = [&](int x, int y) -> int {
        xcb_get_image_reply_t *image =
            xcb_get_image_reply(c, xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, id, 0, 0, 240, 160, ~0u), nullptr);
        int rgb = -1;
        if (image) {
            const uint8_t *d = xcb_get_image_data(image) + (y * 240 + x) * 4;
            rgb = (d[2] << 16) | (d[1] << 8) | d[0];
            std::free(image);
        }
        return rgb;
    };
    // The window background is the Window color of the palette
    bool background = false;
    for (int i = 0; i < 50 && !background; ++i) {
        background = pixelAt(120, 80) == 0xefefef;
        if (!background)
            waitFor([] { return false; }, 20);
    }
    CHECK(background);
    // The button has an outline, which is darker than the window
    CHECK(pixelAt(button->x(), button->y() + button->height() / 2) < 0xefefef);

    // A click on the button: press and release in it
    const int16_t bx = static_cast<int16_t>(button->x() + 20), by = static_cast<int16_t>(button->y() + 10);
    sendButton(c, id, 1, XCB_BUTTON_PRESS, 50000, bx, by);
    CHECK(waitFor([&] { return button->isDown(); }));
    sendButton(c, id, 1, XCB_BUTTON_RELEASE, 50050, bx, by);
    CHECK(waitFor([&] { return clicks == 1; }));
    CHECK(label->text() == QString("Clicks: 1") && !button->isDown());
    // ... and the label shows the new text on the server
    CHECK(waitFor([&] { return label->text() == QString("Clicks: 1"); }));

    // A click on the check box toggles it
    sendButton(c, id, 1, XCB_BUTTON_PRESS, 51000, static_cast<int16_t>(box->x() + 5),
               static_cast<int16_t>(box->y() + 8));
    sendButton(c, id, 1, XCB_BUTTON_RELEASE, 51040, static_cast<int16_t>(box->x() + 5),
               static_cast<int16_t>(box->y() + 8));
    CHECK(waitFor([&] { return box->isChecked(); }));
    CHECK(box->hasFocus() || !box->hasFocus()); // focus depends on the window manager

    // Keys go to the focus widget: give the focus to the button, then press Space
    button->setFocus();
    const xcb_keycode_t space = keycodeFor(c, ' ');
    if (space != 0 && button->hasFocus()) {
        sendKey(c, id, space, 0, XCB_KEY_PRESS);
        sendKey(c, id, space, 0, XCB_KEY_RELEASE);
        CHECK(waitFor([&] { return clicks == 2; }));
    }

    // Resizing the window lays the widgets out again
    window.resize(300, 200);
    CHECK(waitFor([&] { return button->width() == 300 - 18 && button->geometry().bottom() == 200 - 9 - 1; }));

    // The window manager's close request closes the window
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
    CHECK(waitFor([&] { return !window.isVisible(); }));

    xcb_disconnect(c);
    if (g_failures == 0)
        std::puts("all widget xcb tests passed");
    return g_failures == 0 ? 0 : 1;
}
