// MIT License - see LICENSE
#pragma once

// The opening handshake of RFC 6455, section 4: parsing the HTTP head of a request or response,
// checking an upgrade request, and building the answer. Shared by QWebSocket (client side),
// QWebSocketServer and QAbstractHttpServer.

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtNetwork/qhttpheaders.h>
#include <QtWebSockets/qwebsocketprotocol.h>

namespace QWebSocketHandshake {

// Limits on the size of an HTTP head, in bytes, and on the number of header fields.
constexpr qsizetype MaxHeadSize = 16 * 1024;
constexpr qsizetype MaxHeaderCount = 100;

// The start line split at spaces (three parts) and the header fields.
struct Head {
    QByteArray first;
    QByteArray second;
    QByteArray third;
    QHttpHeaders headers;
};

// Finds the end of an HTTP head in buffer, from offset. Returns the offset just past the empty line,
// 0 if more data is needed, or -1 if the head is too large.
qsizetype findHeadEnd(const QByteArray &buffer, qsizetype offset);

// Parses a head (without the empty line). The reason phrase of a status line may contain spaces.
bool parseHead(const QByteArray &head, bool response, Head *out);

QByteArray acceptKey(QByteArrayView key);
QByteArray newKey();

// The comma-separated tokens of a field, trimmed and in lower case.
QList<QByteArray> tokens(const QHttpHeaders &headers, QHttpHeaders::WellKnownHeader name);

bool isUpgradeRequest(const QHttpHeaders &headers);

struct ServerAnswer {
    bool accepted = false;
    QByteArray response; // the complete HTTP response to write
    QString subprotocol;
    QString errorString;
    QWebSocketProtocol::CloseCode closeCode = QWebSocketProtocol::CloseCodeProtocolError;
};

// Checks the header fields of a GET request for HTTP/1.1 or later and builds the answer: 101 with the
// accept key, or 400 or 426 for a request that is not a valid version 13 upgrade.
ServerAnswer answer(const QHttpHeaders &headers, const QStringList &supportedSubprotocols,
                    const QByteArray &serverName);

QByteArray errorResponse(int status, QByteArrayView reason, QByteArrayView extraHeaders = {});

} // namespace QWebSocketHandshake
