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
    case ECONNREFUSED: return QAbstractSocket::ConnectionRefusedError;
    case ETIMEDOUT: return QAbstractSocket::SocketTimeoutError;
    case EADDRINUSE: return QAbstractSocket::AddressInUseError;
    case EADDRNOTAVAIL: return QAbstractSocket::SocketAddressNotAvailableError;
    case EACCES:
    case EPERM: return QAbstractSocket::SocketAccessError;
    case ENETUNREACH:
    case EHOSTUNREACH:
    case ENETDOWN:
    case EHOSTDOWN: return QAbstractSocket::NetworkError;
    case ECONNRESET:
    case ECONNABORTED:
    case EPIPE: return QAbstractSocket::RemoteHostClosedError;
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM: return QAbstractSocket::SocketResourceError;
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
    case EOPNOTSUPP: return QAbstractSocket::UnsupportedSocketOperationError;
    case EMSGSIZE: return QAbstractSocket::DatagramTooLargeError;
    default: return QAbstractSocket::UnknownSocketError;
    }
}

QAbstractSocket::QAbstractSocket(SocketType socketType, QObject *parent)
    : QIODevice(parent), m_type(socketType), d(new Private) {}

QAbstractSocket::~QAbstractSocket() { closeSocket(); }

void QAbstractSocket::setSocketState(SocketState state) {
    if (m_state == state)
        return;
    m_state = state;
    stateChanged(state);
}

void QAbstractSocket::setSocketError(SocketError error, const QString &text) {
    m_error = error;
    setErrorString(text);
    errorOccurred(error);
}

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

bool QAbstractSocket::createSocket(int family) {
    if (m_fd >= 0)
        return true;
    const int type = (m_type == UdpSocket ? SOCK_DGRAM : SOCK_STREAM) | SOCK_NONBLOCK | SOCK_CLOEXEC;
    const int fd = ::socket(family, type, 0);
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
    if (::getsockname(m_fd, reinterpret_cast<sockaddr *>(&ss), &len) == 0)
        d->local = QtPrivate::fromSockaddr(ss, &d->localPort);
    len = sizeof ss;
    if (::getpeername(m_fd, reinterpret_cast<sockaddr *>(&ss), &len) == 0)
        d->peer = QtPrivate::fromSockaddr(ss, &d->peerPort);
}

quint16 QAbstractSocket::localPort() const { return d->localPort; }
QHostAddress QAbstractSocket::localAddress() const { return d->local; }
quint16 QAbstractSocket::peerPort() const { return d->peerPort; }
QHostAddress QAbstractSocket::peerAddress() const { return d->peer; }
QString QAbstractSocket::peerName() const { return d->peerName.isEmpty() ? d->peer.toString() : d->peerName; }

void QAbstractSocket::setReadBufferSize(qint64 size) {
    m_readBufferSize = size;
    updateNotifiers();
}

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
            fatal = errno ? errno : EPIPE;
            break;
        }
    }
    updateNotifiers();
    if (sent > 0) {
        bytesWritten(sent);
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

void QAbstractSocket::finishDisconnect() {
    const bool wasActive = m_state != UnconnectedState;
    closeSocket();
    m_wbuf.clear();
    d->pending.clear();
    setSocketState(UnconnectedState);
    if (wasActive)
        disconnected();
}

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

void QAbstractSocket::readActivated() {
    if (m_fd < 0 || m_state == ConnectingState)
        return;
    if (m_type == UdpSocket) {
        m_readyReadEmitted = true;
        readyRead();
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
        m_rbuf.append(buf, n);
        updateNotifiers();
        m_readyReadEmitted = true;
        readyRead();
    } else if (n == 0) {
        remoteClosed();
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        const int e = errno;
        setSocketErrorFromErrno(e == ECONNRESET ? ECONNRESET : e);
        finishDisconnect();
    }
}

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

qint64 QAbstractSocket::writeData(const char *data, qint64 maxSize) {
    if (m_state != ConnectedState && m_state != ClosingState) {
        setSocketError(OperationError, QString::fromUtf8("Cannot write: socket is not connected"));
        return -1;
    }
    m_wbuf.append(data, maxSize);
    updateNotifiers();
    return maxSize;
}

qint64 QAbstractSocket::bytesAvailable() const { return QIODevice::bytesAvailable() + (m_rbuf.size() - m_rpos); }

qint64 QAbstractSocket::bytesToWrite() const { return m_wbuf.size(); }

bool QAbstractSocket::canReadLine() const { return QIODevice::canReadLine() || m_rbuf.indexOf('\n', m_rpos) != -1; }

bool QAbstractSocket::flush() {
    if (m_fd < 0 || m_wbuf.isEmpty())
        return false;
    return flushWrites() > 0;
}

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

void QAbstractSocket::abort() {
    m_wbuf.clear();
    close();
}

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

bool QAbstractSocket::bind(quint16 port, BindMode mode) { return bind(QHostAddress(QHostAddress::Any), port, mode); }

static bool socketOptionLevel(QAbstractSocket::SocketOption o, int family, int *level, int *name) {
    switch (o) {
    case QAbstractSocket::LowDelayOption: *level = IPPROTO_TCP; *name = TCP_NODELAY; return true;
    case QAbstractSocket::KeepAliveOption: *level = SOL_SOCKET; *name = SO_KEEPALIVE; return true;
    case QAbstractSocket::MulticastTtlOption:
        *level = family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP;
        *name = family == AF_INET6 ? IPV6_MULTICAST_HOPS : IP_MULTICAST_TTL;
        return true;
    case QAbstractSocket::MulticastLoopbackOption:
        *level = family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP;
        *name = family == AF_INET6 ? IPV6_MULTICAST_LOOP : IP_MULTICAST_LOOP;
        return true;
    case QAbstractSocket::TypeOfServiceOption: *level = IPPROTO_IP; *name = IP_TOS; return true;
    case QAbstractSocket::SendBufferSizeSocketOption: *level = SOL_SOCKET; *name = SO_SNDBUF; return true;
    case QAbstractSocket::ReceiveBufferSizeSocketOption: *level = SOL_SOCKET; *name = SO_RCVBUF; return true;
    default: return false;
    }
}

void QAbstractSocket::setSocketOption(SocketOption option, int value) {
    int level, name;
    if (m_fd >= 0 && socketOptionLevel(option, m_family, &level, &name))
        ::setsockopt(m_fd, level, name, &value, sizeof value);
}

int QAbstractSocket::socketOption(SocketOption option) {
    int level, name, value = -1;
    socklen_t len = sizeof value;
    if (m_fd >= 0 && socketOptionLevel(option, m_family, &level, &name) &&
        ::getsockopt(m_fd, level, name, &value, &len) == 0)
        return value;
    return -1;
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
