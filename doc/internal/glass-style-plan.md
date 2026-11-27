# A glass style for Nt Widgets (iPad, iPhone and Android)

Status: plan (2026-11-27). Nothing built. Not part of 6.0.x.

## Goal

A second touch style beside `QMaterialStyle`, in the manner of Apple's current translucent design ("Liquid Glass",
iOS and iPadOS 26): controls and bars of glass that blur, bend and tint what lies under them and float over the
content. One Qt Widgets source builds for iPad, iPhone and Android and runs unchanged in either style:

```cpp
QApplication::setStyle("Glass");      // or "Material3"; by default Glass on iOS, Material3 on Android
```

"Write once" means: one `CMakeLists.txt` (`find_package(Qt6)`, `qt_add_executable`) gives an APK and an IPA, and one
set of widgets and properties gives both looks, so an application does not keep two user interfaces.

## Decisions

- **Name.** The class is `QGlassStyle`, the style key `"Glass"`. "Liquid Glass", "SF" and "iOS" stay out of class
  names, keys, properties and public docs (Apple's names). The style resembles a design; it is not Apple's.
- **Clean room, MIT.** No code, resources or headers from Apple's SDKs. Numbers (sizes, radii, type ramp, system
  colours, spring parameters) come from the public Human Interface Guidelines and from measuring screenshots of running
  system apps; each number gets its source in a comment, as the Material tokens do. Nothing of Apple's is copied into
  the repository: no SF Symbols (their licence allows use only on Apple platforms), no SF Pro, no wallpapers, no
  screenshots in tests.
- **Fonts.** On iOS the style asks for the system font through Core Text at run time, which needs no file of ours.
  On Android it uses the system sans-serif (Roboto / Google Sans). A bundled font (Inter, OFL) would be third-party
  material and needs approval first; not planned.
- **Icons.** The application's own, recoloured as `QMaterialStyle::paintIcon()` does (one-colour icons take the
  style's colour). Nt Material Symbols stays optional and stays out of Nt Widgets.
- **Shared vocabulary.** `QGlassStyle` honours the `material*` dynamic properties by their role, so an application
  written for Material gets the Glass counterpart without changes (table below). Where Glass has something Material
  lacks, a `glass*` property is added; `QMaterialStyle` maps those back where it can. A neutral alias set (`ntRole`)
  can come later if the `material` prefix proves awkward; both styles would read it.
- **Code sharing.** `QGlassStyle` derives from `QCommonStyle`, as `QMaterialStyle` does. The spring solver
  (`qmaterialmotion_p.h`), the ripple-free state machinery, `paintIcon()`, the list-row and tab-indicator hooks and the
  window properties (`_nt_cornerRadius`, `_nt_sheet`) move to a style-neutral private header (`qtouchstyle_p.h`) used
  by both. Each hook that a widget calls on `QMaterialStyle` today becomes a virtual on a private base, so the
  widgets do not learn a second style.
- **Rendering on the CPU first.** Glass is drawn by the raster engine from the backing store, so it works on every
  platform Nt has (Linux offscreen for tests, Android, iOS). A GPU path waits for QRhi (`qrhi-plan.md`).

### Role mapping (Material property → Glass drawing)

| Material                                   | Glass                                                         |
|--------------------------------------------|---------------------------------------------------------------|
| `materialButton` filled / tonal / outlined / text | prominent glass (tinted) / glass / glass / plain       |
| `materialButtonGroup` = connected          | segmented control (one glass capsule, sliding selection)      |
| `materialSwitch`                           | switch                                                        |
| `materialToolBar` = appBar                 | navigation bar: large title that shrinks on scroll, glass buttons |
| `materialToolBar` = floating               | floating glass toolbar                                        |
| `materialTabBar` = navigationBar           | floating tab bar (iPhone); sidebar on a regular-width iPad    |
| `materialTabBar` = navigationRail          | sidebar                                                       |
| `materialList` = navigationDrawer          | sidebar list                                                  |
| `materialSheet` = bottom / drawer          | sheet with detents (glass while partial) / sidebar overlay    |
| `materialStatusBar` = snackbar             | glass banner at the top                                       |
| `materialTextField` = search               | glass search field (at the bottom on iPhone)                  |
| `materialCard`                             | inset grouped section                                         |
| `materialButton` = fab (+ menu)            | glass circle button; its menu as a glass menu                 |
| `materialChip`                             | small glass capsule button                                    |
| `materialProgress` loading / circular / wavy | activity indicator (spokes) / ring / plain bar            |

## The glass material

The effect lives in a private QtGui header (`src/gui/qglass_p.h`), independent of widgets, so the style, a future
Nt Quick and the tests use the same code. For a shape (rounded rectangle or capsule, later any path):

1. **Backdrop.** Read the pixels under the shape from the paint device. In a widget backing store the content below
   has been painted when a widget on top paints, so the backdrop is in the image already. Off a raster device (a
   picture, a printer) the material falls back to its opaque tint.
2. **Blur.** Downsample by 4 (2 on small shapes), three box passes (a Gaussian within a few percent), upsample
   bilinearly. Radius about 10 points for bars, less for small controls.
3. **Lens.** Near the rim the backdrop is refracted: sample it displaced inward by a profile of the distance to the
   edge (a quarter circle over the outer 8 to 16 points), with a slight split of red and blue at the very rim.
4. **Tint and light.** A tint over it (white or black at low opacity, or the accent colour for prominent glass), a
   specular rim brighter towards the light (top left by default; device motion later), and a soft shadow under
   floating elements.
5. **Legibility.** The mean luminance of the blurred backdrop decides whether the foreground of that element (labels,
   icons) is drawn light or dark, with a hysteresis so it does not flicker over busy content. This is what keeps text
   on glass readable and is the heart of the look.
6. **Merging.** Glass elements in one container that come close flow into one another: the union of their signed
   distance fields with a smooth minimum, so two capsules meet with a neck rather than overlap. Used by the tab bar,
   the toolbar groups and a menu growing out of its button.

**Accessibility settings** turn parts off: Reduce Transparency draws the frosted tint opaque, Increase Contrast adds a
border and darker foreground, Reduce Motion drops the lens wobble and merging animation.

**Cost.** An iPad Pro at 120 Hz, a 2732 × 2048 window at 2×: blurring only the glass areas at a quarter size, a tab
bar and a navigation bar are about 0.4 megapixels a frame before downsampling. Cache each element's result while
nothing under it changed (the backing store's dirty region tells), so a still screen costs nothing. Measure on the
Android emulator and a device before the GPU path is considered.

**To check first:** that Nt's backing store repaints a widget on top when a widget under it repaints (Qt does, by
the overlap of the update region), and that a scroll area's blit leaves the floating bar above it repainted.

## Steps

0. **iOS port** (needed for iPhone and iPad; iOS is not yet in the platform list in `AGENTS.md`, adding it is part of
   this step).
   - Toolchain: `cmake/NtIOS.cmake` and an `nt-ios.toolchain.cmake` like Android's, device (arm64) and simulator
     (arm64, x86_64). `qt_add_executable` makes an app bundle: generated `Info.plist`, launch screen, app icon,
     `CFBundleIdentifier` from `MACOSX_BUNDLE_GUI_IDENTIFIER`; signing by the developer's identity; `.ipa` packaging.
   - QPA platform `src/gui/platforms/ios`: `UIApplicationMain` glue that runs `main()` on its own stack as Qt does,
     one `UIView` per top-level window whose `CALayer` contents get the raster backing store (an `IOSurface` or
     `CGImage` per flush), touch as touch events with mouse emulation (the Android code is the model), safe-area
     insets, the soft keyboard via `UITextInput`, clipboard, lifecycle (background drops surfaces, as on Android),
     screen scale as device pixel ratio, dark mode, accent colour and Dynamic Type as style hints.
   - iPad: pointer hover and the trackpad (hover events, scroll), hardware keyboard and its shortcuts, Split View and
     resizable windows (size changes at run time), `QMenuBar` into the iPadOS menu bar, several scenes later.
   - OpenGL ES is deprecated on iOS and Vulkan absent; the GPU path is Metal via a QRhi backend, or MoltenVK
     (Apache-2.0, third-party, needs approval). Raster only in this plan.
   - **Needs a Mac**: Xcode's SDK may only be used on Apple hardware, so iOS builds and the simulator run on a Mac or
     a GitHub `macos` runner, not on the Linux box. The code is written and unit tested on Linux; a `macos` CI job
     builds for the simulator and runs a smoke test (`xcrun simctl`).
1. **Glass material** (`qglass_p.h`) with golden-image tests on the offscreen platform (backdrops of stripes and text,
   checking blur width, rim displacement and foreground choice by numbers, not by eye). Also the `QPainter`-level
   helper the style calls: `drawGlass(painter, path, GlassOptions)`.
2. **Theme.** System colours light and dark, the accent colour (seed: the platform's accent on iOS, the Material
   seed on Android), the type ramp in points (large title 34 … caption 11, body 17) scaled by Dynamic Type or the
   Android font scale, 44-point touch targets, corner radii concentric with the window's (the screen corner radius as
   a platform hint, a default where unknown).
3. **Style: the common controls.** Buttons (glass, prominent, plain; sizes small to extra large), switches,
   sliders, steppers (a `QSpinBox`), segmented controls, check marks in lists (iOS has no check box: a `QCheckBox`
   draws as a switch by default, a check mark in a list), text fields, combo boxes as pop-up menus, menus and context
   menus as glass panels, progress and activity indicators, inset grouped lists, tables and trees with disclosure
   chevrons, alerts (`QMessageBox`) and dialogs with the button order of iOS (`SH_DialogButtonLayout`).
4. **Bars and navigation.** Navigation bar with a large title that collapses as its scroll area scrolls (the style
   finds the scroll area under the bar), floating tab bar that shrinks on scroll down and returns on scroll up,
   floating toolbars with grouped items, the search field at the bottom on iPhone, sheets with detents (medium,
   large; glass while not full height, dragging between detents), the sidebar, the banner.
5. **Size classes.** Compact (iPhone, iPad in narrow Split View) and regular (iPad). The style reads the top-level
   window's width (the same breakpoints as Material's window size classes, 600 and 840) and turns the tab bar into a
   sidebar and sheets into centred form sheets in regular width. `QMaterialStyle` gets the same switch (rail and
   expanded rail), so the adaptation is one rule for both.
6. **Motion.** Apple's springs are given as response and damping fraction; stiffness = (2π / response)², damping
   = 4π · ζ / response, so the existing closed-form solver serves. Selection sliding in segmented controls and tab
   bars, glass merging and splitting, buttons that swell under the finger, sheets, menus growing out of their button.
7. **Write once, deploy twice.** One example application in nt6-examples (`touch/`) with a tab bar, a list, a detail
   page, a sheet and a settings page, built from one source for Android (Material and Glass) and iOS (Glass).
   `doc/glass.h` explains the role mapping and the per-platform default, and how to force one style
   (`-style`, `QT_STYLE_OVERRIDE`).

## Testing

- `tests/test_glass.cpp`: the material by numbers, every control drawn in both themes, property mapping (each
  `material*` value gives its Glass drawing), size-class switching, motion settling, and that the Material tests
  still pass after the shared code moves.
- The Android emulator pass from the Android port (home/resume, rotation) with the Glass style, frame cost measured
  as for Material.
- iOS: simulator on the `macos` runner; iPhone and iPad on a device when one is to hand.
- CI has no FreeType text in some jobs: guard text-pixel checks as the Material tests do.

## Open points

- A Mac for the iOS work (local or CI only).
- Whether a GPU glass path is wanted for 120 Hz iPads, and then Metal or MoltenVK (approval).
- Whether to introduce the neutral `ntRole` properties now or after both styles exist.
- The newest iOS switch, slider and tab bar sizes changed with iOS 26; measure them on a device rather than trust
  older HIG figures.
