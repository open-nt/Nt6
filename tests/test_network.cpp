// MIT License - see LICENSE
#include <QtCore>
#include <QtNetwork>

#include <cstdio>
#include <memory>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

// Runs the loop until quit() or the timeout; returns false on timeout.
static bool runLoop(QEventLoop &loop, int timeoutMs = 5000) {
    bool timedOut = false;
    QTimer t;
    t.setSingleShot(true);
    QObject::connect(&t, &QTimer::timeout, [&] { timedOut = true; loop.quit(); });
    t.start(timeoutMs);
    loop.exec();
    return !timedOut;
}

static void testHostAddress() {
    QHostAddress a("192.168.1.10");
    CHECK(!a.isNull() && a.protocol() == QAbstractSocket::IPv4Protocol);
    CHECK(a.toString() == "192.168.1.10" && a.toIPv4Address() == 0xC0A8010Au);
    CHECK(QHostAddress(0x7f000001u) == QHostAddress::LocalHost && QHostAddress(QHostAddress::LocalHost).isLoopback());
    QHostAddress v6("::1");
    CHECK(v6.protocol() == QAbstractSocket::IPv6Protocol && v6 == QHostAddress::LocalHostIPv6 && v6.isLoopback());
    CHECK(QHostAddress("2001:db8::1").toString() == "2001:db8::1");
    QHostAddress mapped("::ffff:10.0.0.1");
    bool ok = false;
    CHECK(mapped.toIPv4Address(&ok) == 0x0A000001u && ok);
    CHECK(mapped.isEqual(QHostAddress("10.0.0.1"), QHostAddress::ConvertV4MappedToIPv4) && !(mapped == QHostAddress("10.0.0.1")));
    CHECK(QHostAddress("not an address").isNull());
    QHostAddress bad;
    CHECK(!bad.setAddress("999.1.1.1") && bad.isNull());
    CHECK(QHostAddress("224.0.0.1").isMulticast() && QHostAddress("8.8.8.8").isGlobal() && !QHostAddress("10.1.1.1").isGlobal());
    CHECK(QHostAddress(QHostAddress::Any).protocol() == QAbstractSocket::AnyIPProtocol);
    CHECK(QHostAddress(QHostAddress::Broadcast).toString() == "255.255.255.255");
}

static void testAsyncEcho() {
    QTcpServer server;
    CHECK(server.listen(QHostAddress::LocalHost, 0));
    CHECK(server.isListening() && server.serverPort() != 0 && server.serverAddress() == QHostAddress::LocalHost);

    QList<QAbstractSocket::SocketState> states;
    QTcpSocket client;
    QObject::connect(&client, &QAbstractSocket::stateChanged,
                     [&](QAbstractSocket::SocketState s) { states.append(s); });

    QEventLoop loop;
    QTcpSocket *accepted = nullptr;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        accepted = server.nextPendingConnection();
        QObject::connect(accepted, &QIODevice::readyRead, [accepted] {
            while (accepted->canReadLine())
                accepted->write(accepted->readLine().toUpper());
        });
        QObject::connect(accepted, &QAbstractSocket::disconnected, &loop, &QEventLoop::quit);
    });

    QByteArray reply;
    bool connected = false;
    QObject::connect(&client, &QAbstractSocket::connected, [&] {
        connected = true;
        client.write("hello\n");
        client.write("world\n");
    });
    QObject::connect(&client, &QIODevice::readyRead, [&] {
        reply += client.readAll();
        if (reply.endsWith("WORLD\n"))
            client.disconnectFromHost();
    });

    client.connectToHost(QHostAddress::LocalHost, server.serverPort());
    CHECK(client.state() == QAbstractSocket::ConnectingState || client.state() == QAbstractSocket::ConnectedState);
    CHECK(runLoop(loop));
    CHECK(connected);
    CHECK(reply == "HELLO\nWORLD\n");
    CHECK(client.state() == QAbstractSocket::UnconnectedState);
    CHECK(states.size() >= 3 && states.first() == QAbstractSocket::ConnectingState &&
          states.contains(QAbstractSocket::ConnectedState) && states.last() == QAbstractSocket::UnconnectedState);
    CHECK(accepted && accepted->parent() == &server);
}

static void testLargeTransfer() {
    QTcpServer server;
    CHECK(server.listen(QHostAddress::LocalHost));
    QEventLoop loop;
    qint64 received = 0;
    quint32 checksum = 0;
    QTcpSocket *peer = nullptr;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        peer = server.nextPendingConnection();
        QObject::connect(peer, &QIODevice::readyRead, [&] {
            const QByteArray d = peer->readAll();
            received += d.size();
            for (char c : d)
                checksum = checksum * 31 + static_cast<unsigned char>(c);
        });
        QObject::connect(peer, &QAbstractSocket::disconnected, &loop, &QEventLoop::quit);
    });

    QByteArray payload(3 * 1024 * 1024, '\0');
    quint32 expected = 0;
    for (qsizetype i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>(i * 7 + (i >> 8));
        expected = expected * 31 + static_cast<unsigned char>(payload[i]);
    }

    QTcpSocket client;
    qint64 reportedWritten = 0;
    QObject::connect(&client, &QIODevice::bytesWritten, [&](qint64 n) { reportedWritten += n; });
    QObject::connect(&client, &QAbstractSocket::connected, [&] {
        client.write(payload);
        client.disconnectFromHost(); // must flush everything before closing
    });
    client.connectToHost("127.0.0.1", server.serverPort());
    CHECK(runLoop(loop, 15000));
    CHECK(received == payload.size());
    CHECK(checksum == expected);
    CHECK(reportedWritten == payload.size());
}

static void testRefused() {
    quint16 port;
    {
        QTcpServer s;
        CHECK(s.listen(QHostAddress::LocalHost));
        port = s.serverPort();
    }
    QTcpSocket client;
    QEventLoop loop;
    QAbstractSocket::SocketError err = QAbstractSocket::UnknownSocketError;
    QObject::connect(&client, &QAbstractSocket::errorOccurred, [&](QAbstractSocket::SocketError e) {
        err = e;
        loop.quit();
    });
    client.connectToHost(QHostAddress::LocalHost, port);
    CHECK(runLoop(loop));
    CHECK(err == QAbstractSocket::ConnectionRefusedError);
    CHECK(client.error() == QAbstractSocket::ConnectionRefusedError && client.state() == QAbstractSocket::UnconnectedState);
    CHECK(!client.errorString().isEmpty() && !client.isOpen());
}

static void testBlockingApi() {
    QTcpServer server;
    CHECK(server.listen(QHostAddress::LocalHost));
    bool timedOut = true;
    CHECK(!server.waitForNewConnection(10, &timedOut) && timedOut);

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, server.serverPort());
    CHECK(client.waitForConnected(2000) && client.state() == QAbstractSocket::ConnectedState);
    CHECK(server.waitForNewConnection(2000, &timedOut) && !timedOut);
    QTcpSocket *s = server.nextPendingConnection();
    CHECK(s && s->state() == QAbstractSocket::ConnectedState);
    CHECK(s->localPort() == server.serverPort() && s->peerPort() == client.localPort());
    CHECK(client.peerPort() == server.serverPort() && client.peerAddress() == QHostAddress::LocalHost);

    CHECK(client.write("ping") == 4);
    CHECK(client.bytesToWrite() == 4 && client.waitForBytesWritten(1000));
    CHECK(s->waitForReadyRead(2000) && s->readAll() == "ping");
    CHECK(!s->waitForReadyRead(20) && s->error() == QAbstractSocket::SocketTimeoutError);

    s->write("pong\nrest");
    CHECK(s->waitForBytesWritten(1000));
    CHECK(client.waitForReadyRead(2000));
    CHECK(client.canReadLine() || client.waitForReadyRead(200));
    CHECK(client.readLine() == "pong\n");
    CHECK(client.peek(2) == "re" && client.bytesAvailable() == 4);

    client.disconnectFromHost();
    CHECK(client.state() == QAbstractSocket::UnconnectedState);
    CHECK(s->waitForDisconnected(2000));
    CHECK(s->state() == QAbstractSocket::UnconnectedState && s->error() == QAbstractSocket::RemoteHostClosedError);

    // Writing while not connected is an error.
    CHECK(client.write("x") == -1);
}

static void testDualStackAndNames() {
    QTcpServer server;
    CHECK(server.listen(QHostAddress::Any, 0));
    QTcpSocket c4;
    c4.connectToHost("localhost", server.serverPort());
    CHECK(c4.waitForConnected(3000));
    CHECK(server.waitForNewConnection(2000));
    QTcpSocket *s = server.nextPendingConnection();
    CHECK(s && (s->peerAddress().isEqual(QHostAddress::LocalHost, QHostAddress::ConvertV4MappedToIPv4) ||
                s->peerAddress().isLoopback()));
    CHECK(c4.peerName() == "localhost");

    QTcpServer second;
    CHECK(!second.listen(QHostAddress::LocalHost, server.serverPort()) &&
          second.serverError() == QAbstractSocket::AddressInUseError && !second.errorString().isEmpty());
}

static void testUdp() {
    QUdpSocket a, b;
    CHECK(a.bind(QHostAddress::LocalHost, 0));
    CHECK(a.state() == QAbstractSocket::BoundState && a.localPort() != 0 && a.isOpen());
    CHECK(!a.hasPendingDatagrams());

    CHECK(b.writeDatagram(QByteArray("datagram-1"), QHostAddress::LocalHost, a.localPort()) == 10);
    CHECK(a.waitForReadyRead(2000));
    CHECK(a.hasPendingDatagrams() && a.pendingDatagramSize() == 10);
    QNetworkDatagram d = a.receiveDatagram();
    CHECK(d.isValid() && d.data() == "datagram-1" && d.senderAddress() == QHostAddress::LocalHost);
    CHECK(d.senderPort() == b.localPort() && d.destinationPort() == a.localPort());
    CHECK(!a.hasPendingDatagrams());

    // reply through the datagram API, read with the C-style API
    CHECK(a.writeDatagram(d.makeReply("answer")) == 6);
    CHECK(b.waitForReadyRead(2000));
    char buf[32];
    QHostAddress from;
    quint16 fromPort = 0;
    CHECK(b.readDatagram(buf, sizeof buf, &from, &fromPort) == 6 && std::memcmp(buf, "answer", 6) == 0);
    CHECK(fromPort == a.localPort() && from == QHostAddress::LocalHost);

    // async delivery through readyRead
    QEventLoop loop;
    QList<QByteArray> got;
    QObject::connect(&a, &QIODevice::readyRead, [&] {
        while (a.hasPendingDatagrams())
            got.append(a.receiveDatagram().data());
        if (got.size() == 3)
            loop.quit();
    });
    for (int i = 0; i < 3; ++i)
        b.writeDatagram(QByteArray::number(i), QHostAddress::LocalHost, a.localPort());
    CHECK(runLoop(loop));
    CHECK(got.size() == 3 && got[0] == "0" && got[2] == "2");
    QObject::disconnect(&a, &QIODevice::readyRead);

    // connected UDP socket uses write()/read()
    QUdpSocket c;
    c.connectToHost(QHostAddress::LocalHost, a.localPort());
    CHECK(c.waitForConnected(1000) && c.write("via-connect") == 11);
    CHECK(a.waitForReadyRead(2000) && a.receiveDatagram().data() == "via-connect");

    // two sockets cannot share a port without ShareAddress
    QUdpSocket dup;
    CHECK(!dup.bind(QHostAddress::LocalHost, a.localPort()) && dup.error() == QAbstractSocket::AddressInUseError);
}

static void testSocketLifetime() {
    // deleting sockets and servers from inside their own signals must be safe
    QTcpServer server;
    CHECK(server.listen(QHostAddress::LocalHost));
    QEventLoop loop;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        QTcpSocket *s = server.nextPendingConnection();
        QObject::connect(s, &QAbstractSocket::disconnected, s, &QObject::deleteLater);
        QObject::connect(s, &QObject::destroyed, &loop, &QEventLoop::quit);
    });
    auto *client = new QTcpSocket;
    QObject::connect(client, &QAbstractSocket::connected, [client] { client->disconnectFromHost(); });
    QObject::connect(client, &QAbstractSocket::disconnected, client, &QObject::deleteLater);
    client->connectToHost(QHostAddress::LocalHost, server.serverPort());
    CHECK(runLoop(loop));
    QCoreApplication::processEvents();
}

static void testHostInfo() {
    const QHostInfo local = QHostInfo::fromName("localhost");
    CHECK(local.error() == QHostInfo::NoError && !local.addresses().isEmpty());
    bool loopback = false;
    for (const QHostAddress &a : local.addresses())
        loopback = loopback || a.isLoopback();
    CHECK(loopback);
    CHECK(QHostInfo::fromName("127.0.0.1").addresses().first() == QHostAddress(QHostAddress::LocalHost));
    CHECK(QHostInfo::fromName(QString()).error() == QHostInfo::HostNotFound);
    CHECK(QHostInfo::fromName("no-such-host.invalid").error() != QHostInfo::NoError);
    CHECK(!QHostInfo::localHostName().isEmpty());

    // The result arrives from the event loop, not inside lookupHost().
    QEventLoop loop;
    bool delivered = false, insideCall = true;
    int reported = 0;
    const int id = QHostInfo::lookupHost("localhost", [&](const QHostInfo &info) {
        delivered = !insideCall && !info.addresses().isEmpty();
        reported = info.lookupId();
        loop.quit();
    });
    insideCall = false;
    CHECK(runLoop(loop));
    CHECK(delivered && reported == id);

    // A destroyed context and an aborted lookup both suppress the callback.
    int calls = 0;
    {
        QObject context;
        QHostInfo::lookupHost("localhost", &context, [&](const QHostInfo &) { ++calls; });
    }
    const int aborted = QHostInfo::lookupHost("localhost", [&](const QHostInfo &) { ++calls; });
    QHostInfo::abortHostLookup(aborted);
    QHostInfo::lookupHost("localhost", [&](const QHostInfo &) { loop.quit(); });
    QTimer settle;
    settle.setSingleShot(true);
    CHECK(runLoop(loop));
    QTimer::singleShot(100, [&] { loop.quit(); });
    CHECK(runLoop(loop));
    CHECK(calls == 0);
}

static void testNetworkInterface() {
    const QList<QNetworkInterface> all = QNetworkInterface::allInterfaces();
    CHECK(!all.isEmpty());
    bool foundLoopback = false;
    for (const QNetworkInterface &i : all) {
        CHECK(i.isValid() && i.index() > 0);
        CHECK(QNetworkInterface::interfaceNameFromIndex(i.index()) == i.name());
        CHECK(QNetworkInterface::interfaceIndexFromName(i.name()) == i.index());
        if (i.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            foundLoopback = true;
            CHECK(i.type() == QNetworkInterface::Loopback);
            for (const QNetworkAddressEntry &e : i.addressEntries()) {
                if (e.ip() == QHostAddress(QHostAddress::LocalHost)) {
                    CHECK(e.prefixLength() == 8);
                    CHECK(e.netmask() == QHostAddress("255.0.0.0"));
                }
            }
        }
    }
    CHECK(foundLoopback);
    CHECK(QNetworkInterface::allAddresses().contains(QHostAddress(QHostAddress::LocalHost)));
    CHECK(!QNetworkInterface::interfaceFromName("no-such-interface").isValid());

    QNetworkAddressEntry v4;
    v4.setIp(QHostAddress("10.0.0.1"));
    v4.setPrefixLength(20);
    CHECK(v4.netmask() == QHostAddress("255.255.240.0") && v4.prefixLength() == 20);
    QNetworkAddressEntry v6;
    v6.setIp(QHostAddress("fe80::1"));
    v6.setPrefixLength(64);
    CHECK(v6.prefixLength() == 64 && v6.netmask() == QHostAddress("ffff:ffff:ffff:ffff::"));
}

static void testLocalSockets() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString name = dir.filePath("echo.sock");

    QLocalSocket missing;
    QLocalSocket::LocalSocketError seen = QLocalSocket::UnknownSocketError;
    QObject::connect(&missing, &QLocalSocket::errorOccurred, [&](QLocalSocket::LocalSocketError e) { seen = e; });
    missing.connectToServer(name);
    CHECK(seen == QLocalSocket::ServerNotFoundError && missing.state() == QLocalSocket::UnconnectedState);

    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    CHECK(server.listen(name) && server.isListening() && server.fullServerName() == name);
    CHECK(QFileInfo(name).exists());
    QLocalServer clash;
    CHECK(!clash.listen(name));

    QObject::connect(&server, &QLocalServer::newConnection, [&] {
        QLocalSocket *peer = server.nextPendingConnection();
        QObject::connect(peer, &QIODevice::readyRead, [peer] { peer->write(peer->readAll().toUpper()); });
        QObject::connect(peer, &QLocalSocket::disconnected, peer, &QObject::deleteLater);
    });

    QEventLoop loop;
    QLocalSocket client;
    QByteArray reply;
    int disconnects = 0;
    QObject::connect(&client, &QIODevice::readyRead, [&] {
        reply += client.readAll();
        if (reply.size() >= 5)
            client.disconnectFromServer();
    });
    QObject::connect(&client, &QLocalSocket::disconnected, [&] {
        ++disconnects;
        loop.quit();
    });
    client.connectToServer(name);
    CHECK(client.state() == QLocalSocket::ConnectedState && client.waitForConnected());
    CHECK(client.write("hello") == 5);
    CHECK(runLoop(loop));
    CHECK(reply == "HELLO" && disconnects == 1 && client.state() == QLocalSocket::UnconnectedState);

    // Blocking API, and a payload larger than the socket buffers.
    QLocalServer blocking;
    const QString blockingName = dir.filePath("blocking.sock");
    CHECK(blocking.listen(blockingName));
    QLocalSocket a;
    a.connectToServer(blockingName);
    CHECK(blocking.waitForNewConnection(2000));
    std::unique_ptr<QLocalSocket> b(blocking.nextPendingConnection());
    b->setParent(nullptr);
    CHECK(b && !blocking.hasPendingConnections());
    const QByteArray big(4 * 1024 * 1024, 'x');
    a.write(big);
    QByteArray got;
    while (got.size() < big.size()) {
        a.flush();
        if (b->waitForReadyRead(2000))
            got += b->readAll();
        else
            break;
    }
    CHECK(got == big);

    // Data sent before the peer closes stays readable afterwards.
    b->write("last words\n");
    CHECK(b->waitForBytesWritten(2000));
    b->disconnectFromServer();
    CHECK(a.waitForDisconnected(2000));
    CHECK(a.state() == QLocalSocket::UnconnectedState && a.error() == QLocalSocket::PeerClosedError);
    CHECK(a.canReadLine() && a.readAll() == "last words\n");

    blocking.close();
    CHECK(!QFileInfo(blockingName).exists());

    QFile stale(dir.filePath("stale.sock"));
    CHECK(stale.open(QIODevice::WriteOnly));
    stale.close();
    QLocalServer recover;
    CHECK(!recover.listen(stale.fileName()));
    CHECK(QLocalServer::removeServer(stale.fileName()));
    CHECK(recover.listen(stale.fileName()));
}

// A scripted HTTP server: maps a request path to raw bytes sent in pieces.
class TestHttpServer : public QObject {
public:
    TestHttpServer() {
        CHECK(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, this, [this] { accept(); });
    }
    QString url(const QString &path) const {
        return QString("http://127.0.0.1:") + QString::number(server.serverPort()) + path;
    }

    QTcpServer server;
    QHash<QByteArray, QList<QByteArray>> script;
    QList<QByteArray> requests;

private:
    void accept() {
        QTcpSocket *s = server.nextPendingConnection();
        auto buffer = std::make_shared<QByteArray>();
        QObject::connect(s, &QIODevice::readyRead, this, [this, s, buffer] {
            *buffer += s->readAll();
            const qsizetype end = buffer->indexOf("\r\n\r\n");
            if (end < 0)
                return;
            const QByteArray head = buffer->left(end).toLower();
            const qsizetype cl = head.indexOf("content-length: ");
            const qsizetype bodySize = cl < 0 ? 0 : buffer->mid(cl + 16, 12).trimmed().split('\r').first().toInt();
            if (buffer->size() < end + 4 + bodySize)
                return;
            requests.append(*buffer);
            const QByteArray line = buffer->left(buffer->indexOf('\r'));
            const QByteArray path = line.mid(line.indexOf(' ') + 1, line.lastIndexOf(' ') - line.indexOf(' ') - 1);
            buffer->clear();
            respond(s, script.value(path), 0);
        });
    }
    void respond(QTcpSocket *s, QList<QByteArray> parts, int index) {
        if (index >= parts.size()) {
            s->disconnectFromHost();
            return;
        }
        if (parts.at(index) == "HANG")
            return;
        s->write(parts.at(index));
        QTimer::singleShot(2, s, [this, s, parts, index] { respond(s, parts, index + 1); });
    }
};

static QByteArray fetch(QNetworkReply *reply, QNetworkReply::NetworkError *error, int *errorSignals = nullptr) {
    QEventLoop loop;
    QByteArray body;
    if (errorSignals)
        QObject::connect(reply, &QNetworkReply::errorOccurred, [errorSignals] { ++*errorSignals; });
    QObject::connect(reply, &QIODevice::readyRead, [&] { body += reply->readAll(); });
    QObject::connect(reply, &QNetworkReply::finished, [&] { loop.quit(); });
    CHECK(runLoop(loop));
    body += reply->readAll();
    *error = reply->error();
    return body;
}

static void testHttp() {
    using R = QNetworkReply;
    TestHttpServer http;
    QNetworkAccessManager manager;
    R::NetworkError e = R::NoError;
    auto &sc = http.script;

    sc["/plain"] = {"HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-Multi: a\r\nx-multi: b\r\n\r\nhe", "llo"};
    int doneSignals = 0;
    QObject::connect(&manager, &QNetworkAccessManager::finished, [&] { ++doneSignals; });
    R *r = manager.get(QNetworkRequest(QUrl(http.url("/plain"))));
    CHECK(r->isRunning() && !r->isFinished());
    qint64 lastProgress = 0, totalProgress = 0;
    QObject::connect(r, &R::downloadProgress, [&](qint64 got, qint64 total) {
        lastProgress = got;
        totalProgress = total;
    });
    int metaData = 0;
    QObject::connect(r, &R::metaDataChanged, [&] { ++metaData; });
    CHECK(fetch(r, &e) == "hello");
    CHECK(e == R::NoError && r->isFinished() && r->httpStatusCode() == 200 && r->httpReasonPhrase() == "OK");
    CHECK(r->rawHeader("x-multi") == "a, b" && r->rawHeader("CONTENT-LENGTH") == "5" && metaData == 1);
    CHECK(lastProgress == 5 && totalProgress == 5 && doneSignals == 1);
    CHECK(http.requests.last().startsWith("GET /plain HTTP/1.1\r\n"));
    CHECK(http.requests.last().contains("Host: 127.0.0.1:"));
    r->deleteLater();

    // Chunked, split across writes inside sizes and terminators; extensions and trailers.
    sc["/chunked"] = {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5;ext=1\r", "\nhel", "lo\r\n",
                      "6\r\n world\r\n0\r\nX-Trailer: y\r\n\r\n"};
    r = manager.get(QNetworkRequest(QUrl(http.url("/chunked"))));
    CHECK(fetch(r, &e) == "hello world" && e == R::NoError);
    r->deleteLater();

    sc["/close"] = {"HTTP/1.0 200 OK\r\n\r\nuntil ", "close"};
    r = manager.get(QNetworkRequest(QUrl(http.url("/close"))));
    CHECK(fetch(r, &e) == "until close" && e == R::NoError);
    r->deleteLater();

    // Interim responses are skipped; a 404's body stays readable.
    sc["/interim"] = {"HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"};
    r = manager.get(QNetworkRequest(QUrl(http.url("/interim"))));
    CHECK(fetch(r, &e) == "ok" && r->httpStatusCode() == 200);
    r->deleteLater();
    sc["/missing"] = {"HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\n\r\nnope"};
    r = manager.get(QNetworkRequest(QUrl(http.url("/missing"))));
    int errorSignals = 0;
    CHECK(fetch(r, &e, &errorSignals) == "nope" && e == R::ContentNotFoundError && errorSignals == 1);
    r->deleteLater();
    const struct {
        const char *status;
        R::NetworkError expected;
    } statuses[] = {{"401 A", R::AuthenticationRequiredError}, {"403 A", R::ContentAccessDenied},
                    {"500 A", R::InternalServerError},         {"503 A", R::ServiceUnavailableError},
                    {"418 A", R::UnknownContentError},         {"599 A", R::UnknownServerError}};
    for (const auto &st : statuses) {
        sc["/status"] = {QByteArray("HTTP/1.1 ") + st.status + "\r\nContent-Length: 0\r\n\r\n"};
        r = manager.get(QNetworkRequest(QUrl(http.url("/status"))));
        fetch(r, &e);
        CHECK(e == st.expected);
        r->deleteLater();
    }

    // HEAD and 204 have no body, whatever Content-Length says.
    sc["/head"] = {"HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n", "HANG"};
    r = manager.head(QNetworkRequest(QUrl(http.url("/head"))));
    CHECK(fetch(r, &e).isEmpty() && e == R::NoError);
    r->deleteLater();
    sc["/nocontent"] = {"HTTP/1.1 204 No Content\r\n\r\n", "HANG"};
    r = manager.deleteResource(QNetworkRequest(QUrl(http.url("/nocontent"))));
    CHECK(fetch(r, &e).isEmpty() && e == R::NoError);
    CHECK(http.requests.last().startsWith("DELETE "));
    r->deleteLater();

    // Upload, encoded target, custom verbs and caller headers.
    sc["/up%20load?a=1%262"] = {"HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"};
    QNetworkRequest post(QUrl(http.url("/up%20load?a=1%262")));
    post.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
    post.setRawHeader("X-Custom", "yes");
    r = manager.post(post, QByteArray("payload"));
    qint64 sent = 0;
    QObject::connect(r, &R::uploadProgress, [&](qint64 s, qint64) { sent = s; });
    fetch(r, &e);
    CHECK(e == R::NoError && r->httpStatusCode() == 201 && sent == 7);
    CHECK(http.requests.last().startsWith("POST /up%20load?a=1%262 HTTP/1.1\r\n"));
    CHECK(http.requests.last().contains("Content-Length: 7\r\n") && http.requests.last().contains("X-Custom: yes\r\n"));
    CHECK(http.requests.last().endsWith("\r\n\r\npayload"));
    r->deleteLater();
    sc["/patch"] = {"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"};
    QBuffer device;
    device.setData(QByteArray("dev"));
    device.open(QIODevice::ReadOnly);
    r = manager.sendCustomRequest(QNetworkRequest(QUrl(http.url("/patch"))), "PATCH", "abc");
    fetch(r, &e);
    CHECK(e == R::NoError && http.requests.last().startsWith("PATCH /patch "));
    r->deleteLater();
    r = manager.put(QNetworkRequest(QUrl(http.url("/patch"))), &device);
    fetch(r, &e);
    CHECK(e == R::NoError && http.requests.last().startsWith("PUT ") && http.requests.last().endsWith("dev"));
    r->deleteLater();

    // Redirects: relative, chain, method rewriting, limits and policy.
    sc["/r1"] = {"HTTP/1.1 302 Found\r\nLocation: r2?x=1\r\nContent-Length: 3\r\n\r\nold"};
    sc["/r2?x=1"] = {"HTTP/1.1 301 Moved\r\nLocation: /final\r\nContent-Length: 0\r\n\r\n"};
    sc["/final"] = {"HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\ndone"};
    r = manager.get(QNetworkRequest(QUrl(http.url("/r1"))));
    int hops = 0;
    QObject::connect(r, &R::redirected, [&](const QUrl &) { ++hops; });
    CHECK(fetch(r, &e) == "done" && e == R::NoError && hops == 2);
    CHECK(r->url().path() == "/final" && r->request().url().path() != "");
    r->deleteLater();

    sc["/see-other"] = {"HTTP/1.1 303 See Other\r\nLocation: /final\r\nContent-Length: 0\r\n\r\n"};
    r = manager.post(QNetworkRequest(QUrl(http.url("/see-other"))), QByteArray("body"));
    CHECK(fetch(r, &e) == "done");
    CHECK(http.requests.last().startsWith("GET /final ") && !http.requests.last().contains("body"));
    r->deleteLater();
    sc["/keep"] = {"HTTP/1.1 307 Temporary\r\nLocation: /final\r\nContent-Length: 0\r\n\r\n"};
    r = manager.post(QNetworkRequest(QUrl(http.url("/keep"))), QByteArray("body"));
    CHECK(fetch(r, &e) == "done");
    CHECK(http.requests.last().startsWith("POST /final ") && http.requests.last().endsWith("body"));
    r->deleteLater();

    sc["/loop"] = {"HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n"};
    QNetworkRequest looping(QUrl(http.url("/loop")));
    looping.setMaximumRedirectsAllowed(3);
    r = manager.get(looping);
    fetch(r, &e);
    CHECK(e == R::TooManyRedirectsError);
    r->deleteLater();

    QNetworkRequest manual(QUrl(http.url("/r1")));
    manual.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    r = manager.get(manual);
    CHECK(fetch(r, &e) == "old" && e == R::NoError && r->httpStatusCode() == 302);
    CHECK(r->header(QNetworkRequest::LocationHeader) == "r2?x=1");
    r->deleteLater();

    // Credentials stay on their origin.
    TestHttpServer other;
    other.script["/x"] = {"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"};
    sc["/cross"] = {QByteArray("HTTP/1.1 302 Found\r\nLocation: ") + other.url("/x").toUtf8() +
                    "\r\nContent-Length: 0\r\n\r\n"};
    QNetworkRequest secret(QUrl(http.url("/cross")));
    secret.setRawHeader("Authorization", "Bearer secret");
    secret.setRawHeader("Cookie", "session=1");
    r = manager.get(secret);
    CHECK(fetch(r, &e) == "ok" && e == R::NoError);
    CHECK(http.requests.last().contains("Bearer secret"));
    CHECK(!other.requests.last().contains("secret") && !other.requests.last().contains("session"));
    r->deleteLater();
    QNetworkRequest sameOrigin(QUrl(http.url("/cross")));
    sameOrigin.setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
    r = manager.get(sameOrigin);
    fetch(r, &e);
    CHECK(e == R::InsecureRedirectError);
    r->deleteLater();
    sc["/to-ftp"] = {"HTTP/1.1 302 Found\r\nLocation: ftp://example.com/\r\nContent-Length: 0\r\n\r\n"};
    r = manager.get(QNetworkRequest(QUrl(http.url("/to-ftp"))));
    fetch(r, &e);
    CHECK(e == R::InsecureRedirectError);
    r->deleteLater();
    QUrl withUser(http.url("/final"));
    withUser.setUserName("ann");
    withUser.setPassword("p:w");
    r = manager.get(QNetworkRequest(withUser));
    CHECK(fetch(r, &e) == "done");
    CHECK(http.requests.last().contains("Authorization: Basic " + QByteArray("ann:p:w").toBase64() + "\r\n"));
    r->deleteLater();

    // Truncated and malformed responses must not look like success.
    const struct {
        const char *path;
        QByteArray raw;
        R::NetworkError expected;
    } bad[] = {
        {"/short", "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc", R::RemoteHostClosedError},
        {"/short-chunk", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nab", R::RemoteHostClosedError},
        {"/no-last-chunk", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nab\r\n",
         R::RemoteHostClosedError},
        {"/empty", "", R::RemoteHostClosedError},
        {"/partial-head", "HTTP/1.1 200 OK\r\nConte", R::RemoteHostClosedError},
        {"/garbage", "not http at all\r\n\r\n", R::ProtocolFailure},
        {"/bad-code", "HTTP/1.1 2x0 OK\r\n\r\n", R::ProtocolFailure},
        {"/bad-header", "HTTP/1.1 200 OK\r\nno colon here\r\n\r\n", R::ProtocolFailure},
        {"/bad-name", "HTTP/1.1 200 OK\r\nBad Name: x\r\n\r\n", R::ProtocolFailure},
        {"/neg-length", "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n", R::ProtocolFailure},
        {"/two-lengths", "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nab", R::ProtocolFailure},
        {"/big-length", "HTTP/1.1 200 OK\r\nContent-Length: 99999999999999999999999\r\n\r\n", R::ProtocolFailure},
        {"/bad-chunk", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n", R::ProtocolFailure},
        {"/huge-chunk", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFFFFFFFFFFFF\r\n",
         R::ProtocolFailure},
        {"/no-crlf-after-chunk", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nabc\r\n",
         R::ProtocolFailure},
        {"/bad-location", "HTTP/1.1 302 Found\r\nLocation: http://[bad/\r\nContent-Length: 0\r\n\r\n",
         R::ProtocolFailure},
    };
    for (const auto &b : bad) {
        sc[b.path] = {b.raw};
        r = manager.get(QNetworkRequest(QUrl(http.url(QString::fromUtf8(b.path)))));
        fetch(r, &e);
        if (e != b.expected)
            std::printf("  %s gave %d\n", b.path, int(e));
        CHECK(e == b.expected);
        r->deleteLater();
    }
    sc["/huge-headers"] = {"HTTP/1.1 200 OK\r\n", QByteArray("X: ") + QByteArray(2 * 1024 * 1024, 'a'), "HANG"};
    r = manager.get(QNetworkRequest(QUrl(http.url("/huge-headers"))));
    fetch(r, &e);
    CHECK(e == R::ProtocolFailure);
    r->deleteLater();

    // Bad requests never reach the network.
    const qsizetype before = http.requests.size();
    QNetworkRequest injected(QUrl(http.url("/final")));
    injected.setRawHeader("X-Evil", "a\r\nX-Injected: 1");
    r = manager.get(injected);
    fetch(r, &e);
    CHECK(e == R::ProtocolInvalidOperationError);
    r->deleteLater();
    r = manager.sendCustomRequest(QNetworkRequest(QUrl(http.url("/final"))), "GET /x HTTP/1.1\r\n\r\n");
    fetch(r, &e);
    CHECK(e == R::ProtocolInvalidOperationError);
    r->deleteLater();
    CHECK(http.requests.size() == before);
    QUrl newline(http.url("/final"));
    newline.setPath("/a\r\nX-Injected: 1");
    r = manager.get(QNetworkRequest(newline));
    fetch(r, &e);
    CHECK(http.requests.last().contains("/a%0D%0AX-Injected:%201 HTTP/1.1\r\n"));
    r->deleteLater();

    // Failures before any response.
    r = manager.get(QNetworkRequest(QUrl("ftp://example.com/")));
    bool sync = true;
    QObject::connect(r, &R::finished, [&] { CHECK(!sync); });
    sync = false;
    fetch(r, &e);
    CHECK(e == R::ProtocolUnknownError && !r->errorString().isEmpty());
    r->deleteLater();
    r = manager.get(QNetworkRequest(QUrl("http://no-such-host.invalid/")));
    fetch(r, &e);
    CHECK(e == R::HostNotFoundError);
    r->deleteLater();
    r = manager.get(QNetworkRequest(QUrl()));
    fetch(r, &e);
    CHECK(e == R::ProtocolUnknownError);
    r->deleteLater();
    quint16 closedPort;
    {
        QTcpServer temporary;
        CHECK(temporary.listen(QHostAddress::LocalHost));
        closedPort = temporary.serverPort();
    }
    r = manager.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:") + QString::number(closedPort) + QString("/"))));
    fetch(r, &e);
    CHECK(e == R::ConnectionRefusedError);
    r->deleteLater();
    r = manager.get(QNetworkRequest(
        QUrl(QString("http://localhost:") + QString::number(http.server.serverPort()) + QString("/final"))));
    CHECK(fetch(r, &e) == "done" && e == R::NoError);
    r->deleteLater();

    // Timeouts and aborts, each finishing exactly once.
    sc["/hang"] = {"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nab", "HANG"};
    QNetworkRequest slow(QUrl(http.url("/hang")));
    slow.setTransferTimeout(50);
    r = manager.get(slow);
    int finishes = 0;
    QObject::connect(r, &R::finished, [&] { ++finishes; });
    CHECK(fetch(r, &e) == "ab" && e == R::TimeoutError && finishes == 1);
    r->deleteLater();
    r = manager.get(QNetworkRequest(QUrl(http.url("/hang"))));
    QObject::connect(r, &R::metaDataChanged, r, [r] { r->abort(); });
    fetch(r, &e);
    CHECK(e == R::OperationCanceledError);
    r->deleteLater();
    r = manager.get(QNetworkRequest(QUrl(http.url("/hang"))));
    r->abort();
    r->abort();
    finishes = 0;
    QObject::connect(r, &R::finished, [&] { ++finishes; });
    fetch(r, &e);
    CHECK(e == R::OperationCanceledError && finishes == 1);
    r->deleteLater();

    // Deleting a running reply, and a slot deleting its reply, are both safe.
    delete manager.get(QNetworkRequest(QUrl(http.url("/hang"))));
    QEventLoop loop;
    R *self = manager.get(QNetworkRequest(QUrl(http.url("/plain"))));
    QObject::connect(self, &QIODevice::readyRead, [&] {
        delete self;
        self = nullptr;
        QTimer::singleShot(20, [&] { loop.quit(); });
    });
    CHECK(runLoop(loop));
    CHECK(self == nullptr);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testHostAddress();
    testAsyncEcho();
    testLargeTransfer();
    testRefused();
    testBlockingApi();
    testDualStackAndNames();
    testUdp();
    testSocketLifetime();
    testHostInfo();
    testNetworkInterface();
    testLocalSockets();
    testHttp();
    if (g_failures == 0)
        std::puts("all network tests passed");
    return g_failures == 0 ? 0 : 1;
}
