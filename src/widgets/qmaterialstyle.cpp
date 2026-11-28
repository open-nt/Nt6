// MIT License - see LICENSE
#include <QtWidgets/qmaterialstyle.h>

#include "qmaterialcolor_p.h"
#include "qmaterialmotion_p.h"
#include "qmenu_p.h"
#include "gui/qpa/qplatformintegration_p.h"
#include "gui/qwindow_p.h"
#include "qwidget_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qhash.h>
#include <QtCore/qpointer.h>
#include <QtCore/qscopeguard.h>
#include <QtGui/qaction.h>
#include <QtGui/qevent.h>
#include <QtGui/qfont.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpalette.h>
#include <QtGui/qscreen.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qtransform.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qabstractspinbox.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcalendarwidget.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qcompleter.h>
#include <QtWidgets/qdatetimeedit.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qgraphicseffect.h>
#include <QtWidgets/qgroupbox.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qscroller.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtabbar.h>
#include <QtWidgets/qtabwidget.h>
#include <QtWidgets/qtextedit.h>
#include <QtWidgets/qtoolbar.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtreeview.h>

#include <algorithm>
#include <cmath>
#include <utility>
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
constexpr int groupTitle = 24;     // the room of a group box's title,
constexpr int checkableGroupTitle = 40; // with a check box in it,
constexpr int groupTitleGap = 4;        // what is between it and the card under it,
constexpr int groupPadding = 16;        // and what the card keeps around its contents
constexpr int editorFrame = 10;         // between the border of a text editor and its text, with the document's margin
constexpr int spinButton = 48;     // a button of a spin box that steps, as wide as a finger needs
constexpr int scrollBarWidth = 10; // the room of a scroll bar,
constexpr double scrollThumb = 4;  // how wide its handle is drawn,
constexpr double scrollThumbHeld = 8; // and while the pointer is on it or drags it
constexpr int scrollThumbLeast = 48;
constexpr int scrollRestDelay = 1500; // milliseconds that a scroll bar is left alone before its handle fades out
// The child of a scroll bar that wakes it and lets it rest, and the property of the bar in which it says which.
const char *const scrollRestWatcher = "_nt_materialScrollRest";
const char *const scrollAwake = "_nt_scrollAwake";
const char *const scrollHoverWatcher = "_nt_materialScrollHover";
constexpr int timeBoxWidth = 96;   // of the hour and of the minute in a time input
constexpr int timeBoxHeight = 72;
constexpr double timeBoxCorner = 8;
constexpr int timeSeparator = 24;  // between two boxes, where the colon is
constexpr int periodWidth = 52;    // of the choice of AM or PM,
constexpr int periodSpace = 12;    // and what is before it
constexpr int timeCaption = 20;    // under the boxes, for their names
constexpr int timeDigits = 45;     // display medium
constexpr int calendarCell = 40;   // a day of a calendar, and the circle that shows the one that is chosen
constexpr int datePickerHeader = 120;        // the headline of a modal date picker, above a divider of 1
constexpr int datePickerInset = 24;          // of its texts from the left, and 12 from the right
constexpr int datePickerHeadlinePixels = 32; // headline large
constexpr int datePickerModeButtonSize = 48; // the icon button at the end of the row of the date, an icon of 24
constexpr int datePickerFieldTop = 16;       // the text field of the input mode, under the headline
constexpr int datePickerFieldBottom = 8;     // and above the actions of the dialog, which have room of their own
constexpr int calendarYearWidth = 72;        // the pill of a year in the grid of years
constexpr int calendarYearHeight = 36;
constexpr int calendarMenuArrow = 18; // the arrow after the month and the year, which opens the years
constexpr int chipHeight = 32;
constexpr double chipCorner = 8;
constexpr int chipIcon = 18;
constexpr int chipPadding = 16;    // before and after a label with nothing on that side,
constexpr int chipIconPadding = 8; // and on each side of an icon
constexpr int chipAvatar = 24;     // the round picture at the start of an input chip,
constexpr int chipAvatarPadding = 4; // with this much room before it
constexpr double chipRaise = 1.5;    // how far the shadow of an elevated chip falls, and on hover
constexpr double chipRaiseHover = 3;
constexpr double cardRaise = 1; // that of a card that can be clicked, which falls fabShadow on hover
constexpr double cardCorner = 12;
constexpr int textFieldHeight = 56;
constexpr int textFieldCorner = 4;
constexpr int fieldLabelRoom = 8;    // above an outlined field whose label sits on its outline,
constexpr int fieldSupportRoom = 20; // and below one with a line of supporting text
constexpr int fieldSmallPixels = 12; // the label that has floated up, and the supporting text
constexpr int fieldInputPixels = 16;
constexpr int textFieldPadding = 16;
constexpr int editorDocumentMargin = 4;  // QPlainTextEdit's margin around its text
constexpr int editorFilledLabelTop = 26; // over the first line of a filled multi-line field with a label
constexpr int textCursorWidth = 2;
constexpr int fieldIcon = 24;                                      // an icon at an end of a text field,
constexpr int fieldIconMargin = 16;                                // with this much more room, 4 of it toward the edge
constexpr int searchIcon = 24;                                     // the magnifier of a search bar,
constexpr int searchIconTarget = 48;                               // the target of each icon at its end,
constexpr int searchTargetInset = 4;                               // which is this far from the edge,
constexpr int fieldAvatar = 30;                                    // and an avatar there
constexpr int fieldIconLayer = 40;                                 // the circle under an icon button of a text field
constexpr int searchTextLeft = textFieldPadding + searchIcon + 16; // and where the text begins after it
constexpr int secondaryIndicator = 2;
constexpr int checkBoxSize = 18;
constexpr int radioSize = 20;
constexpr int choiceCentre = 20; // where the middle of a check box, a radio button or a switch's handle is, and the
constexpr int choiceRadius = 20; // radius of the circle around it that shows a press
constexpr int indicatorSpace = 12;
constexpr int switchWidth = 52;
constexpr int switchHeight = 32;
constexpr int sliderHandleWidth = 4;
constexpr int sliderPressedWidth = 2;
constexpr int sliderGap = 6;     // between the handle and each part of the track
constexpr int valueWidth = 48;   // the value indicator of a slider: at least this wide,
constexpr int valueHeight = 44;  // this high,
constexpr int valuePadding = 12; // with this much beside its label,
constexpr int valueGap = 4;      // and this far from the handle
constexpr int sliderLength = 16; // the room of the handle along the track
constexpr int stopSize = 4;      // the dots at the ends of tracks, and of the steps of a slider
constexpr int sliderIconPadding = 8; // between an inset icon of a slider and the ends of its part of the track
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
constexpr double fabMenuCloseStagger = 0.02; // seconds between one item of a closing FAB menu going and the next
constexpr double fabMenuCloseFade = 0.08;    // seconds in which an item that goes fades out
const char *const fabMenuProperty = "_nt_fabMenu";
constexpr int loadingSize = 48;           // the container of the loading indicator
constexpr double loadingRadius = 19;      // of the shape that turns and changes in it
constexpr double loadingStep = 0.65;      // seconds from one shape to the next
constexpr double busyPeriod = 1.8;        // seconds for a part of a busy progress bar to cross it
constexpr int snackbarOneLine = 48;
constexpr int snackbarTwoLines = 68;
constexpr int snackbarPadding = 16;
constexpr double snackbarCorner = 4;
constexpr double richToolTipCorner = 12; // corner medium
constexpr int richToolTipShadow = 2;     // the room that QToolTip's label keeps below a rich tooltip for its shadow
const char *const richToolTipProperty = "_nt_materialRichToolTip"; // set by QToolTip's label on a rich tooltip
// A sheet comes in on the default spatial spring of Material, but damped so that it stops at its edge.
constexpr QtPrivate::SpringSpec sheetSpring{1.0, 380};
// It goes faster than it came, as what leaves does in Material.
constexpr QtPrivate::SpringSpec sheetExitSpring{1.0, 800};
// The scrim behind a modal dialog or sheet: black at 32 percent, which the android platform draws, and reads from this
// window property while it fades.
constexpr double scrimOpacity = 0.32;
const char *const scrimProperty = "_nt_scrimOpacity";
constexpr int circularSize = 40;
constexpr double circularThickness = 4;
constexpr double circularGap = 4; // between the part of a circular progress indicator that is done and its track
constexpr int circularWavySize = 48;       // the wavy circular indicator: its room,
constexpr double circularWaveHeight = 1.6; // how far its wave goes out and in from the circle,
constexpr double circularWaveLength = 15;  // from one crest to the next, about, as a whole number go round,
constexpr double circularWaveTurn = 60;    // and how many degrees a second the wave turns while it is shown
constexpr int railWidth = 96;
// The expanded rail: as wide as its longest destination needs, within these, each a row with a pill across it.
constexpr int railExpandedMinimum = 220;
constexpr int railExpandedMaximum = 360;
constexpr int railExpandedInset = 20;       // from the sides of the rail to its pills
constexpr int railExpandedPadding = 16;     // inside a pill, before the icon and after the label or count
constexpr int railExpandedIconSpace = 8;    // between the icon and the label
constexpr int railExpandedLabelPixels = 14; // label large
constexpr int railExpandedItem = 56;        // the height of a row
constexpr int badgeDot = 6;
constexpr int badgeHeight = 16; // of one with a number or a word
constexpr int badgePixels = 11;
constexpr int appBarHeight = 64;
constexpr int appBarTitlePixels = 22;
constexpr int appBarTitleSpace = 16; // before the title of an app bar with nothing leading it
// The medium and large flexible app bars when they are expanded: their title on a line of its own under the row of
// buttons, 16 from the left and 12 above the bottom, in headline medium (28 on 36) and display small (36 on 44).
constexpr int appBarMediumHeight = 112;
constexpr int appBarLargeHeight = 120;
constexpr int appBarMediumTitlePixels = 28;
constexpr int appBarMediumTitleLine = 36;
constexpr int appBarLargeTitlePixels = 36;
constexpr int appBarLargeTitleLine = 44;
constexpr int appBarExpandedTitleBottom = 12;
// A subtitle (AppBar*Tokens): label medium (12 on 16) under the title of the small bar, whose two lines are centred in
// its 64; label large (14 on 20) and title medium (16 on 24) under the title of the medium and large flexible bars,
// which are then 136 and 152 high. A title that does not fit one line of those goes on two, and the bar grows by a line.
constexpr int appBarTitleLine = 28;
constexpr int appBarSubtitlePixels = 12;
constexpr int appBarSubtitleLine = 16;
constexpr int appBarMediumSubtitledHeight = 136;
constexpr int appBarLargeSubtitledHeight = 152;
constexpr int appBarMediumSubtitlePixels = 14;
constexpr int appBarMediumSubtitleLine = 20;
constexpr int appBarLargeSubtitlePixels = 16;
constexpr int appBarLargeSubtitleLine = 24;
constexpr int navigationHeight = 64;
constexpr int navigationPillWidth = 56; // behind the icon of the destination that is shown
constexpr int navigationPillHeight = 32;
constexpr int navigationIcon = 24;
constexpr int navigationLabelPixels = 12;
// The horizontal items of a navigation bar (Expressive): the icon beside the label in a pill of 40, 16 inside its ends
// and 4 between the two, from a window of medium width on; and the room left between one item and the next.
constexpr int navigationRowHeight = 40;
constexpr int navigationRowPadding = 16;
constexpr int navigationRowIconSpace = 4;
constexpr int navigationRowSpace = 8;
constexpr int navigationMediumWidth = 600;
constexpr double modalRailCorner = 16;    // the trailing corners of the modal expanded rail (corner large)
constexpr int toolBarTarget = 48;         // a button of a floating toolbar, as a finger finds it,
constexpr int toolBarInset = 4;           // and what it leaves around the shape that is drawn
constexpr int toolBarShadow = 2;          // the room under a floating toolbar for its shadow
constexpr double listRadius = 4;          // the corners of a row of a list,
constexpr double listSelectedRadius = 16; // and of one that is chosen or held
constexpr double listPressSlop = 8;       // how far a press may stray and still be one
// The rows of a list with one, two and three lines, the line boxes of a headline and of supporting text, the room
// above the content of a three-line row (which is at the top, not in the middle) and between the text and a trailing
// supporting text.
constexpr int listOneLineHeight = 56, listTwoLineHeight = 72, listThreeLineHeight = 88;
constexpr int listHeadlineLine = 24, listSupportingLine = 20, listThreeLineTop = 12, listTrailingSpace = 12;
constexpr int listOverlineLine = 16; // the line box of an overline, label small
// The pictures at the start of a row: an avatar is a circle of 40, an image a square of 56 with corners of 8 (small),
// and either has 8 above and below it at least.
constexpr int listAvatarSize = 40, listImageSize = 56, listImageCorner = 8, listLeadingPadding = 8;
// The trailing check box: a box of 18 in a slot of 24 at the end of the row, 12 after the text, that takes a click
// in the circle of 40 around it, as the layer of a check box shows.
constexpr int listCheckSlot = 24, listCheckReach = 40;
// The rows of a segmented list: corners of 16 at its ends and of a row that is chosen or held, and 4 between rows.
constexpr double listSegmentOuter = 16, listSegmentInner = 4;
constexpr int menuItemHeight = 48;
constexpr double menuCorner = 16; // the container of the standard menu of Material 3 Expressive (corner large)
constexpr int menuVerticalPadding = 4; // above the first row and below the last, inside the container
constexpr int menuInset = 4;           // between the rows and the sides of the container
constexpr int menuPadding = 12;        // inside a row, so that its text is 16 in from the container
constexpr int menuIconSize = 24;
constexpr int menuIconSpace = 12;
constexpr int menuSeparator = 9;
constexpr int menuGroupGap = 2;           // between the groups of a grouped menu
constexpr double menuGroupCorner = 8;     // the corners of a group beside a gap (corner small)
constexpr double menuRowCorner = 4;       // the shape of a row under the pointer (corner extra small),
constexpr double menuSelectedCorner = 12; // of a checked row (corner medium), and of a row at an end of the menu
constexpr int menuMinimumWidth = 112;
constexpr int menuMaximumWidth = 280; // to which supporting text wraps
constexpr int menuGroupLabelHeight =
    32; // a section's row: its label (label medium, a line of 16) and 8 above and below
constexpr int menuSupportingPadding = 8; // above and below the text of a row that has supporting text
const char *const menuTrailingIconProperty = "materialTrailingIcon";
constexpr int menuSectionIconSize = 20; // the icon of a section's label
constexpr int menuSectionIconSpace = 8;
constexpr double searchViewCorner = 28; // the docked search view under a search bar (corner extra large)
constexpr int searchViewRowHeight = 56;
// The full-screen search view, which a search bar opens on a compact window: a header of 72 with an arrow back (24, in
// a target of 48 that is 4 from the edge), the field from 56 to 4 from the far edge, and a divider under it.
constexpr int searchHeaderHeight = 72;
constexpr int searchBackTarget = 48;
constexpr int searchBackInset = 4;
constexpr int searchHeaderFieldLeft = 56;
constexpr int searchHeaderFieldHeight = 56;
const char *const searchViewName = "_nt_searchView";   // the view, a child of the window, over everything in it
const char *const searchFieldName = "_nt_searchField"; // the line edit of its header
constexpr int comboArrowRoom = 48;
constexpr int focusRingWidth = 3;
constexpr int dialogCorner = 28; // extra large
constexpr int dialogPadding = 24;
constexpr int headlinePixels = 24;                       // headline small
constexpr int headlineSpace = 16;                        // between a dialog's headline and what is under it
constexpr int dialogIconSize = 24;                       // over the headline of the centered form
const char *const headlineRoom = "_nt_materialHeadline"; // the top margin that a dialog with a headline had
const char *const handleRoom = "_nt_materialHandle";     // the top margin that a sheet with a drag handle had
// The drag handle of a bottom sheet: a bar of 32 by 4 with corners 2, 22 from the top, in a touch target of 48 high.
constexpr int sheetHandleWidth = 32, sheetHandleHeight = 4, sheetHandleTop = 22, sheetHandleTarget = 48;
// A sheet let go after it was moved toward its edge this fast, in pixels a second, closes however far it went; the
// speed is measured over the last 100 ms of the drag, and not at all over less than 10 ms.
constexpr double sheetFlingSpeed = 1000;
constexpr qint64 sheetFlingWindow = 100, sheetFlingShortest = 10;
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

// The parts of the text of a row of a list: the headline is the first line without what follows a tab in it, which is
// the trailing supporting text, and the supporting text is the rest. A supporting text that has a line break of its
// own makes three lines. With `overline`, the first line of a row of three lines is an overline over the headline,
// which is the second, and the rest is one line of supporting text.
struct ListRowParts {
    QString overline, headline, trailing, supporting;
    int lines = 1;
    bool hasOverline = false;
};

ListRowParts listRowParts(const QString &text, bool overline = false) {
    ListRowParts parts;
    QString rest = text;
    const bool over = overline && text.count(QLatin1Char('\n')) >= 2;
    if (over) {
        parts.hasOverline = true;
        const qsizetype brk = text.indexOf(QLatin1Char('\n'));
        parts.overline = text.left(brk);
        rest = text.mid(brk + 1);
    }
    const qsizetype brk = rest.indexOf(QLatin1Char('\n'));
    parts.headline = brk < 0 ? rest : rest.left(brk);
    if (brk >= 0) {
        parts.supporting = rest.mid(brk + 1);
        parts.lines = over || parts.supporting.contains(QLatin1Char('\n')) ? 3 : 2;
        if (over)
            parts.supporting.replace(QLatin1Char('\n'), QLatin1Char(' '));
    }
    if (const qsizetype tab = parts.headline.indexOf(QLatin1Char('\t')); tab >= 0) {
        parts.trailing = parts.headline.mid(tab + 1);
        parts.headline = parts.headline.left(tab);
    }
    return parts;
}

// A list that draws the first line of its rows of three lines as an overline.
bool hasListOverline(const QWidget *view) { return view && view->property("materialListOverline").toBool(); }

// The supporting text in body medium and the trailing text in label small, as they are to body large: 14 and 11 to 16.
QFont listRowFont(const QFont &headline, int sixteenths, bool medium) {
    QFont font = headline;
    if (headline.pixelSize() > 0)
        font.setPixelSize(std::max(1, headline.pixelSize() * sixteenths / 16));
    else
        font.setPointSizeF(std::max(1.0, headline.pointSizeF() * sixteenths / 16));
    if (medium)
        font.setWeight(QFont::Medium);
    return font;
}

// `text` broken into lines no wider than `width`: at its line breaks, and between words.
QStringList wrapListText(const QString &text, const QFontMetrics &metrics, int width) {
    QStringList lines;
    for (const QString &paragraph : text.split(QLatin1Char('\n'))) {
        QString line;
        for (const QString &word : paragraph.split(QLatin1Char(' '))) {
            const QString longer = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
            if (!line.isEmpty() && metrics.horizontalAdvance(longer) > width) {
                lines.append(line);
                line = word;
            } else {
                line = longer;
            }
        }
        lines.append(line);
    }
    return lines;
}

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

// What the text of a time has between its separators: the hour, the minute and so on, as they stand while they are
// typed.
QStringList timeParts(const QString &text) {
    QStringList parts(1);
    for (const QChar c : text) {
        if (c.isLetterOrNumber())
            parts.last() += c;
        else
            parts.append(QString());
    }
    return parts;
}

// A date edit whose button opens a calendar has that one button, and no two that step.
bool opensCalendar(const QWidget *widget) {
    const auto *edit = qobject_cast<const QDateTimeEdit *>(widget);
    return edit && edit->calendarPopup();
}

QFont headlineFont(const QWidget *dialog) {
    QFont font = dialog->font();
    font.setPixelSize(headlinePixels);
    font.setWeight(QFont::Normal);
    return font;
}

// Gives a dialog back the room that was made above its contents for a headline or a drag handle, as `room` says.
void removeRoom(QWidget *dialog, const char *room) {
    const QVariant before = dialog->property(room);
    if (!before.isValid())
        return;
    QMargins margins = dialog->contentsMargins();
    margins.setTop(before.toInt());
    dialog->setContentsMargins(margins);
    dialog->setProperty(room, QVariant());
}

// Where the parts of a dialog's headline are when the dialog is `width` wide and its contents began at `top` before
// the headline: the icon of the centered form (empty without one) 24 from the top, and the text, 24 from the top or
// 16 under the icon, on as many lines as it takes.
struct HeadlineParts {
    QRect icon, text;
};
HeadlineParts headlineParts(const QWidget *dialog, bool icon, int width, int top) {
    HeadlineParts parts;
    int y = top + dialogPadding;
    if (icon) {
        parts.icon = QRect((width - dialogIconSize) / 2, y, dialogIconSize, dialogIconSize);
        y += dialogIconSize + headlineSpace;
    }
    const int textWidth = std::max(1, width - 2 * dialogPadding);
    const QFontMetrics metrics(headlineFont(dialog));
    const QRect lines =
        metrics.boundingRect(QRect(0, 0, textWidth, QWIDGETSIZE_MAX), Qt::TextWordWrap, dialog->windowTitle());
    parts.text = QRect(dialogPadding, y, textWidth, std::max(metrics.height(), lines.height()));
    return parts;
}

// A message box's icon as one color, drawn as Material's symbols are: a round sign with "i", "?" or "!", or a
// triangle with "!". What is not the sign is transparent, so that QMaterialStyle::paintIcon() colors it.
QPixmap messageSign(QMessageBox::Icon icon, int pixels) {
    QImage image(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(Qt::black));
    const double unit = pixels / 24.0; // the symbols are drawn on a grid of 24
    const double stem = 2 * unit;
    if (icon == QMessageBox::Warning) {
        QPainterPath triangle;
        triangle.moveTo(12 * unit, 2 * unit);
        triangle.lineTo(23 * unit, 21 * unit);
        triangle.lineTo(1 * unit, 21 * unit);
        triangle.closeSubpath();
        painter.drawPath(triangle);
    } else {
        painter.drawEllipse(QRectF(2 * unit, 2 * unit, 20 * unit, 20 * unit));
    }
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    const double middle = 12 * unit - stem / 2;
    switch (icon) {
    case QMessageBox::Information: // a dot over a stem
        painter.drawRect(QRectF(middle, 11 * unit, stem, 6 * unit));
        painter.drawRect(QRectF(middle, 7 * unit, stem, stem));
        break;
    case QMessageBox::Warning: // a stem over a dot, lower in the triangle
        painter.drawRect(QRectF(middle, 9 * unit, stem, 6 * unit));
        painter.drawRect(QRectF(middle, 17 * unit, stem, stem));
        break;
    case QMessageBox::Critical:
        painter.drawRect(QRectF(middle, 7 * unit, stem, 6 * unit));
        painter.drawRect(QRectF(middle, 15 * unit, stem, stem));
        break;
    case QMessageBox::Question: { // a hook over a dot
        QPainterPath hook;
        hook.addEllipse(QRectF(9 * unit, 6 * unit, 6 * unit, 6 * unit));
        QPainterPath hole;
        hole.addEllipse(QRectF(9 * unit + stem, 6 * unit + stem, 6 * unit - 2 * stem, 6 * unit - 2 * stem));
        hole.addRect(QRectF(6 * unit, 9 * unit, 6 * unit, 5 * unit)); // the left of the ring, under its middle
        painter.drawPath(hook.subtracted(hole));
        painter.drawRect(QRectF(middle, 11 * unit, stem, 3 * unit));
        painter.drawRect(QRectF(middle, 15 * unit, stem, stem));
        break;
    }
    case QMessageBox::NoIcon:
        break;
    }
    painter.end();
    return QPixmap::fromImage(image);
}

// A bottom sheet has a drag handle, unless its materialSheetHandle is false.
bool hasSheetHandle(const QWidget *dialog) {
    const QVariant shown = dialog->property("materialSheetHandle");
    return sheetKind(dialog) == QLatin1String("bottom") && (!shown.isValid() || shown.toBool());
}

bool isTextEditor(const QWidget *widget) {
    return qobject_cast<const QTextEdit *>(widget) || qobject_cast<const QPlainTextEdit *>(widget);
}

QFont groupTitleFont(QFont font) {
    font.setPixelSize(14); // label large
    font.setWeight(QFont::Medium);
    return font;
}

int groupTitleHeight(bool checkable) { return checkable ? checkableGroupTitle : groupTitle; }

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

// The parts of the text of a menu row: its label, the supporting text after a line break, and the shortcut after a tab,
// which runs to the end of its line (QMenu puts the shortcut of the action at the end of the text).
struct MenuRowParts {
    QString label, supporting, shortcut;
};

MenuRowParts menuRowParts(const QString &text) {
    MenuRowParts parts;
    QString rest = text;
    if (const qsizetype tab = rest.indexOf(QLatin1Char('\t')); tab >= 0) {
        qsizetype end = rest.indexOf(QLatin1Char('\n'), tab);
        if (end < 0)
            end = rest.size();
        parts.shortcut = rest.mid(tab + 1, end - tab - 1);
        rest.remove(tab, end - tab);
    }
    const qsizetype brk = rest.indexOf(QLatin1Char('\n'));
    parts.label = menuLabel(brk < 0 ? rest : rest.left(brk));
    if (brk >= 0)
        parts.supporting = rest.mid(brk + 1);
    return parts;
}

// The type of a section's label, label medium, as it is to the menu's body large: 12 to 16. QMenu makes it bold.
QFont menuGroupLabelFont(QFont font) {
    font.setBold(false);
    return listRowFont(font, 12, true);
}

// True if `item` is a section of `widget`, a separator with a text, which QMenu draws as an item that cannot be
// chosen. A row that is drawn is found at its rectangle; while the menu measures its rows they have none, and a
// section is known by its text.
bool isMenuSection(const QWidget *widget, const QStyleOptionMenuItem *item) {
    const auto *menu = qobject_cast<const QMenu *>(widget);
    if (!menu || item->menuItemType != QStyleOptionMenuItem::Normal || item->text.isEmpty())
        return false;
    if (item->rect.isValid()) {
        const QAction *action = menu->actionAt(item->rect.center());
        return action && action->isSeparator();
    }
    const QList<QAction *> actions = QtPrivate::MenuAccess::shownActions(menu);
    return std::any_of(actions.begin(), actions.end(),
                       [item](const QAction *action) { return action->isSeparator() && action->text() == item->text; });
}

// The icon that the action of a row asks for at the trailing end of its row.
QIcon menuTrailingIcon(const QWidget *widget, const QRect &row) {
    const auto *menu = qobject_cast<const QMenu *>(widget);
    const QAction *action = menu ? menu->actionAt(row.center()) : nullptr;
    return action ? action->property(menuTrailingIconProperty).value<QIcon>() : QIcon();
}

// True if a row of the menu has a trailing icon: the rows share one width, so every row makes room for it.
bool hasMenuTrailingIcons(const QWidget *widget) {
    const auto *menu = qobject_cast<const QMenu *>(widget);
    if (!menu)
        return false;
    const QList<QAction *> actions = QtPrivate::MenuAccess::shownActions(menu);
    return std::any_of(actions.begin(), actions.end(), [](const QAction *action) {
        return action->isVisible() && !action->property(menuTrailingIconProperty).value<QIcon>().isNull();
    });
}

// True if `row` of the menu `widget` is the row that goes back from a submenu shown in the menu's place.
bool isMenuBackRow(const QWidget *widget, const QRect &row) {
    const auto *menu = qobject_cast<const QMenu *>(widget);
    return menu && row.isValid() && QtPrivate::MenuAccess::isBackAction(menu, menu->actionAt(row.center()));
}

// The width that the supporting text of a row as wide as `rowWidth` wraps to: what the row leaves beside its icon,
// the arrow of a submenu and a trailing icon, in a row no wider than that of the widest menu.
int menuSupportingWidth(int rowWidth, const QStyleOptionMenuItem *item, const QWidget *widget) {
    int width = std::min(rowWidth, menuMaximumWidth - 2 * menuInset) - 2 * menuPadding;
    if (!item->icon.isNull() || item->checkType != QStyleOptionMenuItem::NotCheckable)
        width -= menuIconSize + menuIconSpace;
    if (item->menuItemType == QStyleOptionMenuItem::SubMenu)
        width -= menuPadding;
    if (hasMenuTrailingIcons(widget))
        width -= menuIconSize + menuIconSpace;
    return std::max(1, width);
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

// The kinds of toolbar that are top app bars: the small one, and the medium and large flexible ones.
bool isAppBarKind(const QString &kind) {
    return kind == QLatin1String("appBar") || kind == QLatin1String("appBarMedium") ||
           kind == QLatin1String("appBarLarge");
}

// The subtitle of a top app bar, which it shows under its title if it has one.
QString appBarSubtitle(const QWidget *bar) { return bar->property("materialSubtitle").toString(); }

// True for a top app bar whose title (and subtitle) are in the middle of it, as Compose's CenterAlignedTopAppBar has
// them, rather than at its leading end.
bool centresTitle(const QWidget *bar) {
    return bar->property("materialTitleAlignment").toString() == QLatin1String("center");
}

// The font of the title of a medium or large app bar on its own line(s).
QFont expandedTitleFont(const QWidget *bar, bool large) {
    QFont font = bar->font();
    font.setPixelSize(large ? appBarLargeTitlePixels : appBarMediumTitlePixels);
    font.setWeight(QFont::Normal);
    return font;
}

// The title of a medium or large app bar on its own line, broken at words onto a second line if it does not fit
// `width`; what does not fit the second line is elided, as is a first word wider than the line, on one line.
QStringList wrapAppBarTitle(const QString &title, const QFontMetrics &metrics, int width) {
    if (width <= 0 || metrics.horizontalAdvance(title) <= width)
        return {title};
    const QStringList words = title.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString first;
    qsizetype used = 0;
    for (; used < words.size(); ++used) {
        const QString next = first.isEmpty() ? words[used] : first + QLatin1Char(' ') + words[used];
        if (metrics.horizontalAdvance(next) > width)
            break;
        first = next;
    }
    if (used == 0)
        return {metrics.elidedText(title, Qt::ElideRight, width)};
    if (used == words.size())
        return {first};
    return {first, metrics.elidedText(words.mid(used).join(QLatin1Char(' ')), Qt::ElideRight, width)};
}

// How high a top app bar is when it is expanded: more than the small one for a medium or large one, more again with a
// subtitle, and a line more when its title takes two lines at the width that the bar has.
int expandedAppBarHeight(const QWidget *bar) {
    const QString kind = toolBarKind(bar);
    const bool large = kind == QLatin1String("appBarLarge");
    if (!large && kind != QLatin1String("appBarMedium"))
        return appBarHeight;
    const bool subtitled = !appBarSubtitle(bar).isEmpty();
    int height = large ? (subtitled ? appBarLargeSubtitledHeight : appBarLargeHeight)
                       : (subtitled ? appBarMediumSubtitledHeight : appBarMediumHeight);
    if (wrapAppBarTitle(bar->windowTitle(), QFontMetrics(expandedTitleFont(bar, large)),
                        bar->width() - 2 * appBarTitleSpace)
            .size() > 1)
        height += large ? appBarLargeTitleLine : appBarMediumTitleLine;
    return height;
}

// The kinds of toolbar that are docked toolbars, across the bottom (or the top) of a window.
bool isDockedKind(const QString &kind) {
    return kind == QLatin1String("docked") || kind == QLatin1String("dockedVibrant");
}

// The scroll behaviour that a top app bar or a docked toolbar asks for with its property.
QString scrollBehavior(const QWidget *bar) { return bar->property("materialScrollBehavior").toString(); }

// True for a bar that scrolls away as what is under it scrolls down, and comes back as soon as it scrolls up: a top
// app bar with enterAlways, and a docked toolbar across a window with exitAlways.
bool scrollsAwayKind(const QWidget *bar) {
    const QString kind = toolBarKind(bar);
    if (isAppBarKind(kind))
        return scrollBehavior(bar) == QLatin1String("enterAlways");
    return isDockedKind(kind) && scrollBehavior(bar) == QLatin1String("exitAlways") &&
           static_cast<const QToolBar *>(bar)->orientation() == Qt::Horizontal;
}

// The child of a scroll area whose connection to its vertical scroll bar repaints the app bars of its window.
const char *const appBarWatcher = "_nt_materialAppBarWatcher";
// Whether a bar that scrolls away is away, and the scrolling it last saw.
const char *const barAway = "_nt_materialBarAway";
const char *const barOffset = "_nt_materialBarOffset";

// The vertical scroll bar of what scrolls under a bar. For a top app bar it is that of a scroll area of the same window
// that is shown, starts at or below the bottom of the bar and is at least half as high as the room of the window below
// the bar; for a docked toolbar one that ends at or above its top and is at least half as high as the room above it.
// A small scroll area elsewhere in the window does not count. Of several, the one that is scrolled furthest.
const QScrollBar *contentScrollBar(const QToolBar *bar) {
    const QWidget *window = bar->window();
    const bool below = !isDockedKind(toolBarKind(bar));
    const int top = bar->mapTo(window, QPoint(0, 0)).y();
    const int bottom = top + bar->height();
    const int room = below ? window->height() - bottom : top;
    const QList<QAbstractScrollArea *> areas = window->findChildren<QAbstractScrollArea *>();
    const QScrollBar *found = nullptr;
    for (const QAbstractScrollArea *area : areas) {
        const QScrollBar *scroll = area->verticalScrollBar();
        if (!scroll || !area->isVisible() || 2 * area->height() < room)
            continue;
        const int y = area->mapTo(window, QPoint(0, 0)).y();
        if (below ? y < bottom : y + area->height() > top)
            continue;
        if (!found || scroll->value() - scroll->minimum() > found->value() - found->minimum())
            found = scroll;
    }
    return found;
}

// How far the content under a top app bar is scrolled down from the top, 0 if it is not.
int scrolledUnderBy(const QToolBar *bar) {
    const QScrollBar *scroll = contentScrollBar(bar);
    return scroll ? std::max(0, scroll->value() - scroll->minimum()) : 0;
}

// True while the content under a top app bar is scrolled.
bool scrolledUnder(const QToolBar *bar) { return scrolledUnderBy(bar) > 0; }

// A bar that scrolls away goes when what is under it scrolls down and comes back when it scrolls up, or is at the top.
// A step back that leaves it at its maximum is not the user's: the room that the bar gave up made the scroll area
// longer, which took that much off the range. A scroll area that comes or goes (`scrolled` false) does not move it.
void followScrolling(QToolBar *bar, bool scrolled) {
    const QScrollBar *scroll = contentScrollBar(bar);
    const int offset = scroll ? std::max(0, scroll->value() - scroll->minimum()) : 0;
    const int previous = bar->property(barOffset).toInt();
    bar->setProperty(barOffset, offset);
    bool away = bar->property(barAway).toBool();
    const bool down = scrolled && offset > previous;
    const bool up = scrolled && offset < previous && scroll->value() < scroll->maximum();
    if (offset == 0 || up)
        away = false;
    else if (down)
        away = true;
    bar->setProperty(barAway, away);
}

// Repaints the top app bars of the window of `widget`, whose color may follow what is scrolled in it. A medium or large
// one, and a bar that scrolls away (a docked toolbar too), also asks for the height that the scrolling gives it.
// `scrolled` is true when something scrolled, false when a scroll area was shown or hidden.
void updateAppBars(const QWidget *widget, bool scrolled) {
    const QList<QToolBar *> bars = widget->window()->findChildren<QToolBar *>();
    for (QToolBar *bar : bars) {
        const QString kind = toolBarKind(bar);
        if (!isAppBarKind(kind) && !isDockedKind(kind))
            continue;
        const bool away = scrollsAwayKind(bar);
        if (away)
            followScrolling(bar, scrolled);
        bar->update();
        if (away || expandedAppBarHeight(bar) > appBarHeight)
            bar->updateGeometry();
    }
}

bool isVibrantToolBar(const QWidget *widget) {
    const QString kind = toolBarKind(widget);
    return kind == QLatin1String("floatingVibrant") || kind == QLatin1String("dockedVibrant");
}

bool isToolBarItem(const QWidget *widget) {
    if (!qobject_cast<const QToolButton *>(widget))
        return false;
    const QString kind = toolBarKind(widget->parentWidget());
    return kind == QLatin1String("floating") || kind == QLatin1String("floatingVibrant") || isDockedKind(kind) ||
           isAppBarKind(kind);
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

// How wide the shape of an icon button is: as wide as it is high by default, and narrower or wider with the property
// materialIconButtonWidth. Each is the icon and the room before and after it (IconButton*Tokens): 4 + 20 + 4,
// 6 + 20 + 6 and 10 + 20 + 10 for the extra small size, up to 32 + 40 + 32, 48 + 40 + 48 and 72 + 40 + 72 for the
// extra large one.
int iconButtonWidth(const QWidget *widget) {
    struct Widths {
        int height;
        int narrow;
        int wide;
    };
    static constexpr Widths widths[] = {{32, 28, 40}, {40, 32, 52}, {56, 48, 72}, {96, 64, 128}, {136, 104, 184}};
    const int height = iconButtonSize(widget).height;
    const QString name = widget->property("materialIconButtonWidth").toString();
    const bool narrow = name == QLatin1String("narrow");
    if (!narrow && name != QLatin1String("wide"))
        return height;
    for (const Widths &each : widths)
        if (each.height == height)
            return narrow ? each.narrow : each.wide;
    return height;
}

// The size of an icon button to a finger: its shape, and 48 at least either way, with the shape in the middle.
QSize iconButtonExtent(const QWidget *widget) {
    return QSize(std::max(touchTarget, iconButtonWidth(widget)), std::max(touchTarget, iconButtonSize(widget).height));
}

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

// A chip that helps, filters or suggests is raised over a shadow, without an outline, when it asks to be. An input
// chip has no such kind.
bool isElevatedChip(const QWidget *widget) {
    const QString kind = chipKind(widget);
    return !kind.isEmpty() && kind != QLatin1String("input") && widget->property("materialChipElevated").toBool();
}

// The kind of card that a button asks to be with its property; empty for none.
QString buttonCardKind(const QWidget *widget) {
    if (!qobject_cast<const QAbstractButton *>(widget))
        return QString();
    const QString name = widget->property("materialCard").toString();
    const bool known =
        name == QLatin1String("filled") || name == QLatin1String("elevated") || name == QLatin1String("outlined");
    return known ? name : QString();
}

// What a chip has before its label and after it. A filter chip that is chosen has a check mark where its icon goes,
// an input chip has a cross at the end, and its icon may be a round picture of the person it stands for.
struct ChipParts {
    bool lead = false;
    bool mark = false;   // the lead is the check mark
    bool avatar = false; // the lead is the icon, as a round picture
    bool cross = false;  // there is a cross at the end

    int before() const {
        if (avatar)
            return chipAvatarPadding + chipAvatar + chipIconPadding;
        return lead ? chipIcon + 2 * chipIconPadding : chipPadding;
    }
    int after() const { return cross ? chipIcon + 2 * chipIconPadding : chipPadding; }
};

ChipParts chipParts(const QWidget *widget, bool hasIcon, bool checked) {
    const QString kind = chipKind(widget);
    ChipParts parts;
    parts.mark = checked && kind == QLatin1String("filter");
    parts.lead = parts.mark || hasIcon;
    parts.cross = kind == QLatin1String("input");
    parts.avatar = parts.cross && hasIcon && widget->property("materialChipAvatar").toBool();
    return parts;
}

bool isSquareButton(const QWidget *widget) {
    return widget && widget->property("materialButtonShape").toString() == QLatin1String("square");
}

// What a framed QLineEdit asks of its text field with its properties. A framed spin box is an outlined field too, and
// takes its label the same way.
struct TextField {
    bool filled = false;
    bool search = false; // a search bar: a pill with a magnifier, and nothing over or under it
    QString label;
    QString supporting; // the line under the field: the supporting text, or the error, which takes its place
    bool error = false;
    QString counter; // the count of characters, at the end of the line under the field
    int above = 0; // the room over the container,
    int below = 0; // and under it

    QRect box(const QRect &widget) const { return widget.adjusted(0, above, 0, -below); }
};

// A text editor with its usual frame that is a text field: it asks for one with materialTextField, or has a label,
// supporting text or an error. Another keeps its plain outlined frame.
bool isEditorField(const QWidget *widget) {
    const auto *frame = qobject_cast<const QFrame *>(widget);
    if (!frame || !isTextEditor(widget) || frame->frameShape() != QFrame::StyledPanel)
        return false;
    const QString kind = widget->property("materialTextField").toString();
    return kind == QLatin1String("outlined") || kind == QLatin1String("filled") ||
           !widget->property("materialLabel").toString().isEmpty() ||
           widget->property("materialSupportingText").isValid() || widget->property("materialError").isValid();
}

// The kind, label, supporting text and error that a field has from its properties; the counter is the line edit's.
void readTextField(TextField &field, const QWidget *widget) {
    field.filled = widget->property("materialTextField").toString() == QLatin1String("filled");
    field.label = widget->property("materialLabel").toString();
    const QVariant supporting = widget->property("materialSupportingText");
    const QVariant error = widget->property("materialError");
    field.error = !error.toString().isEmpty();
    field.supporting = field.error ? error.toString() : supporting.toString();
    field.above = !field.filled && !field.label.isEmpty() ? fieldLabelRoom : 0;
    // The room is kept while either is there, even empty, so that an error that comes does not move what is below.
    field.below = supporting.isValid() || error.isValid() ? fieldSupportRoom : 0;
}

TextField textFieldOf(const QWidget *widget) {
    TextField field;
    if (isTextEditor(widget)) {
        if (isEditorField(widget))
            readTextField(field, widget);
        return field;
    }
    if (const auto *spin = qobject_cast<const QAbstractSpinBox *>(widget)) {
        if (spin->hasFrame()) { // a time input lays itself out before it would ask
            field.label = spin->property("materialLabel").toString();
            field.above = field.label.isEmpty() ? 0 : fieldLabelRoom;
        }
        return field;
    }
    const auto *edit = qobject_cast<const QLineEdit *>(widget);
    if (!edit || !edit->hasFrame())
        return field;
    field.search = edit->property("materialTextField").toString() == QLatin1String("search");
    if (field.search)
        return field;
    readTextField(field, edit);
    if (edit->property("materialCounter").toBool()) {
        field.counter = QStringLiteral("%1/%2").arg(edit->text().size()).arg(edit->maxLength());
        field.below = fieldSupportRoom;
    }
    return field;
}

// What a text editor that is a field adds to the room around its viewport: its text is 16 from the sides of the
// container as in a line edit, a single line is in the middle of a container of 56 (under the label of a filled one),
// and there is the room above and below the container.
QMargins editorFieldMargins(const QWidget *editor) {
    if (!isEditorField(editor))
        return {};
    const TextField field = textFieldOf(editor);
    const int line = QFontMetrics(editor->font()).height();
    constexpr int inset = editorFrame + editorDocumentMargin;
    const int top = field.filled && !field.label.isEmpty() ? editorFilledLabelTop
                                                           : std::max(inset, (textFieldHeight - line + 1) / 2);
    const int bottom = std::max(inset, textFieldHeight - top - line);
    constexpr int side = textFieldPadding - inset;
    return {side, field.above + top - inset, side, field.below + bottom - inset};
}

// Whether a text field has the focus; a spin box has it while its line edit has.
bool hasFieldFocus(const QWidget *field) {
    if (field->hasFocus())
        return true;
    if (isTextEditor(field)) { // or its viewport has
        const QWidget *focus = QApplication::focusWidget();
        return focus && field->isAncestorOf(focus);
    }
    if (!qobject_cast<const QAbstractSpinBox *>(field))
        return false;
    const QObjectList &children = field->children();
    return std::any_of(children.begin(), children.end(), [](const QObject *child) {
        const auto *edit = qobject_cast<const QLineEdit *>(child);
        return edit && edit->hasFocus();
    });
}

// The label rests where the text goes in an empty field that does not have the focus, and has floated up otherwise.
// A spin box always shows a value, so there its label has floated unless its text has been cleared.
bool isLabelUp(const QWidget *field) {
    if (const auto *spin = qobject_cast<const QAbstractSpinBox *>(field))
        return hasFieldFocus(spin) || !spin->text().isEmpty();
    if (const auto *editor = qobject_cast<const QPlainTextEdit *>(field)) // a QTextEdit too
        return hasFieldFocus(editor) || !editor->toPlainText().isEmpty();
    const auto *edit = qobject_cast<const QLineEdit *>(field);
    return !edit || edit->hasFocus() || !edit->text().isEmpty();
}

// What the popup of a QCompleter is drawn as: under a search bar its docked search view, under another line edit a
// menu. The popup is a list in the window of the line edit, which is the widget that it is open under.
// In the full-screen search view the completer completes the line edit of the view's header.
enum class CompleterPopup { None, Menu, SearchView, FullScreenView };

// Whether `widget` is the line edit in the header of a full-screen search view.
bool isFullScreenSearchField(const QWidget *widget) {
    return qobject_cast<const QLineEdit *>(widget) && widget->objectName() == QLatin1String(searchFieldName) &&
           widget->parentWidget() && widget->parentWidget()->objectName() == QLatin1String(searchViewName);
}

const QLineEdit *completerEdit(const QWidget *widget) {
    if (!qobject_cast<const QListWidget *>(widget))
        return nullptr;
    const auto *edit = qobject_cast<const QLineEdit *>(QtPrivate::WidgetAccess::data(widget).popupAnchor.data());
    return edit && edit->completer() ? edit : nullptr;
}

CompleterPopup completerPopupOf(const QWidget *widget) {
    const QLineEdit *edit = completerEdit(widget);
    if (!edit)
        return CompleterPopup::None;
    if (isFullScreenSearchField(edit))
        return CompleterPopup::FullScreenView;
    return textFieldOf(edit).search ? CompleterPopup::SearchView : CompleterPopup::Menu;
}

// Whether `popup` is open below `edit`; it opens over it where there is no room below.
bool isBelow(const QWidget *popup, const QWidget *edit) {
    return popup->y() >= edit->mapTo(popup->parentWidget(), QPoint(0, edit->height() / 2)).y();
}

// The docked search view that is open under (or over) the search bar `edit`, or nullptr.
const QWidget *openSearchView(const QWidget *edit) {
    const auto *line = qobject_cast<const QLineEdit *>(edit);
    if (!line || !line->completer())
        return nullptr;
    for (const QObject *child : edit->window()->children()) {
        const auto *list = qobject_cast<const QListWidget *>(child);
        if (list && list->isVisible() && QtPrivate::WidgetAccess::data(list).popupAnchor.data() == edit)
            return list;
    }
    return nullptr;
}

// A rectangle with one radius for its two corners at the top and another for those at the bottom; 0 is square.
QPainterPath endedPath(const QRectF &r, double top, double bottom) {
    const double most = std::min(r.width(), r.height()) / 2;
    top = std::clamp(top, 0.0, most);
    bottom = std::clamp(bottom, 0.0, most);
    QPainterPath path;
    path.moveTo(r.left(), r.top() + top);
    if (top > 0)
        path.arcTo(QRectF(r.left(), r.top(), 2 * top, 2 * top), 180, -90);
    path.lineTo(r.right() - top, r.top());
    if (top > 0)
        path.arcTo(QRectF(r.right() - 2 * top, r.top(), 2 * top, 2 * top), 90, -90);
    path.lineTo(r.right(), r.bottom() - bottom);
    if (bottom > 0)
        path.arcTo(QRectF(r.right() - 2 * bottom, r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom), 0, -90);
    path.lineTo(r.left() + bottom, r.bottom());
    if (bottom > 0)
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom), 270, -90);
    path.closeSubpath();
    return path;
}

// The search bar `box`: a pill, or while its search view is open, square on the side where the view goes on.
QPainterPath searchBarShape(const QRectF &box, const QWidget *edit) {
    const double round = box.height() / 2;
    const QWidget *view = openSearchView(edit);
    if (!view)
        return endedPath(box, round, round);
    const bool below = isBelow(view, edit);
    return endedPath(box, below ? round : 0, below ? 0 : round);
}

// The container of the popup of a completer, in its coordinates: a menu is round at all its corners, a search view
// only at those away from its bar.
QPainterPath completerShape(const QWidget *popup, CompleterPopup kind) {
    const QRectF r(popup->rect());
    if (kind == CompleterPopup::FullScreenView) { // square, under the header of the view
        QPainterPath path;
        path.addRect(r);
        return path;
    }
    if (kind == CompleterPopup::Menu) {
        QPainterPath path;
        path.addRoundedRect(r, menuCorner, menuCorner);
        return path;
    }
    const QLineEdit *edit = completerEdit(popup);
    const bool below = !edit || isBelow(popup, edit);
    return endedPath(r, below ? 0 : searchViewCorner, below ? searchViewCorner : 0);
}

// The divider between a search view and its bar, in the coordinates of the view.
QRectF searchViewDivider(const QWidget *popup) {
    const QLineEdit *edit = completerEdit(popup);
    const bool below = !edit || isBelow(popup, edit);
    return QRectF(0, below ? 0 : popup->height() - 1, popup->width(), 1);
}

// True if the plain separators of a menu part it into groups with gaps between them, rather than being lines.
bool isGroupedMenu(const QWidget *menu) {
    return menu && menu->property("materialMenu").toString() == QLatin1String("grouped");
}

} // namespace

bool QMaterialStyle::opensSubMenusInPlace(const QWidget *menu) {
    return menu && qobject_cast<const QMaterialStyle *>(menu->style()) &&
           menu->property("materialMenu").toString() == QLatin1String("vertical");
}

namespace {

// A container of a menu: the whole menu, or one group of a grouped menu, with the radii of its corners at the top
// and at the bottom.
struct MenuContainer {
    QRectF rect;
    double top;
    double bottom;
};

// The containers of a menu in `r`, its rectangle. A grouped menu has one for each group, with the gaps left out: the
// corners at the ends of the menu are its own, and those beside a gap smaller.
std::vector<MenuContainer> menuContainers(const QWidget *widget, const QRectF &r) {
    std::vector<MenuContainer> containers;
    const auto *menu = qobject_cast<const QMenu *>(widget);
    double top = r.top();
    if (menu && isGroupedMenu(menu)) {
        for (QAction *action : QtPrivate::MenuAccess::shownActions(menu)) {
            if (!action->isSeparator() || !action->text().isEmpty())
                continue;
            const QRect gap = menu->actionGeometry(action); // empty for a separator that is not shown
            if (gap.isEmpty())
                continue;
            // The separator holds the padding of the group above it, the gap and the padding of the one below.
            const double bottom = gap.top() + (gap.height() - menuGroupGap) / 2.0;
            containers.push_back({QRectF(r.left(), top, r.width(), bottom - top),
                                  containers.empty() ? menuCorner : menuGroupCorner, menuGroupCorner});
            top = bottom + menuGroupGap;
        }
    }
    containers.push_back({QRectF(r.left(), top, r.width(), r.bottom() - top),
                          containers.empty() ? menuCorner : menuGroupCorner, menuCorner});
    return containers;
}

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
    AppBarChannel,         // how far a top app bar has gone from its color at rest to that of content scrolled under it
    AppBarCollapseChannel, // how far a medium or large top app bar has collapsed, in pixels
    ToolBarHideChannel,    // how far a top app bar or a docked toolbar has scrolled away, from 0 to 1
    ElevationChannel,      // how far the shadow of an elevated chip or card falls, in pixels
    ScrollBarChannel,      // how far the handle of a scroll bar has faded in
    YearsChannel,          // how far the years of a date picker have faded in over its days
    SliderValueChannel,    // how far the value indicator of a slider has grown, from 0 to 1
    NavigationChannel = 100 // plus the index of a destination: how far its pill has opened
};
const char *const polishedFont = "_nt_materialFont";
const char *const polishedPalette = "_nt_materialPalette";
const char *const polishedFrame = "_nt_materialFrame"; // the frame style that a widget had
const char *const polishedIconSize = "_nt_materialIconSize"; // the icon size was given by the style
const char *const polishedMargins = "_nt_materialMargins";   // the viewport margins of a multi-line field
const char *const polishedPolicy = "_nt_materialPolicy";     // the vertical size policy that a growing field had
const char *const fieldWatcher = "_nt_materialFieldWatcher";

// Gives a text editor the room that its field needs around its viewport, or takes back what it was given.
void layOutEditorField(QAbstractScrollArea *editor) {
    const QMargins margins = editorFieldMargins(editor);
    if (margins.isNull() && !editor->property(polishedMargins).toBool())
        return;
    editor->setViewportMargins(margins);
    editor->setProperty(polishedMargins, margins.isNull() ? QVariant() : QVariant(true));
}

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

// A picture in its own colors that fills `shape`, with corners of `corner` (a circle if it is half the width or more),
// cut to a square from its middle. A painter clips to rectangles, so the picture is cut to the shape in an image of
// its own first.
void paintPicture(QPainter *painter, const QRect &shape, double corner, const QIcon &icon, QIcon::Mode mode,
                  QIcon::State state) {
    const qreal ratio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const QImage picture = icon.pixmap(shape.size(), ratio, mode, state).toImage();
    if (picture.isNull())
        return;
    const int across = std::max(1, static_cast<int>(std::lround(shape.width() * ratio)));
    const int side = std::min(picture.width(), picture.height());
    const QRectF middle((picture.width() - side) / 2.0, (picture.height() - side) / 2.0, side, side);
    QImage cut(across, across, QImage::Format_ARGB32_Premultiplied);
    cut.fill(Qt::transparent);
    {
        QPainter cutter(&cut);
        cutter.setRenderHint(QPainter::Antialiasing);
        const double scaled = std::min(corner * ratio, across / 2.0);
        QPainterPath outline;
        if (scaled >= across / 2.0)
            outline.addEllipse(QRectF(cut.rect()));
        else
            outline.addRoundedRect(QRectF(cut.rect()), scaled, scaled);
        cutter.fillPath(outline, QBrush(QColor(Qt::white)));
        cutter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        cutter.drawImage(QRectF(cut.rect()), picture, middle);
    }
    painter->drawImage(QRectF(shape), cut, QRectF(cut.rect()));
}

// A picture in its own colors that fills the circle `round`.
void paintAvatar(QPainter *painter, const QRect &round, const QIcon &icon, QIcon::Mode mode, QIcon::State state) {
    paintPicture(painter, round, round.width() / 2.0, icon, mode, state);
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

// The outline of an outlined text field or spin box in `around`, its widget's rectangle. The outline makes way for a
// label that has floated onto it.
void strokeOutlinedField(QPainter *painter, const QRectF &around, const TextField &field, const QWidget *widget,
                         const QColor &line, bool focused) {
    const QRectF box = around.adjusted(0, field.above, 0, -field.below);
    const double width = focused ? 3 : 1;
    if (!widget || field.label.isEmpty() || !isLabelUp(widget)) {
        strokeRounded(painter, box, textFieldCorner, line, width);
        return;
    }
    QFont small = widget->font();
    small.setPixelSize(fieldSmallPixels);
    const double from = box.left() + textFieldPadding - 4;
    const double to =
        std::min(box.right() - textFieldCorner, from + QFontMetrics(small).horizontalAdvance(field.label) + 8.0);
    const QRectF parts[] = {QRectF(around.left(), around.top(), from - around.left(), around.height()),
                            QRectF(to, around.top(), around.right() - to, around.height()),
                            QRectF(from, box.top() + 6, to - from, around.bottom() - box.top() - 6)};
    for (const QRectF &part : parts) {
        painter->save();
        painter->setClipRect(part, Qt::IntersectClip);
        strokeRounded(painter, box, textFieldCorner, line, width);
        painter->restore();
    }
}

// A size of slider, which a slider asks for with the property materialSliderSize. One that asks for none is extra
// small, the look that sliders have had here.
struct SliderSize {
    int track;     // how thick the track is,
    int handle;    // how long the handle is across it,
    double corner; // the corners of the track's outer ends,
    double inner;  // and of its ends beside the handle
};

const SliderSize &sizeOfSlider(const QWidget *widget) {
    static constexpr SliderSize extraSmall{16, 44, 8, 8};
    static constexpr SliderSize small{24, 44, 8, 2};
    static constexpr SliderSize medium{40, 52, 12, 2};
    static constexpr SliderSize large{56, 68, 16, 2};
    static constexpr SliderSize extraLarge{96, 108, 28, 2};
    const QString name = widget ? widget->property("materialSliderSize").toString() : QString();
    if (name == QLatin1String("small"))
        return small;
    if (name == QLatin1String("medium"))
        return medium;
    if (name == QLatin1String("large"))
        return large;
    if (name == QLatin1String("extraLarge"))
        return extraLarge;
    return extraSmall;
}

// What the style needs to know of a slider, from its style option or, where it is given none, from the widget.
struct SliderFacts {
    bool horizontal = true;
    int ticks = 0;
    int minimum = 0;
    int maximum = 0;
};

SliderFacts factsOfSlider(const QStyleOption *option, const QWidget *widget) {
    SliderFacts facts;
    if (const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
        facts.horizontal = slider->orientation == Qt::Horizontal;
        facts.ticks = slider->tickPosition;
        facts.minimum = slider->minimum;
        facts.maximum = slider->maximum;
    } else if (const auto *widgetSlider = qobject_cast<const QSlider *>(widget)) {
        facts.horizontal = widgetSlider->orientation() == Qt::Horizontal;
        facts.ticks = widgetSlider->tickPosition();
        facts.minimum = widgetSlider->minimum();
        facts.maximum = widgetSlider->maximum();
    }
    return facts;
}

// Whether a slider shows its value over the handle while it is pressed or changed with the keys: one with tick marks
// does, as Material's discrete sliders do, unless materialSliderValue says otherwise.
bool showsSliderValue(const QWidget *widget, int ticks) {
    const QVariant asked = widget ? widget->property("materialSliderValue") : QVariant();
    if (asked.isValid())
        return asked.toBool();
    return ticks != 0;
}

QFont sliderValueFont(const QWidget *widget) { return groupTitleFont(widget ? widget->font() : QApplication::font()); }

int sliderValueWidth(const QFontMetrics &metrics, int value) {
    return std::max(valueWidth, metrics.horizontalAdvance(QString::number(value)) + 2 * valuePadding);
}

// Whether a slider draws its value indicator inside itself, in room that it keeps for it, rather than in an overlay
// over what is above it: one that asks for that with materialSliderValueRoom, and one that is a window of its own,
// which has nothing above it to draw over.
bool keepsSliderValueRoom(const QWidget *widget) {
    return widget && (widget->isWindow() || widget->property("materialSliderValueRoom").toBool());
}

// The room that a slider keeps for its value indicator: above a horizontal one, at the left of a vertical one.
int sliderValueRoom(const QStyleOption *option, const QWidget *widget) {
    const SliderFacts facts = factsOfSlider(option, widget);
    if (!showsSliderValue(widget, facts.ticks) || !keepsSliderValueRoom(widget))
        return 0;
    if (facts.horizontal)
        return valueHeight + valueGap;
    const QFontMetrics metrics(sliderValueFont(widget));
    return std::max(sliderValueWidth(metrics, facts.minimum), sliderValueWidth(metrics, facts.maximum)) + valueGap;
}

// The part of a slider that is not kept for its value indicator.
QRect sliderBody(const QStyleOption *option, const QWidget *widget) {
    const int room = sliderValueRoom(option, widget);
    if (factsOfSlider(option, widget).horizontal)
        return option->rect.adjusted(0, room, 0, 0);
    return option->rect.adjusted(room, 0, 0, 0);
}

// A part of a slider's track, with one radius for the corners of the end where it starts (its left, or its top) and
// another for those of the other end.
QPainterPath trackPart(const QRectF &r, double first, double second, bool horizontal) {
    if (horizontal)
        return sidedPath(r, first, second);
    // Made across and turned over the diagonal, which swaps x and y.
    return QTransform(0, 1, 1, 0, 0, 0).map(sidedPath(QRectF(r.top(), r.left(), r.height(), r.width()), first, second));
}

// Whether a slider is a centered one, whose active track runs from the middle of its range to the handle.
bool isCenteredSlider(const QWidget *widget) {
    return widget && widget->property("materialSlider").toString() == QLatin1String("centered");
}

// The size of the inset icon of a slider of the given size, or 0 for the sizes that have none.
int sliderIconSize(const SliderSize &size) { return size.track >= 96 ? 32 : size.track >= 40 ? 24 : 0; }

const char *const sliderValueWatcher = "_nt_materialSliderValue";

// The value indicator of a slider, drawn over whatever is above the slider: a child of the slider's window, above its
// other children, as a menu is, that the pointer goes through. The slider's paint tells it what to show.
class SliderValueOverlay : public QWidget {
public:
    explicit SliderValueOverlay(QWidget *parent) : QWidget(parent) {
        setObjectName(QStringLiteral("_nt_sliderValueIndicator"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
    }

    // Shows `text` in a pill as big as the overlay, `scale` of its size about `anchor` (where it meets the handle).
    void set(const QString &text, const QFont &font, const QColor &fill, const QColor &ink, double scale,
             const QPointF &anchor) {
        if (text == m_text && font == m_font && fill == m_fill && ink == m_ink && scale == m_scale &&
            anchor == m_anchor)
            return;
        m_text = text;
        m_font = font;
        m_fill = fill;
        m_ink = ink;
        m_scale = scale;
        m_anchor = anchor;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        if (m_scale <= 0)
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(m_anchor);
        painter.scale(m_scale, m_scale);
        painter.translate(-m_anchor);
        const QRectF box(rect());
        fillRounded(&painter, box, box.height() / 2, m_fill);
        painter.setFont(m_font);
        painter.setPen(m_ink);
        painter.drawText(box, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, m_text);
    }

private:
    QString m_text;
    QFont m_font;
    QColor m_fill;
    QColor m_ink;
    double m_scale = 0;
    QPointF m_anchor;
};

// Keeps the overlay of a slider's value indicator. It makes it as the handle is pressed or the slider gets the focus,
// outside any paint, so that it is there when the slider is next painted; the slider's paint then places it. It is a
// child of the slider and deletes the overlay with it, or when unpolish() deletes it.
class SliderValueWatcher : public QObject {
public:
    explicit SliderValueWatcher(QSlider *slider) : QObject(slider), m_slider(slider) {
        setObjectName(QLatin1String(sliderValueWatcher));
        QObject::connect(slider, &QAbstractSlider::sliderPressed, this, [this] { overlay(); });
        slider->installEventFilter(this);
    }
    ~SliderValueWatcher() override { delete m_overlay.data(); }
    SliderValueWatcher(const SliderValueWatcher &) = delete;
    SliderValueWatcher &operator=(const SliderValueWatcher &) = delete;

    // The overlay, in the slider's window, made if it has none there.
    SliderValueOverlay *overlay() {
        QWidget *window = m_slider->window();
        if (window == m_slider || !showsSliderValue(m_slider, m_slider->tickPosition()) ||
            keepsSliderValueRoom(m_slider))
            return nullptr;
        // The slider has moved to another window. This may be called while the old one is painted.
        if (m_overlay && m_overlay->parentWidget() != window) {
            m_overlay->hide();
            m_overlay->deleteLater();
            m_overlay = nullptr;
        }
        if (!m_overlay) {
            m_overlay = new SliderValueOverlay(window);
            m_overlay->hide();
        }
        return m_overlay;
    }

    // The overlay if there is one.
    SliderValueOverlay *existing() const { return m_overlay; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::KeyPress)
            overlay();
        else if (event->type() == QEvent::Hide && m_overlay)
            m_overlay->hide();
        return QObject::eventFilter(watched, event);
    }

private:
    QSlider *m_slider;
    QPointer<SliderValueOverlay> m_overlay;
};

// Lets the handle of a scroll bar fade out once the bar has been left alone for a while. It wakes the bar when it is
// shown, when its value changes (by the wheel, the keys, a finger that flicks the content with QScroller, or the
// program), when the pointer comes onto it or leaves it, and when it is pressed or released; its timer then lets it
// rest. Whether it is awake is a property of the bar, which the style reads as it paints. It is a child of the bar, so
// it goes with it, or when unpolish() deletes it.
class ScrollBarRest : public QObject {
public:
    explicit ScrollBarRest(QScrollBar *bar) : QObject(bar), m_bar(bar) {
        setObjectName(QLatin1String(scrollRestWatcher));
        m_timer.setSingleShot(true);
        m_timer.setInterval(scrollRestDelay);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { setAwake(false); });
        QObject::connect(bar, &QAbstractSlider::valueChanged, this, [this] { wake(); });
        bar->installEventFilter(this);
    }

    // Shows the handle, which rests again once the bar has been left alone for the delay.
    void wake() {
        m_timer.start();
        setAwake(true);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            wake();
            break;
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void setAwake(bool awake) {
        const QVariant was = m_bar->property(scrollAwake);
        if (!was.isValid() || was.toBool() != awake)
            m_bar->setProperty(scrollAwake, awake);
        m_bar->update();
    }

    QScrollBar *m_bar;
    QTimer m_timer;
};

// Wakes the scroll bars of a scroll area while a mouse moves over the area, its viewport or its bars, as overlay scroll
// bars do; they rest again once the pointer stops or leaves. A move without a button is a mouse that hovers: a finger
// sent as the mouse always holds the left button, so a tap does not wake them. The viewport gets no such moves without
// mouse tracking, which would change what item views emit, so from the time the pointer enters the area until it
// leaves this also filters the moves that reach its window. It is a child of the area, so it goes with it, or when
// unpolish() deletes it, and the filters go with it.
class ScrollAreaHover : public QObject {
public:
    explicit ScrollAreaHover(QAbstractScrollArea *area) : QObject(area), m_area(area) {
        setObjectName(QLatin1String(scrollHoverWatcher));
        area->installEventFilter(this);
        watchViewport();
    }
    ~ScrollAreaHover() override {
        if (m_window)
            m_window->removeEventFilter(this);
        if (m_viewport)
            m_viewport->removeEventFilter(this);
    }
    ScrollAreaHover(const ScrollAreaHover &) = delete;
    ScrollAreaHover &operator=(const ScrollAreaHover &) = delete;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        const QEvent::Type type = event->type();
        if (watched == m_window.data()) {
            if (type == QEvent::MouseMove) {
                const auto *move = static_cast<QMouseEvent *>(event);
                if (move->buttons() == Qt::NoButton && m_area->isVisible() &&
                    m_area->rect().contains(m_area->mapFrom(m_area->window(), move->position().toPoint())))
                    wakeBars();
            }
        } else if (type == QEvent::Enter) {
            // The area is entered before its viewport, and stays entered while the pointer is on either.
            watchViewport();
            watchWindow(true);
        } else if ((type == QEvent::Leave || type == QEvent::Hide) && watched == m_area) {
            watchWindow(false);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    // The viewport may have been replaced with setViewport().
    void watchViewport() {
        QWidget *viewport = m_area->viewport();
        if (viewport == m_viewport.data())
            return;
        if (m_viewport)
            m_viewport->removeEventFilter(this);
        m_viewport = viewport;
        if (viewport)
            viewport->installEventFilter(this);
    }
    void watchWindow(bool on) {
        QWindow *window = on ? m_area->window()->windowHandle() : nullptr;
        if (window == m_window.data())
            return;
        if (m_window)
            m_window->removeEventFilter(this);
        m_window = window;
        if (window)
            window->installEventFilter(this);
    }
    void wakeBars() {
        for (QScrollBar *bar : {m_area->verticalScrollBar(), m_area->horizontalScrollBar()}) {
            if (!bar || !bar->isVisible())
                continue;
            if (auto *rest = static_cast<ScrollBarRest *>(
                    bar->findChild<QObject *>(QLatin1String(scrollRestWatcher), Qt::FindDirectChildrenOnly)))
                rest->wake();
        }
    }

    QAbstractScrollArea *m_area;
    QPointer<QWidget> m_viewport;
    QPointer<QWindow> m_window;
};

// Whether the scroll bar keeps its handle shown at rest: `materialScrollBar` = "always" on it or on its scroll area.
bool scrollBarAlwaysShown(const QWidget *bar) {
    for (const QWidget *widget = bar; widget; widget = widget->parentWidget()) {
        if (widget->property("materialScrollBar").toString() == QLatin1String("always"))
            return true;
        if (qobject_cast<const QAbstractScrollArea *>(widget))
            break;
    }
    return false;
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
    \li \c materialToolBar \c "docked" makes a QToolBar a docked toolbar, a bar of 64 across the whole of the
        window in the surface container color, with square corners and no shadow; \c "dockedVibrant" is the same in
        the primary container color. Put it in the bottom tool bar area of a QMainWindow with
        addToolBar(Qt::BottomToolBarArea, bar), or at the bottom of a layout. Its buttons, 48 and in the colors of a
        floating toolbar's, are spread out along it, 16 in from its ends and from 4 to 32 apart, and centred when 32
        apart leaves room; its separators are not shown. A snackbar goes above it.
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
        start if an error should not move what is below it. Icons added with QLineEdit::addAction() are the leading
        and trailing icons, 24 and 12 from the edges, in the color of the field's icons (a trailing one in that of an
        error). \c materialPrefix and \c materialSuffix are shown before the text and at the end of the field once the
        label has floated, and \c materialCounter set to \c true counts the characters against maxLength() at the end
        of the line under the field.
    \li \c materialIconButton of a QPushButton or a QToolButton: \c "standard", \c "filled", \c "tonal" or
        \c "outlined", a button that is its icon alone, in the sizes of \c materialButtonSize (small without it) with
        icons of 20 to 40, round or, with \c materialButtonShape, square. A checkable one shows that it is checked. One
        smaller than 48 is 48 to a finger. \c materialIconButtonWidth \c "narrow" or \c "wide" makes its shape
        narrower or wider than it is high, with the icon in the middle: 28 or 40 for the extra small size (32 by
        default), 32 or 52 for the small one, 48 or 72 for the medium one, 64 or 128 for the large one and 104 or 184
        for the extra large one; \c "default" or none keeps it as wide as it is high.
    \li \c materialToolBar \c "appBar" makes a QToolBar a top app bar, 64 high, with its window title as the
        heading. The actions before the first separator are before the title, such as the one that goes back, and the
        others after it; without a separator, all are after it. Add it to a QMainWindow with addToolBar(), or to the
        top of a layout. Its container is the surface color, and fades to the surface container color while the
        content under it is scrolled: a QAbstractScrollArea of the same window that is shown, starts at or below the
        bottom of the bar, is at least half as high as the room of the window below the bar and is scrolled down from
        the top, as a list or a QScrollArea that fills the central widget is. \c "appBarMedium" and \c "appBarLarge"
        make it the medium or the large flexible app bar, 112 or 120 high when expanded, with its buttons in the top
        row of 64 and the title on a line of its own below them, 16 from the left, in 28 or 36 pixels. Once that scroll
        area is scrolled down the bar collapses to the height of the small one, and the content moves up with it; on
        the way the big title fades out and the small one comes into the row. At the top it is expanded again. With
        motion it collapses and expands on a spring. A title too long for its line goes on two lines, and the bar is
        a line (36 or 44) higher.
    \li \c materialSubtitle of a top app bar is a subtitle under its title, in the on surface variant color: in 12
        pixels in the small bar, whose title and subtitle are centred together in its 64, and in 14 or 16 pixels in the
        medium or large one, which is then 136 or 152 high when expanded. \c materialTitleAlignment \c "center" puts
        the title and the subtitle in the middle of the bar rather than after what leads it.
    \li \c materialScrollBehavior of a top app bar is how it follows the scroll area under it: \c "pinned" keeps it
        where it is (a medium or large one does not collapse); \c "exitUntilCollapsed", the default, collapses a
        medium or large one as above, and keeps the small one; \c "enterAlways" also takes the bar away as the content
        scrolls down and brings it back as soon as it scrolls up or is at the top. \c "exitAlways" on a docked toolbar
        across the bottom of a window takes it away down in the same way, following the scroll area above it. A bar
        that goes gives its room to the content, and its buttons go with it; with motion it slides on a spring.
    \li \c materialTabBar \c "navigationBar" makes a horizontal QTabBar a bottom navigation bar, 64 high: its tabs
        share the width, each an icon over its label, with a pill behind the one that is current. With a QTabWidget
        whose tabs are at the south it is the navigation of an application and its pages:
        tabs->tabBar()->setProperty("materialTabBar", "navigationBar"). In a window 600 wide or more each destination
        is the icon beside its label in a pill of 40, the destinations as wide as the widest needs and in the middle
        of the bar (as long as they fit). \c materialNavigationItems \c "horizontal" or \c "vertical" on the QTabBar
        chooses one or the other whatever the width of the window.
    \li \c materialTabBar \c "navigationRail" makes a vertical QTabBar a navigation rail, for a screen that is wide: 96
        across, its destinations one under another from the top. With a QTabWidget whose tabs are at the west it is the
        navigation of an application and its pages. \c "navigationRailExpanded" makes it the expanded rail: as wide as
        its longest destination needs, from 220 to 360, each destination a row of 56 with the icon and the label beside
        it, and a pill across the row behind the one that is current. A dot badge is on the icon and a count at the end
        of the row. Setting the property while the rail is shown expands or collapses it at once. Destinations that do
        not fit the height of the rail scroll, with the wheel or a drag, and the current one is scrolled into sight;
        there are no scroll buttons. The corner widget of the QTabWidget at the side of the rail (Qt::TopLeftCorner at
        the west) goes at the top of the rail, 44 down and as wide as it, for a menu button and a floating action
        button; the destinations start 40 under it and scroll out of sight under it. \c "navigationRailModal" is the
        modal expanded rail: the expanded rail in the surface container color, with its trailing corners rounded,
        over the pages, which keep the room of the collapsed rail, and a scrim of black at 32 percent over them. A
        press on the scrim, Escape or the choice of a destination sets the property back to \c "navigationRail"; a
        menu button opens it by setting \c "navigationRailModal".
    \li \c materialBadge of an icon button or a button of a toolbar puts a badge on its icon: \c true is a dot, and a
        number above 0, or a text, is shown in it (over 999 as 999+); 0, \c false or an empty text take it away. The
        destinations of a navigation bar or rail have theirs in \c materialBadges of the QTabBar, a QVariantList with
        one such value for each, in order.
    \li \c materialProgress \c "circular" draws a QProgressBar as a ring of 40: how far it is, or while it is busy an
        arc that goes round. \c "circularWavy" is the wavy circular indicator, 48: the part that is done, or the arc
        of a busy one, is a wave that goes 1.6 out and in from the circle, with a whole number of crests about 15
        apart that turn while motion is enabled; the track stays a plain circle, and the wave is flat below a tenth and
        from 95 in 100.
    \li \c materialScrollBar \c "always", on a QScrollBar or on the QAbstractScrollArea that it belongs to, keeps the
        scroll bar's handle shown at rest. Otherwise the handle fades out once the bar has been left alone for 1.5
        seconds, and fades in when the value changes (by the wheel, the keys, the program, or a finger that flicks the
        content with QScroller), when the pointer comes onto the bar, and while it is pressed. The bar keeps its room
        all the while; only its drawing fades. Without motion the handle stays shown.
    \li \c materialStatusBar \c "snackbar" makes the status bar of a QMainWindow a snackbar: it takes no room, floats
        over the content, above a navigation bar, beside a navigation rail and above a floating action button that is at
        the bottom, while it has a message, from showMessage() until clearMessage() or the message's timeout, and its
        permanent widgets, buttons, are its actions. With \c materialCloseButton set to \c true it has a cross at its
        end, after the actions, which clears the message.
    \li \c materialChip of a QPushButton: \c "assist", \c "filter", \c "input" or \c "suggestion" makes it a chip,
        32 high (48 to a finger) with small corners and an outline. A filter chip is made checkable: checked, it is
        filled and a check mark takes the place of its icon, which makes it wider. An input chip stands for something
        that was entered and has a cross; a click anywhere on it is clicked(), on which the program takes it away. A
        chosen chip of any kind is filled.
    \li \c materialChipElevated \c true of an assist, filter or suggestion chip makes it an elevated chip: it has no
        outline, and its container, \c SurfaceContainerLow, is raised over a shadow, which falls further while the
        pointer is over it. An input chip has no elevated kind and ignores the property.
    \li \c materialChipAvatar \c true of an input chip shows its icon as an avatar: a round picture of 24, in its own
        colors, 4 from the start of the chip, which makes the chip 2 wider than one with a plain icon. Without the
        property an icon is an icon of 18 like that of any chip.
    \li \c materialCard of a QFrame: \c "filled", \c "elevated" or \c "outlined" draws it as a card with corners of
        12, whatever its frame shape; its layout and its children are the contents. Under this style a QFrame whose
        shape is \c HLine or \c VLine is a divider, a quiet line of 1, whatever its shadow and line width.
    \li \c materialCard of a QPushButton makes it a card that can be clicked, as a QFrame cannot be: drawn as the
        card of that kind is, with a layer of the color of its content under the pointer and while it is pressed, the
        ripple of a button, and a shadow that falls further on hover for an elevated card. Its text, if it has one,
        is in the middle; more often its layout holds the contents, and it asks for the size of that layout. The
        program builds unchanged with Qt, where it is a push button.
    \li \c materialTextField \c "search" makes a QLineEdit a search bar: a pill, 56 high, with a magnifier before the
        text. The placeholder text says what is searched, and with \l{QLineEdit::setClearButtonEnabled()}{a clear
        button} there is a cross to empty it. Given a QCompleter, the bar opens Material's docked search view: the
        popup of the completer goes on from the bar, as wide as it, with a divider between them and corners of 28 at
        the far end, while the bar's corners towards it turn square. Its rows are 56 high, with the icon that the model
        has in Qt::DecorationRole before the text. Under another line edit the popup of a completer is drawn as a
        menu, with rows 48 high and corners of 16. In either, the part of each suggestion that matches what was typed
        (whatever its case) is in \c OnSurface and medium weight, and the rest in \c OnSurfaceVariant.
    \li \c materialSearchView of a search bar with a QCompleter: on a compact window (narrower than 600) the bar opens
        Material's full-screen search view when it gets the focus or is tapped, and on a wider one the docked view;
        \c "fullScreen" or \c "docked" chooses either on any window. The full-screen view covers the window, growing
        from the bar's rectangle, with a header of 72 of its own: an arrow back, a line edit with a clear button, and a
        divider under them, and the suggestions under the header. What is typed there is the bar's text, and the bar
        emits \l{QLineEdit::textEdited()}{textEdited()}, \l{QLineEdit::returnPressed()}{returnPressed()} and
        \l{QLineEdit::editingFinished()}{editingFinished()} for it. The arrow back, Escape (after the suggestions have
        closed), Return, a chosen suggestion or the focus moving elsewhere closes the view, leaving nothing focused,
        except in the last case.
    \li \c materialAvatar \c true of a QAction that is added to a QLineEdit with
        \l{QLineEdit::addAction()}{addAction()} at its trailing end shows its icon as an avatar: a round picture of 30,
        in its own colors, at the far end, past the clear button. In a search bar the actions at the end have targets
        of 48, side by side. Under the pointer and while held, each action of a text field shows the circle of 40 of
        a standard icon button, and a press ripples.
    \li \c materialTabBar \c "secondary" makes the tabs of a QTabBar secondary ones, for what is inside a page: the
        line under the current one is as wide as the tab and 2 high, and its label is not in the accent color.
    \li \c materialTimeInput of a QTimeEdit: \c true draws it as Material's time input, a large box for each of the
        hour and the minute (and the second, if the format shows it), and a choice of AM or PM if the format has one.
        A click chooses a box; digits typed, the arrow keys and the wheel change what is in it.
    \li A QTextEdit or QPlainTextEdit with its usual frame is an outlined field, its text 16 and clear of the border.
        With \c materialTextField \c "outlined" or \c "filled", \c materialLabel, \c materialSupportingText or
        \c materialError it is a multi-line text field, drawn as a QLineEdit's is, its label resting on the first
        line. With \c materialTextField it grows with its lines up to its maximum height: the style sets its vertical
        size policy to QSizePolicy::Maximum, so give it a fixed height to keep one. The text cursor of these and of a
        QLineEdit is in the accent color (red with an error) and 2 wide.
        A QGroupBox is a section: its title in the accent color over an outlined card, or over a line if it is flat; the
        check box of a checkable one is a Material one. Neither needs a property.
    \li A spin box, among them a date or time edit, is an outlined field of 56 with a button that steps down and one
        that steps up, each 48 wide; a date edit with a calendar pop-up has one button, for the calendar. Scroll bars
        are thin handles without a track or arrows. Neither needs a property.
    \li The text of an item of a QListView or QListWidget makes it a list item of one, two or three lines. A line
        break puts supporting text under the headline, in a smaller size and a quieter color, and makes the row 72
        high; a second line break in the supporting text makes a row of three lines, 88 high, whose supporting text
        wraps to two lines and is shortened after the second. A tab in the headline puts what follows it at the
        trailing end, as trailing supporting text (\c{"Inbox\t100+"}). The icon and check box of a row of three lines
        are at its top, beside the headline. Qt draws the same text with its line breaks, on as many lines, and the
        tab as a space:

        \code
        list->addItem(new QListWidgetItem(icon, "Brunch this weekend?\nAli Connors: I'll be in your neighborhood"));
        list->addItem(new QListWidgetItem(icon, "Summer BBQ\t15 min\nTo: Alex, Scott, Jennifer\nWish I could come"));
        \endcode
    \li An item that has a check state (Qt::CheckStateRole, QListWidgetItem::setCheckState()) shows Material's
        check box at the trailing end of its row, after any trailing supporting text; a click on it, or Space in a
        QListWidget, toggles it if the item has Qt::ItemIsUserCheckable. The style gives its place as
        \c SE_ItemViewItemCheckIndicator.
    \li \c materialListAvatars \c true of a QListView or QListWidget draws the icons of its rows as avatars, pictures
        in their own colors in circles of 40; \c materialListImages \c true draws them as images, squares of 56 with
        round corners, which make a row at least 72 high.
    \li \c materialListOverline \c true of a QListView or QListWidget draws the first line of the text of a row of
        three lines as an overline, small and quiet, over the headline, which is then the second line, with one line
        of supporting text under it (\c{"PROJECT\nBrunch this weekend?\nAli Connors"}).
    \li \c materialList \c "segmented" of a QListView or QListWidget makes it a segmented list, as in Material 3
        Expressive: each row a container of its own, 2 from the next, with corners of 16 at the ends of the list and
        of 4 between rows; a selected row is in the secondary container and round all over.
    \li \c materialSheet of a QDialog: \c "bottom" makes it a modal bottom sheet, against the bottom of its parent's
        window, as high as what is in it and no wider than 640; \c "drawer" makes it a modal drawer, against the left
        edge, as high as the window and up to 360 wide. It slides in from its edge and has no title bar. Closed, it is
        hidden at once, as any dialog is (isVisible() is \c false, the signals are emitted and exec() returns), and a
        picture of it that takes no input slides back to its edge in a window of its own. On Android the window behind
        is dimmed, the dimming fading in and out with the sheet, and a tap on it or Back dismisses the sheet; elsewhere
        a press in a window that a modal sheet blocks dismisses it. A bottom sheet has a drag handle at its top, for
        which room is made above what is in it. A sheet is dragged toward its edge by a press on its handle or
        background, or on a child that leaves the mouse to it, the dimming fading with the distance: let go past a
        third of the way, or flung, it is rejected as Back does, and otherwise it springs back.
    \li \c materialSheetHandle \c false on a bottom sheet hides its drag handle and the room for it; the sheet is still
        dragged.
    \li \c materialList \c "navigationDrawer" of a QListWidget, for the list of a drawer: the one that is current is
        marked with a pill, and the list has no frame. Open a destination from itemClicked() and accept() the drawer.
    \li \c materialMenu \c "grouped" of a QMenu: the separators that addSeparator() adds are gaps of 2 rather than
        lines, and part the menu into groups, each a container of its own with the round corners of the menu at its
        ends and corners of 8 beside a gap, as in the menus of Material 3 Expressive with gaps. A section, a separator
        with a text, is no gap. Without the property a separator is a line.
    \li \c materialMenu \c "vertical" of a QMenu: the vertical menu of Material 3 Expressive, whose submenus open in
        its own place. A click on a submenu's row, Right or Return shows the submenu's items instead of the menu's,
        under a row with an arrow back and the submenu's title; that row, Left and Escape go back. Moving over a
        submenu's row does not open it. Other menus open their submenus beside them.
    \li \c materialTrailingIcon of a QAction in a QMenu: a QIcon that is drawn at the trailing end of its row, 24 by
        24, in \c OnSurfaceVariant if it has one color, before the shortcut and after the arrow of a submenu. Every
        row of the menu makes room for it.
    \li \c materialToolTip \c "rich" of a widget: its tooltip is a rich tooltip, the first line of its text the
        subhead and the rest supporting text. A tooltip whose text is rich text is one without the property.
    \li \c materialSwitch of a QCheckBox: \c true draws it as a switch.
    \li \c materialSliderSize of a QSlider: \c "extraSmall", \c "small", \c "medium", \c "large" or
        \c "extraLarge", the five sizes of Material 3 Expressive, with tracks 16, 24, 40, 56 and 96 thick and handles
        44, 44, 52, 68 and 108 long, which is also how thick the slider is and how far across a press takes hold of the
        handle. Without it a slider is extra small.
    \li \c materialSliderValue of a QSlider: \c true shows the value indicator, a small container with the value
        in it, over the handle (at its left on a vertical slider) while the handle is pressed or the keys change it;
        \c false hides it. Without the property a slider with tick marks shows it, as Material's discrete sliders
        do, and others do not. It is drawn over what is above the slider (at the left of a vertical one), in an
        overlay that is a child of the slider's window, so the slider keeps no room for it; give the slider that
        room in its layout if nothing else is there. While motion is enabled it grows from the handle as it comes
        and shrinks back into it as it goes.
    \li \c materialSliderValueRoom of a QSlider: \c true draws the value indicator inside the slider instead of in
        an overlay, which makes the slider thicker by its room (48 over a horizontal one, the width of its widest
        value at the left of a vertical one). A slider that is a window of its own always does so.
    \li \c materialSliderIcon of a QSlider of the medium, large or extra large size: a QIcon drawn inside the
        active track, 8 from its outer end, 24 by 24 (32 on the extra large size), in \c OnPrimary if it has one
        color. Where the active part is too short for it, it is in the inactive track beside the handle, in
        \c OnSecondaryContainer. Smaller sliders, and centered ones, show no icon.
    \li \c materialSlider \c "centered" of a QSlider: a centered slider, whose active track runs from the middle of
        the track (the middle of its range) to the handle, with a stop at each end.
    \li \c materialDatePicker \c "modal" of a QCalendarWidget makes it Material's modal date picker: above the month
        it has a headline, "Select date" and the chosen date (as "Mon, Aug 17"), over a divider, 121 high in all. It
        is for a calendar in a dialog of its own; a calendar without it, such as the pop-up of a QDateEdit, is the
        docked date picker, which has no headline. The pencil at the end of the headline switches to the input mode:
        an outlined text field "Date" in the place of the month, where a date is typed in the short format of the
        locale, and a calendar icon in the place of the pencil to go back to the days.
    \li \c materialButtonGroup of a widget: \c "connected" makes the buttons that are its children a connected
        button group, which Material 3 Expressive has in the place of segmented buttons. A horizontal layout of the
        widget puts them 2 apart unless it is told a spacing; the ends of the row are round, the corners between the
        buttons small, and the checked button is a pill. For a choice of one, make the buttons checkable and
        auto-exclusive.
    \endlist

    The buttons of a QDialogButtonBox, and so of a QMessageBox, are in the order of Android: what confirms is last.

    Where dialogs have no title bar, as on Android, a QDialog that has a window title, a QMessageBox among them, shows
    it as Material's headline above what is in it, on as many lines as it takes. A sheet has none. A dialog with a
    window icon of its own (setWindowIcon() on the dialog, not the application's) has the centered form: the icon, 24
    and in the \c Secondary color if it is of one color, over the headline, which is centered. A QMessageBox with an
    icon has it there too, as a symbol of one color, rather than beside its text.

    Tooltips, as QToolTip shows them, are Material's plain tooltips without anything being asked for. A tooltip whose
    text is rich text, as Qt::mightBeRichText() tells, is a rich tooltip: a container of 12 corners in
    \c SurfaceContainer at elevation level 2, at most 320 wide, with 16 at its sides and 12 above and below. If the
    text begins with a bold run or a heading, that is its subhead, in title small, over the rest, the supporting text
    in body medium, both in \c OnSurfaceVariant and wrapped:

    \code
    button->setToolTip("<b>Grid view</b><br>Shows the files as a grid of pictures, which is easier to scan.");
    \endcode

    The rest of the markup is left out. A program that is still to build with Qt can use this, since Qt's tooltips
    show rich text; the property \c materialToolTip described above does the same for plain text. Rich tooltips have
    no action buttons. So is
    QCalendarWidget drawn as Material's date picker: days of 40, the chosen one in a filled circle and today in a ring,
    with icon buttons for the months. Material's has no week numbers, which
    \l{QCalendarWidget::setVerticalHeaderFormat()}{NoVerticalHeader} turns off and makes room for on a narrow screen.
    A paintCell() of your own that calls the calendar's keeps the look. The month and the year between the arrows have
    the arrow of a menu: a click on them shows, in the place of the days, the years from that of
    \l{QCalendarWidget::minimumDate()}{minimumDate()} to that of \l{QCalendarWidget::maximumDate()}{maximumDate()},
    three to a row, the year shown in a filled pill and this year in a ring, with a state layer on the pill under
    the pointer (0.08) and on the one pressed (0.10). The days and the years cross-fade on the default effects spring
    while motion is enabled. The wheel scrolls them, and a click on one
    shows the days of that year, in the same month or, past a limit of the range, the nearest month in it; the
    selected date stays. The arrow keys, Page Up, Page Down, Home and End move among the years, Return, Enter or Space
    chooses one, and Escape or a second click on the month and the year goes back to the days.

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
    Qt Widgets has no Material style. Push buttons, line edits, check boxes, radio buttons, switches, sliders, progress bars (a thin line, with the text beside it), tab bars, combo boxes
    (the outlined exposed dropdown menu), menus, lists and dialogs are drawn in the Material way so far; the other
    widgets are drawn as the Fusion style draws them, in the Material colors. Material has no tree and no table:
    those show their selection in the Material colors, and the rows of a tree are 48 high. The rows of a QListView
    or QListWidget are 56 high, or 72 and 88 for two and three lines; whether a row has three lines is told by a
    second line break, not by the width of its text, and a row of two lines shortens its supporting text to one line.
    Supporting text does not widen a list. List items have no overline, avatars, images, videos, trailing icons or
    check boxes at the trailing end, and no segmented or expressive (gapped) look. Behind a QProxyStyle they are drawn as Fusion draws them. A finger that is dragged
    on a view selects, unless QScroller::grabGesture() makes it scroll. Menus
    are the standard menus of Material 3 Expressive in shape (corners of 16, 4 of padding above and below the rows,
    which are inset 4 from the sides and have shapes of their own, a checked one filled in the tertiary container
    color), with an outline in the place of their shadow. A section, added with QMenu::addSection(), is a group label: its
    text in label medium and \c OnSurfaceVariant, 32 high, with no line, after its icon, 20 by 20, if it has one. A line
    break in the text of an action puts what follows it under the label as supporting text in body medium, a line of 20
    each, with 8 above and below the text (60 high with one line), which wraps between words so that the menu is no
    wider than 280 for it; the shortcut stays at the trailing end. A checked row keeps its check mark. Dialogs get the Material container color and text buttons; their rounded corners and the
    dimming behind them, which fades in while motion is enabled, are the android platform's.
    Split buttons and connected button groups are of the small size, and FABs of the standard one. The items of a FAB menu open one after another,
    and go one after another as a picture of them when it closes; they have no check marks, and a row can be chosen anywhere across the menu, left of its pill too.
    A connected button group is of the small size and lies across; Material's standard button group, whose pressed
    button widens, is not drawn differently from buttons in a row. Text fields are those of QLineEdit, and
    take icons, a prefix, a suffix and a counter only as properties and actions (a search bar has its magnifier, and
    its search view is only the docked one, the popup of a
    QCompleter, which does not emphasize the part of a row that matches the text, has no header of its own and opens
    without motion); their caret is the color of the text. A snackbar fades in and out but does not
    grow as it comes, and is one only in a QMainWindow; showing a message does not queue it. There are no
    rich tooltips. Chips are push buttons only; there is no separate click on the cross, an avatar is the icon cut to
    a circle, and a filter chip takes its new width at once. The shadow of an elevated chip or card moves at once
    from level 1 to level 2 rather than with motion. A card that can be clicked is a push button only, and a press
    on a child that takes clicks of its own does not press the card. A date picker has no range, no field to type a date in and no
    button to switch to one, no menu of months, and no buttons to confirm or cancel (those are the dialog's); it keeps
    the days of the other months, faint, and there is no time picker with a clock dial. The headline is in English and
    shows the selected date also when the user cannot select one. A year is chosen when the button is released over
    it. A dialog's headline is made when the dialog is shown, wraps
    but follows a new window title only when the dialog is shown or resized again, and is not drawn by a subclass whose paintEvent() does not call QDialog's. A text editor has no floating label or
    supporting text, and one with another frame shape keeps it. A spin box has
    no floating label or supporting text, and its buttons do not repeat while
    held; a scroll bar does not hide when the contents are still. A time input is for a format of times alone, does not go on to the minute by itself
    when the hour has been typed, and names its boxes in English. A card is not a button and shows no press. A card or
    a divider behind a QProxyStyle is drawn as a plain frame. An app bar has no subtitle, and does not scroll off the screen with the content. A
    medium or large one collapses by the offset of the scroll area under it, which also moves its content, so the
    content goes up faster than the finger while the bar collapses; the bar expands again as the content comes back
    to the top, as with exitUntilCollapsed, not as soon as it is scrolled up (enterAlways). Its titles fade rather
    than move, and a long title is shortened, not put on two lines. Its color, too, follows a scroll area under it,
    not a scrolling widget of another kind. A navigation bar has no horizontal items for wide screens. A
    navigation rail, collapsed or expanded, has no menu button or floating action button of its own, changes its width
    at once rather than over an animation, is never the modal one over the content, and does not scroll if its
    destinations do not fit; the navigation drawer is the modal one only. A sheet is no taller than leaves 72 above it, unless what is in it cannot be made shorter: put that in
    a QScrollArea. A sheet that leaves is a picture of itself while it slides out, and
    shows no change made to it after it was hidden. Dialogs that are not sheets leave at once, their scrim with them.
    There are no standard (not modal) sheets. A click beside a sheet dismisses it only if the sheet is modal (shown with
    exec() or setModal()), and a sheet stays where it was put if its parent's window moves or changes size. A sheet
    has no state that is partly open, and no key or accessibility action moves it. Badges are on icon buttons,
    toolbar buttons and the destinations of a navigation bar or rail only. A snackbar finds where a floating action
    button is when it is placed, and again when one is shown or hidden, not when one is moved. The value
    indicator of a slider is not shown for a slider whose value is changed by the program, and it is clipped to the
    slider's window. A slider's ends beside the handle are round on the extra small size. There are no range sliders,
    as Qt Widgets has no range slider. A busy progress bar moves while motion is enabled, which the other styles of Nt
    do not do. The shapes of the loading indicator are Nt's own, not Material's, and it shows only that something is
    going on, not how far it is. A wavy bar lies across; a vertical one is straight. While its wave travels it is painted at every frame. A wavy circular indicator's wave turns as a whole, at a steady
    speed, and is painted at every frame while it does. A scroll bar whose handle has faded out still takes its room
    and its clicks, and a scroll area shows its scroll bars only when the pointer is on them, not when it is on the
    content. A floating toolbar does not hide as the content is scrolled, and has no menu for
    what does not fit. A docked toolbar does not hide as the content is scrolled either, has no menu for what does not
    fit and no floating action button of its own, and shows no separators. Icons keep their own colors. Buttons, check boxes, radio buttons, switches, the rows of
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

// A sheet that is being dragged: where the press was, where the sheet rests, how far from there it was at the press and
// is now, and how far it was at moments of the drag, for the speed at which it is let go.
struct QMaterialStyle::SheetDrag {
    QPointer<QDialog> dialog;
    QPointF start;
    QRect rest;
    int from = 0;
    int offset = 0;
    bool moving = false;
    QElapsedTimer clock;
    std::vector<std::pair<qint64, int>> samples;
};

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
    if (!m_motion ||
        !(qobject_cast<const QAbstractButton *>(widget) || qobject_cast<const QSlider *>(widget) ||
          qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTabBar *>(widget) || isTextEditor(widget)))
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
                                 const QBrush &background, const QWidget *view, bool first, bool last) const {
    const auto *list = qobject_cast<const QAbstractItemView *>(view);
    const QWidget *viewport = list ? list->viewport() : nullptr;
    const bool pressedHere = viewport && viewport == m_rippleWidget.data() && row.contains(m_rippleOrigin.toPoint());
    const bool held = pressedHere && m_rowPressed;
    if (const CompleterPopup kind = completerPopupOf(view); kind != CompleterPopup::None) {
        // The current row of a completer's popup has a layer, as that of a menu, inside the corners of the container
        // and off the divider of a search view.
        QPainterPath inside = completerShape(view, kind);
        if (kind == CompleterPopup::SearchView) {
            QPainterPath divider;
            divider.addRect(searchViewDivider(view));
            inside = inside.subtracted(divider);
        }
        QPainterPath outline;
        outline.addRect(QRectF(row));
        outline = outline.intersected(inside.translated(-QPointF(viewport->pos())));
        const QColor onSurface = color(Role::OnSurface);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (background.style() != Qt::NoBrush)
            painter->fillPath(outline, background);
        if (selected && enabled)
            painter->fillPath(outline, QBrush(withOpacity(onSurface, hoverOpacity)));
        if (held && !m_motion)
            painter->fillPath(outline, QBrush(withOpacity(onSurface, pressedOpacity)));
        if (pressedHere)
            drawRipple(painter, viewport, outline, QRectF(row), m_rippleOrigin, held, onSurface);
        painter->restore();
        return;
    }
    const QColor ink = color(selected ? Role::OnSecondaryContainer : Role::OnSurface);
    if (isSegmentedList(view)) {
        // Each row a container of its own, 2 from the next: round at the ends of the list, a little round between
        // rows, and round all over while it is chosen or held.
        const QRectF shape = QRectF(row).adjusted(0, 1, 0, -1);
        const bool round = selected || held;
        const QPainterPath outline = endedPath(shape, round || first ? listSegmentOuter : listSegmentInner,
                                               round || last ? listSegmentOuter : listSegmentInner);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (selected && enabled)
            painter->fillPath(outline, QBrush(color(Role::SecondaryContainer)));
        else
            painter->fillPath(outline,
                              background.style() != Qt::NoBrush ? background : QBrush(color(Role::SurfaceContainer)));
        if (selected && !enabled)
            painter->fillPath(outline, QBrush(withOpacity(color(Role::OnSurface), disabledContainerOpacity)));
        if (held && !m_motion)
            painter->fillPath(outline, QBrush(withOpacity(ink, pressedOpacity)));
        if (pressedHere)
            drawRipple(painter, viewport, outline, shape, m_rippleOrigin, held, ink);
        painter->restore();
        return;
    }
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
    if (qobject_cast<QAbstractButton *>(widget) || qobject_cast<QScrollBar *>(widget))
        widget->setAttribute(Qt::WA_Hover);
    // A scroll bar's handle fades out at rest.
    if (auto *bar = qobject_cast<QScrollBar *>(widget);
        bar && !bar->findChild<QObject *>(QLatin1String(scrollRestWatcher), Qt::FindDirectChildrenOnly))
        new ScrollBarRest(bar);
    // A slider draws its value indicator in an overlay.
    if (auto *slider = qobject_cast<QSlider *>(widget);
        slider && !slider->findChild<QObject *>(QLatin1String(sliderValueWatcher), Qt::FindDirectChildrenOnly))
        new SliderValueWatcher(slider);
    // and fades in while a mouse moves over the content of its scroll area.
    if (auto *area = qobject_cast<QAbstractScrollArea *>(widget);
        area && !area->findChild<QObject *>(QLatin1String(scrollHoverWatcher), Qt::FindDirectChildrenOnly))
        new ScrollAreaHover(area);
    if (qobject_cast<QDialog *>(widget) && !widget->testAttribute(Qt::WA_SetPalette)) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Window,
                         color(sheetKind(widget).isEmpty() ? Role::SurfaceContainerHigh : Role::SurfaceContainerLow));
        widget->setPalette(palette);
        widget->setProperty(polishedPalette, true);
    }
    // A top app bar changes its color while the content under it is scrolled: what scrolls repaints it.
    if (auto *area = qobject_cast<QAbstractScrollArea *>(widget);
        area && area->verticalScrollBar() &&
        !area->findChild<QObject *>(QLatin1String(appBarWatcher), Qt::FindDirectChildrenOnly)) {
        auto *watcher = new QObject(area); // the connection goes when unpolish() deletes it
        watcher->setObjectName(QLatin1String(appBarWatcher));
        QObject::connect(area->verticalScrollBar(), &QScrollBar::valueChanged, watcher,
                         [area] { updateAppBars(area, true); });
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
    // The popup of a completer: the list lies on the container that its frame draws, a menu or a search view.
    if (const CompleterPopup kind = completerPopupOf(widget); kind != CompleterPopup::None) {
        auto *list = static_cast<QListWidget *>(widget);
        if (!widget->testAttribute(Qt::WA_SetPalette) || widget->property(polishedPalette).toBool()) {
            QPalette palette = widget->palette();
            palette.setColor(QPalette::Base, Qt::transparent);
            palette.setColor(QPalette::Text, color(Role::OnSurface));
            palette.setColor(QPalette::HighlightedText, color(Role::OnSurface)); // the current row has a layer only
            widget->setPalette(palette);
            widget->setProperty(polishedPalette, true);
        }
        if (list->frameShape() != QFrame::NoFrame) {
            widget->setProperty(polishedFrame, list->frameStyle());
            list->setFrameShape(QFrame::NoFrame);
        }
        if (!list->iconSize().isValid()) {
            list->setIconSize(QSize(menuIconSize, menuIconSize));
            widget->setProperty(polishedIconSize, true);
        }
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
                      qobject_cast<QMenu *>(widget) || qobject_cast<QCalendarWidget *>(widget) ||
                      qobject_cast<QAbstractSpinBox *>(widget) || isTextEditor(widget) || isList(widget) ||
                      qobject_cast<QTreeView *>(widget);
    if ((choice || button || text) && !widget->testAttribute(Qt::WA_SetFont)) {
        QFont font = widget->font();
        font.setPixelSize(button ? 14 : 16);
        if (button)
            font.setWeight(QFont::Medium);
        if (isNavigationBar(widget) || (isNavigationRail(widget) && !isExpandedRail(widget)))
            font.setPixelSize(navigationLabelPixels);
        if (isExpandedRail(widget)) // label large
            font.setPixelSize(railExpandedLabelPixels);
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
    if (isTextEditor(widget))
        polishEditorField(static_cast<QAbstractScrollArea *>(widget));
}

// A multi-line text field: a filled one's container shows through its viewport, its frame repaints as its text
// comes and goes (the label floats), and one that grows with its text takes the height that its text asks for.
void QMaterialStyle::polishEditorField(QAbstractScrollArea *editor) const {
    if (!isEditorField(editor))
        return;
    if (textFieldOf(editor).filled &&
        (!editor->testAttribute(Qt::WA_SetPalette) || editor->property(polishedPalette).toBool())) {
        QPalette palette = editor->palette();
        palette.setColor(QPalette::Base, color(Role::SurfaceContainerHighest));
        palette.setColor(QPalette::Disabled, QPalette::Base, blend(color(Role::Surface), color(Role::OnSurface), 0.04));
        editor->setPalette(palette);
        editor->setProperty(polishedPalette, true);
    }
    if (!editor->findChild<QObject *>(QLatin1String(fieldWatcher), Qt::FindDirectChildrenOnly)) {
        auto *watcher = new QObject(editor); // the connection goes when unpolish() deletes it
        watcher->setObjectName(QLatin1String(fieldWatcher));
        QObject::connect(static_cast<QPlainTextEdit *>(editor), &QPlainTextEdit::textChanged, watcher,
                         [editor] { editor->update(); });
    }
    if (growsWithContent(editor) && !editor->property(polishedPolicy).isValid()) {
        QSizePolicy policy = editor->sizePolicy();
        editor->setProperty(polishedPolicy, static_cast<int>(policy.verticalPolicy()));
        policy.setVerticalPolicy(QSizePolicy::Maximum);
        editor->setSizePolicy(policy);
    }
    layOutEditorField(editor);
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
    removeHeadline(widget);
    removeRoom(widget, handleRoom);
    if (auto *view = qobject_cast<QAbstractItemView *>(widget); view && widget->property(polishedIconSize).toBool()) {
        view->setIconSize(QSize());
        widget->setProperty(polishedIconSize, QVariant());
    }
    if (qobject_cast<QAbstractScrollArea *>(widget)) {
        delete widget->findChild<QObject *>(QLatin1String(appBarWatcher), Qt::FindDirectChildrenOnly);
        delete widget->findChild<QObject *>(QLatin1String(scrollHoverWatcher), Qt::FindDirectChildrenOnly);
    }
    if (qobject_cast<QSlider *>(widget))
        delete widget->findChild<QObject *>(QLatin1String(sliderValueWatcher), Qt::FindDirectChildrenOnly);
    if (qobject_cast<QScrollBar *>(widget)) {
        delete widget->findChild<QObject *>(QLatin1String(scrollRestWatcher), Qt::FindDirectChildrenOnly);
        widget->setProperty(scrollAwake, QVariant());
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
    if (auto *editor = qobject_cast<QAbstractScrollArea *>(widget); editor && isTextEditor(widget)) {
        delete widget->findChild<QObject *>(QLatin1String(fieldWatcher), Qt::FindDirectChildrenOnly);
        if (const QVariant policy = widget->property(polishedPolicy); policy.isValid()) {
            QSizePolicy restored = widget->sizePolicy();
            restored.setVerticalPolicy(static_cast<QSizePolicy::Policy>(policy.toInt()));
            widget->setSizePolicy(restored);
            widget->setProperty(polishedPolicy, QVariant());
        }
        if (widget->property(polishedMargins).toBool()) {
            editor->setViewportMargins(QMargins());
            widget->setProperty(polishedMargins, QVariant());
        }
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

namespace {

// Whether the search bar `edit` opens the full-screen search view when it gets the focus: by its materialSearchView
// ("fullScreen" or "docked"), or else on a compact window, narrower than 600. Only a bar with a completer opens a view.
bool opensFullScreenSearch(const QWidget *edit) {
    const auto *line = qobject_cast<const QLineEdit *>(edit);
    if (!line || !line->completer() || !textFieldOf(line).search)
        return false;
    const QString mode = line->property("materialSearchView").toString();
    if (mode == QLatin1String("fullScreen"))
        return true;
    if (mode == QLatin1String("docked"))
        return false;
    return line->window()->width() < navigationMediumWidth;
}

// The full-screen search view of a search bar: over the whole window, SurfaceContainerHigh, with a header of its own
// (an arrow back, a line edit and a divider under them) and the completer's popup under it. The search bar keeps its
// place under the view, and its completer completes the header's line edit while the view is open; what is typed
// there is the bar's text, and the bar emits textEdited(), returnPressed() and editingFinished() for it. The view grows
// from the bar's rectangle to the window's.
class SearchView : public QWidget {
public:
    // What the view does with the focus as it closes.
    enum class Focus {
        Clear, // nothing has the focus (the arrow back, Escape, a choice)
        ToBar, // the bar gets it back (the window became too wide for the view)
        Leave  // it went to another widget already
    };

    SearchView(const QMaterialStyle *style, QLineEdit *edit) : QWidget(edit->window()), m_style(style), m_edit(edit) {
        setObjectName(QLatin1String(searchViewName));
        setFocusPolicy(Qt::NoFocus);
        m_field = new QLineEdit(this);
        m_field->setObjectName(QLatin1String(searchFieldName));
        m_field->setFrame(false);
        QPalette palette = m_field->palette();
        palette.setColor(QPalette::Base, Qt::transparent);
        palette.setColor(QPalette::Text, style->color(QMaterialStyle::Role::OnSurface));
        palette.setColor(QPalette::PlaceholderText, style->color(QMaterialStyle::Role::OnSurfaceVariant));
        m_field->setPalette(palette);
        m_field->setPlaceholderText(edit->placeholderText());
        m_field->setMaxLength(edit->maxLength());
        m_field->setText(edit->text());
        m_field->setCursorPosition(edit->cursorPosition());
        m_field->setClearButtonEnabled(true);
        QObject::connect(m_field, &QLineEdit::textChanged, this, [this](const QString &text) {
            if (m_edit && m_edit->text() != text)
                m_edit->setText(text);
        });
        QObject::connect(m_field, &QLineEdit::textEdited, this, [this](const QString &text) {
            if (m_edit)
                emit m_edit->textEdited(text);
        });
        QObject::connect(m_field, &QLineEdit::returnPressed, this, [this] {
            const QPointer<QLineEdit> bar = m_edit;
            close(Focus::Clear);
            if (bar) {
                emit bar->returnPressed();
                emit bar->editingFinished();
            }
        });
        m_completer = edit->completer();
        m_field->setCompleter(m_completer);
        if (m_completer)
            QObject::connect(m_completer.data(), &QCompleter::activated, this, [this] { close(Focus::Clear); });
        parentWidget()->installEventFilter(this);
    }

    // The view of the window of `widget`, if one is open.
    static SearchView *of(const QWidget *widget) {
        const QWidget *window = widget->window();
        for (QObject *child : window->children()) {
            if (auto *view = dynamic_cast<SearchView *>(child); view && !view->m_closing)
                return view;
        }
        return nullptr;
    }

    QLineEdit *field() const { return m_field; }

    // Shows the view and gives its line edit the focus. With motion it grows from the bar's rectangle on the default
    // spatial spring, as round as the bar at the start and square at the end.
    void open(bool motion) {
        const QRect from(m_edit->mapTo(parentWidget(), QPoint(0, 0)), m_edit->size());
        m_barCorner = from.height() / 2.0;
        if (motion) {
            growTo(from, 0);
            QPointer<SearchView> guard(this);
            new QtPrivate::Transition(this, QStringLiteral("_nt_searchViewGrow"), QtPrivate::defaultSpatialSpring,
                                      [guard, from](double progress) {
                                          if (!guard)
                                              return false;
                                          guard->growTo(from, progress);
                                          return true;
                                      });
        } else {
            growTo(from, 1);
        }
        raise();
        show();
        m_field->setFocus();
        QObject::connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
            if (now && now != m_field && !isAncestorOf(now))
                close(Focus::Leave);
        });
    }

    // Closes the view at once: the completer goes back to the bar, and the view is deleted soon.
    void close(Focus focus) {
        if (m_closing)
            return;
        m_closing = true;
        QtPrivate::Transition::stop(this, QStringLiteral("_nt_searchViewGrow"));
        m_field->setCompleter(nullptr);
        if (m_completer && m_edit && m_edit->completer() == m_completer.data())
            m_completer->setWidget(m_edit);
        m_field->clearFocus();
        hide();
        if (m_edit) {
            if (focus == Focus::ToBar)
                m_edit->setFocus();
            else if (focus == Focus::Clear)
                m_edit->clearFocus();
            m_edit->update();
        }
        deleteLater();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double corner = m_barCorner * (1.0 - std::clamp(m_progress, 0.0, 1.0));
        QPainterPath shape;
        shape.addRoundedRect(QRectF(rect()), corner, corner);
        painter.fillPath(shape, QBrush(m_style->color(QMaterialStyle::Role::SurfaceContainerHigh)));
        // The arrow back, 24 in its target, and the layer of a press on the target.
        const QRect target = backTarget();
        const QColor ink = m_style->color(QMaterialStyle::Role::OnSurface);
        if (m_backPressed) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(withOpacity(ink, pressedOpacity));
            painter.drawEllipse(QRectF(target).adjusted(4, 4, -4, -4));
        }
        const QPointF c = QRectF(target).center();
        QPainterPath arrow;
        arrow.moveTo(c + QPointF(7, 0));
        arrow.lineTo(c + QPointF(-7, 0));
        arrow.moveTo(c + QPointF(-1, -6));
        arrow.lineTo(c + QPointF(-7, 0));
        arrow.lineTo(c + QPointF(-1, 6));
        painter.setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(arrow);
        // The divider under the header, once the view is taller than it.
        if (height() > searchHeaderHeight)
            painter.fillRect(QRect(0, searchHeaderHeight, width(), 1),
                             m_style->color(QMaterialStyle::Role::OutlineVariant));
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        const int header = headerHeight();
        m_field->setGeometry(searchHeaderFieldLeft, (header - searchHeaderFieldHeight) / 2,
                             std::max(0, width() - searchHeaderFieldLeft - searchBackInset), searchHeaderFieldHeight);
    }

    void mousePressEvent(QMouseEvent *event) override {
        m_backPressed = event->button() == Qt::LeftButton && backTarget().contains(event->position().toPoint());
        update();
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        const bool back = m_backPressed && backTarget().contains(event->position().toPoint());
        m_backPressed = false;
        update();
        event->accept();
        if (back)
            close(Focus::Clear);
    }

    // The view covers the window as it changes size; on a window that grew too wide for it, the docked view takes
    // its place.
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == parentWidget() && event->type() == QEvent::Resize && !m_closing) {
            if (m_edit && !opensFullScreenSearch(m_edit))
                close(Focus::ToBar);
            else if (m_progress >= 1.0)
                setGeometry(parentWidget()->rect());
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    // The header is 72, or the whole view while it is lower, as it is when it starts to grow from the bar.
    int headerHeight() const { return std::min(searchHeaderHeight, height()); }

    // The target of the arrow back, in the view.
    QRect backTarget() const {
        return QRect(searchBackInset, (headerHeight() - searchBackTarget) / 2, searchBackTarget, searchBackTarget);
    }

    // Puts the view `progress` of the way from the bar's rectangle `from` to the window's.
    void growTo(const QRect &from, double progress) {
        m_progress = progress;
        const QRect to = parentWidget()->rect();
        const auto mix = [progress](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * progress)); };
        setGeometry(QRect(QPoint(mix(from.left(), to.left()), mix(from.top(), to.top())),
                          QPoint(mix(from.right(), to.right()), mix(from.bottom(), to.bottom()))));
        update();
    }

    const QMaterialStyle *m_style;
    QPointer<QLineEdit> m_edit;
    QPointer<QCompleter> m_completer;
    QLineEdit *m_field = nullptr;
    double m_progress = 1;   // how far the view has grown from the bar
    double m_barCorner = 28; // the corners of the bar that it grows from
    bool m_backPressed = false;
    bool m_closing = false;
};

} // namespace

// Opens the full-screen search view of the search bar `edit`, which just got the focus.
void QMaterialStyle::openFullScreenSearch(QLineEdit *edit) const {
    if (SearchView::of(edit))
        return;
    auto *view = new SearchView(this, edit);
    view->open(m_motion);
}

QRect QMaterialStyle::completerPopupGeometry(const QWidget *popup, const QWidget *widget, const QRect &proposed,
                                             int content) {
    if (!popup || !popup->parentWidget() || !isFullScreenSearchField(widget))
        return proposed;
    const QWidget *top = popup->parentWidget();
    const int y = searchHeaderHeight + 1;
    return QRect(0, y, top->width(), std::clamp(content, 0, std::max(0, top->height() - y)));
}

bool QMaterialStyle::drawCompletionText(QPainter *painter, const QRect &area, const QString &text, const QFont &font,
                                        bool enabled, const QWidget *view) const {
    if (completerPopupOf(view) == CompleterPopup::None)
        return false;
    const QCompleter *completer = completerEdit(view)->completer();
    const QString typed = completer->completionPrefix();
    if (typed.isEmpty())
        return false;
    qsizetype at = -1;
    switch (static_cast<int>(completer->filterMode() & Qt::MatchTypeMask)) {
    case Qt::MatchContains:
        at = text.indexOf(typed, 0, Qt::CaseInsensitive);
        break;
    case Qt::MatchEndsWith:
        at = text.endsWith(typed, Qt::CaseInsensitive) ? text.size() - typed.size() : -1;
        break;
    default:
        at = text.startsWith(typed, Qt::CaseInsensitive) ? 0 : -1;
        break;
    }
    if (at < 0)
        return false;
    QFont emphasized = font;
    emphasized.setWeight(QFont::Medium);
    const QColor disabled = withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const QColor rest = enabled ? color(Role::OnSurfaceVariant) : disabled;
    const QColor match = enabled ? color(Role::OnSurface) : disabled;
    struct Run {
        QString text;
        const QFont *font;
        QColor color;
    };
    const Run runs[] = {{text.left(at), &font, rest},
                        {text.mid(at, typed.size()), &emphasized, match},
                        {text.mid(at + typed.size()), &font, rest}};
    painter->save();
    int x = area.left();
    for (const Run &run : runs) {
        if (run.text.isEmpty())
            continue;
        const int room = area.right() + 1 - x;
        if (room <= 0)
            break;
        // What does not fit ends in an ellipsis.
        const QFontMetrics metrics(*run.font);
        const QString shown =
            metrics.horizontalAdvance(run.text) > room ? metrics.elidedText(run.text, Qt::ElideRight, room) : run.text;
        painter->setFont(*run.font);
        painter->setPen(run.color);
        painter->drawText(QRect(x, area.top(), room, area.height()),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, shown);
        x += metrics.horizontalAdvance(shown);
    }
    painter->restore();
    return true;
}

/*!
    \reimp

    Notes whether the user is moving the focus with the keyboard, or points with a mouse or a finger: the focus ring
    shows only in the first case.
*/
bool QMaterialStyle::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Show) {
        // The android platform rounds the corners of a dialog's window by this property.
        if (auto *dialog = qobject_cast<QDialog *>(watched); dialog && dialog->isWindow() && dialog->windowHandle()) {
            QWindow *window = dialog->windowHandle();
            window->setProperty("_nt_cornerRadius",
                                sheetKind(dialog) == QLatin1String("drawer") ? drawerCorner : dialogCorner);
            // The scrim behind it fades in, from where it was if a sheet came back as it went.
            const QVariant was = window->property(scrimProperty);
            if (m_motion)
                fadeScrim(dialog, was.isValid() ? std::clamp(was.toDouble(), 0.0, scrimOpacity) : 0.0);
            else
                window->setProperty(scrimProperty, scrimOpacity);
        }
    }
    if (event->type() == QEvent::Hide) {
        // A sheet that is hidden while it is dragged is no longer dragged.
        if (m_sheetDrag && watched == m_sheetDrag->dialog.data())
            m_sheetDrag.reset();
        // The scrim goes with the dialog, and the next time it fades in from nothing.
        if (auto *dialog = qobject_cast<QDialog *>(watched); m_motion && dialog && dialog->isWindow()) {
            QtPrivate::Transition::stop(dialog, QStringLiteral("_nt_scrimFade"));
            if (QWindow *window = dialog->windowHandle())
                window->setProperty(scrimProperty, 0.0);
        }
        // The items of a FAB menu go one after another.
        if (auto *menu = qobject_cast<QMenu *>(watched); menu && isFabMenu(menu))
            closeFabMenu(menu);
    }
    // What is hidden is no longer pressed: a menu that closed with a choice must not show it when it opens again.
    if (event->type() == QEvent::Hide && m_rippleWidget && watched == m_rippleWidget.data()) {
        m_rippleWidget = nullptr;
        m_rowPressed = false;
    }
    // A scroll area that comes or goes, as with the page of a tab widget that it is on, may change the color of an app
    // bar.
    if ((event->type() == QEvent::Show || event->type() == QEvent::Hide) &&
        qobject_cast<QAbstractScrollArea *>(watched))
        updateAppBars(static_cast<QWidget *>(watched), false);
    // A search bar that gets the focus opens the full-screen search view, unless the focus only came back with its
    // window: the window sends WindowActivate just before it gives its focus widget the focus again.
    if (event->type() == QEvent::WindowActivate) {
        auto *window = qobject_cast<QWidget *>(watched);
        m_activatedWindow = window && window->focusWidget() ? window : nullptr;
    } else if (event->type() == QEvent::FocusIn) {
        auto *edit = qobject_cast<QLineEdit *>(watched);
        const bool returned = edit && m_activatedWindow && m_activatedWindow.data() == edit->window();
        m_activatedWindow = nullptr;
        if (edit && !returned && edit->style() == this && opensFullScreenSearch(edit))
            openFullScreenSearch(edit);
    } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress ||
               event->type() == QEvent::TouchBegin) {
        m_activatedWindow = nullptr;
        // A tap on a bar that has the focus already (its window gave it the focus as it opened) opens the view too.
        auto *edit = qobject_cast<QLineEdit *>(watched);
        if (event->type() == QEvent::MouseButtonPress && edit && edit->hasFocus() && edit->style() == this &&
            static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton && opensFullScreenSearch(edit))
            openFullScreenSearch(edit);
    }
    // In the full-screen search view, Escape (or Back) closes the completer's popup first, and then the view.
    if (event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        auto *field = qobject_cast<QLineEdit *>(watched);
        if ((key == Qt::Key_Escape || key == Qt::Key_Back) && field && isFullScreenSearchField(field) &&
            !(field->completer() && field->completer()->popup()->isVisible())) {
            if (SearchView *view = SearchView::of(field)) {
                view->close(SearchView::Focus::Clear);
                return true;
            }
        }
    }
    // A text editor's border shows that it has the focus.
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
        if (auto *widget = qobject_cast<QWidget *>(watched); widget && isTextEditor(widget))
            widget->update();
    }
    // The room around the text of a multi-line field follows the height of its lines.
    if (event->type() == QEvent::FontChange) {
        if (auto *widget = qobject_cast<QWidget *>(watched); widget && isTextEditor(widget) && widget->style() == this)
            layOutEditorField(static_cast<QAbstractScrollArea *>(widget));
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
    if (sheetMouseEvent(watched, event))
        return true;
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
    if (!buttonCardKind(widget).isEmpty())
        return ButtonKind::Card;
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
    case ButtonKind::Card:
        return color(Role::OnSurface);
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
        if (isAppBarKind(toolBarKind(widget->parentWidget())))
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
    double pill = rect.height() / 2;
    if (isIconButton(widget)) // a narrow one is a circle no wider than itself
        pill = std::min({rect.height(), static_cast<double>(iconButtonSize(widget).height),
                         static_cast<double>(iconButtonWidth(widget))}) /
               2;
    double left = pill;
    double right = pill;
    if (isChip(widget)) {
        drawButtonShape(option, painter, widget, chipCorner, chipCorner); // a chip keeps its shape
        return;
    }
    if (!buttonCardKind(widget).isEmpty()) {
        drawButtonShape(option, painter, widget, cardCorner, cardCorner); // and so does a card
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
        else if (isElevatedChip(widget))
            container = color(Role::SurfaceContainerLow);
        break;
    case ButtonKind::Card: {
        const QString card = buttonCardKind(widget);
        container = color(card == QLatin1String("filled")     ? Role::SurfaceContainerHighest
                          : card == QLatin1String("elevated") ? Role::SurfaceContainerLow
                                                              : Role::Surface);
        // A card that cannot be used fades, rather than taking the color of a disabled button.
        if (!enabled(option))
            container = withOpacity(container, disabledContentOpacity);
        break;
    }
    case ButtonKind::Text:
        break;
    }
    const bool elevatedChip = kind == ButtonKind::Chip && isElevatedChip(widget);
    const bool bare = kind == ButtonKind::Outlined || kind == ButtonKind::ToolBarItem ||
                      kind == ButtonKind::IconOutlined || (kind == ButtonKind::Chip && !elevatedChip);
    if (!enabled(option) && kind != ButtonKind::Text && kind != ButtonKind::IconStandard && kind != ButtonKind::Card &&
        !(bare && !checked))
        container = withOpacity(color(Role::OnSurface), disabledContainerOpacity);
    const double lift = kind == ButtonKind::Fab ? fabShadow : kind == ButtonKind::Elevated ? 1.5 : 0.0;
    const bool raised = lift > 0 && enabled(option) && (kind == ButtonKind::Fab || !(option->state & State_Sunken));
    if (raised)
        painter->fillPath(sidedPath(rect.adjusted(0.5, lift, -0.5, 0), left, right),
                          QBrush(withOpacity(QColor(Qt::black), 0.18)));
    // A button of a toolbar, and a small icon button, are drawn smaller than they are to a finger.
    double inset = kind == ButtonKind::ToolBarItem ? toolBarInset : 0;
    double above = inset;
    if (isIconButton(widget)) {
        // A narrow or a wide one has other room at its sides than above and below.
        const QSize extent = iconButtonExtent(widget);
        inset = std::max(0, extent.width() - iconButtonWidth(widget)) / 2.0;
        above = std::max(0, extent.height() - iconButtonSize(widget).height) / 2.0;
    }
    // A chip is as wide as it is to a finger, but not as high.
    if (kind == ButtonKind::Chip)
        above = std::max(0.0, (rect.height() - chipHeight) / 2);
    QRectF body = rect.adjusted(inset, above, -inset, -above - lift);
    const QString card = kind == ButtonKind::Card ? buttonCardKind(widget) : QString();
    const bool elevatedCard = card == QLatin1String("elevated");
    // An elevated card keeps room for its shadow below it, as the frame that is one does.
    if (elevatedCard)
        body.adjust(0.5, 0, -0.5, -fabShadow);
    const QPainterPath outline = sidedPath(body, left, right);
    // An elevated chip or card is at level 1, and at level 2 under the pointer, where its shadow falls further: a
    // chip has the room of its target for it, a card that which it keeps below itself.
    if ((elevatedChip || elevatedCard) && enabled(option)) {
        const bool hovered = (option->state & State_MouseOver) && !(option->state & State_Sunken);
        const double level = elevatedChip ? (hovered ? chipRaiseHover : chipRaise)
                                          : (hovered ? static_cast<double>(fabShadow) : cardRaise);
        // It rises and settles on the default effects spring, as Compose's elevation does.
        const double fall = animated(widget, ElevationChannel, level, QtPrivate::defaultEffectsSpring, 0.02);
        painter->fillPath(sidedPath(body.translated(0, fall), left, right),
                          QBrush(withOpacity(QColor(Qt::black), 0.18)));
    }
    // A line inside the shape.
    const auto stroke = [&](const QColor &ink, double width) {
        const double half = width / 2;
        painter->strokePath(sidedPath(body.adjusted(half, half, -half, -half), left - half, right - half),
                            QPen(QBrush(ink), width));
    };
    if (container.alpha() > 0)
        painter->fillPath(outline, QBrush(container));
    if ((kind == ButtonKind::Chip && !checked && !elevatedChip) || card == QLatin1String("outlined"))
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

namespace {
// A picture of a FAB menu that closed, where it was: its items go one after another, the one furthest from the button
// first (the other way round from how they opened), each shrinking toward its right end into a circle as it fades.
class FabMenuGhost : public QWidget {
public:
    FabMenuGhost(QWidget *parent, const QPixmap &picture, std::vector<QRect> pills)
        : QWidget(parent), m_picture(picture), m_pills(std::move(pills)) {
        setObjectName(QStringLiteral("_nt_fabMenuGhost"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        std::sort(m_pills.begin(), m_pills.end(), [](const QRect &a, const QRect &b) { return a.top() < b.top(); });
        m_clock.start();
        m_timer.setInterval(16);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
            if (seconds() > lastGoes()) {
                m_timer.stop();
                hide();
                deleteLater();
                return;
            }
            update();
        });
        m_timer.start();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double now = seconds();
        const QtPrivate::Spring spring(0, 0, 1, QtPrivate::fastEffectsSpring);
        for (std::size_t i = 0; i < m_pills.size(); ++i) {
            const QRect &pill = m_pills[i];
            const double since = now - static_cast<double>(i) * fabMenuCloseStagger;
            const double gone = since > 0 ? std::clamp(spring.at(since).position, 0.0, 1.0) : 0.0;
            const double shown = 1.0 - std::clamp(since / fabMenuCloseFade, 0.0, 1.0);
            if (shown <= 0)
                continue;
            const double height = pill.height();
            const double width = std::max(height, pill.width() - (pill.width() - height) * gone);
            const QRectF left(pill.left() + pill.width() - width, pill.top(), width, height);
            QPainterPath shape;
            shape.addRoundedRect(left, height / 2, height / 2);
            painter.save();
            painter.setOpacity(shown);
            painter.setClipPath(shape);
            painter.drawPixmap(pill, m_picture, pill);
            painter.restore();
        }
    }

private:
    double seconds() const { return static_cast<double>(m_clock.nsecsElapsed()) * 1e-9; }
    // When the last item is gone.
    double lastGoes() const {
        return static_cast<double>(std::max<std::size_t>(m_pills.size(), 1) - 1) * fabMenuCloseStagger +
               fabMenuCloseFade;
    }

    QPixmap m_picture;
    std::vector<QRect> m_pills;
    QElapsedTimer m_clock;
    QTimer m_timer;
};
} // namespace

void QMaterialStyle::closeFabMenu(QMenu *menu) const {
    QWidget *host = menu->parentWidget();
    if (!m_motion || !host || !host->isVisible() || QCoreApplication::closingDown())
        return;
    // The pill of each item, as wide as it is when it is open, at the right of its row.
    std::vector<QRect> pills;
    for (QAction *action : menu->actions()) {
        if (!action->isVisible() || action->isSeparator())
            continue;
        const QRect row = menu->actionGeometry(action);
        if (row.isEmpty())
            continue;
        QStyleOptionMenuItem item;
        item.text = action->text();
        item.icon = action->icon();
        item.font = menu->font();
        const int width = std::min(fabMenuItemWidth(item), row.width());
        pills.emplace_back(row.right() + 1 - width, row.top(), width, std::max(1, row.height() - fabMenuItemSpace));
    }
    if (pills.empty())
        return;
    auto *ghost = new FabMenuGhost(host, menu->grab(), std::move(pills));
    ghost->setGeometry(menu->geometry());
    ghost->show();
    ghost->raise();
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
    // A menu that opens again while it closes takes the place of what is left of it.
    if (QWidget *host = menu->parentWidget()) {
        const QList<QWidget *> leftovers =
            host->findChildren<QWidget *>(QStringLiteral("_nt_fabMenuGhost"), Qt::FindDirectChildrenOnly);
        for (QWidget *leftover : leftovers) {
            leftover->hide();
            leftover->deleteLater();
        }
    }
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

bool QMaterialStyle::isAppBar(const QWidget *widget) { return isAppBarKind(toolBarKind(widget)); }

bool QMaterialStyle::isFlexibleAppBar(const QWidget *widget) { return expandedAppBarHeight(widget) > appBarHeight; }

bool QMaterialStyle::scrollsAway(const QWidget *widget) { return scrollsAwayKind(widget); }

int QMaterialStyle::toolBarRest(const QToolBar *bar, int natural) const {
    // A medium or large one collapses to the small one once the content under it is scrolled, and expands at the top,
    // unless it is pinned. It does not follow the scrolling: the content would move up twice as fast as the finger,
    // scrolled and lifted.
    const int expanded = expandedAppBarHeight(bar);
    const int range = expanded - appBarHeight;
    if (range <= 0) // a small one is 64 high even without buttons
        return isAppBarKind(toolBarKind(bar)) ? std::max(natural, appBarHeight) : natural;
    double collapse = scrolledUnder(bar) && scrollBehavior(bar) != QLatin1String("pinned") ? range : 0;
    if (m_motion) {
        collapse =
            std::clamp(m_animator->value(bar, AppBarCollapseChannel, collapse, QtPrivate::defaultEffectsSpring, 0.25),
                       0.0, static_cast<double>(range));
        m_animator->resizes(bar, AppBarCollapseChannel);
    }
    return expanded - static_cast<int>(std::lround(collapse));
}

double QMaterialStyle::toolBarAway(const QToolBar *bar) const {
    // It snaps away and back, as the collapse does, rather than following the scrolling.
    if (!scrollsAwayKind(bar))
        return 0.0;
    double away = bar->property(barAway).toBool() ? 1.0 : 0.0;
    if (m_motion) {
        away = std::clamp(m_animator->value(bar, ToolBarHideChannel, away, QtPrivate::defaultEffectsSpring, 0.004), 0.0,
                          1.0);
        m_animator->resizes(bar, ToolBarHideChannel);
    }
    return away;
}

int QMaterialStyle::appBarExtent(const QToolBar *bar, int natural) const {
    return static_cast<int>(std::lround(toolBarRest(bar, natural) * (1.0 - toolBarAway(bar))));
}

void QMaterialStyle::drawAppBar(QPainter *painter, const QToolBar *bar) const {
    // The container is Surface at rest and SurfaceContainer while the content under the bar is scrolled. A toolbar is
    // not one of the widgets that animated() moves, so this asks the animator itself.
    double scrolled = scrolledUnder(bar) ? 1.0 : 0.0;
    if (m_motion)
        scrolled = std::clamp(m_animator->value(bar, AppBarChannel, scrolled, QtPrivate::defaultEffectsSpring, 0.004),
                              0.0, 1.0);
    // A bar that is partly scrolled away is drawn whole, moved up by what has gone, as QToolBar moves its buttons.
    const int height = toolBarAway(bar) > 0.0 ? std::max(bar->height(), toolBarRest(bar, appBarHeight)) : bar->height();
    painter->save();
    painter->translate(0, bar->height() - height);
    painter->fillRect(QRect(0, 0, bar->width(), height),
                      blend(color(Role::Surface), color(Role::SurfaceContainer), scrolled));
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
    const QString subtitle = appBarSubtitle(bar);
    const bool centred = centresTitle(bar);
    const Qt::Alignment along = centred ? Qt::AlignHCenter : Qt::AlignLeft;
    // A medium or large one has its title on a line (or two) of its own below the buttons while it is expanded, and
    // its subtitle under that. As it collapses they go up with its bottom edge and fade, and the small title comes in
    // the row once it is past half.
    const int expanded = expandedAppBarHeight(bar);
    const int range = expanded - appBarHeight;
    const double collapsed = range > 0 ? std::clamp(static_cast<double>(expanded - height) / range, 0.0, 1.0) : 1.0;
    if (range > 0 && collapsed < 1.0 && height > appBarHeight) {
        const bool large = toolBarKind(bar) == QLatin1String("appBarLarge");
        const int line = large ? appBarLargeTitleLine : appBarMediumTitleLine;
        const int width = bar->width() - 2 * appBarTitleSpace;
        const QFont font = expandedTitleFont(bar, large);
        const QStringList lines = wrapAppBarTitle(bar->windowTitle(), QFontMetrics(font), width);
        QFont subtitleFont = bar->font();
        subtitleFont.setPixelSize(large ? appBarLargeSubtitlePixels : appBarMediumSubtitlePixels);
        subtitleFont.setWeight(QFont::Medium);
        const int subtitleLine = subtitle.isEmpty() ? 0 : large ? appBarLargeSubtitleLine : appBarMediumSubtitleLine;
        painter->save();
        painter->setClipRect(QRect(0, appBarHeight, bar->width(), height - appBarHeight));
        painter->setOpacity(1.0 - collapsed);
        painter->setFont(font);
        painter->setPen(color(Role::OnSurface));
        int y = height - appBarExpandedTitleBottom - subtitleLine - line * static_cast<int>(lines.size());
        for (const QString &text : lines) {
            painter->drawText(QRect(appBarTitleSpace, y, width, line), along | Qt::AlignVCenter | Qt::TextSingleLine,
                              text);
            y += line;
        }
        if (!subtitle.isEmpty()) {
            painter->setFont(subtitleFont);
            painter->setPen(color(Role::OnSurfaceVariant));
            painter->drawText(QRect(appBarTitleSpace, y, width, subtitleLine),
                              along | Qt::AlignVCenter | Qt::TextSingleLine,
                              QFontMetrics(subtitleFont).elidedText(subtitle, Qt::ElideRight, width));
        }
        painter->restore();
    }
    // A centred title is in the middle of the bar, as wide as the room on its narrower side leaves.
    if (centred) {
        const int middle = bar->width() / 2;
        const int half = std::min(middle - from, to - middle);
        from = middle - half;
        to = middle + half;
    }
    const double small = std::clamp(2.0 * collapsed - 1.0, 0.0, 1.0);
    if (to > from && small > 0.0) {
        // With a subtitle, the title (28 high) and the subtitle (16) are centred together in the row.
        QFont font = bar->font();
        font.setPixelSize(appBarTitlePixels);
        font.setWeight(QFont::Normal);
        const int top = subtitle.isEmpty() ? 0 : (appBarHeight - appBarTitleLine - appBarSubtitleLine) / 2;
        painter->save();
        painter->setOpacity(small);
        painter->setFont(font);
        painter->setPen(color(Role::OnSurface));
        painter->drawText(QRect(from, top, to - from, subtitle.isEmpty() ? appBarHeight : appBarTitleLine),
                          along | Qt::AlignVCenter | Qt::TextSingleLine,
                          QFontMetrics(font).elidedText(bar->windowTitle(), Qt::ElideRight, to - from));
        if (!subtitle.isEmpty()) {
            QFont subtitleFont = bar->font();
            subtitleFont.setPixelSize(appBarSubtitlePixels);
            subtitleFont.setWeight(QFont::Medium);
            painter->setFont(subtitleFont);
            painter->setPen(color(Role::OnSurfaceVariant));
            painter->drawText(QRect(from, top + appBarTitleLine, to - from, appBarSubtitleLine),
                              along | Qt::AlignVCenter | Qt::TextSingleLine,
                              QFontMetrics(subtitleFont).elidedText(subtitle, Qt::ElideRight, to - from));
        }
        painter->restore();
    }
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
    const QString kind = bar ? bar->property("materialTabBar").toString() : QString();
    return bar &&
           (kind == QLatin1String("navigationRail") || kind == QLatin1String("navigationRailExpanded") ||
            kind == QLatin1String("navigationRailModal")) &&
           (bar->shape() == QTabBar::RoundedWest || bar->shape() == QTabBar::RoundedEast ||
            bar->shape() == QTabBar::TriangularWest || bar->shape() == QTabBar::TriangularEast);
}

bool QMaterialStyle::isExpandedRail(const QWidget *widget) {
    if (!isNavigationRail(widget))
        return false;
    const QString kind = widget->property("materialTabBar").toString();
    return kind == QLatin1String("navigationRailExpanded") || kind == QLatin1String("navigationRailModal");
}

bool QMaterialStyle::isModalRail(const QWidget *widget) {
    return isNavigationRail(widget) &&
           widget->property("materialTabBar").toString() == QLatin1String("navigationRailModal");
}

int QMaterialStyle::collapsedRailWidth() { return railWidth; }

bool QMaterialStyle::hasHorizontalNavigationItems(const QWidget *widget) {
    if (!isNavigationBar(widget))
        return false;
    const QString items = widget->property("materialNavigationItems").toString();
    if (items == QLatin1String("horizontal"))
        return true;
    if (items == QLatin1String("vertical"))
        return false;
    return widget->window()->width() >= navigationMediumWidth;
}

// As wide as the widest destination needs, so that they are all alike.
int QMaterialStyle::horizontalNavigationItemWidth(const QTabBar *bar) {
    const QFontMetrics metrics(bar->font());
    int widest = 0;
    for (int i = 0; i < bar->count(); ++i) {
        if (!bar->isTabVisible(i))
            continue;
        const int label = metrics.horizontalAdvance(bar->tabText(i));
        const bool icon = !bar->tabIcon(i).isNull();
        int width = (icon ? navigationIcon : 0) + label;
        if (icon && label > 0)
            width += navigationRowIconSpace;
        widest = std::max(widest, width);
    }
    return widest + 2 * navigationRowPadding + navigationRowSpace;
}

namespace {
// What a badge says: true is a dot; a number above none, or some text, is a badge that says it.
QString badgeText(const QVariant &badge, bool *dot) {
    *dot = false;
    if (badge.typeId() == QMetaType::Bool) {
        *dot = badge.toBool();
        return QString();
    }
    if (badge.typeId() == QMetaType::Int || badge.typeId() == QMetaType::LongLong ||
        badge.typeId() == QMetaType::UInt || badge.typeId() == QMetaType::ULongLong) {
        const qlonglong count = badge.toLongLong();
        if (count <= 0)
            return QString();
        return count > 999 ? QStringLiteral("999+") : QString::number(count);
    }
    return badge.toString();
}

QFont badgeFont(QFont font) {
    font.setPixelSize(badgePixels);
    font.setWeight(QFont::Medium);
    return font;
}

// The width of a badge with `text` in it.
int badgeWidth(const QFont &font, const QString &text) {
    return std::max(badgeHeight, QFontMetrics(badgeFont(font)).horizontalAdvance(text) + 8);
}
} // namespace

void QMaterialStyle::drawBadge(QPainter *painter, const QVariant &badge, const QRect &icon) const {
    bool dot = false;
    const QString text = badgeText(badge, &dot);
    if (!dot && text.isEmpty())
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const int right = icon.x() + icon.width();
    if (dot) {
        fillRounded(painter, QRectF(right - badgeDot, icon.y(), badgeDot, badgeDot), badgeDot / 2.0,
                    color(Role::Error));
    } else {
        const int width = badgeWidth(painter->font(), text);
        painter->setFont(badgeFont(painter->font()));
        // It starts over the icon and grows away from it.
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
void QMaterialStyle::drawNavigationBar(QPainter *painter, const QTabBar *bar, int top) const {
    if (isExpandedRail(bar)) {
        drawExpandedRail(painter, bar, top);
        return;
    }
    if (hasHorizontalNavigationItems(bar)) {
        drawHorizontalNavigationBar(painter, bar);
        return;
    }
    painter->fillRect(bar->rect(), color(isNavigationRail(bar) ? Role::Surface : Role::SurfaceContainer));
    const QVariantList badges = bar->property("materialBadges").toList(); // one for each destination, in order
    painter->save();
    if (top > 0)
        painter->setClipRect(QRect(0, top, bar->width(), std::max(0, bar->height() - top)));
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

// Each destination a row: a pill across the rail behind the one that is shown, the icon in it and the label beside the
// icon; a count is at the end of the row and a dot on the icon.
void QMaterialStyle::drawExpandedRail(QPainter *painter, const QTabBar *bar, int top) const {
    const QVariantList badges = bar->property("materialBadges").toList();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (isModalRail(bar)) {
        // Over the scrim, with its trailing corners rounded.
        const bool east = bar->shape() == QTabBar::RoundedEast || bar->shape() == QTabBar::TriangularEast;
        QPainterPath shape;
        shape.addRoundedRect(
            QRectF(bar->rect()).adjusted(east ? 0 : -modalRailCorner, 0, east ? modalRailCorner : 0, 0),
            modalRailCorner, modalRailCorner);
        QPainterPath inside;
        inside.addRect(QRectF(bar->rect()));
        painter->fillPath(shape.intersected(inside), QBrush(color(Role::SurfaceContainer)));
    } else {
        painter->fillRect(bar->rect(), color(Role::Surface));
    }
    if (top > 0)
        painter->setClipRect(QRect(0, top, bar->width(), std::max(0, bar->height() - top)));
    painter->setFont(bar->font());
    for (int i = 0; i < bar->count(); ++i) {
        const QRect tab = bar->tabRect(i);
        if (tab.isEmpty())
            continue;
        const bool selected = i == bar->currentIndex();
        const bool usable = bar->isTabEnabled(i) && bar->isEnabled();
        const QIcon icon = bar->tabIcon(i);
        const QString label = bar->tabText(i);
        const QRectF pill(tab.left() + railExpandedInset, tab.top(), std::max(0, tab.width() - 2 * railExpandedInset),
                          tab.height());
        // It opens from the middle as its destination is chosen.
        double open = selected ? 1.0 : 0.0;
        if (m_motion)
            open = std::clamp(m_animator->value(bar, NavigationChannel + i, open, QtPrivate::fastSpatialSpring, 0.004),
                              0.0, 1.1);
        if (open > 0.02) {
            const double width = std::min(pill.width(), pill.height() + (pill.width() - pill.height()) * open);
            QColor fill = color(Role::SecondaryContainer);
            fill.setAlphaF(std::min(1.0, 2 * open));
            fillRounded(painter, QRectF(pill.center().x() - width / 2, pill.top(), width, pill.height()),
                        pill.height() / 2, fill);
        }
        if (usable && bar == m_rippleWidget.data() && tab.contains(m_rippleOrigin.toPoint())) {
            QPainterPath outline;
            outline.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
            drawRipple(painter, bar, outline, pill, m_rippleOrigin, m_rowPressed,
                       color(selected ? Role::OnSecondaryContainer : Role::OnSurface));
        }
        if (selected && bar->hasFocus() && m_keyboard)
            strokeRounded(painter, pill, pill.height() / 2, color(Role::Secondary), focusRingWidth);
        const QColor faint = withOpacity(color(Role::OnSurface), disabledContentOpacity);
        const QColor ink = !usable ? faint : color(selected ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
        int left = static_cast<int>(pill.left()) + railExpandedPadding;
        int right = static_cast<int>(pill.right()) - railExpandedPadding;
        const QVariant badge = i < badges.size() ? badges.at(i) : QVariant();
        bool dot = false;
        const QString count = badgeText(badge, &dot);
        if (!icon.isNull()) {
            const QRect lead(left, tab.top() + (tab.height() - navigationIcon) / 2, navigationIcon, navigationIcon);
            paintIcon(painter, lead, icon, usable ? QIcon::Normal : QIcon::Disabled, selected ? QIcon::On : QIcon::Off,
                      ink);
            if (dot)
                drawBadge(painter, badge, lead);
            left = lead.right() + 1 + railExpandedIconSpace;
        }
        if (!count.isEmpty()) {
            const int width = badgeWidth(painter->font(), count);
            const QRect end(right - width, tab.top() + (tab.height() - badgeHeight) / 2, width, badgeHeight);
            fillRounded(painter, QRectF(end), badgeHeight / 2.0, color(Role::Error));
            painter->save();
            painter->setFont(badgeFont(painter->font()));
            painter->setPen(color(Role::OnError));
            painter->drawText(end, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, count);
            painter->restore();
            right = end.left() - railExpandedIconSpace;
        }
        if (!label.isEmpty()) {
            const QRect area(left, tab.top(), std::max(0, right - left), tab.height());
            painter->setPen(ink);
            painter->drawText(area, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              painter->fontMetrics().elidedText(label, Qt::ElideRight, area.width()));
        }
    }
    painter->restore();
}

// Each destination a pill of 40 in the middle of the bar, the icon and the label beside it, behind the one that is
// shown; QTabBar centres the destinations.
void QMaterialStyle::drawHorizontalNavigationBar(QPainter *painter, const QTabBar *bar) const {
    painter->fillRect(bar->rect(), color(Role::SurfaceContainer));
    const QVariantList badges = bar->property("materialBadges").toList();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(bar->font());
    const QFontMetrics metrics = painter->fontMetrics();
    for (int i = 0; i < bar->count(); ++i) {
        const QRect tab = bar->tabRect(i);
        if (tab.isEmpty())
            continue;
        const bool selected = i == bar->currentIndex();
        const bool usable = bar->isTabEnabled(i) && bar->isEnabled();
        const QIcon icon = bar->tabIcon(i);
        const int room = std::max(0, tab.width() - navigationRowSpace - 2 * navigationRowPadding);
        const int iconWidth = icon.isNull() ? 0 : navigationIcon;
        const int space = icon.isNull() ? 0 : navigationRowIconSpace;
        const QString label =
            metrics.elidedText(bar->tabText(i), Qt::ElideRight, std::max(0, room - iconWidth - space));
        const int labelWidth = label.isEmpty() ? 0 : metrics.horizontalAdvance(label);
        const int content = iconWidth + (label.isEmpty() ? 0 : space + labelWidth);
        const QRectF pill(tab.center().x() + 0.5 - (content + 2 * navigationRowPadding) / 2.0,
                          tab.top() + (tab.height() - navigationRowHeight) / 2.0, content + 2 * navigationRowPadding,
                          navigationRowHeight);
        // It opens from the middle as its destination is chosen.
        double open = selected ? 1.0 : 0.0;
        if (m_motion)
            open = std::clamp(m_animator->value(bar, NavigationChannel + i, open, QtPrivate::fastSpatialSpring, 0.004),
                              0.0, 1.1);
        if (open > 0.02) {
            const double width = std::min(pill.width(), pill.height() + (pill.width() - pill.height()) * open);
            QColor fill = color(Role::SecondaryContainer);
            fill.setAlphaF(std::min(1.0, 2 * open));
            fillRounded(painter, QRectF(pill.center().x() - width / 2, pill.top(), width, pill.height()),
                        pill.height() / 2, fill);
        }
        if (usable && bar == m_rippleWidget.data() && tab.contains(m_rippleOrigin.toPoint())) {
            QPainterPath outline;
            outline.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
            drawRipple(painter, bar, outline, pill, m_rippleOrigin, m_rowPressed,
                       color(selected ? Role::OnSecondaryContainer : Role::OnSurface));
        }
        if (selected && bar->hasFocus() && m_keyboard)
            strokeRounded(painter, pill, pill.height() / 2, color(Role::Secondary), focusRingWidth);
        const QColor faint = withOpacity(color(Role::OnSurface), disabledContentOpacity);
        const QColor iconInk = !usable ? faint : color(selected ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
        const QColor labelInk = !usable ? faint : color(selected ? Role::Secondary : Role::OnSurfaceVariant);
        int left = static_cast<int>(pill.left()) + navigationRowPadding;
        if (!icon.isNull()) {
            const QRect lead(left, tab.top() + (tab.height() - navigationIcon) / 2, navigationIcon, navigationIcon);
            paintIcon(painter, lead, icon, usable ? QIcon::Normal : QIcon::Disabled, selected ? QIcon::On : QIcon::Off,
                      iconInk);
            if (i < badges.size())
                drawBadge(painter, badges.at(i), lead);
            left = lead.right() + 1 + navigationRowIconSpace;
        }
        if (!label.isEmpty()) {
            painter->setPen(labelInk);
            painter->drawText(QRect(left, static_cast<int>(pill.top()), labelWidth + 1, navigationRowHeight),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, label);
        }
    }
    painter->restore();
}

bool QMaterialStyle::isFloatingToolBar(const QWidget *widget) {
    const QString kind = toolBarKind(widget);
    return kind == QLatin1String("floating") || kind == QLatin1String("floatingVibrant");
}

bool QMaterialStyle::isDockedToolBar(const QWidget *widget) { return isDockedKind(toolBarKind(widget)); }

// The whole of the bar, with square corners and no shadow.
void QMaterialStyle::drawDockedToolBar(QPainter *painter, const QToolBar *bar) const {
    painter->fillRect(bar->rect(), color(isVibrantToolBar(bar) ? Role::PrimaryContainer : Role::SurfaceContainer));
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

void QMaterialStyle::drawClearButton(QPainter *painter, const QRect &slot, const QRect &area, const QLineEdit *field,
                                     bool hovered, bool pressed) const {
    drawFieldIconLayer(painter, slot, area, field, hovered, pressed);
    const QPointF centre = QRectF(area).center();
    constexpr double reach = 5; // from the middle to the end of a stroke, across and down
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(QBrush(color(Role::OnSurfaceVariant)), 2, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(centre + QPointF(-reach, -reach), centre + QPointF(reach, reach));
    painter->drawLine(centre + QPointF(-reach, reach), centre + QPointF(reach, -reach));
    painter->restore();
}

bool QMaterialStyle::hidesPlaceholder(const QWidget *field) const {
    return !textFieldOf(field).label.isEmpty() && !isLabelUp(field);
}

QColor QMaterialStyle::caretColor(const QWidget *field) const {
    return color(textFieldOf(field).error ? Role::Error : Role::Primary);
}

// The viewport of a text editor lies over the frame where the label rests, so it draws the label too, in the same
// place: what each draws joins where the label crosses the edge of the viewport on its way up.
void QMaterialStyle::drawEditorLabel(QPainter *painter, const QAbstractScrollArea *editor) const {
    if (!isEditorField(editor) || textFieldOf(editor).label.isEmpty())
        return;
    QStyleOption option;
    option.initFrom(editor);
    option.rect = editor->frameRect();
    painter->save();
    painter->translate(-QPointF(editor->viewport()->pos()));
    drawTextFieldText(painter, &option, editor);
    painter->restore();
}

bool QMaterialStyle::growsWithContent(const QWidget *editor) {
    if (!isEditorField(editor))
        return false;
    const QString kind = editor->property("materialTextField").toString();
    return kind == QLatin1String("outlined") || kind == QLatin1String("filled");
}

QString QMaterialStyle::fieldAffix(const QLineEdit *field, bool suffix) const {
    const TextField kind = textFieldOf(field);
    if (!field->hasFrame() || kind.search || (!kind.label.isEmpty() && !isLabelUp(field)))
        return QString();
    return field->property(suffix ? "materialSuffix" : "materialPrefix").toString();
}

void QMaterialStyle::drawFieldIcon(QPainter *painter, const QRect &slot, const QRect &area, const QAction *action,
                                   QIcon::Mode mode, const QLineEdit *field, bool trailing, bool hovered,
                                   bool pressed) const {
    drawFieldIconLayer(painter, slot, area, field, hovered, pressed);
    if (isFieldAvatar(action)) {
        // A picture of its own, in a circle of 30 about the middle of the icon.
        paintAvatar(painter,
                    QRect(area.left() - (fieldAvatar - area.width()) / 2,
                          area.top() - (fieldAvatar - area.height()) / 2, fieldAvatar, fieldAvatar),
                    action->icon(), mode, QIcon::Off);
        return;
    }
    const bool usable = mode != QIcon::Disabled;
    QColor ink = usable ? color(Role::OnSurfaceVariant) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    if (usable && trailing && textFieldOf(field).error)
        ink = color(Role::Error); // the trailing icon tells of the error too
    paintIcon(painter, area, action->icon(), mode, QIcon::Off, ink);
}

// The state layer of a standard icon button at an end of a field: OnSurfaceVariant under the pointer and while it is
// held down, in a circle of 40 about the icon. With motion the press is the ripple, from where it was.
void QMaterialStyle::drawFieldIconLayer(QPainter *painter, const QRect &slot, const QRect &area, const QLineEdit *field,
                                        bool hovered, bool pressed) const {
    const QColor ink = color(Role::OnSurfaceVariant);
    const QPointF centre = QRectF(area).center();
    constexpr double radius = fieldIconLayer / 2.0;
    const QRectF circle(centre.x() - radius, centre.y() - radius, fieldIconLayer, fieldIconLayer);
    const bool rippling =
        m_motion && field && field == m_rippleWidget.data() && slot.contains(m_rippleOrigin.toPoint());
    double layer = hovered ? hoverOpacity : 0.0;
    if (pressed && !m_motion)
        layer = pressedOpacity;
    if (layer <= 0 && !rippling)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (layer > 0)
        fillRounded(painter, circle, radius, withOpacity(ink, layer));
    if (rippling) {
        QPainterPath outline;
        outline.addEllipse(circle);
        drawRipple(painter, field, outline, circle, m_rippleOrigin, pressed, ink);
    }
    painter->restore();
}

int QMaterialStyle::fieldIconShift(const QLineEdit *field) const {
    // The outermost target of a search bar is 4 from its edge, beside the room for text that ends 16 from it.
    if (textFieldOf(field).search)
        return textFieldPadding - searchTargetInset;
    return proxy()->pixelMetric(PM_LineEditIconMargin, nullptr, field) / 4;
}

int QMaterialStyle::fieldIconInset(const QLineEdit *field) const {
    return textFieldOf(field).search ? (searchIconTarget - fieldIcon) / 2 : 0;
}

bool QMaterialStyle::isFieldAvatar(const QAction *action) {
    return action && action->property("materialAvatar").toBool();
}

void QMaterialStyle::pressFieldIcon(QLineEdit *field, const QPointF &at) {
    if (!m_motion)
        return;
    m_rippleWidget = field;
    m_rippleOrigin = at;
    m_animator->place(field, RippleRadiusChannel, 0);
    m_animator->place(field, RippleOpacityChannel, 1);
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
    if (!widget || (field.label.isEmpty() && field.supporting.isEmpty() && field.counter.isEmpty()))
        return;
    const QRectF box(field.box(option->rect));
    const QColor quiet = enabled(option) ? color(field.error ? Role::Error : Role::OnSurfaceVariant)
                                         : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    painter->save();
    if (!field.label.isEmpty()) {
        const double up = std::clamp(
            animated(widget, LabelChannel, isLabelUp(widget) ? 1.0 : 0.0, QtPrivate::defaultEffectsSpring, 0.004), 0.0,
            1.0);
        QFont font = widget->font();
        font.setPixelSize(static_cast<int>(std::lround(fieldInputPixels - (fieldInputPixels - fieldSmallPixels) * up)));
        // Floating, it is on the outline of an outlined field, and in the upper part of a filled one.
        // A multi-line field's label rests on its first line.
        double rest = box.center().y();
        if (const auto *editor = qobject_cast<const QAbstractScrollArea *>(widget); editor && isTextEditor(editor))
            rest = editor->viewport()->geometry().top() + editorDocumentMargin +
                   QFontMetrics(widget->font()).height() / 2.0;
        const double floated = field.filled ? box.top() + 16 : box.top();
        const double middle = rest + (floated - rest) * up;
        // It rests where the text begins, after the leading icons; on the outline it floats back to the edge.
        int start = static_cast<int>(box.left()) + textFieldPadding;
        if (edit)
            for (const auto &side : edit->sides())
                if (side.position == QLineEdit::LeadingPosition)
                    start = std::max(start, side.slot.right() + 1);
        const int floatedStart = field.filled ? start : static_cast<int>(box.left()) + textFieldPadding;
        const int left = static_cast<int>(std::lround(start + (floatedStart - start) * up));
        QColor ink = quiet;
        if (enabled(option) && !field.error && hasFieldFocus(widget))
            ink = color(Role::Primary);
        painter->setFont(font);
        painter->setPen(ink);
        // The label of a spin box stops before its buttons.
        int end = static_cast<int>(box.right()) + 1 - textFieldPadding;
        if (qobject_cast<const QAbstractSpinBox *>(widget))
            end = std::min(end, proxy()->subElementRect(SE_SpinBoxEditField, option, widget).right() + 1);
        const int width = std::max(0, end - left);
        painter->drawText(QRect(left, static_cast<int>(middle) - 10, width, 20),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          QFontMetrics(font).elidedText(field.label, Qt::ElideRight, width));
    }
    if (!field.supporting.isEmpty() || !field.counter.isEmpty()) {
        QFont font = widget->font();
        font.setPixelSize(fieldSmallPixels);
        const QFontMetrics metrics(font);
        painter->setFont(font);
        painter->setPen(quiet);
        const QRect line(static_cast<int>(box.left()) + textFieldPadding, static_cast<int>(box.bottom()) + 4,
                         static_cast<int>(box.width()) - 2 * textFieldPadding, fieldSupportRoom - 4);
        // The counter at the end of the line, and the supporting text in what it leaves.
        const int counted = field.counter.isEmpty() ? 0 : metrics.horizontalAdvance(field.counter) + textFieldPadding;
        if (!field.counter.isEmpty())
            painter->drawText(line, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, field.counter);
        const int width = std::max(0, line.width() - counted);
        painter->drawText(QRect(line.left(), line.top(), width, line.height()),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          metrics.elidedText(field.supporting, Qt::ElideRight, width));
    }
    painter->restore();
}

void QMaterialStyle::propertyChanged(QWidget *widget) {
    if (qobject_cast<QToolBar *>(widget) || qobject_cast<QAbstractSpinBox *>(widget) ||
        qobject_cast<QStatusBar *>(widget) || qobject_cast<QCalendarWidget *>(widget) ||
        qobject_cast<QMenu *>(widget)) {
        QEvent change(QEvent::StyleChange); // it lays itself out anew
        QCoreApplication::sendEvent(widget, &change);
    } else if (isTextEditor(widget) || widget->property(polishedFont).toBool() ||
               widget->property(polishedPalette).toBool()) {
        // A text editor may have become a text field, or another kind of one.
        // The font or the colors may be others now: a size of button has its own, and so has a navigation drawer.
        unpolish(widget);
        polish(widget);
    }
    widget->updateGeometry();
    widget->update();
    // A navigation rail that expands or collapses, or whose badges change, may be of another width: the tab widget
    // that holds it, which has no layout, lays it out anew, and a snackbar beside it moves.
    if (qobject_cast<QTabBar *>(widget)) {
        if (auto *pages = qobject_cast<QTabWidget *>(widget->parentWidget())) {
            QEvent change(QEvent::StyleChange);
            QCoreApplication::sendEvent(pages, &change);
        }
        if (const auto *window = qobject_cast<const QMainWindow *>(widget->window())) {
            auto *status = window->findChild<QStatusBar *>(QString(), Qt::FindDirectChildrenOnly);
            if (status && isSnackbar(status) && !status->currentMessage().isEmpty())
                status->updateGeometry();
        }
    }
    // The scroll bars of a scroll area follow its materialScrollBar.
    if (const auto *area = qobject_cast<const QAbstractScrollArea *>(widget)) {
        for (QScrollBar *bar : {area->verticalScrollBar(), area->horizontalScrollBar()}) {
            if (bar)
                bar->update();
        }
    }
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
    } else if (parts.avatar) {
        // The icon is a picture, in its own colors, that fills a circle.
        const QRect round(left + chipAvatarPadding, r.top() + (r.height() - chipAvatar) / 2, chipAvatar, chipAvatar);
        paintAvatar(painter, round, button->icon, enabled(option) ? QIcon::Normal : QIcon::Disabled,
                    checked ? QIcon::On : QIcon::Off);
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

void QMaterialStyle::drawEditorFrame(QPainter *painter, const QFrame *editor) const {
    // The container, outline or line, label and supporting text of a line edit's field.
    QStyleOptionFrame option;
    option.initFrom(editor);
    option.rect = editor->frameRect();
    option.lineWidth = 1;
    if (hasFieldFocus(editor))
        option.state |= State_HasFocus;
    else
        option.state &= ~State_HasFocus;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    proxy()->drawPrimitive(PE_PanelLineEdit, &option, painter, editor);
    painter->restore();
}

// A title in the accent color over an outlined card that holds the rest; a flat one has a line in the card's place.
void QMaterialStyle::drawGroupBox(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *box = static_cast<const QStyleOptionGroupBox *>(option);
    const QRect title = proxy()->subElementRect(SE_GroupBoxTitle, option, widget);
    const bool usable = enabled(option);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const double top = option->rect.top() + groupTitleHeight(box->checkable) + groupTitleGap;
    const QRectF card(option->rect.left(), top, option->rect.width(),
                      std::max(0.0, option->rect.top() + option->rect.height() - top));
    if (box->features & QStyleOptionFrame::Flat)
        painter->fillRect(QRectF(card.left(), card.top(), card.width(), 1), color(Role::OutlineVariant));
    else
        strokeRounded(painter, card, cardCorner, color(Role::OutlineVariant), 1);
    int left = title.left() + 4;
    if (box->checkable) {
        QStyleOptionButton indicator;
        indicator.state = option->state;
        indicator.palette = option->palette;
        indicator.rect = proxy()->subElementRect(SE_GroupBoxIndicator, option, widget);
        proxy()->drawPrimitive(PE_IndicatorCheckBox, &indicator, painter, widget);
        left = indicator.rect.right() + 1 + indicatorSpace;
    }
    painter->setFont(groupTitleFont(widget ? widget->font() : painter->font()));
    painter->setPen(usable || box->checkable ? color(Role::Primary)
                                             : withOpacity(color(Role::OnSurface), disabledContentOpacity));
    painter->drawText(QRect(left, title.top(), std::max(0, title.right() + 1 - left), title.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, box->text);
    painter->restore();
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

int QMaterialStyle::datePickerHeadlineHeight(const QWidget *calendar) {
    return calendar && calendar->property("materialDatePicker").toString() == QLatin1String("modal")
               ? datePickerHeader + 1
               : 0;
}

QRect QMaterialStyle::datePickerModeButton(const QRect &headline) {
    // At the end of the row of the date, 12 from the right, centred on its line of 40, 12 from the bottom.
    const int middle = headline.top() + datePickerHeader - 12 - 20;
    return QRect(headline.right() + 1 - 12 - datePickerModeButtonSize, middle - datePickerModeButtonSize / 2,
                 datePickerModeButtonSize, datePickerModeButtonSize);
}

int QMaterialStyle::datePickerInputHeight(int fieldHeight) {
    return datePickerHeader + 1 + datePickerFieldTop + fieldHeight + datePickerFieldBottom;
}

QRect QMaterialStyle::datePickerField(const QRect &room, int fieldHeight) {
    return QRect(room.left() + datePickerInset, room.top() + datePickerFieldTop,
                 std::max(0, room.width() - 2 * datePickerInset), fieldHeight);
}

void QMaterialStyle::drawDatePickerHeadline(QPainter *painter, const QRect &rect, const QCalendarWidget *calendar,
                                            bool input) const {
    const bool usable = calendar->isEnabled();
    const QColor ink =
        usable ? color(Role::OnSurfaceVariant) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const QRect text(rect.left() + datePickerInset, rect.top(), std::max(0, rect.width() - datePickerInset - 12),
                     datePickerHeader);
    const QRect button = datePickerModeButton(rect);
    painter->save();
    painter->fillRect(rect, calendar->palette().color(usable ? QPalette::Active : QPalette::Disabled, QPalette::Base));
    painter->setPen(ink);
    // The supporting text, label large, 16 from the top on a line of 20,
    QFont supporting = calendar->font();
    supporting.setPixelSize(14);
    supporting.setWeight(QFont::Medium);
    painter->setFont(supporting);
    const QRect top(text.left(), text.top() + 16, text.width(), 20);
    const QString title = input ? QStringLiteral("Enter date") : QStringLiteral("Select date");
    painter->drawText(top, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      QFontMetrics(supporting).elidedText(title, Qt::ElideRight, top.width()));
    // and the chosen date, headline large, 12 from the bottom on a line of 40, up to the icon button.
    QFont headline = calendar->font();
    headline.setPixelSize(datePickerHeadlinePixels);
    headline.setWeight(QFont::Normal);
    painter->setFont(headline);
    const QRect bottom(text.left(), text.bottom() + 1 - 12 - 40, std::max(0, button.left() - text.left()), 40);
    const QString date = calendar->selectedDate().toString(QStringLiteral("ddd, MMM d"));
    painter->drawText(bottom, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      QFontMetrics(headline).elidedText(date, Qt::ElideRight, bottom.width()));
    painter->restore();
    painter->fillRect(QRect(rect.left(), rect.top() + datePickerHeader, rect.width(), 1), color(Role::OutlineVariant));
}

void QMaterialStyle::drawDatePickerModeIcon(QPainter *painter, const QRect &button, const QWidget *widget,
                                            bool input) const {
    const QColor ink = widget->isEnabled() ? color(Role::OnSurfaceVariant)
                                           : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    painter->save();
    // 24 in the middle of the button: a pencil for the input mode, a calendar for the days.
    painter->setRenderHint(QPainter::Antialiasing);
    painter->translate(QPointF(button.center()) + QPointF(0.5, 0.5));
    if (input) {
        // A page with a band at the top, two rings that hold it, and a day marked on it.
        painter->setPen(QPen(ink, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(-8, -6, 16, 15), 2, 2);
        painter->drawLine(QPointF(-8, -2), QPointF(8, -2));
        painter->drawLine(QPointF(-4, -9), QPointF(-4, -5));
        painter->drawLine(QPointF(4, -9), QPointF(4, -5));
        painter->fillRect(QRectF(2, 2, 3, 3), ink);
    } else {
        // A pencil, its point at the bottom left: a body, the point, and the band before the eraser at the top right.
        painter->rotate(-45);
        QPainterPath pencil;
        pencil.moveTo(-10.5, 0);
        pencil.lineTo(-6, -3);
        pencil.lineTo(8, -3);
        pencil.lineTo(8, 3);
        pencil.lineTo(-6, 3);
        pencil.closeSubpath();
        painter->setPen(QPen(ink, 1.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(pencil);
        painter->drawLine(QPointF(4.5, -3), QPointF(4.5, 3));
        painter->drawLine(QPointF(-6, -3), QPointF(-6, 3));
    }
    painter->restore();
}

void QMaterialStyle::drawCalendarTitle(QPainter *painter, const QRect &rect, const QWidget *label, const QString &text,
                                       bool open) const {
    QFont font = label->font();
    font.setPixelSize(14); // label large
    font.setWeight(QFont::Medium);
    const QFontMetrics metrics(font);
    const QString shown = metrics.elidedText(text, Qt::ElideRight, std::max(0, rect.width() - calendarMenuArrow));
    const int width = metrics.horizontalAdvance(shown) + calendarMenuArrow;
    const int left = std::max(rect.left(), rect.left() + (rect.width() - width) / 2);
    const QColor ink = label->isEnabled() ? color(Role::OnSurfaceVariant)
                                          : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(font);
    painter->setPen(ink);
    painter->drawText(QRect(left, rect.top(), width - calendarMenuArrow, rect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, shown);
    // The arrow of a menu, down while the days are shown and up while the years are.
    const QPointF centre(left + width - calendarMenuArrow / 2.0, rect.top() + rect.height() / 2.0);
    const double tip = open ? -2.5 : 2.5;
    QPainterPath arrow;
    arrow.moveTo(centre.x() - 5, centre.y() - tip);
    arrow.lineTo(centre.x() + 5, centre.y() - tip);
    arrow.lineTo(centre.x(), centre.y() + tip);
    arrow.closeSubpath();
    painter->fillPath(arrow, ink);
    painter->restore();
}

void QMaterialStyle::drawCalendarYear(QPainter *painter, const QRect &cell, const QCalendarWidget *calendar, int year,
                                      bool selected, bool current, bool cursor, bool hovered, bool pressed) const {
    const bool usable = calendar->isEnabled();
    const double width = std::min(calendarYearWidth, cell.width());
    const double height = std::min(calendarYearHeight, cell.height());
    const QRectF pill(cell.left() + (cell.width() - width) / 2, cell.top() + (cell.height() - height) / 2, width,
                      height);
    QColor text = color(current ? Role::Primary : Role::OnSurfaceVariant);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (selected) {
        fillRounded(painter, pill, height / 2,
                    usable ? color(Role::Primary) : withOpacity(color(Role::OnSurface), disabledContainerOpacity));
        text = color(Role::OnPrimary);
    } else if (current) {
        strokeRounded(painter, pill, height / 2, usable ? color(Role::Primary) : text, 1);
    }
    if (!usable)
        text = withOpacity(color(Role::OnSurface), disabledContentOpacity);
    // The state layer of the pill under the pointer or pressed, in the color of its text.
    if (usable && (hovered || pressed))
        fillRounded(painter, pill, height / 2, withOpacity(text, pressed ? pressedOpacity : hoverOpacity));
    // The year that the keyboard moves.
    if (cursor && m_keyboard && calendar->hasFocus())
        strokeRounded(painter, pill, height / 2, color(Role::Secondary), focusRingWidth);
    painter->setFont(calendar->font());
    painter->setPen(text);
    painter->drawText(cell, Qt::AlignCenter, QString::number(year));
    painter->restore();
}

double QMaterialStyle::datePickerYearsShown(const QCalendarWidget *calendar, bool choosing) const {
    // A calendar is not one of the widgets that animated() moves, so this asks the animator itself.
    const double target = choosing ? 1.0 : 0.0;
    if (!m_motion)
        return target;
    return std::clamp(m_animator->value(calendar, YearsChannel, target, QtPrivate::defaultEffectsSpring, 0.004), 0.0,
                      1.0);
}

void QMaterialStyle::drawRowIcon(QPainter *painter, const QRect &area, const QIcon &icon, QIcon::Mode mode,
                                 bool selected, bool enabled, const QWidget *view) const {
    if (const QSize picture = listLeadingSize(view); picture.isValid()) {
        // An avatar or an image, in its own colors.
        const QIcon::Mode shown = enabled ? QIcon::Normal : QIcon::Disabled;
        if (picture.width() == listAvatarSize)
            paintAvatar(painter, area, icon, shown, QIcon::Off);
        else
            paintPicture(painter, area, listImageCorner, icon, shown, QIcon::Off);
        return;
    }
    const QColor ink = !enabled ? withOpacity(color(Role::OnSurface), disabledContentOpacity)
                                : color(selected ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
    paintIcon(painter, area, icon, mode == QIcon::Selected ? QIcon::Normal : mode, QIcon::Off, ink);
}

QSize QMaterialStyle::listLeadingSize(const QWidget *view) {
    if (!view)
        return QSize();
    if (view->property("materialListAvatars").toBool())
        return QSize(listAvatarSize, listAvatarSize);
    if (view->property("materialListImages").toBool())
        return QSize(listImageSize, listImageSize);
    return QSize();
}

bool QMaterialStyle::isSegmentedList(const QWidget *view) {
    return view && isList(view) && view->property("materialList").toString() == QLatin1String("segmented");
}

bool QMaterialStyle::isMultiPartRow(const QString &text) {
    return text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\t'));
}

int QMaterialStyle::listRowHeight(const QString &text, int leading) {
    const int lines = listRowParts(text).lines;
    const int height = lines == 3 ? listThreeLineHeight : lines == 2 ? listTwoLineHeight : listOneLineHeight;
    return leading > 0 ? std::max(height, leading + 2 * listLeadingPadding) : height;
}

int QMaterialStyle::listRowContentTop(const QRect &row, int height, const QString &text) {
    return listRowParts(text).lines == 3 ? row.top() + listThreeLineTop : row.top() + (row.height() - height) / 2;
}

QRect QMaterialStyle::listCheckRect(const QRect &row, const QString &text, const QWidget *view) {
    const int margin = isDrawerList(view) ? drawerRowInset + 16 : 16;
    const int inset = (listCheckSlot - checkBoxSize) / 2;
    return QRect(row.right() + 1 - margin - listCheckSlot + inset, listRowContentTop(row, listCheckSlot, text) + inset,
                 checkBoxSize, checkBoxSize);
}

QRect QMaterialStyle::listCheckTarget(const QRect &box, const QRect &row) {
    const int grow = (listCheckReach - box.width()) / 2;
    return box.adjusted(-grow, -grow, grow, grow).intersected(row);
}

int QMaterialStyle::listCheckRoom() { return listCheckSlot + listTrailingSpace; }

void QMaterialStyle::drawListCheck(QPainter *painter, const QRect &box, Qt::CheckState state, bool enabled,
                                   const QWidget *view) const {
    QStyleOptionButton option;
    option.rect = box;
    option.state = enabled ? State_Enabled : State_None;
    option.state |= state == Qt::Checked ? State_On : state == Qt::PartiallyChecked ? State_NoChange : State_Off;
    painter->save();
    proxy()->drawPrimitive(PE_IndicatorCheckBox, &option, painter, view);
    painter->restore();
}

QSize QMaterialStyle::listRowTextSize(const QString &text, const QFont &font, const QWidget *view) {
    const ListRowParts parts = listRowParts(text, hasListOverline(view));
    const QFontMetrics headline(font);
    const QFontMetrics label(listRowFont(font, 11, true));
    int width = headline.horizontalAdvance(parts.headline);
    if (!parts.trailing.isEmpty())
        width += listTrailingSpace + label.horizontalAdvance(parts.trailing);
    width = std::max(width, label.horizontalAdvance(parts.overline));
    // The supporting text is shortened to the width of the row, and does not widen it.
    const QFontMetrics supporting(listRowFont(font, 14, false));
    const int lines = parts.lines;
    return QSize(width, std::max(listHeadlineLine, headline.height()) +
                            (lines - 1) * std::max(listSupportingLine, supporting.height()));
}

void QMaterialStyle::drawListRowText(QPainter *painter, const QRect &area, const QString &text, const QFont &font,
                                     const QColor &headlineInk, Qt::Alignment alignment, bool selected, bool enabled,
                                     const QWidget *view) const {
    const ListRowParts parts = listRowParts(text, hasListOverline(view));
    const QColor quiet = !enabled ? withOpacity(color(Role::OnSurface), disabledContentOpacity)
                                  : color(selected ? Role::OnSecondaryContainer : Role::OnSurfaceVariant);
    const QFont supportingFont = listRowFont(font, 14, false);
    const QFont trailingFont = listRowFont(font, 11, true);
    const QFontMetrics headlineMetrics(font);
    const QFontMetrics supportingMetrics(supportingFont);
    const QFontMetrics labelMetrics(trailingFont);
    const bool over = parts.hasOverline;
    const int overlineHeight = over ? std::max(listOverlineLine, labelMetrics.height()) : 0;
    const int headlineHeight = std::max(listHeadlineLine, headlineMetrics.height());
    const int supportingHeight = std::max(listSupportingLine, supportingMetrics.height());
    // Under an overline, one line of supporting text.
    const int supportingLines = over ? 1 : parts.lines - 1;
    const int block = overlineHeight + headlineHeight + supportingLines * supportingHeight;
    const int top = parts.lines == 3 ? area.top() + listThreeLineTop : area.top() + (area.height() - block) / 2;
    const int headlineTop = top + overlineHeight;
    const int horizontal = static_cast<int>(alignment & Qt::AlignHorizontal_Mask);
    QRect room = area;
    painter->save();
    if (!parts.trailing.isEmpty()) {
        // At the trailing end, in the middle of the row, or beside the headline of a row of three lines.
        const int width = labelMetrics.horizontalAdvance(parts.trailing);
        const QRect trailing = parts.lines == 3 ? QRect(area.right() + 1 - width, headlineTop, width, headlineHeight)
                                                : QRect(area.right() + 1 - width, area.top(), width, area.height());
        painter->setFont(trailingFont);
        painter->setPen(quiet);
        painter->drawText(trailing, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, parts.trailing);
        room.setRight(trailing.left() - listTrailingSpace - 1);
    }
    const int width = std::max(0, room.width());
    if (over) {
        // The overline takes the whole width: the trailing text is beside the headline.
        painter->setFont(trailingFont);
        painter->setPen(quiet);
        painter->drawText(QRect(area.left(), top, std::max(0, area.width()), overlineHeight),
                          horizontal | Qt::AlignVCenter | Qt::TextSingleLine,
                          labelMetrics.elidedText(parts.overline, Qt::ElideRight, std::max(0, area.width())));
    }
    painter->setFont(font);
    painter->setPen(headlineInk);
    painter->drawText(QRect(room.left(), headlineTop, width, headlineHeight),
                      horizontal | Qt::AlignVCenter | Qt::TextSingleLine,
                      headlineMetrics.elidedText(parts.headline, Qt::ElideRight, width));
    if (parts.lines > 1) {
        // One line of a row of two lines, or under an overline; up to two of a row of three, the second shortened if
        // there is more.
        QStringList lines = supportingLines == 2 ? wrapListText(parts.supporting, supportingMetrics, width)
                                                 : QStringList{parts.supporting};
        while (lines.size() > supportingLines) {
            lines[lines.size() - 2] += QLatin1Char(' ') + lines.last();
            lines.removeLast();
        }
        painter->setFont(supportingFont);
        painter->setPen(quiet);
        int y = headlineTop + headlineHeight;
        for (const QString &line : lines) {
            painter->drawText(QRect(room.left(), y, width, supportingHeight),
                              horizontal | Qt::AlignVCenter | Qt::TextSingleLine,
                              supportingMetrics.elidedText(line, Qt::ElideRight, width));
            y += supportingHeight;
        }
    }
    painter->restore();
}

bool QMaterialStyle::isTimeInput(const QWidget *widget) {
    const auto *edit = qobject_cast<const QDateTimeEdit *>(widget);
    return edit && edit->property("materialTimeInput").toBool() &&
           !(edit->displayedSections() & QDateTimeEdit::DateSections_Mask);
}

std::vector<QRect> QMaterialStyle::timeInputRects(const QDateTimeEdit *edit) {
    std::vector<QRect> rects;
    int left = 0;
    bool box = false; // there is a box before this one
    for (int i = 0; i < edit->sectionCount(); ++i) {
        if (edit->sectionAt(i) == QDateTimeEdit::AmPmSection) {
            left += box ? periodSpace : 0;
            rects.push_back(QRect(left, 0, periodWidth, timeBoxHeight));
            left += periodWidth;
        } else {
            left += box ? timeSeparator : 0;
            rects.push_back(QRect(left, 0, timeBoxWidth, timeBoxHeight));
            left += timeBoxWidth;
        }
        box = true;
    }
    return rects;
}

void QMaterialStyle::drawTimeInput(QPainter *painter, const QStyleOption *option, const QDateTimeEdit *edit) const {
    const std::vector<QRect> rects = timeInputRects(edit);
    const QStringList parts = timeParts(edit->text());
    const bool usable = enabled(option);
    const bool focused = option->state & State_HasFocus;
    const int current = focused ? edit->currentSectionIndex() : -1;
    const QColor faint = withOpacity(color(Role::OnSurface), disabledContentOpacity);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QFont digits = edit->font();
    digits.setPixelSize(timeDigits);
    digits.setWeight(QFont::Normal);
    QFont small = edit->font();
    small.setPixelSize(12);
    QFont period = edit->font();
    period.setPixelSize(16);
    period.setWeight(QFont::Medium);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const QRectF at(rects[i]);
        const QDateTimeEdit::Section section = edit->sectionAt(static_cast<int>(i));
        const bool here = static_cast<int>(i) == current;
        if (section == QDateTimeEdit::AmPmSection) {
            // Two halves, the one that the time is in filled.
            const bool after = edit->time().hour() >= 12;
            QPainterPath outline;
            outline.addRoundedRect(at.adjusted(0.5, 0.5, -0.5, -0.5), timeBoxCorner, timeBoxCorner);
            const QRectF halves[] = {QRectF(at.left(), at.top(), at.width(), at.height() / 2),
                                     QRectF(at.left(), at.center().y(), at.width(), at.height() / 2)};
            painter->save();
            painter->setClipRect(halves[after ? 1 : 0], Qt::IntersectClip);
            painter->fillPath(outline, QBrush(usable ? color(Role::TertiaryContainer)
                                                     : withOpacity(color(Role::OnSurface), disabledContainerOpacity)));
            painter->restore();
            const QColor line = here ? color(Role::Primary) : color(Role::Outline);
            painter->strokePath(outline, QPen(QBrush(line), here ? 2 : 1));
            painter->fillRect(QRectF(at.left(), at.center().y(), at.width(), 1), line);
            painter->setFont(period);
            for (int half = 0; half < 2; ++half) {
                const bool chosen = (half == 1) == after;
                painter->setPen(!usable  ? faint
                                : chosen ? color(Role::OnTertiaryContainer)
                                         : color(Role::OnSurfaceVariant));
                painter->drawText(halves[half].toRect(), static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine,
                                  half == 0 ? QStringLiteral("AM") : QStringLiteral("PM"));
            }
            continue;
        }
        fillRounded(painter, at, timeBoxCorner, color(here ? Role::PrimaryContainer : Role::SurfaceContainerHighest));
        if (here)
            strokeRounded(painter, at, timeBoxCorner, color(Role::Primary), 2);
        painter->setFont(digits);
        painter->setPen(!usable ? faint : color(here ? Role::OnPrimaryContainer : Role::OnSurface));
        const int part = static_cast<int>(i);
        painter->drawText(rects[i], static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine,
                          part < parts.size() ? parts.at(part) : QString());
        // A colon before each box but the first.
        if (i > 0) {
            painter->setPen(usable ? color(Role::OnSurface) : faint);
            painter->drawText(QRect(rects[i].left() - timeSeparator, 0, timeSeparator, timeBoxHeight),
                              static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, QStringLiteral(":"));
        }
        const char *const name = section == QDateTimeEdit::HourSection     ? "Hour"
                                 : section == QDateTimeEdit::MinuteSection ? "Minute"
                                 : section == QDateTimeEdit::SecondSection ? "Second"
                                                                           : "";
        painter->setFont(small);
        painter->setPen(usable ? color(Role::OnSurfaceVariant) : faint);
        painter->drawText(QRect(rects[i].left(), timeBoxHeight + 4, rects[i].width(), timeCaption - 4),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, QString::fromLatin1(name));
    }
    painter->restore();
}

bool QMaterialStyle::isDrawerList(const QWidget *widget) {
    return qobject_cast<const QListWidget *>(widget) &&
           widget->property("materialList").toString() == QLatin1String("navigationDrawer");
}

int QMaterialStyle::completerRowHeight(const QWidget *list) {
    switch (completerPopupOf(list)) {
    case CompleterPopup::SearchView:
    case CompleterPopup::FullScreenView:
        return searchViewRowHeight;
    case CompleterPopup::Menu:
        return menuItemHeight;
    case CompleterPopup::None:
        break;
    }
    return 0;
}

void QMaterialStyle::prepareHeadline(QDialog *dialog) const {
    // A bottom sheet has its drag handle there instead, at the top of a target of 48 above the contents.
    if (hasSheetHandle(dialog)) {
        removeHeadline(dialog);
        if (dialog->property(handleRoom).isValid())
            return;
        dialog->ensurePolished();
        QMargins margins = dialog->contentsMargins();
        dialog->setProperty(handleRoom, margins.top());
        const int inside = dialog->layout() ? dialog->layout()->contentsMargins().top() : 0;
        margins.setTop(margins.top() + std::max(0, sheetHandleTarget - inside));
        dialog->setContentsMargins(margins);
        return;
    }
    removeRoom(dialog, handleRoom);
    const QPlatformIntegration *platform = QtPrivate::GuiAccess::integration();
    if (!platform || platform->showsDialogTitles() || dialog->windowTitle().isEmpty() || !sheetKind(dialog).isEmpty()) {
        removeHeadline(dialog);
        return;
    }
    if (!dialog->property(headlineRoom).isValid()) {
        dialog->ensurePolished();
        dialog->setProperty(headlineRoom, dialog->contentsMargins().top());
        if (auto *box = qobject_cast<QMessageBox *>(dialog))
            box->placeTexts(); // its icon goes over the headline
    }
    fitHeadline(dialog);
}

int QMaterialStyle::headlineTop(const QDialog *dialog, int width) const {
    const QVariant before = dialog->property(headlineRoom);
    if (!before.isValid())
        return -1;
    const int top = before.toInt();
    const HeadlineParts parts = headlineParts(dialog, !headlineIcon(dialog).isNull(), width, top);
    // 16 over the contents, part of which a layout's own margin already is.
    const int inside = dialog->layout() ? dialog->layout()->contentsMargins().top() : 0;
    return top + std::max(0, parts.text.bottom() + 1 + headlineSpace - top - inside);
}

void QMaterialStyle::fitHeadline(QDialog *dialog) const {
    const int top = headlineTop(dialog, dialog->width());
    QMargins margins = dialog->contentsMargins();
    if (top < 0 || margins.top() == top)
        return;
    margins.setTop(top);
    dialog->setContentsMargins(margins);
}

QIcon QMaterialStyle::headlineIcon(const QDialog *dialog) {
    // The icon set on the dialog itself, not the application's, which windowIcon() falls back to.
    if (const QIcon &own = QtPrivate::WidgetAccess::data(dialog).icon; !own.isNull())
        return own;
    if (const auto *box = qobject_cast<const QMessageBox *>(dialog)) {
        if (box->icon() != QMessageBox::NoIcon) {
            const qreal ratio = std::max<qreal>(1.0, dialog->devicePixelRatioF());
            QPixmap sign = messageSign(box->icon(), qRound(dialogIconSize * ratio));
            sign.setDevicePixelRatio(ratio);
            return QIcon(sign);
        }
        if (const QPixmap pixmap = box->iconPixmap(); !pixmap.isNull())
            return QIcon(pixmap);
    }
    return QIcon();
}

void QMaterialStyle::removeHeadline(QWidget *widget) {
    if (!widget->property(headlineRoom).isValid())
        return;
    removeRoom(widget, headlineRoom);
    if (auto *box = qobject_cast<QMessageBox *>(widget))
        box->placeTexts(); // its icon goes back beside the text
}

bool QMaterialStyle::movesMessageIcon(const QMessageBox *box) { return box->property(headlineRoom).isValid(); }

void QMaterialStyle::drawHeadline(QPainter *painter, const QDialog *dialog) const {
    if (const QVariant room = dialog->property(handleRoom); room.isValid()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF bar((dialog->width() - sheetHandleWidth) / 2.0, room.toInt() + sheetHandleTop, sheetHandleWidth,
                         sheetHandleHeight);
        fillRounded(painter, bar, sheetHandleHeight / 2.0, withOpacity(color(Role::OnSurfaceVariant), 0.4));
        painter->restore();
    }
    const QVariant before = dialog->property(headlineRoom);
    if (!before.isValid())
        return;
    painter->save();
    const QIcon icon = headlineIcon(dialog);
    const HeadlineParts parts = headlineParts(dialog, !icon.isNull(), dialog->width(), before.toInt());
    if (!icon.isNull())
        paintIcon(painter, parts.icon, icon, QIcon::Normal, QIcon::Off, color(Role::Secondary));
    painter->setFont(headlineFont(dialog));
    painter->setPen(color(Role::OnSurface));
    // Under an icon the headline is centered; without one it is at the start.
    const int align = (icon.isNull() ? Qt::AlignLeft : Qt::AlignHCenter) | Qt::AlignTop | Qt::TextWordWrap;
    painter->drawText(parts.text, align, dialog->windowTitle());
    painter->restore();
}

namespace {
// A picture of a sheet that was hidden, in a window of its own over the sheet's parent, on its way back to its edge.
// The dialog itself is hidden at once, as any other dialog is; this takes no input and only shows where it goes.
class SheetGhost : public QWidget {
public:
    SheetGhost(QDialog *dialog, const QPixmap &picture)
        : QWidget(dialog->parentWidget() ? dialog->parentWidget()->window() : nullptr,
                  Qt::Dialog | Qt::FramelessWindowHint),
          m_dialog(dialog), m_picture(picture) {
        setObjectName(QStringLiteral("_nt_sheetGhost"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
        ghosts().push_back(this);
    }
    ~SheetGhost() override { forget(); }
    SheetGhost(const SheetGhost &) = delete;
    SheetGhost &operator=(const SheetGhost &) = delete;

    // The picture of `dialog` that is on its way out, if there is one.
    static SheetGhost *of(const QDialog *dialog) {
        for (SheetGhost *ghost : ghosts()) {
            if (ghost->m_dialog.data() == dialog)
                return ghost;
        }
        return nullptr;
    }
    // It no longer stands for its dialog, which is shown again.
    void forget() {
        auto &all = ghosts();
        all.erase(std::remove(all.begin(), all.end(), this), all.end());
    }

    // How far it is from its place, as a fraction of its height or width, and the opacity of its scrim.
    double away = 0;
    double scrim = scrimOpacity;

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_picture);
    }

private:
    static std::vector<SheetGhost *> &ghosts() {
        static std::vector<SheetGhost *> all;
        return all;
    }

    QPointer<QDialog> m_dialog;
    QPixmap m_picture;
};
} // namespace

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
    // One that is shown again while it slides out comes back from where its picture is.
    double away = 1.0;
    if (SheetGhost *ghost = SheetGhost::of(dialog)) {
        away = ghost->away;
        if (QWindow *window = dialog->windowHandle())
            window->setProperty(scrimProperty, ghost->scrim);
        ghost->forget();
        QtPrivate::Transition::stop(ghost, QStringLiteral("_nt_sheetSlide"));
        ghost->deleteLater(); // after the dialog is on the screen in its place
    }
    if (m_motion)
        slideSheetIn(dialog, place, kind == QLatin1String("drawer"), away);
    else if (QWindow *window = dialog->windowHandle())
        window->setProperty("_nt_sheetOffset", 0);
    return true;
}

// A sheet comes in from its edge of the window, along a spring that does not overshoot, so the edge never shows a gap.
void QMaterialStyle::slideSheetIn(QDialog *dialog, const QRect &place, bool fromLeft, double from) const {
    const int distance =
        static_cast<int>(std::lround(std::clamp(from, 0.0, 1.0) * (fromLeft ? place.width() : place.height())));
    // The android platform places a sheet itself; it learns how far from its place the sheet is from the window.
    const auto moveTo = [place, fromLeft, distance](QDialog *sheet, double progress) {
        const int away = static_cast<int>(std::lround((1.0 - std::clamp(progress, 0.0, 1.0)) * distance));
        if (QWindow *window = sheet->windowHandle())
            window->setProperty("_nt_sheetOffset", away);
        sheet->move(place.topLeft() + (fromLeft ? QPoint(-away, 0) : QPoint(0, away)));
    };
    moveTo(dialog, 0);
    QPointer<QDialog> guard(dialog);
    new QtPrivate::Transition(dialog, QStringLiteral("_nt_sheetSlide"), sheetSpring, [guard, moveTo](double progress) {
        if (!guard)
            return false;
        if (!guard->isVisible()) {
            moveTo(guard, 1.0); // where it rests, if it is shown again before it is placed
            return false;
        }
        moveTo(guard, progress);
        return true;
    });
}

// A sheet that goes slides back to its edge, faster than it came, as a picture in a window of its own: the dialog is
// hidden at once, so that isVisible(), exec() and the signals are as they are for any dialog, and the dialog may even
// be deleted while its picture slides. Its scrim fades out with it.
void QMaterialStyle::slideSheetOut(QDialog *dialog) const {
    const QString kind = sheetKind(dialog);
    QWindow *window = dialog->windowHandle();
    if (!m_motion || kind.isEmpty() || !window || QCoreApplication::closingDown())
        return;
    if (const QWidget *parent = dialog->parentWidget(); parent && !parent->window()->isVisible())
        return; // the window it is on goes too
    const bool fromLeft = kind == QLatin1String("drawer");
    const int distance = std::max(1, fromLeft ? dialog->width() : dialog->height());
    // It may still be on its way in.
    QtPrivate::Transition::stop(dialog, QStringLiteral("_nt_sheetSlide"));
    QtPrivate::Transition::stop(dialog, QStringLiteral("_nt_scrimFade"));
    const int away = std::clamp(window->property("_nt_sheetOffset").toInt(), 0, distance);
    const QVariant was = window->property(scrimProperty);
    const double scrim = was.isValid() ? std::clamp(was.toDouble(), 0.0, scrimOpacity) : scrimOpacity;
    const QPoint start = dialog->pos();
    const QPoint direction = fromLeft ? QPoint(-1, 0) : QPoint(0, 1);

    auto *ghost = new SheetGhost(dialog, dialog->grab());
    ghost->away = static_cast<double>(away) / distance;
    ghost->scrim = scrim;
    // The android platform places it as it does the sheet, with the corners and the scrim of the sheet.
    ghost->winId();
    if (QWindow *own = ghost->windowHandle()) {
        own->setProperty("_nt_sheet", kind);
        own->setProperty("_nt_sheetOffset", away);
        own->setProperty("_nt_cornerRadius", fromLeft ? drawerCorner : dialogCorner);
        own->setProperty(scrimProperty, scrim);
    }
    ghost->setGeometry(dialog->geometry());
    // Its scrim takes the place of the dialog's, which would otherwise darken what is below twice for a moment.
    window->setProperty(scrimProperty, 0.0);
    ghost->show();
    ghost->raise();
    QPointer<SheetGhost> guard(ghost);
    new QtPrivate::Transition(ghost, QStringLiteral("_nt_sheetSlide"), sheetExitSpring,
                              [guard, away, distance, scrim, start, direction](double progress) {
                                  if (!guard)
                                      return false;
                                  const double p = std::clamp(progress, 0.0, 1.0);
                                  const int now = away + static_cast<int>(std::lround((distance - away) * p));
                                  if (now >= distance || !guard->isVisible()) {
                                      guard->forget();
                                      guard->hide();
                                      guard->deleteLater();
                                      return false;
                                  }
                                  guard->away = static_cast<double>(now) / distance;
                                  guard->scrim = scrim * (1.0 - p);
                                  if (QWindow *own = guard->windowHandle()) {
                                      own->setProperty("_nt_sheetOffset", now);
                                      own->setProperty(scrimProperty, guard->scrim);
                                  }
                                  guard->move(start + direction * (now - away));
                                  return true;
                              });
}

void QMaterialStyle::fadeScrim(QDialog *dialog, double from) const {
    if (QWindow *window = dialog->windowHandle())
        window->setProperty(scrimProperty, from);
    QPointer<QDialog> guard(dialog);
    new QtPrivate::Transition(dialog, QStringLiteral("_nt_scrimFade"), QtPrivate::defaultEffectsSpring,
                              [guard, from](double progress) {
                                  if (!guard || !guard->isVisible() || !guard->windowHandle())
                                      return false;
                                  guard->windowHandle()->setProperty(
                                      scrimProperty, from + (scrimOpacity - from) * std::clamp(progress, 0.0, 1.0));
                                  // The android platform draws the scrim as it puts the dialog on the screen.
                                  guard->update();
                                  return true;
                              });
}

void QMaterialStyle::dragSheetTo(QDialog *dialog, const QRect &rest, int offset) const {
    const bool fromLeft = sheetKind(dialog) == QLatin1String("drawer");
    const int distance = std::max(1, fromLeft ? rest.width() : rest.height());
    const int away = std::clamp(offset, 0, distance);
    // The android platform places the sheet itself, by how far from its place the window says it is.
    if (QWindow *window = dialog->windowHandle()) {
        window->setProperty("_nt_sheetOffset", away);
        window->setProperty(scrimProperty, scrimOpacity * (1.0 - static_cast<double>(away) / distance));
    }
    dialog->move(rest.topLeft() + (fromLeft ? QPoint(-away, 0) : QPoint(0, away)));
    dialog->update(); // the android platform draws the scrim again as the dialog paints
}

bool QMaterialStyle::sheetMouseEvent(QObject *watched, QEvent *event) {
    const QEvent::Type type = event->type();
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseMove && type != QEvent::MouseButtonRelease)
        return false;
    const auto *mouse = static_cast<const QMouseEvent *>(event);
    // A press in a window that a modal sheet blocks, as beside the sheet on a desktop, dismisses the sheet as Escape
    // does. (On Android the platform closes it on a tap on the scrim.)
    if (type == QEvent::MouseButtonPress && watched->isWindowType()) {
        auto *sheet = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!sheet || sheet->style() != this || sheetKind(sheet).isEmpty() || !sheet->isVisible() ||
            sheet->windowHandle() == watched)
            return false;
        const QList<QWidget *> tops = QApplication::topLevelWidgets();
        const bool blocked = std::any_of(tops.begin(), tops.end(), [watched](const QWidget *top) {
            return top->windowHandle() == watched && top->isVisible() && QtPrivate::WidgetAccess::blockedByModal(top);
        });
        if (blocked)
            sheet->reject();
        return blocked;
    }
    auto *dialog = qobject_cast<QDialog *>(watched);
    if (!dialog)
        return false;
    const bool fromLeft = sheetKind(dialog) == QLatin1String("drawer");
    // A press on the sheet itself, its handle or its background, or on a child that leaves the mouse to it.
    if (type == QEvent::MouseButtonPress) {
        if (mouse->button() != Qt::LeftButton || !dialog->isWindow() || !dialog->isVisible() ||
            dialog->style() != this || sheetKind(dialog).isEmpty())
            return false;
        // It may still be on its way in: it stays where it is now.
        QtPrivate::Transition::stop(dialog, QStringLiteral("_nt_sheetSlide"));
        QtPrivate::Transition::stop(dialog, QStringLiteral("_nt_scrimFade"));
        const int distance = std::max(1, fromLeft ? dialog->width() : dialog->height());
        const QWindow *window = dialog->windowHandle();
        const int from = window ? std::clamp(window->property("_nt_sheetOffset").toInt(), 0, distance) : 0;
        m_sheetDrag = std::make_unique<SheetDrag>();
        m_sheetDrag->dialog = dialog;
        m_sheetDrag->start = mouse->globalPosition();
        m_sheetDrag->rest = dialog->geometry().translated(fromLeft ? from : 0, fromLeft ? 0 : -from);
        m_sheetDrag->from = from;
        m_sheetDrag->offset = from;
        m_sheetDrag->clock.start();
        m_sheetDrag->samples.emplace_back(0, from);
        return true;
    }
    if (!m_sheetDrag || m_sheetDrag->dialog.data() != dialog)
        return false;
    SheetDrag &drag = *m_sheetDrag;
    const int distance = std::max(1, fromLeft ? drag.rest.width() : drag.rest.height());
    if (type == QEvent::MouseMove) {
        if (!(mouse->buttons() & Qt::LeftButton))
            return false;
        // Toward the edge: down for a bottom sheet, to the left for a drawer.
        const QPointF moved = mouse->globalPosition() - drag.start;
        const double delta = fromLeft ? -moved.x() : moved.y();
        if (!drag.moving && std::abs(delta) < QApplication::startDragDistance())
            return true;
        drag.moving = true;
        drag.offset = std::clamp(drag.from + static_cast<int>(std::lround(delta)), 0, distance);
        dragSheetTo(dialog, drag.rest, drag.offset);
        const qint64 now = drag.clock.elapsed();
        drag.samples.emplace_back(now, drag.offset);
        while (drag.samples.size() > 2 && drag.samples.front().first < now - sheetFlingWindow)
            drag.samples.erase(drag.samples.begin());
        return true;
    }
    if (mouse->button() != Qt::LeftButton)
        return false;
    const std::unique_ptr<SheetDrag> ended = std::move(m_sheetDrag);
    // How fast it moved toward its edge at the end, if that can be told.
    const qint64 now = ended->clock.elapsed();
    double speed = 0;
    for (const auto &[when, offset] : ended->samples) {
        if (when >= now - sheetFlingWindow) {
            if (now - when >= sheetFlingShortest)
                speed = static_cast<double>(ended->offset - offset) * 1000.0 / static_cast<double>(now - when);
            break;
        }
    }
    // Past a third of the way, unless it was flung back, or flung toward the edge from anywhere, it closes.
    const bool close =
        ended->moving && (speed > sheetFlingSpeed || (ended->offset * 3 > distance && speed > -sheetFlingSpeed));
    if (close) {
        dialog->reject(); // as Back and Escape do; with motion, its picture slides on from where it is
        if (!m_motion)
            dragSheetTo(dialog, ended->rest, 0);
        return true;
    }
    // Otherwise it springs back to its place, and its scrim comes back with it.
    if (m_motion) {
        const QVariant scrim = dialog->windowHandle() ? dialog->windowHandle()->property(scrimProperty) : QVariant();
        slideSheetIn(dialog, ended->rest, fromLeft, static_cast<double>(ended->offset) / distance);
        fadeScrim(dialog, scrim.isValid() ? std::clamp(scrim.toDouble(), 0.0, scrimOpacity) : scrimOpacity);
    } else {
        dragSheetTo(dialog, ended->rest, 0);
    }
    return true;
}

bool QMaterialStyle::drawMaterialFrame(QPainter *painter, const QFrame *frame) const {
    if (const CompleterPopup kind = completerPopupOf(frame); kind != CompleterPopup::None) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (kind == CompleterPopup::Menu) {
            fillRounded(painter, QRectF(frame->rect()), menuCorner, color(Role::SurfaceContainer));
            strokeRounded(painter, QRectF(frame->rect()), menuCorner, color(Role::OutlineVariant), 1);
        } else if (kind == CompleterPopup::FullScreenView) {
            painter->fillRect(frame->rect(), color(Role::SurfaceContainerHigh)); // the header has the divider
        } else {
            // The view goes on from its bar, with a divider between them.
            painter->fillPath(completerShape(frame, kind), QBrush(color(Role::SurfaceContainerHigh)));
            painter->fillRect(searchViewDivider(frame), color(Role::OutlineVariant));
        }
        painter->restore();
        return true;
    }
    if (isTextEditor(frame) && frame->frameShape() == QFrame::StyledPanel) {
        drawEditorFrame(painter, frame);
        return true;
    }
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

// An outlined field, as a line edit is, with a button at its end that steps down and one that steps up.
void QMaterialStyle::drawSpinBox(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *spin = static_cast<const QStyleOptionSpinBox *>(option);
    const TextField field = textFieldOf(widget);
    const bool usable = enabled(option);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (spin->frame) {
        const bool focused = option->state & State_HasFocus;
        fillRounded(painter, QRectF(field.box(option->rect)), textFieldCorner, option->palette.color(QPalette::Base));
        const QColor line =
            !usable ? withOpacity(color(Role::OnSurface), 0.12) : color(focused ? Role::Primary : Role::Outline);
        strokeOutlinedField(painter, QRectF(option->rect), field, widget, line, focused);
        drawTextFieldText(painter, option, widget);
    }
    if (spin->buttonSymbols == QAbstractSpinBox::NoButtons) {
        painter->restore();
        return;
    }
    const auto button = [&](SubElement element, bool up, bool can, bool pressed) {
        const QRect at = proxy()->subElementRect(element, option, widget);
        if (at.isEmpty())
            return;
        const QPointF centre = QRectF(at).center();
        const QColor ink =
            usable && can ? color(Role::OnSurfaceVariant) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
        if (pressed && can)
            fillRounded(painter, QRectF(centre.x() - 20, centre.y() - 20, 40, 40), 20,
                        withOpacity(color(Role::OnSurfaceVariant), pressedOpacity));
        painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (opensCalendar(widget)) {
            // A chevron that points down, to what opens.
            painter->drawLine(centre + QPointF(-5, -2.5), centre + QPointF(0, 2.5));
            painter->drawLine(centre + QPointF(0, 2.5), centre + QPointF(5, -2.5));
            return;
        }
        painter->drawLine(centre + QPointF(-6, 0), centre + QPointF(6, 0));
        if (up)
            painter->drawLine(centre + QPointF(0, -6), centre + QPointF(0, 6));
    };
    // The calendar's button is there whether or not the date can step.
    const bool calendar = opensCalendar(widget);
    button(SE_SpinBoxUp, true, calendar || spin->upEnabled, spin->upPressed);
    button(SE_SpinBoxDown, false, calendar || spin->downEnabled, spin->downPressed);
    painter->restore();
}

// A thin handle with round ends and no track, as on a phone; wider while the pointer is on it or drags it.
void QMaterialStyle::drawScrollBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const auto *bar = static_cast<const QStyleOptionSlider *>(option);
    painter->save();
    painter->fillRect(option->rect, option->palette.color(QPalette::Base));
    if (bar->maximum > bar->minimum) {
        const QRectF handle(proxy()->subElementRect(SE_ScrollBarSlider, option, widget));
        const bool held = option->state & (State_Sunken | State_MouseOver);
        // At rest the handle fades out, and it fades in when the bar wakes; the bar keeps its room. Without motion, or
        // with materialScrollBar "always", it stays.
        double shown = 1;
        if (m_motion && widget && !scrollBarAlwaysShown(widget)) {
            const QVariant awake = widget->property(scrollAwake);
            const bool wanted = held || !awake.isValid() || awake.toBool();
            shown = std::clamp(m_animator->value(widget, ScrollBarChannel, wanted ? 1.0 : 0.0,
                                                 wanted ? QtPrivate::fastEffectsSpring : QtPrivate::slowEffectsSpring,
                                                 0.004),
                               0.0, 1.0);
        }
        if (shown < 0.01) {
            painter->restore();
            return;
        }
        const double width = held ? scrollThumbHeld : scrollThumb;
        const QRectF thumb =
            bar->orientation == Qt::Horizontal
                ? QRectF(handle.left() + 2, handle.center().y() - width / 2, handle.width() - 4, width)
                : QRectF(handle.center().x() - width / 2, handle.top() + 2, width, handle.height() - 4);
        painter->setRenderHint(QPainter::Antialiasing);
        fillRounded(painter, thumb, width / 2, withOpacity(color(Role::OnSurfaceVariant), (held ? 0.7 : 0.4) * shown));
    }
    painter->restore();
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
    const SliderSize &size = sizeOfSlider(widget);
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
    // A centered slider's active part runs from the middle of the track, where the middle of its range is, to the
    // handle; on each side the rest of the track is inactive.
    const bool centered = isCenteredSlider(widget);
    const double middle = (start + end) / 2;
    // The outer ends of the track have the corners of its size, the ends beside the handle small ones.
    painter->setPen(Qt::NoPen);
    if (beforeEnd > start) {
        painter->setBrush(activeFirst && !centered ? active : inactive);
        painter->drawPath(trackPart(part(start, beforeEnd), size.corner, size.inner, horizontal));
        if (centered && beforeEnd > middle) {
            painter->setBrush(active);
            painter->drawPath(trackPart(part(middle, beforeEnd), 0, size.inner, horizontal));
        }
    }
    if (end > afterStart) {
        painter->setBrush(activeFirst || centered ? inactive : active);
        painter->drawPath(trackPart(part(afterStart, end), size.inner, size.corner, horizontal));
        if (centered && afterStart < middle) {
            painter->setBrush(active);
            painter->drawPath(trackPart(part(afterStart, middle), size.inner, 0, horizontal));
        }
    }
    const auto onActive = [&](double at) {
        if (centered)
            return at >= std::min(middle, handle) && at <= std::max(middle, handle);
        return activeFirst ? at < handle : at > handle;
    };
    // The inset icon of a medium, large or extra large slider: inside the active part at its outer end, or, where that
    // is too short for it, in the inactive part beside the handle.
    if (const int iconSize = sliderIconSize(size); iconSize > 0 && widget && !centered) {
        const QIcon icon = widget->property("materialSliderIcon").value<QIcon>();
        const double need = iconSize + 2.0 * sliderIconPadding;
        const double activeLength = activeFirst ? beforeEnd - start : end - afterStart;
        const double inactiveLength = activeFirst ? end - afterStart : beforeEnd - start;
        const bool inActive = activeLength >= need;
        if (!icon.isNull() && (inActive || inactiveLength >= need)) {
            double at = 0; // where the icon begins along the track
            if (inActive)
                at = activeFirst ? start + sliderIconPadding : end - sliderIconPadding - iconSize;
            else
                at = activeFirst ? afterStart + sliderIconPadding : beforeEnd - sliderIconPadding - iconSize;
            const double across = (horizontal ? groove.center().y() : groove.center().x()) - iconSize / 2.0;
            const QRectF place =
                horizontal ? QRectF(at, across, iconSize, iconSize) : QRectF(across, at, iconSize, iconSize);
            QColor ink = color(inActive ? Role::OnPrimary : Role::OnSecondaryContainer);
            if (!enabled(option))
                ink = inActive ? color(Role::Surface) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
            paintIcon(painter, place.toAlignedRect(), icon, enabled(option) ? QIcon::Normal : QIcon::Disabled,
                      QIcon::Off, ink);
        }
    }
    // Stops: one at the far end of the inactive part (at both ends of a centered slider), or one for every step when
    // the slider shows tick marks.
    const auto dot = [&](double at) {
        if (at > beforeEnd && at < afterStart)
            return;
        const QPointF middlePoint = horizontal ? QPointF(at, groove.center().y()) : QPointF(groove.center().x(), at);
        fillRounded(painter,
                    QRectF(middlePoint.x() - stopSize / 2.0, middlePoint.y() - stopSize / 2.0, stopSize, stopSize),
                    stopSize / 2.0, color(onActive(at) ? Role::OnPrimary : Role::OnSecondaryContainer));
    };
    const double inset = size.corner;
    const qint64 range = static_cast<qint64>(slider->maximum) - slider->minimum;
    if (slider->tickPosition != 0 && range > 0) {
        const qint64 interval = slider->tickInterval > 0 ? slider->tickInterval
                                : slider->pageStep > 0   ? slider->pageStep
                                                         : std::max<qint64>(1, range / 10);
        // At most one dot every two dot widths.
        if (range / interval <= static_cast<qint64>((end - start) / (2 * stopSize)))
            for (qint64 v = 0; v <= range; v += interval)
                dot(start + inset + (end - start - 2 * inset) * static_cast<double>(v) / static_cast<double>(range));
    } else if (centered) {
        dot(start + inset);
        dot(end - inset);
    } else {
        dot(activeFirst ? end - inset : start + inset);
    }
    const bool pressed = option->state & State_Sunken;
    const double width =
        std::max(1.0, animated(widget, HandleWidthChannel, pressed ? sliderPressedWidth : sliderHandleWidth,
                               QtPrivate::fastSpatialSpring, 0.05));
    const double length = size.handle;
    const QRectF bar = horizontal ? QRectF(handle - width / 2, centre.y() - length / 2, width, length)
                                  : QRectF(centre.x() - length / 2, handle - width / 2, length, width);
    fillRounded(painter, bar, width / 2, knob);
    if (keyboardFocus(option))
        strokeRounded(painter, bar.adjusted(-4, -2, 4, 2), 6, color(Role::Secondary), 2);
    drawSliderValue(option, painter, widget, bar);
}

// The value indicator, over the handle (at the left of a vertical slider) while it is pressed or the keys change it.
// It grows from the handle as it comes and shrinks back into it as it goes. Near an end of the slider it stays over
// the slider. It is drawn in an overlay over what is above the slider, or in the room that the slider keeps for it.
void QMaterialStyle::drawSliderValue(const QStyleOption *option, QPainter *painter, const QWidget *widget,
                                     const QRectF &bar) const {
    const auto *slider = static_cast<const QStyleOptionSlider *>(option);
    const bool horizontal = slider->orientation == Qt::Horizontal;
    const bool shows = enabled(option) && ((option->state & State_Sunken) || keyboardFocus(option)) &&
                       showsSliderValue(widget, slider->tickPosition);
    const bool inside = !widget || keepsSliderValueRoom(widget);
    auto *watcher = inside ? nullptr
                           : static_cast<SliderValueWatcher *>(widget->findChild<QObject *>(
                                 QLatin1String(sliderValueWatcher), Qt::FindDirectChildrenOnly));
    if (!inside && !watcher)
        return; // a slider that the style has not polished
    const double grown = std::clamp(
        animated(widget, SliderValueChannel, shows ? 1.0 : 0.0, QtPrivate::fastEffectsSpring, 0.01), 0.0, 1.0);
    if (grown < 0.01) {
        if (SliderValueOverlay *overlay = watcher ? watcher->existing() : nullptr; overlay && !overlay->isHidden())
            overlay->hide();
        return;
    }
    const QFont font = sliderValueFont(widget);
    const QString text = QString::number(slider->sliderPosition);
    const double boxWidth = sliderValueWidth(QFontMetrics(font), slider->sliderPosition);
    const double handle = horizontal ? bar.center().x() : bar.center().y();
    const QRectF r(option->rect);
    QRectF box = horizontal
                     ? QRectF(handle - boxWidth / 2, bar.top() - valueGap - valueHeight, boxWidth, valueHeight)
                     : QRectF(bar.left() - valueGap - boxWidth, handle - valueHeight / 2.0, boxWidth, valueHeight);
    if (horizontal)
        box.moveLeft(std::clamp(box.left(), r.left(), std::max(r.left(), r.right() - box.width())));
    else
        box.moveTop(std::clamp(box.top(), r.top(), std::max(r.top(), r.bottom() - box.height())));
    // Where it meets the handle, which it grows from.
    const QPointF anchor = horizontal ? QPointF(std::clamp(handle, box.left(), box.right()), box.bottom())
                                      : QPointF(box.right(), std::clamp(handle, box.top(), box.bottom()));
    const QColor fill = color(Role::InverseSurface);
    const QColor ink = color(Role::InverseOnSurface);
    if (inside) {
        painter->save();
        painter->translate(anchor);
        painter->scale(grown, grown);
        painter->translate(-anchor);
        fillRounded(painter, box, valueHeight / 2.0, fill);
        painter->setFont(font);
        painter->setPen(ink);
        painter->drawText(box, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, text);
        painter->restore();
        return;
    }
    SliderValueOverlay *overlay = watcher->overlay();
    if (!overlay)
        return;
    // In the window, and inside it where the slider is near its top or its left.
    const QWidget *window = overlay->parentWidget();
    QRect place = box.translated(QPointF(widget->mapTo(window, QPoint(0, 0)))).toAlignedRect();
    place.moveTopLeft(QPoint(std::max(0, place.left()), std::max(0, place.top())));
    const QPointF offset = QPointF(widget->mapTo(window, QPoint(0, 0))) - QPointF(place.topLeft());
    if (overlay->geometry() != place)
        overlay->setGeometry(place);
    overlay->set(text, font, fill, ink, grown, anchor + offset);
    if (overlay->isHidden()) {
        overlay->show();
        overlay->raise();
    }
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
    // The wavy one is larger, so that its wave fits; its track is a plain circle along the middle of the wave.
    const bool wavy = widget && widget->property("materialProgress").toString() == QLatin1String("circularWavy");
    const double size = wavy ? circularWavySize : circularSize;
    const double extent = std::min<double>(size, std::min(option->rect.width(), option->rect.height()));
    const QPointF centre = QRectF(option->rect).center();
    const double inset = std::max(1.0, (extent - circularThickness) / 2 - (wavy ? circularWaveHeight : 0));
    const QRectF ring(centre.x() - inset, centre.y() - inset, 2 * inset, 2 * inset);
    const QColor active =
        enabled(option) ? color(Role::Primary) : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const QColor track = enabled(option) ? color(Role::SecondaryContainer)
                                         : withOpacity(color(Role::OnSurface), disabledContainerOpacity);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    constexpr double pi = 3.14159265358979323846;
    // A whole number of crests go round, so that a full circle closes.
    const double crests = std::max(3.0, std::round(2 * pi * inset / circularWaveLength));
    // How far the wave has turned, in degrees; asks to be painted again, as it goes on.
    const auto turned = [&] {
        if (!m_motion || !widget || !enabled(option))
            return 0.0;
        m_animator->keepPainting(widget);
        return m_animator->seconds() * circularWaveTurn;
    };
    // Angles are in degrees clockwise from the top; QPainter's are in sixteenths, anticlockwise from the right. A wave
    // goes `height` out and in from the circle, turned on by `phase` degrees.
    const auto arc = [&](double from, double sweep, const QColor &ink, double height = 0, double phase = 0) {
        const QPen pen(QBrush(ink), circularThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (height < 0.05) {
            painter->setPen(pen);
            painter->drawArc(ring, static_cast<int>(std::lround((90 - from) * 16)),
                             -static_cast<int>(std::lround(sweep * 16)));
            return;
        }
        const auto point = [&](double degrees) {
            const double radians = degrees * pi / 180;
            const double radius = inset + height * std::sin(crests * (degrees - phase) * pi / 180);
            return QPointF(centre.x() + radius * std::sin(radians), centre.y() - radius * std::cos(radians));
        };
        // A point for about every pixel along the circle.
        const int steps = std::max(2, static_cast<int>(std::ceil(sweep * pi / 180 * inset)));
        QPainterPath wave;
        wave.moveTo(point(from));
        for (int step = 1; step <= steps; ++step)
            wave.lineTo(point(from + sweep * step / steps));
        if (sweep >= 359.5)
            wave.closeSubpath();
        painter->strokePath(wave, pen);
    };
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
        arc(from, sweep, active, wavy ? circularWaveHeight : 0, wavy ? turned() : 0);
        painter->restore();
        return;
    }
    const qint64 span = static_cast<qint64>(bar->maximum) - bar->minimum;
    double done = 0;
    if (span > 0 && bar->progress > bar->minimum)
        done = 360.0 * static_cast<double>(std::min<qint64>(bar->progress, bar->maximum) - bar->minimum) /
               static_cast<double>(span);
    // A wave is flat when there is little to show it in, and when nearly all is done; it rises and settles.
    double height = 0;
    if (wavy && enabled(option)) {
        height = done > 36 && done < 342 ? circularWaveHeight : 0;
        if (m_motion && widget)
            height = std::clamp(m_animator->value(widget, WaveChannel, height, QtPrivate::defaultEffectsSpring, 0.02),
                                0.0, circularWaveHeight);
    }
    const double phase = height >= 0.05 ? turned() : 0;
    if (done >= 359.5) {
        arc(0, 360, active, height, phase);
    } else if (done <= 0) {
        arc(0, 360, track);
    } else {
        if (360 - done - 2 * apart > 0)
            arc(done + apart, 360 - done - 2 * apart, track);
        arc(0, std::max(done, 0.5), active, height, phase);
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

namespace {
// What a snackbar showed, while it fades out.
class SnackbarGhost : public QWidget {
public:
    SnackbarGhost(QWidget *parent, const QPixmap &picture) : QWidget(parent), m_picture(picture) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_picture);
    }

private:
    QPixmap m_picture;
};
} // namespace

void QMaterialStyle::snackbarArrives(QStatusBar *bar) const {
    const QString fade = QStringLiteral("_nt_snackbarFade");
    QGraphicsEffect *own = bar->graphicsEffect();
    if (!m_motion || (own && own->objectName() != fade))
        return; // the application's own effect stays
    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(own);
    if (!effect) {
        effect = new QGraphicsOpacityEffect;
        effect->setObjectName(fade);
        bar->setGraphicsEffect(effect);
    }
    // It fades in and grows from 80 percent of its size, about its middle, as Compose's snackbar does.
    const char *const scale = "_nt_effectScale";
    effect->setOpacity(0);
    bar->setProperty(scale, 0.8);
    QPointer<QStatusBar> guard(bar);
    new QtPrivate::Transition(bar, fade, QtPrivate::defaultEffectsSpring, [guard, fade, scale](double progress) {
        if (!guard)
            return false;
        auto *opacity = qobject_cast<QGraphicsOpacityEffect *>(guard->graphicsEffect());
        if (!opacity || opacity->objectName() != fade) {
            guard->setProperty(scale, QVariant());
            return false;
        }
        if (progress >= 1.0) {
            guard->setProperty(scale, QVariant());
            guard->setGraphicsEffect(nullptr);
            return false;
        }
        const double shown = std::clamp(progress, 0.0, 1.0);
        guard->setProperty(scale, 0.8 + 0.2 * shown);
        opacity->setOpacity(shown);
        return true;
    });
}

void QMaterialStyle::snackbarLeaves(QStatusBar *bar) const {
    QWidget *holder = bar->parentWidget();
    if (!m_motion || !holder || !bar->isVisible() || bar->height() <= 2 || bar->currentMessage().isEmpty())
        return;
    // A picture of it as it is now, buttons and all, takes its place and fades.
    QtPrivate::Transition::stop(bar, QStringLiteral("_nt_snackbarFade"));
    if (auto *opacity = qobject_cast<QGraphicsOpacityEffect *>(bar->graphicsEffect());
        opacity && opacity->objectName() == QLatin1String("_nt_snackbarFade"))
        bar->setGraphicsEffect(nullptr);
    bar->setProperty("_nt_effectScale", QVariant());
    auto *ghost = new SnackbarGhost(holder, bar->grab());
    ghost->setObjectName(QStringLiteral("_nt_snackbarGhost"));
    ghost->setGeometry(bar->geometry());
    auto *effect = new QGraphicsOpacityEffect;
    ghost->setGraphicsEffect(effect);
    ghost->show();
    ghost->raise();
    QPointer<SnackbarGhost> guard(ghost);
    new QtPrivate::Transition(ghost, QStringLiteral("_nt_fade"), QtPrivate::fastEffectsSpring,
                              [guard, effect](double progress) {
                                  if (!guard)
                                      return false;
                                  if (progress >= 1.0) {
                                      guard->hide();
                                      guard->deleteLater();
                                      return false;
                                  }
                                  effect->setOpacity(1.0 - std::clamp(progress, 0.0, 1.0));
                                  return true;
                              });
}

// The cross of a snackbar's close button: 24 in the middle of its 48, in the color of the text, with a press shown.
void QMaterialStyle::drawSnackbarClose(QPainter *painter, const QAbstractButton *button) const {
    const QColor ink = color(Role::InverseOnSurface);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    // An icon button of 48: a state layer of 40 under the pointer, the press or the keyboard focus, and the focus ring
    // about it.
    const QRectF bounds(button->rect());
    const double side = std::min(bounds.width(), bounds.height());
    const QPointF centre = bounds.center();
    const QRectF area(centre.x() - side / 2, centre.y() - side / 2, side, side);
    const bool focused = button->hasFocus() && m_keyboard;
    const double layer = !button->isEnabled()   ? 0
                         : button->isDown()     ? pressedOpacity
                         : focused              ? focusOpacity
                         : button->underMouse() ? hoverOpacity
                                                : 0;
    if (layer > 0)
        fillRounded(painter, area.adjusted(4, 4, -4, -4), (side - 8) / 2, withOpacity(ink, layer));
    if (focused)
        strokeRounded(painter, area, side / 2, color(Role::Secondary), focusRingWidth);
    painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::RoundCap));
    const double arm = 6;
    painter->drawLine(centre + QPointF(-arm, -arm), centre + QPointF(arm, arm));
    painter->drawLine(centre + QPointF(-arm, arm), centre + QPointF(arm, -arm));
    painter->restore();
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
    if (const QString kind = widget ? widget->property("materialProgress").toString() : QString();
        kind == QLatin1String("circular") || kind == QLatin1String("circularWavy")) {
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
        // In a grouped menu the separator is the gap between two containers, which the panel leaves out.
        if (!isGroupedMenu(menu))
            painter->fillRect(QRect(r.left(), r.center().y(), r.width(), 1), color(Role::OutlineVariant));
        return;
    }
    if (isMenuSection(menu, item)) {
        // A section is a group label: no line, its text in label medium, as far in as the text of the rows.
        // An icon goes before the text, in the same color.
        int left = r.left() + menuPadding;
        if (!item->icon.isNull()) {
            paintIcon(
                painter,
                QRect(left, r.top() + (r.height() - menuSectionIconSize) / 2, menuSectionIconSize, menuSectionIconSize),
                item->icon, QIcon::Normal, QIcon::Off, color(Role::OnSurfaceVariant));
            left += menuSectionIconSize + menuSectionIconSpace;
        }
        painter->setFont(menuGroupLabelFont(item->font));
        painter->setPen(color(Role::OnSurfaceVariant));
        painter->drawText(QRect(left, r.top(), r.right() + 1 - menuPadding - left, r.height()),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, menuLabel(item->text));
        return;
    }
    const bool usable = item->state & State_Enabled;
    const bool checkable = item->checkType != QStyleOptionMenuItem::NotCheckable;
    const bool checked = checkable && item->checked;
    // A row has a shape of its own, inset in the container: small corners, but at an end of the menu as round as the
    // container's less the inset, and a checked row round all over. The highlight and the ripple fill it.
    double top = menuRowCorner, bottom = menuRowCorner;
    if (checked) {
        top = bottom = menuSelectedCorner;
    } else if (const auto *list = qobject_cast<const QMenu *>(menu)) {
        if (!list->actionAt(QPoint(r.center().x(), r.top() - 1)))
            top = menuSelectedCorner;
        if (!list->actionAt(QPoint(r.center().x(), r.bottom() + 1)))
            bottom = menuSelectedCorner;
    }
    const QPainterPath outline = endedPath(QRectF(r), top, bottom);
    const QColor onRow = checked ? color(Role::OnTertiaryContainer) : color(Role::OnSurface);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (checked)
        painter->fillPath(outline, QBrush(usable ? color(Role::TertiaryContainer)
                                                 : withOpacity(color(Role::OnSurface), disabledContainerOpacity)));
    if (usable && (item->state & State_Selected))
        painter->fillPath(outline, QBrush(withOpacity(onRow, hoverOpacity)));
    if (usable && menu && menu == m_rippleWidget.data() && r.contains(m_rippleOrigin.toPoint()))
        drawRipple(painter, menu, outline, QRectF(r), m_rippleOrigin, m_rowPressed, onRow);
    painter->restore();
    const QColor ink = usable ? onRow : withOpacity(color(Role::OnSurface), disabledContentOpacity);
    const QColor quiet = usable && !checked ? color(Role::OnSurfaceVariant) : ink;
    int left = r.left() + menuPadding;
    const QRect lead(left, r.top() + (r.height() - menuIconSize) / 2, menuIconSize, menuIconSize);
    if (checked) {
        painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawLine(QPointF(lead.left() + 5, lead.top() + 12.5), QPointF(lead.left() + 10, lead.top() + 17.5));
        painter->drawLine(QPointF(lead.left() + 10, lead.top() + 17.5), QPointF(lead.left() + 19, lead.top() + 7.5));
    } else if (isMenuBackRow(menu, r)) {
        // The row that goes back from a submenu shown in place: Material's arrow back, in the color of the label.
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(QBrush(ink), 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        const QPointF tip(lead.left() + 5, lead.top() + 12);
        painter->drawLine(tip, QPointF(lead.left() + 19, lead.top() + 12));
        painter->drawLine(QPointF(lead.left() + 11, lead.top() + 6), tip);
        painter->drawLine(tip, QPointF(lead.left() + 11, lead.top() + 18));
        painter->restore();
    } else if (!item->icon.isNull()) {
        paintIcon(painter, lead, item->icon, usable ? QIcon::Normal : QIcon::Disabled, QIcon::Off, quiet);
    }
    if (checkable || !item->icon.isNull())
        left += menuIconSize + menuIconSpace;
    const MenuRowParts parts = menuRowParts(item->text);
    const QRect room(left, r.top(), r.right() - menuPadding - left + 1, r.height());
    painter->setPen(ink);
    if (parts.supporting.isEmpty()) {
        painter->drawText(room, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, parts.label);
    } else {
        // The label over its supporting text, in body medium, the two in the middle of the row as a list's are.
        const QFont supportingFont = listRowFont(item->font, 14, false);
        const int labelHeight = std::max(listHeadlineLine, QFontMetrics(item->font).height());
        const int lineHeight = std::max(listSupportingLine, QFontMetrics(supportingFont).height());
        const QStringList lines =
            wrapListText(parts.supporting, QFontMetrics(supportingFont), menuSupportingWidth(r.width(), item, menu));
        int y = r.top() + (r.height() - labelHeight - static_cast<int>(lines.size()) * lineHeight) / 2;
        painter->drawText(QRect(room.left(), y, room.width(), labelHeight),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, parts.label);
        y += labelHeight;
        painter->save();
        painter->setFont(supportingFont);
        painter->setPen(quiet);
        for (const QString &line : lines) {
            painter->drawText(QRect(room.left(), y, room.width(), lineHeight),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, line);
            y += lineHeight;
        }
        painter->restore();
    }
    const bool submenu = item->menuItemType == QStyleOptionMenuItem::SubMenu;
    // What is at the trailing end, from the edge in: the arrow of a submenu, the trailing icon, the shortcut.
    int trailing = r.right() + 1 - menuPadding - (submenu ? menuPadding : 0);
    if (const QIcon icon = menuTrailingIcon(menu, r); !icon.isNull()) {
        paintIcon(painter,
                  QRect(trailing - menuIconSize, r.top() + (r.height() - menuIconSize) / 2, menuIconSize, menuIconSize),
                  icon, usable ? QIcon::Normal : QIcon::Disabled, QIcon::Off, quiet);
        trailing -= menuIconSize + menuIconSpace;
    }
    if (!parts.shortcut.isEmpty()) {
        painter->setPen(quiet);
        painter->drawText(QRect(r.left(), r.top(), trailing - r.left(), r.height()),
                          Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, parts.shortcut);
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
        // A menu is drawn in its window, which shows at its round corners and in the gaps between its groups.
        for (const MenuContainer &container : menuContainers(widget, QRectF(option->rect))) {
            painter->fillPath(endedPath(container.rect, container.top, container.bottom),
                              QBrush(color(Role::SurfaceContainer)));
            if (element == PE_FrameMenu)
                continue;
            // The outline lies inside the container.
            painter->setPen(QPen(QBrush(color(Role::OutlineVariant)), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(
                endedPath(container.rect.adjusted(0.5, 0.5, -0.5, -0.5), container.top - 0.5, container.bottom - 0.5));
        }
        break;
    case PE_PanelTipLabel:
        painter->setRenderHint(QPainter::Antialiasing);
        // A rich tooltip, which QToolTip's label says it is: a surface container at level 2, its shadow in the 2 that
        // the label keeps below it.
        if (widget && widget->property(richToolTipProperty).toBool()) {
            const QRectF body = QRectF(option->rect).adjusted(0, 0, 0, -richToolTipShadow);
            fillRounded(painter, body.adjusted(0.5, richToolTipShadow, -0.5, richToolTipShadow), richToolTipCorner,
                        withOpacity(QColor(Qt::black), 0.18));
            fillRounded(painter, body, richToolTipCorner, color(Role::SurfaceContainer));
            break;
        }
        // A plain tooltip: dark on a light scheme and light on a dark one.
        fillRounded(painter, QRectF(option->rect), snackbarCorner, color(Role::InverseSurface));
        break;
    case PE_FrameTabWidget:
        break; // the pages of a tab widget have no frame: the line under the tabs parts them
    case PE_PanelLineEdit: {
        // The line edit inside a spin box lies on the field that the spin box draws.
        if (widget && qobject_cast<const QAbstractSpinBox *>(widget->parentWidget()))
            break;
        const auto *frame =
            option->type == QStyleOption::SO_Frame ? static_cast<const QStyleOptionFrame *>(option) : nullptr;
        const TextField field = textFieldOf(widget);
        const QRectF box(field.box(option->rect));
        if (field.search) {
            painter->fillPath(searchBarShape(box, widget), QBrush(color(Role::SurfaceContainerHigh)));
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
            if (focused && m_keyboard) {
                const double half = focusRingWidth / 2.0;
                painter->setPen(QPen(QBrush(color(Role::Secondary)), focusRingWidth));
                painter->setBrush(Qt::NoBrush);
                painter->drawPath(searchBarShape(box.adjusted(half, half, -half, -half), widget));
            }
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
        strokeOutlinedField(painter, QRectF(option->rect), field, widget, line, focused);
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
    if (element == CE_SpinBox && isTimeInput(widget)) {
        drawTimeInput(painter, option, static_cast<const QDateTimeEdit *>(widget));
        return;
    }
    if (element == CE_SpinBox && option->type == QStyleOption::SO_SpinBox) {
        drawSpinBox(option, painter, widget);
        return;
    }
    if (element == CE_GroupBox && option->type == QStyleOption::SO_GroupBox) {
        drawGroupBox(option, painter, widget);
        return;
    }
    if (element == CE_ScrollBar && option->type == QStyleOption::SO_Slider) {
        drawScrollBar(option, painter, widget);
        return;
    }
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
    // The line edit of a time input has the keys, and nothing to show; there are no buttons that step.
    if ((element == SE_SpinBoxEditField || element == SE_SpinBoxUp || element == SE_SpinBoxDown) && isTimeInput(widget))
        return QRect(r.left(), r.top(), 0, 0);
    if (element == SE_SpinBoxEditField || element == SE_SpinBoxUp || element == SE_SpinBoxDown) {
        const bool none =
            option->type == QStyleOption::SO_SpinBox &&
            static_cast<const QStyleOptionSpinBox *>(option)->buttonSymbols == QAbstractSpinBox::NoButtons;
        const int buttons = none ? 0 : opensCalendar(widget) ? 1 : 2;
        const int width = std::min(spinButton, r.width() / 3);
        // A label on the outline takes room over the field.
        const QRect box = textFieldOf(widget).box(r);
        if (element == SE_SpinBoxEditField)
            return box.adjusted(std::min(textFieldPadding, box.width() / 8), 3,
                                -(buttons ? buttons * width : textFieldPadding), -3);
        // Up is at the end, and down before it.
        const int from = element == SE_SpinBoxUp ? 1 : 2;
        return from > buttons ? QRect()
                              : QRect(box.left() + box.width() - from * width, box.top(), width, box.height());
    }
    if ((element == SE_GroupBoxTitle || element == SE_GroupBoxIndicator || element == SE_GroupBoxContents) &&
        option->type == QStyleOption::SO_GroupBox) {
        const auto *box = static_cast<const QStyleOptionGroupBox *>(option);
        const int height = groupTitleHeight(box->checkable);
        if (element == SE_GroupBoxContents)
            return r.adjusted(1, height + groupTitleGap + 1, -1, -1);
        const QFontMetrics metrics(groupTitleFont(widget ? widget->font() : QFont()));
        const int indicator = box->checkable ? choiceCentre - checkBoxSize / 2 + checkBoxSize + indicatorSpace : 4;
        const int width = std::min(r.width(), indicator + metrics.horizontalAdvance(box->text) + 8);
        int left = r.left();
        if (box->textAlignment & Qt::AlignRight)
            left = r.left() + r.width() - width;
        else if (box->textAlignment & Qt::AlignHCenter)
            left = r.left() + (r.width() - width) / 2;
        if (element == SE_GroupBoxTitle)
            return QRect(left, r.top(), width, height);
        // The check box has the room around it that shows a press.
        return QRect(left + choiceCentre - checkBoxSize / 2, r.top() + (height - checkBoxSize) / 2, checkBoxSize,
                     checkBoxSize);
    }
    switch (element) {
    case SE_ItemViewItemCheckIndicator:
        // At the trailing end of a row of a list.
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option);
            item && isList(widget) &&
            (item->decorationPosition == QStyleOptionViewItem::Left ||
             item->decorationPosition == QStyleOptionViewItem::Right))
            return listCheckRect(r, item->text, widget);
        break;
    case SE_PushButtonContents:
        // The open FAB is a circle at the right of the whole button. A button with a size is made as wide as its label
        // needs with the room of that size, so its label has the whole of it.
        if (isFab(widget) || sizeOfButton(widget) || isIconButton(widget) || isChip(widget))
            return r;
        // A narrow button keeps what room it has for its label.
        return r.adjusted(std::min(buttonPadding, r.width() / 4), 0, -std::min(buttonPadding, r.width() / 4), 0);
    case SE_SliderGroove: {
        // Across the middle of what is not kept for the value indicator.
        const QRect body = sliderBody(option, widget);
        const int track = sizeOfSlider(widget).track;
        if (factsOfSlider(option, widget).horizontal)
            return QRect(body.left(), body.top() + (body.height() - track) / 2, body.width(), track);
        return QRect(body.left() + (body.width() - track) / 2, body.top(), track, body.height());
    }
    case SE_SliderHandle: {
        // Where a press takes hold of the handle: as long across the track as the handle is, centred on the track.
        const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (!slider)
            break;
        const QRect groove = proxy()->subElementRect(SE_SliderGroove, option, widget);
        const int length = proxy()->pixelMetric(PM_SliderLength, option, widget);
        const SliderSize &size = sizeOfSlider(widget);
        const int across = size.handle;
        const int beside = (size.track - across) / 2; // where the handle begins across, from the groove's edge
        const qint64 range = static_cast<qint64>(slider->maximum) - slider->minimum;
        qint64 done = std::clamp<qint64>(static_cast<qint64>(slider->sliderPosition) - slider->minimum, 0, range);
        if (slider->upsideDown)
            done = range - done;
        if (slider->orientation == Qt::Horizontal) {
            const int span = std::max(0, groove.width() - length);
            const int x = range > 0 ? static_cast<int>(done * span / range) : 0;
            return QRect(groove.left() + x, groove.top() + beside, length, across);
        }
        // The smallest value of a vertical slider is at the bottom.
        const int span = std::max(0, groove.height() - length);
        const int y = range > 0 ? static_cast<int>(done * span / range) : 0;
        return QRect(groove.left() + beside, groove.bottom() + 1 - length - y, across, length);
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
    // A navigation rail scrolls its destinations; it has no scroll buttons.
    if (hint == SH_TabBar_PreferNoArrows && isNavigationRail(widget))
        return 1;
    return QCommonStyle::styleHint(hint, option, widget, returnData);
}

/*!
    \reimp
*/
int QMaterialStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const {
    switch (metric) {
    case PM_LayoutLeftMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutBottomMargin:
        // A dialog keeps what is in it 24 from its edges, clear of its round corners.
        if (qobject_cast<const QDialog *>(widget) && widget->isWindow())
            return dialogPadding;
        // A group box keeps its title over the card, and room around what is in the card.
        if (const auto *group = qobject_cast<const QGroupBox *>(widget)) {
            if (metric != PM_LayoutTopMargin)
                return groupPadding;
            return groupTitleHeight(group->isCheckable()) + groupTitleGap + groupPadding;
        }
        return QCommonStyle::pixelMetric(metric, option, widget);
    // The rows of a menu are inset in its container; a FAB menu has no container.
    case PM_MenuHMargin:
        return qobject_cast<const QMenu *>(widget) && !isFabMenu(widget) ? menuInset : 0;
    case PM_MenuVMargin:
        return qobject_cast<const QMenu *>(widget) && !isFabMenu(widget) ? menuVerticalPadding : 0;
    case PM_ScrollBarExtent:
        return scrollBarWidth;
    // The icons at the ends of a text field: 24, 12 from its edges, and 16 from the text.
    case PM_LineEditIconSize:
        return fieldIcon;
    case PM_LineEditIconMargin:
        // In a search bar, targets of 48.
        return widget && textFieldOf(widget).search ? searchIconTarget - fieldIcon : fieldIconMargin;
    case PM_TextCursorWidth:
        return textCursorWidth;
    case PM_DefaultFrameWidth:
        if (isTextEditor(widget))
            return editorFrame;
        return QCommonStyle::pixelMetric(metric, option, widget);
    case PM_ScrollBarSliderMin:
        return scrollThumbLeast;
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
        // The handle of the slider's size, and the room for its value indicator if it shows one.
        return sizeOfSlider(widget).handle + sliderValueRoom(option, widget);
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
    if (type == CT_SpinBox && isTimeInput(widget)) {
        const std::vector<QRect> rects = timeInputRects(static_cast<const QDateTimeEdit *>(widget));
        return QSize(rects.empty() ? 0 : rects.back().x() + rects.back().width(), timeBoxHeight + timeCaption);
    }
    if (type == CT_SpinBox && option && option->type == QStyleOption::SO_SpinBox) {
        const int symbols = static_cast<const QStyleOptionSpinBox *>(option)->buttonSymbols;
        const int buttons = symbols == QAbstractSpinBox::NoButtons ? 0 : opensCalendar(widget) ? 1 : 2;
        // Room for the cursor besides the text.
        return QSize(contentsSize.width() + textFieldPadding + 8 + (buttons ? buttons * spinButton : textFieldPadding),
                     std::max(textFieldHeight, contentsSize.height()) + textFieldOf(widget).above);
    }
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
        // What a card shows is usually the widgets of its layout, which a push button does not ask about.
        if (!buttonCardKind(widget).isEmpty() && widget->layout()) {
            const QSize laid = widget->layout()->sizeHint();
            return QSize(std::max(laid.width(), width + 2 * buttonPadding), std::max(laid.height(), height));
        }
        if (isIconButton(widget))
            return iconButtonExtent(widget);
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
            return iconButtonExtent(widget);
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
        // A grouped menu's separator is a gap, with the padding of the groups on either side of it.
        if (item->menuItemType == QStyleOptionMenuItem::Separator)
            return QSize(menuMinimumWidth - 2 * menuInset,
                         isGroupedMenu(widget) ? 2 * menuVerticalPadding + menuGroupGap : menuSeparator);
        // A section is a group label, lower than a row.
        if (isMenuSection(widget, item))
            return QSize(
                std::max(menuMinimumWidth - 2 * menuInset,
                         QFontMetrics(menuGroupLabelFont(item->font)).horizontalAdvance(menuLabel(item->text)) +
                             2 * menuPadding + (item->icon.isNull() ? 0 : menuSectionIconSize + menuSectionIconSpace)),
                menuGroupLabelHeight);
        int textWidth = contentsSize.width();
        int height = std::max(menuItemHeight, contentsSize.height());
        if (const MenuRowParts parts = menuRowParts(item->text); !parts.supporting.isEmpty()) {
            // A row with supporting text is as wide as its widest line, which wraps to the widest a menu is, and grows
            // by a line of 20 for each.
            const QFontMetrics label(item->font);
            const QFontMetrics supporting(listRowFont(item->font, 14, false));
            const QStringList lines =
                wrapListText(parts.supporting, supporting, menuSupportingWidth(menuMaximumWidth, item, widget));
            textWidth = label.horizontalAdvance(parts.label);
            for (const QString &line : lines)
                textWidth = std::max(textWidth, supporting.horizontalAdvance(line));
            height = std::max(menuItemHeight,
                              2 * menuSupportingPadding + std::max(listHeadlineLine, label.height()) +
                                  static_cast<int>(lines.size()) * std::max(listSupportingLine, supporting.height()));
        }
        int width = textWidth + 2 * menuPadding;
        if (!item->icon.isNull() || item->checkType != QStyleOptionMenuItem::NotCheckable)
            width += menuIconSize + menuIconSpace;
        if (item->text.contains(QLatin1Char('\t')))
            width += item->reservedShortcutWidth + 2 * menuPadding;
        if (item->menuItemType == QStyleOptionMenuItem::SubMenu)
            width += menuPadding;
        if (hasMenuTrailingIcons(widget))
            width += menuIconSize + menuIconSpace;
        return QSize(std::max(width, menuMinimumWidth - 2 * menuInset), height);
    }
    case CT_ProgressBar:
        if (loadingOf(widget) != Loading::No)
            return QSize(loadingSize, loadingSize);
        if (widget && widget->property("materialProgress").toString() == QLatin1String("circular"))
            return QSize(circularSize, circularSize);
        if (widget && widget->property("materialProgress").toString() == QLatin1String("circularWavy"))
            return QSize(circularWavySize, circularWavySize);
        if (widget && widget->property("materialProgress").toString() == QLatin1String("wavy"))
            return QSize(contentsSize.width(), std::max(wavyProgressHeight, contentsSize.height()));
        return QSize(contentsSize.width(), std::max(progressHeight, contentsSize.height()));
    case CT_TabBarTab:
        if (isExpandedRail(widget)) {
            // Wide enough for the longest destination and its count.
            const auto *bar = static_cast<const QTabBar *>(widget);
            const QFontMetrics metrics(bar->font());
            const QVariantList badges = bar->property("materialBadges").toList();
            int widest = 0;
            for (int i = 0; i < bar->count(); ++i) {
                int width = metrics.horizontalAdvance(bar->tabText(i));
                if (!bar->tabIcon(i).isNull())
                    width += navigationIcon + railExpandedIconSpace;
                bool dot = false;
                const QString count = i < badges.size() ? badgeText(badges.at(i), &dot) : QString();
                if (!count.isEmpty())
                    width += railExpandedIconSpace + badgeWidth(bar->font(), count);
                widest = std::max(widest, width);
            }
            return QSize(std::clamp(widest + 2 * (railExpandedInset + railExpandedPadding), railExpandedMinimum,
                                    railExpandedMaximum),
                         railExpandedItem);
        }
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
