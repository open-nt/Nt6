// MIT License - see LICENSE
#include <QtWebSockets/qwebsocketprotocol.h>

/*!
    \module NtWebSockets
    \title Nt WebSockets C++ Classes
    \brief Classes for WebSocket clients and servers.
    \cmaketarget Nt::WebSockets

    Nt WebSockets implements the WebSocket protocol (RFC 6455) on top of Nt
    Network: QWebSocket is a connection, as a client or accepted by a server,
    and QWebSocketServer accepts connections. Nt HTTP Server can also accept
    WebSocket upgrades on its own port.

    See \l{Using Nt WebSockets} for a walk through, and
    \c{examples/websockets/echo.cpp} for a complete program.
*/

/*!
    \page nt-websockets.html
    \title Using Nt WebSockets
    \brief Connecting to a WebSocket server, accepting connections, messages, closing, TLS, and WebSockets on the port of an HTTP server.

    Link \c Nt::WebSockets (or \c Nt6::WebSockets from an installed package)
    and include \c{<QtWebSockets>}:

    \code
    find_package(Nt6 6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt6::WebSockets)
    \endcode

    \section1 A Server

    A QWebSocketServer listens on a port and emits newConnection() for each
    client that has completed the opening handshake. Take the socket with
    nextPendingConnection() and connect to its signals:

    \snippet websockets/echo.cpp server

    The sockets are children of the server. Delete each one when it
    disconnects, as above, so that a long-running server does not collect
    them.

    \section1 A Client

    QWebSocket::open() connects to a \c ws: or \c wss: URL. Messages can be
    sent once connected() has been emitted:

    \snippet websockets/echo.cpp client

    errorOccurred() reports a connection that cannot be made, and an opening
    handshake that the server refuses; errorString() says why.

    \section1 Messages

    A message is text or binary. sendTextMessage() sends a QString as UTF-8,
    and sendBinaryMessage() sends bytes. The receiving side emits
    textMessageReceived() or binaryMessageReceived() with the whole message.
    Long messages travel in several frames (see
    QWebSocket::setOutgoingFrameSize()); textFrameReceived() and
    binaryFrameReceived() report them one by one, for programs that want to
    process a message before it has all arrived.

    Text must be valid UTF-8. A peer that sends anything else is
    disconnected with QWebSocketProtocol::CloseCodeWrongDatatype. Messages
    larger than QWebSocket::maxAllowedIncomingMessageSize(), 64 MiB by
    default, are refused with QWebSocketProtocol::CloseCodeTooMuchData.

    \section1 Subprotocols

    A client can ask for an application protocol by name, and the server
    picks the first one it supports:

    \snippet websockets/echo.cpp subprotocols

    \section1 Closing

    close() sends a close frame with a code and a reason; the peer answers
    with its own, and the connection closes. Both sides then emit
    disconnected(), and closeCode() and closeReason() tell what was sent.
    QWebSocketProtocol::CloseCodeAbnormalDisconnection means the connection
    broke without a closing handshake.

    \section1 Pings

    Both sides answer pings automatically. ping() sends one, and pong()
    reports the round-trip time. A program can ping at intervals to notice a
    connection that has silently gone away.

    \section1 TLS

    A client connects to a \c wss: URL with TLS, using the system's trusted
    certificates. A server in QWebSocketServer::SecureMode needs a
    certificate and a private key:

    \code
    QWebSocketServer server("Secure echo", QWebSocketServer::SecureMode);
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
    tls.setLocalCertificate(certificate);
    tls.setPrivateKey(key);
    server.setSslConfiguration(tls);
    server.listen(QHostAddress::Any, 443);
    \endcode

    \section1 WebSockets on an HTTP Server

    QAbstractHttpServer accepts WebSocket upgrades on the port it serves.
    Add a verifier that accepts the requests you want, and take the sockets
    when newWebSocketConnection() is emitted:

    \snippet httpserver/hello_server.cpp websocket

    \section1 Origins

    Browsers let any page open a WebSocket to any server, and send the
    page's origin in the \c Origin header. A server whose clients are its
    own pages should check it, through
    QWebSocketServer::originAuthenticationRequired() or, on an HTTP server,
    the request's headers in the upgrade verifier.
*/

/*!
    \namespace QWebSocketProtocol
    \brief The QWebSocketProtocol namespace holds the versions and close codes of the WebSocket protocol.
    \since 5.3
*/

/*!
    \enum QWebSocketProtocol::Version

    The versions of the WebSocket protocol. Nt speaks version 13 only.

    \value VersionUnknown An unknown version.
    \value Version0 Draft hixie-76 (hybi-00).
    \value Version4 Draft hybi-04.
    \value Version5 Draft hybi-05.
    \value Version6 Draft hybi-06.
    \value Version7 Draft hybi-07.
    \value Version8 Drafts hybi-08 to hybi-12.
    \value Version13 RFC 6455.
    \value VersionLatest The latest version, Version13.
*/

/*!
    \enum QWebSocketProtocol::CloseCode

    The codes of the closing handshake (RFC 6455, section 7.4.1). Codes from
    3000 to 4999 are for applications and libraries: they have no enumerator,
    but close() sends them and QWebSocket::closeCode() returns them, cast to
    CloseCode.

    \value CloseCodeNormal The purpose of the connection was fulfilled.
    \value CloseCodeGoingAway The endpoint is going away, for example a server shutting down.
    \value CloseCodeProtocolError The peer broke the protocol.
    \value CloseCodeDatatypeNotSupported The endpoint cannot accept the kind of data received.
    \value CloseCodeReserved1004 Reserved.
    \value CloseCodeMissingStatusCode The close frame had no code. Never sent.
    \value CloseCodeAbnormalDisconnection The connection closed without a closing handshake. Never sent.
    \value CloseCodeWrongDatatype A text message was not valid UTF-8.
    \value CloseCodePolicyViolated The message broke a policy of the endpoint.
    \value CloseCodeTooMuchData A message or frame was too large.
    \value CloseCodeMissingExtension The server did not negotiate an extension the client needs.
    \value CloseCodeBadOperation The endpoint met an unexpected condition.
    \value CloseCodeTlsHandshakeFailed The TLS handshake failed. Never sent.
*/
