// MIT License - see LICENSE
#pragma once

// The seam between the TLS classes of Nt Network and the library that does the work. QSslCertificate, QSslKey,
// QSslConfiguration and QSslSocket are written against these functions; qtlsbackend_openssl.cpp implements them
// with OpenSSL and qtlsbackend_mbedtls.cpp with Mbed TLS. Exactly one of the two is built.

#include <QtCore/qbytearray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qssl.h>
#include <QtNetwork/qsslcertificate.h>
#include <QtNetwork/qsslcipher.h>
#include <QtNetwork/qsslerror.h>

#include <memory>
#include <utility>

namespace QtPrivate {
namespace Tls {

// What QSslCertificate tells about a certificate.
struct CertificateInfo {
    bool selfSigned = false;
    int version = 0;
    QByteArray serialNumber; // the bytes of the number, most significant first
    // The attributes of the two names that QSslCertificate::SubjectInfo knows, in the order of the certificate.
    QList<std::pair<QSslCertificate::SubjectInfo, QString>> issuer;
    QList<std::pair<QSslCertificate::SubjectInfo, QString>> subject;
    QMultiMap<QSsl::AlternativeNameEntryType, QString> alternativeNames;
    QDateTime notBefore;
    QDateTime notAfter;
};

// Parses the DER encoding of one certificate. Returns false if it is not one; `info` may be null to ask just that.
bool readCertificate(const QByteArray &der, CertificateInfo *info);
// The DER encoding of each certificate in a PEM text, and the PEM text of one certificate.
QList<QByteArray> certificatesFromPem(const QByteArray &pem);
QByteArray certificateToPem(const QByteArray &der);

struct KeyInfo {
    QByteArray der; // unencrypted: PKCS#8 for a private key, SubjectPublicKeyInfo for a public one
    QSsl::KeyAlgorithm algorithm = QSsl::Opaque;
    int length = -1; // in bits
};

// Parses a key, decrypting it with `passPhrase` if it is protected. Returns false if that fails.
bool readKey(const QByteArray &encoded, QSsl::EncodingFormat format, QSsl::KeyType type, const QByteArray &passPhrase,
             KeyInfo *info);
// The PEM text of a key in the encoding of KeyInfo::der.
QByteArray keyToPem(const QByteArray &der, QSsl::KeyType type);

// The ciphers that a connection offers when none are chosen, and every cipher that the library has.
QList<QSslCipher> defaultCiphers();
QList<QSslCipher> supportedCiphers();

// The version of the library that is running, and of the one that Nt was built with.
long libraryVersionNumber();
QString libraryVersionString();
long libraryBuildVersionNumber();
QString libraryBuildVersionString();

// Everything a connection is set up from. QSslSocket has resolved the defaults: `verify` is never AutoVerifyPeer.
struct SessionSetup {
    bool server = false;
    QSsl::SslProtocol protocol = QSsl::SecureProtocols;
    int verify = 0; // a QSslSocket::PeerVerifyMode
    int verifyDepth = 0;
    QString peerName; // of a client: the name that the certificate must match and that is sent to the server
    QList<QSslCertificate> localChain;
    QByteArray privateKey; // KeyInfo::der
    QList<QSslCertificate> caCertificates;
    bool systemCaCertificates = true;
    bool ciphersSet = false;
    QList<QSslCipher> ciphers;
    QList<QByteArray> nextProtocols;
    QMap<QByteArray, QVariant> backendOptions;
};

// One TLS connection. It never touches the network: QSslSocket hands it what arrived with feed() and sends what
// takeOutput() returns.
class Session {
public:
    enum Status {
        Done,   // the handshake has finished
        Again,  // nothing more to do until more data arrives
        Closed, // the peer has closed its side of the TLS connection
        Failed  // the connection is broken; `error` says why
    };

    Session() = default;
    virtual ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    virtual void feed(const char *data, qint64 size) = 0;
    virtual QByteArray takeOutput() = 0;

    // Returns Done, Again or Failed.
    virtual Status handshake(QString *error) = 0;
    // Appends the decrypted data that is ready to `plain`. Returns Again, Closed or Failed.
    virtual Status read(QByteArray *plain, QString *error) = 0;
    // Encrypts `size` bytes. Returns the number of bytes taken, or -1.
    virtual qint64 write(const char *data, qint64 size, QString *error) = 0;
    // Queues the alert that closes this side of the connection.
    virtual void shutdown() = 0;

    // Valid once the handshake has finished. The chain starts with the certificate of the peer.
    virtual QList<QSslCertificate> peerChain() const = 0;
    virtual QList<QSslError> verifyErrors() const = 0;
    virtual QSslCipher cipher() const = 0;
    virtual QSsl::SslProtocol protocol() const = 0;
    virtual QByteArray nextProtocol() const = 0;
};

// Creates a connection that is ready for handshake(). On failure returns null and sets `error` and `text`.
std::unique_ptr<Session> createSession(const SessionSetup &setup, QAbstractSocket::SocketError *error, QString *text);

} // namespace Tls
} // namespace QtPrivate
