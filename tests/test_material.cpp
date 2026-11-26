// MIT License - see LICENSE
//
// Tests for QMaterialStyle: the HCT color space that its schemes are made in, the color roles, the sizes and what is
// drawn, on the offscreen platform.
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>

#include "qmaterialcolor_p.h"

#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

using QtPrivate::Hct;
using Role = QMaterialStyle::Role;

bool near(const QColor &a, const QColor &b, int tolerance) {
    return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
           std::abs(a.blue() - b.blue()) <= tolerance;
}

bool near(double a, double b, double tolerance) { return std::abs(a - b) <= tolerance; }

void testHct() {
    // The HCT coordinates of the primaries of sRGB, as published with Material's color utilities.
    const struct {
        QColor color;
        double hue, chroma, tone;
    } known[] = {{QColor(255, 0, 0), 27.408, 113.357, 53.233},
                 {QColor(0, 255, 0), 142.139, 108.410, 87.737},
                 {QColor(0, 0, 255), 282.788, 87.230, 32.302}};
    for (const auto &k : known) {
        const Hct h = Hct::fromColor(k.color);
        CHECK(near(h.hue, k.hue, 0.1));
        CHECK(near(h.chroma, k.chroma, 0.1));
        CHECK(near(h.tone, k.tone, 0.05));
    }
    for (const QColor &c : {QColor(255, 0, 0), QColor(0x67, 0x50, 0xa4), QColor(0x00, 0x6c, 0x4c),
                            QColor(128, 128, 128), QColor(Qt::black), QColor(Qt::white), QColor(250, 210, 60)}) {
        const Hct h = Hct::fromColor(c);
        CHECK(near(Hct::toColor(h.hue, h.chroma, h.tone), c, 1));
    }
    // A chroma that sRGB does not reach gives the strongest color that it does, at the tone that was asked for.
    const QColor vivid = Hct::toColor(142, 200, 50);
    CHECK(vivid.isValid() && near(Hct::fromColor(vivid).tone, 50, 0.6));

    // The tones of the baseline primary palette, as Jetpack Compose's tokens have them.
    const Hct seed = Hct::fromColor(QColor(0x67, 0x50, 0xa4));
    const struct {
        int tone;
        QColor color;
    } tones[] = {{10, QColor(0x21, 0x00, 0x5d)}, {20, QColor(0x38, 0x1e, 0x72)}, {30, QColor(0x4f, 0x37, 0x8b)},
                 {40, QColor(0x67, 0x50, 0xa4)}, {50, QColor(0x7f, 0x67, 0xbe)}, {60, QColor(0x9a, 0x82, 0xdb)},
                 {70, QColor(0xb6, 0x9d, 0xf8)}, {80, QColor(0xd0, 0xbc, 0xff)}, {90, QColor(0xea, 0xdd, 0xff)}};
    for (const auto &t : tones)
        CHECK(near(Hct::toColor(seed.hue, seed.chroma, t.tone), t.color, 2));
}

// The relative luminance contrast of WCAG.
double contrast(const QColor &a, const QColor &b) {
    const auto luminance = [](const QColor &c) {
        const auto linear = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * linear(c.red() / 255.0) + 0.7152 * linear(c.green() / 255.0) +
               0.0722 * linear(c.blue() / 255.0);
    };
    const double x = luminance(a), y = luminance(b);
    return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}

void testScheme() {
    for (const QColor &seed : {QColor(0x67, 0x50, 0xa4), QColor(0x00, 0x6c, 0x4c), QColor(0xb3, 0x26, 0x1e),
                               QColor(250, 210, 60), QColor(128, 128, 128)}) {
        for (Qt::ColorScheme scheme : {Qt::ColorScheme::Light, Qt::ColorScheme::Dark}) {
            QMaterialStyle style(seed);
            style.setColorScheme(scheme);
            CHECK(style.isDark() == (scheme == Qt::ColorScheme::Dark));
            // Text on each color can be read, whatever the seed.
            const std::pair<Role, Role> pairs[] = {{Role::Primary, Role::OnPrimary},
                                                   {Role::PrimaryContainer, Role::OnPrimaryContainer},
                                                   {Role::Secondary, Role::OnSecondary},
                                                   {Role::SecondaryContainer, Role::OnSecondaryContainer},
                                                   {Role::TertiaryContainer, Role::OnTertiaryContainer},
                                                   {Role::Error, Role::OnError},
                                                   {Role::Surface, Role::OnSurface},
                                                   {Role::SurfaceContainerHighest, Role::OnSurfaceVariant},
                                                   {Role::InverseSurface, Role::InverseOnSurface}};
            for (const auto &[fill, ink] : pairs)
                CHECK(contrast(style.color(fill), style.color(ink)) >= 4.5);
            const QPalette palette = style.standardPalette();
            CHECK(palette.color(QPalette::Window) == style.color(Role::Surface));
            CHECK(palette.color(QPalette::Highlight) == style.color(Role::Primary));
            CHECK((Hct::fromColor(style.color(Role::Surface)).tone < 50) == style.isDark());
        }
    }
}

// The color of a pixel of what `widget` draws, at a point in logical pixels.
QColor pixel(QWidget &widget, int x, int y) {
    // Taken from the whole window, so that the background behind the widget is there.
    const QImage image = widget.window()->grab().toImage();
    const double ratio = image.devicePixelRatio();
    const QPoint at = widget.mapTo(widget.window(), QPoint(x, y));
    return image.pixelColor(static_cast<int>(at.x() * ratio), static_cast<int>(at.y() * ratio));
}

void testWidgets() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);
    CHECK(QApplication::palette().color(QPalette::Window) == style->color(Role::Surface));

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *filled = new QPushButton(QStringLiteral("Save"));
    filled->setProperty("materialButton", QStringLiteral("filled"));
    auto *tonal = new QPushButton(QStringLiteral("Share"));
    auto *outlined = new QPushButton(QStringLiteral("Cancel"));
    outlined->setProperty("materialButton", QStringLiteral("outlined"));
    auto *disabled = new QPushButton(QStringLiteral("No"));
    disabled->setEnabled(false);
    auto *edit = new QLineEdit;
    auto *check = new QCheckBox(QStringLiteral("Check"));
    check->setChecked(true);
    auto *radio = new QRadioButton(QStringLiteral("Radio"));
    auto *toggle = new QCheckBox(QStringLiteral("Wi-Fi"));
    toggle->setProperty("materialSwitch", true);
    for (QWidget *w : std::initializer_list<QWidget *>{filled, tonal, outlined, disabled, edit, check, radio, toggle})
        layout->addWidget(w);
    window.resize(320, 520);
    window.show();
    QCoreApplication::processEvents();

    // The sizes of the design, and heights that a finger can hit.
    CHECK(filled->sizeHint().height() == 40);
    CHECK(filled->sizeHint().width() >= 48);
    CHECK(edit->sizeHint().height() == 56);
    CHECK(check->sizeHint().height() >= 48);
    CHECK(radio->sizeHint().height() >= 48);
    CHECK(toggle->sizeHint().width() >= 52 + 12);
    CHECK(filled->font().pixelSize() == 14);
    CHECK(edit->font().pixelSize() == 16);

    // A pill: filled at its sides' middle, the background showing at its corners.
    CHECK(near(pixel(*filled, 8, filled->height() / 2), style->color(Role::Primary), 2));
    CHECK(near(pixel(*filled, 1, 1), style->color(Role::Surface), 2));
    CHECK(near(pixel(*tonal, 8, tonal->height() / 2), style->color(Role::SecondaryContainer), 2));
    CHECK(near(pixel(*outlined, 8, outlined->height() / 2), style->color(Role::Surface), 2));
    CHECK(!near(pixel(*disabled, 8, disabled->height() / 2), style->color(Role::SecondaryContainer), 2));

    // Pressed, the corners tighten: a pixel near the corner that the pill leaves out is filled.
    const QColor resting = pixel(*filled, 3, 5);
    filled->setDown(true);
    CHECK(!near(pixel(*filled, 3, 5), resting, 8));
    filled->setDown(false);

    // The switch: the handle is on the left when off and on the right when on, and a click toggles it.
    const int middle = toggle->height() / 2;
    CHECK(near(pixel(*toggle, 16, middle), style->color(Role::Outline), 3));
    CHECK(near(pixel(*toggle, 40, middle), style->color(Role::SurfaceContainerHighest), 3));
    toggle->click();
    CHECK(toggle->isChecked());
    CHECK(near(pixel(*toggle, 36, middle), style->color(Role::OnPrimary), 3));
    CHECK(near(pixel(*toggle, 8, middle), style->color(Role::Primary), 3));

    // A focus ring shows when the keyboard moves the focus, and not after a tap or a click.
    const auto ringShows = [&] {
        return near(pixel(*tonal, 1, tonal->height() / 2), style->color(Role::Secondary), 3);
    };
    const auto send = [&](QEvent &event) { QCoreApplication::sendEvent(&window, &event); };
    tonal->setFocus();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(1, 1), QPointF(1, 1), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    send(press);
    CHECK(tonal->hasFocus());
    CHECK(!ringShows());
    QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    send(tab);
    tonal->setFocus();
    CHECK(ringShows());
    send(press);
    CHECK(!ringShows());

    // The names that the factory knows, and going back to Fusion, which undoes the fonts.
    CHECK(QStyleFactory::keys().contains(QStringLiteral("Material3")));
    std::unique_ptr<QStyle> made(QStyleFactory::create(QStringLiteral("material3")));
    CHECK(made && made->name() == QStringLiteral("Material3"));
    CHECK(QApplication::setStyle(QStringLiteral("Fusion")) != nullptr);
    CHECK(filled->font().pixelSize() != 14);
}

void testSliderProgressTabs() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);
    slider->setValue(50);
    auto *progress = new QProgressBar;
    progress->setValue(50);
    progress->setTextVisible(false);
    auto *tabs = new QTabBar;
    tabs->addTab(QStringLiteral("First"));
    tabs->addTab(QStringLiteral("Second"));
    tabs->setExpanding(false);
    layout->addWidget(slider);
    layout->addWidget(progress);
    layout->addWidget(tabs);
    layout->addStretch(1);
    window.resize(320, 260);
    window.show();
    QCoreApplication::processEvents();

    CHECK(slider->sizeHint().height() == 44);
    CHECK(progress->sizeHint().height() >= 8);
    CHECK(tabs->sizeHint().height() == 48);
    CHECK(tabs->tabRect(0).width() >= 48);
    // Expanding tabs share the width of the bar.
    tabs->setExpanding(true);
    CHECK(tabs->tabRect(0).left() == 0 && tabs->tabRect(1).right() == tabs->width() - 1);
    CHECK(std::abs(tabs->tabRect(0).width() - tabs->tabRect(1).width()) <= tabs->width() / 8);
    // The line is as wide as the label, not as the wider tab.
    tabs->setCurrentIndex(0);
    const QRect wide = tabs->tabRect(0);
    CHECK(near(pixel(*tabs, wide.center().x(), wide.bottom() - 1), style->color(Role::Primary), 3));
    CHECK(!near(pixel(*tabs, wide.left() + 20, wide.bottom() - 1), style->color(Role::Primary), 20));
    tabs->setExpanding(false);
    CHECK(tabs->tabRect(1).right() < tabs->width() - 1);

    // The slider: the active part, the handle, the gap beside it, the rest, and the stop at the end.
    const int w = slider->width(), y = slider->height() / 2;
    CHECK(near(pixel(*slider, w / 4, y), style->color(Role::Primary), 2));
    CHECK(near(pixel(*slider, 3 * w / 4, y), style->color(Role::SecondaryContainer), 2));
    CHECK(near(pixel(*slider, w / 2, 3), style->color(Role::Primary), 3)); // the handle, above the track
    CHECK(near(pixel(*slider, w / 2 + 5, y), style->color(Role::Surface), 2));
    CHECK(near(pixel(*slider, w - 8, y), style->color(Role::OnSecondaryContainer), 3));
    CHECK(near(pixel(*slider, w / 4, 3), style->color(Role::Surface), 2));
    // A press where a finger lands moves the handle there, to the ends too.
    const auto pressAt = [&](int x) {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(x, y), slider->mapToGlobal(QPoint(x, y)), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(slider, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(x, y), slider->mapToGlobal(QPoint(x, y)),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(slider, &release);
    };
    pressAt(0);
    CHECK(slider->value() == 0);
    pressAt(w - 1);
    CHECK(slider->value() == 100);
    pressAt(w / 4);
    CHECK(std::abs(slider->value() - 25) <= 2);

    // The progress bar: done, a gap, what is left and the dot at its end.
    const int pw = progress->width(), py = progress->height() / 2;
    CHECK(near(pixel(*progress, pw / 4, py), style->color(Role::Primary), 3));
    CHECK(near(pixel(*progress, pw / 2 + 2, py), style->color(Role::Surface), 3));
    CHECK(near(pixel(*progress, 3 * pw / 4, py), style->color(Role::SecondaryContainer), 3));
    CHECK(near(pixel(*progress, pw - 2, py), style->color(Role::Primary), 40));

    // The tabs: the line under the selected one only, and it follows the selection.
    const auto indicatorUnder = [&](int tab) {
        const QRect r = tabs->tabRect(tab);
        return near(pixel(*tabs, r.center().x(), r.bottom() - 1), style->color(Role::Primary), 3);
    };
    CHECK(indicatorUnder(0));
    CHECK(!indicatorUnder(1));
    tabs->setCurrentIndex(1);
    CHECK(!indicatorUnder(0));
    CHECK(indicatorUnder(1));
    // The label of the selected tab has the accent color.
    const auto hasAccentText = [&](int tab) {
        const QRect r = tabs->tabRect(tab);
        for (int x = r.left(); x <= r.right(); ++x)
            for (int row = r.top() + 12; row < r.bottom() - 12; ++row)
                if (near(pixel(*tabs, x, row), style->color(Role::Primary), 8))
                    return true;
        return false;
    };
    CHECK(hasAccentText(1));
    CHECK(!hasAccentText(0));
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testComboAndMenu() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *combo = new QComboBox;
    combo->addItems({QStringLiteral("Walnut"), QStringLiteral("Pecan"), QStringLiteral("Almond")});
    layout->addWidget(combo);
    layout->addStretch(1);
    window.resize(320, 360);
    window.show();
    QCoreApplication::processEvents();

    CHECK(combo->sizeHint().height() == 56);
    CHECK(combo->font().pixelSize() == 16);
    // An outlined field: the outline at its edge, the background inside.
    CHECK(near(pixel(*combo, 0, combo->height() / 2), style->color(Role::Outline), 3));
    CHECK(near(pixel(*combo, 4, combo->height() / 2), style->color(Role::Surface), 2));

    // The list has rows of 48, in the container color of a menu; a press on the second row chooses it.
    combo->showPopup();
    QCoreApplication::processEvents();
    const QPoint below = combo->mapTo(&window, QPoint(0, combo->height()));
    const QImage open = window.grab().toImage();
    CHECK(near(open.pixelColor(below.x() + 200, below.y() + 48 + 6), style->color(Role::SurfaceContainer), 3));
    CHECK(near(open.pixelColor(below.x() + 200, below.y() + 6), style->color(Role::SecondaryContainer), 3));
    QWidget *popup = window.childAt(below + QPoint(20, 60));
    CHECK(popup && popup->height() == 3 * 48 + 2);
    if (popup) {
        const QPointF at(20, 1 + 48 + 24);
        QMouseEvent press(QEvent::MouseButtonPress, at, popup->mapToGlobal(at.toPoint()), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(popup, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, at, popup->mapToGlobal(at.toPoint()), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(popup, &release);
        CHECK(combo->currentText() == QStringLiteral("Pecan"));
    }

    QMenu menu;
    QAction *share = menu.addAction(QStringLiteral("&Share"));
    menu.addSeparator();
    QAction *bold = menu.addAction(QStringLiteral("Bold"));
    bold->setCheckable(true);
    bold->setChecked(true);
    CHECK(menu.sizeHint().width() >= 112);
    CHECK(menu.sizeHint().height() == 48 + 9 + 48);
    menu.popup(QPoint(10, 10));
    QCoreApplication::processEvents();
    CHECK(menu.actionGeometry(share).height() == 48);
    const QImage shown = menu.grab().toImage();
    CHECK(near(shown.pixelColor(shown.width() - 6, 8), style->color(Role::SurfaceContainer), 3));
    CHECK(near(shown.pixelColor(shown.width() / 2, 48 + 4), style->color(Role::OutlineVariant), 6));
    menu.hide();

    // A dialog has the container color of Material, text buttons for its actions, and rounded corners on Android.
    QDialog dialog(&window);
    auto *column = new QVBoxLayout(&dialog);
    column->addWidget(new QLabel(QStringLiteral("Delete the note?")));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    column->addWidget(buttons);
    dialog.resize(280, 140);
    dialog.show();
    QCoreApplication::processEvents();
    CHECK(dialog.palette().color(QPalette::Window) == style->color(Role::SurfaceContainerHigh));
    const QImage card = dialog.grab().toImage();
    CHECK(near(card.pixelColor(4, 4), style->color(Role::SurfaceContainerHigh), 2));
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
    CHECK(near(pixel(*ok, 8, ok->height() / 2), style->color(Role::SurfaceContainerHigh), 2));
    CHECK(dialog.windowHandle() && dialog.windowHandle()->property("_nt_cornerRadius").toInt() == 28);
    const QColor container = style->color(Role::SurfaceContainerHigh);
    QApplication::setStyle(QStringLiteral("Fusion")); // deletes the Material style
    CHECK(dialog.palette().color(QPalette::Window) != container);
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    testHct();
    testScheme();
    testWidgets();
    testSliderProgressTabs();
    testComboAndMenu();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
