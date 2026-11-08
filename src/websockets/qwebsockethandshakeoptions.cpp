// MIT License - see LICENSE
#include <QtWebSockets/qwebsockethandshakeoptions.h>

/*!
    \class QWebSocketHandshakeOptions
    \brief The QWebSocketHandshakeOptions class holds options of a WebSocket client's opening handshake.
    \since 6.4

    Pass the options to QWebSocket::open(). The subprotocols are sent in the
    \c Sec-WebSocket-Protocol header, in order of preference; the server
    chooses one of them, and QWebSocket::subprotocol() returns it.

    \snippet websockets/echo.cpp subprotocols

    \differences
    None.
    \enddifferences

    \sa QWebSocket::open(), QWebSocketServer::setSupportedSubprotocols()
*/

/*!
    Constructs options with no subprotocols.
*/
QWebSocketHandshakeOptions::QWebSocketHandshakeOptions() = default;

/*!
    Constructs a copy of \a other.
*/
QWebSocketHandshakeOptions::QWebSocketHandshakeOptions(const QWebSocketHandshakeOptions &other) = default;

/*!
    Move-constructs options from \a other.
*/
QWebSocketHandshakeOptions::QWebSocketHandshakeOptions(QWebSocketHandshakeOptions &&other) noexcept = default;

/*!
    Assigns \a other to these options and returns a reference to them.
*/
QWebSocketHandshakeOptions &QWebSocketHandshakeOptions::operator=(const QWebSocketHandshakeOptions &other) = default;

/*!
    \overload

    Move-assigns \a other to these options and returns a reference to them.
*/
QWebSocketHandshakeOptions &
QWebSocketHandshakeOptions::operator=(QWebSocketHandshakeOptions &&other) noexcept = default;

/*!
    Destroys the options.
*/
QWebSocketHandshakeOptions::~QWebSocketHandshakeOptions() = default;

/*!
    Swaps these options with \a other.
*/
void QWebSocketHandshakeOptions::swap(QWebSocketHandshakeOptions &other) noexcept {
    m_subprotocols.swap(other.m_subprotocols);
}

/*!
    Returns the subprotocols to ask for.

    \sa setSubprotocols()
*/
QStringList QWebSocketHandshakeOptions::subprotocols() const { return m_subprotocols; }

/*!
    Sets the subprotocols to ask for to \a protocols, in order of preference.
    Each must be an HTTP token, such as \c{chat.v2}.

    \sa subprotocols()
*/
void QWebSocketHandshakeOptions::setSubprotocols(const QStringList &protocols) { m_subprotocols = protocols; }
