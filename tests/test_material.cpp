// MIT License - see LICENSE
//
// Tests for QMaterialStyle: the HCT color space that its schemes are made in, the color roles, the sizes and what is
// drawn, on the offscreen platform.
#include <QtGui/QtGui>
#include <QtCore/qstringlistmodel.h>
#include <QtCore/qthread.h>
#include <QtWidgets/QtWidgets>

#include "qmaterialcolor_p.h"
#include "qmaterialmotion_p.h"
#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"

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

// False in a build without FreeType, as the one of the Check workflow is: no text is drawn, and there is none to look
// for.
bool textIsDrawn() {
    QImage image(40, 40, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::black);
    painter.drawText(QRect(0, 0, 40, 40), Qt::AlignCenter, QStringLiteral("M"));
    painter.end();
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y).red() < 128)
                return true;
    return false;
}

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
    // A desktop has no color of its own theme to take: a style that is given none has the baseline's violet.
    CHECK(!QMaterialStyle::systemSeedColor().isValid());
    CHECK(QMaterialStyle().seedColor() == QColor(0x67, 0x50, 0xa4));
    CHECK(QMaterialStyle(QColor(0x00, 0x6c, 0x4c)).seedColor() == QColor(0x00, 0x6c, 0x4c));
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
    style->setMotionEnabled(false); // the pixels are those of the shapes at rest
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
    style->setMotionEnabled(false); // the pixels are those of the shapes at rest
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
    if (textIsDrawn()) {
        CHECK(hasAccentText(1));
        CHECK(!hasAccentText(0));
    }
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testComboAndMenu() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false); // the pixels are those of the shapes at rest
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
    CHECK(menu.sizeHint().height() == 4 + 48 + 9 + 48 + 4); // 4 of padding above and below the rows
    menu.popup(QPoint(10, 10));
    QCoreApplication::processEvents();
    CHECK(menu.actionGeometry(share).height() == 48);
    const QImage shown = menu.grab().toImage();
    CHECK(near(shown.pixelColor(shown.width() - 6, 8), style->color(Role::SurfaceContainer), 3));
    CHECK(near(shown.pixelColor(shown.width() / 2, 4 + 48 + 4), style->color(Role::OutlineVariant), 6));
    menu.hide();
    // The corners are round (16, the standard menu of Material 3 Expressive), and the window shows at them.
    QWidget plain;
    plain.resize(300, 300);
    plain.show();
    QCoreApplication::processEvents();
    QMenu inWindow(&plain);
    inWindow.addAction(QStringLiteral("Share"));
    QAction *rename = inWindow.addAction(QStringLiteral("Rename"));
    inWindow.popup(plain.mapToGlobal(QPoint(40, 40)));
    inWindow.setActiveAction(inWindow.actions().first()); // a highlighted first row keeps inside the corners too
    QCoreApplication::processEvents();
    const QImage behind = plain.grab().toImage();
    const QPoint corner = inWindow.mapTo(&plain, QPoint(0, 0));
    const QColor windowColor = style->color(Role::Surface);
    CHECK(near(behind.pixelColor(corner + QPoint(1, 1)), windowColor, 3));
    CHECK(near(behind.pixelColor(corner + QPoint(inWindow.width() - 2, inWindow.height() - 2)), windowColor, 3));
    CHECK(!near(behind.pixelColor(corner + QPoint(16, 16)), windowColor, 3));
    CHECK(near(behind.pixelColor(corner + QPoint(inWindow.width() - 8, inWindow.actionGeometry(rename).center().y())),
               style->color(Role::SurfaceContainer), 3));
    inWindow.hide();

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

void testSpring() {
    using QtPrivate::Spring;
    // Starts where and as it was told to.
    const Spring expressive(0, 0, 100, QtPrivate::fastSpatialSpring);
    CHECK(near(expressive.at(0).position, 0, 1e-9));
    CHECK(near(expressive.at(0).velocity, 0, 1e-9));
    // The velocity is the derivative of the position, under- and critically damped.
    for (const QtPrivate::SpringSpec spec :
         {QtPrivate::fastSpatialSpring, QtPrivate::defaultSpatialSpring, QtPrivate::fastEffectsSpring}) {
        const Spring spring(10, -300, 50, spec);
        CHECK(near(spring.at(0).velocity, -300, 1e-6));
        for (double t : {0.01, 0.05, 0.1, 0.3}) {
            const double slope = (spring.at(t + 1e-6).position - spring.at(t - 1e-6).position) / 2e-6;
            CHECK(near(slope, spring.at(t).velocity, 1e-3 * (1 + std::abs(slope))));
        }
        CHECK(near(spring.at(3).position, 50, 1e-6));
        CHECK(spring.restsAt(3, 0.01));
        CHECK(!spring.restsAt(0.01, 0.01));
    }
    // The expressive spatial spring passes its mark, by exp(-pi z / sqrt(1 - z z)) of the way; an effects spring does
    // not.
    const auto farthest = [](const Spring &spring) {
        double most = 0;
        for (int i = 0; i < 2000; ++i)
            most = std::max(most, spring.at(i * 0.0005).position);
        return most;
    };
    const double zeta = 0.6;
    const double overshoot = std::exp(-3.14159265358979 * zeta / std::sqrt(1 - zeta * zeta));
    CHECK(near(farthest(expressive), 100 * (1 + overshoot), 0.05));
    CHECK(farthest(Spring(0, 0, 100, QtPrivate::fastEffectsSpring)) <= 100 + 1e-9);
}

void wait(int milliseconds) {
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < milliseconds) {
        QCoreApplication::processEvents();
        QThread::msleep(4);
    }
}

void testMotion() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);
    CHECK(style->isMotionEnabled());

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *button = new QPushButton(QStringLiteral("Save"));
    button->setProperty("materialButton", QStringLiteral("filled"));
    auto *toggle = new QCheckBox(QStringLiteral("Wi-Fi"));
    toggle->setProperty("materialSwitch", true);
    layout->addWidget(button);
    layout->addWidget(toggle);
    layout->addStretch(1);
    window.resize(300, 240);
    window.show();
    QCoreApplication::processEvents();

    // A corner that the pill leaves out, and the pressed shape fills.
    const auto corner = [&] { return pixel(*button, 3, 5); };
    const QColor resting = corner();
    button->setDown(true);
    CHECK(near(corner(), resting, 8)); // it sets off from the shape that it had
    wait(600);
    CHECK(!near(corner(), resting, 40)); // and comes to rest at the pressed one
    button->setDown(false);
    wait(600);
    CHECK(near(corner(), resting, 8));

    // The switch's handle travels: off on the left, then on the way, then on at the right.
    const int middle = toggle->height() / 2;
    CHECK(near(pixel(*toggle, 16, middle), style->color(Role::Outline), 3));
    toggle->setChecked(true);
    CHECK(!near(pixel(*toggle, 36, middle), style->color(Role::OnPrimary), 3));
    wait(600);
    CHECK(near(pixel(*toggle, 36, middle), style->color(Role::OnPrimary), 3));

    // The widgets are repainted while something moves, and left alone once everything rests.
    class Paints : public QObject {
    public:
        int count = 0;
        bool eventFilter(QObject *, QEvent *event) override {
            count += event->type() == QEvent::Paint ? 1 : 0;
            return false;
        }
    } paints;
    toggle->installEventFilter(&paints);
    toggle->setChecked(false);
    wait(150);
    CHECK(paints.count >= 3);
    wait(700);
    const int settled = paints.count;
    wait(200);
    CHECK(paints.count == settled);

    // Without motion the shape changes at once.
    style->setMotionEnabled(false);
    button->setDown(true);
    CHECK(!near(corner(), resting, 40));
    button->setDown(false);
    // A widget that is deleted while it moves is forgotten.
    style->setMotionEnabled(true);
    auto *brief = new QPushButton(QStringLiteral("Brief"), &window);
    brief->show();
    brief->grab();
    brief->setDown(true);
    brief->grab();
    delete brief;
    wait(100);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// The ripple of a press, colors that fade, and the line that slides from tab to tab.
void testRippleColorsAndTabs() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *button = new QPushButton(QStringLiteral("Save"));
    button->setProperty("materialButton", QStringLiteral("filled"));
    auto *toggle = new QCheckBox(QStringLiteral("Wi-Fi"));
    toggle->setProperty("materialSwitch", true);
    auto *box = new QCheckBox(QStringLiteral("Remember"));
    auto *tabs = new QTabBar;
    for (const char *name : {"One", "Two", "Three"})
        tabs->addTab(QString::fromLatin1(name));
    layout->addWidget(button);
    layout->addWidget(toggle);
    layout->addWidget(box);
    layout->addWidget(tabs);
    layout->addStretch(1);
    window.resize(360, 280);
    window.show();
    QCoreApplication::processEvents();

    const auto press = [&](QEvent::Type type, const QPointF &at) {
        QMouseEvent event(type, at, at, Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(button, &event);
    };
    // Two points of the container that the label leaves free: by the press, and at the other end.
    const int row = button->height() / 2;
    const auto nearPress = [&] { return pixel(*button, 24, row); };
    const auto farFromPress = [&] { return pixel(*button, button->width() - 24, row); };
    const QColor resting = farFromPress();
    press(QEvent::MouseButtonPress, QPointF(24, row));
    CHECK(near(farFromPress(), resting, 2)); // the circle has yet to spread
    wait(600);
    CHECK(!near(nearPress(), resting, 6));
    CHECK(near(nearPress(), farFromPress(), 2)); // it has filled the button
    press(QEvent::MouseButtonRelease, QPointF(24, row));
    wait(900);
    CHECK(near(nearPress(), resting, 2));
    CHECK(near(farFromPress(), resting, 2));

    // A switch's track and a check box's fill fade to the accent color.
    const int top = toggle->height() / 2 - 13;
    CHECK(near(pixel(*toggle, 26, top), style->color(Role::SurfaceContainerHighest), 3));
    toggle->setChecked(true);
    CHECK(!near(pixel(*toggle, 26, top), style->color(Role::Primary), 30));
    wait(600);
    CHECK(near(pixel(*toggle, 26, top), style->color(Role::Primary), 3));

    const auto accented = [&] {
        const QImage image = box->grab().toImage();
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < 24; ++x)
                count += near(image.pixelColor(x, y), style->color(Role::Primary), 3) ? 1 : 0;
        return count;
    };
    CHECK(accented() == 0);
    box->setChecked(true);
    CHECK(accented() == 0);
    wait(600);
    CHECK(accented() > 150);

    // The line sets off from under the old tab, and comes to rest under the new one.
    const auto indicatorUnder = [&](int tab) {
        const QRect r = tabs->tabRect(tab);
        return near(pixel(*tabs, r.center().x(), r.bottom() - 1), style->color(Role::Primary), 3);
    };
    CHECK(indicatorUnder(0));
    tabs->setCurrentIndex(2);
    CHECK(indicatorUnder(0));
    CHECK(!indicatorUnder(2));
    wait(800);
    CHECK(!indicatorUnder(0));
    CHECK(indicatorUnder(2));
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// The rows of lists, and what trees take from the style.
void testLists() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false); // the pixels are those of the shapes at rest
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *widgetList = new QListWidget;
    widgetList->addItems(QStringList{QStringLiteral("Inbox"), QStringLiteral("Sent"), QStringLiteral("Drafts")});
    auto *model = new QStringListModel(QStringList{QStringLiteral("Red"), QStringLiteral("Green")}, &window);
    auto *view = new QListView;
    view->setModel(model);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *tree = new QTreeView;
    auto *treeModel = new QStandardItemModel(&window);
    treeModel->appendRow(new QStandardItem(QStringLiteral("Folder")));
    tree->setModel(treeModel);
    layout->addWidget(widgetList);
    layout->addWidget(view);
    layout->addWidget(tree);
    window.resize(300, 560);
    window.show();
    QCoreApplication::processEvents();

    for (QAbstractItemView *each :
         {static_cast<QAbstractItemView *>(widgetList), static_cast<QAbstractItemView *>(view)}) {
        // A QListWidget has items and no model.
        const bool items = each == widgetList;
        const auto rowRect = [&](int n) {
            return items ? widgetList->visualItemRect(widgetList->item(n)) : view->visualRect(model->index(n, 0));
        };
        const auto deselect = [&] {
            if (items)
                widgetList->clearSelection();
            else
                view->selectionModel()->clearSelection();
        };
        const QRect row = rowRect(0);
        const QRect other = rowRect(1);
        CHECK(row.height() == 56);
        CHECK(each->font().pixelSize() == 16);
        QWidget *port = each->viewport();
        const auto inside = [&](const QRect &r) { return pixel(*port, r.right() - 30, r.center().y()); };
        const QColor plain = inside(row);

        // The selected row is a rounded container in the quiet accent color.
        each->setFocus();
        CHECK(each->hasFocus());
        if (items)
            widgetList->setCurrentRow(0);
        else
            view->setCurrentIndex(model->index(0, 0));
        CHECK(near(inside(row), style->color(Role::SecondaryContainer), 2));
        CHECK(near(pixel(*port, row.left() + 1, row.top() + 2), plain, 3));
        CHECK(near(inside(other), plain, 2));
        // After a tap or a call there is no focus ring around it.
        for (const int y : {row.top(), row.top() + 1})
            CHECK(near(pixel(*port, row.center().x(), y),
                       y == row.top() ? plain : style->color(Role::SecondaryContainer), 3));

        // A row that is held down is marked, until the finger is dragged away or lifted.
        const auto mouse = [&](QEvent::Type type, const QPointF &at) {
            QMouseEvent event(type, at, at, type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                              type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(port, &event);
        };
        const QPointF at(other.left() + 40, other.center().y());
        mouse(QEvent::MouseButtonPress, at);
        deselect();
        CHECK(!near(inside(other), plain, 6));
        mouse(QEvent::MouseButtonRelease, at);
        deselect();
        CHECK(near(inside(other), plain, 2));
        mouse(QEvent::MouseButtonPress, at);
        mouse(QEvent::MouseMove, at + QPointF(0, 30));
        deselect();
        CHECK(near(inside(other), plain, 2));
        mouse(QEvent::MouseButtonRelease, at + QPointF(0, 30));

        // With motion the press spreads from where it is.
        style->setMotionEnabled(true);
        deselect();
        mouse(QEvent::MouseButtonPress, at);
        deselect();
        CHECK(near(inside(other), plain, 2));
        wait(600);
        CHECK(!near(inside(other), plain, 6));
        mouse(QEvent::MouseButtonRelease, at);
        deselect();
        wait(900);
        CHECK(near(inside(other), plain, 2));
        style->setMotionEnabled(false);

        // Behind a scroller, which holds the press back, a drag marks no row, and a tap leaves none held.
        QScroller::grabGesture(port, QScroller::LeftMouseButtonGesture);
        deselect();
        mouse(QEvent::MouseButtonPress, at);
        CHECK(near(inside(other), plain, 2));
        mouse(QEvent::MouseButtonRelease, at);
        deselect();
        CHECK(near(inside(other), plain, 2));
        mouse(QEvent::MouseButtonPress, at);
        wait(350);
        deselect();
        CHECK(!near(inside(other), plain, 6)); // held long enough to be a press
        mouse(QEvent::MouseButtonRelease, at);
        deselect();
        CHECK(near(inside(other), plain, 2));
        QScroller::ungrabGesture(port);
    }

    // Material has no tree: it takes the colors, and rows that a finger can hit.
    CHECK(tree->visualRect(treeModel->index(0, 0)).height() == 48);
    CHECK(tree->palette().color(QPalette::Highlight) == style->color(Role::SecondaryContainer));
    CHECK(tree->palette().color(QPalette::HighlightedText) == style->color(Role::OnSecondaryContainer));

    // Another style gets its own colors and rows back.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(tree->palette().color(QPalette::Highlight) == QApplication::palette().color(QPalette::Highlight));
    CHECK(widgetList->rowHeight() < 40);
}

// A connected button group: the property on the widget that holds the buttons.
void testButtonGroup() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false); // the pixels are those of the shapes at rest
    QApplication::setStyle(style);

    QWidget window;
    auto *outer = new QVBoxLayout(&window);
    auto *group = new QWidget;
    group->setProperty("materialButtonGroup", QStringLiteral("connected"));
    auto *row = new QHBoxLayout(group);
    row->setContentsMargins(0, 0, 0, 0);
    QList<QPushButton *> buttons;
    for (const char *name : {"Day", "Week", "Month"}) {
        auto *button = new QPushButton(QString::fromLatin1(name));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        row->addWidget(button);
        buttons << button;
    }
    outer->addWidget(group);
    auto *lone = new QPushButton(QStringLiteral("Alone"));
    outer->addWidget(lone);
    outer->addStretch(1);
    window.resize(360, 200);
    window.show();
    QCoreApplication::processEvents();

    CHECK(buttons[1]->x() - (buttons[0]->x() + buttons[0]->width()) == 2);
    CHECK(buttons[2]->x() - (buttons[1]->x() + buttons[1]->width()) == 2);

    // A point by a corner, which a pill leaves out and a small corner takes in.
    enum Side { Left, Right };
    const auto filled = [&](QPushButton *button, Side side) {
        const QImage image = window.grab().toImage();
        const QPoint at = button->mapTo(&window, QPoint(side == Left ? 3 : button->width() - 4, 3));
        return !near(image.pixelColor(at.x(), at.y()), style->color(Role::Surface), 6);
    };
    CHECK(!filled(buttons[0], Left)); // the ends of the row are round
    CHECK(filled(buttons[0], Right));
    CHECK(filled(buttons[1], Left));
    CHECK(filled(buttons[1], Right));
    CHECK(filled(buttons[2], Left));
    CHECK(!filled(buttons[2], Right));
    CHECK(!filled(lone, Right)); // a button in no group is a pill

    // The checked button is a pill, and one that is pressed is not.
    buttons[1]->setChecked(true);
    CHECK(!filled(buttons[1], Left));
    CHECK(!filled(buttons[1], Right));
    buttons[1]->setDown(true);
    CHECK(filled(buttons[1], Left));
    buttons[1]->setDown(false);
    buttons[0]->setChecked(true);
    CHECK(filled(buttons[1], Left));

    // The end of the row is the last button that shows.
    buttons[2]->hide();
    QCoreApplication::processEvents();
    CHECK(!filled(buttons[1], Right));
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A QToolButton with MenuButtonPopup: two shapes, for the action and for the menu.
void testSplitButton() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *split = new QToolButton;
    split->setText(QStringLiteral("Send"));
    auto *menu = new QMenu(split);
    menu->addAction(QStringLiteral("Send later"));
    split->setMenu(menu);
    split->setPopupMode(QToolButton::MenuButtonPopup);
    layout->addWidget(split, 0, Qt::AlignLeft);
    layout->addStretch(1);
    window.resize(300, 240);
    window.show();
    QCoreApplication::processEvents();

    const int w = split->width(), h = split->height();
    CHECK(h == 40);
    CHECK(w == 16 + split->fontMetrics().horizontalAdvance(QStringLiteral("Send")) + 12 + 2 + 48);
    const auto at = [&](int x, int y) {
        const QPoint point = split->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor surface = style->color(Role::Surface);
    const QColor container = style->color(Role::SecondaryContainer);
    CHECK(near(at(w - 49, h / 2), surface, 2)); // the space between the two parts
    CHECK(near(at(w - 54, h / 2), container, 2));
    CHECK(near(at(w - 44, h / 2), container, 2));
    CHECK(near(at(2, 2), surface, 3)); // the outer ends are round,
    CHECK(near(at(w - 3, 2), surface, 3));
    CHECK(!near(at(w - 46, 3), surface, 6)); // and the corners between the parts small

    int clicks = 0;
    QObject::connect(split, &QToolButton::clicked, [&clicks] { ++clicks; });
    const auto click = [&](int x) {
        for (const QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
            const QPointF point(x, h / 2.0);
            QMouseEvent event(type, point, point, Qt::LeftButton,
                              type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(split, &event);
        }
    };
    click(20);
    CHECK(clicks == 1 && !menu->isVisible());
    click(w - 40); // anywhere on the second part, which is wide enough for a finger
    CHECK(clicks == 1 && menu->isVisible());
    CHECK(near(at(w - 46, 3), surface, 3)); // it is a pill of its own while the menu is open
    menu->hide();
    QCoreApplication::processEvents();
    CHECK(!near(at(w - 46, 3), surface, 6));
    // A button whose menu opens when it is held: that press is no click, and a short one is.
    split->setPopupMode(QToolButton::DelayedPopup);
    click(20);
    CHECK(clicks == 2 && !menu->isVisible());
    const QPointF point(20, h / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(split, &press);
    wait(800);
    CHECK(menu->isVisible());
    QMouseEvent release(QEvent::MouseButtonRelease, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(split, &release);
    CHECK(clicks == 2 && menu->isVisible());
    menu->hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A floating action button and its menu.
void testFab() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    layout->addStretch(1);
    auto *fab = new QPushButton;
    fab->setProperty("materialButton", QStringLiteral("fab"));
    layout->addWidget(fab, 0, Qt::AlignRight);
    auto *extended = new QPushButton(QStringLiteral("Compose"));
    extended->setProperty("materialButton", QStringLiteral("fab"));
    layout->addWidget(extended, 0, Qt::AlignRight);
    window.resize(320, 400);
    window.show();
    QCoreApplication::processEvents();

    CHECK(fab->size() == QSize(56, 58)); // with the room for its shadow
    CHECK(extended->height() == 58 &&
          extended->width() == 32 + extended->fontMetrics().horizontalAdvance(QStringLiteral("Compose")));
    const auto at = [&](QWidget *widget, int x, int y) {
        const QPoint point = widget->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor surface = style->color(Role::Surface);
    CHECK(near(at(fab, 10, 28), style->color(Role::PrimaryContainer), 2));
    CHECK(near(at(fab, 3, 3), surface, 4));                                   // rounded,
    CHECK(!near(at(fab, 6, 6), surface, 6));                                  // but not a circle
    CHECK(near(at(fab, 28, 28), style->color(Role::OnPrimaryContainer), 40)); // the plus

    auto *menu = new QMenu(fab);
    QAction *share = menu->addAction(QStringLiteral("Share"));
    menu->addAction(QStringLiteral("Start a new note"));
    fab->setMenu(menu);
    CHECK(fab->width() == 56);
    int shared = 0;
    QObject::connect(share, &QAction::triggered, [&shared] { ++shared; });
    fab->showMenu();
    QCoreApplication::processEvents();
    CHECK(menu->isVisible());
    // Above the button, their right edges in line, 8 from the last pill to the button.
    const QPoint corner = menu->mapToGlobal(QPoint(menu->width(), menu->height()));
    const QPoint button = fab->mapToGlobal(QPoint(fab->width(), 0));
    CHECK(corner.x() == button.x() && corner.y() == button.y() - 4);
    CHECK(menu->height() == 120);
    // A pill for each action, as wide as it needs, and nothing behind them.
    const QRect row = menu->actionGeometry(share);
    CHECK(near(at(menu, row.right() - 8, row.top() + 28), style->color(Role::PrimaryContainer), 2));
    CHECK(near(at(menu, 4, row.top() + 28), surface, 3));
    CHECK(near(at(menu, menu->width() - 40, row.top() + 58), surface, 3));
    // Its mark, the plus or the cross, is in the middle of the circle: where the ink is, on average.
    const auto markCentre = [&](const QColor &ink) {
        const QImage image = window.grab().toImage();
        const QPoint origin = fab->mapTo(&window, QPoint(0, 0));
        double x = 0, y = 0;
        int count = 0;
        for (int line = 8; line < 48; ++line)
            for (int column = 8; column < 48; ++column)
                if (near(image.pixelColor(origin.x() + column, origin.y() + line), ink, 60)) {
                    x += column + 0.5;
                    y += line + 0.5;
                    ++count;
                }
        return count ? QPointF(x / count, y / count) : QPointF();
    };
    const QPointF cross = markCentre(style->color(Role::OnPrimary));
    CHECK(near(cross.x(), 28.0, 1.0) && near(cross.y(), 28.0, 1.0));
    // The button is the circle that closes the menu.
    CHECK(near(at(fab, 10, 28), style->color(Role::Primary), 2));
    CHECK(near(at(fab, 6, 6), surface, 6));

    const QPointF in(row.right() - 20, row.top() + 28);
    QMouseEvent release(QEvent::MouseButtonRelease, in, in, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(menu, &release);
    QCoreApplication::processEvents();
    CHECK(shared == 1 && !menu->isVisible());
    CHECK(near(at(fab, 10, 28), style->color(Role::PrimaryContainer), 2));

    // Tapped, the button opens the menu, which stays when the finger is lifted.
    for (const QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        const QPointF middle(28, 28);
        QMouseEvent event(type, middle, middle, Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(fab, &event);
    }
    QCoreApplication::processEvents();
    CHECK(menu->isVisible());
    CHECK(near(at(fab, 10, 28), style->color(Role::Primary), 2));
    menu->hide();
    QCoreApplication::processEvents();

    // The menu is an ordinary one again below a button that is no FAB.
    fab->setProperty("materialButton", QStringLiteral("tonal"));
    fab->showMenu();
    QCoreApplication::processEvents();
    CHECK(menu->actionGeometry(share).height() == 48);
    menu->hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A busy progress bar moves, as a line or as the loading indicator.
void testBusy() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *line = new QProgressBar;
    line->setRange(0, 0);
    auto *loading = new QProgressBar;
    loading->setProperty("materialProgress", QStringLiteral("loading"));
    loading->setRange(0, 0);
    auto *contained = new QProgressBar;
    contained->setProperty("materialProgress", QStringLiteral("containedLoading"));
    contained->setRange(0, 0);
    layout->addWidget(line);
    layout->addWidget(loading, 0, Qt::AlignHCenter);
    layout->addWidget(contained, 0, Qt::AlignHCenter);
    layout->addStretch(1);
    window.resize(300, 200);
    window.show();
    QCoreApplication::processEvents();

    CHECK(loading->size() == QSize(48, 48));
    const auto count = [&](QWidget *widget, const QColor &color) {
        const QImage image = widget->grab().toImage();
        int found = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                found += near(image.pixelColor(x, y), color, 3) ? 1 : 0;
        return found;
    };
    // The shape fills most of a circle of 38, and the container the circle of 48.
    const int shape = count(loading, style->color(Role::Primary));
    CHECK(shape > 600 && shape < 1150);
    CHECK(pixel(*loading, 24, 24) == style->color(Role::Primary));
    CHECK(count(contained, style->color(Role::PrimaryContainer)) > 500);
    CHECK(near(pixel(*contained, 24, 24), style->color(Role::OnPrimaryContainer), 3));

    class Paints : public QObject {
    public:
        int count = 0;
        bool eventFilter(QObject *, QEvent *event) override {
            count += event->type() == QEvent::Paint ? 1 : 0;
            return false;
        }
    };
    for (QProgressBar *bar : {line, loading}) {
        Paints paints;
        bar->installEventFilter(&paints);
        bar->update();
        const QImage before = bar->grab().toImage();
        paints.count = 0;
        wait(300);
        CHECK(paints.count >= 5); // it goes on being painted,
        CHECK(bar->grab().toImage() != before);
        // until it is hidden,
        bar->hide();
        wait(100);
        paints.count = 0;
        wait(200);
        CHECK(paints.count == 0);
        bar->show();
        wait(200);
        CHECK(paints.count >= 3);
        // or has a range, when there is nothing left to move.
        bar->setRange(0, 100);
        bar->setValue(50);
        wait(150);
        paints.count = 0;
        wait(200);
        CHECK(paints.count == 0);
        bar->setRange(0, 0);
        bar->removeEventFilter(&paints);
    }

    // Without motion a busy bar stands still, with a part of it in the accent color.
    style->setMotionEnabled(false);
    for (QProgressBar *bar : {line, loading}) {
        const QImage first = bar->grab().toImage();
        wait(120);
        CHECK(bar->grab().toImage() == first);
        CHECK(count(bar, style->color(Role::Primary)) > 100);
    }
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A floating toolbar: the property on a QToolBar.
void testFloatingToolBar() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    layout->addStretch(1);
    auto *bar = new QToolBar;
    bar->addAction(QStringLiteral("A"));
    // The property may come after an action, as long as it is before the toolbar is shown.
    bar->setProperty("materialToolBar", QStringLiteral("floating"));
    QAction *bold = bar->addAction(QStringLiteral("B"));
    bold->setCheckable(true);
    QAction *off = bar->addAction(QStringLiteral("C"));
    off->setEnabled(false);
    layout->addWidget(bar, 0, Qt::AlignHCenter);
    window.resize(360, 220);
    window.show();
    QCoreApplication::processEvents();

    CHECK(bar->size() == QSize(4 + 3 * 48 + 4, 66)); // 64 and the room for the shadow
    const auto at = [&](int x, int y) {
        const QPoint point = bar->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor surface = style->color(Role::Surface);
    const QColor container = style->color(Role::SurfaceContainer);
    CHECK(near(at(3, 32), container, 2));
    CHECK(near(at(3, 3), surface, 3)); // a pill, with no grip although the toolbar is movable
    auto *button = qobject_cast<QToolButton *>(bar->widgetForAction(bold));
    CHECK(button && button->size() == QSize(48, 48));
    // A point in the shape of the second button, left of its label.
    const int x = button->x() + 10, y = button->y() + 24;
    CHECK(near(at(x, y), container, 2));
    int clicked = 0;
    QObject::connect(bold, &QAction::triggered, [&clicked] { ++clicked; });
    button->click();
    CHECK(clicked == 1 && bold->isChecked());
    CHECK(near(at(x, y), style->color(Role::SecondaryContainer), 2));
    CHECK(near(at(button->x() + 1, y), container, 2)); // the shape is smaller than the button

    // The label has the color of an icon of a toolbar: the darkest ink of the first button is near it.
    const auto inked = [&](const QColor &ink) {
        const QWidget *first = bar->widgetForAction(bar->actions().first());
        int found = 0;
        for (int row = 14; row < 34; ++row)
            for (int column = 14; column < 34; ++column)
                found += near(at(first->x() + column, first->y() + row), ink, 24) ? 1 : 0;
        return found;
    };
    if (textIsDrawn())
        CHECK(inked(style->color(Role::OnSurfaceVariant)) > 3);

    // The vibrant one, and a toolbar that stands up.
    bar->setProperty("materialToolBar", QStringLiteral("floatingVibrant"));
    bar->update();
    CHECK(near(at(3, 32), style->color(Role::PrimaryContainer), 2));
    CHECK(near(at(x, y), style->color(Role::SurfaceContainer), 2)); // what is checked
    bar->setOrientation(Qt::Vertical);
    QCoreApplication::processEvents();
    CHECK(bar->size() == QSize(64, 4 + 3 * 48 + 6));

    // Under another style it is a toolbar like any other, with its grip's room at the start.
    bar->setOrientation(Qt::Horizontal);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(bar->height() < 50);
    CHECK(bar->widgetForAction(bold)->x() >= 10);
}

// The items of a FAB menu grow from the button's side, one after another.
void testFabMenuMotion() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    layout->addStretch(1);
    auto *fab = new QPushButton;
    fab->setProperty("materialButton", QStringLiteral("fab"));
    auto *menu = new QMenu(fab);
    QAction *top = menu->addAction(QStringLiteral("A rather long item"));
    menu->addAction(QStringLiteral("Another long item"));
    menu->addAction(QStringLiteral("The one by the button"));
    fab->setMenu(menu);
    layout->addWidget(fab, 0, Qt::AlignRight);
    window.resize(320, 400);
    window.show();
    QCoreApplication::processEvents();

    // Near the left end of the pill of the item farthest from the button, left of its label.
    const auto farEnd = [&] {
        const QRect row = menu->actionGeometry(top);
        const int width = 48 + QFontMetrics([&] {
                                   QFont font = menu->font();
                                   font.setPixelSize(16);
                                   font.setWeight(QFont::Medium);
                                   return font;
                               }())
                                   .horizontalAdvance(top->text());
        const QPoint point = menu->mapTo(&window, QPoint(row.right() - width + 14, row.top() + 28));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor container = style->color(Role::PrimaryContainer);
    fab->showMenu();
    CHECK(!near(farEnd(), container, 3)); // it has yet to grow
    wait(800);
    CHECK(near(farEnd(), container, 2));

    // Once the items are open the menu is left alone.
    class Paints : public QObject {
    public:
        int count = 0;
        bool eventFilter(QObject *, QEvent *event) override {
            count += event->type() == QEvent::Paint ? 1 : 0;
            return false;
        }
    } paints;
    menu->installEventFilter(&paints);
    wait(250);
    CHECK(paints.count == 0);

    // The next time they grow again.
    menu->hide();
    QCoreApplication::processEvents();
    paints.count = 0;
    fab->showMenu();
    CHECK(!near(farEnd(), container, 3));
    wait(800);
    CHECK(near(farEnd(), container, 2));
    CHECK(paints.count >= 10);
    menu->hide();

    // Without motion they are all there at once.
    style->setMotionEnabled(false);
    fab->showMenu();
    CHECK(near(farEnd(), container, 2));
    menu->hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// The wavy progress bar.
void testWavyProgress() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *bar = new QProgressBar;
    bar->setProperty("materialProgress", QStringLiteral("wavy"));
    bar->setTextVisible(false);
    bar->setValue(60);
    layout->addWidget(bar);
    layout->addStretch(1);
    window.resize(300, 100);
    window.show();
    QCoreApplication::processEvents();

    CHECK(bar->height() >= 14); // room for the wave, or for its text line if that is higher
    // Ink above or below where a straight line of 4 would be, in the part that is done or in the track.
    const auto off = [&](double from, double to) {
        const QImage image = bar->grab().toImage();
        int found = 0;
        const double middle = image.height() / 2.0;
        for (int x = static_cast<int>(from * image.width()); x < static_cast<int>(to * image.width()); ++x)
            for (int y = 0; y < image.height(); ++y)
                if (std::abs(y + 0.5 - middle) > 3 && near(image.pixelColor(x, y), style->color(Role::Primary), 40))
                    ++found;
        return found;
    };
    wait(600); // the wave rises
    CHECK(off(0.05, 0.55) > 40);
    CHECK(off(0.65, 0.95) == 0); // the track is straight

    class Paints : public QObject {
    public:
        int count = 0;
        bool eventFilter(QObject *, QEvent *event) override {
            count += event->type() == QEvent::Paint ? 1 : 0;
            return false;
        }
    } paints;
    bar->installEventFilter(&paints);
    const QImage first = bar->grab().toImage();
    paints.count = 0;
    wait(210);
    CHECK(paints.count >= 4); // the wave travels
    CHECK(bar->grab().toImage() != first);

    // Done, or hardly begun, it is flat and left alone.
    for (const int value : {100, 5}) {
        bar->setValue(value);
        wait(700);
        paints.count = 0;
        wait(200);
        CHECK(paints.count == 0);
        CHECK(off(0.0, 1.0) == 0);
    }

    // Busy, its parts are waves.
    bar->setRange(0, 0);
    int most = 0;
    for (int i = 0; i < 6; ++i) {
        wait(80);
        most = std::max(most, off(0.0, 1.0));
    }
    CHECK(most > 20);

    // Without motion the wave stands still.
    bar->setRange(0, 100);
    bar->setValue(60);
    style->setMotionEnabled(false);
    const QImage still = bar->grab().toImage();
    wait(150);
    CHECK(bar->grab().toImage() == still);
    CHECK(off(0.05, 0.55) > 40);
    // A bar that did not ask for a wave has none.
    bar->setProperty("materialProgress", QVariant());
    CHECK(off(0.0, 1.0) == 0);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A press on a check box, a radio button, a switch, a tab and a row of a menu.
void testMoreRipples() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *box = new QCheckBox(QStringLiteral("Check"));
    auto *radio = new QRadioButton(QStringLiteral("Radio"));
    auto *toggle = new QCheckBox(QStringLiteral("Switch"));
    toggle->setProperty("materialSwitch", true);
    auto *tabs = new QTabBar;
    tabs->setExpanding(true);
    tabs->addTab(QStringLiteral("One"));
    tabs->addTab(QStringLiteral("Two"));
    layout->addWidget(box);
    layout->addWidget(radio);
    layout->addWidget(toggle);
    layout->addWidget(tabs);
    layout->addStretch(1);
    window.resize(320, 460);
    window.show();
    QCoreApplication::processEvents();

    // The three are in a column: the middle of a box, of a circle and of the handle of a switch that is off, 20 in.
    QStyleOptionButton option;
    option.rect = QRect(0, 0, 200, 48);
    CHECK(style->subElementRect(QStyle::SE_CheckBoxIndicator, &option, box) == QRect(11, 15, 18, 18));
    CHECK(style->subElementRect(QStyle::SE_RadioButtonIndicator, &option, radio) == QRect(10, 14, 20, 20));
    CHECK(style->subElementRect(QStyle::SE_CheckBoxIndicator, &option, toggle) == QRect(4, 8, 52, 32));
    CHECK(style->subElementRect(QStyle::SE_CheckBoxContents, &option, box).left() == 41);
    CHECK(box->sizeHint().width() == 11 + 18 + 12 + box->fontMetrics().horizontalAdvance(QStringLiteral("Check")));

    const auto at = [&](QWidget *widget, const QPoint &point) {
        const QPoint in = widget->mapTo(&window, point);
        return window.grab().toImage().pixelColor(in.x(), in.y());
    };
    const auto mouse = [&](QWidget *widget, QEvent::Type type, const QPointF &where) {
        QMouseEvent event(type, where, widget->mapToGlobal(where.toPoint()), Qt::LeftButton,
                          type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(widget, &event);
    };

    // The circle around the control, pressed anywhere on the button.
    for (QAbstractButton *button : {static_cast<QAbstractButton *>(box), static_cast<QAbstractButton *>(radio),
                                    static_cast<QAbstractButton *>(toggle)}) {
        const int middle = button->height() / 2;
        const QPoint inside(20, middle - 18);
        const QPoint outside(1, middle - 19);
        const QColor resting = at(button, inside);
        const QPointF press(button->sizeHint().width() - 6, middle); // on the label
        mouse(button, QEvent::MouseButtonPress, press);
        CHECK(near(at(button, inside), resting, 2)); // it has yet to spread, from the control
        wait(600);
        CHECK(!near(at(button, inside), resting, 5));
        CHECK(near(at(button, outside), resting, 2));
        CHECK(near(at(button, QPoint(static_cast<int>(press.x()), middle - 18)), resting, 2)); // not where pressed
        mouse(button, QEvent::MouseButtonRelease, press);
        wait(900);
        CHECK(near(at(button, inside), resting, 2));
        // Past the label there is nothing to press, and nothing to show.
        const QPointF past(button->width() - 4, middle);
        mouse(button, QEvent::MouseButtonPress, past);
        CHECK(!button->isDown());
        wait(300);
        CHECK(near(at(button, inside), resting, 2));
        mouse(button, QEvent::MouseButtonRelease, past);
    }

    // A tab: within the tab that was pressed.
    const QRect second = tabs->tabRect(1);
    const QPoint far(second.right() - 6, 10);
    const QPoint other(tabs->tabRect(0).left() + 6, 10);
    const QColor tabResting = at(tabs, far);
    mouse(tabs, QEvent::MouseButtonPress, QPointF(second.left() + 20, 24));
    wait(600);
    CHECK(!near(at(tabs, far), tabResting, 5));
    CHECK(near(at(tabs, other), tabResting, 2));
    mouse(tabs, QEvent::MouseButtonRelease, QPointF(second.left() + 20, 24));
    wait(900);
    CHECK(near(at(tabs, far), tabResting, 2));

    // A row of a menu; one that closed with a choice shows nothing of it when it opens again.
    auto *menu = new QMenu(tabs);
    QAction *first = menu->addAction(QStringLiteral("A rather long first item"));
    menu->addAction(QStringLiteral("Second"));
    menu->popup(window.mapToGlobal(QPoint(20, 260)));
    QCoreApplication::processEvents();
    const QRect row = menu->actionGeometry(first);
    CHECK(menu->width() >= row.width()); // as wide as its rows, in the style's font, the first time it opens
    const QPoint end(row.right() - 4, row.top() + 6);
    mouse(menu, QEvent::MouseMove, QPointF(40, row.center().y()));
    const QColor hovered = at(menu, end);
    mouse(menu, QEvent::MouseButtonPress, QPointF(40, row.center().y()));
    wait(600);
    CHECK(!near(at(menu, end), hovered, 5));
    mouse(menu, QEvent::MouseButtonRelease, QPointF(40, row.center().y()));
    QCoreApplication::processEvents();
    CHECK(!menu->isVisible());
    wait(300);
    menu->popup(window.mapToGlobal(QPoint(20, 260)));
    QCoreApplication::processEvents();
    mouse(menu, QEvent::MouseMove, QPointF(40, row.center().y()));
    CHECK(near(at(menu, end), hovered, 2));
    wait(100);
    CHECK(near(at(menu, end), hovered, 2));
    menu->hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// The details of the menus of Material 3 Expressive: the padding, the rows' own shapes, a checked row and groups.
void testMenuShapes() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(400, 400);
    window.show();
    QCoreApplication::processEvents();
    const QColor windowColor = style->color(Role::Surface);
    const QColor container = style->color(Role::SurfaceContainer);
    const QColor tertiary = style->color(Role::TertiaryContainer);

    QMenu menu(&window);
    QAction *cut = menu.addAction(QStringLiteral("Cut"));
    QAction *copy = menu.addAction(QStringLiteral("Copy"));
    QAction *bold = menu.addAction(QStringLiteral("Bold"));
    bold->setCheckable(true);
    bold->setChecked(true);
    QAction *line = menu.addSeparator();
    QAction *paste = menu.addAction(QStringLiteral("Paste"));
    menu.popup(window.mapToGlobal(QPoint(40, 40)));
    QCoreApplication::processEvents();
    const auto at = [&](const QPoint &point) { return window.grab().toImage().pixelColor(menu.mapTo(&window, point)); };

    // 4 of padding above and below the rows, which are inset 4 from the sides.
    CHECK(menu.height() == 4 + 3 * 48 + 9 + 48 + 4);
    CHECK(menu.actionGeometry(cut) == QRect(4, 4, menu.width() - 8, 48));
    CHECK(menu.actionGeometry(paste).bottom() == menu.height() - 5);

    // A row under the pointer has its own shape: in the padding and the inset there is none of its layer.
    menu.setActiveAction(cut);
    QCoreApplication::processEvents();
    const QRect first = menu.actionGeometry(cut);
    const QColor hovered = at(QPoint(first.center().x(), first.top() + 8));
    CHECK(!near(hovered, container, 2));
    CHECK(near(at(QPoint(first.center().x(), 2)), container, 2));
    CHECK(near(at(QPoint(2, first.top() + 24)), container, 2));
    // The first row is round at the top as the container is (12, its 16 less the inset), a row between others has
    // small corners.
    CHECK(near(at(first.topLeft() + QPoint(2, 2)), container, 2));
    menu.setActiveAction(copy);
    QCoreApplication::processEvents();
    const QRect middle = menu.actionGeometry(copy);
    CHECK(near(at(middle.topLeft() + QPoint(2, 2)), hovered, 3));
    CHECK(near(at(QPoint(middle.center().x(), middle.top() - 4)), container, 2)); // the row above is at rest

    // A checked row is a shape of 12 in the tertiary container color, its check mark in the color on it.
    menu.setActiveAction(nullptr);
    QCoreApplication::processEvents();
    const QRect checked = menu.actionGeometry(bold);
    CHECK(near(at(QPoint(checked.right() - 8, checked.center().y())), tertiary, 2));
    CHECK(near(at(checked.topLeft() + QPoint(2, 2)), container, 2));
    CHECK(near(at(checked.bottomRight() - QPoint(2, 2)), container, 2));
    const QImage image = window.grab().toImage();
    bool mark = false;
    for (int y = 12; y < 36 && !mark; ++y)
        for (int x = 12; x < 36 && !mark; ++x)
            mark = near(image.pixelColor(menu.mapTo(&window, checked.topLeft() + QPoint(x, y))),
                        style->color(Role::OnTertiaryContainer), 12);
    CHECK(mark);

    // Without the property a separator is a line; with it, the line becomes a gap between two groups, which the
    // window shows through. Setting it on a menu that is open lays the menu out anew.
    const QRect divider = menu.actionGeometry(line);
    CHECK(near(at(QPoint(menu.width() / 2, divider.center().y())), style->color(Role::OutlineVariant), 6));
    menu.setProperty("materialMenu", "grouped");
    QCoreApplication::processEvents();
    CHECK(menu.height() == 4 + 3 * 48 + (4 + 2 + 4) + 48 + 4);
    const QRect gap = menu.actionGeometry(line);
    CHECK(gap.height() == 10 && gap.top() == 4 + 3 * 48);
    CHECK(near(at(QPoint(menu.width() / 2, gap.top() + 4)), windowColor, 3));
    CHECK(near(at(QPoint(menu.width() / 2, gap.top() + 5)), windowColor, 3));
    CHECK(near(at(QPoint(menu.width() / 2, gap.top() + 2)), container, 3));
    CHECK(near(at(QPoint(menu.width() / 2, gap.top() + 7)), container, 3));
    // The corners beside the gap are 8, smaller than the menu's 16: a pixel 2 in and 4 above the gap is in the group.
    CHECK(!near(at(QPoint(2, gap.top())), windowColor, 6));
    CHECK(!near(at(QPoint(2, gap.bottom() - 1)), windowColor, 6));
    CHECK(near(at(QPoint(1, gap.top() + 3)), windowColor, 6));
    // The menu's own corners stay round.
    CHECK(near(at(QPoint(1, 1)), windowColor, 3));
    CHECK(near(at(QPoint(menu.width() - 2, menu.height() - 2)), windowColor, 3));
    menu.hide();

    // A section, a separator with a text, is no gap.
    QMenu sections(&window);
    sections.setProperty("materialMenu", "grouped");
    sections.addAction(QStringLiteral("One"));
    sections.addSection(QStringLiteral("More"));
    sections.addAction(QStringLiteral("Two"));
    sections.popup(window.mapToGlobal(QPoint(40, 40)));
    QCoreApplication::processEvents();
    for (int y = 0; y < sections.height(); ++y)
        CHECK(!near(window.grab().toImage().pixelColor(sections.mapTo(&window, QPoint(sections.width() / 2, y))),
                    windowColor, 3));
    sections.hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A rich tooltip: rich text, or the property of its widget, makes one; a plain tooltip stays as it was.
void testRichToolTips() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(480, 400);
    auto *button = new QPushButton(QStringLiteral("Grid"), &window);
    button->move(20, 20);
    window.show();
    QCoreApplication::processEvents();
    const auto tipLabel = [&]() -> QWidget * {
        for (QWidget *child : window.findChildren<QWidget *>())
            if (child->isVisible() && child->parentWidget() == &window && child != button)
                return child;
        return nullptr;
    };
    const auto at = [&](int x, int y) { return window.grab().toImage().pixelColor(x, y); };
    const QColor container = style->color(Role::SurfaceContainer);
    const QColor ink = style->color(Role::OnSurfaceVariant);

    // A plain text stays a plain tooltip.
    QToolTip::showText(window.mapToGlobal(QPoint(40, 40)), QStringLiteral("Search"), button);
    QCoreApplication::processEvents();
    QWidget *tip = tipLabel();
    CHECK(tip != nullptr);
    if (tip) {
        CHECK(!tip->property("_nt_materialRichToolTip").toBool());
        CHECK(tip->height() < 32);
        CHECK(near(at(tip->x() + 3, tip->y() + 12), style->color(Role::InverseSurface), 3));
    }

    // Rich text that starts with a bold run: the run is the subhead, over the supporting text, in a surface container.
    const QString rich = QStringLiteral("<b>Grid view</b><br>Shows the files as a grid of pictures, which is easier to "
                                        "scan when there are many of them.");
    QToolTip::showText(window.mapToGlobal(QPoint(40, 40)), rich, button);
    QCoreApplication::processEvents();
    tip = tipLabel();
    CHECK(tip != nullptr);
    CHECK(QToolTip::text() == rich);
    if (tip) {
        CHECK(tip->property("_nt_materialRichToolTip").toBool());
        CHECK(tip->font().pixelSize() == 14);
        CHECK(tip->width() <= 320);
        // 12 above, the subhead, 4, the supporting text, 12 below and 2 for the shadow.
        CHECK(tip->height() >= 12 + 14 + 4 + 14 + 12 + 2);
        if (textIsDrawn()) {
            CHECK(tip->width() > 240); // the supporting text wraps at the widest the tooltip may be
            CHECK(tip->height() >= 12 + 14 + 4 + 2 * 14 + 12 + 2);
        }
        const QPoint origin = tip->pos();
        CHECK(near(at(origin.x() + tip->width() / 2, origin.y() + 4), container, 3));  // the padding above the text
        CHECK(near(at(origin.x() + 6, origin.y() + tip->height() / 2), container, 3)); // and beside it
        CHECK(!near(at(origin.x(), origin.y()), container, 3));                        // its corners are round, 12
        CHECK(!near(at(origin.x() + 2, origin.y() + 2), container, 3));
        // The shadow lies in the 2 below the container, darker than the window.
        const QColor shadow = at(origin.x() + tip->width() / 2, origin.y() + tip->height() - 1);
        CHECK(shadow.lightness() < style->color(Role::Surface).lightness() - 10);
        if (textIsDrawn()) {
            const QImage image = window.grab().toImage();
            int inked = 0;
            for (int y = 12; y < tip->height() - 14; ++y)
                for (int x = 16; x < tip->width() - 16; ++x)
                    inked += near(image.pixelColor(origin.x() + x, origin.y() + y), ink, 40) ? 1 : 0;
            CHECK(inked > 20);
        }
    }

    // Rich text without a bold start is a rich tooltip too, with no subhead.
    QToolTip::showText(window.mapToGlobal(QPoint(40, 40)), QStringLiteral("Shows the files as <i>pictures</i>"),
                       button);
    QCoreApplication::processEvents();
    tip = tipLabel();
    CHECK(tip && tip->property("_nt_materialRichToolTip").toBool());

    // The property makes a plain text rich, its first line the subhead.
    button->setProperty("materialToolTip", QStringLiteral("rich"));
    QToolTip::showText(window.mapToGlobal(QPoint(40, 40)), QStringLiteral("Grid view\nShows the files as pictures"),
                       button);
    QCoreApplication::processEvents();
    tip = tipLabel();
    CHECK(tip && tip->property("_nt_materialRichToolTip").toBool());
    if (tip)
        CHECK(tip->height() >= 12 + 14 + 4 + 14 + 12 + 2);
    QToolTip::hideText();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// The extras of a menu row: group labels, supporting text and trailing icons.
void testMenuRowExtras() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(480, 480);
    window.show();
    QCoreApplication::processEvents();
    const QColor container = style->color(Role::SurfaceContainer);
    const QColor quiet = style->color(Role::OnSurfaceVariant);

    QMenu menu(&window);
    QAction *cut = menu.addAction(QStringLiteral("Cut"));
    QAction *recent = menu.addSection(QStringLiteral("Recent"));
    QAction *open = menu.addAction(QStringLiteral("Open\nOpens a file"));
    open->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    QAction *two = menu.addAction(QStringLiteral("Share\nSends a copy\nto someone else"));
    menu.popup(window.mapToGlobal(QPoint(20, 20)));
    QCoreApplication::processEvents();
    const auto at = [&](const QPoint &point) { return window.grab().toImage().pixelColor(menu.mapTo(&window, point)); };

    // A section is a group label of 32, with no line through it.
    const QRect label = menu.actionGeometry(recent);
    CHECK(label.height() == 32);
    CHECK(label.top() == 4 + 48);
    for (int x = label.left() + 90; x < label.right() - 1; x += 4)
        CHECK(near(at(QPoint(x, label.center().y())), container, 2));
    CHECK(near(at(QPoint(label.right() - 4, label.center().y())), container, 2));
    // A row with supporting text: a label of 24 over a line of 20, 8 above and below; one more line, 20 more.
    CHECK(menu.actionGeometry(open).height() == 60);
    CHECK(menu.actionGeometry(two).height() == 80);
    CHECK(menu.actionGeometry(cut).height() == 48);
    CHECK(menu.height() == 4 + 48 + 32 + 60 + 80 + 4);
    if (textIsDrawn()) {
        const QImage image = window.grab().toImage();
        const auto inked = [&](const QRect &area, const QColor &color) {
            int count = 0;
            for (int y = area.top(); y <= area.bottom(); ++y)
                for (int x = area.left(); x <= area.right(); ++x)
                    count += near(image.pixelColor(menu.mapTo(&window, QPoint(x, y))), color, 40) ? 1 : 0;
            return count;
        };
        // The group label's text, 12 in from the row, in the quiet color.
        CHECK(inked(QRect(label.left() + 12, label.top() + 8, 40, 16), quiet) > 4);
        CHECK(inked(QRect(label.left(), label.top(), 10, label.height()), quiet) == 0);
        // The supporting text, under the label, in the quiet color too.
        const QRect row = menu.actionGeometry(open);
        CHECK(inked(QRect(row.left() + 12, row.top() + 8 + 24, 60, 20), quiet) > 4);
        // The shortcut stays at the trailing end.
        CHECK(inked(QRect(row.right() - 60, row.top(), 48, row.height()), quiet) > 4);
    }
    const int plainWidth = menu.width();
    menu.hide();

    // A trailing icon: 24 at the trailing end of its row, an icon of one color in OnSurfaceVariant; every row makes
    // room for it.
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::black);
    QMenu icons(&window);
    icons.addAction(QStringLiteral("Cut"));
    icons.addSection(QStringLiteral("Recent"));
    QAction *withIcon = icons.addAction(QStringLiteral("Open\nOpens a file"));
    withIcon->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    icons.addAction(QStringLiteral("Share\nSends a copy\nto someone else"));
    withIcon->setProperty("materialTrailingIcon", QIcon(pixmap));
    icons.popup(window.mapToGlobal(QPoint(20, 20)));
    QCoreApplication::processEvents();
    CHECK(icons.width() == plainWidth + 24 + 12);
    const QRect row = icons.actionGeometry(withIcon);
    const QPoint centre(row.right() + 1 - 12 - 12, row.center().y());
    CHECK(near(window.grab().toImage().pixelColor(icons.mapTo(&window, centre)), quiet, 3));
    CHECK(near(window.grab().toImage().pixelColor(icons.mapTo(&window, QPoint(row.right() - 4, row.center().y()))),
               container, 2));
    icons.hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// Supporting text that wraps, icons of group labels, and the vertical menu whose submenus open in its place.
void testMenuWrapsAndOpensInPlace() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(640, 640);
    window.show();
    QCoreApplication::processEvents();
    const QColor container = style->color(Role::SurfaceContainer);
    const QColor quiet = style->color(Role::OnSurfaceVariant);
    const auto mouse = [&](QWidget *target, QEvent::Type type, const QPoint &at) {
        QMouseEvent event(type, QPointF(at), QPointF(at), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(target, &event);
        QCoreApplication::processEvents();
    };
    const auto click = [&](QWidget *target, const QPoint &at) {
        mouse(target, QEvent::MouseButtonPress, at);
        mouse(target, QEvent::MouseButtonRelease, at);
    };
    const auto key = [&](QWidget *target, int code) {
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier);
        QCoreApplication::sendEvent(target, &press);
        QCoreApplication::processEvents();
    };

    // Supporting text wraps between words so that the menu is no wider than 280; each line is 20 more.
    QMenu menu(&window);
    menu.addAction(QStringLiteral("Cut"));
    const QString words = QStringLiteral("word ").repeated(30).trimmed();
    QAction *share = menu.addAction(QStringLiteral("Share\n") + words);
    QAction *twice = menu.addAction(QStringLiteral("Send\n") + words + QLatin1Char('\n') + words);
    menu.popup(window.mapToGlobal(QPoint(20, 20)));
    QCoreApplication::processEvents();
    if (QFontMetrics(menu.font()).horizontalAdvance(words) > 280) {
        CHECK(menu.width() <= 280);
        CHECK(menu.width() >= 112);
        const int height = menu.actionGeometry(share).height();
        const int lines = (height - 8 - 24 - 8) / 20;
        CHECK(lines >= 2);
        CHECK(height == 8 + 24 + 20 * lines + 8);
        // A line break still starts a line: the two paragraphs are twice as many lines.
        CHECK(menu.actionGeometry(twice).height() == 8 + 24 + 40 * lines + 8);
        if (textIsDrawn()) {
            // The last line is drawn, in the quiet color.
            const QImage image = window.grab().toImage();
            const QRect row = menu.actionGeometry(share);
            int count = 0;
            for (int y = row.bottom() - 8 - 19; y <= row.bottom() - 8; ++y)
                for (int x = row.left() + 12; x < row.left() + 80; ++x)
                    count += near(image.pixelColor(menu.mapTo(&window, QPoint(x, y))), quiet, 40) ? 1 : 0;
            CHECK(count > 4);
        }
    }
    menu.hide();

    // A group label with an icon: 20 by 20, 12 in from the row, in OnSurfaceVariant, before the text.
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::black);
    QMenu sections(&window);
    sections.addAction(QStringLiteral("Cut"));
    QAction *recent = sections.addSection(QIcon(pixmap), QStringLiteral("Recent"));
    QAction *plain = sections.addSection(QStringLiteral("Recent"));
    sections.addAction(QStringLiteral("Open"));
    sections.popup(window.mapToGlobal(QPoint(20, 20)));
    QCoreApplication::processEvents();
    const QRect label = sections.actionGeometry(recent);
    CHECK(label.height() == 32);
    const auto at = [&](QWidget *list, const QPoint &point) {
        return window.grab().toImage().pixelColor(list->mapTo(&window, point));
    };
    CHECK(near(at(&sections, QPoint(label.left() + 12 + 10, label.center().y())), quiet, 3));
    CHECK(near(at(&sections, QPoint(label.left() + 8, label.center().y())), container, 2));
    const QRect noIcon = sections.actionGeometry(plain);
    CHECK(near(at(&sections, QPoint(noIcon.left() + 12 + 2, noIcon.top() + 4)), container, 2));
    sections.hide();

    // The vertical menu: a submenu's items take the place of the menu's, under a row that goes back.
    QMenu vertical(&window);
    vertical.setProperty("materialMenu", "vertical");
    QAction *cut = vertical.addAction(QStringLiteral("Cut"));
    QMenu *send = vertical.addMenu(QStringLiteral("Send"));
    QAction *mail = send->addAction(QStringLiteral("Mail"));
    send->addAction(QStringLiteral("Copy link"));
    QMenu *more = send->addMenu(QStringLiteral("More"));
    QAction *print = more->addAction(QStringLiteral("Print"));
    int shown = 0, hidden = 0;
    QObject::connect(send, &QMenu::aboutToShow, [&] { ++shown; });
    QObject::connect(send, &QMenu::aboutToHide, [&] { ++hidden; });
    QAction *triggered = nullptr;
    QObject::connect(&vertical, &QMenu::triggered, [&](QAction *action) { triggered = action; });
    QAction *sendTriggered = nullptr;
    QObject::connect(send, &QMenu::triggered, [&](QAction *action) { sendTriggered = action; });
    vertical.popup(window.mapToGlobal(QPoint(20, 20)));
    QCoreApplication::processEvents();
    const int topHeight = vertical.height();
    const QPoint sendRow = vertical.actionGeometry(send->menuAction()).center();
    // Resting on the submenu's row does not open it.
    mouse(&vertical, QEvent::MouseMove, sendRow);
    wait(400);
    CHECK(vertical.actionGeometry(mail).isEmpty());
    CHECK(!send->isVisible());
    // A click does: the row that goes back, 48 high, then the submenu's items; the submenu is not shown itself.
    click(&vertical, sendRow);
    CHECK(!send->isVisible());
    CHECK(vertical.isVisible());
    CHECK(vertical.actionGeometry(cut).isEmpty());
    const QRect mailRow = vertical.actionGeometry(mail);
    CHECK(mailRow.top() == 4 + 48);
    QAction *back = vertical.actionAt(QPoint(mailRow.center().x(), 4 + 24));
    CHECK(back && back != mail && back->text() == QStringLiteral("Send"));
    CHECK(vertical.height() == 4 + 4 * 48 + 4);
    CHECK(shown == 1 && hidden == 0);
    CHECK(vertical.activeAction() != mail); // a click selects nothing
    // The arrow back is drawn in the color of the label, where an icon goes.
    CHECK(near(at(&vertical, QPoint(mailRow.left() + 12 + 12, 4 + 24)), style->color(Role::OnSurface), 40));
    CHECK(near(at(&vertical, QPoint(mailRow.left() + 12 + 12, 4 + 24 - 8)), container, 2));
    // Right opens a submenu in place too, with its first item current; Left goes back, with the submenu's row current.
    vertical.setActiveAction(more->menuAction());
    key(&vertical, Qt::Key_Right);
    CHECK(vertical.activeAction() == print);
    CHECK(vertical.actionAt(QPoint(mailRow.center().x(), 4 + 24))->text() == QStringLiteral("More"));
    key(&vertical, Qt::Key_Left);
    CHECK(vertical.activeAction() == more->menuAction());
    CHECK(vertical.actionGeometry(mail).isValid());
    // Escape goes back to the menu's own items, which closes the submenu.
    key(&vertical, Qt::Key_Escape);
    CHECK(vertical.isVisible());
    CHECK(vertical.activeAction() == send->menuAction());
    CHECK(vertical.actionGeometry(cut).isValid() && vertical.actionGeometry(mail).isEmpty());
    CHECK(vertical.height() == topHeight);
    CHECK(shown == 1 && hidden == 1);
    // A click on the row that goes back does too.
    click(&vertical, sendRow);
    click(&vertical, QPoint(mailRow.center().x(), 4 + 24));
    CHECK(vertical.actionGeometry(cut).isValid());
    CHECK(shown == 2 && hidden == 2);
    // Choosing an item of the submenu triggers it, reports it from both menus and closes the menu.
    click(&vertical, sendRow);
    click(&vertical, vertical.actionGeometry(mail).center());
    CHECK(!vertical.isVisible());
    CHECK(triggered == mail && sendTriggered == mail);
    CHECK(hidden == 3);
    // It opens again with its own items.
    vertical.popup(window.mapToGlobal(QPoint(20, 20)));
    QCoreApplication::processEvents();
    CHECK(vertical.actionGeometry(cut).isValid() && vertical.actionGeometry(mail).isEmpty());
    vertical.hide();

    // Without the property a submenu opens beside the menu.
    vertical.setProperty("materialMenu", QVariant());
    vertical.popup(window.mapToGlobal(QPoint(20, 20)));
    QCoreApplication::processEvents();
    click(&vertical, sendRow);
    CHECK(send->isVisible());
    CHECK(vertical.actionGeometry(cut).isValid());
    vertical.hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// The five sizes of the common button, and the square shape.
void testButtonSizes() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    const struct {
        const char *name;
        int height, padding, labelPixels, pressedCorner;
    } sizes[] = {{"extraSmall", 32, 12, 14, 8},
                 {"small", 40, 16, 14, 8},
                 {"medium", 56, 24, 16, 12},
                 {"large", 96, 48, 24, 16},
                 {"extraLarge", 136, 64, 32, 16}};
    QList<QPushButton *> buttons;
    for (const auto &size : sizes) {
        auto *button = new QPushButton(QStringLiteral("A longer label"));
        button->setProperty("materialButtonSize", QString::fromLatin1(size.name));
        button->setProperty("materialButton", QStringLiteral("filled"));
        layout->addWidget(button, 0, Qt::AlignLeft);
        buttons << button;
    }
    auto *square = new QPushButton(QStringLiteral("Square"));
    square->setProperty("materialButtonSize", QStringLiteral("large"));
    square->setProperty("materialButtonShape", QStringLiteral("square"));
    square->setProperty("materialButton", QStringLiteral("filled"));
    square->setCheckable(true);
    layout->addWidget(square, 0, Qt::AlignLeft);
    auto *plain = new QPushButton(QStringLiteral("No size"));
    layout->addWidget(plain, 0, Qt::AlignLeft);
    auto *tool = new QToolButton;
    tool->setText(QStringLiteral("Tool"));
    tool->setProperty("materialButtonSize", QStringLiteral("medium"));
    layout->addWidget(tool, 0, Qt::AlignLeft);
    layout->addStretch(1);
    window.resize(640, 760);
    window.show();
    QCoreApplication::processEvents();

    for (int i = 0; i < buttons.size(); ++i) {
        const auto &size = sizes[i];
        const QPushButton *button = buttons[i];
        CHECK(button->height() == size.height);
        CHECK(button->font().pixelSize() == size.labelPixels);
        CHECK(button->font().weight() == (i < 3 ? QFont::Medium : QFont::Normal));
        CHECK(button->width() ==
              std::max(40, button->fontMetrics().horizontalAdvance(button->text()) + 2 * size.padding));
        // The room that the label is drawn in holds all of it.
        QStyleOptionButton option;
        option.rect = button->rect();
        CHECK(style->subElementRect(QStyle::SE_PushButtonContents, &option, button).width() >=
              button->fontMetrics().horizontalAdvance(button->text()));
    }
    CHECK(plain->height() == 40 && plain->font().pixelSize() == 14);
    CHECK(tool->height() == 56 && tool->font().pixelSize() == 16);

    // The label of a filled tool button has the color that is read on its container.
    tool->setProperty("materialButton", QStringLiteral("filled"));
    tool->update();
    if (textIsDrawn()) {
        const QImage image = window.grab().toImage();
        const QRect area(tool->mapTo(&window, QPoint(0, 0)), tool->size());
        int light = 0;
        for (int y = area.top() + 16; y < area.bottom() - 16; ++y)
            for (int x = area.left() + 24; x < area.right() - 24; ++x)
                light += near(image.pixelColor(x, y), style->color(Role::OnPrimary), 40) ? 1 : 0;
        CHECK(light > 8);
    }

    // A point on the diagonal of the top left corner, `in` from both edges.
    const auto filled = [&](QWidget *button, int in) {
        const QPoint point = button->mapTo(&window, QPoint(in, in));
        return near(window.grab().toImage().pixelColor(point.x(), point.y()), style->color(Role::Primary), 40);
    };
    QPushButton *large = buttons[3];
    CHECK(!filled(large, 9)); // a pill, 96 high: its corners are 48
    large->setDown(true);
    CHECK(filled(large, 9)); // corners of 16 while it is pressed
    CHECK(!filled(large, 2));
    large->setDown(false);
    CHECK(filled(square, 11)); // corners of 28 at rest
    CHECK(!filled(square, 5));
    square->setChecked(true);
    CHECK(!near(window.grab().toImage().pixelColor(square->mapTo(&window, QPoint(11, 11)).x(),
                                                   square->mapTo(&window, QPoint(11, 11)).y()),
                style->color(Role::Primary), 40)); // a pill while it is checked
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A spin box takes a label as an outlined field does: it sits on the outline while there is a value, and rests inside
// once the text has been cleared.
void testSpinBoxLabel() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.resize(400, 300);
    auto *spin = new QSpinBox(&window);
    spin->setRange(0, 99);
    spin->setValue(7);
    auto *plain = new QSpinBox(&window);
    auto *date = new QDateEdit(QDate(2020, 6, 17), &window);
    date->setProperty("materialLabel", QStringLiteral("Harvested"));
    auto *elsewhere = new QPushButton(QStringLiteral("Elsewhere"), &window);
    elsewhere->setGeometry(300, 250, 90, 40);
    window.show();
    elsewhere->setFocus();
    QCoreApplication::processEvents();
    CHECK(spin->sizeHint().height() == 56);
    spin->setProperty("materialLabel", QStringLiteral("Number of nuts"));
    QCoreApplication::processEvents();
    // The label takes 8 over the field, as on a line edit, and the text and the buttons stay in the field.
    CHECK(spin->sizeHint().height() == 8 + 56);
    CHECK(date->sizeHint().height() == 8 + 56);
    CHECK(plain->sizeHint().height() == 56);
    spin->setGeometry(10, 10, 220, 64);
    date->setGeometry(10, 100, 220, 64);
    plain->setGeometry(10, 190, 220, 56);
    QCoreApplication::processEvents();
    auto *line = spin->findChild<QLineEdit *>();
    CHECK(line && line->geometry() == QRect(16, 8 + 3, 220 - 16 - 96, 50));
    if (!line)
        return;
    QStyleOptionSpinBox option;
    option.initFrom(spin);
    CHECK(style->subElementRect(QStyle::SE_SpinBoxUp, &option, spin) == QRect(220 - 48, 8, 48, 56));

    const auto shot = [&](const QWidget *widget) { return window.grab().toImage().copy(widget->geometry()); };
    const auto inked = [&](const QImage &image, const QRect &area, Role role) {
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                count += near(image.pixelColor(x, y), style->color(role), 60) ? 1 : 0;
        return count;
    };
    // With a value the label has floated: the outline makes way for it, and is whole after it and below.
    QImage image = shot(spin);
    CHECK(is(image.pixelColor(14, 8), Role::Surface));
    CHECK(is(image.pixelColor(200, 8), Role::Outline));
    CHECK(is(image.pixelColor(110, 63), Role::Outline));
    CHECK(is(image.pixelColor(110, 4), Role::Surface)); // nothing over the field but the label
    if (textIsDrawn())
        CHECK(inked(image, QRect(16, 1, 60, 6), Role::OnSurfaceVariant) > 5);
    // The date edit's label floats the same way.
    CHECK(is(shot(date).pixelColor(14, 8), Role::Surface));
    // With the focus the label and the outline are in the accent color.
    line->setFocus();
    QCoreApplication::processEvents();
    image = shot(spin);
    CHECK(is(image.pixelColor(14, 8), Role::Surface));
    CHECK(is(image.pixelColor(200, 9), Role::Primary));
    if (textIsDrawn())
        CHECK(inked(image, QRect(16, 1, 60, 6), Role::Primary) > 5);
    // Cleared without the focus, the label rests inside, before the buttons, and the outline is whole. (Leaving the
    // spin box with no text puts its value back.)
    elsewhere->setFocus();
    line->clear();
    QCoreApplication::processEvents();
    CHECK(spin->text().isEmpty());
    image = shot(spin);

    CHECK(is(image.pixelColor(24, 8), Role::Outline));
    if (textIsDrawn()) {
        CHECK(inked(image, QRect(16, 1, 60, 6), Role::OnSurfaceVariant) == 0);
        CHECK(inked(image, QRect(16, 26, 100, 22), Role::OnSurfaceVariant) > 20);
        CHECK(inked(image, QRect(220 - 94, 26, 14, 22), Role::OnSurfaceVariant) < 3); // none up to the minus
    }
    // Without a frame there is no field, and no label.
    spin->setFrame(false);
    QCoreApplication::processEvents();
    CHECK(spin->sizeHint().height() == 56);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// Text fields: outlined and filled, with a label that floats, supporting text and an error.
void testTextFields() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *outlined = new QLineEdit;
    outlined->setProperty("materialLabel", QStringLiteral("Name of the nut"));
    outlined->setPlaceholderText(QStringLiteral("Acorn"));
    auto *filled = new QLineEdit;
    filled->setProperty("materialTextField", QStringLiteral("filled"));
    filled->setProperty("materialLabel", QStringLiteral("Weight"));
    filled->setProperty("materialSupportingText", QStringLiteral("In grams"));
    auto *plain = new QLineEdit;
    auto *elsewhere = new QPushButton(QStringLiteral("Elsewhere"));
    layout->addWidget(outlined);
    layout->addWidget(filled);
    layout->addWidget(plain);
    layout->addWidget(elsewhere);
    layout->addStretch(1);
    window.resize(340, 420);
    window.show();
    elsewhere->setFocus();
    QCoreApplication::processEvents();

    CHECK(outlined->height() == 8 + 56); // the room for the label on the outline
    CHECK(filled->height() == 56 + 20);  // and for the line below
    CHECK(plain->height() == 56);
    QStyleOptionFrame option;
    option.rect = filled->rect();
    option.lineWidth = 1;
    CHECK(style->subElementRect(QStyle::SE_LineEditContents, &option, filled) ==
          QRect(16, 22, filled->width() - 32, 30));

    const auto at = [&](QWidget *widget, int x, int y) {
        const QPoint point = widget->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor surface = style->color(Role::Surface);
    const QColor outline = style->color(Role::Outline);
    const QColor primary = style->color(Role::Primary);
    const QColor red = style->color(Role::Error);

    // At rest the outline is whole, where a label that floats would sit on it.
    CHECK(near(at(outlined, 24, 8), outline, 40));
    outlined->setText(QStringLiteral("x"));
    CHECK(near(at(outlined, 14, 8), surface, 3)); // it makes way for the label
    CHECK(near(at(outlined, outlined->width() - 24, 8), outline, 40));
    outlined->clear();
    CHECK(near(at(outlined, 24, 8), outline, 40));
    outlined->setFocus();
    CHECK(near(at(outlined, 14, 8), surface, 3));
    CHECK(near(at(outlined, outlined->width() - 24, 9), primary, 3)); // three wide, in the accent color

    // A filled field: rounded at the top, straight at the bottom, with a line that the focus thickens.
    const int w = filled->width();
    CHECK(near(at(filled, w - 40, 6), style->color(Role::SurfaceContainerHighest), 2));
    CHECK(near(at(filled, 0, 0), surface, 3));
    CHECK(near(at(filled, 0, 53), style->color(Role::SurfaceContainerHighest), 3));
    CHECK(near(at(filled, w - 40, 55), style->color(Role::OnSurfaceVariant), 3));
    CHECK(near(at(filled, w - 40, 54), style->color(Role::SurfaceContainerHighest), 3));
    CHECK(near(at(filled, w - 40, 66), surface, 3)); // the supporting text has no container
    filled->setFocus();
    CHECK(near(at(filled, w - 40, 54), primary, 3));

    // An error that comes while the field is shown turns it red, and moves nothing: the room was there.
    filled->setProperty("materialError", QStringLiteral("A weight is a number"));
    QCoreApplication::processEvents();
    CHECK(filled->height() == 76);
    CHECK(near(at(filled, w - 40, 55), red, 3));
    filled->setProperty("materialError", QString());
    CHECK(near(at(filled, w - 40, 55), primary, 3));
    // A field that had no line below makes room for one.
    plain->setProperty("materialError", QStringLiteral("Needed"));
    QCoreApplication::processEvents();
    CHECK(plain->height() == 76);
    CHECK(near(at(plain, plain->width() - 40, 0), red, 40));
    plain->setProperty("materialError", QVariant());
    QCoreApplication::processEvents();
    CHECK(plain->height() == 56);

    // The text of the label and of the line below, where text is drawn.
    if (textIsDrawn()) {
        const auto inked = [&](QWidget *widget, const QRect &area) {
            int found = 0;
            const QImage image = window.grab().toImage();
            const QPoint origin = widget->mapTo(&window, QPoint(0, 0));
            for (int y = area.top(); y <= area.bottom(); ++y)
                for (int x = area.left(); x <= area.right(); ++x)
                    found += image.pixelColor(origin.x() + x, origin.y() + y).lightness() < 170 ? 1 : 0;
            return found;
        };
        elsewhere->setFocus();
        outlined->clear();
        CHECK(inked(outlined, QRect(16, 26, 140, 20)) > 20); // the label rests where the text goes
        const int resting = inked(outlined, QRect(16, 26, 140, 20));
        outlined->setPlaceholderText(QString());
        CHECK(inked(outlined, QRect(16, 26, 140, 20)) == resting); // and the placeholder does not show through it
        outlined->setFocus();
        CHECK(inked(outlined, QRect(16, 2, 120, 11)) > 15); // up, on the outline
        CHECK(inked(filled, QRect(16, 60, 140, 14)) > 15);  // the supporting text
    }

    // Other properties of the style take effect on what is shown, too.
    elsewhere->setProperty("materialButtonSize", QStringLiteral("large"));
    QCoreApplication::processEvents();
    CHECK(elsewhere->height() == 96 && elsewhere->font().pixelSize() == 24);
    elsewhere->setProperty("materialButtonSize", QVariant());
    QCoreApplication::processEvents();
    CHECK(elsewhere->height() == 40 && elsewhere->font().pixelSize() == 14);
    auto *bar = new QToolBar;
    bar->addAction(QStringLiteral("A"));
    layout->insertWidget(0, bar, 0, Qt::AlignHCenter);
    bar->show();
    QCoreApplication::processEvents();
    bar->setProperty("materialToolBar", QStringLiteral("floating"));
    QCoreApplication::processEvents();
    CHECK(bar->height() == 66);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// Icon buttons, and icons that take the color of what they are on.
void testIconButtons() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    // An icon of one color, black, and one of two.
    const auto square = [](const QColor &left, const QColor &right) {
        QImage image(96, 96, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(QRect(16, 16, 32, 64), left);
        painter.fillRect(QRect(48, 16, 32, 64), right);
        painter.end();
        return QIcon(QPixmap::fromImage(image));
    };
    const QIcon plain = square(Qt::black, Qt::black);
    const QIcon logo = square(QColor(220, 30, 30), QColor(30, 60, 220));

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    const auto make = [&](const char *kind, bool checkable, const QIcon &icon) {
        auto *button = new QToolButton;
        button->setProperty("materialIconButton", QString::fromLatin1(kind));
        button->setCheckable(checkable);
        button->setIcon(icon);
        layout->addWidget(button, 0, Qt::AlignLeft);
        return button;
    };
    QToolButton *standard = make("standard", true, plain);
    QToolButton *filled = make("filled", false, plain);
    QToolButton *toggle = make("filled", true, plain);
    QToolButton *tonal = make("tonal", false, plain);
    QToolButton *outlined = make("outlined", true, plain);
    QToolButton *colored = make("tonal", false, logo);
    QToolButton *large = make("filled", false, plain);
    large->setProperty("materialButtonSize", QStringLiteral("large"));
    auto *push = new QPushButton;
    push->setProperty("materialIconButton", QStringLiteral("filled"));
    push->setIcon(plain);
    layout->addWidget(push, 0, Qt::AlignLeft);
    auto *worded = new QPushButton(QStringLiteral("Save"));
    worded->setProperty("materialButton", QStringLiteral("filled"));
    worded->setIcon(plain);
    layout->addWidget(worded, 0, Qt::AlignLeft);
    layout->addStretch(1);
    window.resize(320, 700);
    window.show();
    QCoreApplication::processEvents();

    CHECK(standard->size() == QSize(48, 48)); // 48 to a finger, though its shape is 40
    CHECK(push->size() == QSize(48, 48));
    CHECK(large->size() == QSize(96, 96));
    const auto at = [&](QWidget *widget, int x, int y) {
        const QPoint point = widget->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor surface = style->color(Role::Surface);
    // In the middle of the left half of the icon, and in the shape above the icon.
    const auto ink = [&](QWidget *widget) { return at(widget, widget->width() / 2 - 3, widget->height() / 2); };
    const auto container = [&](QWidget *widget) { return at(widget, widget->width() / 2, widget->height() / 2 - 16); };
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    CHECK(is(container(standard), Role::Surface) && is(ink(standard), Role::OnSurfaceVariant));
    CHECK(is(container(filled), Role::Primary) && is(ink(filled), Role::OnPrimary));
    CHECK(is(container(toggle), Role::SurfaceContainer) && is(ink(toggle), Role::OnSurfaceVariant));
    CHECK(is(container(tonal), Role::SecondaryContainer) && is(ink(tonal), Role::OnSecondaryContainer));
    CHECK(is(container(outlined), Role::Surface) && is(ink(outlined), Role::OnSurfaceVariant));
    CHECK(near(at(outlined, 24, 4), style->color(Role::OutlineVariant), 8)); // its line, 40 across
    CHECK(is(container(push), Role::Primary) && is(ink(push), Role::OnPrimary));
    CHECK(near(at(filled, 24, 1), surface, 3)); // the room around the shape
    for (QToolButton *each : {standard, toggle, outlined})
        each->setChecked(true);
    CHECK(is(ink(standard), Role::Primary));
    CHECK(is(container(toggle), Role::Primary) && is(ink(toggle), Role::OnPrimary));
    CHECK(is(container(outlined), Role::InverseSurface) && is(ink(outlined), Role::InverseOnSurface));

    // Several colors stay as they are.
    CHECK(near(at(colored, 20, 24), QColor(220, 30, 30), 3));
    CHECK(near(at(colored, 28, 24), QColor(30, 60, 220), 3));
    // A large one has an icon of 32: 16 of it either side of the middle, in a box of which 2/3 is painted.
    CHECK(is(at(large, 48 - 9, 48), Role::OnPrimary));
    CHECK(is(at(large, 48 - 13, 48), Role::Primary));
    // Disabled, the icon is faint, not the color of the application's.
    filled->setEnabled(false);
    const QColor faint = ink(filled);
    CHECK(faint.red() > 120 && faint.red() < 210);

    // The icon of a button with a label takes the color of the label.
    const auto count = [&](QWidget *widget, const QColor &color) {
        const QImage image = widget->grab().toImage();
        int found = 0;
        for (int y = 8; y < image.height() - 8; ++y)
            for (int x = 8; x < image.width() - 8; ++x)
                found += near(image.pixelColor(x, y), color, 3) ? 1 : 0;
        return found;
    };
    CHECK(count(worded, style->color(Role::OnPrimary)) > 60);
    CHECK(count(worded, QColor(Qt::black)) == 0);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A top app bar and a bottom navigation bar, together as an application has them.
void testAppBarAndNavigation() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    const auto dot = [] {
        QImage image(48, 48, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(QRect(8, 8, 32, 32), Qt::black);
        painter.end();
        return QIcon(QPixmap::fromImage(image));
    };
    QMainWindow window;
    auto *bar = new QToolBar;
    bar->setProperty("materialToolBar", QStringLiteral("appBar"));
    bar->setWindowTitle(QStringLiteral("Notes"));
    QAction *back = bar->addAction(dot(), QStringLiteral("Back"));
    QAction *separator = bar->addSeparator();
    QAction *search = bar->addAction(dot(), QStringLiteral("Search"));
    QAction *more = bar->addAction(dot(), QStringLiteral("More"));
    window.addToolBar(bar);
    auto *pages = new QTabWidget;
    pages->setTabPosition(QTabWidget::South);
    pages->tabBar()->setProperty("materialTabBar", QStringLiteral("navigationBar"));
    QList<QWidget *> content;
    for (const char *name : {"Home", "Search", "Settings"}) {
        content << new QWidget;
        pages->addTab(content.last(), dot(), QString::fromLatin1(name));
    }
    window.setCentralWidget(pages);
    window.resize(360, 420);
    window.show();
    QCoreApplication::processEvents();

    const auto at = [&](QWidget *widget, int x, int y) {
        const QPoint point = widget->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    // The app bar: 64 high across the window, one action before the title and two after it.
    CHECK(bar->height() == 64 && bar->width() == 360);
    CHECK(bar->y() == 0);
    const QWidget *backButton = bar->widgetForAction(back);
    const QWidget *searchButton = bar->widgetForAction(search);
    const QWidget *moreButton = bar->widgetForAction(more);
    CHECK(backButton->x() == 4 && backButton->width() == 48 && backButton->y() == 8);
    CHECK(moreButton->x() + moreButton->width() == 360 - 4);
    CHECK(searchButton->x() + searchButton->width() == moreButton->x());
    CHECK(bar->widgetForAction(separator)->isHidden()); // it only says where the title is
    CHECK(is(at(bar, 150, 4), Role::Surface));
    // What leads is darker than what follows.
    CHECK(is(at(const_cast<QWidget *>(backButton), 24, 24), Role::OnSurface));
    CHECK(is(at(const_cast<QWidget *>(searchButton), 24, 24), Role::OnSurfaceVariant));
    int triggered = 0;
    QObject::connect(back, &QAction::triggered, [&triggered] { ++triggered; });
    qobject_cast<QToolButton *>(const_cast<QWidget *>(backButton))->click();
    CHECK(triggered == 1);
    const auto titleInk = [&] {
        int found = 0;
        const QImage image = bar->grab().toImage();
        for (int y = 20; y < 44; ++y)
            for (int x = 56; x < 200; ++x)
                found += image.pixelColor(x, y).lightness() < 128 ? 1 : 0;
        return found;
    };
    if (textIsDrawn()) {
        CHECK(titleInk() > 40);
        bar->setWindowTitle(QString());
        CHECK(titleInk() == 0);
        bar->setWindowTitle(QStringLiteral("Notes"));
    }

    // The navigation bar: 64 high at the bottom, three destinations of equal width.
    QTabBar *navigation = pages->tabBar();
    CHECK(navigation->height() == 64 && navigation->width() == 360);
    CHECK(navigation->font().pixelSize() == 12);
    CHECK(navigation->mapTo(&window, QPoint(0, 63)).y() == 419);
    for (int i = 0; i < 3; ++i)
        CHECK(navigation->tabRect(i) == QRect(120 * i, 0, 120, 64));
    const int pageTop = content[0]->mapTo(&window, QPoint(0, 0)).y();
    CHECK(content[0]->isVisible() && pageTop >= 64 && pageTop <= 66); // under the app bar, in the tab widget
    // A pill behind the icon of the destination that is shown, and none behind the others.
    const auto pill = [&](int tab) { return at(navigation, 120 * tab + 60 - 22, 6 + 16); };
    const auto icon = [&](int tab) { return at(navigation, 120 * tab + 60, 6 + 16); };
    CHECK(is(pill(0), Role::SecondaryContainer) && is(icon(0), Role::OnSecondaryContainer));
    CHECK(is(pill(1), Role::SurfaceContainer) && is(icon(1), Role::OnSurfaceVariant));
    const QPointF second(180, 30);
    for (const QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent event(type, second, navigation->mapToGlobal(second.toPoint()), Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(navigation, &event);
    }
    QCoreApplication::processEvents();
    CHECK(pages->currentIndex() == 1 && content[1]->isVisible() && !content[0]->isVisible());
    CHECK(is(pill(1), Role::SecondaryContainer) && is(pill(0), Role::SurfaceContainer));

    // With motion the pill opens from the middle.
    style->setMotionEnabled(true);
    // A destination that is held shows the press in its pill.
    const auto hold = [&](QEvent::Type type) {
        const QPointF third(300, 30);
        QMouseEvent event(type, third, navigation->mapToGlobal(third.toPoint()), Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(navigation, &event);
    };
    navigation->grab();
    hold(QEvent::MouseButtonPress);
    wait(700);
    CHECK(!is(pill(2), Role::SecondaryContainer)); // darker than the pill alone
    hold(QEvent::MouseButtonRelease);
    wait(900);
    CHECK(is(pill(2), Role::SecondaryContainer));
    pages->setCurrentIndex(1);
    wait(700);
    navigation->grab(); // the bar as it is, for the pills to set off from
    pages->setCurrentIndex(2);
    CHECK(!is(pill(2), Role::SecondaryContainer));
    wait(700);
    CHECK(is(pill(2), Role::SecondaryContainer));
    style->setMotionEnabled(false);

    // Under another style they are a toolbar and tabs, with the separator that was the title's place.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(bar->height() < 50);
    CHECK(!bar->widgetForAction(separator)->isHidden());
    CHECK(navigation->height() < 40);
}

// A snackbar, a tooltip and a circular progress indicator.
void testSnackbarTooltipCircular() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QMainWindow window;
    auto *pages = new QTabWidget;
    pages->setTabPosition(QTabWidget::South);
    pages->tabBar()->setProperty("materialTabBar", QStringLiteral("navigationBar"));
    auto *page = new QWidget;
    pages->addTab(page, QStringLiteral("Home"));
    window.setCentralWidget(pages);
    auto *status = new QStatusBar;
    status->setProperty("materialStatusBar", QStringLiteral("snackbar"));
    window.setStatusBar(status);
    window.resize(360, 400);
    window.show();
    QCoreApplication::processEvents();

    const auto at = [&](int x, int y) { return window.grab().toImage().pixelColor(x, y); };
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    const auto navigationTop = [&] { return pages->tabBar()->mapTo(&window, QPoint(0, 0)).y(); };
    // Without a message there is no snackbar, and the pages reach the bottom.
    CHECK(status->height() == 0);
    CHECK(navigationTop() + pages->tabBar()->height() >= 398);

    status->showMessage(QStringLiteral("Note deleted"));
    QCoreApplication::processEvents();
    CHECK(status->x() == 16 && status->width() == 360 - 32);
    CHECK(status->height() == 48 + 2); // with its shadow
    CHECK(status->y() + status->height() == navigationTop() - 16);
    CHECK(navigationTop() + pages->tabBar()->height() >= 398); // it takes no room
    CHECK(status->font().pixelSize() == 14);
    CHECK(is(at(200, status->y() + 6), Role::InverseSurface));
    CHECK(is(at(8, status->y() + 6), Role::Surface));

    // An action is a permanent widget, drawn as a text button in the inverse accent.
    auto *undo = new QPushButton(QStringLiteral("Undo"));
    status->addPermanentWidget(undo);
    QCoreApplication::processEvents();
    CHECK(undo->isVisible() && undo->height() == 40);
    CHECK(undo->x() + undo->width() <= status->width() && undo->x() > status->width() / 2);
    if (textIsDrawn()) {
        const QImage image = window.grab().toImage();
        const QPoint origin = undo->mapTo(&window, QPoint(0, 0));
        int accented = 0;
        for (int y = 8; y < 32; ++y)
            for (int x = 4; x < undo->width() - 4; ++x)
                accented +=
                    near(image.pixelColor(origin.x() + x, origin.y() + y), style->color(Role::InversePrimary), 30) ? 1
                                                                                                                   : 0;
        CHECK(accented > 8);
    }
    // A long message takes two lines.
    status->showMessage(QStringLiteral("The note could not be saved because the storage of the device is full"));
    QCoreApplication::processEvents();
    CHECK(status->height() == (textIsDrawn() ? 70 : status->height()));
    // It goes when its time is up.
    status->showMessage(QStringLiteral("Saved"), 150);
    QCoreApplication::processEvents();
    CHECK(status->height() == 50);
    wait(400);
    CHECK(status->height() == 0);
    CHECK(is(at(200, navigationTop() - 40), Role::Surface));

    // A tooltip: dark in a light scheme, with small text, rounded, and at least 24 high.
    QToolTip::showText(window.mapToGlobal(QPoint(100, 100)), QStringLiteral("Search"), page);
    QCoreApplication::processEvents();
    CHECK(QToolTip::isVisible());
    QWidget *tip = nullptr;
    for (QWidget *child : window.findChildren<QWidget *>()) {
        if (child->isVisible() && child->parentWidget() == &window && child != status && child != pages)
            tip = child;
    }
    CHECK(tip != nullptr);
    if (tip) {
        CHECK(tip->height() >= 24 && tip->height() < 32);
        CHECK(tip->font().pixelSize() == 12);
        const QColor inside = at(tip->x() + 3, tip->y() + 12);
        CHECK(is(inside, Role::InverseSurface));
        CHECK(!is(at(tip->x(), tip->y()), Role::InverseSurface)); // its corners are round
    }
    QToolTip::hideText();

    // A circular progress indicator.
    auto *circle = new QProgressBar(page);
    circle->setProperty("materialProgress", QStringLiteral("circular"));
    circle->setValue(25);
    circle->move(20, 20);
    circle->resize(circle->sizeHint());
    circle->show();
    QCoreApplication::processEvents();
    CHECK(circle->sizeHint() == QSize(40, 40));
    const auto ring = [&](int degrees) { // clockwise from the top
        const double radians = degrees * 3.14159265358979323846 / 180.0;
        const QImage image = circle->grab().toImage();
        return image.pixelColor(static_cast<int>(20 + 18 * std::sin(radians)),
                                static_cast<int>(20 - 18 * std::cos(radians)));
    };
    CHECK(near(ring(45), style->color(Role::Primary), 40));             // a quarter is done,
    CHECK(near(ring(200), style->color(Role::SecondaryContainer), 12)); // and the rest is track
    CHECK(near(ring(300), style->color(Role::SecondaryContainer), 12));
    circle->setValue(100);
    CHECK(near(ring(200), style->color(Role::Primary), 40));
    // Busy, it goes round while it is shown, and stands still without motion.
    circle->setRange(0, 0);
    const QImage still = circle->grab().toImage();
    wait(150);
    CHECK(circle->grab().toImage() == still);
    style->setMotionEnabled(true);
    const QImage first = circle->grab().toImage();
    wait(200);
    CHECK(circle->grab().toImage() != first);
    circle->hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testChipsCardsDividers() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(360, 400);
    window.show();
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    const auto chip = [&](const char *kind, const QString &text, int y) {
        auto *button = new QPushButton(text, &window);
        button->setProperty("materialChip", QLatin1String(kind));
        button->move(10, y);
        button->resize(button->sizeHint());
        button->show();
        return button;
    };
    // As the window shows a widget: a grab of its own has nothing behind it.
    const auto seen = [&](const QWidget *widget) { return window.grab().toImage().copy(widget->geometry()); };
    QPushButton *assist = chip("assist", QStringLiteral("Directions"), 0);
    QPushButton *filter = chip("filter", QStringLiteral("Unread"), 50);
    filter->setCheckable(true);
    QPushButton *input = chip("input", QStringLiteral("Hazel"), 100);
    QCoreApplication::processEvents();

    // 48 to a finger, 32 to the eye; 16 on each side of a label that has nothing there.
    const int label = assist->fontMetrics().horizontalAdvance(assist->text());
    CHECK(assist->sizeHint() == QSize(label + 32, 48));
    CHECK(assist->font().pixelSize() == 14);
    // An outline around nothing, with small corners.
    const QImage plain = seen(assist);
    CHECK(is(plain.pixelColor(assist->width() / 2, 8), Role::OutlineVariant));
    CHECK(is(plain.pixelColor(assist->width() / 2, 39), Role::OutlineVariant));
    CHECK(is(plain.pixelColor(0, 24), Role::OutlineVariant));
    CHECK(is(plain.pixelColor(assist->width() / 2, 3), Role::Surface));
    CHECK(is(plain.pixelColor(assist->width() / 2, 44), Role::Surface));
    CHECK(is(plain.pixelColor(assist->width() / 2, 12), Role::Surface));
    CHECK(is(plain.pixelColor(0, 8), Role::Surface));

    // An icon takes the room of the padding before the label, and is in the primary color.
    QPixmap square(18, 18);
    square.fill(Qt::black);
    assist->setIcon(QIcon(square));
    CHECK(assist->sizeHint().width() == label + 34 + 16);
    assist->resize(assist->sizeHint());
    CHECK(is(seen(assist).pixelColor(17, 24), Role::Primary));

    // A filter chip that is chosen is filled, loses its outline and has a check mark before its label, which makes it
    // wider.
    const int narrow = filter->sizeHint().width();
    int clicks = 0;
    QObject::connect(filter, &QPushButton::clicked, [&] { ++clicks; });
    filter->click();
    CHECK(filter->isChecked() && clicks == 1);
    CHECK(filter->sizeHint().width() == narrow + 18);
    filter->resize(filter->sizeHint());
    const QImage chosen = seen(filter);
    CHECK(is(chosen.pixelColor(filter->width() / 2, 10), Role::SecondaryContainer));
    CHECK(is(chosen.pixelColor(filter->width() / 2, 8), Role::SecondaryContainer));
    int marked = 0;
    for (int y = 15; y < 33; ++y)
        for (int x = 8; x < 26; ++x)
            marked += near(chosen.pixelColor(x, y), style->color(Role::OnSecondaryContainer), 40) ? 1 : 0;
    CHECK(marked > 10);
    filter->click();
    CHECK(filter->sizeHint().width() == narrow);

    // An input chip has a cross after its label; its click is the program's, which takes the chip away.
    const int name = input->fontMetrics().horizontalAdvance(input->text());
    CHECK(input->sizeHint().width() == 16 + name + 34);
    const QImage crossed = seen(input);
    int ink = 0;
    for (int y = 18; y < 30; ++y)
        for (int x = input->width() - 26; x < input->width() - 8; ++x)
            ink += is(crossed.pixelColor(x, y), Role::Surface) ? 0 : 1;
    CHECK(ink > 8);

    // A property that is no kind of chip leaves a button as it was.
    QPushButton *other = chip("round", QStringLiteral("Other"), 150);
    CHECK(other->sizeHint().height() == 40);

    // Cards: a QFrame with the property, of any shape.
    const auto card = [&](const char *kind, int y) {
        auto *frame = new QFrame(&window);
        frame->setProperty("materialCard", QLatin1String(kind));
        frame->setGeometry(10, y, 200, 50);
        frame->show();
        return frame;
    };
    card("filled", 200);
    card("elevated", 260);
    card("outlined", 320);
    QCoreApplication::processEvents();
    const QImage cards = window.grab().toImage();
    CHECK(is(cards.pixelColor(100, 225), Role::SurfaceContainerHighest));
    CHECK(is(cards.pixelColor(10, 200), Role::Surface)); // round corners
    CHECK(is(cards.pixelColor(100, 285), Role::SurfaceContainerLow));
    CHECK(!is(cards.pixelColor(100, 308), Role::SurfaceContainerLow)); // its shadow
    CHECK(!is(cards.pixelColor(100, 308), Role::Surface));
    CHECK(is(cards.pixelColor(100, 345), Role::Surface));
    CHECK(is(cards.pixelColor(100, 320), Role::OutlineVariant));
    CHECK(is(cards.pixelColor(10, 345), Role::OutlineVariant));

    // A line is a divider, one pixel of the outline's quiet color, sunken or plain.
    for (QFrame::Shadow shadow : {QFrame::Sunken, QFrame::Plain}) {
        QFrame line(&window);
        line.setFrameShape(QFrame::HLine);
        line.setFrameShadow(shadow);
        line.setGeometry(220, 200, 100, 9);
        line.show();
        QFrame upright(&window);
        upright.setFrameShape(QFrame::VLine);
        upright.setFrameShadow(shadow);
        upright.setGeometry(340, 200, 9, 100);
        upright.show();
        QCoreApplication::processEvents();
        const QImage lines = window.grab().toImage();
        int rows = 0, columns = 0;
        for (int y = 200; y < 209; ++y)
            rows += is(lines.pixelColor(270, y), Role::Surface) ? 0 : 1;
        for (int x = 340; x < 349; ++x)
            columns += is(lines.pixelColor(x, 250), Role::Surface) ? 0 : 1;
        CHECK(rows == 1 && columns == 1);
        CHECK(is(lines.pixelColor(270, 204), Role::OutlineVariant));
        CHECK(is(lines.pixelColor(344, 250), Role::OutlineVariant));
    }
    // In a layout, a line has the height that it asks for.
    {
        QWidget column;
        auto *layout = new QVBoxLayout(&column);
        auto *line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        layout->addWidget(line);
        layout->addStretch(1);
        column.resize(100, 100);
        column.show();
        QCoreApplication::processEvents();
        CHECK(line->height() == 3);
    }

    // Under another style a card is the frame that it is.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(window.grab().toImage().pixelColor(100, 225) == window.grab().toImage().pixelColor(5, 225));
}

void testElevatedChipsAvatarsClickableCards() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(360, 420);
    window.show();
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    // As the window shows a widget: a grab of its own has nothing behind it.
    const auto seen = [&](const QWidget *widget) { return window.grab().toImage().copy(widget->geometry()); };
    // The panel of a button drawn on the window's color, with the pointer over it or not.
    const auto panel = [&](QPushButton *button, bool hovered) {
        QImage image(button->size(), QImage::Format_ARGB32);
        image.fill(style->color(Role::Surface));
        QPainter painter(&image);
        QStyleOptionButton option;
        option.initFrom(button);
        option.state &= ~QStyle::State(QStyle::State_MouseOver | QStyle::State_HasFocus);
        if (hovered)
            option.state |= QStyle::State_MouseOver;
        style->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, &painter, button);
        painter.end();
        return image;
    };
    const auto chip = [&](const char *kind, const QString &text, int y) {
        auto *button = new QPushButton(text, &window);
        button->setProperty("materialChip", QLatin1String(kind));
        button->move(10, y);
        button->resize(button->sizeHint());
        button->show();
        return button;
    };

    // An elevated chip has no outline: its container is raised over a shadow, and it is as wide as an outlined one.
    QPushButton *assist = chip("assist", QStringLiteral("Directions"), 0);
    const QSize outlined = assist->sizeHint();
    assist->setProperty("materialChipElevated", true);
    CHECK(assist->sizeHint() == outlined);
    QCoreApplication::processEvents();
    const QImage raised = seen(assist);
    CHECK(is(raised.pixelColor(assist->width() / 2, 9), Role::SurfaceContainerLow));
    CHECK(is(raised.pixelColor(4, 24), Role::SurfaceContainerLow));
    CHECK(is(raised.pixelColor(0, 24), Role::SurfaceContainerLow));        // where the outline was
    CHECK(!is(raised.pixelColor(assist->width() / 2, 40), Role::Surface)); // the shadow
    CHECK(is(raised.pixelColor(assist->width() / 2, 43), Role::Surface));
    // Under the pointer it is at level 2, and its shadow falls further.
    CHECK(is(panel(assist, false).pixelColor(assist->width() / 2, 42), Role::Surface));
    CHECK(!is(panel(assist, true).pixelColor(assist->width() / 2, 42), Role::Surface));
    // A chosen filter chip keeps its height, and is filled.
    QPushButton *filter = chip("filter", QStringLiteral("Unread"), 50);
    filter->setCheckable(true);
    filter->setProperty("materialChipElevated", true);
    filter->click();
    filter->resize(filter->sizeHint());
    QCoreApplication::processEvents();
    CHECK(is(seen(filter).pixelColor(filter->width() / 2, 9), Role::SecondaryContainer));
    CHECK(!is(seen(filter).pixelColor(filter->width() / 2, 40), Role::Surface));
    // An input chip has no elevated kind.
    QPushButton *input = chip("input", QStringLiteral("Hazel"), 100);
    input->setProperty("materialChipElevated", true);
    QCoreApplication::processEvents();
    CHECK(is(seen(input).pixelColor(input->width() / 2, 8), Role::OutlineVariant));

    // With the property, the icon of an input chip is an avatar: a round picture of 24, 4 from the start, in its own
    // colors. Without it an icon stays an icon of 18.
    const int name = input->fontMetrics().horizontalAdvance(input->text());
    QPixmap picture(24, 24);
    picture.fill(QColor(200, 30, 30));
    input->setIcon(QIcon(picture));
    CHECK(input->sizeHint().width() == 34 + name + 34);
    input->setProperty("materialChipAvatar", true);
    CHECK(input->sizeHint().width() == 36 + name + 34);
    input->resize(input->sizeHint());
    QCoreApplication::processEvents();
    const QImage avatar = seen(input);
    CHECK(near(avatar.pixelColor(16, 24), QColor(200, 30, 30), 3));
    CHECK(near(avatar.pixelColor(6, 24), QColor(200, 30, 30), 3));
    CHECK(near(avatar.pixelColor(26, 24), QColor(200, 30, 30), 3));
    CHECK(!near(avatar.pixelColor(5, 13), QColor(200, 30, 30), 60)); // round, not square
    CHECK(!near(avatar.pixelColor(27, 35), QColor(200, 30, 30), 60));
    // On another chip the property does nothing.
    assist->setIcon(QIcon(picture));
    const int plainWidth = assist->sizeHint().width();
    assist->setProperty("materialChipAvatar", true);
    CHECK(assist->sizeHint().width() == plainWidth);

    // A push button with materialCard is a card that can be clicked.
    const auto card = [&](const char *kind, int y) {
        auto *button = new QPushButton(&window);
        button->setProperty("materialCard", QLatin1String(kind));
        button->setGeometry(10, y, 200, 60);
        button->show();
        return button;
    };
    QPushButton *filled = card("filled", 160);
    QPushButton *elevated = card("elevated", 230);
    QPushButton *bordered = card("outlined", 300);
    QCoreApplication::processEvents();
    const QImage cards = window.grab().toImage();
    CHECK(is(cards.pixelColor(110, 190), Role::SurfaceContainerHighest));
    CHECK(is(cards.pixelColor(110, 160), Role::SurfaceContainerHighest));
    CHECK(is(cards.pixelColor(10, 160), Role::Surface)); // round corners
    CHECK(is(cards.pixelColor(110, 260), Role::SurfaceContainerLow));
    CHECK(!is(cards.pixelColor(110, 288), Role::SurfaceContainerLow)); // its shadow
    CHECK(!is(cards.pixelColor(110, 288), Role::Surface));
    CHECK(is(cards.pixelColor(110, 330), Role::Surface));
    CHECK(is(cards.pixelColor(110, 300), Role::OutlineVariant));
    CHECK(is(cards.pixelColor(10, 330), Role::OutlineVariant));
    // A layer of the content's color shows the pointer, and the press.
    const auto layered = [&](Role base, double opacity) {
        const QColor under = style->color(base);
        const QColor ink = style->color(Role::OnSurface);
        const auto mix = [&](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * opacity)); };
        return QColor(mix(under.red(), ink.red()), mix(under.green(), ink.green()), mix(under.blue(), ink.blue()));
    };
    CHECK(near(panel(filled, false).pixelColor(100, 30), style->color(Role::SurfaceContainerHighest), 2));
    CHECK(near(panel(filled, true).pixelColor(100, 30), layered(Role::SurfaceContainerHighest, 0.08), 3));
    CHECK(is(panel(elevated, false).pixelColor(100, 59), Role::Surface));
    CHECK(!is(panel(elevated, true).pixelColor(100, 59), Role::Surface)); // level 2
    filled->setDown(true);
    CHECK(near(seen(filled).pixelColor(100, 30), layered(Role::SurfaceContainerHighest, 0.10), 3));
    filled->setDown(false);
    int clicks = 0;
    QObject::connect(bordered, &QPushButton::clicked, [&] { ++clicks; });
    bordered->click();
    CHECK(clicks == 1);
    // With motion, a press ripples over the card from where it was.
    style->setMotionEnabled(true);
    const auto send = [&](QEvent::Type type, const QPointF &at) {
        QMouseEvent event(type, at, at, Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(filled, &event);
    };
    send(QEvent::MouseButtonPress, QPointF(20, 30));
    wait(600);
    CHECK(near(seen(filled).pixelColor(180, 30), layered(Role::SurfaceContainerHighest, 0.10), 4));
    send(QEvent::MouseButtonRelease, QPointF(20, 30));
    style->setMotionEnabled(false);
    // Its contents are those of its layout, whose size it asks for.
    auto *contents = new QVBoxLayout(elevated);
    auto *title = new QLabel(QStringLiteral("Title"));
    title->setFixedSize(240, 90);
    contents->addWidget(title);
    CHECK(elevated->sizeHint().width() >= 240 && elevated->sizeHint().height() >= 90);
    // Other buttons, and a card under another style, are left as they were.
    QPushButton other(QStringLiteral("Other"), &window);
    CHECK(other.sizeHint().height() == 40);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testCalendar() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(400, 420);
    auto *calendar = new QCalendarWidget(&window);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    window.show();
    QCoreApplication::processEvents();
    // Days of 40, under a bar with the arrows as icon buttons.
    const QList<QToolButton *> arrows = calendar->findChild<QLabel *>()->parentWidget()->findChildren<QToolButton *>();
    CHECK(arrows.size() == 2);
    if (arrows.size() != 2)
        return;
    CHECK(arrows.at(0)->sizeHint() == QSize(48, 48));
    const int bar = arrows.at(0)->parentWidget()->sizeHint().height();
    CHECK(calendar->sizeHint() == QSize(7 * 40, bar + 7 * 40));
    calendar->resize(calendar->sizeHint());
    calendar->setSelectedDate(QDate(2020, 6, 17));
    QCoreApplication::processEvents();
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    // The middle of the cell of a day, of weeks that start on Monday; June 2020 starts on one, so that a whole week of
    // May comes first.
    const auto cell = [&](int day) {
        const int at = 7 + day - 1;
        return QPoint((at % 7) * 40 + 20, bar + (at / 7 + 1) * 40 + 20);
    };
    const auto pixel = [&](const QPoint &point) { return calendar->grab().toImage().pixelColor(point.x(), point.y()); };
    CHECK(is(pixel(QPoint(2, bar + 2)), Role::SurfaceContainerHigh));
    // The chosen day is in a circle of the primary color, and no other is.
    CHECK(is(pixel(cell(17) + QPoint(0, -15)), Role::Primary));
    CHECK(is(pixel(cell(17) + QPoint(-15, 0)), Role::Primary));
    CHECK(is(pixel(cell(17) + QPoint(-18, -18)), Role::SurfaceContainerHigh));
    CHECK(is(pixel(cell(18) + QPoint(0, -15)), Role::SurfaceContainerHigh));
    // A click chooses a day, as under any style.
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(cell(18)), QPointF(cell(18)), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(calendar, &press);
    CHECK(calendar->selectedDate() == QDate(2020, 6, 18));
    CHECK(is(pixel(cell(18) + QPoint(0, -15)), Role::Primary));
    CHECK(is(pixel(cell(17) + QPoint(0, -15)), Role::SurfaceContainerHigh));
    arrows.at(1)->click();
    CHECK(calendar->monthShown() == 7);
    arrows.at(0)->click();
    CHECK(calendar->monthShown() == 6);
    // Today has a ring around nothing.
    const QDate today = QDate::currentDate();
    calendar->setSelectedDate(QDate(today.year(), today.month(), today.day() == 1 ? 2 : 1));
    const QDate first(today.year(), today.month(), 1);
    const int back = first.dayOfWeek() == 1 ? 7 : first.dayOfWeek() - 1;
    const int at = back + today.day() - 1;
    const QPoint ringed((at % 7) * 40 + 20, bar + (at / 7 + 1) * 40 + 20);
    CHECK(near(pixel(ringed + QPoint(0, -20)), style->color(Role::Primary), 60));
    CHECK(is(pixel(ringed + QPoint(0, -15)), Role::SurfaceContainerHigh));
    // With the week numbers it is a column wider.
    calendar->setVerticalHeaderFormat(QCalendarWidget::ISOWeekNumbers);
    CHECK(calendar->sizeHint().width() == 8 * 40);

    // Under another style it is the calendar that it was.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(calendar->palette().color(QPalette::Base) == QApplication::palette().color(QPalette::Base));
    CHECK(arrows.at(0)->sizeHint().width() < 48);
}

void testDialogOrderAndFab() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    // What confirms comes last; what dismisses before it.
    QDialogButtonBox box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help);
    box.resize(400, 48);
    box.show();
    QCoreApplication::processEvents();
    const auto at = [&](QDialogButtonBox::StandardButton which) { return box.button(which)->x(); };
    CHECK(at(QDialogButtonBox::Help) < at(QDialogButtonBox::Cancel));
    CHECK(at(QDialogButtonBox::Cancel) < at(QDialogButtonBox::Ok));
    QDialogButtonBox question(QDialogButtonBox::Yes | QDialogButtonBox::No);
    question.resize(300, 48);
    question.show();
    QCoreApplication::processEvents();
    CHECK(question.button(QDialogButtonBox::No)->x() < question.button(QDialogButtonBox::Yes)->x());
    // The order is the style's: another one, set while the box is shown, puts them back.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(at(QDialogButtonBox::Ok) < at(QDialogButtonBox::Cancel));
    box.hide();
    question.hide();

    style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    // A snackbar goes above a floating action button at the bottom, and back down when the button goes.
    QMainWindow window;
    auto *pages = new QStackedWidget;
    auto *with = new QWidget;
    auto *column = new QVBoxLayout(with);
    column->setContentsMargins(16, 16, 16, 16);
    column->addStretch(1);
    auto *fab = new QPushButton(QStringLiteral("+"));
    fab->setProperty("materialButton", QStringLiteral("fab"));
    column->addWidget(fab, 0, Qt::AlignRight);
    pages->addWidget(with);
    pages->addWidget(new QWidget);
    window.setCentralWidget(pages);
    auto *status = new QStatusBar;
    status->setProperty("materialStatusBar", QStringLiteral("snackbar"));
    window.setStatusBar(status);
    window.resize(360, 500);
    window.show();
    QCoreApplication::processEvents();
    status->showMessage(QStringLiteral("Saved"));
    QCoreApplication::processEvents();
    const int top = fab->mapTo(&window, QPoint(0, 0)).y();
    CHECK(status->y() + status->height() == top - 16);
    const int bottom = 500 - 16;
    pages->setCurrentIndex(1);
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    CHECK(status->y() + status->height() == bottom);
    pages->setCurrentIndex(0);
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    CHECK(status->y() + status->height() == top - 16);
    // One that is not at the bottom is no concern of the snackbar's.
    column->removeWidget(fab);
    fab->move(250, 40);
    fab->hide();
    fab->show();
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    CHECK(status->y() + status->height() == bottom);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testRailAndBadges() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    QPixmap square(24, 24);
    square.fill(Qt::black);
    const QIcon icon(square);

    QMainWindow window;
    auto *pages = new QTabWidget;
    pages->setTabPosition(QTabWidget::West);
    QTabBar *bar = pages->tabBar();
    bar->setProperty("materialTabBar", QStringLiteral("navigationRail"));
    for (const char *name : {"Home", "Search", "Settings"})
        pages->addTab(new QWidget, icon, QString::fromLatin1(name));
    window.setCentralWidget(pages);
    auto *status = new QStatusBar;
    status->setProperty("materialStatusBar", QStringLiteral("snackbar"));
    window.setStatusBar(status);
    window.resize(600, 400);
    window.show();
    QCoreApplication::processEvents();

    // 96 across, with the destinations of 64 one under another from 44 down.
    CHECK(bar->width() == 96 && bar->height() == 400);
    CHECK(bar->tabRect(0) == QRect(0, 44, 96, 64));
    CHECK(bar->tabRect(1) == QRect(0, 112, 96, 64));
    CHECK(bar->tabRect(2) == QRect(0, 180, 96, 64));
    CHECK(bar->tabAt(QPoint(48, 140)) == 1);
    CHECK(bar->font().pixelSize() == 12);
    const auto shot = [&] { return window.grab().toImage(); };
    QImage image = shot();
    const QPoint origin = bar->mapTo(&window, QPoint(0, 0));
    const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
    CHECK(is(at(2, 20), Role::Surface));
    CHECK(is(at(24, 44 + 22), Role::SecondaryContainer)); // the pill of the one shown
    CHECK(is(at(24, 112 + 22), Role::Surface));
    CHECK(is(at(48, 44 + 22), Role::OnSecondaryContainer)); // its icon
    CHECK(is(at(48, 112 + 22), Role::OnSurfaceVariant));
    // A click goes to a destination.
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(48, 140), QPointF(48, 140), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(bar, &press);
    CHECK(pages->currentIndex() == 1);
    // A rail is at the side: a snackbar stays at the bottom.
    status->showMessage(QStringLiteral("Saved"));
    QCoreApplication::processEvents();
    CHECK(status->y() + status->height() == 400 - 16);
    CHECK(status->x() == 96 + 16 && status->width() == 600 - 96 - 32); // and beside the rail
    pages->setTabPosition(QTabWidget::East);
    QCoreApplication::processEvents();
    status->showMessage(QStringLiteral("Saved again"));
    QCoreApplication::processEvents();
    CHECK(status->x() == 16 && status->width() == 600 - 96 - 32);
    pages->setTabPosition(QTabWidget::West);
    status->clearMessage();

    // Badges of destinations: a dot, a count, none.
    bar->setProperty("materialBadges", QVariantList{true, 3, 0});
    QCoreApplication::processEvents();
    image = shot();
    // The icon of destination i is 24 wide in the middle, 10 under the top of its destination.
    const auto red = [&](const QRect &area) {
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                count += near(at(x, y), style->color(Role::Error), 6) ? 1 : 0;
        return count;
    };
    const QRect first(36, 44 + 10, 24, 24), second = first.translated(0, 68), third = second.translated(0, 68);
    CHECK(is(at(first.right() - 2, first.top() + 3), Role::Error));
    const int dot = red(first.adjusted(0, -8, 24, 0));
    CHECK(dot > 12 && dot < 40);
    CHECK(red(second.adjusted(0, -8, 24, 0)) > dot * 2);
    CHECK(red(third.adjusted(0, -8, 24, 0)) == 0);

    // Badges of an icon button and of a button of an app bar.
    QWidget panel;
    panel.resize(200, 60);
    auto *button = new QPushButton(&panel);
    button->setProperty("materialIconButton", QStringLiteral("standard"));
    button->setIcon(icon);
    button->setGeometry(10, 6, 48, 48);
    auto *tool = new QToolButton(&panel);
    tool->setProperty("materialIconButton", QStringLiteral("standard"));
    tool->setIcon(icon);
    tool->setGeometry(100, 6, 48, 48);
    panel.show();
    QCoreApplication::processEvents();
    const auto redIn = [&](const QRect &area) {
        const QImage now = panel.grab().toImage();
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                count += near(now.pixelColor(x, y), style->color(Role::Error), 6) ? 1 : 0;
        return count;
    };
    for (QAbstractButton *each : {static_cast<QAbstractButton *>(button), static_cast<QAbstractButton *>(tool)}) {
        const QRect around = each->geometry();
        CHECK(redIn(around) == 0);
        each->setProperty("materialBadge", true);
        const int small = redIn(around);
        CHECK(small > 12 && small < 40);
        each->setProperty("materialBadge", 7);
        const int one = redIn(around);
        CHECK(one > 2 * small);
        each->setProperty("materialBadge", 12345); // 999+, which is wider
        CHECK(!textIsDrawn() || redIn(around.adjusted(0, 0, 20, 0)) > one);
        for (const QVariant &gone : {QVariant(0), QVariant(false), QVariant(QString()), QVariant()}) {
            each->setProperty("materialBadge", gone);
            CHECK(redIn(around) == 0);
        }
    }
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testSheetsAndDrawer() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.setGeometry(100, 50, 400, 700);
    window.show();
    QCoreApplication::processEvents();

    // A bottom sheet: against the bottom of the window, as high as what is in it.
    QDialog sheet(&window);
    sheet.setProperty("materialSheet", QStringLiteral("bottom"));
    auto *column = new QVBoxLayout(&sheet);
    auto *content = new QWidget;
    content->setFixedHeight(180);
    column->addWidget(content);
    sheet.show();
    QCoreApplication::processEvents();
    const int high = sheet.sizeHint().height();
    CHECK(sheet.geometry() == QRect(100, 50 + 700 - high, 400, high));
    CHECK(sheet.windowFlags() & Qt::FramelessWindowHint);
    CHECK(sheet.palette().color(QPalette::Window) == style->color(Role::SurfaceContainerLow));
    CHECK(sheet.windowHandle() && sheet.windowHandle()->property("_nt_sheet").toString() == QLatin1String("bottom"));
    CHECK(sheet.windowHandle()->property("_nt_cornerRadius").toInt() == 28);
    // It goes to the bottom again when it is shown again, wherever the window is by then, and Escape dismisses it.
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&sheet, &escape);
    CHECK(!sheet.isVisible() && sheet.result() == QDialog::Rejected);
    window.setGeometry(0, 0, 900, 800);
    sheet.show();
    QCoreApplication::processEvents();
    // A wide window has a sheet of 640 in its middle.
    CHECK(sheet.geometry() == QRect(130, 800 - high, 640, high));
    sheet.hide();
    // One that would like more than the window holds leaves room above it.
    struct Tall : QWidget {
        QSize sizeHint() const override { return QSize(100, 2000); }
    };
    delete content;
    column->addWidget(new Tall);
    sheet.show();
    QCoreApplication::processEvents();
    CHECK(sheet.height() == 800 - 72);
    sheet.hide();

    // A drawer, with its list.
    window.setGeometry(100, 50, 400, 700);
    QDialog drawer(&window);
    drawer.setProperty("materialSheet", QStringLiteral("drawer"));
    auto *inside = new QVBoxLayout(&drawer);
    inside->setContentsMargins(0, 12, 0, 12);
    auto *list = new QListWidget;
    list->setProperty("materialList", QStringLiteral("navigationDrawer"));
    list->addItems({QStringLiteral("Inbox"), QStringLiteral("Sent"), QStringLiteral("Archive")});
    list->setCurrentRow(0);
    inside->addWidget(list);
    drawer.show();
    QCoreApplication::processEvents();
    CHECK(drawer.geometry() == QRect(100, 50, 400 - 56, 700));
    CHECK(drawer.windowHandle()->property("_nt_cornerRadius").toInt() == 16);
    CHECK(list->font().pixelSize() == 14);
    const QImage image = drawer.grab().toImage();
    const QPoint origin = list->viewport()->mapTo(&drawer, QPoint(0, 0));
    const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
    const int wide = list->viewport()->width();
    CHECK(wide == drawer.width());                          // no frame takes room from it
    CHECK(is(at(wide - 40, 28), Role::SecondaryContainer)); // the pill of the current row,
    CHECK(is(at(4, 28), Role::SurfaceContainerLow));        // in from the sides,
    CHECK(is(at(13, 3), Role::SurfaceContainerLow));        // with round ends
    CHECK(is(at(wide - 40, 84), Role::SurfaceContainerLow));
    // The icons of rows are in the colors of the list, in a drawer and in any other list.
    QPixmap black(24, 24);
    black.fill(Qt::black);
    list->setIconSize(QSize(24, 24));
    for (int i = 0; i < list->count(); ++i)
        list->item(i)->setIcon(QIcon(black));
    QCoreApplication::processEvents();
    const QImage iconed = drawer.grab().toImage();
    const auto in = [&](int x, int y) { return iconed.pixelColor(origin.x() + x, origin.y() + y); };
    CHECK(is(in(28 + 12, 28), Role::OnSecondaryContainer));
    CHECK(is(in(28 + 12, 84), Role::OnSurfaceVariant));
    {
        QStandardItemModel model;
        model.appendRow(new QStandardItem(QIcon(black), QStringLiteral("One")));
        QListView view;
        view.setIconSize(QSize(24, 24));
        view.setModel(&model);
        view.resize(200, 80);
        view.show();
        QCoreApplication::processEvents();
        const QImage rows = view.viewport()->grab().toImage();
        int inked = 0, black_pixels = 0;
        for (int y = 0; y < 56; ++y)
            for (int x = 0; x < 60; ++x) {
                inked += is(rows.pixelColor(x, y), Role::OnSurfaceVariant) ? 1 : 0;
                black_pixels += rows.pixelColor(x, y) == QColor(Qt::black) ? 1 : 0;
            }
        CHECK(inked > 200 && black_pixels == 0);
    }
    drawer.hide();
    window.setGeometry(0, 0, 800, 600);
    drawer.show();
    QCoreApplication::processEvents();
    CHECK(drawer.width() == 360);
    drawer.hide();
    // Another style gives the list its frame back.
    QApplication::setStyle(QStringLiteral("Fusion"));
    CHECK(list->frameShape() == QFrame::StyledPanel);
    style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    CHECK(list->frameShape() == QFrame::NoFrame);

    // A dialog that is no sheet is a dialog as before.
    QDialog plain(&window);
    plain.resize(200, 100);
    plain.show();
    QCoreApplication::processEvents();
    CHECK(!(plain.windowFlags() & Qt::FramelessWindowHint));
    CHECK(plain.palette().color(QPalette::Window) == style->color(Role::SurfaceContainerHigh));
    CHECK(plain.windowHandle()->property("_nt_cornerRadius").toInt() == 28);
    plain.hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A bottom sheet has a drag handle, and a sheet is dragged toward its edge: let go far enough it closes, and otherwise
// it springs back. A press beside a modal sheet, in the window that it blocks, closes it too.
void testSheetDragging() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const char *const scrim = "_nt_scrimOpacity";
    const auto mouse = [](QWidget *target, QEvent::Type type, const QPoint &global) {
        const Qt::MouseButtons held = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
        QMouseEvent event(type, QPointF(target->mapFromGlobal(global)), QPointF(global), Qt::LeftButton, held,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(target, &event);
        return event.isAccepted();
    };

    QWidget window;
    window.setGeometry(100, 50, 400, 700);
    window.show();
    QCoreApplication::processEvents();
    QDialog sheet(&window);
    sheet.setProperty("materialSheet", QStringLiteral("bottom"));
    auto *column = new QVBoxLayout(&sheet);
    auto *content = new QWidget;
    content->setFixedHeight(180);
    column->addWidget(content);
    auto *button = new QPushButton(QStringLiteral("Share"));
    column->addWidget(button);
    sheet.show();
    QCoreApplication::processEvents();

    // The handle, 32 by 4 and 22 from the top in the middle, in a target of 48 above what is in the sheet.
    CHECK(content->mapTo(&sheet, QPoint(0, 0)).y() == 48);
    const QRect rest = sheet.geometry();
    const int high = rest.height();
    CHECK(rest == QRect(100, 50 + 700 - high, 400, high));
    {
        const QImage image = sheet.grab().toImage();
        const QColor surface = style->color(Role::SurfaceContainerLow);
        const QColor handle = style->color(Role::OnSurfaceVariant);
        const QColor mixed =
            QColor::fromRgbF(surface.redF() * 0.6 + handle.redF() * 0.4, surface.greenF() * 0.6 + handle.greenF() * 0.4,
                             surface.blueF() * 0.6 + handle.blueF() * 0.4);
        CHECK(near(image.pixelColor(200, 23), mixed, 4));
        CHECK(near(image.pixelColor(200 - 14, 24), mixed, 4));
        CHECK(near(image.pixelColor(200 - 18, 23), surface, 2)); // 16 to each side of the middle
        CHECK(near(image.pixelColor(200, 20), surface, 2));      // 22 from the top
        CHECK(near(image.pixelColor(200, 27), surface, 2));      // 4 high
    }

    // Dragged down by a little, it follows, its scrim fading with the distance, and springs back when let go.
    const QPoint grip = sheet.mapToGlobal(QPoint(200, 20));
    CHECK(mouse(&sheet, QEvent::MouseButtonPress, grip));
    mouse(&sheet, QEvent::MouseMove, grip + QPoint(0, 30));
    CHECK(sheet.geometry() == rest.translated(0, 30));
    CHECK(sheet.windowHandle()->property("_nt_sheetOffset").toInt() == 30);
    CHECK(near(sheet.windowHandle()->property(scrim).toDouble(), 0.32 * (1.0 - 30.0 / high), 1e-9));
    mouse(&sheet, QEvent::MouseMove, grip + QPoint(0, -40)); // not above its place
    CHECK(sheet.geometry() == rest);
    mouse(&sheet, QEvent::MouseMove, grip + QPoint(0, high / 4));
    CHECK(sheet.y() == rest.y() + high / 4);
    mouse(&sheet, QEvent::MouseButtonRelease, grip + QPoint(0, high / 4));
    CHECK(sheet.isVisible() && sheet.geometry() == rest);
    CHECK(sheet.windowHandle()->property("_nt_sheetOffset").toInt() == 0);
    CHECK(near(sheet.windowHandle()->property(scrim).toDouble(), 0.32, 1e-9));

    // A child that takes the mouse keeps its press, and the sheet does not move.
    const QPoint onButton = button->mapToGlobal(button->rect().center());
    mouse(button, QEvent::MouseButtonPress, onButton);
    mouse(button, QEvent::MouseMove, onButton + QPoint(0, high / 2));
    CHECK(sheet.geometry() == rest);
    mouse(button, QEvent::MouseButtonRelease, onButton + QPoint(0, high / 2));
    CHECK(sheet.isVisible());

    // Past a third of its height, it closes as Back does.
    int rejected = 0;
    QObject::connect(&sheet, &QDialog::rejected, [&rejected] { ++rejected; });
    mouse(&sheet, QEvent::MouseButtonPress, grip);
    mouse(&sheet, QEvent::MouseMove, grip + QPoint(0, high / 2));
    CHECK(sheet.y() == rest.y() + high / 2);
    mouse(&sheet, QEvent::MouseButtonRelease, grip + QPoint(0, high / 2));
    CHECK(!sheet.isVisible() && sheet.result() == QDialog::Rejected && rejected == 1);
    // Shown again, it is in its place with nothing left of the drag.
    sheet.show();
    QCoreApplication::processEvents();
    CHECK(sheet.geometry() == rest && sheet.windowHandle()->property("_nt_sheetOffset").toInt() == 0);
    sheet.hide();

    // Without its handle it makes no room for one, and is still dragged by its background.
    sheet.setProperty("materialSheetHandle", false);
    sheet.show();
    QCoreApplication::processEvents();
    CHECK(content->mapTo(&sheet, QPoint(0, 0)).y() == 24);
    CHECK(near(sheet.grab().toImage().pixelColor(sheet.width() / 2, 23), style->color(Role::SurfaceContainerLow), 2));
    sheet.hide();

    // A drawer goes to the left.
    QDialog drawer(&window);
    drawer.setProperty("materialSheet", QStringLiteral("drawer"));
    auto *inside = new QVBoxLayout(&drawer);
    inside->addWidget(new QLabel(QStringLiteral("Mail")));
    drawer.show();
    QCoreApplication::processEvents();
    const QRect place = drawer.geometry();
    const QPoint at = drawer.mapToGlobal(QPoint(place.width() / 2, place.height() - 10));
    mouse(&drawer, QEvent::MouseButtonPress, at);
    mouse(&drawer, QEvent::MouseMove, at - QPoint(60, 0));
    CHECK(drawer.geometry() == place.translated(-60, 0));
    CHECK(drawer.windowHandle()->property("_nt_sheetOffset").toInt() == 60);
    mouse(&drawer, QEvent::MouseButtonRelease, at - QPoint(60, 0));
    CHECK(drawer.isVisible() && drawer.geometry() == place);
    mouse(&drawer, QEvent::MouseButtonPress, at);
    mouse(&drawer, QEvent::MouseMove, at - QPoint(place.width() / 2, 0));
    mouse(&drawer, QEvent::MouseButtonRelease, at - QPoint(place.width() / 2, 0));
    CHECK(!drawer.isVisible() && drawer.result() == QDialog::Rejected);

    // A press in the window that a modal sheet blocks closes the sheet; one on the sheet does not.
    sheet.setModal(true);
    sheet.show();
    QCoreApplication::processEvents();
    CHECK(QApplication::activeModalWidget() == &sheet);
    const QPoint beside = window.mapToGlobal(QPoint(200, 100));
    QMouseEvent onSheet(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(sheet.mapToGlobal(QPoint(10, 10))),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(sheet.windowHandle(), &onSheet);
    CHECK(sheet.isVisible());
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(200, 100), QPointF(beside), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(window.windowHandle(), &press);
    CHECK(!sheet.isVisible() && sheet.result() == QDialog::Rejected);
    // Nor does one beside a dialog that is no sheet.
    QDialog plain(&window);
    plain.setModal(true);
    plain.resize(200, 100);
    plain.show();
    QCoreApplication::processEvents();
    QMouseEvent again(QEvent::MouseButtonPress, QPointF(200, 100), QPointF(beside), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(window.windowHandle(), &again);
    CHECK(plain.isVisible());
    plain.hide();
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testSearchAndSecondaryTabs() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.resize(360, 300);
    auto *search = new QLineEdit(&window);
    search->setProperty("materialTextField", QStringLiteral("search"));
    search->setPlaceholderText(QStringLiteral("Search notes"));
    search->setClearButtonEnabled(true);
    search->setGeometry(10, 10, 340, 56);
    window.show();
    QCoreApplication::processEvents();
    // 56 high, with no room over or under it for a label or a line of text, even if it is given them.
    CHECK(search->sizeHint().height() == 56);
    search->setProperty("materialLabel", QStringLiteral("Label"));
    search->setProperty("materialSupportingText", QStringLiteral("More"));
    CHECK(search->sizeHint().height() == 56);
    const auto shot = [&] { return window.grab().toImage().copy(search->geometry()); };
    QImage image = shot();
    CHECK(is(image.pixelColor(200, 6), Role::SurfaceContainerHigh)); // a pill with no outline,
    CHECK(is(image.pixelColor(200, 0), Role::SurfaceContainerHigh));
    CHECK(is(image.pixelColor(2, 2), Role::Surface)); // round at its ends
    CHECK(is(image.pixelColor(337, 53), Role::Surface));
    // with a magnifier before the text.
    const auto dark = [&](const QImage &in, const QRect &area) {
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                count += near(in.pixelColor(x, y), style->color(Role::OnSurface), 60) ? 1 : 0;
        return count;
    };
    CHECK(dark(image, QRect(16, 16, 24, 24)) > 20);
    CHECK(dark(image, QRect(42, 8, 12, 40)) == 0);
    // The text begins 56 in.
    search->setText(QStringLiteral("MMMM"));
    image = shot();
    if (textIsDrawn()) {
        CHECK(dark(image, QRect(56, 16, 40, 24)) > 20);
        CHECK(dark(image, QRect(42, 8, 13, 40)) == 0);
    }
    // A cross at the end, in the quiet color.
    image = shot();
    int crossed = 0;
    for (int y = 16; y < 40; ++y)
        for (int x = 340 - 40; x < 340 - 14; ++x)
            crossed += is(image.pixelColor(x, y), Role::OnSurfaceVariant) ? 1 : 0;
    CHECK(crossed > 20);
    // The cross at the end empties it.
    const QPointF cross(340 - 26, 28);
    QMouseEvent press(QEvent::MouseButtonPress, cross, cross, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(search, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, cross, cross, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(search, &release);
    CHECK(search->text().isEmpty());

    // Secondary tabs: a thin line as wide as the tab, where primary tabs have a rounded one as wide as the label.
    auto *tabs = new QTabBar(&window);
    tabs->setExpanding(true);
    for (const char *name : {"Videos", "Photos", "Audio"})
        tabs->addTab(QString::fromLatin1(name));
    tabs->setCurrentIndex(1);
    tabs->setGeometry(0, 100, 360, 48);
    tabs->show();
    QCoreApplication::processEvents();
    const auto bar = [&] { return window.grab().toImage().copy(tabs->geometry()); };
    const QRect current = tabs->tabRect(1);
    image = bar();
    CHECK(is(image.pixelColor(current.center().x(), 46), Role::Primary));
    CHECK(!is(image.pixelColor(current.left() + 2, 46), Role::Primary)); // primary: only as wide as the label
    CHECK(is(image.pixelColor(current.center().x(), 45), Role::Primary));
    tabs->setProperty("materialTabBar", QStringLiteral("secondary"));
    QCoreApplication::processEvents();
    image = bar();
    CHECK(is(image.pixelColor(current.left() + 1, 47), Role::Primary));
    CHECK(is(image.pixelColor(current.right() - 1, 46), Role::Primary));
    CHECK(!is(image.pixelColor(current.center().x(), 45), Role::Primary)); // 2 high
    CHECK(!is(image.pixelColor(current.left() - 3, 47), Role::Primary));
    if (textIsDrawn()) {
        int accented = 0, plain = 0;
        for (int y = 12; y < 36; ++y)
            for (int x = current.left(); x <= current.right(); ++x) {
                accented += is(image.pixelColor(x, y), Role::Primary) ? 1 : 0;
                plain += is(image.pixelColor(x, y), Role::OnSurface) ? 1 : 0;
            }
        CHECK(accented == 0 && plain > 4);
    }
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testTimeInput() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.resize(400, 240);
    auto *edit = new QTimeEdit(QTime(9, 41), &window);
    edit->setDisplayFormat(QStringLiteral("HH:mm"));
    const QSize plain = edit->sizeHint();
    edit->setProperty("materialTimeInput", true);
    window.show();
    window.activateWindow();
    QCoreApplication::processEvents();
    // A box of 96 by 72 for each of the hour and the minute, 24 apart, with their names under them.
    CHECK(edit->sizeHint() == QSize(216, 92));
    edit->setGeometry(10, 10, 216, 92);
    QCoreApplication::processEvents();
    auto *line = edit->findChild<QLineEdit *>();
    CHECK(line && line->width() == 0 && line->height() == 0);
    if (!line)
        return;
    CHECK(line->inputMethodHints() & Qt::ImhPreferNumbers); // the digits of an on-screen keyboard
    const auto shot = [&] { return window.grab().toImage().copy(edit->geometry()); };
    line->clearFocus();
    QImage image = shot();
    CHECK(is(image.pixelColor(10, 10), Role::SurfaceContainerHighest));
    CHECK(is(image.pixelColor(130, 10), Role::SurfaceContainerHighest));
    CHECK(is(image.pixelColor(108, 10), Role::Surface)); // between them

    const auto click = [&](int x, int y) {
        const QPointF at(x, y);
        QMouseEvent press(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(edit, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(edit, &release);
        QCoreApplication::processEvents();
    };
    // Keys go through the window, as from a keyboard.
    const auto key = [&](int code, const QString &text = QString()) {
        QWindowSystemInterface::handleKeyEvent(window.windowHandle(), QEvent::KeyPress, code, Qt::NoModifier, text);
        QWindowSystemInterface::handleKeyEvent(window.windowHandle(), QEvent::KeyRelease, code, Qt::NoModifier, text);
        QCoreApplication::processEvents();
    };
    // A click chooses a box, which is marked; what is typed replaces what is in it.
    click(40, 30);
    CHECK(QApplication::focusWidget() == line);
    CHECK(line->selectedText() == QLatin1String("09"));
    image = shot();
    CHECK(is(image.pixelColor(10, 10), Role::PrimaryContainer));
    CHECK(is(image.pixelColor(48, 1), Role::Primary));
    CHECK(is(image.pixelColor(130, 10), Role::SurfaceContainerHighest));
    key(Qt::Key_1, QStringLiteral("1"));
    CHECK(line->selectedText().isEmpty()); // one digit of the hour: it waits for the second
    key(Qt::Key_7, QStringLiteral("7"));
    CHECK(edit->time() == QTime(17, 41));
    // Two digits move on to the minute, which is then marked.
    CHECK(line->selectedText() == QLatin1String("41"));
    image = shot();
    CHECK(is(image.pixelColor(130, 10), Role::PrimaryContainer));
    CHECK(is(image.pixelColor(10, 10), Role::SurfaceContainerHighest));
    click(40, 30);
    key(Qt::Key_5, QStringLiteral("5")); // no hour 50: one digit is enough
    CHECK(edit->time() == QTime(5, 41) && line->text() == QLatin1String("05:41"));
    CHECK(line->selectedText() == QLatin1String("41"));
    click(40, 30);
    key(Qt::Key_1, QStringLiteral("1"));
    key(Qt::Key_7, QStringLiteral("7"));
    click(160, 30);
    CHECK(line->selectedText() == QLatin1String("41"));
    CHECK(is(shot().pixelColor(130, 10), Role::PrimaryContainer));
    key(Qt::Key_0, QStringLiteral("0"));
    key(Qt::Key_8, QStringLiteral("8"));
    CHECK(edit->time() == QTime(17, 8));
    key(Qt::Key_Up);
    CHECK(edit->time() == QTime(17, 9));
    key(Qt::Key_Down);
    key(Qt::Key_Down);
    CHECK(edit->time() == QTime(17, 7));
    // The digits are drawn anew as the time changes, also by the program.
    if (textIsDrawn()) {
        const QImage before = shot();
        edit->setTime(QTime(11, 7));
        QCoreApplication::processEvents();
        CHECK(shot() != before);
    }

    // With AM and PM there is a choice between them, in two halves.
    edit->setDisplayFormat(QStringLiteral("h:mm AP"));
    edit->setTime(QTime(21, 5));
    QCoreApplication::processEvents();
    CHECK(edit->sizeHint() == QSize(216 + 12 + 52, 92));
    CHECK(!(line->inputMethodHints() & Qt::ImhPreferNumbers)); // AM and PM are letters
    edit->setGeometry(10, 10, 280, 92);
    QCoreApplication::processEvents();
    image = shot();
    CHECK(is(image.pixelColor(236, 54), Role::TertiaryContainer));
    CHECK(is(image.pixelColor(236, 18), Role::Surface));
    click(254, 18);
    CHECK(edit->time() == QTime(9, 5));
    CHECK(is(shot().pixelColor(236, 18), Role::TertiaryContainer));
    click(254, 18);
    CHECK(edit->time() == QTime(9, 5));
    click(254, 54);
    CHECK(edit->time() == QTime(21, 5));
    edit->setReadOnly(true);
    click(254, 18);
    CHECK(edit->time() == QTime(21, 5));
    edit->setReadOnly(false);

    // A format with a date in it is no time input.
    QDateTimeEdit both;
    both.setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    both.setProperty("materialTimeInput", true);
    CHECK(both.sizeHint().height() < 72);
    // Without the property, and under another style, a time edit is the one that it was.
    edit->setDisplayFormat(QStringLiteral("HH:mm"));
    edit->setProperty("materialTimeInput", false);
    QCoreApplication::processEvents();
    CHECK(edit->sizeHint().height() == plain.height() && line->width() > 0);
    edit->setProperty("materialTimeInput", true);
    QCoreApplication::processEvents();
    CHECK(line->width() == 0);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(edit->sizeHint().height() < 72 && line->width() > 0);
}

void testSpinBoxAndScrollBar() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.resize(400, 400);
    auto *spin = new QSpinBox(&window);
    spin->setRange(0, 10);
    spin->setValue(5);
    auto *date = new QDateEdit(QDate(2020, 6, 17), &window);
    date->setCalendarPopup(true);
    window.show();
    window.activateWindow();
    QCoreApplication::processEvents();
    // A field of 56 with two buttons of 48, in the type of the other fields.
    CHECK(spin->sizeHint().height() == 56);
    CHECK(spin->font().pixelSize() == 16);
    spin->setGeometry(10, 10, 220, 56);
    date->setGeometry(10, 90, 220, 56);
    QCoreApplication::processEvents();
    auto *line = spin->findChild<QLineEdit *>();
    CHECK(line && line->geometry() == QRect(16, 3, 220 - 16 - 96, 50));
    if (!line)
        return;
    const auto click = [](QWidget *on, int x, int y) {
        const QPointF at(x, y);
        QMouseEvent press(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(on, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(on, &release);
        QCoreApplication::processEvents();
    };
    click(spin, 220 - 24, 28); // up, at the end
    CHECK(spin->value() == 6);
    click(spin, 220 - 72, 28); // down, before it
    click(spin, 220 - 72, 28);
    CHECK(spin->value() == 4);
    click(spin, 220 - 90, 6); // anywhere on a button
    CHECK(spin->value() == 3);
    line->clearFocus();
    const auto shot = [&](const QWidget *widget) { return window.grab().toImage().copy(widget->geometry()); };
    QImage image = shot(spin);
    CHECK(is(image.pixelColor(110, 0), Role::Outline));
    CHECK(is(image.pixelColor(110, 28), Role::Surface));               // the line edit has no box of its own,
    CHECK(is(image.pixelColor(220 - 24, 28), Role::OnSurfaceVariant)); // a plus at the end
    CHECK(is(image.pixelColor(220 - 24, 23), Role::OnSurfaceVariant));
    CHECK(is(image.pixelColor(220 - 72, 28), Role::OnSurfaceVariant)); // and a minus before it
    CHECK(is(image.pixelColor(220 - 72, 23), Role::Surface));
    line->setFocus();
    CHECK(is(shot(spin).pixelColor(110, 2), Role::Primary));
    // At the end of the range, the button that cannot step is faint.
    spin->setValue(10);
    CHECK(!is(shot(spin).pixelColor(220 - 24, 23), Role::OnSurfaceVariant));
    // Without buttons the text has the room; without a frame there is no outline.
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    QCoreApplication::processEvents();
    CHECK(line->geometry() == QRect(16, 3, 220 - 32, 50));
    CHECK(spin->sizeHint().width() < date->sizeHint().width() + 48);
    spin->setFrame(false);
    line->clearFocus();
    CHECK(is(shot(spin).pixelColor(110, 0), Role::Surface));

    // A date edit with a calendar has one button, which opens the calendar.
    auto *text = date->findChild<QLineEdit *>();
    CHECK(text && text->width() == 220 - 16 - 48);
    image = shot(date);
    CHECK(is(image.pixelColor(220 - 72, 28), Role::Surface));
    CHECK(window.findChildren<QCalendarWidget *>().isEmpty() ||
          !window.findChildren<QCalendarWidget *>().first()->isVisible());
    click(date, 220 - 24, 28);
    CHECK(date->calendarWidget() && date->calendarWidget()->isVisible());
    CHECK(date->date() == QDate(2020, 6, 17));
    if (date->calendarWidget())
        date->calendarWidget()->window()->hide();
    if (date->calendarWidget())
        date->calendarWidget()->hide();

    // A scroll bar is a thin handle with no track.
    QScrollBar bar(Qt::Vertical, &window);
    bar.setRange(0, 100);
    bar.setPageStep(100);
    CHECK(bar.sizeHint().width() == 10);
    bar.setGeometry(300, 10, 10, 200);
    bar.show();
    QCoreApplication::processEvents();
    image = shot(&bar);
    const auto drawn = [&](const QImage &in, int y) {
        int count = 0;
        for (int x = 0; x < 10; ++x)
            count += is(in.pixelColor(x, y), Role::Surface) ? 0 : 1;
        return count;
    };
    CHECK(drawn(image, 50) >= 3 && drawn(image, 50) <= 5); // the handle, in the upper half
    CHECK(drawn(image, 150) == 0);
    // It drags as any scroll bar does.
    const auto move = [&](QEvent::Type type, int y, Qt::MouseButtons buttons) {
        QMouseEvent event(type, QPointF(5, y), QPointF(5, y), Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(&bar, &event);
    };
    move(QEvent::MouseButtonPress, 50, Qt::LeftButton);
    move(QEvent::MouseMove, 100, Qt::LeftButton);
    CHECK(bar.value() == 50);
    CHECK(drawn(shot(&bar), 100) >= 7); // wider while it is held
    move(QEvent::MouseButtonRelease, 100, Qt::NoButton);
    // A long document leaves a handle that a finger finds.
    bar.setRange(0, 100000);
    bar.setValue(0);
    QCoreApplication::processEvents();
    image = shot(&bar);
    CHECK(drawn(image, 40) > 0 && drawn(image, 60) == 0);

    // Under another style they are what they were.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(spin->sizeHint().height() < 56 && bar.sizeHint().width() > 10);
}

void testDialogHeadline() {
    auto *platform = dynamic_cast<QOffscreenIntegration *>(QtPrivate::GuiAccess::integration());
    CHECK(platform != nullptr);
    if (!platform)
        return;
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    // How a dialog with a message is laid out, with the platform showing titles or not.
    struct Shown {
        int height = 0;
        int message = 0; // the top of what is first in it
        int ink = 0;     // pixels of the headline's color in the room over that
    };
    const auto show = [&](QDialog &dialog, const QWidget *first) {
        dialog.show();
        QCoreApplication::processEvents();
        Shown shown;
        shown.height = dialog.height();
        shown.message = first->mapTo(&dialog, QPoint(0, 0)).y();
        const QImage image = dialog.grab().toImage();
        for (int y = 24; y < 56 && y < image.height(); ++y)
            for (int x = 24; x < image.width() - 24; ++x)
                shown.ink += near(image.pixelColor(x, y), style->color(Role::OnSurface), 60) ? 1 : 0;
        return shown;
    };
    const auto make = [](QDialog &dialog, const QString &title) {
        dialog.setWindowTitle(title);
        auto *rows = new QVBoxLayout(&dialog);
        auto *first = new QWidget;
        first->setFixedSize(300, 40);
        rows->addWidget(first);
        return first;
    };
    platform->dialogTitles = true;
    QDialog titled;
    const Shown bar = show(titled, make(titled, QStringLiteral("About")));
    // Material's padding of 24 around what is in a dialog.
    CHECK(bar.message == 24 && bar.height == 88 && bar.ink == 0);

    platform->dialogTitles = false;
    QDialog headed;
    const QWidget *first = make(headed, QStringLiteral("About"));
    const Shown drawn = show(headed, first);
    // The headline is 24 down, and what is in the dialog 16 under it.
    QFont large = headed.font();
    large.setPixelSize(24);
    const int line = QFontMetrics(large).height();
    CHECK(drawn.message == 24 + line + 16);
    CHECK(drawn.height == bar.height + drawn.message - bar.message);
    CHECK(!textIsDrawn() || drawn.ink > 40);
    // Shown again, it is no higher for it.
    headed.hide();
    CHECK(show(headed, first).height == drawn.height);
    // A dialog without a title has no room made for one, and nor has a sheet, which has its drag handle there instead
    // (or nothing without it).
    headed.hide();
    headed.setWindowTitle(QString());
    const Shown without = show(headed, first);
    CHECK(without.message == 24 && without.height == bar.height);
    // And grows again with one.
    headed.hide();
    headed.setWindowTitle(QStringLiteral("About"));
    CHECK(show(headed, first).height == drawn.height);
    QDialog sheet;
    sheet.setProperty("materialSheet", QStringLiteral("bottom"));
    const QWidget *top = make(sheet, QStringLiteral("A sheet"));
    const Shown handled = show(sheet, top);
    CHECK(handled.message == 48 && handled.ink == 0);
    sheet.hide();
    sheet.setProperty("materialSheetHandle", false);
    CHECK(show(sheet, top).message == 24);
    sheet.hide();

    // A message box shows its title, with its text and button under it.
    QMessageBox box(QMessageBox::Information, QStringLiteral("About"), QStringLiteral("Nt"), QMessageBox::Ok);
    box.show();
    QCoreApplication::processEvents();
    const QList<QLabel *> labels = box.findChildren<QLabel *>();
    for (const QLabel *label : labels) {
        if (label->isVisible() && !label->text().isEmpty())
            CHECK(label->mapTo(&box, QPoint(0, 0)).y() >= 24 + line + 16);
    }
    const QList<QPushButton *> buttons = box.findChildren<QPushButton *>();
    CHECK(!buttons.isEmpty() &&
          box.rect().contains(QRect(buttons.first()->mapTo(&box, QPoint(0, 0)), buttons.first()->size())));
    box.hide();
    // Without an icon the text starts where the headline does; with one, beside it.
    const auto textLeft = [](QMessageBox &shown, const QString &text) {
        shown.show();
        QCoreApplication::processEvents();
        const QList<QLabel *> all = shown.findChildren<QLabel *>();
        for (const QLabel *label : all) {
            if (label->text() == text)
                return label->mapTo(&shown, QPoint(0, 0)).x();
        }
        return -1;
    };
    QMessageBox plain(QMessageBox::NoIcon, QStringLiteral("Delete"), QStringLiteral("Gone?"),
                      QMessageBox::Cancel | QMessageBox::Ok);
    CHECK(textLeft(plain, QStringLiteral("Gone?")) == 24);
    plain.hide();
    auto *never = new QCheckBox(QStringLiteral("Do not ask again"));
    plain.setCheckBox(never);
    textLeft(plain, QStringLiteral("Gone?"));
    CHECK(never->isVisible() && never->mapTo(&plain, QPoint(0, 0)).x() == 24);
    plain.hide();
    // With an icon the text starts there too: the icon is over the headline. Where titles are shown, it is beside.
    plain.setIcon(QMessageBox::Warning);
    CHECK(textLeft(plain, QStringLiteral("Gone?")) == 24);
    plain.hide();
    platform->dialogTitles = true;
    CHECK(textLeft(plain, QStringLiteral("Gone?")) > 24 + 30);
    plain.hide();
    platform->dialogTitles = false;
    plain.setIcon(QMessageBox::NoIcon);
    CHECK(textLeft(plain, QStringLiteral("Gone?")) == 24);
    plain.hide();
    // Another style takes the room away again.
    QDialog again;
    const QWidget *content = make(again, QStringLiteral("Again"));
    show(again, content);
    CHECK(again.contentsMargins().top() > 0);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(again.contentsMargins().top() == 0);
    platform->dialogTitles = true;
}

// A headline too long for the dialog wraps, and an icon of the dialog's own puts it in the centered form.
void testDialogHeadlineForms() {
    auto *platform = dynamic_cast<QOffscreenIntegration *>(QtPrivate::GuiAccess::integration());
    CHECK(platform != nullptr);
    if (!platform)
        return;
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    platform->dialogTitles = false;

    struct Dialog {
        QDialog dialog;
        QWidget *first = nullptr;
        explicit Dialog(const QString &title) {
            dialog.setWindowTitle(title);
            auto *rows = new QVBoxLayout(&dialog);
            first = new QWidget;
            first->setFixedSize(300, 40);
            rows->addWidget(first);
        }
        int show() {
            dialog.show();
            QCoreApplication::processEvents();
            return first->mapTo(&dialog, QPoint(0, 0)).y();
        }
    };
    QFont large = QApplication::font();
    large.setPixelSize(24);
    const int line = QFontMetrics(large).height();
    const QString longTitle = QStringLiteral("Discard the changes to this document for good");

    Dialog one(QStringLiteral("About"));
    const int oneTop = one.show();
    CHECK(oneTop == 24 + line + 16);
    // Two lines take the room of two, and the dialog is taller by a line.
    Dialog two(longTitle);
    const int twoTop = two.show();
    CHECK(two.dialog.width() == one.dialog.width());
    CHECK(twoTop >= oneTop + line - 2 && twoTop < oneTop + 2 * line);
    CHECK(two.dialog.height() == one.dialog.height() + twoTop - oneTop);
    // Its height for a width counts the lines at that width: on one line where the dialog is wide enough.
    CHECK(two.dialog.hasHeightForWidth());
    CHECK(two.dialog.heightForWidth(2000) == one.dialog.heightForWidth(2000));
    CHECK(two.dialog.heightForWidth(two.dialog.width()) == two.dialog.height());
    // Made wider, it gives the room of the second line back.
    two.dialog.resize(2000, two.dialog.height());
    QCoreApplication::processEvents();
    CHECK(two.dialog.contentsMargins().top() == one.dialog.contentsMargins().top());
    two.dialog.hide();

    // A platform that narrows its dialogs makes them as high as the wrapped headline needs.
    platform->dialogWidthLimit = 300;
    QDialog narrowed;
    narrowed.setWindowTitle(longTitle);
    auto *rows = new QVBoxLayout(&narrowed);
    auto *message = new QLabel(QStringLiteral("Everything that was typed since the document was last saved is lost "
                                              "when the changes are discarded."));
    message->setWordWrap(true);
    rows->addWidget(message);
    narrowed.show();
    QCoreApplication::processEvents();
    CHECK(narrowed.width() == 300);
    const int below = message->mapTo(&narrowed, QPoint(0, 0)).y();
    CHECK(below >= 24 + 2 * line + 16 - 2);
    CHECK(narrowed.height() >= below + message->height());
    narrowed.hide();
    platform->dialogWidthLimit = 0;

    // With an icon of its own, the icon is 24 from the top in the middle, and the headline is centered 16 under it.
    QPixmap glyph(24, 24);
    glyph.fill(Qt::transparent);
    {
        QPainter painter(&glyph);
        painter.fillRect(QRect(4, 4, 16, 16), Qt::black);
    }
    Dialog centered(QStringLiteral("Hi"));
    centered.dialog.setWindowIcon(QIcon(glyph));
    const int centeredTop = centered.show();
    CHECK(centeredTop == 24 + 24 + 16 + line + 16);
    CHECK(centered.dialog.height() == one.dialog.height() + 24 + 16);
    const QImage image = centered.dialog.grab().toImage();
    const int middle = centered.dialog.width() / 2;
    // The icon, recolored in Secondary, at the top in the middle; the sides of the room are bare.
    CHECK(near(image.pixelColor(middle, 24 + 12), style->color(Role::Secondary), 12));
    CHECK(!near(image.pixelColor(24 + 12, 24 + 12), style->color(Role::Secondary), 40));
    // The headline's ink is around the middle, not at the start.
    const auto ink = [&](int left, int right) {
        int count = 0;
        for (int y = 64; y < 64 + line; ++y)
            for (int x = left; x < right; ++x)
                count += near(image.pixelColor(x, y), style->color(Role::OnSurface), 60) ? 1 : 0;
        return count;
    };
    if (textIsDrawn()) {
        CHECK(ink(middle - 30, middle + 30) > 20);
        CHECK(ink(24, middle - 60) == 0);
    }
    centered.dialog.hide();
    // The application's icon does not count: without its own, a dialog keeps the headline at the start.
    QGuiApplication::setWindowIcon(QIcon(glyph));
    Dialog plain(QStringLiteral("Hi"));
    CHECK(plain.show() == oneTop);
    plain.dialog.hide();
    QGuiApplication::setWindowIcon(QIcon());

    // A message box with an icon has it over its headline, in Secondary, rather than beside its text.
    QMessageBox box(QMessageBox::Information, QStringLiteral("Saved"), QStringLiteral("The file was saved."),
                    QMessageBox::Ok);
    box.show();
    QCoreApplication::processEvents();
    const QImage boxImage = box.grab().toImage();
    const int boxMiddle = box.width() / 2;
    // The round sign: its ring in Secondary, the "i" in the middle cut out of it.
    CHECK(near(boxImage.pixelColor(boxMiddle - 7, 24 + 12), style->color(Role::Secondary), 12));
    CHECK(!near(boxImage.pixelColor(boxMiddle, 24 + 14), style->color(Role::Secondary), 40));
    const QList<QLabel *> labels = box.findChildren<QLabel *>();
    for (const QLabel *label : labels) {
        if (label->text() == QStringLiteral("The file was saved.")) {
            CHECK(label->mapTo(&box, QPoint(0, 0)).x() == 24);
            CHECK(label->mapTo(&box, QPoint(0, 0)).y() >= 24 + 24 + 16 + line + 16);
        }
    }
    box.hide();
    platform->dialogTitles = true;
}

void testGroupBoxAndTextEditors() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.resize(360, 500);
    auto *group = new QGroupBox(QStringLiteral("Notifications"), &window);
    auto *rows = new QVBoxLayout(group);
    auto *inside = new QWidget;
    inside->setFixedHeight(40);
    rows->addWidget(inside);
    group->setGeometry(10, 10, 300, 100);
    window.show();
    window.activateWindow();
    QCoreApplication::processEvents();
    // The title over the card, which keeps 16 around what is in it.
    CHECK(inside->mapTo(group, QPoint(0, 0)) == QPoint(16, 24 + 4 + 16));
    CHECK(group->sizeHint().height() == 24 + 4 + 16 + 40 + 16);
    CHECK(group->font().pixelSize() != 14 || group->font().weight() != QFont::Medium); // its children keep theirs
    const auto shot = [&](const QWidget *widget) { return window.grab().toImage().copy(widget->geometry()); };
    QImage image = shot(group);
    CHECK(is(image.pixelColor(150, 28), Role::OutlineVariant)); // the card,
    CHECK(is(image.pixelColor(0, 28), Role::Surface));          // with round corners
    CHECK(is(image.pixelColor(0, 70), Role::OutlineVariant));
    CHECK(is(image.pixelColor(150, 99), Role::OutlineVariant));
    if (textIsDrawn()) {
        int accented = 0;
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 150; ++x)
                accented += near(image.pixelColor(x, y), style->color(Role::Primary), 40) ? 1 : 0;
        CHECK(accented > 20);
    }
    group->setFlat(true);
    image = shot(group);
    CHECK(is(image.pixelColor(150, 28), Role::OutlineVariant));
    CHECK(is(image.pixelColor(0, 70), Role::Surface) && is(image.pixelColor(150, 99), Role::Surface));
    group->setFlat(false);

    // A checkable one has Material's check box in its title, and a tap on the title turns it.
    group->setCheckable(true);
    group->setChecked(true);
    group->resize(300, 40 + 4 + 16 + 40 + 16);
    QCoreApplication::processEvents();
    CHECK(inside->mapTo(group, QPoint(0, 0)).y() == 40 + 4 + 16);
    CHECK(is(shot(group).pixelColor(11 + 3, 13), Role::Primary)); // filled, in a corner clear of the mark
    const auto tap = [](QWidget *on, int x, int y) {
        const QPointF at(x, y);
        QMouseEvent press(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(on, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(on, &release);
        QCoreApplication::processEvents();
    };
    tap(group, 60, 20);
    CHECK(!group->isChecked() && !inside->isEnabled());
    tap(group, 20, 20);
    CHECK(group->isChecked());

    // A text editor is an outlined field, which shows that it has the focus.
    auto *plain = new QPlainTextEdit(&window);
    plain->setPlainText(QStringLiteral("Text"));
    plain->setGeometry(10, 150, 300, 100);
    auto *rich = new QTextEdit(&window);
    rich->setGeometry(10, 270, 300, 100);
    plain->show();
    rich->show();
    QCoreApplication::processEvents();
    for (QAbstractScrollArea *editor :
         {static_cast<QAbstractScrollArea *>(plain), static_cast<QAbstractScrollArea *>(rich)}) {
        CHECK(editor->frameWidth() == 10);
        CHECK(editor->font().pixelSize() == 16);
        editor->clearFocus();
        window.setFocus();
        image = shot(editor);
        CHECK(is(image.pixelColor(150, 0), Role::Outline));
        CHECK(is(image.pixelColor(150, 99), Role::Outline));
        CHECK(!is(image.pixelColor(150, 2), Role::Outline));
        CHECK(is(image.pixelColor(0, 0), Role::Surface)); // round
        editor->setFocus();
        image = shot(editor);
        CHECK(is(image.pixelColor(150, 0), Role::Primary));
        CHECK(is(image.pixelColor(150, 2), Role::Primary)); // thicker
    }
    // One that the program gave another frame keeps it.
    plain->setFrameShape(QFrame::NoFrame);
    plain->clearFocus();
    CHECK(!is(shot(plain).pixelColor(150, 0), Role::Outline));

    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(rich->frameWidth() < 10);
}

// The text cursor in the accent color and two wide, and multi-line text fields: QPlainTextEdit and QTextEdit with the
// container, outline, label and supporting text of a line edit, growing with their text.
void testMultilineFieldsAndCaret() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *notes = new QPlainTextEdit;
    notes->setProperty("materialTextField", QStringLiteral("outlined"));
    notes->setProperty("materialLabel", QStringLiteral("Notes"));
    notes->setProperty("materialSupportingText", QStringLiteral("About the harvest"));
    auto *story = new QTextEdit;
    story->setProperty("materialTextField", QStringLiteral("filled"));
    story->setProperty("materialLabel", QStringLiteral("Story"));
    auto *name = new QLineEdit;
    auto *elsewhere = new QPushButton(QStringLiteral("Elsewhere"));
    layout->addWidget(notes);
    layout->addWidget(story);
    layout->addWidget(name);
    layout->addWidget(elsewhere);
    layout->addStretch(1);
    window.resize(360, 600);
    window.show();
    elsewhere->setFocus();
    QCoreApplication::processEvents();

    // Two wide, as the style says; a width that the program sets wins.
    CHECK(style->pixelMetric(QStyle::PM_TextCursorWidth) == 2);
    CHECK(notes->cursorWidth() == 2);
    auto *other = new QPlainTextEdit;
    other->setCursorWidth(3);
    CHECK(other->cursorWidth() == 3);
    delete other;

    // One line in a container of 56, with the room for the label on the outline and for the line below.
    const int line = notes->fontMetrics().height();
    CHECK(notes->height() == 8 + 56 + 20);
    CHECK(story->height() >= 56 && story->height() < 56 + line);
    const QPoint text = notes->viewport()->mapTo(notes, QPoint(0, 0));
    CHECK(text.x() + 4 == 16); // the text 16 from the side, as in a line edit

    const auto at = [&](QWidget *widget, int x, int y) {
        const QPoint point = widget->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const int w = notes->width();
    // At rest the outline is whole; with text, it makes way for the label, which has floated onto it.
    CHECK(near(at(notes, 24, 8), style->color(Role::Outline), 40));
    CHECK(is(at(notes, w / 2, 8 + 55), Role::Outline));
    CHECK(is(at(notes, w - 8, notes->height() - 3), Role::Surface)); // the supporting text has no container
    notes->setPlainText(QStringLiteral("Collected in the rain"));
    QCoreApplication::processEvents();
    CHECK(is(at(notes, 14, 8), Role::Surface));
    CHECK(near(at(notes, w - 24, 8), style->color(Role::Outline), 40));

    // It grows with its lines, up to its maximum height, and then scrolls.
    notes->setPlainText(QStringLiteral("One\nTwo\nThree"));
    QCoreApplication::processEvents();
    CHECK(notes->height() == 8 + 56 + 20 + 2 * line);
    notes->setMaximumHeight(140);
    notes->setPlainText(QStringLiteral("1\n2\n3\n4\n5\n6\n7\n8"));
    QCoreApplication::processEvents();
    CHECK(notes->height() == 140);
    CHECK(notes->verticalScrollBar()->maximum() > 0);
    notes->setPlainText(QString());
    QCoreApplication::processEvents();
    CHECK(notes->height() == 8 + 56 + 20);

    // A filled one: its container shows through the text, with a line at the bottom.
    const int sw = story->width();
    CHECK(is(at(story, sw - 40, 6), Role::SurfaceContainerHighest));
    CHECK(is(at(story, sw / 2, story->height() / 2), Role::SurfaceContainerHighest));
    CHECK(is(at(story, sw - 40, story->height() - 1), Role::OnSurfaceVariant));
    CHECK(is(at(story, 0, 0), Role::Surface)); // rounded at the top

    // The caret: Primary, and two wide.
    notes->setFocus();
    QCoreApplication::processEvents();
    CHECK(is(at(notes, w / 2, 8 + 1), Role::Primary)); // the outline in the accent color, thicker
    QRect caret = notes->cursorRect();
    const auto caretAt = [&](int dx) { return at(notes->viewport(), caret.x() + dx, caret.center().y()); };
    CHECK(is(caretAt(0), Role::Primary) && is(caretAt(1), Role::Primary));
    CHECK(!is(caretAt(2), Role::Primary));
    // and Error while the field shows an error, which turns the outline red too.
    notes->setProperty("materialError", QStringLiteral("Too long"));
    QCoreApplication::processEvents();
    caret = notes->cursorRect();
    CHECK(is(caretAt(0), Role::Error) && is(caretAt(1), Role::Error));
    CHECK(is(at(notes, w / 2, 8), Role::Error));
    CHECK(notes->height() == 8 + 56 + 20); // the room was there

    // A line edit's caret too.
    name->setFocus();
    QCoreApplication::processEvents();
    const QImage image = window.grab().toImage();
    const QPoint origin = name->mapTo(&window, QPoint(0, 0));
    int columns = 0;
    for (int x = 8; x < name->width() - 8; ++x)
        columns += is(image.pixelColor(origin.x() + x, origin.y() + name->height() / 2), Role::Primary) ? 1 : 0;
    CHECK(columns == 2);

    // A text editor that is no longer a field takes its plain frame back.
    notes->setProperty("materialTextField", QVariant());
    notes->setProperty("materialLabel", QVariant());
    notes->setProperty("materialSupportingText", QVariant());
    notes->setProperty("materialError", QVariant());
    QCoreApplication::processEvents();
    CHECK(notes->viewportMargins().isNull());
    CHECK(notes->sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// With motion, a sheet slides in from its edge and a snackbar fades in; one that goes leaves a picture that fades out.
void testSheetAndSnackbarMotion() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(true);
    QApplication::setStyle(style);

    QWidget window;
    window.setGeometry(100, 50, 400, 700);
    window.show();
    QCoreApplication::processEvents();
    QDialog sheet(&window);
    sheet.setProperty("materialSheet", QStringLiteral("bottom"));
    auto *column = new QVBoxLayout(&sheet);
    auto *content = new QWidget;
    content->setFixedHeight(180);
    column->addWidget(content);
    sheet.show();
    const int high = sheet.height();
    const QRect rest(100, 50 + 700 - high, 400, high);
    CHECK(sheet.geometry() == rest.translated(0, high)); // below its place, at first
    // A platform that places sheets itself (Android) is told how far from its place it is.
    CHECK(sheet.windowHandle()->property("_nt_sheetOffset").toInt() == high);
    wait(60);
    CHECK(sheet.y() > rest.y() && sheet.y() < rest.y() + high); // on its way
    int lowest = sheet.y();
    for (int i = 0; i < 100 && sheet.y() != rest.y(); ++i) {
        wait(10);
        lowest = std::min(lowest, sheet.y());
    }
    CHECK(sheet.geometry() == rest && lowest >= rest.y()); // and it never goes past it
    CHECK(sheet.windowHandle()->property("_nt_sheetOffset").toInt() == 0);
    sheet.hide();
    QCoreApplication::processEvents();

    QDialog drawer(&window);
    drawer.setProperty("materialSheet", QStringLiteral("drawer"));
    new QVBoxLayout(&drawer);
    drawer.show();
    CHECK(drawer.x() == 100 - drawer.width());
    wait(800);
    CHECK(drawer.geometry() == QRect(100, 50, 400 - 56, 700));
    drawer.hide();

    QMainWindow main;
    main.setCentralWidget(new QWidget);
    auto *status = new QStatusBar;
    status->setProperty("materialStatusBar", QStringLiteral("snackbar"));
    main.setStatusBar(status);
    main.resize(360, 400);
    main.show();
    QCoreApplication::processEvents();
    status->showMessage(QStringLiteral("Archived"));
    auto *fade = qobject_cast<QGraphicsOpacityEffect *>(status->graphicsEffect());
    CHECK(fade && fade->opacity() < 0.5);
    CHECK(status->property("_nt_effectScale").toDouble() < 0.85); // and smaller
    {
        // Drawn smaller about its middle: the corner of the bar is not covered yet, the middle is.
        // grab() leaves graphics effects out, so this reads what the window shows, a little way in: the middle
        // shows through, the corner not yet, since the bar is drawn smaller.
        wait(50);
        main.repaint();
        const QImage early = QOffscreenBackingStore::imageOf(main.windowHandle());
        const QPoint corner = status->mapTo(&main, QPoint(2, status->height() / 2));
        const QPoint middle = status->mapTo(&main, QPoint(status->width() / 2, status->height() / 2));
        const QColor behind = early.pixelColor(status->mapTo(&main, QPoint(status->width() / 2, -20)));
        CHECK(near(early.pixelColor(corner), behind, 6));
        CHECK(!near(early.pixelColor(middle), behind, 30));
    }
    wait(500);
    CHECK(!status->graphicsEffect()); // gone once it is in
    CHECK(!status->property("_nt_effectScale").isValid());
    const QRect shown = status->geometry();
    status->clearMessage();
    QCoreApplication::processEvents();
    auto *ghost = main.findChild<QWidget *>(QStringLiteral("_nt_snackbarGhost"));
    CHECK(ghost && ghost->isVisible() && ghost->geometry() == shown);
    CHECK(status->height() == 0);
    wait(400);
    CHECK(!main.findChild<QWidget *>(QStringLiteral("_nt_snackbarGhost")));

    // An effect of the application's own is left alone.
    auto *own = new QGraphicsOpacityEffect;
    own->setOpacity(0.7);
    status->setGraphicsEffect(own);
    status->showMessage(QStringLiteral("Mine"));
    CHECK(status->graphicsEffect() == own && own->opacity() == 0.7);
    status->setGraphicsEffect(nullptr);

    // Without motion, all of it is at once.
    style->setMotionEnabled(false);
    sheet.show();
    CHECK(sheet.geometry() == rest);
    sheet.hide();
    status->showMessage(QStringLiteral("Again"));
    CHECK(!status->graphicsEffect());
    status->clearMessage();
    CHECK(!main.findChild<QWidget *>(QStringLiteral("_nt_snackbarGhost")));
}

// Text fields with icons at their ends, a prefix, a suffix and a counter.
void testFieldIconsAffixesCounter() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.resize(360, 400);
    QPixmap square(48, 48);
    square.fill(Qt::black); // an icon of one color, which takes the color of the field's icons
    auto *edit = new QLineEdit(&window);
    edit->setProperty("materialLabel", QStringLiteral("Search"));
    QAction *find = edit->addAction(QIcon(square), QLineEdit::LeadingPosition);
    edit->setGeometry(0, 0, 300, edit->sizeHint().height());
    auto *other = new QLineEdit(&window);
    other->setGeometry(0, 200, 300, 56);
    window.show();
    QCoreApplication::processEvents();
    const auto shot = [&](const QWidget *w) { return window.grab().toImage().copy(w->geometry()); };
    const int box = edit->height() - 56; // the room over the box, for the floating label
    QImage image = shot(edit);
    // The icon is 24, 12 from the edge, in the quiet color.
    CHECK(is(image.pixelColor(12 + 12, box + 28), Role::OnSurfaceVariant));
    CHECK(!is(image.pixelColor(12 + 26, box + 28), Role::OnSurfaceVariant));
    CHECK(!is(image.pixelColor(8, box + 28), Role::OnSurfaceVariant));
    // The text begins 52 in, and a tap on the icon triggers it, not the text.
    edit->setFocus();
    edit->setText(QStringLiteral("x"));
    edit->home(false);
    int found = 0;
    QObject::connect(find, &QAction::triggered, [&found] { ++found; });
    const QPointF on(20, box + 28);
    QMouseEvent press(QEvent::MouseButtonPress, on, on, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(edit, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, on, on, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(edit, &release);
    CHECK(found == 1);
    const auto textLeft = [&] {
        QImage picture = shot(edit);
        for (int x = 40; x < 200; ++x)
            for (int y = box + 16; y < box + 40; ++y)
                if (near(picture.pixelColor(x, y), style->color(Role::OnSurface), 90))
                    return x;
        return -1;
    };
    if (textIsDrawn())
        CHECK(std::abs(textLeft() - 52) <= 3);
    // A trailing icon 12 from the other edge, in the color of an error while there is one.
    edit->addAction(QIcon(square), QLineEdit::TrailingPosition);
    QCoreApplication::processEvents();
    CHECK(is(shot(edit).pixelColor(300 - 24, box + 28), Role::OnSurfaceVariant));
    edit->setProperty("materialError", QStringLiteral("Wrong"));
    QCoreApplication::processEvents();
    image = shot(edit);
    CHECK(is(image.pixelColor(300 - 24, box + 28), Role::Error));
    CHECK(is(image.pixelColor(24, box + 28), Role::OnSurfaceVariant)); // the leading one is not part of the error
    edit->setProperty("materialError", QVariant());

    // A prefix and a suffix show once the label has floated, and the text goes between them.
    auto *price = new QLineEdit(&window);
    price->setProperty("materialLabel", QStringLiteral("Price"));
    price->setProperty("materialPrefix", QStringLiteral("$"));
    price->setProperty("materialSuffix", QStringLiteral("per kg"));
    price->setGeometry(0, 100, 300, price->sizeHint().height());
    price->show();
    QCoreApplication::processEvents();
    const auto inked = [&](const QRect &area) {
        const QImage picture = shot(price);
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                count += near(picture.pixelColor(x, y), style->color(Role::OnSurfaceVariant), 60) ? 1 : 0;
        return count;
    };
    const QRect end(220, box + 18, 64, 22);
    if (textIsDrawn()) {
        CHECK(inked(end) == 0); // the label rests: no suffix yet
        price->setText(QStringLiteral("12"));
        QCoreApplication::processEvents();
        CHECK(inked(end) > 20);
        CHECK(inked(QRect(14, box + 18, 12, 22)) > 5); // the dollar
    }
    price->clear();
    other->setFocus();
    QCoreApplication::processEvents();
    if (textIsDrawn())
        CHECK(inked(end) == 0); // the label rests again

    // A counter of the characters, at the end of the line under the field, which it makes room for.
    auto *note = new QLineEdit(&window);
    const int plain = note->sizeHint().height();
    note->setMaxLength(40);
    note->setProperty("materialCounter", true);
    CHECK(note->sizeHint().height() == plain + 20);
    note->setText(QStringLiteral("hazel"));
    note->setGeometry(0, 300, 300, note->sizeHint().height());
    note->show();
    QCoreApplication::processEvents();
    if (textIsDrawn()) {
        const QImage picture = shot(note);
        int count = 0;
        for (int y = 58; y < picture.height(); ++y)
            for (int x = 240; x < 284; ++x)
                count += near(picture.pixelColor(x, y), style->color(Role::OnSurfaceVariant), 60) ? 1 : 0;
        CHECK(count > 10);
    }
}

// The actions at the ends of a text field are standard icon buttons, and an action can be an avatar.
void testFieldIconButtonsAndAvatars() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QWidget window;
    window.resize(360, 200);
    QPixmap square(48, 48);
    square.fill(Qt::black);
    auto *edit = new QLineEdit(&window);
    QAction *action = edit->addAction(QIcon(square), QLineEdit::TrailingPosition);
    edit->setGeometry(0, 0, 300, 56);
    window.show();
    QCoreApplication::processEvents();
    const auto shot = [&](const QWidget *w) { return window.grab().toImage().copy(w->geometry()); };
    const auto send = [](QWidget *w, QEvent::Type type, const QPointF &at, Qt::MouseButtons buttons) {
        QMouseEvent event(type, at, at, type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(w, &event);
    };
    // The icon is 264 to 288; the circle of 40 about it reaches 8 past it, so (276, 12) is on the layer only.
    const QPoint layer(276, 12);
    const QColor rest = shot(edit).pixelColor(layer);
    CHECK(near(shot(edit).pixelColor(276, 4), rest, 3)); // and nothing yet out of the circle either
    send(edit, QEvent::MouseMove, QPointF(276, 28), Qt::NoButton);
    QImage image = shot(edit);
    const auto mixed = [&](double opacity) {
        const QColor ink = style->color(Role::OnSurfaceVariant);
        const auto channel = [opacity](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * opacity)); };
        return QColor(channel(rest.red(), ink.red()), channel(rest.green(), ink.green()),
                      channel(rest.blue(), ink.blue()));
    };
    CHECK(near(image.pixelColor(layer), mixed(0.08), 3));
    CHECK(near(image.pixelColor(276, 4), rest, 3)); // a circle, not the whole slot
    CHECK(near(image.pixelColor(250, 28), rest, 3));
    // A press is stronger, and goes when it ends.
    int triggered = 0;
    QObject::connect(action, &QAction::triggered, [&triggered] { ++triggered; });
    send(edit, QEvent::MouseButtonPress, QPointF(276, 28), Qt::LeftButton);
    const QColor held = shot(edit).pixelColor(layer);
    CHECK(near(held, mixed(0.10), 3));
    CHECK(held.lightness() < image.pixelColor(layer).lightness());
    send(edit, QEvent::MouseButtonRelease, QPointF(276, 28), Qt::NoButton);
    CHECK(triggered == 1);
    CHECK(near(shot(edit).pixelColor(layer), mixed(0.08), 3));
    // Off the icon, and out of the field, there is no layer.
    send(edit, QEvent::MouseMove, QPointF(100, 28), Qt::NoButton);
    CHECK(near(shot(edit).pixelColor(layer), rest, 3));
    send(edit, QEvent::MouseMove, QPointF(276, 28), Qt::NoButton);
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(edit, &leave);
    CHECK(near(shot(edit).pixelColor(layer), rest, 3));
    // A disabled action has none.
    action->setEnabled(false);
    send(edit, QEvent::MouseMove, QPointF(276, 28), Qt::NoButton);
    CHECK(near(shot(edit).pixelColor(layer), rest, 3));
    action->setEnabled(true);

    // With motion, a press ripples from where it was.
    style->setMotionEnabled(true);
    send(edit, QEvent::MouseMove, QPointF(100, 28), Qt::NoButton);
    send(edit, QEvent::MouseButtonPress, QPointF(276, 28), Qt::LeftButton);
    wait(400);
    CHECK(shot(edit).pixelColor(layer).lightness() < rest.lightness());
    send(edit, QEvent::MouseButtonRelease, QPointF(276, 28), Qt::NoButton);
    CHECK(triggered == 2);
    wait(800);
    CHECK(near(shot(edit).pixelColor(layer), rest, 3));
    style->setMotionEnabled(false);

    // In a search bar the icons are 24 in targets of 48, the outermost 4 from the edge; an avatar is a round picture
    // of 30 in its own colors, last, past the clear button.
    auto *search = new QLineEdit(&window);
    search->setProperty("materialTextField", QStringLiteral("search"));
    search->setClearButtonEnabled(true);
    search->setText(QStringLiteral("hazel"));
    search->setGeometry(10, 100, 340, 56);
    search->addAction(QIcon(square), QLineEdit::TrailingPosition);
    QPixmap picture(60, 60);
    picture.fill(QColor(200, 30, 40));
    QAction *avatar = new QAction(QIcon(picture), QStringLiteral("Account"), search);
    avatar->setProperty("materialAvatar", true);
    search->addAction(avatar, QLineEdit::TrailingPosition);
    search->show();
    QCoreApplication::processEvents();
    image = shot(search);
    // The avatar is about (312, 28): 297 to 327 across, 13 to 43 down.
    CHECK(near(image.pixelColor(312, 28), QColor(200, 30, 40), 3));
    CHECK(near(image.pixelColor(300, 28), QColor(200, 30, 40), 3));
    for (const QPoint corner : {QPoint(298, 14), QPoint(326, 14), QPoint(298, 42), QPoint(326, 42)})
        CHECK(is(image.pixelColor(corner), Role::SurfaceContainerHigh));
    // the cross 16 in from the avatar's target, then the icon in the colour of the icons.
    int crossed = 0;
    for (int y = 16; y < 40; ++y)
        for (int x = 252; x < 276; ++x)
            crossed += is(image.pixelColor(x, y), Role::OnSurfaceVariant) ? 1 : 0;
    CHECK(crossed > 20);
    CHECK(is(image.pixelColor(216, 28), Role::OnSurfaceVariant));
    CHECK(is(image.pixelColor(232, 28), Role::SurfaceContainerHigh));
    // A press on the avatar triggers it, and the text keeps out of the way of the icons.
    int account = 0;
    QObject::connect(avatar, &QAction::triggered, [&account] { ++account; });
    send(search, QEvent::MouseButtonPress, QPointF(312, 28), Qt::LeftButton);
    send(search, QEvent::MouseButtonRelease, QPointF(312, 28), Qt::NoButton);
    CHECK(account == 1);
    CHECK(search->text() == QStringLiteral("hazel"));
    send(search, QEvent::MouseButtonPress, QPointF(264, 28), Qt::LeftButton);
    send(search, QEvent::MouseButtonRelease, QPointF(264, 28), Qt::NoButton);
    CHECK(search->text().isEmpty());
    // The avatar has the layer of an icon button under the pointer too, around the picture.
    send(search, QEvent::MouseMove, QPointF(312, 28), Qt::NoButton);
    CHECK(!is(shot(search).pixelColor(312, 10), Role::SurfaceContainerHigh));
    CHECK(near(shot(search).pixelColor(312, 28), QColor(200, 30, 40), 3));
}

// A snackbar may have a close button at its end, which takes the message away.
void testSnackbarClose() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

    QMainWindow window;
    window.setCentralWidget(new QWidget);
    auto *status = new QStatusBar;
    status->setProperty("materialStatusBar", QStringLiteral("snackbar"));
    window.setStatusBar(status);
    window.resize(360, 400);
    window.show();
    QCoreApplication::processEvents();
    const auto close = [&] { return status->findChild<QAbstractButton *>(QStringLiteral("_nt_snackbarClose")); };
    status->showMessage(QStringLiteral("Saved"));
    QCoreApplication::processEvents();
    CHECK(!close()); // only when asked for
    status->setProperty("materialCloseButton", true);
    QCoreApplication::processEvents();
    QAbstractButton *button = close();
    CHECK(button && button->isVisible());
    if (!button)
        return;
    CHECK(button->geometry() == QRect(status->width() - 48, 0, 48, 48));
    // A cross in the color of the text, in the middle of the button.
    const QImage image = window.grab().toImage().copy(status->geometry());
    int ink = 0;
    for (int y = 14; y < 34; ++y)
        for (int x = status->width() - 34; x < status->width() - 14; ++x)
            ink += is(image.pixelColor(x, y), Role::InverseOnSurface) ? 1 : 0;
    CHECK(ink > 10);
    CHECK(is(image.pixelColor(status->width() - 44, 6), Role::InverseSurface));
    // The pointer on it, the press and the keyboard focus show the state layer of an icon button; the focus a ring.
    QWindow *handle = window.windowHandle();
    const QPoint inLayer = button->mapTo(&window, QPoint(24, 8)); // inside the layer, clear of the cross
    const QPoint inRing = button->mapTo(&window, QPoint(24, 1));
    const auto at = [&](QPoint point) { return window.grab().toImage().pixelColor(point); };
    const auto move = [&](QPoint point) {
        QWindowSystemInterface::handleMouseEvent(handle, QPointF(point), QPointF(point), Qt::NoButton, Qt::NoButton,
                                                 QEvent::MouseMove, Qt::NoModifier);
        QCoreApplication::processEvents();
    };
    CHECK(is(at(inLayer), Role::InverseSurface));
    move(inLayer);
    CHECK(button->underMouse());
    const QColor hovered = at(inLayer);
    CHECK(!is(hovered, Role::InverseSurface) && !is(hovered, Role::InverseOnSurface));
    button->setDown(true);
    QCoreApplication::processEvents();
    status->update();
    QCoreApplication::processEvents();
    CHECK(!near(at(inLayer), hovered, 2)); // the press is stronger
    button->setDown(false);
    move(QPoint(10, 10));
    CHECK(!button->underMouse() && is(at(inLayer), Role::InverseSurface));
    CHECK(button->focusPolicy() == Qt::TabFocus);
    QWindowSystemInterface::handleKeyEvent(handle, QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QWindowSystemInterface::handleKeyEvent(handle, QEvent::KeyRelease, Qt::Key_Tab, Qt::NoModifier);
    QCoreApplication::processEvents();
    CHECK(button->hasFocus());
    CHECK(is(at(inRing), Role::Secondary));
    CHECK(!is(at(inLayer), Role::InverseSurface));
    button->clearFocus();
    QCoreApplication::processEvents();
    CHECK(is(at(inRing), Role::InverseSurface) && is(at(inLayer), Role::InverseSurface));
    // An action goes before it.
    auto *undo = new QPushButton(QStringLiteral("Undo"));
    status->addPermanentWidget(undo);
    status->showMessage(QStringLiteral("Deleted"));
    QCoreApplication::processEvents();
    CHECK(undo->geometry().right() < button->geometry().left());
    // A tap on it takes the message away, and the button with it.
    const QPointF middle(24, 24);
    QMouseEvent press(QEvent::MouseButtonPress, middle, middle, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(button, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, middle, middle, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(button, &release);
    QCoreApplication::processEvents();
    CHECK(status->currentMessage().isEmpty() && status->height() == 0 && button->isHidden());
    status->showMessage(QStringLiteral("Again"));
    CHECK(button->isVisible());
    status->setProperty("materialCloseButton", false);
    QCoreApplication::processEvents();
    CHECK(!close());
}

// A top app bar is Surface at rest and SurfaceContainer while the content under it is scrolled.
void testAppBarScrolled() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QMainWindow window;
    auto *bar = new QToolBar;
    bar->setProperty("materialToolBar", QStringLiteral("appBar"));
    bar->setWindowTitle(QStringLiteral("Inbox"));
    window.addToolBar(bar);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *list = new QListWidget;
    for (int i = 0; i < 100; ++i)
        list->addItem(QStringLiteral("Message %1").arg(i));
    layout->addWidget(list, 1);
    // A small scroll area at the bottom, which is not what the bar is over.
    auto *notes = new QPlainTextEdit;
    QString text;
    for (int i = 0; i < 40; ++i)
        text += QStringLiteral("Line %1\n").arg(i);
    notes->setPlainText(text);
    notes->setFixedHeight(60);
    layout->addWidget(notes);
    window.setCentralWidget(central);
    window.resize(360, 480);
    window.show();
    QCoreApplication::processEvents();
    CHECK(list->verticalScrollBar()->maximum() > 0 && notes->verticalScrollBar()->maximum() > 0);

    const auto barColor = [&] { return pixel(*bar, bar->width() - 10, 4); };
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    CHECK(!near(style->color(Role::Surface), style->color(Role::SurfaceContainer), 3));

    // What scrolls repaints the bar.
    int paints = 0;
    struct PaintCounter : QObject {
        int *count;
        explicit PaintCounter(int *c) : count(c) {}
        bool eventFilter(QObject *, QEvent *event) override {
            if (event->type() == QEvent::Paint)
                ++*count;
            return false;
        }
    } counter(&paints);
    bar->installEventFilter(&counter);

    CHECK(is(barColor(), Role::Surface));
    QCoreApplication::processEvents();
    paints = 0;
    list->verticalScrollBar()->setValue(10);
    QCoreApplication::processEvents();
    CHECK(paints > 0);
    CHECK(is(barColor(), Role::SurfaceContainer));
    list->verticalScrollBar()->setValue(0);
    CHECK(is(barColor(), Role::Surface));

    // The small one does not change it.
    notes->verticalScrollBar()->setValue(notes->verticalScrollBar()->maximum());
    CHECK(is(barColor(), Role::Surface));
    notes->verticalScrollBar()->setValue(0);

    // A list that is hidden, as on another page, is not under the bar.
    list->verticalScrollBar()->setValue(10);
    CHECK(is(barColor(), Role::SurfaceContainer));
    list->hide();
    QCoreApplication::processEvents();
    CHECK(is(barColor(), Role::Surface));
    list->show();
    QCoreApplication::processEvents();
    CHECK(is(barColor(), Role::SurfaceContainer));
    list->verticalScrollBar()->setValue(0);

    // A QScrollArea that fills the central widget counts as well.
    auto *area = new QScrollArea;
    auto *page = new QWidget;
    page->setMinimumSize(300, 2000);
    area->setWidget(page);
    window.setCentralWidget(area);
    QCoreApplication::processEvents();
    CHECK(is(barColor(), Role::Surface));
    area->verticalScrollBar()->setValue(50);
    CHECK(is(barColor(), Role::SurfaceContainer));
    area->verticalScrollBar()->setValue(0);
    CHECK(is(barColor(), Role::Surface));

    // With motion the color fades: it sets off from Surface and comes to SurfaceContainer.
    style->setMotionEnabled(true);
    QCoreApplication::processEvents();
    CHECK(is(barColor(), Role::Surface));
    area->verticalScrollBar()->setValue(50);
    CHECK(is(barColor(), Role::Surface));
    wait(700);
    CHECK(is(barColor(), Role::SurfaceContainer));
    area->verticalScrollBar()->setValue(0);
    wait(700);
    CHECK(is(barColor(), Role::Surface));
    bar->removeEventFilter(&counter);
}

void testDatePicker() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(400, 560);
    auto *calendar = new QCalendarWidget(&window);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    const int year = QDate::currentDate().year();
    calendar->setDateRange(QDate(year - 25, 3, 15), QDate(year + 10, 9, 10));
    calendar->setSelectedDate(QDate(year - 1, 1, 20));
    window.show();
    QCoreApplication::processEvents();
    const QList<QToolButton *> arrows = calendar->findChild<QLabel *>()->parentWidget()->findChildren<QToolButton *>();
    auto *title = calendar->findChild<QLabel *>();
    CHECK(arrows.size() == 2 && title);
    if (arrows.size() != 2 || !title)
        return;
    QWidget *navigation = arrows.at(0)->parentWidget();
    const int bar = navigation->sizeHint().height();
    // A calendar has no headline unless it asks for one, and it may ask while it is shown.
    CHECK(calendar->sizeHint() == QSize(7 * 40, bar + 7 * 40));
    calendar->setProperty("materialDatePicker", "modal");
    CHECK(calendar->sizeHint() == QSize(7 * 40, 121 + bar + 7 * 40));
    calendar->resize(calendar->sizeHint());
    QCoreApplication::processEvents();
    CHECK(navigation->geometry() == QRect(0, 121, 7 * 40, bar));
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    const auto pixel = [&](const QPoint &point) { return calendar->grab().toImage().pixelColor(point.x(), point.y()); };
    // The headline is on the container, above a divider; the month is under it.
    CHECK(is(pixel(QPoint(2, 2)), Role::SurfaceContainerHigh));
    CHECK(is(pixel(QPoint(2, 119)), Role::SurfaceContainerHigh));
    CHECK(is(pixel(QPoint(140, 120)), Role::OutlineVariant));
    CHECK(is(pixel(QPoint(2, 121 + bar + 2)), Role::SurfaceContainerHigh));
    // The chosen date, in the headline, 24 in, in the color of the supporting text; it follows the selection.
    const QRect headline(24, 68, 7 * 40 - 36, 40);
    const auto inked = [&](const QRect &area) {
        const QImage image = calendar->grab().toImage().copy(area);
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                count += near(image.pixelColor(x, y), style->color(Role::OnSurfaceVariant), 40) ? 1 : 0;
        return count;
    };
    if (textIsDrawn()) {
        CHECK(inked(headline) > 40);
        CHECK(inked(QRect(24, 16, 120, 20)) > 10); // "Select date"
        CHECK(inked(QRect(0, 68, 20, 40)) == 0);
        const QImage before = calendar->grab().toImage().copy(headline);
        calendar->setSelectedDate(QDate(year - 1, 1, 21));
        CHECK(calendar->grab().toImage().copy(headline) != before);
    }
    calendar->setSelectedDate(QDate(year - 1, 1, 20));

    // A click on the month and the year shows the years, three to a row, in the place of the days, without the arrows.
    const auto click = [&](QWidget *target, const QPoint &at) {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), QPointF(at), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(target, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at), QPointF(at), Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QCoreApplication::sendEvent(target, &release);
        QCoreApplication::processEvents();
    };
    const auto key = [&](int code) {
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier);
        QCoreApplication::sendEvent(calendar, &press);
        QCoreApplication::processEvents();
    };
    click(title, title->rect().center());
    CHECK(!arrows.at(0)->isVisible() && !arrows.at(1)->isVisible());
    CHECK(navigation->geometry().height() == bar);
    // The years are from that of the minimum to that of the maximum, 36 in 12 rows, of which 5 fit; the year shown is
    // in the second row. A year's cell is a third of the width and 52 high, 8 under the bar.
    const int top = 121 + bar;
    int scrolled = 7;
    const auto centre = [&](int shown) {
        const int index = shown - (year - 25);
        const int column = index % 3;
        const int left = 7 * 40 * column / 3;
        const int right = 7 * 40 * (column + 1) / 3;
        return QPoint((left + right) / 2, top + 8 + (index / 3 - scrolled) * 52 + 26);
    };
    // The year shown is a filled pill of 72 by 36, this year is in a ring and the others are on the container.
    CHECK(is(pixel(centre(year - 1) + QPoint(-30, 0)), Role::Primary));
    CHECK(is(pixel(centre(year - 1) + QPoint(30, 0)), Role::Primary));
    CHECK(is(pixel(centre(year - 1) + QPoint(0, -15)), Role::Primary));
    CHECK(is(pixel(centre(year - 1) + QPoint(0, -21)), Role::SurfaceContainerHigh));
    CHECK(is(pixel(centre(year - 1) + QPoint(-40, 0)), Role::SurfaceContainerHigh));
    CHECK(near(pixel(centre(year) + QPoint(0, -18)), style->color(Role::Primary), 60));
    CHECK(is(pixel(centre(year) + QPoint(-30, 0)), Role::SurfaceContainerHigh));
    CHECK(is(pixel(centre(year + 1) + QPoint(-30, 0)), Role::SurfaceContainerHigh));
    CHECK(is(pixel(centre(year + 1) + QPoint(0, -18)), Role::SurfaceContainerHigh));
    // The years are in the color of the supporting text.
    if (textIsDrawn())
        CHECK(inked(QRect(centre(year + 1) - QPoint(30, 12), QSize(60, 24))) > 10);
    // The year under the pointer has a state layer of 0.08 on its pill, in the color of its text, and the one pressed
    // one of 0.10; a year is chosen when the button comes up on the one it went down on.
    const auto yearLayer = [&](Role ink, double opacity) {
        const QColor under = style->color(Role::SurfaceContainerHigh);
        const QColor over = style->color(ink);
        const auto mix = [&](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * opacity)); };
        return QColor(mix(under.red(), over.red()), mix(under.green(), over.green()), mix(under.blue(), over.blue()));
    };
    const auto mouse = [&](QEvent::Type type, const QPoint &at, Qt::MouseButtons buttons) {
        QMouseEvent event(type, QPointF(at), QPointF(at), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                          buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(calendar, &event);
        QCoreApplication::processEvents();
    };
    mouse(QEvent::MouseMove, centre(year + 1), Qt::NoButton);
    CHECK(near(pixel(centre(year + 1) + QPoint(-30, 0)), yearLayer(Role::OnSurfaceVariant, 0.08), 3));
    CHECK(is(pixel(centre(year + 1) + QPoint(-40, 0)), Role::SurfaceContainerHigh)); // only the pill
    CHECK(is(pixel(centre(year + 2) + QPoint(-30, 0)), Role::SurfaceContainerHigh));
    mouse(QEvent::MouseButtonPress, centre(year + 1), Qt::LeftButton);
    CHECK(near(pixel(centre(year + 1) + QPoint(-30, 0)), yearLayer(Role::OnSurfaceVariant, 0.10), 3));
    CHECK(!arrows.at(0)->isVisible()); // not chosen yet
    mouse(QEvent::MouseMove, centre(year + 4), Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, centre(year + 4), Qt::NoButton);
    CHECK(!arrows.at(0)->isVisible()); // released on another year: none is chosen
    CHECK(is(pixel(centre(year + 1) + QPoint(-30, 0)), Role::SurfaceContainerHigh));
    CHECK(near(pixel(centre(year + 4) + QPoint(-30, 0)), yearLayer(Role::OnSurfaceVariant, 0.08), 3));
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(calendar, &leave);
    QCoreApplication::processEvents();
    CHECK(is(pixel(centre(year + 4) + QPoint(-30, 0)), Role::SurfaceContainerHigh));
    // After the last year there is nothing to click.
    click(calendar, QPoint(20, top + 7 * 40 - 4));
    CHECK(calendar->yearShown() == year - 1);
    CHECK(!arrows.at(0)->isVisible());
    // The wheel scrolls the rows.
    QWheelEvent up(QPointF(140, top + 100), QPointF(140, top + 100), QPoint(0, 0), QPoint(0, 240), Qt::NoButton,
                   Qt::NoModifier);
    QCoreApplication::sendEvent(calendar, &up);
    QCoreApplication::processEvents();
    scrolled = 5;
    CHECK(is(pixel(centre(year - 1) + QPoint(-30, 0)), Role::Primary));
    // A click on a year shows the days of that year, in the same month.
    click(calendar, centre(year + 2));
    CHECK(arrows.at(0)->isVisible() && arrows.at(1)->isVisible());
    CHECK(calendar->yearShown() == year + 2 && calendar->monthShown() == 1);
    CHECK(calendar->selectedDate() == QDate(year - 1, 1, 20)); // the selection stays
    CHECK(is(pixel(QPoint(2, top + 2)), Role::SurfaceContainerHigh));

    // Escape goes back to the days, with nothing changed.
    click(title, title->rect().center());
    CHECK(!arrows.at(0)->isVisible());
    key(Qt::Key_Escape);
    CHECK(arrows.at(0)->isVisible());
    CHECK(calendar->yearShown() == year + 2 && calendar->monthShown() == 1);
    // A second click on the month and the year goes back too.
    click(title, title->rect().center());
    click(title, title->rect().center());
    CHECK(arrows.at(0)->isVisible());

    // The keyboard: Home goes to the first year; a month before the minimum becomes that of the minimum.
    click(title, title->rect().center());
    key(Qt::Key_Home);
    key(Qt::Key_Return);
    CHECK(calendar->yearShown() == year - 25 && calendar->monthShown() == 3);
    // And a month after the maximum that of the maximum.
    calendar->setCurrentPage(year - 1, 11);
    click(title, title->rect().center());
    key(Qt::Key_End);
    key(Qt::Key_Left);
    key(Qt::Key_Right);
    key(Qt::Key_Enter);
    CHECK(calendar->yearShown() == year + 10 && calendar->monthShown() == 9);
    // The arrow keys move by a year and by a row.
    click(title, title->rect().center());
    key(Qt::Key_Up);
    key(Qt::Key_Left);
    key(Qt::Key_Space);
    CHECK(calendar->yearShown() == year + 6 && calendar->monthShown() == 9);

    // In the dark, the same roles.
    style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Dark);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    QCoreApplication::processEvents();
    CHECK(is(pixel(QPoint(2, 2)), Role::SurfaceContainerHigh));
    CHECK(is(pixel(QPoint(140, 120)), Role::OutlineVariant));
    calendar->setCurrentPage(year - 1, 1);
    click(title, title->rect().center());
    scrolled = 7;
    CHECK(is(pixel(centre(year - 1) + QPoint(-30, 0)), Role::Primary));
    CHECK(is(pixel(centre(year + 1) + QPoint(-30, 0)), Role::SurfaceContainerHigh));
    key(Qt::Key_Escape);

    // Without the property, the headline goes.
    calendar->setProperty("materialDatePicker", QVariant());
    CHECK(calendar->sizeHint() == QSize(7 * 40, bar + 7 * 40));
    calendar->resize(calendar->sizeHint());
    QCoreApplication::processEvents();
    CHECK(navigation->geometry().top() == 0);
    // Under another style the years are not there: the label is a label.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    const QSize fusion = calendar->sizeHint();
    calendar->setProperty("materialDatePicker", "modal");
    CHECK(calendar->sizeHint() == fusion);
    click(title, title->rect().center());
    CHECK(arrows.at(0)->isVisible());
}

// The input mode of the modal date picker: the pencil in the headline swaps the month for a text field.
// The days and the years cross-fade on the default effects spring while motion is enabled.
void testDatePickerYearsFade() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(400, 560);
    auto *calendar = new QCalendarWidget(&window);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    const int year = QDate::currentDate().year();
    calendar->setDateRange(QDate(year - 25, 3, 15), QDate(year + 10, 9, 10));
    calendar->setSelectedDate(QDate(year - 1, 1, 20));
    calendar->resize(calendar->sizeHint());
    window.show();
    QCoreApplication::processEvents();
    auto *title = calendar->findChild<QLabel *>();
    CHECK(title);
    if (!title)
        return;
    const int top = title->parentWidget()->height();
    // The year shown, in the second row of years, is a filled pill once the years have faded in.
    const int column = ((year - 1) - (year - 25)) % 3;
    const QPoint pill((7 * 40 * column / 3 + 7 * 40 * (column + 1) / 3) / 2 - 30, top + 8 + 52 + 26);
    const auto pixel = [&](const QPoint &point) { return calendar->grab().toImage().pixelColor(point.x(), point.y()); };
    const QColor primary = style->color(Role::Primary);
    const QColor container = style->color(Role::SurfaceContainerHigh);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(title->rect().center()), QPointF(title->rect().center()),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(title, &press);
    QCoreApplication::processEvents();
    // At first the years have hardly come in.
    CHECK(!near(pixel(pill), primary, 40));
    wait(600);
    CHECK(near(pixel(pill), primary, 3));
    // And they fade out again over the days.
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(calendar, &escape);
    QCoreApplication::processEvents();
    CHECK(!near(pixel(pill), container, 3));
    wait(600);
    CHECK(!near(pixel(pill), primary, 40));
    // Without motion the change is immediate.
    style->setMotionEnabled(false);
    QCoreApplication::sendEvent(title, &press);
    QCoreApplication::processEvents();
    CHECK(near(pixel(pill), primary, 3));
    QApplication::setStyle(QStringLiteral("Fusion"));
}

void testDatePickerInput() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QWidget window;
    window.resize(400, 560);
    auto *calendar = new QCalendarWidget(&window);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setProperty("materialDatePicker", "modal");
    const int year = QDate::currentDate().year();
    calendar->setDateRange(QDate(year - 25, 3, 15), QDate(year + 10, 9, 10));
    calendar->setSelectedDate(QDate(year - 1, 1, 20));
    calendar->resize(calendar->sizeHint());
    window.show();
    window.activateWindow();
    QCoreApplication::processEvents();
    int selections = 0;
    int clicks = 0;
    QList<QDate> activations;
    QObject::connect(calendar, &QCalendarWidget::selectionChanged, [&] { ++selections; });
    QObject::connect(calendar, &QCalendarWidget::clicked, [&](QDate) { ++clicks; });
    QObject::connect(calendar, &QCalendarWidget::activated, [&](QDate date) { activations.append(date); });
    QWidget *navigation = calendar->findChild<QLabel *>()->parentWidget();
    CHECK(calendar->findChild<QLineEdit *>() == nullptr); // made when it is first wanted

    const auto click = [&](const QPoint &at) {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), QPointF(at), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(calendar, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at), QPointF(at), Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QCoreApplication::sendEvent(calendar, &release);
        QCoreApplication::processEvents();
    };
    // Keys go through the window, to whatever has the focus.
    const auto key = [&](int code, const QString &text = QString()) {
        QWindowSystemInterface::handleKeyEvent(window.windowHandle(), QEvent::KeyPress, code, Qt::NoModifier, text);
        QWindowSystemInterface::handleKeyEvent(window.windowHandle(), QEvent::KeyRelease, code, Qt::NoModifier, text);
        QCoreApplication::processEvents();
    };
    const auto type = [&](const QString &text) {
        for (const QChar c : text)
            key(c.isDigit() ? Qt::Key_0 + c.digitValue() : Qt::Key_Slash, QString(c));
    };
    const auto clear = [&] {
        for (int i = 0; i < 12; ++i)
            key(Qt::Key_Backspace);
    };
    // The icon button is 48 square at the end of the row of the date, 12 from the right; the pencil is in its middle.
    const QPoint button(7 * 40 - 12 - 24, 120 - 12 - 20);
    const auto inked = [&](const QRect &area) {
        const QImage image = calendar->grab().toImage().copy(area);
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                count += near(image.pixelColor(x, y), style->color(Role::OnSurfaceVariant), 40) ? 1 : 0;
        return count;
    };
    const QRect icon(button - QPoint(12, 12), QSize(24, 24));
    CHECK(inked(icon) > 20);
    const QImage pencil = calendar->grab().toImage().copy(icon);

    // A click on it shows the field, "Date", with the selected date in the form that its supporting text shows, and
    // gives it the focus; the month and its bar go.
    click(button);
    auto *field = calendar->findChild<QLineEdit *>();
    CHECK(field && field->isVisible());
    if (!field)
        return;
    CHECK(field->property("materialLabel").toString() == QLatin1String("Date"));
    CHECK(!navigation->isVisible());
    CHECK(calendar->isNavigationBarVisible()); // the program's wish is kept
    CHECK(QApplication::focusWidget() == field);
    CHECK(field->inputMethodHints() == Qt::ImhDate); // digits and the separators, not digits alone
    // The field is 24 in from the sides, 16 under the headline.
    CHECK(field->geometry().left() == 24 && field->geometry().right() == 7 * 40 - 25);
    CHECK(field->geometry().top() == 121 + 16);
    const QString hint = field->property("materialSupportingText").toString();
    CHECK(hint.contains(QLatin1String("MM")) && hint.contains(QLatin1String("DD")) &&
          hint.contains(QLatin1String("YYYY")));
    QString format = hint;
    format.replace(QLatin1Char('D'), QLatin1Char('d')).replace(QLatin1Char('Y'), QLatin1Char('y'));
    const auto shown = [&](QDate date) { return QLocale().toString(date, format); };
    CHECK(field->text() == shown(QDate(year - 1, 1, 20)));
    CHECK(field->property("materialError").toString().isEmpty());
    // The pencil has become a calendar.
    CHECK(calendar->grab().toImage().copy(icon) != pencil);
    CHECK(inked(icon) > 20);
    CHECK(selections == 0 && clicks == 0);

    // A date typed whole selects itself, and the headline follows; while it is short of the pattern there is no error.
    const QRect headline(24, 68, 7 * 40 - 24 - 60, 40);
    const QImage before = calendar->grab().toImage().copy(headline);
    clear();
    const QString typed = shown(QDate(year + 2, 5, 6));
    type(typed.left(4));
    CHECK(field->property("materialError").toString().isEmpty());
    CHECK(calendar->selectedDate() == QDate(year - 1, 1, 20));
    type(typed.mid(4));
    CHECK(field->text() == typed);
    CHECK(calendar->selectedDate() == QDate(year + 2, 5, 6));
    CHECK(selections == 1 && clicks == 0);
    CHECK(field->property("materialError").toString().isEmpty());
    if (textIsDrawn())
        CHECK(calendar->grab().toImage().copy(headline) != before);
    // Return accepts it, as in Qt's own date edit.
    key(Qt::Key_Return);
    CHECK(activations.size() == 1 && activations.value(0) == QDate(year + 2, 5, 6));

    // A date that cannot be shows the error, and the selection stays; Return does not accept it.
    clear();
    QString impossible = format; // the 45th day of a 13th month
    impossible.replace(QLatin1String("MM"), QLatin1String("13"))
        .replace(QLatin1String("dd"), QLatin1String("45"))
        .replace(QLatin1String("yyyy"), QString::number(year));
    type(impossible);
    CHECK(!field->property("materialError").toString().isEmpty());
    CHECK(calendar->selectedDate() == QDate(year + 2, 5, 6));
    key(Qt::Key_Return);
    CHECK(activations.size() == 1);
    CHECK(!field->property("materialError").toString().isEmpty());
    // So does a date out of the range, before or after it.
    clear();
    type(shown(QDate(year + 11, 1, 1)));
    CHECK(field->property("materialError").toString().contains(QLatin1String("range")));
    CHECK(calendar->selectedDate() == QDate(year + 2, 5, 6));
    clear();
    type(shown(QDate(year - 25, 3, 14)));
    CHECK(field->property("materialError").toString().contains(QLatin1String("range")));
    clear();
    type(shown(QDate(year - 25, 3, 15))); // the first day of the range is in it
    CHECK(field->property("materialError").toString().isEmpty());
    CHECK(calendar->selectedDate() == QDate(year - 25, 3, 15));
    CHECK(selections == 2);
    // A date selected some other way shows in the field.
    calendar->setSelectedDate(QDate(year, 7, 4));
    CHECK(field->text() == shown(QDate(year, 7, 4)));
    CHECK(selections == 3);

    // The calendar goes back to the days, with what was typed if it is a date of the range.
    clear();
    type(shown(QDate(year + 1, 2, 3)));
    clear(); // what is left is not a date, and the selection stays as typed last
    type(shown(QDate(year + 1, 2, 3)).left(3));
    // The days cannot be clicked while the field is in their place.
    const QPoint day(140, calendar->height() - 60);
    click(day);
    CHECK(clicks == 0);
    click(button);
    CHECK(!field->isVisible());
    CHECK(navigation->isVisible());
    CHECK(calendar->selectedDate() == QDate(year + 1, 2, 3));
    CHECK(calendar->yearShown() == year + 1 && calendar->monthShown() == 2);
    CHECK(QApplication::focusWidget() == calendar);
    CHECK(calendar->grab().toImage().copy(icon) == pencil);
    // The days can be clicked again.
    click(day);
    CHECK(clicks == 1);
    calendar->setSelectedDate(QDate(year + 1, 2, 3));

    // Text that is a date when the calendar goes back selects it, without Return.
    click(button);
    CHECK(field->isVisible() && field->text() == shown(QDate(year + 1, 2, 3)));
    CHECK(field->property("materialError").toString().isEmpty());
    calendar->setSelectedDate(QDate(year + 1, 2, 4)); // as if typed with no signal from the field
    field->setText(shown(QDate(year + 3, 8, 9)));
    selections = 0;
    click(button);
    CHECK(calendar->selectedDate() == QDate(year + 3, 8, 9));
    CHECK(selections == 1);
    // Text that is not leaves the selection alone.
    click(button);
    field->setText(QStringLiteral("x"));
    click(button);
    CHECK(calendar->selectedDate() == QDate(year + 3, 8, 9));
    CHECK(!field->isVisible());

    // Without the property the input mode goes with the headline, and the button with it.
    click(button);
    CHECK(field->isVisible());
    calendar->setProperty("materialDatePicker", QVariant());
    QCoreApplication::processEvents();
    CHECK(!field->isVisible());
    CHECK(navigation->isVisible());
    calendar->resize(calendar->sizeHint());
    click(QPoint(7 * 40 - 36, 30));
    CHECK(!field->isVisible());
    // Under another style there is no headline, so no button either.
    calendar->setProperty("materialDatePicker", "modal");
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    calendar->resize(calendar->sizeHint());
    click(button);
    CHECK(!field->isVisible());
}

// The icon button of the modal date picker is a real one: Tab reaches it, Space and Return press it, it shows the
// pointer and the press with the layer of a standard icon button, and the picker is shorter in the input mode.
void testDatePickerModeButton() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QDialog dialog;
    auto *layout = new QVBoxLayout(&dialog);
    auto *calendar = new QCalendarWidget;
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setProperty("materialDatePicker", "modal");
    layout->addWidget(calendar);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    dialog.show();
    dialog.activateWindow();
    QCoreApplication::processEvents();
    QToolButton *mode = nullptr;
    for (QToolButton *each : calendar->findChildren<QToolButton *>())
        if (each->property("materialIconButton").toString() == QLatin1String("standard"))
            mode = each;
    CHECK(mode && mode->isVisible());
    if (!mode)
        return;
    // Where the headline's button is, 48 square, 12 from the right.
    CHECK(mode->geometry() == QRect(7 * 40 - 12 - 48, 120 - 12 - 20 - 24, 48, 48));
    QWindow *handle = dialog.windowHandle();
    const auto key = [&](int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QWindowSystemInterface::handleKeyEvent(handle, QEvent::KeyPress, code, modifiers);
        QWindowSystemInterface::handleKeyEvent(handle, QEvent::KeyRelease, code, modifiers);
        QCoreApplication::processEvents();
    };
    const auto mouse = [&](QEvent::Type type, const QPoint &local, Qt::MouseButtons held) {
        const QPointF at(mode->mapTo(&dialog, local));
        QWindowSystemInterface::handleMouseEvent(handle, at, at, held, Qt::LeftButton, type, Qt::NoModifier);
        QCoreApplication::processEvents();
    };
    // A point of the shape of the button, a circle of 40 in its middle, away from the icon.
    const QPoint inShape(10, 24);
    const auto layered = [&](double opacity) {
        const QColor under = style->color(Role::SurfaceContainerHigh);
        const QColor ink = style->color(Role::OnSurfaceVariant);
        const auto mix = [&](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * opacity)); };
        return QColor(mix(under.red(), ink.red()), mix(under.green(), ink.green()), mix(under.blue(), ink.blue()));
    };
    // At rest it is the headline with the pencil on it, as before.
    CHECK(near(pixel(*mode, inShape.x(), inShape.y()), style->color(Role::SurfaceContainerHigh), 3));

    // Tab from the days comes to it first, with the focus ring; Space switches to the input mode.
    calendar->setFocus();
    key(Qt::Key_Tab);
    CHECK(QApplication::focusWidget() == mode);
    CHECK(near(pixel(*mode, 4 + 1, 24), style->color(Role::Secondary), 3));
    const int calendarHeight = calendar->sizeHint().height();
    const int dialogHeight = dialog.height();
    key(Qt::Key_Space);
    auto *field = calendar->findChild<QLineEdit *>();
    CHECK(field && field->isVisible());
    if (!field)
        return;
    CHECK(QApplication::focusWidget() == field);
    // The picker is the headline and the field now, and the dialog shrinks with it.
    const int inputHeight = calendar->sizeHint().height();
    CHECK(inputHeight < calendarHeight);
    CHECK(inputHeight == 121 + 16 + field->sizeHint().height() + 8);
    CHECK(calendar->minimumSizeHint().height() == inputHeight);
    QCoreApplication::processEvents();
    CHECK(dialog.height() == dialogHeight - (calendarHeight - inputHeight));
    CHECK(field->geometry().bottom() < calendar->height());
    // Shift+Tab goes back to the button, and Return switches back; the dialog grows back.
    key(Qt::Key_Tab, Qt::ShiftModifier);
    CHECK(QApplication::focusWidget() == mode);
    key(Qt::Key_Return);
    CHECK(!field->isVisible());
    CHECK(calendar->sizeHint().height() == calendarHeight);
    QCoreApplication::processEvents();
    CHECK(dialog.height() == dialogHeight);
    CHECK(QApplication::focusWidget() == mode);
    key(Qt::Key_Enter);
    CHECK(field->isVisible());
    key(Qt::Key_Tab, Qt::ShiftModifier);
    key(Qt::Key_Space);
    CHECK(!field->isVisible());

    // The pointer over it shows the layer of 0.08, and a press that of 0.10; the click switches the mode.
    calendar->setFocus();
    QCoreApplication::processEvents();
    CHECK(near(pixel(*mode, inShape.x(), inShape.y()), style->color(Role::SurfaceContainerHigh), 3));
    mouse(QEvent::MouseMove, QPoint(24, 24), Qt::NoButton);
    CHECK(mode->underMouse());
    CHECK(near(pixel(*mode, inShape.x(), inShape.y()), layered(0.08), 3));
    mouse(QEvent::MouseButtonPress, QPoint(24, 24), Qt::LeftButton);
    CHECK(mode->isDown());
    CHECK(near(pixel(*mode, inShape.x(), inShape.y()), layered(0.10), 3));
    mouse(QEvent::MouseButtonRelease, QPoint(24, 24), Qt::NoButton);
    CHECK(field->isVisible());
    // A click on the button keeps no focus ring: it was not the keyboard.
    QCoreApplication::processEvents();
    CHECK(!near(pixel(*mode, 4 + 1, 24), style->color(Role::Secondary), 3));
    mouse(QEvent::MouseButtonPress, QPoint(24, 24), Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, QPoint(24, 24), Qt::NoButton);
    CHECK(!field->isVisible());

    // Without the headline there is no button.
    calendar->setProperty("materialDatePicker", QVariant());
    QCoreApplication::processEvents();
    CHECK(!mode->isVisible());
    calendar->setProperty("materialDatePicker", "modal");
    QCoreApplication::processEvents();
    CHECK(mode->isVisible());
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(!mode->isVisible());
}
// The five sizes of the slider, and the value indicator over its handle.
void testSliderSizesAndValue() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false); // the pixels are those of the shapes at rest
    QApplication::setStyle(style);

    const struct {
        const char *name;
        int track, handle;
    } sizes[] = {{nullptr, 16, 44},  {"extraSmall", 16, 44}, {"small", 24, 44},
                 {"medium", 40, 52}, {"large", 56, 68},      {"extraLarge", 96, 108}};
    for (const auto &size : sizes) {
        QWidget window;
        auto *layout = new QHBoxLayout(&window);
        auto *column = new QVBoxLayout;
        layout->addLayout(column);
        auto *across = new QSlider(Qt::Horizontal);
        auto *upright = new QSlider(Qt::Vertical);
        for (QSlider *s : {across, upright}) {
            s->setRange(0, 100);
            s->setValue(50);
            if (size.name)
                s->setProperty("materialSliderSize", QString::fromLatin1(size.name));
        }
        column->addWidget(across);
        column->addStretch(1);
        layout->addWidget(upright);
        window.resize(360, 300);
        window.show();
        QCoreApplication::processEvents();

        // As thick as the handle, which a press takes hold of across its whole length.
        CHECK(across->sizeHint().height() == size.handle);
        CHECK(upright->sizeHint().width() == size.handle);
        CHECK(across->height() == size.handle);
        QStyleOptionSlider option;
        option.initFrom(across);
        option.orientation = Qt::Horizontal;
        option.minimum = 0;
        option.maximum = 100;
        option.sliderPosition = 50;
        const QRect handle = style->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, across);
        CHECK(handle.height() == size.handle);
        CHECK(std::abs(handle.center().x() - across->width() / 2) <= 2);

        // The track: as thick as its size, the active part before the handle and the inactive one after it.
        const int w = across->width(), y = across->height() / 2;
        CHECK(near(pixel(*across, w / 4, y), style->color(Role::Primary), 2));
        CHECK(near(pixel(*across, w / 4, y - size.track / 2 + 2), style->color(Role::Primary), 2));
        CHECK(near(pixel(*across, w / 4, y + size.track / 2 - 2), style->color(Role::Primary), 2));
        CHECK(near(pixel(*across, w / 4, y - size.track / 2 - 2), style->color(Role::Surface), 2));
        CHECK(near(pixel(*across, 3 * w / 4, y - size.track / 2 + 2), style->color(Role::SecondaryContainer), 2));
        CHECK(near(pixel(*across, w / 2, y - size.handle / 2 + 2), style->color(Role::Primary), 3)); // the handle
        // A vertical one: active below the handle, and as thick.
        const int h = upright->height(), x = upright->width() / 2;
        CHECK(near(pixel(*upright, x - size.track / 2 + 2, 3 * h / 4), style->color(Role::Primary), 2));
        CHECK(near(pixel(*upright, x - size.track / 2 - 2, 3 * h / 4), style->color(Role::Surface), 2));
        CHECK(near(pixel(*upright, x + size.track / 2 - 2, h / 4), style->color(Role::SecondaryContainer), 2));
        // Disabled, the track is a faint one of the same thickness.
        across->setEnabled(false);
        const QColor faint = pixel(*across, w / 4, y - size.track / 2 + 2);
        CHECK(!near(faint, style->color(Role::Primary), 20) && !near(faint, style->color(Role::Surface), 2));
        CHECK(near(pixel(*across, w / 4, y - size.track / 2 - 2), style->color(Role::Surface), 2));
    }

    // The value indicator: room is kept for it over a slider with tick marks, or one that asks for it.
    for (Qt::ColorScheme scheme : {Qt::ColorScheme::Light, Qt::ColorScheme::Dark}) {
        style->setColorScheme(scheme);
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *steps = new QSlider(Qt::Horizontal);
        steps->setRange(0, 10);
        steps->setValue(5);
        steps->setTickPosition(QSlider::TicksBelow);
        auto *plain = new QSlider(Qt::Horizontal);
        plain->setRange(0, 1000);
        plain->setValue(0);
        auto *quiet = new QSlider(Qt::Horizontal);
        quiet->setTickPosition(QSlider::TicksBelow);
        quiet->setProperty("materialSliderValue", false);
        auto *upright = new QSlider(Qt::Vertical);
        upright->setRange(0, 10);
        upright->setTickPosition(QSlider::TicksBothSides);
        for (QWidget *w : std::initializer_list<QWidget *>{steps, plain, quiet, upright})
            layout->addWidget(w);
        layout->addStretch(1);
        window.resize(320, 520);
        window.show();
        QCoreApplication::processEvents();

        CHECK(steps->sizeHint().height() >= 44 + 48);
        CHECK(plain->sizeHint().height() == 44);
        CHECK(quiet->sizeHint().height() < 44 + 48);
        CHECK(upright->sizeHint().width() >= 44 + 52);
        plain->setProperty("materialSliderValue", true);
        QCoreApplication::processEvents();
        CHECK(plain->sizeHint().height() == 44 + 48);
        CHECK(plain->height() == 44 + 48);

        // Shown while the handle is pressed, above it and whole, and gone when it is let go.
        const QColor bubble = style->color(Role::InverseSurface);
        const QColor surface = style->color(Role::Surface);
        const auto handleOf = [&](QSlider *s) {
            QStyleOptionSlider option;
            option.initFrom(s);
            option.orientation = s->orientation();
            option.minimum = s->minimum();
            option.maximum = s->maximum();
            option.sliderPosition = s->sliderPosition();
            option.tickPosition = s->tickPosition();
            return style->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, s);
        };
        const auto bubbleShows = [&](QSlider *s, int bx, int by) { return near(pixel(*s, bx, by), bubble, 3); };
        constexpr int belowText = 15; // from the middle of the bubble, clear of the value written in it
        // Nothing from the keys of an earlier round: a click shows no focus ring.
        QMouseEvent click(QEvent::MouseButtonPress, QPointF(1, 1), QPointF(1, 1), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &click);
        QRect handle = handleOf(steps);
        int sx = handle.center().x();
        int sy = handle.top() - 4 - 44 / 2; // the middle of the bubble
        CHECK(sy - 44 / 2 >= 0);
        CHECK(!bubbleShows(steps, sx, sy + belowText));
        const QPoint at = handle.center();
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), steps->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(steps, &press);
        CHECK(steps->isSliderDown());
        CHECK(bubbleShows(steps, sx, sy + belowText));
        // Not clipped: its round top and its sides are in the slider.
        CHECK(bubbleShows(steps, sx, sy - 44 / 2 + 3));
        CHECK(bubbleShows(steps, sx - 20, sy));
        CHECK(bubbleShows(steps, sx + 20, sy));
        CHECK(near(pixel(*steps, sx - 28, sy), surface, 3));
        if (textIsDrawn()) {
            // The value is written in it, in the color for text on it.
            bool written = false;
            for (int dy = -10; dy <= 10; ++dy)
                for (int dx = -12; dx <= 12; ++dx)
                    written =
                        written || near(pixel(*steps, sx + dx, sy + dy), style->color(Role::InverseOnSurface), 60);
            CHECK(written);
        }
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at), steps->mapToGlobal(at), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(steps, &release);
        CHECK(!steps->isSliderDown());
        CHECK(near(pixel(*steps, sx, sy + belowText), surface, 3));

        // At an end it stays inside the slider.
        plain->setSliderDown(true);
        const int py = handleOf(plain).top() - 4 - 44 / 2;
        CHECK(bubbleShows(plain, 3, py));
        CHECK(bubbleShows(plain, 24, py - 44 / 2 + 2));
        plain->setSliderDown(false);
        CHECK(!bubbleShows(plain, 24, py));
        // One that does not want it shows none.
        quiet->setSliderDown(true);
        CHECK(quiet->height() < 44 + 48);
        quiet->setSliderDown(false);

        // Changed with the keys, it shows while the slider has the focus.
        steps->setFocus();
        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QCoreApplication::sendEvent(steps, &right);
        CHECK(steps->value() == 6);
        sx = handleOf(steps).center().x();
        CHECK(bubbleShows(steps, sx, sy + belowText));
        plain->setFocus();
        CHECK(!bubbleShows(steps, sx, sy + belowText));

        // A vertical slider shows it at the left of the handle.
        upright->setValue(5);
        upright->setSliderDown(true);
        handle = handleOf(upright);
        CHECK(handle.left() - 4 - 48 >= 0);
        CHECK(bubbleShows(upright, handle.left() - 4 - 24, handle.center().y() + belowText));
        upright->setSliderDown(false);
        CHECK(!bubbleShows(upright, handle.left() - 4 - 24, handle.center().y() + belowText));
    }
}

// A navigation rail whose destinations do not fit its height scrolls them: the wheel and a drag move them, and the
// current one is scrolled into sight. There are no scroll buttons.
void testRailScrolls() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    QPixmap square(24, 24);
    square.fill(Qt::black);
    const QIcon icon(square);

    QWidget window;
    auto *pages = new QTabWidget(&window);
    pages->setTabPosition(QTabWidget::West);
    QTabBar *bar = pages->tabBar();
    bar->setProperty("materialTabBar", QStringLiteral("navigationRail"));
    for (int i = 0; i < 10; ++i)
        pages->addTab(new QWidget, icon, QStringLiteral("Page %1").arg(i));
    pages->setGeometry(0, 0, 600, 400);
    window.resize(600, 400);
    window.show();
    QCoreApplication::processEvents();
    CHECK(bar->height() == 400 && bar->tabRect(0) == QRect(0, 44, 96, 64));
    CHECK(style->styleHint(QStyle::SH_TabBar_PreferNoArrows, nullptr, bar) == 1);
    QTabBar plain;
    CHECK(style->styleHint(QStyle::SH_TabBar_PreferNoArrows, nullptr, &plain) == 0);

    // 44 + 10 destinations of 64 with 4 between them reach 720: the rail scrolls by 320, a destination a notch.
    const auto wheel = [&](int notches) {
        QWheelEvent event(QPointF(48, 200), QPointF(48, 200), QPoint(), QPoint(0, 120 * notches), Qt::NoButton,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(bar, &event);
        return event.isAccepted();
    };
    CHECK(wheel(-1));
    CHECK(bar->tabRect(0).top() == 44 - 68 && bar->currentIndex() == 0); // it scrolls, and the current one stays
    CHECK(bar->tabAt(QPoint(48, 44)) == 1);
    CHECK(wheel(-10));
    CHECK(bar->tabRect(9).bottom() + 1 == 400);
    CHECK(!wheel(-1)); // at the end: it goes to what holds the rail
    CHECK(wheel(10));
    CHECK(bar->tabRect(0).top() == 44);
    CHECK(!wheel(1));

    // The destination that becomes current is scrolled into sight, the last one with the end, the first with the room
    // above it.
    bar->setCurrentIndex(9);
    CHECK(bar->tabRect(9).bottom() + 1 == 400);
    {
        const QImage image = window.grab().toImage();
        const QRect last = bar->tabRect(9);
        const QPoint origin = bar->mapTo(&window, QPoint(0, 0));
        CHECK(near(image.pixelColor(origin.x() + 24, origin.y() + last.top() + 22),
                   style->color(Role::SecondaryContainer), 3)); // its pill is drawn where it now is
    }
    bar->setCurrentIndex(3);
    CHECK(bar->tabRect(3).top() >= 0 && bar->tabRect(3).bottom() < 400);
    bar->setCurrentIndex(0);
    CHECK(bar->tabRect(0).top() == 44);
    QKeyEvent end(QEvent::KeyPress, Qt::Key_End, Qt::NoModifier);
    QCoreApplication::sendEvent(bar, &end);
    CHECK(bar->currentIndex() == 9 && bar->tabRect(9).bottom() + 1 == 400);

    // A drag, as of a finger, scrolls; a press that does not move chooses at the release.
    const auto mouse = [&](QEvent::Type type, int y, Qt::MouseButtons buttons) {
        const QPointF at(48, y);
        QMouseEvent event(type, at, at, Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(bar, &event);
    };
    const int before = bar->tabRect(0).top();
    mouse(QEvent::MouseButtonPress, 100, Qt::LeftButton);
    mouse(QEvent::MouseMove, 150, Qt::LeftButton);
    mouse(QEvent::MouseMove, 200, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, 200, Qt::NoButton);
    CHECK(bar->tabRect(0).top() == before + 100 && bar->currentIndex() == 9);
    const int target = bar->tabAt(QPoint(48, 200));
    CHECK(target >= 0 && target != 9);
    mouse(QEvent::MouseButtonPress, 200, Qt::LeftButton);
    CHECK(bar->currentIndex() == 9);
    mouse(QEvent::MouseButtonRelease, 200, Qt::NoButton);
    CHECK(bar->currentIndex() == target && pages->currentIndex() == target);

    // The expanded rail, with rows of 56, scrolls in the same way.
    bar->setProperty("materialTabBar", QStringLiteral("navigationRailExpanded"));
    QCoreApplication::processEvents();
    CHECK(wheel(-20));
    CHECK(bar->tabRect(9).bottom() + 1 == 400 && bar->tabRect(9).height() == 56);
    bar->setProperty("materialTabBar", QStringLiteral("navigationRail"));
    QCoreApplication::processEvents();

    // Once they fit, the destinations are where they were, and the wheel steps through them, as in Qt.
    window.resize(600, 800);
    pages->setGeometry(0, 0, 600, 800);
    QCoreApplication::processEvents();
    CHECK(bar->tabRect(0) == QRect(0, 44, 96, 64));
    bar->setCurrentIndex(4);
    CHECK(wheel(-1) && bar->currentIndex() == 5 && bar->tabRect(0).top() == 44);
    mouse(QEvent::MouseButtonPress, 44 + 10, Qt::LeftButton); // and a press chooses at once
    CHECK(bar->currentIndex() == 0);
    mouse(QEvent::MouseButtonRelease, 44 + 10, Qt::NoButton);
}

void testExpandedRail() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };
    QPixmap square(24, 24);
    square.fill(Qt::black);
    const QIcon icon(square);

    QMainWindow window;
    auto *pages = new QTabWidget;
    pages->setTabPosition(QTabWidget::West);
    QTabBar *bar = pages->tabBar();
    bar->setProperty("materialTabBar", QStringLiteral("navigationRailExpanded"));
    for (const char *name : {"Home", "Search", "Settings"})
        pages->addTab(new QWidget, icon, QString::fromLatin1(name));
    window.setCentralWidget(pages);
    auto *status = new QStatusBar;
    status->setProperty("materialStatusBar", QStringLiteral("snackbar"));
    window.setStatusBar(status);
    window.resize(600, 400);
    window.show();
    QCoreApplication::processEvents();

    // 220 across for short labels, with rows of 56 one under another from 44 down, in label large.
    CHECK(bar->width() == 220 && bar->height() == 400);
    CHECK(bar->tabRect(0) == QRect(0, 44, 220, 56));
    CHECK(bar->tabRect(1) == QRect(0, 104, 220, 56));
    CHECK(bar->tabRect(2) == QRect(0, 164, 220, 56));
    CHECK(bar->font().pixelSize() == 14 && bar->font().weight() == QFont::Medium);
    const auto shot = [&] { return window.grab().toImage(); };
    QImage image = shot();
    const QPoint origin = bar->mapTo(&window, QPoint(0, 0));
    const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
    // The pill of the one shown spans the row, 20 from each side.
    CHECK(is(at(10, 44 + 28), Role::Surface));
    CHECK(is(at(24, 44 + 28), Role::SecondaryContainer));
    CHECK(is(at(195, 44 + 28), Role::SecondaryContainer));
    CHECK(is(at(205, 44 + 28), Role::Surface));
    CHECK(is(at(48, 44 + 4), Role::SecondaryContainer));
    CHECK(is(at(48, 44 - 2), Role::Surface));
    CHECK(is(at(24, 104 + 28), Role::Surface));
    CHECK(is(at(195, 104 + 28), Role::Surface));
    // The icon 16 into the pill, and the label 8 after it.
    CHECK(is(at(48, 44 + 28), Role::OnSecondaryContainer));
    CHECK(is(at(48, 104 + 28), Role::OnSurfaceVariant));
    CHECK(is(at(64, 44 + 28), Role::SecondaryContainer));
    if (textIsDrawn()) {
        const auto inked = [&](const QRect &area, Role role) {
            int count = 0;
            for (int y = area.top(); y <= area.bottom(); ++y)
                for (int x = area.left(); x <= area.right(); ++x)
                    count += near(at(x, y), style->color(role), 40) ? 1 : 0;
            return count;
        };
        CHECK(inked(QRect(68, 44 + 16, 60, 24), Role::OnSecondaryContainer) > 10);
        CHECK(inked(QRect(68, 104 + 16, 60, 24), Role::OnSurfaceVariant) > 10);
        CHECK(inked(QRect(140, 44 + 16, 50, 24), Role::OnSecondaryContainer) == 0); // "Home" is short
    }
    // A click on a row goes to its destination.
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(180, 104 + 28), QPointF(180, 104 + 28), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(bar, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(180, 104 + 28), QPointF(180, 104 + 28), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(bar, &release);
    CHECK(pages->currentIndex() == 1);
    QCoreApplication::processEvents();
    image = shot();
    CHECK(is(at(24, 104 + 28), Role::SecondaryContainer));
    CHECK(is(at(24, 44 + 28), Role::Surface));

    // A snackbar keeps beside it, and moves when it collapses.
    status->showMessage(QStringLiteral("Saved"));
    QCoreApplication::processEvents();
    CHECK(status->x() == 220 + 16 && status->width() == 600 - 220 - 32);
    bar->setProperty("materialTabBar", QStringLiteral("navigationRail"));
    QCoreApplication::processEvents();
    CHECK(bar->width() == 96 && bar->tabRect(1) == QRect(0, 112, 96, 64));
    CHECK(bar->font().pixelSize() == 12);
    CHECK(status->x() == 96 + 16 && status->width() == 600 - 96 - 32);
    bar->setProperty("materialTabBar", QStringLiteral("navigationRailExpanded"));
    QCoreApplication::processEvents();
    CHECK(bar->width() == 220 && bar->tabRect(1) == QRect(0, 104, 220, 56));
    CHECK(bar->font().pixelSize() == 14);
    CHECK(status->x() == 220 + 16);
    status->clearMessage();

    // A dot on the icon, a count at the end of the row.
    bar->setProperty("materialBadges", QVariantList{true, 3, 0});
    QCoreApplication::processEvents();
    image = shot();
    const auto red = [&](const QRect &area) {
        int count = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                count += near(at(x, y), style->color(Role::Error), 6) ? 1 : 0;
        return count;
    };
    CHECK(is(at(57, 44 + 18), Role::Error));   // at the top right of the icon
    CHECK(red(QRect(20, 44, 180, 56)) < 40);   // and nowhere else in the row
    CHECK(is(at(171, 104 + 28), Role::Error)); // 16 from the end of the pill
    CHECK(red(QRect(20, 104, 100, 56)) == 0);  // not on the icon
    CHECK(red(QRect(0, 164, 220, 56)) == 0);

    // A longer label widens the rail, up to 360.
    QTabBar lone;
    lone.setShape(QTabBar::RoundedWest);
    lone.setProperty("materialTabBar", QStringLiteral("navigationRailExpanded"));
    lone.addTab(icon, QStringLiteral("A destination with a long name"));
    lone.show();
    QCoreApplication::processEvents();
    const int wanted = lone.fontMetrics().horizontalAdvance(lone.tabText(0)) + 24 + 8 + 2 * (20 + 16);
    CHECK(lone.sizeHint().width() == std::clamp(wanted, 220, 360));
    CHECK(lone.sizeHint().height() == 44 + 56);
    if (textIsDrawn())
        CHECK(lone.sizeHint().width() > 220);
    lone.addTab(icon, QStringLiteral("A destination whose name is far too long to be shown in full in a rail"));
    CHECK(lone.sizeHint().width() ==
          std::min(360, std::max(wanted, lone.fontMetrics().horizontalAdvance(lone.tabText(1)) + 104)));
    if (textIsDrawn())
        CHECK(lone.sizeHint().width() == 360);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// The medium and large flexible app bars: their title on a line under the buttons, collapsing toward the small bar as
// far as the list under them is scrolled.
void testFlexibleAppBars() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QImage square(48, 48, QImage::Format_ARGB32_Premultiplied);
    square.fill(Qt::transparent);
    {
        QPainter painter(&square);
        painter.fillRect(QRect(8, 8, 32, 32), Qt::black);
    }
    const QIcon icon(QPixmap::fromImage(square));
    for (const char *kind : {"appBarMedium", "appBarLarge"}) {
        const bool large = QByteArray(kind) == "appBarLarge";
        const int expanded = large ? 120 : 112;
        QMainWindow window;
        auto *bar = new QToolBar;
        bar->setProperty("materialToolBar", QString::fromLatin1(kind));
        bar->setWindowTitle(QStringLiteral("Inbox"));
        QAction *back = bar->addAction(icon, QStringLiteral("Back"));
        bar->addSeparator();
        QAction *more = bar->addAction(icon, QStringLiteral("More"));
        window.addToolBar(bar);
        auto *list = new QListWidget;
        for (int i = 0; i < 100; ++i)
            list->addItem(QStringLiteral("Message %1").arg(i));
        window.setCentralWidget(list);
        window.resize(360, 480);
        window.show();
        QCoreApplication::processEvents();

        // Expanded: its height across the window, the buttons in the top row of 64, the list right under it.
        CHECK(bar->sizeHint().height() == expanded);
        CHECK(bar->height() == expanded && bar->width() == 360);
        const QWidget *backButton = bar->widgetForAction(back);
        const QWidget *moreButton = bar->widgetForAction(more);
        CHECK(backButton->geometry() == QRect(4, 8, 48, 48));
        CHECK(moreButton->y() == 8 && moreButton->x() + moreButton->width() == 360 - 4);
        CHECK(list->mapTo(&window, QPoint(0, 0)).y() == expanded);

        // Ink of the title in the row of the buttons, beside the leading one, and on the line below it.
        const auto ink = [&](int left, int top, int bottom) {
            int found = 0;
            const QImage image = bar->grab().toImage();
            for (int y = top; y < std::min(bottom, image.height()); ++y)
                for (int x = left; x < 200; ++x)
                    found += image.pixelColor(x, y).lightness() < 128 ? 1 : 0;
            return found;
        };
        const auto rowInk = [&] { return ink(56, 20, 44); };
        const auto lineInk = [&] { return ink(16, 64, bar->height()); };
        if (textIsDrawn()) {
            CHECK(rowInk() == 0);
            CHECK(lineInk() > 40);
        }

        // Scrolled a little, it collapses all the way: it does not follow the scrolling.
        list->verticalScrollBar()->setValue(10);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
        CHECK(list->mapTo(&window, QPoint(0, 0)).y() == 64);
        CHECK(list->verticalScrollBar()->value() == 10);
        CHECK(backButton->geometry() == QRect(4, 8, 48, 48));
        // Scrolled further than the difference, it is the small bar, with the small title in the row.
        list->verticalScrollBar()->setValue(200);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
        CHECK(list->mapTo(&window, QPoint(0, 0)).y() == 64);
        CHECK(backButton->geometry() == QRect(4, 8, 48, 48));
        CHECK(near(pixel(*bar, 300, 60), style->color(Role::SurfaceContainer), 3));
        if (textIsDrawn())
            CHECK(rowInk() > 40);
        // Back at the top it is expanded again.
        list->verticalScrollBar()->setValue(0);
        QCoreApplication::processEvents();
        CHECK(bar->height() == expanded);
        CHECK(list->mapTo(&window, QPoint(0, 0)).y() == expanded);
        CHECK(near(pixel(*bar, 300, expanded - 4), style->color(Role::Surface), 3));
        if (textIsDrawn()) {
            CHECK(rowInk() == 0);
            CHECK(lineInk() > 40);
        }

        // With motion it gets there along a spring.
        style->setMotionEnabled(true);
        QCoreApplication::processEvents();
        list->verticalScrollBar()->setValue(200);
        QCoreApplication::processEvents();
        CHECK(bar->height() > 64);
        wait(700);
        CHECK(bar->height() == 64);
        list->verticalScrollBar()->setValue(0);
        wait(700);
        CHECK(bar->height() == expanded);
        style->setMotionEnabled(false);
    }

    // The small one stays 64 high, scrolled or not.
    QMainWindow window;
    auto *bar = new QToolBar;
    bar->setProperty("materialToolBar", QStringLiteral("appBar"));
    bar->addAction(QStringLiteral("More"));
    window.addToolBar(bar);
    auto *list = new QListWidget;
    for (int i = 0; i < 100; ++i)
        list->addItem(QStringLiteral("Message %1").arg(i));
    window.setCentralWidget(list);
    window.resize(360, 480);
    window.show();
    QCoreApplication::processEvents();
    CHECK(bar->height() == 64 && bar->sizeHint().height() == 64);
    list->verticalScrollBar()->setValue(200);
    QCoreApplication::processEvents();
    CHECK(bar->height() == 64 && list->mapTo(&window, QPoint(0, 0)).y() == 64);
}

// Rows of two and three lines: a line break in the text of an item puts supporting text under its headline, and a tab
// in the headline a trailing supporting text at its end.
void testMultiLineListRows() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    const QStringList texts{QStringLiteral("Inbox"), QStringLiteral("Headline\nSupporting text"),
                            QStringLiteral("Headline\nFirst line\nSecond line"), QStringLiteral("Inbox\t100+")};
    QPixmap black(24, 24);
    black.fill(Qt::black);
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *widgetList = new QListWidget;
    widgetList->setIconSize(QSize(24, 24));
    for (const QString &text : texts)
        widgetList->addItem(new QListWidgetItem(QIcon(black), text));
    auto *model = new QStandardItemModel(&window);
    for (const QString &text : texts)
        model->appendRow(new QStandardItem(QIcon(black), text));
    auto *view = new QListView;
    view->setIconSize(QSize(24, 24));
    view->setModel(model);
    layout->addWidget(widgetList);
    layout->addWidget(view);
    window.resize(320, 700);
    window.show();
    QCoreApplication::processEvents();

    // The color of the pixel in `r` that is farthest from `background`: the ink of what is drawn there.
    const auto ink = [](const QImage &image, const QRect &r, const QColor &background) {
        QColor most = background;
        int far = -1;
        for (int y = r.top(); y <= r.bottom(); ++y)
            for (int x = r.left(); x <= r.right(); ++x) {
                const QColor c = image.pixelColor(x, y);
                const int d = std::abs(c.red() - background.red()) + std::abs(c.green() - background.green()) +
                              std::abs(c.blue() - background.blue());
                if (d > far) {
                    far = d;
                    most = c;
                }
            }
        return most;
    };

    for (const bool dark : {false, true}) {
        // The palettes of the widgets follow the scheme when the style is set.
        style = new QMaterialStyle;
        style->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
        style->setMotionEnabled(false);
        QApplication::setStyle(style);
        QCoreApplication::processEvents();
        for (QAbstractItemView *each :
             {static_cast<QAbstractItemView *>(widgetList), static_cast<QAbstractItemView *>(view)}) {
            const bool items = each == widgetList;
            const auto rowRect = [&](int n) {
                return items ? widgetList->visualItemRect(widgetList->item(n)) : view->visualRect(model->index(n, 0));
            };
            // One line 56, two 72, three 88, and a trailing text does not add one.
            CHECK(rowRect(0).height() == 56);
            CHECK(rowRect(1).height() == 72);
            CHECK(rowRect(2).height() == 88);
            CHECK(rowRect(3).height() == 56);
            QWidget *port = each->viewport();
            const QImage image = window.grab().toImage();
            const QPoint origin = port->mapTo(&window, QPoint(0, 0));
            const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
            const auto area = [&](const QRect &r) { return r.translated(origin); };
            const QColor surface = at(rowRect(0).right() - 4, rowRect(0).top() + 4);
            const QColor iconInk = style->color(Role::OnSurfaceVariant);

            // The icon is in the middle of a row of one or two lines, 16 in, and at the top of a row of three, beside
            // the headline.
            for (const int n : {0, 1, 3})
                CHECK(near(at(16 + 12, rowRect(n).center().y()), iconInk, 3));
            CHECK(near(at(15, rowRect(1).center().y()), surface, 3));
            const QRect three = rowRect(2);
            CHECK(near(at(16 + 12, three.top() + 12 + 2), iconInk, 3));
            CHECK(near(at(16 + 12, three.top() + 12 + 22), iconInk, 3));
            CHECK(near(at(16 + 12, three.top() + 12 + 28), surface, 3));
            CHECK(near(at(16 + 12, three.center().y()), surface, 3));

            if (!textIsDrawn())
                continue;
            const int textLeft = 16 + 24 + 12;
            const QColor headline = style->color(Role::OnSurface);
            const QColor supporting = style->color(Role::OnSurfaceVariant);
            // A single line in the middle of its row, as it was.
            const QRect one = rowRect(0);
            CHECK(near(ink(image, area(QRect(textLeft, one.top() + 18, 60, 20)), surface), headline, 12));
            CHECK(near(ink(image, area(QRect(textLeft, one.top() + 2, 60, 10)), surface), surface, 3));
            // Two lines: the headline over the supporting text, in the middle of the row together.
            const QRect two = rowRect(1);
            const int top = two.top() + (72 - 44) / 2;
            CHECK(near(ink(image, area(QRect(textLeft, top + 2, 80, 20)), surface), headline, 12));
            CHECK(near(ink(image, area(QRect(textLeft, top + 26, 80, 16)), surface), supporting, 12));
            CHECK(near(ink(image, area(QRect(textLeft, two.top() + 1, 80, 8)), surface), surface, 3));
            // Three lines: the headline 12 from the top, and two lines of supporting text under it.
            CHECK(near(ink(image, area(QRect(textLeft, three.top() + 14, 80, 20)), surface), headline, 12));
            CHECK(near(ink(image, area(QRect(textLeft, three.top() + 38, 80, 16)), surface), supporting, 12));
            CHECK(near(ink(image, area(QRect(textLeft, three.top() + 58, 80, 16)), surface), supporting, 12));
            // A trailing text at the end of the row, 16 from it, in the quiet color; the headline keeps its own.
            const QRect trailing = rowRect(3);
            CHECK(near(ink(image, area(QRect(trailing.right() - 16 - 30, trailing.top() + 18, 30, 20)), surface),
                       supporting, 12));
            CHECK(
                near(ink(image, area(QRect(trailing.right() - 15, trailing.top() + 4, 14, 48)), surface), surface, 3));
            CHECK(near(ink(image, area(QRect(textLeft, trailing.top() + 18, 40, 20)), surface), headline, 12));
        }
    }

    // A disabled item has fainter text; a selected one the colors of its container.
    style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);
    widgetList->item(1)->setFlags(Qt::ItemIsSelectable);
    widgetList->setCurrentRow(2);
    QCoreApplication::processEvents();
    if (textIsDrawn()) {
        const QImage image = window.grab().toImage();
        const QPoint origin = widgetList->viewport()->mapTo(&window, QPoint(0, 0));
        const QRect two = widgetList->visualItemRect(widgetList->item(1)).translated(origin);
        const QRect three = widgetList->visualItemRect(widgetList->item(2)).translated(origin);
        const QColor surface = image.pixelColor(two.right() - 4, two.top() + 4);
        const QColor faint = ink(image, QRect(two.left() + 52, two.top() + 40, 80, 16), surface);
        CHECK(!near(faint, style->color(Role::OnSurfaceVariant), 30));
        CHECK(!near(faint, surface, 20));
        const QColor container = image.pixelColor(three.right() - 4, three.center().y());
        CHECK(near(container, style->color(Role::SecondaryContainer), 3));
        CHECK(near(ink(image, QRect(three.left() + 52, three.top() + 38, 80, 16), container),
                   style->color(Role::OnSecondaryContainer), 12));
    }

    // Another style draws the text as it always has, the rows as high as one line.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::processEvents();
    CHECK(widgetList->visualItemRect(widgetList->item(1)).height() < 56);
}

// The extras of Material list items: trailing check boxes, avatars, images, overlines, and segmented lists, in a
// QListWidget and in a QListView with a model.
void testListItemExtras() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    // The color of the pixel in `r` that is farthest from `background`: the ink of what is drawn there.
    const auto ink = [](const QImage &image, const QRect &r, const QColor &background) {
        QColor most = background;
        int far = -1;
        for (int y = r.top(); y <= r.bottom(); ++y)
            for (int x = r.left(); x <= r.right(); ++x) {
                const QColor c = image.pixelColor(x, y);
                const int d = std::abs(c.red() - background.red()) + std::abs(c.green() - background.green()) +
                              std::abs(c.blue() - background.blue());
                if (d > far) {
                    far = d;
                    most = c;
                }
            }
        return most;
    };
    const auto click = [](QWidget *on, const QPoint &at) {
        const QPointF point(at);
        QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(on, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(on, &release);
        QCoreApplication::processEvents();
    };

    // A list widget and a list view with the same items, side by side in a window.
    struct Pair {
        QListWidget *items;
        QListView *view;
        QStandardItemModel *model;
    };
    QWidget window;
    auto *layout = new QHBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    const auto makePair = [&](const QStringList &texts, const QIcon &icon) {
        Pair pair{new QListWidget, new QListView, new QStandardItemModel(&window)};
        for (const QString &text : texts) {
            pair.items->addItem(new QListWidgetItem(icon, text));
            pair.model->appendRow(new QStandardItem(icon, text));
        }
        pair.items->setIconSize(QSize(24, 24));
        pair.view->setIconSize(QSize(24, 24));
        pair.view->setModel(pair.model);
        pair.items->setFrameShape(QFrame::NoFrame);
        pair.view->setFrameShape(QFrame::NoFrame);
        return pair;
    };
    const auto rowOf = [](const Pair &pair, bool items, int n) {
        return items ? pair.items->visualItemRect(pair.items->item(n)) : pair.view->visualRect(pair.model->index(n, 0));
    };
    const auto portOf = [](const Pair &pair, bool items) {
        return items ? pair.items->viewport() : pair.view->viewport();
    };
    const auto show = [&](const Pair &pair) {
        while (QLayoutItem *item = layout->takeAt(0))
            delete item;
        for (QWidget *child : window.findChildren<QAbstractItemView *>(QString(), Qt::FindDirectChildrenOnly))
            child->hide();
        layout->addWidget(pair.items);
        layout->addWidget(pair.view);
        pair.items->show();
        pair.view->show();
        window.resize(720, 400);
        window.show();
        QCoreApplication::processEvents();
    };
    const QColor surface = style->color(Role::Surface);

    // 1. Check boxes: at the trailing end, an 18 box in the middle of a slot of 24, 16 from the end; checked in the
    // accent color, unchecked an outline. A click on the box toggles it, one elsewhere in the row does not.
    {
        QPixmap black(24, 24);
        black.fill(Qt::black);
        Pair pair = makePair(
            {QStringLiteral("Wi-Fi"), QStringLiteral("Bluetooth"), QStringLiteral("Headline\tNew\nSupporting\nThird")},
            QIcon(black));
        for (int n = 0; n < 3; ++n) {
            QListWidgetItem *item = pair.items->item(n);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(n == 0 ? Qt::Checked : Qt::Unchecked);
            pair.model->item(n)->setCheckable(true);
            pair.model->item(n)->setCheckState(n == 0 ? Qt::Checked : Qt::Unchecked);
        }
        show(pair);
        for (const bool items : {true, false}) {
            QWidget *port = portOf(pair, items);
            const QImage image = window.grab().toImage();
            const QPoint origin = port->mapTo(&window, QPoint(0, 0));
            const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
            const QRect checked = rowOf(pair, items, 0);
            CHECK(checked.height() == 56);
            const QRect box(checked.right() + 1 - 16 - 21, checked.top() + (56 - 18) / 2, 18, 18);
            CHECK(near(at(box.left() + 3, box.top() + 3), style->color(Role::Primary), 3));
            CHECK(near(at(box.right() - 2, box.bottom() - 2), style->color(Role::Primary), 3));
            CHECK(near(at(box.right() + 3, box.center().y()), surface, 3));
            const QRect unchecked = rowOf(pair, items, 1);
            const QRect empty = box.translated(0, unchecked.top() - checked.top());
            CHECK(near(at(empty.left(), empty.center().y()), style->color(Role::OnSurfaceVariant), 3));
            CHECK(near(at(empty.right(), empty.center().y()), style->color(Role::OnSurfaceVariant), 3));
            CHECK(near(at(empty.center().x(), empty.center().y()), surface, 3));
            // The icon stays at the start; the box of a row of three lines is beside the headline, 12 from the top.
            CHECK(near(at(16 + 12, checked.center().y()), style->color(Role::OnSurfaceVariant), 3));
            const QRect three = rowOf(pair, items, 2);
            CHECK(three.height() == 88);
            CHECK(near(at(box.left(), three.top() + 12 + 3 + 9), style->color(Role::OnSurfaceVariant), 3));
            CHECK(near(at(box.left(), three.center().y() + 10), surface, 3));
            if (textIsDrawn()) {
                // The trailing text is before the slot of the box, 12 from it; nothing is drawn under the box.
                const QRect trailing(three.right() + 1 - 16 - 24 - 12 - 24, three.top() + 12, 24, 24);
                CHECK(near(ink(image, trailing.translated(origin), surface), style->color(Role::OnSurfaceVariant), 12));
                CHECK(
                    near(ink(image, QRect(three.right() + 1 - 16 - 24 - 12, three.top() + 2, 12, 84).translated(origin),
                             surface),
                         surface, 3));
            }
            // A click beside the box toggles it; one in the middle of the row does not.
            click(port, QPoint(empty.center().x() - 8, empty.center().y() + 8));
            const auto state = [&](int n) {
                return items ? pair.items->item(n)->checkState() : pair.model->item(n)->checkState();
            };
            CHECK(state(1) == Qt::Checked);
            click(port, QPoint(empty.center().x() - 120, empty.center().y()));
            CHECK(state(1) == Qt::Checked);
            click(port, empty.center());
            CHECK(state(1) == Qt::Unchecked);
            click(port, box.center());
            CHECK(state(0) == Qt::Unchecked);
        }
        // Space toggles the current item of a list widget.
        pair.items->setFocus();
        pair.items->setCurrentRow(1);
        QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        QCoreApplication::sendEvent(pair.items, &space);
        CHECK(pair.items->item(1)->checkState() == Qt::Checked);
        // An item that the user may not check keeps its state.
        pair.items->item(1)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        QCoreApplication::sendEvent(pair.items, &space);
        CHECK(pair.items->item(1)->checkState() == Qt::Checked);
    }

    // 2. Avatars and images: pictures in their own colors, a circle of 40 and a square of 56 with round corners.
    {
        QPixmap red(64, 64);
        red.fill(QColor(220, 20, 20));
        const QColor redInk(220, 20, 20);
        Pair avatars = makePair({QStringLiteral("Ali Connors"), QStringLiteral("Alex\nSupporting")}, QIcon(red));
        avatars.items->setProperty("materialListAvatars", true);
        avatars.view->setProperty("materialListAvatars", true);
        show(avatars);
        for (const bool items : {true, false}) {
            const QImage image = window.grab().toImage();
            const QPoint origin = portOf(avatars, items)->mapTo(&window, QPoint(0, 0));
            const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
            const QRect one = rowOf(avatars, items, 0);
            CHECK(one.height() == 56);
            const QRect round(16, one.top() + 8, 40, 40);
            CHECK(near(at(round.center().x(), round.center().y()), redInk, 3));
            CHECK(near(at(round.left() + 1, round.center().y()), redInk, 30));
            CHECK(near(at(round.left() + 2, round.top() + 2), surface, 3));     // outside the circle
            CHECK(near(at(round.right() - 2, round.bottom() - 2), surface, 3)); // outside the circle
            CHECK(near(at(round.right() + 4, round.center().y()), surface, 3));
            CHECK(rowOf(avatars, items, 1).height() == 72);
        }

        Pair images = makePair({QStringLiteral("Image"), QStringLiteral("Image\nSupporting")}, QIcon(red));
        images.items->setProperty("materialListImages", true);
        images.view->setProperty("materialListImages", true);
        show(images);
        for (const bool items : {true, false}) {
            const QImage image = window.grab().toImage();
            const QPoint origin = portOf(images, items)->mapTo(&window, QPoint(0, 0));
            const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
            // A row of one line is 72, for 8 above and below the image.
            const QRect one = rowOf(images, items, 0);
            CHECK(one.height() == 72);
            CHECK(rowOf(images, items, 1).height() == 72);
            const QRect square(16, one.top() + 8, 56, 56);
            CHECK(near(at(square.center().x(), square.center().y()), redInk, 3));
            CHECK(near(at(square.left() + 4, square.top() + 4), redInk, 3)); // a square, not a circle
            CHECK(near(at(square.left(), square.top()), surface, 30));       // with round corners
            CHECK(near(at(square.center().x(), square.top()), redInk, 3));
            CHECK(near(at(square.center().x(), square.top() - 2), surface, 3));
            CHECK(near(at(square.right() + 4, square.center().y()), surface, 3));
        }
    }

    // 3. An overline: the first line of a row of three lines, in label small and the quiet color, over the headline.
    {
        Pair pair = makePair({QStringLiteral("OVERLINE\nHeadline\nSupporting text")}, QIcon());
        pair.items->setProperty("materialListOverline", true);
        pair.view->setProperty("materialListOverline", true);
        show(pair);
        for (const bool items : {true, false}) {
            const QRect row = rowOf(pair, items, 0);
            CHECK(row.height() == 88);
            if (!textIsDrawn())
                continue;
            const QImage image = window.grab().toImage();
            const QPoint origin = portOf(pair, items)->mapTo(&window, QPoint(0, 0));
            // The overline 12 from the top, 16 high; the headline under it, 24; one line of supporting text, 20.
            const auto band = [&](int top, int height) {
                return ink(image, QRect(16, row.top() + top, 120, height).translated(origin), surface);
            };
            CHECK(near(band(12 + 2, 12), style->color(Role::OnSurfaceVariant), 12));
            CHECK(near(band(28 + 2, 20), style->color(Role::OnSurface), 12));
            CHECK(near(band(52 + 2, 16), style->color(Role::OnSurfaceVariant), 12));
            CHECK(near(band(74, 12), surface, 3));
        }
        // Without the property the first line is the headline.
        pair.items->setProperty("materialListOverline", QVariant());
        pair.items->viewport()->update();
        QCoreApplication::processEvents();
        if (textIsDrawn()) {
            const QImage image = window.grab().toImage();
            const QPoint origin = pair.items->viewport()->mapTo(&window, QPoint(0, 0));
            const QRect row = rowOf(pair, true, 0);
            CHECK(near(ink(image, QRect(16, row.top() + 14, 120, 12).translated(origin), surface),
                       style->color(Role::OnSurface), 12));
        }
    }

    // 4. A segmented list: each row a container of its own, 2 apart, with corners of 16 at the ends of the list and
    // of a selected row, and 4 between rows; a selected row in the secondary container.
    {
        Pair pair = makePair({QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three")}, QIcon());
        pair.items->setProperty("materialList", QStringLiteral("segmented"));
        pair.view->setProperty("materialList", QStringLiteral("segmented"));
        pair.items->setCurrentRow(1);
        pair.view->setCurrentIndex(pair.model->index(1, 0));
        pair.view->selectionModel()->select(pair.model->index(1, 0), QItemSelectionModel::ClearAndSelect);
        show(pair);
        const QColor container = style->color(Role::SurfaceContainer);
        const QColor chosen = style->color(Role::SecondaryContainer);
        for (const bool items : {true, false}) {
            const QImage image = window.grab().toImage();
            const QPoint origin = portOf(pair, items)->mapTo(&window, QPoint(0, 0));
            const auto at = [&](int x, int y) { return image.pixelColor(origin.x() + x, origin.y() + y); };
            const QRect first = rowOf(pair, items, 0), middle = rowOf(pair, items, 1), last = rowOf(pair, items, 2);
            const int x = first.center().x();
            CHECK(near(at(x, first.center().y()), container, 3));
            CHECK(near(at(x, middle.center().y()), chosen, 3));
            CHECK(near(at(x, last.center().y()), container, 3));
            // A gap of 2 between rows.
            CHECK(near(at(x, first.bottom()), surface, 3));
            CHECK(near(at(x, middle.top()), surface, 3));
            CHECK(near(at(x, middle.bottom()), surface, 3));
            // The outer corners round (16), the inner ones a little (4), and the selected row's all round.
            CHECK(near(at(first.left() + 2, first.top() + 3), surface, 3));
            CHECK(near(at(first.right() - 2, first.top() + 3), surface, 3));
            CHECK(near(at(first.left() + 2, first.bottom() - 3), container, 3));
            CHECK(near(at(last.left() + 2, last.top() + 3), container, 3));
            CHECK(near(at(last.left() + 2, last.bottom() - 3), surface, 3));
            CHECK(near(at(middle.left() + 2, middle.top() + 3), surface, 3));
            CHECK(near(at(middle.left() + 2, middle.bottom() - 3), surface, 3));
            CHECK(near(at(middle.left() + 16, middle.top() + 3), chosen, 3));
        }
    }
}

// A docked toolbar at the bottom of a main window, and icon buttons that are narrower or wider than they are high.
void testDockedToolBarAndIconWidths() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    QMainWindow window;
    window.setCentralWidget(new QWidget);
    auto *bar = new QToolBar;
    bar->setProperty("materialToolBar", QStringLiteral("docked"));
    QAction *first = bar->addAction(QStringLiteral("A"));
    QAction *bold = bar->addAction(QStringLiteral("B"));
    bold->setCheckable(true);
    QAction *separator = bar->addSeparator();
    QAction *last = bar->addAction(QStringLiteral("C"));
    window.addToolBar(Qt::BottomToolBarArea, bar);
    auto *status = new QStatusBar;
    status->setProperty("materialStatusBar", QStringLiteral("snackbar"));
    window.setStatusBar(status);
    window.resize(600, 480);
    window.show();
    QCoreApplication::processEvents();

    // Across the whole of the bottom, 64 high.
    CHECK(bar->geometry() == QRect(0, 480 - 64, 600, 64));
    const auto at = [&](int x, int y) {
        const QPoint point = bar->mapTo(&window, QPoint(x, y));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor container = style->color(Role::SurfaceContainer);
    CHECK(near(at(100, 32), container, 2));
    CHECK(near(at(0, 0), container, 2) && near(at(599, 63), container, 2)); // square corners, no shadow
    // Its buttons are 48, in the middle 32 apart while there is room for that; the separator is not shown.
    const auto button = [&](QAction *action) { return bar->widgetForAction(action); };
    CHECK(button(first)->size() == QSize(48, 48) && button(first)->y() == 8);
    CHECK(button(separator)->isHidden());
    CHECK(button(bold)->x() - (button(first)->x() + 48) == 32);
    CHECK(button(last)->x() - (button(bold)->x() + 48) == 32);
    const int group = 3 * 48 + 2 * 32;
    CHECK(near(button(first)->x(), (600 - group) / 2.0, 1.0));
    // In the toolbar item colors: no container until it is checked.
    QWidget *boldButton = button(bold);
    CHECK(near(at(boldButton->x() + 10, 32), container, 2));
    bold->setChecked(true);
    QCoreApplication::processEvents();
    CHECK(near(at(boldButton->x() + 10, 32), style->color(Role::SecondaryContainer), 2));
    // A snackbar is above it.
    status->showMessage(QStringLiteral("Archived"));
    QCoreApplication::processEvents();
    CHECK(status->height() > 0 && status->y() + status->height() == bar->y() - 16);

    // More buttons than fit 32 apart are spread from 16 in at either end, evenly.
    for (int i = 0; i < 3; ++i)
        bar->addAction(QStringLiteral("D"));
    window.resize(360, 480);
    QCoreApplication::processEvents();
    CHECK(bar->geometry() == QRect(0, 480 - 64, 360, 64));
    QList<QWidget *> shown;
    for (QAction *action : bar->actions())
        if (!action->isSeparator())
            shown.append(button(action));
    CHECK(shown.size() == 6);
    CHECK(shown.first()->x() == 16 && shown.last()->x() + 48 >= 360 - 17);
    for (qsizetype i = 1; i < shown.size(); ++i) {
        const int space = shown[i]->x() - (shown[i - 1]->x() + 48);
        CHECK(space >= 4 && space < 32 && near(space, (360 - 32 - 6 * 48) / 5.0, 1.0));
    }
    CHECK(status->y() + status->height() == bar->y() - 16);
    // The vibrant one.
    bar->setProperty("materialToolBar", QStringLiteral("dockedVibrant"));
    bar->update();
    QCoreApplication::processEvents();
    CHECK(near(at(2, 2), style->color(Role::PrimaryContainer), 2));

    // Icon buttons of each width, at the small, medium and extra large sizes.
    QWidget panel;
    auto *column = new QVBoxLayout(&panel);
    const QIcon plain = [] {
        QImage image(96, 96, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(QRect(16, 16, 64, 64), Qt::black);
        painter.end();
        return QIcon(QPixmap::fromImage(image));
    }();
    const auto make = [&](const char *size, const char *width) {
        auto *made = new QToolButton;
        made->setProperty("materialIconButton", QStringLiteral("filled"));
        if (size)
            made->setProperty("materialButtonSize", QString::fromLatin1(size));
        if (width)
            made->setProperty("materialIconButtonWidth", QString::fromLatin1(width));
        made->setIcon(plain);
        column->addWidget(made, 0, Qt::AlignLeft);
        return made;
    };
    QToolButton *small = make(nullptr, nullptr);
    QToolButton *smallDefault = make("small", "default");
    QToolButton *narrow = make("small", "narrow");
    QToolButton *wide = make("small", "wide");
    QToolButton *xsNarrow = make("extraSmall", "narrow");
    QToolButton *mediumNarrow = make("medium", "narrow");
    QToolButton *mediumWide = make("medium", "wide");
    QToolButton *largeNarrow = make("large", "narrow");
    QToolButton *xlWide = make("extraLarge", "wide");
    auto *push = new QPushButton;
    push->setProperty("materialIconButton", QStringLiteral("tonal"));
    push->setProperty("materialIconButtonWidth", QStringLiteral("wide"));
    push->setIcon(plain);
    column->addWidget(push, 0, Qt::AlignLeft);
    column->addStretch(1);
    panel.resize(260, 1000);
    panel.show();
    QCoreApplication::processEvents();
    // The size to a finger: the shape, and 48 at least.
    CHECK(small->size() == QSize(48, 48) && smallDefault->size() == QSize(48, 48));
    CHECK(narrow->size() == QSize(48, 48) && wide->size() == QSize(52, 48));
    CHECK(xsNarrow->size() == QSize(48, 48));
    CHECK(mediumNarrow->size() == QSize(48, 56) && mediumWide->size() == QSize(72, 56));
    CHECK(largeNarrow->size() == QSize(64, 96) && xlWide->size() == QSize(184, 136));
    CHECK(push->sizeHint() == QSize(52, 48));
    const auto pixel = [&](QWidget *widget, int x, int y) {
        const QPoint point = widget->mapTo(&panel, QPoint(x, y));
        return panel.grab().toImage().pixelColor(point.x(), point.y());
    };
    const QColor surface = style->color(Role::Surface);
    const QColor primary = style->color(Role::Primary);
    // A wide small one is 52 by 40: it reaches the sides, not the top.
    CHECK(near(pixel(wide, 2, 24), primary, 3) && near(pixel(wide, 49, 24), primary, 3));
    CHECK(near(pixel(wide, 26, 2), surface, 3) && near(pixel(wide, 26, 6), primary, 3));
    // A narrow small one is 32 by 40, in the middle: room at the sides, a round top.
    CHECK(near(pixel(narrow, 5, 24), surface, 3) && near(pixel(narrow, 10, 24), primary, 3));
    CHECK(near(pixel(narrow, 42, 24), surface, 3) && near(pixel(narrow, 24, 6), primary, 3));
    CHECK(near(pixel(narrow, 10, 6), surface, 3)); // the corner of a shape that is a circle at each end
    // A narrow extra small one is 28 by 32.
    CHECK(near(pixel(xsNarrow, 9, 24), surface, 3) && near(pixel(xsNarrow, 12, 24), primary, 3));
    CHECK(near(pixel(xsNarrow, 24, 6), surface, 3) && near(pixel(xsNarrow, 24, 10), primary, 3));
    // A medium narrow one is 48 by 56, all of its width.
    CHECK(near(pixel(mediumNarrow, 1, 28), primary, 3) && near(pixel(mediumNarrow, 24, 1), primary, 3));
    CHECK(near(pixel(push, 2, 24), style->color(Role::SecondaryContainer), 3));
    // The icon is in the middle of each.
    const auto inkCentre = [&](QWidget *widget) {
        const QImage image = widget->grab().toImage();
        const QColor ink = style->color(Role::OnPrimary);
        double sum = 0;
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (near(image.pixelColor(x, y), ink, 8)) {
                    sum += x;
                    ++count;
                }
        return count > 0 ? sum / count : -1.0;
    };
    for (QToolButton *each : {narrow, wide, mediumWide, largeNarrow, xlWide})
        CHECK(near(inkCentre(each), (each->width() - 1) / 2.0, 1.0));
    // Pressed, the shape still morphs: the corners of the wide one tighten.
    CHECK(near(pixel(wide, 3, 7), surface, 3));
    wide->setDown(true);
    QCoreApplication::processEvents();
    CHECK(near(pixel(wide, 3, 7), primary, 40)); // with the layer of the press over it
    wide->setDown(false);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// Subtitles, centred titles and titles on two lines in top app bars, and the bars that scroll away: a top app bar with
// enterAlways and a docked toolbar with exitAlways. A flexible bar that is pinned does not collapse.
void testAppBarSubtitlesAndScrolling() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(false);
    QApplication::setStyle(style);

    const auto makeList = [] {
        auto *list = new QListWidget;
        for (int i = 0; i < 100; ++i)
            list->addItem(QStringLiteral("Message %1").arg(i));
        return list;
    };
    // The rows of `image` in [top, bottom) and the columns in [left, right) that have ink: darker than half.
    struct Ink {
        int count = 0, left = 1 << 20, right = -1;
        QColor darkest = Qt::white;
    };
    const auto inkOf = [](QWidget *widget, int left, int top, int right, int bottom) {
        Ink ink;
        const QImage image = widget->grab().toImage();
        for (int y = top; y < std::min(bottom, image.height()); ++y)
            for (int x = left; x < std::min(right, image.width()); ++x) {
                const QColor c = image.pixelColor(x, y);
                if (c.lightness() >= 128)
                    continue;
                ++ink.count;
                ink.left = std::min(ink.left, x);
                ink.right = std::max(ink.right, x);
                if (c.lightness() < ink.darkest.lightness())
                    ink.darkest = c;
            }
        return ink;
    };

    // The small bar keeps 64 with a subtitle: the title (22 on 28) and the subtitle (12 on 16) are centred together,
    // the subtitle in OnSurfaceVariant.
    {
        QMainWindow window;
        auto *bar = new QToolBar;
        bar->setProperty("materialToolBar", QStringLiteral("appBar"));
        bar->setProperty("materialSubtitle", QStringLiteral("Subtitle text"));
        bar->setWindowTitle(QStringLiteral("Inbox"));
        window.addToolBar(bar);
        window.setCentralWidget(makeList());
        window.resize(360, 480);
        window.show();
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
        if (textIsDrawn()) {
            const Ink title = inkOf(bar, 0, 10, 360, 38);
            const Ink subtitle = inkOf(bar, 0, 39, 360, 54);
            CHECK(title.count > 20 && subtitle.count > 10);
            CHECK(inkOf(bar, 0, 0, 360, 9).count == 0 && inkOf(bar, 0, 56, 360, 64).count == 0);
            CHECK(subtitle.left >= 16 && subtitle.left < 24);
            CHECK(near(subtitle.darkest, style->color(Role::OnSurfaceVariant), 40));
            CHECK(subtitle.darkest.lightness() > title.darkest.lightness());
        }

        // Centred, the title and the subtitle are in the middle of the bar.
        bar->setProperty("materialTitleAlignment", QStringLiteral("center"));
        bar->update();
        QCoreApplication::processEvents();
        if (textIsDrawn()) {
            const Ink title = inkOf(bar, 0, 10, 360, 38);
            const Ink subtitle = inkOf(bar, 0, 39, 360, 54);
            CHECK(near((title.left + title.right) / 2.0, 180.0, 3.0));
            CHECK(near((subtitle.left + subtitle.right) / 2.0, 180.0, 3.0));
            CHECK(title.left > 100);
        }
        // Without a subtitle the title is in the middle of the row both ways.
        bar->setProperty("materialSubtitle", QVariant());
        bar->update();
        QCoreApplication::processEvents();
        if (textIsDrawn()) {
            const Ink title = inkOf(bar, 0, 0, 360, 64);
            CHECK(near((title.left + title.right) / 2.0, 180.0, 3.0));
        }
    }

    // The medium and large flexible bars are 136 and 152 high with a subtitle, which is under the title; a title too
    // long for a line goes on two and the bar grows by a line, 36 or 44.
    for (const char *kind : {"appBarMedium", "appBarLarge"}) {
        const bool large = QByteArray(kind) == "appBarLarge";
        QMainWindow window;
        auto *bar = new QToolBar;
        bar->setProperty("materialToolBar", QString::fromLatin1(kind));
        bar->setProperty("materialSubtitle", QStringLiteral("Subtitle text"));
        bar->setWindowTitle(QStringLiteral("Inbox"));
        window.addToolBar(bar);
        auto *list = makeList();
        window.setCentralWidget(list);
        window.resize(360, 480);
        window.show();
        QCoreApplication::processEvents();
        const int subtitled = large ? 152 : 136;
        CHECK(bar->height() == subtitled);
        CHECK(list->mapTo(&window, QPoint(0, 0)).y() == subtitled);
        if (textIsDrawn()) {
            // The subtitle is the last line, 12 above the bottom; the title is over it.
            const int subtitleLine = large ? 24 : 20;
            const Ink subtitle = inkOf(bar, 0, subtitled - 12 - subtitleLine, 360, subtitled - 12);
            const Ink title = inkOf(bar, 0, 64, 360, subtitled - 12 - subtitleLine);
            CHECK(subtitle.count > 10 && title.count > 40);
            CHECK(subtitle.darkest.lightness() > title.darkest.lightness());
        }
        // Collapsed, it is the small bar with both in the row.
        list->verticalScrollBar()->setValue(200);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
        if (textIsDrawn())
            CHECK(inkOf(bar, 0, 39, 360, 54).count > 10);
        list->verticalScrollBar()->setValue(0);
        QCoreApplication::processEvents();
        CHECK(bar->height() == subtitled);

        // Two lines of title.
        bar->setProperty("materialSubtitle", QVariant());
        bar->setWindowTitle(QStringLiteral("A title far too long for one line of the bar"));
        bar->updateGeometry();
        QCoreApplication::processEvents();
        const int line = large ? 44 : 36;
        const int single = large ? 120 : 112;
        if (textIsDrawn()) {
            CHECK(bar->height() == single + line);
            // Ink on both lines.
            CHECK(inkOf(bar, 0, single + line - 12 - 2 * line, 360, single + line - 12 - line).count > 40);
            CHECK(inkOf(bar, 0, single + line - 12 - line, 360, single + line - 12).count > 40);
            // Wide enough, it is one line again.
            window.resize(1600, 480);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            CHECK(bar->height() == single);
            window.resize(360, 480);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            CHECK(bar->height() == single + line);
            // With a subtitle too.
            bar->setProperty("materialSubtitle", QStringLiteral("Subtitle"));
            bar->updateGeometry();
            QCoreApplication::processEvents();
            CHECK(bar->height() == subtitled + line);
        }

        // Pinned, it does not collapse.
        bar->setProperty("materialScrollBehavior", QStringLiteral("pinned"));
        bar->setProperty("materialSubtitle", QVariant());
        bar->setWindowTitle(QStringLiteral("Inbox"));
        bar->updateGeometry();
        QCoreApplication::processEvents();
        CHECK(bar->height() == single);
        list->verticalScrollBar()->setValue(200);
        QCoreApplication::processEvents();
        CHECK(bar->height() == single);
        CHECK(near(pixel(*bar, 300, single - 4), style->color(Role::SurfaceContainer), 3));
        list->verticalScrollBar()->setValue(0);
        QCoreApplication::processEvents();
    }

    // enterAlways: the small bar goes as the list scrolls down and comes back as soon as it scrolls up.
    {
        QMainWindow window;
        auto *bar = new QToolBar;
        bar->setProperty("materialToolBar", QStringLiteral("appBar"));
        bar->setProperty("materialScrollBehavior", QStringLiteral("enterAlways"));
        QAction *more = bar->addAction(QStringLiteral("More"));
        window.addToolBar(bar);
        auto *list = makeList();
        window.setCentralWidget(list);
        window.resize(360, 480);
        window.show();
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
        QScrollBar *scroll = list->verticalScrollBar();
        scroll->setValue(10);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 0);
        CHECK(list->mapTo(&window, QPoint(0, 0)).y() == 0);
        CHECK(scroll->value() == 10);
        scroll->setValue(15);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 0);
        scroll->setValue(14);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
        CHECK(list->mapTo(&window, QPoint(0, 0)).y() == 64);
        CHECK(bar->widgetForAction(more)->y() == 8);
        // At the bottom, the room that the bar gives up does not bring it back.
        scroll->setValue(scroll->maximum());
        QCoreApplication::processEvents();
        CHECK(bar->height() == 0);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 0);
        scroll->setValue(0);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);

        // With motion it slides away, its buttons going up with its bottom edge.
        style->setMotionEnabled(true);
        QCoreApplication::processEvents();
        scroll->setValue(10);
        QCoreApplication::processEvents();
        wait(60);
        const QWidget *button = bar->widgetForAction(more);
        CHECK(bar->height() > 0 && bar->height() < 64);
        CHECK(button->y() == bar->height() - 64 + 8);
        wait(700);
        CHECK(bar->height() == 0);
        scroll->setValue(9);
        wait(700);
        CHECK(bar->height() == 64 && button->y() == 8);
        style->setMotionEnabled(false);
    }

    // exitAlways: a docked toolbar at the bottom goes as the list scrolls down and comes back as it scrolls up.
    {
        QMainWindow window;
        auto *list = makeList();
        window.setCentralWidget(list);
        auto *bar = new QToolBar;
        bar->setProperty("materialToolBar", QStringLiteral("docked"));
        bar->setProperty("materialScrollBehavior", QStringLiteral("exitAlways"));
        QAction *first = bar->addAction(QStringLiteral("A"));
        bar->addAction(QStringLiteral("B"));
        window.addToolBar(Qt::BottomToolBarArea, bar);
        window.resize(360, 480);
        window.show();
        QCoreApplication::processEvents();
        CHECK(bar->geometry() == QRect(0, 480 - 64, 360, 64));
        QScrollBar *scroll = list->verticalScrollBar();
        scroll->setValue(10);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 0);
        CHECK(list->height() == 480);
        scroll->setValue(8);
        QCoreApplication::processEvents();
        CHECK(bar->geometry() == QRect(0, 480 - 64, 360, 64));
        CHECK(bar->widgetForAction(first)->y() == 8);

        // With motion its buttons stay with its top edge as it goes down.
        style->setMotionEnabled(true);
        QCoreApplication::processEvents();
        scroll->setValue(15);
        QCoreApplication::processEvents();
        wait(60);
        CHECK(bar->height() > 0 && bar->height() < 64);
        CHECK(bar->y() + bar->height() == 480);
        CHECK(bar->widgetForAction(first)->y() == 8);
        wait(700);
        CHECK(bar->height() == 0);
        style->setMotionEnabled(false);

        // Without the property it stays.
        bar->setProperty("materialScrollBehavior", QVariant());
        bar->updateGeometry();
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
        scroll->setValue(30);
        QCoreApplication::processEvents();
        CHECK(bar->height() == 64);
    }
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A completer under a search bar opens Material's docked search view: the bar and the view are one shape. Under
// another line edit its popup is a menu.
void testSearchView() {
    for (const Qt::ColorScheme scheme : {Qt::ColorScheme::Light, Qt::ColorScheme::Dark}) {
        auto *style = new QMaterialStyle;
        style->setColorScheme(scheme);
        style->setMotionEnabled(false);
        QApplication::setStyle(style);
        const auto is = [&](const QColor &color, Role role) { return near(color, style->color(role), 3); };

        QWidget window;
        window.resize(400, 600);
        auto *search = new QLineEdit(&window);
        search->setProperty("materialTextField", QStringLiteral("search"));
        search->setGeometry(20, 20, 360, 56);
        auto *plain = new QLineEdit(&window);
        plain->setGeometry(20, 520, 200, 56);
        QStandardItemModel words;
        for (int i = 0; i < 10; ++i)
            words.appendRow(new QStandardItem(QStringLiteral("apple %1").arg(i)));
        QPixmap mark(24, 24);
        mark.fill(Qt::red);
        words.item(1)->setIcon(QIcon(mark));
        QCompleter completer(&words);
        search->setCompleter(&completer);
        window.show();
        QCoreApplication::processEvents();
        QWindow *handle = window.windowHandle();
        const auto key = [&](int code, const QString &text = QString()) {
            QWindowSystemInterface::handleKeyEvent(handle, QEvent::KeyPress, code, Qt::NoModifier, text);
            QWindowSystemInterface::handleKeyEvent(handle, QEvent::KeyRelease, code, Qt::NoModifier);
            QCoreApplication::processEvents();
        };
        const auto click = [&](QWidget *widget, QPoint local) {
            const QPointF at(widget->mapTo(&window, local));
            QWindowSystemInterface::handleMouseEvent(handle, at, at, Qt::LeftButton, Qt::LeftButton,
                                                     QEvent::MouseButtonPress, Qt::NoModifier);
            QWindowSystemInterface::handleMouseEvent(handle, at, at, Qt::NoButton, Qt::LeftButton,
                                                     QEvent::MouseButtonRelease, Qt::NoModifier);
            QCoreApplication::processEvents();
        };
        search->setFocus();
        key(Qt::Key_A, QStringLiteral("a"));
        auto *popup = dynamic_cast<QListWidget *>(completer.popup());
        CHECK(popup && popup->isVisible() && popup->count() == 10);
        if (!popup || !popup->isVisible())
            continue;
        // Under the bar and as wide, its rows 56 high, seven of them before it scrolls.
        CHECK(popup->parentWidget() == &window);
        CHECK(popup->geometry() == QRect(20, 76, 360, 7 * 56));
        CHECK(popup->visualItemRect(popup->item(0)).height() == 56);
        CHECK(popup->iconSize() == QSize(24, 24));
        QImage image = window.grab().toImage();
        const QRect view = popup->geometry();
        const int third = view.top() + 2 * 56; // a row that is not the current one
        CHECK(is(image.pixelColor(view.right() - 30, third + 28), Role::SurfaceContainerHigh));
        CHECK(is(image.pixelColor(view.center().x(), view.bottom() - 1), Role::SurfaceContainerHigh));
        CHECK(is(image.pixelColor(view.left() + 2, view.bottom() - 2), Role::Surface)); // round at the bottom,
        CHECK(is(image.pixelColor(view.right() - 2, view.bottom() - 2), Role::Surface));
        // square at the top, where the current row's layer reaches the corner,
        const QColor corner = image.pixelColor(view.left() + 1, view.top() + 2);
        CHECK(near(corner, image.pixelColor(view.right() - 30, view.top() + 2), 3) && !is(corner, Role::Surface));
        CHECK(is(image.pixelColor(view.center().x(), view.top()), Role::OutlineVariant)); // under a divider
        // The bar is square at the bottom while the view is open, and round at the top.
        const QRect bar = search->geometry();
        CHECK(is(image.pixelColor(bar.left() + 2, bar.bottom() - 2), Role::SurfaceContainerHigh));
        CHECK(is(image.pixelColor(bar.right() - 2, bar.bottom() - 2), Role::SurfaceContainerHigh));
        CHECK(is(image.pixelColor(bar.left() + 2, bar.top() + 2), Role::Surface));
        // The current row has a layer; the icon that the model has goes before the second word.
        CHECK(!is(image.pixelColor(view.right() - 30, view.top() + 28), Role::SurfaceContainerHigh));
        int marked = 0;
        for (int y = view.top() + 56 + 16; y < view.top() + 56 + 40; ++y)
            for (int x = view.left() + 16; x < view.left() + 40; ++x)
                marked += is(image.pixelColor(x, y), Role::OnSurfaceVariant) ? 1 : 0;
        CHECK(marked > 100);

        // The arrow keys move the current row, and Return takes it.
        key(Qt::Key_Down);
        CHECK(completer.currentRow() == 1);
        key(Qt::Key_Return);
        CHECK(search->text() == QStringLiteral("apple 1") && !popup->isVisible());
        image = window.grab().toImage();
        CHECK(is(image.pixelColor(bar.left() + 2, bar.bottom() - 2), Role::Surface)); // a pill again
        // A click takes a row.
        search->clear();
        key(Qt::Key_A, QStringLiteral("a"));
        CHECK(popup->isVisible());
        click(popup->viewport(), popup->visualItemRect(popup->item(2)).center());
        CHECK(search->text() == QStringLiteral("apple 2") && !popup->isVisible());
        // Escape closes the view and leaves the text.
        search->clear();
        key(Qt::Key_A, QStringLiteral("a"));
        CHECK(popup->isVisible());
        key(Qt::Key_Escape);
        CHECK(!popup->isVisible() && search->text() == QStringLiteral("a"));

        // Under another line edit the popup is a menu, with rows 48 high; with no room below, it opens above.
        QCompleter others(QStringList{QStringLiteral("berry"), QStringLiteral("blue"), QStringLiteral("brown")});
        plain->setCompleter(&others);
        plain->setFocus();
        key(Qt::Key_B, QStringLiteral("b"));
        auto *menu = dynamic_cast<QListWidget *>(others.popup());
        CHECK(menu && menu->isVisible());
        if (!menu || !menu->isVisible())
            continue;
        CHECK(menu->geometry() == QRect(20, 520 - 3 * 48, 200, 3 * 48));
        CHECK(menu->visualItemRect(menu->item(0)).height() == 48);
        image = window.grab().toImage();
        const QRect list = menu->geometry();
        CHECK(is(image.pixelColor(list.right() - 20, list.top() + 48 + 24), Role::SurfaceContainer));
        CHECK(is(image.pixelColor(list.left() + 1, list.top() + 1), Role::Surface)); // round corners
        CHECK(is(image.pixelColor(list.right() - 1, list.bottom() - 1), Role::Surface));
        key(Qt::Key_Down);
        key(Qt::Key_Return);
        CHECK(plain->text() == QStringLiteral("blue") && !menu->isVisible());
    }
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// What goes moves too: a sheet slides back to its edge, the scrim behind a dialog fades in and out, and the items of a
// FAB menu go one after another.
void testExitMotion() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    style->setMotionEnabled(true);
    QApplication::setStyle(style);
    const char *const scrim = "_nt_scrimOpacity";
    const auto ghostOf = [](QWidget &window) { return window.findChild<QWidget *>(QStringLiteral("_nt_sheetGhost")); };

    QWidget window;
    window.setGeometry(100, 50, 400, 700);
    window.show();
    QCoreApplication::processEvents();
    QDialog sheet(&window);
    sheet.setProperty("materialSheet", QStringLiteral("bottom"));
    auto *column = new QVBoxLayout(&sheet);
    auto *content = new QWidget;
    content->setFixedHeight(180);
    column->addWidget(content);
    int finished = 0, accepted = 0;
    QObject::connect(&sheet, &QDialog::finished, [&finished](int) { ++finished; });
    QObject::connect(&sheet, &QDialog::accepted, [&accepted] { ++accepted; });

    // The scrim fades in with the sheet.
    sheet.show();
    QWindow *handle = sheet.windowHandle();
    CHECK(handle->property(scrim).toDouble() < 0.05);
    wait(40);
    const double rising = handle->property(scrim).toDouble();
    CHECK(rising > 0.0 && rising < 0.32);
    wait(600);
    CHECK(near(handle->property(scrim).toDouble(), 0.32, 1e-9));
    const QRect rest = sheet.geometry();
    const int high = rest.height();
    CHECK(rest == QRect(100, 50 + 700 - high, 400, high));

    // Accepted, it is hidden at once, as any dialog is: the signals come and the result is set now. A picture of it,
    // in a window of its own that takes no input, slides to the edge with its scrim and then goes.
    sheet.accept();
    CHECK(!sheet.isVisible() && sheet.result() == QDialog::Accepted && finished == 1 && accepted == 1);
    QWidget *ghost = ghostOf(window);
    CHECK(ghost && ghost->isVisible() && ghost->isWindow() && ghost->geometry() == rest);
    CHECK(ghost && ghost->testAttribute(Qt::WA_TransparentForMouseEvents));
    CHECK(ghost && ghost->windowHandle()->property("_nt_sheet").toString() == QLatin1String("bottom"));
    CHECK(ghost && near(ghost->windowHandle()->property(scrim).toDouble(), 0.32, 1e-9));
    CHECK(handle->property(scrim).toDouble() == 0.0); // the dialog's own scrim is gone with it
    wait(40);
    ghost = ghostOf(window);
    CHECK(ghost && ghost->y() > rest.y() && ghost->y() < rest.y() + high); // on its way down,
    if (ghost) {
        const double fading = ghost->windowHandle()->property(scrim).toDouble();
        CHECK(fading > 0.0 && fading < 0.32); // its scrim fading
        CHECK(ghost->windowHandle()->property("_nt_sheetOffset").toInt() == ghost->y() - rest.y());
    }
    int lowest = rest.y();
    for (int i = 0; i < 100 && ghostOf(window); ++i) {
        if (const QWidget *going = ghostOf(window)) {
            CHECK(going->y() >= lowest); // only ever down
            lowest = going->y();
        }
        wait(5);
    }
    CHECK(!ghostOf(window)); // and gone at the edge
    CHECK(lowest > rest.y() + high / 2);

    // exec() returns as soon as the sheet is closed, while its picture still slides.
    QTimer::singleShot(400, &sheet, [&sheet] { sheet.reject(); });
    CHECK(sheet.exec() == QDialog::Rejected);
    CHECK(!sheet.isVisible() && finished == 2);
    ghost = ghostOf(window);
    CHECK(ghost && ghost->isVisible() && ghost->y() < rest.y() + high / 2);
    wait(500);
    CHECK(!ghostOf(window));

    // Shown again on its way out, it comes back from where it was, and the picture goes.
    sheet.show();
    wait(600);
    sheet.hide();
    wait(30);
    ghost = ghostOf(window);
    const int halfway = ghost ? ghost->y() : -1;
    CHECK(ghost && halfway > rest.y() && halfway < rest.y() + high);
    const double dimmed = ghost ? ghost->windowHandle()->property(scrim).toDouble() : -1;
    sheet.show();
    CHECK(sheet.isVisible() && sheet.y() >= rest.y() && sheet.y() <= halfway); // not from the edge,
    CHECK(near(handle->property(scrim).toDouble(), dimmed, 0.02));             // nor the scrim from nothing
    QCoreApplication::processEvents();
    CHECK(!ghostOf(window));
    wait(600);
    CHECK(sheet.isVisible() && sheet.geometry() == rest && !ghostOf(window));
    CHECK(near(handle->property(scrim).toDouble(), 0.32, 1e-9));

    // Escape, too; and a sheet that is deleted at once still leaves its picture, which goes on its own.
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&sheet, &escape);
    CHECK(!sheet.isVisible() && sheet.result() == QDialog::Rejected && ghostOf(window));
    wait(500);
    auto *drawer = new QDialog(&window);
    drawer->setProperty("materialSheet", QStringLiteral("drawer"));
    new QVBoxLayout(drawer);
    drawer->show();
    wait(800);
    const QRect drawerRest = drawer->geometry();
    drawer->accept();
    delete drawer;
    ghost = ghostOf(window);
    CHECK(ghost && ghost->geometry() == drawerRest);
    wait(40);
    ghost = ghostOf(window);
    CHECK(ghost && ghost->x() < drawerRest.x() && ghost->y() == drawerRest.y()); // to the left
    wait(500);
    CHECK(!ghostOf(window));

    // The scrim of a dialog that is no sheet fades in too, and goes with it.
    QDialog dialog(&window);
    new QVBoxLayout(&dialog);
    dialog.show();
    CHECK(dialog.windowHandle()->property(scrim).toDouble() < 0.05);
    wait(500);
    CHECK(near(dialog.windowHandle()->property(scrim).toDouble(), 0.32, 1e-9));
    dialog.hide();
    CHECK(dialog.windowHandle()->property(scrim).toDouble() == 0.0 && !ghostOf(window));

    // The items of a FAB menu go one after another, the one farthest from the button first, and then the menu.
    auto *layout = new QVBoxLayout(&window);
    layout->addStretch(1);
    auto *fab = new QPushButton;
    fab->setProperty("materialButton", QStringLiteral("fab"));
    auto *menu = new QMenu(fab);
    QAction *farthest = menu->addAction(QStringLiteral("Far from the button"));
    menu->addAction(QStringLiteral("Between"));
    QAction *closest = menu->addAction(QStringLiteral("By the button"));
    fab->setMenu(menu);
    layout->addWidget(fab, 0, Qt::AlignRight);
    QCoreApplication::processEvents();
    const QColor container = style->color(Role::PrimaryContainer);
    const QColor surface = style->color(Role::Surface);
    const auto pillEnd = [&](QAction *action) {
        const QRect row = menu->actionGeometry(action);
        const QPoint point = menu->mapTo(&window, QPoint(row.right() - 8, row.top() + 28));
        return window.grab().toImage().pixelColor(point.x(), point.y());
    };
    const auto leftovers = [&] { return window.findChildren<QWidget *>(QStringLiteral("_nt_fabMenuGhost")); };
    fab->showMenu();
    wait(800);
    CHECK(near(pillEnd(farthest), container, 2) && near(pillEnd(closest), container, 2));
    const QRect where = menu->geometry();
    menu->hide();
    CHECK(!menu->isVisible() && leftovers().size() == 1);
    CHECK(leftovers().size() == 1 && leftovers().first()->geometry() == where);
    CHECK(!near(pillEnd(closest), surface, 20)); // the items are still there
    QElapsedTimer clock;
    clock.start();
    qint64 farGone = -1, closeGone = -1;
    while (clock.elapsed() < 400 && (farGone < 0 || closeGone < 0)) {
        if (farGone < 0 && near(pillEnd(farthest), surface, 20))
            farGone = clock.elapsed();
        if (closeGone < 0 && near(pillEnd(closest), surface, 20))
            closeGone = clock.elapsed();
        QCoreApplication::processEvents();
    }
    CHECK(farGone >= 0 && closeGone >= 0 && farGone <= closeGone);
    wait(200);
    CHECK(leftovers().isEmpty());
    CHECK(near(fab->grab().toImage().pixelColor(10, 28), container, 2)); // the button is the FAB again

    // Opened again while it closes, the menu takes the place of what is left of it.
    fab->showMenu();
    wait(800);
    menu->hide();
    CHECK(leftovers().size() == 1);
    fab->showMenu();
    QCoreApplication::processEvents();
    CHECK(menu->isVisible() && leftovers().isEmpty());
    wait(800);

    // Without motion all of it is at once.
    style->setMotionEnabled(false);
    menu->hide();
    CHECK(leftovers().isEmpty());
    sheet.show();
    CHECK(sheet.geometry() == rest && near(handle->property(scrim).toDouble(), 0.32, 1e-9));
    sheet.accept();
    CHECK(!sheet.isVisible() && !ghostOf(window));
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A scroll bar's handle fades out at rest and comes back when it is scrolled or the pointer comes onto it.
void testScrollBarRest() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *area = new QScrollArea(&window);
    auto *content = new QWidget;
    content->resize(180, 1000);
    area->setWidget(content);
    area->setGeometry(10, 10, 200, 200);
    window.resize(240, 240);
    window.show();
    QCoreApplication::processEvents();
    QScrollBar *bar = area->verticalScrollBar();
    CHECK(bar->isVisible() && bar->maximum() > 0);
    const int width = bar->width();
    // The pixels of the handle in a row near the top, where it is at the start: those unlike the background at the left.
    const auto handle = [&] {
        const QImage image = bar->grab().toImage();
        int count = 0;
        for (int x = 1; x < image.width(); ++x)
            count += near(image.pixelColor(x, 20), image.pixelColor(0, 20), 3) ? 0 : 1;
        return count;
    };
    const int rest = 1500; // milliseconds before it rests
    CHECK(handle() >= 3);  // shown when it appears,
    wait(rest + 700);
    CHECK(handle() == 0);                             // gone once left alone,
    CHECK(bar->width() == width && bar->isVisible()); // without giving up its room
    bar->setValue(40);
    wait(250);
    CHECK(handle() >= 3); // back as it scrolls
    wait(rest + 700);
    CHECK(handle() == 0);
    // The pointer that comes onto it shows it; once it has left, it goes again.
    QEvent enter(QEvent::Enter);
    QCoreApplication::sendEvent(bar, &enter);
    wait(250);
    CHECK(handle() >= 3);
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(bar, &leave);
    wait(rest + 700);
    CHECK(handle() == 0);
    // "always" on the scroll area keeps it, as does a style without motion.
    area->setProperty("materialScrollBar", QStringLiteral("always"));
    QCoreApplication::processEvents();
    CHECK(handle() >= 3);
    area->setProperty("materialScrollBar", QVariant());
    QCoreApplication::processEvents();
    CHECK(handle() == 0);
    style->setMotionEnabled(false);
    CHECK(handle() >= 3);
    QApplication::setStyle(QStringLiteral("Fusion"));
}

// A resting scroll bar comes back while a mouse moves over the content of its scroll area, not for a tap.
void testScrollAreaHover() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *area = new QScrollArea(&window);
    auto *content = new QWidget;
    content->resize(180, 1000);
    area->setWidget(content);
    area->setGeometry(10, 10, 200, 200);
    window.resize(240, 240);
    window.show();
    QCoreApplication::processEvents();
    QScrollBar *bar = area->verticalScrollBar();
    QWindow *handle = window.windowHandle();
    CHECK(bar->isVisible() && handle);
    if (!handle)
        return;
    const auto shown = [&] {
        const QImage image = bar->grab().toImage();
        int count = 0;
        for (int x = 1; x < image.width(); ++x)
            count += near(image.pixelColor(x, 20), image.pixelColor(0, 20), 3) ? 0 : 1;
        return count;
    };
    const auto send = [&](QPoint at, Qt::MouseButtons buttons, Qt::MouseButton button, QEvent::Type type) {
        QWindowSystemInterface::handleMouseEvent(handle, QPointF(at), QPointF(at), buttons, button, type,
                                                 Qt::NoModifier);
        QCoreApplication::processEvents();
    };
    const auto hover = [&](QPoint at) { send(at, Qt::NoButton, Qt::NoButton, QEvent::MouseMove); };
    const QPoint content1 = area->viewport()->mapTo(&window, QPoint(40, 60));
    const QPoint content2 = area->viewport()->mapTo(&window, QPoint(60, 80));
    const int rest = 1500;
    wait(rest + 700);
    CHECK(shown() == 0);
    // The mouse that moves over the viewport wakes it.
    hover(content1);
    hover(content2);
    wait(250);
    CHECK(shown() >= 3);
    // A mouse that stops over the content lets it rest.
    wait(rest + 700);
    CHECK(shown() == 0);
    // It moves again, then leaves the area: the bar rests after the delay.
    hover(content1);
    wait(250);
    CHECK(shown() >= 3);
    hover(QPoint(4, 4));
    wait(rest + 700);
    CHECK(shown() == 0);
    hover(QPoint(5, 5)); // outside the area the moves do nothing
    wait(250);
    CHECK(shown() == 0);
    // A finger sent as the mouse holds the button: a tap does not wake it.
    send(content1, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress);
    send(content1, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease);
    wait(250);
    CHECK(shown() == 0);
    // unpolish() takes the watcher away.
    CHECK(area->findChild<QObject *>(QStringLiteral("_nt_materialScrollHover"), Qt::FindDirectChildrenOnly));
    QApplication::setStyle(QStringLiteral("Fusion"));
    CHECK(!area->findChild<QObject *>(QStringLiteral("_nt_materialScrollHover"), Qt::FindDirectChildrenOnly));
    hover(content2); // and its filters
}

// The wavy circular progress indicator: its part that is done is a wave about the circle, and its track is plain.
void testCircularWavyProgress() {
    auto *style = new QMaterialStyle;
    style->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(style);

    QWidget window;
    auto *circle = new QProgressBar(&window);
    circle->setProperty("materialProgress", QStringLiteral("circularWavy"));
    circle->setValue(50);
    circle->move(20, 20);
    circle->resize(circle->sizeHint());
    window.resize(100, 100);
    window.show();
    QCoreApplication::processEvents();
    CHECK(circle->sizeHint() == QSize(48, 48));
    // How many of the angles in a range (clockwise from the top) show `role` at a distance from the middle.
    const auto count = [&](const QImage &image, int from, int to, double radius, Role role) {
        int found = 0;
        for (int degrees = from; degrees < to; ++degrees) {
            const double radians = degrees * 3.14159265358979323846 / 180.0;
            const QColor at = image.pixelColor(static_cast<int>(24 + radius * std::sin(radians)),
                                               static_cast<int>(24 - radius * std::cos(radians)));
            found += near(at, style->color(role), 20) ? 1 : 0;
        }
        return found;
    };
    // A plain ring of 4 about a circle of 20.4 reaches from 18.4 to 22.4; the wave goes 1.6 further out and in.
    wait(600); // the wave rises
    QImage image = circle->grab().toImage();
    CHECK(count(image, 15, 165, 23.4, Role::Primary) > 20);
    CHECK(count(image, 15, 165, 17.4, Role::Primary) > 20);
    CHECK(count(image, 200, 340, 20.4, Role::SecondaryContainer) > 120); // the track is a plain circle,
    CHECK(count(image, 200, 340, 23.4, Role::SecondaryContainer) == 0);
    CHECK(count(image, 200, 340, 23.4, Role::Primary) == 0);
    // and the wave turns.
    wait(150);
    CHECK(circle->grab().toImage() != image);

    // Nearly done, it is flat.
    circle->setValue(99);
    wait(700);
    image = circle->grab().toImage();
    CHECK(count(image, 15, 340, 23.4, Role::Primary) == 0);
    CHECK(count(image, 15, 340, 20.4, Role::Primary) > 300);

    // Busy, its arc is a wave that goes round.
    circle->setRange(0, 0);
    int outside = 0;
    for (int i = 0; i < 6; ++i) {
        wait(80);
        outside = std::max(outside, count(circle->grab().toImage(), 0, 360, 23.4, Role::Primary));
    }
    CHECK(outside > 10);

    // Without motion the busy wave stands still, and is still a wave.
    style->setMotionEnabled(false);
    const QImage still = circle->grab().toImage();
    wait(150);
    CHECK(circle->grab().toImage() == still);
    CHECK(count(still, 0, 90, 23.4, Role::Primary) > 5);
    // The plain circular indicator has no wave.
    circle->setProperty("materialProgress", QStringLiteral("circular"));
    CHECK(circle->sizeHint() == QSize(40, 40));
    QApplication::setStyle(QStringLiteral("Fusion"));
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    testHct();
    testScheme();
    testWidgets();
    testSliderProgressTabs();
    testComboAndMenu();
    testMenuShapes();
    testMenuRowExtras();
    testMenuWrapsAndOpensInPlace();
    testRichToolTips();
    testSpring();
    testMotion();
    testRippleColorsAndTabs();
    testLists();
    testButtonGroup();
    testSplitButton();
    testFab();
    testFabMenuMotion();
    testBusy();
    testFloatingToolBar();
    testWavyProgress();
    testMoreRipples();
    testButtonSizes();
    testTextFields();
    testIconButtons();
    testAppBarAndNavigation();
    testSnackbarTooltipCircular();
    testChipsCardsDividers();
    testElevatedChipsAvatarsClickableCards();
    testCalendar();
    testDatePicker();
    testDatePickerYearsFade();
    testDatePickerInput();
    testDatePickerModeButton();
    testDialogOrderAndFab();
    testRailAndBadges();
    testSheetsAndDrawer();
    testSheetDragging();
    testSearchAndSecondaryTabs();
    testTimeInput();
    testSpinBoxAndScrollBar();
    testScrollBarRest();
    testScrollAreaHover();
    testCircularWavyProgress();
    testSpinBoxLabel();
    testDialogHeadline();
    testDialogHeadlineForms();
    testGroupBoxAndTextEditors();
    testMultilineFieldsAndCaret();
    testSheetAndSnackbarMotion();
    testFieldIconsAffixesCounter();
    testFieldIconButtonsAndAvatars();
    testSnackbarClose();
    testAppBarScrolled();
    testSliderSizesAndValue();
    testExpandedRail();
    testRailScrolls();
    testFlexibleAppBars();
    testMultiLineListRows();
    testListItemExtras();
    testDockedToolBarAndIconWidths();
    testAppBarSubtitlesAndScrolling();
    testSearchView();
    testExitMotion();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
