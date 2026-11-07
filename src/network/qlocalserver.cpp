// MIT License - see LICENSE
#include <QtCore/qdir.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtNetwork/qlocalserver.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace {

QString resolveName(const QString &name) {
    return name.startsWith(QChar(u'/')) ? name : QDir::tempPath() + QChar(u'/') + name;
}

} // namespace

/*!
    \class QLocalServer
    \brief The QLocalServer class accepts connections from other processes on
    the same machine.

    Call listen() with a name, then fetch each new connection when
    newConnection() is emitted:

    \code
    QLocalServer server;
    server.listen("my-service");
    QObject::connect(&server, &QLocalServer::newConnection, [&] {
        QLocalSocket *client = server.nextPendingConnection();
        client->write("hello");
    });
    \endcode

    If a previous process crashed without closing its server, listen() fails
    with an address-in-use error. Call removeServer() first to clear the stale
    socket file.

    \differences
    Linux and other POSIX systems only, using Unix domain sockets. There is no
    listening backlog setting or \c AbstractNamespaceOption.
    \enddifferences

    \sa QLocalSocket
*/

/*!
    \enum QLocalServer::SocketOption

    Selects who may connect to the server.

    \value NoOptions Access follows the process's umask.
    \value UserAccessOption The owner may connect.
    \value GroupAccessOption The owner's group may connect.
    \value OtherAccessOption Other users may connect.
    \value WorldAccessOption Everyone may connect.
*/

/*!
    Constructs a local server with the given \a parent.
*/
QLocalServer::QLocalServer(QObject *parent) : QObject(parent) {}

/*!
    Destroys the server and stops listening. Accepted sockets still pending are
    deleted with it.
*/
QLocalServer::~QLocalServer() { close(); }

void QLocalServer::fail(QLocalSocket::LocalSocketError error, const QString &text) {
    m_error = error;
    m_errorString = text;
}

/*!
    Starts listening on the socket \a name. A relative name is placed in
    QDir::tempPath(). Returns \c true on success.

    \sa close(), removeServer(), serverError()
*/
bool QLocalServer::listen(const QString &name) {
    if (isListening()) {
        fail(QLocalSocket::OperationError, QString("Server is already listening"));
        return false;
    }
    if (name.isEmpty()) {
        fail(QLocalSocket::ServerNotFoundError, QString("Name error"));
        return false;
    }
    const QString full = resolveName(name);
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    const QByteArray path = full.toUtf8();
    if (path.size() >= static_cast<qsizetype>(sizeof addr.sun_path)) {
        fail(QLocalSocket::ServerNotFoundError, QString("Socket path is too long"));
        return false;
    }
    std::memcpy(addr.sun_path, path.constData(), static_cast<std::size_t>(path.size()));

    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        fail(QLocalSocket::SocketResourceError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0) {
        const int err = errno;
        ::close(fd);
        fail(err == EACCES ? QLocalSocket::SocketAccessError : QLocalSocket::ConnectionError, QString::fromUtf8(std::strerror(err)));
        return false;
    }
    // Applied before listen(), so nobody can connect under the wrong mode.
    if (m_options != NoOptions) {
        mode_t mode = 0;
        if (m_options.testFlag(UserAccessOption)) mode |= S_IRWXU;
        if (m_options.testFlag(GroupAccessOption)) mode |= S_IRWXG;
        if (m_options.testFlag(OtherAccessOption)) mode |= S_IRWXO;
        ::chmod(path.constData(), mode);
    }
    if (::listen(fd, SOMAXCONN) != 0) {
        const int err = errno;
        ::close(fd);
        ::unlink(path.constData());
        fail(QLocalSocket::SocketResourceError, QString::fromUtf8(std::strerror(err)));
        return false;
    }
    m_fd = fd;
    m_name = name;
    m_fullName = full;
    m_error = QLocalSocket::UnknownSocketError;
    m_errorString.clear();
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    QObject::connect(m_notifier, &QSocketNotifier::activated, this, [this] { acceptPending(); });
    return true;
}

/*!
    Stops listening and removes the socket file. Connections already accepted
    stay open.

    \sa listen()
*/
void QLocalServer::close() {
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        ::unlink(m_fullName.toUtf8().constData());
    }
    m_fd = -1;
    for (QLocalSocket *s : m_pending)
        delete s;
    m_pending.clear();
    m_name.clear();
    m_fullName.clear();
}

/*!
    Returns \c true if the server is listening.

    \sa listen()
*/
bool QLocalServer::isListening() const { return m_fd >= 0; }

/*!
    Returns the name given to listen(), or an empty string if not listening.

    \sa fullServerName()
*/
QString QLocalServer::serverName() const { return m_name; }

/*!
    Returns the path of the listening socket, or an empty string if not
    listening.

    \sa serverName()
*/
QString QLocalServer::fullServerName() const { return m_fullName; }

/*!
    Returns a description of the last error.

    \sa serverError()
*/
QString QLocalServer::errorString() const { return m_errorString; }

/*!
    Returns the last error.

    \sa errorString()
*/
QLocalSocket::LocalSocketError QLocalServer::serverError() const { return m_error; }

/*!
    Returns \c true if a connection is waiting for nextPendingConnection().
*/
bool QLocalServer::hasPendingConnections() const { return !m_pending.isEmpty(); }

/*!
    Returns the next pending connection, or \c nullptr if there is none. The
    socket is a child of the server until you reparent or delete it.

    \sa newConnection()
*/
QLocalSocket *QLocalServer::nextPendingConnection() {
    if (m_pending.isEmpty())
        return nullptr;
    QLocalSocket *s = m_pending.takeFirst();
    updateNotifier();
    return s;
}

/*!
    Returns the most connections held for nextPendingConnection().

    \sa setMaxPendingConnections()
*/
int QLocalServer::maxPendingConnections() const { return m_maxPending; }

/*!
    Sets the most connections held for nextPendingConnection() to
    \a numConnections. Further clients wait in the system's queue.

    \sa maxPendingConnections()
*/
void QLocalServer::setMaxPendingConnections(int numConnections) {
    m_maxPending = numConnections;
    updateNotifier();
}

/*!
    Waits up to \a msec milliseconds for a connection; a negative value waits
    forever. Returns \c true if one is pending. If \a timedOut is not null, it
    says whether the wait timed out.

    \sa hasPendingConnections()
*/
bool QLocalServer::waitForNewConnection(int msec, bool *timedOut) {
    if (timedOut)
        *timedOut = false;
    if (m_fd < 0)
        return false;
    if (m_pending.isEmpty()) {
        pollfd p = {m_fd, POLLIN, 0};
        int rc;
        do {
            rc = ::poll(&p, 1, msec);
        } while (rc < 0 && errno == EINTR);
        if (rc == 0 && timedOut)
            *timedOut = true;
        if (rc > 0)
            acceptPending();
    }
    return !m_pending.isEmpty();
}

/*!
    Returns the access options.

    \sa setSocketOptions()
*/
QLocalServer::SocketOptions QLocalServer::socketOptions() const { return m_options; }

/*!
    Sets the access options to \a options, which apply from the next listen().

    \sa socketOptions()
*/
void QLocalServer::setSocketOptions(SocketOptions options) { m_options = options; }

/*!
    Returns the native listening descriptor, or \c -1 if not listening.
*/
qintptr QLocalServer::socketDescriptor() const { return m_fd; }

/*!
    Removes the socket file for \a name, which clears the leftovers of a crashed
    server. Returns \c true if there is no longer such a file.

    \sa listen()
*/
bool QLocalServer::removeServer(const QString &name) {
    const QString full = resolveName(name);
    return ::unlink(full.toUtf8().constData()) == 0 || errno == ENOENT;
}

/*!
    Called for each accepted connection with its \a socketDescriptor. The
    default wraps it in a QLocalSocket and queues it for
    nextPendingConnection().
*/
void QLocalServer::incomingConnection(quintptr socketDescriptor) {
    auto *socket = new QLocalSocket(this);
    socket->setSocketDescriptor(static_cast<qintptr>(socketDescriptor));
    m_pending.append(socket);
    newConnection();
}

void QLocalServer::acceptPending() {
    QPointer<QLocalServer> guard(this);
    while (guard && m_fd >= 0 && m_pending.size() < m_maxPending) {
        const int fd = ::accept4(m_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno != EINTR)
                break;
            continue;
        }
        incomingConnection(static_cast<quintptr>(fd));
    }
    if (guard)
        updateNotifier();
}

void QLocalServer::updateNotifier() {
    if (m_notifier)
        m_notifier->setEnabled(m_pending.size() < m_maxPending);
}
