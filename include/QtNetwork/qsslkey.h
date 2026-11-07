// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtNetwork/qssl.h>

class QIODevice;

class QSslKey {
public:
    QSslKey() = default;
    QSslKey(const QByteArray &encoded, QSsl::KeyAlgorithm algorithm, QSsl::EncodingFormat format = QSsl::Pem,
            QSsl::KeyType type = QSsl::PrivateKey, const QByteArray &passPhrase = QByteArray());
    QSslKey(QIODevice *device, QSsl::KeyAlgorithm algorithm, QSsl::EncodingFormat format = QSsl::Pem,
            QSsl::KeyType type = QSsl::PrivateKey, const QByteArray &passPhrase = QByteArray());

    bool isNull() const;
    void clear();
    int length() const;
    QSsl::KeyType type() const;
    QSsl::KeyAlgorithm algorithm() const;
    QByteArray toPem() const;
    QByteArray toDer() const;

    friend bool operator==(const QSslKey &a, const QSslKey &b) { return a.m_type == b.m_type && a.m_der == b.m_der; }
    friend bool operator!=(const QSslKey &a, const QSslKey &b) { return !(a == b); }

private:
    QByteArray m_der; // unencrypted: PKCS#8 for private keys, SPKI for public keys
    QSsl::KeyType m_type = QSsl::PrivateKey;
    QSsl::KeyAlgorithm m_algorithm = QSsl::Opaque;
    int m_length = -1;
};
