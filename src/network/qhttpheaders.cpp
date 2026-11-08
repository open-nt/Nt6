// MIT License - see LICENSE
#include <QtNetwork/qhttpheaders.h>

#include <array>

/*!
    \class QHttpHeaders
    \brief The QHttpHeaders class holds the header fields of an HTTP message.
    \since 6.7

    A QHttpHeaders is an ordered list of name and value pairs. A name may
    occur more than once, and the order of the entries is kept, which matters
    for fields such as \c Set-Cookie. Names are compared without regard to
    case, and are stored and returned in lower case.

    Names and values are checked when they are added: a name must be an HTTP
    token, and a value must not contain a line break or a NUL byte.
    Whitespace around a value is removed. The functions that add a field
    return \c false and leave the headers unchanged when a check fails, so
    untrusted input cannot inject further header lines.

    \code
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/plain");
    headers.append("x-request-id", "42");
    QByteArrayView type = headers.value(QHttpHeaders::WellKnownHeader::ContentType);
    \endcode

    \differences
    Names and values are passed as QByteArrayView (Qt takes
    \c QAnyStringView), so a QString must be converted with \c toUtf8() or
    \c toLatin1() first. WellKnownHeader has the common headers only, not the
    whole IANA registry. There are no \c intValue(), \c dateTimeValue() or
    \c QDebug streaming functions.
    \enddifferences

    \sa QHttpServerRequest::headers(), QHttpServerResponse::setHeaders()
*/

/*!
    \enum QHttpHeaders::WellKnownHeader

    Names of common header fields. wellKnownHeaderName() returns the name of
    each value in lower case.

    \value AIM \c a-im
    \value Accept \c accept
    \value AcceptCharset \c accept-charset
    \value AcceptEncoding \c accept-encoding
    \value AcceptLanguage \c accept-language
    \value AcceptRanges \c accept-ranges
    \value AccessControlAllowCredentials \c access-control-allow-credentials
    \value AccessControlAllowHeaders \c access-control-allow-headers
    \value AccessControlAllowMethods \c access-control-allow-methods
    \value AccessControlAllowOrigin \c access-control-allow-origin
    \value AccessControlExposeHeaders \c access-control-expose-headers
    \value AccessControlMaxAge \c access-control-max-age
    \value AccessControlRequestHeaders \c access-control-request-headers
    \value AccessControlRequestMethod \c access-control-request-method
    \value Age \c age
    \value Allow \c allow
    \value AltSvc \c alt-svc
    \value Authorization \c authorization
    \value CacheControl \c cache-control
    \value Connection \c connection
    \value ContentDisposition \c content-disposition
    \value ContentEncoding \c content-encoding
    \value ContentLanguage \c content-language
    \value ContentLength \c content-length
    \value ContentLocation \c content-location
    \value ContentRange \c content-range
    \value ContentSecurityPolicy \c content-security-policy
    \value ContentType \c content-type
    \value Cookie \c cookie
    \value Date \c date
    \value ETag \c etag
    \value Expect \c expect
    \value Expires \c expires
    \value Forwarded \c forwarded
    \value From \c from
    \value Host \c host
    \value IfMatch \c if-match
    \value IfModifiedSince \c if-modified-since
    \value IfNoneMatch \c if-none-match
    \value IfRange \c if-range
    \value IfUnmodifiedSince \c if-unmodified-since
    \value KeepAlive \c keep-alive
    \value LastModified \c last-modified
    \value Link \c link
    \value Location \c location
    \value MaxForwards \c max-forwards
    \value Origin \c origin
    \value Pragma \c pragma
    \value ProxyAuthenticate \c proxy-authenticate
    \value ProxyAuthorization \c proxy-authorization
    \value Range \c range
    \value Referer \c referer
    \value RetryAfter \c retry-after
    \value SecWebSocketAccept \c sec-websocket-accept
    \value SecWebSocketExtensions \c sec-websocket-extensions
    \value SecWebSocketKey \c sec-websocket-key
    \value SecWebSocketProtocol \c sec-websocket-protocol
    \value SecWebSocketVersion \c sec-websocket-version
    \value Server \c server
    \value SetCookie \c set-cookie
    \value StrictTransportSecurity \c strict-transport-security
    \value TE \c te
    \value Trailer \c trailer
    \value TransferEncoding \c transfer-encoding
    \value Upgrade \c upgrade
    \value UserAgent \c user-agent
    \value Vary \c vary
    \value Via \c via
    \value WWWAuthenticate \c www-authenticate
    \value XContentTypeOptions \c x-content-type-options
    \value XForwardedFor \c x-forwarded-for
    \value XFrameOptions \c x-frame-options
*/

namespace {

constexpr std::array<const char *, 72> kWellKnownNames = {
    "a-im",
    "accept",
    "accept-charset",
    "accept-encoding",
    "accept-language",
    "accept-ranges",
    "access-control-allow-credentials",
    "access-control-allow-headers",
    "access-control-allow-methods",
    "access-control-allow-origin",
    "access-control-expose-headers",
    "access-control-max-age",
    "access-control-request-headers",
    "access-control-request-method",
    "age",
    "allow",
    "alt-svc",
    "authorization",
    "cache-control",
    "connection",
    "content-disposition",
    "content-encoding",
    "content-language",
    "content-length",
    "content-location",
    "content-range",
    "content-security-policy",
    "content-type",
    "cookie",
    "date",
    "etag",
    "expect",
    "expires",
    "forwarded",
    "from",
    "host",
    "if-match",
    "if-modified-since",
    "if-none-match",
    "if-range",
    "if-unmodified-since",
    "keep-alive",
    "last-modified",
    "link",
    "location",
    "max-forwards",
    "origin",
    "pragma",
    "proxy-authenticate",
    "proxy-authorization",
    "range",
    "referer",
    "retry-after",
    "sec-websocket-accept",
    "sec-websocket-extensions",
    "sec-websocket-key",
    "sec-websocket-protocol",
    "sec-websocket-version",
    "server",
    "set-cookie",
    "strict-transport-security",
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "user-agent",
    "vary",
    "via",
    "www-authenticate",
    "x-content-type-options",
    "x-forwarded-for",
    "x-frame-options",
};
static_assert(kWellKnownNames.size() == static_cast<std::size_t>(QHttpHeaders::WellKnownHeader::XFrameOptions) + 1,
              "one name per WellKnownHeader value");

// RFC 9110, section 5.6.2: tchar.
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

bool isValidName(QByteArrayView name) {
    if (name.isEmpty())
        return false;
    for (char c : name) {
        if (!isTokenChar(c))
            return false;
    }
    return true;
}

char asciiLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

QByteArray normalizedName(QByteArrayView name) {
    QByteArray out(name.data(), name.size());
    for (char &c : out)
        c = asciiLower(c);
    return out;
}

bool isWhitespace(char c) { return c == ' ' || c == '\t'; }

// Removes surrounding whitespace; returns false for a value that would break the message framing.
bool normalizedValue(QByteArrayView value, QByteArray *out) {
    for (char c : value) {
        if (c == '\r' || c == '\n' || c == '\0')
            return false;
    }
    qsizetype begin = 0;
    qsizetype end = value.size();
    while (begin < end && isWhitespace(value.at(begin)))
        ++begin;
    while (end > begin && isWhitespace(value.at(end - 1)))
        --end;
    *out = QByteArray(value.data() + begin, end - begin);
    if (out->isNull())
        *out = QByteArray("");
    return true;
}

bool sameName(const QByteArray &stored, QByteArrayView name) {
    if (stored.size() != name.size())
        return false;
    for (qsizetype i = 0; i < name.size(); ++i) {
        if (stored.at(i) != asciiLower(name.at(i)))
            return false;
    }
    return true;
}

QByteArrayView nameOf(QHttpHeaders::WellKnownHeader name) { return QHttpHeaders::wellKnownHeaderName(name); }

} // namespace

/*!
    Constructs an empty set of headers.
*/
QHttpHeaders::QHttpHeaders() noexcept = default;

/*!
    Constructs a copy of \a other.
*/
QHttpHeaders::QHttpHeaders(const QHttpHeaders &other) = default;

/*!
    Move-constructs headers from \a other, which is left empty.
*/
QHttpHeaders::QHttpHeaders(QHttpHeaders &&other) noexcept = default;

/*!
    Assigns \a other to these headers and returns a reference to them.
*/
QHttpHeaders &QHttpHeaders::operator=(const QHttpHeaders &other) = default;

/*!
    \overload

    Move-assigns \a other to these headers and returns a reference to them.
*/
QHttpHeaders &QHttpHeaders::operator=(QHttpHeaders &&other) noexcept = default;

/*!
    Destroys the headers.
*/
QHttpHeaders::~QHttpHeaders() = default;

/*!
    Appends a field with the given \a name and \a value. Returns \c true on
    success, or \c false if the name or the value is not valid.

    \sa insert(), replaceOrAppend()
*/
bool QHttpHeaders::append(QByteArrayView name, QByteArrayView value) { return insert(m_headers.size(), name, value); }

/*!
    \overload
*/
bool QHttpHeaders::append(WellKnownHeader name, QByteArrayView value) { return append(nameOf(name), value); }

/*!
    Inserts a field with the given \a name and \a value at index \a i.
    Returns \c true on success, or \c false if \a i is out of range or the
    name or the value is not valid.

    \sa append()
*/
bool QHttpHeaders::insert(qsizetype i, QByteArrayView name, QByteArrayView value) {
    if (i < 0 || i > m_headers.size() || !isValidName(name))
        return false;
    QByteArray v;
    if (!normalizedValue(value, &v))
        return false;
    m_headers.insert(i, {normalizedName(name), v});
    return true;
}

/*!
    \overload
*/
bool QHttpHeaders::insert(qsizetype i, WellKnownHeader name, QByteArrayView value) {
    return insert(i, nameOf(name), value);
}

/*!
    Replaces the field at index \a i with one named \a name whose value is
    \a newValue. Returns \c true on success, or \c false if \a i is out of
    range or the name or the value is not valid.

    \sa replaceOrAppend()
*/
bool QHttpHeaders::replace(qsizetype i, QByteArrayView name, QByteArrayView newValue) {
    if (i < 0 || i >= m_headers.size() || !isValidName(name))
        return false;
    QByteArray v;
    if (!normalizedValue(newValue, &v))
        return false;
    m_headers[i] = {normalizedName(name), v};
    return true;
}

/*!
    \overload
*/
bool QHttpHeaders::replace(qsizetype i, WellKnownHeader name, QByteArrayView newValue) {
    return replace(i, nameOf(name), newValue);
}

/*!
    Sets the field \a name to \a newValue. The first field with that name is
    replaced and any others are removed; if there is none, the field is
    appended. Returns \c true on success, or \c false if the name or the
    value is not valid.

    \sa append(), replace()
*/
bool QHttpHeaders::replaceOrAppend(QByteArrayView name, QByteArrayView newValue) {
    if (!isValidName(name))
        return false;
    QByteArray v;
    if (!normalizedValue(newValue, &v))
        return false;
    qsizetype first = -1;
    for (qsizetype i = 0; i < m_headers.size();) {
        if (sameName(m_headers.at(i).first, name)) {
            if (first < 0) {
                first = i;
                m_headers[i].second = v;
                ++i;
            } else {
                m_headers.removeAt(i);
            }
        } else {
            ++i;
        }
    }
    if (first < 0)
        m_headers.append({normalizedName(name), v});
    return true;
}

/*!
    \overload
*/
bool QHttpHeaders::replaceOrAppend(WellKnownHeader name, QByteArrayView newValue) {
    return replaceOrAppend(nameOf(name), newValue);
}

/*!
    Returns \c true if there is a field named \a name; otherwise returns \c false.
*/
bool QHttpHeaders::contains(QByteArrayView name) const {
    for (const auto &h : m_headers) {
        if (sameName(h.first, name))
            return true;
    }
    return false;
}

/*!
    \overload
*/
bool QHttpHeaders::contains(WellKnownHeader name) const { return contains(nameOf(name)); }

/*!
    Removes all fields.

    \sa isEmpty()
*/
void QHttpHeaders::clear() { m_headers.clear(); }

/*!
    Removes every field named \a name.

    \sa removeAt()
*/
void QHttpHeaders::removeAll(QByteArrayView name) {
    for (qsizetype i = m_headers.size() - 1; i >= 0; --i) {
        if (sameName(m_headers.at(i).first, name))
            m_headers.removeAt(i);
    }
}

/*!
    \overload
*/
void QHttpHeaders::removeAll(WellKnownHeader name) { removeAll(nameOf(name)); }

/*!
    Removes the field at index \a i. \a i must be a valid index.

    \sa removeAll()
*/
void QHttpHeaders::removeAt(qsizetype i) { m_headers.removeAt(i); }

/*!
    Returns the value of the first field named \a name, or \a defaultValue
    if there is none. The view is invalidated when the headers change.

    \sa values(), combinedValue()
*/
QByteArrayView QHttpHeaders::value(QByteArrayView name, QByteArrayView defaultValue) const noexcept {
    for (const auto &h : m_headers) {
        if (sameName(h.first, name))
            return h.second;
    }
    return defaultValue;
}

/*!
    \overload
*/
QByteArrayView QHttpHeaders::value(WellKnownHeader name, QByteArrayView defaultValue) const noexcept {
    return value(nameOf(name), defaultValue);
}

/*!
    Returns the values of all fields named \a name, in order.

    \sa value(), combinedValue()
*/
QList<QByteArray> QHttpHeaders::values(QByteArrayView name) const {
    QList<QByteArray> out;
    for (const auto &h : m_headers) {
        if (sameName(h.first, name))
            out.append(h.second);
    }
    return out;
}

/*!
    \overload
*/
QList<QByteArray> QHttpHeaders::values(WellKnownHeader name) const { return values(nameOf(name)); }

/*!
    Returns the value of the field at index \a i. \a i must be a valid index.

    \sa nameAt()
*/
QByteArrayView QHttpHeaders::valueAt(qsizetype i) const noexcept { return m_headers.at(i).second; }

/*!
    Returns the name of the field at index \a i, in lower case. \a i must be
    a valid index.

    \sa valueAt()
*/
QLatin1StringView QHttpHeaders::nameAt(qsizetype i) const noexcept {
    const QByteArray &name = m_headers.at(i).first;
    return QLatin1StringView(name.constData(), name.size());
}

/*!
    Returns the values of all fields named \a name joined with commas, the
    way HTTP combines repeated fields, or a null QByteArray if there is no
    such field.

    \sa values()
*/
QByteArray QHttpHeaders::combinedValue(QByteArrayView name) const {
    QByteArray out;
    bool found = false;
    for (const auto &h : m_headers) {
        if (!sameName(h.first, name))
            continue;
        if (found)
            out += ", ";
        out += h.second;
        found = true;
    }
    if (found && out.isNull())
        out = QByteArray("");
    return out;
}

/*!
    \overload
*/
QByteArray QHttpHeaders::combinedValue(WellKnownHeader name) const { return combinedValue(nameOf(name)); }

/*!
    Returns the lower-case name of the header \a name.
*/
QByteArrayView QHttpHeaders::wellKnownHeaderName(WellKnownHeader name) noexcept {
    const auto index = static_cast<std::size_t>(name);
    if (index >= kWellKnownNames.size())
        return {};
    return QByteArrayView(kWellKnownNames[index]);
}

/*!
    Returns headers made from the name and value pairs in \a headers. Pairs
    whose name or value is not valid are skipped.

    \sa toListOfPairs()
*/
QHttpHeaders QHttpHeaders::fromListOfPairs(const QList<std::pair<QByteArray, QByteArray>> &headers) {
    QHttpHeaders out;
    out.reserve(headers.size());
    for (const auto &h : headers)
        out.append(h.first, h.second);
    return out;
}

/*!
    Returns headers made from the entries of \a headers. Entries whose name
    or value is not valid are skipped.

    \sa toMultiMap()
*/
QHttpHeaders QHttpHeaders::fromMultiMap(const QMultiMap<QByteArray, QByteArray> &headers) {
    QHttpHeaders out;
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
        out.append(it.key(), it.value());
    return out;
}

/*!
    Returns headers made from the entries of \a headers. Entries whose name
    or value is not valid are skipped.

    \sa toMultiHash()
*/
QHttpHeaders QHttpHeaders::fromMultiHash(const QMultiHash<QByteArray, QByteArray> &headers) {
    QHttpHeaders out;
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
        out.append(it.key(), it.value());
    return out;
}

/*!
    Returns the fields as a list of name and value pairs, in order.

    \sa fromListOfPairs()
*/
QList<std::pair<QByteArray, QByteArray>> QHttpHeaders::toListOfPairs() const { return m_headers; }

/*!
    Returns the fields as a map from name to value.

    \sa fromMultiMap()
*/
QMultiMap<QByteArray, QByteArray> QHttpHeaders::toMultiMap() const {
    QMultiMap<QByteArray, QByteArray> out;
    for (const auto &h : m_headers)
        out.insert(h.first, h.second);
    return out;
}

/*!
    Returns the fields as a hash from name to value.

    \sa fromMultiHash()
*/
QMultiHash<QByteArray, QByteArray> QHttpHeaders::toMultiHash() const {
    QMultiHash<QByteArray, QByteArray> out;
    for (const auto &h : m_headers)
        out.insert(h.first, h.second);
    return out;
}

#include "moc_qhttpheaders.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
