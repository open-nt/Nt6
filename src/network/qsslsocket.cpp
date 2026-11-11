// MIT License - see LICENSE
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslsocket.h>

#include "qtlsbackend_p.h"

#include <deque>

struct QSslSocket::Private {
    std::unique_ptr<QtPrivate::Tls::Session> session; // the TLS connection, while there is one
    QSslConfiguration configuration;
    SslMode mode = UnencryptedMode;
    bool encrypted = false;
    bool autoStart = false;
    bool ignoreAll = false;
    bool waiting = false;
    bool driving = false;
    bool closeSent = false;
    QString peerVerifyName;
    QList<QSslError> ignoreList;
    QList<QSslError> handshakeErrors;
    QByteArray pendingPlain; // written before the handshake finished
    struct Record {
        qint64 cipher;
        qint64 plain;
    };
    std::deque<Record> records; // maps encrypted bytes back to bytesWritten()
    qint64 encryptedToWrite = 0;
};

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
    Requires OpenSSL 1.1.1 or newer, or Mbed TLS 4 where Nt is built for Zephyr; when Nt
    is built without either, these classes are absent and \c QT_NO_SSL is defined. Only
    TLS 1.2 and 1.3 are available. \c connectToHostEncrypted() resolves the name
    synchronously, as QAbstractSocket::connectToHost() does. The library is chosen when Nt
    is built, so there are no backend-selection functions, and no \c setCiphers(), PSK,
    OCSP or session resumption API. A peer that closes without a TLS shutdown
    is reported as an ordinary disconnect, so a protocol that delimits data by
    closing the connection cannot detect truncation from this class.

    On Zephyr the class is built with \c CONFIG_NT_TLS, on Mbed TLS. Mbed TLS has no
    configuration files, so setBackendConfigurationOption() always fails the handshake there;
    it takes the system's certificate authorities from no directory, so
    QSslConfiguration::addCaCertificate() has to name the ones to trust; and its cipher suites
    are named as Mbed TLS names them, such as \c TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256. See
    \l{Nt on Zephyr}.
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
    d->session.reset();
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
    if (state() != ConnectedState || d->session || d->encrypted)
        return;
    const QSslConfiguration &config = d->configuration;

    if (mode == SslServerMode && (config.localCertificate().isNull() || config.privateKey().isNull())) {
        failHandshake(SslInvalidUserDataError, QString("Cannot provide a certificate with no key"));
        return;
    }

    PeerVerifyMode verify = config.peerVerifyMode();
    if (verify == AutoVerifyPeer)
        verify = mode == SslClientMode ? VerifyPeer : VerifyNone;

    QtPrivate::Tls::SessionSetup setup;
    setup.server = mode == SslServerMode;
    setup.protocol = config.protocol();
    setup.verify = verify;
    setup.verifyDepth = config.peerVerifyDepth();
    if (mode == SslClientMode)
        setup.peerName = d->peerVerifyName.isEmpty() ? peerName() : d->peerVerifyName;
    setup.localChain = config.localCertificateChain();
    setup.privateKey = config.privateKey().toDer();
    setup.caCertificates = config.caCertificates();
    setup.systemCaCertificates = config.usesSystemCaCertificates();
    setup.ciphersSet = config.m_ciphersSet;
    setup.ciphers = config.m_ciphers;
    setup.nextProtocols = config.allowedNextProtocols();
    setup.backendOptions = config.m_backend;

    SocketError error = SslInternalError;
    QString text;
    d->session = QtPrivate::Tls::createSession(setup, &error, &text);
    if (!d->session) {
        failHandshake(error, text);
        return;
    }
    d->handshakeErrors.clear();

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

    using QtPrivate::Tls::Session;
    if (d->session && !d->encrypted) {
        QString text;
        const Session::Status status = d->session->handshake(&text);
        flushOut(0);
        if (status == Session::Done) {
            finishHandshake();
            if (!self)
                return;
        } else if (status == Session::Failed) {
            d->driving = false;
            failHandshake(SslHandshakeFailedError, text);
            return;
        }
    }

    if (d->session && d->encrypted) {
        QByteArray plain;
        QString text;
        const Session::Status status = d->session->read(&plain, &text);
        if (status == Session::Failed) {
            d->driving = false;
            if (!plain.isEmpty()) {
                deliverData(plain.constData(), plain.size());
                if (!self)
                    return;
            }
            setSocketError(SslInternalError, text);
            if (self)
                abort();
            return;
        }
        flushOut(0); // key updates and alerts
        if (status == Session::Closed && !d->closeSent) {
            d->closeSent = true;
            d->session->shutdown(); // answers the peer's close_notify
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
    if (!d->session)
        return;
    const QByteArray cipher = d->session->takeOutput();
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

    const QList<QSslCertificate> chain = d->session->peerChain();
    config.m_peerChain = chain;
    config.m_sessionProtocol = d->session->protocol();
    config.m_cipher = d->session->cipher();
    config.m_negotiatedAlpn = d->session->nextProtocol();

    QList<QSslError> errors;
    if (verify == VerifyPeer) {
        errors = d->session->verifyErrors();
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
    if (d->pendingPlain.isEmpty() || !d->session)
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
    if (!d->session)
        return -1;
    QString text;
    const qint64 done = d->session->write(data, maxSize, &text);
    if (done < maxSize)
        setSocketError(SslInternalError, text);
    if (done < 0)
        return -1;
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
    if (!d->session) // the handshake starts when the application asks for it
        return;
    d->session->feed(data, size);
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
    if (self && plain > 0) {
        bytesWritten(plain);
        if (self)
            channelBytesWritten(0, plain);
    }
}

/*!
    \reimp
*/
void QSslSocket::remoteClosed() {
    if (d->mode != UnencryptedMode && !d->encrypted && d->session) {
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
    if (d->mode != UnencryptedMode && !d->encrypted && d->session) {
        remoteClosed(); // a reset during the handshake is a failed handshake
        return;
    }
    QTcpSocket::connectionLost(error);
}

/*!
    \reimp
*/
void QSslSocket::disconnectFromHost() {
    if (d->session && d->encrypted && !d->closeSent) {
        d->closeSent = true;
        d->session->shutdown();
        flushOut(0);
    }
    QTcpSocket::disconnectFromHost();
}

/*!
    \reimp
*/
void QSslSocket::close() {
    if (d->session && d->encrypted && !d->closeSent) {
        d->closeSent = true;
        d->session->shutdown();
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
    Returns \c true. QSslSocket exists only in builds with a TLS library: OpenSSL, or Mbed TLS on Zephyr.
*/
bool QSslSocket::supportsSsl() { return true; }

/*!
    Returns the version number of the TLS library in use, OpenSSL or Mbed TLS.

    \sa sslLibraryVersionString()
*/
long QSslSocket::sslLibraryVersionNumber() { return QtPrivate::Tls::libraryVersionNumber(); }

/*!
    Returns the version of the TLS library in use, as text.

    \sa sslLibraryVersionNumber()
*/
QString QSslSocket::sslLibraryVersionString() { return QtPrivate::Tls::libraryVersionString(); }

/*!
    Returns the version number of the headers of the TLS library that Nt was built with.

    \sa sslLibraryBuildVersionString()
*/
long QSslSocket::sslLibraryBuildVersionNumber() { return QtPrivate::Tls::libraryBuildVersionNumber(); }

/*!
    Returns the version of the headers of the TLS library that Nt was built with, as text.

    \sa sslLibraryBuildVersionNumber()
*/
QString QSslSocket::sslLibraryBuildVersionString() { return QtPrivate::Tls::libraryBuildVersionString(); }

#include "moc_qsslsocket.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
