// MIT License - see LICENSE
#include "qdbusconnection_p.h"
#include "qdbustransport_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qthread.h>
#include <QtCore/qtimer.h>
#include <QtDBus/qdbusconnectioninterface.h>
#include <QtDBus/qdbusmetatype.h>
#include <QtDBus/qdbuspendingcall.h>
#include <QtDBus/qdbusvirtualobject.h>

#include <cstring>

/*!
    \module NtDBus
    \title Nt D-Bus C++ Classes
    \brief Classes for inter-process communication over D-Bus.
    \cmaketarget Nt::DBus

    Nt D-Bus implements the D-Bus protocol itself, without libdbus: connecting to the session
    and system buses and to peers, exporting objects, calling remote objects and receiving
    their signals.

    See \l{Using Nt D-Bus} for a walk through, and \c{examples/dbus/clock.cpp} for a complete
    program.
*/

/*!
    \page nt-dbus.html
    \title Using Nt D-Bus
    \brief Connecting to a bus, exporting an object with an adaptor, calling it, receiving its signals, and the types that travel over D-Bus.

    Link \c Nt::DBus (or \c Nt6::DBus from an installed package) and include \c{<QtDBus>}:

    \code
    find_package(Nt6 6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt6::DBus)
    \endcode

    Nt D-Bus speaks the wire protocol itself, so it needs neither libdbus nor a Qt installation.
    It works with any D-Bus implementation: \c dbus-daemon, \c dbus-broker, GLib's GDBus and
    sd-bus.

    \section1 Connecting

    QDBusConnection::sessionBus() and QDBusConnection::systemBus() return the connections to the
    standard buses, which are opened the first time. The session bus is found through
    \c DBUS_SESSION_BUS_ADDRESS, or \c{$XDG_RUNTIME_DIR/bus}. Check the connection before using
    it:

    \snippet dbus/clock.cpp server

    Nt starts one thread that reads and writes the sockets of all connections. Replies and
    signals are delivered to objects through the event loops of the threads they live in, so
    run QCoreApplication::exec(), or a QEventLoop, in those threads.

    \section1 Exporting an Object

    An adaptor is a small class that a QObject owns. Its public slots, signals and properties
    form one D-Bus interface, named by the \c{"D-Bus Interface"} class information:

    \snippet dbus/clock.cpp adaptor

    Register the object with QDBusConnection::registerObject() and claim a name with
    QDBusConnection::registerService(). The bus then routes calls to the object, and the
    connection answers \c{org.freedesktop.DBus.Introspectable},
    \c{org.freedesktop.DBus.Properties} and \c{org.freedesktop.DBus.Peer} for it. A slot that
    inherits QDBusContext can reply later or send an error.

    A slot's parameters are its input arguments; non-const references are output arguments,
    after the return value. A last parameter of the type QDBusMessage receives the call.

    \section1 Calling an Object

    QDBusInterface calls methods by name, and reads and writes properties:

    \snippet dbus/clock.cpp client

    For an interface used often, a subclass of QDBusAbstractInterface gives typed methods, and
    its signals are emitted when the remote object emits them:

    \snippet dbus/clock.cpp proxy

    \c call() blocks; \c asyncCall() returns a QDBusPendingCall, which a
    QDBusPendingCallWatcher reports on. QDBusReply and QDBusPendingReply convert the reply to
    C++ types and hold the error if the call failed.

    \section1 Signals

    QDBusConnection::connect() delivers a D-Bus signal to a slot, named with \c SLOT():

    \code
    QDBusConnection::sessionBus().connect("org.example.Clock", "/clock", "org.example.Clock",
                                          "alarm", &window, SLOT(showAlarm(QString)));
    \endcode

    QDBusServiceWatcher reports when names appear and disappear on the bus.

    \section1 Types

    The basic D-Bus types map to \c uchar, \c bool, \c short, \c ushort, \c int, \c uint,
    \c qlonglong, \c qulonglong, \c double, QString, QDBusObjectPath, QDBusSignature and
    QDBusUnixFileDescriptor. A QDBusVariant is a D-Bus variant. QStringList, QByteArray,
    QVariantList and QVariantMap are arrays and dictionaries. Structures and other containers
    need stream operators and qDBusRegisterMetaType(); see QDBusArgument.

    \section1 Peer-to-Peer

    QDBusServer listens on a socket of its own, and QDBusConnection::connectToPeer() connects to
    it. There is no bus in between, so there are no names and no QDBusConnectionInterface.

    \section1 What Is Missing

    Only Unix domain sockets are supported, with the \c EXTERNAL and \c ANONYMOUS
    authentication mechanisms. There are no \c qdbusxml2cpp, \c qdbuscpp2xml, \c qdbus or
    \c qdbusviewer tools: use \c gdbus or \c busctl to look at a bus. QDBusInterface has no
    meta-object built from introspection data.
*/

/*!
    \class QDBusConnection
    \brief The QDBusConnection class represents a connection to the D-Bus bus daemon.

    A connection talks to a bus, usually the session bus or the system bus, or directly to one
    peer. Through it a program calls methods of remote objects, exports objects of its own,
    claims service names and receives signals:

    \snippet dbus/clock.cpp server

    A client calls the exported object:

    \snippet dbus/clock.cpp client

    QDBusConnection is a handle to a shared connection: copies refer to the same one, and it can
    be used from any thread. Connections to a bus are identified by a name, so
    \c{QDBusConnection(name)} returns one made earlier with connectToBus().

    Every connection is serviced by one thread that Nt starts. Calls to exported objects and
    signals for slots are delivered through the event loop of the thread that the receiving
    object lives in, so that thread must run an event loop. A blocking call() does not run the
    calling thread's event loop, except with \c QDBus::BlockWithGui; a blocking call to an
    object that this connection exports and that lives in the calling thread is delivered
    directly.

    \differences
    Only \c unix: addresses (\c path, \c abstract and \c runtime, and \c dir or \c tmpdir for a
    server) are supported; \c tcp:, \c nonce-tcp:, \c launchd: and \c autolaunch: are not.
    Authentication uses the \c EXTERNAL mechanism, and \c ANONYMOUS if a server allows it;
    \c DBUS_COOKIE_SHA1 is not available. The signals of objects that ExportChildObjects
    exports are not relayed, and adaptors and children added after registerObject() are found
    for calls but their signals are not relayed. There is no \c sender() and no
    \c registerRoot(); \c QDBus::AutoDetect blocks like \c QDBus::Block. A connection that a
    QDBusServer accepted is closed when its last QDBusConnection is destroyed.
    \enddifferences

    \sa QDBusAbstractInterface, QDBusAbstractAdaptor, QDBusServer, {Using Nt D-Bus}
*/

/*!
    \enum QDBusConnection::BusType

    The standard buses.

    \value SessionBus The bus of the user's login session.
    \value SystemBus The bus of the whole system.
    \value ActivationBus The bus that started this program, from \c DBUS_STARTER_ADDRESS.
*/

/*!
    \enum QDBusConnection::RegisterOption

    Selects what registerObject() exports from an object.

    \value ExportAdaptors The adaptors that are children of the object.
    \value ExportScriptableSlots The slots declared with \c Q_SCRIPTABLE.
    \value ExportScriptableSignals The signals declared with \c Q_SCRIPTABLE.
    \value ExportScriptableProperties The properties that are scriptable.
    \value ExportScriptableInvokables The \c Q_INVOKABLE methods declared with \c Q_SCRIPTABLE.
    \value ExportScriptableContents All four of the above.
    \value ExportNonScriptableSlots The slots that are not declared with \c Q_SCRIPTABLE.
    \value ExportNonScriptableSignals The signals that are not declared with \c Q_SCRIPTABLE.
    \value ExportNonScriptableProperties The properties that are not scriptable.
    \value ExportNonScriptableInvokables The invokable methods not declared with \c Q_SCRIPTABLE.
    \value ExportNonScriptableContents All four of the above.
    \value ExportAllSlots All public slots.
    \value ExportAllSignals All signals.
    \value ExportAllProperties All properties.
    \value ExportAllInvokables All public invokable methods.
    \value ExportAllContents Everything.
    \value ExportChildObjects The children of the object, at sub-paths named after them.
*/

/*!
    \enum QDBusConnection::UnregisterMode

    \value UnregisterNode Unregisters the object at the path only.
    \value UnregisterTree Unregisters the object at the path and all below it.
*/

/*!
    \enum QDBusConnection::VirtualObjectRegisterOption

    \value SingleNode The virtual object handles calls to its own path.
    \value SubPath The virtual object handles calls to its path and to every path below it.
*/

/*!
    \enum QDBusConnection::ConnectionCapability

    \value UnixFileDescriptorPassing The connection can carry QDBusUnixFileDescriptor values.
*/

namespace {

const char kSessionName[] = "qt_default_session_bus";
const char kSystemName[] = "qt_default_system_bus";

QDBusMessage busCall(const QString &method) {
    return QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                          QStringLiteral("/org/freedesktop/DBus"),
                                          QStringLiteral("org.freedesktop.DBus"), method);
}

QDBusError notConnected() {
    return QDBusError(QDBusError::Disconnected, QStringLiteral("Not connected to D-Bus server"));
}

} // namespace

// ---- QDBusTargetGuard ----------------------------------------------------------------------

std::shared_ptr<QDBusTargetGuard> QDBusTargetGuard::create(QObject *target, std::function<void()> onDestroyed) {
    std::shared_ptr<QDBusTargetGuard> g(new QDBusTargetGuard);
    g->m_target = target;
    g->m_onDestroyed = std::move(onDestroyed);
    if (target) {
        std::weak_ptr<QDBusTargetGuard> weak = g;
        g->m_connection = QObject::connect(
            target, &QObject::destroyed,
            [weak] {
                const std::shared_ptr<QDBusTargetGuard> self = weak.lock();
                if (!self)
                    return;
                std::function<void()> f;
                {
                    std::lock_guard<std::mutex> lock(self->m_mutex);
                    self->m_target = nullptr;
                    f.swap(self->m_onDestroyed);
                }
                if (f)
                    f();
            },
            Qt::DirectConnection);
    }
    return g;
}

QDBusTargetGuard::~QDBusTargetGuard() { QObject::disconnect(m_connection); }

bool QDBusTargetGuard::post(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_target)
        return false;
    return QMetaObject::invokeMethod(m_target, std::move(fn), Qt::QueuedConnection);
}

QObject *QDBusTargetGuard::target() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_target;
}

// ---- QDBusPendingCallPrivate ---------------------------------------------------------------

void QDBusPendingCallPrivate::complete(const QDBusMessage &reply) {
    std::vector<std::function<void()>> run;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (finished)
            return;
        replyMessage = checkTypes ? checked(reply, expectedTypes) : reply;
        finished = true;
        run.swap(callbacks);
    }
    cv.notify_all();
    for (auto &f : run)
        f();
}

bool QDBusPendingCallPrivate::waitForFinished(int msecs) {
    std::unique_lock<std::mutex> lock(mutex);
    if (msecs < 0) {
        cv.wait(lock, [this] { return finished; });
        return true;
    }
    return cv.wait_for(lock, std::chrono::milliseconds(msecs), [this] { return finished; });
}

void QDBusPendingCallPrivate::onFinished(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!finished) {
            callbacks.push_back(std::move(fn));
            return;
        }
    }
    fn();
}

QDBusMessage QDBusPendingCallPrivate::reply() {
    std::lock_guard<std::mutex> lock(mutex);
    return replyMessage;
}

bool QDBusPendingCallPrivate::isFinished() {
    std::lock_guard<std::mutex> lock(mutex);
    return finished;
}

QDBusMessage QDBusPendingCallPrivate::checked(const QDBusMessage &reply, const QList<QMetaType> &types) {
    if (reply.type() != QDBusMessage::ReplyMessage)
        return reply;
    QByteArray expected;
    for (const QMetaType &t : types) {
        const char *sig = QDBusMetaType::typeToSignature(t);
        if (!sig)
            return reply; // the caller reads it as a QDBusArgument or a QVariant
        expected += sig;
    }
    const QString got = reply.signature();
    if (got.toLatin1() == expected)
        return reply;
    return QDBusMessage::createError(QDBusError::InvalidSignature,
                                     QStringLiteral("Unexpected reply signature: got \"%1\", expected \"%2\"")
                                         .arg(got, QString::fromLatin1(expected)));
}

// ---- QDBusConnectionManager ----------------------------------------------------------------

std::atomic<bool> QDBusConnectionManager::s_alive{false};

QDBusConnectionManager *QDBusConnectionManager::instance() {
    static QDBusConnectionManager manager;
    return &manager;
}

QDBusConnectionManager::QDBusConnectionManager() { s_alive = true; }

QDBusConnectionManager::~QDBusConnectionManager() {
    std::vector<std::shared_ptr<QDBusConnectionPrivate>> live;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &w : m_all)
            if (auto d = w.lock())
                live.push_back(d);
        m_named.clear();
    }
    for (const auto &d : live)
        d->close();
    live.clear();
    if (m_thread) {
        QObject *context = m_context;
        QMetaObject::invokeMethod(context, [context] { delete context; }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
    }
    s_alive = false;
}

QObject *QDBusConnectionManager::ioContext() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_thread) {
        m_thread = new QThread;
        m_thread->setObjectName(QStringLiteral("QDBusConnection"));
        m_context = new QObject;
        m_context->moveToThread(m_thread);
        m_thread->start();
    }
    return m_context;
}

bool QDBusConnectionManager::isIoThread() const { return m_thread && QThread::currentThread() == m_thread; }

std::shared_ptr<QDBusConnectionPrivate> QDBusConnectionManager::named(const QString &name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_named.value(name);
}

void QDBusConnectionManager::insert(const QString &name, const std::shared_ptr<QDBusConnectionPrivate> &d) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_named.insert(name, d);
}

void QDBusConnectionManager::remove(const QString &name) {
    std::shared_ptr<QDBusConnectionPrivate> d;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        d = m_named.take(name);
    }
    // Released outside the lock: the destructor may close the connection.
}

void QDBusConnectionManager::track(const std::shared_ptr<QDBusConnectionPrivate> &d) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_all.erase(std::remove_if(m_all.begin(), m_all.end(), [](const auto &w) { return w.expired(); }), m_all.end());
    m_all.push_back(d);
}

// ---- QDBusConnectionPrivate: setting up and closing ----------------------------------------

QDBusConnectionPrivate::QDBusConnectionPrivate(const QString &name) : m_name(name) {}

QDBusConnectionPrivate::~QDBusConnectionPrivate() { close(); }

QString QDBusConnectionPrivate::baseService() const { return m_baseService; }

QDBusError QDBusConnectionPrivate::lastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

void QDBusConnectionPrivate::setLastError(const QDBusError &error) const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = error;
}

void QDBusConnectionPrivate::runInIo(const std::function<void()> &fn, bool wait) {
    QDBusConnectionManager *manager = QDBusConnectionManager::instance();
    if (manager->isIoThread()) {
        fn();
        return;
    }
    QObject *context = m_io ? m_io : manager->ioContext();
    QMetaObject::invokeMethod(context, fn, wait ? Qt::BlockingQueuedConnection : Qt::QueuedConnection);
}

bool QDBusConnectionPrivate::startIo(int fd, bool unixFds, const QByteArray &firstBytes) {
    m_unixFds = unixFds;
    QDBusConnectionManager::instance()->track(shared_from_this());
    runInIo(
        [this, fd, firstBytes] {
            m_io = new QObject;
            m_fd = fd;
            m_readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, m_io);
            m_writeNotifier = new QSocketNotifier(fd, QSocketNotifier::Write, m_io);
            m_writeNotifier->setEnabled(false);
            m_timer = new QTimer(m_io);
            m_timer->setSingleShot(true);
            QObject::connect(m_readNotifier, &QSocketNotifier::activated, m_io, [this] { onReadable(); });
            QObject::connect(m_writeNotifier, &QSocketNotifier::activated, m_io, [this] { flush(); });
            QObject::connect(m_timer, &QTimer::timeout, m_io, [this] { onTimeout(); });
            {
                std::lock_guard<std::mutex> lock(m_outMutex);
                m_connected = true;
            }
            if (!firstBytes.isEmpty()) {
                m_inBuffer = firstBytes;
                QMetaObject::invokeMethod(m_io, [this] { onReadable(); }, Qt::QueuedConnection);
            }
        },
        true);
    return true;
}

bool QDBusConnectionPrivate::openBus(const QString &address) {
    m_mode = BusMode;
    QDBusError error;
    bool unixFds = false;
    if (address.isEmpty()) {
        setLastError(QDBusError(QDBusError::NoServer, QStringLiteral("The address of the bus is not known")));
        return false;
    }
    const int fd = QDBusTransport::connectAndAuthenticate(address, kDefaultDBusTimeout, &unixFds, &error);
    if (fd < 0) {
        setLastError(error);
        return false;
    }
    startIo(fd, unixFds, QByteArray());
    const QDBusMessage reply = call(busCall(QStringLiteral("Hello")), QDBus::Block, kDefaultDBusTimeout);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        const QDBusError e = reply.type() == QDBusMessage::ErrorMessage
                                 ? QDBusError(reply)
                                 : QDBusError(QDBusError::NoServer, QStringLiteral("The bus did not answer Hello"));
        close();
        setLastError(e);
        return false;
    }
    m_baseService = reply.arguments().first().toString();
    setLastError(QDBusError());
    return true;
}

bool QDBusConnectionPrivate::openPeer(const QString &address) {
    m_mode = PeerMode;
    QDBusError error;
    bool unixFds = false;
    const int fd = QDBusTransport::connectAndAuthenticate(address, kDefaultDBusTimeout, &unixFds, &error);
    if (fd < 0) {
        setLastError(error);
        return false;
    }
    startIo(fd, unixFds, QByteArray());
    return true;
}

void QDBusConnectionPrivate::adoptAuthenticated(int fd, bool unixFds, const QByteArray &firstBytes) {
    m_mode = PeerMode;
    startIo(fd, unixFds, firstBytes);
}

void QDBusConnectionPrivate::close() {
    {
        std::lock_guard<std::mutex> lock(m_outMutex);
        m_connected = false;
        m_out.clear();
    }
    if (m_io && QDBusConnectionManager::s_alive) {
        runInIo(
            [this] {
                delete m_io;
                m_io = nullptr;
                m_readNotifier = m_writeNotifier = nullptr;
                m_timer = nullptr;
                m_busInterface = nullptr;
                QDBusTransport::closeFd(m_fd);
                m_fd = -1;
            },
            true);
    } else if (!m_io) {
        QDBusTransport::closeFd(m_fd);
        m_fd = -1;
    }
    failAllPending(notConnected());
    removeRegistrations([](const ObjectRegistration &) { return true; });
    std::vector<std::shared_ptr<Hook>> hooks;
    {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        hooks.swap(m_hooks);
    }
}

void QDBusConnectionPrivate::failAllPending(const QDBusError &error) {
    std::unordered_map<uint, std::shared_ptr<QDBusPendingCallPrivate>> pending;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        pending.swap(m_pending);
    }
    for (auto &p : pending)
        p.second->complete(QDBusMessage::createError(error));
}

// ---- QDBusConnectionPrivate: the I/O thread ------------------------------------------------

bool QDBusConnectionPrivate::queue(QDBusWireMessage &&wire, const std::shared_ptr<QDBusPendingCallPrivate> &pending,
                                   int timeout, QDBusError *error) {
    if (!wire.fds.isEmpty() && !m_unixFds) {
        *error = QDBusError(QDBusError::NotSupported,
                            QStringLiteral("The connection does not support passing file descriptors"));
        return false;
    }
    std::lock_guard<std::mutex> lock(m_outMutex);
    if (!m_connected) {
        *error = notConnected();
        return false;
    }
    uint serial = ++m_lastSerial;
    if (!serial)
        serial = ++m_lastSerial;
    std::memcpy(wire.bytes.data() + 8, &serial, 4);
    if (pending) {
        pending->serial = serial;
        pending->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        std::lock_guard<std::mutex> plock(m_pendingMutex);
        m_pending[serial] = pending;
    }
    m_out.push_back({std::move(wire.bytes), std::move(wire.fds), 0});
    if (!m_flushPosted) {
        m_flushPosted = true;
        QMetaObject::invokeMethod(
            m_io,
            [this] {
                flush();
                rearmTimer();
            },
            Qt::QueuedConnection);
    }
    return true;
}

void QDBusConnectionPrivate::flush() {
    bool broken = false;
    {
        std::lock_guard<std::mutex> lock(m_outMutex);
        m_flushPosted = false;
        if (m_fd < 0)
            return;
        while (!m_out.empty()) {
            Outgoing &o = m_out.front();
            const qint64 n =
                QDBusTransport::send(m_fd, o.bytes, o.offset, o.offset == 0 ? o.fds : QList<QDBusUnixFileDescriptor>());
            if (n < 0) {
                broken = true;
                break;
            }
            if (n == 0)
                break;
            o.offset += static_cast<qsizetype>(n);
            if (o.offset == o.bytes.size())
                m_out.pop_front();
        }
        if (m_writeNotifier)
            m_writeNotifier->setEnabled(!broken && !m_out.empty());
    }
    if (broken)
        lostConnection(QStringLiteral("Failed to write to the D-Bus socket"));
}

void QDBusConnectionPrivate::rearmTimer() {
    if (!m_timer)
        return;
    bool any = false;
    std::chrono::steady_clock::time_point earliest;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        for (const auto &p : m_pending) {
            if (!any || p.second->deadline < earliest)
                earliest = p.second->deadline;
            any = true;
        }
    }
    if (!any) {
        m_timer->stop();
        return;
    }
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(earliest - std::chrono::steady_clock::now()).count();
    m_timer->start(static_cast<int>(std::max<long long>(0, ms) + 1));
}

void QDBusConnectionPrivate::onTimeout() {
    std::vector<std::shared_ptr<QDBusPendingCallPrivate>> expired;
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (it->second->deadline <= now) {
                expired.push_back(it->second);
                it = m_pending.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const auto &p : expired)
        p->complete(QDBusMessage::createError(
            QDBusError::NoReply, QStringLiteral("Did not receive a reply. Possible causes include: the remote "
                                                "application did not send a reply, the message bus security policy "
                                                "blocked the reply, the reply timeout expired, or the network "
                                                "connection was broken.")));
    rearmTimer();
}

void QDBusConnectionPrivate::onReadable() {
    if (m_fd < 0)
        return;
    char buffer[65536];
    for (int round = 0; round < 16; ++round) {
        const qint64 n = QDBusTransport::receive(m_fd, buffer, sizeof buffer, m_inFds);
        if (n == -2)
            break;
        if (n <= 0) {
            lostConnection(n == 0 ? QStringLiteral("The D-Bus connection was closed by the other end")
                                  : QStringLiteral("Failed to read from the D-Bus socket"));
            return;
        }
        m_inBuffer.append(buffer, static_cast<qsizetype>(n));
        if (n < static_cast<qint64>(sizeof buffer))
            break;
    }
    qsizetype consumed = 0;
    while (m_inBuffer.size() - consumed >= 16) {
        const qint64 size = QDBusMessagePrivate::frameSize(m_inBuffer.constData() + consumed);
        if (size < 16) {
            lostConnection(QStringLiteral("Received a malformed D-Bus message"));
            return;
        }
        if (m_inBuffer.size() - consumed < size)
            break;
        const QByteArray frame = m_inBuffer.mid(consumed, static_cast<qsizetype>(size));
        consumed += static_cast<qsizetype>(size);
        const int nfds = QDBusMessagePrivate::announcedUnixFds(frame);
        if (nfds < 0 || nfds > m_inFds.size()) {
            lostConnection(QStringLiteral("Received a malformed D-Bus message"));
            return;
        }
        const QList<QDBusUnixFileDescriptor> fds = m_inFds.mid(0, nfds);
        m_inFds.remove(0, nfds);
        QString why;
        const QDBusMessage message = QDBusMessagePrivate::fromWire(frame, fds, &why);
        if (message.type() == QDBusMessage::InvalidMessage) {
            qWarning("QDBusConnection: received an invalid message (%s); closing the connection", qPrintable(why));
            lostConnection(QStringLiteral("Received a malformed D-Bus message: %1").arg(why));
            return;
        }
        handleMessage(message);
        if (m_fd < 0)
            return;
    }
    m_inBuffer.remove(0, consumed);
}

void QDBusConnectionPrivate::lostConnection(const QString &why) {
    {
        std::lock_guard<std::mutex> lock(m_outMutex);
        if (!m_connected && m_fd < 0)
            return;
        m_connected = false;
        m_out.clear();
    }
    if (m_readNotifier)
        m_readNotifier->setEnabled(false);
    if (m_writeNotifier)
        m_writeNotifier->setEnabled(false);
    QDBusTransport::closeFd(m_fd);
    m_fd = -1;
    setLastError(QDBusError(QDBusError::Disconnected, why));
    failAllPending(QDBusError(QDBusError::Disconnected, why));
    QDBusMessage local =
        QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/DBus/Local"),
                                   QStringLiteral("org.freedesktop.DBus.Local"), QStringLiteral("Disconnected"));
    QDBusMessagePrivate::edit(local)->received = true;
    deliverToHooks(local);
}

void QDBusConnectionPrivate::handleMessage(const QDBusMessage &message) {
    switch (message.type()) {
    case QDBusMessage::ReplyMessage:
    case QDBusMessage::ErrorMessage: {
        std::shared_ptr<QDBusPendingCallPrivate> p;
        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            const auto it = m_pending.find(QDBusMessagePrivate::get(message)->replySerial);
            if (it != m_pending.end()) {
                p = it->second;
                m_pending.erase(it);
            }
        }
        if (p)
            p->complete(message);
        return;
    }
    case QDBusMessage::SignalMessage:
        handleSignal(message);
        return;
    case QDBusMessage::MethodCallMessage:
        routeMethodCall(message, false);
        return;
    case QDBusMessage::InvalidMessage:
        return;
    }
}

void QDBusConnectionPrivate::handleSignal(const QDBusMessage &message) {
    if (m_mode == BusMode && message.service() == QLatin1String("org.freedesktop.DBus") &&
        message.interface() == QLatin1String("org.freedesktop.DBus")) {
        const QList<QVariant> args = message.arguments();
        std::lock_guard<std::mutex> lock(m_tableMutex);
        if (message.member() == QLatin1String("NameOwnerChanged") && args.size() == 3) {
            const auto it = m_watched.find(args.at(0).toString());
            if (it != m_watched.end()) {
                it->owner = args.at(2).toString();
                it->updated = true;
            }
        } else if (message.member() == QLatin1String("NameAcquired") && args.size() == 1) {
            m_ownedNames.insert(args.at(0).toString());
        } else if (message.member() == QLatin1String("NameLost") && args.size() == 1) {
            m_ownedNames.remove(args.at(0).toString());
        }
    }
    deliverToHooks(message);
}

// ---- QDBusConnectionPrivate: sending -------------------------------------------------------

bool QDBusConnectionPrivate::isLocalTarget(const QString &service) {
    if (m_mode != BusMode || service.isEmpty())
        return false;
    std::lock_guard<std::mutex> lock(m_tableMutex);
    return service == m_baseService || m_ownedNames.contains(service);
}

bool QDBusConnectionPrivate::send(const QDBusMessage &message) {
    QDBusMessagePrivate *d = QDBusMessagePrivate::get(message);
    if (d->localCall) {
        // The reply to a call that was delivered directly.
        QDBusError error;
        QDBusMessage local = QDBusMessagePrivate::makeLocal(message, m_baseService, &error);
        d->localCall->complete(local.type() == QDBusMessage::InvalidMessage ? QDBusMessage::createError(error) : local);
        return true;
    }
    QDBusWireMessage wire;
    QDBusError error;
    if (!QDBusMessagePrivate::toWire(message, false, &wire, &error) || !queue(std::move(wire), nullptr, 0, &error)) {
        setLastError(error);
        return false;
    }
    return true;
}

std::shared_ptr<QDBusPendingCallPrivate> QDBusConnectionPrivate::sendWithReply(const QDBusMessage &message,
                                                                               int timeout) {
    auto p = std::make_shared<QDBusPendingCallPrivate>();
    p->sentMessage = message;
    p->connection = weak_from_this();
    if (message.type() != QDBusMessage::MethodCallMessage) {
        p->complete(
            QDBusMessage::createError(QDBusError::InvalidArgs, QStringLiteral("Only method calls have replies")));
        return p;
    }
    QDBusWireMessage wire;
    QDBusError error;
    if (!QDBusMessagePrivate::toWire(message, true, &wire, &error) ||
        !queue(std::move(wire), p, timeout < 0 ? kDefaultDBusTimeout : timeout, &error)) {
        setLastError(error);
        p->complete(QDBusMessage::createError(error));
    }
    return p;
}

QDBusMessage QDBusConnectionPrivate::call(const QDBusMessage &message, QDBus::CallMode mode, int timeout) {
    if (mode == QDBus::NoBlock) {
        send(message);
        QDBusMessage none;
        none << QVariant();
        return none;
    }
    if (QDBusConnectionManager::instance()->isIoThread()) {
        const QDBusError error(QDBusError::InternalError,
                               QStringLiteral("A blocking call cannot be made from the D-Bus thread"));
        setLastError(error);
        return QDBusMessage::createError(error);
    }
    const int wait = timeout < 0 ? kDefaultDBusTimeout : timeout;
    std::shared_ptr<QDBusPendingCallPrivate> p;
    bool local = false;
    if (message.type() == QDBusMessage::MethodCallMessage && isLocalTarget(message.service())) {
        QDBusError error;
        QDBusMessage copy = QDBusMessagePrivate::makeLocal(message, m_baseService, &error);
        if (copy.type() == QDBusMessage::InvalidMessage) {
            setLastError(error);
            return QDBusMessage::createError(error);
        }
        p = std::make_shared<QDBusPendingCallPrivate>();
        p->sentMessage = message;
        p->connection = weak_from_this();
        QDBusMessagePrivate::edit(copy)->localCall = p;
        local = true;
        routeMethodCall(copy, true);
    } else {
        p = sendWithReply(message, timeout);
    }
    if (mode == QDBus::BlockWithGui && !p->isFinished()) {
        QEventLoop loop;
        const auto guard = QDBusTargetGuard::create(&loop);
        p->onFinished([guard, &loop] { guard->post([&loop] { loop.quit(); }); });
        if (!p->isFinished())
            loop.exec();
    }
    if (local && !p->waitForFinished(wait))
        p->complete(QDBusMessage::createError(QDBusError::NoReply,
                                              QStringLiteral("Did not receive a reply from the local object")));
    p->waitForFinished();
    const QDBusMessage reply = p->reply();
    setLastError(reply.type() == QDBusMessage::ErrorMessage ? QDBusError(reply) : QDBusError());
    return reply;
}

bool QDBusConnectionPrivate::callWithCallback(const QDBusMessage &message, QObject *receiver, const char *returnMethod,
                                              const char *errorMethod, int timeout) {
    if (!receiver || (!returnMethod && !errorMethod))
        return false;
    QDBusMethodInfo ret;
    if (returnMethod) {
        ret = QDBusMethodInfo::fromMember(receiver, returnMethod);
        if (!ret.valid) {
            qWarning("QDBusConnection::callWithCallback: invalid return slot %s", returnMethod);
            return false;
        }
    }
    QMetaMethod errorSlot;
    bool errorWantsMessage = false;
    if (errorMethod) {
        const QByteArray sig = QMetaObject::normalizedSignature(errorMethod + 1);
        const int index = receiver->metaObject()->indexOfMethod(sig.constData());
        if (index < 0) {
            qWarning("QDBusConnection::callWithCallback: invalid error slot %s", errorMethod);
            return false;
        }
        errorSlot = receiver->metaObject()->method(index);
        const QList<QByteArray> types = errorSlot.parameterTypes();
        if (types.isEmpty() || types.first() != "QDBusError" || types.size() > 2 ||
            (types.size() == 2 && types.at(1) != "QDBusMessage")) {
            qWarning("QDBusConnection::callWithCallback: the error slot must take a QDBusError");
            return false;
        }
        errorWantsMessage = types.size() == 2;
    }
    const std::shared_ptr<QDBusPendingCallPrivate> p = sendWithReply(message, timeout);
    const auto guard = QDBusTargetGuard::create(receiver);
    std::weak_ptr<QDBusPendingCallPrivate> weak = p;
    p->onFinished([guard, weak, ret, errorSlot, errorWantsMessage] {
        const auto finished = weak.lock();
        if (!finished)
            return;
        const QDBusMessage reply = finished->reply();
        guard->post([guard, reply, ret, errorSlot, errorWantsMessage] {
            QObject *target = guard->target();
            if (!target)
                return;
            if (reply.type() == QDBusMessage::ErrorMessage) {
                if (!errorSlot.isValid())
                    return;
                QDBusError error(reply);
                QDBusMessage copy = reply;
                void *argv[] = {nullptr, &error, errorWantsMessage ? static_cast<void *>(&copy) : nullptr};
                errorSlot.enclosingMetaObject()->d.static_metacall(target, QMetaObject::InvokeMetaMethod,
                                                                   errorSlot.relativeMethodIndex(), argv);
                return;
            }
            if (!ret.valid)
                return;
            QDBusError error;
            if (!ret.invoke(target, reply, nullptr, &error))
                qWarning("QDBusConnection: the reply does not fit %s: %s", ret.method.methodSignature().constData(),
                         qPrintable(error.message()));
        });
    });
    // The callback holds only a weak reference: the call itself stays registered with the
    // connection, which keeps it until the reply or the timeout.
    return true;
}

QDBusMessage QDBusConnectionPrivate::requestName(const QString &name, uint flags) {
    QDBusMessage m = busCall(QStringLiteral("RequestName"));
    m << name << flags;
    const QDBusMessage reply = call(m, QDBus::Block, -1);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        const uint result = reply.arguments().first().toUInt();
        if (result == 1 || result == 4) {
            std::lock_guard<std::mutex> lock(m_tableMutex);
            m_ownedNames.insert(name);
        }
    }
    return reply;
}

QDBusMessage QDBusConnectionPrivate::releaseName(const QString &name) {
    QDBusMessage m = busCall(QStringLiteral("ReleaseName"));
    m << name;
    const QDBusMessage reply = call(m, QDBus::Block, -1);
    if (reply.type() == QDBusMessage::ReplyMessage) {
        std::lock_guard<std::mutex> lock(m_tableMutex);
        m_ownedNames.remove(name);
    }
    return reply;
}

bool QDBusConnectionPrivate::registerService(const QString &name) {
    const QDBusMessage reply = requestName(name, 4u); // DBUS_NAME_FLAG_DO_NOT_QUEUE
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return false;
    const uint result = reply.arguments().first().toUInt();
    return result == 1 || result == 4;
}

bool QDBusConnectionPrivate::unregisterService(const QString &name) {
    const QDBusMessage reply = releaseName(name);
    return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() &&
           reply.arguments().first().toUInt() == 1;
}

QDBusConnectionInterface *QDBusConnectionPrivate::busInterface() {
    if (m_mode != BusMode || !m_io)
        return nullptr;
    QDBusConnectionInterface *result = nullptr;
    const QDBusConnection self = wrap(shared_from_this());
    runInIo(
        [this, &result, &self] {
            if (!m_busInterface && m_io)
                m_busInterface = new QDBusConnectionInterface(self, m_io);
            result = m_busInterface;
        },
        true);
    return result;
}

// ---- QDBusConnection -----------------------------------------------------------------------

QDBusConnection::QDBusConnection(std::shared_ptr<QDBusConnectionPrivate> p) : d(std::move(p)) {}

/*!
    Constructs a handle to the connection called \a name, made earlier with connectToBus() or
    connectToPeer(). If there is none, the handle is not connected.
*/
QDBusConnection::QDBusConnection(const QString &name) : d(QDBusConnectionManager::instance()->named(name)) {}

/*!
    Constructs a handle to the connection that \a other refers to.
*/
QDBusConnection::QDBusConnection(const QDBusConnection &other) = default;

/*!
    Constructs a handle that takes the place of \a other.
*/
QDBusConnection::QDBusConnection(QDBusConnection &&other) noexcept = default;

/*!
    Makes this handle refer to the connection of \a other and returns a reference to it.
*/
QDBusConnection &QDBusConnection::operator=(const QDBusConnection &other) = default;

/*!
    Moves \a other into this handle and returns a reference to it.
*/
QDBusConnection &QDBusConnection::operator=(QDBusConnection &&other) noexcept = default;

/*!
    Destroys the handle. The connection stays open while it has a name or other handles.
*/
QDBusConnection::~QDBusConnection() = default;

/*!
    Swaps this handle with \a other.
*/
void QDBusConnection::swap(QDBusConnection &other) noexcept { d.swap(other.d); }

/*!
    Returns \c true if the connection is open.
*/
bool QDBusConnection::isConnected() const { return d && d->isConnected(); }

/*!
    Returns the unique name that the bus gave this connection, such as \c{:1.42}. It is empty
    for a connection to a peer.
*/
QString QDBusConnection::baseService() const { return d ? d->baseService() : QString(); }

/*!
    Returns the error of the last operation that failed, such as connecting or a blocking call.
*/
QDBusError QDBusConnection::lastError() const { return d ? d->lastError() : notConnected(); }

/*!
    Returns the name of the connection.
*/
QString QDBusConnection::name() const { return d ? d->name() : QString(); }

/*!
    Returns what the connection can do.
*/
QDBusConnection::ConnectionCapabilities QDBusConnection::connectionCapabilities() const {
    return d && d->isConnected() && d->unixFdPassing() ? UnixFileDescriptorPassing : ConnectionCapabilities();
}

/*!
    Sends \a message without waiting for a reply and returns \c true if it was queued. A method
    call sent this way tells the receiver that no reply is wanted.
*/
bool QDBusConnection::send(const QDBusMessage &message) const { return d && d->send(message); }

/*!
    Sends the call \a message and returns at once. When the reply arrives, \a returnMethod of
    \a receiver is called with its arguments; on an error, \a errorMethod is called with a
    QDBusError and, if it takes one, the QDBusMessage. Name the slots with \c SLOT(). The call
    fails after \a timeout milliseconds, or the default of 25 seconds if it is -1. Returns
    \c true if the call was sent.
*/
bool QDBusConnection::callWithCallback(const QDBusMessage &message, QObject *receiver, const char *returnMethod,
                                       const char *errorMethod, int timeout) const {
    return d && d->callWithCallback(message, receiver, returnMethod, errorMethod, timeout);
}

/*!
    \overload
    Sends \a message and calls \a slot of \a receiver with the reply. Errors are ignored.
    \a timeout is in milliseconds.
*/
bool QDBusConnection::callWithCallback(const QDBusMessage &message, QObject *receiver, const char *slot,
                                       int timeout) const {
    return d && d->callWithCallback(message, receiver, slot, nullptr, timeout);
}

/*!
    Sends \a message and waits as \a mode says for the reply, which it returns. The call fails
    with \c NoReply after \a timeout milliseconds, or 25 seconds if it is -1. With
    \c QDBus::NoBlock the function returns at once.

    \sa asyncCall(), QDBusReply
*/
QDBusMessage QDBusConnection::call(const QDBusMessage &message, QDBus::CallMode mode, int timeout) const {
    if (!d)
        return QDBusMessage::createError(notConnected());
    return d->call(message, mode, timeout);
}

/*!
    Sends \a message and returns an object that follows the call. The call fails after
    \a timeout milliseconds, or 25 seconds if it is -1.

    \sa QDBusPendingCallWatcher, QDBusPendingReply
*/
QDBusPendingCall QDBusConnection::asyncCall(const QDBusMessage &message, int timeout) const {
    if (!d)
        return QDBusPendingCall::fromError(notConnected());
    return QDBusPendingCall(d->sendWithReply(message, timeout));
}

/*!
    Connects the signal \a name of \a interface, emitted by the object at \a path of \a service,
    to \a slot of \a receiver, named with \c SLOT() or \c SIGNAL(). An empty argument matches
    anything. The slot receives the leading arguments of the signal that it has parameters
    for, and the QDBusMessage if its last parameter is one. Returns \c true on success.

    \sa disconnect()
*/
bool QDBusConnection::connect(const QString &service, const QString &path, const QString &interface,
                              const QString &name, QObject *receiver, const char *slot) {
    return d && d->connectSignal(service, path, interface, name, {}, QString(), receiver, slot);
}

/*!
    \overload
    Connects only to signals whose arguments have the D-Bus \a signature. \a service, \a path,
    \a interface, \a name, \a receiver and \a slot are as above.
*/
bool QDBusConnection::connect(const QString &service, const QString &path, const QString &interface,
                              const QString &name, const QString &signature, QObject *receiver, const char *slot) {
    return d && d->connectSignal(service, path, interface, name, {}, signature, receiver, slot);
}

/*!
    \overload
    Connects only to signals whose leading string arguments equal \a argumentMatch; an empty
    string in the list matches any value. \a service, \a path, \a interface, \a name,
    \a signature, \a receiver and \a slot are as above.
*/
bool QDBusConnection::connect(const QString &service, const QString &path, const QString &interface,
                              const QString &name, const QStringList &argumentMatch, const QString &signature,
                              QObject *receiver, const char *slot) {
    return d && d->connectSignal(service, path, interface, name, argumentMatch, signature, receiver, slot);
}

/*!
    Removes the connection that connect() made with the same \a service, \a path,
    \a interface, \a name, \a receiver and \a slot. Returns \c true if there was one.
*/
bool QDBusConnection::disconnect(const QString &service, const QString &path, const QString &interface,
                                 const QString &name, QObject *receiver, const char *slot) {
    return d && d->disconnectSignal(service, path, interface, name, {}, QString(), receiver, slot);
}

/*!
    \overload
    Removes the connection made with the same \a service, \a path, \a interface, \a name,
    \a signature, \a receiver and \a slot.
*/
bool QDBusConnection::disconnect(const QString &service, const QString &path, const QString &interface,
                                 const QString &name, const QString &signature, QObject *receiver, const char *slot) {
    return d && d->disconnectSignal(service, path, interface, name, {}, signature, receiver, slot);
}

/*!
    \overload
    Removes the connection made with the same \a service, \a path, \a interface, \a name,
    \a argumentMatch, \a signature, \a receiver and \a slot.
*/
bool QDBusConnection::disconnect(const QString &service, const QString &path, const QString &interface,
                                 const QString &name, const QStringList &argumentMatch, const QString &signature,
                                 QObject *receiver, const char *slot) {
    return d && d->disconnectSignal(service, path, interface, name, argumentMatch, signature, receiver, slot);
}

/*!
    Exports \a object at \a path. \a options selects what is exported; by default, the
    adaptors that are children of the object. Returns \c false if the path is taken or invalid.
    The object is unregistered when it is destroyed.

    \sa unregisterObject(), QDBusAbstractAdaptor
*/
bool QDBusConnection::registerObject(const QString &path, QObject *object, RegisterOptions options) {
    return d && d->registerObject(path, QString(), object, options);
}

/*!
    \overload
    Exports \a object at \a path and names the interface of its own members \a interface,
    instead of the name its class gives. \a options selects what is exported.
*/
bool QDBusConnection::registerObject(const QString &path, const QString &interface, QObject *object,
                                     RegisterOptions options) {
    return d && d->registerObject(path, interface, object, options);
}

/*!
    Unregisters the object at \a path, and with \c UnregisterTree as \a mode every object below.
*/
void QDBusConnection::unregisterObject(const QString &path, UnregisterMode mode) {
    if (d)
        d->unregisterObject(path, mode);
}

/*!
    Returns the object registered at \a path, or \c nullptr.
*/
QObject *QDBusConnection::objectRegisteredAt(const QString &path) const {
    return d ? d->objectRegisteredAt(path) : nullptr;
}

/*!
    Exports \a object at \a path, where it handles calls itself. With \c SubPath as \a options
    it also handles every path below. Returns \c false if the path is taken or invalid.
*/
bool QDBusConnection::registerVirtualObject(const QString &path, QDBusVirtualObject *object,
                                            VirtualObjectRegisterOption options) {
    return d && d->registerVirtualObject(path, object, options);
}

/*!
    Claims the well-known name \a serviceName on the bus. Returns \c false if another connection
    owns it.

    \sa unregisterService(), QDBusConnectionInterface::registerService()
*/
bool QDBusConnection::registerService(const QString &serviceName) {
    return d && d->mode() == QDBusConnectionPrivate::BusMode && d->registerService(serviceName);
}

/*!
    Releases the name \a serviceName. Returns \c true if this connection owned it.
*/
bool QDBusConnection::unregisterService(const QString &serviceName) {
    return d && d->mode() == QDBusConnectionPrivate::BusMode && d->unregisterService(serviceName);
}

/*!
    Returns an interface to the bus itself, or \c nullptr for a connection to a peer. The
    connection owns it.
*/
QDBusConnectionInterface *QDBusConnection::interface() const { return d ? d->busInterface() : nullptr; }

/*!
    Connects to the bus \a type and names the connection \a name. If a connection of that name
    exists, it is returned.
*/
QDBusConnection QDBusConnection::connectToBus(BusType type, const QString &name) {
    return connectToBus(QDBusTransport::busAddress(type), name);
}

/*!
    \overload
    Connects to the bus at \a address and names the connection \a name.
*/
QDBusConnection QDBusConnection::connectToBus(const QString &address, const QString &name) {
    QDBusConnectionManager *manager = QDBusConnectionManager::instance();
    if (auto existing = manager->named(name))
        return QDBusConnection(existing);
    auto p = std::make_shared<QDBusConnectionPrivate>(name);
    p->openBus(address);
    manager->insert(name, p);
    return QDBusConnection(p);
}

/*!
    Connects to the peer at \a address, such as a QDBusServer, and names the connection \a name.
*/
QDBusConnection QDBusConnection::connectToPeer(const QString &address, const QString &name) {
    QDBusConnectionManager *manager = QDBusConnectionManager::instance();
    if (auto existing = manager->named(name))
        return QDBusConnection(existing);
    auto p = std::make_shared<QDBusConnectionPrivate>(name);
    p->openPeer(address);
    manager->insert(name, p);
    return QDBusConnection(p);
}

/*!
    Forgets the connection called \a name. It is closed once no QDBusConnection refers to it.
*/
void QDBusConnection::disconnectFromBus(const QString &name) { QDBusConnectionManager::instance()->remove(name); }

/*!
    Forgets the connection to a peer called \a name. It is closed once no QDBusConnection
    refers to it.
*/
void QDBusConnection::disconnectFromPeer(const QString &name) { QDBusConnectionManager::instance()->remove(name); }

/*!
    Returns the machine id of this computer, from \c /etc/machine-id.
*/
QByteArray QDBusConnection::localMachineId() { return QDBusTransport::machineId(); }

/*!
    Returns the connection to the session bus, opening it the first time.
*/
QDBusConnection QDBusConnection::sessionBus() {
    QDBusConnectionManager *manager = QDBusConnectionManager::instance();
    const QString name = QString::fromLatin1(kSessionName);
    if (auto existing = manager->named(name); existing && existing->isConnected())
        return QDBusConnection(existing);
    auto p = std::make_shared<QDBusConnectionPrivate>(name);
    if (p->openBus(QDBusTransport::busAddress(SessionBus)))
        manager->insert(name, p);
    return QDBusConnection(p);
}

/*!
    Returns the connection to the system bus, opening it the first time.
*/
QDBusConnection QDBusConnection::systemBus() {
    QDBusConnectionManager *manager = QDBusConnectionManager::instance();
    const QString name = QString::fromLatin1(kSystemName);
    if (auto existing = manager->named(name); existing && existing->isConnected())
        return QDBusConnection(existing);
    auto p = std::make_shared<QDBusConnectionPrivate>(name);
    if (p->openBus(QDBusTransport::busAddress(SystemBus)))
        manager->insert(name, p);
    return QDBusConnection(p);
}

#include "moc_qdbusconnection.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
