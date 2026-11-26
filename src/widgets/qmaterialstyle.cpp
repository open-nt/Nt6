// MIT License - see LICENSE
#include <QtWidgets/qmaterialstyle.h>

#include "qmaterialcolor_p.h"

#include <QtCore/qhash.h>
#include <QtGui/qevent.h>
#include <QtGui/qfont.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpalette.h>
#include <QtGui/qstylehints.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtabbar.h>

#include <algorithm>

namespace {

// The numbers of the design, in dp, which are the logical pixels of Nt on Android.
constexpr int buttonHeight = 40;  // the small button
constexpr int buttonPadding = 16; // before and after the label
constexpr int buttonIconSize = 20;
constexpr int buttonIconSpace = 8;
constexpr int pressedCorner = 8;   // the shape of a pressed small button
constexpr int selectedCorner = 12; // the shape of a checked button
constexpr int touchTarget = 48;    // the least height of what a finger has to hit
constexpr int textFieldHeight = 56;
constexpr int textFieldCorner = 4;
constexpr int textFieldPadding = 16;
constexpr int checkBoxSize = 18;
constexpr int radioSize = 20;
constexpr int indicatorSpace = 12;
constexpr int switchWidth = 52;
constexpr int switchHeight = 32;
constexpr int sliderTrack = 16; // the extra small slider
constexpr int sliderHandleWidth = 4;
constexpr int sliderPressedWidth = 2;
constexpr int sliderHandleHeight = 44;
constexpr int sliderGap = 6;     // between the handle and each part of the track
constexpr int sliderLength = 16; // the room of the handle along the track
constexpr int stopSize = 4;      // the dots at the ends of tracks, and of the steps of a slider
constexpr int progressThickness = 4;
constexpr int progressGap = 4;
constexpr int progressHeight = 8;
constexpr int tabHeight = 48;
constexpr int tabPadding = 16;
constexpr int tabIndicator = 3;
constexpr int focusRingWidth = 3;
constexpr double hoverOpacity = 0.08;
constexpr double focusOpacity = 0.10;
constexpr double pressedOpacity = 0.10;
constexpr double disabledContentOpacity = 0.38;
constexpr double disabledContainerOpacity = 0.10;
const char *const polishedFont = "_nt_materialFont";

QColor withOpacity(QColor color, double opacity) {
    color.setAlphaF(opacity);
    return color;
}

bool isSwitch(const QWidget *widget) { return widget && widget->property("materialSwitch").toBool(); }

bool enabled(const QStyleOption *option) { return option->state & QStyle::State_Enabled; }

void fillRounded(QPainter *painter, const QRectF &rect, double radius, const QColor &color) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRoundedRect(rect, radius, radius);
}

void strokeRounded(QPainter *painter, const QRectF &rect, double radius, const QColor &color, double width) {
    // The line lies inside the rectangle.
    const double half = width / 2;
    painter->setPen(QPen(QBrush(color), width));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect.adjusted(half, half, -half, -half), std::max(0.0, radius - half),
                             std::max(0.0, radius - half));
}

} // namespace

/*!
    \class QMaterialStyle
    \inmodule NtWidgets
    \since 6.0
    \brief The QMaterialStyle class draws widgets in the Material 3 Expressive look (Nt only).

    Make it the style of the application, by name or as an object, which also takes a seed color that the whole
    color scheme is made from:

    \code
    QApplication::setStyle(QStringLiteral("Material3"));
    // or
    QApplication::setStyle(new QMaterialStyle(QColor(0x00, 0x6c, 0x4c)));
    \endcode

    The scheme is light or dark as the system says, unless setColorScheme() says otherwise. Sizes are in logical
    pixels, which on Android are the dp of Material: buttons are 40 high, text fields 56, and check boxes and radio
    buttons have a height of at least 48 for a finger. A slider moves to where it is pressed and follows from there.

    Choices that the classes of Qt have no function for are dynamic properties, so that the same program also builds
    with Qt:

    \list
    \li \c materialButton of a button: \c "filled", \c "tonal", \c "outlined", \c "text" or \c "elevated". Without
        it the default button is filled, a flat button is a text button, and other buttons are tonal.
    \li \c materialSwitch of a QCheckBox: \c true draws it as a switch.
    \endlist

    \code
    save->setProperty("materialButton", "filled");
    wifi->setProperty("materialSwitch", true);
    \endcode

    A focus ring shows while the focus is moved with the keyboard (Tab or the arrow keys), and not after a click or a
    tap.

    \differences
    Qt Widgets has no Material style. Push buttons, line edits, check boxes, radio buttons, switches, sliders (the
    extra small size), progress bars (a thin line, with the text beside it) and tab bars (primary tabs) are drawn in
    the Material way so far; the other widgets are drawn as the Fusion style draws them, in the Material colors.
    A slider shows no value bubble, and a busy progress bar shows only its track.
    Changes of state show at once: there is no motion, no shape morphing and no ripple yet.
    \enddifferences

    \sa QStyleFactory, QApplication::setStyle()
*/

/*!
    \enum QMaterialStyle::Role

    The color roles of Material 3. A role whose name begins with \c On is for what is drawn on the color without it.

    \value Primary The accent color.
    \value OnPrimary On Primary.
    \value PrimaryContainer A softer fill in the accent hue.
    \value OnPrimaryContainer On PrimaryContainer.
    \value Secondary A quieter accent.
    \value OnSecondary On Secondary.
    \value SecondaryContainer The fill of tonal buttons.
    \value OnSecondaryContainer On SecondaryContainer.
    \value Tertiary A contrasting accent.
    \value OnTertiary On Tertiary.
    \value TertiaryContainer A softer fill in the contrasting hue.
    \value OnTertiaryContainer On TertiaryContainer.
    \value Error What is wrong.
    \value OnError On Error.
    \value ErrorContainer A softer fill for errors.
    \value OnErrorContainer On ErrorContainer.
    \value Surface The background.
    \value OnSurface On Surface.
    \value SurfaceVariant A second background.
    \value OnSurfaceVariant On SurfaceVariant, and secondary text.
    \value SurfaceContainerLowest The lowest of the container backgrounds.
    \value SurfaceContainerLow A low container background.
    \value SurfaceContainer The container background.
    \value SurfaceContainerHigh A high container background.
    \value SurfaceContainerHighest The highest of the container backgrounds.
    \value Outline Borders.
    \value OutlineVariant Quiet borders and dividers.
    \value InverseSurface A background of the opposite lightness.
    \value InverseOnSurface On InverseSurface.
    \value InversePrimary The accent on InverseSurface.
*/

/*!
    Constructs the style with the seed color of the Material baseline scheme, a violet.
*/
QMaterialStyle::QMaterialStyle() : QMaterialStyle(QColor(0x67, 0x50, 0xa4)) {}

/*!
    \overload

    Constructs the style with a scheme made from \a seedColor.
*/
QMaterialStyle::QMaterialStyle(const QColor &seedColor) : m_seed(seedColor) {}

/*!
    Destroys the style.
*/
QMaterialStyle::~QMaterialStyle() = default;

/*!
    \fn QColor QMaterialStyle::seedColor() const

    Returns the color that the scheme is made from.

    \sa setSeedColor()
*/

/*!
    Sets the seed color to \a color: its hue becomes the hue of the accents and, faintly, of the backgrounds. Set it
    before the style is made the style of the application; afterwards, set the palette of the application to
    standardPalette() and update the widgets.
*/
void QMaterialStyle::setSeedColor(const QColor &color) { m_seed = color; }

/*!
    \fn Qt::ColorScheme QMaterialStyle::colorScheme() const

    Returns the scheme set with setColorScheme(); \c Unknown follows the system.
*/

/*!
    Makes the colors light or dark as \a scheme says, or as the system says for \c Qt::ColorScheme::Unknown. As with
    setSeedColor(), set it before the style is made the style of the application.
*/
void QMaterialStyle::setColorScheme(Qt::ColorScheme scheme) { m_scheme = scheme; }

/*!
    Returns \c true if the colors are those of the dark scheme.
*/
bool QMaterialStyle::isDark() const {
    if (m_scheme != Qt::ColorScheme::Unknown)
        return m_scheme == Qt::ColorScheme::Dark;
    const QStyleHints *hints = QGuiApplication::styleHints();
    return hints && hints->colorScheme() == Qt::ColorScheme::Dark;
}

/*!
    Returns the color of \a role in the current scheme.
*/
QColor QMaterialStyle::color(Role role) const {
    // The palettes of the tonal spot scheme, and the tone of each role in the light and in the dark scheme.
    enum Palette { P, S, T, N, V, E };
    static const struct {
        Palette palette;
        int light;
        int dark;
    } roles[] = {{P, 40, 80}, {P, 100, 20}, {P, 90, 30}, {P, 10, 90},  {S, 40, 80}, {S, 100, 20},
                 {S, 90, 30}, {S, 10, 90},  {T, 40, 80}, {T, 100, 20}, {T, 90, 30}, {T, 10, 90},
                 {E, 40, 80}, {E, 100, 20}, {E, 90, 30}, {E, 10, 90},  {N, 98, 6},  {N, 10, 90},
                 {V, 90, 30}, {V, 30, 80},  {N, 100, 4}, {N, 96, 10},  {N, 94, 12}, {N, 92, 17},
                 {N, 90, 22}, {V, 50, 60},  {V, 80, 30}, {N, 20, 90},  {N, 95, 20}, {P, 80, 40}};
    const auto &entry = roles[static_cast<int>(role)];
    const QtPrivate::Hct seed = QtPrivate::Hct::fromColor(m_seed);
    double hue = seed.hue;
    double chroma = 0;
    switch (entry.palette) {
    case P:
        chroma = 36;
        break;
    case S:
        chroma = 16;
        break;
    case T:
        hue += 60;
        chroma = 24;
        break;
    case N:
        chroma = 6;
        break;
    case V:
        chroma = 8;
        break;
    case E:
        hue = 25;
        chroma = 84;
        break;
    }
    // Solving for a colour takes a while, and painting asks for the same few over and over.
    const int tone = isDark() ? entry.dark : entry.light;
    static QHash<quint64, QRgb> cache;
    const quint64 key = (quint64(m_seed.rgb()) << 16) | quint64(static_cast<int>(role) << 8) | quint64(tone);
    auto it = cache.constFind(key);
    if (it == cache.constEnd())
        it = cache.insert(key, QtPrivate::Hct::toColor(hue, chroma, tone).rgb());
    return QColor(it.value());
}

/*!
    \reimp
*/
QPalette QMaterialStyle::standardPalette() const {
    QPalette palette = QCommonStyle::standardPalette();
    const auto set = [&palette, this](QPalette::ColorRole target, Role source) {
        palette.setColor(target, color(source));
    };
    set(QPalette::Window, Role::Surface);
    set(QPalette::WindowText, Role::OnSurface);
    set(QPalette::Base, Role::Surface);
    set(QPalette::AlternateBase, Role::SurfaceContainerLow);
    set(QPalette::Text, Role::OnSurface);
    set(QPalette::Button, Role::SurfaceContainerHigh);
    set(QPalette::ButtonText, Role::OnSurface);
    set(QPalette::Highlight, Role::Primary);
    set(QPalette::HighlightedText, Role::OnPrimary);
    set(QPalette::PlaceholderText, Role::OnSurfaceVariant);
    set(QPalette::Link, Role::Primary);
    set(QPalette::ToolTipBase, Role::InverseSurface);
    set(QPalette::ToolTipText, Role::InverseOnSurface);
    set(QPalette::Light, Role::SurfaceContainerLowest);
    set(QPalette::Midlight, Role::SurfaceContainer);
    set(QPalette::Mid, Role::OutlineVariant);
    set(QPalette::Dark, Role::Outline);
    const QColor disabled = withOpacity(color(Role::OnSurface), disabledContentOpacity);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::ButtonText, QPalette::Text})
        palette.setColor(QPalette::Disabled, role, disabled);
    return palette;
}

/*!
    \reimp
*/
void QMaterialStyle::polish(QWidget *widget) {
    QCommonStyle::polish(widget);
    if (qobject_cast<QAbstractButton *>(widget))
        widget->setAttribute(Qt::WA_Hover);
    // The type scale: label large for buttons, body large for text and choices. A font that the program set stays.
    const bool choice = qobject_cast<QCheckBox *>(widget) || qobject_cast<QRadioButton *>(widget);
    const bool button = (!choice && qobject_cast<QAbstractButton *>(widget)) || qobject_cast<QTabBar *>(widget);
    if ((choice || button || qobject_cast<QLineEdit *>(widget)) && !widget->testAttribute(Qt::WA_SetFont)) {
        QFont font = widget->font();
        font.setPixelSize(button ? 14 : 16);
        if (button)
            font.setWeight(QFont::Medium);
        widget->setFont(font);
        widget->setProperty(polishedFont, true);
    }
}

/*!
    \reimp
*/
void QMaterialStyle::unpolish(QWidget *widget) {
    if (widget->property(polishedFont).toBool()) {
        widget->setFont(QFont());
        widget->setAttribute(Qt::WA_SetFont, false);
        widget->setProperty(polishedFont, QVariant());
    }
    QCommonStyle::unpolish(widget);
}

/*!
    \reimp
*/
void QMaterialStyle::polish(QApplication *application) {
    QCommonStyle::polish(application);
    if (application)
        application->installEventFilter(this);
}

/*!
    \reimp
*/
void QMaterialStyle::unpolish(QApplication *application) {
    if (application)
        application->removeEventFilter(this);
    QCommonStyle::unpolish(application);
}

/*!
    \reimp

    Notes whether the user is moving the focus with the keyboard, or points with a mouse or a finger: the focus ring
    shows only in the first case.
*/
bool QMaterialStyle::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Tab || key == Qt::Key_Backtab || key == Qt::Key_Left || key == Qt::Key_Right ||
            key == Qt::Key_Up || key == Qt::Key_Down)
            m_keyboard = true;
    } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
        m_keyboard = false;
    }
    return QCommonStyle::eventFilter(watched, event);
}

bool QMaterialStyle::keyboardFocus(const QStyleOption *option) const {
    return (option->state & State_HasFocus) && (m_keyboard || (option->state & State_KeyboardFocusChange));
}

// The opacity of the layer that shows what the pointer or the keyboard does to a control.
double QMaterialStyle::stateLayer(const QStyleOption *option) const {
    if (!enabled(option))
        return 0;
    if (option->state & State_Sunken)
        return pressedOpacity;
    if (keyboardFocus(option))
        return focusOpacity;
    return (option->state & State_MouseOver) ? hoverOpacity : 0;
}

QMaterialStyle::ButtonKind QMaterialStyle::buttonKind(const QStyleOption *option, const QWidget *widget) {
    const QString name = widget ? widget->property("materialButton").toString() : QString();
    if (name == QLatin1String("filled"))
        return ButtonKind::Filled;
    if (name == QLatin1String("tonal"))
        return ButtonKind::Tonal;
    if (name == QLatin1String("outlined"))
        return ButtonKind::Outlined;
    if (name == QLatin1String("text"))
        return ButtonKind::Text;
    if (name == QLatin1String("elevated"))
        return ButtonKind::Elevated;
    if (option->type == QStyleOption::SO_Button) {
        const auto *button = static_cast<const QStyleOptionButton *>(option);
        if (button->features & QStyleOptionButton::Flat)
            return ButtonKind::Text;
        if (button->features & QStyleOptionButton::DefaultButton)
            return ButtonKind::Filled;
    }
    return ButtonKind::Tonal;
}

QColor QMaterialStyle::buttonLabelColor(const QStyleOption *option, const QWidget *widget) const {
    if (!enabled(option))
        return withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const bool checked = option->state & State_On;
    switch (buttonKind(option, widget)) {
    case ButtonKind::Filled:
        return color(Role::OnPrimary);
    case ButtonKind::Tonal:
        return color(checked ? Role::OnSecondary : Role::OnSecondaryContainer);
    case ButtonKind::Outlined:
        return color(checked ? Role::InverseOnSurface : Role::OnSurfaceVariant);
    case ButtonKind::Elevated:
        return color(checked ? Role::OnPrimary : Role::Primary);
    case ButtonKind::Text:
        break;
    }
    return color(Role::Primary);
}

void QMaterialStyle::drawButtonContainer(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const ButtonKind kind = buttonKind(option, widget);
    const QRectF rect(option->rect);
    const bool checked = option->state & State_On;
    // A button is a pill; pressed, its corners tighten, and a checked button is squarer.
    double radius = rect.height() / 2;
    if (option->state & State_Sunken)
        radius = std::min<double>(radius, rect.height() >= 56 ? selectedCorner : pressedCorner);
    else if (checked)
        radius = std::min<double>(radius, selectedCorner);

    QColor container(Qt::transparent);
    switch (kind) {
    case ButtonKind::Filled:
        container = color(Role::Primary);
        break;
    case ButtonKind::Tonal:
        container = color(checked ? Role::Secondary : Role::SecondaryContainer);
        break;
    case ButtonKind::Elevated:
        container = color(checked ? Role::Primary : Role::SurfaceContainerLow);
        break;
    case ButtonKind::Outlined:
        if (checked)
            container = color(Role::InverseSurface);
        break;
    case ButtonKind::Text:
        break;
    }
    if (!enabled(option) && kind != ButtonKind::Text && !(kind == ButtonKind::Outlined && !checked))
        container = withOpacity(color(Role::OnSurface), disabledContainerOpacity);
    if (kind == ButtonKind::Elevated && enabled(option) && !(option->state & State_Sunken))
        fillRounded(painter, rect.adjusted(0.5, 1.5, -0.5, 0), radius, withOpacity(QColor(Qt::black), 0.18));
    const QRectF body = kind == ButtonKind::Elevated ? rect.adjusted(0, 0, 0, -1.5) : rect;
    if (container.alpha() > 0)
        fillRounded(painter, body, radius, container);
    if (kind == ButtonKind::Outlined && !checked)
        strokeRounded(painter, body, radius, color(Role::OutlineVariant), 1);
    if (const double layer = stateLayer(option); layer > 0)
        fillRounded(painter, body, radius, withOpacity(buttonLabelColor(option, widget), layer));
    if (keyboardFocus(option))
        strokeRounded(painter, body, radius, color(Role::Secondary), focusRingWidth);
}

void QMaterialStyle::drawButtonLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *button = static_cast<const QStyleOptionButton *>(option);
    const QColor ink = buttonLabelColor(option, widget);
    painter->setPen(ink);
    if (button->icon.isNull()) {
        painter->drawText(button->rect, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, button->text);
        return;
    }
    const QSize icon = button->iconSize.isValid() ? button->iconSize : QSize(buttonIconSize, buttonIconSize);
    const int textWidth = button->text.isEmpty() ? 0 : painter->fontMetrics().horizontalAdvance(button->text);
    const int space = button->text.isEmpty() ? 0 : buttonIconSpace;
    const int left = button->rect.left() + (button->rect.width() - icon.width() - space - textWidth) / 2;
    const QRect iconRect(left, button->rect.top() + (button->rect.height() - icon.height()) / 2, icon.width(),
                         icon.height());
    button->icon.paint(painter, iconRect, Qt::AlignCenter, enabled(option) ? QIcon::Normal : QIcon::Disabled,
                       (option->state & State_On) ? QIcon::On : QIcon::Off);
    QRect textRect = button->rect;
    textRect.setLeft(iconRect.right() + 1 + space);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, button->text);
}

void QMaterialStyle::drawSwitch(const QStyleOption *option, const QRect &rect, QPainter *painter) const {
    const bool on = option->state & State_On;
    const bool pressed = option->state & State_Sunken;
    const QRectF track(rect.left(), rect.top() + (rect.height() - switchHeight) / 2.0, switchWidth, switchHeight);
    QColor trackColor = color(on ? Role::Primary : Role::SurfaceContainerHighest);
    QColor border = color(Role::Outline);
    QColor handle = color(on ? Role::OnPrimary : Role::Outline);
    if (!enabled(option)) {
        const QColor ink = color(Role::OnSurface);
        trackColor = on ? withOpacity(ink, 0.12) : withOpacity(color(Role::SurfaceContainerHighest), 0.12);
        border = withOpacity(ink, 0.12);
        handle = on ? color(Role::Surface) : withOpacity(ink, disabledContentOpacity);
    }
    fillRounded(painter, track, switchHeight / 2.0, trackColor);
    if (!on)
        strokeRounded(painter, track, switchHeight / 2.0, border, 2);
    // The handle is small when off, larger when on, and larger still under a finger.
    const double size = pressed ? 28 : on ? 24 : 16;
    const double centre = on ? track.right() - switchHeight / 2.0 : track.left() + switchHeight / 2.0;
    const QPointF middle(centre, track.center().y());
    if (const double layer = stateLayer(option); layer > 0)
        fillRounded(painter, QRectF(middle.x() - 20, middle.y() - 20, 40, 40), 20,
                    withOpacity(color(on ? Role::Primary : Role::OnSurface), layer));
    fillRounded(painter, QRectF(middle.x() - size / 2, middle.y() - size / 2, size, size), size / 2, handle);
}

// A thick track in two parts, with a gap on each side of a thin handle.
void QMaterialStyle::drawSlider(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *slider = static_cast<const QStyleOptionSlider *>(option);
    const bool horizontal = slider->orientation == Qt::Horizontal;
    const QRectF groove(proxy()->subElementRect(SE_SliderGroove, option, widget));
    const QPointF centre = QRectF(proxy()->subElementRect(SE_SliderHandle, option, widget)).center();
    // Everything is worked out along the track, from its left or top end.
    const double start = horizontal ? groove.left() : groove.top();
    const double end = horizontal ? groove.right() : groove.bottom();
    const double handle = horizontal ? centre.x() : centre.y();
    const auto part = [&](double from, double to) {
        return horizontal ? QRectF(from, groove.top(), to - from, groove.height())
                          : QRectF(groove.left(), from, groove.width(), to - from);
    };
    // As in the Fusion style: the active part starts at the left, or at the bottom of a vertical slider.
    const bool activeFirst = horizontal ? !slider->upsideDown : slider->upsideDown;
    QColor active = color(Role::Primary);
    QColor inactive = color(Role::SecondaryContainer);
    QColor knob = color(Role::Primary);
    if (!enabled(option)) {
        const QColor ink = color(Role::OnSurface);
        active = knob = withOpacity(ink, disabledContentOpacity);
        inactive = withOpacity(ink, disabledContainerOpacity);
    }
    const double beforeEnd = handle - sliderHandleWidth / 2.0 - sliderGap;
    const double afterStart = handle + sliderHandleWidth / 2.0 + sliderGap;
    if (beforeEnd > start)
        fillRounded(painter, part(start, beforeEnd), std::min(sliderTrack / 2.0, (beforeEnd - start) / 2),
                    activeFirst ? active : inactive);
    if (end > afterStart)
        fillRounded(painter, part(afterStart, end), std::min(sliderTrack / 2.0, (end - afterStart) / 2),
                    activeFirst ? inactive : active);
    // Stops: one at the far end of the inactive part, or one for every step when the slider shows tick marks.
    const auto dot = [&](double at) {
        const bool onActive = activeFirst ? at < handle : at > handle;
        if (at > beforeEnd && at < afterStart)
            return;
        const QPointF middle = horizontal ? QPointF(at, groove.center().y()) : QPointF(groove.center().x(), at);
        fillRounded(painter, QRectF(middle.x() - stopSize / 2.0, middle.y() - stopSize / 2.0, stopSize, stopSize),
                    stopSize / 2.0, color(onActive ? Role::OnPrimary : Role::OnSecondaryContainer));
    };
    const double inset = sliderTrack / 2.0;
    const qint64 range = static_cast<qint64>(slider->maximum) - slider->minimum;
    if (slider->tickPosition != 0 && range > 0) {
        const qint64 interval = slider->tickInterval > 0 ? slider->tickInterval
                                : slider->pageStep > 0   ? slider->pageStep
                                                         : std::max<qint64>(1, range / 10);
        // At most one dot every two dot widths.
        if (range / interval <= static_cast<qint64>((end - start) / (2 * stopSize)))
            for (qint64 v = 0; v <= range; v += interval)
                dot(start + inset + (end - start - 2 * inset) * static_cast<double>(v) / static_cast<double>(range));
    } else {
        dot(activeFirst ? end - inset : start + inset);
    }
    const bool pressed = option->state & State_Sunken;
    const double width = pressed ? sliderPressedWidth : sliderHandleWidth;
    const QRectF bar =
        horizontal ? QRectF(handle - width / 2, centre.y() - sliderHandleHeight / 2.0, width, sliderHandleHeight)
                   : QRectF(centre.x() - sliderHandleHeight / 2.0, handle - width / 2, sliderHandleHeight, width);
    fillRounded(painter, bar, width / 2, knob);
    if (keyboardFocus(option))
        strokeRounded(painter, bar.adjusted(-4, -2, 4, 2), 6, color(Role::Secondary), 2);
}

// A thin rounded line: what is done, a gap, what is left, and a dot at the far end.
void QMaterialStyle::drawProgressBar(const QStyleOption *option, QPainter *painter) const {
    const auto *bar = static_cast<const QStyleOptionProgressBar *>(option);
    const QRect r = option->rect;
    const bool vertical = !(option->state & State_Horizontal) && r.height() > r.width();
    QRectF line(r);
    if (bar->textVisible && !bar->text.isEmpty() && !vertical) {
        const int room = painter->fontMetrics().horizontalAdvance(QStringLiteral("100%")) + 8;
        painter->setPen(
            option->palette.color(enabled(option) ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
        painter->drawText(QRect(r.right() + 1 - room, r.top(), room, r.height()),
                          static_cast<int>(Qt::AlignRight | Qt::AlignVCenter) | Qt::TextSingleLine, bar->text);
        line.setRight(line.right() - room - 4);
    }
    // Along the bar from where it starts to fill.
    const double length = vertical ? line.height() : line.width();
    const bool reversed = bar->invertedAppearance;
    const auto part = [&](double from, double to) {
        if (vertical) {
            const double top = reversed ? line.top() + from : line.bottom() - to;
            return QRectF(line.center().x() - progressThickness / 2.0, top, progressThickness, to - from);
        }
        const double left = reversed ? line.right() - to : line.left() + from;
        return QRectF(left, line.center().y() - progressThickness / 2.0, to - from, progressThickness);
    };
    const QColor active =
        enabled(option) ? color(Role::Primary) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const QColor track = enabled(option) ? color(Role::SecondaryContainer)
                                         : withOpacity(color(Role::OnSurface), disabledContainerOpacity);
    const qint64 span = static_cast<qint64>(bar->maximum) - bar->minimum;
    double done = 0;
    if (span > 0 && bar->progress > bar->minimum)
        done = length * static_cast<double>(std::min<qint64>(bar->progress, bar->maximum) - bar->minimum) /
               static_cast<double>(span);
    const double radius = progressThickness / 2.0;
    if (done > 0)
        fillRounded(painter, part(0, std::max<double>(done, progressThickness)), radius, active);
    const double rest = done > 0 ? std::max<double>(done, progressThickness) + progressGap : 0;
    if (length - rest > 0) {
        fillRounded(painter, part(rest, length), radius, track);
        if (length - rest >= stopSize)
            fillRounded(painter, part(length - stopSize, length), radius, active);
    }
}

// A primary tab: the line under the selected one is as wide as its label.
void QMaterialStyle::drawTab(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const QRectF r(option->rect);
    const bool selected = option->state & State_Selected;
    if (const double layer = stateLayer(option); layer > 0)
        painter->fillRect(r, withOpacity(color(selected ? Role::Primary : Role::OnSurface), layer));
    painter->fillRect(QRectF(r.left(), r.bottom() - 1, r.width(), 1), color(Role::OutlineVariant));
    if (!selected)
        return;
    double width = r.width() - 2 * tabPadding;
    // QTabBar draws the shape without the text: the label is that of its current tab, in its font.
    if (const auto *bar = qobject_cast<const QTabBar *>(widget); bar && bar->currentIndex() >= 0)
        width = std::min<double>(width, bar->fontMetrics().horizontalAdvance(bar->tabText(bar->currentIndex())));
    width = std::max(24.0, width);
    painter->save();
    painter->setClipRect(r, Qt::IntersectClip);
    // Twice as high as what shows, so that only the top corners are rounded.
    fillRounded(painter, QRectF(r.center().x() - width / 2, r.bottom() - tabIndicator, width, 2 * tabIndicator),
                tabIndicator, color(Role::Primary));
    painter->restore();
}

/*!
    \reimp
*/
void QMaterialStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                                   const QWidget *widget) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    switch (element) {
    case PE_PanelButtonCommand:
        drawButtonContainer(option, painter, widget);
        break;
    case PE_FrameTabWidget:
        break; // the pages of a tab widget have no frame: the line under the tabs parts them
    case PE_PanelLineEdit: {
        const auto *frame =
            option->type == QStyleOption::SO_Frame ? static_cast<const QStyleOptionFrame *>(option) : nullptr;
        fillRounded(painter, QRectF(option->rect), textFieldCorner, option->palette.color(QPalette::Base));
        if (!frame || frame->lineWidth > 0)
            proxy()->drawPrimitive(PE_FrameLineEdit, option, painter, widget);
        break;
    }
    case PE_FrameLineEdit: {
        const bool focused = option->state & State_HasFocus;
        QColor line = color(focused ? Role::Primary : Role::Outline);
        if (!enabled(option))
            line = withOpacity(color(Role::OnSurface), 0.12);
        strokeRounded(painter, QRectF(option->rect), textFieldCorner, line, focused ? 3 : 1);
        break;
    }
    case PE_IndicatorCheckBox: {
        const QRectF box(option->rect);
        const bool filled = option->state & (State_On | State_NoChange);
        QColor accent = color(filled ? Role::Primary : Role::OnSurfaceVariant);
        QColor mark = color(Role::OnPrimary);
        if (!enabled(option)) {
            accent = withOpacity(color(Role::OnSurface), disabledContentOpacity);
            mark = color(Role::Surface);
        }
        if (const double layer = stateLayer(option); layer > 0)
            fillRounded(painter, box.adjusted(-8, -8, 8, 8), box.width() / 2 + 8, withOpacity(accent, layer));
        if (!filled) {
            strokeRounded(painter, box, 2, accent, 2);
            break;
        }
        fillRounded(painter, box, 2, accent);
        painter->setPen(QPen(QBrush(mark), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const auto at = [&box](double x, double y) {
            return QPointF(box.left() + x * box.width(), box.top() + y * box.height());
        };
        if (option->state & State_On) {
            painter->drawLine(at(0.22, 0.52), at(0.42, 0.72));
            painter->drawLine(at(0.42, 0.72), at(0.79, 0.30));
        } else {
            painter->drawLine(at(0.25, 0.5), at(0.75, 0.5));
        }
        break;
    }
    case PE_IndicatorRadioButton: {
        const QRectF box(option->rect);
        const bool on = option->state & State_On;
        QColor accent = color(on ? Role::Primary : Role::OnSurfaceVariant);
        if (!enabled(option))
            accent = withOpacity(color(Role::OnSurface), disabledContentOpacity);
        if (const double layer = stateLayer(option); layer > 0)
            fillRounded(painter, box.adjusted(-8, -8, 8, 8), box.width() / 2 + 8, withOpacity(accent, layer));
        strokeRounded(painter, box, box.width() / 2, accent, 2);
        if (on)
            fillRounded(painter, box.adjusted(5, 5, -5, -5), box.width() / 2 - 5, accent);
        break;
    }
    default:
        QCommonStyle::drawPrimitive(element, option, painter, widget);
        break;
    }
    painter->restore();
}

/*!
    \reimp
*/
void QMaterialStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                                 const QWidget *widget) const {
    if (element == CE_PushButtonLabel && option->type == QStyleOption::SO_Button) {
        painter->save();
        drawButtonLabel(option, painter, widget);
        painter->restore();
        return;
    }
    if (element == CE_CheckBoxLabel && option->type == QStyleOption::SO_Button) {
        // No underline: the layer around the indicator shows the focus.
        painter->save();
        const auto *button = static_cast<const QStyleOptionButton *>(option);
        painter->setPen(
            option->palette.color(enabled(option) ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
        painter->drawText(button->rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, button->text);
        painter->restore();
        return;
    }
    if ((element == CE_Slider && option->type == QStyleOption::SO_Slider) ||
        (element == CE_ProgressBar && option->type == QStyleOption::SO_ProgressBar) ||
        (element == CE_TabBarTab && option->type == QStyleOption::SO_Tab)) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (element == CE_Slider)
            drawSlider(option, painter, widget);
        else if (element == CE_ProgressBar)
            drawProgressBar(option, painter);
        else
            drawTab(option, painter, widget);
        painter->restore();
        return;
    }
    if (element == CE_CheckBox && isSwitch(widget)) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        drawSwitch(option, proxy()->subElementRect(SE_CheckBoxIndicator, option, widget), painter);
        painter->restore();
        QStyleOptionButton label = *static_cast<const QStyleOptionButton *>(option);
        label.rect = proxy()->subElementRect(SE_CheckBoxContents, option, widget);
        proxy()->drawControl(CE_CheckBoxLabel, &label, painter, widget);
        return;
    }
    QCommonStyle::drawControl(element, option, painter, widget);
}

/*!
    \reimp
*/
QRect QMaterialStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const {
    const QRect r = option->rect;
    switch (element) {
    case SE_PushButtonContents:
        // A narrow button keeps what room it has for its label.
        return r.adjusted(std::min(buttonPadding, r.width() / 4), 0, -std::min(buttonPadding, r.width() / 4), 0);
    case SE_SliderGroove: {
        const auto *slider = static_cast<const QStyleOptionSlider *>(option);
        if (slider->orientation == Qt::Horizontal)
            return QRect(r.left(), r.top() + (r.height() - sliderTrack) / 2, r.width(), sliderTrack);
        return QRect(r.left() + (r.width() - sliderTrack) / 2, r.top(), sliderTrack, r.height());
    }
    case SE_CheckBoxIndicator:
        if (isSwitch(widget))
            return QRect(r.left(), r.top() + (r.height() - switchHeight) / 2, switchWidth, switchHeight);
        break;
    case SE_CheckBoxContents:
        if (isSwitch(widget))
            return r.adjusted(switchWidth + indicatorSpace, 0, 0, 0);
        break;
    case SE_LineEditContents: {
        const bool framed =
            option->type != QStyleOption::SO_Frame || static_cast<const QStyleOptionFrame *>(option)->lineWidth > 0;
        if (framed)
            return r.adjusted(textFieldPadding, 3, -textFieldPadding, -3);
        break;
    }
    default:
        break;
    }
    return QCommonStyle::subElementRect(element, option, widget);
}

/*!
    \reimp
*/
int QMaterialStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                              QStyleHintReturn *returnData) const {
    // A finger moves a slider to where it touches it.
    if (hint == SH_Slider_AbsoluteSetButtons)
        return static_cast<int>(Qt::LeftButton) | static_cast<int>(Qt::MiddleButton);
    return QCommonStyle::styleHint(hint, option, widget, returnData);
}

/*!
    \reimp
*/
int QMaterialStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const {
    switch (metric) {
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
        return checkBoxSize;
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return radioSize;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return indicatorSpace;
    case PM_SliderLength:
        return sliderLength;
    case PM_SliderThickness:
    case PM_SliderControlThickness:
        return sliderHandleHeight;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 0;
    default:
        return QCommonStyle::pixelMetric(metric, option, widget);
    }
}

/*!
    \reimp
*/
QSize QMaterialStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                                       const QWidget *widget) const {
    switch (type) {
    case CT_PushButton: {
        int width = contentsSize.width();
        int height = contentsSize.height();
        const auto *button = option && option->type == QStyleOption::SO_Button
                                 ? static_cast<const QStyleOptionButton *>(option)
                                 : nullptr;
        if (button && !button->icon.isNull()) {
            const QSize icon = button->iconSize.isValid() ? button->iconSize : QSize(buttonIconSize, buttonIconSize);
            width += icon.width() + (button->text.isEmpty() ? 0 : buttonIconSpace);
            height = std::max(height, icon.height());
        }
        return QSize(std::max(touchTarget, width + 2 * buttonPadding), std::max(buttonHeight, height + 8));
    }
    case CT_ProgressBar:
        return QSize(contentsSize.width(), std::max(progressHeight, contentsSize.height()));
    case CT_TabBarTab:
        return QSize(std::max(touchTarget, contentsSize.width() + 2 * tabPadding), tabHeight);
    case CT_CheckBox:
    case CT_RadioButton: {
        const int indicator = isSwitch(widget) ? switchWidth : type == CT_CheckBox ? checkBoxSize : radioSize;
        return QSize(indicator + (contentsSize.width() > 0 ? indicatorSpace + contentsSize.width() : 0),
                     std::max(touchTarget, contentsSize.height()));
    }
    case CT_LineEdit: {
        const bool framed = !option || option->type != QStyleOption::SO_Frame ||
                            static_cast<const QStyleOptionFrame *>(option)->lineWidth > 0;
        if (framed)
            return QSize(contentsSize.width() + 2 * textFieldPadding, std::max(textFieldHeight, contentsSize.height()));
        break;
    }
    default:
        break;
    }
    return QCommonStyle::sizeFromContents(type, option, contentsSize, widget);
}

#include "moc_qmaterialstyle.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
