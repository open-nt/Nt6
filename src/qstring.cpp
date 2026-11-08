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

/*!
    \class QString
    \brief The QString class provides a Unicode character string.

    QString stores a string of 16-bit QChars (UTF-16 code units). It is the
    string type used throughout Nt. Strings are created from UTF-8 literals,
    Latin-1 data or UTF-16 data, and converted back with toUtf8(), toLatin1()
    and toStdString():

    \code
    QString s = QStringLiteral("Hello, %1!").arg("world");
    if (s.startsWith("Hello"))
        qDebug() << s.toUpper();
    \endcode

    A QString can be \e null (the result of the default constructor, see
    isNull()) or empty (no characters); a null string is also empty, and the
    two are interchangeable in comparisons.

    Unlike in Qt the string data is not implicitly shared: every copy of a
    QString copies the characters.

    \differences
    The string is a thin wrapper around \c std::u16string and copies on
    assignment; there is no implicit sharing, so fromRawData() copies its
    argument. Case conversion, case-insensitive comparison and QChar
    classification cover ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic
    only. localeAwareCompare() uses QCollator, whose collation is
    simplified; toLocal8Bit() and fromLocal8Bit() use UTF-8, and a \c{%L1}
    place marker in arg() is handled like \c{%1}. count() returns 0 for an
    empty argument. Functions taking QStringView or QLatin1StringView are
    mostly missing; use the QString overloads. section() has no
    QRegularExpression overload. Not provided are \c normalized(),
    \c toHtmlEscaped(),
    \c toWCharArray(), \c fromWCharArray(), \c toUcs4(), \c fromUcs4(),
    \c setRawData(), \c setUnicode() and \c setUtf16(), nor QStringBuilder
    concatenation.
    \enddifferences

    \sa QChar, QStringList, QStringView, QByteArray
*/

/*!
    Constructs a string from the null-terminated UTF-8 string \a utf8.
    Invalid sequences are replaced by U+FFFD. If \a utf8 is \c nullptr the
    string is null.

    \sa fromUtf8()
*/
QString::QString(const char *utf8) {
    if (!utf8)
        return;
    m_null = false;
    d = decodeUtf8(utf8, std::char_traits<char>::length(utf8));
}

/*!
    \overload

    Constructs a string from the first \a size UTF-16 code units of \a u. If
    \a size is negative, \a u must be null-terminated. If \a u is \c nullptr
    the string is null.

    \sa fromUtf16()
*/
QString::QString(const char16_t *u, qsizetype size) {
    if (!u)
        return;
    m_null = false;
    d.assign(u, size < 0 ? std::char_traits<char16_t>::length(u) : static_cast<std::size_t>(size));
}

/*!
    \overload

    Constructs a string from the first \a size characters of \a u. If \a u is
    \c nullptr the string is null.
*/
QString::QString(const QChar *u, qsizetype size) {
    if (!u)
        return;
    m_null = false;
    d.assign(reinterpret_cast<const char16_t *>(u), static_cast<std::size_t>(size));
}

/*!
    \overload

    Constructs a string from the Latin-1 string \a s. The result is not null,
    even if \a s is empty.

    \sa fromLatin1()
*/
QString::QString(QLatin1StringView s) : m_null(false) {
    d.reserve(static_cast<std::size_t>(s.size()));
    for (qsizetype i = 0; i < s.size(); ++i)
        d.push_back(static_cast<unsigned char>(s.data()[i]));
}

/*!
    \overload

    Constructs a string from the UTF-8 data in \a utf8. The string is null if
    \a utf8 is null.

    \sa fromUtf8()
*/
QString::QString(const QByteArray &utf8)
    : d(decodeUtf8(utf8.constData(), static_cast<std::size_t>(utf8.size()))), m_null(utf8.isNull()) {}

/*!
    Returns a string decoded from the first \a size bytes of the UTF-8 string
    \a s. If \a size is negative, \a s must be null-terminated. Invalid
    sequences are replaced by U+FFFD. The result is null if \a s is
    \c nullptr.

    \sa toUtf8(), fromLatin1()
*/
QString QString::fromUtf8(const char *s, qsizetype size) {
    if (!s)
        return QString();
    return QString(decodeUtf8(s, size < 0 ? std::char_traits<char>::length(s) : static_cast<std::size_t>(size)));
}

/*!
    Returns a string decoded from the first \a size bytes of the Latin-1
    string \a s. If \a size is negative, \a s must be null-terminated. The
    result is null if \a s is \c nullptr.

    \sa toLatin1(), fromUtf8()
*/
QString QString::fromLatin1(const char *s, qsizetype size) {
    if (!s)
        return QString();
    return QString(QLatin1StringView(s, size < 0 ? static_cast<qsizetype>(std::char_traits<char>::length(s)) : size));
}

/*!
    Returns the string as a UTF-8 encoded byte array. Unpaired surrogates are
    encoded as U+FFFD. A null string gives a null byte array.

    \sa fromUtf8(), toLatin1()
*/
QByteArray QString::toUtf8() const {
    if (isNull())
        return QByteArray();
    return QByteArray(encodeUtf8(d));
}

/*!
    Returns the string as a Latin-1 encoded byte array. Characters outside the
    range U+0000 to U+00FF are replaced by \c{'?'}.

    \sa fromLatin1(), toUtf8()
*/
QByteArray QString::toLatin1() const {
    std::string r;
    r.reserve(d.size());
    for (char16_t c : d)
        r.push_back(c > 0xff ? '?' : static_cast<char>(c));
    return QByteArray(std::move(r));
}

/*!
    Sets every character of the string to \a c and returns a reference to it.
    If \a n is not negative the string is first resized to \a n characters;
    otherwise its size is unchanged. The string is no longer null afterwards.

    \sa resize()
*/
QString &QString::fill(QChar c, qsizetype n) {
    if (n >= 0)
        d.assign(static_cast<std::size_t>(n), c.unicode());
    else
        std::fill(d.begin(), d.end(), c.unicode());
    m_null = false;
    return *this;
}

/*!
    Removes \a len characters from the string starting at index position
    \a pos and returns a reference to the string. Nothing happens if \a pos is
    out of range or \a len is not positive.

    \sa insert(), replace(), chop()
*/
QString &QString::remove(qsizetype pos, qsizetype len) {
    if (pos < 0 || pos >= size() || len <= 0)
        return *this;
    d.erase(static_cast<std::size_t>(pos), static_cast<std::size_t>(len));
    return *this;
}

/*!
    Replaces \a len characters starting at index position \a pos with the
    string \a after and returns a reference to this string. If \a len extends
    past the end, the rest of the string is replaced. A negative \a len is
    treated as 0, which inserts \a after. Nothing happens if \a pos is out of
    range.

    \sa insert(), remove()
*/
QString &QString::replace(qsizetype pos, qsizetype len, const QString &after) {
    if (pos < 0 || pos > size())
        return *this;
    d.replace(static_cast<std::size_t>(pos), static_cast<std::size_t>(len < 0 ? 0 : len), after.d);
    return *this;
}

/*!
    \overload

    Replaces every occurrence of the string \a before with the string
    \a after, using the case sensitivity \a cs, and returns a reference to this
    string. Nothing happens if \a before is empty. Replaced text is not
    searched again.
*/
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

/*!
    Returns a string containing \a len characters starting at index position
    \a pos. If \a len is negative or extends past the end, the result extends
    to the end of the string. A negative \a pos is clamped to 0 and reduces
    \a len accordingly. If \a pos is past the end of the string, or \a len is
    0, the result is an empty, non-null string.

    \sa left(), right(), sliced()
*/
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

/*!
    Returns a copy of the string with white space removed from the start and
    the end.

    \sa simplified(), QChar::isSpace()
*/
QString QString::trimmed() const {
    qsizetype b = 0, e = size();
    while (b < e && at(b).isSpace())
        ++b;
    while (e > b && at(e - 1).isSpace())
        --e;
    return mid(b, e - b);
}

/*!
    Returns a copy of the string with white space removed from the start and
    the end, and every internal run of white space replaced by a single space.

    \sa trimmed(), QChar::isSpace()
*/
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

/*!
    Returns an uppercase copy of the string. Case mapping is per character
    (see QChar::toUpper()); special cases such as the German sharp s are not
    expanded.

    \sa toLower(), QChar::toUpper()
*/
QString QString::toUpper() const {
    std::u16string r(d);
    for (char16_t &c : r)
        c = QChar(c).toUpper().unicode();
    return QString(r);
}

/*!
    Returns a lowercase copy of the string. Case mapping is per character
    (see QChar::toLower()).

    \sa toUpper(), toCaseFolded(), QChar::toLower()
*/
QString QString::toLower() const { return QString(foldCase(d)); }

/*!
    Returns a copy of this string repeated \a times times. An empty string is
    returned if \a times is less than 1.
*/
QString QString::repeated(qsizetype times) const {
    std::u16string r;
    for (qsizetype i = 0; i < times; ++i)
        r += d;
    return QString(r);
}

/*!
    Returns a string of the given \a width containing this string padded on
    the right with \a fill. If the string is longer than \a width, it is
    returned unchanged, or cut to \a width characters if \a truncate is
    \c true.

    \sa rightJustified()
*/
QString QString::leftJustified(qsizetype width, QChar fill, bool truncate) const {
    QString r(*this);
    if (r.size() < width)
        r.d.append(static_cast<std::size_t>(width - r.size()), fill.unicode());
    else if (truncate)
        r.truncate(width);
    return r;
}

/*!
    Returns a string of the given \a width containing this string padded on
    the left with \a fill. If the string is longer than \a width, it is
    returned unchanged, or cut to \a width characters if \a truncate is
    \c true.

    \sa leftJustified()
*/
QString QString::rightJustified(qsizetype width, QChar fill, bool truncate) const {
    QString r(*this);
    if (r.size() < width)
        r.d.insert(0, static_cast<std::size_t>(width - r.size()), fill.unicode());
    else if (truncate)
        r.truncate(width);
    return r;
}

/*!
    \overload

    Splits the string at every occurrence of \a sep and returns the fields
    numbered \a start to \a end (inclusive), joined by \a sep. Negative
    field numbers count from the last field. An empty string is returned if the
    range is empty. If \a end is -1, all fields from \a start on are returned.

    \sa split()
*/
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

/*!
    Splits the string at every occurrence of the character \a sep and returns
    the fields numbered \a start to \a end (inclusive), joined by \a sep.
    Negative field numbers count from the last field. An empty string is
    returned if the range is empty. If \a end is -1, all fields from \a start
    on are returned.

    \sa split()
*/
QString QString::section(QChar sep, qsizetype start, qsizetype end) const { return section(QString(sep), start, end); }

/*!
    Returns \c true if the string starts with \a s, using the case
    sensitivity \a cs; otherwise returns \c false.

    \sa endsWith(), left()
*/
bool QString::startsWith(const QString &s, Qt::CaseSensitivity cs) const {
    if (s.size() > size())
        return false;
    return left(s.size()).compare(s, cs) == 0;
}

/*!
    Returns \c true if the string ends with \a s, using the case sensitivity
    \a cs; otherwise returns \c false.

    \sa startsWith(), right()
*/
bool QString::endsWith(const QString &s, Qt::CaseSensitivity cs) const {
    if (s.size() > size())
        return false;
    return right(s.size()).compare(s, cs) == 0;
}

/*!
    Returns the index position of the first occurrence of the string \a s in
    this string, searching forward from index position \a from, or -1 if it is
    not found. If \a from is negative, it is counted from the end of the
    string. \a cs selects the case sensitivity.

    \sa lastIndexOf(), contains(), count()
*/
qsizetype QString::indexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
    if (from < 0)
        from = qMax<qsizetype>(0, from + size());
    const auto p = cs == Qt::CaseSensitive ? d.find(s.d, static_cast<std::size_t>(from))
                                           : foldCase(d).find(foldCase(s.d), static_cast<std::size_t>(from));
    return p == std::u16string::npos ? -1 : static_cast<qsizetype>(p);
}

/*!
    Returns the index position of the last occurrence of the string \a s in
    this string, searching backward from index position \a from, or -1 if it
    is not found. A negative \a from is counted from the end of the string, so
    the default of -1 starts at the last character. The match must begin at or
    before \a from. \a cs selects the case sensitivity.

    \sa indexOf(), contains()
*/
qsizetype QString::lastIndexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
    const qsizetype pos = from < 0 ? from + size() : from;
    if (pos < 0)
        return -1;
    const auto p = cs == Qt::CaseSensitive ? d.rfind(s.d, static_cast<std::size_t>(pos))
                                           : foldCase(d).rfind(foldCase(s.d), static_cast<std::size_t>(pos));
    return p == std::u16string::npos ? -1 : static_cast<qsizetype>(p);
}

/*!
    Returns the number of occurrences of the string \a s in this string, using
    the case sensitivity \a cs. Overlapping occurrences are counted
    separately. Returns 0 if \a s is empty.

    \sa contains(), indexOf()
*/
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

/*!
    Splits the string into substrings wherever \a sep occurs and returns the
    list of those strings. \a behavior selects whether empty parts are kept or
    skipped, and \a cs the case sensitivity of the match. If \a sep is empty,
    the result holds an empty part, one part for each character, and another
    empty part (the empty parts are dropped if \a behavior is
    Qt::SkipEmptyParts).

    \sa QStringList::join(), section()
*/
QStringList QString::split(const QString &sep, Qt::SplitBehavior behavior, Qt::CaseSensitivity cs) const {
    QStringList r;
    const bool skip = behavior.testFlag(Qt::SkipEmptyParts);
    auto add = [&](const std::u16string &part) {
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

/*!
    \overload

    Splits the string into substrings wherever the character \a sep occurs.
    \a behavior selects whether empty parts are kept or skipped, and \a cs the
    case sensitivity of the match.
*/
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

/*!
    Returns a copy of this string in which the lowest-numbered place marker
    (\c{%1} to \c{%99}, optionally written \c{%L1}) is replaced by \a a.

    \a a is padded to at least \a fieldWidth characters with \a fillChar; a
    positive \a fieldWidth right-aligns it and a negative one left-aligns it
    (padding with spaces). If the string contains no place marker, a space and
    \a a are appended.

    \code
    QString s = QString("%1 of %2").arg("1").arg("3"); // "1 of 3"
    \endcode

    \sa number()
*/
QString QString::arg(const QString &a, int fieldWidth, QChar fillChar) const {
    return QString(substituteLowest(d, pad(a.d, fieldWidth, fillChar.unicode())));
}

/*!
    \overload

    Replaces the lowest-numbered place marker with the integer \a a written in
    \a base (2 to 36; other values mean 10), padded to \a fieldWidth with
    \a fillChar. When padding a negative number with \c{'0'}, the zeros follow
    the minus sign.
*/
QString QString::arg(qlonglong a, int fieldWidth, int base, QChar fillChar) const {
    return QString(substituteLowest(d, pad(ascii16(QtPrivate::signedToString(a, base)), fieldWidth, fillChar.unicode(), true)));
}

/*!
    \overload

    Replaces the lowest-numbered place marker with the unsigned integer \a a
    written in \a base, padded to \a fieldWidth with \a fillChar.
*/
QString QString::arg(qulonglong a, int fieldWidth, int base, QChar fillChar) const {
    return QString(substituteLowest(d, pad(ascii16(QtPrivate::intToString(a, false, base)), fieldWidth, fillChar.unicode(), true)));
}

/*!
    \overload

    Replaces the lowest-numbered place marker with the floating-point value
    \a a, formatted as described for number() using \a format and
    \a precision, and padded to \a fieldWidth with \a fillChar.
*/
QString QString::arg(double a, int fieldWidth, char format, int precision, QChar fillChar) const {
    return QString(substituteLowest(d, pad(ascii16(QtPrivate::formatDouble(a, format, precision)), fieldWidth, fillChar.unicode(), true)));
}

/*!
    Returns a string holding the representation of the integer \a n in
    \a base (2 to 36; other values mean 10). Digits above 9 are lowercase
    letters.

    \sa setNum(), toInt(), toLongLong()
*/
QString QString::number(qlonglong n, int base) { return QString(ascii16(QtPrivate::signedToString(n, base))); }
/*!
    \overload

    Returns a string holding the representation of the unsigned integer \a n
    in \a base.
*/
QString QString::number(qulonglong n, int base) { return QString(ascii16(QtPrivate::intToString(n, false, base))); }
/*!
    \overload

    Returns a string holding the representation of the floating-point value
    \a n. \a format is one of \c 'f' (fixed), \c 'e' (scientific) or \c 'g'
    (the shorter of the two), with an uppercase letter selecting an uppercase
    exponent; any other value is treated as \c 'g'. \a precision is the number
    of digits after the decimal point for \c 'f' and \c 'e', or the number of
    significant digits for \c 'g'. If \a precision is negative, the shortest
    representation that round-trips is produced. The C locale is always used.

    \sa toDouble(), setNum()
*/
QString QString::number(double n, char format, int precision) {
    return QString(ascii16(QtPrivate::formatDouble(n, format, precision)));
}

/*!
    Returns the string converted to a \c{long long} using \a base, which must
    be between 2 and 36 or 0. If \a base is 0, the C prefixes \c 0x and \c 0
    select hexadecimal and octal, and decimal is used otherwise. Leading and
    trailing white space is ignored. If \a ok is not \c nullptr, it is set to
    \c true on success and \c false on failure (for example when the string is
    empty, has other trailing characters, contains non-ASCII characters or the
    value overflows), in which case 0 is returned. The C locale is always used.

    \sa number(), toULongLong(), toInt()
*/
qlonglong QString::toLongLong(bool *ok, int base) const {
    std::string a;
    long long v = 0;
    const bool good = asciiOnly(d, a) && QtPrivate::parseSigned(a, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

/*!
    Returns the string converted to an \c{unsigned long long} using \a base.
    Conversion follows toLongLong(), except that a leading minus sign makes
    the conversion fail. On failure \a ok is set to \c false (if it is not
    \c nullptr) and 0 is returned.

    \sa toLongLong(), number()
*/
qulonglong QString::toULongLong(bool *ok, int base) const {
    std::string a;
    unsigned long long v = 0;
    const bool good = asciiOnly(d, a) && QtPrivate::parseUnsigned(a, base, v);
    if (ok) *ok = good;
    return good ? v : 0;
}

/*!
    Returns the string converted to an \c int using \a base. Conversion
    follows toLongLong(), and also fails if the value does not fit in an
    \c int. On failure \a ok is set to \c false (if it is not \c nullptr) and
    0 is returned.

    \sa toLongLong(), number()
*/
int QString::toInt(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= INT_MIN && v <= INT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<int>(v) : 0;
}

/*!
    Returns the string converted to an \c{unsigned int} using \a base.
    Conversion follows toULongLong(), and also fails if the value does not fit
    in an \c{unsigned int}. On failure \a ok is set to \c false (if it is not
    \c nullptr) and 0 is returned.

    \sa toInt(), toULongLong()
*/
uint QString::toUInt(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= UINT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<uint>(v) : 0;
}

/*!
    Returns the string converted to a \c long using \a base. Conversion
    follows toLongLong(), and also fails if the value does not fit in a
    \c long. On failure \a ok is set to \c false (if it is not \c nullptr)
    and 0 is returned.

    \sa toLongLong(), toInt()
*/
long QString::toLong(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= LONG_MIN && v <= LONG_MAX;
    if (ok) *ok = good;
    return good ? static_cast<long>(v) : 0;
}

/*!
    Returns the string converted to a \c short using \a base. Conversion
    follows toLongLong(), and also fails if the value does not fit in a
    \c short. On failure \a ok is set to \c false (if it is not \c nullptr)
    and 0 is returned.

    \sa toInt(), toUShort()
*/
short QString::toShort(bool *ok, int base) const {
    bool good = false;
    const qlonglong v = toLongLong(&good, base);
    good = good && v >= SHRT_MIN && v <= SHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<short>(v) : 0;
}

/*!
    Returns the string converted to an \c{unsigned short} using \a base.
    Conversion follows toULongLong(), and also fails if the value does not fit
    in an \c{unsigned short}. On failure \a ok is set to \c false (if it is
    not \c nullptr) and 0 is returned.

    \sa toShort(), toUInt()
*/
ushort QString::toUShort(bool *ok, int base) const {
    bool good = false;
    const qulonglong v = toULongLong(&good, base);
    good = good && v <= USHRT_MAX;
    if (ok) *ok = good;
    return good ? static_cast<ushort>(v) : 0;
}

/*!
    Returns the string converted to a \c double. Leading and trailing white
    space is ignored. The conversion uses the C locale and accepts what
    \c strtod() accepts, including exponents, \c inf and \c nan. If \a ok is
    not \c nullptr, it is set to \c true on success and \c false on failure
    (an empty string, trailing characters, non-ASCII characters or overflow),
    in which case 0 is returned.

    \sa number(), toFloat()
*/
double QString::toDouble(bool *ok) const {
    std::string a;
    double v = 0;
    const bool good = asciiOnly(d, a) && QtPrivate::parseDouble(a, v);
    if (ok) *ok = good;
    return good ? v : 0.0;
}

/*!
    Returns the string converted to a \c float. The string is converted with
    toDouble() and then narrowed; \a ok is set as described there.

    \sa toDouble()
*/
float QString::toFloat(bool *ok) const { return static_cast<float>(toDouble(ok)); }

/*!
    Compares this string with \a o, using the case sensitivity \a cs, and
    returns a negative integer, zero or a positive integer if this string is
    less than, equal to or greater than \a o. Strings are compared code unit by
    code unit (after case folding with toLower() if \a cs is
    Qt::CaseInsensitive); the comparison is not locale aware.

    \sa localeAwareCompare()
*/
int QString::compare(const QString &o, Qt::CaseSensitivity cs) const {
    return cs == Qt::CaseSensitive ? d.compare(o.d) : foldCase(d).compare(foldCase(o.d));
}

/*!
    Returns a string built from the \c printf()-style format \a fmt and the
    following arguments. \a fmt is interpreted as UTF-8, and the output of
    \c vsnprintf() is decoded as UTF-8.

    \sa arg()
*/
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
