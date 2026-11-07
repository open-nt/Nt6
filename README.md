<p align="center"><img src="assets/nt-logo.svg" alt="Nt logo" width="160"></p>

<p align="center"><a href="https://open-nt.github.io/Nt6/"><img src="https://open-nt.github.io/Nt6/badges/version.svg" alt="Nt version"></a></p>

<p align="center">

[![Ubuntu 22.04](https://github.com/open-nt/Nt6/actions/workflows/ubuntu-22.04.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/ubuntu-22.04.yml)
[![Ubuntu 24.04](https://github.com/open-nt/Nt6/actions/workflows/ubuntu-24.04.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/ubuntu-24.04.yml)
[![Ubuntu 26.04](https://github.com/open-nt/Nt6/actions/workflows/ubuntu-26.04.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/ubuntu-26.04.yml)
[![Enterprise Linux 9](https://github.com/open-nt/Nt6/actions/workflows/el9.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/el9.yml)
[![Enterprise Linux 10](https://github.com/open-nt/Nt6/actions/workflows/el10.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/el10.yml)
[![Arch Linux](https://github.com/open-nt/Nt6/actions/workflows/arch.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/arch.yml)
[![Docs](https://github.com/open-nt/Nt6/actions/workflows/docs.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/docs.yml)
[![SLSA](https://github.com/open-nt/Nt6/actions/workflows/slsa.yml/badge.svg?branch=main)](https://github.com/open-nt/Nt6/actions/workflows/slsa.yml)
[![SLSA 3](https://slsa.dev/images/gh-badge-level3.svg)](https://slsa.dev)

</p>

# Nt ("Nut")

A small, MIT-licensed, clean-room implementation of the Qt 6 API, built module by module: `Nt Core` (partial) and `Nt Network` (partial) so far. Not affiliated with The Qt Company; no Qt source code is used. Statically linked by default (`-DBUILD_SHARED_LIBS=ON` for a shared library). Linux/POSIX only for now.

**Documentation:** <https://open-nt.github.io/Nt6/>

**Branding:** logo and naming rules are in the [Nt Brand Guide](https://open-nt.github.io/Nt6/brand.html); the artwork is in [`assets/brand/`](assets/brand).

## Status

Qt Core has roughly 300 classes; Nt Core has about 100 of them, chosen first for what Nt Network needs. **Nt Core is not complete.**

### Nt Core: implemented

| Area | Classes |
|---|---|
| Object model | `QObject` (parent/child, `connect`/`disconnect` for member slots, functors and context objects, `blockSignals`, `sender()`, `deleteLater()`, `destroyed`, `objectName`, `findChild(ren)`, timers), `QMetaObject::Connection`, `qobject_cast`, `Qt::ConnectionType` (`Auto`, `Direct`, `Queued`), `QFlags`, `qOverload` |
| Events and loop | `QEvent`, `QTimerEvent`, `QEventLoop`, `QCoreApplication` (`exec`, `quit`, `postEvent`, `sendEvent`, `processEvents`, `aboutToQuit`, `qApp`), `QTimer` (incl. `singleShot`), `QSocketNotifier` (fd readiness via `poll()`) |
| Text and bytes | `QString` (UTF-16, `arg`, `split`, `number`, `toInt`/`toDouble`, UTF-8/Latin-1 conversion), `QChar`, `QLatin1Char`, `QLatin1StringView`, `QStringView`, `QByteArray` (hex, Base64, percent-encoding, number conversion), `QByteArrayView`, `QByteArrayList`, `QStringList`, `QTextStream` (UTF-8) |
| Containers | `QList`/`QVector`, `QQueue`, `QStack`, `QMap`, `QMultiMap`, `QHash`, `QMultiHash`, `QSet`, `QVarLengthArray`, `QBitArray`, `QCache`, `QContiguousCache`, `QSpan`, the Java-style iterators (`QListIterator`, `QMutableListIterator`, `QMapIterator`, `QMutableMapIterator`, `QHashIterator`, `QSetIterator`, ...) |
| Smart pointers | `QPointer`, `QScopedPointer`, `QScopedArrayPointer`, `QSharedPointer`, `QWeakPointer`, `QEnableSharedFromThis`, `QSharedData`, `QSharedDataPointer`, `QExplicitlySharedDataPointer`, `QScopeGuard`, `QScopedValueRollback`, `QSignalBlocker` |
| Threading primitives | `QAtomicInt`, `QAtomicInteger`, `QAtomicPointer`, `QMutex`, `QRecursiveMutex`, `QMutexLocker`, `QReadWriteLock`, `QReadLocker`, `QWriteLocker`, `QWaitCondition`, `QSemaphore`, `QSemaphoreReleaser` |
| Time and diagnostics | `QElapsedTimer`, `QDeadlineTimer`, `QBasicTimer`, `QDate`, `QTime`, `QDateTime` (UTC, local time and fixed offsets), `QDebug`, `qDebug()`/`qInfo()`/`qWarning()`/`qCritical()`/`qFatal()`, `qInstallMessageHandler` |
| I/O and file system | `QIODevice`, `QIODeviceBase`, `QBuffer`, `QFileDevice`, `QFile`, `QDataStream` (Qt's wire format), `QFileInfo`, `QDir`, `QSaveFile`, `QTemporaryFile`, `QTemporaryDir`, `QStandardPaths` (XDG) |
| Geometry | `QPoint`/`QPointF`, `QSize`/`QSizeF`, `QRect`/`QRectF`, `QLine`/`QLineF`, `QMargins`/`QMarginsF` |
| Utilities | `QUuid`, `QVersionNumber`, `QRandomGenerator`, `QCryptographicHash` (MD4, MD5, SHA-1, SHA-2, SHA-3, Keccak), `QMessageAuthenticationCode`, `QCommandLineParser`, `QCommandLineOption` |

Known limits: `QTextStream` reads and writes UTF-8 only and `QDataStream` has no transactions or 64-bit container sizes; `QDir` name filters are case-insensitive and `QFileInfo` has no birth time; `QDateTime` has no `QTimeZone` (only local time, UTC and fixed offsets); `QChar` case mapping and classification cover ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic only; `QList<bool>` is rejected at compile time; all `QObject`s must be used from one thread (only `postEvent` is thread-safe); no `QMetaObject` introspection, properties or `SIGNAL()`/`SLOT()` strings.

### Nt Core: not implemented yet

- **Text:** `QAnyStringView`, `QUtf8StringView`, `QStringConverter`/`QStringEncoder`/`QStringDecoder`, `QStringMatcher`, `QStringTokenizer`, `QLocale`, `QCollator`, `QTextBoundaryFinder`, `QRegularExpression` (+ match classes), `QTranslator`, `QStringListModel`
- **Containers:** `QKeyValueIterator`
- **Variant and meta:** `QVariant`, `QMetaType`, `QMetaObject` introspection, `QMetaProperty`/`QMetaMethod`/`QMetaEnum`/`QMetaClassInfo`, `QProperty` and bindable properties, `QTypeRevision`, `QSequentialIterable`/`QAssociativeIterable`, `QObjectCleanupHandler`, `QSignalMapper`
- **Date and time:** `QTimeZone`, `QCalendar`, `QChronoTimer`, `QTimeLine`
- **File system:** `QDirIterator`, `QDirListing`, `QLockFile`, `QFileSystemWatcher`, `QFileSelector`, `QStorageInfo`, `QResource`, `QSettings`
- **Processes and plugins:** `QProcess`, `QProcessEnvironment`, `QLibrary`, `QPluginLoader`, `QSharedMemory`, `QSystemSemaphore`
- **Formats:** `QJsonValue`/`QJsonObject`/`QJsonArray`/`QJsonDocument`, `QCbor*`, `QXmlStreamReader`/`QXmlStreamWriter` (+ helpers), `QMimeDatabase`/`QMimeType`/`QMimeData`
- **Concurrency:** `QThread` (and per-thread event loops / cross-thread queued signals), `QThreadPool`, `QRunnable`, `QThreadStorage`, `QFuture`/`QPromise`/`QFutureWatcher`/`QFutureSynchronizer`, `QException`, `QEventLoopLocker`, `QAbstractEventDispatcher`
- **Item models:** `QAbstractItemModel`, `QAbstractListModel`, `QAbstractTableModel`, `QAbstractProxyModel`, `QSortFilterProxyModel`, `QIdentityProxyModel`, `QConcatenateTablesProxyModel`, `QTransposeProxyModel`, `QModelIndex`, `QPersistentModelIndex`, `QItemSelection*`, `QRangeModel`
- **Animation:** `QAbstractAnimation`, `QVariantAnimation`, `QPropertyAnimation`, `QPauseAnimation`, `QSequentialAnimationGroup`, `QParallelAnimationGroup`, `QAnimationGroup`, `QEasingCurve`
- **System and misc:** `QSysInfo`, `QOperatingSystemVersion`, `QLoggingCategory`, `QMessageLogger`, `QChildEvent`, `QDynamicPropertyChangeEvent`, `QDebugStateSaver`, `QGlobalStatic`, `QBEInteger`/`QLEInteger`, `qfloat16`, `QPartialOrdering`, permission classes, JNI and Windows-only classes

### Nt Network: implemented

| Class | Notes |
|---|---|
| `QHostAddress` | IPv4/IPv6 parse and format, special addresses (`Any` is dual-stack), `isEqual` with conversion modes, loopback/multicast/link-local/global checks |
| `QAbstractSocket` | state machine, `connectToHost` (IP or name), `bind`, `disconnectFromHost` (flushes pending writes first), buffered async I/O through the event loop, `waitFor*` blocking API, read-buffer limit, socket options, `errorOccurred`, `stateChanged`, `hostFound` |
| `QTcpSocket` | client and accepted-connection sockets |
| `QTcpServer` | `listen`, `newConnection`, `nextPendingConnection`, `waitForNewConnection`, pause/resume accepting, pending-connection limit |
| `QUdpSocket` | `bind`, `writeDatagram`, `readDatagram`, `receiveDatagram`, connected mode, IPv4/IPv6 multicast join/leave |
| `QNetworkDatagram` | data, sender, destination, `makeReply` |
| `QHostInfo` | blocking `fromName`, asynchronous `lookupHost` (functor, with optional context object), `abortHostLookup`, local host and domain names |
| `QNetworkInterface`, `QNetworkAddressEntry` | list interfaces, flags, type, MTU, hardware address, addresses, netmask, broadcast (Linux) |
| `QLocalSocket`, `QLocalServer` | Unix domain sockets: event-driven and `waitFor*` APIs, access options, stale-socket recovery |
| `QNetworkAccessManager`, `QNetworkRequest`, `QNetworkReply` | HTTP/1.1 client over `http:` and `https:`: `GET`/`HEAD`/`POST`/`PUT`/`DELETE`/custom verbs, chunked and fixed-length bodies, redirects with policies, inactivity timeouts, Basic auth from the URL |
| `QSslSocket`, `QSslServer`, `QSslConfiguration`, `QSslCertificate`, `QSslKey`, `QSslError`, `QSslCipher` | TLS 1.2 and 1.3 through the system OpenSSL (1.1.1 or newer): certificate and host name verification, client certificates, `STARTTLS`, ALPN. Older protocol versions are not offered |

Differences from Qt: `QAbstractSocket::connectToHost` resolves names synchronously, although `QNetworkAccessManager` and `QHostInfo::lookupHost` do not block; `setSocketOption`/`socketOption` take and return `int` instead of `QVariant` until `QVariant` exists; `QUdpSocket` does not report the destination address or hop limit of received datagrams.

### Nt Network: not implemented yet

`QDnsLookup`, `QNetworkProxy`, `QSctpSocket`, `QAuthenticator`, `QNetworkCookie`/`QNetworkCookieJar`, `QHttpMultiPart`, `QDtls`, HTTP/2, connection reuse and response compression.

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

## TLS

Nt Network links the system OpenSSL when it is found, and builds without TLS otherwise, with `QT_NO_SSL` defined and `https:` URLs failing with `ProtocolUnknownError`. Configure with `-DNT_USE_OPENSSL=OFF` to leave it out on purpose. Install `libssl-dev` (Debian, Ubuntu), `openssl-devel` (Enterprise Linux, Fedora) or `openssl` (Arch) to build with it.

## Build

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Requires C++17 and a POSIX system. Link with `Nt::Core` and `Nt::Network` (or the `Qt6::Core` / `Qt6::Network` aliases).

Release is the default build type. Builds add stack-protector, `_FORTIFY_SOURCE` and RELRO hardening (`-DNT_HARDENING=OFF` disables it), and `-DNT_SANITIZE="address;undefined"` builds with sanitizers.

### Contributing

Run `./check.sh` before every commit (`./check.sh --install-hook` installs a pre-commit hook). It builds with `-Werror`, runs the tests under ASan and UBSan, requires complete NDoc comments, and runs clang-format, clang-tidy and cppcheck. It needs `cmake`, a C++17 compiler, `clang-format`, `clang-tidy` and `cppcheck`; `ninja` is used when present.

## Packages

Install instructions for every system below are in the [Installing Nt](https://open-nt.github.io/Nt6/install.html) guide. CPack builds the native packages on the distribution they are for, in CI and locally:

| Distribution | Packages | Workflow |
|---|---|---|
| Ubuntu 22.04, 24.04, 26.04 | `libnt6-dev_<version>-1~ubuntu<release>_amd64.deb`, `nt6-tools_...deb` | `ubuntu-22.04.yml`, `ubuntu-24.04.yml`, `ubuntu-26.04.yml` |
| Enterprise Linux 9 and 10 (RHEL, Rocky, Alma) | `nt6-devel-<version>-1.el<N>.x86_64.rpm`, `nt6-tools-...rpm` | `el9.yml`, `el10.yml` |
| Arch Linux | `nt6-<version>-1-x86_64.pkg.tar.zst` | `arch.yml` |

`libnt6-dev` / `nt6-devel` holds the headers (in `/usr/include/nt6`, so Nt can sit next to Qt), the static libraries and the CMake package; `nt6-tools` holds `ntmoc` and `ndoc`. Arch has one package with everything. In a project:

```cmake
find_package(Nt6 6 REQUIRED)
target_link_libraries(app PRIVATE Nt6::Network)   # Nt::Network works too
nt_enable_automoc(app)
```

To build the packages yourself:

```sh
packaging/build-packages.sh              # every distribution above, in Docker containers
packaging/build-packages.sh el9 arch     # some of them
packaging/build.sh                       # this machine only, no container
```

Both write to `dist/`. Or run CPack directly: `cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build && cpack --config build/CPackConfig.cmake` picks `.deb`, `.rpm` or pacman from `/etc/os-release`; use `-G DEB`, `-G RPM` or `-G External` (pacman, through `cmake/NtPacman.cmake`) to choose.

### Release provenance

Pushing a `v*` tag runs `slsa.yml`: it builds the packages for all six systems, generates [SLSA](https://slsa.dev) level 3 provenance with the [SLSA GitHub generator](https://github.com/slsa-framework/slsa-github-generator), and attaches the packages and `multiple.intoto.jsonl` to the GitHub release. Check a download with [slsa-verifier](https://github.com/slsa-framework/slsa-verifier):

```sh
slsa-verifier verify-artifact libnt6-dev_6.1.0-1~ubuntu24.04_amd64.deb \
    --provenance-path multiple.intoto.jsonl --source-uri github.com/open-nt/Nt6 --source-tag v6.1.0
```
