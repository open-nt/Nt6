// MIT License - see LICENSE
#pragma once

#include "qdbusmessage_p.h"

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qset.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtDBus/qdbusconnection.h>
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusmessage.h>
#include <QtDBus/qdbusunixfiledescriptor.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class QSocketNotifier;
class QTimer;
class QDBusConnectionInterface;
class QDBusVirtualObject;
class QDBusConnectionPrivate;

constexpr int kDefaultDBusTimeout = 25000;

// Posts functions to a QObject from any thread, and stops once the object is destroyed: the
// object's destroyed() signal and post() take the same lock.
class QDBusTargetGuard {
public:
    static std::shared_ptr<QDBusTargetGuard> create(QObject *target, std::function<void()> onDestroyed = {});
    ~QDBusTargetGuard();
    QDBusTargetGuard(const QDBusTargetGuard &) = delete;
    QDBusTargetGuard &operator=(const QDBusTargetGuard &) = delete;

    // Runs `fn` in the target's thread from its event loop. Returns false if the target is gone.
    bool post(std::function<void()> fn);
    // The target, or null once it is destroyed. Only meaningful in the target's thread.
    QObject *target();

private:
    QDBusTargetGuard() = default;
    std::mutex m_mutex;
    QObject *m_target = nullptr;
    QMetaObject::Connection m_connection;
    std::function<void()> m_onDestroyed;
};

struct QDBusPendingCallPrivate {
    std::mutex mutex;
    std::condition_variable cv;
    QDBusMessage sentMessage;
    QDBusMessage replyMessage;
    bool finished = false;
    QList<QMetaType> expectedTypes; // set by QDBusPendingReply
    bool checkTypes = false;
    std::vector<std::function<void()>> callbacks;
    std::weak_ptr<QDBusConnectionPrivate> connection;
    uint serial = 0;
    std::chrono::steady_clock::time_point deadline;

    // The first call wins. Runs the callbacks in the calling thread, outside the lock.
    void complete(const QDBusMessage &reply);
    // Waits until finished or `msecs` have passed (-1: forever). Returns true if finished.
    bool waitForFinished(int msecs = -1);
    // Runs `fn` once the call has finished: at once if it already has.
    void onFinished(std::function<void()> fn);
    QDBusMessage reply();
    bool isFinished();
    // Turns a reply whose signature does not match expectedTypes into an error.
    static QDBusMessage checked(const QDBusMessage &reply, const QList<QMetaType> &types);
};

// What a method, slot or signal looks like on the bus.
struct QDBusMethodInfo {
    QMetaMethod method;
    QList<QMetaType> inTypes;
    QList<QMetaType> outTypes; // the return value first, if it is not void
    QByteArray inSignature;
    QByteArray outSignature;
    QList<QByteArray> inNames;
    QList<QByteArray> outNames;
    QByteArray kinds; // one per parameter: 'i' input, 'o' output, 'm' the QDBusMessage
    bool hasReturn = false;
    bool wantsMessage = false;
    bool valid = false;

    static QDBusMethodInfo parse(const QMetaMethod &method);
    // The slot that a SLOT() or SIGNAL() string names in `receiver`.
    static QDBusMethodInfo fromMember(QObject *receiver, const char *member);
    // Converts the arguments of `message` and calls the method on `object`. Fills `outputs`.
    bool invoke(QObject *object, const QDBusMessage &message, QList<QVariant> *outputs, QDBusError *error) const;
};

// Converts a received argument to `target`. A QVariant target unwraps a QDBusVariant.
QVariant qDBusAdaptArgument(const QVariant &in, QMetaType target, bool *ok);
// The interface name of the members a class declares.
QString qDBusInterfaceOf(const QMetaObject *mo);

class QDBusConnectionPrivate : public std::enable_shared_from_this<QDBusConnectionPrivate> {
public:
    enum Mode { InvalidMode, BusMode, PeerMode };

    explicit QDBusConnectionPrivate(const QString &name);
    ~QDBusConnectionPrivate();
    QDBusConnectionPrivate(const QDBusConnectionPrivate &) = delete;
    QDBusConnectionPrivate &operator=(const QDBusConnectionPrivate &) = delete;

    static std::shared_ptr<QDBusConnectionPrivate> get(const QDBusConnection &c) { return c.d; }
    static QDBusConnection wrap(std::shared_ptr<QDBusConnectionPrivate> d) { return QDBusConnection(std::move(d)); }

    // Setting up; called once.
    bool openBus(const QString &address);
    bool openPeer(const QString &address);
    // A connection that QDBusServer accepted and authenticated. Called in the I/O thread.
    void adoptAuthenticated(int fd, bool unixFds, const QByteArray &firstBytes);
    void close();

    QString name() const { return m_name; }
    Mode mode() const { return m_mode; }
    bool isConnected() const { return m_connected.load(); }
    QString baseService() const;
    QDBusError lastError() const;
    void setLastError(const QDBusError &error) const;
    bool unixFdPassing() const { return m_unixFds; }

    // Sending, from any thread.
    bool send(const QDBusMessage &message);
    std::shared_ptr<QDBusPendingCallPrivate> sendWithReply(const QDBusMessage &message, int timeout);
    QDBusMessage call(const QDBusMessage &message, QDBus::CallMode mode, int timeout);
    bool callWithCallback(const QDBusMessage &message, QObject *receiver, const char *returnMethod,
                          const char *errorMethod, int timeout);

    // Objects (qdbusobjects.cpp).
    bool registerObject(const QString &path, const QString &interface, QObject *object,
                        QDBusConnection::RegisterOptions options);
    bool registerVirtualObject(const QString &path, QDBusVirtualObject *object,
                               QDBusConnection::VirtualObjectRegisterOption options);
    void unregisterObject(const QString &path, QDBusConnection::UnregisterMode mode);
    QObject *objectRegisteredAt(const QString &path);

    // Signals (qdbusobjects.cpp).
    bool connectSignal(const QString &service, const QString &path, const QString &interface, const QString &name,
                       const QStringList &argumentMatch, const QString &signature, QObject *receiver, const char *slot);
    bool disconnectSignal(const QString &service, const QString &path, const QString &interface, const QString &name,
                          const QStringList &argumentMatch, const QString &signature, QObject *receiver,
                          const char *slot);
    // A hook that calls `fn` in the thread of `context`. `argNamespace` matches arg0 by namespace.
    uint addHook(const QString &service, const QString &path, const QString &interface, const QString &name,
                 const QStringList &argumentMatch, const QString &argNamespace, QObject *context,
                 std::function<void(const QDBusMessage &)> fn);
    void removeHook(uint id);

    bool registerService(const QString &name);
    bool unregisterService(const QString &name);
    // RequestName and ReleaseName, which keep track of the names this connection owns.
    QDBusMessage requestName(const QString &name, uint flags);
    QDBusMessage releaseName(const QString &name);
    QDBusConnectionInterface *busInterface();

private:
    friend class QDBusServerPrivate;

    struct Outgoing {
        QByteArray bytes;
        QList<QDBusUnixFileDescriptor> fds;
        qsizetype offset = 0;
    };

    struct ObjectRegistration {
        QString path;
        QObject *object = nullptr;
        QDBusVirtualObject *virtualObject = nullptr;
        QString interface;
        QDBusConnection::RegisterOptions options;
        bool subPath = false;
        std::shared_ptr<QDBusTargetGuard> guard;
        std::vector<QMetaObject::Connection> relays;
    };

    struct Hook {
        uint id = 0;
        QString service, path, interface, member;
        QStringList argumentMatch;
        QString argNamespace;
        QString signature; // given to connect(); empty matches any
        QByteArray prefix; // the arguments the slot takes must come first
        QString rule;
        std::shared_ptr<QDBusTargetGuard> guard;
        QDBusMethodInfo slot;
        QByteArray slotSignature;
        QObject *receiver = nullptr;
        std::function<void(const QDBusMessage &)> function;
        std::shared_ptr<std::atomic<bool>> active = std::make_shared<std::atomic<bool>>(true);
    };

    struct WatchedName {
        int refs = 0;
        QString owner;
        bool updated = false;
    };

    // The I/O thread.
    bool startIo(int fd, bool unixFds, const QByteArray &firstBytes);
    void runInIo(const std::function<void()> &fn, bool wait);
    void flush();
    void onReadable();
    void rearmTimer();
    void onTimeout();
    void handleMessage(const QDBusMessage &message);
    void handleSignal(const QDBusMessage &message);
    void lostConnection(const QString &why);
    void failAllPending(const QDBusError &error);
    bool queue(QDBusWireMessage &&wire, const std::shared_ptr<QDBusPendingCallPrivate> &pending, int timeout,
               QDBusError *error);

    // Objects (qdbusobjects.cpp).
    void routeMethodCall(const QDBusMessage &message, bool inCallingThread);
    void deliverCall(const ObjectRegistration &reg, const QString &relative, const QDBusMessage &message);
    QString introspect(QObject *object, QDBusConnection::RegisterOptions options, const QString &interface,
                       const QString &path, const QStringList &childNames);
    QStringList childNodes(const QString &path);
    void removeRegistrations(const std::function<bool(const ObjectRegistration &)> &match);
    void installRelays(ObjectRegistration &reg);
    bool isLocalTarget(const QString &service);
    void addMatch(const QString &rule);
    void removeMatch(const QString &rule);
    void watchName(const QString &name);
    void unwatchName(const QString &name);
    void deliverToHooks(const QDBusMessage &message);
    uint insertHook(Hook hook);

    const QString m_name;
    Mode m_mode = InvalidMode;
    std::atomic<bool> m_connected{false};
    bool m_unixFds = false;
    QString m_baseService;
    mutable std::mutex m_errorMutex;
    mutable QDBusError m_lastError;

    // Owned by the I/O thread.
    QObject *m_io = nullptr;
    int m_fd = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QTimer *m_timer = nullptr;
    QByteArray m_inBuffer;
    QList<QDBusUnixFileDescriptor> m_inFds;
    QDBusConnectionInterface *m_busInterface = nullptr;

    // Shared with the threads that send.
    std::mutex m_outMutex;
    std::deque<Outgoing> m_out;
    uint m_lastSerial = 0;
    bool m_flushPosted = false;

    std::mutex m_pendingMutex;
    std::unordered_map<uint, std::shared_ptr<QDBusPendingCallPrivate>> m_pending;

    std::mutex m_tableMutex;
    std::map<QString, ObjectRegistration> m_objects;
    std::vector<std::shared_ptr<Hook>> m_hooks;
    uint m_nextHookId = 0;
    QHash<QString, int> m_matchRefs;
    QHash<QString, WatchedName> m_watched;
    QSet<QString> m_ownedNames;
};

// Owns the thread that does the I/O of all connections, and the named connections.
class QDBusConnectionManager {
public:
    static QDBusConnectionManager *instance();
    // An object in the I/O thread, which runs functions posted to it. Starts the thread.
    QObject *ioContext();
    bool isIoThread() const;

    std::shared_ptr<QDBusConnectionPrivate> named(const QString &name);
    void insert(const QString &name, const std::shared_ptr<QDBusConnectionPrivate> &d);
    void remove(const QString &name);
    void track(const std::shared_ptr<QDBusConnectionPrivate> &d);

    ~QDBusConnectionManager();
    QDBusConnectionManager(const QDBusConnectionManager &) = delete;
    QDBusConnectionManager &operator=(const QDBusConnectionManager &) = delete;

    static std::atomic<bool> s_alive;

private:
    QDBusConnectionManager();

    std::mutex m_mutex;
    QThread *m_thread = nullptr;
    QObject *m_context = nullptr;
    QHash<QString, std::shared_ptr<QDBusConnectionPrivate>> m_named;
    std::vector<std::weak_ptr<QDBusConnectionPrivate>> m_all;
};
