// MIT License - see LICENSE
#include <QtCore/qstringconverter.h>

#include "qstringconverter_tables_p.h"

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace {

using State = QStringConverterBase::State;
using Flag = QStringConverterBase::Flag;

constexpr int kHeaderDone = 1;
constexpr int kBigEndian = 2;
constexpr char16_t kReplacement = 0xfffd;

enum class Kind { Utf8, Utf16, Utf16LE, Utf16BE, Utf32, Utf32LE, Utf32BE, Latin1, Table };

bool hostIsLittleEndian() {
    const unsigned short v = 1;
    unsigned char b = 0;
    std::memcpy(&b, &v, 1);
    return b == 1;
}

bool isHigh(char32_t u) { return (u & 0xfffffc00u) == 0xd800u; }
bool isLow(char32_t u) { return (u & 0xfffffc00u) == 0xdc00u; }

// Lowercases ASCII and drops the separators that encoding names write differently.
std::string normalizeName(const char *name, qsizetype len) {
    std::string r;
    if (!name)
        return r;
    if (len < 0)
        len = static_cast<qsizetype>(std::strlen(name));
    for (qsizetype i = 0; i < len; ++i) {
        const char c = name[i];
        if (c == '-' || c == '_' || c == ' ')
            continue;
        r.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return r;
}

bool hasToken(const char *list, const std::string &token) {
    const std::size_t n = token.size();
    for (const char *p = list; *p;) {
        const char *e = std::strchr(p, ' ');
        const std::size_t len = e ? static_cast<std::size_t>(e - p) : std::strlen(p);
        if (len == n && std::memcmp(p, token.data(), n) == 0)
            return true;
        if (!e)
            break;
        p = e + 1;
    }
    return false;
}

void savePending(State &st, const unsigned char *p, qsizetype n) {
    std::memcpy(st.state_data, p, static_cast<std::size_t>(n));
    st.remainingChars = n;
}

QChar *putCodePoint(QChar *out, char32_t cp) {
    if (cp >= 0x10000) {
        cp -= 0x10000;
        *out++ = QChar(static_cast<char16_t>(0xd800 + (cp >> 10)));
        *out++ = QChar(static_cast<char16_t>(0xdc00 + (cp & 0x3ff)));
    } else {
        *out++ = QChar(static_cast<char16_t>(cp));
    }
    return out;
}

QChar *decodeUtf8(State &st, QChar *out, const unsigned char *p, qsizetype n, bool stateless) {
    const QChar bad = QChar((st.flags & Flag::ConvertInvalidToNull) ? char16_t(0) : kReplacement);
    qsizetype i = 0;
    while (i < n) {
        const unsigned char c = p[i];
        if (c < 0x80) {
            *out++ = QChar(static_cast<char16_t>(c));
            ++i;
            continue;
        }
        int need = 0;
        unsigned char lo = 0x80;
        unsigned char hi = 0xbf;
        char32_t cp = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            need = 1;
            cp = c & 0x1fu;
        } else if (c >= 0xe0 && c <= 0xef) {
            need = 2;
            cp = c & 0x0fu;
            if (c == 0xe0)
                lo = 0xa0;
            if (c == 0xed)
                hi = 0x9f;
        } else if (c >= 0xf0 && c <= 0xf4) {
            need = 3;
            cp = c & 0x07u;
            if (c == 0xf0)
                lo = 0x90;
            if (c == 0xf4)
                hi = 0x8f;
        } else {
            *out++ = bad;
            ++st.invalidChars;
            ++i;
            continue;
        }
        qsizetype j = i + 1;
        int got = 0;
        while (got < need && j < n) {
            const unsigned char d = p[j];
            if (d < (got == 0 ? lo : 0x80) || d > (got == 0 ? hi : 0xbf))
                break;
            cp = (cp << 6) | (d & 0x3fu);
            ++j;
            ++got;
        }
        if (got == need) {
            out = putCodePoint(out, cp);
        } else if (j >= n && !stateless) {
            savePending(st, p + i, n - i);
            return out;
        } else {
            *out++ = bad;
            ++st.invalidChars;
        }
        i = j;
    }
    return out;
}

QChar *decodeUtf16(State &st, QChar *out, const unsigned char *p, qsizetype n, bool big, bool stateless) {
    const QChar bad = QChar((st.flags & Flag::ConvertInvalidToNull) ? char16_t(0) : kReplacement);
    auto unit = [&](qsizetype k) -> char16_t {
        return big ? static_cast<char16_t>((p[k] << 8) | p[k + 1]) : static_cast<char16_t>(p[k] | (p[k + 1] << 8));
    };
    qsizetype i = 0;
    while (i + 2 <= n) {
        const char16_t u = unit(i);
        if (isHigh(u)) {
            if (i + 4 <= n) {
                const char16_t u2 = unit(i + 2);
                if (isLow(u2)) {
                    *out++ = QChar(u);
                    *out++ = QChar(u2);
                    i += 4;
                } else {
                    *out++ = bad;
                    ++st.invalidChars;
                    i += 2;
                }
            } else if (!stateless) {
                savePending(st, p + i, n - i);
                return out;
            } else {
                *out++ = bad;
                ++st.invalidChars;
                i += 2;
            }
        } else if (isLow(u)) {
            *out++ = bad;
            ++st.invalidChars;
            i += 2;
        } else {
            *out++ = QChar(u);
            i += 2;
        }
    }
    if (i < n) {
        if (stateless) {
            *out++ = bad;
            ++st.invalidChars;
        } else {
            savePending(st, p + i, n - i);
        }
    }
    return out;
}

QChar *decodeUtf32(State &st, QChar *out, const unsigned char *p, qsizetype n, bool big, bool stateless) {
    const QChar bad = QChar((st.flags & Flag::ConvertInvalidToNull) ? char16_t(0) : kReplacement);
    qsizetype i = 0;
    while (i + 4 <= n) {
        const char32_t cp = big ? (char32_t(p[i]) << 24) | (char32_t(p[i + 1]) << 16) | (char32_t(p[i + 2]) << 8) | char32_t(p[i + 3])
                                : (char32_t(p[i + 3]) << 24) | (char32_t(p[i + 2]) << 16) | (char32_t(p[i + 1]) << 8) | char32_t(p[i]);
        if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            *out++ = bad;
            ++st.invalidChars;
        } else {
            out = putCodePoint(out, cp);
        }
        i += 4;
    }
    if (i < n) {
        if (stateless) {
            *out++ = bad;
            ++st.invalidChars;
        } else {
            savePending(st, p + i, n - i);
        }
    }
    return out;
}

QChar *decodeChunk(Kind kind, const char16_t *table, State &st, QChar *out, const char *in, qsizetype inLen) {
    const bool stateless = (st.flags & Flag::Stateless) != 0;
    if (stateless) {
        st.internalState = 0;
        st.remainingChars = 0;
        st.invalidChars = 0;
    }
    std::string joined;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(in);
    qsizetype n = inLen;
    if (st.remainingChars > 0) {
        joined.assign(reinterpret_cast<const char *>(st.state_data), static_cast<std::size_t>(st.remainingChars));
        joined.append(in, static_cast<std::size_t>(inLen));
        p = reinterpret_cast<const unsigned char *>(joined.data());
        n = static_cast<qsizetype>(joined.size());
        st.remainingChars = 0;
    }
    if (n == 0)
        return out;

    const bool keepBom = (st.flags & Flag::ConvertInitialBom) != 0;
    bool big = false;
    switch (kind) {
    case Kind::Latin1:
        for (qsizetype i = 0; i < n; ++i)
            *out++ = QChar(static_cast<char16_t>(p[i]));
        return out;
    case Kind::Table: {
        const QChar bad = QChar((st.flags & Flag::ConvertInvalidToNull) ? char16_t(0) : kReplacement);
        for (qsizetype i = 0; i < n; ++i) {
            const unsigned char c = p[i];
            const char16_t u = c < 0x80 ? c : (table ? table[c - 0x80] : char16_t(0xffff));
            if (u == 0xffff) {
                *out++ = bad;
                ++st.invalidChars;
            } else {
                *out++ = QChar(u);
            }
        }
        return out;
    }
    case Kind::Utf8:
        if (!(st.internalState & kHeaderDone)) {
            static const unsigned char bom[3] = {0xef, 0xbb, 0xbf};
            const qsizetype k = n < 3 ? n : 3;
            if (std::memcmp(p, bom, static_cast<std::size_t>(k)) == 0) {
                if (k < 3 && !stateless) {
                    savePending(st, p, n);
                    return out;
                }
                if (k == 3) {
                    st.internalState |= kHeaderDone;
                    if (!keepBom) {
                        p += 3;
                        n -= 3;
                    }
                }
            } else {
                st.internalState |= kHeaderDone;
            }
        }
        return decodeUtf8(st, out, p, n, stateless);
    case Kind::Utf16LE:
        return decodeUtf16(st, out, p, n, false, stateless);
    case Kind::Utf16BE:
        return decodeUtf16(st, out, p, n, true, stateless);
    case Kind::Utf32LE:
        return decodeUtf32(st, out, p, n, false, stateless);
    case Kind::Utf32BE:
        return decodeUtf32(st, out, p, n, true, stateless);
    case Kind::Utf16:
    case Kind::Utf32: {
        const bool wide = kind == Kind::Utf32;
        const qsizetype unitSize = wide ? 4 : 2;
        if (!(st.internalState & kHeaderDone)) {
            if (n < unitSize && !stateless) {
                savePending(st, p, n);
                return out;
            }
            big = !hostIsLittleEndian();
            if (n >= unitSize) {
                bool isBom = false;
                if (wide) {
                    if (p[0] == 0 && p[1] == 0 && p[2] == 0xfe && p[3] == 0xff) {
                        big = true;
                        isBom = true;
                    } else if (p[0] == 0xff && p[1] == 0xfe && p[2] == 0 && p[3] == 0) {
                        big = false;
                        isBom = true;
                    }
                } else if (p[0] == 0xfe && p[1] == 0xff) {
                    big = true;
                    isBom = true;
                } else if (p[0] == 0xff && p[1] == 0xfe) {
                    big = false;
                    isBom = true;
                }
                if (isBom && !keepBom) {
                    p += unitSize;
                    n -= unitSize;
                }
            }
            st.internalState |= kHeaderDone | (big ? kBigEndian : 0);
        } else {
            big = (st.internalState & kBigEndian) != 0;
        }
        return wide ? decodeUtf32(st, out, p, n, big, stateless) : decodeUtf16(st, out, p, n, big, stateless);
    }
    }
    return out;
}

class Writer {
public:
    Writer(char *o, Kind k, const char16_t *t, State &s) : out(o), kind(k), table(t), st(s) {}

    char *out;

    void bom() {
        switch (kind) {
        case Kind::Utf8:
            *out++ = char(0xef);
            *out++ = char(0xbb);
            *out++ = char(0xbf);
            break;
        case Kind::Utf16:
        case Kind::Utf16LE:
        case Kind::Utf16BE:
        case Kind::Utf32:
        case Kind::Utf32LE:
        case Kind::Utf32BE:
            put(0xfeff);
            break;
        default:
            break;
        }
    }

    void put(char32_t cp) {
        switch (kind) {
        case Kind::Utf8:
            if (cp < 0x80) {
                *out++ = static_cast<char>(cp);
            } else if (cp < 0x800) {
                *out++ = static_cast<char>(0xc0 | (cp >> 6));
                *out++ = static_cast<char>(0x80 | (cp & 0x3f));
            } else if (cp < 0x10000) {
                *out++ = static_cast<char>(0xe0 | (cp >> 12));
                *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
                *out++ = static_cast<char>(0x80 | (cp & 0x3f));
            } else {
                *out++ = static_cast<char>(0xf0 | (cp >> 18));
                *out++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
                *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
                *out++ = static_cast<char>(0x80 | (cp & 0x3f));
            }
            break;
        case Kind::Utf16:
        case Kind::Utf16LE:
        case Kind::Utf16BE:
            if (cp >= 0x10000) {
                cp -= 0x10000;
                unit16(static_cast<char16_t>(0xd800 + (cp >> 10)));
                unit16(static_cast<char16_t>(0xdc00 + (cp & 0x3ff)));
            } else {
                unit16(static_cast<char16_t>(cp));
            }
            break;
        case Kind::Utf32:
        case Kind::Utf32LE:
        case Kind::Utf32BE:
            if (bigEndian()) {
                for (int s = 24; s >= 0; s -= 8)
                    *out++ = static_cast<char>((cp >> s) & 0xff);
            } else {
                for (int s = 0; s <= 24; s += 8)
                    *out++ = static_cast<char>((cp >> s) & 0xff);
            }
            break;
        case Kind::Latin1:
            if (cp > 0xff)
                invalid();
            else
                *out++ = static_cast<char>(cp);
            break;
        case Kind::Table:
            putTable(cp);
            break;
        }
    }

    void invalid() {
        ++st.invalidChars;
        const bool toNull = (st.flags & Flag::ConvertInvalidToNull) != 0;
        switch (kind) {
        case Kind::Latin1:
        case Kind::Table:
            *out++ = toNull ? '\0' : '?';
            break;
        default:
            put(kReplacement);
            break;
        }
    }

private:
    Kind kind;
    const char16_t *table;
    State &st;

    bool bigEndian() const {
        if (kind == Kind::Utf16BE || kind == Kind::Utf32BE)
            return true;
        if (kind == Kind::Utf16LE || kind == Kind::Utf32LE)
            return false;
        return !hostIsLittleEndian();
    }

    void unit16(char16_t u) {
        if (bigEndian()) {
            *out++ = static_cast<char>(u >> 8);
            *out++ = static_cast<char>(u & 0xff);
        } else {
            *out++ = static_cast<char>(u & 0xff);
            *out++ = static_cast<char>(u >> 8);
        }
    }

    void putTable(char32_t cp) {
        if (cp < 0x80) {
            *out++ = static_cast<char>(cp);
            return;
        }
        if (table && cp < 0xffff) {
            for (int i = 0; i < 128; ++i) {
                if (table[i] == cp) {
                    *out++ = static_cast<char>(0x80 + i);
                    return;
                }
            }
        }
        invalid();
    }
};

char *encodeChunk(Kind kind, const char16_t *table, State &st, char *out, const char16_t *in, qsizetype n) {
    const bool stateless = (st.flags & Flag::Stateless) != 0;
    if (stateless) {
        st.internalState = 0;
        st.remainingChars = 0;
        st.invalidChars = 0;
    }
    Writer w(out, kind, table, st);
    bool havePending = st.remainingChars > 0;
    char16_t high = havePending ? static_cast<char16_t>(st.state_data[0]) : char16_t(0);
    if ((st.flags & Flag::WriteBom) && !(st.internalState & kHeaderDone) && (n > 0 || havePending)) {
        w.bom();
        st.internalState |= kHeaderDone;
    }
    qsizetype i = 0;
    for (;;) {
        if (havePending) {
            if (i >= n)
                break;
            if (isLow(in[i])) {
                w.put(0x10000u + ((char32_t(high) - 0xd800u) << 10) + (char32_t(in[i]) - 0xdc00u));
                ++i;
            } else {
                w.invalid();
            }
            havePending = false;
            continue;
        }
        if (i >= n)
            break;
        const char16_t u = in[i++];
        if (isHigh(u)) {
            havePending = true;
            high = u;
        } else if (isLow(u)) {
            w.invalid();
        } else {
            w.put(u);
        }
    }
    st.remainingChars = 0;
    if (havePending) {
        if (stateless) {
            w.invalid();
        } else {
            st.state_data[0] = high;
            st.remainingChars = 1;
        }
    }
    return w.out;
}

} // namespace

struct QStringConverter::Interface {
    const char *name;
    Kind kind;
    const char16_t *table;
};

namespace {

constexpr const char *kEncodingNames[] = {"UTF-8", "UTF-16", "UTF-16LE", "UTF-16BE", "UTF-32", "UTF-32LE", "UTF-32BE", "ISO-8859-1", "Locale"};
constexpr Kind kEncodingKinds[] = {Kind::Utf8, Kind::Utf16, Kind::Utf16LE, Kind::Utf16BE, Kind::Utf32, Kind::Utf32LE, Kind::Utf32BE, Kind::Latin1, Kind::Utf8};

} // namespace

/*!
    Returns the name of the encoding of the converter, such as \c{"UTF-8"} or
    \c{"windows-1252"}, or \c nullptr if the converter is not valid. The name
    of QStringConverter::System is \c{"Locale"}.

    \sa isValid(), nameForEncoding()
*/
const char *QStringConverter::name() const noexcept { return iface ? iface->name : nullptr; }

/*!
    Returns the encoding called \a name, or an empty value if \a name is not
    the name of an encoding of the Encoding enumeration. \a nameLength is the
    length of \a name, or -1 if it is null-terminated. The comparison ignores
    case, hyphens, underscores and spaces, so \c{"utf8"}, \c{"UTF-8"} and
    \c{"utf_8"} are the same; \c{"latin1"} and \c{"iso-8859-1"} name
    QStringConverter::Latin1, and \c{"System"} and \c{"Locale"} name
    QStringConverter::System.

    \sa nameForEncoding()
*/
std::optional<QStringConverter::Encoding> QStringConverter::encodingForName(const char *name, qsizetype nameLength) noexcept {
    const std::string n = normalizeName(name, nameLength);
    struct Entry {
        const char *name;
        Encoding encoding;
    };
    static const Entry entries[] = {
        {"utf8", Utf8},       {"utf16", Utf16},    {"utf16le", Utf16LE}, {"utf16be", Utf16BE},
        {"utf32", Utf32},     {"utf32le", Utf32LE}, {"utf32be", Utf32BE}, {"latin1", Latin1},
        {"iso88591", Latin1}, {"l1", Latin1},      {"ibm819", Latin1},   {"cp819", Latin1},
        {"system", System},   {"locale", System},
    };
    for (const Entry &e : entries) {
        if (n == e.name)
            return e.encoding;
    }
    return std::nullopt;
}

/*!
    Returns the name of the encoding \a e: \c{"UTF-8"}, \c{"UTF-16"},
    \c{"UTF-16LE"}, \c{"UTF-16BE"}, \c{"UTF-32"}, \c{"UTF-32LE"},
    \c{"UTF-32BE"}, \c{"ISO-8859-1"} or \c{"Locale"}. Returns \c nullptr if \a e
    is not a value of the enumeration.

    \sa encodingForName()
*/
const char *QStringConverter::nameForEncoding(Encoding e) noexcept {
    return e >= Utf8 && e <= System ? kEncodingNames[e] : nullptr;
}

/*!
    Returns the encoding of \a data judged from its first bytes, or an empty
    value if it cannot be told. A byte order mark selects UTF-8, UTF-16 or
    UTF-32 and the byte order. Without one, if \a expectedFirstCharacter is
    not 0, the encoding is the one in which \a data starts with that
    character: UTF-32 and UTF-16 are tried first, then UTF-8.

    \sa encodingForHtml()
*/
std::optional<QStringConverter::Encoding> QStringConverter::encodingForData(QByteArrayView data, char16_t expectedFirstCharacter) noexcept {
    const auto *p = reinterpret_cast<const unsigned char *>(data.data());
    const qsizetype n = data.size();
    if (n >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 0xfe && p[3] == 0xff)
        return Utf32BE;
    if (n >= 4 && p[0] == 0xff && p[1] == 0xfe && p[2] == 0 && p[3] == 0)
        return Utf32LE;
    if (n >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf)
        return Utf8;
    if (n >= 2 && p[0] == 0xfe && p[1] == 0xff)
        return Utf16BE;
    if (n >= 2 && p[0] == 0xff && p[1] == 0xfe)
        return Utf16LE;
    if (expectedFirstCharacter == 0)
        return std::nullopt;
    const unsigned char hi = static_cast<unsigned char>(expectedFirstCharacter >> 8);
    const unsigned char lo = static_cast<unsigned char>(expectedFirstCharacter & 0xff);
    if (n >= 4 && p[0] == lo && p[1] == hi && p[2] == 0 && p[3] == 0)
        return Utf32LE;
    if (n >= 4 && p[0] == 0 && p[1] == 0 && p[2] == hi && p[3] == lo)
        return Utf32BE;
    if (n >= 2 && p[0] == lo && p[1] == hi)
        return Utf16LE;
    if (n >= 2 && p[0] == hi && p[1] == lo)
        return Utf16BE;
    char buf[4];
    State scratch;
    Writer w(buf, Kind::Utf8, nullptr, scratch);
    w.put(expectedFirstCharacter);
    const qsizetype len = w.out - buf;
    if (n >= len && std::memcmp(p, buf, static_cast<std::size_t>(len)) == 0)
        return Utf8;
    return std::nullopt;
}

/*!
    Returns the encoding of the HTML document \a data, or an empty value if
    it cannot be told. A byte order mark decides first. Otherwise the first
    kilobyte is searched for a \c{<meta>} element with a \c charset
    attribute, and the encoding it names is returned if it is one of the
    Encoding enumeration; a declaration of UTF-16 or UTF-32 means UTF-8,
    because the declaration could not have been read otherwise.

    \sa encodingForData()
*/
std::optional<QStringConverter::Encoding> QStringConverter::encodingForHtml(QByteArrayView data) noexcept {
    if (const auto e = encodingForData(data))
        return e;
    std::string head;
    const qsizetype limit = data.size() < 1024 ? data.size() : 1024;
    for (qsizetype i = 0; i < limit; ++i) {
        const char c = data[i];
        head.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    std::size_t pos = 0;
    while ((pos = head.find("<meta", pos)) != std::string::npos) {
        const std::size_t end = head.find('>', pos);
        const std::size_t cs = head.find("charset", pos);
        pos += 5;
        if (cs == std::string::npos || (end != std::string::npos && cs > end))
            continue;
        std::size_t v = cs + 7;
        while (v < head.size() && head[v] == ' ')
            ++v;
        if (v >= head.size() || head[v] != '=')
            continue;
        ++v;
        while (v < head.size() && head[v] == ' ')
            ++v;
        if (v < head.size() && (head[v] == '"' || head[v] == '\''))
            ++v;
        std::size_t e = v;
        while (e < head.size() && (std::isalnum(static_cast<unsigned char>(head[e])) || head[e] == '-' || head[e] == '_' || head[e] == '.' || head[e] == ':'))
            ++e;
        const auto enc = encodingForName(head.data() + v, static_cast<qsizetype>(e - v));
        if (!enc)
            return std::nullopt;
        if (*enc == Utf16 || *enc == Utf16LE || *enc == Utf16BE || *enc == Utf32 || *enc == Utf32LE || *enc == Utf32BE)
            return Utf8;
        return enc;
    }
    return std::nullopt;
}

namespace {

constexpr std::size_t kTableCodecCount = sizeof(QtPrivate::kSingleByteCodecs) / sizeof(QtPrivate::kSingleByteCodecs[0]);

const QStringConverter::Interface kEncodingInterfaces[] = {
    {kEncodingNames[0], kEncodingKinds[0], nullptr}, {kEncodingNames[1], kEncodingKinds[1], nullptr},
    {kEncodingNames[2], kEncodingKinds[2], nullptr}, {kEncodingNames[3], kEncodingKinds[3], nullptr},
    {kEncodingNames[4], kEncodingKinds[4], nullptr}, {kEncodingNames[5], kEncodingKinds[5], nullptr},
    {kEncodingNames[6], kEncodingKinds[6], nullptr}, {kEncodingNames[7], kEncodingKinds[7], nullptr},
    {kEncodingNames[8], kEncodingKinds[8], nullptr},
};

const QStringConverter::Interface kAsciiInterface = {"US-ASCII", Kind::Table, nullptr};

const std::vector<QStringConverter::Interface> &tableInterfaces() {
    static const std::vector<QStringConverter::Interface> list = [] {
        std::vector<QStringConverter::Interface> v;
        v.reserve(kTableCodecCount);
        for (const QtPrivate::SingleByteCodec &c : QtPrivate::kSingleByteCodecs)
            v.push_back({c.name, Kind::Table, c.high});
        return v;
    }();
    return list;
}

} // namespace

QStringConverter::QStringConverter(Encoding encoding, Flags f) noexcept {
    if (encoding >= Utf8 && encoding <= System)
        iface = &kEncodingInterfaces[encoding];
    state.flags = f;
}

QStringConverter::QStringConverter(const char *name, Flags f) noexcept {
    state.flags = f;
    if (const auto e = encodingForName(name)) {
        iface = &kEncodingInterfaces[*e];
        return;
    }
    const std::string n = normalizeName(name, -1);
    if (n == "ascii" || n == "usascii" || n == "iso646us" || n == "cp367") {
        iface = &kAsciiInterface;
        return;
    }
    const std::vector<Interface> &tables = tableInterfaces();
    for (std::size_t i = 0; i < kTableCodecCount; ++i) {
        if (hasToken(QtPrivate::kSingleByteCodecs[i].aliases, n)) {
            iface = &tables[i];
            return;
        }
    }
}

qsizetype QStringEncoder::requiredSpace(qsizetype inputLength) const noexcept {
    const qsizetype n = (inputLength < 0 ? 0 : inputLength) + 1;
    if (!iface)
        return 0;
    switch (iface->kind) {
    case Kind::Utf8:
        return 3 * n + 3;
    case Kind::Utf16:
    case Kind::Utf16LE:
    case Kind::Utf16BE:
        return 2 * n + 2;
    case Kind::Utf32:
    case Kind::Utf32LE:
    case Kind::Utf32BE:
        return 4 * n + 4;
    default:
        return n;
    }
}

/*!
    Encodes \a in and writes the bytes to \a out, which must have room for
    requiredSpace() bytes for the size of \a in. Returns a pointer just after
    the last byte written. The state of the encoder is updated, so a
    following call continues the text. Nothing is written if the encoder is
    not valid.

    \sa requiredSpace()
*/
char *QStringEncoder::appendToBuffer(char *out, QStringView in) {
    if (!iface)
        return out;
    return encodeChunk(iface->kind, iface->table, state, out, in.utf16(), in.size());
}

qsizetype QStringDecoder::requiredSpace(qsizetype inputLength) const noexcept {
    return iface ? (inputLength < 0 ? 0 : inputLength) + 4 : 0;
}

/*!
    Decodes \a ba and writes the characters to \a out, which must have room
    for requiredSpace() characters for the size of \a ba. Returns a pointer
    just after the last character written. The state of the decoder is
    updated, so a following call continues the text. Nothing is written if
    the decoder is not valid.

    \sa requiredSpace()
*/
QChar *QStringDecoder::appendToBuffer(QChar *out, QByteArrayView ba) {
    if (!iface)
        return out;
    return decodeChunk(iface->kind, iface->table, state, out, ba.data(), ba.size());
}

namespace {

char32_t validCodePoint(char32_t cp) { return cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) ? char32_t(0xfffd) : cp; }

void appendCodePoint(std::u16string &s, char32_t cp) {
    cp = validCodePoint(cp);
    if (cp >= 0x10000) {
        cp -= 0x10000;
        s.push_back(static_cast<char16_t>(0xd800 + (cp >> 10)));
        s.push_back(static_cast<char16_t>(0xdc00 + (cp & 0x3ff)));
    } else {
        s.push_back(static_cast<char16_t>(cp));
    }
}

template <typename Unit>
qsizetype nulTerminatedLength(const Unit *s) {
    qsizetype n = 0;
    while (s[n] != 0)
        ++n;
    return n;
}

// Calls f with each code point of the UTF-16 text, replacing unpaired surrogates by U+FFFD.
template <typename F>
void forEachCodePoint(const std::u16string &d, F f) {
    for (std::size_t i = 0; i < d.size(); ++i) {
        const char16_t u = d[i];
        if (isHigh(u) && i + 1 < d.size() && isLow(d[i + 1])) {
            f(0x10000u + ((char32_t(u) - 0xd800u) << 10) + (char32_t(d[i + 1]) - 0xdc00u));
            ++i;
        } else {
            f(isHigh(u) || isLow(u) ? char32_t(0xfffd) : char32_t(u));
        }
    }
}

} // namespace

QString QString::fromUcs4(const char32_t *s, qsizetype size) {
    if (!s)
        return QString();
    if (size < 0)
        size = nulTerminatedLength(s);
    std::u16string r;
    r.reserve(static_cast<std::size_t>(size));
    for (qsizetype i = 0; i < size; ++i)
        appendCodePoint(r, s[i]);
    return QString(r);
}

QString QString::fromWCharArray(const wchar_t *s, qsizetype size) {
    if (!s)
        return QString();
    if (size < 0)
        size = nulTerminatedLength(s);
    if constexpr (sizeof(wchar_t) == 2) {
        std::u16string r;
        for (qsizetype i = 0; i < size; ++i)
            r.push_back(static_cast<char16_t>(s[i]));
        return QString(r);
    } else {
        std::u16string r;
        r.reserve(static_cast<std::size_t>(size));
        for (qsizetype i = 0; i < size; ++i)
            appendCodePoint(r, static_cast<char32_t>(s[i]));
        return QString(r);
    }
}

/*!
    Returns the string as a list of UTF-32 code points, not terminated by a
    null. A surrogate pair gives one code point, and an unpaired surrogate
    gives U+FFFD.

    \sa fromUcs4(), toStdU32String()
*/
QList<uint> QString::toUcs4() const {
    QList<uint> r;
    r.reserve(size());
    forEachCodePoint(d, [&r](char32_t cp) { r.append(static_cast<uint>(cp)); });
    return r;
}

std::u32string QString::toStdU32String() const {
    std::u32string r;
    r.reserve(d.size());
    forEachCodePoint(d, [&r](char32_t cp) { r.push_back(cp); });
    return r;
}

std::wstring QString::toStdWString() const {
    std::wstring r(static_cast<std::size_t>(size()), L'\0');
    r.resize(static_cast<std::size_t>(toWCharArray(r.data())));
    return r;
}

qsizetype QString::toWCharArray(wchar_t *array) const {
    if constexpr (sizeof(wchar_t) == 2) {
        for (std::size_t i = 0; i < d.size(); ++i)
            array[i] = static_cast<wchar_t>(d[i]);
        return size();
    } else {
        qsizetype n = 0;
        forEachCodePoint(d, [&](char32_t cp) { array[n++] = static_cast<wchar_t>(cp); });
        return n;
    }
}
