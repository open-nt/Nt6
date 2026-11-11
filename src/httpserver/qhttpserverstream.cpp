// MIT License - see LICENSE
#include "qhttpserverstream_p.h"
#include "qwebsocket_p.h"
#include "qwebsockethandshake_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qtimer.h>
#include <QtHttpServer/qabstracthttpserver.h>
#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qhostaddress.h>
#ifndef QT_NO_LOCALSOCKET
#include <QtNetwork/qlocalsocket.h>
#endif
#include <QtNetwork/qnetworkrequest.h>
#include <QtWebSockets/qwebsocket.h>

#include <limits>

namespace {

constexpr qint64 DeviceChunkSize = 64 * 1024;
constexpr qint64 SocketHighWater = 256 * 1024;
constexpr qsizetype MaxChunkLineSize = 4096;
// How much of the next requests is read ahead while a response is pending.
constexpr qsizetype MaxReadAhead = 256 * 1024;

using Method = QHttpServerRequest::Method;
using WKH = QHttpHeaders::WellKnownHeader;

char asciiLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

QByteArray lower(QByteArrayView in) {
    QByteArray out(in.data(), in.size());
    for (char &c : out)
        c = asciiLower(c);
    return out;
}

bool isTokenChar(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        return true;
    switch (c) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
        return true;
    default:
        return false;
    }
}

bool isToken(const QByteArray &s) {
    if (s.isEmpty())
        return false;
    for (char c : s) {
        if (!isTokenChar(c))
            return false;
    }
    return true;
}

Method methodFromName(const QByteArray &name) {
    // Methods are case-sensitive (RFC 9110, section 9.1).
    if (name == "GET")
        return Method::Get;
    if (name == "PUT")
        return Method::Put;
    if (name == "DELETE")
        return Method::Delete;
    if (name == "POST")
        return Method::Post;
    if (name == "HEAD")
        return Method::Head;
    if (name == "OPTIONS")
        return Method::Options;
    if (name == "PATCH")
        return Method::Patch;
    if (name == "CONNECT")
        return Method::Connect;
    if (name == "TRACE")
        return Method::Trace;
    return Method::Unknown;
}

// The comma-separated tokens of a header field, trimmed and in lower case.
QList<QByteArray> tokens(const QHttpHeaders &headers, WKH name) {
    QList<QByteArray> out;
    const QByteArray value = headers.combinedValue(name);
    for (const QByteArray &part : value.split(',')) {
        const QByteArray t = part.trimmed();
        if (!t.isEmpty())
            out.append(lower(t));
    }
    return out;
}

bool parseDecimal(const QByteArray &s, qint64 *out) {
    if (s.isEmpty())
        return false;
    qint64 v = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
        const int digit = c - '0';
        if (v > (std::numeric_limits<qint64>::max() - digit) / 10)
            return false;
        v = v * 10 + digit;
    }
    *out = v;
    return true;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool statusAllowsBody(QHttpServerStream::StatusCode status) {
    const int code = static_cast<int>(status);
    return code >= 200 && code != 204 && code != 304;
}

bool validHost(const QByteArray &host) {
    for (char c : host) {
        if (c == '/' || c == '?' || c == '#' || c == '@' || c == '\\' || c == ' ' || c == '\t' ||
            static_cast<unsigned char>(c) < 0x21 || static_cast<unsigned char>(c) == 0x7f)
            return false;
    }
    return true;
}

QByteArray hostForUrl(const QHostAddress &address, quint16 port) {
    QByteArray host = address.toString().toLatin1();
    if (address.protocol() == QAbstractSocket::IPv6Protocol)
        host = QByteArray("[") + host + QByteArray("]");
    if (host.isEmpty() || host == "[]")
        host = "localhost";
    if (port != 0)
        host += QByteArray(":") + QByteArray::number(port);
    return host;
}

} // namespace

namespace {

// The socket as a local socket, if it is one. Systems without local sockets, such as Zephyr, have none.
QLocalSocket *asLocalSocket(QIODevice *socket) {
#ifndef QT_NO_LOCALSOCKET
    return qobject_cast<QLocalSocket *>(socket);
#else
    (void)socket;
    return nullptr;
#endif
}

} // namespace

QHttpServerStream::QHttpServerStream(QAbstractHttpServer *server, QIODevice *socket, bool encrypted)
    : QObject(server), m_server(server), m_socket(socket), m_tcpSocket(qobject_cast<QAbstractSocket *>(socket)),
      m_localSocket(asLocalSocket(socket)), m_encrypted(encrypted) {
    socket->setParent(this);
    if (m_tcpSocket) {
        m_tcpSocket->setReadBufferSize(MaxReadAhead);
        QObject::connect(m_tcpSocket, &QAbstractSocket::disconnected, this, [this] { socketDisconnected(); });
    }
#ifndef QT_NO_LOCALSOCKET
    else if (m_localSocket) {
        m_localSocket->setReadBufferSize(MaxReadAhead);
        QObject::connect(m_localSocket, &QLocalSocket::disconnected, this, [this] { socketDisconnected(); });
    }
#endif
    QObject::connect(socket, &QIODevice::readyRead, this, [this] { process(); });
    QObject::connect(socket, &QIODevice::bytesWritten, this, [this](qint64) {
        if (m_response == Response::Device)
            pumpDevice();
    });
    // Data may have arrived before the stream was created.
    scheduleProcess();
}

QHttpServerStream::~QHttpServerStream() = default;

bool QHttpServerStream::canRespond() const { return !m_canceled && m_response == Response::Waiting; }

void QHttpServerStream::socketDisconnected() {
    if (m_canceled)
        return;
    m_canceled = true;
    if (m_device) {
        m_device->disconnect(this);
        m_device->deleteLater();
        m_device = nullptr;
    }
    deleteLater();
}

void QHttpServerStream::readFromSocket() {
    if (m_canceled || m_closing)
        return;
    if (m_response != Response::Idle && m_buffer.size() - m_offset >= MaxReadAhead)
        return; // leave the rest in the socket until the pending response is done
    if (m_offset > 0 && m_offset == m_buffer.size()) {
        m_buffer.clear();
        m_offset = 0;
    } else if (m_offset > 64 * 1024 && m_offset * 2 > m_buffer.size()) {
        m_buffer.remove(0, m_offset);
        m_offset = 0;
    }
    const qint64 available = m_socket->bytesAvailable();
    if (available > 0)
        m_buffer += m_socket->read(available);
}

void QHttpServerStream::scheduleProcess() {
    if (m_processScheduled)
        return;
    m_processScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_processScheduled = false;
        process();
    });
}

void QHttpServerStream::process() {
    if (m_processing || m_canceled)
        return;
    m_processing = true;
    readFromSocket();
    while (!m_canceled && !m_closing && m_response == Response::Idle && m_parse != Parse::Failed) {
        if (!parseStep())
            break;
        readFromSocket();
    }
    m_processing = false;
}

bool QHttpServerStream::takeLine(QByteArray *line, qsizetype limit, bool *tooLong) {
    *tooLong = false;
    const qsizetype nl = m_buffer.indexOf('\n', m_offset);
    if (nl < 0) {
        if (m_buffer.size() - m_offset > limit)
            *tooLong = true;
        return false;
    }
    qsizetype end = nl;
    if (end > m_offset && m_buffer.at(end - 1) == '\r')
        --end;
    if (end - m_offset > limit) {
        *tooLong = true;
        return false;
    }
    *line = m_buffer.mid(m_offset, end - m_offset);
    m_offset = nl + 1;
    return true;
}

// Parses as much as the buffer allows; returns true if it made progress.
bool QHttpServerStream::parseStep() {
    QByteArray line;
    bool tooLong = false;
    switch (m_parse) {
    case Parse::RequestLine: {
        // A request line is printable ASCII. Anything else, such as a TLS handshake sent to a plain
        // port, is refused at once instead of waiting for a line that never ends.
        const qsizetype newline = m_buffer.indexOf('\n', m_offset);
        const qsizetype checked = newline < 0 ? m_buffer.size() : newline;
        for (qsizetype i = m_offset; i < checked; ++i) {
            const auto c = static_cast<unsigned char>(m_buffer.at(i));
            if ((c < 0x20 && c != '\r') || c >= 0x7f) {
                fail(StatusCode::BadRequest);
                return false;
            }
        }
        if (!takeLine(&line, MaxRequestLineSize, &tooLong)) {
            if (tooLong)
                fail(StatusCode::UriTooLong);
            return false;
        }
        if (line.isEmpty())
            return true; // empty lines before a request are ignored (RFC 9112, section 2.2)
        if (!parseRequestLine(line))
            return false;
        m_parse = Parse::Headers;
        return true;
    }

    case Parse::Headers:
    case Parse::Trailers: {
        const qsizetype limit = MaxHeaderSize - m_headerBytes;
        if (!takeLine(&line, limit, &tooLong)) {
            if (tooLong)
                fail(StatusCode::RequestHeaderFieldsTooLarge);
            return false;
        }
        m_headerBytes += line.size() + 2;
        if (m_parse == Parse::Trailers) {
            // Trailer fields are read and dropped.
            if (line.isEmpty())
                dispatch();
            return true;
        }
        if (line.isEmpty())
            return headersComplete();
        return parseHeaderLine(line);
    }

    case Parse::Body: {
        const qsizetype have = m_buffer.size() - m_offset;
        if (have <= 0)
            return false;
        const qsizetype take = static_cast<qsizetype>(qMin<qint64>(m_remaining, have));
        m_body.append(m_buffer.constData() + m_offset, take);
        m_offset += take;
        m_remaining -= take;
        if (m_remaining == 0)
            dispatch();
        return true;
    }

    case Parse::ChunkSize: {
        if (!takeLine(&line, MaxChunkLineSize, &tooLong)) {
            if (tooLong)
                fail(StatusCode::BadRequest);
            return false;
        }
        qsizetype end = line.indexOf(';'); // chunk extensions are ignored
        if (end < 0)
            end = line.size();
        while (end > 0 && (line.at(end - 1) == ' ' || line.at(end - 1) == '\t'))
            --end;
        if (end == 0 || end > 15) {
            fail(StatusCode::BadRequest);
            return false;
        }
        qint64 size = 0;
        for (qsizetype i = 0; i < end; ++i) {
            const int h = hexValue(line.at(i));
            if (h < 0) {
                fail(StatusCode::BadRequest);
                return false;
            }
            size = size * 16 + h;
        }
        if (size == 0) {
            m_parse = Parse::Trailers;
            return true;
        }
        if (size > MaxBodySize - m_body.size()) {
            fail(StatusCode::PayloadTooLarge);
            return false;
        }
        m_remaining = size;
        m_parse = Parse::ChunkData;
        return true;
    }

    case Parse::ChunkData: {
        const qsizetype have = m_buffer.size() - m_offset;
        if (have <= 0)
            return false;
        const qsizetype take = static_cast<qsizetype>(qMin<qint64>(m_remaining, have));
        m_body.append(m_buffer.constData() + m_offset, take);
        m_offset += take;
        m_remaining -= take;
        if (m_remaining == 0)
            m_parse = Parse::ChunkDataEnd;
        return true;
    }

    case Parse::ChunkDataEnd:
        if (!takeLine(&line, 2, &tooLong)) {
            if (tooLong)
                fail(StatusCode::BadRequest);
            return false;
        }
        if (!line.isEmpty()) {
            fail(StatusCode::BadRequest);
            return false;
        }
        m_parse = Parse::ChunkSize;
        return true;

    case Parse::Failed:
        return false;
    }
    return false;
}

bool QHttpServerStream::parseRequestLine(const QByteArray &line) {
    const QList<QByteArray> parts = line.split(' ');
    if (parts.size() != 3 || !isToken(parts.at(0)) || parts.at(1).isEmpty()) {
        fail(StatusCode::BadRequest);
        return false;
    }
    for (char c : parts.at(1)) {
        if (static_cast<unsigned char>(c) <= 0x20 || static_cast<unsigned char>(c) >= 0x7f) {
            fail(StatusCode::BadRequest);
            return false;
        }
    }
    const QByteArray &version = parts.at(2);
    if (version.size() != 8 || !version.startsWith("HTTP/") || version.at(6) != '.' || version.at(5) < '0' ||
        version.at(5) > '9' || version.at(7) < '0' || version.at(7) > '9') {
        fail(StatusCode::BadRequest);
        return false;
    }
    if (version.at(5) != '1') {
        fail(StatusCode::HttpVersionNotSupported);
        return false;
    }
    m_method = parts.at(0);
    m_target = parts.at(1);
    m_minorVersion = version.at(7) - '0';
    return true;
}

bool QHttpServerStream::parseHeaderLine(const QByteArray &line) {
    if (m_headers.size() >= MaxHeaderCount) {
        fail(StatusCode::RequestHeaderFieldsTooLarge);
        return false;
    }
    // Obsolete line folding and whitespace before the colon are refused (RFC 9112, sections 5.1 and 5.2).
    const qsizetype colon = line.indexOf(':');
    if (colon <= 0 || line.at(0) == ' ' || line.at(0) == '\t') {
        fail(StatusCode::BadRequest);
        return false;
    }
    const QByteArray name = line.left(colon);
    if (!isToken(name) ||
        !m_headers.append(name, QByteArrayView(line.constData() + colon + 1, line.size() - colon - 1))) {
        fail(StatusCode::BadRequest);
        return false;
    }
    return true;
}

bool QHttpServerStream::headersComplete() {
    m_http10 = m_minorVersion == 0;

    const QList<QByteArray> hosts = m_headers.values(WKH::Host);
    if (hosts.size() > 1 || (!m_http10 && hosts.isEmpty()) || (!hosts.isEmpty() && !validHost(hosts.first()))) {
        fail(StatusCode::BadRequest);
        return false;
    }

    const QList<QByteArray> connection = tokens(m_headers, WKH::Connection);
    m_closeAfterResponse = m_http10 ? !connection.contains("keep-alive") : connection.contains("close");

    // The framing of the body. A request with both Transfer-Encoding and Content-Length could be
    // read differently by a proxy in front of the server, so it is refused (RFC 9112, section 6.1).
    bool chunked = false;
    qint64 length = 0;
    if (m_headers.contains(WKH::TransferEncoding)) {
        const QList<QByteArray> codings = tokens(m_headers, WKH::TransferEncoding);
        if (m_http10 || m_headers.contains(WKH::ContentLength) || codings.isEmpty() || codings.last() != "chunked") {
            fail(StatusCode::BadRequest);
            return false;
        }
        if (codings.size() != 1) {
            fail(StatusCode::NotImplemented);
            return false;
        }
        chunked = true;
    } else if (m_headers.contains(WKH::ContentLength)) {
        bool first = true;
        for (const QByteArray &value : m_headers.values(WKH::ContentLength)) {
            for (const QByteArray &part : value.split(',')) {
                qint64 v = 0;
                if (!parseDecimal(part.trimmed(), &v) || (!first && v != length)) {
                    fail(StatusCode::BadRequest);
                    return false;
                }
                length = v;
                first = false;
            }
        }
        if (length > MaxBodySize) {
            fail(StatusCode::PayloadTooLarge);
            return false;
        }
    }

    if (m_headers.contains(WKH::Expect)) {
        const QList<QByteArray> expect = tokens(m_headers, WKH::Expect);
        if (expect.size() != 1 || expect.first() != "100-continue") {
            fail(StatusCode::ExpectationFailed);
            return false;
        }
        if (!m_http10 && (chunked || length > 0))
            send("HTTP/1.1 100 Continue\r\n\r\n");
    }

    if (chunked) {
        m_parse = Parse::ChunkSize;
    } else if (length > 0) {
        m_remaining = length;
        // The client's claim is not trusted with more than a megabyte up front.
        m_body.reserve(static_cast<qsizetype>(qMin<qint64>(length, 1024 * 1024)));
        m_parse = Parse::Body;
    } else {
        dispatch();
    }
    return true;
}

void QHttpServerStream::dispatch() {
    QHttpServerRequest request;
    request.m_method = methodFromName(m_method);
    request.m_headers = std::move(m_headers);
    request.m_body = std::move(m_body);

    QHostAddress localAddress;
    quint16 localPort = 0;
    if (m_tcpSocket) {
        request.m_remoteAddress = m_tcpSocket->peerAddress();
        request.m_remotePort = m_tcpSocket->peerPort();
        localAddress = m_tcpSocket->localAddress();
        localPort = m_tcpSocket->localPort();
        request.m_localAddress = localAddress;
        request.m_localPort = localPort;
    }

    const QByteArray scheme = m_encrypted ? "https" : "http";
    QByteArray host = request.m_headers.value(WKH::Host).toByteArray();
    if (host.isEmpty())
        host = hostForUrl(localAddress, localPort);
    QUrl url;
    const QByteArray lowerTarget = lower(m_target.left(8));
    if (m_target.startsWith('/')) {
        url = QUrl::fromEncoded(scheme + "://" + host + m_target);
    } else if (lowerTarget.startsWith("http://") || lowerTarget.startsWith("https://")) {
        url = QUrl::fromEncoded(m_target);
    } else if (m_target == "*" && request.m_method == Method::Options) {
        url = QUrl::fromEncoded(scheme + "://" + host);
    } else if (request.m_method == Method::Connect && validHost(m_target)) {
        url = QUrl::fromEncoded(scheme + "://" + m_target);
    }
    if (!url.isValid()) {
        fail(StatusCode::BadRequest);
        return;
    }
    request.m_url = url;

    if (QWebSocketHandshake::isUpgradeRequest(request.m_headers)) {
        upgradeToWebSocket(request);
        return;
    }

    m_headRequest = request.m_method == Method::Head;
    m_response = Response::Waiting;
    resetRequest();

    QHttpServerResponder responder(this);
    m_server->dispatch(request, responder);
}

void QHttpServerStream::upgradeToWebSocket(const QHttpServerRequest &request) {
    // Upgrades need a valid GET over HTTP/1.1 on a TCP connection (RFC 6455, section 4.1); the
    // verifiers only see requests that could be accepted.
    if (!m_tcpSocket || m_http10 || request.method() != Method::Get) {
        fail(StatusCode::BadRequest);
        return;
    }
    const QWebSocketHandshake::ServerAnswer answer =
        QWebSocketHandshake::answer(request.headers(), QStringList(), QByteArray());
    if (!answer.accepted) {
        m_parse = Parse::Failed;
        send(answer.response);
        closeConnection();
        return;
    }
    const QHttpServerWebSocketUpgradeResponse verdict = m_server->verifyWebSocketUpgrade(request);
    if (m_canceled)
        return;
    if (verdict.type() != QHttpServerWebSocketUpgradeResponse::ResponseType::Accept) {
        const bool denied = verdict.type() == QHttpServerWebSocketUpgradeResponse::ResponseType::Deny;
        const int status = denied ? verdict.denyStatus() : 403;
        const QByteArray body = denied ? verdict.denyMessage() : QByteArray("Forbidden");
        m_parse = Parse::Failed;
        send("HTTP/1.1 " + QByteArray::number(status) + " " + reasonPhrase(static_cast<StatusCode>(status)) +
             "\r\ncontent-type: text/plain\r\ncontent-length: " + QByteArray::number(body.size()) +
             "\r\nconnection: close\r\n\r\n" + body);
        closeConnection();
        return;
    }
    send(answer.response);

    // The socket now belongs to a QWebSocket, with the bytes that followed the request.
    QAbstractSocket *socket = m_tcpSocket;
    const QByteArray pending = m_buffer.mid(m_offset);
    socket->disconnect(this);
    m_canceled = true;
    m_socket = nullptr;
    m_tcpSocket = nullptr;

    QUrl wsUrl = request.url();
    wsUrl.setScheme(m_encrypted ? QStringLiteral("wss") : QStringLiteral("ws"));
    QNetworkRequest wsRequest(wsUrl);
    for (qsizetype i = 0; i < request.headers().size(); ++i) {
        const QLatin1StringView name = request.headers().nameAt(i);
        wsRequest.setRawHeader(QByteArray(name.data(), name.size()), request.headers().valueAt(i).toByteArray());
    }
    const QString origin = QString::fromUtf8(request.headers().value(WKH::Origin).toByteArray());
    QWebSocket *ws = QWebSocketPrivate::createServerSide(socket, wsRequest, origin, QString(), pending, nullptr);
    m_server->addWebSocket(ws);
    deleteLater();
}

void QHttpServerStream::resetRequest() {
    m_parse = Parse::RequestLine;
    m_headerBytes = 0;
    m_remaining = 0;
    m_method.clear();
    m_target.clear();
    m_headers.clear();
    m_body.clear();
}

void QHttpServerStream::fail(StatusCode status) {
    m_parse = Parse::Failed;
    if (m_response == Response::Idle && !m_canceled) {
        QByteArray out = "HTTP/1.1 " + QByteArray::number(static_cast<int>(status)) + ' ' + reasonPhrase(status) +
                         "\r\ncontent-length: 0\r\nconnection: close\r\n\r\n";
        send(out);
    }
    closeConnection();
}

QByteArray QHttpServerStream::head(StatusCode status, const QHttpHeaders &headers, qint64 contentLength, bool chunked) {
    const QList<QByteArray> connection = tokens(headers, WKH::Connection);
    if (connection.contains("close"))
        m_closeAfterResponse = true;
    QByteArray out = "HTTP/1.1 " + QByteArray::number(static_cast<int>(status)) + ' ' + reasonPhrase(status) + "\r\n";
    for (qsizetype i = 0; i < headers.size(); ++i) {
        const QLatin1StringView name = headers.nameAt(i);
        const QByteArrayView n(name.data(), name.size());
        // The framing is decided here, not by the handler.
        if (n == "content-length" || n == "transfer-encoding" || n == "connection")
            continue;
        out.append(n.data(), n.size());
        out += ": ";
        const QByteArrayView v = headers.valueAt(i);
        out.append(v.data(), v.size());
        out += "\r\n";
    }
    if (statusAllowsBody(status)) {
        if (chunked)
            out += "transfer-encoding: chunked\r\n";
        else if (contentLength >= 0)
            out += "content-length: " + QByteArray::number(contentLength) + "\r\n";
    }
    if (m_closeAfterResponse)
        out += "connection: close\r\n";
    else if (m_http10)
        out += "connection: keep-alive\r\n";
    out += "\r\n";
    return out;
}

void QHttpServerStream::send(const QByteArray &data) {
    if (!m_canceled && !data.isEmpty())
        m_socket->write(data);
}

void QHttpServerStream::respond(StatusCode status, const QHttpHeaders &headers, const QByteArray &body) {
    if (!canRespond())
        return;
    const bool withBody = statusAllowsBody(status);
    QByteArray out = head(status, headers, withBody ? body.size() : -1, false);
    if (withBody && !m_headRequest)
        out += body;
    send(out);
    finishResponse();
}

void QHttpServerStream::respondWithDevice(StatusCode status, const QHttpHeaders &headers, QIODevice *device) {
    if (!canRespond()) {
        device->deleteLater();
        return;
    }
    if (!device->isOpen() && !device->open(QIODevice::ReadOnly)) {
        qWarning("QHttpServerResponder::write: the device could not be opened");
        device->deleteLater();
        respond(StatusCode::InternalServerError, QHttpHeaders(), QByteArray());
        return;
    }
    if (!device->isReadable()) {
        qWarning("QHttpServerResponder::write: the device is not readable");
        device->deleteLater();
        respond(StatusCode::InternalServerError, QHttpHeaders(), QByteArray());
        return;
    }
    if (!device->isSequential()) {
        const qint64 length = qMax<qint64>(0, device->size() - device->pos());
        send(head(status, headers, statusAllowsBody(status) ? length : -1, false));
        m_deviceRemaining = length;
        m_chunkedOutput = false;
    } else {
        // A device of unknown length is sent chunked, or, to an HTTP/1.0 client, up to the end of the connection.
        if (m_http10)
            m_closeAfterResponse = true;
        m_chunkedOutput = !m_http10;
        send(head(status, headers, -1, m_chunkedOutput));
        m_deviceRemaining = -1;
    }
    if (m_headRequest || !statusAllowsBody(status)) {
        device->deleteLater();
        finishResponse();
        return;
    }
    m_device = device;
    m_deviceEnded = false;
    m_response = Response::Device;
    device->setParent(this);
    QObject::connect(device, &QIODevice::readyRead, this, [this] { pumpDevice(); });
    QObject::connect(device, &QIODevice::readChannelFinished, this, [this] {
        m_deviceEnded = true;
        pumpDevice();
    });
    QObject::connect(device, &QIODevice::aboutToClose, this, [this] {
        m_deviceEnded = true;
        pumpDevice();
    });
    pumpDevice();
}

void QHttpServerStream::pumpDevice() {
    if (m_response != Response::Device || !m_device || m_canceled)
        return;
    QIODevice *device = m_device;
    while (m_socket->bytesToWrite() < SocketHighWater) {
        qint64 want = DeviceChunkSize;
        if (m_deviceRemaining >= 0)
            want = qMin(want, m_deviceRemaining);
        if (want == 0)
            break;
        const QByteArray data = device->read(want);
        if (data.isEmpty()) {
            if (m_deviceRemaining >= 0) {
                // The device is shorter than it said; the promised length cannot be sent any more.
                if (device->atEnd()) {
                    qWarning("QHttpServerResponder::write: the device ended early");
                    m_closeAfterResponse = true;
                    m_deviceRemaining = 0;
                }
            } else if (!device->isOpen() || m_deviceEnded) {
                m_deviceRemaining = 0;
            }
            break;
        }
        if (m_deviceRemaining >= 0)
            m_deviceRemaining -= data.size();
        if (m_chunkedOutput)
            send(QByteArray::number(data.size(), 16) + "\r\n" + data + "\r\n");
        else
            send(data);
    }
    if (m_deviceRemaining == 0)
        finishDevice();
}

void QHttpServerStream::finishDevice() {
    if (m_device) {
        m_device->disconnect(this);
        m_device->deleteLater();
        m_device = nullptr;
    }
    if (m_chunkedOutput)
        send("0\r\n\r\n");
    m_chunkedOutput = false;
    m_deviceRemaining = -1;
    finishResponse();
}

void QHttpServerStream::beginChunked(StatusCode status, const QHttpHeaders &headers,
                                     const QList<QByteArray> &trailerNames) {
    if (!canRespond())
        return;
    QHttpHeaders h = headers;
    if (m_http10) {
        m_closeAfterResponse = true;
        m_chunkedOutput = false;
    } else {
        m_chunkedOutput = true;
        QByteArray names;
        for (const QByteArray &name : trailerNames) {
            if (!names.isEmpty())
                names += ", ";
            names += name;
        }
        if (!names.isEmpty())
            h.replaceOrAppend(WKH::Trailer, names);
    }
    send(head(status, h, -1, m_chunkedOutput));
    m_response = Response::Chunked;
}

void QHttpServerStream::writeChunk(const QByteArray &data) {
    if (m_response != Response::Chunked || m_canceled) {
        qWarning("QHttpServerResponder::writeChunk: no chunked response was begun");
        return;
    }
    if (data.isEmpty() || m_headRequest)
        return; // an empty chunk would end the body
    if (m_chunkedOutput)
        send(QByteArray::number(data.size(), 16) + "\r\n" + data + "\r\n");
    else
        send(data);
}

void QHttpServerStream::endChunked(const QByteArray &data, const QHttpHeaders &trailers) {
    if (m_response != Response::Chunked || m_canceled) {
        qWarning("QHttpServerResponder::writeEndChunked: no chunked response was begun");
        return;
    }
    writeChunk(data);
    if (m_chunkedOutput && !m_headRequest) {
        QByteArray out = "0\r\n";
        for (qsizetype i = 0; i < trailers.size(); ++i) {
            const QLatin1StringView name = trailers.nameAt(i);
            const QByteArrayView v = trailers.valueAt(i);
            out.append(name.data(), name.size());
            out += ": ";
            out.append(v.data(), v.size());
            out += "\r\n";
        }
        out += "\r\n";
        send(out);
    }
    m_chunkedOutput = false;
    finishResponse();
}

void QHttpServerStream::responderDestroyed() {
    if (m_canceled)
        return;
    if (m_response == Response::Waiting) {
        qWarning("QHttpServerResponder: destroyed without a response; sending 500 Internal Server Error");
        respond(StatusCode::InternalServerError, QHttpHeaders(), QByteArray());
    } else if (m_response == Response::Chunked) {
        qWarning("QHttpServerResponder: destroyed before the chunked response ended; closing the connection");
        m_response = Response::Idle;
        closeConnection();
    }
}

void QHttpServerStream::finishResponse() {
    m_response = Response::Idle;
    if (m_closeAfterResponse) {
        closeConnection();
        return;
    }
    if (!m_processing)
        scheduleProcess();
}

void QHttpServerStream::closeConnection() {
    if (m_closing)
        return;
    m_closing = true;
    if (m_canceled)
        return;
    // Both close gracefully: the data still queued is written first.
    if (m_tcpSocket)
        m_tcpSocket->disconnectFromHost();
#ifndef QT_NO_LOCALSOCKET
    else if (m_localSocket)
        m_localSocket->disconnectFromServer();
#endif
    else
        m_socket->close();
}

QByteArray QHttpServerStream::reasonPhrase(StatusCode status) {
    switch (status) {
    case StatusCode::Continue:
        return "Continue";
    case StatusCode::SwitchingProtocols:
        return "Switching Protocols";
    case StatusCode::Processing:
        return "Processing";
    case StatusCode::EarlyHints:
        return "Early Hints";
    case StatusCode::Ok:
        return "OK";
    case StatusCode::Created:
        return "Created";
    case StatusCode::Accepted:
        return "Accepted";
    case StatusCode::NonAuthoritativeInformation:
        return "Non-Authoritative Information";
    case StatusCode::NoContent:
        return "No Content";
    case StatusCode::ResetContent:
        return "Reset Content";
    case StatusCode::PartialContent:
        return "Partial Content";
    case StatusCode::MultiStatus:
        return "Multi-Status";
    case StatusCode::AlreadyReported:
        return "Already Reported";
    case StatusCode::IMUsed:
        return "IM Used";
    case StatusCode::MultipleChoices:
        return "Multiple Choices";
    case StatusCode::MovedPermanently:
        return "Moved Permanently";
    case StatusCode::Found:
        return "Found";
    case StatusCode::SeeOther:
        return "See Other";
    case StatusCode::NotModified:
        return "Not Modified";
    case StatusCode::UseProxy:
        return "Use Proxy";
    case StatusCode::TemporaryRedirect:
        return "Temporary Redirect";
    case StatusCode::PermanentRedirect:
        return "Permanent Redirect";
    case StatusCode::BadRequest:
        return "Bad Request";
    case StatusCode::Unauthorized:
        return "Unauthorized";
    case StatusCode::PaymentRequired:
        return "Payment Required";
    case StatusCode::Forbidden:
        return "Forbidden";
    case StatusCode::NotFound:
        return "Not Found";
    case StatusCode::MethodNotAllowed:
        return "Method Not Allowed";
    case StatusCode::NotAcceptable:
        return "Not Acceptable";
    case StatusCode::ProxyAuthenticationRequired:
        return "Proxy Authentication Required";
    case StatusCode::RequestTimeout:
        return "Request Timeout";
    case StatusCode::Conflict:
        return "Conflict";
    case StatusCode::Gone:
        return "Gone";
    case StatusCode::LengthRequired:
        return "Length Required";
    case StatusCode::PreconditionFailed:
        return "Precondition Failed";
    case StatusCode::PayloadTooLarge:
        return "Content Too Large";
    case StatusCode::UriTooLong:
        return "URI Too Long";
    case StatusCode::UnsupportedMediaType:
        return "Unsupported Media Type";
    case StatusCode::RequestRangeNotSatisfiable:
        return "Range Not Satisfiable";
    case StatusCode::ExpectationFailed:
        return "Expectation Failed";
    case StatusCode::ImATeapot:
        return "I'm a teapot";
    case StatusCode::MisdirectedRequest:
        return "Misdirected Request";
    case StatusCode::UnprocessableEntity:
        return "Unprocessable Content";
    case StatusCode::Locked:
        return "Locked";
    case StatusCode::FailedDependency:
        return "Failed Dependency";
    case StatusCode::TooEarly:
        return "Too Early";
    case StatusCode::UpgradeRequired:
        return "Upgrade Required";
    case StatusCode::PreconditionRequired:
        return "Precondition Required";
    case StatusCode::TooManyRequests:
        return "Too Many Requests";
    case StatusCode::RequestHeaderFieldsTooLarge:
        return "Request Header Fields Too Large";
    case StatusCode::UnavailableForLegalReasons:
        return "Unavailable For Legal Reasons";
    case StatusCode::InternalServerError:
        return "Internal Server Error";
    case StatusCode::NotImplemented:
        return "Not Implemented";
    case StatusCode::BadGateway:
        return "Bad Gateway";
    case StatusCode::ServiceUnavailable:
        return "Service Unavailable";
    case StatusCode::GatewayTimeout:
        return "Gateway Timeout";
    case StatusCode::HttpVersionNotSupported:
        return "HTTP Version Not Supported";
    case StatusCode::VariantAlsoNegotiates:
        return "Variant Also Negotiates";
    case StatusCode::InsufficientStorage:
        return "Insufficient Storage";
    case StatusCode::LoopDetected:
        return "Loop Detected";
    case StatusCode::NotExtended:
        return "Not Extended";
    case StatusCode::NetworkAuthenticationRequired:
        return "Network Authentication Required";
    case StatusCode::NetworkConnectTimeoutError:
        return "Network Connect Timeout Error";
    }
    return "Unknown";
}
