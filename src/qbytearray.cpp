// MIT License - see LICENSE
#include <QtCore/qbytearray.h>

#include "numparse_p.h"

#include <QtCore/qlist.h>

#include <algorithm>
#include <climits>

static char lowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }
static bool isSpaceAscii(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

QByteArray::QByteArray(const char *data, qsizetype size) {
    if (!data)
        return;
    m_null = false;
    d.assign(data, size < 0 ? std::char_traits<char>::length(data) : static_cast<std::size_t>(size));
}

QByteArray &QByteArray::fill(char c, qsizetype n) {
    if (n >= 0)
        d.assign(static_cast<std::size_t>(n), c);
    else
        std::fill(d.begin(), d.end(), c);
    m_null = false;
    return *this;
}

QByteArray &QByteArray::remove(qsizetype pos, qsizetype len) {
    if (pos < 0 || pos >= size() || len <= 0)
        return *this;
    d.erase(static_cast<std::size_t>(pos), static_cast<std::size_t>(len));
    return *this;
}

QByteArray &QByteArray::replace(qsizetype pos, qsizetype len, const QByteArray &after) {
    if (pos < 0 || pos > size())
        return *this;
    d.replace(static_cast<std::size_t>(pos), static_cast<std::size_t>(len < 0 ? 0 : len), after.d);
    return *this;
}

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

QByteArray &QByteArray::replace(char before, char after) {
    std::replace(d.begin(), d.end(), before, after);
    return *this;
}

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

QByteArray QByteArray::trimmed() const {
    qsizetype b = 0, e = size();
    while (b < e && isSpaceAscii(d[static_cast<std::size_t>(b)]))
        ++b;
    while (e > b && isSpaceAscii(d[static_cast<std::size_t>(e - 1)]))
        --e;
    return mid(b, e - b);
}

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

QByteArray QByteArray::toUpper() const {
    QByteArray r(*this);
    for (char &c : r.d)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 32);
    return r;
}

QByteArray QByteArray::toLower() const {
    QByteArray r(*this);
    for (char &c : r.d)
        c = lowerAscii(c);
    return r;
}

QByteArray QByteArray::repeated(qsizetype times) const {
    QByteArray r(static_cast<const char *>(""), 0);
    for (qsizetype i = 0; i < times; ++i)
        r.append(*this);
    return r;
}

QByteArray QByteArray::leftJustified(qsizetype width, char fill, bool truncate) const {
    QByteArray r(*this);
    if (r.size() < width)
        r.append(width - r.size(), fill);
    else if (truncate)
        r.truncate(width);
    return r;
}

QByteArray QByteArray::rightJustified(qsizetype width, char fill, bool truncate) const {
    QByteArray r(*this);
    if (r.size() < width)
        r.prepend(QByteArray(width - r.size(), fill));
    else if (truncate)
        r.truncate(width);
    return r;
}

qsizetype QByteArray::indexOf(const QByteArray &a, qsizetype from) const {
    if (from < 0)
        from = qMax<qsizetype>(0, from + size());
    const auto p = d.find(a.d, static_cast<std::size_t>(from));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

qsizetype QByteArray::indexOf(char c, qsizetype from) const {
    if (from < 0)
        from = qMax<qsizetype>(0, from + size());
    const auto p = d.find(c, static_cast<std::size_t>(from));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

qsizetype QByteArray::lastIndexOf(const QByteArray &a, qsizetype from) const {
    const qsizetype pos = from < 0 ? from + size() : from;
    if (pos < 0)
        return -1;
    const auto p = d.rfind(a.d, static_cast<std::size_t>(pos));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

qsizetype QByteArray::lastIndexOf(char c, qsizetype from) const {
    const qsizetype pos = from < 0 ? from + size() : from;
    if (pos < 0)
        return -1;
    const auto p = d.rfind(c, static_cast<std::size_t>(pos));
    return p == std::string::npos ? -1 : static_cast<qsizetype>(p);
}

qsizetype QByteArray::count(const QByteArray &a) const {
    if (a.isEmpty())
        return 0;
    qsizetype n = 0;
    for (std::size_t p = d.find(a.d); p != std::string::npos; p = d.find(a.d, p + 1))
        ++n;
    return n;
}

qsizetype QByteArray::count(char c) const { return static_cast<qsizetype>(std::count(d.begin(), d.end(), c)); }

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

QByteArray QByteArray::toBase64(Base64Options options) const {
    const char *std64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *url64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const char *tbl = (options & Base64UrlEncoding) ? url64 : std64;
    const bool pad = !(options & OmitTrailingEquals);
    QByteArray r(static_cast<const char *>(""), 0);
    std::size_t i = 0;
    for (; i + 2 < d.size(); i += 3) {
        const unsigned v = (static_cast<unsigned char>(d[i]) << 16) | (static_cast<unsigned char>(d[i + 1]) << 8) |
                           static_cast<unsigned char>(d[i + 2]);
        r.append(tbl[v >> 18]);
        r.append(tbl[(v >> 12) & 63]);
        r.append(tbl[(v >> 6) & 63]);
        r.append(tbl[v & 63]);
    }
    const std::size_t rem = d.size() - i;
    if (rem) {
        unsigned v = static_cast<unsigned char>(d[i]) << 16;
        if (rem == 2)
            v |= static_cast<unsigned char>(d[i + 1]) << 8;
        r.append(tbl[v >> 18]);
        r.append(tbl[(v >> 12) & 63]);
        if (rem == 2)
            r.append(tbl[(v >> 6) & 63]);
        if (pad)
            r.append(rem == 2 ? "=" : "==");
    }
    return r;
}

QByteArray QByteArray::fromBase64(const QByteArray &in, Base64Options) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    QByteArray r(static_cast<const char *>(""), 0);
    unsigned acc = 0;
    int bits = 0;
    for (char c : in.d) {
        const int v = val(c);
        if (v < 0)
            continue;
        acc = (acc << 6) | static_cast<unsigned>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            r.append(static_cast<char>((acc >> bits) & 0xff));
        }
    }
    return r;
}

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

QByteArray QByteArray::number(qlonglong n, int base) { return QByteArray(QtPrivate::signedToString(n, base)); }
QByteArray QByteArray::number(qulonglong n, int base) { return QByteArray(QtPrivate::intToString(n, false, base)); }
QByteArray QByteArray::number(double n, char format, int precision) {
    return QByteArray(QtPrivate::formatDouble(n, format, precision));
}

qlonglong QByteArray::toLongLong(bool *ok, int base) const {
    long long v = 0;
    const bool good = QtPrivate::parseSigned(d, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

qulonglong QByteArray::toULongLong(bool *ok, int base) const {
    unsigned long long v = 0;
    const bool good = QtPrivate::parseUnsigned(d, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

int QByteArray::toInt(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= INT_MIN && v <= INT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<int>(v) : 0;
}

uint QByteArray::toUInt(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= UINT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<uint>(v) : 0;
}

long QByteArray::toLong(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= LONG_MIN && v <= LONG_MAX;
    if (ok) *ok = good;
    return good ? static_cast<long>(v) : 0;
}

short QByteArray::toShort(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= SHRT_MIN && v <= SHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<short>(v) : 0;
}

ushort QByteArray::toUShort(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= USHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<ushort>(v) : 0;
}

double QByteArray::toDouble(bool *ok) const {
    double v = 0;
    const bool good = QtPrivate::parseDouble(d, v);
    if (ok) *ok = good;
    return good ? v : 0.0;
}

float QByteArray::toFloat(bool *ok) const { return static_cast<float>(toDouble(ok)); }

int QByteArray::compare(const QByteArray &o, Qt::CaseSensitivity cs) const {
    if (cs == Qt::CaseSensitive)
        return d.compare(o.d);
    return toLower().d.compare(o.toLower().d);
}
