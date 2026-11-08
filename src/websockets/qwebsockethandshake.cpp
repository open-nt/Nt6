// MIT License - see LICENSE
#include "qwebsockethandshake_p.h"

#include <QtCore/qcryptographichash.h>
#include <QtCore/qrandom.h>

namespace QWebSocketHandshake {

namespace {

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

bool isToken(QByteArrayView s) {
    if (s.isEmpty())
        return false;
    for (char c : s) {
        if (!isTokenChar(c))
            return false;
    }
    return true;
}

} // namespace

qsizetype findHeadEnd(const QByteArray &buffer, qsizetype offset) {
    // Lines may end with a bare LF; an empty line ends the head.
    qsizetype lineStart = offset;
    while (true) {
        const qsizetype nl = buffer.indexOf('\n', lineStart);
        if (nl < 0)
            break;
        const qsizetype len = nl - lineStart;
        if (len == 0 || (len == 1 && buffer.at(lineStart) == '\r'))
            return lineStart == offset ? -1 : nl + 1; // a head cannot start with an empty line
        lineStart = nl + 1;
        if (lineStart - offset > MaxHeadSize)
            return -1;
    }
    if (buffer.size() - offset > MaxHeadSize)
        return -1;
    return 0;
}

bool parseHead(const QByteArray &head, bool response, Head *out) {
    QList<QByteArray> lines = head.split('\n');
    for (QByteArray &line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
    }
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    if (lines.isEmpty())
        return false;

    const QByteArray &start = lines.first();
    const qsizetype sp1 = start.indexOf(' ');
    const qsizetype sp2 = sp1 < 0 ? -1 : start.indexOf(' ', sp1 + 1);
    if (sp1 <= 0)
        return false;
    out->first = start.left(sp1);
    if (sp2 < 0) {
        if (!response)
            return false;
        out->second = start.mid(sp1 + 1);
        out->third.clear();
    } else {
        out->second = start.mid(sp1 + 1, sp2 - sp1 - 1);
        out->third = start.mid(sp2 + 1);
        if (!response && out->third.contains(' '))
            return false;
    }
    if (out->second.isEmpty())
        return false;

    out->headers.clear();
    if (lines.size() - 1 > MaxHeaderCount)
        return false;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray &line = lines.at(i);
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0 || line.at(0) == ' ' || line.at(0) == '\t')
            return false; // no line folding, no whitespace before the colon
        const QByteArrayView name(line.constData(), colon);
        if (!isToken(name) ||
            !out->headers.append(name, QByteArrayView(line.constData() + colon + 1, line.size() - colon - 1)))
            return false;
    }
    return true;
}

QByteArray acceptKey(QByteArrayView key) {
    QByteArray input = key.toByteArray();
    input += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    return QCryptographicHash::hash(input, QCryptographicHash::Sha1).toBase64();
}

QByteArray newKey() {
    QByteArray bytes(16, '\0');
    for (qsizetype i = 0; i < bytes.size(); i += 4) {
        const quint32 r = QRandomGenerator::system()->generate();
        for (qsizetype j = 0; j < 4; ++j)
            bytes[i + j] = static_cast<char>((r >> (8 * j)) & 0xff);
    }
    return bytes.toBase64();
}

QList<QByteArray> tokens(const QHttpHeaders &headers, QHttpHeaders::WellKnownHeader name) {
    QList<QByteArray> out;
    for (const QByteArray &part : headers.combinedValue(name).split(',')) {
        const QByteArray t = part.trimmed();
        if (!t.isEmpty())
            out.append(lower(t));
    }
    return out;
}

bool isUpgradeRequest(const QHttpHeaders &headers) { return tokens(headers, WKH::Upgrade).contains("websocket"); }

QByteArray errorResponse(int status, QByteArrayView reason, QByteArrayView extraHeaders) {
    QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + ' ';
    out.append(reason.data(), reason.size());
    out += "\r\n";
    out.append(extraHeaders.data(), extraHeaders.size());
    out += "content-length: 0\r\nconnection: close\r\n\r\n";
    return out;
}

ServerAnswer answer(const QHttpHeaders &headers, const QStringList &supportedSubprotocols,
                    const QByteArray &serverName) {
    ServerAnswer a;
    if (headers.values(WKH::Host).size() != 1) {
        a.errorString = QStringLiteral("The request has no Host header, or more than one");
        a.response = errorResponse(400, "Bad Request");
        return a;
    }
    if (!isUpgradeRequest(headers) || !tokens(headers, WKH::Connection).contains("upgrade")) {
        a.errorString = QStringLiteral("The request is not a WebSocket upgrade");
        a.response = errorResponse(400, "Bad Request");
        return a;
    }
    if (!tokens(headers, WKH::SecWebSocketVersion).contains("13")) {
        a.errorString = QStringLiteral("Unsupported WebSocket version");
        a.closeCode = QWebSocketProtocol::CloseCodeProtocolError;
        a.response = errorResponse(426, "Upgrade Required", "sec-websocket-version: 13\r\n");
        return a;
    }
    const QList<QByteArray> keys = headers.values(WKH::SecWebSocketKey);
    const QByteArray key = keys.size() == 1 ? keys.first() : QByteArray();
    if (key.size() != 24 || QByteArray::fromBase64(key).size() != 16 || QByteArray::fromBase64(key).toBase64() != key) {
        a.errorString = QStringLiteral("Invalid Sec-WebSocket-Key");
        a.response = errorResponse(400, "Bad Request");
        return a;
    }

    // The first subprotocol of the client's list, in its order of preference, that the server supports.
    for (const QByteArray &offered : headers.combinedValue(WKH::SecWebSocketProtocol).split(',')) {
        const QString name = QString::fromLatin1(offered.trimmed());
        if (!name.isEmpty() && supportedSubprotocols.contains(name)) {
            a.subprotocol = name;
            break;
        }
    }

    QHttpHeaders out;
    if (!serverName.isEmpty())
        out.append(WKH::Server, serverName); // skipped if it is not a valid header value
    out.append(WKH::Upgrade, "websocket");
    out.append(WKH::Connection, "Upgrade");
    out.append(WKH::SecWebSocketAccept, acceptKey(key));
    if (!a.subprotocol.isEmpty())
        out.append(WKH::SecWebSocketProtocol, a.subprotocol.toLatin1());
    a.response = "HTTP/1.1 101 Switching Protocols\r\n";
    for (qsizetype i = 0; i < out.size(); ++i) {
        const QLatin1StringView name = out.nameAt(i);
        const QByteArrayView value = out.valueAt(i);
        a.response.append(name.data(), name.size());
        a.response += ": ";
        a.response.append(value.data(), value.size());
        a.response += "\r\n";
    }
    a.response += "\r\n";
    a.accepted = true;
    return a;
}

} // namespace QWebSocketHandshake
