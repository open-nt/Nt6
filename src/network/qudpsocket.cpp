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

/*!
    \class QTcpSocket
    \brief The QTcpSocket class provides a TCP socket.

    QTcpSocket is a stream-oriented QAbstractSocket for TCP clients. Connect
    with connectToHost() and use the QIODevice read and write functions:

    \code
    QTcpSocket socket;
    socket.connectToHost("example.com", 80);
    if (socket.waitForConnected(3000)) {
        socket.write("GET / HTTP/1.0\r\n\r\n");
        socket.waitForReadyRead(3000);
        QByteArray reply = socket.readAll();
    }
    \endcode

    \differences
    QTcpSocket adds nothing to QAbstractSocket and there is no QSslSocket
    subclass. The protected constructor taking a SocketType replaces Qt's
    constructor that takes a private class.
    \enddifferences

    \sa QAbstractSocket, QUdpSocket, QTcpServer
*/

/*!
    \fn QTcpSocket::QTcpSocket(QObject *parent)

    Creates a TCP socket with the given \a parent. The socket is not
    connected.
*/

/*!
    \fn QTcpSocket::QTcpSocket(SocketType type, QObject *parent)

    Creates a socket of the given \a type with the given \a parent. This
    constructor is for subclasses that need a socket type other than
    TcpSocket.
*/

/*!
    Destroys the socket, closing any connection.
*/
QTcpSocket::~QTcpSocket() = default;

/*!
    \class QUdpSocket
    \brief The QUdpSocket class provides a UDP socket.

    UDP is a message-oriented, unreliable protocol. Bind the socket with
    bind() to receive, then read datagrams when readyRead() is emitted:

    \code
    QUdpSocket socket;
    socket.bind(QHostAddress::Any, 4000);
    QObject::connect(&socket, &QUdpSocket::readyRead, [&] {
        while (socket.hasPendingDatagrams()) {
            QNetworkDatagram d = socket.receiveDatagram();
            socket.writeDatagram(d.makeReply("ack"));
        }
    });
    \endcode

    A datagram can be sent without binding; the socket binds to an ephemeral
    port on first use. After connectToHost() the plain write() functions send
    to the connected peer.

    \differences
    The multicast functions take no interface argument; groups are joined on
    the default interface. There is no \c multicastInterface(),
    \c setMulticastInterface() or QNetworkInterface support. \c pauseMode and
    proxy support are not provided.
    \enddifferences

    \sa QAbstractSocket, QTcpSocket, QNetworkDatagram
*/

/*!
    \fn QUdpSocket::QUdpSocket(QObject *parent)

    Creates a UDP socket with the given \a parent.
*/

/*!
    Destroys the socket, closing it.
*/
QUdpSocket::~QUdpSocket() = default;

/*!
    Returns the size of the first pending datagram, or -1 if there is none.
    A zero-length datagram yields 0.

    \sa hasPendingDatagrams(), readDatagram()
*/
qint64 QUdpSocket::pendingDatagramSize() const {
    if (socketDescriptor() < 0)
        return -1;
    char c;
    const ssize_t n = ::recv(static_cast<int>(socketDescriptor()), &c, 1, MSG_PEEK | MSG_TRUNC | MSG_DONTWAIT);
    return n < 0 ? -1 : n;
}

/*!
    Returns \c true if at least one datagram is waiting to be read;
    otherwise returns \c false.

    \sa pendingDatagramSize(), readDatagram()
*/
bool QUdpSocket::hasPendingDatagrams() const { return pendingDatagramSize() >= 0; }

/*!
    Receives a datagram of no more than \a maxSize bytes into \a data and
    returns its size, or -1 on failure. Bytes beyond \a maxSize are
    discarded. If \a address and \a port are not null, they are set to the
    sender's address and port.

    \sa receiveDatagram(), pendingDatagramSize()
*/
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

/*!
    Receives the next pending datagram and returns it with its payload,
    sender and destination (the socket's local address and port) filled in.
    At most \a maxSize bytes are kept; a negative value keeps the whole
    datagram. Returns an invalid datagram if none is pending or an error
    occurs.

    \sa readDatagram(), hasPendingDatagrams()
*/
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

/*!
    Sends the datagram at \a data of \a size bytes to \a address on \a port.
    Returns the number of bytes sent, or -1 on failure. If the socket is not
    yet bound, it is bound to an ephemeral port first.

    \sa readDatagram()
*/
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

/*!
    \overload

    Sends \a datagram to its destination address and port. Returns the number
    of bytes sent, or -1 on failure.
*/
qint64 QUdpSocket::writeDatagram(const QNetworkDatagram &datagram) {
    const QByteArray d = datagram.data();
    return writeDatagram(d.constData(), d.size(), datagram.destinationAddress(),
                         static_cast<quint16>(datagram.destinationPort()));
}

/*!
    \reimp

    Reads one datagram into \a data, up to \a maxSize bytes. Returns the
    number of bytes read; returns 0 if no datagram is available.
*/
qint64 QUdpSocket::readData(char *data, qint64 maxSize) {
    const qint64 n = readDatagram(data, maxSize);
    return n < 0 ? 0 : n;
}

/*!
    \reimp

    Sends \a maxSize bytes from \a data to the connected peer. Returns the
    number of bytes sent, or -1 with OperationError if the socket is not
    connected.
*/
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

/*!
    Joins the multicast group \a groupAddress on the default interface.
    Returns \c true on success; returns \c false if the socket is not bound,
    \a groupAddress is not a multicast address, or the system refuses.

    \sa leaveMulticastGroup()
*/
bool QUdpSocket::joinMulticastGroup(const QHostAddress &groupAddress) { return membership(this, groupAddress, true); }

/*!
    Leaves the multicast group \a groupAddress. Returns \c true on success;
    otherwise returns \c false.

    \sa joinMulticastGroup()
*/
bool QUdpSocket::leaveMulticastGroup(const QHostAddress &groupAddress) { return membership(this, groupAddress, false); }
