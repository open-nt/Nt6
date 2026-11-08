// MIT License - see LICENSE
#include "qwebsocket_p.h"
#include "qwebsockethandshake_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qrandom.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtWebSockets/qmaskgenerator.h>
#include <QtWebSockets/qwebsocket.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslsocket.h>
#endif

#include <limits>

/*!
    \class QWebSocket
    \brief The QWebSocket class is a WebSocket connection, as a client or as one end accepted by a server.
    \since 5.3

    A QWebSocket sends and receives messages over the WebSocket protocol
    (RFC 6455). As a client, call open() with a \c ws: or \c wss: URL; the
    socket connects, performs the opening handshake, and emits connected().
    A QWebSocketServer, or a QAbstractHttpServer that accepts an upgrade,
    hands out sockets that are already connected.

    \snippet websockets/echo.cpp client

    Messages are text (a QString, sent as UTF-8) or binary (a QByteArray).
    They arrive whole through textMessageReceived() and
    binaryMessageReceived(), and also frame by frame through
    textFrameReceived() and binaryFrameReceived(). Pings are answered
    automatically; ping() measures the round trip.

    close() starts the closing handshake: a close frame with a code and a
    reason goes to the peer, which answers with its own, and then the
    connection closes and disconnected() is emitted. closeCode() and
    closeReason() say why a connection closed. A peer that breaks the
    protocol, for example by sending text that is not UTF-8, is sent a close
    frame with the matching code, and the connection is closed.

    Incoming frames and messages are limited by
    setMaxAllowedIncomingFrameSize() and
    setMaxAllowedIncomingMessageSize(), 64 MiB each by default. A frame is
    received completely before it is reported.

    \differences
    No extensions are negotiated, so there is no \c permessage-deflate
    compression. There is no proxy support, no \c readBufferSize(), no
    \c pauseMode(), no pre-shared keys and no \c continueInterruptedHandshake().
    A client has no timeout for the opening handshake. Only version 13 of
    the protocol is spoken. Host names are resolved synchronously, as by
    QAbstractSocket::connectToHost().
    \enddifferences

    \sa QWebSocketServer, {Using Nt WebSockets}
*/

namespace {

using WKH = QHttpHeaders::WellKnownHeader;
using CloseCode = QWebSocketProtocol::CloseCode;

constexpr quint64 DefaultMaxSize = 64ULL * 1024 * 1024;
constexpr quint64 DefaultOutgoingFrameSize = 512ULL * 1024;
constexpr quint64 MaxOutgoingFrameSize = std::numeric_limits<qint64>::max();

bool isValidCloseCode(int code) {
    return (code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1014) || (code >= 3000 && code <= 4999);
}

// Validates UTF-8 incrementally. complete is the number of bytes that form whole characters.
bool validUtf8(const QByteArray &data, qsizetype *complete) {
    int remaining = 0;
    quint32 cp = 0;
    quint32 min = 0;
    *complete = 0;
    for (qsizetype i = 0; i < data.size(); ++i) {
        const auto b = static_cast<unsigned char>(data.at(i));
        if (remaining == 0) {
            if (b < 0x80) {
                *complete = i + 1;
                continue;
            }
            if (b >= 0xC2 && b <= 0xDF) {
                remaining = 1;
                cp = b & 0x1Fu;
                min = 0x80;
            } else if (b >= 0xE0 && b <= 0xEF) {
                remaining = 2;
                cp = b & 0x0Fu;
                min = 0x800;
            } else if (b >= 0xF0 && b <= 0xF4) {
                remaining = 3;
                cp = b & 0x07u;
                min = 0x10000;
            } else {
                return false;
            }
            continue;
        }
        if ((b & 0xC0) != 0x80)
            return false;
        cp = (cp << 6) | (b & 0x3Fu);
        --remaining;
        // Overlong forms, surrogates and values above U+10FFFF are rejected as early as the bits allow.
        if (remaining == 1 && min == 0x800 && (cp < 0x20 || (cp >= 0x360 && cp <= 0x37F)))
            return false;
        if (remaining == 2 && min == 0x10000 && (cp < 0x10 || cp > 0x10F))
            return false;
        if (remaining == 0) {
            if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                return false;
            *complete = i + 1;
        }
    }
    return true;
}

quint16 defaultPort(const QString &scheme) { return scheme == QLatin1StringView("wss") ? 443 : 80; }

} // namespace

QWebSocketPrivate::QWebSocketPrivate(QWebSocket *qq, const QString &o, QWebSocketProtocol::Version v)
    : q(qq), origin(o), version(v), maxFrameSize(DefaultMaxSize), maxMessageSize(DefaultMaxSize),
      outgoingFrameSize(DefaultOutgoingFrameSize) {}

QWebSocketPrivate::~QWebSocketPrivate() = default;

QWebSocket *QWebSocketPrivate::createServerSide(QAbstractSocket *s, const QNetworkRequest &request,
                                                const QString &origin, const QString &subprotocol,
                                                const QByteArray &pending, QObject *parent) {
    auto *ws = new QWebSocket(origin, QWebSocketProtocol::VersionLatest, parent);
    QWebSocketPrivate *d = ws->d.get();
    d->isServer = true;
    d->request = request;
    d->subprotocol = subprotocol;
    d->m_handshakeDone = true;
    d->m_tcpConnected = true;
    d->m_buffer = pending;
    d->attach(s);
    d->state = QAbstractSocket::ConnectedState;
    // Frames that came with the request are read once the application had a chance to connect.
    QTimer::singleShot(0, ws, [d] { d->readyRead(); });
    return ws;
}

void QWebSocketPrivate::attach(QAbstractSocket *s) {
    socket = s;
    s->setParent(q);
    QObject::connect(s, &QIODevice::readyRead, q, [this] { readyRead(); });
    QObject::connect(s, &QAbstractSocket::disconnected, q, [this] { socketDisconnected(); });
    QObject::connect(s, &QIODevice::bytesWritten, q, [this](qint64 bytes) { q->bytesWritten(bytes); });
    QObject::connect(s, &QAbstractSocket::errorOccurred, q, [this](QAbstractSocket::SocketError e) {
        if (e == QAbstractSocket::RemoteHostClosedError && m_handshakeDone)
            return; // a normal end of the connection, reported by disconnected()
        error = e;
        errorString = socket ? socket->errorString() : QString();
        q->errorOccurred(e);
        if (!m_tcpConnected) {
            // The connection was never made, so there is no disconnected() from the socket.
            setState(QAbstractSocket::UnconnectedState);
        }
    });
#ifndef QT_NO_SSL
    if (auto *ssl = qobject_cast<QSslSocket *>(s)) {
        QObject::connect(ssl, &QSslSocket::sslErrors, q, [this, ssl](const QList<QSslError> &errors) {
            if (!ignoredSslErrors.isEmpty())
                ssl->ignoreSslErrors(ignoredSslErrors);
            q->sslErrors(errors);
        });
        QObject::connect(ssl, &QSslSocket::peerVerifyError, q, [this](const QSslError &e) { q->peerVerifyError(e); });
    }
#endif
}

void QWebSocketPrivate::setState(QAbstractSocket::SocketState s) {
    if (state == s)
        return;
    state = s;
    q->stateChanged(s);
}

void QWebSocketPrivate::fail(QAbstractSocket::SocketError e, const QString &message) {
    error = e;
    errorString = message;
    m_failed = true;
    QPointer<QWebSocket> guard(q);
    q->errorOccurred(e);
    if (!guard)
        return;
    if (socket)
        socket->abort();
    if (guard && state != QAbstractSocket::UnconnectedState && !m_tcpConnected)
        setState(QAbstractSocket::UnconnectedState);
}

void QWebSocketPrivate::open(const QNetworkRequest &req, const QWebSocketHandshakeOptions &opts) {
    if (socket) {
        abort();
        if (socket) {
            socket->disconnect(q);
            socket->deleteLater();
            socket = nullptr;
        }
    }
    request = req;
    options = opts;
    subprotocol.clear();
    error = QAbstractSocket::UnknownSocketError;
    errorString.clear();
    closeCode = QWebSocketProtocol::CloseCodeNormal;
    closeReason.clear();
    isServer = false;
    m_buffer.clear();
    m_offset = 0;
    m_handshakeDone = m_tcpConnected = m_closeSent = m_closeReceived = m_failed = false;
    m_inMessage = false;
    m_textCarry.clear();

    const QUrl url = req.url();
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() ||
        (scheme != QLatin1StringView("ws") && scheme != QLatin1StringView("wss"))) {
        error = QAbstractSocket::UnsupportedSocketOperationError;
        errorString = QStringLiteral("Invalid WebSocket URL: %1").arg(url.toString());
        q->errorOccurred(error);
        return;
    }
    const auto port = static_cast<quint16>(url.port(defaultPort(scheme)));

    QAbstractSocket *s = nullptr;
    if (scheme == QLatin1StringView("wss")) {
#ifndef QT_NO_SSL
        auto *ssl = new QSslSocket;
        if (hasSslConfiguration)
            ssl->setSslConfiguration(sslConfiguration);
        QObject::connect(ssl, &QSslSocket::encrypted, q, [this] {
            m_tcpConnected = true;
            sendHandshake();
        });
        s = ssl;
#else
        error = QAbstractSocket::SslInternalError;
        errorString = QStringLiteral("wss: is not supported: Nt was built without TLS");
        q->errorOccurred(error);
        return;
#endif
    } else {
        s = new QTcpSocket;
        QObject::connect(s, &QAbstractSocket::connected, q, [this] {
            m_tcpConnected = true;
            sendHandshake();
        });
    }
    attach(s);
    setState(QAbstractSocket::ConnectingState);
#ifndef QT_NO_SSL
    if (auto *ssl = qobject_cast<QSslSocket *>(s)) {
        ssl->connectToHostEncrypted(url.host(), port);
        return;
    }
#endif
    s->connectToHost(url.host(), port);
}

void QWebSocketPrivate::sendHandshake() {
    if (!socket)
        return;
    const QUrl url = request.url();
    QByteArray target = url.path(QUrl::FullyEncoded).toUtf8();
    if (target.isEmpty())
        target = "/";
    if (url.hasQuery())
        target += "?" + url.query(QUrl::FullyEncoded).toUtf8();
    QByteArray host = url.host(QUrl::FullyEncoded).toUtf8();
    if (host.contains(':'))
        host = "[" + host + "]";
    const QString scheme = url.scheme().toLower();
    if (url.port() >= 0 && url.port() != defaultPort(scheme))
        host += ":" + QByteArray::number(url.port());

    m_key = QWebSocketHandshake::newKey();
    QHttpHeaders h;
    h.append(WKH::Host, host);
    h.append(WKH::Upgrade, "websocket");
    h.append(WKH::Connection, "Upgrade");
    h.append(WKH::SecWebSocketKey, m_key);
    h.append(WKH::SecWebSocketVersion, "13");
    if (!origin.isEmpty())
        h.append(WKH::Origin, origin.toUtf8());
    if (!options.subprotocols().isEmpty())
        h.append(WKH::SecWebSocketProtocol, options.subprotocols().join(QStringLiteral(", ")).toUtf8());
    if (!url.userName().isEmpty() && !request.hasRawHeader("authorization")) {
        const QByteArray credentials = (url.userName() + QLatin1Char(':') + url.password()).toUtf8();
        h.append(WKH::Authorization, "Basic " + credentials.toBase64());
    }
    for (const QByteArray &name : request.rawHeaderList()) {
        if (!h.contains(name))
            h.append(name, request.rawHeader(name)); // invalid names and values are dropped
    }

    QByteArray out = "GET " + target + " HTTP/1.1\r\n";
    for (qsizetype i = 0; i < h.size(); ++i) {
        const QLatin1StringView name = h.nameAt(i);
        const QByteArrayView value = h.valueAt(i);
        out.append(name.data(), name.size());
        out += ": ";
        out.append(value.data(), value.size());
        out += "\r\n";
    }
    out += "\r\n";
    socket->write(out);
}

bool QWebSocketPrivate::processHandshake() {
    const qsizetype end = QWebSocketHandshake::findHeadEnd(m_buffer, m_offset);
    if (end == 0)
        return false;
    if (end < 0) {
        fail(QAbstractSocket::ConnectionRefusedError, QStringLiteral("The server's handshake response is too large"));
        return false;
    }
    QWebSocketHandshake::Head head;
    const bool parsed = QWebSocketHandshake::parseHead(m_buffer.mid(m_offset, end - m_offset), true, &head);
    m_offset = end;
    if (!parsed || !head.first.startsWith("HTTP/1.")) {
        fail(QAbstractSocket::ConnectionRefusedError, QStringLiteral("Invalid handshake response from the server"));
        return false;
    }
    if (head.second != "101") {
        fail(QAbstractSocket::ConnectionRefusedError,
             QStringLiteral("The server refused the WebSocket upgrade: %1 %2")
                 .arg(QString::fromLatin1(head.second), QString::fromLatin1(head.third)));
        return false;
    }
    const QHttpHeaders &h = head.headers;
    QString problem;
    if (!QWebSocketHandshake::tokens(h, WKH::Upgrade).contains("websocket") ||
        !QWebSocketHandshake::tokens(h, WKH::Connection).contains("upgrade"))
        problem = QStringLiteral("the response does not upgrade to WebSocket");
    else if (h.value(WKH::SecWebSocketAccept) != QWebSocketHandshake::acceptKey(m_key))
        problem = QStringLiteral("Sec-WebSocket-Accept does not match the key");
    else if (h.contains(WKH::SecWebSocketExtensions))
        problem = QStringLiteral("the server chose an extension that was not offered");
    if (problem.isEmpty() && h.contains(WKH::SecWebSocketProtocol)) {
        const QString chosen = QString::fromLatin1(h.value(WKH::SecWebSocketProtocol).toByteArray());
        if (!options.subprotocols().contains(chosen))
            problem = QStringLiteral("the server chose a subprotocol that was not offered");
        else
            subprotocol = chosen;
    }
    if (!problem.isEmpty()) {
        fail(QAbstractSocket::ConnectionRefusedError, QStringLiteral("WebSocket handshake failed: ") + problem);
        return false;
    }
    m_handshakeDone = true;
    QPointer<QWebSocket> guard(q);
    setState(QAbstractSocket::ConnectedState);
    if (guard)
        q->connected();
    return guard && state == QAbstractSocket::ConnectedState;
}

void QWebSocketPrivate::readyRead() {
    if (!socket)
        return;
    const qint64 available = socket->bytesAvailable();
    if (available > 0) {
        if (m_offset > 0 && m_offset == m_buffer.size()) {
            m_buffer.clear();
            m_offset = 0;
        }
        m_buffer += socket->read(available);
    }
    if (m_processing || m_failed)
        return;
    m_processing = true;
    QPointer<QWebSocket> guard(q);
    if (!m_handshakeDone) {
        if (isServer || !processHandshake()) {
            if (guard)
                m_processing = false;
            return;
        }
    }
    processFrames();
    if (guard)
        m_processing = false;
}

void QWebSocketPrivate::processFrames() {
    QPointer<QWebSocket> guard(q);
    while (guard && !m_failed && !m_closeReceived) {
        const qsizetype have = m_buffer.size() - m_offset;
        if (have < 2)
            break;
        const auto *p = reinterpret_cast<const unsigned char *>(m_buffer.constData() + m_offset);
        const bool fin = (p[0] & 0x80) != 0;
        const quint8 rsv = p[0] & 0x70;
        const quint8 opcode = p[0] & 0x0F;
        const bool masked = (p[1] & 0x80) != 0;
        quint64 length = p[1] & 0x7F;
        qsizetype header = 2;
        if (length == 126) {
            if (have < 4)
                break;
            length = (quint64(p[2]) << 8) | p[3];
            header = 4;
        } else if (length == 127) {
            if (have < 10)
                break;
            length = 0;
            for (int i = 0; i < 8; ++i)
                length = (length << 8) | p[2 + i];
            header = 10;
            if (length >> 63) {
                failConnection(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Invalid frame length"));
                return;
            }
        }

        if (rsv != 0) {
            failConnection(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Reserved bits are set"));
            return;
        }
        const bool control = opcode >= 0x8;
        if (opcode != Continuation && opcode != Text && opcode != Binary && opcode != Close && opcode != Ping &&
            opcode != Pong) {
            failConnection(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Reserved opcode"));
            return;
        }
        if (masked != isServer) {
            failConnection(QWebSocketProtocol::CloseCodeProtocolError,
                           isServer ? QStringLiteral("A client frame is not masked")
                                    : QStringLiteral("A server frame is masked"));
            return;
        }
        if (control && (!fin || length > 125)) {
            failConnection(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Invalid control frame"));
            return;
        }
        if (!control) {
            if (opcode == Continuation && !m_inMessage) {
                failConnection(QWebSocketProtocol::CloseCodeProtocolError,
                               QStringLiteral("A continuation frame without a message"));
                return;
            }
            if (opcode != Continuation && m_inMessage) {
                failConnection(QWebSocketProtocol::CloseCodeProtocolError,
                               QStringLiteral("A new message before the previous one ended"));
                return;
            }
            if (length > maxFrameSize ||
                length > maxMessageSize - qMin(maxMessageSize, (opcode == Continuation ? m_messageSize : 0))) {
                failConnection(QWebSocketProtocol::CloseCodeTooMuchData, QStringLiteral("The message is too large"));
                return;
            }
        }
        if (masked)
            header += 4;
        if (quint64(have) < quint64(header) + length)
            break; // wait for the rest of the frame

        QByteArray payload = m_buffer.mid(m_offset + header, static_cast<qsizetype>(length));
        if (masked) {
            const unsigned char *key = p + header - 4;
            char *data = payload.data();
            for (qsizetype i = 0; i < payload.size(); ++i)
                data[i] = static_cast<char>(data[i] ^ static_cast<char>(key[i % 4]));
        }
        m_offset += header + static_cast<qsizetype>(length);
        if (m_offset == m_buffer.size()) {
            m_buffer.clear();
            m_offset = 0;
        } else if (m_offset > 1024 * 1024) {
            m_buffer.remove(0, m_offset);
            m_offset = 0;
        }
        if (!processFrame(fin, opcode, payload))
            return;
    }
}

bool QWebSocketPrivate::processFrame(bool fin, quint8 opcode, const QByteArray &payload) {
    QPointer<QWebSocket> guard(q);
    switch (opcode) {
    case Ping:
        if (!m_closeSent)
            writeFrame(Pong, payload.constData(), payload.size(), true);
        return true;
    case Pong:
        q->pong(m_pingTimer.isValid() ? static_cast<quint64>(m_pingTimer.elapsed()) : 0, payload);
        return bool(guard);
    case Close:
        handleClose(payload);
        return false;
    default:
        break;
    }

    if (opcode != Continuation) {
        m_inMessage = true;
        m_messageOpcode = opcode;
        m_messageSize = 0;
        m_binaryMessage.clear();
        m_textMessage.clear();
        m_textCarry.clear();
    }
    m_messageSize += quint64(payload.size());
    if (m_messageOpcode == Text)
        return handleText(payload, fin);

    q->binaryFrameReceived(payload, fin);
    if (!guard)
        return false;
    m_binaryMessage += payload;
    if (fin) {
        m_inMessage = false;
        const QByteArray message = std::move(m_binaryMessage);
        m_binaryMessage = QByteArray();
        q->binaryMessageReceived(message);
    }
    return bool(guard) && !m_failed;
}

bool QWebSocketPrivate::handleText(const QByteArray &payload, bool fin) {
    QPointer<QWebSocket> guard(q);
    const QByteArray data = m_textCarry + payload;
    qsizetype complete = 0;
    if (!validUtf8(data, &complete) || (fin && complete != data.size())) {
        failConnection(QWebSocketProtocol::CloseCodeWrongDatatype, QStringLiteral("Invalid UTF-8 in a text message"));
        return false;
    }
    m_textCarry = data.mid(complete);
    const QString frame = QString::fromUtf8(data.constData(), complete);
    q->textFrameReceived(frame, fin);
    if (!guard)
        return false;
    m_textMessage += frame;
    if (fin) {
        m_inMessage = false;
        const QString message = std::move(m_textMessage);
        m_textMessage = QString();
        q->textMessageReceived(message);
    }
    return bool(guard) && !m_failed;
}

void QWebSocketPrivate::handleClose(const QByteArray &payload) {
    CloseCode code = QWebSocketProtocol::CloseCodeMissingStatusCode;
    QString reason;
    if (payload.size() == 1) {
        failConnection(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Invalid close frame"));
        return;
    }
    if (payload.size() >= 2) {
        const int raw = (static_cast<unsigned char>(payload.at(0)) << 8) | static_cast<unsigned char>(payload.at(1));
        if (!isValidCloseCode(raw)) {
            failConnection(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Invalid close code"));
            return;
        }
        const QByteArray text = payload.mid(2);
        qsizetype complete = 0;
        if (!validUtf8(text, &complete) || complete != text.size()) {
            failConnection(QWebSocketProtocol::CloseCodeWrongDatatype,
                           QStringLiteral("Invalid UTF-8 in a close reason"));
            return;
        }
        // Application codes (3000-4999) have no enumerator; they are passed through, as in Qt.
        code = static_cast<CloseCode>(raw); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
        reason = QString::fromUtf8(text);
    }
    m_closeReceived = true;
    QPointer<QWebSocket> guard(q);
    if (!m_closeSent) {
        closeCode = code;
        closeReason = reason;
        setState(QAbstractSocket::ClosingState);
        if (!guard)
            return;
        q->aboutToClose();
        if (!guard)
            return;
        // Echo the code, as RFC 6455 section 5.5.1 asks.
        if (code == QWebSocketProtocol::CloseCodeMissingStatusCode)
            writeFrame(Close, nullptr, 0, true);
        else
            sendClose(code, QString());
        m_closeSent = true;
    }
    closeTcp();
}

void QWebSocketPrivate::failConnection(CloseCode code, const QString &reason) {
    m_failed = true;
    if (!m_closeSent) {
        closeCode = code;
        closeReason = reason;
        sendClose(code, reason);
        m_closeSent = true;
    }
    QPointer<QWebSocket> guard(q);
    setState(QAbstractSocket::ClosingState);
    if (!guard)
        return;
    if (socket)
        socket->disconnectFromHost();
}

void QWebSocketPrivate::closeTcp() {
    if (!socket)
        return;
    if (isServer) {
        socket->disconnectFromHost();
        return;
    }
    // The client waits for the server to close the TCP connection (RFC 6455, section 7.1.1).
    if (!m_closeTimer) {
        m_closeTimer = new QTimer(q);
        m_closeTimer->setSingleShot(true);
        QObject::connect(m_closeTimer, &QTimer::timeout, q, [this] {
            if (socket)
                socket->abort();
        });
    }
    m_closeTimer->start(CloseTimeout);
}

quint32 QWebSocketPrivate::nextMask() {
    if (maskGenerator)
        return const_cast<QMaskGenerator *>(maskGenerator)->nextMask();
    return QRandomGenerator::system()->generate();
}

void QWebSocketPrivate::writeFrame(quint8 opcode, const char *data, qsizetype size, bool fin) {
    if (!socket || !m_tcpConnected)
        return;
    QByteArray frame;
    frame.reserve(size + 14);
    frame += static_cast<char>((fin ? 0x80 : 0x00) | opcode);
    const char maskBit = isServer ? 0x00 : static_cast<char>(0x80);
    const auto len = static_cast<quint64>(size);
    if (len < 126) {
        frame += static_cast<char>(maskBit | static_cast<char>(len));
    } else if (len <= 0xFFFF) {
        frame += static_cast<char>(maskBit | 126);
        frame += static_cast<char>((len >> 8) & 0xFF);
        frame += static_cast<char>(len & 0xFF);
    } else {
        frame += static_cast<char>(maskBit | 127);
        for (int i = 7; i >= 0; --i)
            frame += static_cast<char>((len >> (8 * i)) & 0xFF);
    }
    if (isServer) {
        frame.append(data, size);
    } else {
        const quint32 mask = nextMask();
        const char key[4] = {static_cast<char>(mask >> 24), static_cast<char>(mask >> 16), static_cast<char>(mask >> 8),
                             static_cast<char>(mask)};
        frame.append(key, 4);
        const qsizetype start = frame.size();
        frame.append(data, size);
        char *out = frame.data() + start;
        for (qsizetype i = 0; i < size; ++i)
            out[i] = static_cast<char>(out[i] ^ key[i % 4]);
    }
    socket->write(frame);
}

void QWebSocketPrivate::sendClose(CloseCode code, const QString &reason) {
    QByteArray payload;
    payload += static_cast<char>((static_cast<int>(code) >> 8) & 0xFF);
    payload += static_cast<char>(static_cast<int>(code) & 0xFF);
    QByteArray text = reason.toUtf8();
    if (text.size() > 123) {
        // Control frames carry at most 125 bytes; cut at a character boundary.
        qsizetype cut = 123;
        while (cut > 0 && (static_cast<unsigned char>(text.at(cut)) & 0xC0) == 0x80)
            --cut;
        text.truncate(cut);
    }
    payload += text;
    writeFrame(Close, payload.constData(), payload.size(), true);
}

void QWebSocketPrivate::close(CloseCode code, const QString &reason) {
    if (!socket || state == QAbstractSocket::UnconnectedState)
        return;
    if (!m_handshakeDone) {
        abort();
        return;
    }
    if (m_closeSent)
        return;
    QPointer<QWebSocket> guard(q);
    closeCode = code;
    closeReason = reason;
    setState(QAbstractSocket::ClosingState);
    if (!guard)
        return;
    q->aboutToClose();
    if (!guard)
        return;
    sendClose(code, reason);
    m_closeSent = true;
    if (m_closeReceived) {
        closeTcp();
        return;
    }
    // Wait for the peer's close frame, but not for ever.
    if (!m_closeTimer) {
        m_closeTimer = new QTimer(q);
        m_closeTimer->setSingleShot(true);
        QObject::connect(m_closeTimer, &QTimer::timeout, q, [this] {
            if (socket)
                socket->abort();
        });
    }
    m_closeTimer->start(CloseTimeout);
}

void QWebSocketPrivate::abort() {
    if (!socket)
        return;
    const bool wasConnected = m_tcpConnected;
    socket->abort();
    if (!wasConnected)
        setState(QAbstractSocket::UnconnectedState);
}

void QWebSocketPrivate::socketDisconnected() {
    if (m_closeTimer)
        m_closeTimer->stop();
    if (!m_closeReceived && !m_closeSent) {
        closeCode = QWebSocketProtocol::CloseCodeAbnormalDisconnection;
        closeReason = QStringLiteral("The connection closed without a closing handshake");
    }
    const bool hadConnection = m_tcpConnected;
    m_tcpConnected = false;
    m_failed = true;
    QPointer<QWebSocket> guard(q);
    setState(QAbstractSocket::UnconnectedState);
    if (!guard || !hadConnection)
        return;
    q->readChannelFinished();
    if (guard)
        q->disconnected();
}

qint64 QWebSocketPrivate::sendMessage(OpCode opcode, const QByteArray &data) {
    if (state != QAbstractSocket::ConnectedState || m_closeSent || !socket)
        return 0;
    const qsizetype size = data.size();
    const auto frameSize =
        static_cast<qsizetype>(qMin<quint64>(outgoingFrameSize, quint64(std::numeric_limits<qsizetype>::max())));
    qsizetype sent = 0;
    bool first = true;
    do {
        const qsizetype chunk = qMin(frameSize, size - sent);
        const bool fin = sent + chunk == size;
        writeFrame(first ? opcode : Continuation, data.constData() + sent, chunk, fin);
        sent += chunk;
        first = false;
    } while (sent < size);
    return size;
}

void QWebSocketPrivate::ping(const QByteArray &payload) {
    if (state != QAbstractSocket::ConnectedState || m_closeSent)
        return;
    const QByteArray data = payload.left(125);
    m_pingTimer.start();
    writeFrame(Ping, data.constData(), data.size(), true);
}

/*!
    Constructs an unconnected WebSocket with the given \a parent. A client
    sends \a origin in the \c Origin header of the opening handshake, if it
    is not empty. \a version must be QWebSocketProtocol::VersionLatest or
    QWebSocketProtocol::Version13, the only version spoken.

    \sa open()
*/
QWebSocket::QWebSocket(const QString &origin, QWebSocketProtocol::Version version, QObject *parent)
    : QObject(parent), d(std::make_unique<QWebSocketPrivate>(this, origin, version)) {
    if (version != QWebSocketProtocol::Version13)
        qWarning("QWebSocket: only version 13 of the WebSocket protocol is supported");
}

/*!
    Destroys the socket. An open connection is aborted, without a closing
    handshake; call close() first to end it cleanly.
*/
QWebSocket::~QWebSocket() {
    if (d->socket) {
        d->socket->disconnect(this);
        d->socket->abort();
    }
}

/*!
    Aborts the connection at once, without a closing handshake, and discards
    data not written yet.

    \sa close()
*/
void QWebSocket::abort() { d->abort(); }

/*!
    Returns the type of the last error.

    \sa errorString(), errorOccurred()
*/
QAbstractSocket::SocketError QWebSocket::error() const { return d->error; }

/*!
    Returns a description of the last error.

    \sa error()
*/
QString QWebSocket::errorString() const { return d->errorString; }

/*!
    Writes as much buffered data to the network as possible without
    blocking. Returns \c true if any data was written.
*/
bool QWebSocket::flush() { return d->socket && d->socket->flush(); }

/*!
    Returns \c true if the socket is connected and messages can be sent;
    otherwise returns \c false.

    \sa state()
*/
bool QWebSocket::isValid() const { return d->socket && d->state == QAbstractSocket::ConnectedState; }

/*!
    Returns the local address of the connection, or a null address if there is none.
*/
QHostAddress QWebSocket::localAddress() const { return d->socket ? d->socket->localAddress() : QHostAddress(); }

/*!
    Returns the local port of the connection, or 0 if there is none.
*/
quint16 QWebSocket::localPort() const { return d->socket ? d->socket->localPort() : 0; }

/*!
    Returns the address of the peer, or a null address if there is none.
*/
QHostAddress QWebSocket::peerAddress() const { return d->socket ? d->socket->peerAddress() : QHostAddress(); }

/*!
    Returns the name of the peer, as given to open() for a client.
*/
QString QWebSocket::peerName() const { return d->socket ? d->socket->peerName() : QString(); }

/*!
    Returns the port of the peer, or 0 if there is none.
*/
quint16 QWebSocket::peerPort() const { return d->socket ? d->socket->peerPort() : 0; }

/*!
    Returns the state of the socket: \c UnconnectedState,
    \c ConnectingState while the TCP connection and the opening handshake
    are made, \c ConnectedState, or \c ClosingState during the closing
    handshake.

    \sa stateChanged()
*/
QAbstractSocket::SocketState QWebSocket::state() const { return d->state; }

/*!
    Returns the version of the WebSocket protocol in use.
*/
QWebSocketProtocol::Version QWebSocket::version() const { return d->version; }

/*!
    Returns the path and query of the requested URL, such as \c{/chat?room=1}.

    \sa requestUrl()
*/
QString QWebSocket::resourceName() const {
    const QUrl url = d->request.url();
    QString name = url.path(QUrl::FullyEncoded);
    if (name.isEmpty())
        name = QStringLiteral("/");
    if (url.hasQuery())
        name += QLatin1Char('?') + url.query(QUrl::FullyEncoded);
    return name;
}

/*!
    Returns the URL the client opened, or that a client requested from the server.

    \sa request()
*/
QUrl QWebSocket::requestUrl() const { return d->request.url(); }

/*!
    Returns the request of the opening handshake. For a server-side socket it
    holds the header fields the client sent.

    \sa requestUrl()
*/
QNetworkRequest QWebSocket::request() const { return d->request; }

/*!
    Returns the options given to open().
*/
QWebSocketHandshakeOptions QWebSocket::handshakeOptions() const { return d->options; }

/*!
    Returns the origin: the one given to the constructor for a client, or the
    \c Origin header the client sent for a server-side socket.
*/
QString QWebSocket::origin() const { return d->origin; }

/*!
    Returns the subprotocol chosen in the opening handshake, or an empty
    string if there is none.

    \sa QWebSocketHandshakeOptions::setSubprotocols(), QWebSocketServer::setSupportedSubprotocols()
*/
QString QWebSocket::subprotocol() const { return d->subprotocol; }

/*!
    Returns the code of the closing handshake: the one sent by close(), or
    the one received from the peer.
    QWebSocketProtocol::CloseCodeAbnormalDisconnection means the connection
    ended without a closing handshake.

    \sa closeReason()
*/
QWebSocketProtocol::CloseCode QWebSocket::closeCode() const { return d->closeCode; }

/*!
    Returns the reason of the closing handshake.

    \sa closeCode()
*/
QString QWebSocket::closeReason() const { return d->closeReason; }

/*!
    Returns the number of bytes waiting to be written to the network.
*/
qint64 QWebSocket::bytesToWrite() const { return d->socket ? d->socket->bytesToWrite() : 0; }

/*!
    Sets the generator of masking keys to \a maskGenerator, or back to the
    default, which uses the system's random number generator, if it is
    \c nullptr. The generator is not owned by the socket and must outlive it.
    Only clients mask their frames.
*/
void QWebSocket::setMaskGenerator(const QMaskGenerator *maskGenerator) {
    if (maskGenerator && !const_cast<QMaskGenerator *>(maskGenerator)->seed()) {
        qWarning("QWebSocket::setMaskGenerator: the generator could not be seeded");
        return;
    }
    d->maskGenerator = maskGenerator;
}

/*!
    Returns the mask generator set with setMaskGenerator(), or \c nullptr
    for the default one.
*/
const QMaskGenerator *QWebSocket::maskGenerator() const { return d->maskGenerator; }

/*!
    Sends \a message as a text message, encoded in UTF-8, and returns the
    number of bytes sent, or 0 if the socket is not connected. Messages
    longer than outgoingFrameSize() are sent in several frames.

    \sa sendBinaryMessage(), textMessageReceived()
*/
qint64 QWebSocket::sendTextMessage(const QString &message) {
    return d->sendMessage(QWebSocketPrivate::Text, message.toUtf8());
}

/*!
    Sends \a data as a binary message and returns the number of bytes sent,
    or 0 if the socket is not connected.

    \sa sendTextMessage(), binaryMessageReceived()
*/
qint64 QWebSocket::sendBinaryMessage(const QByteArray &data) { return d->sendMessage(QWebSocketPrivate::Binary, data); }

#ifndef QT_NO_SSL
/*!
    Ignores the TLS \a errors when they occur while connecting. Call it
    before open(), or from a slot connected to sslErrors().

    \sa sslErrors()
*/
void QWebSocket::ignoreSslErrors(const QList<QSslError> &errors) {
    d->ignoredSslErrors = errors;
    if (auto *ssl = qobject_cast<QSslSocket *>(d->socket.data()))
        ssl->ignoreSslErrors(errors);
}

/*!
    \overload

    Ignores the TLS errors of the current handshake. Call it from a slot
    connected to sslErrors().
*/
void QWebSocket::ignoreSslErrors() {
    if (auto *ssl = qobject_cast<QSslSocket *>(d->socket.data()))
        ssl->ignoreSslErrors();
}

/*!
    Sets the TLS configuration of \c wss: connections to \a sslConfiguration.
    It takes effect on the next open().

    \sa sslConfiguration()
*/
void QWebSocket::setSslConfiguration(const QSslConfiguration &sslConfiguration) {
    d->sslConfiguration = sslConfiguration;
    d->hasSslConfiguration = true;
}

/*!
    Returns the TLS configuration of the connection, or the one set with
    setSslConfiguration() before it is open.
*/
QSslConfiguration QWebSocket::sslConfiguration() const {
    if (auto *ssl = qobject_cast<QSslSocket *>(d->socket.data()))
        return ssl->sslConfiguration();
    return d->sslConfiguration;
}
#endif

/*!
    Sets the largest incoming frame to \a maxAllowedIncomingFrameSize bytes,
    at most maxIncomingFrameSize(). A larger frame closes the connection with
    QWebSocketProtocol::CloseCodeTooMuchData.
*/
void QWebSocket::setMaxAllowedIncomingFrameSize(quint64 maxAllowedIncomingFrameSize) {
    d->maxFrameSize = qMin(maxAllowedIncomingFrameSize, maxIncomingFrameSize());
}

/*!
    Returns the largest incoming frame allowed, in bytes. The default is 64 MiB.
*/
quint64 QWebSocket::maxAllowedIncomingFrameSize() const { return d->maxFrameSize; }

/*!
    Sets the largest incoming message to \a maxAllowedIncomingMessageSize
    bytes, at most maxIncomingMessageSize(). A larger message closes the
    connection with QWebSocketProtocol::CloseCodeTooMuchData.
*/
void QWebSocket::setMaxAllowedIncomingMessageSize(quint64 maxAllowedIncomingMessageSize) {
    d->maxMessageSize = qMin(maxAllowedIncomingMessageSize, maxIncomingMessageSize());
}

/*!
    Returns the largest incoming message allowed, in bytes. The default is 64 MiB.
*/
quint64 QWebSocket::maxAllowedIncomingMessageSize() const { return d->maxMessageSize; }

/*!
    Returns the largest value setMaxAllowedIncomingMessageSize() accepts: 1 GiB.
*/
quint64 QWebSocket::maxIncomingMessageSize() { return 1024ULL * 1024 * 1024; }

/*!
    Returns the largest value setMaxAllowedIncomingFrameSize() accepts: 1 GiB.
*/
quint64 QWebSocket::maxIncomingFrameSize() { return 1024ULL * 1024 * 1024; }

/*!
    Sets the size of the frames that outgoing messages are split into to
    \a outgoingFrameSize bytes. The default is 512 KiB.
*/
void QWebSocket::setOutgoingFrameSize(quint64 outgoingFrameSize) {
    d->outgoingFrameSize = qBound<quint64>(1, outgoingFrameSize, maxOutgoingFrameSize());
}

/*!
    Returns the size of the frames that outgoing messages are split into.
*/
quint64 QWebSocket::outgoingFrameSize() const { return d->outgoingFrameSize; }

/*!
    Returns the largest value setOutgoingFrameSize() accepts.
*/
quint64 QWebSocket::maxOutgoingFrameSize() { return MaxOutgoingFrameSize; }

/*!
    Starts the closing handshake with \a closeCode and \a reason. The reason
    is cut to 123 bytes of UTF-8. disconnected() is emitted when the peer has
    answered and the connection has closed, or after five seconds without an
    answer. A socket still in the opening handshake is aborted.

    \sa abort(), closeCode()
*/
void QWebSocket::close(QWebSocketProtocol::CloseCode closeCode, const QString &reason) { d->close(closeCode, reason); }

/*!
    Opens a connection to \a url, which must have the scheme \c ws or
    \c wss. connected() is emitted when the opening handshake has succeeded,
    and errorOccurred() if it fails.

    \sa close(), connected()
*/
void QWebSocket::open(const QUrl &url) { d->open(QNetworkRequest(url), QWebSocketHandshakeOptions()); }

/*!
    \overload

    Opens a connection to the URL of \a request, sending its raw headers in
    the opening handshake.
*/
void QWebSocket::open(const QNetworkRequest &request) { d->open(request, QWebSocketHandshakeOptions()); }

/*!
    \overload

    Opens a connection to \a url, asking for the subprotocols in \a options.
*/
void QWebSocket::open(const QUrl &url, const QWebSocketHandshakeOptions &options) {
    d->open(QNetworkRequest(url), options);
}

/*!
    \overload

    Opens a connection to the URL of \a request, with its raw headers and the
    subprotocols in \a options.
*/
void QWebSocket::open(const QNetworkRequest &request, const QWebSocketHandshakeOptions &options) {
    d->open(request, options);
}

/*!
    Sends a ping with \a payload, cut to 125 bytes. The peer answers with a
    pong, reported by pong() with the round-trip time.
*/
void QWebSocket::ping(const QByteArray &payload) { d->ping(payload); }

#include "moc_qwebsocket.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
