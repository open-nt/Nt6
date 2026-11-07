// MIT License - see LICENSE
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslsocket.h>

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <deque>

namespace {

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

} // namespace

struct QSslSocket::Private {
    SSL_CTX *ctx = nullptr;
    SSL *ssl = nullptr;
    BIO *in = nullptr;  // network to OpenSSL
    BIO *out = nullptr; // OpenSSL to network
    QSslConfiguration configuration;
    QByteArray alpnWire;
    SslMode mode = UnencryptedMode;
    bool encrypted = false;
    bool autoStart = false;
    bool ignoreAll = false;
    bool waiting = false;
    bool driving = false;
    bool closeSent = false;
    QString peerVerifyName;
    QList<QSslError> ignoreList;
    QList<QSslError> found; // reported by the verify callback
    QList<QSslError> handshakeErrors;
    QByteArray pendingPlain; // written before the handshake finished
    struct Record {
        qint64 cipher;
        qint64 plain;
    };
    std::deque<Record> records; // maps encrypted bytes back to bytesWritten()
    qint64 encryptedToWrite = 0;

    void release() {
        if (ssl)
            SSL_free(ssl); // also frees the BIOs
        if (ctx)
            SSL_CTX_free(ctx);
        ssl = nullptr;
        ctx = nullptr;
        in = out = nullptr;
    }
    ~Private() { release(); }
};

namespace {

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

} // namespace

/*!
    \class QSslSocket
    \brief The QSslSocket class provides an encrypted TCP connection using TLS.

    QSslSocket is a QTcpSocket that encrypts what you write and decrypts what
    you read. Use it like any other socket, starting when encrypted() is
    emitted:

    \code
    QSslSocket socket;
    QObject::connect(&socket, &QSslSocket::encrypted, [&] {
        socket.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    });
    QObject::connect(&socket, &QIODevice::readyRead, [&] {
        qDebug() << socket.readAll();
    });
    socket.connectToHostEncrypted("example.com", 443);
    \endcode

    Clients verify the server by default: the certificate must chain to a
    trusted CA and match the host name. On failure sslErrors() is emitted and
    the connection closes, with nothing sent. A slot can call ignoreSslErrors()
    to continue, which removes the protection TLS gives against impersonation,
    so pass the specific errors you expect, not none at all.

    To encrypt an existing connection, as with \c STARTTLS, call
    startClientEncryption() once connected.

    Servers need a certificate and key; see QSslServer.

    \differences
    Requires OpenSSL 1.1.1 or newer; when Nt is built without it, these classes
    are absent and \c QT_NO_SSL is defined. Only TLS 1.2 and 1.3 are available.
    \c connectToHostEncrypted() resolves the name synchronously, as
    QAbstractSocket::connectToHost() does. Only the OpenSSL backend exists, so
    there are no backend-selection functions, and no \c setCiphers(), PSK,
    OCSP or session resumption API. A peer that closes without a TLS shutdown
    is reported as an ordinary disconnect, so a protocol that delimits data by
    closing the connection cannot detect truncation from this class.
    \enddifferences

    \sa QSslConfiguration, QSslServer, QSslCertificate
*/

/*!
    \enum QSslSocket::SslMode

    Describes what the socket is doing.

    \value UnencryptedMode The connection is plain TCP.
    \value SslClientMode The socket is the TLS client.
    \value SslServerMode The socket is the TLS server.
*/

/*!
    \enum QSslSocket::PeerVerifyMode

    Selects how the peer's certificate is treated.

    \value VerifyNone The certificate is not requested or checked.
    \value QueryPeer The certificate is requested, but problems are ignored.
    \value VerifyPeer The certificate is required and must verify.
    \value AutoVerifyPeer VerifyPeer for clients, and VerifyNone for servers.
*/

/*!
    Constructs a socket with the given \a parent.
*/
QSslSocket::QSslSocket(QObject *parent) : QTcpSocket(parent), d(std::make_unique<Private>()) {
    d->configuration = QSslConfiguration::defaultConfiguration();
    // These run first, as they are connected before anyone else can connect.
    QObject::connect(this, &QAbstractSocket::connected, this, [this] {
        if (d->autoStart) {
            d->autoStart = false;
            beginHandshake(SslClientMode);
        }
    });
    QObject::connect(this, &QAbstractSocket::disconnected, this, [this] { resetState(); });
}

/*!
    Destroys the socket.
*/
QSslSocket::~QSslSocket() = default;

void QSslSocket::resetState() {
    d->release();
    d->autoStart = false;
    d->ignoreAll = false;
    d->ignoreList.clear();
    d->closeSent = false;
    d->pendingPlain.clear();
    d->records.clear();
    d->encryptedToWrite = 0;
    d->encrypted = false;
    if (d->mode != UnencryptedMode) {
        d->mode = UnencryptedMode;
        modeChanged(UnencryptedMode);
    }
}

/*!
    Connects to \a hostName on \a port with the open mode \a mode, using the
    network protocol \a protocol, then starts a TLS handshake. The host name is
    also the name the certificate must match.

    \sa encrypted(), setPeerVerifyName()
*/
void QSslSocket::connectToHostEncrypted(const QString &hostName, quint16 port, OpenMode mode,
                                        NetworkLayerProtocol protocol) {
    connectToHostEncrypted(hostName, port, hostName, mode, protocol);
}

/*!
    \overload

    Connects to \a hostName on \a port as above, and checks the certificate
    against \a sslPeerName, using the open mode \a mode and network protocol
    \a protocol.
*/
void QSslSocket::connectToHostEncrypted(const QString &hostName, quint16 port, const QString &sslPeerName,
                                        OpenMode mode, NetworkLayerProtocol protocol) {
    if (state() != UnconnectedState)
        return;
    d->peerVerifyName = sslPeerName;
    d->autoStart = true;
    d->mode = SslClientMode;
    modeChanged(SslClientMode);
    connectToHost(hostName, port, mode, protocol);
}

/*!
    Starts a client handshake on a connection that is already established.

    \sa startServerEncryption(), encrypted()
*/
void QSslSocket::startClientEncryption() { beginHandshake(SslClientMode); }

/*!
    Starts a server handshake on a connection that is already established. A
    local certificate and private key must be set.

    \sa QSslServer, setLocalCertificate(), setPrivateKey()
*/
void QSslSocket::startServerEncryption() { beginHandshake(SslServerMode); }

void QSslSocket::beginHandshake(SslMode mode) {
    if (state() != ConnectedState || d->ssl || d->encrypted)
        return;
    const QSslConfiguration &config = d->configuration;

    if (mode == SslServerMode && (config.localCertificate().isNull() || config.privateKey().isNull())) {
        failHandshake(SslInvalidUserDataError, QString("Cannot provide a certificate with no key"));
        return;
    }

    ERR_clear_error();
    d->ctx = SSL_CTX_new(TLS_method());
    if (!d->ctx) {
        failHandshake(SslInternalError, lastSslError("Could not create a TLS context"));
        return;
    }

    int minimum = TLS1_2_VERSION, maximum = 0;
    switch (config.protocol()) {
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
        failHandshake(SslInvalidUserDataError, QString("Unsupported protocol"));
        return;
    default:
        break;
    }
    SSL_CTX_set_min_proto_version(d->ctx, minimum);
    SSL_CTX_set_max_proto_version(d->ctx, maximum);
    SSL_CTX_set_options(d->ctx, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);

    PeerVerifyMode verify = config.peerVerifyMode();
    if (verify == AutoVerifyPeer)
        verify = mode == SslClientMode ? VerifyPeer : VerifyNone;

    if (verify != VerifyNone) {
        if (config.usesSystemCaCertificates())
            SSL_CTX_set_default_verify_paths(d->ctx);
        X509_STORE *store = SSL_CTX_get_cert_store(d->ctx);
        for (const QSslCertificate &ca : config.caCertificates()) {
            X509 *x = unwrapCertificate(ca);
            if (x) {
                X509_STORE_add_cert(store, x);
                if (mode == SslServerMode)
                    SSL_CTX_add_client_CA(d->ctx, x);
                X509_free(x);
            }
        }
        int flags = SSL_VERIFY_PEER;
        if (verify == VerifyPeer && mode == SslServerMode)
            flags |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        SSL_CTX_set_verify(d->ctx, flags, verifyCallback);
        if (config.peerVerifyDepth() > 0)
            SSL_CTX_set_verify_depth(d->ctx, config.peerVerifyDepth());
    } else {
        SSL_CTX_set_verify(d->ctx, SSL_VERIFY_NONE, nullptr);
    }

    const QList<QSslCertificate> chain = config.localCertificateChain();
    if (!chain.isEmpty()) {
        X509 *leaf = unwrapCertificate(chain.first());
        const QByteArray keyDer = config.privateKey().toDer();
        const auto *keyBytes = reinterpret_cast<const unsigned char *>(keyDer.constData());
        EVP_PKEY *key =
            keyDer.isEmpty() ? nullptr : d2i_AutoPrivateKey(nullptr, &keyBytes, static_cast<long>(keyDer.size()));
        const bool ok = leaf && key && SSL_CTX_use_certificate(d->ctx, leaf) == 1 &&
                        SSL_CTX_use_PrivateKey(d->ctx, key) == 1 && SSL_CTX_check_private_key(d->ctx) == 1;
        X509_free(leaf);
        EVP_PKEY_free(key);
        if (!ok) {
            failHandshake(SslInvalidUserDataError, QString("The private key does not match the certificate"));
            return;
        }
        for (qsizetype i = 1; i < chain.size(); ++i) {
            X509 *extra = unwrapCertificate(chain.at(i));
            if (extra && SSL_CTX_add_extra_chain_cert(d->ctx, extra) != 1)
                X509_free(extra); // the context takes ownership on success
        }
    }

    d->alpnWire = wireProtocols(config.allowedNextProtocols());
    if (!d->alpnWire.isEmpty() && mode == SslServerMode)
        SSL_CTX_set_alpn_select_cb(d->ctx, alpnSelect, &d->alpnWire);

    d->ssl = SSL_new(d->ctx);
    d->in = BIO_new(BIO_s_mem());
    d->out = BIO_new(BIO_s_mem());
    if (!d->ssl || !d->in || !d->out) {
        failHandshake(SslInternalError, lastSslError("Could not create a TLS session"));
        return;
    }
    SSL_set_bio(d->ssl, d->in, d->out);
    SSL_set_ex_data(d->ssl, exDataIndex(), &d->found);
    d->found.clear();
    d->handshakeErrors.clear();

    if (mode == SslClientMode) {
        SSL_set_connect_state(d->ssl);
        if (!d->alpnWire.isEmpty())
            SSL_set_alpn_protos(d->ssl, reinterpret_cast<const unsigned char *>(d->alpnWire.constData()),
                                static_cast<unsigned int>(d->alpnWire.size()));

        QString name = d->peerVerifyName.isEmpty() ? peerName() : d->peerVerifyName;
        const QHostAddress literal(name);
        const QByteArray utf8 = name.toUtf8();
        if (verify == VerifyPeer && !name.isEmpty()) {
            SSL_set_hostflags(d->ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            const int ok = literal.isNull() ? SSL_set1_host(d->ssl, utf8.constData())
                                            : X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(d->ssl), utf8.constData());
            if (ok != 1) {
                failHandshake(SslInternalError, QString("Could not set the expected host name"));
                return;
            }
        }
        if (literal.isNull() && !name.isEmpty()) // server name indication takes host names only
            SSL_ctrl(d->ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name,
                     const_cast<char *>(utf8.constData()));
    } else {
        SSL_set_accept_state(d->ssl);
    }

    if (d->mode != mode) {
        d->mode = mode;
        modeChanged(mode);
    }
    drive();
}

void QSslSocket::drive() {
    if (d->driving) // a slot called back in; the outer loop picks up the work
        return;
    d->driving = true;
    QPointer<QSslSocket> self(this);

    if (d->ssl && !d->encrypted) {
        ERR_clear_error();
        const int rc = SSL_do_handshake(d->ssl);
        flushOut(0);
        if (rc == 1) {
            finishHandshake();
            if (!self)
                return;
        } else {
            const int e = SSL_get_error(d->ssl, rc);
            if (e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE) {
                d->driving = false;
                const QString text = lastSslError("The TLS handshake failed");
                failHandshake(SslHandshakeFailedError, text);
                return;
            }
        }
    }

    if (d->ssl && d->encrypted) {
        QByteArray plain;
        char chunk[16384];
        bool peerClosed = false;
        for (;;) {
            ERR_clear_error();
            const int n = SSL_read(d->ssl, chunk, sizeof chunk);
            if (n > 0) {
                plain.append(chunk, n);
                continue;
            }
            const int e = SSL_get_error(d->ssl, n);
            if (e == SSL_ERROR_ZERO_RETURN)
                peerClosed = true;
            else if (e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE) {
                d->driving = false;
                if (!plain.isEmpty()) {
                    deliverData(plain.constData(), plain.size());
                    if (!self)
                        return;
                }
                setSocketError(SslInternalError, lastSslError("TLS protocol error"));
                if (self)
                    abort();
                return;
            }
            break;
        }
        flushOut(0); // key updates and alerts
        if (peerClosed && !d->closeSent) {
            d->closeSent = true;
            SSL_shutdown(d->ssl); // answers the peer's close_notify
            flushOut(0);
        }
        d->driving = false;
        if (!plain.isEmpty())
            deliverData(plain.constData(), plain.size());
        return;
    }
    d->driving = false;
}

void QSslSocket::flushOut(qint64 plainBytes) {
    if (!d->out)
        return;
    QByteArray cipher;
    char chunk[16384];
    int n;
    while (BIO_ctrl_pending(d->out) > 0 && (n = BIO_read(d->out, chunk, sizeof chunk)) > 0)
        cipher.append(chunk, n);
    if (cipher.isEmpty())
        return;
    d->records.push_back({cipher.size(), plainBytes});
    d->encryptedToWrite += cipher.size();
    QTcpSocket::writeData(cipher.constData(), cipher.size());
}

void QSslSocket::finishHandshake() {
    QPointer<QSslSocket> self(this);
    QSslConfiguration &config = d->configuration;

    PeerVerifyMode verify = config.peerVerifyMode();
    if (verify == AutoVerifyPeer)
        verify = d->mode == SslClientMode ? VerifyPeer : VerifyNone;

    X509 *peer = SSL_get_peer_certificate(d->ssl);
    QList<QSslCertificate> chain;
    if (peer)
        chain.append(wrapCertificate(peer));
    if (STACK_OF(X509) *stack = SSL_get_peer_cert_chain(d->ssl)) {
        for (int i = 0; i < sk_X509_num(stack); ++i) {
            const QSslCertificate c = wrapCertificate(sk_X509_value(stack, i));
            if (!chain.contains(c))
                chain.append(c);
        }
    }
    X509_free(peer);
    config.m_peerChain = chain;
    config.m_sessionProtocol = protocolFromVersion(SSL_version(d->ssl));
    if (const SSL_CIPHER *cipher = SSL_get_current_cipher(d->ssl)) {
        int supported = 0;
        const int used = SSL_CIPHER_get_bits(cipher, &supported);
        config.m_cipher = QSslCipher(QString::fromUtf8(SSL_CIPHER_get_name(cipher)), used, supported,
                                     QString::fromUtf8(SSL_get_version(d->ssl)), config.m_sessionProtocol);
    }
    const unsigned char *alpn = nullptr;
    unsigned int alpnLength = 0;
    SSL_get0_alpn_selected(d->ssl, &alpn, &alpnLength);
    config.m_negotiatedAlpn = alpn ? QByteArray(reinterpret_cast<const char *>(alpn), alpnLength) : QByteArray();

    QList<QSslError> errors;
    if (verify == VerifyPeer) {
        errors = d->found;
        if (chain.isEmpty())
            errors.append(QSslError(QSslError::NoPeerCertificate));
        // Without a name to compare, nothing proves who the peer is.
        if (d->mode == SslClientMode && d->peerVerifyName.isEmpty() && peerName().isEmpty())
            errors.append(QSslError(QSslError::HostNameMismatch, config.peerCertificate()));
    }
    d->handshakeErrors = errors;

    if (!errors.isEmpty()) {
        for (const QSslError &e : errors) {
            peerVerifyError(e);
            if (!self)
                return;
        }
        sslErrors(errors);
        if (!self)
            return;
        bool accepted = d->ignoreAll;
        if (!accepted) {
            accepted = true;
            for (const QSslError &e : errors)
                accepted = accepted && d->ignoreList.contains(e);
        }
        if (!accepted) {
            d->driving = false;
            failHandshake(SslHandshakeFailedError, errors.first().errorString());
            return;
        }
    }

    d->encrypted = true;
    encryptPending();
    if (d->waiting)
        endWaitForReadyRead();
    encrypted();
    if (!self)
        return;
}

void QSslSocket::encryptPending() {
    if (d->pendingPlain.isEmpty() || !d->ssl)
        return;
    const QByteArray data = d->pendingPlain;
    d->pendingPlain.clear();
    writeData(data.constData(), data.size());
}

void QSslSocket::failHandshake(SocketError error, const QString &text) {
    QPointer<QSslSocket> self(this);
    flush(); // lets a TLS alert reach the peer
    setSocketError(error, text);
    if (self)
        abort();
}

/*!
    \reimp
*/
qint64 QSslSocket::writeData(const char *data, qint64 maxSize) {
    if (d->mode == UnencryptedMode)
        return QTcpSocket::writeData(data, maxSize);
    if (state() == UnconnectedState)
        return -1;
    if (!d->encrypted) { // held back until the peer has been verified
        d->pendingPlain.append(data, maxSize);
        return maxSize;
    }
    if (!d->ssl)
        return -1;
    qint64 done = 0;
    while (done < maxSize) {
        const int chunk = static_cast<int>(qMin<qint64>(maxSize - done, 1 << 24));
        ERR_clear_error();
        const int n = SSL_write(d->ssl, data + done, chunk);
        if (n <= 0) {
            setSocketError(SslInternalError, lastSslError("Could not encrypt data"));
            return done > 0 ? done : -1;
        }
        done += n;
    }
    flushOut(done);
    return done;
}

/*!
    \reimp
*/
void QSslSocket::receivedRaw(const char *data, qint64 size) {
    if (d->mode == UnencryptedMode) {
        QTcpSocket::receivedRaw(data, size);
        return;
    }
    if (!d->in) // the handshake starts when the application asks for it
        return;
    BIO_write(d->in, data, static_cast<int>(size));
    drive();
}

/*!
    \reimp
*/
void QSslSocket::socketBytesWritten(qint64 size) {
    if (d->mode == UnencryptedMode && d->records.empty()) {
        QTcpSocket::socketBytesWritten(size);
        return;
    }
    QPointer<QSslSocket> self(this);
    d->encryptedToWrite -= size;
    encryptedBytesWritten(size);
    qint64 plain = 0;
    qint64 left = size;
    while (self && left > 0 && !d->records.empty()) {
        Private::Record &r = d->records.front();
        const qint64 used = qMin(left, r.cipher);
        r.cipher -= used;
        left -= used;
        if (r.cipher == 0) {
            plain += r.plain;
            d->records.pop_front();
        }
    }
    if (self && plain > 0)
        bytesWritten(plain);
}

/*!
    \reimp
*/
void QSslSocket::remoteClosed() {
    if (d->mode != UnencryptedMode && !d->encrypted && d->ssl) {
        QPointer<QSslSocket> self(this);
        setSocketError(SslHandshakeFailedError,
                       QString("The remote host closed the connection during the TLS handshake"));
        if (self)
            finishDisconnect();
        return;
    }
    QTcpSocket::remoteClosed();
}

/*!
    \reimp
*/
void QSslSocket::connectionLost(int error) {
    if (d->mode != UnencryptedMode && !d->encrypted && d->ssl) {
        remoteClosed(); // a reset during the handshake is a failed handshake
        return;
    }
    QTcpSocket::connectionLost(error);
}

/*!
    \reimp
*/
void QSslSocket::disconnectFromHost() {
    if (d->ssl && d->encrypted && !d->closeSent) {
        d->closeSent = true;
        ERR_clear_error();
        SSL_shutdown(d->ssl);
        flushOut(0);
    }
    QTcpSocket::disconnectFromHost();
}

/*!
    \reimp
*/
void QSslSocket::close() {
    if (d->ssl && d->encrypted && !d->closeSent) {
        d->closeSent = true;
        ERR_clear_error();
        SSL_shutdown(d->ssl);
        flushOut(0);
        flush();
    }
    QTcpSocket::close();
}

/*!
    \reimp
*/
qint64 QSslSocket::bytesToWrite() const { return QTcpSocket::bytesToWrite() + d->pendingPlain.size(); }

/*!
    Returns the socket's mode.

    \sa modeChanged(), isEncrypted()
*/
QSslSocket::SslMode QSslSocket::mode() const { return d->mode; }

/*!
    Returns \c true once the handshake has finished and the peer has been
    verified.

    \sa encrypted()
*/
bool QSslSocket::isEncrypted() const { return d->encrypted; }

/*!
    Returns the allowed TLS versions.

    \sa setProtocol()
*/
QSsl::SslProtocol QSslSocket::protocol() const { return d->configuration.protocol(); }

/*!
    Sets the allowed TLS versions to \a protocol, for the next handshake.

    \sa protocol()
*/
void QSslSocket::setProtocol(QSsl::SslProtocol protocol) { d->configuration.setProtocol(protocol); }

/*!
    Returns how the peer is verified.

    \sa setPeerVerifyMode()
*/
QSslSocket::PeerVerifyMode QSslSocket::peerVerifyMode() const { return d->configuration.peerVerifyMode(); }

/*!
    Sets how the peer is verified to \a mode. Using VerifyNone on a client
    removes the protection against impersonation.

    \sa peerVerifyMode()
*/
void QSslSocket::setPeerVerifyMode(PeerVerifyMode mode) { d->configuration.setPeerVerifyMode(mode); }

/*!
    Returns the longest certificate chain accepted; \c 0 means no limit.

    \sa setPeerVerifyDepth()
*/
int QSslSocket::peerVerifyDepth() const { return d->configuration.peerVerifyDepth(); }

/*!
    Sets the longest certificate chain accepted to \a depth.

    \sa peerVerifyDepth()
*/
void QSslSocket::setPeerVerifyDepth(int depth) { d->configuration.setPeerVerifyDepth(depth); }

/*!
    Returns the name the peer's certificate is checked against.

    \sa setPeerVerifyName()
*/
QString QSslSocket::peerVerifyName() const { return d->peerVerifyName; }

/*!
    Sets the name the peer's certificate is checked against to \a hostName, for
    when it differs from the name connected to.

    \sa peerVerifyName()
*/
void QSslSocket::setPeerVerifyName(const QString &hostName) { d->peerVerifyName = hostName; }

/*!
    Returns the certificate this side presents.

    \sa setLocalCertificate()
*/
QSslCertificate QSslSocket::localCertificate() const { return d->configuration.localCertificate(); }

/*!
    Sets the certificate this side presents to \a certificate.

    \sa localCertificate(), setPrivateKey()
*/
void QSslSocket::setLocalCertificate(const QSslCertificate &certificate) {
    d->configuration.setLocalCertificate(certificate);
}

/*!
    Returns the private key.

    \sa setPrivateKey()
*/
QSslKey QSslSocket::privateKey() const { return d->configuration.privateKey(); }

/*!
    Sets the private key of the local certificate to \a key.

    \sa privateKey()
*/
void QSslSocket::setPrivateKey(const QSslKey &key) { d->configuration.setPrivateKey(key); }

/*!
    Returns the configuration, including details of the session once the
    handshake has finished.

    \sa setSslConfiguration()
*/
QSslConfiguration QSslSocket::sslConfiguration() const { return d->configuration; }

/*!
    Sets the configuration to \a configuration, for the next handshake.

    \sa sslConfiguration()
*/
void QSslSocket::setSslConfiguration(const QSslConfiguration &configuration) { d->configuration = configuration; }

/*!
    Returns the peer's certificate, or a null certificate if it has not been
    received. It stays available after the connection closes.

    \sa peerCertificateChain()
*/
QSslCertificate QSslSocket::peerCertificate() const { return d->configuration.peerCertificate(); }

/*!
    Returns the peer's certificate chain, leaf first.

    \sa peerCertificate()
*/
QList<QSslCertificate> QSslSocket::peerCertificateChain() const { return d->configuration.peerCertificateChain(); }

/*!
    Returns the cipher in use, or a null cipher before the handshake finishes.
*/
QSslCipher QSslSocket::sessionCipher() const { return d->configuration.sessionCipher(); }

/*!
    Returns the TLS version in use, or UnknownProtocol before the handshake
    finishes.
*/
QSsl::SslProtocol QSslSocket::sessionProtocol() const { return d->configuration.sessionProtocol(); }

/*!
    Returns the problems found during the last handshake.

    \sa sslErrors()
*/
QList<QSslError> QSslSocket::sslHandshakeErrors() const { return d->handshakeErrors; }

/*!
    Returns the number of encrypted bytes waiting to be decrypted. Nt decrypts
    data as it arrives, so this is always \c 0.
*/
qint64 QSslSocket::encryptedBytesAvailable() const { return 0; }

/*!
    Returns the number of encrypted bytes waiting to reach the operating system.
*/
qint64 QSslSocket::encryptedBytesToWrite() const { return d->encryptedToWrite; }

/*!
    Continues the connection despite the \a errors listed, and no others. Call
    this before connecting, or from a slot connected to sslErrors().

    \sa sslErrors()
*/
void QSslSocket::ignoreSslErrors(const QList<QSslError> &errors) { d->ignoreList = errors; }

/*!
    \overload

    Continues the connection despite every verification problem. Call this from
    a slot connected to sslErrors(), after looking at the errors. It removes the
    protection TLS gives against impersonation.
*/
void QSslSocket::ignoreSslErrors() { d->ignoreAll = true; }

/*!
    Waits up to \a msecs milliseconds for the handshake to finish, processing
    socket events meanwhile. A negative value waits forever. Returns \c true if
    the connection is encrypted.

    \sa encrypted()
*/
bool QSslSocket::waitForEncrypted(int msecs) {
    if (d->encrypted)
        return true;
    if (d->mode == UnencryptedMode)
        return false;
    QPointer<QSslSocket> self(this);
    QElapsedTimer t;
    t.start();
    d->waiting = true;
    while (self && !d->encrypted && state() != UnconnectedState) {
        const int left = msecs < 0 ? -1 : static_cast<int>(qMax<qint64>(msecs - t.elapsed(), 0));
        const bool ok = state() == ConnectingState ? waitForConnected(left) : QTcpSocket::waitForReadyRead(left);
        if (!self)
            return false;
        if (!ok && !d->encrypted)
            break;
    }
    d->waiting = false;
    return d->encrypted;
}

/*!
    Returns \c true. QSslSocket exists only in builds with OpenSSL.
*/
bool QSslSocket::supportsSsl() { return true; }

/*!
    Returns the version number of the OpenSSL library in use.

    \sa sslLibraryVersionString()
*/
long QSslSocket::sslLibraryVersionNumber() { return static_cast<long>(OpenSSL_version_num()); }

/*!
    Returns the version of the OpenSSL library in use, as text.

    \sa sslLibraryVersionNumber()
*/
QString QSslSocket::sslLibraryVersionString() { return QString::fromUtf8(OpenSSL_version(OPENSSL_VERSION)); }

/*!
    Returns the version number of the OpenSSL headers Nt was built with.

    \sa sslLibraryBuildVersionString()
*/
long QSslSocket::sslLibraryBuildVersionNumber() { return OPENSSL_VERSION_NUMBER; }

/*!
    Returns the version of the OpenSSL headers Nt was built with, as text.

    \sa sslLibraryBuildVersionNumber()
*/
QString QSslSocket::sslLibraryBuildVersionString() { return QString::fromUtf8(OPENSSL_VERSION_TEXT); }

#include "moc_qsslsocket.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
