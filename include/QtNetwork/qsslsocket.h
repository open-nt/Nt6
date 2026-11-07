// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtNetwork/qssl.h>
#include <QtNetwork/qsslcertificate.h>
#include <QtNetwork/qsslcipher.h>
#include <QtNetwork/qsslerror.h>
#include <QtNetwork/qsslkey.h>
#include <QtNetwork/qtcpsocket.h>

#include <memory>

class QSslConfiguration;

class QSslSocket : public QTcpSocket {
    Q_OBJECT
public:
    enum SslMode { UnencryptedMode, SslClientMode, SslServerMode };
    enum PeerVerifyMode { VerifyNone, QueryPeer, VerifyPeer, AutoVerifyPeer };

    explicit QSslSocket(QObject *parent = nullptr);
    ~QSslSocket() override;

    void connectToHostEncrypted(const QString &hostName, quint16 port, OpenMode mode = ReadWrite, NetworkLayerProtocol protocol = AnyIPProtocol);
    void connectToHostEncrypted(const QString &hostName, quint16 port, const QString &sslPeerName, OpenMode mode = ReadWrite, NetworkLayerProtocol protocol = AnyIPProtocol);

    SslMode mode() const;
    bool isEncrypted() const;
    QSsl::SslProtocol protocol() const;
    void setProtocol(QSsl::SslProtocol protocol);
    PeerVerifyMode peerVerifyMode() const;
    void setPeerVerifyMode(PeerVerifyMode mode);
    int peerVerifyDepth() const;
    void setPeerVerifyDepth(int depth);
    QString peerVerifyName() const;
    void setPeerVerifyName(const QString &hostName);
    QSslCertificate localCertificate() const;
    void setLocalCertificate(const QSslCertificate &certificate);
    QSslKey privateKey() const;
    void setPrivateKey(const QSslKey &key);
    QSslConfiguration sslConfiguration() const;
    void setSslConfiguration(const QSslConfiguration &configuration);
    QSslCertificate peerCertificate() const;
    QList<QSslCertificate> peerCertificateChain() const;
    QSslCipher sessionCipher() const;
    QSsl::SslProtocol sessionProtocol() const;
    QList<QSslError> sslHandshakeErrors() const;
    qint64 encryptedBytesAvailable() const;
    qint64 encryptedBytesToWrite() const;

    void ignoreSslErrors(const QList<QSslError> &errors);
    bool waitForEncrypted(int msecs = 30000);

    void disconnectFromHost() override;
    qint64 bytesToWrite() const override;
    void close() override;

    static bool supportsSsl();
    static long sslLibraryVersionNumber();
    static QString sslLibraryVersionString();
    static long sslLibraryBuildVersionNumber();
    static QString sslLibraryBuildVersionString();

public slots:
    void startClientEncryption();
    void startServerEncryption();
    void ignoreSslErrors();

signals:
    /*!
        This signal is emitted when the handshake has finished and the peer has
        been verified. Data can flow from this point.

        \sa isEncrypted()
    */
    void encrypted() { emitSignal(&QSslSocket::encrypted); }
    /*!
        This signal is emitted once per verification problem, with \a error,
        before sslErrors().
    */
    void peerVerifyError(const QSslError &error) { emitSignal(&QSslSocket::peerVerifyError, error); }
    /*!
        This signal is emitted after the handshake if verification found
        \a errors. Unless a connected slot calls ignoreSslErrors(), the
        connection is closed when this signal returns.
    */
    void sslErrors(const QList<QSslError> &errors) { emitSignal(&QSslSocket::sslErrors, errors); }
    /*!
        This signal is emitted when the mode changes to \a mode.
    */
    void modeChanged(QSslSocket::SslMode mode) { emitSignal(&QSslSocket::modeChanged, mode); }
    /*!
        This signal is emitted when \a totalBytes of encrypted data have reached
        the operating system.
    */
    void encryptedBytesWritten(qint64 totalBytes) { emitSignal(&QSslSocket::encryptedBytesWritten, totalBytes); }

protected:
    qint64 writeData(const char *data, qint64 maxSize) override;
    void receivedRaw(const char *data, qint64 size) override;
    void socketBytesWritten(qint64 size) override;
    void remoteClosed() override;
    void connectionLost(int error) override;

private:
    struct Private;
    void beginHandshake(SslMode mode);
    void drive();
    void flushOut(qint64 plainBytes);
    void encryptPending();
    void finishHandshake();
    void failHandshake(SocketError error, const QString &text);
    void resetState();

    std::unique_ptr<Private> d;
};
