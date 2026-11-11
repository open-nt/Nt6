// MIT License - see LICENSE
// The TLS backend of Nt Network on Mbed TLS 4, for the systems that have no OpenSSL, such as Zephyr. See
// qtlsbackend_p.h.
#include "qtlsbackend_p.h"

#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qsslsocket.h>

#include <mbedtls/build_info.h>
#include <mbedtls/error.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace QtPrivate {
namespace Tls {

Session::~Session() = default;

namespace {

// Mbed TLS 4 does all of its cryptography, the random numbers included, through PSA, which is started once.
bool ensureCrypto() { return psa_crypto_init() == PSA_SUCCESS; }

const unsigned char *bytes(const QByteArray &data) { return reinterpret_cast<const unsigned char *>(data.constData()); }

QString errorText(int code, const char *fallback) {
    char text[160] = {};
#if defined(MBEDTLS_ERROR_C)
    mbedtls_strerror(code, text, sizeof text);
#endif
    if (text[0] == '\0')
        std::snprintf(text, sizeof text, "%s (Mbed TLS error -0x%04X)", fallback, static_cast<unsigned>(-code));
    return QString::fromUtf8(text);
}

// A certificate, or a chain of them, that frees itself.
struct Certificates {
    Certificates() { mbedtls_x509_crt_init(&crt); }
    ~Certificates() { mbedtls_x509_crt_free(&crt); }
    Certificates(const Certificates &) = delete;
    Certificates &operator=(const Certificates &) = delete;

    bool add(const QByteArray &der) {
        return !der.isEmpty() &&
               mbedtls_x509_crt_parse_der(&crt, bytes(der), static_cast<std::size_t>(der.size())) == 0;
    }

    mbedtls_x509_crt crt;
};

struct Key {
    Key() { mbedtls_pk_init(&pk); }
    ~Key() { mbedtls_pk_free(&pk); }
    Key(const Key &) = delete;
    Key &operator=(const Key &) = delete;

    mbedtls_pk_context pk;
};

bool sameBytes(const mbedtls_x509_buf &a, const mbedtls_x509_buf &b) {
    return a.len == b.len && (a.len == 0 || std::memcmp(a.p, b.p, a.len) == 0);
}

QByteArray toDer(const mbedtls_x509_crt *crt) {
    return QByteArray(reinterpret_cast<const char *>(crt->raw.p), static_cast<qsizetype>(crt->raw.len));
}

bool oidIs(const mbedtls_x509_buf &oid, const char *known, std::size_t size) {
    return oid.len == size && std::memcmp(oid.p, known, size) == 0;
}

QList<std::pair<QSslCertificate::SubjectInfo, QString>> nameList(const mbedtls_x509_name *name) {
    struct Known {
        const char *oid;
        std::size_t size;
        QSslCertificate::SubjectInfo info;
    };
// The OID macros are string literals with embedded zero bytes; sizeof counts the terminator.
#define NT_OID(oid, info)                                                                                              \
    {                                                                                                                  \
        oid, sizeof(oid) - 1, info                                                                                     \
    }
    static const Known known[] = {
        NT_OID(MBEDTLS_OID_AT_ORGANIZATION, QSslCertificate::Organization),
        NT_OID(MBEDTLS_OID_AT_CN, QSslCertificate::CommonName),
        NT_OID(MBEDTLS_OID_AT_LOCALITY, QSslCertificate::LocalityName),
        NT_OID(MBEDTLS_OID_AT_ORG_UNIT, QSslCertificate::OrganizationalUnitName),
        NT_OID(MBEDTLS_OID_AT_COUNTRY, QSslCertificate::CountryName),
        NT_OID(MBEDTLS_OID_AT_STATE, QSslCertificate::StateOrProvinceName),
        NT_OID(MBEDTLS_OID_AT_DN_QUALIFIER, QSslCertificate::DistinguishedNameQualifier),
        NT_OID(MBEDTLS_OID_AT_SERIAL_NUMBER, QSslCertificate::SerialNumber),
        NT_OID(MBEDTLS_OID_PKCS9_EMAIL, QSslCertificate::EmailAddress),
    };
#undef NT_OID
    QList<std::pair<QSslCertificate::SubjectInfo, QString>> out;
    for (; name; name = name->next) {
        for (const Known &k : known) {
            if (oidIs(name->oid, k.oid, k.size))
                out.append({k.info, QString::fromUtf8(reinterpret_cast<const char *>(name->val.p),
                                                      static_cast<qsizetype>(name->val.len))});
        }
    }
    return out;
}

QDateTime toDateTime(const mbedtls_x509_time &t) {
    return QDateTime(QDate(t.year, t.mon, t.day), QTime(t.hour, t.min, t.sec), Qt::UTC);
}

QString bufferText(const mbedtls_x509_buf &buf) {
    return QString::fromUtf8(reinterpret_cast<const char *>(buf.p), static_cast<qsizetype>(buf.len));
}

// Base64 in the lines of 64 characters that PEM asks for, between the two markers of `label`.
QByteArray pemText(const QByteArray &der, const char *label) {
    const QByteArray base64 = der.toBase64();
    QByteArray out = QByteArray("-----BEGIN ") + label + "-----\n";
    for (qsizetype i = 0; i < base64.size(); i += 64) {
        out += base64.mid(i, 64);
        out += '\n';
    }
    out += QByteArray("-----END ") + label + "-----\n";
    return out;
}

// What a key writer left at the end of its buffer, or nothing if it failed.
QByteArray written(const std::vector<unsigned char> &buffer, int size) {
    if (size <= 0 || static_cast<std::size_t>(size) > buffer.size())
        return QByteArray();
    return QByteArray(reinterpret_cast<const char *>(buffer.data() + buffer.size() - static_cast<std::size_t>(size)),
                      size);
}

bool parsePrivateKey(Key &key, const QByteArray &encoded, QSsl::EncodingFormat format, const QByteArray &passPhrase) {
    // A PEM text has to end with a zero byte, which counts in its length.
    const std::size_t size = static_cast<std::size_t>(encoded.size()) + (format == QSsl::Pem ? 1 : 0);
    const unsigned char *pass = passPhrase.isEmpty() ? nullptr : bytes(passPhrase);
    return mbedtls_pk_parse_key(&key.pk, bytes(encoded), size, pass, static_cast<std::size_t>(passPhrase.size())) == 0;
}

QSslError::SslError selfSignedOrUnknown(const mbedtls_x509_crt *crt, int depth) {
    if (sameBytes(crt->issuer_raw, crt->subject_raw))
        return depth == 0 ? QSslError::SelfSignedCertificate : QSslError::SelfSignedCertificateInChain;
    return QSslError::UnableToGetLocalIssuerCertificate;
}

QSslCipher cipherFromId(int id) {
    const mbedtls_ssl_ciphersuite_t *info = mbedtls_ssl_ciphersuite_from_id(id);
    if (!info)
        return QSslCipher();
    const QString name = QString::fromUtf8(mbedtls_ssl_ciphersuite_get_name(info));
    const int bits = static_cast<int>(mbedtls_ssl_ciphersuite_get_cipher_key_bitlen(info));
    // The suites of TLS 1.3 are the ones that Mbed TLS names "TLS1-3-...".
    const bool tls13 = name.startsWith(QString("TLS1-3-"));
    return QSslCipher(name, bits, bits, QString(tls13 ? "TLSv1.3" : "TLSv1.2"), tls13 ? QSsl::TlsV1_3 : QSsl::TlsV1_2);
}

QList<QSslCipher> listCiphers() {
    QList<QSslCipher> out;
    for (const int *id = mbedtls_ssl_list_ciphersuites(); id && *id != 0; ++id) {
        const QSslCipher cipher = cipherFromId(*id);
        if (!cipher.isNull())
            out.append(cipher);
    }
    return out;
}

class MbedTlsSession : public Session {
public:
    MbedTlsSession() {
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
    }
    ~MbedTlsSession() override {
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
    }

    void feed(const char *data, qint64 size) override { incoming.append(data, static_cast<qsizetype>(size)); }

    QByteArray takeOutput() override {
        QByteArray out;
        out.swap(outgoing);
        return out;
    }

    Status handshake(QString *error) override {
        const int rc = mbedtls_ssl_handshake(&ssl);
        if (rc == 0)
            return Done;
        if (wouldBlock(rc))
            return Again;
        *error = errorText(rc, "The TLS handshake failed");
        return Failed;
    }

    Status read(QByteArray *plain, QString *error) override {
        unsigned char chunk[4096];
        for (;;) {
            const int n = mbedtls_ssl_read(&ssl, chunk, sizeof chunk);
            if (n > 0) {
                plain->append(reinterpret_cast<const char *>(chunk), n);
                continue;
            }
            if (n == 0 || n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
                return Closed;
#if defined(MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
            if (n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
                continue; // a ticket of TLS 1.3, which this class does not keep
#endif
            if (wouldBlock(n))
                return Again;
            *error = errorText(n, "TLS protocol error");
            return Failed;
        }
    }

    qint64 write(const char *data, qint64 size, QString *error) override {
        qint64 done = 0;
        int stalled = 0;
        while (done < size) {
            const std::size_t chunk = static_cast<std::size_t>(qMin<qint64>(size - done, 1 << 24));
            const int n = mbedtls_ssl_write(&ssl, reinterpret_cast<const unsigned char *>(data + done), chunk);
            if (n > 0) {
                done += n;
                stalled = 0;
                continue;
            }
            // Sending never blocks here, so a request to come back is answered at once, a few times at most.
            if (wouldBlock(n) && ++stalled < 8)
                continue;
            *error = errorText(n, "Could not encrypt data");
            return done > 0 ? done : -1;
        }
        return done;
    }

    void shutdown() override { (void)mbedtls_ssl_close_notify(&ssl); }

    QList<QSslCertificate> peerChain() const override {
        // The verification has seen the chain. Mbed TLS itself keeps it only if it was configured to, which
        // Zephyr does for TLS 1.3 alone.
        QList<QSslCertificate> chain = seen;
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
        for (const mbedtls_x509_crt *crt = mbedtls_ssl_get_peer_cert(&ssl); crt; crt = crt->next) {
            const QSslCertificate c(toDer(crt), QSsl::Der);
            if (!c.isNull() && !chain.contains(c))
                chain.append(c);
        }
#endif
        return chain;
    }

    QList<QSslError> verifyErrors() const override { return found; }

    QSslCipher cipher() const override { return cipherFromId(mbedtls_ssl_get_ciphersuite_id_from_ssl(&ssl)); }

    QSsl::SslProtocol protocol() const override {
        switch (mbedtls_ssl_get_version_number(&ssl)) {
        case MBEDTLS_SSL_VERSION_TLS1_3:
            return QSsl::TlsV1_3;
        case MBEDTLS_SSL_VERSION_TLS1_2:
            return QSsl::TlsV1_2;
        default:
            return QSsl::UnknownProtocol;
        }
    }

    QByteArray nextProtocol() const override {
#if defined(MBEDTLS_SSL_ALPN)
        if (const char *chosen = mbedtls_ssl_get_alpn_protocol(&ssl))
            return QByteArray(chosen);
#endif
        return QByteArray();
    }

    static bool wouldBlock(int rc) {
        return rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
               rc == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS || rc == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS;
    }

    static int sendCallback(void *context, const unsigned char *data, std::size_t size) {
        auto *self = static_cast<MbedTlsSession *>(context);
        self->outgoing.append(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
        return static_cast<int>(size);
    }

    static int receiveCallback(void *context, unsigned char *data, std::size_t size) {
        auto *self = static_cast<MbedTlsSession *>(context);
        const std::size_t ready = static_cast<std::size_t>(self->incoming.size() - self->consumed);
        if (ready == 0)
            return MBEDTLS_ERR_SSL_WANT_READ;
        const std::size_t n = qMin(ready, size);
        std::memcpy(data, self->incoming.constData() + self->consumed, n);
        self->consumed += static_cast<qsizetype>(n);
        if (self->consumed == self->incoming.size()) {
            self->incoming.clear();
            self->consumed = 0;
        }
        return static_cast<int>(n);
    }

    // Records every failure and lets the handshake continue, as the verification is optional for Mbed TLS. The
    // decision is made by QSslSocket, before any application data can flow.
    static int verifyCallback(void *context, mbedtls_x509_crt *crt, int depth, uint32_t *flags) {
        auto *self = static_cast<MbedTlsSession *>(context);
        const QSslCertificate certificate(toDer(crt), QSsl::Der);
        // The callback runs from the far end of the chain down to the certificate of the peer, at depth 0.
        if (!certificate.isNull() && !self->seen.contains(certificate))
            self->seen.prepend(certificate);
        const auto add = [self, &certificate](QSslError::SslError code) {
            const QSslError error(code, certificate);
            if (!self->found.contains(error))
                self->found.append(error);
        };
        const uint32_t f = *flags;
        if (f & MBEDTLS_X509_BADCERT_EXPIRED)
            add(QSslError::CertificateExpired);
        if (f & MBEDTLS_X509_BADCERT_FUTURE)
            add(QSslError::CertificateNotYetValid);
        if (f & MBEDTLS_X509_BADCERT_REVOKED)
            add(QSslError::CertificateRevoked);
        if (f & MBEDTLS_X509_BADCERT_CN_MISMATCH)
            add(QSslError::HostNameMismatch);
        if (f & MBEDTLS_X509_BADCERT_NOT_TRUSTED)
            add(selfSignedOrUnknown(crt, depth));
        if (f &
            (MBEDTLS_X509_BADCERT_KEY_USAGE | MBEDTLS_X509_BADCERT_EXT_KEY_USAGE | MBEDTLS_X509_BADCERT_NS_CERT_TYPE))
            add(QSslError::InvalidPurpose);
        if (f & (MBEDTLS_X509_BADCERT_BAD_MD | MBEDTLS_X509_BADCERT_BAD_PK | MBEDTLS_X509_BADCERT_BAD_KEY))
            add(QSslError::CertificateUntrusted);
        if (f & (MBEDTLS_X509_BADCERT_OTHER | MBEDTLS_X509_BADCRL_NOT_TRUSTED | MBEDTLS_X509_BADCRL_EXPIRED |
                 MBEDTLS_X509_BADCRL_FUTURE | MBEDTLS_X509_BADCRL_BAD_MD | MBEDTLS_X509_BADCRL_BAD_PK |
                 MBEDTLS_X509_BADCRL_BAD_KEY))
            add(QSslError::UnspecifiedError);
        if (self->verifyDepth > 0 && depth > self->verifyDepth)
            add(QSslError::PathLengthExceeded);
        return 0;
    }

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    Certificates authorities;
    Certificates own;
    Key ownKey;
    std::vector<int> suites;            // zero-terminated, as Mbed TLS keeps the pointer
    QList<QByteArray> alpnNames;        // the storage of alpnList
    std::vector<const char *> alpnList; // null-terminated, as Mbed TLS keeps the pointer
    QByteArray peerName;
    QByteArray incoming; // network to Mbed TLS
    qsizetype consumed = 0;
    QByteArray outgoing; // Mbed TLS to network
    QList<QSslError> found;
    QList<QSslCertificate> seen; // the chain of the peer, as the verification met it
    int verifyDepth = 0;
};

} // namespace

bool readCertificate(const QByteArray &der, CertificateInfo *info) {
    Certificates parsed;
    if (!ensureCrypto() || !parsed.add(der))
        return false;
    const mbedtls_x509_crt &crt = parsed.crt;
    if (crt.raw.len != static_cast<std::size_t>(der.size())) // something follows the certificate
        return false;
    if (!info)
        return true;
    info->selfSigned = sameBytes(crt.issuer_raw, crt.subject_raw);
    info->version = crt.version;
    info->serialNumber =
        QByteArray(reinterpret_cast<const char *>(crt.serial.p), static_cast<qsizetype>(crt.serial.len));
    info->issuer = nameList(&crt.issuer);
    info->subject = nameList(&crt.subject);
    info->notBefore = toDateTime(crt.valid_from);
    info->notAfter = toDateTime(crt.valid_to);

    for (const mbedtls_x509_sequence *entry = &crt.subject_alt_names; entry && entry->buf.p; entry = entry->next) {
        mbedtls_x509_subject_alternative_name name;
        std::memset(&name, 0, sizeof name);
        if (mbedtls_x509_parse_subject_alt_name(&entry->buf, &name) != 0)
            continue; // a kind of name that Mbed TLS does not read
        const mbedtls_x509_buf &value = name.san.unstructured_name;
        if (name.type == MBEDTLS_X509_SAN_DNS_NAME) {
            info->alternativeNames.insert(QSsl::DnsEntry, bufferText(value));
        } else if (name.type == MBEDTLS_X509_SAN_RFC822_NAME) {
            info->alternativeNames.insert(QSsl::EmailEntry, bufferText(value));
        } else if (name.type == MBEDTLS_X509_SAN_IP_ADDRESS) {
            const unsigned char *data = value.p;
            if (value.len == 4)
                info->alternativeNames.insert(
                    QSsl::IpAddressEntry,
                    QHostAddress(quint32(data[0]) << 24 | quint32(data[1]) << 16 | quint32(data[2]) << 8 | data[3])
                        .toString());
            else if (value.len == 16)
                info->alternativeNames.insert(QSsl::IpAddressEntry, QHostAddress(data).toString());
        }
        mbedtls_x509_free_subject_alt_name(&name);
    }
    return true;
}

QList<QByteArray> certificatesFromPem(const QByteArray &pem) {
    static const QByteArray begin("-----BEGIN CERTIFICATE-----");
    static const QByteArray end("-----END CERTIFICATE-----");
    QList<QByteArray> out;
    qsizetype from = 0;
    for (;;) {
        const qsizetype start = pem.indexOf(begin, from);
        if (start < 0)
            break;
        const qsizetype stop = pem.indexOf(end, start);
        if (stop < 0)
            break;
        const QByteArray der = QByteArray::fromBase64(pem.mid(start + begin.size(), stop - start - begin.size()));
        if (readCertificate(der, nullptr))
            out.append(der);
        from = stop + end.size();
    }
    return out;
}

QByteArray certificateToPem(const QByteArray &der) {
    return readCertificate(der, nullptr) ? pemText(der, "CERTIFICATE") : QByteArray();
}

bool readKey(const QByteArray &encoded, QSsl::EncodingFormat format, QSsl::KeyType type, const QByteArray &passPhrase,
             KeyInfo *info) {
#if defined(MBEDTLS_PK_PARSE_C) && defined(MBEDTLS_PK_WRITE_C)
    if (encoded.isEmpty() || !ensureCrypto())
        return false;
    Key key;
    int rc = 0;
    if (type == QSsl::PrivateKey) {
        rc = parsePrivateKey(key, encoded, format, passPhrase) ? 0 : -1;
    } else {
        const std::size_t size = static_cast<std::size_t>(encoded.size()) + (format == QSsl::Pem ? 1 : 0);
        rc = mbedtls_pk_parse_public_key(&key.pk, bytes(encoded), size);
    }
    if (rc != 0)
        return false;

    const psa_key_type_t kind = mbedtls_pk_get_key_type(&key.pk);
    info->algorithm = PSA_KEY_TYPE_IS_RSA(kind) ? QSsl::Rsa : PSA_KEY_TYPE_IS_ECC(kind) ? QSsl::Ec : QSsl::Opaque;
    info->length = static_cast<int>(mbedtls_pk_get_bitlen(&key.pk));

    // Large enough for an RSA key of 8192 bits.
    std::vector<unsigned char> buffer(6144);
    const int size = type == QSsl::PrivateKey ? mbedtls_pk_write_key_der(&key.pk, buffer.data(), buffer.size())
                                              : mbedtls_pk_write_pubkey_der(&key.pk, buffer.data(), buffer.size());
    info->der = written(buffer, size);
    return !info->der.isEmpty();
#else
    (void)encoded;
    (void)format;
    (void)type;
    (void)passPhrase;
    (void)info;
    return false; // Mbed TLS was configured without the reading or the writing of keys
#endif
}

// Mbed TLS writes a private key in the format of its algorithm (PKCS#1 for RSA, SEC 1 for an elliptic curve), not
// in PKCS#8, so the PEM text is labeled to match.
QByteArray keyToPem(const QByteArray &der, QSsl::KeyType type) {
    if (type == QSsl::PublicKey)
        return pemText(der, "PUBLIC KEY");
    KeyInfo info;
    if (!readKey(der, QSsl::Der, QSsl::PrivateKey, QByteArray(), &info))
        return QByteArray();
    return pemText(info.der, info.algorithm == QSsl::Rsa ? "RSA PRIVATE KEY" : "EC PRIVATE KEY");
}

QList<QSslCipher> defaultCiphers() { return listCiphers(); }

QList<QSslCipher> supportedCiphers() { return listCiphers(); }

long libraryVersionNumber() { return static_cast<long>(MBEDTLS_VERSION_NUMBER); }

QString libraryVersionString() { return QString::fromUtf8(MBEDTLS_VERSION_STRING_FULL); }

long libraryBuildVersionNumber() { return static_cast<long>(MBEDTLS_VERSION_NUMBER); }

QString libraryBuildVersionString() { return QString::fromUtf8(MBEDTLS_VERSION_STRING_FULL); }

std::unique_ptr<Session> createSession(const SessionSetup &setup, QAbstractSocket::SocketError *error, QString *text) {
    const auto fail = [error, text](QAbstractSocket::SocketError code, const QString &why) {
        *error = code;
        *text = why;
        return std::unique_ptr<Session>();
    };
    if (!ensureCrypto())
        return fail(QAbstractSocket::SslInternalError, QString("Could not start the cryptography of Mbed TLS"));
#if !defined(MBEDTLS_SSL_SRV_C)
    if (setup.server)
        return fail(QAbstractSocket::SslInternalError, QString("Mbed TLS was built without the server side of TLS"));
#endif
#if !defined(MBEDTLS_SSL_CLI_C)
    if (!setup.server)
        return fail(QAbstractSocket::SslInternalError, QString("Mbed TLS was built without the client side of TLS"));
#endif

    auto session = std::make_unique<MbedTlsSession>();
    mbedtls_ssl_config *conf = &session->conf;
    int rc = mbedtls_ssl_config_defaults(conf, setup.server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0)
        return fail(QAbstractSocket::SslInternalError, errorText(rc, "Could not create a TLS context"));

    // The versions. 0 stands for the newest or the oldest that Mbed TLS was built with.
    int minimum = 0, maximum = 0;
    switch (setup.protocol) {
    case QSsl::TlsV1_2:
        minimum = maximum = MBEDTLS_SSL_VERSION_TLS1_2;
        break;
    case QSsl::TlsV1_3:
    case QSsl::TlsV1_3OrLater:
        minimum = MBEDTLS_SSL_VERSION_TLS1_3;
        break;
    case QSsl::UnknownProtocol:
        return fail(QAbstractSocket::SslInvalidUserDataError, QString("Unsupported protocol"));
    default:
        break;
    }
#if !defined(MBEDTLS_SSL_PROTO_TLS1_3)
    if (minimum == MBEDTLS_SSL_VERSION_TLS1_3)
        return fail(QAbstractSocket::SslInvalidUserDataError, QString("Mbed TLS was built without TLS 1.3"));
#endif
#if !defined(MBEDTLS_SSL_PROTO_TLS1_2)
    if (maximum == MBEDTLS_SSL_VERSION_TLS1_2)
        return fail(QAbstractSocket::SslInvalidUserDataError, QString("Mbed TLS was built without TLS 1.2"));
#endif

    // The ciphers. A restriction that cannot be applied fails the handshake: falling back to the defaults of
    // Mbed TLS would allow what the application ruled out.
    if (setup.ciphersSet) {
        bool older = false, newer = false;
        for (const QSslCipher &cipher : setup.ciphers) {
            const int id = mbedtls_ssl_get_ciphersuite_id(cipher.name().toLatin1().constData());
            if (id == 0)
                continue;
            session->suites.push_back(id);
            (cipher.protocol() == QSsl::TlsV1_3 ? newer : older) = true;
        }
        if (session->suites.empty())
            return fail(QAbstractSocket::SslInvalidUserDataError, QString("The list of ciphers is empty"));
        session->suites.push_back(0);
        bool ok = true;
        if (!older) { // no cipher of TLS 1.2: that version is ruled out
            ok = maximum != MBEDTLS_SSL_VERSION_TLS1_2;
            minimum = MBEDTLS_SSL_VERSION_TLS1_3;
        } else if (!newer) { // no cipher of TLS 1.3: that version is ruled out
            ok = minimum != MBEDTLS_SSL_VERSION_TLS1_3;
            maximum = MBEDTLS_SSL_VERSION_TLS1_2;
        }
        if (!ok)
            return fail(QAbstractSocket::SslInvalidUserDataError,
                        QString("The ciphers cannot be used with this protocol"));
        mbedtls_ssl_conf_ciphersuites(conf, session->suites.data());
    }
    if (minimum != 0)
        mbedtls_ssl_conf_min_tls_version(conf, static_cast<mbedtls_ssl_protocol_version>(minimum));
    if (maximum != 0)
        mbedtls_ssl_conf_max_tls_version(conf, static_cast<mbedtls_ssl_protocol_version>(maximum));

    // The options of OpenSSL's configuration files mean nothing to Mbed TLS, and an option that is not applied
    // must not pass silently.
    if (!setup.backendOptions.isEmpty())
        return fail(QAbstractSocket::SslInvalidUserDataError,
                    QString::fromUtf8("The TLS backend option " + setup.backendOptions.firstKey() + " cannot be set"));

    // Verification is optional for Mbed TLS, so that the handshake goes on and QSslSocket can report every
    // problem and let the application decide.
    session->verifyDepth = setup.verifyDepth;
    // A server that does not verify asks for no certificate. A client always gets one, and looks at it even
    // if it does not verify, so that QSslSocket::peerCertificate() can tell what it was.
    if (setup.verify == QSslSocket::VerifyNone && setup.server) {
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
    } else {
        for (const QSslCertificate &ca : setup.caCertificates)
            (void)session->authorities.add(ca.toDer());
#if defined(MBEDTLS_FS_IO) && !defined(__ZEPHYR__)
        if (setup.systemCaCertificates)
            (void)mbedtls_x509_crt_parse_path(&session->authorities.crt, "/etc/ssl/certs");
#endif
        mbedtls_ssl_conf_ca_chain(conf, &session->authorities.crt, nullptr);
        mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
        mbedtls_ssl_conf_verify(conf, &MbedTlsSession::verifyCallback, session.get());
    }

    if (!setup.localChain.isEmpty()) {
        bool ok = true;
        for (const QSslCertificate &certificate : setup.localChain)
            ok = session->own.add(certificate.toDer()) && ok;
        ok = ok && parsePrivateKey(session->ownKey, setup.privateKey, QSsl::Der, QByteArray()) &&
             mbedtls_pk_check_pair(&session->own.crt.pk, &session->ownKey.pk) == 0 &&
             mbedtls_ssl_conf_own_cert(conf, &session->own.crt, &session->ownKey.pk) == 0;
        if (!ok)
            return fail(QAbstractSocket::SslInvalidUserDataError,
                        QString("The private key does not match the certificate"));
    }

#if defined(MBEDTLS_SSL_ALPN)
    for (const QByteArray &protocol : setup.nextProtocols) {
        if (!protocol.isEmpty() && protocol.size() <= 255)
            session->alpnNames.append(protocol);
    }
    if (!session->alpnNames.isEmpty()) {
        for (const QByteArray &protocol : session->alpnNames)
            session->alpnList.push_back(protocol.constData());
        session->alpnList.push_back(nullptr);
        rc = mbedtls_ssl_conf_alpn_protocols(conf, session->alpnList.data());
        if (rc != 0)
            return fail(QAbstractSocket::SslInternalError, errorText(rc, "Could not set the protocols to negotiate"));
    }
#endif

    rc = mbedtls_ssl_setup(&session->ssl, conf);
    if (rc != 0)
        return fail(QAbstractSocket::SslInternalError, errorText(rc, "Could not create a TLS session"));
    mbedtls_ssl_set_bio(&session->ssl, session.get(), &MbedTlsSession::sendCallback, &MbedTlsSession::receiveCallback,
                        nullptr);

    if (!setup.server) {
        // The name is what the certificate must match and what is sent to the server. Without one, Mbed TLS is
        // told so explicitly; QSslSocket then reports that nothing proves who the peer is.
        session->peerName = setup.peerName.toUtf8();
        rc = mbedtls_ssl_set_hostname(&session->ssl,
                                      session->peerName.isEmpty() ? nullptr : session->peerName.constData());
        if (rc != 0)
            return fail(QAbstractSocket::SslInternalError, QString("Could not set the expected host name"));
    }
    return session;
}

} // namespace Tls
} // namespace QtPrivate
