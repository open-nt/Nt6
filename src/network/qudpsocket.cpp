// MIT License - see LICENSE
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include "sockaddr_p.h"

#include <QtNetwork/qtcpsocket.h>
#include <QtNetwork/qudpsocket.h>

#include <cerrno>
#include <cstring>

QTcpSocket::~QTcpSocket() = default;

QUdpSocket::~QUdpSocket() = default;

qint64 QUdpSocket::pendingDatagramSize() const {
    if (socketDescriptor() < 0)
        return -1;
    char c;
    const ssize_t n = ::recv(static_cast<int>(socketDescriptor()), &c, 1, MSG_PEEK | MSG_TRUNC | MSG_DONTWAIT);
    return n < 0 ? -1 : n;
}

bool QUdpSocket::hasPendingDatagrams() const { return pendingDatagramSize() >= 0; }

qint64 QUdpSocket::readDatagram(char *data, qint64 maxSize, QHostAddress *address, quint16 *port) {
    if (socketDescriptor() < 0)
        return -1;
    sockaddr_storage ss;
    socklen_t len = sizeof ss;
    ssize_t n;
    do {
        n = ::recvfrom(static_cast<int>(socketDescriptor()), data, static_cast<std::size_t>(maxSize), 0,
                       reinterpret_cast<sockaddr *>(&ss), &len);
    } while (n < 0 && errno == EINTR);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            setSocketErrorFromErrno(errno);
        return -1;
    }
    quint16 p = 0;
    const QHostAddress a = QtPrivate::fromSockaddr(ss, &p);
    if (address)
        *address = a;
    if (port)
        *port = p;
    return n;
}

QNetworkDatagram QUdpSocket::receiveDatagram(qint64 maxSize) {
    const qint64 size = pendingDatagramSize();
    if (size < 0)
        return QNetworkDatagram();
    const qint64 cap = maxSize < 0 ? size : qMin(maxSize, size);
    QByteArray buf(cap, '\0');
    QHostAddress from;
    quint16 port = 0;
    const qint64 n = readDatagram(buf.data(), cap, &from, &port);
    if (n < 0)
        return QNetworkDatagram();
    buf.resize(n);
    QNetworkDatagram d(buf, localAddress(), localPort());
    d.setSender(from, port);
    return d;
}

qint64 QUdpSocket::writeDatagram(const char *data, qint64 size, const QHostAddress &address, quint16 port) {
    if (!isValid()) {
        const QHostAddress any = address.kind() == 4 ? QHostAddress(QHostAddress::AnyIPv4)
                                 : address.kind() == 6 ? QHostAddress(QHostAddress::AnyIPv6)
                                                       : QHostAddress(QHostAddress::Any);
        if (!bind(any, 0))
            return -1;
    }
    sockaddr_storage ss;
    const socklen_t len = QtPrivate::toSockaddr(address, port, socketFamily(), &ss);
    ssize_t n;
    do {
        n = ::sendto(static_cast<int>(socketDescriptor()), data, static_cast<std::size_t>(size), MSG_NOSIGNAL,
                     reinterpret_cast<sockaddr *>(&ss), len);
    } while (n < 0 && errno == EINTR);
    if (n < 0) {
        setSocketErrorFromErrno(errno);
        return -1;
    }
    return n;
}

qint64 QUdpSocket::writeDatagram(const QNetworkDatagram &datagram) {
    const QByteArray d = datagram.data();
    return writeDatagram(d.constData(), d.size(), datagram.destinationAddress(),
                         static_cast<quint16>(datagram.destinationPort()));
}

qint64 QUdpSocket::readData(char *data, qint64 maxSize) {
    const qint64 n = readDatagram(data, maxSize);
    return n < 0 ? 0 : n;
}

qint64 QUdpSocket::writeData(const char *data, qint64 maxSize) {
    if (state() != ConnectedState) {
        setSocketError(OperationError, QString::fromUtf8("Cannot write: UDP socket is not connected"));
        return -1;
    }
    ssize_t n;
    do {
        n = ::send(static_cast<int>(socketDescriptor()), data, static_cast<std::size_t>(maxSize), MSG_NOSIGNAL);
    } while (n < 0 && errno == EINTR);
    if (n < 0) {
        setSocketErrorFromErrno(errno);
        return -1;
    }
    return n;
}

static bool membership(QUdpSocket *s, const QHostAddress &group, bool join) {
    if (!s->isValid() || !group.isMulticast())
        return false;
    const int fd = static_cast<int>(s->socketDescriptor());
    int rc;
    if (group.kind() == 4) {
        ip_mreq m;
        std::memset(&m, 0, sizeof m);
        m.imr_multiaddr.s_addr = htonl(group.toIPv4Address());
        m.imr_interface.s_addr = htonl(INADDR_ANY);
        rc = ::setsockopt(fd, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &m, sizeof m);
    } else {
        ipv6_mreq m;
        std::memset(&m, 0, sizeof m);
        std::memcpy(&m.ipv6mr_multiaddr, group.bytes(), 16);
        rc = ::setsockopt(fd, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, &m, sizeof m);
    }
    return rc == 0;
}

bool QUdpSocket::joinMulticastGroup(const QHostAddress &groupAddress) { return membership(this, groupAddress, true); }

bool QUdpSocket::leaveMulticastGroup(const QHostAddress &groupAddress) { return membership(this, groupAddress, false); }
