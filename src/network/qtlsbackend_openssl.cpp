// MIT License - see LICENSE
// The TLS backend of Nt Network on OpenSSL. See qtlsbackend_p.h.
#include "qtlsbackend_p.h"

#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qsslsocket.h>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <memory>

namespace QtPrivate {
namespace Tls {

Session::~Session() = default;

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

// The size of a string in bytes. OpenSSL 4.1 deprecates the function that returns it as an int.
int asn1Size(const ASN1_STRING *value) {
#if OPENSSL_VERSION_NUMBER >= 0x40100000L
    return static_cast<int>(ASN1_STRING_get_length(value));
#else
    return ASN1_STRING_length(value);
#endif
}

// OpenSSL 4 returns the names of a certificate as const; before OpenSSL 3 the functions that read a name took it as
// non-const.
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
const X509_NAME *readableName(const X509_NAME *name) { return name; }
#else
X509_NAME *readableName(const X509_NAME *name) { return const_cast<X509_NAME *>(name); }
#endif

QDateTime asn1Time(const ASN1_TIME *value) {
    struct tm t = {};
    if (!value || ASN1_TIME_to_tm(value, &t) != 1)
        return QDateTime();
    return QDateTime(QDate(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday), QTime(t.tm_hour, t.tm_min, t.tm_sec), Qt::UTC);
}

int passwordCallback(char *buffer, int size, int, void *user) {
    const auto *pass = static_cast<const QByteArray *>(user);
    const int n = static_cast<int>(qMin<qsizetype>(pass->size(), size));
    std::copy(pass->constData(), pass->constData() + n, buffer);
    return n;
}

// The attributes of a name that QSslCertificate::SubjectInfo knows, in the order of the certificate.
QList<std::pair<QSslCertificate::SubjectInfo, QString>> nameList(const X509_NAME *name) {
    QList<std::pair<QSslCertificate::SubjectInfo, QString>> out;
    const int count = X509_NAME_entry_count(readableName(name));
    for (int i = 0; i < count; ++i) {
        const X509_NAME_ENTRY *entry = X509_NAME_get_entry(name, i);
        const int nid = OBJ_obj2nid(X509_NAME_ENTRY_get_object(entry));
        for (auto info : {QSslCertificate::Organization, QSslCertificate::CommonName, QSslCertificate::LocalityName,
                          QSslCertificate::OrganizationalUnitName, QSslCertificate::CountryName,
                          QSslCertificate::StateOrProvinceName, QSslCertificate::DistinguishedNameQualifier,
                          QSslCertificate::SerialNumber, QSslCertificate::EmailAddress}) {
            if (infoNid(info) == nid)
                out.append({info, asn1Text(X509_NAME_ENTRY_get_data(entry))});
        }
    }
    return out;
}

QSslError::SslError fromVerifyError(int code) {
    switch (code) {
    case X509_V_OK:
        return QSslError::NoError;
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        return QSslError::UnableToGetIssuerCertificate;
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
        return QSslError::UnableToDecryptCertificateSignature;
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
        return QSslError::UnableToDecodeIssuerPublicKey;
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
        return QSslError::CertificateSignatureFailed;
    case X509_V_ERR_CERT_NOT_YET_VALID:
        return QSslError::CertificateNotYetValid;
    case X509_V_ERR_CERT_HAS_EXPIRED:
        return QSslError::CertificateExpired;
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
        return QSslError::InvalidNotBeforeField;
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
        return QSslError::InvalidNotAfterField;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
        return QSslError::SelfSignedCertificate;
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        return QSslError::SelfSignedCertificateInChain;
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        return QSslError::UnableToGetLocalIssuerCertificate;
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        return QSslError::UnableToVerifyFirstCertificate;
    case X509_V_ERR_CERT_REVOKED:
        return QSslError::CertificateRevoked;
    case X509_V_ERR_INVALID_CA:
        return QSslError::InvalidCaCertificate;
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
        return QSslError::PathLengthExceeded;
    case X509_V_ERR_INVALID_PURPOSE:
        return QSslError::InvalidPurpose;
    case X509_V_ERR_CERT_UNTRUSTED:
        return QSslError::CertificateUntrusted;
    case X509_V_ERR_CERT_REJECTED:
        return QSslError::CertificateRejected;
    case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
        return QSslError::SubjectIssuerMismatch;
    case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
        return QSslError::AuthorityIssuerSerialNumberMismatch;
    case X509_V_ERR_HOSTNAME_MISMATCH:
    case X509_V_ERR_IP_ADDRESS_MISMATCH:
        return QSslError::HostNameMismatch;
    default:
        return QSslError::UnspecifiedError;
    }
}

QSslCertificate wrapCertificate(X509 *cert) {
    const int size = cert ? i2d_X509(cert, nullptr) : 0;
    if (size <= 0)
        return QSslCertificate();
    QByteArray der(size, '\0');
    auto *p = reinterpret_cast<unsigned char *>(der.data());
    i2d_X509(cert, &p);
    return QSslCertificate(der, QSsl::Der);
}

X509 *unwrapCertificate(const QSslCertificate &certificate) {
    const QByteArray der = certificate.toDer();
    const auto *p = reinterpret_cast<const unsigned char *>(der.constData());
    return d2i_X509(nullptr, &p, static_cast<long>(der.size()));
}

QString lastSslError(const char *fallback) {
    const unsigned long code = ERR_peek_last_error();
    ERR_clear_error();
    if (code == 0)
        return QString::fromUtf8(fallback);
    char text[256];
    ERR_error_string_n(code, text, sizeof text);
    return QString::fromUtf8(text);
}

QSsl::SslProtocol protocolFromVersion(int version) {
    if (version == TLS1_3_VERSION)
        return QSsl::TlsV1_3;
    if (version == TLS1_2_VERSION)
        return QSsl::TlsV1_2;
    return QSsl::UnknownProtocol;
}

QByteArray wireProtocols(const QList<QByteArray> &protocols) {
    QByteArray out;
    for (const QByteArray &p : protocols) {
        if (p.isEmpty() || p.size() > 255)
            continue;
        out.append(static_cast<char>(p.size()));
        out.append(p);
    }
    return out;
}

int exDataIndex() {
    static const int index = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
    return index;
}

// Records every failure and lets the handshake continue. The decision is made
// in finishHandshake(), before any application data can flow.
int verifyCallback(int ok, X509_STORE_CTX *store) {
    if (!ok) {
        auto *ssl = static_cast<SSL *>(X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx()));
        auto *found = static_cast<QList<QSslError> *>(SSL_get_ex_data(ssl, exDataIndex()));
        if (found) {
            const QSslError error(fromVerifyError(X509_STORE_CTX_get_error(store)),
                                  wrapCertificate(X509_STORE_CTX_get_current_cert(store)));
            if (!found->contains(error))
                found->append(error);
        }
    }
    return 1;
}

int alpnSelect(SSL *, const unsigned char **selected, unsigned char *selectedLength, const unsigned char *offered,
               unsigned int offeredLength, void *arg) {
    const auto *ours = static_cast<const QByteArray *>(arg);
    unsigned char *result = nullptr;
    const int rc =
        SSL_select_next_proto(&result, selectedLength, reinterpret_cast<const unsigned char *>(ours->constData()),
                              static_cast<unsigned int>(ours->size()), offered, offeredLength);
    if (rc != OPENSSL_NPN_NEGOTIATED)
        return SSL_TLSEXT_ERR_NOACK;
    *selected = result;
    return SSL_TLSEXT_ERR_OK;
}

QSsl::SslProtocol cipherProtocol(const SSL_CIPHER *cipher) {
    const char *version = SSL_CIPHER_get_version(cipher);
    const QByteArray v(version ? version : "");
    if (v == "TLSv1.3")
        return QSsl::TlsV1_3;
    if (v == "TLSv1.2")
        return QSsl::TlsV1_2;
    return QSsl::UnknownProtocol; // older suites, which TLS 1.2 can still negotiate
}

// The ciphers that a new context of OpenSSL has after its cipher list was set to list; all of TLS 1.3 come
// with every list.
QList<QSslCipher> listCiphers(const char *list) {
    QList<QSslCipher> result;
    SSL_CTX *ctx = SSL_CTX_new(TLS_method());
    if (!ctx)
        return result;
    if (!list || SSL_CTX_set_cipher_list(ctx, list) == 1) {
        const STACK_OF(SSL_CIPHER) *stack = SSL_CTX_get_ciphers(ctx);
        for (int i = 0; stack && i < sk_SSL_CIPHER_num(stack); ++i) {
            const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(stack, i);
            int supported = 0;
            const int used = SSL_CIPHER_get_bits(cipher, &supported);
            const char *version = SSL_CIPHER_get_version(cipher);
            result.append(QSslCipher(QString::fromUtf8(SSL_CIPHER_get_name(cipher)), used, supported,
                                     QString::fromUtf8(version ? version : ""), cipherProtocol(cipher)));
        }
    }
    SSL_CTX_free(ctx);
    return result;
}

class OpenSslSession : public Session {
public:
    ~OpenSslSession() override {
        if (ssl)
            SSL_free(ssl); // also frees the BIOs
        if (ctx)
            SSL_CTX_free(ctx);
    }

    void feed(const char *data, qint64 size) override { BIO_write(in, data, static_cast<int>(size)); }

    QByteArray takeOutput() override {
        QByteArray cipher;
        char chunk[16384];
        int n;
        while (BIO_ctrl_pending(out) > 0 && (n = BIO_read(out, chunk, sizeof chunk)) > 0)
            cipher.append(chunk, n);
        return cipher;
    }

    Status handshake(QString *error) override {
        ERR_clear_error();
        const int rc = SSL_do_handshake(ssl);
        if (rc == 1)
            return Done;
        const int e = SSL_get_error(ssl, rc);
        if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
            return Again;
        *error = lastSslError("The TLS handshake failed");
        return Failed;
    }

    Status read(QByteArray *plain, QString *error) override {
        char chunk[16384];
        for (;;) {
            ERR_clear_error();
            const int n = SSL_read(ssl, chunk, sizeof chunk);
            if (n > 0) {
                plain->append(chunk, n);
                continue;
            }
            const int e = SSL_get_error(ssl, n);
            if (e == SSL_ERROR_ZERO_RETURN)
                return Closed;
            if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
                return Again;
            *error = lastSslError("TLS protocol error");
            return Failed;
        }
    }

    qint64 write(const char *data, qint64 size, QString *error) override {
        qint64 done = 0;
        while (done < size) {
            const int chunk = static_cast<int>(qMin<qint64>(size - done, 1 << 24));
            ERR_clear_error();
            const int n = SSL_write(ssl, data + done, chunk);
            if (n <= 0) {
                *error = lastSslError("Could not encrypt data");
                return done > 0 ? done : -1;
            }
            done += n;
        }
        return done;
    }

    void shutdown() override {
        ERR_clear_error();
        SSL_shutdown(ssl);
    }

    QList<QSslCertificate> peerChain() const override {
        QList<QSslCertificate> chain;
        X509 *peer = SSL_get_peer_certificate(ssl);
        if (peer)
            chain.append(wrapCertificate(peer));
        if (STACK_OF(X509) *stack = SSL_get_peer_cert_chain(ssl)) {
            for (int i = 0; i < sk_X509_num(stack); ++i) {
                const QSslCertificate c = wrapCertificate(sk_X509_value(stack, i));
                if (!chain.contains(c))
                    chain.append(c);
            }
        }
        X509_free(peer);
        return chain;
    }

    QList<QSslError> verifyErrors() const override { return found; }

    QSslCipher cipher() const override {
        const SSL_CIPHER *current = SSL_get_current_cipher(ssl);
        if (!current)
            return QSslCipher();
        int supported = 0;
        const int used = SSL_CIPHER_get_bits(current, &supported);
        return QSslCipher(QString::fromUtf8(SSL_CIPHER_get_name(current)), used, supported,
                          QString::fromUtf8(SSL_get_version(ssl)), protocol());
    }

    QSsl::SslProtocol protocol() const override { return protocolFromVersion(SSL_version(ssl)); }

    QByteArray nextProtocol() const override {
        const unsigned char *alpn = nullptr;
        unsigned int alpnLength = 0;
        SSL_get0_alpn_selected(ssl, &alpn, &alpnLength);
        return alpn ? QByteArray(reinterpret_cast<const char *>(alpn), alpnLength) : QByteArray();
    }

    SSL_CTX *ctx = nullptr;
    SSL *ssl = nullptr;
    BIO *in = nullptr;  // network to OpenSSL
    BIO *out = nullptr; // OpenSSL to network
    QByteArray alpnWire;
    QList<QSslError> found; // reported by the verify callback
};

} // namespace

bool readCertificate(const QByteArray &der, CertificateInfo *info) {
    const X509Ptr x = parseDer(der);
    if (!x)
        return false;
    if (!info)
        return true;
    info->selfSigned = X509_check_issued(x.get(), x.get()) == X509_V_OK;
    info->version = static_cast<int>(X509_get_version(x.get())) + 1;
    const ASN1_INTEGER *serial = X509_get0_serialNumber(x.get());
    info->serialNumber = QByteArray(reinterpret_cast<const char *>(ASN1_STRING_get0_data(serial)), asn1Size(serial));
    info->issuer = nameList(X509_get_issuer_name(x.get()));
    info->subject = nameList(X509_get_subject_name(x.get()));
    info->notBefore = asn1Time(X509_get0_notBefore(x.get()));
    info->notAfter = asn1Time(X509_get0_notAfter(x.get()));

    auto *names = static_cast<GENERAL_NAMES *>(X509_get_ext_d2i(x.get(), NID_subject_alt_name, nullptr, nullptr));
    for (int i = 0; names && i < sk_GENERAL_NAME_num(names); ++i) {
        const GENERAL_NAME *n = sk_GENERAL_NAME_value(names, i);
        if (n->type == GEN_DNS) {
            info->alternativeNames.insert(QSsl::DnsEntry, asn1Text(n->d.dNSName));
        } else if (n->type == GEN_EMAIL) {
            info->alternativeNames.insert(QSsl::EmailEntry, asn1Text(n->d.rfc822Name));
        } else if (n->type == GEN_IPADD) {
            const int size = asn1Size(n->d.iPAddress);
            const unsigned char *data = ASN1_STRING_get0_data(n->d.iPAddress);
            if (size == 4)
                info->alternativeNames.insert(
                    QSsl::IpAddressEntry,
                    QHostAddress(quint32(data[0]) << 24 | quint32(data[1]) << 16 | quint32(data[2]) << 8 | data[3])
                        .toString());
            else if (size == 16)
                info->alternativeNames.insert(QSsl::IpAddressEntry, QHostAddress(data).toString());
        }
    }
    if (names)
        GENERAL_NAMES_free(names);
    return true;
}

QList<QByteArray> certificatesFromPem(const QByteArray &pem) {
    QList<QByteArray> out;
    const BioPtr bio = memoryBio(pem);
    while (X509Ptr x = X509Ptr(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr)))
        out.append(toDerBytes(x.get()));
    ERR_clear_error();
    return out;
}

QByteArray certificateToPem(const QByteArray &der) {
    const X509Ptr x = parseDer(der);
    if (!x)
        return QByteArray();
    const BioPtr bio(BIO_new(BIO_s_mem()));
    PEM_write_bio_X509(bio.get(), x.get());
    return bioContents(bio.get());
}

bool readKey(const QByteArray &encoded, QSsl::EncodingFormat format, QSsl::KeyType type, const QByteArray &passPhrase,
             KeyInfo *info) {
    const BioPtr bio = memoryBio(encoded);
    if (!bio || encoded.isEmpty())
        return false;
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
        return false;
    info->algorithm = QSsl::Opaque;
    switch (EVP_PKEY_base_id(key.get())) {
    case EVP_PKEY_RSA:
        info->algorithm = QSsl::Rsa;
        break;
    case EVP_PKEY_DSA:
        info->algorithm = QSsl::Dsa;
        break;
    case EVP_PKEY_EC:
        info->algorithm = QSsl::Ec;
        break;
    case EVP_PKEY_DH:
        info->algorithm = QSsl::Dh;
        break;
    default:
        break;
    }
    const BioPtr out(BIO_new(BIO_s_mem()));
    const int ok = type == QSsl::PrivateKey ? i2d_PKCS8PrivateKeyInfo_bio(out.get(), key.get())
                                            : i2d_PUBKEY_bio(out.get(), key.get());
    if (ok != 1)
        return false;
    info->der = bioContents(out.get());
    info->length = EVP_PKEY_bits(key.get());
    return true;
}

QByteArray keyToPem(const QByteArray &der, QSsl::KeyType type) {
    const char *label = type == QSsl::PrivateKey ? "PRIVATE KEY" : "PUBLIC KEY";
    const BioPtr out(BIO_new(BIO_s_mem()));
    PEM_write_bio(out.get(), label, "", reinterpret_cast<const unsigned char *>(der.constData()),
                  static_cast<long>(der.size()));
    return bioContents(out.get());
}

QList<QSslCipher> defaultCiphers() { return listCiphers(nullptr); }

QList<QSslCipher> supportedCiphers() { return listCiphers("ALL:@SECLEVEL=0"); }

long libraryVersionNumber() { return static_cast<long>(OpenSSL_version_num()); }

QString libraryVersionString() { return QString::fromUtf8(OpenSSL_version(OPENSSL_VERSION)); }

long libraryBuildVersionNumber() { return OPENSSL_VERSION_NUMBER; }

QString libraryBuildVersionString() { return QString::fromUtf8(OPENSSL_VERSION_TEXT); }

std::unique_ptr<Session> createSession(const SessionSetup &setup, QAbstractSocket::SocketError *error, QString *text) {
    const auto fail = [error, text](QAbstractSocket::SocketError code, const QString &why) {
        *error = code;
        *text = why;
        return std::unique_ptr<Session>();
    };
    auto session = std::make_unique<OpenSslSession>();
    SSL_CTX *&ctx = session->ctx;
    SSL *&ssl = session->ssl;
    BIO *&in = session->in;
    BIO *&out = session->out;
    const int verify = setup.verify;

    ERR_clear_error();
    ctx = SSL_CTX_new(TLS_method());
    if (!ctx) {
        return fail(QAbstractSocket::SslInternalError, lastSslError("Could not create a TLS context"));
    }

    int minimum = TLS1_2_VERSION, maximum = 0;
    switch (setup.protocol) {
    case QSsl::TlsV1_2:
        maximum = TLS1_2_VERSION;
        break;
    case QSsl::TlsV1_3:
        minimum = maximum = TLS1_3_VERSION;
        break;
    case QSsl::TlsV1_3OrLater:
        minimum = TLS1_3_VERSION;
        break;
    case QSsl::UnknownProtocol:
        return fail(QAbstractSocket::SslInvalidUserDataError, QString("Unsupported protocol"));
    default:
        break;
    }
    SSL_CTX_set_min_proto_version(ctx, minimum);
    SSL_CTX_set_max_proto_version(ctx, maximum);
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);

    if (verify != QSslSocket::VerifyNone) {
        if (setup.systemCaCertificates)
            SSL_CTX_set_default_verify_paths(ctx);
        X509_STORE *store = SSL_CTX_get_cert_store(ctx);
        for (const QSslCertificate &ca : setup.caCertificates) {
            X509 *x = unwrapCertificate(ca);
            if (x) {
                X509_STORE_add_cert(store, x);
                if (setup.server)
                    SSL_CTX_add_client_CA(ctx, x);
                X509_free(x);
            }
        }
        int flags = SSL_VERIFY_PEER;
        if (verify == QSslSocket::VerifyPeer && setup.server)
            flags |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        SSL_CTX_set_verify(ctx, flags, verifyCallback);
        if (setup.verifyDepth > 0)
            SSL_CTX_set_verify_depth(ctx, setup.verifyDepth);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }

    const QList<QSslCertificate> chain = setup.localChain;
    if (!chain.isEmpty()) {
        X509 *leaf = unwrapCertificate(chain.first());
        const QByteArray keyDer = setup.privateKey;
        const auto *keyBytes = reinterpret_cast<const unsigned char *>(keyDer.constData());
        EVP_PKEY *key =
            keyDer.isEmpty() ? nullptr : d2i_AutoPrivateKey(nullptr, &keyBytes, static_cast<long>(keyDer.size()));
        const bool ok = leaf && key && SSL_CTX_use_certificate(ctx, leaf) == 1 &&
                        SSL_CTX_use_PrivateKey(ctx, key) == 1 && SSL_CTX_check_private_key(ctx) == 1;
        X509_free(leaf);
        EVP_PKEY_free(key);
        if (!ok) {
            return fail(QAbstractSocket::SslInvalidUserDataError,
                        QString("The private key does not match the certificate"));
        }
        for (qsizetype i = 1; i < chain.size(); ++i) {
            X509 *extra = unwrapCertificate(chain.at(i));
            if (extra && SSL_CTX_add_extra_chain_cert(ctx, extra) != 1)
                X509_free(extra); // the context takes ownership on success
        }
    }

    // The ciphers. A restriction that cannot be applied fails the handshake: falling back to the defaults
    // of OpenSSL would allow what the application ruled out.
    if (setup.ciphersSet) {
        QByteArray older;  // TLS 1.2 and before, as a cipher list
        QByteArray suites; // TLS 1.3
        for (const QSslCipher &cipher : setup.ciphers) {
            QByteArray &list = cipher.protocol() == QSsl::TlsV1_3 ? suites : older;
            if (!list.isEmpty())
                list += ':';
            list += cipher.name().toLatin1();
        }
        if (older.isEmpty() && suites.isEmpty()) {
            return fail(QAbstractSocket::SslInvalidUserDataError, QString("The list of ciphers is empty"));
        }
        bool ok = SSL_CTX_set_ciphersuites(ctx, suites.constData()) == 1;
        if (older.isEmpty()) {
            // No cipher of the earlier versions: they are ruled out.
            ok = ok && SSL_CTX_set_min_proto_version(ctx, std::max(minimum, TLS1_3_VERSION)) == 1;
            if (maximum != 0 && maximum < TLS1_3_VERSION)
                ok = false;
        } else {
            ok = ok && SSL_CTX_set_cipher_list(ctx, older.constData()) == 1;
            if (suites.isEmpty()) {
                // No cipher of TLS 1.3: that version is ruled out.
                if (minimum >= TLS1_3_VERSION)
                    ok = false;
                else
                    ok = ok && SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION) == 1;
            }
        }
        if (!ok) {
            return fail(QAbstractSocket::SslInvalidUserDataError,
                        lastSslError("The ciphers cannot be used with this protocol"));
        }
    }

    // The options of the TLS library, which come last so that they can override what is set above.
    if (!setup.backendOptions.isEmpty()) {
        SSL_CONF_CTX *conf = SSL_CONF_CTX_new();
        bool ok = conf != nullptr;
        QByteArray failed;
        if (conf) {
            SSL_CONF_CTX_set_flags(conf, SSL_CONF_FLAG_FILE | SSL_CONF_FLAG_CERTIFICATE | SSL_CONF_FLAG_SHOW_ERRORS |
                                             (setup.server ? SSL_CONF_FLAG_SERVER : SSL_CONF_FLAG_CLIENT));
            SSL_CONF_CTX_set_ssl_ctx(conf, ctx);
            for (auto it = setup.backendOptions.constBegin(); ok && it != setup.backendOptions.constEnd(); ++it) {
                const QByteArray value = it.value().toByteArray();
                // 2: the option and its value were used; 1: only the option; anything else is a failure.
                if (it.key().isEmpty() || SSL_CONF_cmd(conf, it.key().constData(), value.constData()) <= 0) {
                    ok = false;
                    failed = it.key();
                }
            }
            ok = ok && SSL_CONF_CTX_finish(conf) == 1;
            SSL_CONF_CTX_free(conf);
        }
        if (!ok) {
            return fail(QAbstractSocket::SslInvalidUserDataError,
                        lastSslError(QByteArray("The TLS backend option " + failed + " cannot be set").constData()));
        }
    }

    session->alpnWire = wireProtocols(setup.nextProtocols);
    if (!session->alpnWire.isEmpty() && setup.server)
        SSL_CTX_set_alpn_select_cb(ctx, alpnSelect, &session->alpnWire);

    ssl = SSL_new(ctx);
    in = BIO_new(BIO_s_mem());
    out = BIO_new(BIO_s_mem());
    if (!ssl || !in || !out) {
        return fail(QAbstractSocket::SslInternalError, lastSslError("Could not create a TLS session"));
    }
    SSL_set_bio(ssl, in, out);
    SSL_set_ex_data(ssl, exDataIndex(), &session->found);

    if (!setup.server) {
        SSL_set_connect_state(ssl);
        if (!session->alpnWire.isEmpty())
            SSL_set_alpn_protos(ssl, reinterpret_cast<const unsigned char *>(session->alpnWire.constData()),
                                static_cast<unsigned int>(session->alpnWire.size()));

        const QString name = setup.peerName;
        const QHostAddress literal(name);
        const QByteArray utf8 = name.toUtf8();
        if (verify == QSslSocket::VerifyPeer && !name.isEmpty()) {
            SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            // The name goes to the parameters of the verification, as the address does: OpenSSL 4 deprecates
            // SSL_set1_host(), which took either.
            X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
            const int ok = literal.isNull() ? X509_VERIFY_PARAM_set1_host(param, utf8.constData(), 0)
                                            : X509_VERIFY_PARAM_set1_ip_asc(param, utf8.constData());
            if (ok != 1) {
                return fail(QAbstractSocket::SslInternalError, QString("Could not set the expected host name"));
            }
        }
        if (literal.isNull() && !name.isEmpty()) // server name indication takes host names only
            SSL_ctrl(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name,
                     const_cast<char *>(utf8.constData()));
    } else {
        SSL_set_accept_state(ssl);
    }
    return session;
}

} // namespace Tls
} // namespace QtPrivate
