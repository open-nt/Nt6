// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtNetwork/qnetworkreply.h>
#include <QtNetwork/qnetworkrequest.h>

class QIODevice;

class QNetworkAccessManager : public QObject {
    Q_OBJECT
public:
    enum Operation {
        HeadOperation = 1,
        GetOperation,
        PutOperation,
        PostOperation,
        DeleteOperation,
        CustomOperation,
        UnknownOperation = 0
    };

    explicit QNetworkAccessManager(QObject *parent = nullptr);
    ~QNetworkAccessManager() override;

    QNetworkReply *head(const QNetworkRequest &request);
    QNetworkReply *get(const QNetworkRequest &request);
    QNetworkReply *post(const QNetworkRequest &request, const QByteArray &data);
    QNetworkReply *post(const QNetworkRequest &request, QIODevice *data);
    QNetworkReply *put(const QNetworkRequest &request, const QByteArray &data);
    QNetworkReply *put(const QNetworkRequest &request, QIODevice *data);
    QNetworkReply *deleteResource(const QNetworkRequest &request);
    QNetworkReply *sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb, const QByteArray &data = QByteArray());

    int transferTimeout() const;
    void setTransferTimeout(int timeout);

signals:
    /*!
        This signal is emitted whenever \a reply finishes. The reply stays
        valid until you delete it; use deleteLater() from a slot.
    */
    void finished(QNetworkReply *reply) { emitSignal(&QNetworkAccessManager::finished, reply); }

#ifndef QT_NO_SSL
    /*!
        This signal is emitted when the TLS handshake of \a reply has finished.
    */
    void encrypted(QNetworkReply *reply) { emitSignal(&QNetworkAccessManager::encrypted, reply); }
    /*!
        This signal is emitted if verifying the server of \a reply found
        \a errors. Unless a connected slot calls
        QNetworkReply::ignoreSslErrors(), the reply fails.
    */
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &errors) { emitSignal(&QNetworkAccessManager::sslErrors, reply, errors); }
#endif

private:
    QNetworkReply *start(const QNetworkRequest &request, const QByteArray &verb, const QByteArray &body);

    int m_timeout = 0;
};
