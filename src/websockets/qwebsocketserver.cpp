// MIT License - see LICENSE
#include "qwebsocket_p.h"
#include "qwebsockethandshake_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtWebSockets/qwebsocket.h>
#include <QtWebSockets/qwebsocketcorsauthenticator.h>
#include <QtWebSockets/qwebsocketserver.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslserver.h>
#include <QtNetwork/qsslsocket.h>
#endif

#include <memory>

/*!
    \class QWebSocketServer
    \brief The QWebSocketServer class accepts WebSocket connections.
    \since 5.3

    A QWebSocketServer listens on a TCP port, answers the opening handshake
    of each client, and hands out a connected QWebSocket for each one through
    newConnection() and nextPendingConnection():

    \snippet websockets/echo.cpp server

    In SecureMode, the server speaks TLS (\c wss:) with the configuration set
    by setSslConfiguration(), which must hold a certificate and a private key.

    The server checks every opening handshake before it accepts a client: the
    request must be a \c GET for HTTP/1.1 with a \c Host header, the
    \c Upgrade and \c Connection fields, a valid \c Sec-WebSocket-Key and
    version 13. A client asking for another version gets
    \c{426 Upgrade Required}; other bad requests get \c{400 Bad Request}.
    A client that does not complete its handshake within
    handshakeTimeoutMS() is disconnected. Connect to
    originAuthenticationRequired() to accept only some origins, which
    browsers send in the \c Origin header.

    Accepted sockets are children of the server. Delete them when done, or
    they are deleted with the server.

    \differences
    There is no proxy support, no pre-shared keys and no \c alertSent() or
    \c handshakeInterruptedOnError() signals. \c peerVerifyError() is never
    emitted. The handshake request may not exceed 16 KiB.
    \enddifferences

    \sa QWebSocket, {Using Nt WebSockets}
*/

/*!
    \enum QWebSocketServer::SslMode

    Whether the server encrypts its connections.

    \value SecureMode The server speaks TLS: its URL has the scheme \c wss.
    \value NonSecureMode The server does not encrypt: its URL has the scheme \c ws.
*/

class QWebSocketServerPrivate {
public:
    QWebSocketServer *q;
    QString serverName;
    QWebSocketServer::SslMode mode;
    QTcpServer *tcp = nullptr;
    int maxPending = 30;
    int handshakeTimeout = 10000;
    QStringList subprotocols;
    QList<QPointer<QWebSocket>> pending;
    QWebSocketProtocol::CloseCode error = QWebSocketProtocol::CloseCodeNormal;
    QString errorString;
#ifndef QT_NO_SSL
    QSslConfiguration sslConfiguration;
    QSslServer *sslServer = nullptr; // tcp, in SecureMode
#endif

    void accept();
    void handle(QTcpSocket *socket);
    void setError(QWebSocketProtocol::CloseCode code, const QString &message) {
        error = code;
        errorString = message;
        q->serverError(code);
    }
};

void QWebSocketServerPrivate::accept() {
    while (tcp->hasPendingConnections()) {
        QTcpSocket *s = tcp->nextPendingConnection();
        if (!s)
            break;
        handle(s);
    }
}

void QWebSocketServerPrivate::handle(QTcpSocket *s) {
    struct State {
        QByteArray buffer;
        bool done = false;
    };
    auto state = std::make_shared<State>();
    s->setParent(q);
    auto *timer = new QTimer(s);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, q, [s, state] {
        state->done = true;
        s->abort();
        s->deleteLater();
    });
    timer->start(handshakeTimeout);
    QObject::connect(s, &QAbstractSocket::disconnected, q, [s, state] {
        state->done = true;
        s->deleteLater();
    });

    auto reject = [this, s, state](const QByteArray &response, QWebSocketProtocol::CloseCode code,
                                   const QString &message) {
        state->done = true;
        s->write(response);
        s->disconnectFromHost();
        setError(code, message);
    };

    auto process = [this, s, state, timer, reject] {
        if (state->done)
            return;
        state->buffer += s->readAll();
        // A request line is printable ASCII. Anything else, such as a TLS handshake sent to a plain
        // server, is refused at once rather than after the handshake timeout.
        const qsizetype firstLine = state->buffer.indexOf('\n');
        const qsizetype checked = firstLine < 0 ? state->buffer.size() : firstLine;
        for (qsizetype i = 0; i < checked; ++i) {
            const auto c = static_cast<unsigned char>(state->buffer.at(i));
            if ((c < 0x20 && c != '\r') || c >= 0x7f) {
                reject(QWebSocketHandshake::errorResponse(400, "Bad Request"),
                       QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("The request is not HTTP"));
                return;
            }
        }
        const qsizetype end = QWebSocketHandshake::findHeadEnd(state->buffer, 0);
        if (end == 0)
            return;
        if (end < 0) {
            reject(QWebSocketHandshake::errorResponse(431, "Request Header Fields Too Large"),
                   QWebSocketProtocol::CloseCodeTooMuchData, QStringLiteral("The handshake request is too large"));
            return;
        }
        QWebSocketHandshake::Head head;
        if (!QWebSocketHandshake::parseHead(state->buffer.left(end), false, &head) ||
            !head.third.startsWith("HTTP/1.") || head.third == "HTTP/1.0" || !head.second.startsWith('/')) {
            reject(QWebSocketHandshake::errorResponse(400, "Bad Request"), QWebSocketProtocol::CloseCodeProtocolError,
                   QStringLiteral("Invalid handshake request"));
            return;
        }
        if (head.first != "GET") {
            reject(QWebSocketHandshake::errorResponse(405, "Method Not Allowed", "allow: GET\r\n"),
                   QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("The handshake request is not a GET"));
            return;
        }
        QWebSocketHandshake::ServerAnswer answer =
            QWebSocketHandshake::answer(head.headers, subprotocols, serverName.toUtf8());
        if (!answer.accepted) {
            reject(answer.response, answer.closeCode, answer.errorString);
            return;
        }
        const QString origin =
            QString::fromUtf8(head.headers.value(QHttpHeaders::WellKnownHeader::Origin).toByteArray());
        QWebSocketCorsAuthenticator authenticator(origin);
        QPointer<QWebSocketServer> guard(q);
        q->originAuthenticationRequired(&authenticator);
        if (!guard)
            return;
        if (!authenticator.allowed()) {
            reject(QWebSocketHandshake::errorResponse(403, "Forbidden"), QWebSocketProtocol::CloseCodePolicyViolated,
                   QStringLiteral("The origin %1 is not allowed").arg(origin));
            return;
        }
        pending.removeAll(QPointer<QWebSocket>());
        if (pending.size() >= maxPending) {
            reject(QWebSocketHandshake::errorResponse(503, "Service Unavailable"),
                   QWebSocketProtocol::CloseCodeAbnormalDisconnection, QStringLiteral("Too many pending connections"));
            return;
        }

        state->done = true;
        delete timer;
        s->disconnect(q);
        s->write(answer.response);

        const bool secure = mode == QWebSocketServer::SecureMode;
        const QByteArray host = head.headers.value(QHttpHeaders::WellKnownHeader::Host).toByteArray();
        QNetworkRequest request(QUrl::fromEncoded((secure ? "wss://" : "ws://") + host + head.second));
        for (const auto &field : head.headers.toListOfPairs())
            request.setRawHeader(field.first, field.second);
        QWebSocket *ws =
            QWebSocketPrivate::createServerSide(s, request, origin, answer.subprotocol, state->buffer.mid(end), q);
        pending.append(ws);
        q->newConnection();
    };
    QObject::connect(s, &QIODevice::readyRead, q, process);
    process();
}

/*!
    Constructs a WebSocket server with the given \a parent. \a serverName is
    sent in the \c Server header of the opening handshake, and \a secureMode
    says whether the server speaks TLS. The server does not listen until
    listen() is called.
*/
QWebSocketServer::QWebSocketServer(const QString &serverName, SslMode secureMode, QObject *parent)
    : QObject(parent), d(std::make_unique<QWebSocketServerPrivate>()) {
    d->q = this;
    d->serverName = serverName;
    d->mode = secureMode;
#ifndef QT_NO_SSL
    if (secureMode == SecureMode) {
        d->sslServer = new QSslServer(this);
        QObject::connect(d->sslServer, &QSslServer::sslErrors, this,
                         [this](QSslSocket *, const QList<QSslError> &errors) { sslErrors(errors); });
        d->tcp = d->sslServer;
    }
#else
    if (secureMode == SecureMode)
        qWarning("QWebSocketServer: SecureMode is not available: Nt was built without TLS");
#endif
    if (!d->tcp)
        d->tcp = new QTcpServer(this);
    d->tcp->setMaxPendingConnections(d->maxPending);
    QObject::connect(d->tcp, &QTcpServer::newConnection, this, [this] { d->accept(); });
    QObject::connect(d->tcp, &QTcpServer::acceptError, this,
                     [this](QAbstractSocket::SocketError e) { acceptError(e); });
}

/*!
    Destroys the server, closing it if it is listening. Sockets it accepted
    are deleted too.
*/
QWebSocketServer::~QWebSocketServer() = default;

/*!
    Starts listening on \a address and \a port, or a free port if \a port is
    0. Returns \c true on success; otherwise sets errorString() and returns
    \c false.

    \sa close(), serverUrl()
*/
bool QWebSocketServer::listen(const QHostAddress &address, quint16 port) {
#ifdef QT_NO_SSL
    if (d->mode == SecureMode) {
        d->errorString = QStringLiteral("SecureMode is not available: Nt was built without TLS");
        return false;
    }
#else
    if (d->sslServer)
        d->sslServer->setSslConfiguration(d->sslConfiguration);
#endif
    if (!d->tcp->listen(address, port)) {
        d->errorString = d->tcp->errorString();
        return false;
    }
    return true;
}

/*!
    Stops listening and emits closed(). Accepted sockets stay open.
*/
void QWebSocketServer::close() {
    const bool was = d->tcp->isListening();
    d->tcp->close();
    if (was)
        closed();
}

/*!
    Returns \c true if the server is listening; otherwise returns \c false.
*/
bool QWebSocketServer::isListening() const { return d->tcp->isListening(); }

/*!
    Sets the largest number of accepted sockets that wait for
    nextPendingConnection() to \a numConnections. Clients beyond it are
    refused with \c{503 Service Unavailable}. The default is 30.
*/
void QWebSocketServer::setMaxPendingConnections(int numConnections) {
    d->maxPending = numConnections;
    d->tcp->setMaxPendingConnections(numConnections);
}

/*!
    Returns the largest number of accepted sockets waiting for nextPendingConnection().
*/
int QWebSocketServer::maxPendingConnections() const { return d->maxPending; }

/*!
    Sets the time a client has to complete its opening handshake to \a msec
    milliseconds. The default is 10 seconds.
*/
void QWebSocketServer::setHandshakeTimeout(int msec) { d->handshakeTimeout = msec; }

/*!
    Returns the time a client has to complete its opening handshake, in milliseconds.
*/
int QWebSocketServer::handshakeTimeoutMS() const { return d->handshakeTimeout; }

/*!
    Returns the port the server listens on, or 0 if it is not listening.
*/
quint16 QWebSocketServer::serverPort() const { return d->tcp->serverPort(); }

/*!
    Returns the address the server listens on.
*/
QHostAddress QWebSocketServer::serverAddress() const { return d->tcp->serverAddress(); }

/*!
    Returns the URL clients connect to, such as \c{ws://127.0.0.1:1234}, or
    an empty URL if the server is not listening.
*/
QUrl QWebSocketServer::serverUrl() const {
    if (!isListening())
        return QUrl();
    QUrl url;
    url.setScheme(d->mode == SecureMode ? QStringLiteral("wss") : QStringLiteral("ws"));
    url.setHost(serverAddress().toString());
    url.setPort(serverPort());
    return url;
}

/*!
    Returns whether the server speaks TLS.
*/
QWebSocketServer::SslMode QWebSocketServer::secureMode() const { return d->mode; }

/*!
    Makes the server listen on the already listening native socket
    \a socketDescriptor. Returns \c true on success.
*/
bool QWebSocketServer::setSocketDescriptor(qintptr socketDescriptor) {
    return d->tcp->setSocketDescriptor(socketDescriptor);
}

/*!
    Returns the native socket the server listens on, or -1.
*/
qintptr QWebSocketServer::socketDescriptor() const { return d->tcp->socketDescriptor(); }

/*!
    Returns \c true if accepted sockets are waiting for
    nextPendingConnection(); otherwise returns \c false.
*/
bool QWebSocketServer::hasPendingConnections() const {
    for (const QPointer<QWebSocket> &ws : d->pending) {
        if (ws)
            return true;
    }
    return false;
}

/*!
    Returns the next accepted socket, or \c nullptr if there is none. The
    socket is connected and a child of the server.

    \sa newConnection()
*/
QWebSocket *QWebSocketServer::nextPendingConnection() {
    while (!d->pending.isEmpty()) {
        QPointer<QWebSocket> ws = d->pending.takeFirst();
        if (ws)
            return ws.data();
    }
    return nullptr;
}

/*!
    Returns the code of the last handshake error.

    \sa errorString(), serverError()
*/
QWebSocketProtocol::CloseCode QWebSocketServer::error() const { return d->error; }

/*!
    Returns a description of the last error.
*/
QString QWebSocketServer::errorString() const { return d->errorString; }

/*!
    Stops accepting new TCP connections until resumeAccepting() is called.
*/
void QWebSocketServer::pauseAccepting() { d->tcp->pauseAccepting(); }

/*!
    Accepts new TCP connections again after pauseAccepting().
*/
void QWebSocketServer::resumeAccepting() { d->tcp->resumeAccepting(); }

/*!
    Sets the name sent in the \c Server header of the opening handshake to \a serverName.
*/
void QWebSocketServer::setServerName(const QString &serverName) { d->serverName = serverName; }

/*!
    Returns the name sent in the \c Server header of the opening handshake.
*/
QString QWebSocketServer::serverName() const { return d->serverName; }

/*!
    Sets the subprotocols the server supports to \a protocols. Of the ones a
    client asks for, the first that the server supports is chosen, and
    QWebSocket::subprotocol() returns it.
*/
void QWebSocketServer::setSupportedSubprotocols(const QStringList &protocols) { d->subprotocols = protocols; }

/*!
    Returns the subprotocols the server supports.
*/
QStringList QWebSocketServer::supportedSubprotocols() const { return d->subprotocols; }

#ifndef QT_NO_SSL
/*!
    Sets the TLS configuration of a server in SecureMode to
    \a sslConfiguration. It takes effect when listen() is called.
*/
void QWebSocketServer::setSslConfiguration(const QSslConfiguration &sslConfiguration) {
    d->sslConfiguration = sslConfiguration;
    if (d->sslServer)
        d->sslServer->setSslConfiguration(sslConfiguration);
}

/*!
    Returns the TLS configuration of the server.
*/
QSslConfiguration QWebSocketServer::sslConfiguration() const { return d->sslConfiguration; }
#endif

/*!
    Returns the versions of the protocol the server speaks: only version 13.
*/
QList<QWebSocketProtocol::Version> QWebSocketServer::supportedVersions() const {
    return {QWebSocketProtocol::Version13};
}

/*!
    Performs the opening handshake on \a socket, a connected TCP socket that
    was accepted elsewhere, and emits newConnection() when it succeeds. The
    server takes ownership of the socket.
*/
void QWebSocketServer::handleConnection(QTcpSocket *socket) const {
    if (socket)
        d->handle(socket);
}

#include "moc_qwebsocketserver.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
