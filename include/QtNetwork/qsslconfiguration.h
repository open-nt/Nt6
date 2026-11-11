// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qvariant.h>
#include <QtNetwork/qssl.h>
#include <QtNetwork/qsslcertificate.h>
#include <QtNetwork/qsslcipher.h>
#include <QtNetwork/qsslkey.h>
#include <QtNetwork/qsslsocket.h>

class QSslConfiguration {
public:
    QSslConfiguration();

    bool isNull() const;
    QSsl::SslProtocol protocol() const;
    void setProtocol(QSsl::SslProtocol protocol);
    QSslSocket::PeerVerifyMode peerVerifyMode() const;
    void setPeerVerifyMode(QSslSocket::PeerVerifyMode mode);
    int peerVerifyDepth() const;
    void setPeerVerifyDepth(int depth);
    QSslCertificate localCertificate() const;
    void setLocalCertificate(const QSslCertificate &certificate);
    QList<QSslCertificate> localCertificateChain() const;
    void setLocalCertificateChain(const QList<QSslCertificate> &localChain);
    QSslKey privateKey() const;
    void setPrivateKey(const QSslKey &key);
    QList<QSslCertificate> caCertificates() const;
    void setCaCertificates(const QList<QSslCertificate> &certificates);
    void addCaCertificate(const QSslCertificate &certificate);
    void addCaCertificates(const QList<QSslCertificate> &certificates);
    bool usesSystemCaCertificates() const;
    QList<QByteArray> allowedNextProtocols() const;
    void setAllowedNextProtocols(const QList<QByteArray> &protocols);
    QByteArray nextNegotiatedProtocol() const;
    QSslCertificate peerCertificate() const;
    QList<QSslCertificate> peerCertificateChain() const;
    QSslCipher sessionCipher() const;

    QList<QSslCipher> ciphers() const;
    void setCiphers(const QList<QSslCipher> &ciphers);
    void setCiphers(const QString &ciphers);
    static QList<QSslCipher> supportedCiphers();

    QMap<QByteArray, QVariant> backendConfiguration() const;
    void setBackendConfigurationOption(const QByteArray &name, const QVariant &value);
    void setBackendConfiguration(const QMap<QByteArray, QVariant> &backendConfiguration = QMap<QByteArray, QVariant>());
    QSsl::SslProtocol sessionProtocol() const;

    static QSslConfiguration defaultConfiguration();
    static void setDefaultConfiguration(const QSslConfiguration &configuration);

    friend bool operator==(const QSslConfiguration &a, const QSslConfiguration &b);
    friend bool operator!=(const QSslConfiguration &a, const QSslConfiguration &b) { return !(a == b); }

private:
    friend class QSslSocket;

    QSsl::SslProtocol m_protocol = QSsl::SecureProtocols;
    QSslSocket::PeerVerifyMode m_verifyMode = QSslSocket::AutoVerifyPeer;
    int m_verifyDepth = 0;
    QList<QSslCertificate> m_localChain;
    QSslKey m_key;
    QList<QSslCertificate> m_caCertificates;
    bool m_useSystemCas = true;
    QList<QByteArray> m_alpn;
    QList<QSslCipher> m_ciphers;
    bool m_ciphersSet = false; // setCiphers() was called: only m_ciphers may be used, even if it is empty
    QMap<QByteArray, QVariant> m_backend;
    QByteArray m_negotiatedAlpn;
    QList<QSslCertificate> m_peerChain;
    QSslCipher m_cipher;
    QSsl::SslProtocol m_sessionProtocol = QSsl::UnknownProtocol;
};
