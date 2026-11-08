// MIT License - see LICENSE
#include <QtCore>
#include <QtNetwork>

#include "test_certificates.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool runLoop(QEventLoop &loop, int timeoutMs = 5000) {
    bool timedOut = false;
    QTimer t;
    t.setSingleShot(true);
    QObject::connect(&t, &QTimer::timeout, [&] {
        timedOut = true;
        loop.quit();
    });
    t.start(timeoutMs);
    loop.exec();
    return !timedOut;
}

struct Fixture {
    Identity ca = makeIdentity("Nt Test CA", nullptr, nullptr, -1, 3650, true);
    Identity good = makeIdentity("localhost", "DNS:localhost,DNS:*.nt.test,IP:127.0.0.1", &ca, -1, 365);
    Identity wrongHost = makeIdentity("other.example", "DNS:other.example", &ca, -1, 365);
    Identity wrongIp = makeIdentity("ip", "IP:127.0.0.2", &ca, -1, 365);
    Identity cnOnly = makeIdentity("127.0.0.1", nullptr, &ca, -1, 365);
    Identity expired = makeIdentity("localhost", "DNS:localhost,IP:127.0.0.1", &ca, -30, -1);
    Identity future = makeIdentity("localhost", "DNS:localhost,IP:127.0.0.1", &ca, 10, 30);
    Identity selfSigned = makeIdentity("localhost", "DNS:localhost,IP:127.0.0.1", nullptr, -1, 365);
    Identity otherCa = makeIdentity("Other CA", nullptr, nullptr, -1, 3650, true);
    Identity foreign = makeIdentity("localhost", "DNS:localhost,IP:127.0.0.1", &otherCa, -1, 365);
    Identity client = makeIdentity("client", nullptr, &ca, -1, 365);
};
static Fixture *f;

static QSslConfiguration serverConfig(const Identity &id) {
    QSslConfiguration c = QSslConfiguration::defaultConfiguration();
    c.setLocalCertificate(id.certificate);
    c.setPrivateKey(id.key);
    return c;
}

static QSslConfiguration clientConfig() {
    QSslConfiguration c = QSslConfiguration::defaultConfiguration();
    c.setCaCertificates({f->ca.certificate});
    return c;
}

// Accepts connections, records everything received and echoes it upper-cased.
class EchoServer : public QObject {
public:
    explicit EchoServer(const QSslConfiguration &config) {
        server.setSslConfiguration(config);
        CHECK(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QSslServer::newConnection, this, [this] {
            auto *s = static_cast<QSslSocket *>(server.nextPendingConnection());
            ++accepted;
            lastPeer = s->peerCertificate();
            CHECK(s->isEncrypted());
            QObject::connect(s, &QIODevice::readyRead, s, [this, s] {
                const QByteArray data = s->readAll();
                received += data;
                s->write(data.toUpper());
            });
        });
    }
    quint16 port() const { return server.serverPort(); }

    QSslServer server;
    QByteArray received;
    int accepted = 0;
    QSslCertificate lastPeer;
};

static void testCertificates() {
    const QSslCertificate &c = f->good.certificate;
    CHECK(!c.isNull() && QSslCertificate().isNull() && QSslCertificate(QByteArray("junk")).isNull());
    CHECK(c.subjectInfo(QSslCertificate::CommonName) == QStringList("localhost"));
    CHECK(c.issuerInfo(QSslCertificate::CommonName) == QStringList("Nt Test CA"));
    CHECK(c.subjectInfo(QSslCertificate::Organization) == QStringList("Nt Tests"));
    CHECK(c.subjectDisplayName() == "localhost" && c.issuerDisplayName() == "Nt Test CA");
    CHECK(c.version() == "3" && !c.serialNumber().isEmpty());
    CHECK(!c.isSelfSigned() && f->selfSigned.certificate.isSelfSigned() && f->ca.certificate.isSelfSigned());
    CHECK(QSslCertificate(c.toDer(), QSsl::Der) == c && QSslCertificate(c.toPem()) == c);
    CHECK(c.digest(QCryptographicHash::Sha256) == QCryptographicHash::hash(c.toDer(), QCryptographicHash::Sha256));
    CHECK(c.effectiveDate() < QDateTime::currentDateTimeUtc() && c.expiryDate() > QDateTime::currentDateTimeUtc());
    CHECK(f->expired.certificate.expiryDate() < QDateTime::currentDateTimeUtc());
    const auto alt = c.subjectAlternativeNames();
    CHECK(alt.values(QSsl::DnsEntry).contains("localhost") && alt.values(QSsl::DnsEntry).contains("*.nt.test"));
    CHECK(alt.values(QSsl::IpAddressEntry).contains("127.0.0.1"));
    CHECK(QSslCertificate::fromData(c.toPem() + f->ca.certificate.toPem()).size() == 2);
    QTemporaryDir dir;
    QFile file(dir.filePath("c.pem"));
    CHECK(file.open(QIODevice::WriteOnly));
    file.write(c.toPem());
    file.close();
    CHECK(QSslCertificate::fromPath(file.fileName()).first() == c);
    CHECK(QSslCertificate::fromPath(dir.filePath("missing")).isEmpty());
    CHECK(QSslCertificate(&file).isNull());

    const QSslKey &k = f->good.key;
    CHECK(!k.isNull() && k.algorithm() == QSsl::Ec && k.type() == QSsl::PrivateKey && k.length() == 256);
    CHECK(QSslKey(k.toPem(), QSsl::Ec) == k && QSslKey(k.toDer(), QSsl::Ec, QSsl::Der) == k);
    CHECK(QSslKey(f->good.keyPem, QSsl::Rsa).isNull());
    CHECK(QSslKey(QByteArray("junk"), QSsl::Ec).isNull() && QSslKey().isNull());

    BIO *b = BIO_new(BIO_s_mem());
    char pass[] = "hunter2";
    PEM_write_bio_PKCS8PrivateKey(b, f->good.pkey, EVP_aes_256_cbc(), nullptr, 0, nullptr, pass);
    char *data = nullptr;
    const long n = BIO_get_mem_data(b, &data);
    const QByteArray locked(data, n);
    BIO_free(b);
    CHECK(QSslKey(locked, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey, "hunter2") == k);
    CHECK(QSslKey(locked, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey, "wrong").isNull());
    CHECK(QSslKey(locked, QSsl::Ec).isNull());
    CHECK(!QSslError(QSslError::CertificateExpired).errorString().isEmpty());
}

static void testHandshake() {
    CHECK(QSslSocket::supportsSsl() && QSslSocket::sslLibraryVersionString().contains("OpenSSL"));

    const struct {
        QSsl::SslProtocol want, got;
        const char *text;
    } versions[] = {
        {QSsl::SecureProtocols, QSsl::TlsV1_3, "TLSv1.3"},
        {QSsl::TlsV1_3, QSsl::TlsV1_3, "TLSv1.3"},
        {QSsl::TlsV1_2, QSsl::TlsV1_2, "TLSv1.2"},
        {QSsl::TlsV1_2OrLater, QSsl::TlsV1_3, "TLSv1.3"},
    };
    for (const auto &v : versions) {
        EchoServer server(serverConfig(f->good));
        QSslSocket client;
        client.setSslConfiguration(clientConfig());
        client.setProtocol(v.want);
        QStringList order;
        QByteArray reply;
        QEventLoop loop;
        QObject::connect(&client, &QAbstractSocket::connected, [&] { order << "connected"; });
        QObject::connect(&client, &QSslSocket::encrypted, [&] { order << "encrypted"; });
        QObject::connect(&client, &QIODevice::readyRead, [&] {
            reply += client.readAll();
            if (reply.size() == 5)
                loop.quit();
        });
        QObject::connect(&client, &QSslSocket::sslErrors, [&](const QList<QSslError> &) { CHECK(false); });
        client.connectToHostEncrypted("127.0.0.1", server.port());
        CHECK(client.mode() == QSslSocket::SslClientMode && !client.isEncrypted());
        CHECK(client.write("hello") == 5); // queued until verified
        CHECK(client.bytesToWrite() == 5);
        CHECK(runLoop(loop));
        CHECK(reply == "HELLO" && server.received == "hello");
        CHECK(order == QStringList({"connected", "encrypted"}));
        CHECK(client.isEncrypted() && client.sessionProtocol() == v.got);
        CHECK(client.sessionCipher().protocolString() == v.text && client.sessionCipher().usedBits() >= 128);
        CHECK(client.peerCertificate() == f->good.certificate);
        CHECK(client.sslHandshakeErrors().isEmpty());
    }

    // Host names and wildcards, using the name overload to set what is checked.
    for (const char *name : {"localhost", "a.nt.test"}) {
        EchoServer server(serverConfig(f->good));
        QSslSocket client;
        client.setSslConfiguration(clientConfig());
        client.connectToHostEncrypted("127.0.0.1", server.port(), QString(name));
        QEventLoop loop;
        QObject::connect(&client, &QSslSocket::encrypted, [&] { loop.quit(); });
        CHECK(runLoop(loop));
    }

    // Large transfer: progress counts plaintext, and all of it arrives intact.
    QByteArray big(3 * 1024 * 1024, '\0');
    for (qsizetype i = 0; i < big.size(); ++i)
        big[i] = static_cast<char>('a' + i % 26);
    EchoServer server(serverConfig(f->good));
    QSslSocket client;
    client.setSslConfiguration(clientConfig());
    QByteArray reply;
    qint64 written = 0, encryptedWritten = 0;
    QEventLoop loop;
    QObject::connect(&client, &QIODevice::bytesWritten, [&](qint64 n) { written += n; });
    QObject::connect(&client, &QSslSocket::encryptedBytesWritten, [&](qint64 n) { encryptedWritten += n; });
    QObject::connect(&client, &QIODevice::readyRead, [&] {
        reply += client.readAll();
        if (reply.size() >= big.size())
            loop.quit();
    });
    client.connectToHostEncrypted("127.0.0.1", server.port());
    client.write(big);
    CHECK(runLoop(loop, 15000));
    CHECK(reply == big.toUpper() && written == big.size() && encryptedWritten > big.size());
    CHECK(client.encryptedBytesToWrite() == 0 && client.bytesToWrite() == 0);

    // A clean shutdown delivers everything first, and details survive it.
    QSslServer closing;
    closing.setSslConfiguration(serverConfig(f->good));
    CHECK(closing.listen(QHostAddress::LocalHost));
    QObject::connect(&closing, &QSslServer::newConnection, [&] {
        auto *s = static_cast<QSslSocket *>(closing.nextPendingConnection());
        s->write("final words\n");
        s->disconnectFromHost();
    });
    QSslSocket reader;
    reader.setSslConfiguration(clientConfig());
    QEventLoop closeLoop;
    QObject::connect(&reader, &QAbstractSocket::disconnected, [&] { closeLoop.quit(); });
    reader.connectToHostEncrypted("127.0.0.1", closing.serverPort());
    CHECK(runLoop(closeLoop));
    CHECK(reader.canReadLine() && reader.readLine() == "final words\n");
    CHECK(reader.state() == QAbstractSocket::UnconnectedState && !reader.isEncrypted());
    CHECK(reader.mode() == QSslSocket::UnencryptedMode);
    CHECK(reader.peerCertificate() == f->good.certificate);

    // The socket can be used again after closing.
    EchoServer again(serverConfig(f->good));
    QEventLoop againLoop;
    QObject::connect(&reader, &QSslSocket::encrypted, [&] { againLoop.quit(); });
    reader.connectToHostEncrypted("127.0.0.1", again.port());
    CHECK(runLoop(againLoop));
    CHECK(reader.isEncrypted());
}

struct Attempt {
    bool encrypted = false;
    QList<QSslError> errors;
    QAbstractSocket::SocketError error = QAbstractSocket::UnknownSocketError;
    QByteArray serverSaw;
    int accepted = 0;
};

// Connects a client to a server presenting `id`, writing a secret as early as it can.
static Attempt attempt(const Identity &id, const QSslConfiguration &config, const QString &name,
                       const std::function<void(QSslSocket &)> &setup = {}) {
    Attempt a;
    EchoServer server(serverConfig(id));
    QSslSocket client;
    client.setSslConfiguration(config);
    if (setup)
        setup(client);
    QEventLoop loop;
    QObject::connect(&client, &QSslSocket::encrypted, [&] { a.encrypted = true; });
    QObject::connect(&client, &QSslSocket::sslErrors, [&](const QList<QSslError> &e) { a.errors = e; });
    QObject::connect(&client, &QAbstractSocket::errorOccurred, [&](QAbstractSocket::SocketError e) {
        if (a.error == QAbstractSocket::UnknownSocketError)
            a.error = e;
        loop.quit();
    });
    QObject::connect(&client, &QIODevice::readyRead, [&] { loop.quit(); });
    client.connectToHostEncrypted("127.0.0.1", server.port(), name);
    client.write("secret");
    CHECK(runLoop(loop));
    QEventLoop settle;
    QTimer::singleShot(30, [&] { settle.quit(); });
    settle.exec();
    a.serverSaw = server.received;
    a.accepted = server.accepted;
    return a;
}

static bool hasError(const Attempt &a, QSslError::SslError e) {
    for (const QSslError &x : a.errors)
        if (x.error() == e)
            return true;
    return false;
}

static void expectRejected(const Attempt &a, QSslError::SslError e) {
    CHECK(!a.encrypted);
    CHECK(a.error == QAbstractSocket::SslHandshakeFailedError);
    CHECK(hasError(a, e));
    CHECK(a.serverSaw.isEmpty()); // the secret must never leave the client
}

static void testRejections() {
    const QSslConfiguration trust = clientConfig();
    const QString ip("127.0.0.1");

    Attempt ok = attempt(f->good, trust, ip);
    CHECK(ok.encrypted && ok.serverSaw == "secret" && ok.errors.isEmpty());

    expectRejected(attempt(f->wrongHost, trust, ip), QSslError::HostNameMismatch);
    expectRejected(attempt(f->wrongIp, trust, ip), QSslError::HostNameMismatch);
    expectRejected(attempt(f->good, trust, "evil.example"), QSslError::HostNameMismatch);
    expectRejected(attempt(f->good, trust, "a.b.nt.test"), QSslError::HostNameMismatch); // a wildcard spans one label
    expectRejected(attempt(f->good, trust, "nt.test"), QSslError::HostNameMismatch);
    // A name only in the common name does not count.
    expectRejected(attempt(f->cnOnly, trust, ip), QSslError::HostNameMismatch);
    expectRejected(attempt(f->expired, trust, ip), QSslError::CertificateExpired);
    expectRejected(attempt(f->future, trust, ip), QSslError::CertificateNotYetValid);
    expectRejected(attempt(f->selfSigned, trust, ip), QSslError::SelfSignedCertificate);
    expectRejected(attempt(f->foreign, trust, ip), QSslError::UnableToGetLocalIssuerCertificate);

    // An issuer that is not a CA cannot vouch for anyone.
    Identity fakeLeaf = makeIdentity("localhost", "DNS:localhost,IP:127.0.0.1", &f->good, -1, 365);
    Attempt fake = attempt(fakeLeaf, trust, ip);
    CHECK(!fake.encrypted && fake.serverSaw.isEmpty() && !fake.errors.isEmpty());

    // The system store is not used when a list is given, and defaults verify.
    CHECK(QSslConfiguration::defaultConfiguration().usesSystemCaCertificates());
    CHECK(!trust.usesSystemCaCertificates());
    CHECK(QSslConfiguration().peerVerifyMode() == QSslSocket::AutoVerifyPeer);
    expectRejected(attempt(f->good, QSslConfiguration::defaultConfiguration(), ip),
                   QSslError::UnableToGetLocalIssuerCertificate);

    // Ignoring: everything, exactly the right errors, or the wrong ones.
    Attempt all = attempt(f->expired, trust, ip, [](QSslSocket &s) {
        QObject::connect(&s, &QSslSocket::sslErrors, &s, [&s] { s.ignoreSslErrors(); });
    });
    CHECK(all.encrypted && all.serverSaw == "secret" && hasError(all, QSslError::CertificateExpired));
    Attempt exact = attempt(f->expired, trust, ip, [](QSslSocket &s) {
        s.ignoreSslErrors({QSslError(QSslError::CertificateExpired, f->expired.certificate)});
    });
    CHECK(exact.encrypted && exact.serverSaw == "secret");
    expectRejected(attempt(f->expired, trust, ip,
                           [](QSslSocket &s) {
                               s.ignoreSslErrors({QSslError(QSslError::CertificateExpired, f->good.certificate)});
                           }),
                   QSslError::CertificateExpired);
    expectRejected(attempt(f->expired, trust, ip,
                           [](QSslSocket &s) {
                               s.ignoreSslErrors({QSslError(QSslError::HostNameMismatch, f->expired.certificate)});
                           }),
                   QSslError::CertificateExpired);

    // Data written at any moment before verification finishes stays on the
    // client: as the handshake starts, and after it ends but before the verdict.
    expectRejected(attempt(f->expired, trust, ip,
                           [](QSslSocket &s) {
                               QObject::connect(&s, &QAbstractSocket::connected, &s,
                                                [&s] { CHECK(s.write("early") == 5); });
                           }),
                   QSslError::CertificateExpired);
    expectRejected(attempt(f->expired, trust, ip,
                           [](QSslSocket &s) {
                               QObject::connect(&s, &QSslSocket::peerVerifyError, &s,
                                                [&s] { CHECK(s.write("verdict") == 7); });
                           }),
                   QSslError::CertificateExpired);
    expectRejected(attempt(f->expired, trust, ip,
                           [](QSslSocket &s) {
                               QObject::connect(&s, &QSslSocket::sslErrors, &s,
                                                [&s] { CHECK(s.write("verdict") == 7); });
                           }),
                   QSslError::CertificateExpired);
    // The same data arrives, in order, once a connection is accepted.
    Attempt queued = attempt(f->good, trust, ip, [](QSslSocket &s) {
        QObject::connect(&s, &QAbstractSocket::connected, &s, [&s] { CHECK(s.write("early-") == 6); });
    });
    CHECK(queued.encrypted && queued.serverSaw == "secret" + QByteArray("early-"));

    // An allowance for one connection does not last into the next.
    {
        EchoServer bad(serverConfig(f->expired));
        QSslSocket client;
        client.setSslConfiguration(trust);
        client.ignoreSslErrors();
        QEventLoop loop;
        QObject::connect(&client, &QSslSocket::encrypted, [&] { client.disconnectFromHost(); });
        QObject::connect(&client, &QAbstractSocket::disconnected, [&] { loop.quit(); });
        client.connectToHostEncrypted("127.0.0.1", bad.port());
        CHECK(runLoop(loop));
        bool encrypted = false;
        QEventLoop second;
        QObject::connect(&client, &QSslSocket::encrypted, [&] { encrypted = true; });
        QObject::connect(&client, &QAbstractSocket::errorOccurred, [&] { second.quit(); });
        client.connectToHostEncrypted("127.0.0.1", bad.port());
        CHECK(runLoop(second));
        CHECK(!encrypted && client.error() == QAbstractSocket::SslHandshakeFailedError);
    }

    // VerifyNone is explicit, and QueryPeer ignores problems.
    Attempt none = attempt(f->expired, trust, ip, [](QSslSocket &s) { s.setPeerVerifyMode(QSslSocket::VerifyNone); });
    CHECK(none.encrypted && none.errors.isEmpty());
    Attempt query = attempt(f->expired, trust, ip, [](QSslSocket &s) { s.setPeerVerifyMode(QSslSocket::QueryPeer); });
    CHECK(query.encrypted);
    Attempt deep = attempt(f->good, trust, ip, [](QSslSocket &s) { s.setPeerVerifyDepth(1); });
    CHECK(deep.encrypted);

    // A server limited to TLS 1.3 and a client limited to 1.2 cannot agree.
    QSslConfiguration only13 = serverConfig(f->good);
    only13.setProtocol(QSsl::TlsV1_3);
    EchoServer modern(only13);
    QSslSocket old;
    old.setSslConfiguration(trust);
    old.setProtocol(QSsl::TlsV1_2);
    QEventLoop loop;
    QObject::connect(&old, &QAbstractSocket::errorOccurred, [&] { loop.quit(); });
    old.connectToHostEncrypted("127.0.0.1", modern.port());
    CHECK(runLoop(loop));
    CHECK(old.error() == QAbstractSocket::SslHandshakeFailedError && !old.isEncrypted());
}

static void testServer() {
    // Mutual TLS.
    QSslConfiguration strict = serverConfig(f->good);
    strict.setCaCertificates({f->ca.certificate});
    strict.setPeerVerifyMode(QSslSocket::VerifyPeer);
    {
        EchoServer server(strict);
        QSslSocket client;
        QSslConfiguration c = clientConfig();
        c.setLocalCertificate(f->client.certificate);
        c.setPrivateKey(f->client.key);
        client.setSslConfiguration(c);
        QEventLoop loop;
        QObject::connect(&client, &QIODevice::readyRead, [&] { loop.quit(); });
        client.connectToHostEncrypted("127.0.0.1", server.port());
        client.write("mtls");
        CHECK(runLoop(loop));
        CHECK(server.received == "mtls" && server.lastPeer == f->client.certificate);
    }
    {
        EchoServer server(strict);
        QSslSocket client;
        client.setSslConfiguration(clientConfig());
        QEventLoop loop;
        QObject::connect(&client, &QAbstractSocket::disconnected, [&] { loop.quit(); });
        QAbstractSocket::SocketError serverError = QAbstractSocket::UnknownSocketError;
        QObject::connect(&server.server, &QSslServer::errorOccurred,
                         [&](QSslSocket *, QAbstractSocket::SocketError e) { serverError = e; });
        client.connectToHostEncrypted("127.0.0.1", server.port());
        client.write("no cert");
        CHECK(runLoop(loop));
        CHECK(server.accepted == 0 && server.received.isEmpty());
        CHECK(serverError == QAbstractSocket::SslHandshakeFailedError);
    }
    {
        Identity stranger = makeIdentity("client", nullptr, &f->otherCa, -1, 365);
        EchoServer server(strict);
        QSslSocket client;
        QSslConfiguration c = clientConfig();
        c.setLocalCertificate(stranger.certificate);
        c.setPrivateKey(stranger.key);
        client.setSslConfiguration(c);
        QEventLoop loop;
        QObject::connect(&client, &QAbstractSocket::disconnected, [&] { loop.quit(); });
        client.connectToHostEncrypted("127.0.0.1", server.port());
        client.write("forged");
        CHECK(runLoop(loop));
        CHECK(server.accepted == 0 && server.received.isEmpty());
    }

    // A plain-text client, and one that never speaks, are both dropped.
    QSslServer server;
    server.setSslConfiguration(serverConfig(f->good));
    server.setHandshakeTimeout(60);
    CHECK(server.listen(QHostAddress::LocalHost));
    int emitted = 0;
    QAbstractSocket::SocketError seen = QAbstractSocket::UnknownSocketError;
    QEventLoop loop;
    QObject::connect(&server, &QSslServer::newConnection, [&] { ++emitted; });
    QObject::connect(&server, &QSslServer::errorOccurred, [&](QSslSocket *, QAbstractSocket::SocketError e) {
        seen = e;
        loop.quit();
    });
    QTcpSocket plain;
    QObject::connect(&plain, &QAbstractSocket::connected, [&] { plain.write("GET / HTTP/1.1\r\n\r\n"); });
    plain.connectToHost(QHostAddress::LocalHost, server.serverPort());
    CHECK(runLoop(loop));
    CHECK(seen == QAbstractSocket::SslHandshakeFailedError && emitted == 0 && !server.hasPendingConnections());
    seen = QAbstractSocket::UnknownSocketError;
    QTcpSocket silent;
    silent.connectToHost(QHostAddress::LocalHost, server.serverPort());
    CHECK(runLoop(loop));
    CHECK(seen == QAbstractSocket::SocketTimeoutError && emitted == 0);

    // A server missing its key, or with the wrong one, fails rather than serving.
    for (int i = 0; i < 2; ++i) {
        QSslConfiguration bad = serverConfig(f->good);
        bad.setPrivateKey(i == 0 ? QSslKey() : f->client.key);
        QSslServer broken;
        broken.setSslConfiguration(bad);
        CHECK(broken.listen(QHostAddress::LocalHost));
        QAbstractSocket::SocketError e = QAbstractSocket::UnknownSocketError;
        QEventLoop wait;
        QObject::connect(&broken, &QSslServer::errorOccurred, [&](QSslSocket *, QAbstractSocket::SocketError x) {
            e = x;
            wait.quit();
        });
        QTcpSocket c;
        c.connectToHost(QHostAddress::LocalHost, broken.serverPort());
        CHECK(runLoop(wait));
        CHECK(e == QAbstractSocket::SslInvalidUserDataError);
    }

    // A client talking to a server that is not speaking TLS.
    QTcpServer notTls;
    CHECK(notTls.listen(QHostAddress::LocalHost));
    QObject::connect(&notTls, &QTcpServer::newConnection,
                     [&] { notTls.nextPendingConnection()->write("220 hello, not TLS at all\r\n"); });
    QSslSocket client;
    client.setSslConfiguration(clientConfig());
    QEventLoop failed;
    QObject::connect(&client, &QAbstractSocket::errorOccurred, [&] { failed.quit(); });
    client.connectToHostEncrypted("127.0.0.1", notTls.serverPort());
    CHECK(runLoop(failed));
    CHECK(client.error() == QAbstractSocket::SslHandshakeFailedError && !client.isEncrypted());

    // A connection dropped mid-handshake is a failed handshake, not a network
    // blip, whether the peer closes politely or resets.
    for (const bool reset : {false, true}) {
        QTcpServer dropper;
        CHECK(dropper.listen(QHostAddress::LocalHost));
        QObject::connect(&dropper, &QTcpServer::newConnection, [&, reset] {
            QTcpSocket *s = dropper.nextPendingConnection();
            QObject::connect(s, &QIODevice::readyRead, s, [s, reset] {
                s->readAll();
                if (reset) {
                    const linger hard = {1, 0};
                    setsockopt(static_cast<int>(s->socketDescriptor()), SOL_SOCKET, SO_LINGER, &hard, sizeof hard);
                }
                s->abort();
            });
        });
        QSslSocket dropped;
        dropped.setSslConfiguration(clientConfig());
        QEventLoop wait;
        QObject::connect(&dropped, &QAbstractSocket::errorOccurred, [&] { wait.quit(); });
        dropped.connectToHostEncrypted("127.0.0.1", dropper.serverPort());
        dropped.write("secret");
        CHECK(runLoop(wait));
        CHECK(dropped.error() == QAbstractSocket::SslHandshakeFailedError && !dropped.isEncrypted());

        QNetworkAccessManager manager;
        QNetworkRequest request{
            QUrl(QString("https://127.0.0.1:") + QString::number(dropper.serverPort()) + QString("/"))};
        request.setSslConfiguration(clientConfig());
        QNetworkReply *reply = manager.get(request);
        QEventLoop replyLoop;
        QObject::connect(reply, &QNetworkReply::finished, [&] { replyLoop.quit(); });
        CHECK(runLoop(replyLoop));
        CHECK(reply->error() == QNetworkReply::SslHandshakeFailedError);
    }

    // ALPN.
    QSslConfiguration withAlpn = serverConfig(f->good);
    withAlpn.setAllowedNextProtocols({"h2", "http/1.1"});
    EchoServer alpnServer(withAlpn);
    QSslSocket alpn;
    QSslConfiguration alpnClient = clientConfig();
    alpnClient.setAllowedNextProtocols({"http/1.1"});
    alpn.setSslConfiguration(alpnClient);
    alpn.connectToHostEncrypted("127.0.0.1", alpnServer.port());
    QEventLoop alpnLoop;
    QObject::connect(&alpn, &QSslSocket::encrypted, [&] { alpnLoop.quit(); });
    CHECK(runLoop(alpnLoop));
    CHECK(alpn.sslConfiguration().nextNegotiatedProtocol() == "http/1.1");
}

static void testStartTls() {
    QSslSocket *serverSide = nullptr;
    QByteArray serverGot;
    QEventLoop loop;

    class Adopting : public QTcpServer {
    public:
        QSslSocket *adopted = nullptr;
        std::function<void(QSslSocket *)> onAdopt;

    protected:
        void incomingConnection(qintptr handle) override {
            adopted = new QSslSocket(this);
            adopted->setSocketDescriptor(handle);
            onAdopt(adopted);
        }
    } adopting;
    CHECK(adopting.listen(QHostAddress::LocalHost));
    adopting.onAdopt = [&](QSslSocket *s) {
        serverSide = s;
        s->setSslConfiguration(serverConfig(f->good));
        QObject::connect(s, &QIODevice::readyRead, s, [&, s] {
            const QByteArray data = s->readAll();
            serverGot += data;
            if (data == "STARTTLS\n") {
                s->write("OK\n");
                s->flush();
                s->startServerEncryption();
            } else if (s->isEncrypted()) {
                s->write(data);
            }
        });
    };

    QSslSocket client;
    client.setSslConfiguration(clientConfig());
    QByteArray got;
    QObject::connect(&client, &QAbstractSocket::connected, [&] {
        CHECK(client.mode() == QSslSocket::UnencryptedMode);
        client.write("STARTTLS\n");
    });
    QObject::connect(&client, &QIODevice::readyRead, [&] {
        const QByteArray data = client.readAll();
        if (data == "OK\n")
            client.startClientEncryption();
        else {
            got += data;
            loop.quit();
        }
    });
    QObject::connect(&client, &QSslSocket::encrypted, [&] { client.write("private"); });
    client.setPeerVerifyName("localhost");
    client.connectToHost(QHostAddress::LocalHost, adopting.serverPort());
    CHECK(runLoop(loop));
    CHECK(got == "private" && serverGot == "STARTTLS\nprivate");
    CHECK(client.isEncrypted() && serverSide && serverSide->isEncrypted());
}

// A blocking peer on its own thread, so the client can use the waitFor API.
static void testBlocking() {
    QTcpServer listener;
    CHECK(listener.listen(QHostAddress::LocalHost));
    const int fdListen = static_cast<int>(listener.socketDescriptor());
    listener.pauseAccepting();
    std::atomic<bool> served{false};
    std::thread peer([&] {
        SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_use_certificate(ctx, f->good.x509);
        SSL_CTX_use_PrivateKey(ctx, f->good.pkey);
        int fd = -1;
        for (int i = 0; i < 4000 && fd < 0; ++i) {
            fd = accept(fdListen, nullptr, nullptr);
            if (fd < 0)
                usleep(1000);
        }
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, fd);
        int rc;
        do {
            rc = SSL_accept(ssl);
            if (rc != 1)
                usleep(1000);
        } while (rc != 1 &&
                 (SSL_get_error(ssl, rc) == SSL_ERROR_WANT_READ || SSL_get_error(ssl, rc) == SSL_ERROR_WANT_WRITE));
        char buf[16];
        int n = 0;
        for (int i = 0; i < 2000 && n <= 0; ++i) {
            n = SSL_read(ssl, buf, sizeof buf);
            if (n <= 0)
                usleep(1000);
        }
        if (n > 0)
            SSL_write(ssl, buf, n);
        served = n > 0;
        for (int i = 0; i < 2000 && SSL_shutdown(ssl) == 0; ++i)
            usleep(1000);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        ::close(fd);
    });

    QSslSocket client;
    client.setSslConfiguration(clientConfig());
    client.connectToHostEncrypted("127.0.0.1", listener.serverPort());
    CHECK(client.waitForEncrypted(4000));
    CHECK(client.isEncrypted());
    client.write("ping");
    CHECK(client.waitForBytesWritten(2000));
    CHECK(client.waitForReadyRead(4000));
    CHECK(client.readAll() == "ping");
    CHECK(client.waitForDisconnected(4000));
    peer.join();
    CHECK(served);

    QTcpServer mute;
    CHECK(mute.listen(QHostAddress::LocalHost));
    QSslSocket waiting;
    waiting.setSslConfiguration(clientConfig());
    waiting.connectToHostEncrypted("127.0.0.1", mute.serverPort());
    CHECK(!waiting.waitForEncrypted(100));
    QSslSocket unencrypted;
    CHECK(!unencrypted.waitForEncrypted(10));
}

static void testLifetime() {
    // Slots may delete the socket at every stage.
    EchoServer server(serverConfig(f->good));
    EchoServer bad(serverConfig(f->expired));
    QEventLoop loop;
    {
        auto *s = new QSslSocket;
        s->setSslConfiguration(clientConfig());
        QObject::connect(s, &QSslSocket::encrypted, [&, s] {
            delete s;
            loop.quit();
        });
        s->connectToHostEncrypted("127.0.0.1", server.port());
        CHECK(runLoop(loop));
    }
    {
        auto *s = new QSslSocket;
        s->setSslConfiguration(clientConfig());
        QObject::connect(s, &QIODevice::readyRead, [&, s] {
            delete s;
            loop.quit();
        });
        s->connectToHostEncrypted("127.0.0.1", server.port());
        s->write("x");
        CHECK(runLoop(loop));
    }
    {
        auto *s = new QSslSocket;
        s->setSslConfiguration(clientConfig());
        QObject::connect(s, &QSslSocket::sslErrors, [&, s] {
            delete s;
            loop.quit();
        });
        s->connectToHostEncrypted("127.0.0.1", bad.port());
        CHECK(runLoop(loop));
    }
    {
        auto *s = new QSslSocket;
        s->setSslConfiguration(clientConfig());
        QObject::connect(s, &QSslSocket::peerVerifyError, [&, s] {
            delete s;
            loop.quit();
        });
        s->connectToHostEncrypted("127.0.0.1", bad.port());
        CHECK(runLoop(loop));
    }
    {
        auto *s = new QSslSocket;
        s->setSslConfiguration(clientConfig());
        QObject::connect(s, &QAbstractSocket::errorOccurred, [&, s] {
            delete s;
            loop.quit();
        });
        s->connectToHostEncrypted("127.0.0.1", bad.port());
        CHECK(runLoop(loop));
    }
    {
        // abort() from inside a slot, and writing after it, are safe.
        QSslSocket s;
        s.setSslConfiguration(clientConfig());
        QObject::connect(&s, &QSslSocket::encrypted, [&] {
            s.abort();
            CHECK(s.write("x") == -1);
            loop.quit();
        });
        s.connectToHostEncrypted("127.0.0.1", server.port());
        CHECK(runLoop(loop));
    }
}

class HttpsServer : public QObject {
public:
    explicit HttpsServer(const QSslConfiguration &config) {
        server.setSslConfiguration(config);
        CHECK(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QSslServer::newConnection, this, [this] {
            auto *s = static_cast<QSslSocket *>(server.nextPendingConnection());
            QObject::connect(s, &QIODevice::readyRead, s, [this, s] {
                buffer += s->readAll();
                if (buffer.indexOf("\r\n\r\n") < 0)
                    return;
                requests.append(buffer);
                const QByteArray line = buffer.left(buffer.indexOf('\r'));
                const QByteArray path = line.mid(line.indexOf(' ') + 1, line.lastIndexOf(' ') - line.indexOf(' ') - 1);
                buffer.clear();
                s->write(script.value(path, "HTTP/1.1 404 Nope\r\nContent-Length: 0\r\n\r\n"));
                s->disconnectFromHost();
            });
        });
    }
    QString url(const QString &path, const char *host = "127.0.0.1") const {
        return QString("https://") + QString(host) + QChar(u':') + QString::number(server.serverPort()) + path;
    }

    QSslServer server;
    QByteArray buffer;
    QHash<QByteArray, QByteArray> script;
    QList<QByteArray> requests;
};

static QByteArray fetch(QNetworkReply *reply, QNetworkReply::NetworkError *error) {
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, [&] { loop.quit(); });
    CHECK(runLoop(loop));
    *error = reply->error();
    return reply->readAll();
}

static void testHttps() {
    using R = QNetworkReply;
    QNetworkAccessManager manager;
    R::NetworkError e = R::NoError;
    HttpsServer https(serverConfig(f->good));
    https.script["/ok"] = "HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\nsecure";

    const auto request = [](const QString &url) {
        QNetworkRequest r{QUrl(url)};
        r.setSslConfiguration(clientConfig());
        return r;
    };

    for (const char *host : {"127.0.0.1", "localhost"}) {
        R *r = manager.get(request(https.url("/ok", host)));
        int encrypted = 0;
        QObject::connect(r, &R::encrypted, [&] { ++encrypted; });
        CHECK(fetch(r, &e) == "secure" && e == R::NoError && encrypted == 1);
        CHECK(r->sslConfiguration().peerCertificate() == f->good.certificate);
        CHECK(r->sslConfiguration().sessionProtocol() == QSsl::TlsV1_3);
        CHECK(https.requests.last().contains(QByteArray("Host: ") + host + ":"));
        delete r;
    }

    // Untrusted servers fail, and neither the request nor credentials are sent.
    HttpsServer untrusted(serverConfig(f->selfSigned));
    untrusted.script["/ok"] = https.script["/ok"];
    QNetworkRequest secret = request(untrusted.url("/ok"));
    secret.setRawHeader("Authorization", "Bearer secret");
    R *r = manager.get(secret);
    QList<QSslError> reported;
    QObject::connect(r, &R::sslErrors, [&](const QList<QSslError> &errors) { reported = errors; });
    int managerSignals = 0;
    auto connection = QObject::connect(&manager, &QNetworkAccessManager::sslErrors, [&] { ++managerSignals; });
    CHECK(fetch(r, &e).isEmpty() && e == R::SslHandshakeFailedError);
    CHECK(!reported.isEmpty() && managerSignals == 1 && untrusted.requests.isEmpty());
    QObject::disconnect(connection);
    delete r;
    r = manager.get(QNetworkRequest(QUrl(https.url("/ok")))); // only the system CAs
    fetch(r, &e);
    CHECK(e == R::SslHandshakeFailedError);
    delete r;

    // The application may accept the error, for this reply.
    r = manager.get(request(untrusted.url("/ok")));
    QObject::connect(r, &R::sslErrors, r, [r] { r->ignoreSslErrors(); });
    CHECK(fetch(r, &e) == "secure" && e == R::NoError);
    delete r;
    r = manager.get(request(untrusted.url("/ok")));
    r->ignoreSslErrors({QSslError(QSslError::SelfSignedCertificate, f->selfSigned.certificate)});
    CHECK(fetch(r, &e) == "secure" && e == R::NoError);
    delete r;
    r = manager.get(request(untrusted.url("/ok")));
    r->ignoreSslErrors({QSslError(QSslError::CertificateExpired, f->selfSigned.certificate)});
    fetch(r, &e);
    CHECK(e == R::SslHandshakeFailedError);
    delete r;

    // An accepted error does not follow a redirect to somewhere else.
    HttpsServer alsoUntrusted(serverConfig(f->selfSigned));
    alsoUntrusted.script["/stolen"] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nno";
    untrusted.script["/bounce"] = QByteArray("HTTP/1.1 302 Found\r\nLocation: ") +
                                  alsoUntrusted.url("/stolen").toUtf8() + "\r\nContent-Length: 0\r\n\r\n";
    r = manager.get(request(untrusted.url("/bounce")));
    r->ignoreSslErrors();
    QObject::connect(r, &R::sslErrors, r, [] {});
    fetch(r, &e);
    CHECK(e == R::SslHandshakeFailedError && alsoUntrusted.requests.isEmpty());
    delete r;

    // Downgrades are refused; upgrades and same-scheme redirects work.
    QTcpServer plain;
    CHECK(plain.listen(QHostAddress::LocalHost));
    const QByteArray plainUrl = QByteArray("http://127.0.0.1:") + QByteArray::number(plain.serverPort()) + "/x";
    int plainHits = 0;
    QObject::connect(&plain, &QTcpServer::newConnection, [&] {
        ++plainHits;
        QTcpSocket *s = plain.nextPendingConnection();
        QObject::connect(s, &QIODevice::readyRead, s, [&, s] {
            s->readAll();
            s->write(QByteArray("HTTP/1.1 302 Found\r\nLocation: ") + https.url("/ok").toUtf8() +
                     "\r\nContent-Length: 0\r\n\r\n");
            s->disconnectFromHost();
        });
    });
    https.script["/down"] = "HTTP/1.1 302 Found\r\nLocation: " + plainUrl + "\r\nContent-Length: 0\r\n\r\n";
    r = manager.get(request(https.url("/down")));
    fetch(r, &e);
    CHECK(e == R::InsecureRedirectError && plainHits == 0);
    delete r;
    r = manager.get(request(QString::fromUtf8(plainUrl)));
    CHECK(fetch(r, &e) == "secure" && e == R::NoError && r->url().scheme() == "https");
    delete r;
    https.script["/same"] = "HTTP/1.1 302 Found\r\nLocation: /ok\r\nContent-Length: 0\r\n\r\n";
    r = manager.get(request(https.url("/same")));
    CHECK(fetch(r, &e) == "secure");
    delete r;

    // Secrets stay on their origin across an https redirect.
    HttpsServer other(serverConfig(f->good));
    other.script["/x"] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    https.script["/cross"] =
        QByteArray("HTTP/1.1 302 Found\r\nLocation: ") + other.url("/x").toUtf8() + "\r\nContent-Length: 0\r\n\r\n";
    QNetworkRequest authorized = request(https.url("/cross"));
    authorized.setRawHeader("Authorization", "Bearer secret");
    r = manager.get(authorized);
    CHECK(fetch(r, &e) == "ok");
    CHECK(!other.requests.last().contains("secret"));
    delete r;

    // A POST over TLS, a hostile status line, and a dead server.
    https.script["/garbage"] = "not http";
    r = manager.get(request(https.url("/garbage")));
    fetch(r, &e);
    CHECK(e == R::ProtocolFailure || e == R::RemoteHostClosedError);
    delete r;
    https.script["/post"] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    r = manager.post(request(https.url("/post")), QByteArray("x"));
    fetch(r, &e);
    CHECK(e == R::NoError);
    delete r;
    QTcpServer notTls;
    CHECK(notTls.listen(QHostAddress::LocalHost));
    QObject::connect(&notTls, &QTcpServer::newConnection,
                     [&] { notTls.nextPendingConnection()->write("HTTP/1.1 200 OK\r\n\r\n"); });
    r = manager.get(request(QString("https://127.0.0.1:") + QString::number(notTls.serverPort()) + QString("/")));
    fetch(r, &e);
    CHECK(e == R::SslHandshakeFailedError);
    delete r;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    Fixture fixture;
    f = &fixture;
    testCertificates();
    testHandshake();
    testRejections();
    testServer();
    testStartTls();
    testBlocking();
    testLifetime();
    testHttps();
    if (g_failures == 0)
        std::puts("all ssl tests passed");
    return g_failures == 0 ? 0 : 1;
}
