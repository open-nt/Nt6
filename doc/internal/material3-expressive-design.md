# Material 3 Expressive for Nt Widgets

Status: step 1 and part of step 2 (2026-11-26). Not part of 6.0.7.

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
2. Style: buttons, text fields, check boxes, radio buttons, switches (**done**); sliders, progress bars, tabs, combo
   boxes, menus, dialogs, item views (to do).
3. Motion: springs (expressive scheme: spatial default damping 0.8, stiffness 380; fast 0.6, 800; slow 0.8, 200;
   effects damping 1.0 with stiffness 1600, 3800, 800) and shape morphing. Until then the pressed shape of a button
   changes at once.
4. New components, Nt only: button groups, split button, FAB menu, floating toolbar, loading indicator.

## Open points

- The system's dynamic colour (Android 12 and later) as the seed.
- The emphasized type styles and the Roboto Flex variable font axes.
- Ripples.
