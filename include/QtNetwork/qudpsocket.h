// MIT License - see LICENSE
#pragma once

#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qnetworkdatagram.h>

class QUdpSocket : public QAbstractSocket {
    Q_OBJECT
public:
    explicit QUdpSocket(QObject *parent = nullptr) : QAbstractSocket(UdpSocket, parent) {}
    ~QUdpSocket() override;

    bool hasPendingDatagrams() const;
    qint64 pendingDatagramSize() const;
    qint64 readDatagram(char *data, qint64 maxSize, QHostAddress *address = nullptr, quint16 *port = nullptr);
    QNetworkDatagram receiveDatagram(qint64 maxSize = -1);
    qint64 writeDatagram(const char *data, qint64 size, const QHostAddress &address, quint16 port);
    qint64 writeDatagram(const QByteArray &datagram, const QHostAddress &host, quint16 port) {
        return writeDatagram(datagram.constData(), datagram.size(), host, port);
    }
    qint64 writeDatagram(const QNetworkDatagram &datagram);

    bool joinMulticastGroup(const QHostAddress &groupAddress);
    bool leaveMulticastGroup(const QHostAddress &groupAddress);

    qint64 bytesAvailable() const override { return pendingDatagramSize() > 0 ? pendingDatagramSize() : 0; }
    bool canReadLine() const override { return false; }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
};
