// MIT License - see LICENSE
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <unistd.h>

#include "sockaddr_p.h"

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtNetwork/qabstractsocket.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>

/*!
    \module NtNetwork
    \title Nt Network C++ Classes
    \brief Classes for TCP and UDP networking.
    \cmaketarget Nt::Network

    Nt Network provides sockets driven by the Nt Core event loop.
*/

/*!
    \class QAbstractSocket
    \brief The QAbstractSocket class is the base class of QTcpSocket and QUdpSocket.

    QAbstractSocket is a non-blocking, event-loop driven socket built on POSIX
    sockets. Call connectToHost() to connect, or bind() to receive datagrams,
    then react to the connected(), readyRead() and disconnected() signals:

    \code
    QTcpSocket socket;
    QObject::connect(&socket, &QTcpSocket::connected, [&] { socket.write("hello\n"); });
    socket.connectToHost("localhost", 8080);
    \endcode

    The waitFor...() functions block the calling thread by polling the socket
    and can be used without an event loop.

    \differences
    Host names are resolved synchronously inside connectToHost(), so
    hostFound() is emitted before the function returns. There is no proxy or
    SSL support: the proxy functions, \c proxyAuthenticationRequired() and the
    related error values are declared or omitted but never produced.
    \c pauseMode(), \c setPauseMode(), \c resume(),
    \c setLocalPort() and the other protected peer/local setters are not
    available. Sockets must be used from a single thread.

    On Zephyr the number of sockets is bounded by \c CONFIG_ZVFS_OPEN_MAX, \c
    CONFIG_NET_MAX_CONTEXTS and \c CONFIG_NET_MAX_CONN, and the sockets that
    one thread serves by \c CONFIG_ZVFS_POLL_MAX. See \l{Nt on Zephyr}.
    \enddifferences

    \sa QTcpSocket, QUdpSocket, QTcpServer
*/

/*!
    \enum QAbstractSocket::SocketType

    This enum describes the transport layer protocol of a socket.

    \value TcpSocket TCP, a reliable stream-oriented protocol.
    \value UdpSocket UDP, an unreliable datagram protocol.
    \value SctpSocket SCTP. Declared for source compatibility; not supported.
    \value UnknownSocketType Any other type.
*/

/*!
    \enum QAbstractSocket::NetworkLayerProtocol

    This enum describes the network layer protocol of an address.

    \value IPv4Protocol IPv4.
    \value IPv6Protocol IPv6.
    \value AnyIPProtocol Either IPv4 or IPv6 (a dual-stack address).
    \value UnknownNetworkLayerProtocol Neither IPv4 nor IPv6.
*/

/*!
    \enum QAbstractSocket::SocketError

    This enum describes the socket errors that can occur.

    \value ConnectionRefusedError The connection was refused by the peer.
    \value RemoteHostClosedError The remote host closed the connection.
    \value HostNotFoundError The host address was not found.
    \value SocketAccessError The socket operation failed because the application lacked the required privileges.
    \value SocketResourceError The local system ran out of resources (for example, file descriptors).
    \value SocketTimeoutError The socket operation timed out.
    \value DatagramTooLargeError The datagram was larger than the operating system's limit.
    \value NetworkError An error occurred with the network (for example, the network was unreachable).
    \value AddressInUseError The address specified to bind() is already in use.
    \value SocketAddressNotAvailableError The address specified to bind() does not belong to the host.
    \value UnsupportedSocketOperationError The requested socket operation is not supported by the local operating system.
    \value UnfinishedSocketOperationError Declared for source compatibility; not used.
    \value ProxyAuthenticationRequiredError Declared for source compatibility; not used.
    \value SslHandshakeFailedError Declared for source compatibility; not used.
    \value ProxyConnectionRefusedError Declared for source compatibility; not used.
    \value ProxyConnectionClosedError Declared for source compatibility; not used.
    \value ProxyConnectionTimeoutError Declared for source compatibility; not used.
    \value ProxyNotFoundError Declared for source compatibility; not used.
    \value ProxyProtocolError Declared for source compatibility; not used.
    \value OperationError An operation was attempted while the socket was in a state that did not permit it.
    \value SslInternalError Declared for source compatibility; not used.
    \value SslInvalidUserDataError Declared for source compatibility; not used.
    \value TemporaryError Declared for source compatibility; not used.
    \value UnknownSocketError An unidentified error occurred.
*/

/*!
    \enum QAbstractSocket::SocketState

    This enum describes the different states in which a socket can be.

    \value UnconnectedState The socket is not connected.
    \value HostLookupState The socket is performing a host name lookup.
    \value ConnectingState The socket has started establishing a connection.
    \value ConnectedState A connection is established.
    \value BoundState The socket is bound to an address and port.
    \value ListeningState For internal use only.
    \value ClosingState The socket is about to close; pending data is still being written.
*/

/*!
    \enum QAbstractSocket::SocketOption

    This enum represents the options that can be set on a socket. Use
    setSocketOption() and socketOption() to access them.

    \value LowDelayOption Disables Nagle's algorithm (\c TCP_NODELAY) when set to 1.
    \value KeepAliveOption Enables TCP keep-alive (\c SO_KEEPALIVE) when set to 1.
    \value MulticastTtlOption Sets the time-to-live (hop limit) of outgoing multicast datagrams.
    \value MulticastLoopbackOption Controls whether multicast datagrams are looped back to the local host.
    \value TypeOfServiceOption Sets the IP type-of-service field (\c IP_TOS).
    \value SendBufferSizeSocketOption Sets the socket send buffer size in bytes.
    \value ReceiveBufferSizeSocketOption Sets the socket receive buffer size in bytes.
    \value PathMtuSocketOption Declared for source compatibility; not supported.
*/

/*!
    \enum QAbstractSocket::BindFlag

    This enum describes the different flags you can pass to modify the
    behavior of bind().

    \value ShareAddress Allows other services to bind to the same address and port (\c SO_REUSEADDR).
    \value DontShareAddress Binds the address and port exclusively. This is the behavior when no sharing flag is given.
    \value ReuseAddressHint Provides a hint that the address may be reused; treated like ShareAddress.
    \value DefaultForPlatform The default option for the current platform. No special options are set.

    \sa BindMode
*/

/*!
    \typedef QAbstractSocket::BindMode

    Stores an OR combination of BindFlag values.
*/

struct QAbstractSocket::Private {
    QHostAddress local, peer;
    quint16 localPort = 0, peerPort = 0;
    QString peerName;
    std::deque<QHostAddress> pending;
    quint16 connectPort = 0;
    int lastErr = 0;
};

using SocketError = QAbstractSocket::SocketError;

static SocketError mapErrno(int e) {
    switch (e) {
    case ECONNREFUSED:
        return QAbstractSocket::ConnectionRefusedError;
    case ETIMEDOUT:
        return QAbstractSocket::SocketTimeoutError;
    case EADDRINUSE:
        return QAbstractSocket::AddressInUseError;
    case EADDRNOTAVAIL:
        return QAbstractSocket::SocketAddressNotAvailableError;
    case EACCES:
    case EPERM:
        return QAbstractSocket::SocketAccessError;
    case ENETUNREACH:
    case EHOSTUNREACH:
    case ENETDOWN:
    case EHOSTDOWN:
        return QAbstractSocket::NetworkError;
    case ECONNRESET:
    case ECONNABORTED:
    case EPIPE:
        return QAbstractSocket::RemoteHostClosedError;
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM:
        return QAbstractSocket::SocketResourceError;
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
    case EOPNOTSUPP:
        return QAbstractSocket::UnsupportedSocketOperationError;
    case EMSGSIZE:
        return QAbstractSocket::DatagramTooLargeError;
    default:
        return QAbstractSocket::UnknownSocketError;
    }
}

/*!
    Creates a new abstract socket of type \a socketType with the given
    \a parent. The socket starts in the UnconnectedState and no operating
    system socket exists until connectToHost() or bind() is called.
*/
QAbstractSocket::QAbstractSocket(SocketType socketType, QObject *parent)
    : QIODevice(parent), m_type(socketType), d(new Private) {}

/*!
    Destroys the socket and closes the underlying operating system socket
    without emitting disconnected().
*/
QAbstractSocket::~QAbstractSocket() { closeSocket(); }

/*!
    Sets the socket state to \a state and emits stateChanged() if the state
    actually changed.

    \sa state()
*/
void QAbstractSocket::setSocketState(SocketState state) {
    if (m_state == state)
        return;
    m_state = state;
    stateChanged(state);
}

/*!
    Sets the type of error that last occurred to \a error, sets the
    human-readable description to \a text and emits errorOccurred().

    \sa error(), errorString()
*/
void QAbstractSocket::setSocketError(SocketError error, const QString &text) {
    m_error = error;
    setErrorString(text);
    errorOccurred(error);
}

/*!
    Maps the POSIX error number \a err to a SocketError, uses \c strerror()
    as the error string and emits errorOccurred().

    \sa setSocketError()
*/
void QAbstractSocket::setSocketErrorFromErrno(int err) {
    setSocketError(mapErrno(err), QString::fromUtf8(std::strerror(err)));
}

bool QAbstractSocket::adoptSocket(int fd, int family) {
    const int fl = ::fcntl(fd, F_GETFL, 0);
    if (fl >= 0)
        ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    m_fd = fd;
    m_family = family;
    m_rn = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    m_wn = new QSocketNotifier(fd, QSocketNotifier::Write, this);
    m_rn->setEnabled(false);
    m_wn->setEnabled(false);
    QObject::connect(m_rn, &QSocketNotifier::activated, this, [this] { readActivated(); });
    QObject::connect(m_wn, &QSocketNotifier::activated, this, [this] { writeActivated(); });
    return true;
}

/*!
    Creates a non-blocking operating system socket of the given address
    \a family (\c AF_INET or \c AF_INET6) if none exists yet. Returns \c true
    if a socket is available; otherwise sets the error and returns \c false.

    \sa socketFamily()
*/
bool QAbstractSocket::createSocket(int family) {
    if (m_fd >= 0)
        return true;
    const int fd = QtPrivate::nonBlockingSocket(family, m_type == UdpSocket ? SOCK_DGRAM : SOCK_STREAM);
    if (fd < 0) {
        setSocketErrorFromErrno(errno);
        return false;
    }
    return adoptSocket(fd, family);
}

void QAbstractSocket::closeSocket() {
    if (m_rn) {
        m_rn->setEnabled(false);
        m_rn->deleteLater();
        m_rn = nullptr;
    }
    if (m_wn) {
        m_wn->setEnabled(false);
        m_wn->deleteLater();
        m_wn = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    m_family = 0;
}

void QAbstractSocket::updateNotifiers() {
    const bool open = m_fd >= 0;
    const bool wantRead = open && (m_state == ConnectedState || m_state == BoundState || m_state == ClosingState) &&
                          (m_readBufferSize <= 0 || m_rbuf.size() - m_rpos < m_readBufferSize);
    const bool wantWrite = open && (m_state == ConnectingState || !m_wbuf.isEmpty());
    if (m_rn)
        m_rn->setEnabled(wantRead);
    if (m_wn)
        m_wn->setEnabled(wantWrite);
}

void QAbstractSocket::updateAddresses() {
    if (m_fd < 0)
        return;
    sockaddr_storage ss;
    socklen_t len = sizeof ss;
    // d is created by the constructor and never null.
    // NOLINTBEGIN(clang-analyzer-core.CallAndMessage)
    if (::getsockname(m_fd, reinterpret_cast<sockaddr *>(&ss), &len) == 0)
        d->local = QtPrivate::fromSockaddr(ss, &d->localPort);
    len = sizeof ss;
    if (::getpeername(m_fd, reinterpret_cast<sockaddr *>(&ss), &len) == 0)
        d->peer = QtPrivate::fromSockaddr(ss, &d->peerPort);
    // NOLINTEND(clang-analyzer-core.CallAndMessage)
}

/*!
    Returns the host port number, in native byte order, of the local socket,
    or 0 if the socket is neither connected nor bound.

    \sa localAddress(), peerPort()
*/
quint16 QAbstractSocket::localPort() const { return d->localPort; }

/*!
    Returns the host address of the local socket, or a null address if the
    socket is neither connected nor bound.

    \sa localPort(), peerAddress()
*/
QHostAddress QAbstractSocket::localAddress() const { return d->local; }

/*!
    Returns the port of the connected peer, or 0 if the socket is not
    connected.

    \sa peerAddress(), localPort()
*/
quint16 QAbstractSocket::peerPort() const { return d->peerPort; }

/*!
    Returns the address of the connected peer, or a null address if the
    socket is not connected.

    \sa peerName(), peerPort(), localAddress()
*/
QHostAddress QAbstractSocket::peerAddress() const { return d->peer; }

/*!
    Returns the name of the peer as specified to connectToHost(), or the
    peer address as a string if connectToHost() was given an address or the
    name is unknown.

    \sa peerAddress(), connectToHost()
*/
QString QAbstractSocket::peerName() const { return d->peerName.isEmpty() ? d->peer.toString() : d->peerName; }

/*!
    Sets the size of the internal read buffer to \a size bytes. If the buffer
    is full, the socket stops reading from the network until the application
    consumes data. A size of 0 (the default) means the buffer is unlimited.

    \sa readBufferSize(), read()
*/
void QAbstractSocket::setReadBufferSize(qint64 size) {
    m_readBufferSize = size;
    updateNotifiers();
}

/*!
    Initializes the socket to use the existing operating system socket
    \a socketDescriptor, in the given \a state and \a openMode. The
    descriptor is switched to non-blocking mode and the socket takes ownership
    of it. Returns \c true on success; otherwise sets the error and returns
    \c false.

    \sa socketDescriptor()
*/
bool QAbstractSocket::setSocketDescriptor(qintptr fd, SocketState state, OpenMode mode) {
    closeSocket();
    m_rbuf.clear();
    m_rpos = 0;
    m_wbuf.clear();
    sockaddr_storage ss;
    socklen_t len = sizeof ss;
    if (::getsockname(static_cast<int>(fd), reinterpret_cast<sockaddr *>(&ss), &len) != 0) {
        setSocketErrorFromErrno(errno);
        return false;
    }
    adoptSocket(static_cast<int>(fd), ss.ss_family);
    QIODevice::open(mode);
    d->peerName.clear();
    updateAddresses();
    setSocketState(state);
    updateNotifiers();
    return true;
}

/*!
    Attempts to make a connection to \a hostName on the given \a port. The
    \a protocol parameter restricts the lookup to IPv4 or IPv6 addresses.

    The host name is resolved synchronously and hostFound() is emitted. The
    socket then enters ConnectingState and connected() is emitted once the
    connection is established, or errorOccurred() if every address fails.
    The socket is opened in \a mode.

    \sa connected(), waitForConnected(), disconnectFromHost()
*/
void QAbstractSocket::connectToHost(const QString &hostName, quint16 port, OpenMode mode,
                                    NetworkLayerProtocol protocol) {
    if (m_state != UnconnectedState && m_state != BoundState) {
        setSocketError(OperationError, QString::fromUtf8("Trying to connect while connection is in progress"));
        return;
    }
    QIODevice::open(mode);
    m_rbuf.clear();
    m_rpos = 0;
    m_wbuf.clear();
    d->pending.clear();
    d->peerName = hostName;
    d->connectPort = port;
    d->lastErr = 0;
    setSocketState(HostLookupState);

    QHostAddress literal;
    if (literal.setAddress(hostName)) {
        d->pending.push_back(literal);
    } else {
        addrinfo hints;
        std::memset(&hints, 0, sizeof hints);
        hints.ai_family = protocol == IPv4Protocol ? AF_INET : protocol == IPv6Protocol ? AF_INET6 : AF_UNSPEC;
        hints.ai_socktype = m_type == UdpSocket ? SOCK_DGRAM : SOCK_STREAM;
        addrinfo *res = nullptr;
        const std::string host = hostName.toUtf8().toStdString();
        const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &res);
        if (rc == 0) {
            for (addrinfo *p = res; p; p = p->ai_next) {
                sockaddr_storage ss;
                std::memset(&ss, 0, sizeof ss);
                std::memcpy(&ss, p->ai_addr, std::min<std::size_t>(p->ai_addrlen, sizeof ss));
                d->pending.push_back(QtPrivate::fromSockaddr(ss, nullptr));
            }
        }
        if (res)
            ::freeaddrinfo(res);
        if (d->pending.empty()) {
            setSocketState(UnconnectedState);
            setOpenMode(NotOpen);
            setSocketError(HostNotFoundError, QString::fromUtf8("Host not found"));
            return;
        }
    }
    QPointer<QAbstractSocket> self(this);
    hostFound();
    if (!self)
        return;
    connectNext();
}

/*!
    \overload

    Attempts to make a connection to \a address on the given \a port. No
    host name lookup is performed, so hostFound() is not emitted.
*/
void QAbstractSocket::connectToHost(const QHostAddress &address, quint16 port, OpenMode mode) {
    if (m_state != UnconnectedState && m_state != BoundState) {
        setSocketError(OperationError, QString::fromUtf8("Trying to connect while connection is in progress"));
        return;
    }
    QIODevice::open(mode);
    m_rbuf.clear();
    m_rpos = 0;
    m_wbuf.clear();
    d->pending.clear();
    d->pending.push_back(address);
    d->peerName.clear();
    d->connectPort = port;
    d->lastErr = 0;
    connectNext();
}

void QAbstractSocket::connectNext() {
    while (!d->pending.empty()) {
        const QHostAddress a = d->pending.front();
        d->pending.pop_front();
        const int fam = a.kind() == 4 ? AF_INET : AF_INET6;
        if (m_fd >= 0 && m_family != fam && m_state != ConnectingState && m_state != HostLookupState)
            closeSocket();
        if (m_fd < 0 && !createSocket(fam)) {
            d->lastErr = errno ? errno : EAFNOSUPPORT;
            continue;
        }
        sockaddr_storage ss;
        const socklen_t len = QtPrivate::toSockaddr(a, d->connectPort, m_family, &ss);
        setSocketState(ConnectingState);
        const int r = ::connect(m_fd, reinterpret_cast<sockaddr *>(&ss), len);
        if (r == 0) {
            socketConnected();
            return;
        }
        if (errno == EINPROGRESS || errno == EINTR) {
            updateNotifiers();
            return;
        }
        d->lastErr = errno;
        closeSocket();
    }
    setSocketState(UnconnectedState);
    setOpenMode(NotOpen);
    if (d->lastErr)
        setSocketErrorFromErrno(d->lastErr);
    else
        setSocketError(HostNotFoundError, QString::fromUtf8("Host not found"));
}

void QAbstractSocket::socketConnected() {
    updateAddresses();
    setSocketState(ConnectedState);
    updateNotifiers();
    connected();
}

void QAbstractSocket::writeActivated() {
    if (m_state == ConnectingState) {
        int err = 0;
        socklen_t l = sizeof err;
        if (::getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &err, &l) != 0)
            err = errno;
        if (err == 0) {
            socketConnected();
        } else {
            d->lastErr = err;
            closeSocket();
            connectNext();
        }
        return;
    }
    flushWrites();
}

qint64 QAbstractSocket::flushWrites() {
    if (m_fd < 0)
        return 0;
    QPointer<QAbstractSocket> self(this);
    qint64 sent = 0;
    int fatal = 0;
    while (!m_wbuf.isEmpty()) {
        const ssize_t n = ::send(m_fd, m_wbuf.constData(), static_cast<std::size_t>(m_wbuf.size()), MSG_NOSIGNAL);
        if (n > 0) {
            m_wbuf.remove(0, n);
            sent += n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else {
            fatal = errno ? errno : EPIPE; // NOLINT(clang-analyzer-unix.Errno)
            break;
        }
    }
    updateNotifiers();
    if (sent > 0) {
        socketBytesWritten(sent);
        if (!self)
            return sent;
    }
    if (fatal) {
        setSocketErrorFromErrno(fatal);
        if (!self)
            return sent;
        finishDisconnect();
        return sent;
    }
    if (m_wbuf.isEmpty() && m_state == ClosingState)
        finishDisconnect();
    return sent;
}

/*!
    \internal

    Closes the socket and emits disconnected().
*/
void QAbstractSocket::finishDisconnect() {
    const bool wasActive = m_state != UnconnectedState;
    closeSocket();
    m_wbuf.clear();
    d->pending.clear();
    setSocketState(UnconnectedState);
    if (wasActive)
        disconnected();
}

/*!
    \internal

    Handles the peer closing the connection.
*/
void QAbstractSocket::remoteClosed() {
    QPointer<QAbstractSocket> self(this);
    readChannelFinished();
    if (!self)
        return;
    setSocketError(RemoteHostClosedError, QString::fromUtf8("The remote host closed the connection"));
    if (!self)
        return;
    finishDisconnect();
}

/*!
    \internal

    Handles \a size bytes in \a data as read from the operating system.
    QSslSocket reimplements this to decrypt them.
*/
void QAbstractSocket::receivedRaw(const char *data, qint64 size) { deliverData(data, size); }

/*!
    \internal

    Queues \a size bytes in \a data for the reader, and emits readyRead().
*/
void QAbstractSocket::deliverData(const char *data, qint64 size) {
    m_rbuf.append(data, size);
    updateNotifiers();
    m_readyReadEmitted = true;
    QPointer<QAbstractSocket> self(this); // a slot may delete the socket
    readyRead();
    if (self)
        channelReadyRead(0);
}

/*!
    \internal

    Reports that \a size bytes reached the operating system. QSslSocket
    reimplements this, as those bytes are encrypted.
*/
void QAbstractSocket::socketBytesWritten(qint64 size) {
    QPointer<QAbstractSocket> self(this);
    bytesWritten(size);
    if (self)
        channelBytesWritten(0, size);
}

/*!
    Called when the operating system socket becomes readable. TCP sockets
    read the available bytes into the internal buffer and emit readyRead();
    a zero-byte read closes the socket with RemoteHostClosedError. UDP
    sockets only emit readyRead() and leave the datagram queued.
*/
void QAbstractSocket::readActivated() {
    if (m_fd < 0 || m_state == ConnectingState)
        return;
    if (m_type == UdpSocket) {
        m_readyReadEmitted = true;
        QPointer<QAbstractSocket> self(this);
        readyRead();
        if (self)
            channelReadyRead(0);
        return;
    }
    if (m_readBufferSize > 0 && m_rbuf.size() - m_rpos >= m_readBufferSize) {
        updateNotifiers();
        return;
    }
    char buf[65536];
    ssize_t n;
    do {
        n = ::recv(m_fd, buf, sizeof buf, 0);
    } while (n < 0 && errno == EINTR);
    if (n > 0) {
        receivedRaw(buf, n);
    } else if (n == 0) {
        remoteClosed();
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        connectionLost(errno);
    }
}

/*!
    \fn void QAbstractSocket::endWaitForReadyRead()
    \internal

    Makes a waitForReadyRead() in progress return.
*/

/*!
    \internal

    Handles a read failing with the system error \a error.
*/
void QAbstractSocket::connectionLost(int error) {
    QPointer<QAbstractSocket> self(this);
    setSocketErrorFromErrno(error);
    if (self)
        finishDisconnect();
}

/*!
    \reimp

    Copies up to \a maxSize bytes from the internal read buffer into \a data
    and returns the number of bytes copied.
*/
qint64 QAbstractSocket::readData(char *data, qint64 maxSize) {
    const qint64 avail = m_rbuf.size() - m_rpos;
    if (avail <= 0)
        return 0;
    const qint64 n = std::min(avail, maxSize);
    std::copy(m_rbuf.constData() + m_rpos, m_rbuf.constData() + m_rpos + n, data);
    m_rpos += n;
    if (m_rpos == m_rbuf.size()) {
        m_rbuf.clear();
        m_rpos = 0;
    } else if (m_rpos > 65536) {
        m_rbuf.remove(0, m_rpos);
        m_rpos = 0;
    }
    if (m_readBufferSize > 0)
        updateNotifiers();
    return n;
}

/*!
    \reimp

    Queues \a maxSize bytes from \a data for sending and returns \a maxSize.
    Returns -1 with OperationError if the socket is not connected.
*/
qint64 QAbstractSocket::writeData(const char *data, qint64 maxSize) {
    if (m_state != ConnectedState && m_state != ClosingState) {
        setSocketError(OperationError, QString::fromUtf8("Cannot write: socket is not connected"));
        return -1;
    }
    m_wbuf.append(data, maxSize);
    updateNotifiers();
    return maxSize;
}

/*!
    \reimp

    Returns the number of incoming bytes that are waiting to be read.
*/
qint64 QAbstractSocket::bytesAvailable() const { return QIODevice::bytesAvailable() + (m_rbuf.size() - m_rpos); }

/*!
    \reimp

    Returns the number of bytes waiting to be written to the network.
*/
qint64 QAbstractSocket::bytesToWrite() const { return m_wbuf.size(); }

/*!
    \reimp

    Returns \c true if a complete line of data can be read from the socket.
*/
bool QAbstractSocket::canReadLine() const { return QIODevice::canReadLine() || m_rbuf.indexOf('\n', m_rpos) != -1; }

/*!
    Attempts to send the queued outgoing data to the network without
    blocking. Returns \c true if at least one byte was sent; otherwise
    returns \c false.

    \sa bytesToWrite(), waitForBytesWritten()
*/
bool QAbstractSocket::flush() {
    if (m_fd < 0 || m_wbuf.isEmpty())
        return false;
    return flushWrites() > 0;
}

/*!
    Attempts to close the socket. If there is pending data waiting to be
    written, the socket enters ClosingState and closes once the data has been
    sent. Otherwise it closes immediately and emits disconnected().

    \sa connectToHost(), close(), abort()
*/
void QAbstractSocket::disconnectFromHost() {
    switch (m_state) {
    case UnconnectedState:
    case ClosingState:
        return;
    case HostLookupState:
    case ConnectingState:
        closeSocket();
        d->pending.clear();
        setSocketState(UnconnectedState);
        setOpenMode(NotOpen);
        return;
    case BoundState:
    case ListeningState:
        closeSocket();
        setSocketState(UnconnectedState);
        return;
    case ConnectedState:
        if (!m_wbuf.isEmpty()) {
            setSocketState(ClosingState);
            updateNotifiers();
            return;
        }
        finishDisconnect();
        return;
    }
}

/*!
    \reimp

    Flushes pending data, closes the socket, discards any unread data and
    emits disconnected() if the socket was connected.

    \sa disconnectFromHost(), abort()
*/
void QAbstractSocket::close() {
    QPointer<QAbstractSocket> self(this);
    QIODevice::close();
    if (!self)
        return;
    if (m_state == ConnectedState && !m_wbuf.isEmpty())
        flushWrites();
    if (!self)
        return;
    m_rbuf.clear();
    m_rpos = 0;
    const bool wasConnected = m_state == ConnectedState || m_state == ClosingState;
    closeSocket();
    m_wbuf.clear();
    d->pending.clear();
    setSocketState(UnconnectedState);
    if (wasConnected && self)
        disconnected();
}

/*!
    Aborts the current connection and discards any pending outgoing data,
    then closes the socket.

    \sa disconnectFromHost(), close()
*/
void QAbstractSocket::abort() {
    m_wbuf.clear();
    close();
}

/*!
    Binds the socket to \a address on \a port using the given \a mode. A
    \a port of 0 lets the system choose a free port. Returns \c true on
    success, after which the socket is in BoundState; otherwise sets the error
    and returns \c false.

    For UDP sockets, the socket is opened for reading and writing once bound.
    Binding to QHostAddress::Any creates a dual-stack socket when the system
    supports it.

    \sa localPort(), localAddress()
*/
bool QAbstractSocket::bind(const QHostAddress &address, quint16 port, BindMode mode) {
    if (m_state != UnconnectedState) {
        setSocketError(OperationError, QString::fromUtf8("The socket is already bound or connected"));
        return false;
    }
    int fam = address.kind() == 4 ? AF_INET : AF_INET6;
    const bool dual = address.kind() == 7 || address.kind() == 0;
    if (!createSocket(fam)) {
        if (!dual)
            return false;
        fam = AF_INET;
        if (!createSocket(fam))
            return false;
    }
    if (m_family == AF_INET6) {
        const int v6only = address.kind() == 6 ? 1 : 0;
        ::setsockopt(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof v6only);
    }
    if (mode.testFlag(ShareAddress) || mode.testFlag(ReuseAddressHint)) {
        const int on = 1;
        ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    }
    sockaddr_storage ss;
    const socklen_t len = QtPrivate::toSockaddr(address, port, m_family, &ss);
    if (::bind(m_fd, reinterpret_cast<sockaddr *>(&ss), len) != 0) {
        const int e = errno;
        closeSocket();
        setSocketErrorFromErrno(e);
        return false;
    }
    updateAddresses();
    if (m_type == UdpSocket)
        QIODevice::open(ReadWrite);
    setSocketState(BoundState);
    updateNotifiers();
    return true;
}

/*!
    \overload

    Binds to QHostAddress::Any on \a port using the given \a mode.
*/
bool QAbstractSocket::bind(quint16 port, BindMode mode) { return bind(QHostAddress(QHostAddress::Any), port, mode); }

static bool socketOptionLevel(QAbstractSocket::SocketOption o, int family, int *level, int *name) {
    switch (o) {
    case QAbstractSocket::LowDelayOption:
        *level = IPPROTO_TCP;
        *name = TCP_NODELAY;
        return true;
    case QAbstractSocket::KeepAliveOption:
        *level = SOL_SOCKET;
        *name = SO_KEEPALIVE;
        return true;
    case QAbstractSocket::MulticastTtlOption:
        *level = family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP;
        *name = family == AF_INET6 ? IPV6_MULTICAST_HOPS : IP_MULTICAST_TTL;
        return true;
    case QAbstractSocket::MulticastLoopbackOption:
        *level = family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP;
        *name = family == AF_INET6 ? IPV6_MULTICAST_LOOP : IP_MULTICAST_LOOP;
        return true;
    case QAbstractSocket::TypeOfServiceOption:
        *level = IPPROTO_IP;
        *name = IP_TOS;
        return true;
    case QAbstractSocket::SendBufferSizeSocketOption:
        *level = SOL_SOCKET;
        *name = SO_SNDBUF;
        return true;
    case QAbstractSocket::ReceiveBufferSizeSocketOption:
        *level = SOL_SOCKET;
        *name = SO_RCVBUF;
        return true;
    default:
        return false;
    }
}

/*!
    Sets \a option to \a value, converted to an integer. Does nothing if the
    socket has no operating system socket yet, the option is not supported or
    \a value cannot be converted.

    \sa socketOption()
*/
void QAbstractSocket::setSocketOption(SocketOption option, const QVariant &value) {
    int level, name;
    bool ok = false;
    int v = value.toInt(&ok);
    if (ok && m_fd >= 0 && socketOptionLevel(option, m_family, &level, &name))
        ::setsockopt(m_fd, level, name, &v, sizeof v);
}

/*!
    Returns the value of \a option as an \c int QVariant, or an invalid
    QVariant if the socket has no operating system socket or the option is
    not supported.

    \sa setSocketOption()
*/
QVariant QAbstractSocket::socketOption(SocketOption option) {
    int level, name, value = -1;
    socklen_t len = sizeof value;
    if (m_fd >= 0 && socketOptionLevel(option, m_family, &level, &name) &&
        ::getsockopt(m_fd, level, name, &value, &len) == 0)
        return QVariant(value);
    return QVariant();
}

short QAbstractSocket::pollOnce(short events, int timeoutMs) {
    if (m_fd < 0)
        return 0;
    pollfd p{m_fd, events, 0};
    int r;
    do {
        r = ::poll(&p, 1, timeoutMs);
    } while (r < 0 && errno == EINTR);
    return r > 0 ? p.revents : 0;
}

static int remainingMs(int msecs, const QElapsedTimer &t) {
    if (msecs < 0)
        return -1;
    const qint64 left = msecs - t.elapsed();
    return left < 0 ? 0 : static_cast<int>(left);
}

/*!
    Waits until the socket is connected, up to \a msecs milliseconds (a
    negative value waits forever). Returns \c true if the connection has been
    established; otherwise returns \c false and, on a timeout, sets
    SocketTimeoutError.

    \sa connectToHost(), connected()
*/
bool QAbstractSocket::waitForConnected(int msecs) {
    if (m_state == ConnectedState)
        return true;
    if (m_state != HostLookupState && m_state != ConnectingState)
        return false;
    QElapsedTimer t;
    t.start();
    while (m_state == ConnectingState && m_fd >= 0) {
        if (!pollOnce(POLLOUT, remainingMs(msecs, t))) {
            setSocketError(SocketTimeoutError, QString::fromUtf8("Socket operation timed out"));
            return false;
        }
        writeActivated();
    }
    return m_state == ConnectedState;
}

/*!
    \reimp

    Blocks until new data is available for reading and readyRead() has been
    emitted, or until \a msecs milliseconds have passed. Returns \c true if
    data arrived; otherwise returns \c false.

    \sa waitForBytesWritten()
*/
bool QAbstractSocket::waitForReadyRead(int msecs) {
    if (m_state == ConnectingState && !waitForConnected(msecs))
        return false;
    if (m_fd < 0)
        return false;
    QElapsedTimer t;
    t.start();
    m_readyReadEmitted = false;
    QPointer<QAbstractSocket> self(this);
    while (self && m_fd >= 0 && !m_readyReadEmitted) {
        const short want = static_cast<short>(POLLIN | (m_wbuf.isEmpty() ? 0 : POLLOUT));
        const short ev = pollOnce(want, remainingMs(msecs, t));
        if (!ev) {
            setSocketError(SocketTimeoutError, QString::fromUtf8("Socket operation timed out"));
            return false;
        }
        if (ev & POLLOUT)
            flushWrites();
        if (!self)
            return false;
        if (ev & (POLLIN | POLLHUP | POLLERR))
            readActivated();
    }
    return self && m_readyReadEmitted;
}

/*!
    \reimp

    Blocks until at least some of the queued data has been written to the
    network, or until \a msecs milliseconds have passed. Returns \c true if
    data was written; otherwise returns \c false.

    \sa waitForReadyRead(), flush()
*/
bool QAbstractSocket::waitForBytesWritten(int msecs) {
    if (m_state == ConnectingState && !waitForConnected(msecs))
        return false;
    if (m_fd < 0 || m_wbuf.isEmpty())
        return false;
    QElapsedTimer t;
    t.start();
    QPointer<QAbstractSocket> self(this);
    while (self && m_fd >= 0 && !m_wbuf.isEmpty()) {
        if (!pollOnce(POLLOUT, remainingMs(msecs, t))) {
            setSocketError(SocketTimeoutError, QString::fromUtf8("Socket operation timed out"));
            return false;
        }
        if (flushWrites() > 0)
            return true;
    }
    return false;
}

/*!
    Waits until the socket has disconnected, up to \a msecs milliseconds.
    Returns \c true if the connection has been closed; otherwise returns
    \c false.

    \sa disconnectFromHost(), disconnected()
*/
bool QAbstractSocket::waitForDisconnected(int msecs) {
    if (m_state == UnconnectedState)
        return false;
    QElapsedTimer t;
    t.start();
    QPointer<QAbstractSocket> self(this);
    while (self && m_fd >= 0 && m_state != UnconnectedState) {
        const short want = static_cast<short>(POLLIN | (m_wbuf.isEmpty() ? 0 : POLLOUT));
        const short ev = pollOnce(want, remainingMs(msecs, t));
        if (!ev) {
            setSocketError(SocketTimeoutError, QString::fromUtf8("Socket operation timed out"));
            return false;
        }
        if (ev & POLLOUT)
            flushWrites();
        if (!self)
            return true;
        if (m_fd >= 0 && (ev & (POLLIN | POLLHUP | POLLERR)))
            readActivated();
    }
    return !self || m_state == UnconnectedState;
}

#include "moc_qabstractsocket.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qtcpsocket.cpp"      // NOLINT(bugprone-suspicious-include): generated by ntmoc
