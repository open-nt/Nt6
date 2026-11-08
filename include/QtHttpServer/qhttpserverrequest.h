// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qurl.h>
#include <QtCore/qurlquery.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qhttpheaders.h>

class QHttpServerRequest {
    Q_GADGET
public:
    enum class Method {
        Unknown = 0x0000,
        Get = 0x0001,
        Put = 0x0002,
        Delete = 0x0004,
        Post = 0x0008,
        Head = 0x0010,
        Options = 0x0020,
        Patch = 0x0040,
        Connect = 0x0080,
        Trace = 0x0100,
        AnyKnown = 0x01FF,
    };
    Q_ENUM(Method)
    Q_DECLARE_FLAGS(Methods, Method)
    Q_FLAG(Methods)

    QHttpServerRequest(const QHttpServerRequest &other);
    QHttpServerRequest(QHttpServerRequest &&other) noexcept;
    QHttpServerRequest &operator=(const QHttpServerRequest &other);
    QHttpServerRequest &operator=(QHttpServerRequest &&other) noexcept;
    ~QHttpServerRequest();

    QByteArray value(const QByteArray &key) const;
    QUrl url() const;
    QUrlQuery query() const;
    Method method() const;
    /*!
        Returns the header fields of the request, in the order they arrived.

        \sa value()
    */
    const QHttpHeaders &headers() const & { return m_headers; }
    /*!
        \overload

        Returns the header fields of the request, moved out of a temporary request.
    */
    QHttpHeaders headers() && { return std::move(m_headers); }
    QByteArray body() const;
    QHostAddress remoteAddress() const;
    quint16 remotePort() const;
    QHostAddress localAddress() const;
    quint16 localPort() const;

private:
    friend class QHttpServerStream;
    QHttpServerRequest() = default;

    QUrl m_url;
    Method m_method = Method::Unknown;
    QHttpHeaders m_headers;
    QByteArray m_body;
    QHostAddress m_remoteAddress;
    quint16 m_remotePort = 0;
    QHostAddress m_localAddress;
    quint16 m_localPort = 0;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QHttpServerRequest::Methods)
