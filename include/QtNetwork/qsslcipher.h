// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtNetwork/qssl.h>

class QSslCipher {
public:
    QSslCipher() = default;
    QSslCipher(const QString &name, int usedBits, int supportedBits, const QString &protocolString,
               QSsl::SslProtocol protocol);

    bool isNull() const;
    QString name() const;
    int usedBits() const;
    int supportedBits() const;
    QString protocolString() const;
    QSsl::SslProtocol protocol() const;

    friend bool operator==(const QSslCipher &a, const QSslCipher &b) {
        return a.m_name == b.m_name && a.m_protocolString == b.m_protocolString;
    }
    friend bool operator!=(const QSslCipher &a, const QSslCipher &b) { return !(a == b); }

private:
    QString m_name;
    int m_usedBits = 0;
    int m_supportedBits = 0;
    QString m_protocolString;
    QSsl::SslProtocol m_protocol = QSsl::UnknownProtocol;
};
