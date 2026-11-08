// MIT License - see LICENSE
#include <QtWebSockets/qwebsocketcorsauthenticator.h>

#include <utility>

/*!
    \class QWebSocketCorsAuthenticator
    \brief The QWebSocketCorsAuthenticator class decides whether a WebSocket client's origin is accepted.
    \since 5.3

    QWebSocketServer::originAuthenticationRequired() passes one to its slots
    during each opening handshake. Browsers put the origin of the page that
    opens the connection in the \c Origin header, so a server that only
    serves its own pages can refuse the others:

    \snippet websockets/echo.cpp origin

    \differences
    None.
    \enddifferences

    \sa QWebSocketServer::originAuthenticationRequired()
*/

/*!
    Constructs an authenticator for \a origin. The origin is allowed until
    setAllowed(false) is called.
*/
QWebSocketCorsAuthenticator::QWebSocketCorsAuthenticator(const QString &origin) : m_origin(origin) {}

/*!
    Constructs a copy of \a other.
*/
QWebSocketCorsAuthenticator::QWebSocketCorsAuthenticator(const QWebSocketCorsAuthenticator &other) = default;

/*!
    Move-constructs an authenticator from \a other.
*/
QWebSocketCorsAuthenticator::QWebSocketCorsAuthenticator(QWebSocketCorsAuthenticator &&other) noexcept = default;

/*!
    Assigns \a other to this authenticator and returns a reference to it.
*/
QWebSocketCorsAuthenticator &QWebSocketCorsAuthenticator::operator=(const QWebSocketCorsAuthenticator &other) = default;

/*!
    \overload

    Move-assigns \a other to this authenticator and returns a reference to it.
*/
QWebSocketCorsAuthenticator &
QWebSocketCorsAuthenticator::operator=(QWebSocketCorsAuthenticator &&other) noexcept = default;

/*!
    Destroys the authenticator.
*/
QWebSocketCorsAuthenticator::~QWebSocketCorsAuthenticator() = default;

/*!
    Swaps this authenticator with \a other.
*/
void QWebSocketCorsAuthenticator::swap(QWebSocketCorsAuthenticator &other) noexcept {
    m_origin.swap(other.m_origin);
    std::swap(m_allowed, other.m_allowed);
}

/*!
    Returns the origin the client sent, or an empty string if it sent none.
*/
QString QWebSocketCorsAuthenticator::origin() const { return m_origin; }

/*!
    Sets whether the origin is accepted to \a allowed.

    \sa allowed()
*/
void QWebSocketCorsAuthenticator::setAllowed(bool allowed) { m_allowed = allowed; }

/*!
    Returns \c true if the origin is accepted; otherwise returns \c false.

    \sa setAllowed()
*/
bool QWebSocketCorsAuthenticator::allowed() const { return m_allowed; }
