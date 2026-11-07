// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qlist.h>
#include <QtCore/qurl.h>
#include <QtNetwork/qnetworkrequest.h>

#include <utility>

class QNetworkAccessManager;
namespace QtPrivate {
class HttpReply;
}

class QNetworkReply : public QIODevice {
    Q_OBJECT
public:
    enum NetworkError {
        NoError = 0,
        ConnectionRefusedError = 1,
        RemoteHostClosedError,
        HostNotFoundError,
        TimeoutError,
        OperationCanceledError,
        SslHandshakeFailedError,
        TemporaryNetworkFailureError,
        NetworkSessionFailedError,
        BackgroundRequestNotAllowedError,
        TooManyRedirectsError,
        InsecureRedirectError,
        UnknownNetworkError = 99,
        ContentAccessDenied = 201,
        ContentOperationNotPermittedError,
        ContentNotFoundError,
        AuthenticationRequiredError,
        ContentReSendError,
        ContentConflictError,
        ContentGoneError,
        UnknownContentError = 299,
        ProtocolUnknownError = 301,
        ProtocolInvalidOperationError,
        ProtocolFailure = 399,
        InternalServerError = 401,
        OperationNotImplementedError,
        ServiceUnavailableError,
        UnknownServerError = 499
    };

    ~QNetworkReply() override;

    QUrl url() const;
    QNetworkRequest request() const;
    QNetworkAccessManager *manager() const;
    NetworkError error() const;
    bool isFinished() const;
    bool isRunning() const;
    int httpStatusCode() const;
    QByteArray httpReasonPhrase() const;
    QByteArray header(QNetworkRequest::KnownHeaders header) const;
    bool hasRawHeader(const QByteArray &headerName) const;
    QList<QByteArray> rawHeaderList() const;
    QByteArray rawHeader(const QByteArray &headerName) const;
    QList<std::pair<QByteArray, QByteArray>> rawHeaderPairs() const;

    bool isSequential() const override;
    qint64 bytesAvailable() const override;
    bool canReadLine() const override;
    bool atEnd() const override;
    void close() override;

#ifndef QT_NO_SSL
    QSslConfiguration sslConfiguration() const;
    virtual void ignoreSslErrors(const QList<QSslError> &errors);
#endif

public slots:
    virtual void abort() = 0;
#ifndef QT_NO_SSL
    virtual void ignoreSslErrors();
#endif

signals:
    /*!
        This signal is emitted when the reply has finished, whether it
        succeeded or not. It is emitted exactly once.
    */
    void finished() { emitSignal(&QNetworkReply::finished); }
    /*!
        This signal is emitted just before finished() when the reply fails with
        \a code.
    */
    void errorOccurred(QNetworkReply::NetworkError code) { emitSignal(&QNetworkReply::errorOccurred, code); }
    /*!
        This signal is emitted when the response headers have been received.
    */
    void metaDataChanged() { emitSignal(&QNetworkReply::metaDataChanged); }
    /*!
        This signal is emitted when a redirect to \a url is followed.
    */
    void redirected(const QUrl &url) { emitSignal(&QNetworkReply::redirected, url); }
    /*!
        This signal is emitted as the body is sent, with \a bytesSent of
        \a bytesTotal bytes done.
    */
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal) {
        emitSignal(&QNetworkReply::uploadProgress, bytesSent, bytesTotal);
    }
    /*!
        This signal is emitted as data arrives, with \a bytesReceived of
        \a bytesTotal bytes done. \a bytesTotal is \c -1 if the size is unknown.
    */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
        emitSignal(&QNetworkReply::downloadProgress, bytesReceived, bytesTotal);
    }

#ifndef QT_NO_SSL
    /*!
        This signal is emitted when the TLS handshake has finished.
    */
    void encrypted() { emitSignal(&QNetworkReply::encrypted); }
    /*!
        This signal is emitted if verifying the server found \a errors. Unless
        a connected slot calls ignoreSslErrors(), the reply fails.
    */
    void sslErrors(const QList<QSslError> &errors) { emitSignal(&QNetworkReply::sslErrors, errors); }
#endif

protected:
    explicit QNetworkReply(QObject *parent = nullptr);
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    friend class QtPrivate::HttpReply;

    QNetworkAccessManager *m_manager = nullptr;
    QNetworkRequest m_request;
    QUrl m_url;
    NetworkError m_error = NoError;
    bool m_finished = false;
    int m_status = 0;
    QByteArray m_reason;
    QList<std::pair<QByteArray, QByteArray>> m_headers;
    QByteArray m_body;
    qint64 m_bodyPos = 0;
#ifndef QT_NO_SSL
    QSslConfiguration m_sslConfiguration;
#endif
};
