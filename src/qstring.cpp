// MIT License - see LICENSE
#include <QtCore/qstring.h>

#include "numparse_p.h"

#include <algorithm>
#include <climits>
#include <cstdarg>

namespace {

std::u16string decodeUtf8(const char *s, std::size_t n) {
    std::u16string out;
    out.reserve(n);
    for (std::size_t i = 0; i < n;) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0xfffd;
        std::size_t len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xe0) == 0xc0 && i + 1 < n && (s[i + 1] & 0xc0) == 0x80) {
            cp = ((c & 0x1f) << 6) | (s[i + 1] & 0x3f);
            len = 2;
            if (cp < 0x80) cp = 0xfffd;
        } else if ((c & 0xf0) == 0xe0 && i + 2 < n && (s[i + 1] & 0xc0) == 0x80 && (s[i + 2] & 0xc0) == 0x80) {
            cp = ((c & 0x0f) << 12) | ((s[i + 1] & 0x3f) << 6) | (s[i + 2] & 0x3f);
            len = 3;
            if (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff)) cp = 0xfffd;
        } else if ((c & 0xf8) == 0xf0 && i + 3 < n && (s[i + 1] & 0xc0) == 0x80 && (s[i + 2] & 0xc0) == 0x80 &&
                   (s[i + 3] & 0xc0) == 0x80) {
            cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3f) << 12) | ((s[i + 2] & 0x3f) << 6) | (s[i + 3] & 0x3f);
            len = 4;
            if (cp < 0x10000 || cp > 0x10ffff) cp = 0xfffd;
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xd800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xdc00 + (cp & 0x3ff)));
        } else {
            out.push_back(static_cast<char16_t>(cp));
        }
        i += len;
    }
    return out;
}

std::string encodeUtf8(const std::u16string &s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        char32_t cp = s[i];
        if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < s.size() && s[i + 1] >= 0xdc00 && s[i + 1] <= 0xdfff) {
            cp = 0x10000 + ((cp - 0xd800) << 10) + (s[i + 1] - 0xdc00);
            ++i;
        } else if (cp >= 0xd800 && cp <= 0xdfff) {
            cp = 0xfffd;
        }
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
    return out;
}

std::u16string foldCase(const std::u16string &s) {
    std::u16string r(s);
    for (char16_t &c : r)
        c = QChar(c).toLower().unicode();
    return r;
}

bool asciiOnly(const std::u16string &s, std::string &out) {
    out.clear();
    for (char16_t c : s) {
        if (c > 0x7f)
            return false;
        out.push_back(static_cast<char>(c));
    }
    return true;
}

std::u16string ascii16(const std::string &s) { return std::u16string(s.begin(), s.end()); }

std::u16string pad(std::u16string s, int fieldWidth, char16_t fill, bool numeric = false) {
    const std::size_t w = static_cast<std::size_t>(fieldWidth < 0 ? -fieldWidth : fieldWidth);
    if (s.size() >= w)
        return s;
    const std::size_t n = w - s.size();
    if (fieldWidth < 0)
        return s + std::u16string(n, u' ');
    if (numeric && fill == u'0' && !s.empty() && (s[0] == u'-' || s[0] == u'+'))
        return s.substr(0, 1) + std::u16string(n, fill) + s.substr(1);
    return std::u16string(n, fill) + s;
}

} // namespace

QString::QString(const char *utf8) {
    if (!utf8)
        return;
    m_null = false;
    d = decodeUtf8(utf8, std::char_traits<char>::length(utf8));
}

QString::QString(const char16_t *u, qsizetype size) {
    if (!u)
        return;
    m_null = false;
    d.assign(u, size < 0 ? std::char_traits<char16_t>::length(u) : static_cast<std::size_t>(size));
}

QString::QString(const QChar *u, qsizetype size) {
    if (!u)
        return;
    m_null = false;
    d.assign(reinterpret_cast<const char16_t *>(u), static_cast<std::size_t>(size));
}

QString::QString(QLatin1StringView s) : m_null(false) {
    d.reserve(static_cast<std::size_t>(s.size()));
    for (qsizetype i = 0; i < s.size(); ++i)
        d.push_back(static_cast<unsigned char>(s.data()[i]));
}

QString::QString(const QByteArray &utf8) : m_null(utf8.isNull()) { d = decodeUtf8(utf8.constData(), static_cast<std::size_t>(utf8.size())); }

QString QString::fromUtf8(const char *s, qsizetype size) {
    if (!s)
        return QString();
    return QString(decodeUtf8(s, size < 0 ? std::char_traits<char>::length(s) : static_cast<std::size_t>(size)));
}

QString QString::fromLatin1(const char *s, qsizetype size) {
    if (!s)
        return QString();
    return QString(QLatin1StringView(s, size < 0 ? static_cast<qsizetype>(std::char_traits<char>::length(s)) : size));
}

QByteArray QString::toUtf8() const {
    if (isNull())
        return QByteArray();
    return QByteArray(encodeUtf8(d));
}

QByteArray QString::toLatin1() const {
    std::string r;
    r.reserve(d.size());
    for (char16_t c : d)
        r.push_back(c > 0xff ? '?' : static_cast<char>(c));
    return QByteArray(std::move(r));
}

QString &QString::fill(QChar c, qsizetype n) {
    if (n >= 0)
        d.assign(static_cast<std::size_t>(n), c.unicode());
    else
        std::fill(d.begin(), d.end(), c.unicode());
    m_null = false;
    return *this;
}

QString &QString::remove(qsizetype pos, qsizetype len) {
    if (pos < 0 || pos >= size() || len <= 0)
        return *this;
    d.erase(static_cast<std::size_t>(pos), static_cast<std::size_t>(len));
    return *this;
}

QString &QString::replace(qsizetype pos, qsizetype len, const QString &after) {
    if (pos < 0 || pos > size())
        return *this;
    d.replace(static_cast<std::size_t>(pos), static_cast<std::size_t>(len < 0 ? 0 : len), after.d);
    return *this;
}

QString &QString::replace(const QString &before, const QString &after, Qt::CaseSensitivity cs) {
    if (before.isEmpty())
        return *this;
    const std::u16string hay = cs == Qt::CaseSensitive ? d : foldCase(d);
    const std::u16string needle = cs == Qt::CaseSensitive ? before.d : foldCase(before.d);
    std::u16string out;
    std::size_t last = 0, pos;
    while ((pos = hay.find(needle, last)) != std::u16string::npos) {
        out.append(d, last, pos - last);
        out.append(after.d);
        last = pos + needle.size();
    }
    out.append(d, last, std::u16string::npos);
    d.swap(out);
    return *this;
}

QString QString::mid(qsizetype pos, qsizetype len) const {
    if (pos < 0) {
        if (len >= 0)
            len += pos;
        pos = 0;
    }
    if (pos > size() || len == 0)
        return QString(QLatin1StringView("", 0));
    if (len < 0 || pos + len > size())
        len = size() - pos;
    return QString(d.substr(static_cast<std::size_t>(pos), static_cast<std::size_t>(len)));
}

QString QString::trimmed() const {
    qsizetype b = 0, e = size();
    while (b < e && at(b).isSpace())
        ++b;
    while (e > b && at(e - 1).isSpace())
        --e;
    return mid(b, e - b);
}

QString QString::simplified() const {
    std::u16string r;
    bool pending = false;
    for (char16_t c : d) {
        if (QChar(c).isSpace()) {
            pending = !r.empty();
        } else {
            if (pending)
                r.push_back(u' ');
            pending = false;
            r.push_back(c);
        }
    }
    return QString(r);
}

QString QString::toUpper() const {
    std::u16string r(d);
    for (char16_t &c : r)
        c = QChar(c).toUpper().unicode();
    return QString(r);
}

QString QString::toLower() const { return QString(foldCase(d)); }

QString QString::repeated(qsizetype times) const {
    std::u16string r;
    for (qsizetype i = 0; i < times; ++i)
        r += d;
    return QString(r);
}

QString QString::leftJustified(qsizetype width, QChar fill, bool truncate) const {
    QString r(*this);
    if (r.size() < width)
        r.d.append(static_cast<std::size_t>(width - r.size()), fill.unicode());
    else if (truncate)
        r.truncate(width);
    return r;
}

QString QString::rightJustified(qsizetype width, QChar fill, bool truncate) const {
    QString r(*this);
    if (r.size() < width)
        r.d.insert(0, static_cast<std::size_t>(width - r.size()), fill.unicode());
    else if (truncate)
        r.truncate(width);
    return r;
}

QString QString::section(const QString &sep, qsizetype start, qsizetype end) const {
    const QStringList parts = split(sep);
    const qsizetype n = parts.size();
    if (start < 0)
        start += n;
    if (end < 0)
        end += n;
    if (start < 0)
        start = 0;
    if (end >= n)
        end = n - 1;
    if (start > end || start >= n)
        return QString(QLatin1StringView("", 0));
    return QStringList(parts.mid(start, end - start + 1)).join(sep);
}

QString QString::section(QChar sep, qsizetype start, qsizetype end) const { return section(QString(sep), start, end); }

bool QString::startsWith(const QString &s, Qt::CaseSensitivity cs) const {
    if (s.size() > size())
        return false;
    return left(s.size()).compare(s, cs) == 0;
}

bool QString::endsWith(const QString &s, Qt::CaseSensitivity cs) const {
    if (s.size() > size())
        return false;
    return right(s.size()).compare(s, cs) == 0;
}

qsizetype QString::indexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
    if (from < 0)
        from = qMax<qsizetype>(0, from + size());
    const auto p = cs == Qt::CaseSensitive ? d.find(s.d, static_cast<std::size_t>(from))
                                           : foldCase(d).find(foldCase(s.d), static_cast<std::size_t>(from));
    return p == std::u16string::npos ? -1 : static_cast<qsizetype>(p);
}

qsizetype QString::lastIndexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
    const qsizetype pos = from < 0 ? from + size() : from;
    if (pos < 0)
        return -1;
    const auto p = cs == Qt::CaseSensitive ? d.rfind(s.d, static_cast<std::size_t>(pos))
                                           : foldCase(d).rfind(foldCase(s.d), static_cast<std::size_t>(pos));
    return p == std::u16string::npos ? -1 : static_cast<qsizetype>(p);
}

qsizetype QString::count(const QString &s, Qt::CaseSensitivity cs) const {
    if (s.isEmpty())
        return 0;
    const std::u16string hay = cs == Qt::CaseSensitive ? d : foldCase(d);
    const std::u16string needle = cs == Qt::CaseSensitive ? s.d : foldCase(s.d);
    qsizetype n = 0;
    for (auto p = hay.find(needle); p != std::u16string::npos; p = hay.find(needle, p + 1))
        ++n;
    return n;
}

QStringList QString::split(const QString &sep, Qt::SplitBehavior behavior, Qt::CaseSensitivity cs) const {
    QStringList r;
    const bool skip = behavior.testFlag(Qt::SkipEmptyParts);
    auto add = [&](std::u16string part) {
        if (!(skip && part.empty()))
            r.append(QString(part));
    };
    if (sep.isEmpty()) {
        add(std::u16string());
        for (char16_t c : d)
            add(std::u16string(1, c));
        add(std::u16string());
        return r;
    }
    const std::u16string hay = cs == Qt::CaseSensitive ? d : foldCase(d);
    const std::u16string needle = cs == Qt::CaseSensitive ? sep.d : foldCase(sep.d);
    std::size_t start = 0;
    for (;;) {
        const auto p = hay.find(needle, start);
        if (p == std::u16string::npos) {
            add(d.substr(start));
            return r;
        }
        add(d.substr(start, p - start));
        start = p + needle.size();
    }
}

QStringList QString::split(QChar sep, Qt::SplitBehavior behavior, Qt::CaseSensitivity cs) const {
    return split(QString(sep), behavior, cs);
}

static std::u16string substituteLowest(const std::u16string &s, const std::u16string &rep) {
    auto parse = [&](std::size_t i, std::size_t &after) -> int {
        if (i >= s.size() || s[i] != u'%')
            return -1;
        std::size_t j = i + 1;
        if (j < s.size() && s[j] == u'L')
            ++j;
        if (j >= s.size() || s[j] < u'0' || s[j] > u'9')
            return -1;
        int v = s[j++] - u'0';
        if (j < s.size() && s[j] >= u'0' && s[j] <= u'9')
            v = v * 10 + (s[j++] - u'0');
        after = j;
        return v;
    };
    int lowest = INT_MAX;
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::size_t after;
        const int v = parse(i, after);
        if (v >= 0 && v < lowest)
            lowest = v;
    }
    if (lowest == INT_MAX)
        return s + u" " + rep;
    std::u16string out;
    for (std::size_t i = 0; i < s.size();) {
        std::size_t after;
        if (parse(i, after) == lowest) {
            out += rep;
            i = after;
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

QString QString::arg(const QString &a, int fieldWidth, QChar fillChar) const {
    return QString(substituteLowest(d, pad(a.d, fieldWidth, fillChar.unicode())));
}

QString QString::arg(qlonglong a, int fieldWidth, int base, QChar fillChar) const {
    return QString(substituteLowest(d, pad(ascii16(QtPrivate::signedToString(a, base)), fieldWidth, fillChar.unicode(), true)));
}

QString QString::arg(qulonglong a, int fieldWidth, int base, QChar fillChar) const {
    return QString(substituteLowest(d, pad(ascii16(QtPrivate::intToString(a, false, base)), fieldWidth, fillChar.unicode(), true)));
}

QString QString::arg(double a, int fieldWidth, char format, int precision, QChar fillChar) const {
    return QString(substituteLowest(d, pad(ascii16(QtPrivate::formatDouble(a, format, precision)), fieldWidth, fillChar.unicode(), true)));
}

QString QString::number(qlonglong n, int base) { return QString(ascii16(QtPrivate::signedToString(n, base))); }
QString QString::number(qulonglong n, int base) { return QString(ascii16(QtPrivate::intToString(n, false, base))); }
QString QString::number(double n, char format, int precision) {
    return QString(ascii16(QtPrivate::formatDouble(n, format, precision)));
}

qlonglong QString::toLongLong(bool *ok, int base) const {
    std::string a;
    long long v = 0;
    const bool good = asciiOnly(d, a) && QtPrivate::parseSigned(a, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

qulonglong QString::toULongLong(bool *ok, int base) const {
    std::string a;
    unsigned long long v = 0;
    const bool good = asciiOnly(d, a) && QtPrivate::parseUnsigned(a, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

int QString::toInt(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= INT_MIN && v <= INT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<int>(v) : 0;
}

uint QString::toUInt(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= UINT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<uint>(v) : 0;
}

long QString::toLong(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= LONG_MIN && v <= LONG_MAX;
    if (ok) *ok = good;
    return good ? static_cast<long>(v) : 0;
}

short QString::toShort(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= SHRT_MIN && v <= SHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<short>(v) : 0;
}

ushort QString::toUShort(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= USHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<ushort>(v) : 0;
}

double QString::toDouble(bool *ok) const {
    std::string a;
    double v = 0;
    const bool good = asciiOnly(d, a) && QtPrivate::parseDouble(a, v);
    if (ok) *ok = good;
    return good ? v : 0.0;
}

float QString::toFloat(bool *ok) const { return static_cast<float>(toDouble(ok)); }

int QString::compare(const QString &o, Qt::CaseSensitivity cs) const {
    return cs == Qt::CaseSensitive ? d.compare(o.d) : foldCase(d).compare(foldCase(o.d));
}

QString QString::asprintf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    const int n = std::vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    std::string buf(static_cast<std::size_t>(n < 0 ? 0 : n), '\0');
    if (n > 0)
        std::vsnprintf(buf.data(), buf.size() + 1, fmt, ap2);
    va_end(ap2);
    return fromUtf8(buf.data(), static_cast<qsizetype>(buf.size()));
}
