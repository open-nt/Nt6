// MIT License - see LICENSE
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qurl.h>
#include <QtCore/qurlquery.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace {

constexpr const char *kUnreserved = "-._~";
constexpr const char *kSubDelims = "!$&'()*+,;=";

bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isUnreservedChar(char c) { return isAlpha(c) || isDigit(c) || std::strchr(kUnreserved, c) != nullptr; }

int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool isEscape(const QByteArray &s, qsizetype i) {
    return i + 2 < s.size() && s[i] == '%' && hexValue(s[i + 1]) >= 0 && hexValue(s[i + 2]) >= 0;
}

// Which bytes may appear unescaped in a component, beyond unreserved ones.
enum Component { UserComponent, PathComponent, QueryComponent, FragmentComponent };

const char *allowedIn(Component c) {
    switch (c) {
    case UserComponent:
        return "!$&'()*+,;=";
    case PathComponent:
        return "!$&'()*+,;=:@/";
    default:
        return "!$&'()*+,;=:@/?";
    }
}

void appendEscaped(QByteArray &out, uchar b) {
    static const char *hex = "0123456789ABCDEF";
    out.append('%');
    out.append(hex[b >> 4]);
    out.append(hex[b & 15]);
}

// Encodes text. In tolerant mode an existing %XX stays as it is; a stray % does not.
QByteArray encodeComponent(const QByteArray &utf8, Component c, bool keepEscapes) {
    const char *allowed = allowedIn(c);
    QByteArray out;
    out.reserve(utf8.size());
    for (qsizetype i = 0; i < utf8.size(); ++i) {
        const char ch = utf8[i];
        if (keepEscapes && isEscape(utf8, i)) {
            out.append('%');
            out.append(static_cast<char>(std::toupper(static_cast<uchar>(utf8[i + 1]))));
            out.append(static_cast<char>(std::toupper(static_cast<uchar>(utf8[i + 2]))));
            i += 2;
        } else if (ch != '\0' && (isUnreservedChar(ch) || std::strchr(allowed, ch))) {
            out.append(ch);
        } else {
            appendEscaped(out, static_cast<uchar>(ch));
        }
    }
    return out;
}

QByteArray decodeBytes(const QByteArray &s) {
    QByteArray out;
    out.reserve(s.size());
    for (qsizetype i = 0; i < s.size(); ++i) {
        if (isEscape(s, i)) {
            out.append(static_cast<char>(hexValue(s[i + 1]) << 4 | hexValue(s[i + 2])));
            i += 2;
        } else {
            out.append(s[i]);
        }
    }
    return out;
}

// Pretty form: readable, but anything that would change the meaning stays escaped.
QString prettyDecode(const QByteArray &s) {
    QByteArray out;
    for (qsizetype i = 0; i < s.size(); ++i) {
        if (isEscape(s, i)) {
            const uchar b = static_cast<uchar>(hexValue(s[i + 1]) << 4 | hexValue(s[i + 2]));
            if (b >= 0x80 || b == ' ' || isUnreservedChar(static_cast<char>(b))) {
                out.append(static_cast<char>(b));
            } else {
                out.append(s.mid(i, 3));
            }
            i += 2;
        } else {
            out.append(s[i]);
        }
    }
    return QString::fromUtf8(out);
}

QString present(const QByteArray &encoded, QUrl::ComponentFormattingOptions options) {
    if (options.testFlag(QUrl::FullyEncoded))
        return QString::fromUtf8(encoded);
    if (options.testFlag(QUrl::FullyDecoded))
        return QString::fromUtf8(decodeBytes(encoded));
    return prettyDecode(encoded);
}

// Rejects characters that can never appear in strict input.
bool strictlyValid(const QByteArray &s, Component c) {
    const char *allowed = allowedIn(c);
    for (qsizetype i = 0; i < s.size(); ++i) {
        const char ch = s[i];
        if (ch == '%') {
            if (!isEscape(s, i))
                return false;
            i += 2;
        } else if (!isUnreservedChar(ch) && !std::strchr(allowed, ch)) {
            return false;
        }
    }
    return true;
}

// RFC 3986 section 5.2.4.
QByteArray removeDotSegments(QByteArray in) {
    QByteArray out;
    while (!in.isEmpty()) {
        if (in.startsWith("../")) {
            in = in.mid(3);
        } else if (in.startsWith("./") || in.startsWith("/./")) {
            in = in.mid(2);
        } else if (in == "/.") {
            in = "/";
        } else if (in.startsWith("/../") || in == "/..") {
            in = in == "/.." ? QByteArray("/") : in.mid(3);
            const qsizetype slash = out.lastIndexOf('/');
            out.truncate(slash < 0 ? 0 : slash);
        } else if (in == "." || in == "..") {
            in.clear();
        } else {
            qsizetype next = in.indexOf('/', in.startsWith("/") ? 1 : 0);
            if (next < 0)
                next = in.size();
            out.append(in.left(next));
            in = in.mid(next);
        }
    }
    return out;
}

bool validScheme(const QString &s) {
    if (s.isEmpty() || !isAlpha(static_cast<char>(s[0].unicode() < 128 ? s[0].unicode() : 0)))
        return false;
    for (QChar c : s) {
        const char16_t u = c.unicode();
        const char b = u < 128 ? static_cast<char>(u) : '\0';
        if (!isAlpha(b) && !isDigit(b) && b != '+' && b != '-' && b != '.')
            return false;
    }
    return true;
}

bool validHost(const QString &host) {
    for (QChar c : host) {
        const char16_t u = c.unicode();
        if (u <= 0x20 || u == 0x7f)
            return false;
        if (u < 128 && std::strchr("#%/:<>?@[\\]^`{|}", static_cast<char>(u)))
            return false;
    }
    return true;
}

bool validIpLiteral(const QString &inner) {
    if (inner.isEmpty() || !inner.contains(QChar(u':')))
        return false;
    for (QChar c : inner) {
        const char16_t u = c.unicode();
        if (u >= 128 || hexValue(static_cast<char>(u)) < 0) {
            if (u != u':' && u != u'.')
                return false;
        }
    }
    return true;
}

// Punycode (RFC 3492): the encoding of a Unicode label in the letters, digits and hyphen that a
// host name may contain. The parameters are those of section 5.
constexpr quint32 kPunyBase = 36;
constexpr quint32 kPunyTMin = 1;
constexpr quint32 kPunyTMax = 26;
constexpr quint32 kPunySkew = 38;
constexpr quint32 kPunyDamp = 700;
constexpr quint32 kPunyInitialBias = 72;
constexpr quint32 kPunyInitialN = 128;
constexpr quint32 kPunyMax = std::numeric_limits<quint32>::max();

quint32 punyAdapt(quint32 delta, quint32 points, bool first) {
    delta = first ? delta / kPunyDamp : delta / 2;
    delta += delta / points;
    quint32 k = 0;
    while (delta > ((kPunyBase - kPunyTMin) * kPunyTMax) / 2) {
        delta /= kPunyBase - kPunyTMin;
        k += kPunyBase;
    }
    return k + (kPunyBase - kPunyTMin + 1) * delta / (delta + kPunySkew);
}

quint32 punyThreshold(quint32 k, quint32 bias) {
    if (k <= bias)
        return kPunyTMin;
    return k >= bias + kPunyTMax ? kPunyTMax : k - bias;
}

char punyDigit(quint32 d) { return static_cast<char>(d < 26 ? 'a' + d : '0' + (d - 26)); }

int punyValue(char c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a';
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= '0' && c <= '9')
        return c - '0' + 26;
    return -1;
}

// Encodes the code points of a label; returns false if a number overflows.
bool punyEncode(const std::vector<char32_t> &input, QByteArray &out) {
    out.clear();
    for (char32_t c : input)
        if (c < 0x80)
            out.append(static_cast<char>(c));
    const auto basic = static_cast<quint32>(out.size());
    quint32 handled = basic;
    if (basic > 0)
        out.append('-');

    quint32 n = kPunyInitialN;
    quint32 delta = 0;
    quint32 bias = kPunyInitialBias;
    while (handled < input.size()) {
        quint32 next = kPunyMax;
        for (char32_t c : input)
            if (c >= n && c < next)
                next = c;
        if (next - n > (kPunyMax - delta) / (handled + 1))
            return false;
        delta += (next - n) * (handled + 1);
        n = next;
        for (char32_t c : input) {
            if (c < n && ++delta == 0)
                return false;
            if (c != n)
                continue;
            quint32 q = delta;
            for (quint32 k = kPunyBase;; k += kPunyBase) {
                const quint32 t = punyThreshold(k, bias);
                if (q < t)
                    break;
                out.append(punyDigit(t + (q - t) % (kPunyBase - t)));
                q = (q - t) / (kPunyBase - t);
            }
            out.append(punyDigit(q));
            bias = punyAdapt(delta, handled + 1, handled == basic);
            delta = 0;
            ++handled;
        }
        ++delta;
        ++n;
    }
    return true;
}

// Decodes a label (without its "xn--" prefix); returns false if it is not valid Punycode.
bool punyDecode(const QByteArray &input, std::vector<char32_t> &out) {
    out.clear();
    const qsizetype dash = input.lastIndexOf('-');
    for (qsizetype i = 0; i < dash; ++i) {
        if (static_cast<uchar>(input[i]) >= 0x80)
            return false;
        out.push_back(static_cast<char32_t>(input[i]));
    }
    quint32 n = kPunyInitialN;
    quint32 i = 0;
    quint32 bias = kPunyInitialBias;
    qsizetype in = dash > 0 ? dash + 1 : 0;
    while (in < input.size()) {
        const quint32 old = i;
        quint32 w = 1;
        for (quint32 k = kPunyBase;; k += kPunyBase) {
            if (in >= input.size())
                return false;
            const int value = punyValue(input[in++]);
            if (value < 0)
                return false;
            const auto digit = static_cast<quint32>(value);
            if (digit > (kPunyMax - i) / w)
                return false;
            i += digit * w;
            const quint32 t = punyThreshold(k, bias);
            if (digit < t)
                break;
            if (w > kPunyMax / (kPunyBase - t))
                return false;
            w *= kPunyBase - t;
        }
        const auto count = static_cast<quint32>(out.size()) + 1;
        bias = punyAdapt(i - old, count, old == 0);
        if (i / count > kPunyMax - n)
            return false;
        n += i / count;
        i %= count;
        if (n > 0x10ffff || (n >= 0xd800 && n <= 0xdfff))
            return false;
        out.insert(out.begin() + static_cast<std::ptrdiff_t>(i), static_cast<char32_t>(n));
        ++i;
    }
    return true;
}

constexpr qsizetype kMaxLabel = 63;

bool isAsciiText(const QString &s) {
    for (QChar c : s)
        if (c.unicode() >= 0x80)
            return false;
    return true;
}

std::vector<char32_t> codePoints(const QString &s) {
    std::vector<char32_t> out;
    const QList<uint> ucs4 = s.toUcs4();
    out.reserve(static_cast<std::size_t>(ucs4.size()));
    for (uint c : ucs4)
        out.push_back(static_cast<char32_t>(c));
    return out;
}

} // namespace

/*!
    \class QUrl
    \brief The QUrl class parses, builds and resolves URLs.

    Parse a URL from text, or build one a component at a time:

    \code
    QUrl url("https://user@example.com:8080/a%20b?q=1#top");
    url.host();                // "example.com"
    url.port();                // 8080
    url.path();                // "/a b"

    QUrl built;
    built.setScheme("http");
    built.setHost("example.com");
    built.setPath("/a b");
    built.toString();          // "http://example.com/a b"
    built.toEncoded();         // "http://example.com/a%20b"
    \endcode

    Use resolved() to combine a base URL with a relative reference, following
    RFC 3986.

    \differences
    A host name is stored as it was given, in lower case: one in ACE form
    (\c{xn--}) is not turned into Unicode for display, so call fromAce() on it.
    The encoded forms always use the ACE form. toAce() and fromAce() apply
    Punycode but not the full UTS #46 mapping tables: labels are lower-cased,
    not normalized or validated, and there is no IDN whitelist, so
    \c setIdnWhitelist() and \c idnWhitelist() do not exist and
    \c IgnoreIDNWhitelist has no effect. The individual delimiter-decoding
    flags of \c ComponentFormattingOption are absent. fromUserInput() does not
    recognise Windows drive paths.
    \enddifferences

    \sa QUrlQuery
*/

/*!
    \enum QUrl::ParsingMode

    Selects how input text is interpreted.

    \value TolerantMode Fixes common mistakes, escaping characters that are not allowed.
    \value StrictMode Rejects anything that is not a valid URL.
    \value DecodedMode Treats the input as plain text and escapes it, so \c % is literal.
*/

/*!
    \enum QUrl::UrlFormattingOption

    Selects what toString() and adjusted() leave out or change.

    \value None The URL as it is.
    \value RemoveScheme Leaves out the scheme.
    \value RemovePassword Leaves out the password.
    \value RemoveUserInfo Leaves out the user name and password.
    \value RemovePort Leaves out the port.
    \value RemoveAuthority Leaves out the whole authority.
    \value RemovePath Leaves out the path.
    \value RemoveQuery Leaves out the query.
    \value RemoveFragment Leaves out the fragment.
    \value PreferLocalFile Makes toString() return the local file path if the URL is a
           local file with no query and no fragment.
    \value RemoveFilename Leaves out the last segment of the path, keeping the slash before it.
    \value StripTrailingSlash Removes a trailing slash from the path.
    \value NormalizePathSegments Removes \c . and \c .. segments from the path.
*/

/*!
    \enum QUrl::ComponentFormattingOption

    Selects how a component is returned.

    \value PrettyDecoded Readable, keeping escapes that would change the meaning.
    \value FullyEncoded Percent-encoded, as on the wire.
    \value FullyDecoded Every escape decoded.
*/

/*!
    \enum QUrl::UserInputResolutionOption

    Selects how fromUserInput() treats text that could be a file name.

    \value DefaultResolution Text that is not an existing file is taken as a host name.
    \value AssumeLocalFile Text without a scheme is taken as a file in the working directory,
           even if no such file exists.
*/

/*!
    \enum QUrl::AceProcessingOption

    Options for toAce() and fromAce().

    \value IgnoreIDNWhitelist Accepted for compatibility; Nt has no IDN whitelist.
    \value AceTransitionalProcessing Maps \c ß to \c ss and final sigma to sigma, and removes
           zero-width joiners, as registries did before IDNA 2008.
*/

/*!
    Constructs a URL by parsing \a url using \a mode.

    \sa setUrl(), isValid()
*/
QUrl::QUrl(const QString &url, ParsingMode mode) { parse(url, mode); }

/*!
    Parses \a url using \a mode, replacing the current contents.

    \sa isValid(), errorString()
*/
void QUrl::setUrl(const QString &url, ParsingMode mode) {
    *this = QUrl();
    parse(url, mode);
}

void QUrl::fail(const QString &message) {
    m_valid = false;
    if (m_error.isEmpty())
        m_error = message;
}

void QUrl::parse(const QString &text, ParsingMode mode) {
    if (mode == DecodedMode) {
        fail(QString("DecodedMode is not allowed for a whole URL"));
        return;
    }
    QString rest = mode == TolerantMode ? text.trimmed() : text;
    if (rest.isEmpty())
        return;

    const qsizetype hash = rest.indexOf(QChar(u'#'));
    if (hash >= 0) {
        setFragment(rest.mid(hash + 1), mode);
        rest.truncate(hash);
    }
    const qsizetype question = rest.indexOf(QChar(u'?'));
    if (question >= 0) {
        setQuery(rest.mid(question + 1), mode);
        rest.truncate(question);
    }
    // A scheme ends at the first ':' that comes before any '/'.
    const qsizetype colon = rest.indexOf(QChar(u':'));
    const qsizetype slash = rest.indexOf(QChar(u'/'));
    if (colon > 0 && (slash < 0 || colon < slash)) {
        const QString candidate = rest.left(colon);
        if (validScheme(candidate)) {
            m_scheme = candidate.toLower();
            rest = rest.mid(colon + 1);
        } else if (mode == StrictMode) {
            fail(QString("Invalid scheme"));
            return;
        }
    }
    if (rest.startsWith(QString("//"))) {
        qsizetype end = rest.indexOf(QChar(u'/'), 2);
        if (end < 0)
            end = rest.size();
        setAuthority(rest.mid(2, end - 2), mode);
        rest = rest.mid(end);
    }
    setPath(rest, mode);
}

/*!
    Returns the URL as text, with \a options applied. Characters are shown
    readably where that does not change the meaning.

    \sa toEncoded()
*/
QString QUrl::toString(FormattingOptions options) const {
    if (options.testFlag(PreferLocalFile) && isLocalFile() && !m_hasQuery && !m_hasFragment)
        return adjusted(options).toLocalFile();
    QString r;
    const QUrl u = adjusted(options);
    if (!u.m_scheme.isEmpty() && !options.testFlag(RemoveScheme))
        r += u.m_scheme + QChar(u':');
    if (u.m_hasHost && (int(options) & 0x10) == 0)
        r += QString("//") + u.authority(PrettyDecoded);
    if (!options.testFlag(RemovePath))
        r += prettyDecode(u.m_path);
    if (u.m_hasQuery && !options.testFlag(RemoveQuery))
        r += QChar(u'?') + prettyDecode(u.m_query);
    if (u.m_hasFragment && !options.testFlag(RemoveFragment))
        r += QChar(u'#') + prettyDecode(u.m_fragment);
    return r;
}

/*!
    Returns the URL percent-encoded, with \a options applied.

    \sa fromEncoded(), toString()
*/
QByteArray QUrl::toEncoded(FormattingOptions options) const {
    QByteArray r;
    const QUrl u = adjusted(options);
    if (!u.m_scheme.isEmpty() && !options.testFlag(RemoveScheme))
        r += u.m_scheme.toUtf8() + ':';
    if (u.m_hasHost && (int(options) & 0x10) == 0)
        r += "//" + u.encodedAuthority();
    if (!options.testFlag(RemovePath))
        r += u.m_path;
    if (u.m_hasQuery && !options.testFlag(RemoveQuery))
        r += '?' + u.m_query;
    if (u.m_hasFragment && !options.testFlag(RemoveFragment))
        r += '#' + u.m_fragment;
    return r;
}

/*!
    Returns a copy of the URL with \a options applied to its components.

    \sa toString()
*/
QUrl QUrl::adjusted(FormattingOptions options) const {
    QUrl u = *this;
    if (options.testFlag(RemoveScheme))
        u.m_scheme.clear();
    if ((int(options) & 0x10) != 0) {
        u.m_hasHost = false;
        u.m_host.clear();
    }
    if ((int(options) & 0x4) != 0) {
        u.m_hasUser = false;
        u.m_user.clear();
    }
    if (options.testFlag(RemovePassword)) {
        u.m_hasPassword = false;
        u.m_password.clear();
    }
    if (options.testFlag(RemovePort))
        u.m_port = -1;
    if (options.testFlag(RemovePath))
        u.m_path.clear();
    if (options.testFlag(RemoveQuery)) {
        u.m_hasQuery = false;
        u.m_query.clear();
    }
    if (options.testFlag(RemoveFragment)) {
        u.m_hasFragment = false;
        u.m_fragment.clear();
    }
    if (options.testFlag(RemoveFilename))
        u.m_path.truncate(u.m_path.lastIndexOf('/') + 1);
    if (options.testFlag(NormalizePathSegments))
        u.m_path = removeDotSegments(u.m_path);
    if (options.testFlag(StripTrailingSlash)) {
        while (u.m_path.size() > 1 && u.m_path.endsWith('/'))
            u.m_path.chop(1);
    }
    return u;
}

/*!
    Returns \c true if the URL has no components.

    \sa clear(), isValid()
*/
bool QUrl::isEmpty() const {
    return m_scheme.isEmpty() && !m_hasHost && m_path.isEmpty() && !m_hasQuery && !m_hasFragment;
}

/*!
    Sets the scheme to \a scheme, which is stored in lower case. An invalid
    scheme makes the URL invalid.

    \sa scheme()
*/
void QUrl::setScheme(const QString &scheme) {
    if (!scheme.isEmpty() && !validScheme(scheme)) {
        fail(QString("Invalid scheme"));
        return;
    }
    m_scheme = scheme.toLower();
}

/*!
    Parses \a authority, which has the form
    \c{[user[:password]@]host[:port]}, using \a mode.

    \sa authority()
*/
void QUrl::setAuthority(const QString &authority, ParsingMode mode) {
    m_hasHost = true;
    m_host.clear();
    m_port = -1;
    m_hasUser = m_hasPassword = false;
    m_user.clear();
    m_password.clear();

    QString rest = authority;
    const qsizetype at = rest.lastIndexOf(QChar(u'@'));
    if (at >= 0) {
        setUserInfo(rest.left(at), mode);
        rest = rest.mid(at + 1);
    }
    const qsizetype bracket = rest.lastIndexOf(QChar(u']'));
    const qsizetype colon = rest.lastIndexOf(QChar(u':'));
    if (colon > bracket) {
        const QString portText = rest.mid(colon + 1);
        rest.truncate(colon);
        if (!portText.isEmpty()) {
            bool ok = false;
            const int p = portText.toInt(&ok);
            bool digits = ok;
            for (QChar c : portText)
                digits = digits && QtPrivate::isAsciiDigit(c);
            if (digits && p <= 65535)
                m_port = p;
            else
                fail(QString("Invalid port"));
        }
    }
    setHost(rest, mode == DecodedMode ? DecodedMode : mode);
}

QByteArray QUrl::encodedAuthority(bool aceHost) const {
    QByteArray r;
    if (m_hasUser) {
        r += m_user;
        if (m_hasPassword)
            r += ':' + m_password;
        r += '@';
    }
    const QByteArray ace = aceHost && !isAsciiText(m_host) ? toAce(m_host) : QByteArray();
    r += ace.isEmpty() ? m_host.toUtf8() : ace;
    if (m_port >= 0)
        r += ':' + QByteArray::number(m_port);
    return r;
}

/*!
    Returns the authority, formatted according to \a options.

    \sa setAuthority()
*/
QString QUrl::authority(ComponentFormattingOptions options) const {
    return present(encodedAuthority(options.testFlag(FullyEncoded)), options);
}

/*!
    Sets the user name and password from \a userInfo, which has the form
    \c{user[:password]}, interpreted using \a mode.

    \sa userInfo()
*/
void QUrl::setUserInfo(const QString &userInfo, ParsingMode mode) {
    const qsizetype colon = userInfo.indexOf(QChar(u':'));
    if (colon >= 0) {
        setUserName(userInfo.left(colon), mode);
        setPassword(userInfo.mid(colon + 1), mode);
    } else {
        setUserName(userInfo, mode);
        m_hasPassword = false;
        m_password.clear();
    }
}

/*!
    Returns the user name and password, formatted according to \a options.

    \sa setUserInfo()
*/
QString QUrl::userInfo(ComponentFormattingOptions options) const {
    QString r = present(m_user, options);
    if (m_hasPassword)
        r += QChar(u':') + present(m_password, options);
    return r;
}

/*!
    Sets the user name to \a userName, interpreted using \a mode.

    \sa userName()
*/
void QUrl::setUserName(const QString &userName, ParsingMode mode) {
    const QByteArray utf8 = userName.toUtf8();
    if (mode == StrictMode && !strictlyValid(utf8, UserComponent))
        fail(QString("Invalid user name"));
    m_user = encodeComponent(utf8, UserComponent, mode != DecodedMode);
    m_hasUser = !userName.isNull();
    if (m_hasUser && !m_hasHost)
        m_hasHost = true;
}

/*!
    Returns the user name, formatted according to \a options.

    \sa setUserName()
*/
QString QUrl::userName(ComponentFormattingOptions options) const { return present(m_user, options); }

/*!
    Sets the password to \a password, interpreted using \a mode.

    \sa password()
*/
void QUrl::setPassword(const QString &password, ParsingMode mode) {
    const QByteArray utf8 = password.toUtf8();
    if (mode == StrictMode && !strictlyValid(utf8, UserComponent))
        fail(QString("Invalid password"));
    m_password = encodeComponent(utf8, UserComponent, mode != DecodedMode);
    m_hasPassword = !password.isNull();
}

/*!
    Returns the password, formatted according to \a options.

    \sa setPassword()
*/
QString QUrl::password(ComponentFormattingOptions options) const { return present(m_password, options); }

/*!
    Sets the host to \a host, which is stored in lower case. IPv6 addresses may
    be given with or without brackets. \a mode is accepted for compatibility:
    a host is checked the same way in every mode.

    \sa host()
*/
void QUrl::setHost(const QString &host, ParsingMode mode) {
    Q_UNUSED(mode)
    m_hasHost = true;
    QString h = host.toLower();
    if (h.startsWith(QChar(u'[')) && h.endsWith(QChar(u']')))
        h = h.mid(1, h.size() - 2);
    if (h.contains(QChar(u':'))) {
        if (!validIpLiteral(h)) {
            fail(QString("Invalid IPv6 address"));
            return;
        }
        m_host = QChar(u'[') + h + QChar(u']');
        return;
    }
    if (!validHost(h)) {
        fail(QString("Invalid hostname"));
        return;
    }
    m_host = h;
}

/*!
    Returns the host. An IPv6 address is returned in brackets, unless \a options
    is FullyDecoded. With FullyEncoded, an internationalized host name is returned
    in its ACE form, as it is used in DNS.

    \sa setHost(), toAce()
*/
QString QUrl::host(ComponentFormattingOptions options) const {
    if (options.testFlag(FullyDecoded) && m_host.startsWith(QChar(u'[')))
        return m_host.mid(1, m_host.size() - 2);
    if (options.testFlag(FullyEncoded) && !isAsciiText(m_host)) {
        const QByteArray ace = toAce(m_host);
        if (!ace.isEmpty())
            return QString::fromLatin1(ace);
    }
    return m_host;
}

/*!
    Sets the port to \a port; \c -1 unsets it. A value outside 0 to 65535 makes
    the URL invalid.

    \sa port()
*/
void QUrl::setPort(int port) {
    if (port < -1 || port > 65535) {
        fail(QString("Invalid port"));
        return;
    }
    m_port = port;
    if (port >= 0)
        m_hasHost = true;
}

/*!
    \fn int QUrl::port(int defaultPort) const

    Returns the port, or \a defaultPort if none is set.

    \sa setPort()
*/

/*!
    Sets the path to \a path, interpreted using \a mode. By default \c % is
    literal; use TolerantMode to pass an already encoded path.

    \sa path()
*/
void QUrl::setPath(const QString &path, ParsingMode mode) {
    const QByteArray utf8 = path.toUtf8();
    if (mode == StrictMode && !strictlyValid(utf8, PathComponent))
        fail(QString("Invalid path"));
    m_path = encodeComponent(utf8, PathComponent, mode != DecodedMode);
    if (m_hasHost && !m_path.isEmpty() && !m_path.startsWith('/'))
        fail(QString("Path must start with a slash when there is an authority"));
}

/*!
    Returns the path, formatted according to \a options.

    \sa setPath(), fileName()
*/
QString QUrl::path(ComponentFormattingOptions options) const { return present(m_path, options); }

/*!
    Returns the last segment of the path, formatted according to \a options. It
    is empty if the path ends with a slash.

    \sa path()
*/
QString QUrl::fileName(ComponentFormattingOptions options) const {
    return present(m_path.mid(m_path.lastIndexOf('/') + 1), options);
}

/*!
    Sets the query to \a query, interpreted using \a mode. A null string removes
    the query; an empty one keeps a bare \c ?.

    \sa query(), QUrlQuery
*/
void QUrl::setQuery(const QString &query, ParsingMode mode) {
    m_hasQuery = !query.isNull();
    const QByteArray utf8 = query.toUtf8();
    if (mode == StrictMode && !strictlyValid(utf8, QueryComponent))
        fail(QString("Invalid query"));
    m_query = encodeComponent(utf8, QueryComponent, mode != DecodedMode);
}

/*!
    Returns the query, formatted according to \a options.

    \sa setQuery(), hasQuery()
*/
QString QUrl::query(ComponentFormattingOptions options) const { return present(m_query, options); }

/*!
    Sets the fragment to \a fragment, interpreted using \a mode. A null string
    removes it.

    \sa fragment()
*/
void QUrl::setFragment(const QString &fragment, ParsingMode mode) {
    m_hasFragment = !fragment.isNull();
    const QByteArray utf8 = fragment.toUtf8();
    if (mode == StrictMode && !strictlyValid(utf8, FragmentComponent))
        fail(QString("Invalid fragment"));
    m_fragment = encodeComponent(utf8, FragmentComponent, mode != DecodedMode);
}

/*!
    Returns the fragment, formatted according to \a options.

    \sa setFragment(), hasFragment()
*/
QString QUrl::fragment(ComponentFormattingOptions options) const { return present(m_fragment, options); }

/*!
    Returns \a relative resolved against this URL, following RFC 3986.

    \code
    QUrl base("http://a/b/c/d");
    base.resolved(QUrl("../e"));    // "http://a/b/e"
    \endcode
*/
QUrl QUrl::resolved(const QUrl &relative) const {
    if (!isValid())
        return relative;
    if (!relative.m_valid)
        return QUrl();
    QUrl t = relative;
    if (!relative.m_scheme.isEmpty()) {
        t.m_path = removeDotSegments(relative.m_path);
        return t;
    }
    t.m_scheme = m_scheme;
    if (relative.m_hasHost) {
        t.m_path = removeDotSegments(relative.m_path);
        return t;
    }
    t.m_hasHost = m_hasHost;
    t.m_host = m_host;
    t.m_port = m_port;
    t.m_hasUser = m_hasUser;
    t.m_user = m_user;
    t.m_hasPassword = m_hasPassword;
    t.m_password = m_password;
    if (relative.m_path.isEmpty()) {
        t.m_path = m_path;
        if (!relative.m_hasQuery) {
            t.m_hasQuery = m_hasQuery;
            t.m_query = m_query;
        }
        return t;
    }
    if (relative.m_path.startsWith('/')) {
        t.m_path = removeDotSegments(relative.m_path);
        return t;
    }
    QByteArray merged;
    if (m_hasHost && m_path.isEmpty())
        merged = "/" + relative.m_path;
    else
        merged = m_path.left(m_path.lastIndexOf('/') + 1) + relative.m_path;
    t.m_path = removeDotSegments(merged);
    return t;
}

/*!
    Returns \c true if this URL is a parent of \a url: the same scheme and
    authority, and a path that the path of \a url extends.
*/
bool QUrl::isParentOf(const QUrl &url) const {
    if (m_scheme != url.m_scheme || encodedAuthority() != url.encodedAuthority())
        return false;
    QByteArray parent = m_path;
    if (!parent.endsWith('/'))
        parent.append('/');
    return url.m_path.size() > parent.size() && url.m_path.startsWith(parent);
}

/*!
    Returns the local file path of a \c file: URL, or an empty string for any
    other URL.

    \sa fromLocalFile(), isLocalFile()
*/
QString QUrl::toLocalFile() const { return isLocalFile() ? path(FullyDecoded) : QString(); }

/*!
    Returns a \c file: URL for the path \a localFile.

    \sa toLocalFile()
*/
QUrl QUrl::fromLocalFile(const QString &localFile) {
    QUrl u;
    if (localFile.isEmpty())
        return u;
    u.setScheme(QString("file"));
    u.m_hasHost = localFile.startsWith(QChar(u'/'));
    u.setPath(localFile, DecodedMode);
    return u;
}

/*!
    Returns \c true if this URL and \a url are the same once \a options have been applied to
    both; otherwise returns \c false. This compares two URLs while ignoring, for example, the
    fragment or a trailing slash:

    \code
    QUrl a("http://example.com/docs/#intro");
    QUrl b("http://example.com/docs");
    a.matches(b, QUrl::RemoveFragment | QUrl::StripTrailingSlash);   // true
    \endcode

    \since 5.2
    \sa adjusted()
*/
bool QUrl::matches(const QUrl &url, FormattingOptions options) const {
    const FormattingOptions applied = options & ~FormattingOptions(PreferLocalFile);
    return toEncoded(applied) == url.toEncoded(applied);
}

/*!
    \fn void QUrl::swap(QUrl &other)

    Swaps this URL with \a other.

    \since 5.0
*/

/*!
    Returns a URL for text that a person typed, such as the contents of an address bar. The
    text \a userInput is trimmed, and then:

    \list
    \li an absolute path, or the name of a file that exists in \a workingDirectory, gives a
        \c file: URL;
    \li text that is a URL with a scheme is parsed as it is;
    \li anything else is taken as a host name, with an optional port and path, and gets the
        scheme \c http, or \c ftp if the host name starts with \c{ftp.}.
    \endlist

    With \c AssumeLocalFile in \a options, text without a scheme is resolved against
    \a workingDirectory even if no such file exists. Returns an invalid URL if nothing can be
    made of the text.

    \code
    QUrl::fromUserInput("example.com/index.html");   // http://example.com/index.html
    QUrl::fromUserInput("localhost:8080");            // http://localhost:8080
    QUrl::fromUserInput("/etc/hosts");                // file:///etc/hosts
    \endcode

    \since 5.4
    \sa fromLocalFile()
*/
QUrl QUrl::fromUserInput(const QString &userInput, const QString &workingDirectory,
                         UserInputResolutionOptions options) {
    const QString trimmed = userInput.trimmed();
    if (trimmed.isEmpty())
        return QUrl();
    // An IPv6 address has at least two colons, which a host name with a port does not.
    if (trimmed.count(QChar(u':')) >= 2 && validIpLiteral(trimmed.toLower())) {
        QUrl address;
        address.setScheme(QString("http"));
        address.setHost(trimmed);
        return address;
    }
    if (QDir::isAbsolutePath(trimmed))
        return fromLocalFile(trimmed);

    QUrl url(trimmed, TolerantMode);
    if (!workingDirectory.isEmpty()) {
        const QFileInfo file(QDir(workingDirectory), trimmed);
        if (file.exists() || (options.testFlag(AssumeLocalFile) && url.isRelative()))
            return fromLocalFile(file.absoluteFilePath());
    }

    // "host:8080" parses as a URL with the scheme "host", so see what it is as a host name too.
    QUrl prepended(QString("http://") + trimmed, TolerantMode);
    if (url.isValid() && !url.isRelative() && prepended.port() == -1)
        return url;
    if (prepended.isValid() && !prepended.host().isEmpty()) {
        if (prepended.host().startsWith(QString("ftp.")))
            prepended.setScheme(QString("ftp"));
        return prepended;
    }
    return QUrl();
}

/*!
    Returns the ASCII Compatible Encoding of \a domain: each label that has characters outside
    ASCII is lower-cased and replaced by \c{xn--} followed by its Punycode form (RFC 3492). This
    is the form of an internationalized domain name that DNS uses. \a options adjusts the
    mapping. Returns an empty byte array if a label cannot be encoded or is longer than 63
    characters.

    \code
    QUrl::toAce("bücher.example");   // "xn--bcher-kva.example"
    \endcode

    \since 4.2
    \sa fromAce()
*/
QByteArray QUrl::toAce(const QString &domain, AceProcessingOptions options) {
    QString text = domain.toLower();
    // The other full stops that separate labels.
    for (const char16_t dot : {u'\u3002', u'\uff0e', u'\uff61'})
        text.replace(QChar(dot), QChar(u'.'));
    if (options.testFlag(AceTransitionalProcessing)) {
        text.replace(QChar(u'\u00df'), QString("ss"));
        text.replace(QChar(u'\u03c2'), QChar(u'\u03c3'));
        text.remove(QChar(u'\u200c'));
        text.remove(QChar(u'\u200d'));
    }
    QByteArray result;
    const QStringList labels = text.split(QChar(u'.'));
    for (qsizetype i = 0; i < labels.size(); ++i) {
        const QString &label = labels.at(i);
        // Only the last label may be empty: the root of a fully qualified name.
        if (label.isEmpty() && (i + 1 < labels.size() || labels.size() == 1))
            return QByteArray();
        QByteArray encoded;
        if (isAsciiText(label)) {
            encoded = label.toLatin1();
        } else {
            if (!punyEncode(codePoints(label), encoded))
                return QByteArray();
            encoded.prepend("xn--");
        }
        if (encoded.size() > kMaxLabel)
            return QByteArray();
        if (i > 0)
            result.append('.');
        result.append(encoded);
    }
    return result;
}

/*!
    Returns the Unicode form of the ACE-encoded \a domain: each label that starts with
    \c{xn--} and is valid Punycode is decoded, and every other label is kept, in lower case.
    \a options is accepted for compatibility.

    \code
    QUrl::fromAce("xn--bcher-kva.example");   // "bücher.example"
    \endcode

    \since 4.2
    \sa toAce()
*/
QString QUrl::fromAce(const QByteArray &domain, AceProcessingOptions options) {
    Q_UNUSED(options)
    QString result;
    const QList<QByteArray> labels = domain.toLower().split('.');
    for (qsizetype i = 0; i < labels.size(); ++i) {
        const QByteArray &label = labels.at(i);
        if (i > 0)
            result.append(QChar(u'.'));
        std::vector<char32_t> decoded;
        QByteArray again;
        // A label is decoded only if it has a character to decode, and encoding the result gives
        // the same label back.
        const auto beyondAscii = [](char32_t c) { return c >= 0x80; };
        if (label.startsWith("xn--") && punyDecode(label.mid(4), decoded) &&
            std::any_of(decoded.begin(), decoded.end(), beyondAscii) && punyEncode(decoded, again) &&
            again == label.mid(4)) {
            result.append(QString::fromUcs4(decoded.data(), static_cast<qsizetype>(decoded.size())));
        } else {
            result.append(QString::fromLatin1(label));
        }
    }
    return result;
}

/*!
    Returns the URLs in \a urls as strings, each formatted with \a options as toString() does.

    \since 5.1
    \sa fromStringList(), toString()
*/
QStringList QUrl::toStringList(const QList<QUrl> &urls, FormattingOptions options) {
    QStringList result;
    result.reserve(urls.size());
    for (const QUrl &url : urls)
        result.append(url.toString(options));
    return result;
}

/*!
    Returns the strings in \a urls as URLs, each parsed using \a mode.

    \since 5.1
    \sa toStringList()
*/
QList<QUrl> QUrl::fromStringList(const QStringList &urls, ParsingMode mode) {
    QList<QUrl> result;
    result.reserve(urls.size());
    for (const QString &url : urls)
        result.append(QUrl(url, mode));
    return result;
}

/*!
    Returns a URL parsed from the percent-encoded \a input using \a mode.

    \sa toEncoded()
*/
QUrl QUrl::fromEncoded(const QByteArray &input, ParsingMode mode) { return QUrl(QString::fromUtf8(input), mode); }

/*!
    Returns \a input as percent-encoded UTF-8. Unreserved characters stay as
    they are, as do bytes listed in \a exclude. Bytes listed in \a include are
    encoded even if they are unreserved.

    \sa fromPercentEncoding()
*/
QByteArray QUrl::toPercentEncoding(const QString &input, const QByteArray &exclude, const QByteArray &include) {
    const QByteArray utf8 = input.toUtf8();
    QByteArray out;
    for (char c : utf8) {
        if ((isUnreservedChar(c) && !include.contains(c)) || exclude.contains(c))
            out.append(c);
        else
            appendEscaped(out, static_cast<uchar>(c));
    }
    return out;
}

/*!
    Returns the text of the percent-encoded \a input, decoded as UTF-8.

    \sa toPercentEncoding()
*/
QString QUrl::fromPercentEncoding(const QByteArray &input) { return QString::fromUtf8(decodeBytes(input)); }

// Hashes the encoded form, so equal URLs hash equally.
std::size_t qHash(const QUrl &url, std::size_t seed) noexcept { return qHash(url.toEncoded(), seed); }

/*!
    \fn QUrl::QUrl()

    Constructs an empty URL.
*/

/*!
    \fn QString QUrl::url(FormattingOptions options) const

    Returns the URL as text, with \a options applied. This is the same as
    toString().
*/

/*!
    \fn QString QUrl::toDisplayString(FormattingOptions options) const

    Returns the URL as text for showing to a person, with \a options applied.
    The password is left out by default.

    \sa toString()
*/

/*!
    \fn bool QUrl::isValid() const

    Returns \c true if the URL is not empty and parsed without an error.

    \sa errorString(), isEmpty()
*/

/*!
    \fn bool QUrl::isRelative() const

    Returns \c true if the URL has no scheme.

    \sa resolved()
*/

/*!
    \fn bool QUrl::isLocalFile() const

    Returns \c true if the URL has the \c file scheme.

    \sa toLocalFile()
*/

/*!
    \fn void QUrl::clear()

    Resets the URL to the empty state.

    \sa isEmpty()
*/

/*!
    \fn QString QUrl::errorString() const

    Returns a description of the first parsing error, or an empty string if
    there is none.

    \sa isValid()
*/

/*!
    \fn QString QUrl::scheme() const

    Returns the scheme in lower case, or an empty string if there is none.

    \sa setScheme()
*/

/*!
    \fn bool QUrl::hasQuery() const

    Returns \c true if the URL has a query, even an empty one.

    \sa query()
*/

/*!
    \fn bool QUrl::hasFragment() const

    Returns \c true if the URL has a fragment, even an empty one.

    \sa fragment()
*/

/*!
    \class QUrlQuery
    \brief The QUrlQuery class reads and builds the key-value pairs of a URL query.

    \code
    QUrlQuery query;
    query.addQueryItem("q", "a b");
    query.addQueryItem("page", "2");

    QUrl url("http://example.com/search");
    url.setQuery(query.query(QUrl::FullyEncoded));
    url.toEncoded();    // "http://example.com/search?q=a%20b&page=2"
    \endcode

    Pairs are separated by \c & and keys from values by \c = unless
    setQueryDelimiters() chooses other characters, such as \c ; for queries
    in the older HTML style.

    \differences
    Items are \c std::pair rather than \c QPair. The delimiters must be
    ASCII characters.
    \enddifferences

    \sa QUrl
*/

/*!
    \fn QUrlQuery::QUrlQuery()

    Constructs an empty query.
*/

/*!
    \fn QUrlQuery::QUrlQuery(const QString &queryString)

    Constructs a query by parsing \a queryString.

    \sa setQuery()
*/

/*!
    \fn bool QUrlQuery::isEmpty() const

    Returns \c true if there are no items.
*/

/*!
    \fn void QUrlQuery::clear()

    Removes every item.
*/

/*!
    \fn QString QUrlQuery::toString(QUrl::ComponentFormattingOptions encoding) const

    Returns the query as text, formatted according to \a encoding. This is the
    same as query().
*/

/*!
    \fn void QUrlQuery::setQueryItems(const QList<std::pair<QString, QString>> &query)

    Replaces the items with the key-value pairs in \a query.

    \sa queryItems()
*/

/*!
    \fn void QUrlQuery::addQueryItem(const QString &key, const QString &value)

    Appends an item with the given \a key and \a value. Existing items with the
    same key are kept.

    \sa removeQueryItem()
*/

/*!
    Constructs a query from the query part of \a url.
*/
QUrlQuery::QUrlQuery(const QUrl &url) { setQuery(url.query(QUrl::FullyEncoded)); }

/*!
    Constructs a query from the key-value pairs in \a list.
*/
QUrlQuery::QUrlQuery(std::initializer_list<std::pair<QString, QString>> list) {
    for (const auto &item : list)
        m_items.append(item);
}

/*!
    Replaces the contents by parsing \a queryString, whose keys and values may
    be percent-encoded.

    \sa query()
*/
void QUrlQuery::setQuery(const QString &queryString) {
    m_items.clear();
    const QByteArray bytes = queryString.toUtf8();
    qsizetype start = 0;
    const char pairDelimiter = m_pairDelimiter.toLatin1();
    const char valueDelimiter = m_valueDelimiter.toLatin1();
    while (start <= bytes.size()) {
        qsizetype end = bytes.indexOf(pairDelimiter, start);
        if (end < 0)
            end = bytes.size();
        const QByteArray pair = bytes.mid(start, end - start);
        if (!pair.isEmpty()) {
            const qsizetype eq = pair.indexOf(valueDelimiter);
            const QByteArray key = eq < 0 ? pair : pair.left(eq);
            const QByteArray value = eq < 0 ? QByteArray() : pair.mid(eq + 1);
            m_items.append({QUrl::fromPercentEncoding(key), QUrl::fromPercentEncoding(value)});
        }
        start = end + 1;
    }
}

/*!
    Returns the query as text, formatted according to \a encoding.

    \sa setQuery()
*/
QString QUrlQuery::query(QUrl::ComponentFormattingOptions encoding) const {
    QString r;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (i)
            r.append(m_pairDelimiter);
        r += format(m_items.at(i).first, encoding);
        if (!m_items.at(i).second.isNull())
            r += m_valueDelimiter + format(m_items.at(i).second, encoding);
    }
    return r;
}

// Returns the key or value `s` formatted according to `encoding`. The delimiters, '#' and '%' are
// always encoded, since they would change how the text is split.
QString QUrlQuery::format(const QString &s, QUrl::ComponentFormattingOptions encoding) const {
    if (encoding.testFlag(QUrl::FullyDecoded))
        return s;
    QByteArray keep =
        encoding.testFlag(QUrl::FullyEncoded) ? QByteArray("!$'()*+,;:@/?=&") : QByteArray("!$'()*+,;:@/?=& ");
    keep.replace(m_pairDelimiter.toLatin1(), QByteArray());
    keep.replace(m_valueDelimiter.toLatin1(), QByteArray());
    const QByteArray encoded = QUrl::toPercentEncoding(s, keep);
    return encoding.testFlag(QUrl::FullyEncoded) ? QString::fromUtf8(encoded)
                                                 : QUrl(QString("x:?") + QString::fromUtf8(encoded)).query();
}

/*!
    Sets the characters that separate a key from its value to
    \a valueDelimiter and the pairs from each other to \a pairDelimiter.
    They apply to setQuery() and query() calls that follow; the items are
    kept. Both must be ASCII characters, and different.

    \code
    QUrlQuery q;
    q.setQueryDelimiters(u':', u';');
    q.setQuery("a:1;b:2");
    q.queryItemValue("b");   // "2"
    \endcode

    \sa queryValueDelimiter(), queryPairDelimiter()
*/
void QUrlQuery::setQueryDelimiters(QChar valueDelimiter, QChar pairDelimiter) {
    m_valueDelimiter = valueDelimiter;
    m_pairDelimiter = pairDelimiter;
}

/*!
    Returns the items as key-value pairs, in order, with keys and values
    formatted according to \a encoding.

    \sa setQueryItems()
*/
QList<std::pair<QString, QString>> QUrlQuery::queryItems(QUrl::ComponentFormattingOptions encoding) const {
    if (encoding == QUrl::PrettyDecoded || encoding.testFlag(QUrl::FullyDecoded))
        return m_items;
    QList<std::pair<QString, QString>> r;
    r.reserve(m_items.size());
    for (const auto &item : m_items)
        r.append({format(item.first, encoding), item.second.isNull() ? QString() : format(item.second, encoding)});
    return r;
}

/*!
    Returns \c true if there is an item with the given \a key.

    \sa queryItemValue()
*/
bool QUrlQuery::hasQueryItem(const QString &key) const {
    for (const auto &item : m_items)
        if (item.first == key)
            return true;
    return false;
}

/*!
    Removes the first item with the given \a key.

    \sa removeAllQueryItems()
*/
void QUrlQuery::removeQueryItem(const QString &key) {
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).first == key) {
            m_items.removeAt(i);
            return;
        }
    }
}

/*!
    Returns the value of the first item with the given \a key, formatted
    according to \a encoding, or an empty string if there is none.

    \sa allQueryItemValues()
*/
QString QUrlQuery::queryItemValue(const QString &key, QUrl::ComponentFormattingOptions encoding) const {
    for (const auto &item : m_items)
        if (item.first == key)
            return encoding == QUrl::PrettyDecoded ? item.second : format(item.second, encoding);
    return QString();
}

/*!
    Returns the values of every item with the given \a key, formatted
    according to \a encoding.

    \sa queryItemValue()
*/
QStringList QUrlQuery::allQueryItemValues(const QString &key, QUrl::ComponentFormattingOptions encoding) const {
    QStringList r;
    for (const auto &item : m_items)
        if (item.first == key)
            r.append(encoding == QUrl::PrettyDecoded ? item.second : format(item.second, encoding));
    return r;
}

/*!
    Removes every item with the given \a key.

    \sa removeQueryItem()
*/
void QUrlQuery::removeAllQueryItems(const QString &key) {
    for (qsizetype i = m_items.size() - 1; i >= 0; --i)
        if (m_items.at(i).first == key)
            m_items.removeAt(i);
}
