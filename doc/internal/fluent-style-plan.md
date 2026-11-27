# A Fluent style for Nt Widgets on Windows

Status: plan (2026-11-27). Nothing built. Not part of 6.0.x. Companion to `glass-style-plan.md` and
`material3-expressive-design.md`.

## Goal

The Windows 11 look (Microsoft's Fluent Design System, Fluent 2 as in WinUI 3) for Qt Widgets applications on
Windows, without changes to the application. Qt 6.7 and later ship this as the style `"windows11"`, so the key is
the same and a Qt application that asks for it gets it from Nt:

```cpp
QApplication::setStyle("windows11");   // the default on Windows 11; "Fluent" is accepted as well
```

With Material 3 (Android) and Glass (iOS) this gives each target its native look from one source, and Fusion stays
the neutral fallback on Linux.

## Decisions

- **Name.** The style key is `"windows11"`, Qt's. Qt's `QWindows11Style` is a plugin class, not public API, so Nt's
  class is the private `QFluentStyle` (`src/widgets/qfluentstyle*.cpp`), created by `QStyleFactory`. "Fluent" is a
  second key. No Microsoft logos or branding in the docs or the style.
- **Desktop first, not touch first.** Fluent's standard density: controls 32 high (24 in compact density), mouse and
  keyboard, hover states everywhere, a visible focus rectangle. Touch works through larger hit areas only where
  WinUI has them (sliders, toggles).
- **Design values.** Numbers come from the public Fluent 2 and WinUI documentation and from Microsoft's
  `microsoft/fluentui` and `microsoft/microsoft-ui-xaml` repositories, which are MIT. Only numbers are taken (radii,
  sizes, opacities, colour ramps, durations, easing curves), each with its source in a comment, as the Material
  tokens do; no code or resource files are copied. Copying MIT files would still be third-party material with its
  own notice, so it would need approval and `third_party/`.
- **Fonts and glyphs.** Segoe UI Variable and Segoe Fluent Icons come with Windows and may not be shipped. On
  Windows the style uses them from the system (as Qt's windows11 style does for chevrons, check marks and caption
  buttons); off Windows, for tests and for an application that forces the style on Linux, it falls back to the
  default sans-serif and draws its glyphs as paths. The Fluent System Icons (MIT) would make a good optional package
  like `nt6-material-symbols`, only with approval and never in Nt Widgets.
- **Shared code.** `QFluentStyle` derives from `QCommonStyle` and uses the style-neutral helpers planned in
  `glass-style-plan.md` (`qtouchstyle_p.h`: the spring solver, `paintIcon()`, the widget hooks as virtuals on a
  private base). Its in-app Acrylic uses the blur of `qglass_p.h` without the lens.
- **Shared vocabulary.** The `material*` properties map to their Fluent role where one exists (table below), so the
  same application gets a sensible desktop look; Fluent-only choices are `fluent*` properties.

### Role mapping (Material property → Fluent drawing)

| Material                                   | Fluent (WinUI control)                                       |
|--------------------------------------------|--------------------------------------------------------------|
| `materialButton` filled / tonal, outlined / text | accent button / standard button / subtle button        |
| `materialButtonGroup` = connected          | segmented control                                             |
| `materialSwitch`                           | toggle switch                                                 |
| `materialToolBar` = appBar                 | command bar under a title bar of Mica                         |
| `materialTabBar` = navigationRail / navigationBar | `NavigationView` left pane (compact or expanded) / top   |
| `materialTabBar` = secondary               | `TabView` (browser-like tabs on the title bar)                |
| `materialSheet` = drawer / bottom          | navigation pane overlay / content dialog                      |
| `materialStatusBar` = snackbar             | `InfoBar`                                                     |
| `materialTextField` = search               | `AutoSuggestBox` with a magnifier                             |
| `materialCard`                             | card (Layer fill, 1 stroke, corners 8)                        |
| `materialChip`                             | subtle toggle button                                          |
| `materialProgress` loading / circular / wavy | `ProgressRing` / `ProgressRing` / `ProgressBar`             |
| `materialBadge`                            | `InfoBadge`                                                   |

## Look

- **Materials.** Mica for the window background (tinted by the desktop wallpaper), Mica Alt behind tabbed title
  bars, Acrylic for flyouts, menus and tooltips. On Windows 11 22H2 and later the platform asks DWM for them
  (`DWMWA_SYSTEMBACKDROP_TYPE`: main window, tabbed, transient) and the style leaves those areas transparent; DWM
  does the blur, so it costs Nt nothing. Older Windows, Linux and inactive windows get Fluent's solid fallback
  colours. Reduce Transparency (Windows' "Transparency effects" off) gives the same fallback.
- **Colour.** Light and dark themes from the system (`AppsUseLightTheme`), the accent colour and its light and dark
  ramps (three steps each way) from the system, High Contrast themes honoured by using the system colours as they are.
- **Shape.** Corners of 4 on controls, 8 on cards, flyouts, dialogs and windows (DWM rounds top-level windows
  itself; `DWMWA_WINDOW_CORNER_PREFERENCE` for popups). Strokes of 1 with the elevation gradient (a darker bottom
  edge on controls, lighter on accent buttons), and shadows by elevation for flyouts (16) and dialogs (64).
- **Type.** The Fluent ramp: caption 12, body 14, body strong 14 semibold, subtitle 20, title 28, title large 40,
  display 68, scaled by the system text size setting.
- **Controls.** Buttons 32 high with 11 × 5 padding; check boxes and radio buttons of 20 whose glyph grows on hover
  and shrinks on press; toggle switch 40 × 20 with a knob that stretches while pressed; slider with a thumb of 20 and
  an inner dot that grows on hover; text boxes with an accent line of 2 at the bottom when focused; combo boxes that
  open their list over the current item; menus and context menus in Acrylic with 4 inset rows; tooltips; scroll bars
  that are thin lines until hovered; lists and trees with a pill of 3 × 16 at the left of the selected row; tab views;
  `QCalendarWidget` as the calendar view; focus rectangles of 2 (outer) and 1 (inner) for keyboard focus only.
- **Motion.** WinUI's durations (83, 167, 250, 333 ms) and its easing curves (fast out slow in, and the soft spring
  of the toggle and the navigation indicator); the selection pill slides between rows and navigation items. Windows'
  "Animation effects" off turns motion off.

## Steps

0. **Windows port.** Nt Core already has `_WIN32` paths in places; Nt has no Windows GUI platform yet.
   - Build: MSVC and clang-cl in CI (`windows` runner), `check.sh` gains a Windows job or a PowerShell twin for the
     build and tests; MinGW cross builds from Linux as a quick check where they cost little.
   - QPA platform `src/gui/platforms/windows`: Win32 windows with per-monitor DPI awareness v2, the raster backing
     store flushed with `UpdateLayeredWindow` or a DIB section (DirectComposition later for transparency with Mica),
     mouse, pointer (pen, touch), wheel, keyboard and IME (TSF or IMM32 first), clipboard, cursors, screens, dark mode
     and accent from the registry / `UISettings`, WGL and Vulkan surfaces (`VK_KHR_win32_surface`).
   - Title bar: Mica extended into a custom title bar (`DwmExtendFrameIntoClientArea`), with `WM_NCHITTEST` answering
     `HTMAXBUTTON` over the maximise button so Snap Layouts appear.
   - Packaging: `qt_add_executable` makes a `WIN32` executable with a manifest (DPI awareness, Common Controls 6, UTF-8
     code page); MSIX packaging later.
1. **Theme.** Colour tokens light, dark and high contrast with the accent ramps, type ramp, radii, strokes,
   elevations; system hints read through the platform (stubbed off Windows).
2. **Style: the common controls** (above), with Mica and Acrylic as fallbacks first so the whole style is testable on
   Linux offscreen.
3. **Materials through DWM.** Backdrop types per window, transparent areas in the backing store, popups with Acrylic,
   window corners, the custom title bar with Snap Layouts.
4. **Navigation and bars.** `NavigationView` (left, left compact, top) from the tab bar mapping with the moving
   selection indicator, command bar, `InfoBar`, `TabView`, content dialogs with WinUI's button order (primary,
   secondary, close; `SH_DialogButtonLayout` = `WinLayout`).
5. **Motion** with the shared solver and the WinUI curves.
6. **Example.** The same application as the glass plan's `touch/` example, built on Windows, showing that one source
   gives Material on Android, Glass on iOS and Fluent on Windows. `doc/fluent.h` describes the style and its
   differences from Qt's windows11 style.

## Testing

- `tests/test_fluent.cpp`: every control in light, dark and high contrast, hover, press and focus states by pixel
  numbers, the property mapping, and the solid fallbacks; runs on Linux offscreen (guard text pixels where CI lacks
  FreeType).
- On the `windows` runner: the platform smoke test, DPI changes between monitors (simulated), Mica requested on
  Windows 11 (checked through `DwmGetWindowAttribute`), and a comparison of control sizes with Qt's windows11 style
  where Qt is at hand.

## Open points

- A Windows machine or runner for the port (CI only is enough to start).
- Whether Windows 10 should get Fluent with solid fallbacks or the older Windows look (Qt gives it `windowsvista`).
- Fluent System Icons as an optional package (MIT, needs approval).
- Compact density as a property or as Qt's `QStyle::PM_*` answers only.
