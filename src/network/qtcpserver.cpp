// MIT License - see LICENSE
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include "sockaddr_p.h"

#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>

struct QTcpServer::Private {
    std::deque<QTcpSocket *> pending;
    QHostAddress address;
    quint16 port = 0;
    bool paused = false;
};

/*!
    \class QTcpServer
    \brief The QTcpServer class provides a TCP-based server.

    Call listen() to start accepting connections, then connect to
    newConnection() and fetch each accepted socket with
    nextPendingConnection():

    \code
    QTcpServer server;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        QTcpSocket *client = server.nextPendingConnection();
        client->write("hello\n");
    });
    server.listen(QHostAddress::Any, 8080);
    \endcode

    Accepted sockets are children of the server. The server stops accepting
    once maxPendingConnections() sockets are waiting and resumes as they are
    taken.

    \differences
    There is no \c listenBacklogSize(), \c setListenBacklogSize() or proxy
    support; the backlog is fixed at 128. Sockets are always bound with
    \c SO_REUSEADDR. \c serverError() and \c errorString() report only the
    errors of listen(), setSocketDescriptor() and accept. Servers must be
    used from a single thread.
    \enddifferences

    \sa QTcpSocket, QAbstractSocket
*/

/*!
    Constructs a TCP server with the given \a parent. The server is not
    listening.
*/
QTcpServer::QTcpServer(QObject *parent) : QObject(parent), d(new Private) {}

/*!
    Destroys the server, closing it if it is listening.

    \sa close()
*/
QTcpServer::~QTcpServer() { close(); }

/*!
    Sets the server to listen on the existing operating system socket
    \a socketDescriptor, which must already be bound and listening. The
    descriptor is switched to non-blocking mode and the server takes ownership
    of it. Returns \c true on success; returns \c false if the server is
    already listening or the descriptor is invalid.

    \sa socketDescriptor(), listen()
*/
bool QTcpServer::setSocketDescriptor(qintptr fd) {
    if (isListening())
        return false;
    sockaddr_storage ss;
    socklen_t len = sizeof ss;
    if (::getsockname(static_cast<int>(fd), reinterpret_cast<sockaddr *>(&ss), &len) != 0) {
        m_errorString = QString::fromUtf8(std::strerror(errno));
        return false;
    }
    const int fl = ::fcntl(static_cast<int>(fd), F_GETFL, 0);
    if (fl >= 0)
        ::fcntl(static_cast<int>(fd), F_SETFL, fl | O_NONBLOCK);
    m_fd = static_cast<int>(fd);
    d->address = QtPrivate::fromSockaddr(ss, &d->port); // NOLINT(clang-analyzer-core.CallAndMessage)
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    QObject::connect(m_notifier, &QSocketNotifier::activated, this, [this] { acceptActivated(); });
    return true;
}

/*!
    Tells the server to listen for incoming connections on \a address and
    \a port. A \a port of 0 lets the system choose a free port. Returns
    \c true on success; otherwise sets serverError() and errorString() and
    returns \c false.

    Listening on QHostAddress::Any accepts both IPv4 and IPv6 connections
    when the system supports it.

    \sa isListening(), close(), serverPort()
*/
bool QTcpServer::listen(const QHostAddress &address, quint16 port) {
    if (isListening()) {
        m_errorString = QString::fromUtf8("Server is already listening");
        return false;
    }
    int fam = address.kind() == 4 ? AF_INET : AF_INET6;
    const bool dual = address.kind() == 7 || address.kind() == 0;
    int fd = ::socket(fam, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0 && dual) {
        fam = AF_INET;
        fd = ::socket(fam, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    }
    auto fail = [&](int e) {
        m_errorString = QString::fromUtf8(std::strerror(e));
        m_error = e == EADDRINUSE ? QAbstractSocket::AddressInUseError
                  : e == EADDRNOTAVAIL ? QAbstractSocket::SocketAddressNotAvailableError
                  : (e == EACCES || e == EPERM) ? QAbstractSocket::SocketAccessError
                                                : QAbstractSocket::UnknownSocketError;
        if (fd >= 0)
            ::close(fd);
        return false;
    };
    if (fd < 0)
        return fail(errno);
    const int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    if (fam == AF_INET6) {
        const int v6only = address.kind() == 6 ? 1 : 0;
        ::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof v6only);
    }
    sockaddr_storage ss;
    const socklen_t len = QtPrivate::toSockaddr(address, port, fam, &ss);
    if (::bind(fd, reinterpret_cast<sockaddr *>(&ss), len) != 0)
        return fail(errno);
    if (::listen(fd, 128) != 0)
        return fail(errno);
    return setSocketDescriptor(fd);
}

/*!
    Closes the server so it no longer listens for incoming connections.
    Connections that were already accepted remain valid.

    \sa listen(), isListening()
*/
void QTcpServer::close() {
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    d->port = 0;
    d->address.clear();
}

/*!
    Returns the port the server is listening on, or 0 if it is not
    listening.

    \sa serverAddress(), listen()
*/
quint16 QTcpServer::serverPort() const { return d->port; }

/*!
    Returns the address the server is listening on, or a null address if it
    is not listening.

    \sa serverPort(), listen()
*/
QHostAddress QTcpServer::serverAddress() const { return d->address; }

/*!
    Pauses accepting new connections. Queued connections stay in the
    operating system backlog until resumeAccepting() is called.

    \sa resumeAccepting()
*/
void QTcpServer::pauseAccepting() {
    d->paused = true;
    if (m_notifier)
        m_notifier->setEnabled(false);
}

/*!
    Resumes accepting new connections after pauseAccepting().

    \sa pauseAccepting()
*/
void QTcpServer::resumeAccepting() {
    d->paused = false;
    if (m_notifier && static_cast<int>(d->pending.size()) < m_maxPending)
        m_notifier->setEnabled(true);
}

void QTcpServer::acceptActivated() {
    QPointer<QTcpServer> self(this);
    while (isListening() && static_cast<int>(d->pending.size()) < m_maxPending) {
        const int fd = ::accept4(m_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                m_errorString = QString::fromUtf8(std::strerror(errno));
                m_error = (errno == EMFILE || errno == ENFILE || errno == ENOBUFS || errno == ENOMEM)
                              ? QAbstractSocket::SocketResourceError
                              : QAbstractSocket::UnknownSocketError;
                acceptError(m_error);
                if (!self)
                    return;
            }
            break;
        }
        const std::size_t before = d->pending.size();
        incomingConnection(fd);
        if (!self)
            return;
        // A subclass may add the socket later, as QSslServer does.
        if (d->pending.size() > before) {
            newConnection();
            if (!self)
                return;
        }
    }
    if (m_notifier && static_cast<int>(d->pending.size()) >= m_maxPending)
        m_notifier->setEnabled(false);
}

/*!
    This virtual function is called when a new connection is available.
    \a handle is the native descriptor of the accepted socket. The default
    implementation creates a QTcpSocket on it and adds it to the pending
    connections with addPendingConnection(). Reimplement it to create a
    different socket type.

    \sa newConnection(), addPendingConnection()
*/
void QTcpServer::incomingConnection(qintptr handle) {
    auto *s = new QTcpSocket(this);
    if (!s->setSocketDescriptor(handle)) {
        ::close(static_cast<int>(handle));
        delete s;
        return;
    }
    addPendingConnection(s);
}

/*!
    Called by incomingConnection() to add \a socket to the queue of pending
    connections. The server becomes the socket's parent.

    \sa nextPendingConnection(), incomingConnection()
*/
void QTcpServer::addPendingConnection(QTcpSocket *socket) {
    socket->setParent(this);
    d->pending.push_back(socket);
    QObject::connect(socket, &QObject::destroyed, this, [this, socket] {
        auto &p = d->pending;
        p.erase(std::remove(p.begin(), p.end(), socket), p.end());
    });
    pendingConnectionAvailable();
}

/*!
    Returns \c true if the server has pending connections; otherwise returns
    \c false.

    \sa nextPendingConnection(), newConnection()
*/
bool QTcpServer::hasPendingConnections() const { return !d->pending.empty(); }

/*!
    Returns the next pending connection as a connected QTcpSocket, or
    \c nullptr if there is none. The server owns the socket as its parent;
    delete it when you are done, or reparent it.

    \sa hasPendingConnections(), newConnection()
*/
QTcpSocket *QTcpServer::nextPendingConnection() {
    if (d->pending.empty())
        return nullptr;
    QTcpSocket *s = d->pending.front();
    d->pending.pop_front();
    if (!d->paused && m_notifier && static_cast<int>(d->pending.size()) < m_maxPending)
        m_notifier->setEnabled(true);
    return s;
}

/*!
    Waits up to \a msec milliseconds for a new connection to arrive (a
    negative value waits forever). Returns \c true if a connection is
    available. Returns \c false on timeout or error; if \a timedOut is not
    null, it is set to \c true on a timeout.

    \sa hasPendingConnections(), nextPendingConnection()
*/
bool QTcpServer::waitForNewConnection(int msec, bool *timedOut) {
    if (timedOut)
        *timedOut = false;
    if (!isListening())
        return false;
    if (!d->pending.empty())
        return true;
    pollfd p{m_fd, POLLIN, 0};
    int r;
    do {
        r = ::poll(&p, 1, msec);
    } while (r < 0 && errno == EINTR);
    if (r <= 0) {
        if (timedOut && r == 0)
            *timedOut = true;
        return false;
    }
    acceptActivated();
    return !d->pending.empty();
}

#include "moc_qtcpserver.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
