// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qsslcertificate.h>

class QSslError {
public:
    enum SslError {
        NoError,
        UnableToGetIssuerCertificate,
        UnableToDecryptCertificateSignature,
        UnableToDecodeIssuerPublicKey,
        CertificateSignatureFailed,
        CertificateNotYetValid,
        CertificateExpired,
        InvalidNotBeforeField,
        InvalidNotAfterField,
        SelfSignedCertificate,
        SelfSignedCertificateInChain,
        UnableToGetLocalIssuerCertificate,
        UnableToVerifyFirstCertificate,
        CertificateRevoked,
        InvalidCaCertificate,
        PathLengthExceeded,
        InvalidPurpose,
        CertificateUntrusted,
        CertificateRejected,
        SubjectIssuerMismatch,
        AuthorityIssuerSerialNumberMismatch,
        NoPeerCertificate,
        HostNameMismatch,
        NoSslSupport,
        CertificateBlacklisted,
        UnspecifiedError = -1
    };

    QSslError() = default;
    explicit QSslError(SslError error);
    QSslError(SslError error, const QSslCertificate &certificate);

    SslError error() const;
    QString errorString() const;
    QSslCertificate certificate() const;

    friend bool operator==(const QSslError &a, const QSslError &b) { return a.m_error == b.m_error && a.m_certificate == b.m_certificate; }
    friend bool operator!=(const QSslError &a, const QSslError &b) { return !(a == b); }

private:
    SslError m_error = NoError;
    QSslCertificate m_certificate;
};
