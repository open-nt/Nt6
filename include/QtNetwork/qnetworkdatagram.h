// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtNetwork/qhostaddress.h>

class QNetworkDatagram {
public:
    QNetworkDatagram() = default;
    QNetworkDatagram(const QByteArray &data, const QHostAddress &destinationAddress = QHostAddress(),
                     quint16 port = 0)
        : m_data(data), m_dest(destinationAddress), m_destPort(port) {}

    bool isValid() const { return !m_data.isNull() || !m_sender.isNull() || !m_dest.isNull(); }
    bool isNull() const { return !isValid(); }

    QHostAddress senderAddress() const { return m_sender; }
    QHostAddress destinationAddress() const { return m_dest; }
    int senderPort() const { return m_senderPort; }
    int destinationPort() const { return m_destPort; }
    void setSender(const QHostAddress &address, quint16 port = 0) { m_sender = address; m_senderPort = port; }
    void setDestination(const QHostAddress &address, quint16 port) { m_dest = address; m_destPort = port; }
    int hopLimit() const { return m_hopLimit; }
    void setHopLimit(int count) { m_hopLimit = count; }
    uint interfaceIndex() const { return m_ifindex; }
    void setInterfaceIndex(uint index) { m_ifindex = index; }
    QByteArray data() const { return m_data; }
    void setData(const QByteArray &data) { m_data = data; }
    void clear() { *this = QNetworkDatagram(); }

    // Datagram addressed back to this one's sender.
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
