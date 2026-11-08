// MIT License - see LICENSE
#include <QtHttpServer/qhttpserverwebsocketupgraderesponse.h>

#include <utility>

/*!
    \class QHttpServerWebSocketUpgradeResponse
    \brief The QHttpServerWebSocketUpgradeResponse class is the verdict of a verifier on a WebSocket upgrade request.
    \since 6.8

    A verifier added with QAbstractHttpServer::addWebSocketUpgradeVerifier()
    returns accept() to turn the connection into a QWebSocket, deny() to
    refuse it with an HTTP error, or passToNext() to leave the decision to
    the next verifier.

    \differences
    None.
    \enddifferences

    \sa QAbstractHttpServer::addWebSocketUpgradeVerifier()
*/

/*!
    \enum QHttpServerWebSocketUpgradeResponse::ResponseType

    The verdict.

    \value Accept The upgrade is accepted.
    \value Deny The upgrade is refused with denyStatus() and denyMessage().
    \value PassToNext The next verifier decides.
*/

QHttpServerWebSocketUpgradeResponse::QHttpServerWebSocketUpgradeResponse(ResponseType type, int status,
                                                                         QByteArray message)
    : m_type(type), m_status(status), m_message(std::move(message)) {}

/*!
    Constructs a copy of \a other.
*/
QHttpServerWebSocketUpgradeResponse::QHttpServerWebSocketUpgradeResponse(
    const QHttpServerWebSocketUpgradeResponse &other) = default;

/*!
    Move-constructs a verdict from \a other.
*/
QHttpServerWebSocketUpgradeResponse::QHttpServerWebSocketUpgradeResponse(
    QHttpServerWebSocketUpgradeResponse &&other) noexcept = default;

/*!
    Assigns \a other to this verdict and returns a reference to it.
*/
QHttpServerWebSocketUpgradeResponse &
QHttpServerWebSocketUpgradeResponse::operator=(const QHttpServerWebSocketUpgradeResponse &other) = default;

/*!
    \overload

    Move-assigns \a other to this verdict and returns a reference to it.
*/
QHttpServerWebSocketUpgradeResponse &
QHttpServerWebSocketUpgradeResponse::operator=(QHttpServerWebSocketUpgradeResponse &&other) noexcept = default;

/*!
    Destroys the verdict.
*/
QHttpServerWebSocketUpgradeResponse::~QHttpServerWebSocketUpgradeResponse() = default;

/*!
    Swaps this verdict with \a other.
*/
void QHttpServerWebSocketUpgradeResponse::swap(QHttpServerWebSocketUpgradeResponse &other) noexcept {
    std::swap(m_type, other.m_type);
    std::swap(m_status, other.m_status);
    m_message.swap(other.m_message);
}

/*!
    Returns the verdict.
*/
QHttpServerWebSocketUpgradeResponse::ResponseType QHttpServerWebSocketUpgradeResponse::type() const { return m_type; }

/*!
    Returns the HTTP status sent when the upgrade is denied.
*/
int QHttpServerWebSocketUpgradeResponse::denyStatus() const { return m_status; }

/*!
    Returns the body sent when the upgrade is denied.
*/
QByteArray QHttpServerWebSocketUpgradeResponse::denyMessage() const { return m_message; }

/*!
    Returns a verdict that accepts the upgrade.
*/
QHttpServerWebSocketUpgradeResponse QHttpServerWebSocketUpgradeResponse::accept() {
    return QHttpServerWebSocketUpgradeResponse(ResponseType::Accept, 101, QByteArray());
}

/*!
    Returns a verdict that refuses the upgrade with \c{403 Forbidden}.
*/
QHttpServerWebSocketUpgradeResponse QHttpServerWebSocketUpgradeResponse::deny() {
    return QHttpServerWebSocketUpgradeResponse(ResponseType::Deny, 403, QByteArrayLiteral("Forbidden"));
}

/*!
    \overload

    Returns a verdict that refuses the upgrade with the HTTP status \a status
    and the body \a message.
*/
QHttpServerWebSocketUpgradeResponse QHttpServerWebSocketUpgradeResponse::deny(int status, QByteArray message) {
    return QHttpServerWebSocketUpgradeResponse(ResponseType::Deny, status, std::move(message));
}

/*!
    Returns a verdict that leaves the decision to the next verifier.
*/
QHttpServerWebSocketUpgradeResponse QHttpServerWebSocketUpgradeResponse::passToNext() {
    return QHttpServerWebSocketUpgradeResponse(ResponseType::PassToNext, 0, QByteArray());
}
