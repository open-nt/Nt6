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
#include <QtGui/qscreen.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcalendarwidget.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qscroller.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qstatusbar.h>
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
constexpr int calendarCell = 40;   // a day of a calendar, and the circle that shows the one that is chosen
constexpr int chipHeight = 32;
constexpr double chipCorner = 8;
constexpr int chipIcon = 18;
constexpr int chipPadding = 16;    // before and after a label with nothing on that side,
constexpr int chipIconPadding = 8; // and on each side of an icon
constexpr double cardCorner = 12;
constexpr int textFieldHeight = 56;
constexpr int textFieldCorner = 4;
constexpr int fieldLabelRoom = 8;    // above an outlined field whose label sits on its outline,
constexpr int fieldSupportRoom = 20; // and below one with a line of supporting text
constexpr int fieldSmallPixels = 12; // the label that has floated up, and the supporting text
constexpr int fieldInputPixels = 16;
constexpr int textFieldPadding = 16;
constexpr int searchIcon = 24;                                     // the magnifier of a search bar,
constexpr int searchTextLeft = textFieldPadding + searchIcon + 16; // and where the text begins after it
constexpr int secondaryIndicator = 2;
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
constexpr int snackbarOneLine = 48;
constexpr int snackbarTwoLines = 68;
constexpr int snackbarPadding = 16;
constexpr double snackbarCorner = 4;
constexpr int circularSize = 40;
constexpr double circularThickness = 4;
constexpr double circularGap = 4; // between the part of a circular progress indicator that is done and its track
constexpr int railWidth = 96;
constexpr int badgeDot = 6;
constexpr int badgeHeight = 16; // of one with a number or a word
constexpr int badgePixels = 11;
constexpr int appBarHeight = 64;
constexpr int appBarTitlePixels = 22;
constexpr int appBarTitleSpace = 16; // before the title of an app bar with nothing leading it
constexpr int navigationHeight = 64;
constexpr int navigationPillWidth = 56; // behind the icon of the destination that is shown
constexpr int navigationPillHeight = 32;
constexpr int navigationIcon = 24;
constexpr int navigationLabelPixels = 12;
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
constexpr int bottomSheetWidest = 640;
constexpr int bottomSheetTopRoom = 72; // that a bottom sheet leaves above it
constexpr int drawerWidth = 360;
constexpr int drawerRoom = 56; // that a drawer leaves beside it
constexpr int drawerCorner = 16;
constexpr int drawerRowInset = 12; // of the pill of a row from the sides of the drawer
constexpr double hoverOpacity = 0.08;
constexpr double focusOpacity = 0.10;
constexpr double pressedOpacity = 0.10;
constexpr double disabledContentOpacity = 0.38;
constexpr double disabledContainerOpacity = 0.10;
// What of a widget a spring moves.
// What a dialog asks to be with its property: "bottom", "drawer", or empty for a dialog like any other.
QString sheetKind(const QWidget *widget) {
    if (!qobject_cast<const QDialog *>(widget))
        return QString();
    const QString name = widget->property("materialSheet").toString();
    return name == QLatin1String("bottom") || name == QLatin1String("drawer") ? name : QString();
}

// Secondary tabs, for what is inside a page: quieter than the primary ones.
bool isSecondaryTabBar(const QWidget *widget) {
    return qobject_cast<const QTabBar *>(widget) &&
           widget->property("materialTabBar").toString() == QLatin1String("secondary");
}

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
    return kind == QLatin1String("floating") || kind == QLatin1String("floatingVibrant") ||
           kind == QLatin1String("appBar");
}

// True for a button of a top app bar that is before the title: one of the actions before the first separator.
bool leadsAppBar(const QWidget *widget) {
    auto *bar = qobject_cast<QToolBar *>(widget->parentWidget());
    if (!bar)
        return false;
    bool found = false;
    for (QAction *action : bar->actions()) {
        if (action->isSeparator())
            return found;
        found = found || bar->widgetForAction(action) == widget;
    }
    return false; // without a separator, every action is after the title
}

// How far in from the left a check box, a radio button or a switch is drawn, so that the circle of a press fits: its
// middle, or the middle of the handle of a switch that is off, is choiceCentre in.
int choiceInset(int width, bool toggle) { return choiceCentre - (toggle ? switchHeight : width) / 2; }

// The seed of a style that is given none.
QColor defaultSeedColor() {
    const QColor system = QMaterialStyle::systemSeedColor();
    return system.isValid() ? system : QColor(0x67, 0x50, 0xa4);
}

// A size of the common button, which a button asks for with the property materialButtonSize. One that asks for none is
// small, and keeps the room around its label and the size of its icon that buttons have had here.
struct ButtonSize {
    int height;
    int padding; // before and after the label
    int icon;
    int iconSpace;
    int labelPixels;
    QFont::Weight labelWeight;
    double pressedCorner; // the shape of one that is pressed,
    double checkedCorner; // of a round one that is checked,
    double squareCorner;  // and of a square one at rest
    int outline;          // of an outlined one
};

constexpr ButtonSize smallButton{40, 16, 20, 8, 14, QFont::Medium, 8, 12, 12, 1};

// The size that a push button or a tool button asks for; nullptr for one that asks for none.
const ButtonSize *sizeOfButton(const QWidget *widget) {
    static constexpr ButtonSize extraSmall{32, 12, 20, 4, 14, QFont::Medium, 8, 12, 12, 1};
    static constexpr ButtonSize medium{56, 24, 24, 8, 16, QFont::Medium, 12, 16, 16, 1};
    static constexpr ButtonSize large{96, 48, 32, 12, 24, QFont::Normal, 16, 28, 28, 2};
    static constexpr ButtonSize extraLarge{136, 64, 40, 16, 32, QFont::Normal, 16, 28, 28, 3};
    if (!qobject_cast<const QPushButton *>(widget) && !qobject_cast<const QToolButton *>(widget))
        return nullptr;
    const QString name = widget->property("materialButtonSize").toString();
    if (name == QLatin1String("extraSmall"))
        return &extraSmall;
    if (name == QLatin1String("small"))
        return &smallButton;
    if (name == QLatin1String("medium"))
        return &medium;
    if (name == QLatin1String("large"))
        return &large;
    if (name == QLatin1String("extraLarge"))
        return &extraLarge;
    return nullptr;
}

// The buttons of a calendar that show the month before and the month after.
bool isMonthArrow(const QWidget *widget) {
    const auto *button = qobject_cast<const QToolButton *>(widget);
    return button && button->arrowType() != Qt::NoArrow && button->parentWidget() &&
           qobject_cast<const QCalendarWidget *>(button->parentWidget()->parentWidget());
}

// The kind of icon button that a push button or a tool button asks to be with its property; empty for none. The arrows
// of a calendar are standard ones without asking.
QString iconButtonKind(const QWidget *widget) {
    if (isMonthArrow(widget))
        return QStringLiteral("standard");
    if (!qobject_cast<const QPushButton *>(widget) && !qobject_cast<const QToolButton *>(widget))
        return QString();
    const QString name = widget->property("materialIconButton").toString();
    const bool known = name == QLatin1String("standard") || name == QLatin1String("filled") ||
                       name == QLatin1String("tonal") || name == QLatin1String("outlined");
    return known ? name : QString();
}

bool isIconButton(const QWidget *widget) { return !iconButtonKind(widget).isEmpty(); }

// An icon button is as high and as wide as the button of its size, with an icon of its own size in the middle. What is
// smaller than a finger needs is 48 all the same, with the shape in the middle.
const ButtonSize &iconButtonSize(const QWidget *widget) {
    const ButtonSize *size = sizeOfButton(widget);
    return size ? *size : smallButton;
}

int iconButtonExtent(const QWidget *widget) { return std::max(touchTarget, iconButtonSize(widget).height); }

// One color is all that an icon has, if the pixels that show are all close to one of them.
bool hasOneColor(const QImage &image) {
    QColor first;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() < 64)
                continue;
            if (!first.isValid())
                first = pixel;
            else if (std::abs(pixel.red() - first.red()) > 32 || std::abs(pixel.green() - first.green()) > 32 ||
                     std::abs(pixel.blue() - first.blue()) > 32)
                return false;
        }
    }
    return first.isValid();
}

// The kind of chip that a push button asks to be with its property; empty for none.
QString chipKind(const QWidget *widget) {
    if (!qobject_cast<const QPushButton *>(widget))
        return QString();
    const QString name = widget->property("materialChip").toString();
    const bool known = name == QLatin1String("assist") || name == QLatin1String("filter") ||
                       name == QLatin1String("input") || name == QLatin1String("suggestion");
    return known ? name : QString();
}

bool isChip(const QWidget *widget) { return !chipKind(widget).isEmpty(); }

// What a chip has before its label and after it. A filter chip that is chosen has a check mark where its icon goes,
// and an input chip has a cross at the end.
struct ChipParts {
    bool lead = false;
    bool mark = false;  // the lead is the check mark
    bool cross = false; // there is a cross at the end

    int before() const { return lead ? chipIcon + 2 * chipIconPadding : chipPadding; }
    int after() const { return cross ? chipIcon + 2 * chipIconPadding : chipPadding; }
};

ChipParts chipParts(const QWidget *widget, bool hasIcon, bool checked) {
    const QString kind = chipKind(widget);
    ChipParts parts;
    parts.mark = checked && kind == QLatin1String("filter");
    parts.lead = parts.mark || hasIcon;
    parts.cross = kind == QLatin1String("input");
    return parts;
}

bool isSquareButton(const QWidget *widget) {
    return widget && widget->property("materialButtonShape").toString() == QLatin1String("square");
}

// What a framed QLineEdit asks of its text field with its properties.
struct TextField {
    bool filled = false;
    bool search = false; // a search bar: a pill with a magnifier, and nothing over or under it
    QString label;
    QString supporting; // the line under the field: the supporting text, or the error, which takes its place
    bool error = false;
    int above = 0; // the room over the container,
    int below = 0; // and under it

    QRect box(const QRect &widget) const { return widget.adjusted(0, above, 0, -below); }
};

TextField textFieldOf(const QWidget *widget) {
    TextField field;
    const auto *edit = qobject_cast<const QLineEdit *>(widget);
    if (!edit || !edit->hasFrame())
        return field;
    field.search = edit->property("materialTextField").toString() == QLatin1String("search");
    if (field.search)
        return field;
    field.filled = edit->property("materialTextField").toString() == QLatin1String("filled");
    field.label = edit->property("materialLabel").toString();
    const QVariant supporting = edit->property("materialSupportingText");
    const QVariant error = edit->property("materialError");
    field.error = !error.toString().isEmpty();
    field.supporting = field.error ? error.toString() : supporting.toString();
    field.above = !field.filled && !field.label.isEmpty() ? fieldLabelRoom : 0;
    // The room is kept while either is there, even empty, so that an error that comes does not move what is below.
    field.below = supporting.isValid() || error.isValid() ? fieldSupportRoom : 0;
    return field;
}

// The label rests where the text goes in an empty field that does not have the focus, and has floated up otherwise.
bool isLabelUp(const QLineEdit *field) { return field->hasFocus() || !field->text().isEmpty(); }

enum Channel {
    LabelChannel,  // how far the label of a text field has floated
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
    WaveChannel,
    NavigationChannel = 100 // plus the index of a destination: how far its pill has opened
};
const char *const polishedFont = "_nt_materialFont";
const char *const polishedPalette = "_nt_materialPalette";
const char *const polishedFrame = "_nt_materialFrame"; // the frame style that a widget had

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
        its buttons need, so place it with an alignment, or as a child that is moved over the content.
    \li \c materialButtonSize of a QPushButton or a QToolButton: \c "extraSmall", \c "small", \c "medium",
        \c "large" or \c "extraLarge", the five sizes of Material 3 Expressive, 32, 40, 56, 96 and 136 high, each
        with its own label, icon, room around them, corners and outline. Without it a button is small.
        \c materialButtonShape \c "square" gives a button square corners at rest, and a pill while it is checked,
        the other way round from a round one.
    \li A framed QLineEdit is an outlined text field; \c materialTextField \c "filled" fills it, with a line at the
        bottom. \c materialLabel is a label that rests where the text goes while the field is empty without the focus,
        and floats up otherwise; the placeholder text shows once the label is up. \c materialSupportingText is a line
        under the field, and a \c materialError that is not empty turns the field red and takes that line's place. The
        room for the line is kept while either property is there, so give a field an empty \c materialError from the
        start if an error should not move what is below it.
    \li \c materialIconButton of a QPushButton or a QToolButton: \c "standard", \c "filled", \c "tonal" or
        \c "outlined", a button that is its icon alone, in the sizes of \c materialButtonSize (small without it) with
        icons of 20 to 40, round or, with \c materialButtonShape, square. A checkable one shows that it is checked. One
        smaller than 48 is 48 to a finger.
    \li \c materialToolBar \c "appBar" makes a QToolBar a top app bar, 64 high, with its window title as the
        heading. The actions before the first separator are before the title, such as the one that goes back, and the
        others after it; without a separator, all are after it. Add it to a QMainWindow with addToolBar(), or to the
        top of a layout.
    \li \c materialTabBar \c "navigationBar" makes a horizontal QTabBar a bottom navigation bar, 64 high: its tabs
        share the width, each an icon over its label, with a pill behind the one that is current. With a QTabWidget
        whose tabs are at the south it is the navigation of an application and its pages:
        tabs->tabBar()->setProperty("materialTabBar", "navigationBar").
    \li \c materialTabBar \c "navigationRail" makes a vertical QTabBar a navigation rail, for a screen that is wide: 96
        across, its destinations one under another from the top. With a QTabWidget whose tabs are at the west it is the
        navigation of an application and its pages.
    \li \c materialBadge of an icon button or a button of a toolbar puts a badge on its icon: \c true is a dot, and a
        number above 0, or a text, is shown in it (over 999 as 999+); 0, \c false or an empty text take it away. The
        destinations of a navigation bar or rail have theirs in \c materialBadges of the QTabBar, a QVariantList with
        one such value for each, in order.
    \li \c materialProgress \c "circular" draws a QProgressBar as a ring of 40: how far it is, or while it is busy an
        arc that goes round.
    \li \c materialStatusBar \c "snackbar" makes the status bar of a QMainWindow a snackbar: it takes no room, floats
        over the content, above a navigation bar, beside a navigation rail and above a floating action button that is at
        the bottom, while it has a message, from showMessage() until clearMessage() or the message's timeout, and its
        permanent widgets, buttons, are its actions.
    \li \c materialChip of a QPushButton: \c "assist", \c "filter", \c "input" or \c "suggestion" makes it a chip,
        32 high (48 to a finger) with small corners and an outline. A filter chip is made checkable: checked, it is
        filled and a check mark takes the place of its icon, which makes it wider. An input chip stands for something
        that was entered and has a cross; a click anywhere on it is clicked(), on which the program takes it away. A
        chosen chip of any kind is filled.
    \li \c materialCard of a QFrame: \c "filled", \c "elevated" or \c "outlined" draws it as a card with corners of
        12, whatever its frame shape; its layout and its children are the contents. Under this style a QFrame whose
        shape is \c HLine or \c VLine is a divider, a quiet line of 1, whatever its shadow and line width.
    \li \c materialTextField \c "search" makes a QLineEdit a search bar: a pill, 56 high, with a magnifier before the
        text. The placeholder text says what is searched, and with \l{QLineEdit::setClearButtonEnabled()}{a clear
        button} there is a cross to empty it.
    \li \c materialTabBar \c "secondary" makes the tabs of a QTabBar secondary ones, for what is inside a page: the
        line under the current one is as wide as the tab and 2 high, and its label is not in the accent color.
    \li \c materialSheet of a QDialog: \c "bottom" makes it a modal bottom sheet, against the bottom of its parent's
        window, as high as what is in it and no wider than 640; \c "drawer" makes it a modal drawer, against the left
        edge, as high as the window and up to 360 wide. It has no title bar. On Android the window behind is dimmed, and
        a tap on it or Back dismisses the sheet.
    \li \c materialList \c "navigationDrawer" of a QListWidget, for the list of a drawer: the one that is current is
        marked with a pill, and the list has no frame. Open a destination from itemClicked() and accept() the drawer.
    \li \c materialSwitch of a QCheckBox: \c true draws it as a switch.
    \li \c materialButtonGroup of a widget: \c "connected" makes the buttons that are its children a connected
        button group, which Material 3 Expressive has in the place of segmented buttons. A horizontal layout of the
        widget puts them 2 apart unless it is told a spacing; the ends of the row are round, the corners between the
        buttons small, and the checked button is a pill. For a choice of one, make the buttons checkable and
        auto-exclusive.
    \endlist

    The buttons of a QDialogButtonBox, and so of a QMessageBox, are in the order of Android: what confirms is last.

    Tooltips, as QToolTip shows them, are Material's plain tooltips without anything being asked for. So is
    QCalendarWidget drawn as Material's date picker: days of 40, the chosen one in a filled circle and today in a ring,
    with icon buttons for the months. Material's has no week numbers, which
    \l{QCalendarWidget::setVerticalHeaderFormat()}{NoVerticalHeader} turns off and makes room for on a narrow screen.
    A paintCell() of your own that calls the calendar's keeps the look.

    An icon of one color is drawn in the color that the style gives what it is on: on buttons, floating action
    buttons, toolbars and menus. An icon with several colors, such as a logo, is drawn as it is. The icons are the
    application's own, from files or, with the optional package \l{Nt Material Symbols}, Google's.

    A widget that is shown takes a change of these properties at once.

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
    extra small size), progress bars (a thin line, with the text beside it), tab bars, combo boxes
    (the outlined exposed dropdown menu), menus, lists and dialogs are drawn in the Material way so far; the other
    widgets are drawn as the Fusion style draws them, in the Material colors. Material has no tree and no table:
    those show their selection in the Material colors, and the rows of a tree are 48 high. The rows of a QListView
    or QListWidget are 56 high; behind a QProxyStyle they are drawn as Fusion draws them. A finger that is dragged
    on a view selects, unless QScroller::grabGesture() makes it scroll. Menus
    have square corners. Dialogs get the Material container color and text buttons; their rounded corners and the
    dimming behind them are the android platform's. A QMessageBox has no headline type style.
    Split buttons and connected button groups are of the small size, and FABs of the standard one. The items of a FAB menu open one after another
    but close at once, have no check marks, and a row can be chosen anywhere across the menu, left of its pill too.
    A connected button group is of the small size and lies across; Material's standard button group, whose pressed
    button widens, is not drawn differently from buttons in a row. Text fields are those of QLineEdit, and
    have no icons, prefix, suffix or character counter (a search bar has its magnifier, and does not open a view of
    results of its own); their caret is the color of the text. A snackbar appears and goes without an
    animation, has no close icon, and is one only in a QMainWindow; showing a message does not queue it. There are no
    rich tooltips. Chips are push buttons only; there are no elevated chips, avatars in input chips or separate click
    on the cross, and a filter chip takes its new width at once. The date picker is the calendar alone: it has no headline with the chosen date, no choice of
    years, no range and no field to type a date in, it keeps the days of the other months, faint, and it has no time
    picker beside it. A card is not a button and shows no press. A card or
    a divider behind a QProxyStyle is drawn as a plain frame. Icon buttons have the default width, not
    the narrow or the wide one. An app bar is the small one: it has no subtitle, does not grow into a medium or large
    one, and keeps its color as the content scrolls. A navigation bar has no horizontal items for wide screens. A
    navigation rail is the collapsed one: it does not expand, has no menu button or floating action button of its own,
    and does not scroll if its destinations do not fit; the navigation drawer is the modal one only. A sheet is no taller than leaves 72 above it, unless what is in it cannot be made shorter: put that in
    a QScrollArea. Sheets do not slide in, are not dragged and have no drag handle; there are no standard (not modal)
    sheets. On a desktop a click beside a sheet does not dismiss it, only Escape or the program does, and a sheet stays
    where it was put if its parent's window moves or changes size. Badges are on icon buttons,
    toolbar buttons and the destinations of a navigation bar or rail only. A snackbar finds where a floating action
    button is when it is placed, and again when one is shown or hidden, not when one is moved. A slider shows no value bubble. A busy progress bar moves while motion is enabled, which the other styles of Nt
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
                       qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTabBar *>(widget)))
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
    // A little apart, so that two rows that are selected show as two. A row that is chosen or held is rounder. In a
    // navigation drawer it is a pill, in from the sides.
    const bool drawer = isDrawerList(view);
    const QRectF shape =
        drawer ? QRectF(row).adjusted(drawerRowInset, 0, -drawerRowInset, 0) : QRectF(row).adjusted(0, 1, 0, -1);
    const double radius = drawer ? shape.height() / 2 : selected || held ? listSelectedRadius : listRadius;
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
                   color(pressed == bar->currentIndex() && !isSecondaryTabBar(bar) ? Role::Primary : Role::OnSurface));
        painter->restore();
    }
    if (!bar || bar->currentIndex() < 0)
        return;
    // As wide as the label, or for secondary tabs as the tab; on its way from where it was.
    const bool secondary = isSecondaryTabBar(bar);
    const double label = bar->fontMetrics().horizontalAdvance(bar->tabText(bar->currentIndex()));
    const double width =
        secondary ? tab.width() : std::max(24.0, std::min<double>(label, tab.width() - 2 * tabPadding));
    const double centre =
        animated(bar, TabCentreChannel, QRectF(tab).center().x(), QtPrivate::defaultSpatialSpring, 0.1);
    const double shown = std::max(24.0, animated(bar, TabWidthChannel, width, QtPrivate::defaultSpatialSpring, 0.1));
    const double edge = below ? tab.top() : tab.bottom() + 1.0;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipRect(QRectF(0, tab.top(), bar->width(), tab.height()), Qt::IntersectClip);
    if (secondary) {
        painter->fillRect(
            QRectF(centre - shown / 2, below ? edge : edge - secondaryIndicator, shown, secondaryIndicator),
            color(Role::Primary));
    } else {
        // Twice as high as what shows, so that only the corners towards the labels are rounded.
        fillRounded(painter, QRectF(centre - shown / 2, edge - tabIndicator, shown, 2 * tabIndicator), tabIndicator,
                    color(Role::Primary));
    }
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
        palette.setColor(QPalette::Window,
                         color(sheetKind(widget).isEmpty() ? Role::SurfaceContainerHigh : Role::SurfaceContainerLow));
        widget->setPalette(palette);
        widget->setProperty(polishedPalette, true);
    }
    // The drawer is all the frame that its list has.
    if (isDrawerList(widget)) {
        auto *list = static_cast<QListWidget *>(widget);
        if (list->frameShape() != QFrame::NoFrame) {
            widget->setProperty(polishedFrame, list->frameStyle());
            list->setFrameShape(QFrame::NoFrame);
        }
    }
    // A calendar is Material's date picker, on its container.
    if (qobject_cast<QCalendarWidget *>(widget) && !widget->testAttribute(Qt::WA_SetPalette)) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Window, color(Role::SurfaceContainerHigh));
        palette.setColor(QPalette::Base, color(Role::SurfaceContainerHigh));
        palette.setColor(QPalette::Text, color(Role::OnSurface));
        palette.setColor(QPalette::Disabled, QPalette::Text, color(Role::OnSurfaceVariant)); // the week numbers
        widget->setPalette(palette);
        widget->setProperty(polishedPalette, true);
    }
    // Lists, trees and tables mark what is selected quietly, as menus do.
    if (qobject_cast<QAbstractItemView *>(widget) && !widget->testAttribute(Qt::WA_SetPalette)) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Highlight, color(Role::SecondaryContainer));
        palette.setColor(QPalette::HighlightedText, color(Role::OnSecondaryContainer));
        if (isDrawerList(widget)) {
            palette.setColor(QPalette::Base, color(Role::SurfaceContainerLow));
            palette.setColor(QPalette::Text, color(Role::OnSurfaceVariant));
        }
        widget->setPalette(palette);
        widget->setProperty(polishedPalette, true);
    }
    // The type scale: label large for buttons, body large for text and choices. A font that the program set stays.
    const bool choice = qobject_cast<QCheckBox *>(widget) || qobject_cast<QRadioButton *>(widget);
    const bool button = (!choice && qobject_cast<QAbstractButton *>(widget)) || qobject_cast<QTabBar *>(widget);
    if (isSnackbar(widget) && !widget->testAttribute(Qt::WA_SetFont)) {
        QFont font = widget->font();
        font.setPixelSize(14);
        widget->setFont(font);
        widget->setProperty(polishedFont, true);
    }
    const bool text = qobject_cast<QLineEdit *>(widget) || qobject_cast<QComboBox *>(widget) ||
                      qobject_cast<QMenu *>(widget) || qobject_cast<QCalendarWidget *>(widget) || isList(widget) ||
                      qobject_cast<QTreeView *>(widget);
    if ((choice || button || text) && !widget->testAttribute(Qt::WA_SetFont)) {
        QFont font = widget->font();
        font.setPixelSize(button ? 14 : 16);
        if (button)
            font.setWeight(QFont::Medium);
        if (isNavigationBar(widget) || isNavigationRail(widget))
            font.setPixelSize(navigationLabelPixels);
        if (isDrawerList(widget)) { // label large
            font.setPixelSize(14);
            font.setWeight(QFont::Medium);
        }
        if (const ButtonSize *size = sizeOfButton(widget)) {
            font.setPixelSize(size->labelPixels);
            font.setWeight(size->labelWeight);
        }
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
    if (auto *frame = qobject_cast<QFrame *>(widget); frame && widget->property(polishedFrame).isValid()) {
        frame->setFrameStyle(widget->property(polishedFrame).toInt());
        widget->setProperty(polishedFrame, QVariant());
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
            dialog->windowHandle()->setProperty(
                "_nt_cornerRadius", sheetKind(dialog) == QLatin1String("drawer") ? drawerCorner : dialogCorner);
    }
    // What is hidden is no longer pressed: a menu that closed with a choice must not show it when it opens again.
    if (event->type() == QEvent::Hide && m_rippleWidget && watched == m_rippleWidget.data()) {
        m_rippleWidget = nullptr;
        m_rowPressed = false;
    }
    // A floating action button that comes or goes, as with the page that it is on, moves a snackbar that is shown.
    if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
        auto *button = qobject_cast<QAbstractButton *>(watched);
        if (button && isFab(button)) {
            if (const auto *window = qobject_cast<const QMainWindow *>(button->window())) {
                auto *status = window->findChild<QStatusBar *>(QString(), Qt::FindDirectChildrenOnly);
                if (status && isSnackbar(status) && !status->currentMessage().isEmpty())
                    status->updateGeometry();
            }
        }
    }
    if (event->type() == QEvent::DynamicPropertyChange) {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->style() == this &&
            static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName().startsWith("material"))
            propertyChanged(widget);
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
    if (isChip(widget))
        return ButtonKind::Chip;
    if (isFab(widget))
        return ButtonKind::Fab;
    if (const QString icon = iconButtonKind(widget); !icon.isEmpty()) {
        if (icon == QLatin1String("filled"))
            return ButtonKind::IconFilled;
        if (icon == QLatin1String("tonal"))
            return ButtonKind::IconTonal;
        return icon == QLatin1String("outlined") ? ButtonKind::IconOutlined : ButtonKind::IconStandard;
    }
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
    if (widget && isSnackbar(widget->parentWidget()))
        return ButtonKind::Text; // an action of a snackbar
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
    case ButtonKind::Chip:
        if (checked)
            return color(Role::OnSecondaryContainer);
        return color(chipKind(widget) == QLatin1String("assist") ? Role::OnSurface : Role::OnSurfaceVariant);
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
    case ButtonKind::IconStandard:
        return color(checked ? Role::Primary : Role::OnSurfaceVariant);
    case ButtonKind::IconFilled: {
        // One that can be checked is quiet until it is.
        const auto *button = qobject_cast<const QAbstractButton *>(widget);
        return color(button && button->isCheckable() && !checked ? Role::OnSurfaceVariant : Role::OnPrimary);
    }
    case ButtonKind::IconTonal:
        return color(checked ? Role::OnSecondary : Role::OnSecondaryContainer);
    case ButtonKind::IconOutlined:
        return color(checked ? Role::InverseOnSurface : Role::OnSurfaceVariant);
    case ButtonKind::ToolBarItem:
        if (toolBarKind(widget->parentWidget()) == QLatin1String("appBar"))
            return color(checked               ? Role::OnSecondaryContainer
                         : leadsAppBar(widget) ? Role::OnSurface
                                               : Role::OnSurfaceVariant);
        if (isVibrantToolBar(widget->parentWidget()))
            return color(checked ? Role::OnSurface : Role::OnPrimaryContainer);
        return color(checked ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
    case ButtonKind::Text:
        if (widget && isSnackbar(widget->parentWidget()))
            return color(Role::InversePrimary);
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
    // Of the shape that is drawn, which for a small icon button is less than the widget.
    const double pill =
        isIconButton(widget) ? std::min<double>(rect.height(), iconButtonSize(widget).height) / 2 : rect.height() / 2;
    double left = pill;
    double right = pill;
    if (isChip(widget)) {
        drawButtonShape(option, painter, widget, chipCorner, chipCorner); // a chip keeps its shape
        return;
    }
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
    } else {
        // A round button is a pill, and a squarer one while it is checked; a square one the other way round.
        const ButtonSize *size = isIconButton(widget) ? &iconButtonSize(widget) : sizeOfButton(widget);
        const bool square = isSquareButton(widget);
        const double squared = size ? size->squareCorner : selectedCorner;
        if (option->state & State_Sunken)
            left = right = std::min<double>(pill, size                  ? size->pressedCorner
                                                  : rect.height() >= 56 ? selectedCorner
                                                                        : pressedCorner);
        else if (square && !checked)
            left = right = std::min(pill, squared);
        else if (!square && checked)
            left = right = std::min<double>(pill, size ? size->checkedCorner : selectedCorner);
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
    case ButtonKind::IconStandard:
        break;
    case ButtonKind::IconFilled: {
        const auto *button = qobject_cast<const QAbstractButton *>(widget);
        container = color(button && button->isCheckable() && !checked ? Role::SurfaceContainer : Role::Primary);
        break;
    }
    case ButtonKind::IconTonal:
        container = color(checked ? Role::Secondary : Role::SecondaryContainer);
        break;
    case ButtonKind::IconOutlined:
        if (checked)
            container = color(Role::InverseSurface);
        break;
    case ButtonKind::ToolBarItem:
        if (checked)
            container =
                color(isVibrantToolBar(widget->parentWidget()) ? Role::SurfaceContainer : Role::SecondaryContainer);
        break;
    case ButtonKind::Chip:
        if (checked)
            container = color(Role::SecondaryContainer);
        break;
    case ButtonKind::Text:
        break;
    }
    const bool bare = kind == ButtonKind::Outlined || kind == ButtonKind::ToolBarItem ||
                      kind == ButtonKind::IconOutlined || kind == ButtonKind::Chip;
    if (!enabled(option) && kind != ButtonKind::Text && kind != ButtonKind::IconStandard && !(bare && !checked))
        container = withOpacity(color(Role::OnSurface), disabledContainerOpacity);
    const double lift = kind == ButtonKind::Fab ? fabShadow : kind == ButtonKind::Elevated ? 1.5 : 0.0;
    const bool raised = lift > 0 && enabled(option) && (kind == ButtonKind::Fab || !(option->state & State_Sunken));
    if (raised)
        painter->fillPath(sidedPath(rect.adjusted(0.5, lift, -0.5, 0), left, right),
                          QBrush(withOpacity(QColor(Qt::black), 0.18)));
    // A button of a toolbar, and a small icon button, are drawn smaller than they are to a finger.
    double inset = kind == ButtonKind::ToolBarItem ? toolBarInset : 0;
    if (isIconButton(widget))
        inset = std::max(0, iconButtonExtent(widget) - iconButtonSize(widget).height) / 2.0;
    // A chip is as wide as it is to a finger, but not as high.
    const double above = kind == ButtonKind::Chip ? std::max(0.0, (rect.height() - chipHeight) / 2) : inset;
    const QRectF body = rect.adjusted(inset, above, -inset, -above - lift);
    const QPainterPath outline = sidedPath(body, left, right);
    // A line inside the shape.
    const auto stroke = [&](const QColor &ink, double width) {
        const double half = width / 2;
        painter->strokePath(sidedPath(body.adjusted(half, half, -half, -half), left - half, right - half),
                            QPen(QBrush(ink), width));
    };
    if (container.alpha() > 0)
        painter->fillPath(outline, QBrush(container));
    if (kind == ButtonKind::Chip && !checked)
        stroke(enabled(option) ? color(Role::OutlineVariant) : withOpacity(color(Role::OnSurface), 0.12), 1);
    if ((kind == ButtonKind::Outlined || kind == ButtonKind::IconOutlined) && !checked) {
        const ButtonSize *size = sizeOfButton(widget);
        stroke(color(Role::OutlineVariant), size ? size->outline : 1);
    }
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

void QMaterialStyle::paintIcon(QPainter *painter, const QRect &rect, const QIcon &icon, QIcon::Mode mode,
                               QIcon::State state, const QColor &ink) {
    const qreal ratio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const QPixmap plain = icon.pixmap(rect.size(), ratio, QIcon::Normal, state);
    QImage image = plain.toImage().convertToFormat(QImage::Format_ARGB32);
    if (!ink.isValid() || !hasOneColor(image)) {
        icon.paint(painter, rect, Qt::AlignCenter, mode, state);
        return;
    }
    // The shape is the icon's; the color, and how much of it shows, are the ink's. A disabled color is a faint one.
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(image.pixel(x, y));
            image.setPixel(x, y, qRgba(ink.red(), ink.green(), ink.blue(), (alpha * ink.alpha() + 127) / 255));
        }
    }
    const QSizeF shown = QSizeF(image.size()) / plain.devicePixelRatio();
    const QRectF target(rect.x() + (rect.width() - shown.width()) / 2, rect.y() + (rect.height() - shown.height()) / 2,
                        shown.width(), shown.height());
    painter->drawImage(target, image);
}

int QMaterialStyle::iconButtonIconSize(const QWidget *widget) {
    if (!isIconButton(widget))
        return 0;
    const int height = iconButtonSize(widget).height;
    return height <= 32 ? 20 : height <= 56 ? 24 : height <= 96 ? 32 : 40;
}

bool QMaterialStyle::isAppBar(const QWidget *widget) { return toolBarKind(widget) == QLatin1String("appBar"); }

void QMaterialStyle::drawAppBar(QPainter *painter, const QToolBar *bar) const {
    painter->fillRect(bar->rect(), color(Role::Surface));
    // The title is between what leads and what follows: the buttons before the first separator, and the others.
    int from = appBarTitleSpace;
    int to = bar->width() - appBarTitleSpace;
    bool titled = false;
    const QList<QAction *> actions = bar->actions();
    const bool separated = std::any_of(actions.begin(), actions.end(), [](QAction *a) { return a->isSeparator(); });
    titled = !separated;
    for (QAction *action : actions) {
        if (action->isSeparator()) {
            titled = true;
            continue;
        }
        const QWidget *widget = const_cast<QToolBar *>(bar)->widgetForAction(action); // NOLINT: it only reads
        if (!widget || widget->isHidden())
            continue;
        if (titled)
            to = std::min(to, widget->x() - 8);
        else
            from = std::max(from, widget->x() + widget->width() + 4);
    }
    if (to <= from)
        return;
    QFont font = bar->font();
    font.setPixelSize(appBarTitlePixels);
    font.setWeight(QFont::Normal);
    painter->save();
    painter->setFont(font);
    painter->setPen(color(Role::OnSurface));
    painter->drawText(QRect(from, 0, to - from, appBarHeight), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      QFontMetrics(font).elidedText(bar->windowTitle(), Qt::ElideRight, to - from));
    painter->restore();
}

bool QMaterialStyle::isNavigationBar(const QWidget *widget) {
    const auto *bar = qobject_cast<const QTabBar *>(widget);
    return bar && bar->property("materialTabBar").toString() == QLatin1String("navigationBar") &&
           (bar->shape() == QTabBar::RoundedNorth || bar->shape() == QTabBar::RoundedSouth ||
            bar->shape() == QTabBar::TriangularNorth || bar->shape() == QTabBar::TriangularSouth);
}

bool QMaterialStyle::isNavigationRail(const QWidget *widget) {
    const auto *bar = qobject_cast<const QTabBar *>(widget);
    return bar && bar->property("materialTabBar").toString() == QLatin1String("navigationRail") &&
           (bar->shape() == QTabBar::RoundedWest || bar->shape() == QTabBar::RoundedEast ||
            bar->shape() == QTabBar::TriangularWest || bar->shape() == QTabBar::TriangularEast);
}

void QMaterialStyle::drawBadge(QPainter *painter, const QVariant &badge, const QRect &icon) const {
    // true is a dot; a number above none, or some text, is a badge that says it.
    QString text;
    bool dot = false;
    if (badge.typeId() == QMetaType::Bool) {
        dot = badge.toBool();
    } else if (badge.typeId() == QMetaType::Int || badge.typeId() == QMetaType::LongLong ||
               badge.typeId() == QMetaType::UInt || badge.typeId() == QMetaType::ULongLong) {
        const qlonglong count = badge.toLongLong();
        if (count > 0)
            text = count > 999 ? QStringLiteral("999+") : QString::number(count);
    } else {
        text = badge.toString();
    }
    if (!dot && text.isEmpty())
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const int right = icon.x() + icon.width();
    if (dot) {
        fillRounded(painter, QRectF(right - badgeDot, icon.y(), badgeDot, badgeDot), badgeDot / 2.0,
                    color(Role::Error));
    } else {
        QFont font = painter->font();
        font.setPixelSize(badgePixels);
        font.setWeight(QFont::Medium);
        painter->setFont(font);
        // It starts over the icon and grows away from it.
        const int width = std::max(badgeHeight, QFontMetrics(font).horizontalAdvance(text) + 8);
        const QRect pill(right - icon.width() / 2, icon.y() + icon.height() / 2 - badgeHeight - 1, width, badgeHeight);
        fillRounded(painter, QRectF(pill), badgeHeight / 2.0, color(Role::Error));
        painter->setPen(color(Role::OnError));
        painter->drawText(pill, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, text);
    }
    painter->restore();
}

void QMaterialStyle::drawButtonBadge(QPainter *painter, const QAbstractButton *button) const {
    const QVariant badge = button->property("materialBadge");
    if (!badge.isValid())
        return;
    int extent = iconButtonIconSize(button);
    if (extent <= 0)
        extent = button->iconSize().width();
    const QRect icon((button->width() - extent) / 2, (button->height() - extent) / 2, extent, extent);
    drawBadge(painter, badge, icon);
}

// Each destination: an icon with a label under it, and a pill behind the icon of the one that is shown.
void QMaterialStyle::drawNavigationBar(QPainter *painter, const QTabBar *bar) const {
    painter->fillRect(bar->rect(), color(isNavigationRail(bar) ? Role::Surface : Role::SurfaceContainer));
    const QVariantList badges = bar->property("materialBadges").toList(); // one for each destination, in order
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(bar->font());
    for (int i = 0; i < bar->count(); ++i) {
        const QRect tab = bar->tabRect(i);
        if (tab.isEmpty())
            continue;
        const bool selected = i == bar->currentIndex();
        const bool usable = bar->isTabEnabled(i) && bar->isEnabled();
        const QIcon icon = bar->tabIcon(i);
        const QString label = bar->tabText(i);
        // The icon in the pill, 6 from the top, and the label 4 under it; one without the other is in the middle.
        const int pillTop =
            tab.top() + (icon.isNull() || label.isEmpty() ? (tab.height() - navigationPillHeight) / 2 : 6);
        const QRectF pill(tab.center().x() + 0.5 - navigationPillWidth / 2.0, pillTop, navigationPillWidth,
                          navigationPillHeight);
        // It opens from the middle as its destination is chosen.
        double open = selected ? 1.0 : 0.0;
        if (m_motion)
            open = std::clamp(m_animator->value(bar, NavigationChannel + i, open, QtPrivate::fastSpatialSpring, 0.004),
                              0.0, 1.1);
        if (open > 0.02) {
            const double width = navigationPillHeight + (navigationPillWidth - navigationPillHeight) * open;
            QColor fill = color(Role::SecondaryContainer);
            fill.setAlphaF(std::min(1.0, 2 * open));
            fillRounded(painter, QRectF(pill.center().x() - width / 2, pill.top(), width, pill.height()),
                        pill.height() / 2, fill);
        }
        if (usable && bar == m_rippleWidget.data() && tab.contains(m_rippleOrigin.toPoint())) {
            QPainterPath outline;
            outline.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
            drawRipple(painter, bar, outline, pill, pill.center(), m_rowPressed,
                       color(selected ? Role::OnSecondaryContainer : Role::OnSurface));
        }
        if (selected && bar->hasFocus() && m_keyboard)
            strokeRounded(painter, pill, pill.height() / 2, color(Role::Secondary), focusRingWidth);
        const QColor faint = withOpacity(color(Role::OnSurface), disabledContentOpacity);
        const QColor iconInk = !usable ? faint : color(selected ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
        const QColor labelInk = !usable ? faint : color(selected ? Role::Secondary : Role::OnSurfaceVariant);
        if (!icon.isNull()) {
            const QRect middle(static_cast<int>(pill.center().x()) - navigationIcon / 2,
                               pillTop + (navigationPillHeight - navigationIcon) / 2, navigationIcon, navigationIcon);
            paintIcon(painter, middle, icon, usable ? QIcon::Normal : QIcon::Disabled,
                      selected ? QIcon::On : QIcon::Off, iconInk);
            if (i < badges.size())
                drawBadge(painter, badges.at(i), middle);
        }
        if (!label.isEmpty()) {
            const QRect area =
                icon.isNull() ? tab : QRect(tab.left(), pillTop + navigationPillHeight + 4, tab.width(), 16);
            painter->setPen(labelInk);
            painter->drawText(area, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine,
                              painter->fontMetrics().elidedText(label, Qt::ElideRight, area.width() - 4));
        }
    }
    painter->restore();
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
    // And for any tool button that says what kind of button it is, or how large: its container is the style's.
    const bool styled = isToolBarItem(widget) || sizeOfButton(widget) || isIconButton(widget) ||
                        (qobject_cast<const QToolButton *>(widget) && widget->property("materialButton").isValid());
    return styled ? buttonLabelColor(option, widget) : QColor();
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
    paintIcon(painter, iconRect, button->icon, enabled(option) ? QIcon::Normal : QIcon::Disabled, QIcon::Off, ink);
    QRect textRect = body;
    textRect.setLeft(iconRect.right() + 1 + space);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, button->text);
}

void QMaterialStyle::drawClearButton(QPainter *painter, const QRect &area) const {
    const QPointF centre = QRectF(area).center();
    constexpr double reach = 5; // from the middle to the end of a stroke, across and down
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(QBrush(color(Role::OnSurfaceVariant)), 2, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(centre + QPointF(-reach, -reach), centre + QPointF(reach, reach));
    painter->drawLine(centre + QPointF(-reach, reach), centre + QPointF(reach, -reach));
    painter->restore();
}

bool QMaterialStyle::hidesPlaceholder(const QLineEdit *field) const {
    return !textFieldOf(field).label.isEmpty() && !isLabelUp(field);
}

// The label of a text field, on its way between where it rests and where it floats, and the line under the field.
void QMaterialStyle::drawTextFieldText(QPainter *painter, const QStyleOption *option, const QWidget *widget) const {
    const auto *edit = qobject_cast<const QLineEdit *>(widget);
    const TextField field = textFieldOf(widget);
    if (edit && field.search) {
        // A magnifier before the text.
        const double x = option->rect.left() + textFieldPadding;
        const double y = option->rect.top() + (option->rect.height() - searchIcon) / 2.0;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(QBrush(enabled(option) ? color(Role::OnSurface)
                                                    : withOpacity(color(Role::OnSurface), disabledContentOpacity)),
                             2, Qt::SolidLine, Qt::RoundCap));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QRectF(x + 4, y + 4, 11, 11));
        painter->drawLine(QPointF(x + 13.5, y + 13.5), QPointF(x + 20, y + 20));
        painter->restore();
        return;
    }
    if (!edit || (field.label.isEmpty() && field.supporting.isEmpty()))
        return;
    const QRectF box(field.box(option->rect));
    const QColor quiet = enabled(option) ? color(field.error ? Role::Error : Role::OnSurfaceVariant)
                                         : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    painter->save();
    if (!field.label.isEmpty()) {
        const double up = std::clamp(
            animated(edit, LabelChannel, isLabelUp(edit) ? 1.0 : 0.0, QtPrivate::defaultEffectsSpring, 0.004), 0.0,
            1.0);
        QFont font = edit->font();
        font.setPixelSize(static_cast<int>(std::lround(fieldInputPixels - (fieldInputPixels - fieldSmallPixels) * up)));
        // Floating, it is on the outline of an outlined field, and in the upper part of a filled one.
        const double rest = box.center().y();
        const double floated = field.filled ? box.top() + 16 : box.top();
        const double middle = rest + (floated - rest) * up;
        QColor ink = quiet;
        if (enabled(option) && !field.error && edit->hasFocus())
            ink = color(Role::Primary);
        painter->setFont(font);
        painter->setPen(ink);
        const int width = static_cast<int>(box.width()) - 2 * textFieldPadding;
        painter->drawText(
            QRect(static_cast<int>(box.left()) + textFieldPadding, static_cast<int>(middle) - 10, width, 20),
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
            QFontMetrics(font).elidedText(field.label, Qt::ElideRight, width));
    }
    if (!field.supporting.isEmpty()) {
        QFont font = edit->font();
        font.setPixelSize(fieldSmallPixels);
        painter->setFont(font);
        painter->setPen(quiet);
        const int width = static_cast<int>(box.width()) - 2 * textFieldPadding;
        painter->drawText(QRect(static_cast<int>(box.left()) + textFieldPadding, static_cast<int>(box.bottom()) + 4,
                                width, fieldSupportRoom - 4),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          QFontMetrics(font).elidedText(field.supporting, Qt::ElideRight, width));
    }
    painter->restore();
}

void QMaterialStyle::propertyChanged(QWidget *widget) {
    if (qobject_cast<QToolBar *>(widget)) {
        QEvent change(QEvent::StyleChange); // it lays itself out anew
        QCoreApplication::sendEvent(widget, &change);
    } else if (widget->property(polishedFont).toBool() || widget->property(polishedPalette).toBool()) {
        // The font or the colors may be others now: a size of button has its own, and so has a navigation drawer.
        unpolish(widget);
        polish(widget);
    }
    widget->updateGeometry();
    widget->update();
    // The buttons of a group take their shapes from the widget that holds them.
    for (QObject *child : widget->children()) {
        if (auto *button = qobject_cast<QAbstractButton *>(child))
            button->update();
    }
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
        paintIcon(painter, lead, item->icon, usable ? QIcon::Normal : QIcon::Disabled, QIcon::Off, ink);
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
    const QIcon::Mode iconMode = enabled(option) ? QIcon::Normal : QIcon::Disabled;
    const QIcon::State iconState = (option->state & State_On) ? QIcon::On : QIcon::Off;
    if (isChip(widget)) {
        drawChipLabel(option, painter, widget);
        return;
    }
    if (const int extent = iconButtonIconSize(widget); extent > 0) {
        // The icon alone; a button without one shows its text, a letter or a sign, in its place.
        if (button->icon.isNull()) {
            painter->drawText(button->rect, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, button->text);
            return;
        }
        const QRect middle(button->rect.left() + (button->rect.width() - extent) / 2,
                           button->rect.top() + (button->rect.height() - extent) / 2, extent, extent);
        paintIcon(painter, middle, button->icon, iconMode, iconState, ink);
        return;
    }
    if (button->icon.isNull()) {
        painter->drawText(button->rect, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, button->text);
        return;
    }
    const ButtonSize *size = sizeOfButton(widget);
    const QSize icon = size                         ? QSize(size->icon, size->icon)
                       : button->iconSize.isValid() ? button->iconSize
                                                    : QSize(buttonIconSize, buttonIconSize);
    const int textWidth = button->text.isEmpty() ? 0 : painter->fontMetrics().horizontalAdvance(button->text);
    const int space = button->text.isEmpty() ? 0 : size ? size->iconSpace : buttonIconSpace;
    const int left = button->rect.left() + (button->rect.width() - icon.width() - space - textWidth) / 2;
    const QRect iconRect(left, button->rect.top() + (button->rect.height() - icon.height()) / 2, icon.width(),
                         icon.height());
    paintIcon(painter, iconRect, button->icon, iconMode, iconState, ink);
    QRect textRect = button->rect;
    textRect.setLeft(iconRect.right() + 1 + space);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, button->text);
}

void QMaterialStyle::drawChipLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *button = static_cast<const QStyleOptionButton *>(option);
    const bool checked = option->state & State_On;
    const ChipParts parts = chipParts(widget, !button->icon.isNull(), checked);
    const QRect r = button->rect;
    const QColor ink = buttonLabelColor(option, widget);
    // The icon of a chip that helps, suggests or filters is in the primary color until the chip is chosen; that of an
    // input chip is the other way round.
    const bool input = chipKind(widget) == QLatin1String("input");
    const QColor iconInk = enabled(option) && input == checked ? color(Role::Primary) : ink;
    const int textWidth = painter->fontMetrics().horizontalAdvance(button->text);
    // A chip that is wider than it asked to be has what it shows in the middle.
    int left = r.left() + std::max(0, r.width() - parts.before() - textWidth - parts.after()) / 2;
    const int top = r.top() + (r.height() - chipIcon) / 2;
    painter->setRenderHint(QPainter::Antialiasing);
    if (parts.mark) {
        const QRectF box(left + chipIconPadding, top, chipIcon, chipIcon);
        const auto at = [&box](double x, double y) {
            return QPointF(box.left() + x * box.width(), box.top() + y * box.height());
        };
        painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawLine(at(0.20, 0.52), at(0.40, 0.72));
        painter->drawLine(at(0.40, 0.72), at(0.80, 0.30));
    } else if (parts.lead) {
        paintIcon(painter, QRect(left + chipIconPadding, top, chipIcon, chipIcon), button->icon,
                  enabled(option) ? QIcon::Normal : QIcon::Disabled, checked ? QIcon::On : QIcon::Off, iconInk);
    }
    left += parts.before();
    painter->setPen(ink);
    painter->drawText(QRect(left, r.top(), r.left() + r.width() - left, r.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, button->text);
    if (!parts.cross)
        return;
    const QRectF box(left + textWidth + chipIconPadding, top, chipIcon, chipIcon);
    const QRectF cross = box.adjusted(5, 5, -5, -5);
    painter->setPen(QPen(QBrush(ink), 1.75, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(cross.topLeft(), cross.bottomRight());
    painter->drawLine(cross.topRight(), cross.bottomLeft());
}

void QMaterialStyle::drawCalendarDay(QPainter *painter, const QRect &cell, const QCalendarWidget *calendar, int day,
                                     bool selected, bool today, bool usable, const QColor &ink) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const double extent = std::min(calendarCell, std::min(cell.width(), cell.height()));
    const QRectF circle(cell.left() + (cell.width() - extent) / 2, cell.top() + (cell.height() - extent) / 2, extent,
                        extent);
    QColor text = ink.isValid() ? ink : color(today ? Role::Primary : Role::OnSurface);
    if (!usable)
        text = withOpacity(color(Role::OnSurface), disabledContentOpacity);
    if (selected) {
        fillRounded(painter, circle, extent / 2,
                    usable ? color(Role::Primary) : withOpacity(color(Role::OnSurface), disabledContainerOpacity));
        if (usable)
            text = color(Role::OnPrimary);
        // The day that the keyboard moves.
        if (m_keyboard && calendar->hasFocus())
            strokeRounded(painter, circle, extent / 2, color(Role::Secondary), focusRingWidth);
    } else if (today) {
        strokeRounded(painter, circle, extent / 2, usable ? color(Role::Primary) : text, 1);
    }
    painter->setPen(text);
    painter->drawText(cell, Qt::AlignCenter, QString::number(day));
    painter->restore();
}

void QMaterialStyle::drawRowIcon(QPainter *painter, const QRect &area, const QIcon &icon, QIcon::Mode mode,
                                 bool selected, bool enabled) const {
    const QColor ink = !enabled ? withOpacity(color(Role::OnSurface), disabledContentOpacity)
                                : color(selected ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
    paintIcon(painter, area, icon, mode == QIcon::Selected ? QIcon::Normal : mode, QIcon::Off, ink);
}

bool QMaterialStyle::isDrawerList(const QWidget *widget) {
    return qobject_cast<const QListWidget *>(widget) &&
           widget->property("materialList").toString() == QLatin1String("navigationDrawer");
}

bool QMaterialStyle::placeSheet(QDialog *dialog) const {
    const QString kind = sheetKind(dialog);
    if (kind.isEmpty())
        return false;
    dialog->ensurePolished();
    dialog->setWindowFlag(Qt::FramelessWindowHint);
    QRect area;
    if (const QWidget *parent = dialog->parentWidget())
        area = parent->window()->geometry();
    else if (const QScreen *screen = QGuiApplication::primaryScreen())
        area = screen->availableGeometry();
    QRect place;
    if (kind == QLatin1String("drawer")) {
        // Against the left edge, as high as the window.
        const int width = std::max(1, std::min(drawerWidth, area.width() - drawerRoom));
        place = QRect(area.left(), area.top(), width, area.height());
    } else {
        // Against the bottom, as high as what is in it, in the middle of a window that is wider than a sheet gets.
        const int width = std::max(1, std::min(bottomSheetWidest, area.width()));
        const int wanted = dialog->heightForWidth(width);
        const int height = std::clamp(wanted > 0 ? wanted : dialog->sizeHint().height(), 1,
                                      std::max(1, area.height() - bottomSheetTopRoom));
        place = QRect(area.left() + (area.width() - width) / 2, area.top() + area.height() - height, width, height);
    }
    // The android platform places dialogs itself, and learns from this that the dialog is a sheet.
    dialog->winId();
    if (QWindow *window = dialog->windowHandle())
        window->setProperty("_nt_sheet", kind);
    dialog->setGeometry(place);
    return true;
}

bool QMaterialStyle::drawMaterialFrame(QPainter *painter, const QFrame *frame) const {
    const QString card = frame->property("materialCard").toString();
    const bool filled = card == QLatin1String("filled");
    const bool elevated = card == QLatin1String("elevated");
    const bool outlined = card == QLatin1String("outlined");
    const bool isCard = filled || elevated || outlined;
    if (!isCard && frame->frameShape() != QFrame::HLine && frame->frameShape() != QFrame::VLine)
        return false;
    painter->save();
    const QRect r = frame->frameRect();
    if (!isCard) {
        // A divider: one thin line, whatever shadow and width the frame has.
        const QRect band = frame->frameShape() == QFrame::HLine
                               ? QRect(r.left(), r.top() + (r.height() - 1) / 2, r.width(), 1)
                               : QRect(r.left() + (r.width() - 1) / 2, r.top(), 1, r.height());
        painter->fillRect(band, color(Role::OutlineVariant));
        painter->restore();
        return true;
    }
    painter->setRenderHint(QPainter::Antialiasing);
    QRectF body(r);
    if (elevated) {
        body.adjust(0.5, 0, -0.5, -fabShadow);
        fillRounded(painter, body.translated(0, 1.5), cardCorner, withOpacity(QColor(Qt::black), 0.18));
    }
    fillRounded(painter, body, cardCorner,
                color(filled     ? Role::SurfaceContainerHighest
                      : elevated ? Role::SurfaceContainerLow
                                 : Role::Surface));
    if (outlined)
        strokeRounded(painter, body, cardCorner, color(Role::OutlineVariant), 1);
    painter->restore();
    return true;
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

void QMaterialStyle::drawCircularProgress(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *bar = static_cast<const QStyleOptionProgressBar *>(option);
    const double extent = std::min<double>(circularSize, std::min(option->rect.width(), option->rect.height()));
    const QPointF centre = QRectF(option->rect).center();
    const double inset = (extent - circularThickness) / 2;
    const QRectF ring(centre.x() - inset, centre.y() - inset, 2 * inset, 2 * inset);
    const QColor active =
        enabled(option) ? color(Role::Primary) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const QColor track = enabled(option) ? color(Role::SecondaryContainer)
                                         : withOpacity(color(Role::OnSurface), disabledContainerOpacity);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    // Angles are in degrees clockwise from the top; QPainter's are in sixteenths, anticlockwise from the right.
    const auto arc = [&](double from, double sweep, const QColor &ink) {
        painter->setPen(QPen(QBrush(ink), circularThickness, Qt::SolidLine, Qt::RoundCap));
        painter->drawArc(ring, static_cast<int>(std::lround((90 - from) * 16)),
                         -static_cast<int>(std::lround(sweep * 16)));
    };
    constexpr double pi = 3.14159265358979323846;
    // The room that a gap and the two round ends take, as an angle.
    const double apart = (circularGap + circularThickness) / (pi * ring.width()) * 360;
    if (bar->minimum == 0 && bar->maximum == 0) {
        // Busy: an arc that grows and shrinks while it goes round. Without motion it stands as a quarter.
        double from = 0;
        double sweep = 90;
        if (m_motion && widget && enabled(option)) {
            const double time = m_animator->seconds();
            m_animator->keepPainting(widget);
            from = std::fmod(time * 300, 360.0);
            sweep = 40 + 220 * (0.5 - 0.5 * std::cos(time * 2 * pi / 1.4));
        }
        arc(from, sweep, active);
        painter->restore();
        return;
    }
    const qint64 span = static_cast<qint64>(bar->maximum) - bar->minimum;
    double done = 0;
    if (span > 0 && bar->progress > bar->minimum)
        done = 360.0 * static_cast<double>(std::min<qint64>(bar->progress, bar->maximum) - bar->minimum) /
               static_cast<double>(span);
    if (done >= 359.5) {
        arc(0, 360, active);
    } else if (done <= 0) {
        arc(0, 360, track);
    } else {
        if (360 - done - 2 * apart > 0)
            arc(done + apart, 360 - done - 2 * apart, track);
        arc(0, std::max(done, 0.5), active);
    }
    painter->restore();
}

bool QMaterialStyle::isFloatingActionButton(const QWidget *widget) { return isFab(widget); }

bool QMaterialStyle::isSnackbar(const QWidget *widget) {
    return qobject_cast<const QStatusBar *>(widget) &&
           widget->property("materialStatusBar").toString() == QLatin1String("snackbar");
}

int QMaterialStyle::snackbarHeight(const QStatusBar *bar, int width) {
    // Two lines if the message does not go on one beside the actions.
    int room = width - 2 * snackbarPadding;
    for (const QObject *child : bar->children()) {
        const auto *action = qobject_cast<const QAbstractButton *>(child);
        if (action && !action->isHidden())
            room -= action->sizeHint().width() + 4;
    }
    return bar->fontMetrics().horizontalAdvance(bar->currentMessage()) > room ? snackbarTwoLines : snackbarOneLine;
}

void QMaterialStyle::drawSnackbar(QPainter *painter, const QStatusBar *bar) const {
    if (bar->currentMessage().isEmpty() || bar->height() <= 0)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF body = QRectF(bar->rect()).adjusted(0, 0, 0, -2);
    fillRounded(painter, body.adjusted(0.5, 2, -0.5, 2), snackbarCorner, withOpacity(QColor(Qt::black), 0.18));
    fillRounded(painter, body, snackbarCorner, color(Role::InverseSurface));
    int right = bar->width() - snackbarPadding;
    for (const QObject *child : bar->children()) {
        const auto *action = qobject_cast<const QAbstractButton *>(child);
        if (action && !action->isHidden())
            right = std::min(right, action->x() - 4);
    }
    painter->setFont(bar->font());
    painter->setPen(color(Role::InverseOnSurface));
    painter->drawText(QRect(snackbarPadding, 0, std::max(0, right - snackbarPadding), static_cast<int>(body.height())),
                      static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter) | Qt::TextWordWrap, bar->currentMessage());
    painter->restore();
}

void QMaterialStyle::drawProgressBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *bar = static_cast<const QStyleOptionProgressBar *>(option);
    if (widget && widget->property("materialProgress").toString() == QLatin1String("circular")) {
        drawCircularProgress(option, painter, widget);
        return;
    }
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
        painter->fillRect(
            r, withOpacity(color(selected && !isSecondaryTabBar(widget) ? Role::Primary : Role::OnSurface), layer));
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
        paintIcon(painter, lead, item->icon, usable ? QIcon::Normal : QIcon::Disabled, QIcon::Off, quiet);
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
    case PE_PanelTipLabel:
        // A plain tooltip: dark on a light scheme and light on a dark one.
        painter->setRenderHint(QPainter::Antialiasing);
        fillRounded(painter, QRectF(option->rect), snackbarCorner, color(Role::InverseSurface));
        break;
    case PE_FrameTabWidget:
        break; // the pages of a tab widget have no frame: the line under the tabs parts them
    case PE_PanelLineEdit: {
        const auto *frame =
            option->type == QStyleOption::SO_Frame ? static_cast<const QStyleOptionFrame *>(option) : nullptr;
        const TextField field = textFieldOf(widget);
        const QRectF box(field.box(option->rect));
        if (field.search) {
            fillRounded(painter, box, box.height() / 2, color(Role::SurfaceContainerHigh));
        } else if (field.filled) {
            // Rounded at the top only: the line at the bottom is straight.
            QPainterPath shape;
            shape.addRoundedRect(box.adjusted(0, 0, 0, textFieldCorner), textFieldCorner, textFieldCorner);
            QPainterPath inside;
            inside.addRect(box);
            painter->fillPath(shape.intersected(inside),
                              QBrush(enabled(option) ? color(Role::SurfaceContainerHighest)
                                                     : withOpacity(color(Role::OnSurface), 0.04)));
        } else {
            fillRounded(painter, box, textFieldCorner, option->palette.color(QPalette::Base));
        }
        if (!frame || frame->lineWidth > 0)
            proxy()->drawPrimitive(PE_FrameLineEdit, option, painter, widget);
        drawTextFieldText(painter, option, widget);
        break;
    }
    case PE_FrameLineEdit: {
        const bool focused = option->state & State_HasFocus;
        const TextField field = textFieldOf(widget);
        const QRectF box(field.box(option->rect));
        if (field.search) {
            // No outline: the cursor shows where the text goes, and the ring what the keyboard moved to.
            if (focused && m_keyboard)
                strokeRounded(painter, box, box.height() / 2, color(Role::Secondary), focusRingWidth);
            break;
        }
        QColor line = color(field.error    ? Role::Error
                            : focused      ? Role::Primary
                            : field.filled ? Role::OnSurfaceVariant
                                           : Role::Outline);
        if (!enabled(option))
            line = withOpacity(color(Role::OnSurface), field.filled ? disabledContentOpacity : 0.12);
        if (field.filled) {
            const double height = focused ? 2 : 1;
            painter->fillRect(QRectF(box.left(), box.bottom() - height, box.width(), height), line);
            break;
        }
        const auto *edit = qobject_cast<const QLineEdit *>(widget);
        if (!edit || field.label.isEmpty() || !isLabelUp(edit)) {
            strokeRounded(painter, box, textFieldCorner, line, focused ? 3 : 1);
            break;
        }
        // The outline makes way for the label that sits on it.
        QFont small = edit->font();
        small.setPixelSize(fieldSmallPixels);
        const double from = box.left() + textFieldPadding - 4;
        const double to =
            std::min(box.right() - textFieldCorner, from + QFontMetrics(small).horizontalAdvance(field.label) + 8.0);
        const QRectF around = QRectF(option->rect);
        const QRectF parts[] = {QRectF(around.left(), around.top(), from - around.left(), around.height()),
                                QRectF(to, around.top(), around.right() - to, around.height()),
                                QRectF(from, box.top() + 6, to - from, around.bottom() - box.top() - 6)};
        for (const QRectF &part : parts) {
            painter->save();
            painter->setClipRect(part, Qt::IntersectClip);
            strokeRounded(painter, box, textFieldCorner, line, focused ? 3 : 1);
            painter->restore();
        }
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
    case PE_IndicatorArrowLeft:
    case PE_IndicatorArrowRight: {
        if (!isMonthArrow(widget)) {
            QCommonStyle::drawPrimitive(element, option, painter, widget);
            break;
        }
        // A chevron.
        const double x = option->rect.left() + option->rect.width() / 2.0;
        const double y = option->rect.top() + option->rect.height() / 2.0;
        const double out = element == PE_IndicatorArrowLeft ? -3 : 3;
        painter->setPen(QPen(QBrush(buttonLabelColor(option, widget)), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawLine(QPointF(x - out, y - 6), QPointF(x + out, y));
        painter->drawLine(QPointF(x + out, y), QPointF(x - out, y + 6));
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
        // The open FAB is a circle at the right of the whole button. A button with a size is made as wide as its label
        // needs with the room of that size, so its label has the whole of it.
        if (isFab(widget) || sizeOfButton(widget) || isIconButton(widget) || isChip(widget))
            return r;
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
        if (framed) {
            const TextField field = textFieldOf(widget);
            const QRect box = field.box(r);
            if (field.search)
                return box.adjusted(searchTextLeft, 3, -textFieldPadding, -3);
            // Under a label that floats inside a filled field, the text is in the lower part.
            if (field.filled && !field.label.isEmpty())
                return box.adjusted(textFieldPadding, 22, -textFieldPadding, -4);
            return box.adjusted(textFieldPadding, 3, -textFieldPadding, -3);
        }
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
    // What confirms comes last, as on Android.
    if (hint == SH_DialogButtonLayout)
        return QDialogButtonBox::AndroidLayout;
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
        if (isChip(widget)) {
            const auto *chip = static_cast<const QAbstractButton *>(widget);
            const ChipParts parts = chipParts(widget, button && !button->icon.isNull(), chip->isChecked());
            const int text = button && button->text.isEmpty() ? 0 : width;
            return QSize(parts.before() + text + parts.after(), touchTarget);
        }
        if (isIconButton(widget))
            return QSize(iconButtonExtent(widget), iconButtonExtent(widget));
        if (isFab(widget)) {
            const bool text = button && !button->text.isEmpty();
            if (button && !button->icon.isNull())
                width += fabIconSize + (text ? fabIconSpace : 0);
            return QSize(std::max(fabSize, text ? width + 2 * fabPadding : fabSize), fabSize + fabShadow);
        }
        if (const ButtonSize *size = sizeOfButton(widget)) {
            // A size says how high the button is; its width is the label's, with the room around it, and no less than
            // the height of a small button.
            if (button && !button->icon.isNull())
                width += size->icon + (button->text.isEmpty() ? 0 : size->iconSpace);
            return QSize(std::max(buttonHeight, width + 2 * size->padding), size->height);
        }
        if (button && !button->icon.isNull()) {
            const QSize icon = button->iconSize.isValid() ? button->iconSize : QSize(buttonIconSize, buttonIconSize);
            width += icon.width() + (button->text.isEmpty() ? 0 : buttonIconSpace);
            height = std::max(height, icon.height());
        }
        return QSize(std::max(touchTarget, width + 2 * buttonPadding), std::max(buttonHeight, height + 8));
    }
    case CT_ToolButton:
        if (isIconButton(widget))
            return QSize(iconButtonExtent(widget), iconButtonExtent(widget));
        if (isToolBarItem(widget))
            return QSize(std::max(toolBarTarget, contentsSize.width() + 6 * toolBarInset), toolBarTarget);
        if (isSplitButton(widget))
            return QSize(splitLeadingSpace + contentsSize.width() + splitTrailingSpace + groupSpace + splitTrailing,
                         std::max(buttonHeight, contentsSize.height() + 8));
        if (const ButtonSize *size = sizeOfButton(widget))
            return QSize(std::max(buttonHeight, contentsSize.width() + 2 * size->padding), size->height);
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
        if (widget && widget->property("materialProgress").toString() == QLatin1String("circular"))
            return QSize(circularSize, circularSize);
        if (widget && widget->property("materialProgress").toString() == QLatin1String("wavy"))
            return QSize(contentsSize.width(), std::max(wavyProgressHeight, contentsSize.height()));
        return QSize(contentsSize.width(), std::max(progressHeight, contentsSize.height()));
    case CT_TabBarTab:
        if (isNavigationRail(widget))
            return QSize(railWidth, navigationHeight);
        if (isNavigationBar(widget))
            return QSize(std::max(navigationPillWidth + 8, contentsSize.width() + 8), navigationHeight);
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
        if (framed) {
            const TextField field = textFieldOf(widget);
            return QSize(contentsSize.width() + (field.search ? searchTextLeft : textFieldPadding) + textFieldPadding,
                         std::max(textFieldHeight, contentsSize.height()) + field.above + field.below);
        }
        break;
    }
    default:
        break;
    }
    return QCommonStyle::sizeFromContents(type, option, contentsSize, widget);
}

#include "moc_qmaterialstyle.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
