// MIT License - see LICENSE
#include <QtCore/qfile.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qmutex.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslerror.h>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <memory>

namespace {

struct X509Free {
    void operator()(X509 *p) const { X509_free(p); }
};
struct BioFree {
    void operator()(BIO *p) const { BIO_free(p); }
};
struct KeyFree {
    void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
using X509Ptr = std::unique_ptr<X509, X509Free>;
using BioPtr = std::unique_ptr<BIO, BioFree>;
using KeyPtr = std::unique_ptr<EVP_PKEY, KeyFree>;

BioPtr memoryBio(const QByteArray &data) {
    return BioPtr(BIO_new_mem_buf(data.constData(), static_cast<int>(data.size())));
}

X509Ptr parseDer(const QByteArray &der) {
    if (der.isEmpty())
        return nullptr;
    const auto *p = reinterpret_cast<const unsigned char *>(der.constData());
    return X509Ptr(d2i_X509(nullptr, &p, static_cast<long>(der.size())));
}

QByteArray toDerBytes(X509 *cert) {
    const int size = i2d_X509(cert, nullptr);
    if (size <= 0)
        return QByteArray();
    QByteArray out(size, '\0');
    auto *p = reinterpret_cast<unsigned char *>(out.data());
    i2d_X509(cert, &p);
    return out;
}

QByteArray bioContents(BIO *bio) {
    char *data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    return size > 0 ? QByteArray(data, size) : QByteArray();
}

QString asn1Text(const ASN1_STRING *value) {
    unsigned char *utf8 = nullptr;
    const int size = ASN1_STRING_to_UTF8(&utf8, value);
    if (size < 0)
        return QString();
    QString out = QString::fromUtf8(reinterpret_cast<const char *>(utf8), size);
    OPENSSL_free(utf8);
    return out;
}

int infoNid(QSslCertificate::SubjectInfo info) {
    switch (info) {
    case QSslCertificate::Organization:
        return NID_organizationName;
    case QSslCertificate::CommonName:
        return NID_commonName;
    case QSslCertificate::LocalityName:
        return NID_localityName;
    case QSslCertificate::OrganizationalUnitName:
        return NID_organizationalUnitName;
    case QSslCertificate::CountryName:
        return NID_countryName;
    case QSslCertificate::StateOrProvinceName:
        return NID_stateOrProvinceName;
    case QSslCertificate::DistinguishedNameQualifier:
        return NID_dnQualifier;
    case QSslCertificate::SerialNumber:
        return NID_serialNumber;
    case QSslCertificate::EmailAddress:
        return NID_pkcs9_emailAddress;
    }
    return NID_undef;
}

QStringList nameEntries(X509_NAME *name, QSslCertificate::SubjectInfo info) {
    QStringList out;
    int index = -1;
    while ((index = X509_NAME_get_index_by_NID(name, infoNid(info), index)) >= 0)
        out.append(asn1Text(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(name, index))));
    return out;
}

QString displayName(X509_NAME *name) {
    for (auto info :
         {QSslCertificate::CommonName, QSslCertificate::Organization, QSslCertificate::OrganizationalUnitName}) {
        const QStringList values = nameEntries(name, info);
        if (!values.isEmpty())
            return values.first();
    }
    return QString();
}

QDateTime asn1Time(const ASN1_TIME *value) {
    struct tm t = {};
    if (!value || ASN1_TIME_to_tm(value, &t) != 1)
        return QDateTime();
    return QDateTime(QDate(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday), QTime(t.tm_hour, t.tm_min, t.tm_sec), Qt::UTC);
}

QByteArray readDevice(QIODevice *device) { return device ? device->readAll() : QByteArray(); }

int passwordCallback(char *buffer, int size, int, void *user) {
    const auto *pass = static_cast<const QByteArray *>(user);
    const int n = static_cast<int>(qMin<qsizetype>(pass->size(), size));
    std::copy(pass->constData(), pass->constData() + n, buffer);
    return n;
}

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
bool QSslCertificate::isSelfSigned() const {
    const X509Ptr x = parseDer(m_der);
    return x && X509_check_issued(x.get(), x.get()) == X509_V_OK;
}

/*!
    Returns the X.509 version, such as \c 3.
*/
QByteArray QSslCertificate::version() const {
    const X509Ptr x = parseDer(m_der);
    return x ? QByteArray::number(X509_get_version(x.get()) + 1) : QByteArray();
}

/*!
    Returns the serial number as colon-separated hex.
*/
QByteArray QSslCertificate::serialNumber() const {
    const X509Ptr x = parseDer(m_der);
    if (!x)
        return QByteArray();
    const ASN1_INTEGER *serial = X509_get0_serialNumber(x.get());
    const QByteArray raw(reinterpret_cast<const char *>(ASN1_STRING_get0_data(serial)), ASN1_STRING_length(serial));
    return raw.toHex(':');
}

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
    const X509Ptr x = parseDer(m_der);
    return x ? nameEntries(X509_get_issuer_name(x.get()), info) : QStringList();
}

/*!
    Returns the values of the subject's \a info field.

    \sa issuerInfo()
*/
QStringList QSslCertificate::subjectInfo(SubjectInfo info) const {
    const X509Ptr x = parseDer(m_der);
    return x ? nameEntries(X509_get_subject_name(x.get()), info) : QStringList();
}

/*!
    Returns a name for showing the issuer.

    \sa subjectDisplayName()
*/
QString QSslCertificate::issuerDisplayName() const {
    const X509Ptr x = parseDer(m_der);
    return x ? displayName(X509_get_issuer_name(x.get())) : QString();
}

/*!
    Returns a name for showing the subject.

    \sa issuerDisplayName()
*/
QString QSslCertificate::subjectDisplayName() const {
    const X509Ptr x = parseDer(m_der);
    return x ? displayName(X509_get_subject_name(x.get())) : QString();
}

/*!
    Returns the alternative names the certificate is valid for.
*/
QMultiMap<QSsl::AlternativeNameEntryType, QString> QSslCertificate::subjectAlternativeNames() const {
    QMultiMap<QSsl::AlternativeNameEntryType, QString> out;
    const X509Ptr x = parseDer(m_der);
    if (!x)
        return out;
    auto *names = static_cast<GENERAL_NAMES *>(X509_get_ext_d2i(x.get(), NID_subject_alt_name, nullptr, nullptr));
    for (int i = 0; names && i < sk_GENERAL_NAME_num(names); ++i) {
        const GENERAL_NAME *n = sk_GENERAL_NAME_value(names, i);
        if (n->type == GEN_DNS) {
            out.insert(QSsl::DnsEntry, asn1Text(n->d.dNSName));
        } else if (n->type == GEN_EMAIL) {
            out.insert(QSsl::EmailEntry, asn1Text(n->d.rfc822Name));
        } else if (n->type == GEN_IPADD) {
            const int size = ASN1_STRING_length(n->d.iPAddress);
            const unsigned char *data = ASN1_STRING_get0_data(n->d.iPAddress);
            if (size == 4)
                out.insert(QSsl::IpAddressEntry, QHostAddress(quint32(data[0]) << 24 | quint32(data[1]) << 16 |
                                                              quint32(data[2]) << 8 | data[3])
                                                     .toString());
            else if (size == 16)
                out.insert(QSsl::IpAddressEntry, QHostAddress(data).toString());
        }
    }
    if (names)
        GENERAL_NAMES_free(names);
    return out;
}

/*!
    Returns the time from which the certificate is valid, in UTC.

    \sa expiryDate()
*/
QDateTime QSslCertificate::effectiveDate() const {
    const X509Ptr x = parseDer(m_der);
    return x ? asn1Time(X509_get0_notBefore(x.get())) : QDateTime();
}

/*!
    Returns the time at which the certificate stops being valid, in UTC.

    \sa effectiveDate()
*/
QDateTime QSslCertificate::expiryDate() const {
    const X509Ptr x = parseDer(m_der);
    return x ? asn1Time(X509_get0_notAfter(x.get())) : QDateTime();
}

/*!
    Returns the certificate in PEM format.

    \sa toDer()
*/
QByteArray QSslCertificate::toPem() const {
    const X509Ptr x = parseDer(m_der);
    if (!x)
        return QByteArray();
    const BioPtr bio(BIO_new(BIO_s_mem()));
    PEM_write_bio_X509(bio.get(), x.get());
    return bioContents(bio.get());
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
    if (format == QSsl::Der) {
        if (parseDer(data)) {
            QSslCertificate c;
            c.m_der = data;
            out.append(c);
        }
        return out;
    }
    const BioPtr bio = memoryBio(data);
    while (X509Ptr x = X509Ptr(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr))) {
        QSslCertificate c;
        c.m_der = toDerBytes(x.get());
        out.append(c);
    }
    ERR_clear_error();
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
    unencrypted in memory after it is loaded. There is no \c handle().
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
    const BioPtr bio = memoryBio(encoded);
    if (!bio || encoded.isEmpty())
        return;
    QByteArray pass = passPhrase;
    KeyPtr key;
    if (format == QSsl::Pem) {
        key.reset(type == QSsl::PrivateKey ? PEM_read_bio_PrivateKey(bio.get(), nullptr, passwordCallback, &pass)
                                           : PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
    } else {
        key.reset(type == QSsl::PrivateKey ? d2i_PrivateKey_bio(bio.get(), nullptr)
                                           : d2i_PUBKEY_bio(bio.get(), nullptr));
    }
    ERR_clear_error();
    if (!key)
        return;

    QSsl::KeyAlgorithm actual = QSsl::Opaque;
    switch (EVP_PKEY_base_id(key.get())) {
    case EVP_PKEY_RSA:
        actual = QSsl::Rsa;
        break;
    case EVP_PKEY_DSA:
        actual = QSsl::Dsa;
        break;
    case EVP_PKEY_EC:
        actual = QSsl::Ec;
        break;
    case EVP_PKEY_DH:
        actual = QSsl::Dh;
        break;
    default:
        break;
    }
    if (algorithm != QSsl::Opaque && algorithm != actual)
        return;

    const BioPtr out(BIO_new(BIO_s_mem()));
    const int ok = type == QSsl::PrivateKey ? i2d_PKCS8PrivateKeyInfo_bio(out.get(), key.get())
                                            : i2d_PUBKEY_bio(out.get(), key.get());
    if (ok != 1)
        return;
    m_der = bioContents(out.get());
    m_type = type;
    m_algorithm = actual;
    m_length = EVP_PKEY_bits(key.get());
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
QByteArray QSslKey::toPem() const {
    if (isNull())
        return QByteArray();
    const char *label = m_type == QSsl::PrivateKey ? "PRIVATE KEY" : "PUBLIC KEY";
    const BioPtr out(BIO_new(BIO_s_mem()));
    PEM_write_bio(out.get(), label, "", reinterpret_cast<const unsigned char *>(m_der.constData()),
                  static_cast<long>(m_der.size()));
    return bioContents(out.get());
}

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
    not exist. usesSystemCaCertificates() is an Nt addition. Cipher selection,
    session tickets, OCSP and DTLS settings are absent.
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
           a.m_useSystemCas == b.m_useSystemCas && a.m_alpn == b.m_alpn;
}
