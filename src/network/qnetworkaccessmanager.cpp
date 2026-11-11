// MIT License - see LICENSE
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qhostinfo.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtNetwork/qtcpsocket.h>

#ifndef QT_NO_SSL
#include <QtNetwork/qsslsocket.h>
#endif

#include <cstring>

namespace {

constexpr qint64 kMaxHeaderBytes = 1024 * 1024;

QByteArray headerName(QNetworkRequest::KnownHeaders h) {
    switch (h) {
    case QNetworkRequest::ContentTypeHeader:
        return "Content-Type";
    case QNetworkRequest::ContentLengthHeader:
        return "Content-Length";
    case QNetworkRequest::LocationHeader:
        return "Location";
    case QNetworkRequest::LastModifiedHeader:
        return "Last-Modified";
    case QNetworkRequest::CookieHeader:
        return "Cookie";
    case QNetworkRequest::SetCookieHeader:
        return "Set-Cookie";
    case QNetworkRequest::ContentDispositionHeader:
        return "Content-Disposition";
    case QNetworkRequest::UserAgentHeader:
        return "User-Agent";
    case QNetworkRequest::ServerHeader:
        return "Server";
    case QNetworkRequest::IfModifiedSinceHeader:
        return "If-Modified-Since";
    case QNetworkRequest::ETagHeader:
        return "ETag";
    case QNetworkRequest::IfMatchHeader:
        return "If-Match";
    case QNetworkRequest::IfNoneMatchHeader:
        return "If-None-Match";
    }
    return QByteArray();
}

bool sameName(const QByteArray &a, const QByteArray &b) { return a.toLower() == b.toLower(); }

// A token per RFC 9110, which keeps a name or verb from breaking the request.
bool isToken(const QByteArray &s) {
    if (s.isEmpty())
        return false;
    for (char c : s) {
        const bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (!alnum && !std::strchr("!#$%&'*+-.^_`|~", c))
            return false;
    }
    return !std::memchr(s.constData(), 0, static_cast<std::size_t>(s.size()));
}

bool safeValue(const QByteArray &s) {
    for (char c : s)
        if (c == '\r' || c == '\n' || c == '\0')
            return false;
    return true;
}

// All digits, without overflowing. Rejects signs, spaces and empty input.
bool parseLength(const QByteArray &text, qint64 *out) {
    const QByteArray t = text.trimmed();
    if (t.isEmpty())
        return false;
    qint64 v = 0;
    for (char c : t) {
        if (c < '0' || c > '9' || v > (std::numeric_limits<qint64>::max() - 9) / 10)
            return false;
        v = v * 10 + (c - '0');
    }
    *out = v;
    return true;
}

bool parseChunkSize(const QByteArray &line, qint64 *out) {
    const qsizetype semi = line.indexOf(';');
    const QByteArray t = (semi >= 0 ? line.left(semi) : line).trimmed();
    if (t.isEmpty())
        return false;
    qint64 v = 0;
    for (char c : t) {
        int d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return false;
        if (v > (std::numeric_limits<qint64>::max() >> 4))
            return false;
        v = v << 4 | d;
    }
    *out = v;
    return true;
}

int defaultPort(const QUrl &u) { return u.scheme() == QString("https") ? 443 : 80; }

QString originOf(const QUrl &u) {
    return u.scheme() + QChar(u'|') + u.host() + QChar(u'|') + QString::number(u.port(defaultPort(u)));
}

bool supportedScheme(const QUrl &u) {
#ifndef QT_NO_SSL
    if (u.scheme() == QString("https"))
        return true;
#endif
    return u.scheme() == QString("http");
}

QNetworkReply::NetworkError errorForStatus(int status) {
    switch (status) {
    case 401:
        return QNetworkReply::AuthenticationRequiredError;
    case 403:
        return QNetworkReply::ContentAccessDenied;
    case 404:
        return QNetworkReply::ContentNotFoundError;
    case 405:
        return QNetworkReply::ContentOperationNotPermittedError;
    case 409:
        return QNetworkReply::ContentConflictError;
    case 410:
        return QNetworkReply::ContentGoneError;
    case 500:
        return QNetworkReply::InternalServerError;
    case 501:
        return QNetworkReply::OperationNotImplementedError;
    case 503:
        return QNetworkReply::ServiceUnavailableError;
    default:
        break;
    }
    if (status >= 400 && status < 500)
        return QNetworkReply::UnknownContentError;
    if (status >= 500)
        return QNetworkReply::UnknownServerError;
    return QNetworkReply::NoError;
}

} // namespace

namespace QtPrivate {

// One HTTP/1.1 exchange per connection, plus any redirects it follows.
class HttpReply : public QNetworkReply {
public:
    HttpReply(QNetworkAccessManager *manager, const QNetworkRequest &request, const QByteArray &verb,
              const QByteArray &body, int defaultTimeout)
        : QNetworkReply(manager), m_verb(verb), m_upload(body) {
        m_manager = manager;
        m_request = request;
        m_url = request.url();
        m_timeoutMs = request.transferTimeout() > 0 ? request.transferTimeout() : defaultTimeout;
        QIODevice::open(ReadOnly | Unbuffered);
        QTimer::singleShot(0, this, [this] { begin(); }); // signals never fire inside get()
    }

    void abort() override { finish(OperationCanceledError, QString("Operation canceled")); }

#ifndef QT_NO_SSL
    void ignoreSslErrors() override { m_ignoreAll = true; }
    void ignoreSslErrors(const QList<QSslError> &errors) override { m_ignoreList = errors; }
#endif

    void emitFinished() {
        QPointer<QNetworkReply> guard(this);
        if (m_error != NoError)
            errorOccurred(m_error);
        if (guard)
            readChannelFinished();
        if (guard)
            finished();
        if (guard && m_manager)
            m_manager->finished(this);
    }

private:
    enum Phase { StatusLine, Headers, FixedBody, ChunkSize, ChunkData, ChunkEnd, Trailers, CloseBody, Done };

    void begin() {
        if (m_finished)
            return;
        if (!m_url.isValid() || m_url.host().isEmpty()) {
            finish(ProtocolUnknownError, QString("Invalid URL"));
            return;
        }
        if (!supportedScheme(m_url)) {
            finish(ProtocolUnknownError, QString("Protocol \"") + m_url.scheme() + QString("\" is unknown"));
            return;
        }
        if (!isToken(m_verb)) {
            finish(ProtocolInvalidOperationError, QString("Invalid request method"));
            return;
        }
        for (const QByteArray &name : m_request.rawHeaderList()) {
            if (!isToken(name) || !safeValue(m_request.rawHeader(name))) {
                finish(ProtocolInvalidOperationError, QString("Invalid request header"));
                return;
            }
        }
        armTimeout();
        const QHostAddress literal(m_url.host());
        if (!literal.isNull()) {
            m_addresses = {literal};
            connectNext();
            return;
        }
        m_lookup = QHostInfo::lookupHost(m_url.host(), this, [this](const QHostInfo &info) {
            m_lookup = -1;
            if (m_finished)
                return;
            if (info.error() != QHostInfo::NoError) {
                finish(HostNotFoundError, QString("Host ") + m_url.host() + QString(" not found"));
                return;
            }
            m_addresses = info.addresses();
            connectNext();
        });
    }

    void connectNext() {
        dropSocket();
        if (m_addresses.isEmpty()) {
            finish(m_lastConnectError, m_lastConnectText);
            return;
        }
        const QHostAddress address = m_addresses.takeFirst();
#ifndef QT_NO_SSL
        if (m_url.scheme() == QString("https")) {
            auto *secure = new QSslSocket(this);
            secure->setSslConfiguration(m_request.sslConfiguration());
            secure->setPeerVerifyName(m_url.host());
            secure->ignoreSslErrors(m_ignoreList);
            QObject::connect(secure, &QAbstractSocket::connected, this, [secure] { secure->startClientEncryption(); });
            QObject::connect(secure, &QSslSocket::sslErrors, this,
                             [this, secure](const QList<QSslError> &errors) { onSslErrors(secure, errors); });
            QObject::connect(secure, &QSslSocket::encrypted, this, [this, secure] { onEncrypted(secure); });
            m_socket = secure;
        } else
#endif
        {
            m_socket = new QTcpSocket(this);
            QObject::connect(m_socket, &QAbstractSocket::connected, this, [this] { sendRequest(); });
        }
        QObject::connect(m_socket, &QIODevice::readyRead, this, [this] { onData(); });
        QObject::connect(m_socket, &QIODevice::bytesWritten, this, [this](qint64 n) { onWritten(n); });
        QObject::connect(m_socket, &QAbstractSocket::disconnected, this, [this] { onClosed(); });
        QObject::connect(m_socket, &QAbstractSocket::errorOccurred, this,
                         [this](QAbstractSocket::SocketError e) { onSocketError(e); });
        m_socket->connectToHost(address, static_cast<quint16>(m_url.port(defaultPort(m_url))));
    }

#ifndef QT_NO_SSL
    void onSslErrors(QSslSocket *socket, const QList<QSslError> &errors) {
        QPointer<QNetworkReply> guard(this);
        sslErrors(errors);
        if (guard && m_manager)
            m_manager->sslErrors(this, errors);
        if (guard && m_ignoreAll)
            socket->ignoreSslErrors();
    }

    void onEncrypted(QSslSocket *socket) {
        m_sslConfiguration = socket->sslConfiguration();
        QPointer<QNetworkReply> guard(this);
        encrypted();
        if (guard && m_manager)
            m_manager->encrypted(this);
        if (guard && !m_finished)
            sendRequest();
    }
#endif

    void dropSocket() {
        if (!m_socket)
            return;
        QTcpSocket *s = m_socket;
        m_socket = nullptr;
        s->disconnect(this);
        s->abort();
        s->deleteLater();
    }

    void sendRequest() {
        QByteArray target = m_url.path(QUrl::FullyEncoded).toUtf8();
        if (target.isEmpty())
            target = "/";
        if (m_url.hasQuery())
            target += '?' + m_url.query(QUrl::FullyEncoded).toUtf8();

        QByteArray host = m_url.host(QUrl::FullyEncoded).toUtf8();
        if (m_url.port() >= 0 && m_url.port() != defaultPort(m_url))
            host += ':' + QByteArray::number(m_url.port());

        QByteArray out = m_verb + ' ' + target + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n";
        const auto addDefault = [&](const QByteArray &name, const QByteArray &value) {
            if (!m_request.hasRawHeader(name))
                out += name + ": " + value + "\r\n";
        };
        addDefault("User-Agent", "Nt/6");
        addDefault("Accept", "*/*");
        if (!m_url.userName().isEmpty() && !m_request.hasRawHeader("Authorization")) {
            const QByteArray credentials = (m_url.userName() + QChar(u':') + m_url.password()).toUtf8();
            out += "Authorization: Basic " + credentials.toBase64() + "\r\n";
        }
        if (!m_upload.isEmpty() || m_verb == "POST" || m_verb == "PUT")
            out += "Content-Length: " + QByteArray::number(m_upload.size()) + "\r\n";
        for (const QByteArray &name : m_request.rawHeaderList()) {
            if (!sameName(name, "Content-Length") && !sameName(name, "Host"))
                out += name + ": " + m_request.rawHeader(name) + "\r\n";
        }
        out += "\r\n";
        m_headerBytesLeft = out.size();
        m_uploaded = 0;
        m_socket->write(out + m_upload);
    }

    void onWritten(qint64 n) {
        armTimeout();
        const qint64 forHeaders = qMin(n, m_headerBytesLeft);
        m_headerBytesLeft -= forHeaders;
        const qint64 forBody = n - forHeaders;
        if (forBody > 0) {
            m_uploaded += forBody;
            uploadProgress(m_uploaded, m_upload.size());
        }
    }

    void onSocketError(QAbstractSocket::SocketError e) {
        if (m_finished || m_phase == Done)
            return;
        if (e == QAbstractSocket::RemoteHostClosedError)
            return; // onClosed() decides whether this was premature
        if (m_socket && m_socket->state() != QAbstractSocket::ConnectedState && !m_receivedAny &&
            !m_addresses.isEmpty()) {
            QTimer::singleShot(0, this, [this] { connectNext(); });
            return;
        }
        const QString text = m_socket ? m_socket->errorString() : QString();
        if (e == QAbstractSocket::SslHandshakeFailedError || e == QAbstractSocket::SslInvalidUserDataError ||
            e == QAbstractSocket::SslInternalError) {
            finish(SslHandshakeFailedError, text);
        } else if (e == QAbstractSocket::ConnectionRefusedError) {
            m_lastConnectError = ConnectionRefusedError;
            finish(ConnectionRefusedError, text);
        } else if (e == QAbstractSocket::SocketTimeoutError) {
            finish(TimeoutError, text);
        } else {
            finish(UnknownNetworkError, text);
        }
    }

    void onClosed() {
        if (m_finished || m_phase == Done)
            return;
        if (m_socket)
            m_in += m_socket->readAll();
        parse();
        if (m_finished || m_phase == Done)
            return;
        if (m_phase == CloseBody)
            complete();
        else
            finish(RemoteHostClosedError, QString("Connection closed before the response was complete"));
    }

    void onData() {
        if (m_finished || !m_socket)
            return;
        m_in += m_socket->readAll();
        m_receivedAny = true;
        armTimeout();
        parse();
    }

    // Returns the next line without its terminator, or false if incomplete.
    bool takeLine(QByteArray *line) {
        const qsizetype nl = m_in.indexOf('\n', m_inPos);
        if (nl < 0)
            return false;
        qsizetype end = nl;
        if (end > m_inPos && m_in[end - 1] == '\r')
            --end;
        *line = m_in.mid(m_inPos, end - m_inPos);
        m_inPos = nl + 1;
        return true;
    }

    void parse() {
        QPointer<QNetworkReply> guard(this);
        while (guard && !m_finished && m_phase != Done) {
            QByteArray line;
            switch (m_phase) {
            case StatusLine:
                if (!takeLine(&line)) {
                    if (!checkHeaderLimit())
                        return;
                    compact();
                    return;
                }
                if (!parseStatusLine(line))
                    return;
                m_phase = Headers;
                break;
            case Headers:
                if (!takeLine(&line)) {
                    if (!checkHeaderLimit())
                        return;
                    compact();
                    return;
                }
                if (!line.isEmpty()) {
                    if (!addHeader(line))
                        return;
                } else if (!headersDone()) {
                    return;
                }
                break;
            case FixedBody: {
                const qint64 n = qMin(m_remaining, m_in.size() - m_inPos);
                deliver(n);
                if (!guard || m_finished)
                    return;
                m_remaining -= n;
                if (m_remaining == 0)
                    complete();
                else {
                    compact();
                    return;
                }
                break;
            }
            case CloseBody:
                deliver(m_in.size() - m_inPos);
                compact();
                return;
            case ChunkSize:
                if (!takeLine(&line)) {
                    if (m_in.size() - m_inPos > 1024)
                        protocolError(QString("Chunk size line is too long"));
                    return;
                }
                if (!parseChunkSize(line, &m_remaining)) {
                    protocolError(QString("Invalid chunk size"));
                    return;
                }
                m_phase = m_remaining == 0 ? Trailers : ChunkData;
                break;
            case ChunkData: {
                const qint64 n = qMin(m_remaining, m_in.size() - m_inPos);
                deliver(n);
                if (!guard || m_finished)
                    return;
                m_remaining -= n;
                if (m_remaining > 0) {
                    compact();
                    return;
                }
                m_phase = ChunkEnd;
                break;
            }
            case ChunkEnd:
                if (!takeLine(&line))
                    return;
                if (!line.isEmpty()) {
                    protocolError(QString("Missing chunk terminator"));
                    return;
                }
                m_phase = ChunkSize;
                break;
            case Trailers:
                if (!takeLine(&line))
                    return;
                if (line.isEmpty())
                    complete();
                break;
            case Done:
                break;
            }
        }
    }

    void compact() {
        m_in = m_in.mid(m_inPos);
        m_inPos = 0;
    }

    bool checkHeaderLimit() {
        if (m_in.size() - m_inPos > kMaxHeaderBytes) {
            protocolError(QString("Response headers are too large"));
            return false;
        }
        return true;
    }

    void protocolError(const QString &text) { finish(ProtocolFailure, text); }

    bool parseStatusLine(const QByteArray &line) {
        const bool version = line.startsWith("HTTP/1.1 ") || line.startsWith("HTTP/1.0 ");
        if (!version || line.size() < 12) {
            protocolError(QString("Invalid status line"));
            return false;
        }
        int code = 0;
        for (int i = 9; i < 12; ++i) {
            if (line[i] < '0' || line[i] > '9') {
                protocolError(QString("Invalid status code"));
                return false;
            }
            code = code * 10 + (line[i] - '0');
        }
        if (line.size() > 12 && line[12] != ' ') {
            protocolError(QString("Invalid status line"));
            return false;
        }
        m_status = code;
        m_reason = line.mid(13);
        m_headers.clear();
        m_headerTotal = line.size();
        return true;
    }

    bool addHeader(const QByteArray &line) {
        m_headerTotal += line.size();
        if (m_headerTotal > kMaxHeaderBytes) {
            protocolError(QString("Response headers are too large"));
            return false;
        }
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0 || !isToken(line.left(colon))) {
            protocolError(QString("Invalid response header"));
            return false;
        }
        m_headers.append({line.left(colon), line.mid(colon + 1).trimmed()});
        return true;
    }

    bool headersDone() {
        if (m_status >= 100 && m_status < 200) { // interim response
            m_phase = StatusLine;
            return true;
        }

        qint64 total = -1;
        bool chunked = false;
        for (const auto &h : m_headers) {
            if (sameName(h.first, "Transfer-Encoding")) {
                if (h.second.toLower().contains("chunked"))
                    chunked = true;
            } else if (sameName(h.first, "Content-Length")) {
                qint64 v = 0;
                if (!parseLength(h.second, &v) || (total >= 0 && total != v)) {
                    protocolError(QString("Invalid Content-Length"));
                    return false;
                }
                total = v;
            }
        }
        const bool noBody = m_verb == "HEAD" || m_status == 204 || m_status == 304;
        m_expected = chunked || noBody ? (noBody ? 0 : -1) : total;

        if (followsRedirect())
            return redirect();

        QPointer<QNetworkReply> guard(this);
        metaDataChanged();
        if (!guard || m_finished)
            return false;
        if (noBody) {
            complete();
        } else if (chunked) {
            m_phase = ChunkSize;
        } else if (total >= 0) {
            m_remaining = total;
            m_phase = FixedBody;
            if (total == 0)
                complete();
        } else {
            m_phase = CloseBody;
        }
        return !m_finished;
    }

    bool followsRedirect() const {
        const bool redirectStatus =
            m_status == 301 || m_status == 302 || m_status == 303 || m_status == 307 || m_status == 308;
        return redirectStatus && m_request.redirectPolicy() != QNetworkRequest::ManualRedirectPolicy &&
               hasRawHeader("Location");
    }

    bool redirect() {
        if (++m_redirects > m_request.maximumRedirectsAllowed()) {
            finish(TooManyRedirectsError, QString("Too many redirects"));
            return false;
        }
        const QUrl target = m_url.resolved(QUrl(QString::fromUtf8(rawHeader("Location"))));
        if (!target.isValid()) {
            protocolError(QString("Invalid redirect location"));
            return false;
        }
        const bool sameOrigin = originOf(target) == originOf(m_url);
        if (m_request.redirectPolicy() == QNetworkRequest::SameOriginRedirectPolicy && !sameOrigin) {
            finish(InsecureRedirectError, QString("Redirect leaves the origin"));
            return false;
        }
        if (!supportedScheme(target)) {
            finish(InsecureRedirectError, QString("Redirect to an unsupported scheme"));
            return false;
        }
        if (m_request.redirectPolicy() == QNetworkRequest::NoLessSafeRedirectPolicy &&
            m_url.scheme() == QString("https") && target.scheme() != QString("https")) {
            finish(InsecureRedirectError, QString("Redirect from https to http"));
            return false;
        }
        // Credentials belong to the origin they were given for.
        if (!sameOrigin) {
            m_request.setRawHeader("Authorization", QByteArray());
            m_request.setRawHeader("Cookie", QByteArray());
#ifndef QT_NO_SSL
            // Accepting one server's bad certificate says nothing about another.
            m_ignoreAll = false;
            m_ignoreList.clear();
#endif
        }
        // Only 307 and 308 promise to repeat the method and body.
        if (m_status == 303 || ((m_status == 301 || m_status == 302) && m_verb == "POST")) {
            if (m_verb != "HEAD")
                m_verb = "GET";
            m_upload.clear();
            m_request.setRawHeader("Content-Type", QByteArray());
        }
        m_url = target;
        m_in.clear();
        m_inPos = 0;
        m_phase = StatusLine;
        m_status = 0;
        m_headers.clear();
        m_receivedAny = false;
        dropSocket();
        QPointer<QNetworkReply> guard(this);
        redirected(target);
        if (guard && !m_finished)
            QTimer::singleShot(0, this, [this] { begin(); });
        return false;
    }

    void deliver(qint64 n) {
        if (n <= 0)
            return;
        m_body.append(m_in.constData() + m_inPos, n);
        m_inPos += n;
        m_downloaded += n;
        QPointer<QNetworkReply> guard(this);
        downloadProgress(m_downloaded, m_expected);
        if (guard && !m_finished) {
            readyRead();
            if (guard)
                channelReadyRead(0);
        }
    }

    void complete() {
        m_phase = Done;
        const NetworkError e = errorForStatus(m_status);
        finish(e, e == NoError ? QString() : QString("Server replied: ") + QString::fromUtf8(m_reason));
    }

    void finish(NetworkError error, const QString &text) {
        if (m_finished)
            return;
        m_finished = true;
        m_error = error;
        if (!text.isEmpty())
            setErrorString(text);
        m_phase = Done;
        m_timer.stop();
        if (m_lookup >= 0) {
            QHostInfo::abortHostLookup(m_lookup);
            m_lookup = -1;
        }
        dropSocket();
        // abort() and early failures may be called from inside a slot.
        QTimer::singleShot(0, this, [this] { emitFinished(); });
    }

    void armTimeout() {
        if (m_timeoutMs <= 0)
            return;
        if (m_timer.interval() != m_timeoutMs) {
            m_timer.setSingleShot(true);
            QObject::connect(&m_timer, &QTimer::timeout, this,
                             [this] { finish(TimeoutError, QString("Operation timed out")); });
        }
        m_timer.start(m_timeoutMs);
    }

    QByteArray m_verb;
    QByteArray m_upload;
    QList<QHostAddress> m_addresses;
    QTcpSocket *m_socket = nullptr;
    QTimer m_timer;
    int m_timeoutMs = 0;
    int m_lookup = -1;
    int m_redirects = 0;
    Phase m_phase = StatusLine;
    QByteArray m_in;
    qint64 m_inPos = 0;
    qint64 m_remaining = 0;
    qint64 m_expected = -1;
    qint64 m_downloaded = 0;
    qint64 m_uploaded = 0;
    qint64 m_headerBytesLeft = 0;
    qint64 m_headerTotal = 0;
    bool m_receivedAny = false;
    NetworkError m_lastConnectError = ConnectionRefusedError;
    QString m_lastConnectText = QString("Could not connect");
#ifndef QT_NO_SSL
    bool m_ignoreAll = false;
    QList<QSslError> m_ignoreList;
#endif
};

} // namespace QtPrivate

/*!
    \class QNetworkRequest
    \brief The QNetworkRequest class describes a network request: a URL plus
    headers and transfer settings.

    \differences
    Attributes are stored and returned, but none of them changes how the
    request is sent; setRedirectPolicy(),
    setMaximumRedirectsAllowed() and setTransferTimeout() cover the common
    settings. Known headers are read and written as QByteArray rather than
    QVariant. There is no priority or HTTP/2 setting, and no
    \c UserVerifiedRedirectPolicy.
    \enddifferences

    \sa QNetworkAccessManager, QNetworkReply
*/

/*!
    \enum QNetworkRequest::KnownHeaders

    Common HTTP headers.

    \value ContentTypeHeader \c Content-Type
    \value ContentLengthHeader \c Content-Length
    \value LocationHeader \c Location
    \value LastModifiedHeader \c Last-Modified
    \value CookieHeader \c Cookie
    \value SetCookieHeader \c Set-Cookie
    \value ContentDispositionHeader \c Content-Disposition
    \value UserAgentHeader \c User-Agent
    \value ServerHeader \c Server
    \value IfModifiedSinceHeader \c If-Modified-Since
    \value ETagHeader \c ETag
    \value IfMatchHeader \c If-Match
    \value IfNoneMatchHeader \c If-None-Match
*/

/*!
    \enum QNetworkRequest::RedirectPolicy

    Selects what happens when the server redirects a request.

    \value ManualRedirectPolicy Redirects are not followed; the 3xx reply is returned.
    \value NoLessSafeRedirectPolicy Redirects are followed, but not to a less secure scheme.
    \value SameOriginRedirectPolicy Only redirects to the same scheme, host and port are followed.
*/

/*!
    \fn QNetworkRequest::QNetworkRequest()

    Constructs a request with no URL.
*/

/*!
    Constructs a request for \a url.
*/
QNetworkRequest::QNetworkRequest(const QUrl &url) : m_url(url) {}

/*!
    Returns the URL.

    \sa setUrl()
*/
QUrl QNetworkRequest::url() const { return m_url; }

/*!
    Sets the URL to \a url.

    \sa url()
*/
void QNetworkRequest::setUrl(const QUrl &url) { m_url = url; }

/*!
    Returns the value of the known \a header, or an empty array if it is unset.

    \sa setHeader()
*/
QByteArray QNetworkRequest::header(KnownHeaders header) const { return rawHeader(headerName(header)); }

/*!
    Sets the known \a header to \a value.

    \sa header()
*/
void QNetworkRequest::setHeader(KnownHeaders header, const QByteArray &value) {
    setRawHeader(headerName(header), value);
}

/*!
    Returns \c true if the header \a headerName is set. Names are not case
    sensitive.

    \sa rawHeader()
*/
bool QNetworkRequest::hasRawHeader(const QByteArray &headerName) const {
    for (const auto &h : m_headers)
        if (sameName(h.first, headerName))
            return true;
    return false;
}

/*!
    Returns the names of the headers that are set.
*/
QList<QByteArray> QNetworkRequest::rawHeaderList() const {
    QList<QByteArray> r;
    for (const auto &h : m_headers)
        r.append(h.first);
    return r;
}

/*!
    Returns the value of the header \a headerName, or an empty array if it is
    unset.

    \sa setRawHeader()
*/
QByteArray QNetworkRequest::rawHeader(const QByteArray &headerName) const {
    for (const auto &h : m_headers)
        if (sameName(h.first, headerName))
            return h.second;
    return QByteArray();
}

/*!
    Sets the header \a headerName to \a headerValue, replacing any previous
    value. An empty value removes the header. A name or value containing line
    breaks makes the request fail with ProtocolInvalidOperationError, as it
    could otherwise inject extra headers.

    \sa rawHeader()
*/
void QNetworkRequest::setRawHeader(const QByteArray &headerName, const QByteArray &headerValue) {
    for (qsizetype i = 0; i < m_headers.size(); ++i) {
        if (sameName(m_headers.at(i).first, headerName)) {
            m_headers.removeAt(i);
            break;
        }
    }
    if (!headerValue.isEmpty())
        m_headers.append({headerName, headerValue});
}

/*!
    \enum QNetworkRequest::Attribute

    Keys for the values attached to a request with setAttribute() or reported
    on a reply with QNetworkReply::attribute(). Only the first group is
    produced by a reply; the rest are stored on a request but have no effect.

    \value HttpStatusCodeAttribute The HTTP status code, as an \c int. Reply only.
    \value HttpReasonPhraseAttribute The HTTP reason phrase, as a QByteArray. Reply only.
    \value RedirectionTargetAttribute The \c Location of a redirect that was not followed, as a QUrl, possibly relative. Reply only.
    \value ConnectionEncryptedAttribute \c true if the reply came over TLS. Reply only.
    \value SourceIsFromCacheAttribute Always \c false, as there is no cache. Reply only.
    \value HttpPipeliningWasUsedAttribute Always \c false. Reply only.
    \value Http2WasUsedAttribute Always \c false. Reply only.
    \value CacheLoadControlAttribute Stored only.
    \value CacheSaveControlAttribute Stored only.
    \value DoNotBufferUploadDataAttribute Stored only.
    \value HttpPipeliningAllowedAttribute Stored only.
    \value CustomVerbAttribute Stored only.
    \value CookieLoadControlAttribute Stored only.
    \value AuthenticationReuseAttribute Stored only.
    \value CookieSaveControlAttribute Stored only.
    \value MaximumDownloadBufferSizeAttribute Stored only.
    \value DownloadBufferAttribute Stored only.
    \value SynchronousRequestAttribute Stored only.
    \value BackgroundRequestAttribute Stored only.
    \value EmitAllUploadProgressSignalsAttribute Stored only.
    \value Http2AllowedAttribute Stored only.
    \value OriginalContentLengthAttribute Stored only.
    \value RedirectPolicyAttribute Stored only; use setRedirectPolicy().
    \value Http2DirectAttribute Stored only.
    \value ResourceTypeAttribute Stored only.
    \value AutoDeleteReplyOnFinishAttribute Stored only.
    \value ConnectionCacheExpiryTimeoutSecondsAttribute Stored only.
    \value Http2CleartextAllowedAttribute Stored only.
    \value UseCredentialsAttribute Stored only.
    \value User The first code reserved for applications.
    \value UserMax The last code reserved for applications.
*/

/*!
    Returns the value stored for \a code, or \a defaultValue if none is set.

    \sa setAttribute()
*/
QVariant QNetworkRequest::attribute(Attribute code, const QVariant &defaultValue) const {
    for (const auto &a : m_attributes)
        if (a.first == static_cast<int>(code))
            return a.second;
    return defaultValue;
}

/*!
    Stores \a value for \a code, replacing any previous value. An invalid
    QVariant removes the attribute.

    \sa attribute()
*/
void QNetworkRequest::setAttribute(Attribute code, const QVariant &value) {
    for (qsizetype i = 0; i < m_attributes.size(); ++i) {
        if (m_attributes.at(i).first == static_cast<int>(code)) {
            m_attributes.removeAt(i);
            break;
        }
    }
    if (value.isValid())
        m_attributes.append({static_cast<int>(code), value});
}

/*!
    Returns the redirect policy. The default is NoLessSafeRedirectPolicy.

    \sa setRedirectPolicy()
*/
QNetworkRequest::RedirectPolicy QNetworkRequest::redirectPolicy() const { return m_policy; }

/*!
    Sets the redirect policy to \a policy.

    \sa redirectPolicy()
*/
void QNetworkRequest::setRedirectPolicy(RedirectPolicy policy) { m_policy = policy; }

/*!
    Returns the most redirects that are followed. The default is \c 50.

    \sa setMaximumRedirectsAllowed()
*/
int QNetworkRequest::maximumRedirectsAllowed() const { return m_maxRedirects; }

/*!
    Sets the most redirects that are followed to \a maximumRedirectsAllowed.

    \sa maximumRedirectsAllowed()
*/
void QNetworkRequest::setMaximumRedirectsAllowed(int maximumRedirectsAllowed) {
    m_maxRedirects = maximumRedirectsAllowed;
}

/*!
    Returns the inactivity timeout in milliseconds; \c 0 uses the manager's.

    \sa setTransferTimeout()
*/
int QNetworkRequest::transferTimeout() const { return m_timeout; }

/*!
    Fails the request with TimeoutError if no data moves for \a timeout
    milliseconds.

    \sa transferTimeout()
*/
void QNetworkRequest::setTransferTimeout(int timeout) { m_timeout = timeout; }

#ifndef QT_NO_SSL
/*!
    Returns the TLS settings for this request.

    \sa setSslConfiguration()
*/
QSslConfiguration QNetworkRequest::sslConfiguration() const { return m_sslConfiguration; }

/*!
    Sets the TLS settings for this request to \a configuration, for example to
    trust a private CA.

    \sa sslConfiguration()
*/
void QNetworkRequest::setSslConfiguration(const QSslConfiguration &configuration) {
    m_sslConfiguration = configuration;
}
#endif

/*!
    \class QNetworkReply
    \brief The QNetworkReply class holds the response to a QNetworkAccessManager
    request.

    A reply is a sequential QIODevice. Data becomes readable as it arrives, and
    finished() is emitted once at the end, on success or failure. An HTTP error
    status such as 404 sets error(), and its body stays readable.

    \differences
    Only the attributes listed under QNetworkRequest::Attribute as reply
    attributes are produced; the others read back what the request stored.
    header() returns
    QByteArray. Unlike Qt, the whole response stays buffered until read, with
    no read buffer limit. There are no proxy errors, as proxies do not exist
    yet.
    \enddifferences

    \sa QNetworkAccessManager, QNetworkRequest
*/

/*!
    \enum QNetworkReply::NetworkError

    Describes how a reply failed.

    \value NoError The request succeeded.
    \value ConnectionRefusedError The server refused the connection.
    \value RemoteHostClosedError The server closed the connection before the reply was complete.
    \value HostNotFoundError The host name could not be resolved.
    \value TimeoutError The transfer timed out.
    \value OperationCanceledError abort() was called.
    \value SslHandshakeFailedError The TLS handshake failed, or the server could not be verified.
    \value TemporaryNetworkFailureError Not used.
    \value NetworkSessionFailedError Not used.
    \value BackgroundRequestNotAllowedError Not used.
    \value TooManyRedirectsError The redirect limit was reached.
    \value InsecureRedirectError The redirect policy refused a redirect.
    \value UnknownNetworkError Another network error occurred.
    \value ContentAccessDenied The server replied 403.
    \value ContentOperationNotPermittedError The server replied 405.
    \value ContentNotFoundError The server replied 404.
    \value AuthenticationRequiredError The server replied 401.
    \value ContentReSendError Not used.
    \value ContentConflictError The server replied 409.
    \value ContentGoneError The server replied 410.
    \value UnknownContentError The server replied with another 4xx status.
    \value ProtocolUnknownError The URL's scheme is not supported, or the URL is invalid.
    \value ProtocolInvalidOperationError The request itself is malformed.
    \value ProtocolFailure The server sent an invalid response.
    \value InternalServerError The server replied 500.
    \value OperationNotImplementedError The server replied 501.
    \value ServiceUnavailableError The server replied 503.
    \value UnknownServerError The server replied with another 5xx status.
*/

/*!
    Constructs a reply with the given \a parent.
*/
QNetworkReply::QNetworkReply(QObject *parent) : QIODevice(parent) {}

/*!
    Destroys the reply, abandoning the transfer if it is still running.
*/
QNetworkReply::~QNetworkReply() = default;

/*!
    Returns the URL of the content that was fetched, which differs from the
    request's URL after a redirect.

    \sa request()
*/
QUrl QNetworkReply::url() const { return m_url; }

/*!
    Returns the request this reply answers.
*/
QNetworkRequest QNetworkReply::request() const { return m_request; }

/*!
    Returns the manager that created the reply.
*/
QNetworkAccessManager *QNetworkReply::manager() const { return m_manager; }

/*!
    Returns the error, or NoError if there has been none.

    \sa errorOccurred()
*/
QNetworkReply::NetworkError QNetworkReply::error() const { return m_error; }

/*!
    Returns \c true once the reply has finished.

    \sa finished(), isRunning()
*/
bool QNetworkReply::isFinished() const { return m_finished; }

/*!
    Returns \c true while the request is in progress.

    \sa isFinished()
*/
bool QNetworkReply::isRunning() const { return !m_finished; }

/*!
    Returns the HTTP status code, or \c 0 if none has been received.

    \sa httpReasonPhrase()
*/
int QNetworkReply::httpStatusCode() const { return m_status; }

/*!
    Returns the HTTP reason phrase.

    \sa httpStatusCode()
*/
QByteArray QNetworkReply::httpReasonPhrase() const { return m_reason; }

/*!
    Returns the attribute for \a code, or an invalid QVariant if it has none.
    Status, reason, redirect target and encryption are taken from the
    response; any other code returns what the request stored with
    QNetworkRequest::setAttribute(), which is how applications tag a request
    and find the tag again in the reply.

    \sa httpStatusCode(), request()
*/
QVariant QNetworkReply::attribute(QNetworkRequest::Attribute code) const {
    const bool answered = m_status > 0;
    switch (code) {
    case QNetworkRequest::HttpStatusCodeAttribute:
        return answered ? QVariant(m_status) : QVariant();
    case QNetworkRequest::HttpReasonPhraseAttribute:
        return answered ? QVariant(m_reason) : QVariant();
    case QNetworkRequest::RedirectionTargetAttribute: {
        const QByteArray location = header(QNetworkRequest::LocationHeader);
        const bool redirect = m_status == 301 || m_status == 302 || m_status == 303 || m_status == 305 ||
                              m_status == 307 || m_status == 308;
        return redirect && !location.isEmpty() ? QVariant::fromValue(QUrl(QString::fromUtf8(location))) : QVariant();
    }
    case QNetworkRequest::ConnectionEncryptedAttribute:
        return answered ? QVariant(m_url.scheme().compare(QString("https"), Qt::CaseInsensitive) == 0) : QVariant();
    case QNetworkRequest::SourceIsFromCacheAttribute:
    case QNetworkRequest::HttpPipeliningWasUsedAttribute:
    case QNetworkRequest::Http2WasUsedAttribute:
        return answered ? QVariant(false) : QVariant();
    default:
        return m_request.attribute(code);
    }
}

/*!
    Returns the value of the known \a header.

    \sa rawHeader()
*/
QByteArray QNetworkReply::header(QNetworkRequest::KnownHeaders header) const { return rawHeader(headerName(header)); }

/*!
    Returns \c true if the response has the header \a headerName. Names are not
    case sensitive.
*/
bool QNetworkReply::hasRawHeader(const QByteArray &headerName) const {
    for (const auto &h : m_headers)
        if (sameName(h.first, headerName))
            return true;
    return false;
}

/*!
    Returns the names of the response headers.
*/
QList<QByteArray> QNetworkReply::rawHeaderList() const {
    QList<QByteArray> r;
    for (const auto &h : m_headers)
        r.append(h.first);
    return r;
}

/*!
    Returns the value of the header \a headerName. Repeated headers are joined
    with \c{", "}, or with a newline for \c Set-Cookie.
*/
QByteArray QNetworkReply::rawHeader(const QByteArray &headerName) const {
    QByteArray r;
    bool found = false;
    const QByteArray separator = sameName(headerName, "Set-Cookie") ? QByteArray("\n") : QByteArray(", ");
    for (const auto &h : m_headers) {
        if (!sameName(h.first, headerName))
            continue;
        if (found)
            r += separator;
        r += h.second;
        found = true;
    }
    return r;
}

/*!
    Returns the response headers in the order they were received.
*/
QList<std::pair<QByteArray, QByteArray>> QNetworkReply::rawHeaderPairs() const { return m_headers; }

/*!
    \reimp
*/
bool QNetworkReply::isSequential() const { return true; }

/*!
    \reimp
*/
qint64 QNetworkReply::bytesAvailable() const { return QIODevice::bytesAvailable() + (m_body.size() - m_bodyPos); }

/*!
    \reimp
*/
bool QNetworkReply::canReadLine() const { return QIODevice::canReadLine() || m_body.indexOf('\n', m_bodyPos) != -1; }

/*!
    \reimp
*/
bool QNetworkReply::atEnd() const { return m_finished && bytesAvailable() == 0; }

/*!
    \reimp
*/
void QNetworkReply::close() {
    if (!m_finished)
        abort();
    QIODevice::close();
}

#ifndef QT_NO_SSL
/*!
    Returns the TLS details of the connection, once the handshake has finished.
*/
QSslConfiguration QNetworkReply::sslConfiguration() const { return m_sslConfiguration; }

/*!
    Continues despite the \a errors listed, and no others. Call this before the
    handshake or from a slot connected to sslErrors(). It does not carry over to
    a redirect that leaves the origin.
*/
void QNetworkReply::ignoreSslErrors(const QList<QSslError> &errors) { Q_UNUSED(errors) }

/*!
    \overload

    Continues despite every verification problem. Call this from a slot
    connected to sslErrors(). It removes the protection TLS gives against
    impersonation, and does not carry over to a redirect that leaves the origin.
*/
void QNetworkReply::ignoreSslErrors() {}
#endif

/*!
    \fn void QNetworkReply::abort()

    Stops the transfer. The reply finishes with OperationCanceledError.
*/

/*!
    \reimp
*/
qint64 QNetworkReply::readData(char *data, qint64 maxSize) {
    const qint64 n = qMin(maxSize, m_body.size() - m_bodyPos);
    if (n > 0) {
        std::memcpy(data, m_body.constData() + m_bodyPos, static_cast<std::size_t>(n));
        m_bodyPos += n;
        if (m_bodyPos == m_body.size()) {
            m_body.clear();
            m_bodyPos = 0;
        }
        return n;
    }
    return m_finished ? -1 : 0;
}

/*!
    \reimp
*/
qint64 QNetworkReply::writeData(const char *data, qint64 maxSize) {
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    return -1;
}

/*!
    \class QNetworkAccessManager
    \brief The QNetworkAccessManager class sends HTTP requests and returns the
    replies.

    One manager is enough for a whole application. Each request returns a
    QNetworkReply straight away, and the transfer happens on the event loop:

    \code
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(QUrl("http://example.com/")));
    QObject::connect(reply, &QNetworkReply::finished, [reply] {
        if (reply->error() == QNetworkReply::NoError)
            qDebug() << reply->readAll();
        reply->deleteLater();
    });
    \endcode

    You own the reply: delete it with deleteLater() when finished() is emitted.

    Redirects are followed by default, and credentials are not sent to a
    different origin. A response cut short reports RemoteHostClosedError rather
    than appearing to succeed.

    \differences
    HTTP/1.1 only, over \c http: and \c https: (https needs a build with
    OpenSSL, or with Mbed TLS on Zephyr); \c ftp: and \c file: URLs fail with ProtocolUnknownError. Each
    request uses its own
    connection, with no keep-alive pooling. There is no cookie jar, proxy,
    cache, compressed-transfer support or \c authenticationRequired() signal;
    credentials in the URL are sent as HTTP Basic. A QIODevice body is read in
    full before the request starts, so uploads are not streamed.
    \enddifferences

    \sa QNetworkRequest, QNetworkReply, QUrl
*/

/*!
    \enum QNetworkAccessManager::Operation

    Identifies a kind of request.

    \value HeadOperation A \c HEAD request.
    \value GetOperation A \c GET request.
    \value PutOperation A \c PUT request.
    \value PostOperation A \c POST request.
    \value DeleteOperation A \c DELETE request.
    \value CustomOperation A request with a custom verb.
    \value UnknownOperation Not a valid operation.
*/

/*!
    Constructs a manager with the given \a parent.
*/
QNetworkAccessManager::QNetworkAccessManager(QObject *parent) : QObject(parent) {}

/*!
    Destroys the manager. Replies that are its children are destroyed too.
*/
QNetworkAccessManager::~QNetworkAccessManager() = default;

QNetworkReply *QNetworkAccessManager::start(const QNetworkRequest &request, const QByteArray &verb,
                                            const QByteArray &body) {
    return new QtPrivate::HttpReply(this, request, verb, body, m_timeout);
}

/*!
    Sends a \c HEAD request for \a request and returns the reply.
*/
QNetworkReply *QNetworkAccessManager::head(const QNetworkRequest &request) {
    return start(request, "HEAD", QByteArray());
}

/*!
    Sends a \c GET request for \a request and returns the reply.
*/
QNetworkReply *QNetworkAccessManager::get(const QNetworkRequest &request) {
    return start(request, "GET", QByteArray());
}

/*!
    Sends a \c POST request for \a request with the body \a data and returns the
    reply.
*/
QNetworkReply *QNetworkAccessManager::post(const QNetworkRequest &request, const QByteArray &data) {
    return start(request, "POST", data);
}

/*!
    \overload

    Sends a \c POST request for \a request, reading the body from \a data at
    once.
*/
QNetworkReply *QNetworkAccessManager::post(const QNetworkRequest &request, QIODevice *data) {
    return start(request, "POST", data ? data->readAll() : QByteArray());
}

/*!
    Sends a \c PUT request for \a request with the body \a data and returns the
    reply.
*/
QNetworkReply *QNetworkAccessManager::put(const QNetworkRequest &request, const QByteArray &data) {
    return start(request, "PUT", data);
}

/*!
    \overload

    Sends a \c PUT request for \a request, reading the body from \a data at once.
*/
QNetworkReply *QNetworkAccessManager::put(const QNetworkRequest &request, QIODevice *data) {
    return start(request, "PUT", data ? data->readAll() : QByteArray());
}

/*!
    Sends a \c DELETE request for \a request and returns the reply.
*/
QNetworkReply *QNetworkAccessManager::deleteResource(const QNetworkRequest &request) {
    return start(request, "DELETE", QByteArray());
}

/*!
    Sends \a request with the method \a verb and the body \a data, and returns
    the reply.
*/
QNetworkReply *QNetworkAccessManager::sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb,
                                                        const QByteArray &data) {
    return start(request, verb, data);
}

/*!
    Returns the default inactivity timeout in milliseconds; \c 0 means none.

    \sa setTransferTimeout()
*/
int QNetworkAccessManager::transferTimeout() const { return m_timeout; }

/*!
    Sets the default inactivity timeout to \a timeout milliseconds, for requests
    that do not set their own.

    \sa transferTimeout()
*/
void QNetworkAccessManager::setTransferTimeout(int timeout) { m_timeout = timeout; }

#include "moc_qnetworkaccessmanager.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qnetworkreply.cpp"         // NOLINT(bugprone-suspicious-include): generated by ntmoc
