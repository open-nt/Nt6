// MIT License - see LICENSE
#include "qdbusconnection_p.h"
#include "qdbustransport_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtimer.h>
#include <QtDBus/qdbusserver.h>

#include <atomic>
#include <map>

/*!
    \class QDBusServer
    \brief The QDBusServer class provides peer-to-peer communication between processes on the
    same computer.

    A server listens on an address and hands each client that connects to it as a
    QDBusConnection, without a bus in between:

    \code
    QDBusServer server;
    QObject::connect(&server, &QDBusServer::newConnection, [&](const QDBusConnection &peer) {
        connections.append(peer);
        connections.last().registerObject("/clock", &clock, QDBusConnection::ExportAllSlots);
    });
    // A client calls QDBusConnection::connectToPeer(server.address(), "clock").
    \endcode

    Clients authenticate with \c EXTERNAL and must run as the same user as the server, unless
    setAnonymousAuthenticationAllowed() lets anyone in.

    \differences
    Only \c unix: addresses are supported; \c tmpdir and \c dir create a socket file there.
    A connection that the server accepted is closed when the last QDBusConnection that refers
    to it is destroyed, so keep a copy for as long as it is used.
    \enddifferences

    \sa QDBusConnection::connectToPeer()
*/

class QDBusServerPrivate : public std::enable_shared_from_this<QDBusServerPrivate> {
public:
    struct Session {
        int fd = -1;
        QDBusTransport::ServerAuth auth;
        QSocketNotifier *notifier = nullptr;
        QTimer *timer = nullptr;
        Session(int f, const QByteArray &guid, bool anonymous) : fd(f), auth(f, guid, anonymous) {}
    };

    QObject *io = nullptr;
    int listenFd = -1;
    QString address;
    QString unlinkPath;
    QByteArray guid = QDBusTransport::randomGuid();
    std::atomic<bool> anonymous{false};
    QDBusError error;
    std::shared_ptr<QDBusTargetGuard> owner;
    std::map<int, std::unique_ptr<Session>> sessions;
    int counter = 0;

    void run(const std::function<void()> &fn) {
        QDBusConnectionManager *manager = QDBusConnectionManager::instance();
        if (manager->isIoThread())
            fn();
        else
            QMetaObject::invokeMethod(manager->ioContext(), fn, Qt::BlockingQueuedConnection);
    }

    void listen(const QString &where) {
        run([this, where] {
            QString clientAddress;
            listenFd = QDBusTransport::listen(where, &clientAddress, &unlinkPath, &error);
            if (listenFd < 0)
                return;
            address = clientAddress + QStringLiteral(",guid=") + QString::fromLatin1(guid);
            io = new QObject;
            auto *n = new QSocketNotifier(listenFd, QSocketNotifier::Read, io);
            QObject::connect(n, &QSocketNotifier::activated, io, [this] { acceptAll(); });
        });
    }

    void acceptAll() {
        for (;;) {
            const int fd = QDBusTransport::accept(listenFd);
            if (fd < 0)
                return;
            auto session = std::make_unique<Session>(fd, guid, anonymous.load());
            session->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, io);
            session->timer = new QTimer(io);
            session->timer->setSingleShot(true);
            QObject::connect(session->notifier, &QSocketNotifier::activated, io, [this, fd] { readAuth(fd); });
            QObject::connect(session->timer, &QTimer::timeout, io, [this, fd] { dropSession(fd, true); });
            session->timer->start(kDefaultDBusTimeout);
            sessions[fd] = std::move(session);
        }
    }

    void dropSession(int fd, bool closeIt) {
        const auto it = sessions.find(fd);
        if (it == sessions.end())
            return;
        it->second->notifier->setEnabled(false);
        it->second->notifier->deleteLater();
        it->second->timer->stop();
        it->second->timer->deleteLater();
        if (closeIt)
            QDBusTransport::closeFd(fd);
        sessions.erase(it);
    }

    void readAuth(int fd) {
        const auto it = sessions.find(fd);
        if (it == sessions.end())
            return;
        Session &s = *it->second;
        char buffer[4096];
        QList<QDBusUnixFileDescriptor> fds;
        const qint64 n = QDBusTransport::receive(fd, buffer, sizeof buffer, fds);
        if (n == -2)
            return;
        if (n <= 0 || !fds.isEmpty()) {
            dropSession(fd, true);
            return;
        }
        QByteArray rest;
        const QByteArray reply = s.auth.feed(QByteArray(buffer, static_cast<qsizetype>(n)), &rest);
        if (!reply.isEmpty() && QDBusTransport::send(fd, reply, 0, {}) != reply.size()) {
            dropSession(fd, true);
            return;
        }
        if (s.auth.state() == QDBusTransport::ServerAuth::Failed) {
            dropSession(fd, true);
            return;
        }
        if (s.auth.state() != QDBusTransport::ServerAuth::Authenticated)
            return;
        const bool unixFds = s.auth.unixFds();
        dropSession(fd, false);
        auto connection = std::make_shared<QDBusConnectionPrivate>(QStringLiteral("QDBusServer-%1").arg(++counter));
        connection->adoptAuthenticated(fd, unixFds, rest);
        const auto guard = owner;
        owner->post([guard, connection] {
            if (auto *server = dynamic_cast<QDBusServer *>(guard->target()))
                server->newConnection(QDBusConnectionPrivate::wrap(connection));
        });
    }

    void shutdown() {
        run([this] {
            for (auto &s : sessions)
                QDBusTransport::closeFd(s.first);
            sessions.clear();
            delete io;
            io = nullptr;
            QDBusTransport::closeFd(listenFd);
            listenFd = -1;
        });
        if (!unlinkPath.isEmpty())
            QFile::remove(unlinkPath);
    }
};

/*!
    Constructs a server that listens on \a address, such as \c{unix:path=/tmp/app.sock}, with
    the given \a parent.
*/
QDBusServer::QDBusServer(const QString &address, QObject *parent)
    : QObject(parent), d(std::make_shared<QDBusServerPrivate>()) {
    d->owner = QDBusTargetGuard::create(this);
    d->listen(address);
}

/*!
    Constructs a server with the given \a parent that listens on a new socket in the directory
    for temporary files.
*/
QDBusServer::QDBusServer(QObject *parent)
    : QDBusServer(QStringLiteral("unix:tmpdir=") + QDBusTransport::escapeValue(QDir::tempPath()), parent) {}

/*!
    Destroys the server and stops listening. The connections it accepted stay open.
*/
QDBusServer::~QDBusServer() { d->shutdown(); }

/*!
    Returns \c true if the server is listening.
*/
bool QDBusServer::isConnected() const { return d->listenFd >= 0; }

/*!
    Returns the error that stopped the server from listening.
*/
QDBusError QDBusServer::lastError() const { return d->error; }

/*!
    Returns the address that clients pass to QDBusConnection::connectToPeer().
*/
QString QDBusServer::address() const { return d->address; }

/*!
    Lets clients authenticate as anonymous if \a value is \c true, so that any user can connect.
*/
void QDBusServer::setAnonymousAuthenticationAllowed(bool value) { d->anonymous = value; }

#include "moc_qdbusserver.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
