// MIT License - see LICENSE
#include "qhttpserverstream_p.h"

#include <QtCore/qdebug.h>
#include <QtHttpServer/qabstracthttpserver.h>
#include <QtHttpServer/qhttpserverrequest.h>
#include <QtHttpServer/qhttpserverresponder.h>
#ifndef QT_NO_LOCALSERVER
#include <QtNetwork/qlocalserver.h>
#include <QtNetwork/qlocalsocket.h>
#endif
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtWebSockets/qwebsocket.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslsocket.h>
#endif

#include <vector>

/*!
    \class QAbstractHttpServer
    \brief The QAbstractHttpServer class is the base of HTTP servers: it accepts connections and parses requests.
    \since 6.4

    A QAbstractHttpServer serves the connections of the QTcpServer and
    QLocalServer objects bound to it with bind(). It reads HTTP/1.0 and
    HTTP/1.1 requests, with persistent connections and pipelining, and calls
    handleRequest() for each complete request. Subclasses decide how to
    answer; QHttpServer answers with routes.

    \code
    class Hello : public QAbstractHttpServer {
    protected:
        bool handleRequest(const QHttpServerRequest &request, QHttpServerResponder &responder) override {
            if (request.url().path() != "/")
                return false;
            responder.write("Hello\n", "text/plain");
            return true;
        }
        void missingHandler(const QHttpServerRequest &, QHttpServerResponder &responder) override {
            responder.write(QHttpServerResponder::StatusCode::NotFound);
        }
    };
    \endcode

    Bind a QSslServer to serve HTTPS: its connections reach the server once
    the TLS handshake is done, and the URLs of their requests have the
    scheme \c https.

    The server refuses requests it cannot parse with \c{400 Bad Request} and
    closes the connection. It also refuses a request line over 8 KiB
    (\c{414}), header fields over 64 KiB or more than 100 of them
    (\c{431}), and a body over 64 MiB (\c{413}). A request with both
    \c Transfer-Encoding and \c Content-Length is refused, since a proxy in
    front of the server could frame it differently.

    Requests with an \c{Upgrade: websocket} header can become WebSocket
    connections on the same port; see addWebSocketUpgradeVerifier().

    \differences
    There is no \c QHttpServerConfiguration (rate limits, keep-alive timeout,
    address filters) and no HTTP/2. WebSocket upgrades are accepted on TCP
    connections only, not on a QLocalServer, and negotiate no subprotocol.
    On Zephyr there are no local sockets, so \c bind(QLocalServer *) and
    \c localServers() are absent, and \c QT_NO_LOCALSERVER is defined; see
    \l{Nt on Zephyr}.
    The size limits above are fixed. All objects must live in the thread of
    the server.
    \enddifferences

    \sa QHttpServer, {Using Nt HTTP Server}
*/

struct QAbstractHttpServer::WebSocketData {
    struct Verifier {
        QPointer<QObject> context;
        UpgradeVerifier verifier;
    };
    std::vector<Verifier> verifiers;
    std::vector<std::unique_ptr<QWebSocket>> pending;
};

/*!
    Constructs an HTTP server with the given \a parent. It serves nothing
    until a server is bound to it with bind().
*/
QAbstractHttpServer::QAbstractHttpServer(QObject *parent)
    : QObject(parent), m_webSockets(std::make_unique<WebSocketData>()) {}

/*!
    Destroys the HTTP server and closes its connections. Bound servers that
    are its children are destroyed too.
*/
QAbstractHttpServer::~QAbstractHttpServer() = default;

/*!
    Returns the ports of the bound TCP servers that are listening.

    \sa bind(), servers()
*/
QList<quint16> QAbstractHttpServer::serverPorts() const {
    QList<quint16> ports;
    for (const QPointer<QTcpServer> &server : m_servers) {
        if (server && server->isListening())
            ports.append(server->serverPort());
    }
    return ports;
}

/*!
    Serves the connections that \a server accepts from now on, and returns
    \c true. The server must already be listening; if it is not, or if it
    is null, nothing happens and \c false is returned.

    A server without a parent becomes a child of this object. Several
    servers can be bound, for example one for HTTP and one for HTTPS.

    \snippet httpserver/hello_server.cpp bind

    \sa servers(), serverPorts()
*/
bool QAbstractHttpServer::bind(QTcpServer *server) {
    if (!server || !server->isListening()) {
        qWarning("QAbstractHttpServer::bind: the QTcpServer is not listening");
        return false;
    }
    for (const QPointer<QTcpServer> &s : m_servers) {
        if (s.data() == server)
            return true;
    }
    if (!server->parent())
        server->setParent(this);
    m_servers.append(server);
    QObject::connect(server, &QTcpServer::newConnection, this, [this, server] { acceptTcp(server); });
    acceptTcp(server);
    return true;
}

#ifndef QT_NO_LOCALSERVER
/*!
    \overload

    Serves the connections that the local socket server \a server accepts
    from now on, and returns \c true. The server must already be listening;
    if it is not, or if it is null, nothing happens and \c false is returned.
*/
bool QAbstractHttpServer::bind(QLocalServer *server) {
    if (!server || !server->isListening()) {
        qWarning("QAbstractHttpServer::bind: the QLocalServer is not listening");
        return false;
    }
    for (const QPointer<QLocalServer> &s : m_localServers) {
        if (s.data() == server)
            return true;
    }
    if (!server->parent())
        server->setParent(this);
    m_localServers.append(server);
    QObject::connect(server, &QLocalServer::newConnection, this, [this, server] { acceptLocal(server); });
    acceptLocal(server);
    return true;
}
#endif // QT_NO_LOCALSERVER

/*!
    Returns the TCP servers bound to this server that still exist.

    \sa bind(), localServers()
*/
QList<QTcpServer *> QAbstractHttpServer::servers() const {
    QList<QTcpServer *> out;
    for (const QPointer<QTcpServer> &s : m_servers) {
        if (s)
            out.append(s.data());
    }
    return out;
}

#ifndef QT_NO_LOCALSERVER
/*!
    Returns the local socket servers bound to this server that still exist.

    \sa bind(), servers()
*/
QList<QLocalServer *> QAbstractHttpServer::localServers() const {
    QList<QLocalServer *> out;
    for (const QPointer<QLocalServer> &s : m_localServers) {
        if (s)
            out.append(s.data());
    }
    return out;
}
#endif // QT_NO_LOCALSERVER

void QAbstractHttpServer::acceptTcp(QTcpServer *server) {
    while (server->hasPendingConnections()) {
        QTcpSocket *socket = server->nextPendingConnection();
        if (!socket)
            break;
        bool encrypted = false;
#ifndef QT_NO_SSL
        if (auto *ssl = qobject_cast<QSslSocket *>(socket))
            encrypted = ssl->isEncrypted();
#endif
        new QHttpServerStream(this, socket, encrypted);
    }
}

#ifndef QT_NO_LOCALSERVER
void QAbstractHttpServer::acceptLocal(QLocalServer *server) {
    while (server->hasPendingConnections()) {
        QLocalSocket *socket = server->nextPendingConnection();
        if (!socket)
            break;
        new QHttpServerStream(this, socket, false);
    }
}
#endif // QT_NO_LOCALSERVER

/*!
    Returns \c true if accepted WebSocket connections are waiting for
    nextPendingWebSocketConnection(); otherwise returns \c false.
*/
bool QAbstractHttpServer::hasPendingWebSocketConnections() const { return !m_webSockets->pending.empty(); }

/*!
    Returns the next accepted WebSocket connection, or \c nullptr if there
    is none. The caller owns the socket, which has no parent.

    \sa newWebSocketConnection(), addWebSocketUpgradeVerifier()
*/
std::unique_ptr<QWebSocket> QAbstractHttpServer::nextPendingWebSocketConnection() {
    if (m_webSockets->pending.empty())
        return nullptr;
    std::unique_ptr<QWebSocket> socket = std::move(m_webSockets->pending.front());
    m_webSockets->pending.erase(m_webSockets->pending.begin());
    return socket;
}

void QAbstractHttpServer::addWebSocketUpgradeVerifierImpl(const QObject *context, UpgradeVerifier verifier) {
    if (!context) {
        qWarning("QAbstractHttpServer::addWebSocketUpgradeVerifier: a context object is required");
        return;
    }
    m_webSockets->verifiers.push_back({const_cast<QObject *>(context), std::move(verifier)});
}

QHttpServerWebSocketUpgradeResponse QAbstractHttpServer::verifyWebSocketUpgrade(const QHttpServerRequest &request) {
    // Copied, since a verifier may add more verifiers.
    const std::vector<WebSocketData::Verifier> verifiers = m_webSockets->verifiers;
    for (const WebSocketData::Verifier &v : verifiers) {
        if (!v.context)
            continue;
        QHttpServerWebSocketUpgradeResponse verdict = v.verifier(request);
        if (verdict.type() != QHttpServerWebSocketUpgradeResponse::ResponseType::PassToNext)
            return verdict;
    }
    return QHttpServerWebSocketUpgradeResponse::deny();
}

void QAbstractHttpServer::addWebSocket(QWebSocket *socket) {
    m_webSockets->pending.emplace_back(socket);
    newWebSocketConnection();
}

void QAbstractHttpServer::dispatch(const QHttpServerRequest &request, QHttpServerResponder &responder) {
    if (!handleRequest(request, responder))
        missingHandler(request, responder);
}

#include "moc_qabstracthttpserver.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
