// MIT License - see LICENSE
#include <QtCore>
#include <QtHttpServer>
#include <QtNetwork>
#include <QtWebSockets>

#ifdef NT_TEST_TLS
#include "test_certificates.h"
#endif

#include <cstdio>
#include <cstring>
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

using Status = QHttpServerResponder::StatusCode;
using Method = QHttpServerRequest::Method;

// Warnings the library prints on purpose are collected instead of shown.
static QStringList g_warnings;
static void captureWarnings(QtMsgType type, const QMessageLogContext &, const QString &message) {
    if (type == QtWarningMsg)
        g_warnings.append(message);
    else
        std::fprintf(stderr, "%s\n", qPrintable(message));
}

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

// A server listening on a free port of the loopback interface.
struct Fixture {
    QHttpServer server;
    quint16 port = 0;
    Fixture() {
        auto *tcp = new QTcpServer;
        CHECK(tcp->listen(QHostAddress::LocalHost, 0));
        CHECK(server.bind(tcp));
        CHECK(tcp->parent() == &server);
        port = tcp->serverPort();
    }
    QString url(const QString &path) const { return QStringLiteral("http://127.0.0.1:%1%2").arg(port).arg(path); }
};

// Sends raw bytes and returns what the server sends back, until it closes the connection or done() is satisfied.
static QByteArray exchange(quint16 port, const QByteArray &request,
                           const std::function<bool(const QByteArray &)> &done = {}, bool *closed = nullptr,
                           int timeoutMs = 5000) {
    QTcpSocket socket;
    QByteArray got;
    QEventLoop loop;
    bool disconnected = false;
    QObject::connect(&socket, &QAbstractSocket::connected, [&] { socket.write(request); });
    QObject::connect(&socket, &QIODevice::readyRead, [&] {
        got += socket.readAll();
        if (done && done(got))
            loop.quit();
    });
    QObject::connect(&socket, &QAbstractSocket::disconnected, [&] {
        disconnected = true;
        loop.quit();
    });
    socket.connectToHost(QHostAddress::LocalHost, port);
    runLoop(loop, timeoutMs);
    got += socket.readAll();
    if (closed)
        *closed = disconnected;
    return got;
}

static int statusOf(const QByteArray &response) {
    if (!response.startsWith("HTTP/1.1 "))
        return -1;
    return response.mid(9, 3).toInt();
}

static QByteArray headerOf(const QByteArray &response, const QByteArray &name) {
    const qsizetype end = response.indexOf("\r\n\r\n");
    for (const QByteArray &line : response.left(end).split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon > 0 && line.left(colon).toLower() == name.toLower())
            return line.mid(colon + 1).trimmed();
    }
    return QByteArray();
}

static QByteArray bodyOf(const QByteArray &response) {
    const qsizetype end = response.indexOf("\r\n\r\n");
    return end < 0 ? QByteArray() : response.mid(end + 4);
}

static int countOf(const QByteArray &haystack, const QByteArray &needle) {
    int n = 0;
    for (qsizetype i = haystack.indexOf(needle); i >= 0; i = haystack.indexOf(needle, i + 1))
        ++n;
    return n;
}

// GET with Connection: close, so the response ends with the connection.
static QByteArray get(quint16 port, const QByteArray &path, const QByteArray &extra = {}) {
    return exchange(port, "GET " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n" + extra + "\r\n");
}

struct Fetched {
    int status = 0;
    QByteArray body;
    QByteArray contentType;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
};

static Fetched finish(QNetworkReply *reply) {
    QEventLoop loop;
    if (!reply->isFinished()) {
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        runLoop(loop);
    }
    Fetched f;
    f.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    f.body = reply->readAll();
    f.contentType = reply->rawHeader("content-type");
    f.error = reply->error();
    reply->deleteLater();
    return f;
}

class Counter : public QObject {
public:
    int calls = 0;
    QHttpServerResponse count(int step) {
        calls += step;
        return QHttpServerResponse(QString::number(calls));
    }
};

// A sequential device that delivers its data in pieces from the event loop, then finishes.
class SlowDevice : public QIODevice {
public:
    explicit SlowDevice(QList<QByteArray> pieces) : m_pieces(std::move(pieces)) {}
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_ready.size() + QIODevice::bytesAvailable(); }
    bool open(OpenMode mode) override {
        if (!QIODevice::open(mode))
            return false;
        QTimer::singleShot(5, this, [this] { next(); });
        return true;
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        const qint64 n = qMin<qint64>(maxSize, m_ready.size());
        std::memcpy(data, m_ready.constData(), static_cast<std::size_t>(n));
        m_ready.remove(0, static_cast<qsizetype>(n));
        return n;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    void next() {
        if (m_pieces.isEmpty()) {
            readChannelFinished();
            return;
        }
        m_ready += m_pieces.takeFirst();
        readyRead();
        QTimer::singleShot(5, this, [this] { next(); });
    }
    QList<QByteArray> m_pieces;
    QByteArray m_ready;
};

static void testRoutes() {
    Fixture f;
    Counter counter;
    auto *temporary = new QObject;

    f.server.route("/", [] { return "root"; });
    f.server.route("/int/<arg>", [](int v) { return QString::number(v * 2); });
    f.server.route("/u8/<arg>", [](quint8 v) { return QString::number(v); });
    f.server.route("/name/<arg>", [](const QString &name) { return "hello " + name; });
    f.server.route("/bytes/<arg>/<arg>", [](QByteArray a, QByteArray b) { return QString::fromUtf8(a + "|" + b); });
    f.server.route("/real/<arg>", [](double v) { return QString::number(v * 2); });
    f.server.route("/file/<arg>.json", [](const QString &stem) { return "json " + stem; });
    f.server.route("/rest/<arg>", [](const QUrl &rest) { return "rest " + rest.path(); });
    f.server.route("/auto/", [](qint64 v) { return QString::number(v + 1); });
    f.server.route("/post", Method::Post | Method::Put, [](const QHttpServerRequest &request) {
        return QHttpServerResponse("text/plain", request.body() + " " + request.value("x-tag"));
    });
    f.server.route("/both/<arg>", [](int v, const QHttpServerRequest &request, QHttpServerResponder &responder) {
        responder.write(QByteArray::number(v) + " " + request.url().path().toUtf8(), "text/plain");
    });
    f.server.route("/count/<arg>", &counter, &Counter::count);
    f.server.route("/temporary", temporary, [] { return "alive"; });

    g_warnings.clear();
    CHECK(f.server.route("/mismatch/<arg>", [] { return "x"; }) == nullptr);
    CHECK(f.server.route("/mismatch", [](int) { return "x"; }) == nullptr);
    CHECK(g_warnings.size() == 2);
    QHttpServerRouterRule *rule = f.server.route("/rule/<arg>", Method::Get, &counter, [](int) { return "r"; });
    CHECK(rule && rule->contextObject() == &counter && rule->pathPattern() == "/rule/<arg>");
    CHECK(rule && rule->methods() == QHttpServerRequest::Methods(Method::Get));

    CHECK(bodyOf(get(f.port, "/")) == "root");
    CHECK(bodyOf(get(f.port, "/int/21")) == "42");
    CHECK(bodyOf(get(f.port, "/int/-21")) == "-42");
    CHECK(statusOf(get(f.port, "/int/abc")) == 404);
    CHECK(statusOf(get(f.port, "/int/99999999999")) == 404);
    CHECK(statusOf(get(f.port, "/int/")) == 404);
    CHECK(bodyOf(get(f.port, "/u8/255")) == "255");
    CHECK(statusOf(get(f.port, "/u8/256")) == 404 && statusOf(get(f.port, "/u8/-1")) == 404);
    CHECK(bodyOf(get(f.port, "/name/N%C3%BCt%20x")) == QString("hello Nüt x").toUtf8());
    CHECK(statusOf(get(f.port, "/name/a/b")) == 404);
    CHECK(bodyOf(get(f.port, "/bytes/a/b")) == "a|b");
    CHECK(bodyOf(get(f.port, "/real/1.25")) == "2.5");
    CHECK(bodyOf(get(f.port, "/file/report.json")) == "json report");
    CHECK(bodyOf(get(f.port, "/file/a.b.json")) == "json a.b");
    CHECK(bodyOf(get(f.port, "/rest/a/b/c")) == "rest a/b/c");
    CHECK(bodyOf(get(f.port, "/auto/41")) == "42");
    CHECK(bodyOf(get(f.port, "/both/7")) == "7 /both/7");
    CHECK(bodyOf(get(f.port, "/count/2")) == "2" && bodyOf(get(f.port, "/count/3")) == "5" && counter.calls == 5);
    CHECK(bodyOf(get(f.port, "/rule/1")) == "r");
    CHECK(statusOf(get(f.port, "/nothing")) == 404);

    // Methods: the route accepts POST and PUT only.
    const QByteArray post = exchange(f.port, "POST /post HTTP/1.1\r\nHost: x\r\nConnection: close\r\nX-Tag: t\r\n"
                                             "Content-Length: 4\r\n\r\nbody");
    CHECK(statusOf(post) == 200 && bodyOf(post) == "body t");
    CHECK(statusOf(get(f.port, "/post")) == 404);
    CHECK(statusOf(exchange(f.port, "PROPFIND / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")) == 404);

    // A rule whose context is destroyed stops matching.
    CHECK(bodyOf(get(f.port, "/temporary")) == "alive");
    delete temporary;
    CHECK(statusOf(get(f.port, "/temporary")) == 404);

    // After-request handlers see and change the responses that handlers return.
    QObject context;
    QList<QByteArray> seen;
    f.server.addAfterRequestHandler(&context, [&](const QHttpServerRequest &request, QHttpServerResponse &response) {
        seen.append(request.url().path().toUtf8());
        QHttpHeaders h = response.headers();
        h.append("x-after", "1");
        response.setHeaders(std::move(h));
    });
    const QByteArray after = get(f.port, "/");
    CHECK(headerOf(after, "x-after") == "1" && headerOf(after, "content-type") == "text/plain");
    CHECK(seen == QList<QByteArray>{"/"});
    CHECK(headerOf(get(f.port, "/both/1"), "x-after").isEmpty()); // written through the responder

    // The missing handler, until it is cleared or its context dies.
    auto *missing = new QObject;
    f.server.setMissingHandler(missing, [](const QHttpServerRequest &request, QHttpServerResponder &responder) {
        responder.write("no " + request.url().path().toUtf8(), "text/plain", Status::Gone);
    });
    QByteArray gone = get(f.port, "/nowhere");
    CHECK(statusOf(gone) == 410 && bodyOf(gone) == "no /nowhere");
    f.server.clearMissingHandler();
    CHECK(statusOf(get(f.port, "/nowhere")) == 404);
    f.server.setMissingHandler(
        missing, [](const QHttpServerRequest &, QHttpServerResponder &responder) { responder.write(Status::Gone); });
    delete missing;
    CHECK(statusOf(get(f.port, "/nowhere")) == 404);
}

static void testResponses() {
    Fixture f;
    QTemporaryDir dir;
    QFile page(dir.filePath("page.html"));
    CHECK(page.open(QIODevice::WriteOnly) && page.write("<p>hi</p>") == 9);
    page.close();

    f.server.route("/json", [] { return QJsonObject{{"a", 1}}; });
    f.server.route("/array", [] { return QJsonArray{1, 2}; });
    f.server.route("/status", [] { return Status::NoContent; });
    f.server.route("/created", [] { return QHttpServerResponse(Status::Created); });
    f.server.route("/bytes", [] { return QHttpServerResponse(QByteArray("\x89PNG\r\n\x1a\nxx", 10)); });
    f.server.route("/html", [] { return QHttpServerResponse(QByteArray("<!DOCTYPE html><html></html>")); });
    f.server.route("/typed", [] { return QHttpServerResponse("application/x-custom", "data", Status::Accepted); });
    f.server.route("/page", [&] { return QHttpServerResponse::fromFile(dir.filePath("page.html")); });
    f.server.route("/missing", [&] { return QHttpServerResponse::fromFile(dir.filePath("none.html")); });
    f.server.route("/framing", [] {
        QHttpServerResponse r("text/plain", "abc");
        QHttpHeaders h = r.headers();
        h.append("content-length", "999");
        h.append("connection", "keep-alive");
        h.append("x-ok", "yes");
        r.setHeaders(h);
        return r;
    });

    QByteArray r = get(f.port, "/json");
    CHECK(statusOf(r) == 200 && headerOf(r, "content-type") == "application/json" && bodyOf(r) == "{\"a\":1}");
    CHECK(headerOf(r, "content-length") == "7" && headerOf(r, "connection") == "close");
    CHECK(bodyOf(get(f.port, "/array")) == "[1,2]");
    r = get(f.port, "/status");
    CHECK(statusOf(r) == 204 && headerOf(r, "content-length").isEmpty() && bodyOf(r).isEmpty());
    CHECK(r.startsWith("HTTP/1.1 204 No Content\r\n"));
    r = get(f.port, "/created");
    CHECK(statusOf(r) == 201 && headerOf(r, "content-length") == "0" &&
          headerOf(r, "content-type") == "application/x-empty");
    CHECK(headerOf(get(f.port, "/bytes"), "content-type") == "image/png");
    CHECK(headerOf(get(f.port, "/html"), "content-type") == "text/html");
    r = get(f.port, "/typed");
    CHECK(statusOf(r) == 202 && headerOf(r, "content-type") == "application/x-custom" && bodyOf(r) == "data");
    r = get(f.port, "/page");
    CHECK(statusOf(r) == 200 && headerOf(r, "content-type") == "text/html" && bodyOf(r) == "<p>hi</p>");
    CHECK(statusOf(get(f.port, "/missing")) == 404);
    r = get(f.port, "/framing");
    CHECK(headerOf(r, "content-length") == "3" && headerOf(r, "x-ok") == "yes" && headerOf(r, "connection") == "close");
    CHECK(countOf(r, "content-length") == 1 && bodyOf(r) == "abc");

    // The value classes on their own.
    QHttpServerResponse text(QStringLiteral("t"));
    CHECK(text.mimeType() == "text/plain" && text.data() == "t" && text.statusCode() == Status::Ok);
    QHttpServerResponse other(Status::NotFound);
    text.swap(other);
    CHECK(text.statusCode() == Status::NotFound && other.data() == "t");
    QHttpServerResponse moved(std::move(other));
    CHECK(moved.data() == "t");
    CHECK(QHttpServerResponse(QByteArray("plain words\n")).mimeType() == "text/plain");
    CHECK(QHttpServerResponse(QByteArray("\x01\x02", 2)).mimeType() == "application/octet-stream");

    // A HEAD request gets the headers, with the length of the body, but no body.
    bool closed = false;
    r = exchange(f.port,
                 "HEAD /json HTTP/1.1\r\nHost: x\r\n\r\nGET /array HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
                 {}, &closed);
    CHECK(closed && r.startsWith("HTTP/1.1 200 OK\r\n") && r.contains("content-length: 7\r\n"));
    CHECK(countOf(r, "HTTP/1.1 200") == 2 && r.endsWith("\r\n\r\n[1,2]") && !r.contains("{\"a\""));
}

static void testResponder() {
    Fixture f;
    std::shared_ptr<QHttpServerResponder> pending;
    bool canceled = false;

    f.server.route("/chunked", [](QHttpServerResponder &responder) {
        responder.writeBeginChunked("text/plain");
        responder.writeChunk("one ");
        responder.writeChunk("");
        responder.writeChunk("two ");
        responder.writeEndChunked("three");
    });
    f.server.route("/trailers", [](QHttpServerResponder &responder) {
        QHttpHeaders trailers;
        trailers.append(QHttpHeaders::WellKnownHeader::ETag, "\"x\"");
        responder.writeBeginChunked(QHttpHeaders(), {QHttpHeaders::WellKnownHeader::ETag});
        responder.writeEndChunked("data", trailers);
    });
    f.server.route("/buffer", [](QHttpServerResponder &responder) {
        auto *buffer = new QBuffer;
        buffer->setData(QByteArray(300 * 1024, 'b') + "end");
        responder.write(buffer, "application/octet-stream");
    });
    f.server.route("/slow", [](QHttpServerResponder &responder) {
        responder.write(new SlowDevice({"al", "pha", "bet"}), "text/plain");
    });
    f.server.route("/json", [](QHttpServerResponder &responder) {
        responder.write(QJsonDocument(QJsonObject{{"k", "v"}}), Status::Created);
    });
    f.server.route("/headers", [](QHttpServerResponder &responder) {
        QHttpHeaders h;
        h.append(QHttpHeaders::WellKnownHeader::Location, "/elsewhere");
        responder.write(h, Status::SeeOther);
    });
    f.server.route("/twice", [](QHttpServerResponder &responder) {
        responder.write("first", "text/plain");
        responder.write("second", "text/plain");
    });
    f.server.route("/silent", [](QHttpServerResponder &) {});
    f.server.route("/later", [&](QHttpServerResponder &responder) {
        pending = std::make_shared<QHttpServerResponder>(std::move(responder));
        CHECK(responder.isResponseCanceled()); // moved from
        CHECK(!pending->isResponseCanceled());
    });
    f.server.route("/abandoned", [&](QHttpServerResponder &responder) {
        auto r = std::make_shared<QHttpServerResponder>(std::move(responder));
        QTimer::singleShot(100, [r, &canceled] {
            canceled = r->isResponseCanceled();
            r->write("too late", "text/plain");
        });
    });
    f.server.route("/now", [] { return "now"; });

    // The chunked responses, decoded by QNetworkAccessManager, and on the wire.
    QNetworkAccessManager manager;
    Fetched fetched = finish(manager.get(QNetworkRequest(QUrl(f.url("/chunked")))));
    CHECK(fetched.status == 200 && fetched.body == "one two three" && fetched.contentType == "text/plain");
    QByteArray r = get(f.port, "/chunked");
    CHECK(headerOf(r, "transfer-encoding") == "chunked" && headerOf(r, "content-length").isEmpty());
    CHECK(bodyOf(r) == "4\r\none \r\n4\r\ntwo \r\n5\r\nthree\r\n0\r\n\r\n");
    r = get(f.port, "/trailers");
    CHECK(headerOf(r, "trailer") == "etag" && bodyOf(r) == "4\r\ndata\r\n0\r\netag: \"x\"\r\n\r\n");

    // HTTP/1.0 has no chunked coding: the body ends with the connection.
    bool closed = false;
    r = exchange(f.port, "GET /chunked HTTP/1.0\r\nConnection: keep-alive\r\n\r\n", {}, &closed);
    CHECK(closed && headerOf(r, "transfer-encoding").isEmpty() && headerOf(r, "connection") == "close");
    CHECK(bodyOf(r) == "one two three");

    // Devices: a buffer larger than the socket's high-water mark, and a sequential device.
    fetched = finish(manager.get(QNetworkRequest(QUrl(f.url("/buffer")))));
    CHECK(fetched.status == 200 && fetched.body.size() == 300 * 1024 + 3 && fetched.body.endsWith("bend"));
    r = get(f.port, "/buffer");
    CHECK(headerOf(r, "content-length") == QByteArray::number(300 * 1024 + 3));
    fetched = finish(manager.get(QNetworkRequest(QUrl(f.url("/slow")))));
    CHECK(fetched.status == 200 && fetched.body == "alphabet");
    r = get(f.port, "/slow");
    CHECK(headerOf(r, "transfer-encoding") == "chunked" && bodyOf(r).endsWith("0\r\n\r\n"));

    r = get(f.port, "/json");
    CHECK(statusOf(r) == 201 && headerOf(r, "content-type") == "application/json" && bodyOf(r) == "{\"k\":\"v\"}");
    r = get(f.port, "/headers");
    CHECK(statusOf(r) == 303 && headerOf(r, "location") == "/elsewhere" && headerOf(r, "content-length") == "0");

    g_warnings.clear();
    r = get(f.port, "/twice");
    CHECK(bodyOf(r) == "first" && g_warnings.size() == 1);
    g_warnings.clear();
    CHECK(statusOf(get(f.port, "/silent")) == 500 && g_warnings.size() == 1);

    // A response written later holds back the next request on the same connection.
    QTcpSocket socket;
    QByteArray got;
    QEventLoop loop;
    QObject::connect(&socket, &QIODevice::readyRead, [&] {
        got += socket.readAll();
        if (got.contains("now"))
            loop.quit();
    });
    QObject::connect(&socket, &QAbstractSocket::connected, [&] {
        socket.write("GET /later HTTP/1.1\r\nHost: x\r\n\r\nGET /now HTTP/1.1\r\nHost: x\r\n\r\n");
    });
    QTimer::singleShot(100, [&] {
        CHECK(got.isEmpty() && pending);
        if (pending)
            pending->write("later", "text/plain");
    });
    socket.connectToHost(QHostAddress::LocalHost, f.port);
    CHECK(runLoop(loop));
    CHECK(got.indexOf("later") >= 0 && got.indexOf("later") < got.indexOf("now"));
    CHECK(countOf(got, "HTTP/1.1 200 OK") == 2);
    socket.abort();

    // The client goes away before the answer.
    QTcpSocket quitter;
    QObject::connect(&quitter, &QAbstractSocket::connected, [&] {
        quitter.write("GET /abandoned HTTP/1.1\r\nHost: x\r\n\r\n");
        QTimer::singleShot(30, [&] { quitter.abort(); });
    });
    quitter.connectToHost(QHostAddress::LocalHost, f.port);
    QEventLoop wait;
    QTimer::singleShot(300, &wait, &QEventLoop::quit);
    wait.exec();
    CHECK(canceled);
}

static void testProtocol() {
    Fixture f;
    f.server.route("/", [] { return "ok"; });
    f.server.route("/echo",
                   [](const QHttpServerRequest &request) { return QHttpServerResponse("text/plain", request.body()); });
    f.server.route("/url", [](const QHttpServerRequest &request) {
        return QHttpServerResponse("text/plain",
                                   request.url().toEncoded() + " " + request.query().queryItemValue("q").toUtf8());
    });
    f.server.route("/peer", [](const QHttpServerRequest &request) {
        return QString("%1 %2 %3")
            .arg(request.remoteAddress().toString())
            .arg(request.localAddress().toString())
            .arg(request.localPort() != 0 && request.remotePort() != 0 ? "ports" : "none");
    });

    // Keep-alive: two requests on one connection, then pipelined ones in a single write.
    auto two = [](const QByteArray &got) { return countOf(got, "\r\n\r\nok") == 2; };
    bool closed = true;
    QByteArray r =
        exchange(f.port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n\r\nGET / HTTP/1.1\r\nHost: x\r\n\r\n", two, &closed);
    CHECK(!closed && countOf(r, "HTTP/1.1 200 OK") == 2 && !r.contains("connection:"));
    r = exchange(f.port,
                 "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\n\r\nabc"
                 "POST /echo HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: "
                 "chunked\r\n\r\n3;ext=1\r\ndef\r\nA\r\n0123456789\r\n0\r\nx-trailer: t\r\n\r\n"
                 "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
                 {}, &closed);
    CHECK(closed && countOf(r, "HTTP/1.1 200 OK") == 3);
    CHECK(r.indexOf("\r\n\r\nabc") < r.indexOf("\r\n\r\ndef0123456789") && r.endsWith("\r\n\r\nok"));

    // HTTP/1.0 closes unless asked to keep the connection.
    r = exchange(f.port, "GET / HTTP/1.0\r\n\r\n", {}, &closed);
    CHECK(closed && statusOf(r) == 200 && headerOf(r, "connection") == "close");
    r = exchange(
        f.port, "GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n",
        [](const QByteArray &got) { return got.endsWith("ok"); }, &closed);
    CHECK(!closed && headerOf(r, "connection") == "keep-alive");

    // The URL is absolute, built from the Host header; absolute-form targets are accepted too.
    r = get(f.port, "/url?q=a%20b");
    CHECK(bodyOf(r) == "http://localhost/url?q=a%20b a b");
    r = exchange(f.port,
                 "GET http://example.org:81/url?q=1 HTTP/1.1\r\nHost: example.org:81\r\nConnection: close\r\n\r\n");
    CHECK(bodyOf(r) == "http://example.org:81/url?q=1 1");
    CHECK(bodyOf(get(f.port, "/peer")) == "127.0.0.1 127.0.0.1 ports");

    // Expect: 100-continue is answered before the body is sent.
    QTcpSocket socket;
    QByteArray got;
    QEventLoop loop;
    QObject::connect(&socket, &QAbstractSocket::connected, [&] {
        socket.write("POST /echo HTTP/1.1\r\nHost: x\r\nExpect: 100-continue\r\nContent-Length: 2\r\n\r\n");
    });
    QObject::connect(&socket, &QIODevice::readyRead, [&] {
        got += socket.readAll();
        if (got == "HTTP/1.1 100 Continue\r\n\r\n")
            socket.write("hi");
        if (got.endsWith("hi"))
            loop.quit();
    });
    socket.connectToHost(QHostAddress::LocalHost, f.port);
    CHECK(runLoop(loop) && got.startsWith("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\n"));
    socket.abort();

    // Requests that are refused, and the connection closed.
    auto refused = [&](const QByteArray &request) {
        bool c = false;
        const QByteArray response = exchange(f.port, request, {}, &c);
        if (!c || headerOf(response, "connection") != "close")
            return -1;
        return statusOf(response);
    };
    CHECK(refused("GARBAGE\r\n\r\n") == 400);
    CHECK(refused(QByteArray("\x16\x03\x01\x00\x05hello", 10)) == 400);    // a TLS handshake on a plain port
    CHECK(refused("GET / HTTP/1.1\r\n\r\n") == 400);                       // no Host
    CHECK(refused("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n") == 400); // two Hosts
    CHECK(refused("GET / HTTP/1.1\r\nHost: a/b\r\n\r\n") == 400);          // bad Host
    CHECK(refused("GET / HTTP/2.0\r\nHost: x\r\n\r\n") == 505);
    CHECK(refused("GET / HTTP/1.1\r\nHost: x\r\nBad Name: v\r\n\r\n") == 400);
    CHECK(refused("GET / HTTP/1.1\r\nHost: x\r\nName : v\r\n\r\n") == 400);
    CHECK(refused("GET / HTTP/1.1\r\nHost: x\r\nA: b\r\n folded\r\n\r\n") == 400);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1x\r\n\r\n") == 400);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n") == 400);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n") == 400);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip\r\n\r\n") == 400);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip, chunked\r\n\r\n") == 501);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n") == 400);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nabX\r\n") == 400);
    CHECK(refused("GET / HTTP/1.1\r\nHost: x\r\nExpect: magic\r\n\r\n") == 417);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 999999999999\r\n\r\n") == 413);
    CHECK(refused("POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n7FFFFFFF\r\n") == 413);
    CHECK(refused("GET /" + QByteArray(9000, 'a') + " HTTP/1.1\r\nHost: x\r\n\r\n") == 414);
    CHECK(refused("GET / HTTP/1.1\r\nHost: x\r\nBig: " + QByteArray(70000, 'a') + "\r\n\r\n") == 431);
    QByteArray many = "GET / HTTP/1.1\r\nHost: x\r\n";
    for (int i = 0; i < 120; ++i)
        many += "H" + QByteArray::number(i) + ": v\r\n";
    CHECK(refused(many + "\r\n") == 431);

    // A refused request ends the connection, so a request smuggled behind it is never read.
    r = exchange(f.port, "GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n"
                         "0\r\n\r\nGET / HTTP/1.1\r\nHost: x\r\n\r\n");
    CHECK(statusOf(r) == 400 && countOf(r, "HTTP/1.1") == 1);
}

static void testServers() {
    QHttpServer server;
    server.route("/", [] { return "hello"; });

    g_warnings.clear();
    QTcpServer idle;
    CHECK(!server.bind(&idle) && !server.bind(static_cast<QTcpServer *>(nullptr)));
    CHECK(g_warnings.size() == 2 && server.servers().isEmpty() && server.serverPorts().isEmpty());

    auto *owned = new QTcpServer;
    CHECK(owned->listen(QHostAddress::LocalHost, 0) && server.bind(owned) && server.bind(owned));
    CHECK(owned->parent() == &server && server.servers() == QList<QTcpServer *>{owned});
    CHECK(server.serverPorts() == QList<quint16>{owned->serverPort()});
    const quint16 port = owned->serverPort();
    CHECK(bodyOf(get(port, "/")) == "hello");

    // A local socket.
    const QString name =
        QStringLiteral("nt-httpserver-test-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QLocalServer::removeServer(name);
    auto *local = new QLocalServer;
    CHECK(local->listen(name) && server.bind(local) && local->parent() == &server);
    CHECK(server.localServers() == QList<QLocalServer *>{local});
    QLocalSocket client;
    QByteArray got;
    QEventLoop loop;
    QObject::connect(&client, &QLocalSocket::connected, [&] { client.write("GET / HTTP/1.1\r\nHost: unix\r\n\r\n"); });
    QObject::connect(&client, &QIODevice::readyRead, [&] {
        got += client.readAll();
        if (got.endsWith("hello"))
            loop.quit();
    });
    client.connectToServer(name);
    CHECK(runLoop(loop) && statusOf(got) == 200);
    client.abort();

    // The QNetworkAccessManager client against the server.
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(port)));
    Fetched fetched = finish(manager.get(request));
    CHECK(fetched.status == 200 && fetched.body == "hello" && fetched.error == QNetworkReply::NoError);
    fetched = finish(manager.post(request, QByteArray("ignored")));
    CHECK(fetched.status == 200 && fetched.body == "hello");
    fetched = finish(manager.get(QNetworkRequest(QUrl(QStringLiteral("http://127.0.0.1:%1/x").arg(port)))));
    CHECK(fetched.status == 404 && fetched.error == QNetworkReply::ContentNotFoundError);
}

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
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    return cond();
}

class Gate : public QObject {
public:
    QHttpServerWebSocketUpgradeResponse check(const QHttpServerRequest &request) const {
        return request.url().path() == "/member" ? QHttpServerWebSocketUpgradeResponse::accept()
                                                 : QHttpServerWebSocketUpgradeResponse::passToNext();
    }
};

static void testWebSockets() {
    Fixture f;
    f.server.route("/", [] { return "plain http"; });
    QList<QWebSocket *> sockets;
    QStringList received;
    QObject::connect(&f.server, &QHttpServer::newWebSocketConnection, [&] {
        CHECK(f.server.hasPendingWebSocketConnections());
        std::unique_ptr<QWebSocket> socket = f.server.nextPendingWebSocketConnection();
        CHECK(socket && socket->parent() == nullptr && !f.server.hasPendingWebSocketConnections());
        QWebSocket *s = socket.release();
        s->setParent(&f.server);
        sockets.append(s);
        QObject::connect(s, &QWebSocket::textMessageReceived, [s, &received](const QString &m) {
            received.append(m);
            s->sendTextMessage(m.toUpper());
        });
    });
    CHECK(f.server.nextPendingWebSocketConnection() == nullptr);

    auto openSocket = [&](QWebSocket &ws, const QString &path, QStringList *replies) {
        bool done = false;
        QObject::connect(&ws, &QWebSocket::connected, [&] { done = true; });
        QObject::connect(&ws, &QWebSocket::errorOccurred, [&](QAbstractSocket::SocketError) { done = true; });
        QObject::connect(&ws, &QWebSocket::textMessageReceived, [replies](const QString &m) { replies->append(m); });
        ws.open(QUrl(QStringLiteral("ws://127.0.0.1:%1%2").arg(f.port).arg(path)));
        waitFor([&] { return done; });
        return ws.isValid();
    };

    // Without a verifier, upgrades are refused.
    QStringList replies;
    QWebSocket refused;
    CHECK(!openSocket(refused, "/ws", &replies) && refused.errorString().contains("403"));

    // Verifiers run in order until one decides; a destroyed context drops its verifier.
    auto *temporary = new QObject;
    f.server.addWebSocketUpgradeVerifier(
        temporary, [](const QHttpServerRequest &) { return QHttpServerWebSocketUpgradeResponse::deny(500, "never"); });
    delete temporary;
    QStringList order;
    f.server.addWebSocketUpgradeVerifier(&f.server, [&](const QHttpServerRequest &request) {
        order.append("first");
        if (request.url().path() == "/private")
            return QHttpServerWebSocketUpgradeResponse::deny(401, "who are you?");
        return QHttpServerWebSocketUpgradeResponse::passToNext();
    });
    f.server.addWebSocketUpgradeVerifier(&f.server, [&](const QHttpServerRequest &request) {
        order.append("second");
        CHECK(request.method() == Method::Get && request.url().scheme() == "http");
        return request.url().path() == "/ws" ? QHttpServerWebSocketUpgradeResponse::accept()
                                             : QHttpServerWebSocketUpgradeResponse::passToNext();
    });
    Gate gate;
    f.server.addWebSocketUpgradeVerifier(&gate, &Gate::check);

    QWebSocket client(QStringLiteral("http://app.example"));
    CHECK(openSocket(client, "/ws?x=1", &replies));
    CHECK(order == (QStringList{"first", "second"}));
    CHECK(waitFor([&] { return sockets.size() == 1; }));
    QWebSocket *peer = sockets.value(0);
    CHECK(peer && peer->requestUrl().scheme() == "ws" && peer->resourceName() == "/ws?x=1");
    CHECK(peer && peer->origin() == "http://app.example" && peer->request().rawHeader("host").startsWith("127.0.0.1"));
    client.sendTextMessage(QStringLiteral("hello"));
    CHECK(waitFor([&] { return replies.size() == 1; }) && replies.value(0) == "HELLO" &&
          received == QStringList{"hello"});

    QWebSocket member;
    CHECK(openSocket(member, "/member", &replies));

    // The port still serves HTTP, and a denied upgrade gets the verifier's status and message.
    CHECK(bodyOf(get(f.port, "/")) == "plain http");
    QWebSocket denied;
    CHECK(!openSocket(denied, "/private", &replies) && denied.errorString().contains("401"));
    QByteArray r = exchange(f.port, "GET /private HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");
    CHECK(statusOf(r) == 401 && bodyOf(r) == "who are you?" && headerOf(r, "connection") == "close");
    r = exchange(f.port, "GET /other HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");
    CHECK(statusOf(r) == 403 && bodyOf(r) == "Forbidden");

    // Invalid upgrade requests are refused before the verifiers see them.
    const int verified = static_cast<int>(order.size());
    r = exchange(f.port, "GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         "Sec-WebSocket-Version: 13\r\n\r\n");
    CHECK(statusOf(r) == 400);
    r = exchange(f.port, "GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 7\r\n\r\n");
    CHECK(statusOf(r) == 426 && headerOf(r, "sec-websocket-version") == "13");
    r = exchange(f.port, "POST /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");
    CHECK(statusOf(r) == 400 && order.size() == verified);

    // A frame sent right behind the request reaches the new socket.
    const char frame[] = {'\x81', '\x82', 0, 0, 0, 0, 'h', 'i'}; // "hi", masked with a zero key
    r = exchange(f.port,
                 "GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                 "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n" +
                     QByteArray(frame, sizeof frame),
                 [](const QByteArray &got) { return got.endsWith("HI"); });
    CHECK(r.startsWith("HTTP/1.1 101 ") && r.contains("sec-websocket-accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"));
    CHECK(r.endsWith(QByteArray("\x81\x02HI", 4)) && received.contains("hi"));

    // A local socket cannot be upgraded.
    const QString name = QStringLiteral("nt-httpserver-ws-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    auto *local = new QLocalServer;
    CHECK(local->listen(name) && f.server.bind(local));
    QLocalSocket localClient;
    QByteArray got;
    QObject::connect(&localClient, &QLocalSocket::connected, [&] {
        localClient.write("GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");
    });
    QObject::connect(&localClient, &QIODevice::readyRead, [&] { got += localClient.readAll(); });
    localClient.connectToServer(name);
    CHECK(waitFor([&] { return got.contains("\r\n\r\n"); }) && statusOf(got) == 400);
    localClient.abort();

    client.close();
    member.close();
    CHECK(waitFor([&] { return client.state() == QAbstractSocket::UnconnectedState; }));
}

#ifdef NT_TEST_TLS
static void testHttps() {
    const TestPki pki;
    QHttpServer server;
    server.route("/", [](const QHttpServerRequest &request) {
        return QString("%1 %2").arg(request.url().scheme(), request.url().host());
    });
    server.route("/echo", Method::Post, [](const QHttpServerRequest &request) {
        return QHttpServerResponse("application/octet-stream", request.body());
    });
    server.addWebSocketUpgradeVerifier(
        &server, [](const QHttpServerRequest &) { return QHttpServerWebSocketUpgradeResponse::accept(); });
    QList<QWebSocket *> sockets;
    QObject::connect(&server, &QHttpServer::newWebSocketConnection, [&] {
        QWebSocket *s = server.nextPendingWebSocketConnection().release();
        s->setParent(&server);
        sockets.append(s);
        QObject::connect(s, &QWebSocket::textMessageReceived, s,
                         [s](const QString &m) { s->sendTextMessage(m + "!"); });
    });

    auto *tls = new QSslServer;
    tls->setSslConfiguration(TestPki::serverConfiguration(pki.server));
    CHECK(tls->listen(QHostAddress::LocalHost, 0) && server.bind(tls));
    const quint16 port = tls->serverPort();

    // HTTPS through QNetworkAccessManager, trusting the test CA.
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QStringLiteral("https://127.0.0.1:%1/").arg(port)));
    request.setSslConfiguration(pki.clientConfiguration());
    Fetched fetched = finish(manager.get(request));
    CHECK(fetched.status == 200 && fetched.body == "https 127.0.0.1" && fetched.error == QNetworkReply::NoError);
    QNetworkRequest post(QUrl(QStringLiteral("https://127.0.0.1:%1/echo").arg(port)));
    post.setSslConfiguration(pki.clientConfiguration());
    QByteArray payload(500 * 1024, 'q');
    payload.append("end");
    fetched = finish(manager.post(post, payload));
    CHECK(fetched.status == 200 && fetched.body == payload);

    // Without the CA the certificate is not trusted, and the request fails.
    fetched = finish(manager.get(QNetworkRequest(QUrl(QStringLiteral("https://127.0.0.1:%1/").arg(port)))));
    CHECK(fetched.error == QNetworkReply::SslHandshakeFailedError && fetched.status == 0);

    // Plain HTTP on the TLS port gets nothing back.
    bool closed = false;
    const QByteArray plain =
        exchange(port, "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", {}, &closed, 3000);
    CHECK(closed && statusOf(plain) == -1);

    // A secure WebSocket on the HTTPS port.
    QWebSocket ws;
    ws.setSslConfiguration(pki.clientConfiguration());
    QStringList replies;
    bool connected = false;
    QObject::connect(&ws, &QWebSocket::connected, [&] { connected = true; });
    QObject::connect(&ws, &QWebSocket::textMessageReceived, [&](const QString &m) { replies.append(m); });
    ws.open(QUrl(QStringLiteral("wss://127.0.0.1:%1/live").arg(port)));
    CHECK(waitFor([&] { return connected; }));
    CHECK(waitFor([&] { return sockets.size() == 1; }) && sockets.value(0)->requestUrl().scheme() == "wss");
    CHECK(ws.sslConfiguration().peerCertificate() == pki.server.certificate);
    ws.sendTextMessage(QStringLiteral("secure"));
    CHECK(waitFor([&] { return replies.size() == 1; }) && replies.value(0) == "secure!");
    ws.close();
    CHECK(waitFor([&] { return ws.state() == QAbstractSocket::UnconnectedState; }));

    // A TLS client on a plain port fails at once.
    Fixture plainServer;
    plainServer.server.route("/", [] { return "plain"; });
    QNetworkRequest wrongPort(QUrl(QStringLiteral("https://127.0.0.1:%1/").arg(plainServer.port)));
    wrongPort.setSslConfiguration(pki.clientConfiguration());
    QElapsedTimer quick;
    quick.start();
    fetched = finish(manager.get(wrongPort));
    CHECK(fetched.error != QNetworkReply::NoError && quick.elapsed() < 3000);
}
#endif

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(captureWarnings);
    testRoutes();
    testResponses();
    testResponder();
    testProtocol();
    testServers();
    testWebSockets();
#ifdef NT_TEST_TLS
    testHttps();
#endif
    qInstallMessageHandler(nullptr);
    if (g_failures == 0)
        std::puts("all HTTP server tests passed");
    return g_failures == 0 ? 0 : 1;
}
