# Material 3 Expressive for Nt Widgets

Status: step 1, most of steps 2 and 3 (2026-11-26). Not part of 6.0.7.

## Goal

A widget style that gives a Qt Widgets application the Material 3 Expressive look, mainly for Android, without
changes to the application: `QApplication::setStyle("Material3")`.

## Decisions

- `QMaterialStyle` derives from `QCommonStyle`. What it does not draw itself is drawn like Fusion, in the Material
  palette, so every widget works from the first step.
- The colour engine (HCT: CAM16 hue and chroma with CIELAB L* as tone) is written from the published CAM16 and CIELAB
  formulas. Google's `material-color-utilities` is Apache-2.0 and none of its code is used. Nt stays MIT.
- Design values (numbers only) come from the token files of Jetpack Compose Material 3 (`androidx/androidx`,
  `compose/material3/.../tokens/`, Apache-2.0): spring constants, shapes, button sizes, state layer opacities, switch
  sizes and the tone of each colour role. The chromas of the tonal spot scheme (primary 36, secondary 16, tertiary
  24 at hue + 60, neutral 6, neutral variant 8) are those of `scheme_tonal_spot.cc`.
- Logical pixels are Android's dp: the android platform's device pixel ratio is the density over 160.
- Choices that Qt's API has no word for are dynamic properties, so that the same source builds with Qt:
  `materialButton` ("filled", "tonal", "outlined", "text", "elevated") and `materialSwitch` (a QCheckBox drawn as a
  switch). By default the default button is filled, a flat button is a text button, and others are tonal.

## Steps

1. Theme: colour roles, light and dark, from a seed colour. **Done.**
2. Style: buttons, text fields, check boxes, radio buttons, switches, sliders, progress bars, tabs, combo boxes,
   menus, dialogs, lists (**done**). Rows of `QListView` and `QListWidget` follow the Expressive list item: 56 high,
   16 at the ends, 12 between the parts, corners of 4, and of 16 when selected or pressed, in the secondary container
   colour. `QStyledItemDelegate` and `QListWidget` paint their rows themselves, so as friends they call the private
   `QMaterialStyle::drawListRow()`; a press is followed by the style's event filter on the viewport, and ends when
   it strays 8 pixels. Material has no tree or table: they get the selection colours (the view's palette, set in
   `polish()`), and tree rows are 48 high. The android platform shows a dialog over another window as a card
   over the dimmed window, and rounds its corners by the window property `_nt_cornerRadius` that the style sets.
3. Motion: springs (expressive scheme: spatial default damping 0.8, stiffness 380; fast 0.6, 800; slow 0.8, 200;
   effects damping 1.0 with stiffness 1600, 3800, 800) and shape morphing. **Mostly done:** `qmaterialmotion_p.h`
   solves the spring in closed form and repaints a widget until its springs rest. Button corners (pressed and
   checked), the switch handle, the radio dot and the slider handle use the fast spatial spring; the line under the
   tabs the default spatial spring; the switch and check box colours (and the check mark being drawn) the default
   effects spring. A button's ripple spreads from the press along the slow effects spring, stays while the button is
   held or until it has nearly filled it, then fades; it takes the place of the pressed state layer. `QTabBar` paints
   a tab at a time, so after the tabs it calls the private `QMaterialStyle::drawTabIndicator()` (a friend) to draw
   the one line. To do: fading of button, label and radio colours. Motion is kept per widget, so item views, which
   draw many indicators, do not move (the rows of a list ripple).
4. New components, Nt only: button groups, split button, FAB menu, floating toolbar, loading indicator.
   **Started:** the connected button group (small), which takes the place of segmented buttons, is the property
   `materialButtonGroup` = `"connected"` on the widget that holds the buttons: 2 between them (the style's
   `PM_LayoutHorizontalSpacing` for that widget), round ends, inner corners of 8 and of 4 while pressed, and a
   checked button is a pill. A button's place comes from the geometry of its siblings. The standard group's
   widening of a pressed button would move widgets against their layout, and is not drawn.
   **Done:** the split button (small) is a `QToolButton` in `MenuButtonPopup` mode, so it needs no new API: the part
   for the menu is 48 wide (`menuArrowRect()`), 2 from the other, and turns into a pill with its chevron turned over
   while the menu is open; `QToolButton` calls the private `drawSplitButton()`. The FAB is a `QPushButton` with
   `materialButton` = `"fab"`; a menu on it opens as the FAB menu, which `QPushButton::showMenu()` leaves to the
   private `popupFabMenu()`. The menu is marked with `_nt_fabMenu`: no panel, a pill of 56 for each action, 4 apart
   and 8 above the button, which shows as a round close button. The items open with motion: each grows from a circle at the right to its width along the fast spatial spring,
   0.03 s after the one below it, fading in over 0.1 s; they close the other way round (see the motion of what goes,
   below). To do: the other sizes.
   **Done:** the floating toolbar is a `QToolBar` with `materialToolBar` = `"floating"` or `"floatingVibrant"`: a
   pill of 64 with a shadow, in a row or a column, whose `QToolButton`s are 48 to a finger with a shape of 40, with no
   container until checked. `QToolBar` lays itself out for it (no grip, no stretch) and asks the private
   `drawFloatingToolBar()` to paint it. A change of any of the style's properties on a shown widget takes
   effect at once: the style's event filter answers `QEvent::DynamicPropertyChange`.
   **Done:** a busy `QProgressBar` (range 0 to 0) moves: as a line, two parts that cross it, or, with
   `materialProgress` = `"loading"` or `"containedLoading"`, as the loading indicator (a shape of 38, in a container
   of 48). Compose defines its shapes in code, not as tokens, so Nt has its own: circles with 2 to 10 swellings,
   changing along a spring while they turn. What never rests asks `QtPrivate::Animator::keepPainting()` each time it
   is painted, so repainting stops once it is hidden or has a range.

The wavy progress bar is `materialProgress` = `"wavy"` on a horizontal `QProgressBar`: the done part is a sine 3 high
with a wavelength of 40 (20 for the parts of a busy bar), travelling 40 a second, flat at a tenth and below and from
95%; the height springs between the two. The track stays straight.

Check boxes, radio buttons, switches, tabs and menu rows ripple too. The first three are drawn with their middle (a
switch's handle when off) 20 in from the left, so that the circle of 40 around the control fits in the widget and the
three line up; the ripple spreads from the control, and only for a press that the button takes (not on the room past
its label). A tab's and a menu row's ripples stay within them. A widget that is hidden forgets its press, so that a
menu opens clean. `QMenu` now has the style polish it before it is measured: it was too narrow for Material's font the
first time it opened.

The sizes of the common button are `materialButtonSize` = `"extraSmall"` to `"extraLarge"` on a `QPushButton` or a
`QToolButton`: heights 32, 40, 56, 96, 136; room before and after the label 12, 16, 24, 48, 64; icons 20, 20, 24, 32,
40; labels of 14, 14, 16 (medium weight), 24 and 32 (regular); pressed corners 8, 8, 12, 16, 16; checked (round) and
resting (`materialButtonShape` = `"square"`) corners 12, 12, 16, 28, 28; outlines 1, 1, 1, 2, 3. A button without a
size is as before. A tool button with a size or a kind takes the label colour of its container. Split buttons, groups
and FABs keep one size.

Text fields: a framed `QLineEdit` is outlined, or filled with `materialTextField` = `"filled"`. `materialLabel` rests
at 16 in the middle of an empty unfocused field and floats to 12 along the default effects spring: onto the outline,
which makes way for it (an outlined field then has 8 above its container), or into the top of a filled one. The
placeholder is hidden while the label rests (`QLineEdit` asks the private `hidesPlaceholder()`).
`materialSupportingText` and `materialError` have 20 under the field. Not done: the caret in the accent colour and
multi-line fields (icons, prefix and suffix and the counter are below).

Icon buttons are `materialIconButton` = `"standard"`, `"filled"`, `"tonal"` or `"outlined"` on a push or tool button:
square in the five button sizes with icons of 20, 24, 24, 32 and 40, the buttons' corners and outlines, 48 to a finger.
Icons: an icon of one colour is repainted in the colour the style gives its button, floating toolbar, FAB or menu item
(`paintIcon()`; one colour means that all pixels with some opacity are within 32 of each other), so logos stay as they
are without a property. The icons are the application's, or those of the optional package Nt Material Symbols
(`third_party/material-symbols`, Apache-2.0), which Nt Widgets does not use. The narrow and wide widths are below.

The top app bar (small) is a `QToolBar` with `materialToolBar` = `"appBar"`: 64 high, `Surface`, the window title in
22 (title large); the first separator marks where the title goes, so the actions before it lead (icons `OnSurface`)
and those after it trail (`OnSurfaceVariant`). `QMainWindow` now gives what a tool bar line leaves free to bars whose
size policy expands, as Qt does, which the app bar's does. The bottom navigation bar is a horizontal `QTabBar` with
`materialTabBar` = `"navigationBar"` (in a `QTabWidget` with the tabs at the south): 64 high, `SurfaceContainer`, equal
widths, a 24 icon in a 56 by 32 pill over a label of 12; the pill opens along the fast spatial spring. Not done:
horizontal items.

A snackbar is the status bar of a `QMainWindow` with `materialStatusBar` = `"snackbar"`: `InverseSurface`, corners 4,
48 high (68 when the message takes two lines), 16 from the sides and from the bottom, above a navigation bar, at most
600 wide; it takes no room and has none while there is no message. Permanent widgets are its actions, text buttons in
`InversePrimary`. `QMainWindow` places it through private functions of its own. Tooltips (`QToolTip`) are Material's
plain tooltips without a property. A tooltip whose text is rich text (`Qt::mightBeRichText()`) is a rich tooltip, so
that a program which still builds with Qt, whose tooltips show HTML, gets one: `SurfaceContainer`, corners 12 (medium),
16 at the sides and 12 above and below, at most 320 wide (less if the window is narrower), at level 2 with its shadow
in 2 that the label keeps below it. A bold run (`<b>`, `<strong>`) or a heading (`<h1>` to `<h6>`) at the start,
after any `<qt>`, `<html>`, `<body>`, `<p>` or `<div>`, is the subhead in title small; the rest, made plain by
`QTextDocument` (paragraphs and `<br>` become line breaks), is the supporting text in body medium; both are
`OnSurfaceVariant` and wrap. The Nt-only property `materialToolTip` = `"rich"` of the widget the tooltip is for makes a
plain text one rich, its first line the subhead. QToolTip's label does the layout and the text, as it does for a plain
tooltip, and sets `_nt_materialRichToolTip` on itself so that `PE_PanelTipLabel` draws the container. `materialProgress`
= `"circular"` is the circular indicator: 40, 4 thick, a gap of 4 before the track; busy, an arc that grows and shrinks
as it goes round. `"circularWavy"` is the wavy circular indicator: 48, the part that is done (or the busy arc) a wave
1.6 out and in from a circle of radius 20.4, with a whole number of crests (9, about 15 apart) so that a full circle
closes, turning at 60 degrees a second; flat at a tenth and below and from 95%, its height springing between the two as
on the wavy bar. The track stays a plain circle. Not done: a queue of messages and the action buttons of a rich tooltip
(Nt's tooltip is a label the mouse goes through, gone when the mouse leaves the widget, so it has nothing to press;
Qt's `QToolTip` has no actions either). The snackbar's motion and close icon are below.

Chips are `QPushButton`s with `materialChip` = `"assist"`, `"filter"`, `"input"` or `"suggestion"`: 32 high in a
target of 48, corners 8, an `OutlineVariant` outline, label large, an 18 icon; 16 beside a bare label, 8 around an
icon. A chosen one is `SecondaryContainer`; a filter chip then shows a check mark, and `QAbstractButton` asks for a new
size when its checked state changes. The input chip's cross is not a target of its own, so that no Nt-only signal is
needed. Cards are `QFrame`s with `materialCard` = `"filled"` (`SurfaceContainerHighest`), `"elevated"`
(`SurfaceContainerLow` over a shadow) or `"outlined"` (`Surface`, `OutlineVariant`), corners 12. Under the style a
`QFrame` line is the divider, 1 of `OutlineVariant`, so a Qt application gets it unchanged. `QFrame` asks the style
through a private function. `materialChipElevated` = `true` makes an assist, filter or suggestion chip elevated: no
outline, `SurfaceContainerLow` over the shadow of the elevated button (level 1), whose fall grows from 1.5 to 3 in the
room of the target while the pointer is over it (level 2); an input chip has no elevated kind. `materialChipAvatar` =
`true` on an input chip with an icon shows the icon as an avatar, a picture of 24 cut to a circle, 4 from the start
and 8 before the label, in its own colours; without it an icon stays an icon of 18, so that plain icons are not
cropped. A card that can be clicked is a `QPushButton` (any `QAbstractButton` that draws through the push button's
panel) with `materialCard`, as a `QFrame` cannot be clicked in Qt: the same shapes and colours as the frame, with the
state layer of `OnSurface` (0.08 on hover, 0.10 pressed), the ripple of buttons and a focus ring; an elevated one keeps
2 below for its shadow, which falls 1, and 2 on hover. Its size is that of its layout. With motion, the shadow
of an elevated chip or card moves between its levels on the default effects spring. Not done: a separate target for
the input chip's cross.

`QCalendarWidget` is the date picker without a property: `SurfaceContainerHigh`, days of 40, the chosen one a
`Primary` circle, today a `Primary` ring and label, names of days and days in body large. The base `paintCell()` asks
the style, so an application's override that calls it keeps the look, and the colors of its text formats win. The
style treats the two arrow buttons as standard icon buttons and draws chevrons. Days of other months stay, faint,
because Qt lets a click on one go to its month. Not done here: ranges, date input and the time
picker (Qt Widgets has no widget that is one); the headline and the year grid are below.

The navigation rail is a vertical `QTabBar` with `materialTabBar` = `"navigationRail"` (a `QTabWidget` with the tabs
at the west): 96 wide, `Surface`, destinations of 64 with 4 between them from 44 down, drawn as those of the bar.
Badges are `Error` with `OnError`: a dot of 6, or 16 high with a label of 11; `materialBadge` on icon and toolbar
buttons, `materialBadges` (a list, as the tabs have no object of their own and tab data is the application's) on the
tab bar. Under the style `SH_DialogButtonLayout` is `AndroidLayout`, which `QDialogButtonBox` now asks for. A
snackbar goes above a floating action button at the bottom, as Compose's `Scaffold` places it, since a button that a
layout places cannot be moved aside, and keeps to the pages beside a rail. Not done: a rail that scrolls.

Sheets are `QDialog`s with `materialSheet` = `"bottom"` (against the bottom, up to 640 wide, corners 28 at the top)
or `"drawer"` (against the left edge, up to 360 wide and 56 short of the window, corners 16 at the right), both
`SurfaceContainerLow`. `QDialog` asks the style to place one each time it is shown. The android platform, which places
dialogs itself, reads the window property `_nt_sheet`, rounds only the corners away from the edge, and closes the
sheet on a tap on the scrim; Back closes it as it does any dialog. A `QListWidget` with `materialList` =
`"navigationDrawer"` is the drawer's list: pills of 56 inset by 12, no frame. Icons of one color in the rows of any
list now take `OnSurfaceVariant` (`OnSecondaryContainer` when chosen). Not done: dragging and the
drag handle (which would promise it), standard sheets, dismissal by an outside click on a desktop.

`materialTextField` = `"search"` makes a `QLineEdit` the search bar: a `SurfaceContainerHigh` pill of 56 with a
magnifier at 16 and the text at 56; the clear button is its trailing action. The style draws the magnifier itself, as
`QLineEdit` has no leading actions in Nt. `materialTabBar` = `"secondary"` gives the secondary tabs: an indicator of 2
as wide as the tab, and the current label in `OnSurface`. Not done: an avatar and other icons in the bar (the
search view is below).

`materialTimeInput` on a `QTimeEdit` is Material's time input (the keyboard form of its time picker; the dial would
need a widget that Qt Widgets has not got, and an application using it would not build with Qt). Boxes of 96 by 72,
corners 8, `SurfaceContainerHighest`, the current one `PrimaryContainer` with a 2 `Primary` outline, digits in 45; a
period selector of 52 by 72 with `TertiaryContainer` for the half that applies. All editing stays
`QDateTimeEdit`'s: its line edit keeps the focus with no size, and the style draws its text by section. Not done: the
dial, moving on to the minute after two digits, translated captions. A format of numbers alone sets
`ImhPreferNumbers`, with which Android shows its number keys (see `doc/android.h`).

Spin boxes and scroll bars are drawn without a property, as every Qt form and list has them. Material has no spin box:
this one is the outlined field, 56 high, with a minus and a plus in 48 each (a chevron alone for a date edit with a
calendar pop-up). On a platform with an on-screen keyboard the two buttons leave the focus where it is, since the keys
would slide the page from under the finger. `materialLabel` on a spin box (Nt only, as on a line edit) is drawn by
the line edit's code: it takes 8 over the field, sits on the outline, which makes way for it, while there is text or
the focus, and rests inside, before the buttons, once the text has been cleared, along the same spring. A button that
is held repeats, for every style, as in Qt: `QAbstractSpinBox` steps once at the press, again after
`SH_SpinBox_ClickAutoRepeatThreshold` (500) and then every `SH_SpinBox_ClickAutoRepeatRate` (150) until the release,
the pointer leaving the button or the end of the range. A scroll bar is 10 wide with a handle of 4 (8 under the
pointer or while dragged), no track and no arrows, and at least 48 long. Its handle hides at rest, as on a phone: a child
of the bar (`ScrollBarRest`) wakes it when it is shown, when its value changes (the wheel, the keys, the program, or
QScroller under a finger), when the pointer comes onto it or leaves it and when it is pressed or released, and lets it
rest 1.5 s later; the style fades the handle in on the fast effects spring and out on the slow one. The bar keeps its
width and its clicks, so nothing is laid out again. Without motion the handle stays, which keeps forms usable and tests
still; `materialScrollBar` = `"always"` (Nt only) on the bar or its scroll area keeps it too.

A dialog keeps its contents 24 from its edges. Where the platform narrows dialogs (`maximumDialogWidth()`, Android),
`QDialog` narrows itself before it is shown and takes the height that its layout needs at that width. (Nt's layouts
have height-for-width since then: a wrapping `QLabel` gets the height of its lines in box, grid, form and stacked
layouts, group boxes and scroll areas, and a wrapping label would like lines of at most 80 average characters.)

Where `showsDialogTitles()` is false (Android), a dialog with a window title shows it as the headline: 24, `OnSurface`,
24 from the top and 16 over the contents. The style makes the room as top contents margin before the dialog is sized,
and `QDialog::paintEvent()` asks it to draw, so no application layout changes. Sheets have none. A message box without
an icon now gives its text both columns: the empty icon column kept some of the width of the button row. Not done: a
headline of two lines, the centered form under an icon.

Group boxes and text editors need no property. A `QGroupBox` is a section: its title 14 medium in `Primary`, 4 over an
outlined card (corners 12, `OutlineVariant`), or over a line if it is flat; the layout margins are 16 inside the card.
The title font is only used for drawing, so the children keep the widget's font. A checkable title is 40 high with the
style's check box. A `QTextEdit` or `QPlainTextEdit` whose frame shape is `StyledPanel` is an outlined field: corners 4,
`Outline` 1, `Primary` 3 with the focus, text 16 with 10 between border and text (4 more come from the document's
margin). It has no floating label.

Sheets and snackbars move. A sheet is placed at its resting place and then moved from just beyond its edge along a
spring of stiffness 380 (the default spatial one) with a damping ratio of 1, so it never passes its edge and shows a
gap; the Android platform accepts a sheet's geometry beyond its edge and recomposes when a dialog moves. A snackbar
fades in on the default effects spring through a `QGraphicsOpacityEffect` (only if the application set none) and grows
from 80 percent about its middle: the private property `_nt_effectScale` scales the picture that the opacity effect
draws, so the bar keeps its geometry and its buttons stay where clicks find them. One
that is cleared or replaced leaves a picture of itself (`grab()`, with its buttons) that fades out on the fast effects
spring. (Sheets sliding out and the scrim fading: see the motion
of what goes, below.)

Text fields have icons, a prefix, a suffix and a counter. The icons come from Qt's own `QLineEdit::addAction(action,
position)`, which Nt now has: `QLineEdit` draws the visible actions' icons at its ends and triggers an action on a
click, and the style gives them 24 with 16 more room (`PM_LineEditIconSize`, `PM_LineEditIconMargin`), a quarter of
which goes toward the edge, so an icon is 12 from the edge and the text 16 from the icon (52 in). The style colours
one-colour icons `OnSurfaceVariant`, a trailing one `Error` while there is an error. The label rests where the text
begins and floats to the edge on an outlined field. `materialPrefix` and `materialSuffix` (Nt only) show, in the
placeholder colour, once the label has floated; `materialCounter` puts `length/maxLength` at the end of the supporting
line. Not done: the icon buttons' own ripple and hover, avatars.

A snackbar can have its close affordance: `materialCloseButton` (Nt only) on the status bar makes `QStatusBar` give
itself a button of 48 at its very end, after the actions, while there is a message. The snackbar draws its cross (24,
`InverseOnSurface`, a light circle while pressed) over the button, which only takes the press, and the click clears
the message. Not done: hover on the cross, a focus ring.

The top app bar follows what is scrolled under it, as Compose's `TopAppBarDefaults` colors do: its container is
`Surface` at rest and `SurfaceContainer` (`scrolledContainerColor`) while the content beneath it is scrolled, faded on
the default effects spring (at once without motion). The bar has no link to its content in Qt, so the style looks for
it when it paints the bar: a `QAbstractScrollArea` of the same window that is shown, starts at or below the bar's
bottom edge, is at least half as high as the window below the bar, and whose vertical scroll bar is past its minimum.
`polish()` gives each scroll area a hidden child object that connects its vertical scroll bar's `valueChanged` to a
repaint of the window's app bars (`unpolish()` deletes it, which disconnects it), and the style's event filter does the
same when a scroll area is shown or hidden, as when a tab widget changes page. The title and icons keep their colors.
Not done: the bar does not scroll away (`enterAlways`; the medium and large bars that collapse are below), and content that scrolls without a `QAbstractScrollArea` (a `QGraphicsView` is one,
so it counts; a custom widget that scrolls itself does not). A scroll bar that is replaced with
`setVerticalScrollBar()` after polishing loses the connection until the area is polished again.

The date picker has a headline and a choice of years (numbers from `DatePickerModalTokens`). The headline is opt-in:
`materialDatePicker` `"modal"` on a `QCalendarWidget` puts above the month a header of 120 (`SurfaceContainerHigh`):
"Select date" 14 medium 16 from the top and the selected date as `ddd, MMM d` 32 regular 12 from the bottom, both
`OnSurfaceVariant` and 24 in, then an `OutlineVariant` divider of 1. Without it a calendar keeps its look, since a
calendar that is not in a dialog of its own, such as the pop-up of a `QDateEdit`, is the docked picker, which has no
headline. The choice of years is for every calendar under the style: `QCalendarWidget` filters the paint and press
events of its month label, which `QMaterialStyle::drawCalendarTitle()` paints as 14 medium `OnSurfaceVariant` text
with an arrow of a menu (up while open). A click hides the month arrows (the bar keeps the height they give it) and
replaces the days with the years from `minimumDate().year()` to `maximumDate().year()`, three columns of cells 52 high,
8 under the bar; `drawCalendarYear()` draws a pill of 72 by 36, `Primary` with `OnPrimary` for the year shown, a 1
`Primary` outline for this year, `OnSurfaceVariant` otherwise, and the focus ring for the keyboard's year. The year
shown opens in the second row; the wheel scrolls by rows; arrows, Page Up/Down, Home/End move, Return/Enter/Space
choose, Escape (or another click on the label) goes back. Choosing a year shows its days in the same month, clamped to
the months of the range, and leaves the selection alone, as Compose does. Nothing moves, so motion off changes nothing.
The modal picker has the input mode too. At the end of the row of the date, 12 from the right and centred on its
line, is an icon button of 48 with an icon of 24, drawn with `QPainter` paths in `OnSurfaceVariant` (no Material
Symbols, which only `NtMaterialSymbols` may use): a pencil, which switches to the input mode, and there a calendar,
which switches back (`datePickerModeButton()`; the date is elided before it). The input mode hides the navigation bar
(`isNavigationBarVisible()` still says what the program asked for), paints nothing where the month was and shows a
real `QLineEdit` child, made the first time, as an outlined field 24 in from the sides and 16 under the divider
(`datePickerField()`), with `materialLabel` "Date", the pattern as supporting text and placeholder, an empty
`materialError` from the start so that an error does not move it, and the focus. The headline's first line says
"Enter date". The pattern is the locale's short date format with days and months of two digits and years of four
("MM/dd/yyyy" in the US; a format with names becomes that one). Once the text is as long as the pattern, a date of
the range selects itself (`selectionChanged()`, not `clicked()`) and the headline follows, while text that is not a
date, or a date outside `minimumDate()` to `maximumDate()`, shows the error ("Date does not match the pattern
MM/DD/YYYY", "Date out of range: ... to ...") and keeps the selection; shorter text is left alone, since where the day
comes last its first digit is already a date. Return, the field losing the focus and going back read days and months
of one digit too, and show the error for text that is not a date of the range. Return on a good date emits `activated()`, as Qt's own
date edit of the calendar does. Going back takes the text if it is a date of the range and otherwise leaves the
selection alone; the calendar gets the focus back. A date selected by the program while the field is shown is written
into it. The field asks for `ImhDate`, which the android platform turns into a keyboard of digits with the separators
of a date (`ImhPreferNumbers` would give digits alone, without the slash). A calendar that loses the property or the
style leaves the input mode. Not done: the icon button is not a widget, so it cannot be reached with Tab and has no
state layer.
Not done: the range picker, a menu of months (the docked picker's), the confirm and cancel buttons (left to the
dialog), state layers on the years, animating the switch, and a localized headline.

Sliders have the five Expressive sizes, `materialSliderSize` = `"extraSmall"` (the default, the look they had) to
`"extraLarge"`: tracks of 16, 24, 40, 56 and 96, outer corners 8, 8, 12, 16 and 28, handles of 44, 44, 52, 68 and 108
(`PM_SliderThickness`, and `SE_SliderHandle` is as long across, so that is where a press takes hold). The ends beside
the handle have corners of 2, except on the extra small size, which keeps its round ones; the stops sit as far in as
the outer corner. The value indicator is a pill of 44 by at least 48 (`InverseSurface`, the value in label large,
`InverseOnSurface`, 12 beside it), 4 over the handle, or 4 at its left on a vertical slider, and pushed back inside
the slider near an end. It shows while the handle is pressed or the slider has the keyboard focus ring (that is, the
arrow keys are moving it). A slider with tick marks shows it, as Material's discrete sliders do; `materialSliderValue`
= `true` or `false` overrides that. It is drawn inside the slider, not in an overlay: a slider that shows it is
thicker by 48 (or by the width of its widest value, vertically), and the track and handle are centred in the rest, so
it is never clipped as long as the slider gets its minimum size. Not done: drawing it over the content above without
the room, its scaling in and out with motion, inset icons, centered and range sliders.

Menus have the shape of the standard menu of Material 3 Expressive: corners of 16 (`CornerLarge`), with the outline
kept in the place of the shadow. A menu is a child of its window in Nt, so the window shows at the corners. The rows
are 4 below the top of the container and 4 above its bottom (`PM_MenuVMargin`) and inset 4 from its sides
(`PM_MenuHMargin`); `QMenu::layoutItems()` now asks for both, as Qt's `QMenu` does (not for `PM_MenuPanelWidth`, which
would move the rows of Fusion's menus by its 1), and Fusion answers 0, and so does a FAB menu here. Inside a row the
padding is 12, so the text stays 16 from the container's edge, and a row's width is at least 112 less the insets. A
row has its own shape, which the highlight and the ripple fill (Nt's `QPainter::setClipPath()` clips to the bounding
rectangle, so the shape is the filled path itself): corners of 4 (extra small), but 12 at the top of the first row
and at the bottom of the last (the container's 16 less the inset, found with `actionAt()` just outside the row), and
a checked row (a checkable `QAction` that is checked) is a shape of 12 all round in `TertiaryContainer`, its text,
check mark and icon `OnTertiaryContainer`, the state layer in that color. Groups and gaps are the Nt-only property
`materialMenu` = `"grouped"` of the `QMenu`, since Qt's menus have only `addSeparator()`: a plain separator is then
2 + 4 + 4 high (the padding of the group above, the gap and the padding of the group below, from `CT_MenuItem`) and
draws nothing, and `PE_PanelMenu` fills and outlines a container for each group, with the menu's 16 at the ends of
the menu and 8 (corner small) beside a gap; the window shows in the gap. A section (a separator with a text) is not
a gap. Setting the property sends the menu a style change, so it lays itself out anew. A section
(`QMenu::addSection()`) is a group label: a row of 32 with no line, its text in label medium (12 of the menu's 16, medium
weight) and `OnSurfaceVariant`, 12 in from the row as the rows' text is; in a grouped menu it sits inside its group. Nt's
`QMenu` gives the style a section as a normal item that is not enabled, so the style knows it by its action, found with
`actionAt()` at the row, or, while the menu measures its rows (which have no rectangle then), by a separator with that
text. A line break in an action's text puts what follows under the label as supporting text (body medium,
`OnSurfaceVariant`, a line of 20 for each line break), the label in a line of 24, 8 above and below, so 60 with one
line; the type is that of the list's rows (`listRowFont()`), and the shortcut, which `QMenu` puts after a tab at the end
of the text, stays at the trailing end in the middle of the row. The Nt-only property `materialTrailingIcon` (a `QIcon`)
of a `QAction` draws it 24 by 24 at the trailing end, tinted `OnSurfaceVariant` by `paintIcon()` when it has one color,
inside the arrow of a submenu and outside the shortcut; the rows share one width, so all make room for it when one has
it. Not done: the vertical menu whose submenu opens in place, supporting text that wraps (each line is one line break),
and icons of section labels.

The expanded navigation rail (Compose's `WideNavigationRail` expanded, the baseline item tokens) is `materialTabBar` =
`"navigationRailExpanded"`, a value of the same property rather than a boolean beside it, so that a rail is one or the
other and `isNavigationRail()` counts both: the snackbar, layout and press code take it as a rail with no more work.
It is as wide as its longest destination needs (20 + 16 + icon 24 + 8 + label + 8 + count + 16 + 20) within 220 and
360, `Surface`, with rows of 56 and 4 between them from 44 down. The label is label large (14, medium) beside the
icon, both `OnSecondaryContainer` on a pill of `SecondaryContainer` across the row (20 from each side, corners full)
for the destination shown and `OnSurfaceVariant` for the others. A dot sits on the icon as on the collapsed rail; a
count or text is a badge at the end of the row, 16 inside the pill. The pill opens from the middle with the spring of
the collapsed rail, and a press ripples from where it is. Setting the property at runtime switches between the two:
the style's `propertyChanged()` sends the `QTabWidget` that holds the bar a style change so that it lays the bar out
at its new width, and moves a snackbar that is shown. The width changes at once, not animated: `QTabWidget` lays its
bar out at its size hint, and growing that over frames would relayout the pages at every one. Not done: the menu
button and the floating action button at the top of the rail, the modal expanded rail (`SurfaceContainer`, over the
content with a scrim), and a rail that hides its labels as it collapses.

The medium and large flexible top app bars are `materialToolBar` = `"appBarMedium"` and `"appBarLarge"`, app bars in
every other way (`QMaterialStyle::isAppBar()`). Expanded they are 112 and 120 high (the Compose medium and large
flexible app bars without a subtitle): the buttons stay in the top row of 64 (`Qt::AlignTop` in the toolbar's layout)
and the window title is on a line of its own below them, 16 from the left and 12 above the bottom, in headline medium
(28 on 36) or display small (36 on 44). They collapse to the small bar's 64 once the scroll area that the color
rule picks is scrolled down, and expand when it is back at the top; `QToolBar::sizeHint()` asks the style for the
height, and `updateAppBars()` calls `updateGeometry()` so the main window lays it out again. The big title goes up with
the bottom edge, clipped to below the row, and fades as 1 - collapsed; the small title fades in over the second half
(2 * collapsed - 1). With motion the collapse is the target of a critically damped spring (`AppBarCollapseChannel`,
default effects), and each frame that paints a height other than the bar's own calls `updateGeometry()`. The bar first
followed the offset (height = max(64, expanded - offset)), but then the content moved up twice as fast as the finger,
scrolled and lifted with the bar; Compose avoids that with nested scrolling, which gives the scroll to the bar first and
has no counterpart here, so the bar now snaps. A list barely longer than the room cannot collapse it: the room the
collapse gives clamps the scroll back to the top, which expands the bar again, and it settles there. Not done: subtitles (136 and 152 high), centred titles, titles on two lines, enterAlways
and pinned behaviour.

**Two- and three-line list items.** QListView and QListWidget rows take Material's one-, two- and three-line list
items from the display text alone, so that a program reads the same under Qt (which draws the line breaks as lines of
its own): the first line is the headline (body large, `OnSurface`), the text after the first `\n` the supporting text
(body medium, 14/16 of the headline's size, `OnSurfaceVariant`), and a `\t` in the headline puts the rest at the
trailing end as trailing supporting text (label small, 11/16, medium weight, `OnSurfaceVariant`, 12 after the text).
Selected rows use `OnSecondaryContainer` for both, disabled ones `OnSurface` at 0.38. One line is 56, two lines 72,
three lines 88 (`ListTokens`); the headline line box is 24 and a supporting line 20. A row has three lines when the
supporting text has a `\n` of its own: the count does not depend on the width, so a row keeps its height as the list
is resized, and a size hint needs no width. The supporting text of a two-line row is shortened to one line; that of a
three-line row is wrapped at words (and at its line breaks) and the second line shortened if there is more. Two
lines and their trailing text are centred as a block; a three-line row puts its content 12 from the top, with the
icon and check box beside the headline. Margins stay 16 and the gap after the icon 12 (`ItemBetweenSpace` of the
expressive tokens, as the one-line rows have it). The style draws the text (`drawListRowText()`) for both
QStyledItemDelegate and QListWidget's own painting; the supporting text is left out of the width hint, so a long one
is shortened rather than giving the list a horizontal scroll bar. Not done: overline, avatars, images and videos,
trailing icons and checkboxes, segmented or gapped lists.

**Docked toolbar and icon button widths.** `materialToolBar` = `"docked"` or `"dockedVibrant"` makes a `QToolBar` the
docked toolbar: 64 high across its whole side (`DockedToolbarTokens`), square corners and no shadow, in
`SurfaceContainer` (`StandardToolbarTokens`) or `PrimaryContainer` (`VibrantToolbarTokens`), its buttons 48 and in the
colours of the floating toolbar's items, which are the same tokens. The app puts it in `Qt::BottomToolBarArea` of a
`QMainWindow` (or the top, or at the bottom of a layout); its size policy is `Expanding` along it, so the main window's
tool bar line gives it the width. The items are 16 in from the ends (leading and trailing space) and from 4 to 32 apart
(min and max spacing): `QToolBar::relayout()` puts a stretch at each end and, between two buttons, a fixed spacer of 4
and one of 28 that may shrink to nothing, so a narrow window spreads them edge to edge and a wide one centres them 32
apart. Separators are hidden; the spacing is what separates the buttons. A snackbar already sits above a bar in the
bottom tool bar area, since the main window gives the snackbar only the room above its tool bars;
`QMainWindow::placeSnackbar()` also goes above a docked toolbar at the bottom of the central widget, as it does for a
navigation bar. `materialIconButtonWidth` = `"narrow"` or `"wide"` changes the width of an icon button's shape
(`"default"` or none keeps it square): each width is the icon and its leading and trailing space of
`IconButton*Tokens`, narrow / default / wide 28 / 32 / 40 for extra small, 32 / 40 / 52 for small, 48 / 56 / 72 for
medium, 64 / 96 / 128 for large and 104 / 136 / 184 for extra large. The size hint is the shape and 48 at least in each
direction, the shape is centred in it with its own room at the sides and above and below, a full corner is no more
than half of the narrower side, and the pressed and checked corners morph as before. Not done: the docked toolbar does
not hide on scroll, and has no FAB of its own or overflow menu.

**Search view.** A search bar (`materialTextField` `"search"`) with a QCompleter opens Material's docked search view,
so that a program written for Qt gets an ordinary completer there. The completer's popup is a QListWidget in the
line edit's window (as menus are), so its corners show the window behind it. The style knows it by its popup anchor
(the line edit, set by QCompleter before the popup is polished, and polished again if the completer moves to another
widget): it makes the list's `Base` transparent and its frame `NoFrame`, and `drawMaterialFrame()` draws the container
under the rows. The view is `SurfaceContainerHigh` (`SearchViewTokens`), goes on from the bar at its full width with a
1 divider in `OutlineVariant`, and has corners of 28 (corner extra large, the docked container shape) at the far end;
while it is open the bar's corners towards it are square, so the two read as one shape (opened over the bar where
there is no room below, the shapes flip). Rows are 56, at most `maxVisibleItems()` (7) before the list scrolls, text
body large in `OnSurface`, and the icon that the model has in `Qt::DecorationRole` is a 24 leading icon. The current
row has the menu's hover layer (`OnSurface` 0.08) inside the container rather than the list's `SecondaryContainer`
pill. Under any other line edit the completer's popup is drawn as a menu instead: `SurfaceContainer`, corner 16,
outline `OutlineVariant`, rows 48. Keyboard and mouse stay QCompleter's (arrows, Return, a click, Escape). Not done:
the full-screen search view, a header of the view's own (the bar is the header), emphasis of the matching text,
motion when the view opens.

Things that go move too. A sheet that is closed (accept, reject, `done()`, `hide()`, Escape, Back, a tap beside it on
Android) is hidden at once, as any dialog is, so the semantics stay Qt's: `isVisible()` is false at once, `accepted`,
`rejected` and `finished` are emitted and the result set as before, and `exec()` returns at once. `QDialog::setVisible`
asks the private `slideSheetOut()` first, which puts a picture of the sheet (`grab()`) in a frameless `Qt::Dialog`
window of its own (`_nt_sheetGhost`, transparent for the mouse, a child of the parent's window), with the sheet's
`_nt_sheet`, `_nt_cornerRadius` and `_nt_sheetOffset`, and slides that to the edge on a spring of stiffness 800
(critically damped, faster than the 380 it came in on); it goes when it reaches the edge. The other way, a window that
stays on the screen while the widget says it is hidden, or signals that wait for the slide, would have broken code that
deletes the dialog after `exec()` or tests `isVisible()`; the picture outlives the dialog. A sheet shown again during its
exit takes the picture's offset and scrim and slides in from there, and the picture is deleted. The scrim: the style
sets `_nt_scrimOpacity` on the window of every dialog, fading it from 0 (or the picture's value) to 0.32 on the default
effects spring as the dialog is shown, and to 0 when it is hidden; the picture of a sheet carries the scrim and fades it
to 0 as it slides. The android platform dims by that property (32 percent without it) and composes again when a dialog
paints, which the fade asks for at each step. A dialog that is not a sheet still goes at once, and its scrim with it.
A FAB menu that closes leaves a picture of itself (`_nt_fabMenuGhost`, a child of the menu's window, which on every
platform is where a `QMenu` lives): its pills go one after another, the one farthest from the button first, 0.02 s
apart, each shrinking toward its right end into a circle on the fast effects spring while it fades out over 0.08 s;
the menu opening again deletes what is left. The FAB's cross turns back into the plus as before. With motion off all of
it is at once. Not run on a device yet: the Android scrim and the picture window on the compositor (the ghost is a
dialog window, so it is activated while it slides and Back or a tap beside it closes it early).

## Open points

- `QTabBar` draws its labels itself and asks `QMaterialStyle` for their color; behind a `QProxyStyle` the selected
  label is not accented.
- Dynamic colour is the seed only: `systemSeedColor()` reads `android.R.color.system_accent1_500` on API 31 and later,
  and the default constructor uses it. The system's own five palettes are not copied (their tones would be closer to
  other applications'), a grey theme still gives a coloured scheme, and a change of theme is not followed while the
  application runs.
- The emphasized type styles and the Roboto Flex variable font axes.
