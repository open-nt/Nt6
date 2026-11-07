<p align="center"><img src="assets/nt-logo.svg" alt="Nt logo" width="160"></p>

# Nt ("Nut")

A small, MIT-licensed, clean-room implementation of the Qt 6 API, built module by module: `Nt Core` (partial) and `Nt Network` (partial) so far. Not affiliated with The Qt Company; no Qt source code is used. Statically linked by default (`-DBUILD_SHARED_LIBS=ON` for a shared library). Linux/POSIX only for now.

## Status

Qt Core has roughly 300 classes; Nt Core has 57 of them, chosen first for what Nt Network needs. **Nt Core is not complete.**

### Nt Core: implemented

| Area | Classes |
|---|---|
| Object model | `QObject` (parent/child, `connect`/`disconnect` for member slots, functors and context objects, `blockSignals`, `sender()`, `deleteLater()`, `destroyed`, `objectName`, `findChild(ren)`, timers), `QMetaObject::Connection`, `qobject_cast`, `Qt::ConnectionType` (`Auto`, `Direct`, `Queued`), `QFlags`, `qOverload` |
| Events and loop | `QEvent`, `QTimerEvent`, `QEventLoop`, `QCoreApplication` (`exec`, `quit`, `postEvent`, `sendEvent`, `processEvents`, `aboutToQuit`, `qApp`), `QTimer` (incl. `singleShot`), `QSocketNotifier` (fd readiness via `poll()`) |
| Text and bytes | `QString` (UTF-16, `arg`, `split`, `number`, `toInt`/`toDouble`, UTF-8/Latin-1 conversion), `QChar`, `QLatin1Char`, `QLatin1StringView`, `QByteArray` (hex, Base64, percent-encoding, number conversion), `QByteArrayList`, `QStringList` |
| Containers | `QList`/`QVector`, `QQueue`, `QStack`, `QMap`, `QMultiMap`, `QHash`, `QMultiHash`, `QSet` |
| Smart pointers | `QPointer`, `QScopedPointer`, `QScopedArrayPointer`, `QSharedPointer`, `QWeakPointer`, `QEnableSharedFromThis`, `QSharedData`, `QSharedDataPointer`, `QExplicitlySharedDataPointer`, `QScopeGuard`, `QScopedValueRollback`, `QSignalBlocker` |
| Threading primitives | `QAtomicInt`, `QAtomicInteger`, `QAtomicPointer`, `QMutex`, `QRecursiveMutex`, `QMutexLocker`, `QReadWriteLock`, `QReadLocker`, `QWriteLocker`, `QWaitCondition`, `QSemaphore`, `QSemaphoreReleaser` |
| Time and diagnostics | `QElapsedTimer`, `QDeadlineTimer`, `QDebug`, `qDebug()`/`qInfo()`/`qWarning()`/`qCritical()`/`qFatal()`, `qInstallMessageHandler` |
| I/O | `QIODevice`, `QIODeviceBase`, `QBuffer`, `QFileDevice`, `QFile` |

Known limits: `QChar` case mapping and classification cover ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic only; `QList<bool>` is rejected at compile time; all `QObject`s must be used from one thread (only `postEvent` is thread-safe); no `QMetaObject` introspection, properties or `SIGNAL()`/`SLOT()` strings.

### Nt Core: not implemented yet

- **Text:** `QStringView`, `QAnyStringView`, `QUtf8StringView`, `QByteArrayView`, `QStringConverter`/`QStringEncoder`/`QStringDecoder`, `QStringMatcher`, `QStringTokenizer`, `QLocale`, `QCollator`, `QTextStream`, `QTextBoundaryFinder`, `QRegularExpression` (+ match classes), `QTranslator`, `QStringListModel`
- **Containers:** `QVarLengthArray`, `QBitArray`, `QCache`, `QContiguousCache`, `QSpan`, Java-style iterators (`QListIterator`, `QMutableListIterator`, `QMapIterator`, ...), `QKeyValueIterator`
- **Variant and meta:** `QVariant`, `QMetaType`, `QMetaObject` introspection, `QMetaProperty`/`QMetaMethod`/`QMetaEnum`/`QMetaClassInfo`, `QProperty` and bindable properties, `QTypeRevision`, `QSequentialIterable`/`QAssociativeIterable`, `QObjectCleanupHandler`, `QSignalMapper`
- **Geometry:** `QPoint`/`QPointF`, `QSize`/`QSizeF`, `QRect`/`QRectF`, `QLine`/`QLineF`, `QMargins`/`QMarginsF`
- **Date and time:** `QDate`, `QTime`, `QDateTime`, `QTimeZone`, `QCalendar`, `QChronoTimer`, `QBasicTimer`, `QTimeLine`
- **File system:** `QDir`, `QDirIterator`, `QDirListing`, `QFileInfo`, `QSaveFile`, `QTemporaryFile`, `QTemporaryDir`, `QLockFile`, `QFileSystemWatcher`, `QFileSelector`, `QStandardPaths`, `QStorageInfo`, `QResource`, `QSettings`, `QDataStream`
- **Processes and plugins:** `QProcess`, `QProcessEnvironment`, `QLibrary`, `QPluginLoader`, `QSharedMemory`, `QSystemSemaphore`, `QCommandLineParser`, `QCommandLineOption`
- **Formats:** `QJsonValue`/`QJsonObject`/`QJsonArray`/`QJsonDocument`, `QCbor*`, `QXmlStreamReader`/`QXmlStreamWriter` (+ helpers), `QUrl`, `QUrlQuery`, `QUuid`, `QVersionNumber`, `QMimeDatabase`/`QMimeType`/`QMimeData`, `QCryptographicHash`, `QMessageAuthenticationCode`
- **Concurrency:** `QThread` (and per-thread event loops / cross-thread queued signals), `QThreadPool`, `QRunnable`, `QThreadStorage`, `QFuture`/`QPromise`/`QFutureWatcher`/`QFutureSynchronizer`, `QException`, `QEventLoopLocker`, `QAbstractEventDispatcher`
- **Item models:** `QAbstractItemModel`, `QAbstractListModel`, `QAbstractTableModel`, `QAbstractProxyModel`, `QSortFilterProxyModel`, `QIdentityProxyModel`, `QConcatenateTablesProxyModel`, `QTransposeProxyModel`, `QModelIndex`, `QPersistentModelIndex`, `QItemSelection*`, `QRangeModel`
- **Animation:** `QAbstractAnimation`, `QVariantAnimation`, `QPropertyAnimation`, `QPauseAnimation`, `QSequentialAnimationGroup`, `QParallelAnimationGroup`, `QAnimationGroup`, `QEasingCurve`
- **System and misc:** `QSysInfo`, `QOperatingSystemVersion`, `QLibraryInfo`, `QRandomGenerator`, `QLoggingCategory`, `QMessageLogger`, `QChildEvent`, `QDynamicPropertyChangeEvent`, `QDebugStateSaver`, `QGlobalStatic`, `QBEInteger`/`QLEInteger`, `qfloat16`, `QPartialOrdering`, permission classes, JNI and Windows-only classes

### Nt Network: implemented

| Class | Notes |
|---|---|
| `QHostAddress` | IPv4/IPv6 parse and format, special addresses (`Any` is dual-stack), `isEqual` with conversion modes, loopback/multicast/link-local/global checks |
| `QAbstractSocket` | state machine, `connectToHost` (IP or name), `bind`, `disconnectFromHost` (flushes pending writes first), buffered async I/O through the event loop, `waitFor*` blocking API, read-buffer limit, socket options, `errorOccurred`, `stateChanged`, `hostFound` |
| `QTcpSocket` | client and accepted-connection sockets |
| `QTcpServer` | `listen`, `newConnection`, `nextPendingConnection`, `waitForNewConnection`, pause/resume accepting, pending-connection limit |
| `QUdpSocket` | `bind`, `writeDatagram`, `readDatagram`, `receiveDatagram`, connected mode, IPv4/IPv6 multicast join/leave |
| `QNetworkDatagram` | data, sender, destination, `makeReply` |

Differences from Qt: host-name lookup is synchronous (blocks inside `connectToHost`); `setSocketOption`/`socketOption` take and return `int` instead of `QVariant` until `QVariant` exists; `QUdpSocket` does not report the destination address or hop limit of received datagrams.

### Nt Network: not implemented yet

`QHostInfo` (async DNS), `QDnsLookup`, `QNetworkInterface`/`QNetworkAddressEntry`, `QNetworkProxy`, `QLocalSocket`/`QLocalServer`, `QSslSocket` and the TLS classes, `QSctpSocket`, `QAuthenticator`, and the HTTP stack (`QNetworkAccessManager`, `QNetworkRequest`, `QNetworkReply`, `QNetworkCookie`, `QHttpMultiPart`, ...), which also needs `QUrl`.

```cpp
QTcpServer server;
server.listen(QHostAddress::LocalHost, 0);
QObject::connect(&server, &QTcpServer::newConnection, [&] {
    QTcpSocket *s = server.nextPendingConnection();
    QObject::connect(s, &QIODevice::readyRead, [s] { s->write(s->readAll().toUpper()); });
});
```

Header names match Qt: `#include <QObject>`, `#include <QtCore/QTimer>`, `#include <QtNetwork/QTcpSocket>`, `#include <QtCore/qobject.h>`.

## Qt 6 modules and their Nt names

The library is called **Nt**, not Qt: modules are `Nt Core`, `Nt Network`, and so on, and CMake targets are `Nt::<Module>` (`Nt::Core` and `Nt::Network` today). The C++ API keeps Qt's class names and headers (`QObject`, `<QtCore/QTimer>`) so Qt code compiles unchanged; `Qt6::Core` remains as an alias of `Nt::Core`.

✔ = Nt has a version (partial unless noted). Blank = not started. Module list checked against the Qt 6 module overview.

### Essentials

| Qt module | Nt library | Done |
|---|---|---|
| Qt Core | Nt Core | ✔ (partial, see Status) |
| Qt D-Bus | Nt D-Bus |  |
| Qt GUI | Nt GUI |  |
| Qt Network | Nt Network | ✔ (partial, see Status) |
| Qt Qml | Nt Qml |  |
| Qt Qml Core | Nt Qml Core |  |
| Qt Qml WorkerScript | Nt Qml WorkerScript |  |
| Qt Qml Models | Nt Qml Models |  |
| Qt Quick | Nt Quick |  |
| Qt Quick Controls | Nt Quick Controls |  |
| Qt Quick Dialogs | Nt Quick Dialogs |  |
| Qt Quick Layouts | Nt Quick Layouts |  |
| Qt Quick Test | Nt Quick Test |  |
| Qt Test | Nt Test |  |
| Qt Widgets | Nt Widgets |  |

### Add-ons

| Qt module | Nt library | Done |
|---|---|---|
| Qt Active Qt | Nt Active Qt |  |
| Qt Bluetooth | Nt Bluetooth |  |
| Qt Canvas Painter | Nt Canvas Painter |  |
| Qt CoAP | Nt CoAP |  |
| Qt Concurrent | Nt Concurrent |  |
| Qt Graphs | Nt Graphs |  |
| Qt GRPC | Nt GRPC |  |
| Qt Help | Nt Help |  |
| Qt HTTP Server | Nt HTTP Server |  |
| Qt Image Formats | Nt Image Formats |  |
| Qt Lottie Animation | Nt Lottie Animation |  |
| Qt MQTT | Nt MQTT |  |
| Qt Multimedia | Nt Multimedia |  |
| Qt Network Authorization | Nt Network Authorization |  |
| Qt NFC | Nt NFC |  |
| Qt OPC UA | Nt OPC UA |  |
| Qt OpenGL | Nt OpenGL |  |
| Qt Positioning | Nt Positioning |  |
| Qt Print Support | Nt Print Support |  |
| Qt Protobuf | Nt Protobuf |  |
| Qt Qml Compiler | Nt Qml Compiler |  |
| Qt Qml Design Support | Nt Qml Design Support |  |
| Qt Qml Network | Nt Qml Network |  |
| Qt Qml XmlListModel | Nt Qml XmlListModel |  |
| Qt Quick 3D | Nt Quick 3D |  |
| Qt Quick 3D Physics | Nt Quick 3D Physics |  |
| Qt Quick Effects | Nt Quick Effects |  |
| Qt Quick Particles | Nt Quick Particles |  |
| Qt Quick Timeline | Nt Quick Timeline |  |
| Qt Quick Widgets | Nt Quick Widgets |  |
| Qt Remote Objects | Nt Remote Objects |  |
| Qt SCXML | Nt SCXML |  |
| Qt Sensors | Nt Sensors |  |
| Qt Serial Bus | Nt Serial Bus |  |
| Qt Serial Port | Nt Serial Port |  |
| Qt Shader Tools | Nt Shader Tools |  |
| Qt Spatial Audio | Nt Spatial Audio |  |
| Qt SQL | Nt SQL |  |
| Qt State Machine | Nt State Machine |  |
| Qt SVG | Nt SVG |  |
| Qt TextToSpeech | Nt TextToSpeech |  |
| Qt UI Tools | Nt UI Tools |  |
| Qt Virtual Keyboard | Nt Virtual Keyboard |  |
| Qt Wayland Compositor | Nt Wayland Compositor |  |
| Qt WebChannel | Nt WebChannel |  |
| Qt WebSockets | Nt WebSockets |  |
| Qt WebView | Nt WebView |  |
| Qt XML | Nt XML |  |
| Qt 5 Compatibility APIs: Graphical Effects | Nt 5 Compatibility APIs: Graphical Effects |  |
| Qt 5 Core Compatibility APIs | Nt 5 Core Compatibility APIs |  |

### Add-ons in technology preview

| Qt module | Nt library | Done |
|---|---|---|
| Qt Labs Platform | Nt Labs Platform |  |
| Qt Labs StyleKit | Nt Labs StyleKit |  |
| Qt Location | Nt Location |  |
| Qt OpenAPI | Nt OpenAPI |  |
| Qt TaskTree | Nt TaskTree |  |

### Deprecated add-ons

| Qt module | Nt library | Done |
|---|---|---|
| Qt 3D | Nt 3D |  |
| Qt Charts | Nt Charts |  |
| Qt Data Visualization | Nt Data Visualization |  |

### Extension modules

| Qt module | Nt library | Done |
|---|---|---|
| Qt PDF | Nt PDF |  |
| Qt WebEngine | Nt WebEngine |  |

### Commercial modules

| Qt module | Nt library | Done |
|---|---|---|
| Qt Insight Tracker | Nt Insight Tracker |  |
| Qt VNC Server | Nt VNC Server |  |

## ntmoc: MIT moc replacement

Qt's moc is not used. `ntmoc` (`tools/ntmoc`) is a small MIT scanner that generates the bodies of signals declared without one:

```cpp
class Counter : public QObject {
    Q_OBJECT
public:
    void setValue(int v) { emit valueChanged(v); }
signals:
    void valueChanged(int value);   // body generated by ntmoc
};
```

It handles namespaces, nested classes, overloaded signals (use `qOverload`), default arguments and unnamed parameters. Signals with a hand-written body (`{ emitSignal(&Counter::valueChanged, value); }`) also work without ntmoc. Limitations: classes must have `Q_OBJECT`; signals of template classes are not supported; parameter names are guessed by a heuristic, not a C++ parser.

### CMake

AUTOMOC, driven by ntmoc (see `examples/moc`):

```cmake
add_subdirectory(Nt6)                      # or include Nt's CMakeLists.txt
add_executable(app main.cpp counter.h)
target_link_libraries(app PRIVATE Nt::Core)
nt_enable_automoc(app)                     # sets AUTOMOC and AUTOMOC_EXECUTABLE
```

Explicit form, like `qt_wrap_cpp`:

```cmake
nt_wrap_cpp(moc_sources counter.h)
add_executable(app main.cpp ${moc_sources})
```

ntmoc also accepts the moc command line CMake's AUTOMOC uses (`-o`, `-I`, `-D`, `@file`, `--output-dep-file`, `--version`), and is available as `Nt::moc` (and `Qt6::moc`).

Everything else (`signals:`, `slots:`, `emit`, `Q_OBJECT`, pointer-to-member `connect`, slots taking fewer arguments than the signal) is source compatible. Define `QT_NO_KEYWORDS` to use `Q_SIGNALS`/`Q_SLOTS`/`Q_EMIT`.

## NDoc: MIT documentation generator

Qt's qdoc is not used. `ndoc` (`tools/ndoc`) is a small MIT tool that generates Qt-style reference pages from `/*! */` comments written next to the code, in headers or source files:

```cpp
/*!
    Returns \c true if the timer is running; otherwise returns \c false.

    \sa start(), stop()
*/
bool QTimer::isActive() const { return m_id != 0; }
```

```sh
cmake --build build --target docs      # writes build/doc/html
```

The documentation is published to <https://open-nt.github.io/Nt6/> on every push to `main`.

The `docs` target fails on any broken link or bad parameter name. `coverage.txt` in the output lists public API that has no documentation yet. See [tools/ndoc/README.md](tools/ndoc/README.md) for the comment syntax. Write documentation from Nt's behaviour: Qt's documentation text is not MIT licensed, so it must not be copied or reworded.

## Also not supported

`SIGNAL()`/`SLOT()` string connections, `BlockingQueuedConnection`, `UniqueConnection`, and `Q_PROPERTY`/`Q_GADGET`/`Q_ENUM` (the macros compile to nothing, so `ntmoc` only generates signal bodies).

## Build

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Requires C++17 and a POSIX system. Link with `Nt::Core` and `Nt::Network` (or the `Qt6::Core` / `Qt6::Network` aliases).
