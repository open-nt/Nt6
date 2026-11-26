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

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    testHct();
    testScheme();
    testWidgets();
    testSliderProgressTabs();
    testComboAndMenu();
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
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
