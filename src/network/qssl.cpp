// MIT License - see LICENSE
#include <QtCore/qfile.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qmutex.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslerror.h>

#include "qtlsbackend_p.h"

#include <memory>

namespace {

using NameList = QList<std::pair<QSslCertificate::SubjectInfo, QString>>;

QStringList nameEntries(const NameList &name, QSslCertificate::SubjectInfo info) {
    QStringList out;
    for (const auto &entry : name) {
        if (entry.first == info)
            out.append(entry.second);
    }
    return out;
}

QString displayName(const NameList &name) {
    for (auto info :
         {QSslCertificate::CommonName, QSslCertificate::Organization, QSslCertificate::OrganizationalUnitName}) {
        const QStringList values = nameEntries(name, info);
        if (!values.isEmpty())
            return values.first();
    }
    return QString();
}

// What the TLS library reads from a certificate; empty if it is not one.
QtPrivate::Tls::CertificateInfo certificateInfo(const QByteArray &der) {
    QtPrivate::Tls::CertificateInfo info;
    if (!der.isEmpty())
        (void)QtPrivate::Tls::readCertificate(der, &info);
    return info;
}

QByteArray readDevice(QIODevice *device) { return device ? device->readAll() : QByteArray(); }

QMutex g_defaultLock;
QSslConfiguration *g_default = nullptr;

} // namespace

/*!
    \namespace QSsl
    \brief The QSsl namespace holds enums shared by the TLS classes.

    \differences
    Only the TLS 1.2 and 1.3 protocols exist: older versions are insecure and
    are deliberately not offered. There is no \c SslOption enum.
    \enddifferences
*/

/*!
    \enum QSsl::KeyType

    Selects a kind of key.

    \value PrivateKey A private key.
    \value PublicKey A public key.
*/

/*!
    \enum QSsl::EncodingFormat

    Selects an encoding for certificates and keys.

    \value Pem Base64 text, with header lines.
    \value Der Binary.
*/

/*!
    \enum QSsl::KeyAlgorithm

    Identifies a key's algorithm.

    \value Opaque The algorithm is not known.
    \value Rsa RSA.
    \value Dsa DSA.
    \value Ec Elliptic curve.
    \value Dh Diffie-Hellman.
*/

/*!
    \enum QSsl::AlternativeNameEntryType

    Identifies an entry in a certificate's alternative names.

    \value EmailEntry An email address.
    \value DnsEntry A DNS host name.
    \value IpAddressEntry An IP address.
*/

/*!
    \enum QSsl::SslProtocol

    Selects TLS versions.

    \value TlsV1_2 TLS 1.2 only.
    \value AnyProtocol Every supported version.
    \value SecureProtocols Every version considered secure. This is the default.
    \value TlsV1_2OrLater TLS 1.2 or newer.
    \value TlsV1_3 TLS 1.3 only.
    \value TlsV1_3OrLater TLS 1.3 or newer.
    \value UnknownProtocol The version is not known.
*/

/*!
    \class QSslCertificate
    \brief The QSslCertificate class holds an X.509 certificate.

    \code
    const auto certificates = QSslCertificate::fromPath("server.pem");
    qDebug() << certificates.first().subjectDisplayName();
    \endcode

    \differences
    There is no \c verify(), \c isBlacklisted(), \c extensions(), \c publicKey(),
    \c toText() or PKCS#12 import. fromPath() takes a single file, not a
    wildcard pattern.
    \enddifferences

    \sa QSslKey, QSslConfiguration
*/

/*!
    \enum QSslCertificate::SubjectInfo

    Selects a field of a certificate's subject or issuer.

    \value Organization The organization.
    \value CommonName The common name.
    \value LocalityName The locality.
    \value OrganizationalUnitName The organizational unit.
    \value CountryName The country.
    \value StateOrProvinceName The state or province.
    \value DistinguishedNameQualifier The distinguished name qualifier.
    \value SerialNumber The subject serial number.
    \value EmailAddress The email address.
*/

/*!
    Constructs a certificate by parsing \a data in the given \a format. If
    \a data is empty or invalid, the certificate is null.

    \sa isNull()
*/
QSslCertificate::QSslCertificate(const QByteArray &data, QSsl::EncodingFormat format) {
    const QList<QSslCertificate> parsed = fromData(data, format);
    if (!parsed.isEmpty())
        m_der = parsed.first().m_der;
}

/*!
    \overload

    Constructs a certificate from the contents of \a device, in the given
    \a format.
*/
QSslCertificate::QSslCertificate(QIODevice *device, QSsl::EncodingFormat format)
    : QSslCertificate(readDevice(device), format) {}

/*!
    Returns \c true if no certificate is held.
*/
bool QSslCertificate::isNull() const { return m_der.isEmpty(); }

/*!
    Makes the certificate null.
*/
void QSslCertificate::clear() { m_der.clear(); }

/*!
    Returns \c true if the certificate is its own issuer.
*/
bool QSslCertificate::isSelfSigned() const { return certificateInfo(m_der).selfSigned; }

/*!
    Returns the X.509 version, such as \c 3.
*/
QByteArray QSslCertificate::version() const {
    const int version = certificateInfo(m_der).version;
    return version > 0 ? QByteArray::number(version) : QByteArray();
}

/*!
    Returns the serial number as colon-separated hex.
*/
QByteArray QSslCertificate::serialNumber() const { return certificateInfo(m_der).serialNumber.toHex(':'); }

/*!
    Returns the hash of the certificate, computed with \a algorithm.
*/
QByteArray QSslCertificate::digest(QCryptographicHash::Algorithm algorithm) const {
    return QCryptographicHash::hash(m_der, algorithm);
}

/*!
    Returns the values of the issuer's \a info field.

    \sa subjectInfo()
*/
QStringList QSslCertificate::issuerInfo(SubjectInfo info) const {
    return nameEntries(certificateInfo(m_der).issuer, info);
}

/*!
    Returns the values of the subject's \a info field.

    \sa issuerInfo()
*/
QStringList QSslCertificate::subjectInfo(SubjectInfo info) const {
    return nameEntries(certificateInfo(m_der).subject, info);
}

/*!
    Returns a name for showing the issuer.

    \sa subjectDisplayName()
*/
QString QSslCertificate::issuerDisplayName() const { return displayName(certificateInfo(m_der).issuer); }

/*!
    Returns a name for showing the subject.

    \sa issuerDisplayName()
*/
QString QSslCertificate::subjectDisplayName() const { return displayName(certificateInfo(m_der).subject); }

/*!
    Returns the alternative names the certificate is valid for.
*/
QMultiMap<QSsl::AlternativeNameEntryType, QString> QSslCertificate::subjectAlternativeNames() const {
    return certificateInfo(m_der).alternativeNames;
}

/*!
    Returns the time from which the certificate is valid, in UTC.

    \sa expiryDate()
*/
QDateTime QSslCertificate::effectiveDate() const { return certificateInfo(m_der).notBefore; }

/*!
    Returns the time at which the certificate stops being valid, in UTC.

    \sa effectiveDate()
*/
QDateTime QSslCertificate::expiryDate() const { return certificateInfo(m_der).notAfter; }

/*!
    Returns the certificate in PEM format.

    \sa toDer()
*/
QByteArray QSslCertificate::toPem() const {
    return m_der.isEmpty() ? QByteArray() : QtPrivate::Tls::certificateToPem(m_der);
}

/*!
    Returns the certificate in DER format.

    \sa toPem()
*/
QByteArray QSslCertificate::toDer() const { return m_der; }

/*!
    Returns the certificates in the file \a path, in the given \a format.

    \sa fromData()
*/
QList<QSslCertificate> QSslCertificate::fromPath(const QString &path, QSsl::EncodingFormat format) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? fromData(file.readAll(), format) : QList<QSslCertificate>();
}

/*!
    Returns the certificates in \a data, in the given \a format. PEM data may
    hold several.

    \sa fromPath()
*/
QList<QSslCertificate> QSslCertificate::fromData(const QByteArray &data, QSsl::EncodingFormat format) {
    QList<QSslCertificate> out;
    if (data.isEmpty())
        return out;
    QList<QByteArray> encodings;
    if (format == QSsl::Der) {
        if (QtPrivate::Tls::readCertificate(data, nullptr))
            encodings.append(data);
    } else {
        encodings = QtPrivate::Tls::certificatesFromPem(data);
    }
    for (const QByteArray &der : encodings) {
        QSslCertificate c;
        c.m_der = der;
        out.append(c);
    }
    return out;
}

/*!
    Returns the certificates read from \a device, in the given \a format.
*/
QList<QSslCertificate> QSslCertificate::fromDevice(QIODevice *device, QSsl::EncodingFormat format) {
    return fromData(readDevice(device), format);
}

// Hashes the DER form, so equal certificates hash equally.
std::size_t qHash(const QSslCertificate &certificate, std::size_t seed) noexcept {
    return qHash(certificate.toDer(), seed);
}

/*!
    \class QSslKey
    \brief The QSslKey class holds a public or private key.

    \code
    QFile file("server.key");
    file.open(QIODevice::ReadOnly);
    QSslKey key(file.readAll(), QSsl::Ec);
    \endcode

    \differences
    toPem() and toDer() have no pass phrase argument, and the key is held
    unencrypted in memory after it is loaded. There is no \c handle(). On
    Zephyr, with Mbed TLS, a private key is held in the form of its algorithm
    (PKCS #1 for RSA, SEC 1 for an elliptic curve) and not as PKCS #8, so
    toDer() and toPem() give that form, and only RSA and elliptic curve keys
    are read.
    \enddifferences

    \sa QSslCertificate, QSslConfiguration
*/

/*!
    \fn QSslKey::QSslKey()

    Constructs a null key.
*/

/*!
    Constructs a key by parsing \a encoded, which is in \a format and holds a
    key of the given \a type. \a passPhrase decrypts a protected key. The key is
    null if parsing fails, or if the key is not an \a algorithm key.

    \sa isNull()
*/
QSslKey::QSslKey(const QByteArray &encoded, QSsl::KeyAlgorithm algorithm, QSsl::EncodingFormat format,
                 QSsl::KeyType type, const QByteArray &passPhrase) {
    QtPrivate::Tls::KeyInfo info;
    if (encoded.isEmpty() || !QtPrivate::Tls::readKey(encoded, format, type, passPhrase, &info))
        return;
    if (algorithm != QSsl::Opaque && algorithm != info.algorithm)
        return;
    m_der = info.der;
    m_type = type;
    m_algorithm = info.algorithm;
    m_length = info.length;
}

/*!
    \overload

    Constructs a key from the contents of \a device, using \a algorithm,
    \a format, \a type and \a passPhrase as above.
*/
QSslKey::QSslKey(QIODevice *device, QSsl::KeyAlgorithm algorithm, QSsl::EncodingFormat format, QSsl::KeyType type,
                 const QByteArray &passPhrase)
    : QSslKey(readDevice(device), algorithm, format, type, passPhrase) {}

/*!
    Returns \c true if no key is held.
*/
bool QSslKey::isNull() const { return m_der.isEmpty(); }

/*!
    Makes the key null.
*/
void QSslKey::clear() { *this = QSslKey(); }

/*!
    Returns the key's length in bits, or \c -1 if the key is null.
*/
int QSslKey::length() const { return m_length; }

/*!
    Returns whether this is a private or public key.
*/
QSsl::KeyType QSslKey::type() const { return m_type; }

/*!
    Returns the key's algorithm.
*/
QSsl::KeyAlgorithm QSslKey::algorithm() const { return m_algorithm; }

/*!
    Returns the key in PEM format, unencrypted.

    \sa toDer()
*/
QByteArray QSslKey::toPem() const { return isNull() ? QByteArray() : QtPrivate::Tls::keyToPem(m_der, m_type); }

/*!
    Returns the key in DER format, unencrypted.

    \sa toPem()
*/
QByteArray QSslKey::toDer() const { return m_der; }

/*!
    \class QSslError
    \brief The QSslError class describes a problem found while verifying a peer.

    \sa QSslSocket::sslErrors()
*/

/*!
    \enum QSslError::SslError

    Identifies a verification problem.

    \value NoError No problem.
    \value UnableToGetIssuerCertificate The issuer's certificate is unavailable.
    \value UnableToDecryptCertificateSignature The signature could not be read.
    \value UnableToDecodeIssuerPublicKey The issuer's key could not be read.
    \value CertificateSignatureFailed The signature is invalid.
    \value CertificateNotYetValid The certificate is not valid yet.
    \value CertificateExpired The certificate has expired.
    \value InvalidNotBeforeField The start date is malformed.
    \value InvalidNotAfterField The expiry date is malformed.
    \value SelfSignedCertificate The certificate is self-signed and not trusted.
    \value SelfSignedCertificateInChain The chain ends in an untrusted self-signed certificate.
    \value UnableToGetLocalIssuerCertificate The issuer is not in the trusted set.
    \value UnableToVerifyFirstCertificate The first certificate could not be verified.
    \value CertificateRevoked The certificate was revoked.
    \value InvalidCaCertificate An issuer is not a valid CA.
    \value PathLengthExceeded The chain is longer than its CA allows.
    \value InvalidPurpose The certificate may not be used for this purpose.
    \value CertificateUntrusted The certificate is not trusted for this purpose.
    \value CertificateRejected The certificate is marked to reject this purpose.
    \value SubjectIssuerMismatch The issuer name does not match.
    \value AuthorityIssuerSerialNumberMismatch The issuer serial number does not match.
    \value NoPeerCertificate The peer sent no certificate.
    \value HostNameMismatch The certificate is not valid for this host name.
    \value NoSslSupport TLS is not available.
    \value CertificateBlacklisted The certificate is blacklisted.
    \value UnspecifiedError Another problem occurred.
*/

/*!
    \fn QSslError::QSslError()

    Constructs an object holding NoError.
*/

/*!
    Constructs an object holding \a error.
*/
QSslError::QSslError(SslError error) : m_error(error) {}

/*!
    Constructs an object holding \a error, caused by \a certificate.
*/
QSslError::QSslError(SslError error, const QSslCertificate &certificate) : m_error(error), m_certificate(certificate) {}

/*!
    Returns the kind of problem.
*/
QSslError::SslError QSslError::error() const { return m_error; }

/*!
    Returns a description of the problem.
*/
QString QSslError::errorString() const {
    switch (m_error) {
    case NoError:
        return QString("No error");
    case UnableToGetIssuerCertificate:
        return QString("The issuer certificate could not be found");
    case UnableToDecryptCertificateSignature:
        return QString("The certificate signature could not be decrypted");
    case UnableToDecodeIssuerPublicKey:
        return QString("The public key in the certificate could not be read");
    case CertificateSignatureFailed:
        return QString("The signature of the certificate is invalid");
    case CertificateNotYetValid:
        return QString("The certificate is not yet valid");
    case CertificateExpired:
        return QString("The certificate has expired");
    case InvalidNotBeforeField:
        return QString("The certificate's notBefore field contains an invalid time");
    case InvalidNotAfterField:
        return QString("The certificate's notAfter field contains an invalid time");
    case SelfSignedCertificate:
        return QString("The certificate is self-signed, and untrusted");
    case SelfSignedCertificateInChain:
        return QString("The root certificate of the certificate chain is self-signed, and untrusted");
    case UnableToGetLocalIssuerCertificate:
        return QString("The issuer certificate of a locally looked up certificate could not be found");
    case UnableToVerifyFirstCertificate:
        return QString("No certificates could be verified");
    case CertificateRevoked:
        return QString("The certificate has been revoked");
    case InvalidCaCertificate:
        return QString("One of the CA certificates is invalid");
    case PathLengthExceeded:
        return QString("The basicConstraints path length parameter has been exceeded");
    case InvalidPurpose:
        return QString("The supplied certificate is unsuitable for this purpose");
    case CertificateUntrusted:
        return QString("The root CA certificate is not trusted for this purpose");
    case CertificateRejected:
        return QString("The root CA certificate is marked to reject the specified purpose");
    case SubjectIssuerMismatch:
        return QString("The current candidate issuer certificate was rejected because its subject name did not match");
    case AuthorityIssuerSerialNumberMismatch:
        return QString("The candidate issuer certificate was rejected because its serial number did not match");
    case NoPeerCertificate:
        return QString("The peer did not present any certificate");
    case HostNameMismatch:
        return QString("The host name did not match any of the valid hosts for this certificate");
    case NoSslSupport:
        return QString("TLS is not supported");
    case CertificateBlacklisted:
        return QString("The peer certificate is blacklisted");
    case UnspecifiedError:
        break;
    }
    return QString("Unknown error");
}

/*!
    Returns the certificate that caused the problem, which may be null.
*/
QSslCertificate QSslError::certificate() const { return m_certificate; }

/*!
    \class QSslCipher
    \brief The QSslCipher class describes the cipher of a connection.

    \differences
    The key exchange, authentication and encryption method names are not
    available separately; use name().
    \enddifferences

    \sa QSslSocket::sessionCipher()
*/

/*!
    \fn QSslCipher::QSslCipher()

    Constructs a null cipher.
*/

/*!
    Constructs a cipher called \a name, using \a usedBits of \a supportedBits
    key bits, for the protocol \a protocol described by \a protocolString.
*/
QSslCipher::QSslCipher(const QString &name, int usedBits, int supportedBits, const QString &protocolString,
                       QSsl::SslProtocol protocol)
    : m_name(name), m_usedBits(usedBits), m_supportedBits(supportedBits), m_protocolString(protocolString),
      m_protocol(protocol) {}

/*!
    \overload

    Constructs the cipher called \a name, as OpenSSL names it, such as
    \c TLS_AES_256_GCM_SHA384 or \c ECDHE-RSA-AES256-GCM-SHA384. The cipher
    is null if this build of OpenSSL has none of that name.

    \sa QSslConfiguration::supportedCiphers()
*/
QSslCipher::QSslCipher(const QString &name) {
    for (const QSslCipher &cipher : QSslConfiguration::supportedCiphers()) {
        if (cipher.name() == name) {
            *this = cipher;
            return;
        }
    }
}

/*!
    \overload

    Constructs the cipher called \a name of the protocol \a protocol. The
    cipher is null if there is no such cipher for that protocol.
*/
QSslCipher::QSslCipher(const QString &name, QSsl::SslProtocol protocol) {
    for (const QSslCipher &cipher : QSslConfiguration::supportedCiphers()) {
        if (cipher.name() == name && cipher.protocol() == protocol) {
            *this = cipher;
            return;
        }
    }
}

/*!
    Returns \c true if this object describes no cipher.
*/
bool QSslCipher::isNull() const { return m_name.isEmpty(); }

/*!
    Returns the cipher's name.
*/
QString QSslCipher::name() const { return m_name; }

/*!
    Returns the number of key bits in use.
*/
int QSslCipher::usedBits() const { return m_usedBits; }

/*!
    Returns the number of key bits the cipher supports.
*/
int QSslCipher::supportedBits() const { return m_supportedBits; }

/*!
    Returns the protocol as text, such as \c TLSv1.3.
*/
QString QSslCipher::protocolString() const { return m_protocolString; }

/*!
    Returns the protocol.
*/
QSsl::SslProtocol QSslCipher::protocol() const { return m_protocol; }

/*!
    \class QSslConfiguration
    \brief The QSslConfiguration class holds the settings of a TLS connection.

    The default verifies peers against the system's trusted certificates and
    requires TLS 1.2 or newer. To trust a private CA alongside them:

    \code
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.addCaCertificates(QSslCertificate::fromPath("company-ca.pem"));
    socket.setSslConfiguration(config);
    \endcode

    Use setCaCertificates() instead to trust only the certificates you list.

    \differences
    caCertificates() lists only certificates you added: system ones are used by
    OpenSSL directly and are not enumerated, so \c systemCaCertificates() does
    not exist. usesSystemCaCertificates() is an Nt addition. A cipher or a
    backend option that OpenSSL does not accept makes the handshake fail,
    where Qt warns and goes on. The elliptic curves are set as the backend
    option \c Groups; \c setEllipticCurves() is absent, as are session
    tickets, OCSP, Diffie-Hellman parameters and DTLS settings. With Mbed TLS,
    on Zephyr, no backend option is accepted, and the system's certificate
    authorities are not used.
    \enddifferences

    \sa QSslSocket
*/

/*!
    Constructs a configuration with the default settings.

    \sa defaultConfiguration()
*/
QSslConfiguration::QSslConfiguration() = default;

/*!
    Returns \c true if every setting has its initial value.
*/
bool QSslConfiguration::isNull() const { return *this == QSslConfiguration(); }

/*!
    Returns the allowed TLS versions.

    \sa setProtocol()
*/
QSsl::SslProtocol QSslConfiguration::protocol() const { return m_protocol; }

/*!
    Sets the allowed TLS versions to \a protocol.

    \sa protocol()
*/
void QSslConfiguration::setProtocol(QSsl::SslProtocol protocol) { m_protocol = protocol; }

/*!
    Returns how the peer is verified.

    \sa setPeerVerifyMode()
*/
QSslSocket::PeerVerifyMode QSslConfiguration::peerVerifyMode() const { return m_verifyMode; }

/*!
    Sets how the peer is verified to \a mode.

    \sa peerVerifyMode()
*/
void QSslConfiguration::setPeerVerifyMode(QSslSocket::PeerVerifyMode mode) { m_verifyMode = mode; }

/*!
    Returns the longest certificate chain accepted; \c 0 means no limit.

    \sa setPeerVerifyDepth()
*/
int QSslConfiguration::peerVerifyDepth() const { return m_verifyDepth; }

/*!
    Sets the longest certificate chain accepted to \a depth.

    \sa peerVerifyDepth()
*/
void QSslConfiguration::setPeerVerifyDepth(int depth) { m_verifyDepth = depth; }

/*!
    Returns the certificate this side presents.

    \sa setLocalCertificate()
*/
QSslCertificate QSslConfiguration::localCertificate() const {
    return m_localChain.isEmpty() ? QSslCertificate() : m_localChain.first();
}

/*!
    Sets the certificate this side presents to \a certificate.

    \sa localCertificate(), setPrivateKey()
*/
void QSslConfiguration::setLocalCertificate(const QSslCertificate &certificate) {
    m_localChain.clear();
    if (!certificate.isNull())
        m_localChain.append(certificate);
}

/*!
    Returns the local certificate and any intermediate certificates.

    \sa setLocalCertificateChain()
*/
QList<QSslCertificate> QSslConfiguration::localCertificateChain() const { return m_localChain; }

/*!
    Sets the local certificate and intermediates to \a localChain, leaf first.

    \sa localCertificateChain()
*/
void QSslConfiguration::setLocalCertificateChain(const QList<QSslCertificate> &localChain) {
    m_localChain = localChain;
}

/*!
    Returns the private key of the local certificate.

    \sa setPrivateKey()
*/
QSslKey QSslConfiguration::privateKey() const { return m_key; }

/*!
    Sets the private key of the local certificate to \a key.

    \sa privateKey()
*/
void QSslConfiguration::setPrivateKey(const QSslKey &key) { m_key = key; }

/*!
    Returns the certificates added to the trusted set. System certificates are
    not listed.

    \sa setCaCertificates(), addCaCertificate()
*/
QList<QSslCertificate> QSslConfiguration::caCertificates() const { return m_caCertificates; }

/*!
    Trusts only \a certificates, and no system certificates.

    \sa addCaCertificates()
*/
void QSslConfiguration::setCaCertificates(const QList<QSslCertificate> &certificates) {
    m_caCertificates = certificates;
    m_useSystemCas = false;
}

/*!
    Trusts \a certificate as well as the current set.

    \sa setCaCertificates()
*/
void QSslConfiguration::addCaCertificate(const QSslCertificate &certificate) { m_caCertificates.append(certificate); }

/*!
    Trusts \a certificates as well as the current set.

    \sa setCaCertificates()
*/
void QSslConfiguration::addCaCertificates(const QList<QSslCertificate> &certificates) {
    m_caCertificates.append(certificates);
}

/*!
    Returns \c true if the system's trusted certificates are used.
*/
bool QSslConfiguration::usesSystemCaCertificates() const { return m_useSystemCas; }

/*!
    Returns the application protocols offered during the handshake.

    \sa setAllowedNextProtocols()
*/
QList<QByteArray> QSslConfiguration::allowedNextProtocols() const { return m_alpn; }

/*!
    Sets the application protocols offered during the handshake to
    \a protocols, most preferred first, using ALPN.

    \sa nextNegotiatedProtocol()
*/
void QSslConfiguration::setAllowedNextProtocols(const QList<QByteArray> &protocols) { m_alpn = protocols; }

/*!
    Returns the application protocol agreed with the peer. This is set on the
    configuration returned by QSslSocket::sslConfiguration().
*/
QByteArray QSslConfiguration::nextNegotiatedProtocol() const { return m_negotiatedAlpn; }

/*!
    Returns the peer's certificate. This is set on the configuration returned
    by QSslSocket::sslConfiguration().
*/
QSslCertificate QSslConfiguration::peerCertificate() const {
    return m_peerChain.isEmpty() ? QSslCertificate() : m_peerChain.first();
}

/*!
    Returns the peer's certificate chain, leaf first. This is set on the
    configuration returned by QSslSocket::sslConfiguration().
*/
QList<QSslCertificate> QSslConfiguration::peerCertificateChain() const { return m_peerChain; }

/*!
    Returns the cipher in use. This is set on the configuration returned by
    QSslSocket::sslConfiguration().
*/
QSslCipher QSslConfiguration::sessionCipher() const { return m_cipher; }

/*!
    Returns the ciphers that a connection with this configuration offers
    and accepts, in the order of preference: those given to setCiphers(),
    or, if it was never called, the ones that OpenSSL enables by default.

    \sa setCiphers(), supportedCiphers()
*/
QList<QSslCipher> QSslConfiguration::ciphers() const {
    return m_ciphersSet ? m_ciphers : QtPrivate::Tls::defaultCiphers();
}

/*!
    Restricts the connection to the ciphers in \a ciphers, in that order of
    preference. Ciphers that are not among supportedCiphers() are left out.
    Only these ciphers are
    offered and accepted: a handshake with a peer that has none of them
    fails, and so does one with an empty list. TLS 1.3 and the earlier
    versions have separate sets of ciphers, so a list without any cipher of
    TLS 1.3 rules that version out, and a list with only such ciphers rules
    out the earlier ones.

    \sa ciphers(), supportedCiphers()
*/
void QSslConfiguration::setCiphers(const QList<QSslCipher> &ciphers) {
    // Only what the TLS library itself lists goes in, under its own name and protocol: the names are later
    // given to the library, where a made-up name such as "ALL" could select many ciphers.
    const QList<QSslCipher> supported = supportedCiphers();
    m_ciphers.clear();
    for (const QSslCipher &cipher : ciphers) {
        for (const QSslCipher &known : supported) {
            if (known.name() == cipher.name() && !m_ciphers.contains(known))
                m_ciphers.append(known);
        }
    }
    m_ciphersSet = true;
}

/*!
    \overload

    Restricts the connection to the ciphers named in \a ciphers, a list of
    names separated by colons, as in
    \c{"TLS_AES_256_GCM_SHA384:ECDHE-RSA-AES256-GCM-SHA384"}. A name that
    this build of OpenSSL does not know is left out, which makes the list
    shorter, never wider; if no name is known, every handshake fails.
*/
void QSslConfiguration::setCiphers(const QString &ciphers) {
    const QList<QSslCipher> supported = supportedCiphers();
    QList<QSslCipher> list;
    for (const QString &name : ciphers.split(QChar(':'), Qt::SkipEmptyParts)) {
        for (const QSslCipher &cipher : supported) {
            if (cipher.name() == name.trimmed())
                list.append(cipher);
        }
    }
    setCiphers(list);
}

/*!
    Returns the ciphers that this build of OpenSSL can use: every cipher
    with encryption and authentication, of TLS 1.3 and of the earlier
    versions. Many of them are not enabled by default; see ciphers().

    \sa setCiphers()
*/
QList<QSslCipher> QSslConfiguration::supportedCiphers() { return QtPrivate::Tls::supportedCiphers(); }

/*!
    Returns the options that were set for the TLS library with
    setBackendConfigurationOption().

    \sa setBackendConfiguration()
*/
QMap<QByteArray, QVariant> QSslConfiguration::backendConfiguration() const { return m_backend; }

/*!
    Sets the option \a name of the TLS library to \a value. The options are
    those of OpenSSL's configuration files, as \c SSL_CONF_cmd() documents
    them: \c Groups (or \c Curves) for the key exchange groups,
    \c SignatureAlgorithms, \c MinProtocol, \c MaxProtocol, \c Options and
    others. \a value is given to OpenSSL as text.

    \code
    config.setBackendConfigurationOption("Groups", "P-384:P-521");
    \endcode

    The options are applied when a handshake starts, in the order of their
    names and after the other settings of the configuration, which they can
    therefore override. If OpenSSL does not accept an option or its value,
    the handshake fails with \c SslInvalidUserDataError: a restriction that
    cannot be applied is never silently dropped.

    \sa backendConfiguration(), setBackendConfiguration()
*/
void QSslConfiguration::setBackendConfigurationOption(const QByteArray &name, const QVariant &value) {
    m_backend.insert(name, value);
}

/*!
    Replaces all options of the TLS library with \a backendConfiguration.
    Without an argument, the options are removed.

    \sa setBackendConfigurationOption()
*/
void QSslConfiguration::setBackendConfiguration(const QMap<QByteArray, QVariant> &backendConfiguration) {
    m_backend = backendConfiguration;
}

/*!
    Returns the TLS version in use. This is set on the configuration returned
    by QSslSocket::sslConfiguration().
*/
QSsl::SslProtocol QSslConfiguration::sessionProtocol() const { return m_sessionProtocol; }

/*!
    Returns the configuration new sockets start with.

    \sa setDefaultConfiguration()
*/
QSslConfiguration QSslConfiguration::defaultConfiguration() {
    QMutexLocker lock(&g_defaultLock);
    return g_default ? *g_default : QSslConfiguration();
}

/*!
    Sets the configuration new sockets start with to \a configuration.

    \sa defaultConfiguration()
*/
void QSslConfiguration::setDefaultConfiguration(const QSslConfiguration &configuration) {
    QMutexLocker lock(&g_defaultLock);
    if (!g_default)
        g_default = new QSslConfiguration;
    *g_default = configuration;
}

// Compares the settings, not the state of a finished session.
bool operator==(const QSslConfiguration &a, const QSslConfiguration &b) {
    return a.m_protocol == b.m_protocol && a.m_verifyMode == b.m_verifyMode && a.m_verifyDepth == b.m_verifyDepth &&
           a.m_localChain == b.m_localChain && a.m_key == b.m_key && a.m_caCertificates == b.m_caCertificates &&
           a.m_useSystemCas == b.m_useSystemCas && a.m_alpn == b.m_alpn && a.m_ciphersSet == b.m_ciphersSet &&
           a.m_ciphers == b.m_ciphers && a.m_backend == b.m_backend;
}
