# Nt GUI platform survey (Nest, Phase 0)

Survey of what Nt GUI, Nt Widgets and Nt Core already provide for a built-in,
single-process window system ("Nest"). No code was changed for this survey. It
covers the repository at 6.0.1 (main, 2026-11-15). The final section proposes
how the Nest task list should change in the light of it.

This file lives in `doc/internal/` because the repository has a `doc/`
directory, not `docs/`; ndoc only reads `.h` and `.cpp` files there, so this
Markdown page is not part of the published documentation.

## Summary

- Nt GUI already has a platform abstraction layer modelled on QPA, in tree since
  2026-11-08, with two built-in platforms: `xcb` and `offscreen`. Phase 1 of the
  Nest plan (define an "NPA", move the desktop backend behind it, write an
  offscreen plugin, run widgets headless in CI) is already done in substance.
- QPainter has a complete software path: a scanline rasteriser with
  antialiasing, stroking, dashes, gradients and bilinear image sampling, on
  32-bit QImages. Every widget paints through it; widgets never use OpenGL.
  Phase 2 is therefore an extension (16-bit and 8-bit formats, more
  composition modes, path clips, a blitter hook, benchmarks), not a rewrite.
- Menus, combo-box lists, completer lists and tool tips are child widgets inside
  the top-level window, not windows of their own. Modality is enforced in Nt
  Widgets. A Nest compositor therefore only has to stack top-level windows and
  dialogs; the popup semantics of Phase 5.5 are already solved one level up.
- Fonts are fontconfig plus FreeType, optional at build time. Without them text
  draws nothing. There is no kerning, no shaping and no glyph-cache cap.
- Nt Core on Zephyr already has the event dispatcher (POSIX `poll()` and
  `eventfd`), timers, socket notifiers, posted events and QThread. A GUI on
  Zephyr needs no new dispatcher, only a way to hand input events to the GUI
  thread. Nt GUI and Nt Widgets are not built for Zephyr today, and their
  optional dependencies (FreeType, zlib, libjpeg, XCB, EGL) are all absent there.
- There are no touch events, no clipboard, no input method, no drag and drop,
  no window flags or opacity on QWindow, and no child windows anywhere in Nt.

## 0.1 Windows, frames and input

### The platform layer

Nt GUI talks to the window system through a small clean-room equivalent of
QPA. The interfaces are private headers under `src/gui/qpa/`:

| Header | Interface | Role |
| --- | --- | --- |
| `qplatformintegration_p.h` | `QPlatformIntegration` | Entry point of a platform. Creates platform windows and backing stores, reports the colour scheme and cursor position, describes how EGL and Vulkan reach the window system. |
| `qplatformscreen_p.h` | `QPlatformScreen` | Geometry, depth, name, physical size, logical DPI, device pixel ratio. Wrapped by `QScreen`. |
| `qplatformwindow_p.h` | `QPlatformWindow` | Geometry, visibility, title, activation, transient parent, full screen, native id, cursor shape; holds the Vulkan surface. |
| `qplatformbackingstore_p.h` | `QPlatformBackingStore` | Paint device of one window (always a QImage so far), `flush(region)`, `resize()`, `beginPaint()`/`endPaint()`, `toImage()`. |
| `qwindowsysteminterface_p.h` | `QWindowSystemInterface` | Static handlers a platform calls: screen added/removed/changed, colour scheme, expose, geometry, close, activation, enter/leave, mouse, wheel, key. |
| `qdesktopportal_p.h` | `QDesktopPortalSettings` | Reads the desktop colour scheme over D-Bus (xcb only). |

`QtPrivate::GuiAccess` in `src/gui/qwindow_p.h` is the door from the platform
layer back into the private parts of `QWindow`, `QScreen` and
`QGuiApplication` (apply geometry, set exposed/active, register screens, set
focus window, cursor position, shut down).

What the Nest plan lists and Nt does not have: a `PlatformFontDatabase` (fonts
bypass the platform layer, see 0.2), `PlatformInputContext`,
`PlatformClipboard`, `PlatformServices` and `PlatformTheme`. The only theme
input is the colour scheme, which flows from the integration into
`QStyleHints`.

### Plugin selection

Platforms are compiled in and registered statically in `src/gui/qplatform.cpp`
(`QtPrivate::availablePlatforms()` and `createPlatformIntegration()`); there is
no `dlopen`, which already suits Zephyr. `QGuiApplication` picks the platform
from `-platform <name>` (also `--platform` and the `=` forms), then
`QT_QPA_PLATFORM`, then `xcb` if `DISPLAY` is set and `offscreen` otherwise,
with a message on stderr. The `xcb` platform is built when `NT_USE_XCB` is on
and pkg-config finds `xcb` (`NT_HAVE_XCB`). Platform options after a colon
(`xcb:foo`) are ignored.

### The two platforms

`src/gui/platforms/offscreen/` (about 190 lines): one 800x600 screen, device
pixel ratio from `QT_SCALE_FACTOR`, windows that exist only as ids, a backing
store that is an `ARGB32_Premultiplied` QImage filled white. Showing a window
sends an expose and activates it. Test hooks: `QOffscreenBackingStore::imageOf(window)`
returns the pixels, `QOffscreenWindow::cursorOf(window)` the cursor shape. Input
is injected by the tests directly through `QWindowSystemInterface::handle*()`
(ten test files do this; `tests/test_widgets.cpp` alone has 21 such calls).
OpenGL on offscreen uses `EGL_PLATFORM_SURFACELESS_MESA`, so each window
renders into a framebuffer object.

`src/gui/platforms/xcb/` (about 1400 lines): one connection, one screen
(scale from `Xft.dpi`), windows as X windows with WM_PROTOCOLS, WM_DELETE_WINDOW,
_NET_WM_PING, _NET_WM_NAME and PID, transient-for, full screen, cursor glyphs
from the X cursor font or libxcb-cursor loaded at run time, keyboard mapping
read from the server, double-click detection. The backing store is a QImage
pushed with `xcb_put_image` in chunks under the request size limit.

### Ownership

- `QGuiApplication` owns the `QPlatformIntegration` (a `unique_ptr` in its
  state object) and deletes it in `GuiAccess::shutdown()`.
- The integration owns its `QPlatformScreen` objects and announces them with
  `handleScreenAdded()`; `QGuiApplication` wraps each in a `QScreen`.
- `QWindow::create()` asks the integration for a `QPlatformWindow`, which the
  window owns and destroys in `close()`/`destroy()`. Showing a window again
  creates a new platform window.
- `QBackingStore` owns its `QPlatformBackingStore`. `QPaintDeviceWindow` owns
  a `QBackingStore` that it creates lazily on the first paint.
- A top-level `QWidget` owns a `QWidgetWindow` (a `QPaintDeviceWindow`).

### Threading

Everything runs on the thread of the application object. The xcb connection
reads events through a `QSocketNotifier` from the event loop and processes
them synchronously; `QWindowSystemInterface` handlers deliver with
`QCoreApplication::sendEvent()` before they return, not through the posted-event
queue. There is no asynchronous or threaded event path, which is the one thing
a Zephyr input backend (callbacks from an input thread or ISR) will need to add
(see 0.3). The offscreen backing-store map is behind a mutex only so tests can
read it from other threads.

### Presenting a frame

`QPaintDeviceWindow` accumulates a dirty `QRegion` from `update()` and expose
events, and on `QEvent::UpdateRequest` paints that region into the backing
store between `beginPaint()` and `endPaint()`, then calls `flush(region)`. Nt
therefore already tracks damage per window; a compositor can reuse the regions
that reach `QPlatformBackingStore::flush()` as the screen damage.

`QWidgetWindow::paintEvent()` paints the widget tree with
`WidgetAccess::paintRecursive()`, clipped to the dirty region and to each
widget's visible rectangle. `QWidget::paintEngine()` is the engine of the
top-level window's backing store, so every widget paints in software.
`QRasterWindow` adds nothing to `QPaintDeviceWindow`. `QOpenGLWindow` is a
separate class that renders with the OpenGL paint engine; there is no
`QOpenGLWidget`.

Documented `QWindow` differences that matter for Nest: no parent windows, no
window flags, no modality on `QWindow`, no minimum size, icon or opacity, no
window states other than full screen, no `requestUpdate()` throttling to the
display refresh.

### Popups, menus, tool tips and dialogs

This is the biggest departure from Qt and it simplifies Nest considerably.

- `QMenu::popup()` reparents the menu into the window of its parent (or the
  active window). A menu is a child widget and cannot reach beyond its window.
- `QComboBox` shows a `QComboPopup` child widget; `QCompleter` shows a
  `CompleterPopup` list widget; both register with
  `WidgetAccess::setPopup()`, and `QWidgetWindow` closes the popup on a press
  outside it (implicit grab and dismissal are handled inside the window).
- `QToolTip` is always inside the window and no wider than it. `QWidgetWindow`
  runs the tool-tip timer.
- Dialogs and any other top-level widget are separate `QWindow`s. A child
  top-level gets `setTransientParent()` on its platform window.
- Modality is enforced in Nt Widgets, not by the window system:
  `WidgetAccess::blockedByModal()` in `src/widgets/qapplication.cpp` keeps a
  list of shown modal windows and blocks mouse, keyboard and close for the
  windows they block. Those windows are still painted.
- `raise()` and `lower()` on a top level "ask the window system"; the platform
  interface has no stacking call yet, so on xcb it is whatever X does.

### Input that reaches a window

Mouse press, release, move and double click with buttons and modifiers; wheel
with pixel and angle deltas; key press and release with key, modifiers, text
and auto-repeat; enter and leave; activation; close; geometry change; expose.
`QGuiApplication` remembers the last buttons, modifiers and cursor position.

Absent: touch (`QTouchEvent` does not exist in Nt), tablet, input methods,
clipboard, drag and drop, keyboard or mouse grabs on `QWindow`, key mapping
tables other than the one read from the X server.

### Cursor

`QCursor` has shapes only (no pixmap cursors). `QPlatformWindow::setCursor(shape)`
shows it; `QPlatformIntegration::setCursorPosition()` moves it. Nothing in Nt
draws a cursor, so a Nest backend without a hardware cursor plane needs a
software cursor layer (Phase 5.8 stands).

### OpenGL and Vulkan

EGL is loaded at run time (`src/gui/qegl_p.h`), so Nt GUI links no GL library.
The integration reports an `EGL_PLATFORM_*` and native display, and whether
windows get EGL window surfaces or render into framebuffer objects. Vulkan
surfaces are created through `createVulkanSurface()`. Both are out of scope for
Nest on Zephyr; a DRM/GBM backend can reuse `eglPlatform()` later.

### Call sites to touch when adding a platform

- `src/gui/qplatform.cpp`: register the name and constructor.
- `CMakeLists.txt` near line 634: the `NT_USE_XCB` block is the model for an
  optional platform with pkg-config dependencies and `NT_HAVE_*` defines.
- `src/gui/qguiapplication.cpp`: `chooseDefaultPlatform()` if the default
  should change (for example `nest-drm` when `DISPLAY` is unset and
  `/dev/dri/card0` exists).
- `doc/gui.h` "Platforms" section and the `QGuiApplication` class comment list
  the platforms; ndoc runs with `--require-complete`.
- Tests set `QT_QPA_PLATFORM=offscreen` per test in `CMakeLists.txt`
  (lines 1425 to 1623); xcb-specific tests are `test_gui_xcb*.cpp` and
  `test_widgets_xcb.cpp`.

## 0.2 How QPainter renders

### Engines

`QPaintEngine` (`include/QtGui/qpaintengine.h`) is small: `begin`, `end`,
`updateState`, `drawRects`, `drawLines`, `drawPoints`, `drawEllipse`,
`drawImage`, `drawTransformedImage`, `drawAlphaMask`, `drawPath`, `type`.
QPainter does all the geometry (transforms, rounded rects, arcs, pies,
polygons, text placement) and feeds the engine device-space primitives.
Text is placed by `QTextLayout` and drawn glyph by glyph as 8-bit coverage
bitmaps through `drawAlphaMask()`, so a text backend only has to produce
`QGlyph` bitmaps.

| Device | Engine |
| --- | --- |
| `QImage`, `QPixmap` (a QImage inside) | `QRasterPaintEngine` |
| `QWidget`, `QPaintDeviceWindow`, `QRasterWindow` | the raster engine of the backing-store QImage |
| `QOpenGLPaintDevice`, `QOpenGLWindow` | `QOpenGL2PaintEngine` (stencil-then-cover, multisample AA) |

Both engines share the path flattening, stroking and integer-primitive
rectangle code in `QtRasterPrivate`, so they draw the same shapes.

### The software raster engine

`src/gui/qrasterpaintengine.cpp` (691 lines) plus `src/gui/qrasterizer.cpp`
(949 lines, header `qrasterizer_p.h`):

- Paths: lines and cubic Béziers, quads and SVG arcs converted, flattened to a
  tolerance; fill with non-zero or even-odd; coverage-based antialiasing on a
  premultiplied ARGB surface. Aliased drawing paints whole pixels.
- Strokes: width, butt/round/square caps, miter/round/bevel joins, miter limit,
  dash arrays and offsets; aliased strokes are shifted half a pixel.
- Brushes: solid colours and linear, radial and conical gradients with pad,
  reflect and repeat spreads through a colour table. No texture or pattern
  brushes.
- Images: `drawImage` with an opaque fast path; `drawTransformedImage` maps
  device pixels back through the inverse transform with nearest or bilinear
  sampling (`SmoothPixmapTransform`) and four-sample edge antialiasing.
- Clipping: a list of disjoint device rectangles (`setClipRect`,
  `setClipRegion`). No path clip.
- Composition: `SourceOver`, `Source` and `Clear` only, plus global opacity.
  The remaining Porter-Duff modes are not in the `CompositionMode` enum.
- Formats: `Format_RGB32`, `Format_ARGB32` and `Format_ARGB32_Premultiplied`
  only. `QImage::Format` has no 16-bit or 8-bit members, and
  `convertToFormat()` is a generic per-pixel loop.

Nt SVG and Nt Charts rasterise straight onto `QtRasterPrivate::Surface`, which
is its own ARGB vector, not a QImage; any new blitter hook should sit below
both.

### Fonts and text

`QFontEngine` (`src/gui/qfontengine.cpp`, 235 lines) is one font at one pixel
size: found through fontconfig, rasterised by FreeType with
`FT_LOAD_TARGET_LIGHT`, cached per `(font, scale)` and never freed. Glyphs are
8-bit coverage `QGlyph` bitmaps cached per engine in an unbounded map.
FreeType and fontconfig are optional (`NT_USE_FREETYPE`); without them the
engine reports monospace metrics and draws nothing. There is no kerning, no
shaping, no fallback fonts and no application-font loading (`QFontDatabase`
does not exist). `QTextLayout` (131 lines) breaks lines by advances only.

### Tests and benchmarks

`tests/test_painter.cpp`, `test_gui*.cpp`, `test_svg.cpp` and `test_charts.cpp`
check rendering by probing pixels with a colour tolerance of 3; there are no
checked-in reference PNGs and no benchmarks. `tests/image_fixtures.h` holds
small PNGs for the codec tests only.

## 0.3 Nt Core on Zephyr

### What works

Nt is a Zephyr module (`zephyr/module.yml`, `zephyr/Kconfig`,
`zephyr/CMakeLists.txt`). `CONFIG_NT` sets `NT_ZEPHYR=ON`, which builds Nt Core,
and with further options Nt Network, Nt GPIO, HTTP Server/WebSockets and TLS on
Mbed TLS. The sample for QEMU (`mps2/an385`) passes; a hello sample links for
`mimxrt1170_evk/mimxrt1176/cm7` at about 920 KB of code. Nothing has run on a
real board yet.

The event dispatcher (`src/dispatcher.cpp`) is POSIX: one `poll()` over the
socket notifiers plus an `eventfd` wake-up, with timers and posted events. On
Zephyr it copes with `CONFIG_ZVFS_POLL_MAX` by polling in groups. `QTimer`,
`QSocketNotifier`, `QEventLoop`, `QThread` (on a `std::mutex` family
implemented over `k_mutex`/`k_condvar`), `QThreadPool`,
`QtPrivate::startDetachedThread()` and queued signal delivery from a worker
thread to the main thread are all exercised by the QEMU sample.

### What a GUI needs that is missing

- `NtGui` and `NtWidgets` are inside `if(NOT NT_ZEPHYR)` in `CMakeLists.txt`
  (line 512 onward) and have never been compiled with the Zephyr SDK. They use
  `std::mutex`, `std::atomic`, `std::function`, `std::shared_ptr` and
  `std::unordered_map`, which the shim supports, but the GUI code must be
  audited for `std::thread` and `std::this_thread::get_id()` (forbidden on
  Zephyr) and for the `QT_NO_*` classes that are left out there (`QProcess`,
  `QLibrary`, `QFileSystemWatcher`, `QStorageInfo`, `QSharedMemory`).
- Every optional dependency of Nt GUI is absent: FreeType and fontconfig
  (text), zlib (PNG), libjpeg, XCB, EGL. The image codecs degrade gracefully;
  text does not.
- Input delivery from another context. Zephyr input callbacks run on the
  input thread (or in an ISR for some drivers). The cheapest bridge that fits
  the existing design is a lock-free or `k_msgq` queue drained by the GUI
  thread, woken either through a `QSocketNotifier` on an `eventfd` or a queued
  `QMetaObject::invokeMethod()` from a small Nt thread. A `k_poll`/`k_timer`
  dispatcher of its own is not needed.
- Memory. With 32-bit pixels only, a backing store costs 4 bytes per pixel:
  about 522 KB at 480x272, 1.5 MB at 800x480 and 2.4 MB at 1024x600, per window
  and again per screen buffer in the full-composition mode. The 16-bit format
  of Phase 2.4 and the direct and partial modes of Phase 6 are what make
  Zephyr targets fit; they should be scheduled before the Zephyr backend, as
  the plan already has them.
- Fonts on a device without a file system: the glyph atlas and font baking of
  Phase 3.3 are the realistic path; an embedded TTF rasteriser (3.2) is a
  fallback for boards with flash to spare.
- The examples repository (`nt6-examples`, where Zephyr samples live per
  `AGENTS.md`) is not checked out next to this one, so Phase 7's samples will
  go there rather than into a `samples/nest/` directory in this repository.

## 0.4 Proposed adjustments to the task list

The existing platform layer does not conflict with the Nest design, but it
makes Phase 1 almost entirely redundant and changes the shape of Phases 2, 3,
5 and 7. The proposal is to keep the layer and its names (`QPlatform*`,
`QWindowSystemInterface`, `src/gui/qpa/`, `src/gui/platforms/<name>/`) and
drop the "NPA" naming.

### Phase 1: replace with "Extend the platform layer"

Already done: 1.1 (minus the stubs), 1.2, 1.3, 1.4 and 1.5 (all GUI and widget
tests already run on `offscreen` in `check.sh` and CI).

Proposed 1.x items instead:

1. Add to `QPlatformWindow`: `raise()`, `lower()`, window flags and type
   (`Qt::Dialog`, `Qt::Tool`, `Qt::FramelessWindowHint`), opacity, and
   minimum size, with `QWindow` and `QWidget` plumbing and documented
   `\differences` updates. These are what a compositor needs to stack windows
   and what the desktop policy of 5.7 needs for decorations.
2. Add touch events: `QTouchEvent` in Nt GUI,
   `QWindowSystemInterface::handleTouchEvent()`, widget delivery (synthesised
   mouse events as a first step).
3. Add `QPlatformIntegration::createPlatformFontDatabase()` only when Phase 3
   introduces a second text backend; until then fonts stay where they are.
4. Give `offscreen` the test API the plan asks for in one place: a public
   `QtTest` helper (or a `QOffscreen*` private header) that injects
   input, grabs frames and lists windows with their stacking order, so Phase 5
   tests do not each reach into private headers.
5. Keep the stubs (`PlatformClipboard`, `PlatformServices`,
   `PlatformInputContext`) out until something needs them.

### Phase 2: scope as extensions of the existing engine

Already done: 2.1 (rasteriser, fill rules, antialiasing, stroking with joins,
caps and dashes), 2.5 (translate fast path, nearest and bilinear sampling), 2.6
gradients. Remaining or partial:

- 2.2: path clips (region and rect clips exist).
- 2.3: the other Porter-Duff modes; the enum has three values today.
- 2.4: `Format_RGB16`, `Format_Alpha8`, `Format_Grayscale8` (and probably
  `Format_RGB888` for SPI panels) across `QImage`, the codecs, the raster
  engine and `QOffscreenBackingStore`; fast conversion paths.
- 2.6: pattern brushes (`Qt::BrushStyle` patterns and texture brushes).
- 2.7: the `QNestBlitter` hook. Place it under both `QRasterPaintEngine` and
  `QtRasterPrivate::Surface`, since SVG and Charts draw on the latter.
- 2.8: benchmarks (none exist). Add a `tests/bench_*` target that `check.sh`
  does not time but CI records.
- Add: pixel-exact reference-image tests with a documented tolerance; today
  the tests probe single pixels.

### Phase 3: fonts

- 3.1 exists as `QFontEngine` on fontconfig and FreeType, but not behind a
  platform interface. Introduce a `QFontEngine` backend interface first, then
  the embedded rasteriser (3.2) and the baked atlas (3.3) as further backends.
- 3.4: add the memory cap and eviction to the existing glyph map, and free
  engines that are no longer referenced.
- 3.5: kerning from the `kern` or `GPOS` pair table; no shaping exists today
  and none is in scope.

### Phase 4: unchanged

Linux DRM and fbdev backends slot in as `src/gui/platforms/nest-drm/` and
`nest-fb/` following the xcb block in `CMakeLists.txt`. The VT handling (4.3),
evdev (4.4) and keymap (4.5) have no existing counterpart.

### Phase 5: narrower

- 5.1, 5.3, 5.4 (hit-test, focus, grabs across windows), 5.6 (modality is
  already enforced by Nt Widgets; the compositor only needs to respect it for
  raw `QWindow` clients), 5.7, 5.8, 5.9 stand.
- 5.2 can reuse the regions that already reach `QPlatformBackingStore::flush()`
  as per-window damage.
- 5.5 shrinks to "popup top-level windows for raw QWindow clients": menus,
  combo boxes, tool tips and completers are already children of their window.
  It should be re-checked if Nt Widgets later moves them to their own windows
  (which would let a menu extend past a small window, a real need on 320x240
  panels; decide this before Phase 5 starts).
- 5.10: the stacking, damage and focus tests go on `offscreen` through the
  helper from the new Phase 1 item 4.

### Phase 5b (new): Nest as a Wayland host

Added 2026-11-15 for the gva-hmi port: the display extensions are separate
processes whose windows the HMI must stack, clip and route input to, and a
Wayland client under GNOME cannot do that. `NtWaylandCompositor`, with the
API of Qt Wayland Compositor, lets the Nest application be the display
server instead. The first increment speaks the core protocol and xdg-shell
over Nt's own wire implementation (no libwayland) and is tested against
libwayland-client. Later: subsurfaces, dmabuf buffers, xdg-decoration, and a
`QWaylandSurface` item for Graphics View so client windows composite inside
the HMI scene.

### Phase 6: unchanged

Memory modes depend on 2.4 (16-bit formats) and on `QPlatformBackingStore`
gaining a way to paint into a shared screen buffer (direct mode) or a strip
(partial mode). Both fit the existing `beginPaint()`/`endPaint()`/`flush()`
protocol.

### Phase 7: smaller

- 7.1: extend the existing `zephyr/Kconfig` and `zephyr/CMakeLists.txt`
  (`CONFIG_NT_GUI`, `CONFIG_NT_WIDGETS`, `CONFIG_NT_NEST` with depth, mode,
  strip lines, font backend, blitter), and lift `NtGui`/`NtWidgets` out of the
  `if(NOT NT_ZEPHYR)` block behind those options. Add a first task: "compile
  Nt GUI and Nt Widgets with the Zephyr SDK, headless, on `offscreen`".
- 7.4 is not needed: use the existing dispatcher with an input queue and
  `eventfd` or queued invocation (see 0.3).
- 7.6: the sample goes to `nt6-examples/zephyr/`, not `samples/nest/` here.
- 7.5: `native_sim` with SDL display and input remains the CI target; its
  frame grabs can reuse the offscreen test helper if the Zephyr platform
  exposes `toImage()`.

### Phase 8: unchanged

### Ground rules

- "Pixel-exact tests against checked-in PNGs" is new for this repository;
  the images go under `tests/` and the comparison helper into Nt Test with a
  per-test tolerance.
- `check.sh` already enforces hygiene, `-Werror`, clang-tidy, cppcheck, full
  ndoc coverage and sanitizers; every Nest PR runs it.
- New public classes follow `AGENTS.md`: ndoc comments with `\differences`,
  `Q_OBJECT` headers processed by `ntmoc`, source files listed once in
  `CMakeLists.txt`.
