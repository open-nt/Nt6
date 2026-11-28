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
`materialSupportingText` and `materialError` have 20 under the field. The text cursor is `Primary` (`Error` with an
error) and 2 wide (`PM_TextCursorWidth`, which `QLineEdit` and the text editors ask for; a `setCursorWidth()` wins).
A `QTextEdit` or `QPlainTextEdit` with `materialTextField` = `"outlined"` or `"filled"`, or a label, supporting text
or error, is a multi-line field: `PE_PanelLineEdit` draws its frame, viewport margins set by the style put its text 16
from the sides and one line in the middle of 56 (26 under a filled one's label), and the label rests on the first line
(the viewport draws it too where it lies over it). With `materialTextField` it grows with its lines: its vertical size
policy becomes `Maximum` and `viewportSizeHint()` is the height of its lines, up to its maximum height, then it
scrolls. Its scroll bars keep to the container, 4 clear of its corners: `QAbstractScrollArea` has private scroll bar
margins that the style (a friend) sets, zero for every other area. `materialCounter` puts the count of characters at
the end of the supporting line, alone, as an editor has no `maxLength()`, or as `length/max` with the Nt-only
`materialMaxLength`, which does not limit the typing. Not done: icons, prefix and suffix (see below).

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
widths, a 24 icon in a 56 by 32 pill over a label of 12; the pill opens along the fast spatial spring. In a window of
medium width (600 on) the items are horizontal (the Expressive short bar): the icon, 4, then the label in a pill of 40
with 16 inside its ends, each item as wide as the widest needs plus 8 and the row centred, not stretched (shared again
when they do not fit). `materialNavigationItems` = `"horizontal"` or `"vertical"` forces one or the other.

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
layout places cannot be moved aside, and keeps to the pages beside a rail. Destinations that do not fit the height
scroll, the 44 above the first one with them (those under a menu button or FAB stay still, see below): the wheel moves them
a destination a notch, a drag of more than 4 moves them, the current one is scrolled into sight as it changes (the
first with its 44 above it), and on a rail that scrolls a press chooses at the release, if it has not moved, so that
a drag does not first go to where it started. The scroll into sight follows the default spatial spring (the style's
`railScrollPosition()`, at once without motion); the wheel and a drag move at once. A rail that fits keeps Qt's press and wheel. `SH_TabBar_PreferNoArrows`
is true for a rail; Nt's `QTabBar` draws no scroll buttons for any style.

Sheets are `QDialog`s with `materialSheet` = `"bottom"` (against the bottom, up to 640 wide, corners 28 at the top)
or `"drawer"` (against the left edge, up to 360 wide and 56 short of the window, corners 16 at the right), both
`SurfaceContainerLow`. `QDialog` asks the style to place one each time it is shown. The android platform, which places
dialogs itself, reads the window property `_nt_sheet`, rounds only the corners away from the edge, and closes a
modal sheet on a tap on the scrim; Back closes it as it does any dialog. A `QListWidget` with `materialList` =
`"navigationDrawer"` is the drawer's list: pills of 56 inset by 12, no frame. Icons of one color in the rows of any
list now take `OnSurfaceVariant` (`OnSecondaryContainer` when chosen). A bottom sheet has a drag handle, 32 by 4,
`OnSurfaceVariant` at 0.4, corners 2, 22 from the top in a target of 48; the style makes that room as top contents
margin before the sheet is sized, as for a headline, and `QDialog::paintEvent()` asks for it in the same call;
`materialSheetHandle` = false (Nt only) leaves it out. A press on a sheet itself (its handle, its background, or a
child that ignores the press) and a move of more than the drag distance toward its edge drags it: the style, as the
application's event filter, moves the dialog and sets `_nt_sheetOffset` (which the android platform places it by) and
`_nt_scrimOpacity` (0.32 times what is left of the way). Let go past a third of its height or width, or at more than
1000 px/s toward the edge (over the last 100 ms of the drag, not measured under 10 ms), it is `reject()`ed and its
picture slides on from there; flung back, or short of a third, it springs back on the entry spring with its scrim.
A press that reaches a window blocked by a modal sheet (`exec()`, `setModal()`) rejects the sheet and goes no further;
nothing public is added for it. `materialSheet` = `"standardBottom"` is the standard bottom sheet (Compose's
`BottomSheetScaffold`): placed and handled as a bottom sheet, but shown with `show()`, not modal, without a scrim
(`_nt_scrimOpacity` 0; `_nt_sheet` is `"bottom"` for the android platform), and it rests collapsed, `_nt_sheetOffset`
leaving its peek showing: `materialSheetPeek` (Nt only, default 56). It is dragged up to expand and no lower than the
peek; let go, it goes to the nearer of the two, or to the next one the way it was flung, on the entry spring; it never
closes by dragging (Escape still rejects it). The style, as the application's event filter, puts it back at the bottom
of its window, expanded or collapsed as it was, when the window moves or is resized. It lies on its window with the
shadow of elevation level 1: Nt's windows are opaque, so the shadow is a child of the window below
(`_nt_sheetShadow`, transparent for the mouse) that follows the sheet's Show, Move, Resize and Hide, four rings of
black at 0.045 rounded as the top corners (about 0.17 at the edge, gone 4 out); the android platform shows it in
the rounded corners too. On the android platform it works as elsewhere: `QWidgetWindow` sets `_nt_modal` (from
`QWidget::isModal()`) on each window it shows, and a finger that comes down goes to the topmost window it is in,
through any that is not modal or has `Qt::WindowTransparentForInput` (the picture of a sheet that slides out), and
stops at a modal one, which takes it, or is closed by it if it is a sheet; a window without `_nt_modal` counts as
modal (`routeTouch()` in `qandroidtouchroute_p.h`). The window touched becomes the active one, and the gesture stays
with it. A sheet that the platform makes shorter than it asked (the screen less 72) keeps the part showing that the
style meant, so a collapsed sheet keeps its peek. A modal bottom sheet with
`materialSheetHalf` = true (Nt only) that is taller than half its window opens half open, the top at the middle of
the window (Compose's partially expanded), and rests there or expanded the same way, or closes when let go nearer the
bottom; its scrim is whole at both and fades only below half open. A click on the handle (its target of 48) of either
goes to the other resting place. Not done: keyboard and accessibility moves (Escape closes, as before), and the shadow
of a standard sheet while its picture slides out, or of one without a parent.

`materialTextField` = `"search"` makes a `QLineEdit` the search bar: a `SurfaceContainerHigh` pill of 56 with a
magnifier at 16 and the text at 56. The style draws the magnifier itself, not as a leading action. Its trailing
actions and the clear button have targets of 48 side by side, icons of 24 in their middle, the outermost target 4 from
the edge (an icon 16 from it). An action with `materialAvatar` = `true` (Nt only, on the `QAction`) is an avatar: its
icon as a round picture of 30 in its own colors, cut to the circle in an image of its own (a painter clips to
rectangles), always last, past the clear button. `materialTabBar` = `"secondary"` gives the secondary tabs: an
indicator of 2 as wide as the tab, and the current label in `OnSurface`. (The search view is below.)

`materialTimeInput` on a `QTimeEdit` is Material's time input (the keyboard form of its time picker). Boxes of 96 by
72, corners 8, `SurfaceContainerHighest`, the current one `PrimaryContainer` with a 2 `Primary` outline, digits in 45;
a period selector of 52 by 72 with `TertiaryContainer` for the half that applies. All editing stays
`QDateTimeEdit`'s: its line edit keeps the focus with no size, and the style draws its text by section. Two digits
of the hour, or one that no second could follow, move on to the minute, selected, as `QDateTimeEdit` now does for
every format, as Qt does; so does the separator that follows a section (`3:` writes `03`), and `a` or `p` in the
AM/PM section chooses it and moves on (the private `QAbstractSpinBox::typeCharacter()` takes a key before the line
edit). A format of numbers alone sets `ImhPreferNumbers`, with which Android shows its number keys
(see `doc/android.h`). `materialTimeInput` = `"dial"` is the vertical time picker, inside the `QTimeEdit` (no popup;
its size hint is the picker's, 280 by 408 with AM/PM, 256 wide without), so an application stays Qt's: the headline
"Select time" (label medium) in 16, 20 under it the boxes of 96 by 80 in 57 (the one the dial shows
`PrimaryContainer`, no outline), the period selector 52 by 80, and 36 under them a dial of 256
(`SurfaceContainerHighest`) with its numbers in 16, 101 from the middle, a 24-hour format's 00 and 13 to 23 in a ring
of 69 (as Compose), a `Primary` selector of 48 on the number, a line of 2 to a dot of 8. The dial shows the minutes
(or seconds) while the line edit has the focus in them, else the hours. A press or drag on the dial sets the time with
`setTime()` (`QDateTimeEdit` keeps the press), the hour snapped to a number, inner ring under 85 from the middle;
letting go of an hour selects the minute section, whose 5-minute labels a drag passes for any minute. Captions
("Select time", "Hour", "Minute", "Second") go through `tr()`, context `QMaterialStyle`. Not done: the keyboard icon
that switches the picker to the input ("Enter time"), the horizontal layout, motion of the selector.

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
rest 1.5 s later. A child of each scroll area (`ScrollAreaHover`) wakes its bars too while a mouse moves over the area
or its viewport, as overlay scroll bars do, so they rest 1.5 s after the pointer stops or leaves: it filters the area and
the viewport, and between their Enter and Leave the moves that reach the window, since the viewport has no mouse tracking
(turning it on would make item views emit `entered()`). Only moves without a button count, so a finger sent as the mouse
does not wake them with a tap; the style fades the handle in on the fast effects spring and out on the slow one. The bar keeps its
width and its clicks, so nothing is laid out again. Without motion the handle stays, which keeps forms usable and tests
still; `materialScrollBar` = `"always"` (Nt only) on the bar or its scroll area keeps it too.

A dialog keeps its contents 24 from its edges. Where the platform narrows dialogs (`maximumDialogWidth()`, Android),
`QDialog` narrows itself before it is shown and takes the height that its layout needs at that width. (Nt's layouts
have height-for-width since then: a wrapping `QLabel` gets the height of its lines in box, grid, form and stacked
layouts, group boxes and scroll areas, and a wrapping label would like lines of at most 80 average characters.)

Where `showsDialogTitles()` is false (Android), a dialog with a window title shows it as the headline: 24, `OnSurface`,
24 from the top and 16 over the contents. The style makes the room as top contents margin before the dialog is sized,
and `QDialog::paintEvent()` asks it to draw, so no application layout changes. Sheets have none. A message box without
an icon now gives its text both columns: the empty icon column kept some of the width of the button row. A headline too
long for the dialog wraps (24 on its line height): the style sets the top margin for the width whenever the dialog is
resized, and `QDialog::heightForWidth()` adds the room the lines take at a width, so a dialog that a platform narrows
grows for them. A dialog with its own window icon (the icon set on the dialog itself, which Nt keeps apart from the
application's) has the centered form: the icon 24, `Secondary` through `paintIcon()`, 24 from the top in the middle,
and the headline centered 16 under it. A message box with an icon has the same form: its icon widget is hidden and its
texts take both columns, and the style draws its sign of one color (a round "i", "?" or "!", or a triangle with "!")
over the headline; a box with an icon pixmap shows the pixmap there. A dialog with such an icon and no window title
has the icon alone (`headlineParts()` gives an empty text rectangle 16 above the contents, so they are 16 under the
icon). The style's application filter takes `WindowTitleChange` and `WindowIconChange` of a shown dialog: it runs
`prepareHeadline()` again (room made, refitted or given back), resizes the dialog by the room that came or went, as
showing it again does, and repaints.

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
line. The actions and the clear button are standard icon buttons: `QLineEdit` tracks the one under the pointer and the
one held down, and the style draws a circle of 40 about the icon, `OnSurfaceVariant` at 0.08 on hover and 0.10 while
held; with motion the press is the ripple from where it was, inside that circle. They take no focus, as in Qt.

A snackbar can have its close affordance: `materialCloseButton` (Nt only) on the status bar makes `QStatusBar` give
itself a button of 48 at its very end, after the actions, while there is a message. The snackbar draws its cross (24,
`InverseOnSurface`) over the button, which takes the press and the keyboard focus (Tab only, so a click leaves the focus
where it is), and the click clears the message. The cross has the state layer of an icon button, a circle of 40 in
`InverseOnSurface` at 8% under the pointer and 10% while pressed or focused from the keyboard, and the focus ring
(3, `Secondary`) about it; the button repaints the snackbar on Enter, Leave, FocusIn and FocusOut.

The top app bar follows what is scrolled under it, as Compose's `TopAppBarDefaults` colors do: its container is
`Surface` at rest and `SurfaceContainer` (`scrolledContainerColor`) while the content beneath it is scrolled, faded on
the default effects spring (at once without motion). The bar has no link to its content in Qt, so the style looks for
it when it paints the bar: a `QAbstractScrollArea` of the same window that is shown, starts at or below the bar's
bottom edge, is at least half as high as the window below the bar, and whose vertical scroll bar is past its minimum.
`polish()` gives each scroll area a hidden child object that connects its vertical scroll bar's `valueChanged` to a
repaint of the window's app bars (`unpolish()` deletes it, which disconnects it), and the style's event filter does the
same when a scroll area is shown or hidden, as when a tab widget changes page. The title and icons keep their colors.
`materialScrollBehavior` = `"enterAlways"` takes the bar away as that scroll area scrolls down and brings it back as
soon as it scrolls up or reaches the top (Compose's `enterAlwaysScrollBehavior`), snapping as the flexible bars do
(below): the watcher's repaint compares the offset with the last one the bar saw (`followScrolling()`), and a step
back that leaves the scroll bar at its maximum is ignored, since that is the clamp from the room the bar gave up. The
height goes on `ToolBarHideChannel` (default effects); `appBarExtent()` is `toolBarRest()` times 1 - away, and while
the bar is partly away `QToolBar::event()` lays the buttons out at the rest height, moved up with the bottom edge,
and `drawAppBar()` translates the same way. The animator's `resizes()` makes each frame of such a spring call
`updateGeometry()` too, since a bar with no height left is not painted. `"pinned"` keeps a medium or large bar
expanded; `"exitUntilCollapsed"` (or none) is the collapse below. A medium or large bar with `enterAlways` collapses
and goes away, and comes back collapsed until the top. Not done: content that scrolls without a `QAbstractScrollArea` (a `QGraphicsView` is one,
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
which switches back (`datePickerModeButton()`; the date is elided before it). The button is a private
`QToolButton` child with `materialIconButton` `"standard"`, made before the navigation bar so that Tab reaches it
before the arrows; the style draws its state layer (0.08 hover, 0.10 pressed), focus ring and ripple, and
`drawDatePickerModeIcon()` the icon over them, so at rest it looks as it did. Space, Return and Enter press it, and
its accessible name says which mode it switches to. The input mode hides the navigation bar
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
style leaves the input mode. In the input mode the picker's size hint (and minimum) is only the headline, the field
and 8 under it (`datePickerInputHeight()`), as Material's date input dialog; a shown `QDialog` with a layout that
holds the picker shrinks by the height that went and grows back with the days.
The years have state layers on their pills, in the color of the text: 0.08 under the pointer (the calendar tracks the
mouse and handles the moves in `event()`) and 0.10 while pressed; a year is chosen on the release over the year it was
pressed on, so the press shows. The days and the years cross-fade (`datePickerYearsShown()`, the default effects spring
on the animator's `YearsChannel`): the calendar paints the years at that opacity and the days over them at the rest,
and at once without motion.
`materialDatePicker` `"docked"` makes the bar the docked picker's: the short month between its arrows at the start
and the year between its arrows at the end (`showPreviousYear()`/`showNextYear()`, disabled where no day of that
month is in the range). The year's label and arrows are made the first time a calendar is docked, so other calendars
keep two arrows and one label. The month opens a menu of the twelve months in the place of the days, as the year opens
the years: rows of 48, 8 under the bar, drawn by `drawCalendarMonth()` as menu rows (a check mark of 24 at 12 for the
month shown, `OnSurfaceVariant`; the name in body large `OnSurface` at 48; a state layer across the row; the focus
ring inset by 2), months without a day in the range at 0.38 and not chosen. The month shown opens in the second row;
the same keys, wheel, press and release, Escape and cross-fade as the years (the two share the scroll, the cursor and
`YearsChannel`; one replaces the other at once). The docked picker's years are such a menu too, one to a row of 48
with a check mark for the year shown (as the M3 docked picker; the grid of three stays for other calendars), the
year shown in the second row; a calendar docked or undocked while the years are open scrolls them anew. Choosing a month shows it in the year shown and leaves the
selection alone. While one menu is open the arrows go and the other label is disabled. The names come from the
calendar's `locale()`: the month in the bar (`standaloneMonthName()`, short when docked), the days above the days
(`dayName()`, `standaloneDayName()` narrow for single letters), the date of the headline
(`datePickerHeadlineText()`: `ddd, MMM d`, or `ddd d MMM` where the locale's short date has the day before the month)
and the input mode's pattern; a `LocaleChange` updates them, and a day-first locale reads the field day first
("25.12.2026" in German selects the 25th of December). "Select date", "Enter date", the field's "Date", its errors
and the accessible names of the mode button and the year arrows go through `QMaterialStyle::tr()`.
The calendar that a `QDateTimeEdit` with `calendarPopup()` makes (`CalendarPopup`) gets `materialDatePicker`
`"docked"`; one given with `setCalendarWidget()`, or a property the application sets, is left alone. `showPopup()`
sets the calendar's `popupAnchor` before it shows, and `QMaterialStyle::isDatePickerPopup()` (a calendar under the
style whose anchor is a date edit) makes it a menu's container: `drawDatePickerPopup()` fills corners of 16 in
`SurfaceContainerHigh` with the `OutlineVariant` outline that menus have in the place of their shadow (level 3), the
grid is not filled square and the navigation bar does not fill its background, so the window shows at the corners.
It goes below the field, above it where there is no room, inside the window less `menuWindowMargins()` (8) along a
side where it fits, as `QMenu::windowArea()` does.
Not done: the range picker (Qt Widgets' calendar has no range), the confirm and cancel buttons (left to the dialog).

Sliders have the five Expressive sizes, `materialSliderSize` = `"extraSmall"` (the default, the look they had) to
`"extraLarge"`: tracks of 16, 24, 40, 56 and 96, outer corners 8, 8, 12, 16 and 28, handles of 44, 44, 52, 68 and 108
(`PM_SliderThickness`, and `SE_SliderHandle` is as long across, so that is where a press takes hold). The ends beside
the handle have corners of 2, except on the extra small size, which keeps its round ones; the stops sit as far in as
the outer corner. The value indicator is a pill of 44 by at least 48 (`InverseSurface`, the value in label large,
`InverseOnSurface`, 12 beside it), 4 over the handle, or 4 at its left on a vertical slider, and pushed back inside
the slider near an end. It shows while the handle is pressed or the slider has the keyboard focus ring (that is, the
arrow keys are moving it). A slider with tick marks shows it, as Material's discrete sliders do; `materialSliderValue`
= `true` or `false` overrides that. It is drawn over the content above the slider, in an overlay that is a child of
the slider's window (as menus are), raised above its siblings and transparent to the mouse; the slider keeps the
overlay through a watcher that makes it on `sliderPressed`, focus in or a key press, outside any paint, and its paint
places and fills it. So the slider is only as thick as its handle (plus the tick room). It grows from 0 at the point
where it meets the handle and shrinks back into it on the fast effects spring (`SliderValueChannel`), at once without
motion. `materialSliderValueRoom` = `true`, and a slider that is a window, draw it inside the slider in kept room, as
before. Inset icons: `materialSliderIcon` (a QIcon) on the medium, large and extra large sizes, 24 (32 on extra large),
8 from the outer end of the active track in `OnPrimary`, or 8 after the handle's gap in the inactive track in
`OnSecondaryContainer` where the active part is shorter than the icon and its padding. `materialSlider` = `"centered"`:
the active track runs from the middle of the track to the handle (square at the middle), stops at both ends; no inset
icon. Not done: range sliders (Qt Widgets has no range slider), an icon on a centered slider.

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
weight) and `OnSurfaceVariant`, 12 in from the row as the rows' text is, after its icon (`addSection(icon, text)`), 20 by
20 and 8 before the text, tinted the same; in a grouped menu it sits inside its group. Nt's
`QMenu` gives the style a section as a normal item that is not enabled, so the style knows it by its action, found with
`actionAt()` at the row, or, while the menu measures its rows (which have no rectangle then), by a separator with that
text. A line break in an action's text puts what follows under the label as supporting text (body medium,
`OnSurfaceVariant`, a line of 20 for each line), the label in a line of 24, 8 above and below, so 60 with one
line. It wraps between words (`wrapListText()`) to what a row of the widest menu, 280, leaves beside the leading icon,
the arrow of a submenu and a trailing icon (`menuSupportingWidth()`), measured and drawn alike, so a row that wraps makes
the menu 280 wide and grows by 20 a line, and a line break still starts a line; the type is that of the list's rows (`listRowFont()`), and the shortcut, which `QMenu` puts after a tab at the end
of the text, stays at the trailing end in the middle of the row. The Nt-only property `materialTrailingIcon` (a `QIcon`)
of a `QAction` draws it 24 by 24 at the trailing end, tinted `OnSurfaceVariant` by `paintIcon()` when it has one color,
inside the arrow of a submenu and outside the shortcut; the rows share one width, so all make room for it when one has
it. The vertical menu of Expressive, whose submenu opens in place, is `materialMenu` = `"vertical"`: `QMenu` asks
`QMaterialStyle::opensSubMenusInPlace()` (a friend), and then a click on a submenu's row (on the release, so that the
release does not choose what takes its place), Right or Return lays out the submenu's actions in the menu's place, under
a back row; hovering does not open it. The back row is a private `QAction` of the menu with the submenu's title and
`SP_ArrowBack` for other styles; this style draws Material's arrow back (a 2 stroke) in it, knowing the row through
`QtPrivate::MenuAccess` (`qmenu_p.h`), which also gives the shown actions to the measuring of sections, trailing icons
and groups. The back row, Left and Escape go back, with the submenu's row current; submenus nest. The submenu emits
`aboutToShow()`/`aboutToHide()` and `triggered()`, but is never shown, and a change to its actions lays the menu out
again. The menu resizes to its new rows and is moved back inside its window. With motion the rows slide sideways
between the levels on the fast spatial spring, the new level in from the right going deeper and from the left going
back (mirrored right to left), the old one out ahead of it: `QMenu` draws the old rows apart (`rowsPicture()`, no
container) before it lays out the new ones, and both inside the new container while they move; without motion they
change at once. A menu keeps 8 from the edges of its window (`QMaterialStyle::menuWindowMargins()`, asked by
`QMenu::windowArea()` as it is placed and as it grows), so the menu of an app bar's overflow button on a phone ends 8
short of the right edge; a FAB menu and other styles' menus keep none, and along a side where the menu does not fit
within the margins it takes the whole window. Compose keeps its dropdown 48 from the top and bottom
(`MenuVerticalMargin`) and only inside the window across, where it lines the menu up with its anchor; 8 all round
keeps a menu bar's menus right under the bar.

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
bar out at its size hint, and growing that over frames would relayout the pages at every one. The menu button and the
FAB at the top of a rail are the application's widget in the `QTabWidget`'s corner, `Qt::TopLeftCorner` at the west
(`TopRightCorner` at the east), Qt's place for a widget beside the tabs: the tab widget puts it 44 down, as wide as the
rail and as high as its size hint, over the rail; the destinations start 40 under it, scroll out of sight under its
bottom (clipped there, and `tabAt()` finds none above it) and hold it still. Other corners get no room beside a vertical
bar, as before, but are no longer hidden (they came back as hidden when the tabs went back to the north). The modal
expanded rail is `materialTabBar` = `"navigationRailModal"`, an expanded rail in `SurfaceContainer` with trailing
corners of 16: the tab widget lays it over the pages, which keep the 96 of the collapsed rail, with a scrim (a child of
the tab widget, black at 32 percent) over them and the corner widget raised over the rail. A press on the scrim, Escape
on the rail, a destination chosen or the current one chosen again set the property back to `"navigationRail"`; the
menu button opens it by setting the property, a standard call. The scrim covers the whole window, an app bar above
the tab widget too: a child cannot be under the rail and over the window's other children both, so the part over the
tab widget is its child, under the rail, and up to four more are children of the window, raised, tiling the window
around the tab widget (kept in place as the tab widget moves or the window is resized); a press on any part closes the
rail. With motion the scrim fades in and out on the default effects spring and lets presses through as soon as the
rail closes; the rail and the corner widget slide in from the edge on the default spatial spring, stopped at the edge
rather than passing it. As it closes the bar is the collapsed rail at once, and a picture of the modal rail taken
before it collapsed (the style draws the bar as the modal rail while it has the private `_nt_railPicture`, and without
labels while `_nt_railLabels` is false) slides out over it on the same spring, its labels fading out as it goes.
Without motion all of it is immediate.

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
collapse gives clamps the scroll back to the top, which expands the bar again, and it settles there.
`materialSubtitle` adds a subtitle in `OnSurfaceVariant` (`AppBar*Tokens`): label medium (12 on 16) in the small bar,
whose title line (28) and subtitle line are centred together in its 64, and label large (14 on 20) or title medium
(16 on 24) in the medium and large bars, which are then 136 and 152 high, title and subtitle stacked 12 above the
bottom. A title wider than its line goes on two lines (`wrapAppBarTitle()`: broken at words, the rest elided on the
second, a first word wider than the line elided on one), and the expanded height grows by a title line, 36 or 44
(148 / 164, or 172 / 196 with a subtitle): Compose's flexible bars grow to fit their content above the minimum height,
and the spec gives no fixed height for two lines. It follows the bar's width; `QToolBar::event()` asks again on a
resize. `materialTitleAlignment` = `"center"` centres the title and subtitle on the bar (Compose's
`CenterAlignedTopAppBar`, and the `titleHorizontalAlignment` of the flexible bars), as wide as the room on the
narrower side leaves. The buttons of a medium or large bar are aligned to the top row and the expanded title is
clipped to below it, so a long title never goes under them (checked with long and centred titles beside trailing
buttons). Not done: three-line titles; Compose's flexible bars take a title of one line by default, and the guidelines
show at most two, so two stay the limit.

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
is shortened rather than giving the list a horizontal scroll bar.

An item with a check state (`Qt::CheckStateRole`) gets the trailing checkbox: the 18 box of `PE_IndicatorCheckBox`
(`Primary` with an `OnPrimary` mark checked, an `OnSurfaceVariant` outline unchecked, the dash partly checked) in the
middle of a slot of 24 at the end, 16 in, the text (and its trailing text) 12 before the slot; beside the headline
in a three-line row. `listCheckRect()` places it, and `subElementRect(SE_ItemViewItemCheckIndicator)` returns it for
a list's `QStyleOptionViewItem`, which QStyledItemDelegate's layout asks for. A click in the circle of 40 around the
box (the checkbox's state layer) toggles a `Qt::ItemIsUserCheckable` item, in the delegate's `editorEvent()` as Qt
does and in QListWidget's mouse release (before `itemClicked()`); Space toggles the current item of a QListWidget.
QListWidget draws the box only under this style; elsewhere it keeps the state, as before. `materialListAvatars` =
`true` makes the icons 40 avatars in their own colours, `materialListImages` = `true` 56 images with corners of 8
(small), neither recoloured; both are cut in an image of their own (`paintPicture()`, which `paintAvatar()` now
uses; the picture is cropped to a square from its middle), and a row is at least the picture + 16 high (an image row
72). The delegate takes the size into `decorationSize`. `materialListOverline` = `true` reads a three-line row as
overline, headline and one supporting line (the rest joined): the overline in label small, `OnSurfaceVariant`, on a
line of 16 from 12, the headline under it, the trailing text beside the headline; the row is still 88. The trailing
supporting text is the `\t` text above. `materialList` = `"segmented"` (Expressive's segmented, gapped list) draws
each row as a `SurfaceContainer` item inset 1 at top and bottom (2 between rows, without changing the layout), corners
16 at the outer ends of the first and last rows and 4 elsewhere; a selected or held row is `SecondaryContainer` (or the
pressed layer) with 16 all round. `drawListRow()` is told whether the row is the first or last, and its number: the
corners of each end of a row grow and shrink on the fast effects spring, a pair of springs per row kept for the
viewport (`ListSegmentChannel` + 2 × row), and change at once with motion off. A navigation drawer's rows take the
check box as other rows do, 16 in from the end of the pill. Not done: videos, trailing icons.

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
than half of the narrower side, and the pressed and checked corners morph as before. `materialScrollBehavior` =
`"exitAlways"` on a horizontal docked toolbar takes it away down as the scroll area above it (ending at or above its
top, at least half the room above) scrolls down and brings it back on scrolling up, as the app bar's `enterAlways`
does; its buttons stay with its top edge. A FAB in a docked toolbar (a QToolButton of an action, or a QPushButton
given to `addWidget()`, with `materialButton` = `"fab"`; `isFab()` takes tool buttons now, which are their icon alone,
56 square) goes to the end with a gap of 4 at least before it: 56 in `PrimaryContainer`, 16 from the end, 4 from top
and bottom, the bar still 64 and its other buttons in the middle; it lies flat (`fabLift()` is 0 there, no shadow and
no room for one), as Compose's bottom app bar FAB does. `QToolBar` had no extension: now, as in Qt, when the actions
do not fit (the layout's minimum length with all of them is more than the bar's) the last of them are hidden, one at
a time, until those left and the extension button (`qt_toolbar_ext_button`) fit, and the button opens a `QMenu` of the
rest, below it or above it when the window has no room below (beside a vertical bar), at its end; the chosen action
is reported by its hidden button, once. The size hint is still that of all of them, and the minimum only the
extension button (and the FAB). Widgets from `addWidget()` that do not fit are hidden without an entry (Qt shows them
in an expanded bar). A docked toolbar's extension button is the last of its buttons, spaced as they are, and the FAB
stays; a top app bar has no extension. The style draws the symbol (`standardPixmap()` of
`SP_ToolBarHorizontalExtensionButton` is Material's "more vertical", `SP_ToolBarVerticalExtensionButton` "more
horizontal"; QStyle draws two chevrons), which an item of a Material toolbar recolours; on a plain tool bar the
button is a standard icon button (`materialIconButton` set by the bar). Not done: the bar has no overflow that keeps
widgets.

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
outline `OutlineVariant`, rows 48. Keyboard and mouse stay QCompleter's (arrows, Return, a click, Escape). In both,
the part of a row that matches the completion prefix (case-insensitive, by the completer's filter mode: start,
contains or end) is `OnSurface` in body large emphasized (medium), the rest `OnSurfaceVariant`
(`drawCompletionText()`, which QListWidget asks before it draws a single-line row). On a compact window (narrower than
600) a search bar with a completer opens the full-screen view instead, when it gets the focus (not the focus that
comes back with its window, which the style tells by the `WindowActivate` just before) or is tapped while focused;
`materialSearchView` = `"fullScreen"` or `"docked"` chooses on any width. The view (`_nt_searchView`) is a child of
the window over everything in it, `SurfaceContainerHigh`, square, with a header of its own (`FullScreenHeader`, 72):
an arrow back (24, in a target of 48 4 from the edge, `OnSurface`, a press layer), a frameless line edit
(`_nt_searchField`, from 56 to 4 from the far edge, 56 high, a clear button) and a 1 divider in `OutlineVariant` under
it. The bar stays where it is under the view; its completer is moved to the header's line edit while the view is
open (`setWidget()`), which keeps QCompleter's keys, and the text is mirrored to the bar, which emits `textEdited`,
`returnPressed` and `editingFinished` for it. QCompleter asks `completerPopupGeometry()`, which puts the popup under the
header at the window's width, as many rows of 56 as fit, square. The arrow back, Escape (once the popup is closed),
Return, a chosen row or the focus going elsewhere closes the view at once and gives the completer back; all but the
last leave nothing focused, and a window that grows to 600 or more closes it and gives the bar the focus (docked
again). It grows from the bar's rectangle to the window's on the default spatial spring (a `Transition`), its corners
from the bar's half height to 0; without motion it is there at once. Closing gives the completer back and hides its
popup at once, and the live view (transparent for the mouse, no longer found as the open view) shrinks back to the
bar's rectangle on the same spring, round again, and is deleted at rest; without motion it goes at once. The bar's
trailing actions and avatars (`m_sideActions` at `TrailingPosition`, which the style reads as QLineEdit's friend) are
added to the header's field, so they show there and trigger the same QAction. Recent searches: while the view's field
is empty the popup shows every row of the model (an empty prefix matches all) rather than none: the view asks for it
once it has grown (at once without motion), and `QCompleter::textEdited()` completes rather than hides on an empty
text where the private `completerOffersAll()` says the widget is the view's field; under other line edits, docked
search bars included, an empty text still closes the popup, as in Qt.

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
