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
   0.03 s after the one below it, fading in over 0.1 s; they close at once. To do: the other sizes.
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
`materialSupportingText` and `materialError` have 20 under the field. Not done: icons, prefix and suffix, character
counter, the caret in the accent colour, and multi-line fields.

Icon buttons are `materialIconButton` = `"standard"`, `"filled"`, `"tonal"` or `"outlined"` on a push or tool button:
square in the five button sizes with icons of 20, 24, 24, 32 and 40, the buttons' corners and outlines, 48 to a finger.
Icons: an icon of one colour is repainted in the colour the style gives its button, floating toolbar, FAB or menu item
(`paintIcon()`; one colour means that all pixels with some opacity are within 32 of each other), so logos stay as they
are without a property. The icons are the application's, or those of the optional package Nt Material Symbols
(`third_party/material-symbols`, Apache-2.0), which Nt Widgets does not use. Not done: the narrow and wide widths.

The top app bar (small) is a `QToolBar` with `materialToolBar` = `"appBar"`: 64 high, `Surface`, the window title in
22 (title large); the first separator marks where the title goes, so the actions before it lead (icons `OnSurface`)
and those after it trail (`OnSurfaceVariant`). `QMainWindow` now gives what a tool bar line leaves free to bars whose
size policy expands, as Qt does, which the app bar's does. The bottom navigation bar is a horizontal `QTabBar` with
`materialTabBar` = `"navigationBar"` (in a `QTabWidget` with the tabs at the south): 64 high, `SurfaceContainer`, equal
widths, a 24 icon in a 56 by 32 pill over a label of 12; the pill opens along the fast spatial spring. Not done: medium
and large app bars, the colour on scroll, horizontal items.

A snackbar is the status bar of a `QMainWindow` with `materialStatusBar` = `"snackbar"`: `InverseSurface`, corners 4,
48 high (68 when the message takes two lines), 16 from the sides and from the bottom, above a navigation bar, at most
600 wide; it takes no room and has none while there is no message. Permanent widgets are its actions, text buttons in
`InversePrimary`. `QMainWindow` places it through private functions of its own. Tooltips (`QToolTip`) are Material's
plain tooltips without a property. `materialProgress` = `"circular"` is the circular indicator: 40, 4 thick, a gap of 4
before the track; busy, an arc that grows and shrinks as it goes round. Not done: animation of the snackbar, a queue of
messages, a close icon, rich tooltips, the wavy circular indicator.

Chips are `QPushButton`s with `materialChip` = `"assist"`, `"filter"`, `"input"` or `"suggestion"`: 32 high in a
target of 48, corners 8, an `OutlineVariant` outline, label large, an 18 icon; 16 beside a bare label, 8 around an
icon. A chosen one is `SecondaryContainer`; a filter chip then shows a check mark, and `QAbstractButton` asks for a new
size when its checked state changes. The input chip's cross is not a target of its own, so that no Nt-only signal is
needed. Cards are `QFrame`s with `materialCard` = `"filled"` (`SurfaceContainerHighest`), `"elevated"`
(`SurfaceContainerLow` over a shadow) or `"outlined"` (`Surface`, `OutlineVariant`), corners 12. Under the style a
`QFrame` line is the divider, 1 of `OutlineVariant`, so a Qt application gets it unchanged. `QFrame` asks the style
through a private function. Not done: elevated chips, avatars, clickable cards.

`QCalendarWidget` is the date picker without a property: `SurfaceContainerHigh`, days of 40, the chosen one a
`Primary` circle, today a `Primary` ring and label, names of days and days in body large. The base `paintCell()` asks
the style, so an application's override that calls it keeps the look, and the colors of its text formats win. The
style treats the two arrow buttons as standard icon buttons and draws chevrons. Days of other months stay, faint,
because Qt lets a click on one go to its month. Not done: the headline, the year grid, ranges, date input and the time
picker (Qt Widgets has no widget that is one).

The navigation rail is a vertical `QTabBar` with `materialTabBar` = `"navigationRail"` (a `QTabWidget` with the tabs
at the west): 96 wide, `Surface`, destinations of 64 with 4 between them from 44 down, drawn as those of the bar.
Badges are `Error` with `OnError`: a dot of 6, or 16 high with a label of 11; `materialBadge` on icon and toolbar
buttons, `materialBadges` (a list, as the tabs have no object of their own and tab data is the application's) on the
tab bar. Under the style `SH_DialogButtonLayout` is `AndroidLayout`, which `QDialogButtonBox` now asks for. A
snackbar goes above a floating action button at the bottom, as Compose's `Scaffold` places it, since a button that a
layout places cannot be moved aside, and keeps to the pages beside a rail. Not done: the expanded rail, a rail that
scrolls.

Sheets are `QDialog`s with `materialSheet` = `"bottom"` (against the bottom, up to 640 wide, corners 28 at the top)
or `"drawer"` (against the left edge, up to 360 wide and 56 short of the window, corners 16 at the right), both
`SurfaceContainerLow`. `QDialog` asks the style to place one each time it is shown. The android platform, which places
dialogs itself, reads the window property `_nt_sheet`, rounds only the corners away from the edge, and closes the
sheet on a tap on the scrim; Back closes it as it does any dialog. A `QListWidget` with `materialList` =
`"navigationDrawer"` is the drawer's list: pills of 56 inset by 12, no frame. Icons of one color in the rows of any
list now take `OnSurfaceVariant` (`OnSecondaryContainer` when chosen). Not done: the sliding motion, dragging and the
drag handle (which would promise it), standard sheets, dismissal by an outside click on a desktop.

`materialTextField` = `"search"` makes a `QLineEdit` the search bar: a `SurfaceContainerHigh` pill of 56 with a
magnifier at 16 and the text at 56; the clear button is its trailing action. The style draws the magnifier itself, as
`QLineEdit` has no leading actions in Nt. `materialTabBar` = `"secondary"` gives the secondary tabs: an indicator of 2
as wide as the tab, and the current label in `OnSurface`. Not done: the search view with its results, an avatar and
other icons in the bar.

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
would slide the page from under the finger. A scroll bar is 10 wide with a handle of 4 (8 under the pointer or while
dragged), no track and no arrows, and at least 48 long. Not done: a floating label on a spin box, repeat while a
button is held, hiding the scroll bar at rest.

A dialog keeps its contents 24 from its edges. Where the platform narrows dialogs (`maximumDialogWidth()`, Android),
`QDialog` narrows itself before it is shown and gives each wrapping label the height of its lines, because Nt's layouts
have no height-for-width and a message box asked for one line. Not done: the headline (a message box's window title is
not shown on Android).

## Open points

- `QTabBar` draws its labels itself and asks `QMaterialStyle` for their color; behind a `QProxyStyle` the selected
  label is not accented.
- Dynamic colour is the seed only: `systemSeedColor()` reads `android.R.color.system_accent1_500` on API 31 and later,
  and the default constructor uses it. The system's own five palettes are not copied (their tones would be closer to
  other applications'), a grey theme still gives a coloured scheme, and a change of theme is not followed while the
  application runs.
- The emphasized type styles and the Roboto Flex variable font axes.
