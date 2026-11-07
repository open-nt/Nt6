// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtNetwork/qhostaddress.h>

/*!
    \class QNetworkDatagram
    \brief The QNetworkDatagram class represents a UDP datagram.

    A QNetworkDatagram holds the payload of a datagram together with its
    sender and destination addresses and ports. QUdpSocket::receiveDatagram()
    returns one, and QUdpSocket::writeDatagram() sends one:

    \code
    QNetworkDatagram d = socket.receiveDatagram();
    socket.writeDatagram(d.makeReply("ack"));
    \endcode

    \differences
    The class is not implicitly shared, so copying a datagram copies its
    addresses (the QByteArray payload still shares its data). There is no
    \c swap() and isNull() is simply the negation of isValid().
    \enddifferences

    \sa QUdpSocket, QHostAddress
*/
class QNetworkDatagram {
public:
    /*!
        Constructs an empty, invalid datagram.
    */
    QNetworkDatagram() = default;
    /*!
        Constructs a datagram containing \a data, to be sent to
        \a destinationAddress on \a port.
    */
    QNetworkDatagram(const QByteArray &data, const QHostAddress &destinationAddress = QHostAddress(),
                     quint16 port = 0)
        : m_data(data), m_dest(destinationAddress), m_destPort(port) {}

    /*!
        Returns \c true if the datagram has data or a sender or destination
        address; otherwise returns \c false.

        \sa isNull()
    */
    bool isValid() const { return !m_data.isNull() || !m_sender.isNull() || !m_dest.isNull(); }
    /*!
        Returns \c true if the datagram is not valid.

        \sa isValid()
    */
    bool isNull() const { return !isValid(); }

    /*!
        Returns the address of the sender of the datagram.

        \sa setSender(), senderPort()
    */
    QHostAddress senderAddress() const { return m_sender; }
    /*!
        Returns the address the datagram was or will be sent to.

        \sa setDestination(), destinationPort()
    */
    QHostAddress destinationAddress() const { return m_dest; }
    /*!
        Returns the port of the sender, or -1 if it is not set.

        \sa setSender(), senderAddress()
    */
    int senderPort() const { return m_senderPort; }
    /*!
        Returns the destination port, or -1 if it is not set.

        \sa setDestination(), destinationAddress()
    */
    int destinationPort() const { return m_destPort; }
    /*!
        Sets the sender of the datagram to \a address and \a port.

        \sa senderAddress(), senderPort()
    */
    void setSender(const QHostAddress &address, quint16 port = 0) { m_sender = address; m_senderPort = port; }
    /*!
        Sets the destination of the datagram to \a address and \a port.

        \sa destinationAddress(), destinationPort()
    */
    void setDestination(const QHostAddress &address, quint16 port) { m_dest = address; m_destPort = port; }
    /*!
        Returns the hop limit (TTL) of the datagram, or -1 if it is not set.

        \sa setHopLimit()
    */
    int hopLimit() const { return m_hopLimit; }
    /*!
        Sets the hop limit of the datagram to \a count.

        \sa hopLimit()
    */
    void setHopLimit(int count) { m_hopLimit = count; }
    /*!
        Returns the index of the network interface the datagram was received
        on or will be sent through, or 0 if it is not set.

        \sa setInterfaceIndex()
    */
    uint interfaceIndex() const { return m_ifindex; }
    /*!
        Sets the network interface index of the datagram to \a index.

        \sa interfaceIndex()
    */
    void setInterfaceIndex(uint index) { m_ifindex = index; }
    /*!
        Returns the payload of the datagram.

        \sa setData()
    */
    QByteArray data() const { return m_data; }
    /*!
        Sets the payload of the datagram to \a data.

        \sa data()
    */
    void setData(const QByteArray &data) { m_data = data; }
    /*!
        Resets the datagram to an empty, invalid state.
    */
    void clear() { *this = QNetworkDatagram(); }

    // Datagram addressed back to this one's sender.
    /*!
        Returns a datagram carrying \a payload that is addressed to this
        datagram's sender and appears to come from its destination.
    */
    QNetworkDatagram makeReply(const QByteArray &payload) const {
        QNetworkDatagram r(payload, m_sender, static_cast<quint16>(m_senderPort));
        r.setSender(m_dest, static_cast<quint16>(m_destPort));
        return r;
    }

private:
    QByteArray m_data;
    QHostAddress m_sender, m_dest;
    int m_senderPort = -1;
    int m_destPort = -1;
    int m_hopLimit = -1;
    uint m_ifindex = 0;
};
