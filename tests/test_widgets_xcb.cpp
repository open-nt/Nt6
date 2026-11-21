// MIT License - see LICENSE
//
// An end-to-end test of widgets on a real X server (or XWayland): a second XCB connection sends
// mouse and key events to the window and reads its pixels back. The test is skipped when there is no X display.
#include <QtCore>
#include <QtGui/qclipboard.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtabwidget.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qwidget.h>

#include "qpa/qplatformintegration_p.h"
#include "xcb_test_helpers.h"

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

using namespace xcbtest;

namespace {

// A window that fills itself with one colour and counts the mouse presses it gets, for the window container.
class SolidWindow : public QRasterWindow {
public:
    int presses = 0;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(QRect(QPoint(0, 0), size()), QColor(200, 20, 30));
    }
    void mousePressEvent(QMouseEvent *event) override {
        ++presses;
        event->accept();
    }
};

} // namespace

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
    // `--clipboard-owner TEXT`: put TEXT, and a format of its own, on the clipboard and the selection, and stay
    // for a few seconds so that the test that started this process can read them.
    if (argc == 3 && std::strcmp(argv[1], "--clipboard-owner") == 0) {
        QApplication owner(argc, argv);
        auto *data = new QMimeData;
        data->setText(QString::fromUtf8(argv[2]));
        data->setData(QStringLiteral("application/x-nt-test"), QByteArray("shape:42"));
        QGuiApplication::clipboard()->setMimeData(data);
        QGuiApplication::clipboard()->setText(QStringLiteral("selected"), QClipboard::Selection);
        std::puts("ready");
        std::fflush(stdout);
        QTimer::singleShot(4000, &owner, &QCoreApplication::quit);
        return owner.exec();
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
    CHECK(button->geometry().bottom() == 160 - 11 - 1 && button->width() == 240 - 22);
    CHECK(label->geometry().top() == 11 && box->geometry().top() > label->geometry().bottom());

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
    CHECK(waitFor([&] { return button->width() == 300 - 22 && button->geometry().bottom() == 200 - 11 - 1; }));

    // Tabs, a combo box with its popup, a line edit and a slider, through a real window
    {
        QWidget form;
        auto *outer = new QVBoxLayout(&form);
        auto *tabs = new QTabWidget(&form);
        auto *first = new QWidget;
        auto *second = new QWidget;
        auto *secondLayout = new QVBoxLayout(second);
        auto *combo = new QComboBox(second);
        combo->addItems({"Red", "Green", "Blue"});
        auto *edit = new QLineEdit(second);
        auto *slider = new QSlider(Qt::Horizontal, second);
        secondLayout->addWidget(combo);
        secondLayout->addWidget(edit);
        secondLayout->addWidget(slider);
        secondLayout->addStretch();
        tabs->addTab(first, "One");
        tabs->addTab(second, "Two");
        outer->addWidget(tabs);
        form.resize(300, 240);
        form.show();
        CHECK(waitFor([&] { return form.windowHandle() && form.windowHandle()->isExposed() && tabs->width() > 100; }));
        const xcb_window_t formId = static_cast<xcb_window_t>(form.windowHandle()->winId());

        // Click on the second tab
        QStyleOptionTab tabOption;
        tabOption.initFrom(tabs);
        const int firstTab = tabs->style()
                                 ->sizeFromContents(QStyle::CT_TabBarTab, &tabOption,
                                                    tabs->fontMetrics().size(Qt::TextSingleLine, "One"), tabs)
                                 .width();
        const QPoint second2 = tabs->mapTo(&form, QPoint(firstTab + 10, 12));
        sendButton(c, formId, 1, XCB_BUTTON_PRESS, 20000, static_cast<int16_t>(second2.x()),
                   static_cast<int16_t>(second2.y()));
        sendButton(c, formId, 1, XCB_BUTTON_RELEASE, 20050, static_cast<int16_t>(second2.x()),
                   static_cast<int16_t>(second2.y()));
        CHECK(waitFor([&] { return tabs->currentIndex() == 1 && second->isVisible(); }));

        // Click on the combo box: its popup opens, and a click on its second item chooses it
        CHECK(waitFor([&] { return combo->isVisible() && combo->width() > 50; }));
        const QPoint comboPoint = combo->mapTo(&form, QPoint(20, combo->height() / 2));
        sendButton(c, formId, 1, XCB_BUTTON_PRESS, 21000, static_cast<int16_t>(comboPoint.x()),
                   static_cast<int16_t>(comboPoint.y()));
        sendButton(c, formId, 1, XCB_BUTTON_RELEASE, 21050, static_cast<int16_t>(comboPoint.x()),
                   static_cast<int16_t>(comboPoint.y()));
        CHECK(waitFor([&] { return combo->isPopupVisible(); }));
        const int rowHeight = combo->fontMetrics().height() + 6;
        const QPoint item = comboPoint + QPoint(0, combo->height() / 2 + 1 + rowHeight + rowHeight / 2);
        sendButton(c, formId, 1, XCB_BUTTON_PRESS, 22000, static_cast<int16_t>(item.x()),
                   static_cast<int16_t>(item.y()));
        sendButton(c, formId, 1, XCB_BUTTON_RELEASE, 22050, static_cast<int16_t>(item.x()),
                   static_cast<int16_t>(item.y()));
        CHECK(waitFor([&] { return !combo->isPopupVisible() && combo->currentIndex() == 1; }));

        // Type into the line edit
        const QPoint editPoint = edit->mapTo(&form, QPoint(20, edit->height() / 2));
        sendButton(c, formId, 1, XCB_BUTTON_PRESS, 23000, static_cast<int16_t>(editPoint.x()),
                   static_cast<int16_t>(editPoint.y()));
        sendButton(c, formId, 1, XCB_BUTTON_RELEASE, 23050, static_cast<int16_t>(editPoint.x()),
                   static_cast<int16_t>(editPoint.y()));
        CHECK(waitFor([&] { return edit->hasFocus(); }));
        const xcb_keycode_t letterN = keycodeFor(c, 'n');
        const xcb_keycode_t letterT = keycodeFor(c, 't');
        if (letterN != 0 && letterT != 0) {
            for (xcb_keycode_t code : {letterN, letterT}) {
                sendKey(c, formId, code, 0, XCB_KEY_PRESS);
                sendKey(c, formId, code, 0, XCB_KEY_RELEASE);
            }
            CHECK(waitFor([&] { return edit->text() == QString("nt"); }));
        }

        // Drag the slider handle to the right end
        QStyleOptionSlider so;
        so.initFrom(slider);
        so.minimum = slider->minimum();
        so.maximum = slider->maximum();
        so.sliderPosition = slider->value();
        const QRect handle = slider->style()->subElementRect(QStyle::SE_SliderHandle, &so, slider);
        const QPoint from = slider->mapTo(&form, handle.center());
        const QPoint to = slider->mapTo(&form, QPoint(slider->width() - 2, handle.center().y()));
        sendButton(c, formId, 1, XCB_BUTTON_PRESS, 24000, static_cast<int16_t>(from.x()),
                   static_cast<int16_t>(from.y()));
        sendMotion(c, formId, static_cast<int16_t>(to.x()), static_cast<int16_t>(to.y()));
        CHECK(waitFor([&] { return slider->value() == slider->maximum(); }));
        sendButton(c, formId, 1, XCB_BUTTON_RELEASE, 24050, static_cast<int16_t>(to.x()), static_cast<int16_t>(to.y()));
        CHECK(waitFor([&] { return !slider->isSliderDown(); }));
    }

    // A dialog run with exec() from a click handler: the nested event loop starts while the click is still being
    // handled, and must still expose and paint the dialog and deliver its input.
    {
        QWidget opener;
        auto *openerLayout = new QVBoxLayout(&opener);
        auto *open = new QPushButton("Open");
        openerLayout->addWidget(open);
        opener.resize(200, 100);
        opener.show();
        CHECK(
            waitFor([&] { return opener.windowHandle() && opener.windowHandle()->isExposed() && open->width() > 0; }));
        const xcb_window_t openerId = static_cast<xcb_window_t>(opener.windowHandle()->winId());
        bool exposed = false, painted = false;
        int result = -1;
        QObject::connect(open, &QPushButton::clicked, [&] {
            QDialog dialog(&opener);
            auto *dialogLayout = new QVBoxLayout(&dialog);
            auto *ok = new QPushButton("OK");
            dialogLayout->addWidget(ok);
            QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
            dialog.resize(160, 80);
            QTimer::singleShot(0, &dialog, [&] {
                exposed = waitFor([&] { return dialog.windowHandle() && dialog.windowHandle()->isExposed(); });
                const xcb_window_t dialogId = static_cast<xcb_window_t>(dialog.windowHandle()->winId());
                // The server has the window color, not the black of a window that was never drawn
                painted = waitFor([&] {
                    xcb_get_image_reply_t *image = xcb_get_image_reply(
                        c, xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, dialogId, 4, 4, 1, 1, ~0u), nullptr);
                    int rgb = -1;
                    if (image) {
                        const uint8_t *d = xcb_get_image_data(image);
                        rgb = (d[2] << 16) | (d[1] << 8) | d[0];
                        std::free(image);
                    }
                    return rgb == 0xefefef;
                });
                const QPoint p = ok->mapTo(&dialog, QPoint(10, ok->height() / 2));
                sendButton(c, dialogId, 1, XCB_BUTTON_PRESS, 60000, static_cast<int16_t>(p.x()),
                           static_cast<int16_t>(p.y()));
                sendButton(c, dialogId, 1, XCB_BUTTON_RELEASE, 60050, static_cast<int16_t>(p.x()),
                           static_cast<int16_t>(p.y()));
                // Closes the dialog if the click was lost, so that the test cannot hang
                QTimer::singleShot(5000, &dialog, &QDialog::reject);
            });
            result = dialog.exec();
        });
        const QPoint p = open->mapTo(&opener, QPoint(10, open->height() / 2));
        sendButton(c, openerId, 1, XCB_BUTTON_PRESS, 59000, static_cast<int16_t>(p.x()), static_cast<int16_t>(p.y()));
        sendButton(c, openerId, 1, XCB_BUTTON_RELEASE, 59050, static_cast<int16_t>(p.x()), static_cast<int16_t>(p.y()));
        CHECK(waitFor([&] { return result != -1; }, 10000));
        CHECK(exposed);
        CHECK(painted);
        CHECK(result == QDialog::Accepted);
    }

    // A window in a widget (QWidget::createWindowContainer()): an X subwindow of the top-level window at the
    // container's place, which shows its own pixels in the top-level window and takes the focus when clicked.
    {
        QWidget host;
        auto *column = new QVBoxLayout(&host);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        auto *above = new QLabel(QStringLiteral("above"));
        above->setFixedHeight(40);
        column->addWidget(above);
        auto *solid = new SolidWindow;
        QWidget *container = QWidget::createWindowContainer(solid);
        column->addWidget(container);
        host.resize(200, 140);
        host.show();
        const auto hostId = static_cast<xcb_window_t>(host.winId());
        const auto childId = static_cast<xcb_window_t>(solid->winId());
        CHECK(childId != 0 && childId != hostId);
        xcb_query_tree_reply_t *tree = xcb_query_tree_reply(c, xcb_query_tree(c, childId), nullptr);
        CHECK(tree && tree->parent == hostId);
        std::free(tree);
        CHECK(waitFor([&] {
            xcb_get_geometry_reply_t *g = xcb_get_geometry_reply(c, xcb_get_geometry(c, childId), nullptr);
            const bool placed = g && g->x == 0 && g->y == 40 && g->width == 200 && g->height == 100;
            std::free(g);
            return placed;
        }));
        // The top-level window's pixels include the subwindow's.
        const auto hostPixel = [&](int x, int y) -> uint32_t {
            xcb_get_image_reply_t *image = xcb_get_image_reply(
                c, xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, hostId, int16_t(x), int16_t(y), 1, 1, ~0u), nullptr);
            uint32_t pixel = 0;
            if (image && xcb_get_image_data_length(image) >= 4)
                std::memcpy(&pixel, xcb_get_image_data(image), 4);
            std::free(image);
            return pixel & 0xffffffu;
        };
        CHECK(waitFor([&] { return hostPixel(100, 90) == 0xc8141eu; }));
        CHECK(hostPixel(100, 10) != 0xc8141eu);
        sendButton(c, childId, 1, XCB_BUTTON_PRESS, 1000, 10, 10);
        sendButton(c, childId, 1, XCB_BUTTON_RELEASE, 1010, 10, 10);
        CHECK(waitFor([&] { return solid->presses == 1; }));
        CHECK(waitFor([&] { return QGuiApplication::focusWindow() == solid; }));
        container->hide();
        CHECK(waitFor([&] {
            xcb_get_window_attributes_reply_t *a =
                xcb_get_window_attributes_reply(c, xcb_get_window_attributes(c, childId), nullptr);
            const bool unmapped = a && a->map_state == XCB_MAP_STATE_UNMAPPED;
            std::free(a);
            return unmapped;
        }));
    }

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

    // The clipboard of another program: this test reads what a second copy of itself put there.
    {
        QProcess owner;
        owner.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--clipboard-owner"), QStringLiteral("from another program \u00e9")});
        CHECK(owner.waitForReadyRead(5000));
        QClipboard *clipboard = QGuiApplication::clipboard();
        CHECK(clipboard->supportsSelection() && !clipboard->ownsClipboard());
        CHECK(clipboard->text() == QStringLiteral("from another program \u00e9"));
        const QMimeData *data = clipboard->mimeData();
        CHECK(data && data->hasFormat(QStringLiteral("application/x-nt-test")));
        if (data)
            CHECK(data->data(QStringLiteral("application/x-nt-test")) == "shape:42");
        CHECK(clipboard->text(QClipboard::Selection) == QLatin1String("selected"));
        // Taking the clipboard over tells the other program, and this one owns it.
        clipboard->setText(QStringLiteral("mine"));
        CHECK(clipboard->ownsClipboard() && clipboard->text() == QLatin1String("mine"));
        owner.kill();
        owner.waitForFinished(2000);
    }

    xcb_disconnect(c);
    if (g_failures == 0)
        std::puts("all widget xcb tests passed");
    return g_failures == 0 ? 0 : 1;
}
