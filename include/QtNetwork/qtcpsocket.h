// MIT License - see LICENSE
#pragma once

#include <QtNetwork/qabstractsocket.h>

class QTcpSocket : public QAbstractSocket {
    Q_OBJECT
public:
    explicit QTcpSocket(QObject *parent = nullptr) : QAbstractSocket(TcpSocket, parent) {}
    ~QTcpSocket() override;

protected:
    QTcpSocket(SocketType type, QObject *parent) : QAbstractSocket(type, parent) {}
};
