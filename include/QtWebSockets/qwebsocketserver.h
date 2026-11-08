// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qhostaddress.h>
#include <QtWebSockets/qwebsocketprotocol.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslerror.h>
#endif

#include <memory>

class QTcpSocket;
class QWebSocket;
class QWebSocketCorsAuthenticator;
class QWebSocketServerPrivate;

class QWebSocketServer : public QObject {
    Q_OBJECT
public:
    enum SslMode { SecureMode = 0, NonSecureMode = 1 };
    Q_ENUM(SslMode)

    explicit QWebSocketServer(const QString &serverName, SslMode secureMode, QObject *parent = nullptr);
    ~QWebSocketServer() override;

    bool listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);
    void close();
    bool isListening() const;

    void setMaxPendingConnections(int numConnections);
    int maxPendingConnections() const;
    void setHandshakeTimeout(int msec);
    int handshakeTimeoutMS() const;

    quint16 serverPort() const;
    QHostAddress serverAddress() const;
    QUrl serverUrl() const;
    SslMode secureMode() const;

    bool setSocketDescriptor(qintptr socketDescriptor);
    qintptr socketDescriptor() const;

    bool hasPendingConnections() const;
    virtual QWebSocket *nextPendingConnection();

    QWebSocketProtocol::CloseCode error() const;
    QString errorString() const;

    void pauseAccepting();
    void resumeAccepting();

    void setServerName(const QString &serverName);
    QString serverName() const;
    void setSupportedSubprotocols(const QStringList &protocols);
    QStringList supportedSubprotocols() const;

#ifndef QT_NO_SSL
    void setSslConfiguration(const QSslConfiguration &sslConfiguration);
    QSslConfiguration sslConfiguration() const;
#endif

    QList<QWebSocketProtocol::Version> supportedVersions() const;
    void handleConnection(QTcpSocket *socket) const;

signals:
    /*!
        This signal is emitted when accepting a TCP connection failed with
        \a socketError.

        \sa error()
    */
    void acceptError(QAbstractSocket::SocketError socketError) {
        emitSignal(&QWebSocketServer::acceptError, socketError);
    }
    /*!
        This signal is emitted when the opening handshake of a client failed;
        \a closeCode says why and errorString() describes it.

        \sa error()
    */
    void serverError(QWebSocketProtocol::CloseCode closeCode) { emitSignal(&QWebSocketServer::serverError, closeCode); }
    /*!
        This signal is emitted during the opening handshake of each client,
        before it is accepted. A connected slot reads the \c Origin header
        from \a pAuthenticator, with \c origin(), and calls
        \c{setAllowed(false)} on it to refuse the client with
        \c{403 Forbidden}. Clients are allowed if no slot refuses them.

        The authenticator is only valid during the call.
    */
    void originAuthenticationRequired(QWebSocketCorsAuthenticator *pAuthenticator) {
        emitSignal(&QWebSocketServer::originAuthenticationRequired, pAuthenticator);
    }
    /*!
        This signal is emitted each time a client has completed the opening
        handshake. Call nextPendingConnection() to take it.

        \sa hasPendingConnections()
    */
    void newConnection() { emitSignal(&QWebSocketServer::newConnection); }
    /*!
        This signal is emitted when the server stops listening.

        \sa close()
    */
    void closed() { emitSignal(&QWebSocketServer::closed); }
#ifndef QT_NO_SSL
    /*!
        This signal is emitted when the TLS handshake with a client found
        \a errors.
    */
    void sslErrors(const QList<QSslError> &errors) { emitSignal(&QWebSocketServer::sslErrors, errors); }
    /*!
        This signal is emitted for each \a error found while verifying a
        client certificate.
    */
    void peerVerifyError(const QSslError &error) { emitSignal(&QWebSocketServer::peerVerifyError, error); }
#endif

private:
    std::unique_ptr<QWebSocketServerPrivate> d;
};
