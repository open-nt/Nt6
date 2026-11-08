// MIT License - see LICENSE
#include <QtCore>
#include <QtNetwork>
#include <QtWebSockets>

#ifdef NT_TEST_TLS
#include "test_certificates.h"
#endif

#include <cstdio>
#include <functional>
#include <memory>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using CloseCode = QWebSocketProtocol::CloseCode;

static QStringList g_warnings;
static void captureWarnings(QtMsgType type, const QMessageLogContext &, const QString &message) {
    if (type == QtWarningMsg)
        g_warnings.append(message);
    else
        std::fprintf(stderr, "%s\n", qPrintable(message));
}

// Runs the event loop until cond() holds; returns false on timeout.
static bool waitFor(const std::function<bool()> &cond, int timeoutMs = 5000) {
    if (cond())
        return true;
    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&] {
        if (cond())
            loop.quit();
    });
    poll.start(2);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();
    return cond();
}

static void spin(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

// An echo server that records what its sockets received.
struct EchoServer {
    QWebSocketServer server{QStringLiteral("test server"), QWebSocketServer::NonSecureMode};
    QList<QWebSocket *> sockets;
    QStringList texts;
    QList<QByteArray> binaries;
    QList<std::pair<QString, bool>> textFrames;
    QList<CloseCode> closeCodes;
    QStringList closeReasons;
    bool echo = true;

    EchoServer() {
        CHECK(server.listen(QHostAddress::LocalHost, 0));
        QObject::connect(&server, &QWebSocketServer::newConnection, [this] {
            while (QWebSocket *s = server.nextPendingConnection()) {
                sockets.append(s);
                QObject::connect(s, &QWebSocket::textMessageReceived, [this, s](const QString &m) {
                    texts.append(m);
                    if (echo)
                        s->sendTextMessage(m);
                });
                QObject::connect(s, &QWebSocket::binaryMessageReceived, [this, s](const QByteArray &m) {
                    binaries.append(m);
                    if (echo)
                        s->sendBinaryMessage(m);
                });
                QObject::connect(s, &QWebSocket::textFrameReceived,
                                 [this](const QString &f, bool last) { textFrames.append({f, last}); });
                QObject::connect(s, &QWebSocket::disconnected, [this, s] {
                    closeCodes.append(s->closeCode());
                    closeReasons.append(s->closeReason());
                });
            }
        });
    }
    QUrl url(const QString &path = QStringLiteral("/")) const {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
    }
};

// A client that records what it received.
struct Client {
    QWebSocket socket;
    QStringList texts;
    QList<QByteArray> binaries;
    QList<QAbstractSocket::SocketState> states;
    QList<QAbstractSocket::SocketError> errors;
    int connected = 0;
    int disconnected = 0;
    int aboutToClose = 0;

    explicit Client(const QString &origin = QString()) : socket(origin) {
        QObject::connect(&socket, &QWebSocket::textMessageReceived, [this](const QString &m) { texts.append(m); });
        QObject::connect(&socket, &QWebSocket::binaryMessageReceived,
                         [this](const QByteArray &m) { binaries.append(m); });
        QObject::connect(&socket, &QWebSocket::stateChanged,
                         [this](QAbstractSocket::SocketState s) { states.append(s); });
        QObject::connect(&socket, &QWebSocket::errorOccurred,
                         [this](QAbstractSocket::SocketError e) { errors.append(e); });
        QObject::connect(&socket, &QWebSocket::connected, [this] { ++connected; });
        QObject::connect(&socket, &QWebSocket::disconnected, [this] { ++disconnected; });
        QObject::connect(&socket, &QWebSocket::aboutToClose, [this] { ++aboutToClose; });
    }
    bool open(const QUrl &url, const QWebSocketHandshakeOptions &options = {}) {
        socket.open(url, options);
        return waitFor([this] { return connected > 0 || !errors.isEmpty(); }) && connected == 1;
    }
};

// A frame as a client sends it: masked unless told otherwise.
static QByteArray frame(quint8 opcode, const QByteArray &payload, bool fin = true, bool masked = true, quint8 rsv = 0) {
    QByteArray out;
    out += static_cast<char>((fin ? 0x80 : 0) | rsv | opcode);
    const char maskBit = masked ? static_cast<char>(0x80) : 0;
    if (payload.size() < 126) {
        out += static_cast<char>(maskBit | static_cast<char>(payload.size()));
    } else {
        out += static_cast<char>(maskBit | 126);
        out += static_cast<char>((payload.size() >> 8) & 0xff);
        out += static_cast<char>(payload.size() & 0xff);
    }
    const char key[4] = {0x12, 0x34, 0x56, 0x78};
    if (masked)
        out.append(key, 4);
    for (qsizetype i = 0; i < payload.size(); ++i)
        out += masked ? static_cast<char>(payload.at(i) ^ key[i % 4]) : payload.at(i);
    return out;
}

struct Frame {
    quint8 opcode = 0;
    bool fin = false;
    bool masked = false;
    QByteArray payload;
};

static QList<Frame> parseFrames(const QByteArray &data) {
    QList<Frame> out;
    qsizetype i = 0;
    while (data.size() - i >= 2) {
        Frame f;
        const auto b0 = static_cast<unsigned char>(data.at(i));
        const auto b1 = static_cast<unsigned char>(data.at(i + 1));
        f.fin = b0 & 0x80;
        f.opcode = b0 & 0x0f;
        f.masked = b1 & 0x80;
        qsizetype len = b1 & 0x7f;
        qsizetype pos = i + 2;
        if (len == 126) {
            len = (static_cast<unsigned char>(data.at(pos)) << 8) | static_cast<unsigned char>(data.at(pos + 1));
            pos += 2;
        }
        QByteArray key;
        if (f.masked) {
            key = data.mid(pos, 4);
            pos += 4;
        }
        if (data.size() < pos + len)
            break;
        f.payload = data.mid(pos, len);
        for (qsizetype k = 0; f.masked && k < len; ++k)
            f.payload[k] = static_cast<char>(f.payload.at(k) ^ key.at(k % 4));
        out.append(f);
        i = pos + len;
    }
    return out;
}

static int closeCodeOf(const QList<Frame> &frames) {
    for (const Frame &f : frames) {
        if (f.opcode == 0x8)
            return f.payload.size() >= 2 ? (static_cast<unsigned char>(f.payload.at(0)) << 8) |
                                               static_cast<unsigned char>(f.payload.at(1))
                                         : 1005;
    }
    return -1;
}

static const QByteArray SampleKey = "dGhlIHNhbXBsZSBub25jZQ=="; // RFC 6455, section 1.3
static const QByteArray SampleAccept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

static QByteArray upgradeRequest(quint16 port, const QByteArray &extra = {}) {
    return "GET /chat HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(port) +
           "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + SampleKey +
           "\r\nSec-WebSocket-Version: 13\r\n" + extra + "\r\n";
}

// A raw TCP client: sends request, then frames once the handshake answer arrived, and returns what the
// server sent until it closed the connection.
static QByteArray rawExchange(quint16 port, const QByteArray &request, const QByteArray &afterHandshake = {},
                              bool *closed = nullptr, int timeoutMs = 3000) {
    QTcpSocket s;
    QByteArray got;
    bool sentFrames = false;
    bool disconnected = false;
    QObject::connect(&s, &QAbstractSocket::connected, [&] { s.write(request); });
    QObject::connect(&s, &QIODevice::readyRead, [&] {
        got += s.readAll();
        if (!sentFrames && got.contains("\r\n\r\n") && !afterHandshake.isEmpty()) {
            sentFrames = true;
            s.write(afterHandshake);
        }
    });
    QObject::connect(&s, &QAbstractSocket::disconnected, [&] { disconnected = true; });
    s.connectToHost(QHostAddress::LocalHost, port);
    waitFor([&] { return disconnected; }, timeoutMs);
    got += s.readAll();
    if (closed)
        *closed = disconnected;
    return got;
}

static QByteArray afterHead(const QByteArray &response) {
    const qsizetype end = response.indexOf("\r\n\r\n");
    return end < 0 ? QByteArray() : response.mid(end + 4);
}

static void testEcho() {
    EchoServer server;
    CHECK(server.server.isListening() && server.server.serverUrl().scheme() == "ws");
    CHECK(server.server.serverUrl().port() == server.server.serverPort());
    Client client(QStringLiteral("http://example.org"));
    CHECK(client.socket.state() == QAbstractSocket::UnconnectedState && !client.socket.isValid());
    CHECK(client.open(server.url(QStringLiteral("/chat?room=1"))));
    CHECK(client.socket.isValid() && client.socket.state() == QAbstractSocket::ConnectedState);
    CHECK(client.states ==
          (QList<QAbstractSocket::SocketState>{QAbstractSocket::ConnectingState, QAbstractSocket::ConnectedState}));
    CHECK(client.socket.resourceName() == "/chat?room=1" && client.socket.peerPort() == server.server.serverPort());
    CHECK(client.socket.version() == QWebSocketProtocol::Version13 && client.socket.subprotocol().isEmpty());

    CHECK(waitFor([&] { return server.sockets.size() == 1; }));
    QWebSocket *peer = server.sockets.value(0);
    CHECK(peer && peer->parent() == &server.server && peer->origin() == "http://example.org");
    CHECK(peer && peer->resourceName() == "/chat?room=1" && peer->requestUrl().scheme() == "ws");
    CHECK(peer && peer->request().rawHeader("sec-websocket-version") == "13");
    CHECK(peer && peer->peerAddress() == QHostAddress::LocalHost && peer->isValid());
    CHECK(!server.server.hasPendingConnections() && server.server.nextPendingConnection() == nullptr);

    const QString text = QString::fromUtf8("h\xc3\xa9llo \xf0\x9f\x8c\x8d");
    CHECK(client.socket.sendTextMessage(text) == text.toUtf8().size());
    CHECK(client.socket.sendBinaryMessage(QByteArray("\x00\x01\x02", 3)) == 3);
    CHECK(client.socket.sendTextMessage(QString()) == 0);
    CHECK(waitFor([&] { return client.texts.size() == 2 && client.binaries.size() == 1; }));
    CHECK(client.texts.value(0) == text && client.texts.value(1).isEmpty() && server.texts.value(0) == text);
    CHECK(client.binaries.value(0) == QByteArray("\x00\x01\x02", 3));

    // A large message travels in many frames.
    QByteArray big(3 * 1024 * 1024 + 7, '\0');
    for (qsizetype i = 0; i < big.size(); ++i)
        big[i] = static_cast<char>(i * 31);
    client.socket.sendBinaryMessage(big);
    CHECK(waitFor([&] { return client.binaries.size() == 2; }, 10000) && client.binaries.value(1) == big);

    // Pings in both directions.
    QList<QByteArray> pongs;
    QObject::connect(&client.socket, &QWebSocket::pong, [&](quint64, const QByteArray &p) { pongs.append(p); });
    client.socket.ping("are you there");
    CHECK(waitFor([&] { return pongs.size() == 1; }) && pongs.value(0) == "are you there");
    bool serverPong = false;
    if (peer) {
        QObject::connect(peer, &QWebSocket::pong, [&](quint64, const QByteArray &p) { serverPong = p == "x"; });
        peer->ping("x");
    }
    CHECK(waitFor([&] { return serverPong; }));

    // The client closes; both sides see the code and the reason.
    client.socket.close(QWebSocketProtocol::CloseCodeGoingAway, QStringLiteral("bye"));
    CHECK(client.socket.state() == QAbstractSocket::ClosingState && client.aboutToClose == 1);
    CHECK(client.socket.sendTextMessage(QStringLiteral("too late")) == 0);
    CHECK(waitFor([&] { return client.disconnected == 1 && server.closeCodes.size() == 1; }));
    CHECK(client.socket.state() == QAbstractSocket::UnconnectedState);
    CHECK(client.socket.closeCode() == QWebSocketProtocol::CloseCodeGoingAway && client.socket.closeReason() == "bye");
    CHECK(server.closeCodes.value(0) == QWebSocketProtocol::CloseCodeGoingAway &&
          server.closeReasons.value(0) == "bye");
    CHECK(client.errors.isEmpty());

    // The server closes; the client echoes and the server ends the TCP connection.
    Client second;
    CHECK(second.open(server.url()));
    CHECK(waitFor([&] { return server.sockets.size() == 2; }));
    server.sockets.value(1)->close(QWebSocketProtocol::CloseCodePolicyViolated, QStringLiteral("rules"));
    CHECK(waitFor([&] { return second.disconnected == 1 && server.closeCodes.size() == 2; }));
    CHECK(second.socket.closeCode() == QWebSocketProtocol::CloseCodePolicyViolated &&
          second.socket.closeReason() == "rules");
    CHECK(second.aboutToClose == 1);

    // abort() ends the connection without a closing handshake.
    Client third;
    CHECK(third.open(server.url()));
    CHECK(waitFor([&] { return server.sockets.size() == 3; }));
    third.socket.abort();
    CHECK(waitFor([&] { return server.closeCodes.size() == 3; }));
    CHECK(server.closeCodes.value(2) == QWebSocketProtocol::CloseCodeAbnormalDisconnection);

    // The server can be closed while its sockets stay open.
    int closedSignals = 0;
    QObject::connect(&server.server, &QWebSocketServer::closed, [&] { ++closedSignals; });
    server.server.close();
    CHECK(!server.server.isListening() && closedSignals == 1 && server.server.serverUrl().isEmpty());
}

static void testFrames() {
    EchoServer server;
    server.echo = false;
    Client client;
    CHECK(client.open(server.url()));
    client.socket.setOutgoingFrameSize(4);
    CHECK(client.socket.outgoingFrameSize() == 4);
    // "ab€cd" is 7 bytes: frames "ab\xe2\x82", "\xac" "cd" - the euro sign is split between frames.
    client.socket.sendTextMessage(QString::fromUtf8("ab\xe2\x82\xac"
                                                    "cd"));
    CHECK(waitFor([&] { return server.texts.size() == 1; }));
    CHECK(server.texts.value(0) == QString::fromUtf8("ab\xe2\x82\xac"
                                                     "cd"));
    CHECK(server.textFrames.size() == 2);
    CHECK(server.textFrames.value(0) == (std::pair<QString, bool>{QStringLiteral("ab"), false}));
    CHECK(server.textFrames.value(1) == (std::pair<QString, bool>{QString::fromUtf8("\xe2\x82\xac"
                                                                                    "cd"),
                                                                  true}));

    // Limits on the server side.
    QWebSocket *peer = server.sockets.value(0);
    CHECK(peer && peer->maxAllowedIncomingMessageSize() == 64ULL * 1024 * 1024);
    if (peer)
        peer->setMaxAllowedIncomingMessageSize(10);
    client.socket.sendBinaryMessage(QByteArray(20, 'x')); // five frames of four bytes
    CHECK(waitFor([&] { return client.disconnected == 1; }));
    CHECK(client.socket.closeCode() == QWebSocketProtocol::CloseCodeTooMuchData);
    CHECK(server.binaries.isEmpty());
    CHECK(QWebSocket::maxIncomingMessageSize() >= 64ULL * 1024 * 1024);
}

static void testHandshakeRejections() {
    EchoServer server;
    const quint16 port = server.server.serverPort();
    QList<CloseCode> errors;
    QObject::connect(&server.server, &QWebSocketServer::serverError, [&](CloseCode c) { errors.append(c); });

    // The accept key of the sample request in RFC 6455.
    QByteArray r = rawExchange(port, upgradeRequest(port), frame(0x8, QByteArray("\x03\xe8", 2)));
    CHECK(r.startsWith("HTTP/1.1 101 Switching Protocols\r\n"));
    CHECK(r.contains("sec-websocket-accept: " + SampleAccept + "\r\n") && r.contains("server: test server\r\n"));
    CHECK(closeCodeOf(parseFrames(afterHead(r))) == 1000); // the close frame is echoed

    r = rawExchange(port, upgradeRequest(port).replace("Version: 13", "Version: 8"));
    CHECK(r.startsWith("HTTP/1.1 426 ") && r.contains("sec-websocket-version: 13"));
    r = rawExchange(port, upgradeRequest(port).replace(SampleKey, "short"));
    CHECK(r.startsWith("HTTP/1.1 400 "));
    r = rawExchange(port, upgradeRequest(port).replace("Upgrade: websocket\r\n", ""));
    CHECK(r.startsWith("HTTP/1.1 400 "));
    r = rawExchange(port, upgradeRequest(port).replace("GET ", "POST "));
    CHECK(r.startsWith("HTTP/1.1 405 "));
    r = rawExchange(port, upgradeRequest(port).replace("HTTP/1.1", "HTTP/1.0"));
    CHECK(r.startsWith("HTTP/1.1 400 "));
    r = rawExchange(port, upgradeRequest(port).replace("Host: ", "X-Host: "));
    CHECK(r.startsWith("HTTP/1.1 400 "));
    r = rawExchange(port, "GET / HTTP/1.1\r\nHost: x\r\nBig: " + QByteArray(20000, 'a') + "\r\n\r\n");
    CHECK(r.startsWith("HTTP/1.1 431 "));
    QElapsedTimer quick;
    quick.start();
    r = rawExchange(port, QByteArray("\x16\x03\x01\x00\x05hello", 10)); // the start of a TLS handshake
    CHECK(r.startsWith("HTTP/1.1 400 ") && quick.elapsed() < 2000);
    CHECK(errors.size() == 8 && server.sockets.size() == 1);
    CHECK(server.server.error() == QWebSocketProtocol::CloseCodeProtocolError &&
          server.server.errorString() == "The request is not HTTP");

    // Origins can be refused.
    QObject::connect(&server.server, &QWebSocketServer::originAuthenticationRequired,
                     [](QWebSocketCorsAuthenticator *a) { a->setAllowed(a->origin() != "http://evil.example"); });
    r = rawExchange(port, upgradeRequest(port, "Origin: http://evil.example\r\n"));
    CHECK(r.startsWith("HTTP/1.1 403 ") && server.server.error() == QWebSocketProtocol::CloseCodePolicyViolated);
    Client evil(QStringLiteral("http://evil.example"));
    CHECK(!evil.open(server.url()) && evil.errors.value(0) == QAbstractSocket::ConnectionRefusedError);
    CHECK(evil.socket.errorString().contains("403") && evil.socket.state() == QAbstractSocket::UnconnectedState);
    Client good(QStringLiteral("http://good.example"));
    CHECK(good.open(server.url()));

    // A client that never finishes its handshake is dropped.
    server.server.setHandshakeTimeout(100);
    CHECK(server.server.handshakeTimeoutMS() == 100);
    bool closed = false;
    rawExchange(port, "GET / HTTP/1.1\r\n", {}, &closed, 2000);
    CHECK(closed);
}

static void testProtocolErrors() {
    EchoServer server;
    const quint16 port = server.server.serverPort();
    auto closeAfter = [&](const QByteArray &frames) {
        bool closed = false;
        const QByteArray r = rawExchange(port, upgradeRequest(port), frames, &closed);
        return closed ? closeCodeOf(parseFrames(afterHead(r))) : -2;
    };
    CHECK(closeAfter(frame(0x1, "hi", true, false)) == 1002);             // not masked
    CHECK(closeAfter(frame(0x1, "hi", true, true, 0x40)) == 1002);        // RSV1
    CHECK(closeAfter(frame(0x3, "hi")) == 1002);                          // reserved opcode
    CHECK(closeAfter(frame(0x0, "hi")) == 1002);                          // continuation first
    CHECK(closeAfter(frame(0x1, "a", false) + frame(0x2, "b")) == 1002);  // new message inside one
    CHECK(closeAfter(frame(0x9, QByteArray(126, 'p'))) == 1002);          // control frame too long
    CHECK(closeAfter(frame(0x9, "p", false)) == 1002);                    // fragmented control frame
    CHECK(closeAfter(frame(0x1, "\xc3\x28")) == 1007);                    // invalid UTF-8
    CHECK(closeAfter(frame(0x1, "\xed\xa0\x80")) == 1007);                // a surrogate
    CHECK(closeAfter(frame(0x1, "\xe2\x82", true)) == 1007);              // truncated character
    CHECK(closeAfter(frame(0x8, QByteArray("\x03\xed", 2))) == 1002);     // close code 1005
    CHECK(closeAfter(frame(0x8, QByteArray("\x03", 1))) == 1002);         // one-byte close payload
    CHECK(closeAfter(frame(0x8, QByteArray("\x03\xe8\xff", 3))) == 1007); // reason not UTF-8
    CHECK(closeAfter(frame(0x8, QByteArray("\x0f\xa0", 2))) == 4000);     // an application code, echoed
    CHECK(closeAfter(frame(0x8, QByteArray())) == 1005);                  // empty close, echoed empty

    // A ping between the fragments of a message is answered, and the message still arrives.
    bool closed = false;
    const QByteArray r = rawExchange(port, upgradeRequest(port),
                                     frame(0x1, "fra", false) + frame(0x9, "ping!") + frame(0x0, "gment") +
                                         frame(0x8, QByteArray("\x03\xe8", 2)),
                                     &closed);
    const QList<Frame> frames = parseFrames(afterHead(r));
    CHECK(closed && frames.size() == 3);
    CHECK(frames.value(0).opcode == 0xA && frames.value(0).payload == "ping!" && !frames.value(0).masked);
    CHECK(frames.value(1).opcode == 0x1 && frames.value(1).payload == "fragment");
    CHECK(frames.value(2).opcode == 0x8);
    CHECK(server.texts.contains("fragment"));
}

// A fake server answering a client's handshake with a scripted response.
static void testClientAgainstBadServers() {
    QTcpServer fake;
    CHECK(fake.listen(QHostAddress::LocalHost, 0));
    std::function<QByteArray(const QByteArray &)> answer;
    QObject::connect(&fake, &QTcpServer::newConnection, [&] {
        QTcpSocket *s = fake.nextPendingConnection();
        auto buffer = std::make_shared<QByteArray>();
        QObject::connect(s, &QIODevice::readyRead, [s, buffer, &answer] {
            *buffer += s->readAll();
            if (!buffer->contains("\r\n\r\n"))
                return;
            s->write(answer(*buffer));
        });
    });
    const QUrl url(QStringLiteral("ws://127.0.0.1:%1/x").arg(fake.serverPort()));
    auto acceptFor = [](const QByteArray &request) {
        const qsizetype at = request.toLower().indexOf("sec-websocket-key: ") + 19;
        const QByteArray key = request.mid(at, request.indexOf('\r', at) - at);
        return QCryptographicHash::hash(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", QCryptographicHash::Sha1)
            .toBase64();
    };
    auto ok = [&](const QByteArray &request, const QByteArray &extra = {}) {
        return "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Accept: " +
               acceptFor(request) + "\r\n" + extra + "\r\n";
    };

    QByteArray seenRequest;
    answer = [&](const QByteArray &request) {
        seenRequest = request;
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    };
    Client notFound;
    QNetworkRequest request(url);
    request.setRawHeader("X-Token", "secret");
    request.setRawHeader("X-Bad", "a\r\nInjected: yes");
    notFound.socket.open(request);
    CHECK(waitFor([&] { return !notFound.errors.isEmpty(); }));
    CHECK(notFound.errors.value(0) == QAbstractSocket::ConnectionRefusedError &&
          notFound.socket.errorString().contains("404"));
    CHECK(seenRequest.startsWith("GET /x HTTP/1.1\r\n") && seenRequest.contains("x-token: secret\r\n"));
    CHECK(!seenRequest.contains("Injected") && seenRequest.contains("sec-websocket-version: 13\r\n"));
    CHECK(waitFor([&] { return notFound.socket.state() == QAbstractSocket::UnconnectedState; }));

    answer = [&](const QByteArray &) {
        return "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Accept: wrong=\r\n\r\n";
    };
    Client wrongKey;
    CHECK(!wrongKey.open(url) && wrongKey.socket.errorString().contains("Sec-WebSocket-Accept"));

    answer = [&](const QByteArray &r) { return ok(r, "Sec-WebSocket-Protocol: other\r\n"); };
    Client wrongProtocol;
    QWebSocketHandshakeOptions options;
    options.setSubprotocols({QStringLiteral("mine")});
    CHECK(!wrongProtocol.open(url, options) && wrongProtocol.socket.errorString().contains("subprotocol"));

    answer = [&](const QByteArray &r) { return ok(r, "Sec-WebSocket-Extensions: permessage-deflate\r\n"); };
    Client extension;
    CHECK(!extension.open(url));

    // The frame sent with the handshake answer is read; a masked one breaks the protocol.
    answer = [&](const QByteArray &r) {
        return ok(r, "Sec-WebSocket-Protocol: mine\r\n") + frame(0x1, "hello", true, false);
    };
    Client withFrame;
    CHECK(withFrame.open(url, options) && withFrame.socket.subprotocol() == "mine");
    CHECK(waitFor([&] { return withFrame.texts.size() == 1; }) && withFrame.texts.value(0) == "hello");
    answer = [&](const QByteArray &r) { return ok(r) + frame(0x1, "masked", true, true); };
    Client masked;
    CHECK(masked.open(url));
    CHECK(waitFor([&] { return masked.socket.state() != QAbstractSocket::ConnectedState; }));
    CHECK(masked.socket.closeCode() == QWebSocketProtocol::CloseCodeProtocolError && masked.texts.isEmpty());

    // URLs that cannot work, and a port nobody listens on.
    Client badUrl;
    badUrl.socket.open(QUrl(QStringLiteral("http://127.0.0.1/")));
    CHECK(badUrl.errors.value(0) == QAbstractSocket::UnsupportedSocketOperationError);
    CHECK(badUrl.socket.state() == QAbstractSocket::UnconnectedState);
    QTcpServer closedPort;
    CHECK(closedPort.listen(QHostAddress::LocalHost, 0));
    const quint16 unused = closedPort.serverPort();
    closedPort.close();
    Client refused;
    refused.socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(unused)));
    CHECK(waitFor([&] { return !refused.errors.isEmpty(); }));
    CHECK(refused.errors.value(0) == QAbstractSocket::ConnectionRefusedError);
    CHECK(waitFor([&] { return refused.socket.state() == QAbstractSocket::UnconnectedState; }));
    CHECK(refused.connected == 0 && refused.disconnected == 0);
}

class FixedMask : public QMaskGenerator {
public:
    int seeded = 0;
    bool seed() override {
        ++seeded;
        return true;
    }
    quint32 nextMask() override { return 0x01020304; }
};

static void testOptionsAndHelpers() {
    EchoServer server;
    server.server.setSupportedSubprotocols({QStringLiteral("v2"), QStringLiteral("v1")});
    CHECK(server.server.supportedSubprotocols() == (QStringList{"v2", "v1"}));
    CHECK(server.server.supportedVersions() == QList<QWebSocketProtocol::Version>{QWebSocketProtocol::Version13});
    CHECK(server.server.serverName() == "test server" && server.server.secureMode() == QWebSocketServer::NonSecureMode);

    QWebSocketHandshakeOptions options;
    options.setSubprotocols({QStringLiteral("v0"), QStringLiteral("v1"), QStringLiteral("v2")});
    QWebSocketHandshakeOptions copy = options;
    CHECK(copy == options && !(copy != options));
    QWebSocketHandshakeOptions other;
    other.swap(copy);
    CHECK(copy.subprotocols().isEmpty() && other.subprotocols().size() == 3 && copy != other);

    // The client's order of preference wins: v1 before v2.
    Client client;
    CHECK(client.open(server.url(), options) && client.socket.subprotocol() == "v1");
    CHECK(client.socket.handshakeOptions() == options);
    CHECK(waitFor([&] { return server.sockets.size() == 1; }) && server.sockets.value(0)->subprotocol() == "v1");

    QWebSocketCorsAuthenticator a(QStringLiteral("http://a"));
    CHECK(a.allowed() && a.origin() == "http://a");
    a.setAllowed(false);
    QWebSocketCorsAuthenticator b(QStringLiteral("http://b"));
    b.swap(a);
    CHECK(b.origin() == "http://a" && !b.allowed() && a.allowed());

    // A custom mask generator decides the masking keys of a client.
    FixedMask mask;
    Client masked;
    masked.socket.setMaskGenerator(&mask);
    CHECK(mask.seeded == 1 && masked.socket.maskGenerator() == &mask);
    QTcpServer raw;
    CHECK(raw.listen(QHostAddress::LocalHost, 0));
    QByteArray received;
    QObject::connect(&raw, &QTcpServer::newConnection, [&] {
        QTcpSocket *s = raw.nextPendingConnection();
        QObject::connect(s, &QIODevice::readyRead, [s, &received] {
            received += s->readAll();
            const qsizetype end = received.indexOf("\r\n\r\n");
            if (end < 0 || received.startsWith("done"))
                return;
            const qsizetype at = received.toLower().indexOf("sec-websocket-key: ") + 19;
            const QByteArray key = received.mid(at, received.indexOf('\r', at) - at);
            s->write("HTTP/1.1 101 OK\r\nUpgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Accept: " +
                     QCryptographicHash::hash(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", QCryptographicHash::Sha1)
                         .toBase64() +
                     "\r\n\r\n");
            received = "done" + received.mid(end + 4);
        });
    });
    CHECK(masked.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(raw.serverPort()))));
    masked.socket.sendTextMessage(QStringLiteral("abc"));
    CHECK(waitFor([&] { return received.size() >= 4 + 9; }));
    CHECK(received.mid(4) == QByteArray("\x81\x83\x01\x02\x03\x04", 6) + QByteArray("\x60\x60\x60", 3));

    // handleConnection() takes over a socket accepted elsewhere.
    QTcpServer elsewhere;
    CHECK(elsewhere.listen(QHostAddress::LocalHost, 0));
    QObject::connect(&elsewhere, &QTcpServer::newConnection,
                     [&] { server.server.handleConnection(elsewhere.nextPendingConnection()); });
    Client handed;
    CHECK(handed.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(elsewhere.serverPort()))));
    handed.socket.sendTextMessage(QStringLiteral("via handleConnection"));
    CHECK(waitFor([&] { return handed.texts.size() == 1; }) && handed.texts.value(0) == "via handleConnection");

    // Pending connections beyond the limit are refused.
    QWebSocketServer lazy(QStringLiteral("lazy"), QWebSocketServer::NonSecureMode);
    lazy.setMaxPendingConnections(1);
    CHECK(lazy.maxPendingConnections() == 1 && lazy.listen(QHostAddress::LocalHost, 0));
    Client first;
    CHECK(first.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(lazy.serverPort()))));
    CHECK(lazy.hasPendingConnections());
    Client second;
    CHECK(!second.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(lazy.serverPort()))));
    CHECK(second.socket.errorString().contains("503"));
    QWebSocket *taken = lazy.nextPendingConnection();
    CHECK(taken && taken->isValid() && !lazy.hasPendingConnections());
}

#ifdef NT_TEST_TLS
static void testTls() {
    const TestPki pki;
    QWebSocketServer server(QStringLiteral("secure"), QWebSocketServer::SecureMode);
    server.setSslConfiguration(TestPki::serverConfiguration(pki.server));
    CHECK(server.secureMode() == QWebSocketServer::SecureMode);
    CHECK(server.sslConfiguration().localCertificate() == pki.server.certificate);
    CHECK(server.listen(QHostAddress::LocalHost, 0) && server.serverUrl().scheme() == "wss");
    QList<QWebSocket *> peers;
    QObject::connect(&server, &QWebSocketServer::newConnection, [&] {
        QWebSocket *s = server.nextPendingConnection();
        peers.append(s);
        QObject::connect(s, &QWebSocket::textMessageReceived, s,
                         [s](const QString &m) { s->sendTextMessage(QStringLiteral("secure ") + m); });
    });
    const QUrl url(QStringLiteral("wss://127.0.0.1:%1/secure").arg(server.serverPort()));

    // A client that trusts the test CA.
    Client trusted;
    trusted.socket.setSslConfiguration(pki.clientConfiguration());
    CHECK(trusted.socket.sslConfiguration().caCertificates() == QList<QSslCertificate>{pki.ca.certificate});
    int sslErrorSignals = 0;
    QObject::connect(&trusted.socket, &QWebSocket::sslErrors, [&](const QList<QSslError> &) { ++sslErrorSignals; });
    CHECK(trusted.open(url));
    CHECK(sslErrorSignals == 0 && trusted.socket.sslConfiguration().peerCertificate() == pki.server.certificate);
    CHECK(waitFor([&] { return peers.size() == 1; }));
    CHECK(peers.value(0)->requestUrl().scheme() == "wss" && peers.value(0)->resourceName() == "/secure");
    trusted.socket.sendTextMessage(QStringLiteral("hello"));
    CHECK(waitFor([&] { return trusted.texts.size() == 1; }) && trusted.texts.value(0) == "secure hello");
    QByteArray big(200 * 1024, 'z');
    trusted.socket.sendBinaryMessage(big); // not echoed, but must not break the TLS stream
    trusted.socket.sendTextMessage(QStringLiteral("after"));
    CHECK(waitFor([&] { return trusted.texts.size() == 2; }) && trusted.texts.value(1) == "secure after");
    trusted.socket.close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("done"));
    CHECK(waitFor([&] { return trusted.disconnected == 1; }));
    CHECK(trusted.socket.closeCode() == QWebSocketProtocol::CloseCodeNormal);

    // A client with the system's CAs does not trust the test CA: the handshake fails.
    Client untrusted;
    QList<QSslError> reported;
    QObject::connect(&untrusted.socket, &QWebSocket::sslErrors, [&](const QList<QSslError> &e) { reported = e; });
    CHECK(!untrusted.open(url) && !reported.isEmpty() && untrusted.connected == 0);
    CHECK(waitFor([&] { return untrusted.socket.state() == QAbstractSocket::UnconnectedState; }));

    // A self-signed server: refused, unless the errors are ignored from the slot or listed before open().
    QWebSocketServer selfSigned(QStringLiteral("self"), QWebSocketServer::SecureMode);
    selfSigned.setSslConfiguration(TestPki::serverConfiguration(pki.selfSigned));
    CHECK(selfSigned.listen(QHostAddress::LocalHost, 0));
    QObject::connect(&selfSigned, &QWebSocketServer::newConnection, [&] {
        QWebSocket *s = selfSigned.nextPendingConnection();
        QObject::connect(s, &QWebSocket::textMessageReceived, s, [s](const QString &m) { s->sendTextMessage(m); });
    });
    const QUrl selfUrl(QStringLiteral("wss://127.0.0.1:%1/").arg(selfSigned.serverPort()));
    Client strict;
    QList<QSslError> selfErrors;
    QObject::connect(&strict.socket, &QWebSocket::sslErrors, [&](const QList<QSslError> &e) { selfErrors = e; });
    CHECK(!strict.open(selfUrl) && !selfErrors.isEmpty());

    Client lenient;
    QObject::connect(&lenient.socket, &QWebSocket::sslErrors,
                     [&](const QList<QSslError> &) { lenient.socket.ignoreSslErrors(); });
    CHECK(lenient.open(selfUrl));
    lenient.socket.sendTextMessage(QStringLiteral("self"));
    CHECK(waitFor([&] { return lenient.texts.size() == 1; }) && lenient.texts.value(0) == "self");

    Client listed;
    listed.socket.ignoreSslErrors(selfErrors);
    CHECK(listed.open(selfUrl) && listed.socket.sslConfiguration().peerCertificate() == pki.selfSigned.certificate);

    // Plain WebSocket against a TLS server, and TLS against a plain server, both fail.
    Client plainToTls;
    CHECK(!plainToTls.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(server.serverPort()))));
    CHECK(waitFor([&] { return plainToTls.socket.state() == QAbstractSocket::UnconnectedState; }));
    EchoServer plain;
    Client tlsToPlain;
    tlsToPlain.socket.setSslConfiguration(pki.clientConfiguration());
    CHECK(!tlsToPlain.open(QUrl(QStringLiteral("wss://127.0.0.1:%1/").arg(plain.server.serverPort()))));
    CHECK(waitFor([&] { return tlsToPlain.socket.state() == QAbstractSocket::UnconnectedState; }));
    CHECK(plain.sockets.isEmpty());

    // A server without a certificate cannot complete a TLS handshake.
    QWebSocketServer noCertificate(QStringLiteral("none"), QWebSocketServer::SecureMode);
    CHECK(noCertificate.listen(QHostAddress::LocalHost, 0));
    Client hopeful;
    hopeful.socket.setSslConfiguration(pki.clientConfiguration());
    CHECK(!hopeful.open(QUrl(QStringLiteral("wss://127.0.0.1:%1/").arg(noCertificate.serverPort()))));
    CHECK(!noCertificate.hasPendingConnections());
}
#endif

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(captureWarnings);
    testEcho();
    testFrames();
    testHandshakeRejections();
    testProtocolErrors();
    testClientAgainstBadServers();
    testOptionsAndHelpers();
#ifdef NT_TEST_TLS
    testTls();
#endif
    spin(10);
    qInstallMessageHandler(nullptr);
    if (g_failures == 0)
        std::puts("all WebSockets tests passed");
    return g_failures == 0 ? 0 : 1;
}
