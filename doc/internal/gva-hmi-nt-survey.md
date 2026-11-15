# Building gva-hmi with Nt instead of Qt

Survey of `ldm/src/qt6/gva-hmi` (the GVA Human-Machine Interface) and the
libraries it links, against what Nt provides, as of 2026-11-15 (Nt 6.0.1).
It answers one question: can gva-hmi be built with Nt in place of Qt 6, and
if not, what has to change on which side.

## Summary

Not as it stands. The non-UI parts map onto Nt (Core, Network, Concurrent,
Test and the multimedia classes it uses), but the whole user interface is
built on Qt's Graphics View framework, which Nt does not have. That single
gap covers about 95k hand-written lines (the application and the
`gva-widgets` library), so the port is a matter of adding Graphics View to
Nt, not of patching the application. After that, a handful of small Nt GUI
gaps and two optional native pieces (the OpenGL 3D viewer and the X11 input
shaping) remain.

## What gva-hmi is

| Part | Size | Role |
| --- | --- | --- |
| `gva-hmi` | 133 files, about 48.7k lines (`HmiWindow.cpp` 9.4k) | A `QMainWindow` holding one `QGraphicsView` and `QGraphicsScene`; the GVA pages in `states/` add items to that scene. DDS registration, alarms, HUMS, power, video, 3D vehicle model. |
| `gva-widgets` (`src/qt6/widgets`) | 47k hand-written lines plus 155k lines of generated MIL symbol data | The GVA widget set. Every widget derives from `WidgetBase`, which is `QObject` plus `QGraphicsItem`; 78 of 99 headers use `QGraphics*`. |
| `qt6-gva-resource-lib` | 1.3k lines | Icons and fonts as resources. |
| `ldm10` generated Qt wrappers | 1174 headers | `QObject` publish/subscribe wrappers for the DDS topics, with `QMetaType` declarations. |
| `gva-display-ext-lib` | 4.9k lines | Listed in the README; uses `QQuickView`. Not on the gva-hmi link line. |

Qt modules: Core, Widgets, Concurrent, Network, Test; optionally
OpenGLWidgets (with Assimp), Multimedia and MultimediaWidgets; LinguistTools
for the `.ts` translations. Nt has all of these except OpenGLWidgets and the
Linguist tools.

The tests use `QSignalSpy`, data-driven rows (`QTest::addColumn`,
`QTest::newRow`, `QFETCH`) and `QTest::qWait`, which Nt Test supports.

## Gaps, largest first

### 1. Graphics View

Missing in Nt: `QGraphicsView`, `QGraphicsScene`, `QGraphicsItem`,
`QGraphicsObject`, `QGraphicsWidget`, `QGraphicsLinearLayout`, the rect,
path, pixmap, ellipse, line, simple-text and text items,
`QGraphicsProxyWidget`, `QGraphicsSvgItem`, `QGraphicsVideoItem`,
`QGraphicsDropShadowEffect`, the scene mouse, hover and wheel events, and
`QStyleOptionGraphicsItem`.

The surface the code calls is bounded. Scene and view methods, by number of
call sites: `addItem` 58, `removeItem` 20, `sceneRect` 18, `addText` 16,
`viewport` 15, `setBackgroundBrush` 10, `fitInView` 8, `transform` 5,
`mapFromScene` 4, `addRect` 4, `addPath` 4, `setRenderHint` 3,
`viewportTransform` 2, `setSceneRect` 2, `mapToScene` 2,
`itemsBoundingRect` 2, `items` 2, `addSimpleText` 2, then single uses of
`setViewportUpdateMode`, `setTransformationAnchor`, `setResizeAnchor`,
`setInteractive`, `setDragMode`, `addPixmap` and the scroll bar policies.

Item methods: `update` 1113, `setVisible` 368, `boundingRect` 259 (mostly
overrides), `paint` 189, `setPos` 105, `setZValue` 92,
`prepareGeometryChange` 77, `mousePressEvent` 64, `setOpacity` 57,
`setAcceptedMouseButtons` 30, `setCursor` 26, `hoverLeaveEvent` 25,
`setFlag` 23, `hoverEnterEvent` 18, `wheelEvent` 12, `sceneBoundingRect` 10,
`setAcceptHoverEvents` 9, `scenePos` 8, `shape` 7, `setParentItem` 7,
`itemAt` 7, `hoverMoveEvent` 7, `sceneEvent` 5, `centerOn` 5, `setScale` 4,
`setToolTip` 3, `setFocus` 3, `itemChange` 3, `setData` 1,
`setAcceptTouchEvents` 1. `setRenderHint` appears 2519 times, almost all on
a `QPainter` inside `paint()`.

### 2. The 3D vehicle viewer

`Vehicle3DWidget` is a `QOpenGLWidget` with legacy `glBegin()` calls,
`QOpenGLTexture` and Assimp, embedded in the scene through a proxy widget.
Nt has `QOpenGLWindow` and `QOpenGLPaintDevice` only. The CMake already
treats OpenGLWidgets as optional, so the viewer can be left out of a first
build.

### 3. X11 specifics

- `WindowInputShape` uses XShape to punch input holes in the frameless,
  translucent, full-screen window. It has a no-op fallback for other
  platforms.
- `HmiWindow` reads `QKeyEvent::nativeScanCode()` and `nativeVirtualKey()`
  to tell left from right modifier keys. Nt's `QKeyEvent` has neither.
- The window is `Qt::FramelessWindowHint` with `WA_TranslucentBackground`
  and `winId()` is used in ten places. Nt has no window flags, no
  translucent windows and no `QWidget::winId()`.

The first two are feature losses the application already tolerates; the
third depends on the Nest Phase 1 work (window flags on `QPlatformWindow`).

### 4. Small Nt GUI gaps

- `QFontDatabase::addApplicationFont()` loads the Nasalization font from a
  resource in three places. Nt finds fonts through fontconfig only.
- `QFontMetricsF` (Nt has `QFontMetrics`).
- `QImageReader` (Nt reads with `QImage::load()`).
- `setStyleSheet()` in 13 places (Nt has no style sheets).
- `QGraphicsDropShadowEffect` and `QGraphicsSvgItem` in gva-widgets.

### 5. Tooling

- Nt's `QTranslator` reads `.qm` files, but Nt has no `lrelease`; the `.qm`
  files have to be built with Qt's tool or checked in.
- `ntmoc` has never processed anything like the 1174 generated DDS wrapper
  headers. It should work, but it is untested at that scale.
- The `Qt6::Core`, `Qt6::Widgets` and other alias targets exist in Nt's
  CMake, so the application's own link lines need no change; only the
  `find_package(Qt6 ...)` calls do.

## What maps directly

`QString`, `QStringList`, `QMap`, `QSet`, `QHash`, the JSON classes,
`QSettings`, `QTimer`, `QProcess`, `QStandardPaths`, `QStorageInfo`,
`QTimeZone`, `QDateTime`, `QCommandLineParser`, `QRegularExpression`,
`QLoggingCategory`, `QMutex`, `QtConcurrent::run()` with `QFutureWatcher`,
`QNetworkAccessManager`, `QNetworkInterface`, `QSocketNotifier`,
`QMainWindow`, `QLabel`, `QPainter` with gradients and paths, `QPixmap`,
`QImage`, `QRegion`, `QTransform`, `QVector3D`, `QMatrix4x4`, `QScreen`,
`QCursor`, `QPalette`, `QTranslator`, and the multimedia classes
`QMediaPlayer`, `QAudioOutput`, `QAudioSource`, `QAudioFormat`, `QCamera`,
`QCameraDevice`, `QCameraFormat`, `QMediaCaptureSession` and
`QMediaDevices`.

## Recommendation

Rewriting gva-widgets away from Graphics View is not realistic at 47k lines.
The workable route, in order:

1. Add a Graphics View subset to Nt Widgets sized to the surface above:
   `QGraphicsScene`, `QGraphicsView`, `QGraphicsItem`, `QGraphicsObject`,
   the seven shape and text items, `QGraphicsProxyWidget`, the scene events,
   `QStyleOptionGraphicsItem`, then `QGraphicsSvgItem` (Nt SVG),
   `QGraphicsVideoItem` (Nt Multimedia Widgets) and
   `QGraphicsDropShadowEffect`.
2. Close the small gaps: `QFontDatabase::addApplicationFont()`,
   `QFontMetricsF`, `QImageReader`, native key codes on `QKeyEvent`, and
   window flags with a translucent top level (Nest Phase 1).
3. Build gva-widgets against Nt behind a CMake switch, then gva-hmi with the
   3D viewer and XShape left out.
4. `QOpenGLWidget` for the vehicle viewer as a later, separate task.

Graphics View also suits the Nest plan: a scene is a natural unit for a
single-process compositor, and this HMI, a frameless full-screen window at
1280x720 with its own input handling, is a realistic first Nest target on a
DRM display.
