// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qpointer.h>
#include <QtNetwork/qhttpheaders.h>

class QHttpServerResponse;
class QHttpServerStream;
class QIODevice;
class QJsonDocument;

class QHttpServerResponder {
    Q_GADGET
public:
    enum class StatusCode {
        Continue = 100,
        SwitchingProtocols = 101,
        Processing = 102,
        EarlyHints = 103,

        Ok = 200,
        Created = 201,
        Accepted = 202,
        NonAuthoritativeInformation = 203,
        NoContent = 204,
        ResetContent = 205,
        PartialContent = 206,
        MultiStatus = 207,
        AlreadyReported = 208,
        IMUsed = 226,

        MultipleChoices = 300,
        MovedPermanently = 301,
        Found = 302,
        SeeOther = 303,
        NotModified = 304,
        UseProxy = 305,
        TemporaryRedirect = 307,
        PermanentRedirect = 308,

        BadRequest = 400,
        Unauthorized = 401,
        PaymentRequired = 402,
        Forbidden = 403,
        NotFound = 404,
        MethodNotAllowed = 405,
        NotAcceptable = 406,
        ProxyAuthenticationRequired = 407,
        RequestTimeout = 408,
        Conflict = 409,
        Gone = 410,
        LengthRequired = 411,
        PreconditionFailed = 412,
        PayloadTooLarge = 413,
        UriTooLong = 414,
        UnsupportedMediaType = 415,
        RequestRangeNotSatisfiable = 416,
        ExpectationFailed = 417,
        ImATeapot = 418,
        MisdirectedRequest = 421,
        UnprocessableEntity = 422,
        Locked = 423,
        FailedDependency = 424,
        TooEarly = 425,
        UpgradeRequired = 426,
        PreconditionRequired = 428,
        TooManyRequests = 429,
        RequestHeaderFieldsTooLarge = 431,
        UnavailableForLegalReasons = 451,

        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnavailable = 503,
        GatewayTimeout = 504,
        HttpVersionNotSupported = 505,
        VariantAlsoNegotiates = 506,
        InsufficientStorage = 507,
        LoopDetected = 508,
        NotExtended = 510,
        NetworkAuthenticationRequired = 511,
        NetworkConnectTimeoutError = 599,
    };
    Q_ENUM(StatusCode)

    QHttpServerResponder(QHttpServerResponder &&other) noexcept;
    QHttpServerResponder &operator=(QHttpServerResponder &&other) = delete;
    QHttpServerResponder(const QHttpServerResponder &) = delete;
    QHttpServerResponder &operator=(const QHttpServerResponder &) = delete;
    ~QHttpServerResponder();

    void write(QIODevice *data, const QHttpHeaders &headers, StatusCode status = StatusCode::Ok);
    void write(QIODevice *data, const QByteArray &mimeType, StatusCode status = StatusCode::Ok);
    void write(const QJsonDocument &document, const QHttpHeaders &headers, StatusCode status = StatusCode::Ok);
    void write(const QJsonDocument &document, StatusCode status = StatusCode::Ok);
    void write(const QByteArray &data, const QHttpHeaders &headers, StatusCode status = StatusCode::Ok);
    void write(const QByteArray &data, const QByteArray &mimeType, StatusCode status = StatusCode::Ok);
    void write(const QHttpHeaders &headers, StatusCode status = StatusCode::Ok);
    void write(StatusCode status = StatusCode::Ok);

    void writeBeginChunked(const QHttpHeaders &headers, StatusCode status = StatusCode::Ok);
    void writeBeginChunked(const QHttpHeaders &headers, const QList<QHttpHeaders::WellKnownHeader> &trailerNames,
                           StatusCode status = StatusCode::Ok);
    void writeBeginChunked(const QByteArray &mimeType, StatusCode status = StatusCode::Ok);
    void writeChunk(const QByteArray &data);
    void writeEndChunked(const QByteArray &data, const QHttpHeaders &trailers);
    void writeEndChunked(const QByteArray &data);

    void sendResponse(const QHttpServerResponse &response);
    bool isResponseCanceled() const;

private:
    friend class QHttpServerStream;
    explicit QHttpServerResponder(QHttpServerStream *stream);

    QPointer<QObject> m_stream; // the QHttpServerStream, which is private
};
