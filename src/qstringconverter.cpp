// MIT License - see LICENSE
#include <QtCore/qstringconverter.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace {

constexpr bool hostIsLittleEndian() {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return false;
#else
    return true;
#endif
}

std::string normalizedName(QAnyStringView name) {
    const QByteArray utf8 = name.toString().toLatin1();
    std::string r;
    for (char c : utf8) {
        if (c >= 'A' && c <= 'Z')
            r.push_back(static_cast<char>(c - 'A' + 'a'));
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            r.push_back(c);
    }
    return r;
}

void appendUtf8(std::string &out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
}

void appendUnit(std::string &out, std::uint32_t v, int bytes, bool little) {
    for (int i = 0; i < bytes; ++i) {
        const int shift = little ? 8 * i : 8 * (bytes - 1 - i);
        out.push_back(static_cast<char>((v >> shift) & 0xff));
    }
}

void appendUtf16(std::u16string &out, char32_t cp) {
    if (cp >= 0x10000) {
        cp -= 0x10000;
        out.push_back(static_cast<char16_t>(0xd800 + (cp >> 10)));
        out.push_back(static_cast<char16_t>(0xdc00 + (cp & 0x3ff)));
    } else {
        out.push_back(static_cast<char16_t>(cp));
    }
}

std::uint32_t readUnit(const unsigned char *p, int bytes, bool little) {
    std::uint32_t v = 0;
    for (int i = 0; i < bytes; ++i) {
        const int shift = little ? 8 * i : 8 * (bytes - 1 - i);
        v |= static_cast<std::uint32_t>(p[i]) << shift;
    }
    return v;
}

} // namespace

/*!
    \internal
    Constructs a converter for the encoding called \a name with the flags \a f.
*/
QStringConverter::QStringConverter(QAnyStringView name, Flags f) noexcept : m_flags(f) {
    if (const auto e = encodingForName(name)) {
        m_encoding = *e == System ? Utf8 : *e;
        m_system = *e == System;
        m_valid = true;
    }
}

/*!
    Resets the conversion state: pending partial sequences are dropped, the
    error state is cleared and the next input is treated as the start of the
    data.

    \sa hasError()
*/
void QStringConverter::resetState() noexcept {
    m_started = false;
    m_order = 0;
    m_invalid = 0;
    m_pendingCount = 0;
}

/*!
    Returns the canonical name of the converter's encoding, or \c nullptr if
    the converter is invalid.

    \sa nameForEncoding()
*/
const char *QStringConverter::name() const noexcept {
    if (!m_valid)
        return nullptr;
    return nameForEncoding(m_system ? System : m_encoding);
}

/*!
    Returns the encoding called \a name, or an empty optional if it is not
    supported. Case and the characters other than letters and digits are
    ignored, so \c{"utf8"}, \c{"UTF-8"} and \c{"Utf_8"} are the same. The
    names \c{"latin1"}, \c{"ISO-8859-1"} and \c{"locale"} are recognized.

    \sa nameForEncoding()
*/
std::optional<QStringConverter::Encoding> QStringConverter::encodingForName(QAnyStringView name) noexcept {
    const std::string n = normalizedName(name);
    struct Alias {
        const char *name;
        Encoding encoding;
    };
    static const Alias aliases[] = {{"utf8", Utf8},          {"utf16", Utf16},     {"utf16le", Utf16LE}, {"utf16be", Utf16BE},
                                    {"utf32", Utf32},        {"utf32le", Utf32LE}, {"utf32be", Utf32BE}, {"iso88591", Latin1},
                                    {"latin1", Latin1},      {"l1", Latin1},       {"cp819", Latin1},    {"ibm819", Latin1},
                                    {"isoir100", Latin1},    {"csisolatin1", Latin1}, {"locale", System}, {"system", System},
                                    {"ucs2", Utf16},         {"ucs4", Utf32},      {"csutf8", Utf8}};
    for (const Alias &a : aliases)
        if (n == a.name)
            return a.encoding;
    return std::nullopt;
}

/*!
    Returns the canonical name of the encoding \a e, such as \c{"UTF-8"}.

    \sa encodingForName()
*/
const char *QStringConverter::nameForEncoding(Encoding e) noexcept {
    switch (e) {
    case Utf8:
        return "UTF-8";
    case Utf16:
        return "UTF-16";
    case Utf16LE:
        return "UTF-16LE";
    case Utf16BE:
        return "UTF-16BE";
    case Utf32:
        return "UTF-32";
    case Utf32LE:
        return "UTF-32LE";
    case Utf32BE:
        return "UTF-32BE";
    case Latin1:
        return "ISO-8859-1";
    case System:
        return "Locale";
    }
    return nullptr;
}

/*!
    Returns the encoding of \a data as indicated by a byte order mark at its
    start. Without one, \a expectedFirstCharacter (if not 0) is used: the
    position of its zero bytes tells UTF-16 and UTF-32 and their byte order
    apart. Returns an empty optional if the encoding cannot be told.

    \sa encodingForHtml()
*/
std::optional<QStringConverter::Encoding> QStringConverter::encodingForData(QByteArrayView data, char16_t expectedFirstCharacter) noexcept {
    const auto *p = reinterpret_cast<const unsigned char *>(data.data());
    const qsizetype n = data.size();
    if (n >= 4) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0xfe && p[3] == 0xff)
            return Utf32BE;
        if (p[0] == 0xff && p[1] == 0xfe && p[2] == 0 && p[3] == 0)
            return Utf32LE;
    }
    if (n >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf)
        return Utf8;
    if (n >= 2) {
        if (p[0] == 0xfe && p[1] == 0xff)
            return Utf16BE;
        if (p[0] == 0xff && p[1] == 0xfe)
            return Utf16LE;
    }
    if (expectedFirstCharacter != 0) {
        const auto hi = static_cast<unsigned char>(expectedFirstCharacter >> 8);
        const auto lo = static_cast<unsigned char>(expectedFirstCharacter & 0xff);
        if (n >= 4 && p[0] == 0 && p[1] == 0 && p[2] == hi && p[3] == lo)
            return Utf32BE;
        if (n >= 4 && p[0] == lo && p[1] == hi && p[2] == 0 && p[3] == 0)
            return Utf32LE;
        if (n >= 2 && p[0] == hi && p[1] == lo)
            return Utf16BE;
        if (n >= 2 && p[0] == lo && p[1] == hi)
            return Utf16LE;
    }
    return std::nullopt;
}

/*!
    Returns the encoding of the HTML document \a data: the one given by a byte
    order mark, or else by a \c charset attribute in the first 1024 bytes.
    Returns Utf8 if neither is present, and an empty optional if the document
    names an encoding that is not supported.

    \sa decoderForHtml(), encodingForData()
*/
std::optional<QStringConverter::Encoding> QStringConverter::encodingForHtml(QByteArrayView data) {
    if (const auto bom = encodingForData(data))
        return bom;
    std::string head(data.data() ? data.data() : "", static_cast<std::size_t>(std::min<qsizetype>(data.size(), 1024)));
    for (char &c : head)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    std::size_t pos = 0;
    while ((pos = head.find("<meta", pos)) != std::string::npos) {
        const std::size_t close = head.find('>', pos);
        const std::size_t cs = head.find("charset", pos);
        pos += 5;
        if (cs == std::string::npos || (close != std::string::npos && cs > close))
            continue;
        std::size_t i = cs + 7;
        while (i < head.size() && head[i] == ' ')
            ++i;
        if (i >= head.size() || head[i] != '=')
            continue;
        ++i;
        while (i < head.size() && (head[i] == ' ' || head[i] == '"' || head[i] == '\''))
            ++i;
        std::size_t end = i;
        while (end < head.size() && head[end] != '"' && head[end] != '\'' && head[end] != ' ' && head[end] != ';' && head[end] != '>' && head[end] != '/')
            ++end;
        const std::string name = head.substr(i, end - i);
        auto e = encodingForName(QAnyStringView(name.data(), static_cast<qsizetype>(name.size())));
        // A document that could be parsed this far is ASCII compatible, so a UTF-16 label is wrong.
        if (e && (*e == Utf16 || *e == Utf16LE || *e == Utf16BE))
            return Utf8;
        return e;
    }
    return Utf8;
}

/*!
    \since 6.7
    Returns the names of the encodings that the converters support.

    \sa encodingForName()
*/
QStringList QStringConverter::availableCodecs() {
    QStringList r;
    for (int e = Utf8; e <= LastEncoding; ++e)
        r.append(QString::fromLatin1(nameForEncoding(static_cast<Encoding>(e))));
    return r;
}

/*!
    Encodes \a in and returns the bytes. If the encoder was created with
    Flag::WriteBom, a byte order mark is written before the first character.
    Unpaired surrogates are encoded as the replacement character (as \c{'?'}
    for Latin1) and counted as errors.

    \sa hasError(), requiredSpace()
*/
QByteArray QStringEncoder::encode(QStringView in) {
    std::string out;
    if (!m_valid)
        return QByteArray();
    const bool little16 = m_encoding == Utf16LE || (m_encoding == Utf16 && hostIsLittleEndian());
    const bool little32 = m_encoding == Utf32LE || (m_encoding == Utf32 && hostIsLittleEndian());
    if (!m_started) {
        m_started = true;
        if (m_flags.testFlag(Flag::WriteBom)) {
            switch (m_encoding) {
            case Utf8:
                appendUtf8(out, 0xfeff);
                break;
            case Utf16:
            case Utf16LE:
            case Utf16BE:
                appendUnit(out, 0xfeff, 2, little16);
                break;
            case Utf32:
            case Utf32LE:
            case Utf32BE:
                appendUnit(out, 0xfeff, 4, little32);
                break;
            default:
                break;
            }
        }
    }
    const bool nullForInvalid = m_flags.testFlag(Flag::ConvertInvalidToNull);
    std::u16string units;
    if (m_pendingCount) {
        units.push_back(static_cast<char16_t>(m_pending[0] | (m_pending[1] << 8)));
        m_pendingCount = 0;
    }
    units.append(reinterpret_cast<const char16_t *>(in.data()), static_cast<std::size_t>(in.size()));
    out.reserve(out.size() + units.size() * 3);
    for (std::size_t i = 0; i < units.size(); ++i) {
        char32_t cp = units[i];
        bool invalid = false;
        if (cp >= 0xd800 && cp <= 0xdbff) {
            if (i + 1 < units.size() && units[i + 1] >= 0xdc00 && units[i + 1] <= 0xdfff) {
                cp = 0x10000 + ((cp - 0xd800) << 10) + (units[i + 1] - 0xdc00);
                ++i;
            } else if (i + 1 == units.size() && !m_flags.testFlag(Flag::Stateless)) {
                m_pending[0] = static_cast<unsigned char>(cp & 0xff);
                m_pending[1] = static_cast<unsigned char>(cp >> 8);
                m_pendingCount = 1;
                break;
            } else {
                invalid = true;
            }
        } else if (cp >= 0xdc00 && cp <= 0xdfff) {
            invalid = true;
        }
        if (invalid) {
            ++m_invalid;
            cp = nullForInvalid ? 0 : 0xfffd;
        }
        switch (m_encoding) {
        case Utf16:
        case Utf16LE:
        case Utf16BE:
            if (cp >= 0x10000) {
                appendUnit(out, 0xd800 + ((cp - 0x10000) >> 10), 2, little16);
                appendUnit(out, 0xdc00 + ((cp - 0x10000) & 0x3ff), 2, little16);
            } else {
                appendUnit(out, cp, 2, little16);
            }
            break;
        case Utf32:
        case Utf32LE:
        case Utf32BE:
            appendUnit(out, cp, 4, little32);
            break;
        case Latin1:
            if (cp > 0xff) {
                if (!invalid)
                    ++m_invalid;
                out.push_back(nullForInvalid ? '\0' : '?');
            } else {
                out.push_back(static_cast<char>(cp));
            }
            break;
        default:
            appendUtf8(out, cp);
            break;
        }
    }
    return QByteArray(std::move(out));
}

/*!
    Returns the largest number of bytes that encoding \a inputLength UTF-16
    code units can produce, including a byte order mark and pending state.

    \sa appendToBuffer()
*/
qsizetype QStringEncoder::requiredSpace(qsizetype inputLength) const noexcept {
    const qsizetype n = inputLength + 1;
    switch (m_encoding) {
    case Latin1:
        return n;
    case Utf16:
    case Utf16LE:
    case Utf16BE:
        return 2 * n + 2;
    case Utf32:
    case Utf32LE:
    case Utf32BE:
        return 4 * n + 4;
    default:
        return 3 * n + 3;
    }
}

/*!
    Encodes \a in into the buffer \a out, which must hold at least
    requiredSpace() bytes, and returns a pointer just past the last byte
    written.
*/
char *QStringEncoder::appendToBuffer(char *out, QStringView in) {
    const QByteArray bytes = encode(in);
    std::memcpy(out, bytes.constData(), static_cast<std::size_t>(bytes.size()));
    return out + bytes.size();
}

/*!
    Decodes \a ba and returns the text. Unless the decoder is
    Flag::Stateless, an incomplete sequence at the end of \a ba is kept and
    completed by the next call. Invalid sequences become the replacement
    character U+FFFD (U+0000 with Flag::ConvertInvalidToNull) and are counted
    as errors. A byte order mark at the start of the data is skipped unless
    Flag::ConvertInitialBom is set; for Utf16 and Utf32 it also selects the
    byte order.

    \sa hasError(), resetState()
*/
QString QStringDecoder::decode(QByteArrayView ba) {
    if (!m_valid)
        return QString();
    std::basic_string<unsigned char> buf(m_pending, m_pending + m_pendingCount);
    buf.append(reinterpret_cast<const unsigned char *>(ba.data() ? ba.data() : ""), static_cast<std::size_t>(ba.size()));
    m_pendingCount = 0;
    const bool stateless = m_flags.testFlag(Flag::Stateless);
    const char32_t bad = m_flags.testFlag(Flag::ConvertInvalidToNull) ? 0 : 0xfffd;
    const std::size_t n = buf.size();
    const unsigned char *p = buf.data();
    std::u16string out;
    out.reserve(n);
    std::size_t i = 0;

    auto keep = [&](std::size_t from) {
        // Keeps the bytes of an incomplete sequence for the next call.
        m_pendingCount = static_cast<int>(n - from);
        std::copy(p + from, p + n, m_pending);
    };

    int unit = 1;
    bool little = false;
    switch (m_encoding) {
    case Utf16LE:
    case Utf16BE:
    case Utf16:
        unit = 2;
        break;
    case Utf32LE:
    case Utf32BE:
    case Utf32:
        unit = 4;
        break;
    default:
        break;
    }
    if (unit > 1) {
        if (m_encoding == Utf16 || m_encoding == Utf32) {
            if (m_order == 0) {
                if (n < static_cast<std::size_t>(unit) && !stateless) {
                    keep(0);
                    return QString(u"");
                }
                const std::uint32_t le = n >= static_cast<std::size_t>(unit) ? readUnit(p, unit, true) : 0;
                const std::uint32_t be = n >= static_cast<std::size_t>(unit) ? readUnit(p, unit, false) : 0;
                m_order = le == 0xfeff ? 1 : (be == 0xfeff ? 2 : (hostIsLittleEndian() ? 1 : 2));
            }
            little = m_order == 1;
        } else {
            little = m_encoding == Utf16LE || m_encoding == Utf32LE;
        }
    }

    while (i < n) {
        if (m_encoding == Latin1) {
            out.push_back(p[i++]);
            continue;
        }
        if (unit == 2) {
            if (i + 2 > n) {
                if (stateless) {
                    ++m_invalid;
                    out.push_back(static_cast<char16_t>(bad));
                    i = n;
                } else {
                    keep(i);
                    i = n;
                }
                break;
            }
            const auto c = static_cast<char16_t>(readUnit(p + i, 2, little));
            if (c >= 0xd800 && c <= 0xdbff) {
                if (i + 4 > n) {
                    if (stateless) {
                        ++m_invalid;
                        out.push_back(static_cast<char16_t>(bad));
                        i = n;
                    } else {
                        keep(i);
                        i = n;
                    }
                    break;
                }
                const auto c2 = static_cast<char16_t>(readUnit(p + i + 2, 2, little));
                if (c2 >= 0xdc00 && c2 <= 0xdfff) {
                    out.push_back(c);
                    out.push_back(c2);
                    i += 4;
                } else {
                    ++m_invalid;
                    out.push_back(static_cast<char16_t>(bad));
                    i += 2;
                }
            } else if (c >= 0xdc00 && c <= 0xdfff) {
                ++m_invalid;
                out.push_back(static_cast<char16_t>(bad));
                i += 2;
            } else {
                out.push_back(c);
                i += 2;
            }
            continue;
        }
        if (unit == 4) {
            if (i + 4 > n) {
                if (stateless) {
                    ++m_invalid;
                    out.push_back(static_cast<char16_t>(bad));
                    i = n;
                } else {
                    keep(i);
                    i = n;
                }
                break;
            }
            const char32_t c = readUnit(p + i, 4, little);
            i += 4;
            if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
                ++m_invalid;
                out.push_back(static_cast<char16_t>(bad));
            } else {
                appendUtf16(out, c);
            }
            continue;
        }
        // UTF-8
        const unsigned char c = p[i];
        if (c < 0x80) {
            out.push_back(c);
            ++i;
            continue;
        }
        int extra = 0;
        char32_t min = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            extra = 1;
            min = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            extra = 2;
            min = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            extra = 3;
            min = 0x10000;
        } else {
            ++m_invalid;
            out.push_back(static_cast<char16_t>(bad));
            ++i;
            continue;
        }
        char32_t cp = c & (0x3f >> extra);
        int k = 1;
        bool ok = true;
        for (; k <= extra; ++k) {
            if (i + static_cast<std::size_t>(k) >= n)
                break;
            const unsigned char cc = p[i + static_cast<std::size_t>(k)];
            if ((cc & 0xc0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3f);
        }
        if (ok && k <= extra) {
            // The input ended inside the sequence.
            if (stateless) {
                ++m_invalid;
                out.push_back(static_cast<char16_t>(bad));
            } else {
                keep(i);
            }
            i = n;
            break;
        }
        if (!ok || cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            ++m_invalid;
            out.push_back(static_cast<char16_t>(bad));
            i += ok ? static_cast<std::size_t>(extra) + 1 : static_cast<std::size_t>(k);
            continue;
        }
        appendUtf16(out, cp);
        i += static_cast<std::size_t>(extra) + 1;
    }

    if (!m_started && !out.empty()) {
        m_started = true;
        if (out.front() == 0xfeff && !m_flags.testFlag(Flag::ConvertInitialBom))
            out.erase(0, 1);
    }
    return QString(out);
}

/*!
    Returns the largest number of UTF-16 code units that decoding
    \a inputLength bytes can produce, including pending state.

    \sa appendToBuffer()
*/
qsizetype QStringDecoder::requiredSpace(qsizetype inputLength) const noexcept {
    return inputLength + m_pendingCount + 1;
}

/*!
    Decodes \a ba into the buffer \a out, which must hold at least
    requiredSpace() characters, and returns a pointer just past the last
    character written.
*/
QChar *QStringDecoder::appendToBuffer(QChar *out, QByteArrayView ba) {
    const QString s = decode(ba);
    std::copy(s.constData(), s.constData() + s.size(), out);
    return out + s.size();
}

/*!
    Returns a decoder for the HTML document \a data, using the encoding found
    by encodingForHtml(). The decoder is invalid if the document names an
    encoding that is not supported.
*/
QStringDecoder QStringDecoder::decoderForHtml(QByteArrayView data) {
    const auto e = encodingForHtml(data);
    return e ? QStringDecoder(*e) : QStringDecoder();
}
