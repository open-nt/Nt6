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
[![SLSA 3](https://slsa.dev/images/gh-badge-level3.svg)](https://slsa.dev)
[![Snap Store](https://snapcraft.io/open-nt6/badge.svg)](https://snapcraft.io/open-nt6)

</p>

<p align="left"><a href="https://snapcraft.io/open-nt6"><img src="https://snapcraft.io/en/dark/install.svg" alt="Get it from the Snap Store" height="40"></a></p>

# Nt ("Nut")

**Write Qt. Ship MIT.** Nt is a small, MIT-licensed, clean-room implementation of the Qt 6 API in C++17. It keeps Qt's class names and headers (`QObject`, `<QtCore/QTimer>`, `QHttpServer`), so code written for Qt compiles against Nt, and it links statically by default.

- **MIT, all the way down.** The libraries and the tools (`ntmoc`, `ntrcc`, `ndoc`) are MIT licensed, so you can link Nt statically into any program, open or closed.
- **Source compatible.** `#include <QTimer>`, `Q_OBJECT`, `signals:`, `connect()` and the `Qt6::` CMake targets work as they do in Qt.
- **Built module by module.** `Nt Core`, `Nt Network`, `Nt Serial Port`, `Nt SQL`, `Nt D-Bus`, `Nt HTTP Server`, `Nt WebSockets`, `Nt GUI`, `Nt Widgets`, `Nt SVG`, `Nt Charts`, `Nt OpenGL`, `Nt Qml`, `Nt Multimedia` (audio and cameras), `Nt FM`, `Nt Concurrent` and `Nt Test` so far; [Status](#status) lists what each one has and what is missing.
- **Checked on every commit.** Tests run under AddressSanitizer, UBSan and ThreadSanitizer, and release packages carry SLSA level 3 provenance.

Nt is young: Core, Network, GUI and Widgets are partial, and it runs on Linux/POSIX only for now. It is not affiliated with The Qt Company, and no Qt source code is used.

**[Documentation](https://open-nt.github.io/Nt6/)** · **[Install](https://open-nt.github.io/Nt6/install.html)** · **[Examples](examples)** · **[Contributing](CONTRIBUTING.md)** · **[Discussions](https://github.com/open-nt/Nt6/discussions)**

## Quick start

A web service in one file, `main.cpp`:

```cpp
#include <QtCore>
#include <QtHttpServer>
#include <QtNetwork>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QHttpServer server;
    server.route("/hello/<arg>", [](const QString &name) { return QString("Hello, %1!\n").arg(name); });

    auto *tcp = new QTcpServer;
    if (!tcp->listen(QHostAddress::LocalHost, 8080) || !server.bind(tcp))
        return 1;
    return app.exec();
}
```

Its `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.18)
project(hello LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Nt6 6 REQUIRED)

add_executable(hello main.cpp)
target_link_libraries(hello PRIVATE Nt6::HttpServer)
```

Build it, run it and call it:

```sh
cmake -S . -B build && cmake --build build
./build/hello &
curl http://127.0.0.1:8080/hello/Nt      # Hello, Nt!
```

Classes with `Q_OBJECT` need one more line, `nt_enable_automoc(hello)`; see [ntmoc](#ntmoc-mit-moc-replacement).

### Install

| System | Command |
|---|---|
| Ubuntu 22.04, 24.04, 26.04 | add the signed [apt repository](https://open-nt.github.io/Nt6/install.html), then `sudo apt install libnt6-dev nt6-tools` |
| Any Linux with snapd | `sudo snap install open-nt6 --channel=candidate` ([Snap Store](https://snapcraft.io/open-nt6); pass `-DCMAKE_PREFIX_PATH=/snap/open-nt6/current/usr` to CMake) |
| Enterprise Linux 9 and 10, Arch Linux | `.rpm` and pacman packages from the [releases](https://github.com/open-nt/Nt6/releases); see [Packages](#packages) |
| From source | `cmake -S . -B build && cmake --build build && sudo cmake --install build` |

Or use Nt without installing it: `add_subdirectory(Nt6)` in your project and link `Nt::HttpServer`.

### Widgets

Nt Widgets draws its own Fusion-like style, light and dark. This is [`hello_widgets`](examples/widgets/hello_widgets.cpp), one of every widget:

<p align="center"><img src="doc/images/hello-widgets-window.png" alt="The hello_widgets example, dark and light" width="720"></p>

### More examples

| Example | Shows |
|---|---|
| [`examples/hello`](examples/hello) | a standalone project that uses an installed Nt |
| [`examples/signals`](examples/signals), [`examples/moc`](examples/moc) | signals and slots, and `ntmoc` |
| [`examples/httpserver`](examples/httpserver/hello_server.cpp) | routes, JSON, streaming, delayed answers and WebSockets on one port |
| [`examples/websockets`](examples/websockets), [`examples/dbus`](examples/dbus), [`examples/serialport`](examples/serialport), [`examples/sql`](examples/sql) | an echo server and client, a clock on the session bus, serial ports, an address book in SQLite |
| [`examples/gui`](examples/gui), [`examples/widgets`](examples/widgets), [`examples/svg`](examples/svg), [`examples/charts`](examples/charts), [`examples/opengl`](examples/opengl) | a painted window, the widget gallery, SVG rendering, a live chart dashboard, a window rendered with OpenGL |
| [`examples/fm`](examples/fm) | game sound effects synthesised with FM, chip music and the QML types: [`fmplay`](examples/fm/fmplay.cpp) and [`fmqml`](examples/fm/fmqml.cpp) |

## Contents

- [Status](#status): what each module implements, and what it does not yet
- [Nt Only](#nt-only): modules, functions and platforms that Qt does not have
- [Qt 6 modules and their Nt names](#qt-6-modules-and-their-nt-names)
- [ntmoc](#ntmoc-mit-moc-replacement), [ntrcc](#ntrcc-mit-rcc-replacement) and [NDoc](#ndoc-mit-documentation-generator): the tools
- [Build](#build) and [Packages](#packages)
- [Contributing](CONTRIBUTING.md), [Security](SECURITY.md) and the [Nt Brand Guide](https://open-nt.github.io/Nt6/brand.html) (artwork in [`assets/brand/`](assets/brand))

## Status

Qt Core has roughly 300 classes; Nt Core has about 180 of them, chosen first for what the other Nt modules need. **Nt Core is not complete.**

### Nt Core: implemented

| Area | Classes |
|---|---|
| Object model | `QObject` (parent/child, `connect`/`disconnect` for member slots, functors, context objects and `SIGNAL()`/`SLOT()` strings, `blockSignals`, `sender()`, `deleteLater()`, `destroyed`, `objectName`, `findChild(ren)`, timers, thread affinity), `QMetaObject::Connection`, `qobject_cast`, `Qt::ConnectionType` (`Auto`, `Direct`, `Queued`), `QFlags`, `qOverload`, `QObjectCleanupHandler`, `QSignalMapper` |
| Meta-objects and variants | `QMetaObject` (generated by `ntmoc`; `invokeMethod()` by name, pointer to member function or functor), `QMetaMethod`, `QMetaProperty`, `QMetaEnum`, `QMetaClassInfo`, `QMetaType`, `QVariant`, `QSequentialIterable`, `QAssociativeIterable`, `QMetaSequence`, `QMetaAssociation` and `QMetaContainer` (for any container with `begin()`, `end()` and `value_type`), `QTypeRevision`, `QPartialOrdering` |
| Events and loop | `QEvent`, `QTimerEvent`, `QDynamicPropertyChangeEvent`, `QEventLoop`, `QEventLoopLocker`, `QCoreApplication` (`exec`, `quit`, `postEvent`, `sendEvent`, `processEvents`, `aboutToQuit`, `qApp`), `QTimer` (incl. `singleShot`), `QSocketNotifier` (fd readiness via `poll()`) |
| Text and bytes | `QString` (UTF-16, `arg`, `split`, `number`, `toInt`/`toDouble`, UTF-8/Latin-1/UCS-4 conversion, Unicode normalization and full case mapping), `QChar` (Unicode 16 categories, scripts, bidi classes, joining types, mirroring and age), `QLatin1Char`, `QLatin1StringView`, `QStringView`, `QAnyStringView`, `QUtf8StringView`, `QStringConverter`/`QStringEncoder`/`QStringDecoder`, `QStringMatcher`, `QByteArrayMatcher`, `QStaticByteArrayMatcher`, `QLatin1StringMatcher`, `QStringTokenizer`, `QRegularExpression` (+ `QRegularExpressionMatch` and its iterator), `QLocale` (about thirty locales, no CLDR), `QCollator`, `QTextBoundaryFinder`, `QTranslator` (`.qm` files, with `tr()` and `QCoreApplication::installTranslator()`), `QByteArray` (hex, Base64, percent-encoding, number conversion), `QByteArrayView`, `QByteArrayList`, `QStringList`, `QTextStream` (UTF-8, UTF-16, UTF-32 and Latin-1, BOM detection) |
| Containers | `QList`/`QVector`, `QQueue`, `QStack`, `QMap`, `QMultiMap`, `QHash`, `QMultiHash`, `QSet`, `QVarLengthArray`, `QBitArray`, `QCache`, `QContiguousCache`, `QSpan`, `QKeyValueIterator` (with `asKeyValueRange()`), `qHash()`/`qHashRange()`/`qHashMulti()`, the Java-style iterators (`QListIterator`, `QMutableListIterator`, `QMapIterator`, `QMutableMapIterator`, `QHashIterator`, `QSetIterator`, ...) |
| Smart pointers | `QPointer`, `QScopedPointer`, `QScopedArrayPointer`, `QSharedPointer`, `QWeakPointer`, `QEnableSharedFromThis`, `QSharedData`, `QSharedDataPointer`, `QExplicitlySharedDataPointer`, `QScopeGuard`, `QScopedValueRollback`, `QSignalBlocker` |
| Threads | `QThread` (per-thread event loops and queued signals across threads), `QThreadPool`, `QRunnable`, `QThreadStorage`, `QException`, `QAtomicInt`, `QAtomicInteger`, `QAtomicPointer`, `QAtomicScopedValueRollback`, `QMutex`, `QRecursiveMutex`, `QMutexLocker`, `QReadWriteLock` (recursive or not), `QReadLocker`, `QWriteLocker`, `QWaitCondition` (with `QMutex` or `QReadWriteLock`), `QSemaphore`, `QSemaphoreReleaser`; deadline and `std::chrono` overloads |
| Futures | `QFuture`/`QFuture<void>` (state, results, iteration, progress, `cancel`, `suspend`/`resume`, exceptions, `then`/`onCanceled`/`onFailed` with a launch policy, thread pool or context object, `cancelChain`), `QPromise`, `QFutureInterface`, `QFutureWatcher` (signals through the watcher's event loop, pending-results limit), `QFutureSynchronizer`, `QFutureIterator`, `QtFuture::makeReady*`/`makeExceptionalFuture`/`connect`/`whenAll`/`whenAny`, `QAbstractEventDispatcher` (a handle onto the thread's dispatcher: timers, `wakeUp`, `interrupt`, `aboutToBlock`/`awake`; from `instance()`, `QThread::eventDispatcher()` or `QCoreApplication::eventDispatcher()`), `QAbstractNativeEventFilter` (the events of the `xcb` platform, through `QCoreApplication::installNativeEventFilter()` or a dispatcher) |
| Time and diagnostics | `QElapsedTimer`, `QDeadlineTimer`, `QBasicTimer`, `QDate`, `QTime`, `QDateTime` (UTC, local time, fixed offsets and time zones), `QTimeZone` (the system's IANA database), `QCalendar` (Gregorian, Julian, Milanković, Jalali, Islamic civil), `QChronoTimer`, `QTimeLine`, `QEasingCurve`, `QDebug`, `QDebugStateSaver`, `qDebug()`/`qInfo()`/`qWarning()`/`qCritical()`/`qFatal()`, `qInstallMessageHandler`, `QMessageLogger`, `QLoggingCategory` |
| I/O and file system | `QIODevice` (with read transactions and read/write channels), `QIODeviceBase`, `QBuffer`, `QFileDevice`, `QFile` (permissions, file times, symbolic links), `QDataStream` (Qt's wire format, transactions), `QFileInfo`, `QDir`, `QDirIterator`, `QLockFile`, `QFileSystemWatcher` (inotify), `QSaveFile`, `QTemporaryFile`, `QTemporaryDir`, `QStandardPaths` (XDG), `QSettings` (INI), `QResource` (`:/` paths in `QFile`, `QFileInfo`, `QDir` and `QDirIterator`; resources compiled by `ntrcc`, compressed with Zstandard or zlib) |
| Processes and formats | `QProcess`, `QProcessEnvironment`, `QJsonValue`, `QJsonObject`, `QJsonArray`, `QJsonDocument`, `QMimeDatabase` and `QMimeType` (the freedesktop.org shared-mime-info database, or a built-in table of common types), `QCborValue`, `QCborArray`, `QCborMap`, `QCborStreamReader` and `QCborStreamWriter` (RFC 8949, incremental reading, the extended types, conversion to and from JSON and QVariant, diagnostic notation), `QXmlStreamReader` and `QXmlStreamWriter` (namespaces, entities of the internal DTD subset, incremental parsing, UTF-8/UTF-16/Latin-1) with `QXmlStreamAttributes` and the declaration classes |
| Animation | `QAbstractAnimation`, `QVariantAnimation` (key values, easing curves, `qRegisterAnimationInterpolator()`), `QPropertyAnimation`, `QPauseAnimation`, `QAnimationGroup`, `QSequentialAnimationGroup`, `QParallelAnimationGroup`; see also `QTimeLine` and `QEasingCurve` |
| Item models | `QAbstractItemModel`, `QAbstractListModel`, `QAbstractTableModel`, `QModelIndex`, `QPersistentModelIndex`, `QStringListModel`, `QItemSelectionModel`, `QItemSelection`, `QItemSelectionRange`, `QAbstractProxyModel`, `QIdentityProxyModel`, `QSortFilterProxyModel` (filtering, sorting, recursive filtering), `QTransposeProxyModel`, `QConcatenateTablesProxyModel`, `QMimeData` |
| Geometry | `QPoint`/`QPointF`, `QSize`/`QSizeF`, `QRect`/`QRectF`, `QLine`/`QLineF`, `QMargins`/`QMarginsF` |
| Utilities | `QUrl` (`fromUserInput()`, internationalized host names in Punycode) and `QUrlQuery`, `QUuid`, `QVersionNumber`, `QRandomGenerator`, `QCryptographicHash` (MD4, MD5, SHA-1, SHA-2, SHA-3, Keccak, BLAKE2b, BLAKE2s), `QMessageAuthenticationCode`, `QCommandLineParser`, `QCommandLineOption`, `QSysInfo`, `QOperatingSystemVersion`, `QLibraryInfo`, `QLibrary`, `QSharedMemory` and `QSystemSemaphore` (POSIX shared memory and named semaphores), `QGlobalStatic`, `QApplicationStatic` (`Q_APPLICATION_STATIC()`), `qfloat16`, `QBEInteger`/`QLEInteger`, `QPermission` with `QCameraPermission`, `QMicrophonePermission`, `QBluetoothPermission`, `QLocationPermission`, `QContactsPermission` and `QCalendarPermission` (`QCoreApplication::checkPermission()`/`requestPermission()`; granted, as there are no permission prompts), `qYieldCpu()` |
| Global functions and macros | the environment (`qgetenv()`, `qputenv()`, `qEnvironmentVariable()` and the rest), `<QtMath>` (`qSin()`, `qDegreesToRadians()`, `qNextPowerOfTwo()`, `M_PI`), `<QtNumeric>` (`qIsNaN()`, `qInf()`, `qFloatDistance()`, `qAddOverflow()`), `<QtAlgorithms>` (`qPopulationCount()`, `qCountTrailingZeroBits()`), `qAsConst()`, `qExchange()`, `qDeleteAll()`, `qMakePair()`, `qToUnderlying()`, the C string functions (`qstrcmp()`, `qstricmp()`, `qstrncpy()`, ...), `qCompress()`/`qUncompress()` (with zlib), `qAppName()`, `qErrnoWarning()`, `qAddPostRoutine()` and `Q_COREAPP_STARTUP_FUNCTION()`, `Q_UNREACHABLE()`, `Q_ASSUME()`, `Q_ASSERT_X()`, `Q_CHECK_PTR()`, `Q_FALLTHROUGH()`, `Q_DISABLE_COPY_MOVE()`, `Q_DECLARE_INTERFACE()`, `Q_DECLARE_TYPEINFO()`, `Q_STATIC_LOGGING_CATEGORY()` and the `Q_DECL_*` keywords |

Known limits: `QDataStream` has no 64-bit container sizes; `QDir` name filters are case-insensitive; `QTimeZone` has no localized names (no CLDR/ICU); no bindable properties (`QProperty`). Each class's reference page lists how it differs from Qt.

### Nt Core: not implemented yet

- **Meta:** `QProperty` and bindable properties
- **Plugins:** `QPluginLoader`, `Q_PLUGIN_METADATA`
- **Concurrency:** custom event dispatchers (`QThread::setEventDispatcher()`); QtConcurrent is in Nt Concurrent, below
- **Item models:** `QRangeModel`
- **System and misc:** JNI and Windows-only classes

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
| `QSslSocket`, `QSslServer`, `QSslConfiguration`, `QSslCertificate`, `QSslKey`, `QSslCipher`, `QSslError` | TLS with the system's OpenSSL (built when OpenSSL 1.1.1 or newer is found, `-DNT_USE_OPENSSL=OFF` to leave it out), or with Mbed TLS 4 on Zephyr (`CONFIG_NT_TLS`); `https:` in `QNetworkAccessManager`; the ciphers of a connection restricted with `setCiphers()`, and key exchange groups and other OpenSSL settings with `setBackendConfigurationOption()`, where a restriction that cannot be applied fails the handshake |
| `QLocalSocket`, `QLocalServer` | Unix domain sockets: event-driven and `waitFor*` APIs, access options, stale-socket recovery |
| `QHttpHeaders` | ordered, case-insensitive header fields; names and values are validated, so CR/LF cannot be injected |
| `QNetworkAccessManager`, `QNetworkRequest`, `QNetworkReply` | HTTP/1.1 client: `GET`/`HEAD`/`POST`/`PUT`/`DELETE`/custom verbs, chunked and fixed-length bodies, redirects with policies, inactivity timeouts, Basic auth from the URL |

Differences from Qt: `QAbstractSocket::connectToHost` resolves names synchronously, although `QNetworkAccessManager` and `QHostInfo::lookupHost` do not block; `QUdpSocket` does not report the destination address or hop limit of received datagrams.

### Nt Network: not implemented yet

`QDnsLookup`, `QNetworkProxy`, DTLS, `QSctpSocket`, `QAuthenticator`, `QNetworkCookie`/`QNetworkCookieJar`, `QHttpMultiPart`, HTTP/2, connection reuse and response compression.

### Nt SQL: implemented

Nt SQL gives access to SQL databases, with a built-in driver for SQLite on the system's `libsqlite3` (without it, the module is built without a driver).

| Class | Notes |
|---|---|
| `QSqlDatabase` | named connections that are found from anywhere, a default connection, `cloneDatabase()` for a connection per thread, transactions, `tables()`, `record()`, `primaryIndex()`, drivers of the application with `registerSqlDriver()` |
| `QSqlQuery` | statements with named (`:id`) and positional (`?`) placeholders, bound values that are never part of the SQL text, moving freely through the rows or forward-only, `execBatch()`, `lastInsertId()`, `finish()` |
| `QSqlError`, `QSqlRecord`, `QSqlField`, `QSqlIndex` | errors with the message and the code of the database; rows and columns as values |
| `QSqlDriver`, `QSqlResult`, `QSqlDriverCreator` | the interface for drivers, with the SQL generation that the table model uses |
| `QSqlQueryModel`, `QSqlTableModel` | a read-only model of any `SELECT`; an editable model of one table with filter, sort order and the three edit strategies, which reads a row again after writing it instead of resetting |
| `QSqlRelationalTableModel`, `QSqlRelation` | foreign keys shown as a value of the row that they refer to, with inner or left joins |
| `QSQLITE` driver | files, `:memory:` and temporary databases; the connection options `QSQLITE_BUSY_TIMEOUT`, `QSQLITE_OPEN_READONLY`, `QSQLITE_OPEN_URI`, `QSQLITE_OPEN_NOFOLLOW`, `QSQLITE_ENABLE_SHARED_CACHE`, `QSQLITE_ENABLE_REGEXP` and `QSQLITE_NO_USE_EXTENDED_RESULT_CODES`; notifications when a table changes; the `sqlite3 *` and `sqlite3_stmt *` handles |

Guide: [Using Nt SQL](https://open-nt.github.io/Nt6/nt-sql.html). The [`contacts`](examples/sql/contacts.cpp) example keeps an address book in an SQLite file.

Differences from Qt: SQLite is the only driver and there are no driver plugins; no `QSqlRelationalDelegate`; `QSqlQuery` can be moved but not copied.

### Nt D-Bus: implemented

Nt D-Bus implements the D-Bus wire protocol itself (no libdbus) and works with `dbus-daemon`, `dbus-broker`, GDBus and sd-bus peers.

| Class | Notes |
|---|---|
| `QDBusConnection` | session, system and activation buses, named connections, peer-to-peer, blocking (`Block`, `BlockWithGui`), asynchronous and callback calls, `registerObject` with all export options and child objects, `registerVirtualObject`, `registerService`, signal connections with argument and signature matching; thread-safe, with one I/O thread for all connections |
| `QDBusAbstractAdaptor`, `QDBusContext`, `QDBusVirtualObject` | exported interfaces with automatic signal relay, delayed and error replies, objects that handle whole sub-trees; `Introspectable`, `Properties` and `Peer` are answered for every object |
| `QDBusAbstractInterface`, `QDBusInterface`, `QDBusConnectionInterface` | proxies with typed methods and relayed signals, remote properties, argument conversion from introspection data, the bus daemon's own interface |
| `QDBusMessage`, `QDBusArgument`, `QDBusMetaType`, `QDBusVariant`, `QDBusObjectPath`, `QDBusSignature`, `QDBusUnixFileDescriptor`, `QDBusError` | the full type system including structures, maps, variants and file-descriptor passing; custom types with `qDBusRegisterMetaType`; every received message is validated |
| `QDBusReply`, `QDBusPendingCall`, `QDBusPendingCallWatcher`, `QDBusPendingReply` | typed replies with signature checks; `QDBusPendingReply` is variadic |
| `QDBusServer`, `QDBusServiceWatcher` | a peer-to-peer server (`EXTERNAL`, optional `ANONYMOUS`), watching names including `org.example.*` wildcards |

Guide: [Using Nt D-Bus](https://open-nt.github.io/Nt6/nt-dbus.html). The [`dbus_clock`](examples/dbus/clock.cpp) example exports a clock on the session bus (`dbus_clock server`) and calls it (`dbus_clock client`).

Differences from Qt: only `unix:` addresses (no `tcp:`, `nonce-tcp:` or `autolaunch:`), and no `DBUS_COOKIE_SHA1` authentication; `QDBusInterface` has no meta-object built from introspection, so connect to remote signals with `QDBusConnection::connect()`; no `qdbusxml2cpp`, `qdbuscpp2xml`, `qdbus` or `qdbusviewer` tools.

### Nt HTTP Server: implemented

Nt HTTP Server is an HTTP/1.1 server for embedding in an application, on top of Nt Network.

| Class | Notes |
|---|---|
| `QHttpServer` | routes with typed path arguments (`/user/<arg>` → `int`, `QString`, `QByteArray`, `double`, `QUrl`, ...), method filters, context objects and member-function slots, after-request handlers, a missing handler |
| `QAbstractHttpServer` | serves any number of `QTcpServer`, `QSslServer` (HTTPS) and `QLocalServer` objects; persistent connections, pipelining, chunked request bodies, `Expect: 100-continue`; malformed or oversized requests are refused, and requests with both `Transfer-Encoding` and `Content-Length` are rejected |
| `QHttpServerRequest`, `QHttpServerResponse`, `QHttpServerResponder` | the request with its absolute URL, headers and body; responses from strings, JSON, bytes or files; writing through the responder, including chunked bodies with trailers, streaming a `QIODevice`, and answering later |
| `QHttpServerRouterRule` | the rule a route returns |
| `QHttpServerWebSocketUpgradeResponse` | WebSocket upgrades on the server's own port: verifiers accept, deny or pass each upgrade request, and accepted connections arrive through `newWebSocketConnection()` as `QWebSocket`s |

Guide: [Using Nt HTTP Server](https://open-nt.github.io/Nt6/nt-httpserver.html). The [`hello_server`](examples/httpserver/hello_server.cpp) example is a small web service with JSON, streaming and delayed answers.

```cpp
QHttpServer server;
server.route("/hello/<arg>", [](const QString &name) { return "Hello, " + name; });
auto *tcp = new QTcpServer;
tcp->listen(QHostAddress::Any, 8080);
server.bind(tcp);
```

Differences from Qt: no HTTP/2, no WebSocket upgrades on a `QLocalServer`, no `QHttpServerConfiguration` (rate limits, keep-alive timeout, address filters) and no idle timeout; no `router()`, custom rules or converters; handlers cannot return a `QFuture`; the request size limits are fixed (8 KiB request line, 64 KiB of header fields, 64 MiB body); `QHttpHeaders` takes `QByteArrayView` rather than `QAnyStringView` and knows the common header names only.

### Nt WebSockets: implemented

Nt WebSockets implements the WebSocket protocol (RFC 6455) itself, on top of Nt Network, and interoperates with browsers and other implementations (tested against Python's `websockets`).

| Class | Notes |
|---|---|
| `QWebSocket` | client (`ws:` and `wss:`) and server side; text and binary messages, frame-by-frame signals, fragmentation of large messages, ping/pong with round-trip time, the closing handshake with codes and reasons, size limits on incoming frames and messages, custom request headers, TLS settings |
| `QWebSocketServer` | `ws:` and `wss:` (`SecureMode`) servers; every opening handshake is checked (426 for other versions), handshake timeout, a limit on pending connections, `handleConnection()` for sockets accepted elsewhere |
| `QWebSocketCorsAuthenticator`, `QWebSocketHandshakeOptions`, `QMaskGenerator`, `QWebSocketProtocol` | origin checks, subprotocol negotiation, custom masking keys, versions and close codes |

Every frame from the peer is validated: masking, reserved bits and opcodes, control-frame rules, fragmentation, UTF-8 of text (also across frames) and close codes; a peer that breaks the protocol is sent the matching close code and disconnected. Nt HTTP Server can also accept upgrades on its own port.

Guide: [Using Nt WebSockets](https://open-nt.github.io/Nt6/nt-websockets.html). The [`websocket_echo`](examples/websockets/echo.cpp) example is an echo server (`websocket_echo server`) and a client for it (`websocket_echo client ws://127.0.0.1:1234 hello`).

```cpp
QWebSocket socket;
QObject::connect(&socket, &QWebSocket::connected, [&] { socket.sendTextMessage("hello"); });
QObject::connect(&socket, &QWebSocket::textMessageReceived, [](const QString &reply) { qDebug() << reply; });
socket.open(QUrl("ws://127.0.0.1:1234"));
```

Differences from Qt: no extensions (so no `permessage-deflate` compression), no proxies, only version 13 of the protocol, no timeout for a client's opening handshake, and frames are received whole before they are reported; incoming frames and messages are limited to 64 MiB by default (1 GiB at most).

```cpp
QTcpServer server;
server.listen(QHostAddress::LocalHost, 0);
QObject::connect(&server, &QTcpServer::newConnection, [&] {
    QTcpSocket *s = server.nextPendingConnection();
    QObject::connect(s, &QIODevice::readyRead, [s] { s->write(s->readAll().toUpper()); });
});
```

Header names match Qt: `#include <QObject>`, `#include <QtCore/QTimer>`, `#include <QtNetwork/QTcpSocket>`, `#include <QtCore/qobject.h>`.

### Nt GUI: implemented

Nt GUI is layered like Qt GUI: a `QGuiApplication` loads a platform integration (QPA), which creates the windows and delivers input. The platforms are `xcb` (X11, including XWayland) and `offscreen` (no display, for tests); pick one with `-platform` or `QT_QPA_PLATFORM`.

| Class | Notes |
|---|---|
| `QGuiApplication`, `QScreen` | platform selection, screens, focus window, keyboard modifiers and mouse buttons, `quitOnLastWindowClosed` |
| `QWindow`, `QBackingStore` | top-level windows: geometry, title, show/hide/close, expose, `requestUpdate`, mouse, wheel, key, focus, enter/leave, move, resize and close events |
| `QPaintDeviceWindow`, `QRasterWindow` | windows you paint on with `QPainter`, repainted from the event loop |
| `QPainter`, `QPaintDevice`, `QPaintEngine` | software raster engine, or OpenGL through Nt OpenGL: pen, brush, opacity, clipping, transformations, composition modes; rectangles, lines, points, ellipses, arcs, polygons, paths, images and text, with integer and floating-point coordinates, antialiased on request |
| `QPainterPath`, `QTransform`, `QPolygon`, `QPolygonF`, `QLinearGradient`, `QRadialGradient`, `QConicalGradient` | Bezier paths with both fill rules, affine transformations, gradients with spread modes; pens with caps, joins and dashes |
| `QVector2D`, `QVector3D`, `QVector4D`, `QQuaternion`, `QMatrix4x4` | 3D math in `float`: dot and cross products, normals, distances, `project()`/`unproject()`; axis-angle, Euler, `slerp()`; translate/rotate/scale, `perspective()`, `ortho()`, `frustum()`, `lookAt()`, inverse, column-major `data()` for OpenGL. No `QMatrix3x3` or `QGenericMatrix` |
| `QOpenGLContext`, `QOpenGLFunctions`, `QSurfaceFormat`, `QSurface`, `QOffscreenSurface` | OpenGL through EGL, loaded at run time: desktop OpenGL or OpenGL ES contexts, sharing, the OpenGL ES 2.0 functions, swap interval (vsync on or off), multisampling; windows of the `offscreen` platform render into framebuffer objects |
| `QFont`, `QFontMetrics` | families found with fontconfig, point or pixel size, weight, italic; metrics, `elidedText()`, `drawText()` with alignment, word wrap and clipping; glyphs are anti-aliased by FreeType |
| `QImage`, `QPixmap`, `QColor`, `QRgb`, `QPen`, `QBrush`, `QRegion` | 32-bit images (`RGB32`, `ARGB32`, `ARGB32_Premultiplied`) with implicit sharing, loading and saving of PNG, BMP and JPEG files, colors with `#rgb` and CSS names, solid pens and brushes, rectangle-list regions |
| `QAction`, `QActionGroup`, `QKeySequence`, `QActionEvent` | what a user can do, shared by menus and shortcuts: text, check state, enabled and visible, a shortcut (a key with modifiers, from a string or a standard key), exclusive groups, and the events for widgets that show actions. No icons, and one key per sequence |
| `QMouseEvent`, `QWheelEvent`, `QKeyEvent`, `QFocusEvent`, `QEnterEvent`, `QPaintEvent`, `QExposeEvent`, `QResizeEvent`, `QMoveEvent`, `QShowEvent`, `QHideEvent`, `QCloseEvent` | the event classes, with the `Qt::Key`, `Qt::MouseButton` and `Qt::KeyboardModifier` enums |
| `QTextDocument`, `QTextLayout`, `QTextLine`, `QTextCharFormat`, `QTextBlockFormat`, `QTextOption` | rich text: a document of paragraphs with character and paragraph formats, read from and written as a subset of HTML (headings, bold/italic/underline, fonts, sizes, colors, links, lists, `pre`, rules, alignment and margins), laid out for a width and drawn with a painter; `anchorAt()` for the link at a point; line breaking, cursor positions and format ranges for one paragraph |

Guide: [Using Nt GUI](https://open-nt.github.io/Nt6/nt-gui.html). The [`hello_window`](examples/gui/hello_window.cpp) example paints a window and follows the mouse.

Not implemented yet: text shaping (HarfBuzz) and font fallback, `QFontDatabase`, editing of a `QTextDocument` (`QTextCursor` over it, tables, images, style sheets), other image formats and `QImageReader`/`QImageWriter`, the clipboard, cursors made of pixmaps and themed cursors (`QCursor` has the standard shapes, from the cursor font of the X server), RandR multi-screen, and Wayland.

### Nt Widgets: implemented

Nt Widgets builds on Nt GUI. The widgets of a window are painted into the backing store of one native window, so only top-level widgets are `QWindow`s.

| Class | Notes |
|---|---|
| `QApplication` | the style, the default palette, top-level widgets, `activeWindow()`, `focusWidget()`, `closeAllWindows()` |
| `QWidget` | parent/child tree, windows with a parent (`Qt::Window`, `Qt::Dialog`, `Qt::Tool`) that the window manager keeps above it, application- and window-modal windows that block the input of the others, geometry and size limits, size hints and policies, show/hide/enable, palette and font inheritance, painting with `update()`/`repaint()`, mouse (grab, enter/leave, tracking), wheel, key and focus events with propagation to the parent, click and tab focus, `mapTo*()`, `childAt()`, `raise()`/`lower()`, layouts |
| `QLayout`, `QBoxLayout`, `QHBoxLayout`, `QVBoxLayout`, `QLayoutItem`, `QWidgetItem`, `QSpacerItem` | box layouts with stretch factors, spacing, margins, nested layouts, alignment across the box, reversed directions, stretches and spacers; laid out again from the event loop when a size hint changes |
| `QSizePolicy` | the Qt policies |
| `QLabel`, `QPushButton`, `QCheckBox`, `QRadioButton`, `QAbstractButton`, `QButtonGroup` | text with alignment and word wrap, rich text (HTML) with links (`linkActivated`, `linkHovered`); clicked/pressed/released/toggled, checkable and flat buttons, mouse and Space handling, default and auto-default buttons for Return; radio buttons that share a parent are exclusive, and the arrow keys move the choice; button groups with ids that make buttons exclusive across parents |
| `QLineEdit` | one line of text: typing, cursor and selection with keys and mouse, placeholder, password and no-echo modes, maximum length, read-only, `textChanged`/`textEdited`/`returnPressed`/`editingFinished` |
| `QAbstractSlider`, `QSlider`, `QProgressBar`, `QAbstractSpinBox`, `QSpinBox`, `QDoubleSpinBox` | a base class with the range, tracking, `triggerAction()` and inverted controls; a handle to drag (click for a page, keys, wheel, horizontal or vertical); a bar with a text format (`%p`, `%v`, `%m`); an integer or a floating-point number with decimals to type or step, with a prefix, a suffix, wrapping, a special value text, read-only and keyboard tracking |
| `QComboBox` | items to choose from a list that drops down inside the window, above or below the box; keys and wheel change the item without opening it |
| `QGroupBox`, `QTabWidget`, `QTabBar`, `QToolBox` | a titled frame, optionally with a check box that enables its children; pages with a row of tabs that can have close buttons, tool tips and be dragged into another order; a column of items of which one is open |
| `QDateTimeEdit`, `QDateEdit`, `QTimeEdit`, `QCalendarWidget` | dates and times in a display format of sections that the keys and buttons step, typed or picked from a calendar popup, within a range; a month calendar with week numbers, keys and a date range |
| `QCompleter`, `QKeySequenceEdit`, `QFontComboBox`, `QCommandLinkButton` | completion of a line edit from a list of words, in a popup or inline; a shortcut recorder; the fontconfig families in a combo box; a large flat button with a description |
| `QMdiArea`, `QMdiSubWindow` | sub-windows inside an area that the user moves by the title bar, resizes by the edges, minimizes, maximizes and closes; tiling, cascading, activation order, and a tabbed view |
| `QTextBrowser`, `QSplashScreen`, `QRubberBand`, `QFocusFrame`, `QWidgetAction` | a read-only viewer of text and HTML files with backward/forward history, search paths and links to click (`anchorClicked`, `highlighted`); a splash screen with a pixmap and messages that closes when the main window shows; a selection rectangle or line; a frame that follows a widget; actions that put a widget in a tool bar |
| `QGridLayout` | a grid of rows and columns with spans, stretch factors, minimum sizes, alignment and separate horizontal and vertical spacing; shares its algorithm with the box layouts, so the sizes match Qt's |
| `QFormLayout`, `QStackedLayout`, `QStackedWidget` | a form of labels and fields with the Fusion defaults (growing fields, labels on the left), wrapping of rows, row insertion and removal; pages of which one is shown, with `currentChanged`/`widgetRemoved` |
| `QFrame`, `QScrollBar`, `QAbstractScrollArea`, `QScrollArea` | box, panel, line and styled frames with a shadow; a scroll bar (handle, page click, wheel); a base class with a viewport, two scroll bars and margins; a scrolling view of a widget, resizable or not, with `ensureVisible()` |
| `QPlainTextEdit`, `QTextEdit`, `QTextCursor` | plain text of many lines: typing, the arrow, Page, Home and End keys with words and Shift, mouse and double-click selection, wrapping or not, tab stops, undo and redo, read-only, a placeholder, `appendPlainText()` that follows the end of a log; a cursor that moves by characters, words, lines and blocks, selects and edits, with edit blocks. `QTextEdit::setHtml()` shows rich text (read-only: editing goes on with the plain text), `toHtml()` writes it. No clipboard yet |
| `QListWidget`, `QListWidgetItem`, `QAbstractItemView` | a list of text items with single, multi, extended (Control and Shift) and no selection, keys (arrows, Page, Home, End, Space, Return, type-ahead), sorting, colors, alignment, tool tips and scrolling; the base class has the selection mode and the alternating row colors that the table shares |
| `QTreeView`, `QAbstractItemView`, `QHeaderView` | a tree with columns for any `QAbstractItemModel`: expanding and collapsing with the mouse, the keys and from the program, a header whose sections are resized by dragging, row, item and column selection in all selection modes through a `QItemSelectionModel`, sorting by a click on the header (`setSortingEnabled()`), tool tips, lazy models (`fetchMore()`), `pressed`/`clicked`/`doubleClicked`/`activated` |
| `QStyledItemDelegate`, `QAbstractItemDelegate`, `QStyleOptionViewItem` | items drawn from the roles of the model (text, check box, decoration, font, colors, alignment) and edited in place: a line edit, spin box, combo box or date editor by the type of the value, opened by a double click, F2 or typing (`setEditTriggers()`), with Return, Escape and Tab; delegates per view, row and column |
| `QTreeWidget`, `QTreeWidgetItem` | a tree that keeps its own items: columns and header labels, text, check state, colors, font and any role per column, flags, expanding, sorting (numbers as numbers), `findItems()`, editing of editable items, and signals that tell about items |
| `QCursor`, `QStyleFactory`, widget attributes | `QWidget::setCursor()` and `QGuiApplication::setOverrideCursor()` with the standard shapes; `QWidget::setAttribute()` with `Qt::WA_DeleteOnClose`, `WA_QuitOnClose`, `WA_TransparentForMouseEvents`, `WA_NoMousePropagation`, `WA_OpaquePaintEvent` and the others, `setUpdatesEnabled()`, `setWindowModified()` with the `[*]` placeholder; `QStyleFactory::keys()`/`create()` |
| `QMenu`, `QMenuBar`, `QWidget::addAction()` | popup menus inside the window (a menu is a child widget, so it never leaves the window): entries with mnemonics, shortcuts, check boxes, separators and submenus that open on hover; keys (arrows, Return, Space, Escape, letters), `popup()` and a blocking `exec()`; a menu bar with menus that switch while one is open, Left and Right between them, and Alt+letter. A shortcut of an action works anywhere in the window it was added to. No icons, tear-off or scrolling menus |
| `QMainWindow`, `QToolBar`, `QDockWidget`, `QSizeGrip` | a main window with a menu bar, rows and columns of tool bars, dock widgets around the central widget and a status bar; tool bars of buttons for actions, separators and widgets, with toggle view actions; dock widgets with a title bar that close and float as windows above the main window; a grip that resizes its window. Tool bars and dock widgets cannot be dragged |
| `QDialog`, `QDialogButtonBox`, `QMessageBox`, `QInputDialog`, `QProgressDialog`, `QErrorMessage` | dialogs that are windows above their parent, centered on it, with `exec()`, `open()`, Escape and default buttons; buttons in the order of the platform by role; message boxes with drawn icons, informative and detailed text, a check box and the static `question()`/`warning()`/...; input of a text, an item or a number; progress that shows after a delay and can be canceled; error messages that the user can silence |
| `QFileDialog`, `QColorDialog`, `QFontDialog`, `QWizard`, `QWizardPage` | a file dialog drawn by Nt for opening files, saving with overwrite confirmation and choosing directories, with name filters; colors from a grid, a hue and saturation field, HSV/RGB/alpha and HTML; fonts from the fontconfig families with styles and sizes; wizards with pages, fields (mandatory ones too), commit pages and the Back/Next/Finish buttons |
| `QToolButton`, `QDial`, `QLCDNumber`, `QSplitter`, `QStatusBar` | a tool button with text or an arrow, checkable, flat with `autoRaise`; a dial with notches and wrapping; a seven-segment display in decimal, hex, octal and binary with three segment styles; a splitter with draggable handles, collapsible and stretching panes, as many panes as there are; a status bar with a temporary message, and widgets on the left and permanent ones on the right |
| `QTableWidget`, `QTableWidgetItem`, `QHeaderView` | a table of items with text, alignment, colors, font and a tool tip; row and column headers whose sections the user can resize; a current cell (click and keys), scrolling, alternating row colors, grid lines, inserting and removing rows and columns |
| `QToolTip`, `QHelpEvent`, `QWidget::setToolTip()` | tool tips that appear after the mouse rests, follow the widget under the mouse, and show the text of the nearest widget that has one; `QEvent::ToolTip` for tool tips that depend on the position |
| `QStyle`, `QStyleOption` and its subclasses, `QPalette` | a Fusion-like default style behind `QStyle`, with a light and a dark palette, replaceable with `QApplication::setStyle()` |

Guide: [Using Nt Widgets](https://open-nt.github.io/Nt6/nt-widgets.html). The [`hello_widgets`](examples/widgets/hello_widgets.cpp) example has the Nt logo, an SVG resource shown by a `QSvgWidget`, and one of every widget on the pages of a tab widget (its first page, *Nt*, is a showcase of all the widgets, live, and the *Views* page has a table and a scroll area), and [`gallery`](examples/widgets/gallery.cpp) shows more: custom widgets, a dark theme, enabling groups of controls, and the same logo.

<p align="center"><img src="doc/images/hello-widgets-nt.png" alt="The Nt page of hello_widgets, which shows every widget" width="360"> <img src="doc/images/hello-widgets-light.png" alt="The pages of hello_widgets, light, with the open list of a combo box" width="360"> <img src="doc/images/hello-widgets-dark.png" alt="The pages of hello_widgets, dark" width="360"></p>
<p align="center"><img src="doc/images/widgets-gallery-light.png" alt="The gallery example, light" width="320"> <img src="doc/images/widgets-gallery-dark.png" alt="The gallery example, dark" width="320"></p>

The examples follow the desktop: it uses twice as many pixels on a high-resolution screen (`QScreen::devicePixelRatio()`, from `Xft.dpi` or `QT_SCALE_FACTOR`) and the dark or light colors of the desktop (`QStyleHints::colorScheme()`, read from the desktop portal). The widget examples use only API that Qt has too, so `-DNT_EXAMPLES_QT=ON` also builds them against Qt 6 (`hello_widgets_qt`, `gallery_qt` in `qt-examples/`) to compare the two side by side.

Not implemented yet: editing of rich text and the clipboard, model-based list and table views (`QListView`, `QTableView`; `QTreeView` shows a model without hierarchy as a table of rows), `QItemDelegate` and persistent editors, native dialogs, dragging tool bars and dock widgets, drag and drop, style sheets.

## Nt Only

Nt follows Qt's API, but it also has a few things Qt does not. Code that uses them builds only against Nt, not against Qt. The reference documentation marks each one in the class's *Differences from Qt* section.

### Modules

| Module | CMake target | What it does |
|---|---|---|
| Nt FM | `Nt::FM` | FM synthesis of game sound effects from small JSON patches; see [Nt FM](#nt-fm-fm-synthesis-for-game-audio) and the [`fmplay`](examples/fm/fmplay.cpp) example |
| Nt FM QML | `Nt::FMQml` | the QML types of Nt FM (`import QtFM`); see the [`fmqml`](examples/fm/fmqml.cpp) example |
| Nt Gamepad | `Nt::Gamepad` | Qt Gamepad from Qt 5, brought back: `QGamepad`, `QGamepadManager` and `QGamepadKeyNavigation`, plus SDL GameControllerDB mappings, stable ids across replugging, generic button and axis signals, dead zones and rumble. Reads evdev on Linux, event-driven, with hot plugging. Guide: [Using Nt Gamepad](https://open-nt.github.io/Nt6/nt-gamepad.html); example: [`gamepadinfo`](examples/gamepad/gamepadinfo.cpp) |
| Nt GPIO | `Nt::Gpio` | `QGpioLine`: one GPIO line, as input with `valueChanged()`, debounce and bias, or as output. Uses the Linux GPIO character device (v2, Linux 5.10 or later) or Zephyr. Guide: [Using Nt GPIO](https://open-nt.github.io/Nt6/nt-gpio.html); example: [`ptt`](examples/gpio/ptt.cpp) |

### Additions to Qt classes

| Class | Addition |
|---|---|
| `QByteArray` | `toStdString()` and `stdString()` |
| `QDataStream` | `operator<<` and `operator>>` for `long` and `unsigned long`, written as 64-bit integers on every platform |
| `QSslConfiguration` | `usesSystemCaCertificates()` |

### Platforms and tools

- **Zephyr.** Nt Core, Nt Network and Nt HTTP Server build as a Zephyr module for microcontrollers, headless; see [Zephyr](#zephyr-embedded-headless).
- **MIT tools.** `ntmoc`, `ntrcc` and `ndoc` replace moc, rcc and qdoc. Their command lines and CMake functions match Qt's, but the `.rcc` format is Nt's own; see [ntmoc](#ntmoc-mit-moc-replacement), [ntrcc](#ntrcc-mit-rcc-replacement) and [NDoc](#ndoc-mit-documentation-generator).

## Qt 6 modules and their Nt names

The library is called **Nt**, not Qt: modules are `Nt Core`, `Nt Network`, and so on, and CMake targets are `Nt::<Module>` (`Nt::Core`, `Nt::Network`, `Nt::SerialPort`, `Nt::DBus`, `Nt::HttpServer`, `Nt::WebSockets`, `Nt::Gui`, `Nt::Widgets`, `Nt::Svg`, `Nt::SvgWidgets`, `Nt::Charts`, `Nt::OpenGL`, `Nt::Qml`, `Nt::Multimedia`, `Nt::MultimediaWidgets`, `Nt::Concurrent` and `Nt::Test` today). The C++ API keeps Qt's class names and headers (`QObject`, `<QtCore/QTimer>`) so Qt code compiles unchanged; `Qt6::Core` remains as an alias of `Nt::Core`.

✔ = Nt has a version (partial unless noted). Blank = not started. Module list checked against the Qt 6 module overview.

### Nt Concurrent: implemented

Nt Concurrent runs functions in the threads of a `QThreadPool` and returns a `QFuture` for what they produce.

| Function | Notes |
|---|---|
| `QtConcurrent::run()` | a function, member function or function object with its arguments, in the global pool or a given one; a function that takes a `QPromise<T> &` first reports several results and progress and can be canceled and suspended |
| `QtConcurrent::task()` | `QTaskBuilder`: `withArguments()`, `onThreadPool()`, `withPriority()`, `spawn()` |
| `map()`, `mapped()`, `mappedReduced()` | on a sequence or a pair of iterators; results in the order of the items; `ReduceOptions` (`UnorderedReduce`, `OrderedReduce`, `SequentialReduce`), initial values, result type named or taken from the reduce function |
| `filter()`, `filtered()`, `filteredReduced()` | the same forms for keeping items |
| `blocking...()` | every map and filter function, run in the pool and the calling thread |

The futures of the map and filter functions report progress, can be canceled and suspended, and rethrow the first exception. Link with `Nt::Concurrent` (or `Qt6::Concurrent`) and include `<QtConcurrent>`.

```cpp
QFuture<QString> text = QtConcurrent::run(readFile, path);
int words = QtConcurrent::blockingMappedReduced(files, countWords, [](int &total, int n) { total += n; });
```

Differences from Qt: items are handed out in blocks of a fixed size rather than an adaptive one; waiting for a non-blocking map from a thread of a saturated pool does not lend that thread to the pool (the `blocking` functions do work there); results of type `bool` cannot be read from a `QFuture`.

### Nt SVG: implemented

Nt SVG reads SVG documents and draws them with a `QPainter`, and writes what a painter draws as SVG. The XML reader and the antialiased rasterizer are its own: shapes are filled and stroked at the resolution of the device, whatever the painter can do itself. `Nt::Svg` needs only Nt GUI; `Nt::SvgWidgets` adds the widget.

| Class | Notes |
|---|---|
| `QSvgRenderer` | loads from a file, a resource or a `QByteArray`; draws the document, or one element by its `id`, into any rectangle of any paint device; default size, view box, aspect ratio modes, `boundsOnElement()`, `elementExists()` |
| `QSvgGenerator` | a paint device that writes rectangles, lines, points and ellipses as SVG elements and images as embedded PNG, to a file or a `QIODevice`; title, description, size, view box, resolution; SVG Tiny 1.2 or SVG 1.1 (with clipping) |
| `QSvgWidget` | shows a document scaled to the widget |

Drawn: `svg`, `g`, `defs`, `symbol`, `use`, `a`, `switch`; `path` (all commands, arcs included), `rect`, `circle`, `ellipse`, `line`, `polyline`, `polygon`; `text` and `tspan`; linear and radial gradients (units, transformations, spread methods, focal point, references); `clipPath`; fills and strokes with opacity, both fill rules, caps, joins, miter limits and dashes; group opacity; transformations; lengths with units; properties from attributes, `style` attributes and `<style>` sheets (element, class and id selectors).

Documents are treated as untrusted input: loading reads only the bytes it is given, nesting and references have a fixed depth, and one drawing visits a bounded number of elements (`QtSvg::AssumeTrustedSource` lifts that limit).

Guide: [Using Nt SVG](https://open-nt.github.io/Nt6/nt-svg.html). The [`svg_viewer`](examples/svg/svg_viewer.cpp) example shows a file in a window and writes one with `QSvgGenerator`.

```cpp
QSvgRenderer renderer(QString(":/logo.svg"));
QImage image(256, 256, QImage::Format_ARGB32_Premultiplied);
image.fill(Qt::transparent);
QPainter painter(&image);
renderer.render(&painter);
```

Differences from Qt: static documents only (no SMIL or CSS animation); no `image`, `mask`, `pattern`, `marker`, filters or `textPath`; text is drawn upright in one color with system fonts, following position and scale but not rotation; `preserveAspectRatio` is not read (use `setAspectRatioMode()`); no `load(QXmlStreamReader *)`, `transformForElement()` or `QGraphicsSvgItem`. `QSvgGenerator` writes text as an image of its glyphs, not as `text` elements.

### Nt Charts: implemented

Nt Charts has the API of Qt Charts: line, spline, scatter, area, bar (grouped, stacked, percent, vertical and horizontal) and pie or donut charts, with value, logarithmic, date-time and category axes, a legend, eight themes, zooming and mouse signals. Shapes are drawn antialiased by the rasterizer of Nt SVG. Link with `Nt::Charts`.

<p align="center"><img src="doc/images/charts/qlineseries.png" alt="A line chart" width="320"> <img src="doc/images/charts/qbarseries.png" alt="A bar chart" width="320"> <img src="doc/images/charts/qpieseries.png" alt="A pie chart" width="320"></p>

| Class | Notes |
|---|---|
| `QChart` | series, axes, title, legend, themes, margins, plot area, `createDefaultAxes()`, `zoomIn()`, `zoom()`, `scroll()`, `zoomReset()`, `mapToValue()`, `mapToPosition()`, `paint()` to any painter |
| `QChartView` | shows a chart; rubber band zooming; mouse events reach the series |
| `QLineSeries`, `QSplineSeries`, `QScatterSeries` | points, markers, point labels; `replace()` for live data |
| `QAreaSeries` | between two line series, or down to zero |
| `QBarSet`, `QBarSeries`, `QStackedBarSeries`, `QPercentBarSeries`, and the horizontal ones | value labels and their positions, bar width |
| `QPieSeries`, `QPieSlice` | holes, partial circles, exploded slices, labels outside or inside |
| `QValueAxis`, `QLogValueAxis`, `QDateTimeAxis`, `QBarCategoryAxis` | fixed or dynamic ticks, minor grid lines, shades, printf-style label formats, reversed axes |
| `QLegend` | four sides, rectangle, circle or series markers |

Guide: [Using Nt Charts](https://open-nt.github.io/Nt6/nt-charts.html), with pictures of each kind of chart and theme. The [`chart_dashboard`](examples/charts/dashboard.cpp) example shows four charts, one of them live.

Differences from Qt: `QChart` is a `QObject` and `QChartView` a `QWidget`, with no graphics scene; no animations, polar charts, box plots, candlesticks, `QCategoryAxis`, `QColorAxis`, model mappers or `QLegendMarker`; labels are drawn upright; a series has at most one axis of each orientation.

### Nt OpenGL: implemented

Nt OpenGL renders with OpenGL in windows and offscreen, and lets `QPainter` draw with OpenGL. Contexts come from EGL, which Nt loads at run time, so neither building nor running Nt needs an OpenGL library unless a program uses one. Link with `Nt::OpenGL`.

| Class | Notes |
|---|---|
| `QOpenGLWindow` | `initializeGL()`, `resizeGL()`, `paintGL()`, `paintUnderGL()`/`paintOverGL()`, `frameSwapped()`, partial updates (blit and blend), `grabFramebuffer()`; a `QPainter` on the window draws with OpenGL |
| `QOpenGLPaintDevice` | `QPainter` through OpenGL into the bound framebuffer: paths (stencil, then cover, both fill rules), pens, gradients, transformations, images, text and clip regions, drawn with the same geometry as the raster engine; antialiased by multisampling |
| `QOpenGLFramebufferObject`, `QOpenGLFramebufferObjectFormat` | texture or multisampled targets with depth and stencil, `toImage()`, `blitFramebuffer()` |

On an NVIDIA RTX 5070 Ti, `painting --bench` draws the flower scene of the QPainter documentation (800×600, 4× multisampling) in 1.5 ms with OpenGL against 9.4 ms in software, and gradients in 0.09 ms against 7.5 ms. With `QSurfaceFormat::setSwapInterval(0)` frames are not held to the display's refresh rate.

Guide: [Using Nt OpenGL](https://open-nt.github.io/Nt6/nt-opengl.html). The [`glwindow`](examples/opengl/glwindow.cpp) example renders with OpenGL and QPainter and prints its frame rate, with or without vsync; [`painting --gl`](examples/gui/painting.cpp) shows the painter scenes in a `QOpenGLWindow`.

Differences from Qt: EGL only (X11 through XCB, and headless), so no GLX, WGL or CGL yet; no `QOpenGLWidget`, `QOpenGLShaderProgram`, `QOpenGLBuffer`, `QOpenGLTexture`, `QOpenGLVertexArrayObject` or `QOpenGLExtraFunctions`; no `QPainter::beginNativePainting()`; antialiasing needs multisampling.

### Nt Qml: JavaScript and the QML engine

Nt Qml runs JavaScript and QML documents. The JavaScript engine is Nt's own interpreter of ECMAScript 2017 without classes, generators, async functions and destructuring; QObjects take part in scripts with their properties, methods and signals. The QML engine makes objects from documents, with bindings that follow their dependencies, signal handlers, declared properties, signals, functions, aliases and enumerations, C++ types registered with `qmlRegisterType()`, types written in QML (directories, `qmldir` modules, inline components), singletons, and the QtQml types. Link with `Nt::Qml`.

| Class | Notes |
|---|---|
| `QJSEngine`, `QJSValue`, `QJSValueIterator` | evaluate, call, objects and arrays, QObjects with signals connectable from scripts, errors with file and line, `throwError()`, interruption, console and translation extensions, garbage collection |
| `QQmlEngine`, `QQmlContext`, `QQmlComponent` | documents from files, resources or data; contexts and context properties; `beginCreate()`/`completeCreate()`, initial properties; warnings with locations |
| `QQmlApplicationEngine` | loads the main document; `Qt.quit()` and `Qt.exit()` |
| `qmlRegisterType()` and the rest of `<qqml.h>`, `QQmlListProperty`, `QQmlParserStatus` | C++ types, singletons, uncreatable types, QML files as types, default and list properties |
| `QtObject`, `Timer`, `Connections`, `Component` | the QtQml module, and the `Qt` object |

Guides: [Using Nt Qml](https://open-nt.github.io/Nt6/nt-qml.html) (JavaScript) and [Using Nt QML](https://open-nt.github.io/Nt6/nt-qml-language.html) (documents). The [`js_scripting`](examples/qml/scripting.cpp) and [`hello_qml`](examples/qml/hello_qml.cpp) examples show both.

Differences from Qt: interpreted, not compiled; no Qt Quick (visual types) yet; no attached properties other than `Component.onCompleted`/`onDestruction`, no `Behavior`, `Binding`, `Loader`, `Repeater` or models; no JavaScript modules or `.js` imports; no network URLs; `QML_ELEMENT` macros compile but types are registered with `qmlRegisterType()`.

### Nt FM: FM synthesis for game audio

Nt FM has no Qt counterpart. It synthesises game sound effects in real time with four-operator FM, so a game ships JSON patches of a few hundred bytes instead of WAV files, and each play can vary in pitch and texture. Link with `Nt::FM`; it needs only Nt Core.

| Class | Notes |
|---|---|
| `QtFM::Engine` | real-time voices with stealing by priority, level and age; lock-free, allocation-free audio thread; sample-accurate delayed starts; buses and master gain with a soft-clip limiter; `render()` for pull mode; `renderToWav()` |
| `QtFM::Patch` | JSON and CBOR, seeded random variance per play, generators for nine categories (native or within a chip's limits), mutation, `fitsChip()`, AdLib SBI import |
| `QtFM::VoiceHandle` | weak handle: stop, and smoothed pitch, gain, pan, filter, LFO and modulation index |
| `QtFM::AudioOutput` | backend interface; ALSA (loaded at run time) is the default on Linux |
| `QtFM::ChipCore`, `QtFM::Chip` | Nt FM's own clean-room OPL2 and OPL3 emulation, run in the engine's render; sample-accurate register writes; OPL patches play on chip channels through `play()` |
| `QtFM::MusicPlayer` | VGM/VGZ, DRO and IMF music on OPL2/OPL3 cores of its own, with loop points |
| `FmSound`, `FmSoundBank`, `FmSequence`, `FmMusic`, `FmPatch`, `FmEngine` (QML, `Nt::FMQml`) | `import QtFM`: a sound is one element, with pitch, gain and pan bound live, random pitch per play and an instance cap |
| `QtFM::MidiFile`, `QtFM::MidiPlayer` | Standard MIDI Files (formats 0-2, tempo maps, sustain pedal, volume, pan, bend) played with a built-in General MIDI bank of FM instruments and drums, or on the emulated OPL3 as a Sound Blaster would; controller changes reach sounding notes; streamed sample-accurately |
| `QtFM::PatchBank`, `QtFM::Sequence` | named sets of patches in JSON or CBOR, AdLib IBK banks and SBI export, C headers for targets without files; notes on a tempo grid, each starting on its exact sample |

The synthesiser: eight OPN/OPM algorithms or the two OPL connections, the OPL2 waveforms plus square, saw and triangle, DAHDSR envelopes with curvature, pitch envelope, slide and arpeggio, one LFO, white and NES-style LFSR noise, a state-variable filter with sweep, and a bit crusher.

Guide: [Using Nt FM](https://open-nt.github.io/Nt6/nt-fm.html), with the patch format. The [`fmplay`](examples/fm/fmplay.cpp) example plays, generates, renders and benchmarks sounds and music; [`fmqml`](examples/fm/fmqml.cpp) does it from a QML document.

Not yet: OPLL, OPN2, OPM and SN76489 emulation (so no Mega Drive or MSX music), RAD files, the fixed-point path and Zephyr build, oversampling, and backends other than ALSA.

### Nt Multimedia: audio and cameras

Nt Multimedia has the audio and camera parts of Qt Multimedia. Link with `Nt::Multimedia` (or `Qt6::Multimedia`); it needs Nt Core and Nt GUI. On Linux it uses ALSA, loaded at run time, so it builds without ALSA's headers and reaches PipeWire and PulseAudio through ALSA's `default` device, and Video4Linux2 for cameras.

| Class | Notes |
|---|---|
| `QAudioFormat`, `QtAudio` (`QAudio`) | sample rate, channel count or configuration, `UInt8`/`Int16`/`Int32`/`Float` samples, byte, frame and duration conversions; the state and error enums and `convertVolume()` |
| `QMediaDevices`, `QAudioDevice`, `QCameraDevice`, `QCameraFormat` | the audio inputs and outputs and the cameras, the default ones first, with each camera's formats, sizes and frame rates; `audioInputsChanged`/`audioOutputsChanged`/`videoInputsChanged` when a device is plugged in or out |
| `QAudioSink`, `QAudioSource` | raw samples played or recorded in pull mode (from or to a `QIODevice`) or push mode, with suspend and resume, buffer size, volume, processed and elapsed time; a worker thread does the blocking I/O |
| `QSoundEffect` | a WAV file held in memory and played with low latency, with loops (or `Infinite`), volume and mute |
| `QMediaPlayer`, `QAudioOutput` | WAV files played from disk, a resource or a `QIODevice`: play, pause, stop, seeking, loops, playback rate (pitch follows speed), position and duration, metadata, status and errors; the output sets the device, volume and mute |
| `QAudioDecoder`, `QAudioBuffer`, `QAudioFrame` | WAV files decoded into buffers, converted to another sample format, channel count or (by linear interpolation) sample rate; buffers that share their samples, read as typed frames (`S16S`, `F32M`, ...) |
| `QMediaCaptureSession`, `QCamera`, `QVideoSink` | a camera streaming to a sink from a thread of its own, in a chosen or automatic format, with errors when it fails or is unplugged; the microphone played back on an audio output |
| `QVideoWidget` (`Nt::MultimediaWidgets`) | a widget that shows a camera, scaled with the aspect ratio kept, stretched or cropped, drawn with `QPainter`; full screen |
| `QVideoFrame`, `QVideoFrameFormat` | frames in memory that share their pixels: mapping, planes, start and end times, rotation and mirroring; `toImage()` from packed RGB, YUYV, UYVY, NV12, NV21, P010, YUV420P, YV12, YUV422P, grey and JPEG, in BT.601 or BT.709 and video or full range |
| `QImageCapture` | pictures taken from the camera's stream, scaled to a resolution if set, as a `QImage` or saved as JPEG or PNG |
| `QMediaRecorder`, `QAudioInput`, `QMediaFormat`, `QMediaMetaData` | the microphone recorded to a WAV file, with pause, duration, numbered files in a folder and text tags (title, artist, date, ...) in its `LIST/INFO` chunk; the input sets the device, gain and mute |

WAV files with 8, 16, 24 or 32-bit integer or 32 or 64-bit floating-point samples, up to 8 channels, can be played and decoded. Guide: [Using Nt Multimedia](https://open-nt.github.io/Nt6/nt-multimedia.html). Examples: [`audiodevices`](examples/multimedia/audiodevices.cpp) lists the devices and plays a tone, [`audioplayer`](examples/multimedia/audioplayer.cpp) plays a WAV file, [`camera`](examples/multimedia/camera.cpp) lists the cameras, streams and saves a picture, [`viewfinder`](examples/multimedia/viewfinder.cpp) shows a camera in a window, [`recorder`](examples/multimedia/recorder.cpp) records the microphone, and [`decoder`](examples/multimedia/decoder.cpp) decodes a file to 16 kHz mono.

Not yet: video playback and recording (no video codecs), compressed audio (MP3, AAC, Ogg, FLAC), a QML video item and GPU drawing of video, camera controls (focus, zoom, exposure), pitch-preserving playback rates, and backends other than ALSA and Video4Linux2.

### Essentials

| Qt module | Nt library | Done |
|---|---|---|
| Qt Core | Nt Core | ✔ (partial, see Status) |
| Qt D-Bus | Nt D-Bus | ✔ (see Status) |
| Qt GUI | Nt GUI | ✔ (partial, see Status) |
| Qt Network | Nt Network | ✔ (partial, see Status) |
| Qt Qml | Nt Qml | ✔ (partial, see Status) |
| Qt Qml Core | Nt Qml Core |  |
| Qt Qml WorkerScript | Nt Qml WorkerScript |  |
| Qt Qml Models | Nt Qml Models |  |
| Qt Quick | Nt Quick |  |
| Qt Quick Controls | Nt Quick Controls |  |
| Qt Quick Dialogs | Nt Quick Dialogs |  |
| Qt Quick Layouts | Nt Quick Layouts |  |
| Qt Quick Test | Nt Quick Test |  |
| Qt Test | Nt Test | ✔ |
| Qt Widgets | Nt Widgets | ✔ (partial, see Status) |

### Add-ons

| Qt module | Nt library | Done |
|---|---|---|
| Qt Active Qt | Nt Active Qt |  |
| Qt Bluetooth | Nt Bluetooth |  |
| Qt Canvas Painter | Nt Canvas Painter |  |
| Qt CoAP | Nt CoAP |  |
| Qt Concurrent | Nt Concurrent | ✔ |
| (none, [Nt Only](#nt-only)) | Nt FM | ✔ (see Status) |
| Qt Gamepad (Qt 5 only, [Nt Only](#nt-only)) | Nt Gamepad | ✔ (Linux) |
| (none, [Nt Only](#nt-only)) | Nt GPIO | ✔ |
| Qt Graphs | Nt Graphs |  |
| Qt GRPC | Nt GRPC |  |
| Qt Help | Nt Help |  |
| Qt HTTP Server | Nt HTTP Server | ✔ (see Status) |
| Qt Image Formats | Nt Image Formats |  |
| Qt Lottie Animation | Nt Lottie Animation |  |
| Qt MQTT | Nt MQTT |  |
| Qt Multimedia | Nt Multimedia | ✔ (audio and cameras, Linux; see Status) |
| Qt Network Authorization | Nt Network Authorization |  |
| Qt NFC | Nt NFC |  |
| Qt OPC UA | Nt OPC UA |  |
| Qt OpenGL | Nt OpenGL | ✔ (see Status) |
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
| Qt Serial Port | Nt Serial Port | ✔ |
| Qt Shader Tools | Nt Shader Tools |  |
| Qt Spatial Audio | Nt Spatial Audio |  |
| Qt SQL | Nt SQL | ✔ |
| Qt State Machine | Nt State Machine |  |
| Qt SVG | Nt SVG | ✔ (see Status) |
| Qt TextToSpeech | Nt TextToSpeech |  |
| Qt UI Tools | Nt UI Tools |  |
| Qt Virtual Keyboard | Nt Virtual Keyboard |  |
| Qt Wayland Compositor | Nt Wayland Compositor |  |
| Qt WebChannel | Nt WebChannel |  |
| Qt WebSockets | Nt WebSockets | ✔ (see Status) |
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
| Qt Charts | Nt Charts | ✔ (see Status) |
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

## ntrcc: MIT rcc replacement

Qt's rcc is not used. `ntrcc` (`tools/ntrcc`) compiles `.qrc` resource collection files into C++ that registers the resources when the program starts, or into a binary `.rcc` file for `QResource::registerResource()`. Files are compressed with Zstandard or zlib when that saves at least 70% (`--compress-algo`, `--compress`, `--threshold` and the matching `<file>` attributes change it); zlib and libzstd are used when they are found at configure time (`-DNT_USE_ZLIB=OFF`, `-DNT_USE_ZSTD=OFF`).

```cmake
add_executable(app main.cpp app.qrc)
nt_enable_autorcc(app)                      # CMAKE_AUTORCC works too: Qt6::rcc is ntrcc

nt_add_resources(sources icons.qrc)         # or qt_add_resources(sources icons.qrc)
nt_add_resources(app images PREFIX /images FILES open.png save.png)
```

The compiled format is Nt's own, so `.rcc` files from Qt's rcc cannot be loaded. See "The Nt Resource System" in the documentation.

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

The class pages of the widgets show how each widget is drawn, in its states, in the light color scheme. The pictures in `doc/images/widgets` come from the real widgets: `cmake --build build --target widget_images` renders them with `QWidget::render()` on the offscreen platform (no display needed), so run it again after changing how a widget or the style draws, and commit the result. The `docs` target fails on any broken link or bad parameter name. `coverage.txt` in the output lists public API that has no documentation yet. See [tools/ndoc/README.md](tools/ndoc/README.md) for the comment syntax. Write documentation from Nt's behaviour: Qt's documentation text is not MIT licensed, so it must not be copied or reworded.

## Build

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Requires C++17 and a POSIX system. Link with the modules you use, such as `Nt::Core`, `Nt::Network`, `Nt::HttpServer` or `Nt::WebSockets` (or the `Qt6::` aliases, `Qt6::Core`, `Qt6::Network`, `Qt6::HttpServer`, `Qt6::WebSockets`).

### Zephyr (embedded, headless)

Nt Core and Nt Network cross build as a [Zephyr](https://zephyrproject.org) module for boards in the class of the NXP i.MX RT1170. Set `CONFIG_NT=y` (and `CONFIG_NT_NETWORK=y`) in `prj.conf`, link the application with `Nt::Core` and `Nt::Network`, and build with west; [`examples/zephyr/hello`](examples/zephyr/hello) is a complete application:

```sh
west build -b mimxrt1170_evk/mimxrt1176/cm7 examples/zephyr/hello
```

[`examples/zephyr/qemu`](examples/zephyr/qemu) is a self-checking application that runs without hardware, with `west build -b mps2/an385 examples/zephyr/qemu -t run`. It also serves a web page of its results with Nt HTTP Server (`CONFIG_NT_HTTPSERVER`), which QEMU can forward to your browser; the guide shows how.

The port is at an early stage: it runs under QEMU, and has not been run on a real board yet. The guide [Nt on Zephyr](https://open-nt.github.io/Nt6/nt-zephyr.html) lists what is left out and what differs.

## Packages

Install instructions for every system below are in the [Installing Nt](https://open-nt.github.io/Nt6/install.html) guide. CPack builds the native packages on the distribution they are for, in CI and locally:

| Distribution | Packages | Workflow |
|---|---|---|
| Ubuntu 22.04, 24.04, 26.04 | `libnt6-dev_<version>-1~ubuntu<release>_amd64.deb`, `nt6-tools_...deb` | `ubuntu-22.04.yml`, `ubuntu-24.04.yml`, `ubuntu-26.04.yml` |
| Enterprise Linux 9 and 10 (RHEL, Rocky, Alma) | `nt6-devel-<version>-1.el<N>.x86_64.rpm`, `nt6-tools-...rpm` | `el9.yml`, `el10.yml` |
| Arch Linux | `nt6-<version>-1-x86_64.pkg.tar.zst` | `arch.yml` |
| Any Linux with snapd (amd64 and arm64) | `open-nt6_<version>_amd64.snap`, `open-nt6_<version>_arm64.snap` (`snap install open-nt6`, from the [Snap Store](https://snapcraft.io/open-nt6)) | `snap.yml` |

`libnt6-dev` / `nt6-devel` holds the headers (in `/usr/include/nt6`, so Nt can sit next to Qt), the static libraries and the CMake package; `nt6-tools` holds `ntmoc`, `ntrcc` and `ndoc`. Arch has one package with everything. In a project:

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

Both write to `dist/`. Or run CPack directly: `cmake -S . -B build && cmake --build build && cpack --config build/CPackConfig.cmake` picks `.deb`, `.rpm` or pacman from `/etc/os-release`; use `-G DEB`, `-G RPM` or `-G External` (pacman, through `cmake/NtPacman.cmake`) to choose.

The snap is on the Snap Store as [`open-nt6`](https://snapcraft.io/open-nt6). A pre-release version such as `6.0.0-rc2` is on the `candidate` channel until a final release reaches `stable`, so install it with `sudo snap install open-nt6 --channel=candidate`. The tools are `open-nt6.ntmoc` and `open-nt6.ndoc`, and the headers, libraries and CMake package are in `/snap/open-nt6/current/usr` (pass `-DCMAKE_PREFIX_PATH=/snap/open-nt6/current/usr` to CMake).

The snap has its own target, `cmake --build build --target snap`, which runs `snapcraft pack` (in LXD or Multipass) and writes `open-nt6_<version>_<arch>.snap` to the build directory. Pass more options with `-DNT_SNAPCRAFT_ARGS="..."`. In CI each architecture is built natively, the arm64 snap on GitHub's hosted `ubuntu-24.04-arm` runner. On a machine without a container runtime, such as a Jetson running Ubuntu 24.04, pass `-DNT_SNAPCRAFT_ARGS=--destructive-mode`.

### Release provenance

Pushing a `v*` tag runs `slsa.yml`: it builds the packages for all six systems and the snap, generates [SLSA](https://slsa.dev) level 3 provenance with the [SLSA GitHub generator](https://github.com/slsa-framework/slsa-github-generator), and attaches the packages and `multiple.intoto.jsonl` to the GitHub release, and publishes the snap to the Snap Store (the `candidate` channel for a pre-release version such as `6.1.0-rc1`, otherwise `stable`; this needs the `SNAPCRAFT_STORE_CREDENTIALS` secret, see `snap.yml`). Check a download with [slsa-verifier](https://github.com/slsa-framework/slsa-verifier):

```sh
slsa-verifier verify-artifact libnt6-dev_6.1.0-1~ubuntu24.04_amd64.deb \
    --provenance-path multiple.intoto.jsonl --source-uri github.com/open-nt/Nt6 --source-tag v6.1.0
```
