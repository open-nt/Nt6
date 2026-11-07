// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtNetwork/qssl.h>

class QIODevice;

class QSslCertificate {
public:
    enum SubjectInfo {
        Organization,
        CommonName,
        LocalityName,
        OrganizationalUnitName,
        CountryName,
        StateOrProvinceName,
        DistinguishedNameQualifier,
        SerialNumber,
        EmailAddress
    };

    explicit QSslCertificate(const QByteArray &data = QByteArray(), QSsl::EncodingFormat format = QSsl::Pem);
    explicit QSslCertificate(QIODevice *device, QSsl::EncodingFormat format = QSsl::Pem);

    bool isNull() const;
    void clear();
    bool isSelfSigned() const;
    QByteArray version() const;
    QByteArray serialNumber() const;
    QByteArray digest(QCryptographicHash::Algorithm algorithm = QCryptographicHash::Md5) const;
    QStringList issuerInfo(SubjectInfo info) const;
    QStringList subjectInfo(SubjectInfo info) const;
    QString issuerDisplayName() const;
    QString subjectDisplayName() const;
    QMultiMap<QSsl::AlternativeNameEntryType, QString> subjectAlternativeNames() const;
    QDateTime effectiveDate() const;
    QDateTime expiryDate() const;
    QByteArray toPem() const;
    QByteArray toDer() const;

    static QList<QSslCertificate> fromPath(const QString &path, QSsl::EncodingFormat format = QSsl::Pem);
    static QList<QSslCertificate> fromData(const QByteArray &data, QSsl::EncodingFormat format = QSsl::Pem);
    static QList<QSslCertificate> fromDevice(QIODevice *device, QSsl::EncodingFormat format = QSsl::Pem);

    friend bool operator==(const QSslCertificate &a, const QSslCertificate &b) { return a.m_der == b.m_der; }
    friend bool operator!=(const QSslCertificate &a, const QSslCertificate &b) { return a.m_der != b.m_der; }

private:
    QByteArray m_der;
};

std::size_t qHash(const QSslCertificate &certificate, std::size_t seed = 0) noexcept;
