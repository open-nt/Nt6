// MIT License - see LICENSE
#pragma once

#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslsocket.h>
#include <QtNetwork/qtcpserver.h>

class QSslServer : public QTcpServer {
    Q_OBJECT
public:
    explicit QSslServer(QObject *parent = nullptr);
    ~QSslServer() override;

    void setSslConfiguration(const QSslConfiguration &sslConfiguration);
    QSslConfiguration sslConfiguration() const;
    void setHandshakeTimeout(int timeout);
    int handshakeTimeout() const;

signals:
    /*!
        This signal is emitted when \a socket reports verification \a errors.
    */
    void sslErrors(QSslSocket *socket, const QList<QSslError> &errors) {
        emitSignal(&QSslServer::sslErrors, socket, errors);
    }
    /*!
        This signal is emitted when \a socket fails with \a error before its
        handshake finishes. The socket is then deleted.
    */
    void errorOccurred(QSslSocket *socket, QAbstractSocket::SocketError error) {
        emitSignal(&QSslServer::errorOccurred, socket, error);
    }
    /*!
        This signal is emitted when a handshake begins on \a socket.
    */
    void startedEncryptionHandshake(QSslSocket *socket) { emitSignal(&QSslServer::startedEncryptionHandshake, socket); }

protected:
    void incomingConnection(qintptr socket) override;

private:
    QSslConfiguration m_configuration;
    int m_timeout = 5000;
};
