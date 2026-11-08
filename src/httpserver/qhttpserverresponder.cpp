// MIT License - see LICENSE
#include "qhttpserverstream_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qjsondocument.h>
#include <QtHttpServer/qhttpserverresponder.h>
#include <QtHttpServer/qhttpserverresponse.h>

/*!
    \class QHttpServerResponder
    \brief The QHttpServerResponder class writes the response to one request.
    \since 6.4

    Each request that a QAbstractHttpServer receives comes with a responder.
    Call one of the write() functions, or sendResponse(), once to send a
    complete response. To send a body in pieces as it becomes available,
    call writeBeginChunked(), then writeChunk() as often as needed, and
    finally writeEndChunked().

    \snippet httpserver/hello_server.cpp responder

    A responder can be moved, for example into a lambda, to answer later.
    The connection reads no further request until the response is complete.
    If the client disconnects in the meantime, isResponseCanceled() returns
    \c true and writes do nothing.

    A responder that is destroyed before it has written anything sends
    \c{500 Internal Server Error}; one destroyed in the middle of a chunked
    response closes the connection, so the client sees that the body is
    incomplete.

    The server decides the framing of the response: it sets
    \c Content-Length, or uses chunked transfer coding when the length is
    not known (or, for an HTTP/1.0 client, closes the connection after the
    body). The same header fields given by the caller are ignored. The body
    is left out of the response to a \c HEAD request.

    \differences
    There is no \c write() for a \c QFuture.
    \enddifferences

    \sa QHttpServerResponse, QHttpServer::route()
*/

/*!
    \enum QHttpServerResponder::StatusCode

    The HTTP status codes. Each value is the number of the code.

    \value Continue 100
    \value SwitchingProtocols 101
    \value Processing 102
    \value EarlyHints 103
    \value Ok 200
    \value Created 201
    \value Accepted 202
    \value NonAuthoritativeInformation 203
    \value NoContent 204. The response has no body.
    \value ResetContent 205
    \value PartialContent 206
    \value MultiStatus 207
    \value AlreadyReported 208
    \value IMUsed 226
    \value MultipleChoices 300
    \value MovedPermanently 301
    \value Found 302
    \value SeeOther 303
    \value NotModified 304. The response has no body.
    \value UseProxy 305
    \value TemporaryRedirect 307
    \value PermanentRedirect 308
    \value BadRequest 400. Also sent by the server for a request it cannot parse.
    \value Unauthorized 401
    \value PaymentRequired 402
    \value Forbidden 403
    \value NotFound 404. Also sent when no route matches.
    \value MethodNotAllowed 405
    \value NotAcceptable 406
    \value ProxyAuthenticationRequired 407
    \value RequestTimeout 408
    \value Conflict 409
    \value Gone 410
    \value LengthRequired 411
    \value PreconditionFailed 412
    \value PayloadTooLarge 413. Also sent by the server for a body over 64 MiB.
    \value UriTooLong 414. Also sent by the server for a request line over 8 KiB.
    \value UnsupportedMediaType 415
    \value RequestRangeNotSatisfiable 416
    \value ExpectationFailed 417
    \value ImATeapot 418
    \value MisdirectedRequest 421
    \value UnprocessableEntity 422
    \value Locked 423
    \value FailedDependency 424
    \value TooEarly 425
    \value UpgradeRequired 426
    \value PreconditionRequired 428
    \value TooManyRequests 429
    \value RequestHeaderFieldsTooLarge 431. Also sent by the server for header fields over 64 KiB.
    \value UnavailableForLegalReasons 451
    \value InternalServerError 500
    \value NotImplemented 501
    \value BadGateway 502
    \value ServiceUnavailable 503
    \value GatewayTimeout 504
    \value HttpVersionNotSupported 505
    \value VariantAlsoNegotiates 506
    \value InsufficientStorage 507
    \value LoopDetected 508
    \value NotExtended 510
    \value NetworkAuthenticationRequired 511
    \value NetworkConnectTimeoutError 599
*/

namespace {

using WKH = QHttpHeaders::WellKnownHeader;

QHttpHeaders withContentType(const QHttpHeaders &headers, const QByteArray &mimeType) {
    if (mimeType.isEmpty() || headers.contains(WKH::ContentType))
        return headers;
    QHttpHeaders h = headers;
    h.append(WKH::ContentType, mimeType);
    return h;
}

} // namespace

QHttpServerResponder::QHttpServerResponder(QHttpServerStream *stream) : m_stream(stream) {}

/*!
    Move-constructs a responder from \a other, which can no longer write.
*/
QHttpServerResponder::QHttpServerResponder(QHttpServerResponder &&other) noexcept : m_stream(other.m_stream) {
    other.m_stream = nullptr;
}

/*!
    Destroys the responder. If it has not written a response yet,
    \c{500 Internal Server Error} is sent.
*/
QHttpServerResponder::~QHttpServerResponder() {
    if (auto *stream = static_cast<QHttpServerStream *>(m_stream.data()))
        stream->responderDestroyed();
}

/*!
    Sends the contents of \a data as the body, with the header fields
    \a headers and the status \a status. The responder takes ownership of
    \a data and deletes it when it has been sent. The device is opened for
    reading if it is not open yet.

    A random-access device such as a QFile is sent from its current position
    to its end, with a \c Content-Length. A sequential device, such as a
    QProcess, is sent until it closes or emits \c readChannelFinished(),
    using chunked transfer coding. The data is read as the connection can
    take it, so a large file is not loaded into memory.
*/
void QHttpServerResponder::write(QIODevice *data, const QHttpHeaders &headers, StatusCode status) {
    auto *stream = static_cast<QHttpServerStream *>(m_stream.data());
    if (!data)
        return write(headers, status);
    if (!stream || !stream->canRespond()) {
        if (!stream)
            qWarning("QHttpServerResponder::write: the responder cannot write any more");
        data->deleteLater();
        return;
    }
    stream->respondWithDevice(status, headers, data);
}

/*!
    \overload

    Sends the contents of \a data with the content type \a mimeType and the
    status \a status. The responder takes ownership of \a data.
*/
void QHttpServerResponder::write(QIODevice *data, const QByteArray &mimeType, StatusCode status) {
    write(data, withContentType(QHttpHeaders(), mimeType), status);
}

/*!
    Sends \a document, written compactly, as an \c application/json body
    with the header fields \a headers and the status \a status.
*/
void QHttpServerResponder::write(const QJsonDocument &document, const QHttpHeaders &headers, StatusCode status) {
    write(document.toJson(QJsonDocument::Compact), withContentType(headers, "application/json"), status);
}

/*!
    \overload

    Sends \a document as an \c application/json body with the status \a status.
*/
void QHttpServerResponder::write(const QJsonDocument &document, StatusCode status) {
    write(document, QHttpHeaders(), status);
}

/*!
    Sends \a data as the body, with the header fields \a headers and the
    status \a status.
*/
void QHttpServerResponder::write(const QByteArray &data, const QHttpHeaders &headers, StatusCode status) {
    auto *stream = static_cast<QHttpServerStream *>(m_stream.data());
    if (!stream || !stream->canRespond()) {
        if (!stream || !stream->isCanceled())
            qWarning("QHttpServerResponder::write: a response was already sent");
        return;
    }
    stream->respond(status, headers, data);
}

/*!
    \overload

    Sends \a data with the content type \a mimeType and the status \a status.
*/
void QHttpServerResponder::write(const QByteArray &data, const QByteArray &mimeType, StatusCode status) {
    write(data, withContentType(QHttpHeaders(), mimeType), status);
}

/*!
    \overload

    Sends a response without a body, with the header fields \a headers and
    the status \a status.
*/
void QHttpServerResponder::write(const QHttpHeaders &headers, StatusCode status) {
    write(QByteArray(), headers, status);
}

/*!
    \overload

    Sends a response without a body and with the status \a status.
*/
void QHttpServerResponder::write(StatusCode status) { write(QByteArray(), QHttpHeaders(), status); }

/*!
    Starts a response with the header fields \a headers and the status
    \a status whose body follows in pieces. Send the pieces with writeChunk()
    and end the body with writeEndChunked().

    \sa writeChunk(), writeEndChunked()
*/
void QHttpServerResponder::writeBeginChunked(const QHttpHeaders &headers, StatusCode status) {
    writeBeginChunked(headers, {}, status);
}

/*!
    \overload

    Starts a chunked response with the header fields \a headers and the
    status \a status, announcing in a \c Trailer header that the fields
    \a trailerNames will follow the body. Pass their values to
    writeEndChunked().
*/
void QHttpServerResponder::writeBeginChunked(const QHttpHeaders &headers,
                                             const QList<QHttpHeaders::WellKnownHeader> &trailerNames,
                                             StatusCode status) {
    auto *stream = static_cast<QHttpServerStream *>(m_stream.data());
    if (!stream || !stream->canRespond()) {
        if (!stream || !stream->isCanceled())
            qWarning("QHttpServerResponder::writeBeginChunked: a response was already sent");
        return;
    }
    QList<QByteArray> names;
    for (QHttpHeaders::WellKnownHeader name : trailerNames)
        names.append(QHttpHeaders::wellKnownHeaderName(name).toByteArray());
    stream->beginChunked(status, headers, names);
}

/*!
    \overload

    Starts a chunked response with the content type \a mimeType and the
    status \a status.
*/
void QHttpServerResponder::writeBeginChunked(const QByteArray &mimeType, StatusCode status) {
    writeBeginChunked(withContentType(QHttpHeaders(), mimeType), status);
}

/*!
    Sends \a data as the next piece of a chunked response. Empty data is
    ignored.

    \sa writeBeginChunked(), writeEndChunked()
*/
void QHttpServerResponder::writeChunk(const QByteArray &data) {
    if (auto *stream = static_cast<QHttpServerStream *>(m_stream.data()))
        stream->writeChunk(data);
}

/*!
    Sends \a data as the last piece of a chunked response, followed by the
    trailer fields \a trailers, and completes the response.

    \sa writeBeginChunked()
*/
void QHttpServerResponder::writeEndChunked(const QByteArray &data, const QHttpHeaders &trailers) {
    if (auto *stream = static_cast<QHttpServerStream *>(m_stream.data()))
        stream->endChunked(data, trailers);
}

/*!
    \overload

    Sends \a data as the last piece of a chunked response and completes the
    response.
*/
void QHttpServerResponder::writeEndChunked(const QByteArray &data) { writeEndChunked(data, QHttpHeaders()); }

/*!
    Sends \a response: its status, its header fields and its body.
*/
void QHttpServerResponder::sendResponse(const QHttpServerResponse &response) {
    write(response.data(), response.headers(), response.statusCode());
}

/*!
    Returns \c true if the response can no longer be delivered because the
    client has disconnected; otherwise returns \c false.
*/
bool QHttpServerResponder::isResponseCanceled() const {
    auto *stream = static_cast<QHttpServerStream *>(m_stream.data());
    return !stream || stream->isCanceled();
}

#include "moc_qhttpserverresponder.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
