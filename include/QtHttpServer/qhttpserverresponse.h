// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtHttpServer/qhttpserverresponder.h>
#include <QtNetwork/qhttpheaders.h>

class QJsonArray;
class QJsonObject;

class QHttpServerResponse {
public:
    using StatusCode = QHttpServerResponder::StatusCode;

    QHttpServerResponse(StatusCode statusCode);
    QHttpServerResponse(const char *data, StatusCode status = StatusCode::Ok);
    QHttpServerResponse(const QString &data, StatusCode status = StatusCode::Ok);
    explicit QHttpServerResponse(const QByteArray &data, StatusCode status = StatusCode::Ok);
    explicit QHttpServerResponse(QByteArray &&data, StatusCode status = StatusCode::Ok);
    QHttpServerResponse(const QJsonObject &data, StatusCode status = StatusCode::Ok);
    QHttpServerResponse(const QJsonArray &data, StatusCode status = StatusCode::Ok);
    QHttpServerResponse(const QByteArray &mimeType, const QByteArray &data, StatusCode status = StatusCode::Ok);
    QHttpServerResponse(const QByteArray &mimeType, QByteArray &&data, StatusCode status = StatusCode::Ok);
    QHttpServerResponse(QHttpServerResponse &&other) noexcept;
    QHttpServerResponse &operator=(QHttpServerResponse &&other) noexcept;
    QHttpServerResponse(const QHttpServerResponse &) = delete;
    QHttpServerResponse &operator=(const QHttpServerResponse &) = delete;
    ~QHttpServerResponse();

    static QHttpServerResponse fromFile(const QString &fileName);

    QByteArray data() const;
    QByteArray mimeType() const;
    StatusCode statusCode() const;
    QHttpHeaders headers() const;
    void setHeaders(const QHttpHeaders &newHeaders);
    void setHeaders(QHttpHeaders &&newHeaders);
    void swap(QHttpServerResponse &other) noexcept;

private:
    QByteArray m_data;
    QHttpHeaders m_headers;
    StatusCode m_status = StatusCode::Ok;
};
