// MIT License - see LICENSE
#include <QtWidgets/qmaterialstyle.h>

#include "qmaterialcolor_p.h"
#include "qmaterialmotion_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qhash.h>
#include <QtCore/qscopeguard.h>
#include <QtGui/qevent.h>
#include <QtGui/qfont.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpalette.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qscroller.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtabbar.h>
#include <QtWidgets/qtoolbar.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtreeview.h>

#include <algorithm>
#ifdef __ANDROID__
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#endif

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
constexpr int choiceCentre = 20; // where the middle of a check box, a radio button or a switch's handle is, and the
constexpr int choiceRadius = 20; // radius of the circle around it that shows a press
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
constexpr double waveHeight = 3;       // of the wavy progress bar, above and below its line,
constexpr double waveLength = 40;      // from one crest to the next,
constexpr double busyWaveLength = 20;  // and of a busy one
constexpr double waveSpeed = 40;       // how far it travels in a second
constexpr int wavyProgressHeight = 14; // the room of a wavy bar
constexpr int progressGap = 4;
constexpr int progressHeight = 8;
constexpr int tabHeight = 48;
constexpr int tabPadding = 16;
constexpr int tabIndicator = 3;
constexpr int groupSpace = 2;          // between the buttons of a connected group
constexpr double groupCorner = 8;      // the corners that they turn to each other,
constexpr double groupPressed = 4;     // and those while a button is pressed
constexpr int splitTrailing = 48;      // the part of a split button that opens the menu
constexpr int splitLeadingSpace = 16;  // before the label of the other part,
constexpr int splitTrailingSpace = 12; // and after it
constexpr double splitCorner = 4;      // the corners that the two parts turn to each other,
constexpr double splitPressed = 12;    // and that of a part that is pressed
constexpr int fabSize = 56;
constexpr double fabCorner = 16;
constexpr int fabIconSize = 24;
constexpr int fabIconSpace = 8;
constexpr int fabPadding = 16;      // before and after the label of an extended FAB
constexpr int fabShadow = 2;        // the room under a FAB for its shadow
constexpr int fabMenuItemSpace = 4; // between the items of a FAB menu
constexpr int fabMenuItemPadding = 24;
constexpr int fabMenuGap = 8; // between a FAB menu and its button
constexpr double fabMenuStagger = 0.03; // seconds between one item of a FAB menu setting off and the next
constexpr double fabMenuFade = 0.1;     // seconds in which an item that sets off fades in
const char *const fabMenuOpened = "_nt_fabMenuOpened"; // when the menu was opened, on the animator's clock
const char *const fabMenuProperty = "_nt_fabMenu";
constexpr int loadingSize = 48;           // the container of the loading indicator
constexpr double loadingRadius = 19;      // of the shape that turns and changes in it
constexpr double loadingStep = 0.65;      // seconds from one shape to the next
constexpr double busyPeriod = 1.8;        // seconds for a part of a busy progress bar to cross it
constexpr int toolBarTarget = 48;         // a button of a floating toolbar, as a finger finds it,
constexpr int toolBarInset = 4;           // and what it leaves around the shape that is drawn
constexpr int toolBarShadow = 2;          // the room under a floating toolbar for its shadow
constexpr double listRadius = 4;          // the corners of a row of a list,
constexpr double listSelectedRadius = 16; // and of one that is chosen or held
constexpr double listPressSlop = 8;       // how far a press may stray and still be one
constexpr int menuItemHeight = 48;
constexpr int menuPadding = 16;
constexpr int menuIconSize = 24;
constexpr int menuIconSpace = 12;
constexpr int menuSeparator = 9;
constexpr int menuMinimumWidth = 112;
constexpr int comboArrowRoom = 48;
constexpr int focusRingWidth = 3;
constexpr int dialogCorner = 28; // extra large
constexpr double hoverOpacity = 0.08;
constexpr double focusOpacity = 0.10;
constexpr double pressedOpacity = 0.10;
constexpr double disabledContentOpacity = 0.38;
constexpr double disabledContainerOpacity = 0.10;
// What of a widget a spring moves.
bool isList(const QWidget *widget) {
    return qobject_cast<const QListView *>(widget) || qobject_cast<const QListWidget *>(widget);
}

// A rectangle with one radius for its two corners at the left and another for those at the right.
QPainterPath sidedPath(const QRectF &r, double left, double right) {
    const double most = std::min(r.width(), r.height()) / 2;
    const double a = 2 * std::clamp(left, 0.01, std::max(0.01, most));
    const double b = 2 * std::clamp(right, 0.01, std::max(0.01, most));
    QPainterPath path;
    path.arcMoveTo(QRectF(r.left(), r.top(), a, a), 180);
    path.arcTo(QRectF(r.left(), r.top(), a, a), 180, -90);
    path.arcTo(QRectF(r.right() - b, r.top(), b, b), 90, -90);
    path.arcTo(QRectF(r.right() - b, r.bottom() - b, b, b), 0, -90);
    path.arcTo(QRectF(r.left(), r.bottom() - a, a, a), 270, -90);
    path.closeSubpath();
    return path;
}

// True if the buttons that `widget` holds form a connected button group.
bool isConnectedGroup(const QWidget *widget) {
    return widget && widget->property("materialButtonGroup").toString() == QLatin1String("connected");
}

// Where a button is in its connected group: whether it is the first or the last of its row. One that is in no group
// is both, so that all its corners are those of an end.
struct Segment {
    bool grouped = false;
    bool first = true;
    bool last = true;
};

Segment segmentOf(const QWidget *widget) {
    Segment segment;
    const QWidget *group = widget ? widget->parentWidget() : nullptr;
    if (!isConnectedGroup(group))
        return segment;
    segment.grouped = true;
    const QRect own = widget->geometry();
    for (const QObject *child : group->children()) {
        const auto *other = qobject_cast<const QAbstractButton *>(child);
        if (!other || other == widget || other->isHidden() || other->geometry().top() > own.bottom() ||
            other->geometry().bottom() < own.top())
            continue;
        if (other->geometry().left() < own.left())
            segment.first = false;
        else
            segment.last = false;
    }
    return segment;
}

bool isFab(const QWidget *widget) {
    return qobject_cast<const QPushButton *>(widget) &&
           widget->property("materialButton").toString() == QLatin1String("fab");
}

// A FAB whose menu is open is the button that closes it.
bool isOpenFab(const QWidget *widget) {
    const auto *button = qobject_cast<const QPushButton *>(widget);
    return button && isFab(widget) && button->menu() && button->menu()->isVisible();
}

bool isFabMenu(const QWidget *widget) { return widget && widget->property(fabMenuProperty).toBool(); }

bool isSplitButton(const QWidget *widget) {
    const auto *button = qobject_cast<const QToolButton *>(widget);
    return button && button->popupMode() == QToolButton::MenuButtonPopup &&
           (button->menu() || (button->defaultAction() && button->defaultAction()->menu<QMenu *>()));
}

// The text of a menu item without the ampersand that marks its mnemonic (two stand for one) and without the shortcut.
QString menuLabel(const QString &text) {
    const qsizetype tab = text.indexOf(QLatin1Char('\t'));
    const QString marked = tab < 0 ? text : text.left(tab);
    QString label;
    for (qsizetype i = 0; i < marked.size(); ++i) {
        if (marked.at(i) == QLatin1Char('&') && i + 1 < marked.size())
            ++i;
        label.append(marked.at(i));
    }
    return label;
}

// The type of the items of a FAB menu: title medium.
QFont fabMenuFont(QFont font) {
    font.setPixelSize(16);
    font.setWeight(QFont::Medium);
    return font;
}

int fabMenuItemWidth(const QStyleOptionMenuItem &item) {
    return 2 * fabMenuItemPadding + (item.icon.isNull() ? 0 : fabIconSize + fabIconSpace) +
           QFontMetrics(fabMenuFont(item.font)).horizontalAdvance(menuLabel(item.text));
}

// How the loading indicator is to be drawn, if a progress bar asks for it with its property and is busy.
enum class Loading { No, Plain, Contained };

Loading loadingOf(const QWidget *widget) {
    const QString name = widget ? widget->property("materialProgress").toString() : QString();
    if (name == QLatin1String("loading"))
        return Loading::Plain;
    if (name == QLatin1String("containedLoading"))
        return Loading::Contained;
    return Loading::No;
}

// The shapes that the loading indicator goes through: circles with `lobes` swellings, each `depth` of the radius high.
// They are Nt's own.
struct LoadingShape {
    int lobes;
    double depth;
};
constexpr LoadingShape loadingShapes[] = {{10, 0.07}, {9, 0.045}, {5, 0.09}, {2, 0.22},
                                          {8, 0.10},  {4, 0.13},  {2, 0.12}};

// The kind of floating toolbar that a widget asks to be with its property.
QString toolBarKind(const QWidget *widget) {
    return qobject_cast<const QToolBar *>(widget) ? widget->property("materialToolBar").toString() : QString();
}

bool isVibrantToolBar(const QWidget *widget) { return toolBarKind(widget) == QLatin1String("floatingVibrant"); }

bool isToolBarItem(const QWidget *widget) {
    if (!qobject_cast<const QToolButton *>(widget))
        return false;
    const QString kind = toolBarKind(widget->parentWidget());
    return kind == QLatin1String("floating") || kind == QLatin1String("floatingVibrant");
}

// How far in from the left a check box, a radio button or a switch is drawn, so that the circle of a press fits: its
// middle, or the middle of the handle of a switch that is off, is choiceCentre in.
int choiceInset(int width, bool toggle) { return choiceCentre - (toggle ? switchHeight : width) / 2; }

// The seed of a style that is given none.
QColor defaultSeedColor() {
    const QColor system = QMaterialStyle::systemSeedColor();
    return system.isValid() ? system : QColor(0x67, 0x50, 0xa4);
}

enum Channel {
    CornerChannel, // of the corners at the left
    RightCornerChannel,
    GlyphChannel, // how far the chevron of a split button, or the plus of a FAB, has turned
    SwitchPositionChannel,
    SwitchSizeChannel,
    SwitchColorChannel,
    RadioDotChannel,
    CheckChannel,
    HandleWidthChannel,
    RippleRadiusChannel,
    RippleOpacityChannel,
    TabCentreChannel,
    TabWidthChannel,
    WaveChannel
};
const char *const polishedFont = "_nt_materialFont";
const char *const polishedPalette = "_nt_materialPalette";

QColor withOpacity(QColor color, double opacity) {
    color.setAlphaF(opacity);
    return color;
}

bool isSwitch(const QWidget *widget) { return widget && widget->property("materialSwitch").toBool(); }

bool enabled(const QStyleOption *option) { return option->state & QStyle::State_Enabled; }

// The color that is `amount` of the way from one color to another.
QColor blend(const QColor &from, const QColor &to, double amount) {
    const auto channel = [amount](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * amount)); };
    return QColor(channel(from.red(), to.red()), channel(from.green(), to.green()), channel(from.blue(), to.blue()),
                  channel(from.alpha(), to.alpha()));
}

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
    // or, with colors of its own
    QApplication::setStyle(new QMaterialStyle(QColor(0x00, 0x6c, 0x4c)));
    \endcode

    Without a seed color, the scheme is made from the color of the system's theme where there is one, on Android 12
    and later (see systemSeedColor()).

    The scheme is light or dark as the system says, unless setColorScheme() says otherwise. Sizes are in logical
    pixels, which on Android are the dp of Material: buttons are 40 high, text fields 56, and check boxes and radio
    buttons have a height of at least 48 for a finger, as do the rows of menus and of the list of a combo box. A slider
    moves to where it is pressed and follows from there.

    A QToolButton in the \c MenuButtonPopup mode, with a menu, is drawn as a split button: its action and the part that
    opens the menu are two shapes, and the second turns round while the menu is open. This is Qt's own API.

    Choices that the classes of Qt have no function for are dynamic properties, so that the same program also builds
    with Qt:

    \list
    \li \c materialButton of a button: \c "filled", \c "tonal", \c "outlined", \c "text" or \c "elevated". Without
        it the default button is filled, a flat button is a text button, and other buttons are tonal.
    \li \c materialButton \c "fab" of a QPushButton makes it a floating action button: 56 square, or wider with a
        text (an extended FAB). Without an icon and a text it shows a plus. A menu that it is given with
        QPushButton::setMenu() opens as Material's FAB menu: a pill for each action above the button, which turns into
        the button that closes it. A layout stretches a FAB like any button, so add it with an alignment.
    \li \c materialProgress of a QProgressBar: \c "wavy" draws the part that is done as a wave, which travels while
        motion is enabled and is flat below a tenth and from 95 in 100; a busy bar's parts are waves too. \c "loading" shows a busy bar (a range of 0 to 0) as the loading
        indicator, a shape of 38 that turns and changes, and \c "containedLoading" puts it in a round container of 48.
    \li \c materialToolBar of a QToolBar: \c "floating" makes it a floating toolbar, a pill of 64 with a shadow
        that goes over the content, in a row or, with a vertical orientation, in a column; \c "floatingVibrant" is the
        same in the primary container color. Its buttons have no container until they are checked. It is as long as
        its buttons need, so place it with an alignment, or as a child that is moved over the content. Set the property
        before the toolbar is shown: it is read when actions are added, when the toolbar is shown and when the style
        changes.
    \li \c materialSwitch of a QCheckBox: \c true draws it as a switch.
    \li \c materialButtonGroup of a widget: \c "connected" makes the buttons that are its children a connected
        button group, which Material 3 Expressive has in the place of segmented buttons. A horizontal layout of the
        widget puts them 2 apart unless it is told a spacing; the ends of the row are round, the corners between the
        buttons small, and the checked button is a pill. For a choice of one, make the buttons checkable and
        auto-exclusive.
    \endlist

    \code
    save->setProperty("materialButton", "filled");
    period->setProperty("materialButtonGroup", "connected"); // the widget that holds Day, Week and Month
    wifi->setProperty("materialSwitch", true);
    \endcode

    Changes of shape and position move along springs, colors fade, a press on a button spreads a ripple from where it
    is, and the line under the tabs slides to the one that is chosen (see setMotionEnabled()).

    A focus ring shows while the focus is moved with the keyboard (Tab or the arrow keys), and not after a click or a
    tap.

    \differences
    Qt Widgets has no Material style. Push buttons, line edits, check boxes, radio buttons, switches, sliders (the
    extra small size), progress bars (a thin line, with the text beside it), tab bars (primary tabs), combo boxes
    (the outlined exposed dropdown menu), menus, lists and dialogs are drawn in the Material way so far; the other
    widgets are drawn as the Fusion style draws them, in the Material colors. Material has no tree and no table:
    those show their selection in the Material colors, and the rows of a tree are 48 high. The rows of a QListView
    or QListWidget are 56 high; behind a QProxyStyle they are drawn as Fusion draws them. A finger that is dragged
    on a view selects, unless QScroller::grabGesture() makes it scroll. Menus
    have square corners. Dialogs get the Material container color and text buttons; their rounded corners and the
    dimming behind them are the android platform's. A QMessageBox has no headline type style.
    Split buttons are of the small size, and FABs of the standard one. The items of a FAB menu open one after another
    but close at once, have no check marks, and a row can be chosen anywhere across the menu, left of its pill too.
    A connected button group is of the small size and lies across; Material's standard button group, whose pressed
    button widens, is not drawn differently from buttons in a row. A slider shows no value bubble. A busy progress bar moves while motion is enabled, which the other styles of Nt
    do not do. The shapes of the loading indicator are Nt's own, not Material's, and it shows only that something is
    going on, not how far it is. A wavy bar lies across; a vertical one is straight. While its wave travels it is painted at every frame. A floating toolbar does not hide as the content is scrolled, and has no menu for
    what does not fit. Icons keep their own colors. Buttons, check boxes, radio buttons, switches, the rows of
    lists and menus, and the tabs of a horizontal QTabBar ripple; trees and tables do not. A check box, a radio button
    and a switch are drawn a little in from the left, their middle 20 in, so that the circle of the press fits. The colors of buttons, labels and radio
    buttons change at once. The line under the tabs slides on a horizontal QTabBar, and not on one at the side. In an
    item view nothing moves.
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
    Constructs the style with the system's color, systemSeedColor(), where there is one: the application then has the
    colors that the user chose for the device. Elsewhere the seed is that of the Material baseline scheme, a violet.

    \sa systemSeedColor()
*/
QMaterialStyle::QMaterialStyle() : QMaterialStyle(defaultSeedColor()) {}

/*!
    \since 6.0

    Returns the color that the system makes its own theme from, which on Android 12 and later follows the wallpaper or
    the user's choice (dynamic color): the middle tone of the system's first accent palette. On an older Android and on
    other platforms the color is invalid.

    The style makes its scheme from this one color, as it does from any seed. It does not take the other colors of the
    system's palettes, so they are close to those of the system's own applications and not always the same; and a theme
    of greys gives a scheme with a color. The color is read when this is called: a style does not follow a later change
    of the system's theme.
*/
QColor QMaterialStyle::systemSeedColor() {
#ifdef __ANDROID__
    constexpr int dynamicColorLevel = 31; // Android 12, which has the system's palettes
    if (QNativeInterface::QAndroidApplication::sdkVersion() >= dynamicColorLevel) {
        const jint id = QJniObject::getStaticField<jint>("android/R$color", "system_accent1_500");
        const QJniObject context(QNativeInterface::QAndroidApplication::context());
        if (id != 0 && context.isValid()) {
            const jint argb = context.callMethod<jint>("getColor", "(I)I", id);
            if (argb != 0) // nothing came back
                return QColor::fromRgb(static_cast<QRgb>(argb));
        }
    }
#endif
    return QColor();
}

/*!
    \overload

    Constructs the style with a scheme made from \a seedColor.
*/
QMaterialStyle::QMaterialStyle(const QColor &seedColor)
    : m_seed(seedColor), m_animator(std::make_unique<QtPrivate::Animator>()) {
#ifdef __ANDROID__
    // "Remove animations" of the accessibility settings.
    m_motion = QJniObject::callStaticMethod<jboolean>("android/animation/ValueAnimator", "areAnimatorsEnabled", "()Z");
#endif
}

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
    \fn bool QMaterialStyle::isMotionEnabled() const

    Returns \c true if changes of state move along springs.

    \sa setMotionEnabled()
*/

/*!
    If \a enabled is \c true, which is the default, a button's corners, a switch's handle, the dot of a radio button,
    a slider's handle and the line under the tabs move along the springs of Material's expressive motion scheme, the
    colors of switches and check boxes fade, and buttons ripple; otherwise changes show at once.
    On Android motion starts off when the system's accessibility settings remove animations.
*/
void QMaterialStyle::setMotionEnabled(bool enabled) {
    m_motion = enabled;
    if (!enabled)
        m_animator->clear();
}

double QMaterialStyle::animated(const QWidget *widget, int channel, double target, const QtPrivate::SpringSpec &spring,
                                double precision) const {
    // Only widgets that draw one such thing each: a view draws many indicators with the same widget.
    if (!m_motion || !(qobject_cast<const QAbstractButton *>(widget) || qobject_cast<const QSlider *>(widget) ||
                       qobject_cast<const QTabBar *>(widget)))
        return target;
    return m_animator->value(widget, channel, target, spring, precision);
}

// A circle that spreads from where the button was pressed until it fills it, and fades once the press has ended.
void QMaterialStyle::drawRipple(QPainter *painter, const QWidget *widget, const QPainterPath &outline,
                                const QRectF &shape, const QPointF &origin, bool held, const QColor &ink) const {
    if (!m_motion || !widget || widget != m_rippleWidget.data())
        return;
    // A view's viewport ripples too, so this asks the animator itself rather than animated().
    const double spread =
        std::clamp(m_animator->value(widget, RippleRadiusChannel, 1.0, QtPrivate::slowEffectsSpring, 0.004), 0.0, 1.0);
    // A tap is over before the ripple has spread: it stays until it nearly has.
    const double strength = std::clamp(m_animator->value(widget, RippleOpacityChannel, held || spread < 0.9 ? 1.0 : 0.0,
                                                         QtPrivate::slowEffectsSpring, 0.004),
                                       0.0, 1.0);
    if (strength < 0.01)
        return;
    // As far as the corner that is farthest from where the press was.
    const double dx = std::max(std::abs(origin.x() - shape.left()), std::abs(shape.right() - origin.x()));
    const double dy = std::max(std::abs(origin.y() - shape.top()), std::abs(shape.bottom() - origin.y()));
    const double reach = spread * std::hypot(dx, dy);
    if (reach <= 0)
        return;
    QPainterPath circle;
    circle.addEllipse(origin, reach, reach);
    painter->fillPath(outline.intersected(circle), QBrush(withOpacity(ink, pressedOpacity * strength)));
}

void QMaterialStyle::drawChoicePress(const QStyleOption *option, QPainter *painter, const QWidget *widget,
                                     const QPointF &centre, const QColor &ink) const {
    const QRectF around(centre.x() - choiceRadius, centre.y() - choiceRadius, 2 * choiceRadius, 2 * choiceRadius);
    // With motion, the ripple shows the press; the layer is for the pointer and the keyboard.
    QStyleOption resting = *option;
    if (m_motion && widget && widget == m_rippleWidget.data())
        resting.state &= ~QStyle::State(State_Sunken);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (const double layer = stateLayer(&resting); layer > 0)
        fillRounded(painter, around, choiceRadius, withOpacity(ink, layer));
    QPainterPath circle;
    circle.addEllipse(centre, choiceRadius, choiceRadius);
    // From the middle of the control, wherever on the button the press was.
    drawRipple(painter, widget, circle, around, centre, option->state & State_Sunken, ink);
    painter->restore();
}

void QMaterialStyle::drawListRow(QPainter *painter, const QRect &row, bool selected, bool enabled,
                                 const QBrush &background, const QWidget *view) const {
    const auto *list = qobject_cast<const QAbstractItemView *>(view);
    const QWidget *viewport = list ? list->viewport() : nullptr;
    const bool pressedHere = viewport && viewport == m_rippleWidget.data() && row.contains(m_rippleOrigin.toPoint());
    const bool held = pressedHere && m_rowPressed;
    const QColor ink = color(selected ? Role::OnSecondaryContainer : Role::OnSurface);
    // A little apart, so that two rows that are selected show as two. A row that is chosen or held is rounder.
    const QRectF shape = QRectF(row).adjusted(0, 1, 0, -1);
    const double radius = selected || held ? listSelectedRadius : listRadius;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (selected)
        fillRounded(painter, shape, radius,
                    enabled ? color(Role::SecondaryContainer)
                            : withOpacity(color(Role::OnSurface), disabledContainerOpacity));
    else if (background.style() != Qt::NoBrush)
        painter->fillRect(row, background);
    if (held && !m_motion)
        fillRounded(painter, shape, radius, withOpacity(ink, pressedOpacity));
    if (pressedHere)
        drawRipple(painter, viewport, sidedPath(shape, radius, radius), shape, m_rippleOrigin, held, ink);
    painter->restore();
}

void QMaterialStyle::drawTabIndicator(QPainter *painter, const QTabBar *bar, const QRect &tab, bool below) const {
    if (const int pressed = bar && bar == m_rippleWidget.data() ? bar->tabAt(m_rippleOrigin.toPoint()) : -1;
        pressed >= 0) {
        const QRectF shape(bar->tabRect(pressed));
        QPainterPath outline;
        outline.addRect(shape);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        drawRipple(painter, bar, outline, shape, m_rippleOrigin, m_rowPressed,
                   color(pressed == bar->currentIndex() ? Role::Primary : Role::OnSurface));
        painter->restore();
    }
    if (!bar || bar->currentIndex() < 0)
        return;
    // As wide as the label, on its way from where it was.
    const double label = bar->fontMetrics().horizontalAdvance(bar->tabText(bar->currentIndex()));
    const double width = std::max(24.0, std::min<double>(label, tab.width() - 2 * tabPadding));
    const double centre =
        animated(bar, TabCentreChannel, QRectF(tab).center().x(), QtPrivate::defaultSpatialSpring, 0.1);
    const double shown = std::max(24.0, animated(bar, TabWidthChannel, width, QtPrivate::defaultSpatialSpring, 0.1));
    const double edge = below ? tab.top() : tab.bottom() + 1.0;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipRect(QRectF(0, tab.top(), bar->width(), tab.height()), Qt::IntersectClip);
    // Twice as high as what shows, so that only the corners towards the labels are rounded.
    fillRounded(painter, QRectF(centre - shown / 2, edge - tabIndicator, shown, 2 * tabIndicator), tabIndicator,
                color(Role::Primary));
    painter->restore();
}

/*!
    \reimp
*/
void QMaterialStyle::polish(QWidget *widget) {
    QCommonStyle::polish(widget);
    if (qobject_cast<QAbstractButton *>(widget))
        widget->setAttribute(Qt::WA_Hover);
    if (qobject_cast<QDialog *>(widget) && !widget->testAttribute(Qt::WA_SetPalette)) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Window, color(Role::SurfaceContainerHigh));
        widget->setPalette(palette);
        widget->setProperty(polishedPalette, true);
    }
    // Lists, trees and tables mark what is selected quietly, as menus do.
    if (qobject_cast<QAbstractItemView *>(widget) && !widget->testAttribute(Qt::WA_SetPalette)) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Highlight, color(Role::SecondaryContainer));
        palette.setColor(QPalette::HighlightedText, color(Role::OnSecondaryContainer));
        widget->setPalette(palette);
        widget->setProperty(polishedPalette, true);
    }
    // The type scale: label large for buttons, body large for text and choices. A font that the program set stays.
    const bool choice = qobject_cast<QCheckBox *>(widget) || qobject_cast<QRadioButton *>(widget);
    const bool button = (!choice && qobject_cast<QAbstractButton *>(widget)) || qobject_cast<QTabBar *>(widget);
    const bool text = qobject_cast<QLineEdit *>(widget) || qobject_cast<QComboBox *>(widget) ||
                      qobject_cast<QMenu *>(widget) || isList(widget) || qobject_cast<QTreeView *>(widget);
    if ((choice || button || text) && !widget->testAttribute(Qt::WA_SetFont)) {
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
    if (widget->property(polishedPalette).toBool()) {
        widget->setPalette(QPalette());
        widget->setAttribute(Qt::WA_SetPalette, false);
        widget->setProperty(polishedPalette, QVariant());
    }
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
    if (event->type() == QEvent::Show) {
        // The android platform rounds the corners of a dialog's window by this property.
        if (auto *dialog = qobject_cast<QDialog *>(watched); dialog && dialog->isWindow() && dialog->windowHandle())
            dialog->windowHandle()->setProperty("_nt_cornerRadius", dialogCorner);
    }
    // What is hidden is no longer pressed: a menu that closed with a choice must not show it when it opens again.
    if (event->type() == QEvent::Hide && m_rippleWidget && watched == m_rippleWidget.data()) {
        m_rippleWidget = nullptr;
        m_rowPressed = false;
    }
    if (event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Tab || key == Qt::Key_Backtab || key == Qt::Key_Left || key == Qt::Key_Right ||
            key == Qt::Key_Up || key == Qt::Key_Down)
            m_keyboard = true;
    } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
        m_keyboard = false;
    }
    // The row of a list that is pressed. A finger or a pointer that moves away is not pressing it any more.
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
        (m_rowPressed && event->type() == QEvent::MouseMove)) {
        auto *viewport = qobject_cast<QWidget *>(watched);
        const auto *view = viewport ? qobject_cast<QAbstractItemView *>(viewport->parentWidget()) : nullptr;
        // Tabs and the rows of menus show a press in the same way.
        const bool row = (view && view->viewport() == viewport && isList(view)) || qobject_cast<QTabBar *>(watched) ||
                         qobject_cast<QMenu *>(watched);
        if (viewport && row &&
            !(event->type() == QEvent::MouseButtonPress &&
              QScroller::holdsBackPress(viewport, static_cast<QMouseEvent *>(event)->button()))) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton) {
                m_rippleWidget = viewport;
                m_rippleOrigin = mouse->position();
                m_rowPressed = true;
                m_animator->place(viewport, RippleRadiusChannel, 0);
                m_animator->place(viewport, RippleOpacityChannel, 1);
                viewport->update();
            } else if (m_rowPressed && viewport == m_rippleWidget.data()) {
                const bool moved = event->type() == QEvent::MouseMove &&
                                   (mouse->position() - m_rippleOrigin).manhattanLength() > listPressSlop;
                if (moved)
                    m_rippleWidget = nullptr;
                if (moved || event->type() == QEvent::MouseButtonRelease) {
                    m_rowPressed = false;
                    viewport->update();
                }
            }
        }
    }
    if (m_motion && event->type() == QEvent::MouseButtonPress) {
        // A button's ripple starts where it is pressed; that of a check box, a radio button or a switch spreads from
        // its control.
        auto *button = qobject_cast<QAbstractButton *>(watched);
        // A check box and a radio button take a press on their control and its label only, not on room past them.
        const bool choice = qobject_cast<QCheckBox *>(watched) || qobject_cast<QRadioButton *>(watched);
        if (button && button->isEnabled() && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton &&
            !(choice && static_cast<QMouseEvent *>(event)->position().x() >= button->sizeHint().width())) {
            m_rippleWidget = button;
            m_rippleOrigin = static_cast<QMouseEvent *>(event)->position();
            m_animator->place(button, RippleRadiusChannel, 0);
            m_animator->place(button, RippleOpacityChannel, 1);
        }
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
    if (isFab(widget))
        return ButtonKind::Fab;
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
    if (isToolBarItem(widget))
        return ButtonKind::ToolBarItem;
    // The actions of a Material dialog are text buttons.
    if (widget && qobject_cast<QDialogButtonBox *>(widget->parentWidget()))
        return ButtonKind::Text;
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
    case ButtonKind::Fab:
        return color(isOpenFab(widget) ? Role::OnPrimary : Role::OnPrimaryContainer);
    case ButtonKind::ToolBarItem:
        if (isVibrantToolBar(widget->parentWidget()))
            return color(checked ? Role::OnSurface : Role::OnPrimaryContainer);
        return color(checked ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
    case ButtonKind::Text:
        break;
    }
    return color(Role::Primary);
}

void QMaterialStyle::drawButtonContainer(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    QRectF rect(option->rect);
    const bool checked = option->state & State_On;
    // A button is a pill; pressed, its corners tighten, and a checked button is squarer. In a connected group the
    // ends of the row stay round, the corners between the buttons are small, and a checked button is a pill.
    const Segment segment = segmentOf(widget);
    const double pill = rect.height() / 2;
    double left = pill;
    double right = pill;
    const bool fab = isFab(widget);
    if (fab) {
        // A rounded square; open, the close button of its menu, which is a circle at the right.
        const bool open = isOpenFab(widget);
        left = right = open ? (rect.height() - fabShadow) / 2 : fabCorner;
    } else if (segment.grouped) {
        if (!checked || (option->state & State_Sunken)) {
            const double inner = (option->state & State_Sunken) ? groupPressed : groupCorner;
            if (!segment.first)
                left = inner;
            if (!segment.last)
                right = inner;
        }
    } else if (option->state & State_Sunken) {
        left = right = std::min<double>(pill, rect.height() >= 56 ? selectedCorner : pressedCorner);
    } else if (checked) {
        left = right = std::min<double>(pill, selectedCorner);
    }
    // The shape springs from one to the other, and may pass it a little.
    left = std::clamp(animated(widget, CornerChannel, left, QtPrivate::fastSpatialSpring, 0.05), 0.0, pill);
    right = std::clamp(animated(widget, RightCornerChannel, right, QtPrivate::fastSpatialSpring, 0.05), 0.0, pill);
    drawButtonShape(option, painter, widget, left, right);
}

void QMaterialStyle::drawButtonShape(const QStyleOption *option, QPainter *painter, const QWidget *widget, double left,
                                     double right) const {
    const ButtonKind kind = buttonKind(option, widget);
    QRectF rect(option->rect);
    if (isOpenFab(widget))
        rect.setLeft(rect.right() - (rect.height() - fabShadow));
    const bool checked = option->state & State_On;

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
    case ButtonKind::Fab:
        container = color(isOpenFab(widget) ? Role::Primary : Role::PrimaryContainer);
        break;
    case ButtonKind::ToolBarItem:
        if (checked)
            container =
                color(isVibrantToolBar(widget->parentWidget()) ? Role::SurfaceContainer : Role::SecondaryContainer);
        break;
    case ButtonKind::Text:
        break;
    }
    if (!enabled(option) && kind != ButtonKind::Text &&
        !((kind == ButtonKind::Outlined || kind == ButtonKind::ToolBarItem) && !checked))
        container = withOpacity(color(Role::OnSurface), disabledContainerOpacity);
    const double lift = kind == ButtonKind::Fab ? fabShadow : kind == ButtonKind::Elevated ? 1.5 : 0.0;
    const bool raised = lift > 0 && enabled(option) && (kind == ButtonKind::Fab || !(option->state & State_Sunken));
    if (raised)
        painter->fillPath(sidedPath(rect.adjusted(0.5, lift, -0.5, 0), left, right),
                          QBrush(withOpacity(QColor(Qt::black), 0.18)));
    // A button of a toolbar is drawn smaller than it is to a finger.
    const double inset = kind == ButtonKind::ToolBarItem ? toolBarInset : 0;
    const QRectF body = rect.adjusted(inset, inset, -inset, -inset - lift);
    const QPainterPath outline = sidedPath(body, left, right);
    // A line inside the shape.
    const auto stroke = [&](const QColor &ink, double width) {
        const double half = width / 2;
        painter->strokePath(sidedPath(body.adjusted(half, half, -half, -half), left - half, right - half),
                            QPen(QBrush(ink), width));
    };
    if (container.alpha() > 0)
        painter->fillPath(outline, QBrush(container));
    if (kind == ButtonKind::Outlined && !checked)
        stroke(color(Role::OutlineVariant), 1);
    // With motion, the ripple shows the press; the layer is for the pointer and the keyboard.
    QStyleOption resting = *option;
    // An open FAB is down only to say that its menu is open.
    if (isOpenFab(widget) || (m_motion && widget && widget == m_rippleWidget.data()))
        resting.state &= ~QStyle::State(State_Sunken);
    if (const double layer = stateLayer(&resting); layer > 0)
        painter->fillPath(outline, QBrush(withOpacity(buttonLabelColor(option, widget), layer)));
    // Only on the part that was pressed, of a button that has two.
    if (isOpenFab(widget) ||
        (m_rippleOrigin.x() >= option->rect.left() && m_rippleOrigin.x() < option->rect.left() + option->rect.width()))
        drawRipple(painter, widget, outline, body, m_rippleOrigin, (option->state & State_Sunken) && !isOpenFab(widget),
                   buttonLabelColor(option, widget));
    if (keyboardFocus(option))
        stroke(color(Role::Secondary), focusRingWidth);
}

QColor QMaterialStyle::drawSplitButton(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget,
                                       const QRect &trailing, bool open) const {
    const double pill = option->rect.height() / 2.0;
    const bool pressed = (option->state & State_Sunken) && !open;
    // While the menu is open, the part that opened it is a pill of its own.
    const double inner = std::clamp(
        animated(widget, RightCornerChannel, pressed ? splitPressed : splitCorner, QtPrivate::fastSpatialSpring, 0.05),
        0.0, pill);
    const double opening = std::clamp(
        animated(widget, CornerChannel, open ? pill : splitCorner, QtPrivate::fastSpatialSpring, 0.05), 0.0, pill);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QStyleOptionButton part = *option; // the whole of it: buttonKind() reads what a button's option has
    part.rect.setWidth(trailing.left() - groupSpace - option->rect.left());
    if (open)
        part.state &= ~QStyle::State(State_Sunken);
    drawButtonShape(&part, painter, widget, pill, inner);
    part.rect = trailing;
    part.state &= ~QStyle::State(State_Sunken);
    drawButtonShape(&part, painter, widget, opening, pill);
    // The chevron, which turns over as the menu opens.
    const QColor ink = buttonLabelColor(&part, widget);
    painter->translate(QRectF(trailing).center());
    painter->rotate(animated(widget, GlyphChannel, open ? 180 : 0, QtPrivate::fastSpatialSpring, 0.5));
    painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(QPointF(-5, -2.5), QPointF(0, 2.5));
    painter->drawLine(QPointF(0, 2.5), QPointF(5, -2.5));
    painter->restore();
    QStyleOptionButton label = *option;
    label.state &= ~QStyle::State(State_Sunken);
    return buttonLabelColor(&label, widget);
}

bool QMaterialStyle::popupFabMenu(QPushButton *button, QMenu *menu) const {
    const bool fab = isFab(button);
    if (fab != isFabMenu(menu)) {
        menu->setProperty(fabMenuProperty, fab ? QVariant(true) : QVariant());
        QEvent change(QEvent::StyleChange); // its items have other sizes
        QCoreApplication::sendEvent(menu, &change);
    }
    if (!fab)
        return false;
    // Above the button, with their right edges in line. The last item has its space below it.
    const QSize size = menu->sizeHint();
    menu->setProperty(fabMenuOpened, m_animator->seconds()); // its items set off from now
    menu->popup(
        button->mapToGlobal(QPoint(button->width() - size.width(), -size.height() - (fabMenuGap - fabMenuItemSpace))));
    return true;
}

bool QMaterialStyle::isFloatingToolBar(const QWidget *widget) {
    const QString kind = toolBarKind(widget);
    return kind == QLatin1String("floating") || kind == QLatin1String("floatingVibrant");
}

void QMaterialStyle::drawFloatingToolBar(QPainter *painter, const QToolBar *bar) const {
    const QRectF body = QRectF(bar->rect()).adjusted(0, 0, 0, -toolBarShadow);
    const double radius = std::min(body.width(), body.height()) / 2;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    fillRounded(painter, body.adjusted(0.5, toolBarShadow, -0.5, toolBarShadow), radius,
                withOpacity(QColor(Qt::black), 0.18));
    fillRounded(painter, body, radius, color(isVibrantToolBar(bar) ? Role::PrimaryContainer : Role::SurfaceContainer));
    painter->restore();
}

QColor QMaterialStyle::toolBarItemColor(const QStyleOption *option, const QWidget *widget) const {
    return isToolBarItem(widget) ? buttonLabelColor(option, widget) : QColor();
}

void QMaterialStyle::drawFabLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *button = static_cast<const QStyleOptionButton *>(option);
    const QColor ink = buttonLabelColor(option, widget);
    const bool open = isOpenFab(widget);
    QRect body = button->rect.adjusted(0, 0, 0, -fabShadow);
    if (open)
        body.setLeft(body.right() + 1 - body.height());
    const bool plain = button->icon.isNull() && button->text.isEmpty();
    const double turned = animated(widget, GlyphChannel, open ? 45 : 0, QtPrivate::fastSpatialSpring, 0.5);
    if (open || plain) {
        // A plus, which turns into the cross that closes the menu.
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(QRectF(body).center());
        painter->rotate(turned);
        painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawLine(QPointF(-7, 0), QPointF(7, 0));
        painter->drawLine(QPointF(0, -7), QPointF(0, 7));
        painter->restore();
        return;
    }
    painter->setPen(ink);
    if (button->icon.isNull()) {
        painter->drawText(body, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, button->text);
        return;
    }
    const int textWidth = button->text.isEmpty() ? 0 : painter->fontMetrics().horizontalAdvance(button->text);
    const int space = button->text.isEmpty() ? 0 : fabIconSpace;
    const int left = body.left() + (body.width() - fabIconSize - space - textWidth) / 2;
    const QRect iconRect(left, body.top() + (body.height() - fabIconSize) / 2, fabIconSize, fabIconSize);
    button->icon.paint(painter, iconRect, Qt::AlignCenter, enabled(option) ? QIcon::Normal : QIcon::Disabled);
    QRect textRect = body;
    textRect.setLeft(iconRect.right() + 1 + space);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, button->text);
}

void QMaterialStyle::drawFabMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *menu) const {
    const auto *item = static_cast<const QStyleOptionMenuItem *>(option);
    if (item->menuItemType == QStyleOptionMenuItem::Separator)
        return;
    const double full = std::min(fabMenuItemWidth(*item), item->rect.width());
    const double height = item->rect.height() - fabMenuItemSpace;
    // The items open one after another, the one by the button first: each grows from a circle at the right to its
    // width, along a spring that passes it a little, and fades in as it sets off.
    double width = full;
    double shown = 1;
    if (m_motion && menu) {
        const int fromButton = (menu->height() - item->rect.top() - item->rect.height()) / item->rect.height();
        const double since =
            m_animator->seconds() - menu->property(fabMenuOpened).toDouble() - std::max(0, fromButton) * fabMenuStagger;
        const QtPrivate::Spring spring(0, 0, 1, QtPrivate::fastSpatialSpring);
        const double grown = since > 0 ? spring.at(since).position : 0.0;
        width = std::clamp(height + (full - height) * grown, std::min(height, full),
                           static_cast<double>(item->rect.width()));
        shown = std::clamp(since / fabMenuFade, 0.0, 1.0);
        if (since < 0 || !spring.restsAt(since, 0.002))
            m_animator->keepPainting(menu);
    }
    const QRectF pill(item->rect.left() + item->rect.width() - width, item->rect.top(), width, height);
    const bool usable = item->state & State_Enabled;
    const QColor ink =
        usable ? color(Role::OnPrimaryContainer) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setOpacity(shown);
    fillRounded(painter, pill, pill.height() / 2, color(usable ? Role::PrimaryContainer : Role::SurfaceContainerHigh));
    if (usable && (item->state & State_Selected))
        fillRounded(painter, pill, pill.height() / 2, withOpacity(ink, pressedOpacity));
    if (usable && menu && menu == m_rippleWidget.data() && pill.contains(m_rippleOrigin)) {
        QPainterPath outline;
        outline.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
        drawRipple(painter, menu, outline, pill, m_rippleOrigin, m_rowPressed, ink);
    }
    // What is in the pill keeps its place from the right end, and shows as the pill reaches it.
    painter->setClipRect(pill, Qt::IntersectClip);
    int left = static_cast<int>(pill.right() - full) + fabMenuItemPadding;
    if (!item->icon.isNull()) {
        const QRect lead(left, item->rect.top() + (static_cast<int>(pill.height()) - fabIconSize) / 2, fabIconSize,
                         fabIconSize);
        item->icon.paint(painter, lead, Qt::AlignCenter, usable ? QIcon::Normal : QIcon::Disabled);
        left += fabIconSize + fabIconSpace;
    }
    painter->setFont(fabMenuFont(item->font));
    painter->setPen(ink);
    painter->drawText(
        QRect(left, item->rect.top(), static_cast<int>(pill.right()) - left, static_cast<int>(pill.height())),
        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, menuLabel(item->text));
    painter->restore();
}

void QMaterialStyle::drawButtonLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    if (isFab(widget)) {
        drawFabLabel(option, painter, widget);
        return;
    }
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

void QMaterialStyle::drawSwitch(const QStyleOption *option, const QRect &rect, QPainter *painter,
                                const QWidget *widget) const {
    const bool on = option->state & State_On;
    const bool pressed = option->state & State_Sunken;
    const QRectF track(rect.left(), rect.top() + (rect.height() - switchHeight) / 2.0, switchWidth, switchHeight);
    // The colors cross over as the handle does, along a spring that does not pass its mark.
    const double filled = std::clamp(
        animated(widget, SwitchColorChannel, on ? 1.0 : 0.0, QtPrivate::defaultEffectsSpring, 0.004), 0.0, 1.0);
    QColor trackColor = blend(color(Role::SurfaceContainerHighest), color(Role::Primary), filled);
    QColor border = blend(color(Role::Outline), color(Role::Primary), filled);
    QColor handle = blend(color(Role::Outline), color(Role::OnPrimary), filled);
    if (!enabled(option)) {
        const QColor ink = color(Role::OnSurface);
        trackColor = on ? withOpacity(ink, 0.12) : withOpacity(color(Role::SurfaceContainerHighest), 0.12);
        border = withOpacity(ink, 0.12);
        handle = on ? color(Role::Surface) : withOpacity(ink, disabledContentOpacity);
    }
    fillRounded(painter, track, switchHeight / 2.0, trackColor);
    if (filled < 0.999)
        strokeRounded(painter, track, switchHeight / 2.0, border, 2);
    // The handle is small when off, larger when on, and larger still under a finger.
    const double size = std::clamp(animated(widget, SwitchSizeChannel,
                                            pressed ? 28
                                            : on    ? 24
                                                    : 16,
                                            QtPrivate::fastSpatialSpring, 0.05),
                                   12.0, 30.0);
    // 0 is off and 1 on; the handle springs across, and a little past.
    const double position = std::clamp(
        animated(widget, SwitchPositionChannel, on ? 1.0 : 0.0, QtPrivate::fastSpatialSpring, 0.002), -0.08, 1.08);
    const double travel = switchWidth - switchHeight;
    const double centre = track.left() + switchHeight / 2.0 + position * travel;
    const QPointF middle(centre, track.center().y());
    drawChoicePress(option, painter, widget, middle, color(on ? Role::Primary : Role::OnSurface));
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
    const double width =
        std::max(1.0, animated(widget, HandleWidthChannel, pressed ? sliderPressedWidth : sliderHandleWidth,
                               QtPrivate::fastSpatialSpring, 0.05));
    const QRectF bar =
        horizontal ? QRectF(handle - width / 2, centre.y() - sliderHandleHeight / 2.0, width, sliderHandleHeight)
                   : QRectF(centre.x() - sliderHandleHeight / 2.0, handle - width / 2, sliderHandleHeight, width);
    fillRounded(painter, bar, width / 2, knob);
    if (keyboardFocus(option))
        strokeRounded(painter, bar.adjusted(-4, -2, 4, 2), 6, color(Role::Secondary), 2);
}

// A thin rounded line: what is done, a gap, what is left, and a dot at the far end.
void QMaterialStyle::drawLoadingIndicator(const QStyleOption *option, QPainter *painter, const QWidget *widget,
                                          bool contained) const {
    double time = 0;
    if (m_motion && widget && enabled(option)) {
        time = m_animator->seconds();
        m_animator->keepPainting(widget);
    }
    // From one shape to the next along a spring that passes its mark a little, turning all the while, and a quarter
    // turn further with each change.
    constexpr int count = static_cast<int>(std::size(loadingShapes));
    const double steps = time / loadingStep;
    const double whole = std::floor(steps);
    const double part =
        QtPrivate::Spring(0, 0, 1, QtPrivate::SpringSpec{0.6, 200}).at((steps - whole) * loadingStep).position;
    const int index = static_cast<int>(std::fmod(whole, count));
    const LoadingShape from = loadingShapes[index];
    const LoadingShape to = loadingShapes[(index + 1) % count];
    const double turn = time * 50 + std::fmod(whole + part, 4.0) * 90;
    const QPointF centre = QRectF(option->rect).center();
    constexpr double pi = 3.14159265358979323846;
    constexpr int corners = 120;
    QPainterPath shape;
    for (int i = 0; i < corners; ++i) {
        const double angle = 2 * pi * i / corners;
        const auto radiusOf = [angle](const LoadingShape &s) {
            return (1 + s.depth * std::cos(s.lobes * angle)) / (1 + s.depth);
        };
        const double radius = loadingRadius * (radiusOf(from) + (radiusOf(to) - radiusOf(from)) * part);
        const double turned = angle + turn * pi / 180;
        const QPointF point = centre + QPointF(radius * std::cos(turned), radius * std::sin(turned));
        if (i == 0)
            shape.moveTo(point);
        else
            shape.lineTo(point);
    }
    shape.closeSubpath();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (contained)
        fillRounded(painter,
                    QRectF(centre.x() - loadingSize / 2.0, centre.y() - loadingSize / 2.0, loadingSize, loadingSize),
                    loadingSize / 2.0, color(Role::PrimaryContainer));
    const QColor ink = !enabled(option) ? withOpacity(color(Role::OnSurface), disabledContentOpacity)
                                        : color(contained ? Role::OnPrimaryContainer : Role::Primary);
    painter->fillPath(shape, QBrush(ink));
    painter->restore();
}

void QMaterialStyle::drawProgressBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *bar = static_cast<const QStyleOptionProgressBar *>(option);
    const QRect r = option->rect;
    const bool busy = bar->minimum == 0 && bar->maximum == 0;
    if (const Loading loading = loadingOf(widget); busy && loading != Loading::No) {
        drawLoadingIndicator(option, painter, widget, loading == Loading::Contained);
        return;
    }
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
    // A wavy bar: the part that is done is a wave, which travels while motion is on. The track stays straight.
    const bool wavy = !vertical && widget && widget->property("materialProgress").toString() == QLatin1String("wavy");
    const double radius = progressThickness / 2.0;
    const auto drawActive = [&](double from, double to, double height, double crest, double phase) {
        if (!wavy || height < 0.05) {
            fillRounded(painter, part(from, to), radius, active);
            return;
        }
        // A line along the wave, whose round ends stay within `from` and `to`.
        constexpr double pi = 3.14159265358979323846;
        const double first = from + radius;
        const double last = std::max(first, to - radius);
        const auto point = [&](double along) {
            const double x = reversed ? line.right() - along : line.left() + along;
            return QPointF(x, line.center().y() + height * std::sin(2 * pi * (along - phase) / crest));
        };
        QPainterPath wave;
        wave.moveTo(point(first));
        const int steps = static_cast<int>(last - first);
        for (int step = 1; step <= steps; ++step)
            wave.lineTo(point(first + step));
        wave.lineTo(point(last));
        painter->strokePath(wave, QPen(QBrush(active), progressThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    };
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const auto restore = qScopeGuard([painter] { painter->restore(); });
    // How far the wave has travelled; asks to be painted again, as it goes on.
    const auto travelled = [&] {
        if (!m_motion || !widget || !enabled(option))
            return 0.0;
        m_animator->keepPainting(widget);
        return m_animator->seconds() * waveSpeed;
    };
    if (busy && enabled(option)) {
        // Two parts that cross the bar, half a period apart: the front of each leaves first, and its back follows.
        // Without motion, one stands in the middle.
        std::vector<std::pair<double, double>> parts{{0.3, 0.6}};
        if (m_motion && widget) {
            const double time = m_animator->seconds() / busyPeriod;
            m_animator->keepPainting(widget);
            const auto eased = [](double t) {
                const double u = std::clamp(t, 0.0, 1.0);
                return u * u * (3 - 2 * u);
            };
            parts.clear();
            for (const double offset : {0.0, 0.5}) {
                const double at = std::fmod(time + offset, 1.0);
                parts.emplace_back(eased((at - 0.3) / 0.7), eased(at / 0.7));
            }
            std::sort(parts.begin(), parts.end());
        }
        // The track fills what is between them, apart from them.
        const double phase = wavy ? travelled() : 0;
        double from = 0;
        for (const auto &[back, front] : parts) {
            const double start = back * length;
            const double end = front * length;
            if (end - start < 1)
                continue;
            if (start - progressGap - from >= 1)
                fillRounded(painter, part(from, start - progressGap), radius, track);
            drawActive(start, end, waveHeight, busyWaveLength, phase);
            from = std::max(from, end + progressGap);
        }
        if (length - from >= 1)
            fillRounded(painter, part(from, length), radius, track);
        return;
    }
    const qint64 span = static_cast<qint64>(bar->maximum) - bar->minimum;
    double done = 0;
    if (span > 0 && bar->progress > bar->minimum)
        done = length * static_cast<double>(std::min<qint64>(bar->progress, bar->maximum) - bar->minimum) /
               static_cast<double>(span);
    if (done > 0) {
        double height = 0;
        if (wavy && enabled(option)) {
            // Flat when there is little to show a wave in, and when nearly all is done; it rises and settles.
            const double share = done / length;
            height = share > 0.1 && share < 0.95 ? waveHeight : 0;
            if (m_motion)
                height =
                    std::clamp(m_animator->value(widget, WaveChannel, height, QtPrivate::defaultEffectsSpring, 0.02),
                               0.0, waveHeight);
        }
        const double phase = height >= 0.05 ? travelled() : 0;
        drawActive(0, std::max<double>(done, progressThickness), height, waveLength, phase);
    }
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
    // A horizontal QTabBar draws the line itself, once, so that it can slide (drawTabIndicator()).
    const auto *tabBar = qobject_cast<const QTabBar *>(widget);
    if (!selected || (tabBar && tabBar->shape() != QTabBar::RoundedWest && tabBar->shape() != QTabBar::RoundedEast &&
                      tabBar->shape() != QTabBar::TriangularWest && tabBar->shape() != QTabBar::TriangularEast))
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

// The outlined text field of Material, with a triangle that opens the list: its exposed dropdown menu.
void QMaterialStyle::drawComboBox(const QStyleOption *option, QPainter *painter) const {
    const auto *combo = static_cast<const QStyleOptionComboBox *>(option);
    const bool open = option->state & State_On;
    const bool focused = open || keyboardFocus(option);
    QColor line = color(focused ? Role::Primary : Role::Outline);
    if (!enabled(option))
        line = withOpacity(color(Role::OnSurface), 0.12);
    const QRectF rect(option->rect);
    fillRounded(painter, rect, textFieldCorner, option->palette.color(QPalette::Base));
    if (const double layer = stateLayer(option); layer > 0)
        fillRounded(painter, rect, textFieldCorner, withOpacity(color(Role::OnSurface), layer));
    strokeRounded(painter, rect, textFieldCorner, line, focused ? 3 : 1);

    const QColor ink =
        enabled(option) ? color(Role::OnSurface) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    QRect text = option->rect.adjusted(textFieldPadding, 0, -comboArrowRoom, 0);
    if (!combo->currentIcon.isNull()) {
        const QSize size = combo->iconSize.isValid() ? combo->iconSize : QSize(menuIconSize, menuIconSize);
        combo->currentIcon.paint(
            painter, QRect(text.left(), text.top() + (text.height() - size.height()) / 2, size.width(), size.height()),
            Qt::AlignCenter, enabled(option) ? QIcon::Normal : QIcon::Disabled);
        text.setLeft(text.left() + size.width() + menuIconSpace);
    }
    // An editable combo box shows its text in a line edit of its own.
    if (!combo->editable) {
        painter->setPen(ink);
        painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, combo->currentText);
    }
    const QPointF centre(rect.right() - comboArrowRoom / 2.0, rect.center().y());
    const double up = open ? -1 : 1;
    QPainterPath arrow;
    arrow.moveTo(centre.x() - 5, centre.y() - 2.5 * up);
    arrow.lineTo(centre.x() + 5, centre.y() - 2.5 * up);
    arrow.lineTo(centre.x(), centre.y() + 2.5 * up);
    arrow.closeSubpath();
    painter->fillPath(arrow, QBrush(enabled(option) ? color(Role::OnSurfaceVariant) : ink));
}

// A row of a menu: 48 high, with room for an icon or a check mark when the item has one.
void QMaterialStyle::drawMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *menu) const {
    const auto *item = static_cast<const QStyleOptionMenuItem *>(option);
    const QRect r = item->rect;
    if (item->menuItemType == QStyleOptionMenuItem::Separator) {
        painter->fillRect(QRect(r.left(), r.center().y(), r.width(), 1), color(Role::OutlineVariant));
        return;
    }
    const bool usable = item->state & State_Enabled;
    if (usable && (item->state & State_Selected))
        painter->fillRect(r, withOpacity(color(Role::OnSurface), hoverOpacity));
    if (usable && menu && menu == m_rippleWidget.data() && r.contains(m_rippleOrigin.toPoint())) {
        QPainterPath outline;
        outline.addRect(QRectF(r));
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        drawRipple(painter, menu, outline, QRectF(r), m_rippleOrigin, m_rowPressed, color(Role::OnSurface));
        painter->restore();
    }
    const QColor ink = usable ? color(Role::OnSurface) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const QColor quiet = usable ? color(Role::OnSurfaceVariant) : ink;
    int left = r.left() + menuPadding;
    const bool checkable = item->checkType != QStyleOptionMenuItem::NotCheckable;
    const QRect lead(left, r.top() + (r.height() - menuIconSize) / 2, menuIconSize, menuIconSize);
    if (checkable && item->checked) {
        painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawLine(QPointF(lead.left() + 5, lead.top() + 12.5), QPointF(lead.left() + 10, lead.top() + 17.5));
        painter->drawLine(QPointF(lead.left() + 10, lead.top() + 17.5), QPointF(lead.left() + 19, lead.top() + 7.5));
    } else if (!item->icon.isNull()) {
        item->icon.paint(painter, lead, Qt::AlignCenter, usable ? QIcon::Normal : QIcon::Disabled);
    }
    if (checkable || !item->icon.isNull())
        left += menuIconSize + menuIconSpace;
    const qsizetype tab = item->text.indexOf(QLatin1Char('\t'));
    const QString label = menuLabel(item->text);
    painter->setPen(ink);
    painter->drawText(QRect(left, r.top(), r.right() - menuPadding - left + 1, r.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, label);
    const bool submenu = item->menuItemType == QStyleOptionMenuItem::SubMenu;
    if (tab >= 0) {
        painter->setPen(quiet);
        painter->drawText(QRect(r.left(), r.top(), r.width() - menuPadding - (submenu ? menuPadding : 0), r.height()),
                          Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, item->text.mid(tab + 1));
    }
    if (submenu) {
        const QPointF centre(r.right() - menuPadding, r.center().y());
        QPainterPath arrow;
        arrow.moveTo(centre.x() - 2.5, centre.y() - 5);
        arrow.lineTo(centre.x() + 2.5, centre.y());
        arrow.lineTo(centre.x() - 2.5, centre.y() + 5);
        arrow.closeSubpath();
        painter->fillPath(arrow, QBrush(quiet));
    }
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
    case PE_PanelMenu:
    case PE_FrameMenu:
        if (isFabMenu(widget))
            break; // the items of a FAB menu float over what is below
        // Only the fill: a menu is a window, whose corners would show behind rounded ones.
        painter->fillRect(option->rect, color(Role::SurfaceContainer));
        if (element == PE_FrameMenu)
            break;
        painter->setPen(QPen(QBrush(color(Role::OutlineVariant)), 1));
        painter->setBrush(Qt::NoBrush);
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
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
        // 0 is empty and 1 filled: the fill fades in, and the mark is drawn as it does.
        const double amount = std::clamp(
            animated(widget, CheckChannel, filled ? 1.0 : 0.0, QtPrivate::defaultEffectsSpring, 0.004), 0.0, 1.0);
        QColor accent = color(Role::Primary);
        QColor outline = color(Role::OnSurfaceVariant);
        QColor mark = color(Role::OnPrimary);
        if (!enabled(option)) {
            accent = outline = withOpacity(color(Role::OnSurface), disabledContentOpacity);
            mark = color(Role::Surface);
        }
        drawChoicePress(option, painter, widget, box.center(), filled ? accent : outline);
        if (amount < 0.999)
            strokeRounded(painter, box, 2, outline, 2);
        if (amount < 0.001)
            break;
        QColor fill = accent;
        fill.setAlphaF(fill.alphaF() * amount);
        fillRounded(painter, box, 2, fill);
        painter->setPen(QPen(QBrush(mark), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const auto at = [&box](double x, double y) {
            return QPointF(box.left() + x * box.width(), box.top() + y * box.height());
        };
        const auto towards = [](const QPointF &from, const QPointF &to, double part) {
            return from + (to - from) * part;
        };
        if (option->state & State_NoChange) {
            painter->drawLine(at(0.25, 0.5), towards(at(0.25, 0.5), at(0.75, 0.5), amount));
            break;
        }
        // The short stroke first, then the long one.
        const QPointF start = at(0.22, 0.52), bend = at(0.42, 0.72), end = at(0.79, 0.30);
        painter->drawLine(start, towards(start, bend, std::min(1.0, amount / 0.35)));
        if (amount > 0.35)
            painter->drawLine(bend, towards(bend, end, (amount - 0.35) / 0.65));
        break;
    }
    case PE_IndicatorRadioButton: {
        const QRectF box(option->rect);
        const bool on = option->state & State_On;
        QColor accent = color(on ? Role::Primary : Role::OnSurfaceVariant);
        if (!enabled(option))
            accent = withOpacity(color(Role::OnSurface), disabledContentOpacity);
        drawChoicePress(option, painter, widget, box.center(), accent);
        strokeRounded(painter, box, box.width() / 2, accent, 2);
        // The dot grows from the middle, and shrinks away.
        const double grown = std::clamp(
            animated(widget, RadioDotChannel, on ? 1.0 : 0.0, QtPrivate::fastSpatialSpring, 0.004), 0.0, 1.2);
        if (grown > 0.02) {
            const double dot = (box.width() - 10) * grown;
            fillRounded(painter, QRectF(box.center().x() - dot / 2, box.center().y() - dot / 2, dot, dot), dot / 2,
                        accent);
        }
        break;
    }
    case PE_FrameFocusRect:
        // The current row of a view, while the keyboard moves it: after a tap the selection says enough.
        if (qobject_cast<const QAbstractItemView *>(widget)) {
            if (keyboardFocus(option))
                strokeRounded(painter, QRectF(option->rect).adjusted(0, 1, 0, -1), listSelectedRadius,
                              color(Role::Secondary), 2);
            break;
        }
        QCommonStyle::drawPrimitive(element, option, painter, widget);
        break;
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
    if ((element == CE_ComboBox && option->type == QStyleOption::SO_ComboBox) ||
        (element == CE_MenuItem && option->type == QStyleOption::SO_MenuItem)) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (element == CE_ComboBox) {
            drawComboBox(option, painter);
        } else {
            painter->setFont(static_cast<const QStyleOptionMenuItem *>(option)->font);
            if (isFabMenu(widget))
                drawFabMenuItem(option, painter, widget);
            else
                drawMenuItem(option, painter, widget);
        }
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
            drawProgressBar(option, painter, widget);
        else
            drawTab(option, painter, widget);
        painter->restore();
        return;
    }
    if (element == CE_CheckBox && isSwitch(widget)) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        drawSwitch(option, proxy()->subElementRect(SE_CheckBoxIndicator, option, widget), painter, widget);
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
        if (isFab(widget))
            return r; // the open one is a circle at the right of the whole button
        // A narrow button keeps what room it has for its label.
        return r.adjusted(std::min(buttonPadding, r.width() / 4), 0, -std::min(buttonPadding, r.width() / 4), 0);
    case SE_SliderGroove: {
        const auto *slider = static_cast<const QStyleOptionSlider *>(option);
        if (slider->orientation == Qt::Horizontal)
            return QRect(r.left(), r.top() + (r.height() - sliderTrack) / 2, r.width(), sliderTrack);
        return QRect(r.left() + (r.width() - sliderTrack) / 2, r.top(), sliderTrack, r.height());
    }
    case SE_CheckBoxIndicator:
    case SE_CheckBoxContents:
    case SE_RadioButtonIndicator:
    case SE_RadioButtonContents: {
        const bool radio = element == SE_RadioButtonIndicator || element == SE_RadioButtonContents;
        const bool toggle = !radio && isSwitch(widget);
        const QSize size(toggle  ? switchWidth
                         : radio ? radioSize
                                 : checkBoxSize,
                         toggle  ? switchHeight
                         : radio ? radioSize
                                 : checkBoxSize);
        const int left = choiceInset(size.width(), toggle);
        if (element == SE_CheckBoxIndicator || element == SE_RadioButtonIndicator)
            return QRect(QPoint(r.left() + left, r.top() + (r.height() - size.height()) / 2), size);
        return r.adjusted(left + size.width() + indicatorSpace, 0, 0, 0);
    }
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
    case PM_ComboBoxArrowWidth:
        return comboArrowRoom;
    case PM_LayoutHorizontalSpacing:
        // A layout asks with the widget that it is the layout of.
        if (isConnectedGroup(widget))
            return groupSpace;
        return QCommonStyle::pixelMetric(metric, option, widget);
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
        if (isFab(widget)) {
            const bool text = button && !button->text.isEmpty();
            if (button && !button->icon.isNull())
                width += fabIconSize + (text ? fabIconSpace : 0);
            return QSize(std::max(fabSize, text ? width + 2 * fabPadding : fabSize), fabSize + fabShadow);
        }
        if (button && !button->icon.isNull()) {
            const QSize icon = button->iconSize.isValid() ? button->iconSize : QSize(buttonIconSize, buttonIconSize);
            width += icon.width() + (button->text.isEmpty() ? 0 : buttonIconSpace);
            height = std::max(height, icon.height());
        }
        return QSize(std::max(touchTarget, width + 2 * buttonPadding), std::max(buttonHeight, height + 8));
    }
    case CT_ToolButton:
        if (isToolBarItem(widget))
            return QSize(std::max(toolBarTarget, contentsSize.width() + 6 * toolBarInset), toolBarTarget);
        if (isSplitButton(widget))
            return QSize(splitLeadingSpace + contentsSize.width() + splitTrailingSpace + groupSpace + splitTrailing,
                         std::max(buttonHeight, contentsSize.height() + 8));
        return QCommonStyle::sizeFromContents(type, option, contentsSize, widget);
    case CT_ComboBox:
        return QSize(contentsSize.width() + textFieldPadding + comboArrowRoom,
                     std::max(textFieldHeight, contentsSize.height()));
    case CT_MenuItem: {
        const auto *item = static_cast<const QStyleOptionMenuItem *>(option);
        if (isFabMenu(widget)) {
            if (item->menuItemType == QStyleOptionMenuItem::Separator)
                return QSize(fabSize, fabMenuItemSpace);
            return QSize(fabMenuItemWidth(*item), fabSize + fabMenuItemSpace);
        }
        if (item->menuItemType == QStyleOptionMenuItem::Separator)
            return QSize(menuMinimumWidth, menuSeparator);
        int width = contentsSize.width() + 2 * menuPadding;
        if (!item->icon.isNull() || item->checkType != QStyleOptionMenuItem::NotCheckable)
            width += menuIconSize + menuIconSpace;
        if (item->text.contains(QLatin1Char('\t')))
            width += item->reservedShortcutWidth + 2 * menuPadding;
        if (item->menuItemType == QStyleOptionMenuItem::SubMenu)
            width += menuPadding;
        return QSize(std::max(width, menuMinimumWidth), std::max(menuItemHeight, contentsSize.height()));
    }
    case CT_ProgressBar:
        if (loadingOf(widget) != Loading::No)
            return QSize(loadingSize, loadingSize);
        if (widget && widget->property("materialProgress").toString() == QLatin1String("wavy"))
            return QSize(contentsSize.width(), std::max(wavyProgressHeight, contentsSize.height()));
        return QSize(contentsSize.width(), std::max(progressHeight, contentsSize.height()));
    case CT_TabBarTab:
        return QSize(std::max(touchTarget, contentsSize.width() + 2 * tabPadding), tabHeight);
    case CT_CheckBox:
    case CT_RadioButton: {
        const int indicator = isSwitch(widget) ? switchWidth : type == CT_CheckBox ? checkBoxSize : radioSize;
        return QSize(choiceInset(indicator, isSwitch(widget)) + indicator +
                         (contentsSize.width() > 0 ? indicatorSpace + contentsSize.width() : 0),
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
