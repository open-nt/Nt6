// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtHttpServer/qhttpserverwebsocketupgraderesponse.h>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

class QHttpServerRequest;
class QHttpServerResponder;
class QHttpServerStream;
class QLocalServer;
class QTcpServer;
class QWebSocket;

class QAbstractHttpServer : public QObject {
    Q_OBJECT
public:
    explicit QAbstractHttpServer(QObject *parent = nullptr);
    ~QAbstractHttpServer() override;

    QList<quint16> serverPorts() const;
    bool bind(QTcpServer *server);
    bool bind(QLocalServer *server);
    QList<QTcpServer *> servers() const;
    QList<QLocalServer *> localServers() const;

    bool hasPendingWebSocketConnections() const;
    std::unique_ptr<QWebSocket> nextPendingWebSocketConnection();

    /*!
        \fn template <typename Handler> void QAbstractHttpServer::addWebSocketUpgradeVerifier(const QObject *context, Handler &&func)

        Adds \a func, which decides whether a request to upgrade to WebSocket
        is accepted. It takes \c{const QHttpServerRequest &} and returns a
        QHttpServerWebSocketUpgradeResponse. If \a func is a member function,
        it is called on \a context; the verifier is dropped when \a context
        is destroyed.

        Each request with an \c{Upgrade: websocket} header goes to the
        verifiers in the order they were added, until one accepts or denies
        it. A request that no verifier accepts is refused with
        \c{403 Forbidden}, so a server without verifiers accepts no
        WebSockets. An accepted request becomes a QWebSocket, and
        newWebSocketConnection() is emitted.

        \sa nextPendingWebSocketConnection()
    */
    template <typename Handler> void addWebSocketUpgradeVerifier(const QObject *context, Handler &&func) {
        using F = std::decay_t<Handler>;
        addWebSocketUpgradeVerifierImpl(
            context, [context, f = F(std::forward<Handler>(func))](const QHttpServerRequest &request) mutable {
                if constexpr (std::is_member_function_pointer_v<F>)
                    return invokeOnContext(context, f, request);
                else
                    return QHttpServerWebSocketUpgradeResponse(f(request));
            });
    }

signals:
    /*!
        This signal is emitted when a request to upgrade to WebSocket was
        accepted. Take the socket with nextPendingWebSocketConnection().

        \sa addWebSocketUpgradeVerifier()
    */
    void newWebSocketConnection() { emitSignal(&QAbstractHttpServer::newWebSocketConnection); }

protected:
    /*!
        Handles \a request and answers it through \a responder. Returns
        \c true if the request was handled, or \c false to have
        missingHandler() called for it.

        The responder may be moved out to answer the request later; the
        connection waits for that answer before it reads the next request.

        \sa missingHandler()
    */
    virtual bool handleRequest(const QHttpServerRequest &request, QHttpServerResponder &responder) = 0;
    /*!
        Answers \a request, which handleRequest() did not handle, through
        \a responder. A typical implementation sends \c{404 Not Found}.

        \sa handleRequest()
    */
    virtual void missingHandler(const QHttpServerRequest &request, QHttpServerResponder &responder) = 0;

private:
    friend class QHttpServerStream;
    void acceptTcp(QTcpServer *server);
    void acceptLocal(QLocalServer *server);
    void dispatch(const QHttpServerRequest &request, QHttpServerResponder &responder);

    using UpgradeVerifier = std::function<QHttpServerWebSocketUpgradeResponse(const QHttpServerRequest &)>;
    void addWebSocketUpgradeVerifierImpl(const QObject *context, UpgradeVerifier verifier);
    QHttpServerWebSocketUpgradeResponse verifyWebSocketUpgrade(const QHttpServerRequest &request);
    void addWebSocket(QWebSocket *socket);

    template <typename F>
    static QHttpServerWebSocketUpgradeResponse invokeOnContext(const QObject *context, F f,
                                                               const QHttpServerRequest &request) {
        using C = typename MemberClass<F>::Type;
        static_assert(std::is_base_of_v<QObject, C>, "A member function verifier must belong to a QObject subclass");
        return (static_cast<C *>(const_cast<QObject *>(context))->*f)(request);
    }
    template <typename F> struct MemberClass;
    template <typename C, typename R, typename A> struct MemberClass<R (C::*)(A)> {
        using Type = C;
    };
    template <typename C, typename R, typename A> struct MemberClass<R (C::*)(A) const> {
        using Type = C;
    };

    struct WebSocketData;
    QList<QPointer<QTcpServer>> m_servers;
    QList<QPointer<QLocalServer>> m_localServers;
    std::unique_ptr<WebSocketData> m_webSockets;
};
