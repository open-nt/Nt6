// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qstring.h>

#include <utility>

class QHttpHeaders {
    Q_GADGET
public:
    enum class WellKnownHeader {
        AIM,
        Accept,
        AcceptCharset,
        AcceptEncoding,
        AcceptLanguage,
        AcceptRanges,
        AccessControlAllowCredentials,
        AccessControlAllowHeaders,
        AccessControlAllowMethods,
        AccessControlAllowOrigin,
        AccessControlExposeHeaders,
        AccessControlMaxAge,
        AccessControlRequestHeaders,
        AccessControlRequestMethod,
        Age,
        Allow,
        AltSvc,
        Authorization,
        CacheControl,
        Connection,
        ContentDisposition,
        ContentEncoding,
        ContentLanguage,
        ContentLength,
        ContentLocation,
        ContentRange,
        ContentSecurityPolicy,
        ContentType,
        Cookie,
        Date,
        ETag,
        Expect,
        Expires,
        Forwarded,
        From,
        Host,
        IfMatch,
        IfModifiedSince,
        IfNoneMatch,
        IfRange,
        IfUnmodifiedSince,
        KeepAlive,
        LastModified,
        Link,
        Location,
        MaxForwards,
        Origin,
        Pragma,
        ProxyAuthenticate,
        ProxyAuthorization,
        Range,
        Referer,
        RetryAfter,
        SecWebSocketAccept,
        SecWebSocketExtensions,
        SecWebSocketKey,
        SecWebSocketProtocol,
        SecWebSocketVersion,
        Server,
        SetCookie,
        StrictTransportSecurity,
        TE,
        Trailer,
        TransferEncoding,
        Upgrade,
        UserAgent,
        Vary,
        Via,
        WWWAuthenticate,
        XContentTypeOptions,
        XForwardedFor,
        XFrameOptions,
    };
    Q_ENUM(WellKnownHeader)

    QHttpHeaders() noexcept;
    QHttpHeaders(const QHttpHeaders &other);
    QHttpHeaders(QHttpHeaders &&other) noexcept;
    QHttpHeaders &operator=(const QHttpHeaders &other);
    QHttpHeaders &operator=(QHttpHeaders &&other) noexcept;
    ~QHttpHeaders();

    /*!
        Swaps these headers with \a other. This operation is very fast and never fails.
    */
    void swap(QHttpHeaders &other) noexcept { m_headers.swap(other.m_headers); }

    bool append(QByteArrayView name, QByteArrayView value);
    bool append(WellKnownHeader name, QByteArrayView value);
    bool insert(qsizetype i, QByteArrayView name, QByteArrayView value);
    bool insert(qsizetype i, WellKnownHeader name, QByteArrayView value);
    bool replace(qsizetype i, QByteArrayView name, QByteArrayView newValue);
    bool replace(qsizetype i, WellKnownHeader name, QByteArrayView newValue);
    bool replaceOrAppend(QByteArrayView name, QByteArrayView newValue);
    bool replaceOrAppend(WellKnownHeader name, QByteArrayView newValue);

    bool contains(QByteArrayView name) const;
    bool contains(WellKnownHeader name) const;

    void clear();
    void removeAll(QByteArrayView name);
    void removeAll(WellKnownHeader name);
    void removeAt(qsizetype i);

    QByteArrayView value(QByteArrayView name, QByteArrayView defaultValue = {}) const noexcept;
    QByteArrayView value(WellKnownHeader name, QByteArrayView defaultValue = {}) const noexcept;
    QList<QByteArray> values(QByteArrayView name) const;
    QList<QByteArray> values(WellKnownHeader name) const;
    QByteArrayView valueAt(qsizetype i) const noexcept;
    QLatin1StringView nameAt(qsizetype i) const noexcept;
    QByteArray combinedValue(QByteArrayView name) const;
    QByteArray combinedValue(WellKnownHeader name) const;

    /*!
        Returns the number of header entries. A name that occurs several times
        is counted once per entry.

        \sa isEmpty()
    */
    qsizetype size() const noexcept { return m_headers.size(); }
    /*!
        Reserves space for \a size header entries.
    */
    void reserve(qsizetype size) { m_headers.reserve(size); }
    /*!
        Returns \c true if there are no header entries; otherwise returns \c false.

        \sa size()
    */
    bool isEmpty() const noexcept { return m_headers.isEmpty(); }

    static QByteArrayView wellKnownHeaderName(WellKnownHeader name) noexcept;

    static QHttpHeaders fromListOfPairs(const QList<std::pair<QByteArray, QByteArray>> &headers);
    static QHttpHeaders fromMultiMap(const QMultiMap<QByteArray, QByteArray> &headers);
    static QHttpHeaders fromMultiHash(const QMultiHash<QByteArray, QByteArray> &headers);
    QList<std::pair<QByteArray, QByteArray>> toListOfPairs() const;
    QMultiMap<QByteArray, QByteArray> toMultiMap() const;
    QMultiHash<QByteArray, QByteArray> toMultiHash() const;

private:
    QList<std::pair<QByteArray, QByteArray>> m_headers; // names are stored in lower case
};
