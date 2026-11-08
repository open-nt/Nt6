// MIT License - see LICENSE
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtHttpServer/qhttpserverresponse.h>

/*!
    \class QHttpServerResponse
    \brief The QHttpServerResponse class holds a complete response: a status code, header fields and a body.
    \since 6.4

    A route handler of QHttpServer usually returns a QHttpServerResponse,
    or a value that converts to one: a string, a JSON object or array, or a
    status code. The server adds the \c Content-Length header and the
    framing when it sends the response.

    \snippet httpserver/hello_server.cpp responses

    Each constructor sets the \c Content-Type header from the kind of data:

    \table
    \header
        \li Data
        \li Content type
    \row
        \li QString, \c{const char *}
        \li \c text/plain
    \row
        \li QJsonObject, QJsonArray
        \li \c application/json, written compactly
    \row
        \li QByteArray
        \li guessed from the first bytes (see below)
    \row
        \li StatusCode only
        \li \c application/x-empty
    \endtable

    For a QByteArray, the type is \c text/html if the data starts with an
    HTML document type or tag, \c image/png, \c image/jpeg, \c image/gif or
    \c application/pdf for those signatures, \c text/plain if it has no
    control characters other than white space, and
    \c application/octet-stream otherwise. Pass the type explicitly to avoid
    the guess.

    \differences
    The content type of a QByteArray and of fromFile() is guessed from a
    small built-in table, not from a MIME database. There is no
    \c QHttpServerResponse(QHttpServerResponse::StatusCode) for a
    \c QFuture, since Nt has no \c QFuture.
    \enddifferences

    \sa QHttpServerResponder::sendResponse(), QHttpServer::addAfterRequestHandler()
*/

namespace {

using WKH = QHttpHeaders::WellKnownHeader;

QByteArray sniffMimeType(const QByteArray &data) {
    if (data.isEmpty())
        return "application/x-empty";
    const QByteArray start = data.left(64).trimmed().toLower();
    if (start.startsWith("<!doctype html") || start.startsWith("<html"))
        return "text/html";
    if (data.startsWith("\x89PNG\r\n\x1a\n"))
        return "image/png";
    if (data.startsWith("\xff\xd8\xff"))
        return "image/jpeg";
    if (data.startsWith("GIF87a") || data.startsWith("GIF89a"))
        return "image/gif";
    if (data.startsWith("%PDF-"))
        return "application/pdf";
    for (char c : data.left(1024)) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != '\f')
            return "application/octet-stream";
    }
    return "text/plain";
}

QByteArray mimeTypeForFile(const QString &fileName, const QByteArray &data) {
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    struct Entry {
        const char *suffix;
        const char *type;
    };
    static const Entry table[] = {
        {"html", "text/html"},      {"htm", "text/html"},
        {"css", "text/css"},        {"js", "text/javascript"},
        {"mjs", "text/javascript"}, {"json", "application/json"},
        {"txt", "text/plain"},      {"csv", "text/csv"},
        {"xml", "application/xml"}, {"svg", "image/svg+xml"},
        {"png", "image/png"},       {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},     {"gif", "image/gif"},
        {"webp", "image/webp"},     {"ico", "image/vnd.microsoft.icon"},
        {"pdf", "application/pdf"}, {"wasm", "application/wasm"},
        {"zip", "application/zip"}, {"gz", "application/gzip"},
        {"md", "text/markdown"},    {"woff", "font/woff"},
        {"woff2", "font/woff2"},    {"mp4", "video/mp4"},
        {"mp3", "audio/mpeg"},      {"wav", "audio/wav"},
    };
    for (const Entry &e : table) {
        if (suffix == QLatin1StringView(e.suffix))
            return e.type;
    }
    return sniffMimeType(data);
}

QHttpHeaders contentType(const QByteArray &mimeType) {
    QHttpHeaders h;
    if (!mimeType.isEmpty())
        h.append(WKH::ContentType, mimeType);
    return h;
}

} // namespace

/*!
    \typedef QHttpServerResponse::StatusCode

    The same enum as QHttpServerResponder::StatusCode.
*/

/*!
    Constructs a response with the status \a statusCode and no body.
*/
QHttpServerResponse::QHttpServerResponse(StatusCode statusCode)
    : m_headers(contentType("application/x-empty")), m_status(statusCode) {}

/*!
    Constructs a \c text/plain response with the status \a status whose body
    is the null-terminated string \a data.
*/
QHttpServerResponse::QHttpServerResponse(const char *data, StatusCode status)
    : m_data(data), m_headers(contentType("text/plain")), m_status(status) {}

/*!
    Constructs a \c text/plain response with the status \a status whose body
    is \a data encoded in UTF-8.
*/
QHttpServerResponse::QHttpServerResponse(const QString &data, StatusCode status)
    : m_data(data.toUtf8()), m_headers(contentType("text/plain")), m_status(status) {}

/*!
    Constructs a response with the status \a status whose body is \a data.
    The content type is guessed from the data.
*/
QHttpServerResponse::QHttpServerResponse(const QByteArray &data, StatusCode status)
    : m_data(data), m_headers(contentType(sniffMimeType(data))), m_status(status) {}

/*!
    \overload
*/
QHttpServerResponse::QHttpServerResponse(QByteArray &&data, StatusCode status)
    : m_data(std::move(data)), m_headers(contentType(sniffMimeType(m_data))), m_status(status) {}

/*!
    Constructs an \c application/json response with the status \a status
    whose body is \a data.
*/
QHttpServerResponse::QHttpServerResponse(const QJsonObject &data, StatusCode status)
    : m_data(QJsonDocument(data).toJson(QJsonDocument::Compact)), m_headers(contentType("application/json")),
      m_status(status) {}

/*!
    \overload
*/
QHttpServerResponse::QHttpServerResponse(const QJsonArray &data, StatusCode status)
    : m_data(QJsonDocument(data).toJson(QJsonDocument::Compact)), m_headers(contentType("application/json")),
      m_status(status) {}

/*!
    Constructs a response with the status \a status whose body is \a data
    and whose content type is \a mimeType.
*/
QHttpServerResponse::QHttpServerResponse(const QByteArray &mimeType, const QByteArray &data, StatusCode status)
    : m_data(data), m_headers(contentType(mimeType)), m_status(status) {}

/*!
    \overload
*/
QHttpServerResponse::QHttpServerResponse(const QByteArray &mimeType, QByteArray &&data, StatusCode status)
    : m_data(std::move(data)), m_headers(contentType(mimeType)), m_status(status) {}

/*!
    Move-constructs a response from \a other.
*/
QHttpServerResponse::QHttpServerResponse(QHttpServerResponse &&other) noexcept = default;

/*!
    Move-assigns \a other to this response and returns a reference to it.
*/
QHttpServerResponse &QHttpServerResponse::operator=(QHttpServerResponse &&other) noexcept = default;

/*!
    Destroys the response.
*/
QHttpServerResponse::~QHttpServerResponse() = default;

/*!
    Returns a response whose body is the contents of the file \a fileName,
    or a \c{404 Not Found} response if the file cannot be read. The content
    type is chosen from the file name's suffix, or guessed from the data.

    The whole file is read into memory. To send a large file, pass a QFile
    to QHttpServerResponder::write() instead, which sends it in pieces.
*/
QHttpServerResponse QHttpServerResponse::fromFile(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return QHttpServerResponse(StatusCode::NotFound);
    QByteArray data = file.readAll();
    const QByteArray type = mimeTypeForFile(fileName, data);
    return QHttpServerResponse(type, std::move(data));
}

/*!
    Returns the body of the response.
*/
QByteArray QHttpServerResponse::data() const { return m_data; }

/*!
    Returns the value of the \c Content-Type header, or an empty QByteArray if there is none.

    \sa headers()
*/
QByteArray QHttpServerResponse::mimeType() const { return m_headers.value(WKH::ContentType).toByteArray(); }

/*!
    Returns the status code of the response.
*/
QHttpServerResponse::StatusCode QHttpServerResponse::statusCode() const { return m_status; }

/*!
    Returns the header fields of the response, including \c Content-Type.

    \sa setHeaders()
*/
QHttpHeaders QHttpServerResponse::headers() const { return m_headers; }

/*!
    Replaces the header fields of the response with \a newHeaders. Include
    \c Content-Type in them to keep a content type.

    \c Content-Length, \c Transfer-Encoding and \c Connection are set by the
    server when it sends the response; those in \a newHeaders are ignored.

    \sa headers()
*/
void QHttpServerResponse::setHeaders(const QHttpHeaders &newHeaders) { m_headers = newHeaders; }

/*!
    \overload
*/
void QHttpServerResponse::setHeaders(QHttpHeaders &&newHeaders) { m_headers = std::move(newHeaders); }

/*!
    Swaps this response with \a other.
*/
void QHttpServerResponse::swap(QHttpServerResponse &other) noexcept {
    m_data.swap(other.m_data);
    m_headers.swap(other.m_headers);
    std::swap(m_status, other.m_status);
}
