// MIT License - see LICENSE
#include <QtCore/qbytearray.h>

#include "numparse_p.h"

#include <QtCore/qlist.h>

#include <algorithm>
#include <climits>

static char lowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }
static bool isSpaceAscii(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

/*!
    \class QByteArray
    \brief The QByteArray class provides an array of bytes.

    QByteArray stores raw bytes, or 8-bit text, in a contiguous buffer. It is
    used wherever binary data is needed, such as I/O buffers, hashes and
    encoded text.

    \code
    QByteArray ba("Hello");
    ba.append(", world");
    ba.replace("world", "Nt");
    // ba == "Hello, Nt"
    \endcode

    A QByteArray can be null or empty. A null array is one that was default
    constructed, constructed from a null \c{const char *}, or cleared; a
    null array is also empty.

    \differences
    The bytes are stored in a \c std::string and the array is not implicitly
    shared, so copies are deep and fromRawData() copies the data. There is
    no \c QString conversion. split() takes a single \c char separator and
    always keeps empty parts. Unless \c AbortOnBase64DecodingErrors is given,
    fromBase64() and fromBase64Encoding() accept both the standard and the
    URL-safe alphabets and skip other characters; a strict decode accepts
    missing \c{=} padding. The conversions
    toStdString() and stdString() are Nt additions. Indexes are qsizetype
    but are not range-checked.
    \enddifferences

    \sa QList
*/

/*!
    Constructs a byte array holding a copy of the first \a size bytes of
    \a data. If \a size is negative, \a data is treated as a
    \c{'\0'}-terminated string and its length is used. If \a data is
    \c nullptr, a null byte array is constructed.
*/
QByteArray::QByteArray(const char *data, qsizetype size) {
    if (!data)
        return;
    m_null = false;
    d.assign(data, size < 0 ? std::char_traits<char>::length(data) : static_cast<std::size_t>(size));
}

/*!
    Sets every byte in the array to \a c and returns a reference to this
    byte array. If \a n is not negative, the array is first resized to
    \a n bytes. The array is no longer null afterwards.

    \sa resize()
*/
QByteArray &QByteArray::fill(char c, qsizetype n) {
    if (n >= 0)
        d.assign(static_cast<std::size_t>(n), c);
    else
        std::fill(d.begin(), d.end(), c);
    m_null = false;
    return *this;
}

/*!
    Removes \a len bytes from the array, starting at index position \a pos,
    and returns a reference to this byte array. If \a pos is out of range or
    \a len is not positive, nothing happens. If \a pos + \a len is beyond the
    end of the array, bytes are removed up to the end.

    \sa insert(), replace(), chop()
*/
QByteArray &QByteArray::remove(qsizetype pos, qsizetype len) {
    if (pos < 0 || pos >= size() || len <= 0)
        return *this;
    d.erase(static_cast<std::size_t>(pos), static_cast<std::size_t>(len));
    return *this;
}

/*!
    Replaces \a len bytes from index position \a pos with the byte array
    \a after and returns a reference to this byte array. If \a pos is out of
    range, nothing happens.

    \sa insert(), remove()
*/
QByteArray &QByteArray::replace(qsizetype pos, qsizetype len, const QByteArray &after) {
    if (pos < 0 || pos > size())
        return *this;
    d.replace(static_cast<std::size_t>(pos), static_cast<std::size_t>(len < 0 ? 0 : len), after.d);
    return *this;
}

/*!
    \overload

    Replaces every occurrence of the byte array \a before with the byte array
    \a after and returns a reference to this byte array. If \a before is
    empty, nothing happens.
*/
QByteArray &QByteArray::replace(const QByteArray &before, const QByteArray &after) {
    if (before.isEmpty())
        return *this;
    std::size_t pos = 0;
    while ((pos = d.find(before.d, pos)) != std::string::npos) {
        d.replace(pos, before.d.size(), after.d);
        pos += after.d.size();
    }
    return *this;
}

/*!
    \overload

    Replaces every occurrence of the byte \a before with the byte \a after
    and returns a reference to this byte array.
*/
QByteArray &QByteArray::replace(char before, char after) {
    std::replace(d.begin(), d.end(), before, after);
    return *this;
}

/*!
    Returns a byte array containing \a len bytes from this byte array,
    starting at index position \a pos. If \a len is negative, or if
    \a pos + \a len is beyond the end of the array, all bytes from \a pos to
    the end are returned. If \a pos is negative, it is treated as 0 and
    \a len is reduced accordingly. If \a pos is beyond the end of the array,
    an empty, non-null byte array is returned.

    \sa left(), right(), sliced()
*/
QByteArray QByteArray::mid(qsizetype pos, qsizetype len) const {
    if (pos < 0) {
        if (len >= 0)
            len += pos;
        pos = 0;
    }
    if (pos > size() || len == 0)
        return QByteArray(static_cast<const char *>(""), 0);
    if (len < 0 || pos + len > size())
        len = size() - pos;
    return QByteArray(d.data() + pos, len);
}

/*!
    Returns a byte array with whitespace removed from the start and the end.
    Whitespace means the ASCII characters space, \c{'\t'}, \c{'\n'},
    \c{'\v'}, \c{'\f'} and \c{'\r'}.

    \sa simplified()
*/
QByteArray QByteArray::trimmed() const {
    qsizetype b = 0, e = size();
    while (b < e && isSpaceAscii(d[static_cast<std::size_t>(b)]))
        ++b;
    while (e > b && isSpaceAscii(d[static_cast<std::size_t>(e - 1)]))
        --e;
    return mid(b, e - b);
}

/*!
    Returns a byte array with whitespace removed from the start and the end,
    and with each sequence of internal whitespace replaced with a single
    space. Whitespace means the ASCII characters space, \c{'\t'}, \c{'\n'},
    \c{'\v'}, \c{'\f'} and \c{'\r'}.

    \sa trimmed()
*/
QByteArray QByteArray::simplified() const {
    QByteArray r(static_cast<const char *>(""), 0);
    bool pendingSpace = false;
    for (char c : d) {
        if (isSpaceAscii(c)) {
            pendingSpace = !r.isEmpty();
        } else {
            if (pendingSpace)
                r.append(' ');
            pendingSpace = false;
            r.append(c);
        }
    }
    return r;
}

/*!
    Returns an uppercase copy of the byte array. Only the ASCII letters
    \c{a} to \c{z} are converted; all other bytes are left unchanged.

    \sa toLower()
*/
QByteArray QByteArray::toUpper() const {
    QByteArray r(*this);
    for (char &c : r.d)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 32);
    return r;
}

/*!
    Returns a lowercase copy of the byte array. Only the ASCII letters
    \c{A} to \c{Z} are converted; all other bytes are left unchanged.

    \sa toUpper()
*/
QByteArray QByteArray::toLower() const {
    QByteArray r(*this);
    for (char &c : r.d)
        c = lowerAscii(c);
    return r;
}

/*!
    Returns a copy of this byte array repeated \a times times. If \a times is
    less than 1, an empty byte array is returned.
*/
QByteArray QByteArray::repeated(qsizetype times) const {
    QByteArray r(static_cast<const char *>(""), 0);
    for (qsizetype i = 0; i < times; ++i)
        r.append(*this);
    return r;
}

/*!
    Returns a byte array of size \a width containing this byte array padded
    on the right with the byte \a fill. If this byte array is longer than
    \a width and \a truncate is \c true, the result is truncated to \a width
    bytes; otherwise it is returned unchanged.

    \sa rightJustified()
*/
QByteArray QByteArray::leftJustified(qsizetype width, char fill, bool truncate) const {
    QByteArray r(*this);
    if (r.size() < width)
        r.append(width - r.size(), fill);
    else if (truncate)
        r.truncate(width);
    return r;
}

/*!
    Returns a byte array of size \a width containing the byte \a fill
    followed by this byte array. If this byte array is longer than \a width
    and \a truncate is \c true, the result is truncated to the first
    \a width bytes; otherwise it is returned unchanged.

    \sa leftJustified()
*/
QByteArray QByteArray::rightJustified(qsizetype width, char fill, bool truncate) const {
    QByteArray r(*this);
    if (r.size() < width)
        r.prepend(QByteArray(width - r.size(), fill));
    else if (truncate)
        r.truncate(width);
    return r;
}

/*!
    Returns the index position of the first occurrence of the byte array
    \a a in this byte array, searching forward from index position \a from.
    A negative \a from counts from the end. Returns -1 if \a a is not found.

    \sa lastIndexOf(), contains(), count()
*/
qsizetype QByteArray::indexOf(const QByteArray &a, qsizetype from) const {
    if (from < 0)
        from = qMax<qsizetype>(0, from + size());
    const auto p = d.find(a.d, static_cast<std::size_t>(from));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

/*!
    \overload

    Returns the index position of the first occurrence of the byte \a c in
    this byte array, searching forward from index position \a from. Returns
    -1 if \a c is not found.
*/
qsizetype QByteArray::indexOf(char c, qsizetype from) const {
    if (from < 0)
        from = qMax<qsizetype>(0, from + size());
    const auto p = d.find(c, static_cast<std::size_t>(from));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

/*!
    Returns the index position of the last occurrence of the byte array
    \a a in this byte array, searching backward from index position \a from.
    A negative \a from counts from the end, so the default of -1 starts at
    the last byte. Returns -1 if \a a is not found.

    \sa indexOf(), contains()
*/
qsizetype QByteArray::lastIndexOf(const QByteArray &a, qsizetype from) const {
    const qsizetype pos = from < 0 ? from + size() : from;
    if (pos < 0)
        return -1;
    const auto p = d.rfind(a.d, static_cast<std::size_t>(pos));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

/*!
    \overload

    Returns the index position of the last occurrence of the byte \a c in
    this byte array, searching backward from index position \a from. Returns
    -1 if \a c is not found.
*/
qsizetype QByteArray::lastIndexOf(char c, qsizetype from) const {
    const qsizetype pos = from < 0 ? from + size() : from;
    if (pos < 0)
        return -1;
    const auto p = d.rfind(c, static_cast<std::size_t>(pos));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

/*!
    Returns the number of (potentially overlapping) occurrences of the byte
    array \a a in this byte array. Returns 0 if \a a is empty.

    \sa contains(), indexOf()
*/
qsizetype QByteArray::count(const QByteArray &a) const {
    if (a.isEmpty())
        return 0;
    qsizetype n = 0;
    for (std::size_t p = d.find(a.d); p != std::string::npos; p = d.find(a.d, p + 1))
        ++n;
    return n;
}

/*!
    \overload

    Returns the number of occurrences of the byte \a c in this byte array.
*/
qsizetype QByteArray::count(char c) const { return static_cast<qsizetype>(std::count(d.begin(), d.end(), c)); }

/*!
    Splits the byte array into subarrays wherever the byte \a sep occurs and
    returns the list of those arrays. Empty parts are kept, so an array
    without \a sep yields a list containing a copy of the whole array.

    \sa indexOf()
*/
QList<QByteArray> QByteArray::split(char sep) const {
    QList<QByteArray> r;
    std::size_t start = 0;
    for (;;) {
        const auto p = d.find(sep, start);
        if (p == std::string::npos) {
            r.append(QByteArray(d.data() + start, static_cast<qsizetype>(d.size() - start)));
            return r;
        }
        r.append(QByteArray(d.data() + start, static_cast<qsizetype>(p - start)));
        start = p + 1;
    }
}

/*!
    Returns a hex encoded copy of the byte array, using lowercase digits. If
    \a separator is not \c{'\0'}, it is inserted between the hex digits of
    consecutive bytes.

    \sa fromHex()
*/
QByteArray QByteArray::toHex(char separator) const {
    static const char digits[] = "0123456789abcdef";
    QByteArray r(static_cast<const char *>(""), 0);
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (separator && i)
            r.append(separator);
        const auto c = static_cast<unsigned char>(d[i]);
        r.append(digits[c >> 4]);
        r.append(digits[c & 15]);
    }
    return r;
}

/*!
    Returns a decoded copy of the hex encoded array \a hex. Bytes that are
    not hexadecimal digits are skipped, and a trailing odd digit is ignored.

    \sa toHex()
*/
QByteArray QByteArray::fromHex(const QByteArray &hex) {
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    QByteArray r(static_cast<const char *>(""), 0);
    int hi = -1;
    for (char c : hex.d) {
        const int v = val(c);
        if (v < 0)
            continue;
        if (hi < 0) {
            hi = v;
        } else {
            r.append(static_cast<char>(hi * 16 + v));
            hi = -1;
        }
    }
    return r;
}

/*!
    Returns a copy of the byte array encoded as Base64, using the alphabet
    and padding selected by \a options.

    \sa fromBase64(), Base64Option
*/
QByteArray QByteArray::toBase64(Base64Options options) const {
    const char *std64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *url64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const char *tbl = (options & Base64UrlEncoding) ? url64 : std64;
    const bool pad = !(options & OmitTrailingEquals);
    QByteArray r(static_cast<const char *>(""), 0);
    std::size_t i = 0;
    const auto byteAt = [this](std::size_t k) { return static_cast<unsigned>(static_cast<unsigned char>(d[k])); };
    for (; i + 2 < d.size(); i += 3) {
        const unsigned v = (byteAt(i) << 16) | (byteAt(i + 1) << 8) | byteAt(i + 2);
        r.append(tbl[v >> 18]);
        r.append(tbl[(v >> 12) & 63]);
        r.append(tbl[(v >> 6) & 63]);
        r.append(tbl[v & 63]);
    }
    const std::size_t rem = d.size() - i;
    if (rem) {
        unsigned v = byteAt(i) << 16;
        if (rem == 2)
            v |= byteAt(i + 1) << 8;
        r.append(tbl[v >> 18]);
        r.append(tbl[(v >> 12) & 63]);
        if (rem == 2)
            r.append(tbl[(v >> 6) & 63]);
        if (pad)
            r.append(rem == 2 ? "=" : "==");
    }
    return r;
}

/*!
    Returns the decoded copy of the Base64 array \a base64. Both the
    standard and the URL-safe alphabets are accepted, and bytes outside the
    alphabet, including \c{'='} padding, are skipped. The \a options argument
    is accepted for compatibility with Qt but has no effect.

    \sa toBase64()
*/
QByteArray QByteArray::fromBase64(const QByteArray &in, Base64Options options) {
    return fromBase64Encoding(in, options).decoded;
}

/*!
    \since 5.15
    Decodes \a base64 with the alphabet and error handling selected by
    \a options, and returns the bytes with the status of the decoding.

    By default characters outside the alphabet are skipped, as in
    fromBase64(). With \c AbortOnBase64DecodingErrors, only the alphabet
    chosen by \c Base64Encoding or \c Base64UrlEncoding is accepted, \c{=}
    may only pad the end, and the status tells what was wrong; the decoded
    bytes are then empty.

    \sa fromBase64(), toBase64()
*/
QByteArray::FromBase64Result QByteArray::fromBase64Encoding(const QByteArray &in, Base64Options options) {
    const bool strict = options.testFlag(AbortOnBase64DecodingErrors);
    const bool url = options.testFlag(Base64UrlEncoding);
    auto val = [strict, url](char c) -> int {
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' && c <= '9')
            return c - '0' + 52;
        if ((c == '+' && (!strict || !url)) || (c == '-' && (!strict || url)))
            return 62;
        if ((c == '/' && (!strict || !url)) || (c == '_' && (!strict || url)))
            return 63;
        return -1;
    };
    FromBase64Result result;
    auto fail = [&result](Base64DecodingStatus status) {
        result.decoded = QByteArray();
        result.decodingStatus = status;
        return result;
    };
    std::size_t end = in.d.size();
    std::size_t padding = 0;
    if (strict) {
        while (end > 0 && in.d[end - 1] == '=') {
            --end;
            ++padding;
        }
    }
    QByteArray r(static_cast<const char *>(""), 0);
    unsigned acc = 0;
    int bits = 0;
    std::size_t digits = 0;
    for (std::size_t i = 0; i < end; ++i) {
        const int v = val(in.d[i]);
        if (v < 0) {
            if (!strict)
                continue;
            return fail(in.d[i] == '=' ? Base64DecodingStatus::IllegalPadding : Base64DecodingStatus::IllegalCharacter);
        }
        ++digits;
        acc = ((acc << 6) | static_cast<unsigned>(v)) & 0xffffff;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            r.append(static_cast<char>((acc >> bits) & 0xff));
        }
    }
    if (strict) {
        if (digits % 4 == 1)
            return fail(Base64DecodingStatus::IllegalInputLength);
        if (padding > 0 && (digits % 4 == 0 || (digits + padding) % 4 != 0))
            return fail(Base64DecodingStatus::IllegalPadding);
    }
    result.decoded = r;
    return result;
}

/*!
    \since 5.12
    Returns \c true if the byte array contains no lowercase ASCII letters,
    that is, if it equals its toUpper().

    \sa isLower(), toUpper()
*/
bool QByteArray::isUpper() const {
    return std::none_of(d.begin(), d.end(), [](char c) { return c >= 'a' && c <= 'z'; });
}

/*!
    \since 5.12
    Returns \c true if the byte array contains no uppercase ASCII letters,
    that is, if it equals its toLower().

    \sa isUpper(), toLower()
*/
bool QByteArray::isLower() const {
    return std::none_of(d.begin(), d.end(), [](char c) { return c >= 'A' && c <= 'Z'; });
}

/*!
    \since 6.4
    Returns a copy of the byte array with each \a percent followed by two hex
    digits replaced by the byte they encode. Same as
    fromPercentEncoding(*this, \a percent).

    \sa fromPercentEncoding()
*/
QByteArray QByteArray::percentDecoded(char percent) const { return fromPercentEncoding(*this, percent); }

/*!
    Returns a percent-encoded copy of the byte array. The unreserved
    characters \c{A-Z a-z 0-9 - . _ ~} and the bytes in \a exclude are left
    as they are, unless they also occur in \a include. Every other byte is
    replaced by \a percent followed by two uppercase hex digits.

    \sa fromPercentEncoding()
*/
QByteArray QByteArray::toPercentEncoding(const QByteArray &exclude, const QByteArray &include, char percent) const {
    static const char hex[] = "0123456789ABCDEF";
    QByteArray r(static_cast<const char *>(""), 0);
    for (char c : d) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                c == '-' || c == '.' || c == '_' || c == '~';
        if ((unreserved || exclude.contains(c)) && !include.contains(c)) {
            r.append(c);
        } else {
            r.append(percent);
            r.append(hex[(static_cast<unsigned char>(c)) >> 4]);
            r.append(hex[static_cast<unsigned char>(c) & 15]);
        }
    }
    return r;
}

/*!
    Returns a decoded copy of the percent-encoded array \a pctEncoded. Each
    \a percent byte followed by two hex digits is replaced by the byte they
    encode; anything else is copied unchanged.

    \sa toPercentEncoding()
*/
QByteArray QByteArray::fromPercentEncoding(const QByteArray &in, char percent) {
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    QByteArray r(static_cast<const char *>(""), 0);
    for (std::size_t i = 0; i < in.d.size(); ++i) {
        if (in.d[i] == percent && i + 2 < in.d.size() + 0 && val(in.d[i + 1]) >= 0 && val(in.d[i + 2]) >= 0) {
            r.append(static_cast<char>(val(in.d[i + 1]) * 16 + val(in.d[i + 2])));
            i += 2;
        } else {
            r.append(in.d[i]);
        }
    }
    return r;
}

/*!
    Returns a byte array containing the string equivalent of the number \a n
    in the given \a base, which should be between 2 and 36. The default base
    is 10.

    \sa setNum(), toLongLong()
*/
QByteArray QByteArray::number(qlonglong n, int base) { return QByteArray(QtPrivate::signedToString(n, base)); }
/*!
    \overload

    Returns a byte array containing the string equivalent of the unsigned
    number \a n in the given \a base.
*/
QByteArray QByteArray::number(qulonglong n, int base) { return QByteArray(QtPrivate::intToString(n, false, base)); }
/*!
    \overload

    Returns a byte array containing the string equivalent of the
    floating-point number \a n. \a format is one of \c{'e'}, \c{'E'},
    \c{'f'}, \c{'g'} or \c{'G'}, and \a precision is the number of digits
    after the decimal point (or of significant digits for \c{'g'} and
    \c{'G'}).
*/
QByteArray QByteArray::number(double n, char format, int precision) {
    return QByteArray(QtPrivate::formatDouble(n, format, precision));
}

/*!
    Returns the byte array converted to a \c{long long} using the given
    \a base, which must be between 2 and 36, or 0 to detect the base from
    the prefix. If \a ok is not \c nullptr, it is set to \c true if the
    conversion succeeded and to \c false otherwise. Returns 0 on failure.

    \sa number(), toULongLong(), toInt()
*/
qlonglong QByteArray::toLongLong(bool *ok, int base) const {
    long long v = 0;
    const bool good = QtPrivate::parseSigned(d, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

/*!
    Returns the byte array converted to an \c{unsigned long long} using the
    given \a base. If \a ok is not \c nullptr, it is set to \c true if the
    conversion succeeded and to \c false otherwise. Returns 0 on failure.

    \sa number(), toLongLong()
*/
qulonglong QByteArray::toULongLong(bool *ok, int base) const {
    unsigned long long v = 0;
    const bool good = QtPrivate::parseUnsigned(d, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

/*!
    Returns the byte array converted to an \c int using the given \a base.
    If \a ok is not \c nullptr, it is set to \c true if the conversion
    succeeded and the value fits in an \c int, and to \c false otherwise.
    Returns 0 on failure.

    \sa number(), toLongLong()
*/
int QByteArray::toInt(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= INT_MIN && v <= INT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<int>(v) : 0;
}

/*!
    Returns the byte array converted to an \c{unsigned int} using the given
    \a base. If \a ok is not \c nullptr, it is set to \c true if the
    conversion succeeded and the value fits in an \c{unsigned int}, and to
    \c false otherwise. Returns 0 on failure.

    \sa number(), toULongLong()
*/
uint QByteArray::toUInt(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= UINT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<uint>(v) : 0;
}

/*!
    Returns the byte array converted to a \c long using the given \a base.
    If \a ok is not \c nullptr, it is set to \c true if the conversion
    succeeded and the value fits in a \c long, and to \c false otherwise.
    Returns 0 on failure.

    \sa number(), toLongLong()
*/
long QByteArray::toLong(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= LONG_MIN && v <= LONG_MAX;
    if (ok) *ok = good;
    return good ? static_cast<long>(v) : 0;
}

/*!
    Returns the byte array converted to a \c short using the given \a base.
    If \a ok is not \c nullptr, it is set to \c true if the conversion
    succeeded and the value fits in a \c short, and to \c false otherwise.
    Returns 0 on failure.

    \sa number(), toLongLong()
*/
short QByteArray::toShort(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= SHRT_MIN && v <= SHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<short>(v) : 0;
}

/*!
    Returns the byte array converted to an \c{unsigned short} using the given
    \a base. If \a ok is not \c nullptr, it is set to \c true if the
    conversion succeeded and the value fits in an \c{unsigned short}, and to
    \c false otherwise. Returns 0 on failure.

    \sa number(), toULongLong()
*/
ushort QByteArray::toUShort(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= USHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<ushort>(v) : 0;
}

/*!
    Returns the byte array converted to a \c double. If \a ok is not
    \c nullptr, it is set to \c true if the conversion succeeded and to
    \c false otherwise. Returns 0.0 on failure.

    \sa number(), toFloat()
*/
double QByteArray::toDouble(bool *ok) const {
    double v = 0;
    const bool good = QtPrivate::parseDouble(d, v);
    if (ok) *ok = good;
    return good ? v : 0.0;
}

/*!
    Returns the byte array converted to a \c float. If \a ok is not
    \c nullptr, it is set to \c true if the conversion succeeded and to
    \c false otherwise. Returns 0.0 on failure.

    \sa toDouble(), number()
*/
float QByteArray::toFloat(bool *ok) const { return static_cast<float>(toDouble(ok)); }

/*!
    Compares this byte array with \a o, using the case sensitivity \a cs, and
    returns a negative value, 0 or a positive value if this array sorts
    before, equal to or after \a o. For \c Qt::CaseInsensitive, only ASCII
    letters are folded.
*/
int QByteArray::compare(const QByteArray &o, Qt::CaseSensitivity cs) const {
    if (cs == Qt::CaseSensitive)
        return d.compare(o.d);
    return toLower().d.compare(o.toLower().d);
}
