// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qpointer.h>
#include <QtGui/qcolor.h>
#include <QtGui/qicon.h>
#include <QtWidgets/qcommonstyle.h>

#include <memory>
#include <vector>

namespace QtPrivate {
class Animator;
struct SpringSpec;
} // namespace QtPrivate

class QAbstractScrollArea;
class QAction;
class QFont;
class QCalendarWidget;
class QAbstractButton;
class QDate;
class QDateTimeEdit;
class QDialog;
class QTime;
class QFrame;
class QLineEdit;
class QListWidget;
class QLocale;
class QMainWindow;
class QMessageBox;
class QStatusBar;
class QToolBar;
class QStyleOptionButton;
class QMenu;
class QPushButton;
class QToolButton;
class QPainterPath;
class QStyledItemDelegate;
class QTabBar;

// The Material 3 Expressive look. Nt only: Qt Widgets has no such style.
class QMaterialStyle : public QCommonStyle {
    Q_OBJECT
public:
    // The colour roles of Material 3.
    enum class Role {
        Primary,
        OnPrimary,
        PrimaryContainer,
        OnPrimaryContainer,
        Secondary,
        OnSecondary,
        SecondaryContainer,
        OnSecondaryContainer,
        Tertiary,
        OnTertiary,
        TertiaryContainer,
        OnTertiaryContainer,
        Error,
        OnError,
        ErrorContainer,
        OnErrorContainer,
        Surface,
        OnSurface,
        SurfaceVariant,
        OnSurfaceVariant,
        SurfaceContainerLowest,
        SurfaceContainerLow,
        SurfaceContainer,
        SurfaceContainerHigh,
        SurfaceContainerHighest,
        Outline,
        OutlineVariant,
        InverseSurface,
        InverseOnSurface,
        InversePrimary
    };
    Q_ENUM(Role)

    QMaterialStyle();
    explicit QMaterialStyle(const QColor &seedColor);
    ~QMaterialStyle() override;

    // The color that the system's own theme is made from, on Android 12 and later; invalid elsewhere.
    static QColor systemSeedColor();
    QColor seedColor() const { return m_seed; }
    void setSeedColor(const QColor &color);
    Qt::ColorScheme colorScheme() const { return m_scheme; }
    void setColorScheme(Qt::ColorScheme scheme);
    bool isDark() const;
    QColor color(Role role) const;

    bool isMotionEnabled() const { return m_motion; }
    void setMotionEnabled(bool enabled);

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                           const QWidget *widget = nullptr) const override;
    QPalette standardPalette() const override;
    QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option = nullptr,
                           const QWidget *widget = nullptr) const override;
    using QCommonStyle::polish;
    using QCommonStyle::unpolish;
    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    void polish(QApplication *application) override;
    void unpolish(QApplication *application) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class ButtonKind {
        Filled,
        Tonal,
        Outlined,
        Text,
        Elevated,
        Fab,
        ToolBarItem,
        IconStandard,
        IconFilled,
        IconTonal,
        IconOutlined,
        Chip,
        Card // a button that is a card, which can be clicked
    };
    static ButtonKind buttonKind(const QStyleOption *option, const QWidget *widget);
    void drawButtonContainer(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    // The container of a button, or of one half of a split button, with its corners at the left and the right.
    void drawButtonShape(const QStyleOption *option, QPainter *painter, const QWidget *widget, double left,
                         double right) const;
    void drawButtonLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawFabLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawChipLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    // A QToolButton with MenuButtonPopup is a split button. It paints its label itself, in the color returned.
    friend class QToolButton;
    QColor drawSplitButton(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget,
                           const QRect &trailing, bool open) const;
    // Opens the menu of a floating action button as Material's FAB menu, above it. False for another button, whose
    // menu is left to open as menus do.
    friend class QPushButton;
    bool popupFabMenu(QPushButton *button, QMenu *menu) const;
    // A vertical menu (materialMenu "vertical") opens its submenus in its own place: QMenu asks whether `menu` is one.
    friend class QMenu;
    static bool opensSubMenusInPlace(const QWidget *menu);
    // How far a menu keeps from the edges of its window when it is placed: none for a FAB menu or another style's.
    static QMargins menuWindowMargins(const QWidget *menu);
    // A FAB menu that closed leaves a picture of its items in its place, which go one after another.
    void closeFabMenu(QMenu *menu) const;
    // A floating toolbar. QToolBar lays itself out for one and asks for its container; its buttons ask for the color
    // of their labels (as does another tool button with a kind or a size); for any other button the color is invalid.
    // A text field: its container, label and the line under it. QLineEdit draws its placeholder itself, and asks if
    // the resting label stands where it would.
    friend class QLineEdit;
    bool hidesPlaceholder(const QWidget *field) const;
    // The text cursor of a field, in Primary, or in Error while the field shows an error. QPlainTextEdit and QTextEdit
    // ask too, and also draw the label of a multi-line field where it lies over their viewport (its resting place),
    // and ask if the field grows with its text: they then give the height of their lines as their size hint.
    QColor caretColor(const QWidget *field) const;
    friend class QPlainTextEdit;
    friend class QTextEdit;
    void drawEditorLabel(QPainter *painter, const QAbstractScrollArea *editor) const;
    static bool growsWithContent(const QWidget *editor);
    void polishEditorField(QAbstractScrollArea *editor) const;
    // The prefix or the suffix of a text field, while it shows: when the label has floated, or there is none.
    QString fieldAffix(const QLineEdit *field, bool suffix) const;
    // The icon of an action at an end of a text field, in `area`, in the color of the field's icons, or the picture of
    // an avatar; the actions are standard icon buttons, with a circle of 40 under the pointer and while held down
    // (`hovered`, `pressed`) and the ripple of a press anywhere in their `slot`.
    void drawFieldIcon(QPainter *painter, const QRect &slot, const QRect &area, const QAction *action, QIcon::Mode mode,
                       const QLineEdit *field, bool trailing, bool hovered, bool pressed) const;
    // The cross of a clear button, in lines, which stay sharp on a screen of any scale; an icon button too.
    void drawClearButton(QPainter *painter, const QRect &slot, const QRect &area, const QLineEdit *field, bool hovered,
                         bool pressed) const;
    void drawFieldIconLayer(QPainter *painter, const QRect &slot, const QRect &area, const QLineEdit *field,
                            bool hovered, bool pressed) const;
    // How far the icons of a field go out of its area for text, toward the edges, and how far an icon is in from the
    // outer side of its slot: a search bar has targets of 48 with the icon in the middle.
    int fieldIconShift(const QLineEdit *field) const;
    int fieldIconInset(const QLineEdit *field) const;
    // An action whose materialAvatar is true is a round picture of 30, at the far end of the trailing actions.
    static bool isFieldAvatar(const QAction *action);
    // An icon button of a field is pressed at `at`: its ripple starts there.
    void pressFieldIcon(QLineEdit *field, const QPointF &at);
    void drawTextFieldText(QPainter *painter, const QStyleOption *option, const QWidget *widget) const;
    // A property of the style's that a widget was given, or lost, while it is shown.
    void propertyChanged(QWidget *widget);
    // A QDateTimeEdit that is Material's time input: large boxes for the hour and the minute. The style lays them out
    // and draws them from the text of the line edit, which keeps the focus but has no size; QDateTimeEdit asks which
    // box was clicked.
    friend class QDateTimeEdit;
    static bool isTimeInput(const QWidget *widget);
    // The rectangle of each section of the time, in their order; an empty one for a section that has none.
    static std::vector<QRect> timeInputRects(const QDateTimeEdit *edit);
    void drawTimeInput(QPainter *painter, const QStyleOption *option, const QDateTimeEdit *edit) const;
    // With materialTimeInput "dial" it is Material's time picker: a headline, the boxes and a dial under them, all in
    // the widget. QDateTimeEdit asks whether a point is on the dial, what time a press or a drag there gives, and
    // whether the dial shows the hours (to go on to the minutes when it is let go).
    static bool isTimeDial(const QWidget *widget);
    static QRect timeDialRect(const QDateTimeEdit *edit);
    static bool timeDialHours(const QDateTimeEdit *edit);
    static bool onTimeDial(const QDateTimeEdit *edit, const QPoint &pos);
    static QTime timeDialTime(const QDateTimeEdit *edit, const QPoint &pos);
    void drawTimeDial(QPainter *painter, const QStyleOption *option, const QDateTimeEdit *edit) const;
    // A day of a calendar, as the date picker of Material shows it: QCalendarWidget::paintCell() asks for it. The
    // text is in `ink` if the program formatted the day with a color.
    friend class QCalendarWidget;
    void drawCalendarDay(QPainter *painter, const QRect &cell, const QCalendarWidget *calendar, int day, bool selected,
                         bool today, bool usable, const QColor &ink) const;
    // A calendar whose materialDatePicker is "modal" has the headline of Material's modal date picker above the month:
    // QCalendarWidget makes room for it, as high as this (0 for another calendar), and asks for it to be drawn. At
    // the end of the row of the date is the icon button that switches to the input mode, a pencil, or back to the
    // days, a calendar, while `input` is true: a standard icon button child of the calendar, put where
    // datePickerModeButton() says, which draws its icon in its `button` rectangle over its state layer. QCalendarWidget
    // also asks where the text field of the input mode goes in the room under the headline, for a field as high as
    // `fieldHeight`, and how high the picker is in the input mode, which has only the headline and the field.
    static int datePickerHeadlineHeight(const QWidget *calendar);
    static QRect datePickerModeButton(const QRect &headline);
    static QRect datePickerField(const QRect &room, int fieldHeight);
    static int datePickerInputHeight(int fieldHeight);
    void drawDatePickerHeadline(QPainter *painter, const QRect &rect, const QCalendarWidget *calendar,
                                bool input) const;
    void drawDatePickerModeIcon(QPainter *painter, const QRect &button, const QWidget *widget, bool input) const;
    // The month and the year of a calendar, with the arrow of the menu of years, which is `open` while the years are
    // shown in the place of the days; QCalendarWidget paints its label with it, and lays out the grid of years and asks
    // for each, in its cell.
    void drawCalendarTitle(QPainter *painter, const QRect &rect, const QWidget *label, const QString &text,
                           bool open) const;
    // A year has a state layer while it is `hovered` or `pressed`. How far the years have faded in over the days, on
    // their way to 1 while `choosing` a year and to 0 otherwise, is asked as the calendar paints.
    void drawCalendarYear(QPainter *painter, const QRect &cell, const QCalendarWidget *calendar, int year,
                          bool selected, bool current, bool cursor, bool hovered, bool pressed) const;
    double datePickerYearsShown(const QCalendarWidget *calendar, bool choosing) const;
    // A row of the menu of months of the docked date picker (materialDatePicker "docked"), which shows them in the
    // place of the days as the years are: the month shown is `checked`, a month out of the range is not `usable`.
    // The years and the months share the fade of datePickerYearsShown().
    void drawCalendarMonth(QPainter *painter, const QRect &row, const QCalendarWidget *calendar, const QString &name,
                           bool checked, bool usable, bool cursor, bool hovered, bool pressed) const;
    // The date of the headline in the names and the order of the locale.
    static QString datePickerHeadlineText(QDate date, const QLocale &locale);
    // A dialog that is a sheet against an edge of its parent's window: QDialog asks for it to be put there when it
    // is shown. False for another dialog.
    friend class QDialog;
    bool placeSheet(QDialog *dialog) const;
    // A sheet slides in from its edge, or from `from` (a fraction of its height or width away) when it was on its way
    // out, to `rest` from its expanded `place` (a sheet that rests collapsed or half open).
    void slideSheetIn(QDialog *dialog, const QRect &place, bool fromLeft, double from, int rest = 0) const;
    // The standard sheets of a window that moved or changed its size go to its bottom again.
    void followWindow(QWidget *window) const;
    // A sheet that is hidden leaves a picture of itself that slides back to its edge: QDialog asks for it just before
    // it hides the dialog, which goes at once as any other dialog does.
    void slideSheetOut(QDialog *dialog) const;
    // The scrim behind a dialog fades in from `from`; the android platform reads it from the window.
    void fadeScrim(QDialog *dialog, double from) const;
    // Where dialogs have no title bar, a dialog shows its window title as Material's headline: room is made for it
    // above what is in the dialog, before it is shown, and QDialog asks for it to be drawn there.
    // A bottom sheet has a drag handle at its top in the place of a headline, room made for it in the same way, and
    // QDialog asks for both in the same call.
    void prepareHeadline(QDialog *dialog) const;
    void drawHeadline(QPainter *painter, const QDialog *dialog) const;
    // The headline wraps to the width of the dialog: the top contents margin that the dialog has with its headline at
    // `width` (-1 without one), which QDialog's height for a width takes, and the margin set for the width it has,
    // which QDialog asks for when it is resized.
    int headlineTop(const QDialog *dialog, int width) const;
    void fitHeadline(QDialog *dialog) const;
    // The icon of the centered form, over the headline: the dialog's own window icon, or a message box's icon.
    static QIcon headlineIcon(const QDialog *dialog);
    static void removeHeadline(QWidget *widget);
    // A message box under a headline shows its icon over the headline rather than beside its text.
    friend class QMessageBox;
    static bool movesMessageIcon(const QMessageBox *box);
    // A sheet is dragged toward its edge by a press on its background, and closes if it is let go far enough or flung;
    // a modal sheet also closes on a press in a window that it blocks. True if the style took the event.
    bool sheetMouseEvent(QObject *watched, QEvent *event);
    // Puts a sheet `offset` from its place at `rest`, toward its edge, and fades its scrim with the distance.
    void dragSheetTo(QDialog *dialog, const QRect &rest, int offset) const;
    // The rows of a list that is a navigation drawer keep their icons and text further in.
    static bool isDrawerList(const QWidget *widget);
    // A frame that is a card, or a line that is a divider: QFrame asks for it. False for another frame.
    friend class QFrame;
    bool drawMaterialFrame(QPainter *painter, const QFrame *frame) const;
    friend class QToolBar;
    static bool isFloatingToolBar(const QWidget *widget);
    // A docked toolbar: across the whole of its side, its buttons spread out along it.
    static bool isDockedToolBar(const QWidget *widget);
    // A top app bar: QToolBar lays itself out for one and asks for its background and title.
    static bool isAppBar(const QWidget *widget);
    void drawAppBar(QPainter *painter, const QToolBar *bar) const;
    // A medium or large app bar, whose title is on a line below its buttons until it collapses as what is under it
    // scrolls, and how high it is now: QToolBar keeps its buttons in the top row and asks for that height.
    static bool isFlexibleAppBar(const QWidget *widget);
    // How high a top app bar or a docked toolbar is now, of `natural`, the height its buttons ask for: a medium or
    // large one as the scrolling leaves it, and less of it once it scrolls away (enterAlways or exitAlways).
    // toolBarRest() is the height without what has scrolled away, and toolBarAway() how far it has, from 0 to 1;
    // QToolBar lays its buttons out at the rest height, moved with the edge that leaves, while that is more than 0.
    int appBarExtent(const QToolBar *bar, int natural) const;
    int toolBarRest(const QToolBar *bar, int natural) const;
    double toolBarAway(const QToolBar *bar) const;
    static bool scrollsAway(const QWidget *widget);
    void drawFloatingToolBar(QPainter *painter, const QToolBar *bar) const;
    void drawDockedToolBar(QPainter *painter, const QToolBar *bar) const;
    QColor toolBarItemColor(const QStyleOption *option, const QWidget *widget) const;
    // An icon in `ink`, if it has one color of its own; one with several is drawn as it is.
    static void paintIcon(QPainter *painter, const QRect &rect, const QIcon &icon, QIcon::Mode mode, QIcon::State state,
                          const QColor &ink);
    // The width and height of the icon of an icon button; 0 for another button.
    static int iconButtonIconSize(const QWidget *widget);
    void drawGroupBox(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    // The border of a text editor, as that of an outlined text field.
    void drawEditorFrame(QPainter *painter, const QFrame *editor) const;
    void drawSpinBox(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawScrollBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawSwitch(const QStyleOption *option, const QRect &rect, QPainter *painter, const QWidget *widget) const;
    void drawSlider(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawSliderValue(const QStyleOption *option, QPainter *painter, const QWidget *widget, const QRectF &bar) const;
    void drawProgressBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawLoadingIndicator(const QStyleOption *option, QPainter *painter, const QWidget *widget,
                              bool contained) const;
    void drawTab(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawComboBox(const QStyleOption *option, QPainter *painter) const;
    void drawMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *menu) const;
    void drawFabMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *menu) const;
    QColor buttonLabelColor(const QStyleOption *option, const QWidget *widget) const;
    bool keyboardFocus(const QStyleOption *option) const;
    double stateLayer(const QStyleOption *option) const;
    // Where a value of a widget is on its way to `target`, along a spring; the target itself without motion.
    double animated(const QWidget *widget, int channel, double target, const QtPrivate::SpringSpec &spring,
                    double precision) const;
    void drawRipple(QPainter *painter, const QWidget *widget, const QPainterPath &outline, const QRectF &shape,
                    const QPointF &origin, bool held, const QColor &ink) const;
    // What a press, the pointer or the focus shows around the control of a check box, a radio button or a switch.
    void drawChoicePress(const QStyleOption *option, QPainter *painter, const QWidget *widget, const QPointF &centre,
                         const QColor &ink) const;
    // The background of a row of a list: its selection, and the press on it. The views that paint rows ask for it.
    friend class QListWidget;
    friend class QStyledItemDelegate;
    // In a list whose materialList is "segmented" each row is a container of its own, `first` and `last` rounder;
    // `index` is the number of the row, which keys the springs of its corners (-1 for none).
    void drawListRow(QPainter *painter, const QRect &row, bool selected, bool enabled, const QBrush &background,
                     const QWidget *view, bool first, bool last, int index = -1) const;
    static bool isSegmentedList(const QWidget *view);
    // The height of the rows of a QCompleter's popup: 56 in the docked search view under a search bar, 48 in the menu
    // under another line edit, and 0 for a list that is no completer's popup. QListWidget asks for it.
    static int completerRowHeight(const QWidget *list);
    // The text of a row of a completer's popup, with the part that matches what was typed (in any case) in OnSurface
    // and body large emphasized (medium), the rest in OnSurfaceVariant. False if nothing in it matches, or `view` is
    // no completer's popup: QListWidget then draws the text itself.
    bool drawCompletionText(QPainter *painter, const QRect &area, const QString &text, const QFont &font, bool enabled,
                            const QWidget *view) const;
    // Where QCompleter puts its `popup` for `widget`: `proposed` (under or over the widget), or in the full-screen
    // search view the whole width of the window under the view's header, `content` high at most.
    friend class QCompleter;
    static QRect completerPopupGeometry(const QWidget *popup, const QWidget *widget, const QRect &proposed,
                                        int content);
    // Whether QCompleter shows all its rows for an empty prefix under `widget`: in the full-screen search view, whose
    // empty field offers the recent or suggested searches of the model.
    static bool completerOffersAll(const QWidget *widget);
    // A search bar that gets the focus on a compact window (or with materialSearchView "fullScreen") opens the
    // full-screen search view over its window.
    void openFullScreenSearch(QLineEdit *edit) const;
    // The icon of a row, in `area`: one of one color is drawn in the color of the icons of a list, and those of a list
    // with materialListAvatars or materialListImages as pictures in their own colors, in a circle of 40 or a square of
    // 56, which listLeadingSize() gives (an invalid size for other lists).
    void drawRowIcon(QPainter *painter, const QRect &area, const QIcon &icon, QIcon::Mode mode, bool selected,
                     bool enabled, const QWidget *view) const;
    static QSize listLeadingSize(const QWidget *view);
    // A row whose text has a line break is a headline over supporting text, and a tab in its headline puts what follows
    // at the trailing end: its height (56, 72 or 88, or more for an icon `leading` high), where an icon of `height`
    // goes in it, the size of its text, and the drawing of that text in `area`, the row without its margins, icon and
    // check box. With materialListOverline on `view`, the first line of a row of three lines is an overline.
    static bool isMultiPartRow(const QString &text);
    static int listRowHeight(const QString &text, int leading = 0);
    static int listRowContentTop(const QRect &row, int height, const QString &text);
    static QSize listRowTextSize(const QString &text, const QFont &font, const QWidget *view);
    void drawListRowText(QPainter *painter, const QRect &area, const QString &text, const QFont &font,
                         const QColor &headlineInk, Qt::Alignment alignment, bool selected, bool enabled,
                         const QWidget *view) const;
    // The check box of a row is at its trailing end: the box in `row`, where a click toggles it, how much room it
    // takes from the text, and its drawing.
    static QRect listCheckRect(const QRect &row, const QString &text, const QWidget *view);
    static QRect listCheckTarget(const QRect &box, const QRect &row);
    static int listCheckRoom();
    void drawListCheck(QPainter *painter, const QRect &box, Qt::CheckState state, bool enabled,
                       const QWidget *view) const;
    // The line under the current tab of a horizontal tab bar. QTabBar paints a tab at a time, so it asks for the line,
    // which slides from tab to tab, once they are all painted.
    friend class QTabBar;
    void drawTabIndicator(QPainter *painter, const QTabBar *bar, const QRect &tab, bool below) const;
    // A status bar that is a snackbar: QMainWindow floats it over the content, QStatusBar asks for it to be drawn and
    // lays its actions out.
    friend class QMainWindow;
    friend class QStatusBar;
    static bool isSnackbar(const QWidget *widget);
    static bool isFloatingActionButton(const QWidget *widget); // which a snackbar goes above
    static int snackbarHeight(const QStatusBar *bar, int width);
    // A message that comes fades in; one that goes, or is replaced, fades out over what follows.
    void snackbarArrives(QStatusBar *bar) const;
    void drawSnackbarClose(QPainter *painter, const QAbstractButton *button) const;
    void snackbarLeaves(QStatusBar *bar) const;
    void drawSnackbar(QPainter *painter, const QStatusBar *bar) const;
    void drawCircularProgress(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    // A bottom navigation bar: QTabBar gives its tabs equal widths and asks for the whole of it to be drawn.
    static bool isNavigationBar(const QWidget *widget);
    // A navigation rail: the same at the side, its destinations one under another.
    static bool isNavigationRail(const QWidget *widget);
    // The expanded one, a rail too: its destinations are rows, the icon beside the label.
    static bool isExpandedRail(const QWidget *widget);
    // The modal expanded rail, an expanded rail too: QTabWidget lays it over its pages, which keep the room of the
    // collapsed rail (collapsedRailWidth()), with a scrim beside it.
    friend class QTabWidget;
    static bool isModalRail(const QWidget *widget);
    static int collapsedRailWidth();
    // A navigation bar whose destinations put the icon beside the label: QTabBar centres them instead of sharing its
    // width, each as wide as horizontalNavigationItemWidth().
    static bool hasHorizontalNavigationItems(const QWidget *widget);
    static int horizontalNavigationItemWidth(const QTabBar *bar);
    // The badge that a button or a destination has: a dot, or a few characters, at the top right of its icon.
    void drawBadge(QPainter *painter, const QVariant &badge, const QRect &icon) const;
    void drawButtonBadge(QPainter *painter, const QAbstractButton *button) const;
    // The destinations of a rail are clipped to below `top`, where a rail with a menu button or a FAB at its top (the
    // corner widget of its QTabWidget) scrolls them out of sight.
    void drawNavigationBar(QPainter *painter, const QTabBar *bar, int top = 0) const;
    void drawExpandedRail(QPainter *painter, const QTabBar *bar, int top) const;
    void drawHorizontalNavigationBar(QPainter *painter, const QTabBar *bar) const;

    QColor m_seed;
    Qt::ColorScheme m_scheme = Qt::ColorScheme::Unknown; // Unknown follows the system
    bool m_keyboard = false;                             // the focus was last moved with the keyboard
    bool m_motion = true;
    std::unique_ptr<QtPrivate::Animator> m_animator;
    QPointer<QWidget> m_rippleWidget; // the button that was pressed last, and where
    QPointF m_rippleOrigin;
    bool m_rowPressed = false; // a row of a list, a tab or a row of a menu is held down
    QPointer<QWidget> m_activatedWindow; // a window that was activated and is about to give the focus back
    struct SheetDrag;
    std::unique_ptr<SheetDrag> m_sheetDrag; // the sheet that is being dragged, while it is
};
