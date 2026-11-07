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

QTcpServer::QTcpServer(QObject *parent) : QObject(parent), d(new Private) {}

QTcpServer::~QTcpServer() { close(); }

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
    d->address = QtPrivate::fromSockaddr(ss, &d->port);
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    QObject::connect(m_notifier, &QSocketNotifier::activated, this, [this] { acceptActivated(); });
    return true;
}

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

quint16 QTcpServer::serverPort() const { return d->port; }

QHostAddress QTcpServer::serverAddress() const { return d->address; }

void QTcpServer::pauseAccepting() {
    d->paused = true;
    if (m_notifier)
        m_notifier->setEnabled(false);
}

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
        incomingConnection(fd);
        if (!self)
            return;
        newConnection();
        if (!self)
            return;
    }
    if (m_notifier && static_cast<int>(d->pending.size()) >= m_maxPending)
        m_notifier->setEnabled(false);
}

void QTcpServer::incomingConnection(qintptr handle) {
    auto *s = new QTcpSocket(this);
    if (!s->setSocketDescriptor(handle)) {
        ::close(static_cast<int>(handle));
        delete s;
        return;
    }
    addPendingConnection(s);
}

void QTcpServer::addPendingConnection(QTcpSocket *socket) {
    socket->setParent(this);
    d->pending.push_back(socket);
    QObject::connect(socket, &QObject::destroyed, this, [this, socket] {
        auto &p = d->pending;
        p.erase(std::remove(p.begin(), p.end(), socket), p.end());
    });
}

bool QTcpServer::hasPendingConnections() const { return !d->pending.empty(); }

QTcpSocket *QTcpServer::nextPendingConnection() {
    if (d->pending.empty())
        return nullptr;
    QTcpSocket *s = d->pending.front();
    d->pending.pop_front();
    if (!d->paused && m_notifier && static_cast<int>(d->pending.size()) < m_maxPending)
        m_notifier->setEnabled(true);
    return s;
}

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
