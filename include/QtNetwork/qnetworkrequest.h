// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslconfiguration.h>
#endif

#include <utility>

class QNetworkRequest {
public:
    enum KnownHeaders {
        ContentTypeHeader,
        ContentLengthHeader,
        LocationHeader,
        LastModifiedHeader,
        CookieHeader,
        SetCookieHeader,
        ContentDispositionHeader,
        UserAgentHeader,
        ServerHeader,
        IfModifiedSinceHeader,
        ETagHeader,
        IfMatchHeader,
        IfNoneMatchHeader
    };
    enum RedirectPolicy { ManualRedirectPolicy, NoLessSafeRedirectPolicy, SameOriginRedirectPolicy };
    enum Attribute {
        HttpStatusCodeAttribute,
        HttpReasonPhraseAttribute,
        RedirectionTargetAttribute,
        ConnectionEncryptedAttribute,
        CacheLoadControlAttribute,
        CacheSaveControlAttribute,
        SourceIsFromCacheAttribute,
        DoNotBufferUploadDataAttribute,
        HttpPipeliningAllowedAttribute,
        HttpPipeliningWasUsedAttribute,
        CustomVerbAttribute,
        CookieLoadControlAttribute,
        AuthenticationReuseAttribute,
        CookieSaveControlAttribute,
        MaximumDownloadBufferSizeAttribute,
        DownloadBufferAttribute,
        SynchronousRequestAttribute,
        BackgroundRequestAttribute,
        EmitAllUploadProgressSignalsAttribute,
        Http2AllowedAttribute,
        Http2WasUsedAttribute,
        OriginalContentLengthAttribute,
        RedirectPolicyAttribute,
        Http2DirectAttribute,
        ResourceTypeAttribute,
        AutoDeleteReplyOnFinishAttribute,
        ConnectionCacheExpiryTimeoutSecondsAttribute,
        Http2CleartextAllowedAttribute,
        UseCredentialsAttribute,
        User = 1000,
        UserMax = 32767
    };

    QNetworkRequest() = default;
    explicit QNetworkRequest(const QUrl &url);

    QUrl url() const;
    void setUrl(const QUrl &url);
    QByteArray header(KnownHeaders header) const;
    void setHeader(KnownHeaders header, const QByteArray &value);
    bool hasRawHeader(const QByteArray &headerName) const;
    QList<QByteArray> rawHeaderList() const;
    QByteArray rawHeader(const QByteArray &headerName) const;
    void setRawHeader(const QByteArray &headerName, const QByteArray &headerValue);
    QVariant attribute(Attribute code, const QVariant &defaultValue = QVariant()) const;
    void setAttribute(Attribute code, const QVariant &value);
    RedirectPolicy redirectPolicy() const;
    void setRedirectPolicy(RedirectPolicy policy);
    int maximumRedirectsAllowed() const;
    void setMaximumRedirectsAllowed(int maximumRedirectsAllowed);
    int transferTimeout() const;
    void setTransferTimeout(int timeout);
#ifndef QT_NO_SSL
    QSslConfiguration sslConfiguration() const;
    void setSslConfiguration(const QSslConfiguration &configuration);
#endif

    friend bool operator==(const QNetworkRequest &a, const QNetworkRequest &b) {
        return a.m_url == b.m_url && a.m_headers == b.m_headers && a.m_attributes == b.m_attributes;
    }
    friend bool operator!=(const QNetworkRequest &a, const QNetworkRequest &b) { return !(a == b); }

private:
    QUrl m_url;
    QList<std::pair<QByteArray, QByteArray>> m_headers;
    QList<std::pair<int, QVariant>> m_attributes;
    RedirectPolicy m_policy = NoLessSafeRedirectPolicy;
    int m_maxRedirects = 50;
    int m_timeout = 0;
#ifndef QT_NO_SSL
    QSslConfiguration m_sslConfiguration = QSslConfiguration::defaultConfiguration();
#endif
};
