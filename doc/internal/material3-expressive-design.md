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
   the one line. To do: ripples on check boxes, radio buttons, tabs and menu rows; fading of button, label and radio
   colours. Motion is kept per widget, so item views, which draw many indicators, do
   not move.
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
   `drawFloatingToolBar()` to paint it. Nt sends no `QEvent::DynamicPropertyChange`, so the property is read when
   actions are added, when the toolbar is shown and when the style changes.
   **Done:** a busy `QProgressBar` (range 0 to 0) moves: as a line, two parts that cross it, or, with
   `materialProgress` = `"loading"` or `"containedLoading"`, as the loading indicator (a shape of 38, in a container
   of 48). Compose defines its shapes in code, not as tokens, so Nt has its own: circles with 2 to 10 swellings,
   changing along a spring while they turn. What never rests asks `QtPrivate::Animator::keepPainting()` each time it
   is painted, so repainting stops once it is hidden or has a range.

The wavy progress bar is `materialProgress` = `"wavy"` on a horizontal `QProgressBar`: the done part is a sine 3 high
with a wavelength of 40 (20 for the parts of a busy bar), travelling 40 a second, flat at a tenth and below and from
95%; the height springs between the two. The track stays straight.

## Open points

- `QTabBar` draws its labels itself and asks `QMaterialStyle` for their color; behind a `QProxyStyle` the selected
  label is not accented.
- The system's dynamic colour (Android 12 and later) as the seed.
- The emphasized type styles and the Roboto Flex variable font axes.
