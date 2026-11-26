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
    CHECK(hasAccentText(1));
    CHECK(!hasAccentText(0));
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
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
