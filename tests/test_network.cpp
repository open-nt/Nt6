#include <QtCore>
#include <QtNetwork>

#include <cstdio>

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
    CHECK(s && s->peerAddress().isEqual(QHostAddress::LocalHost, QHostAddress::ConvertV4MappedToIPv4) ||
          s->peerAddress().isLoopback());
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
    if (g_failures == 0)
        std::puts("all network tests passed");
    return g_failures == 0 ? 0 : 1;
}
