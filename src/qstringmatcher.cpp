// MIT License - see LICENSE
#include <QtCore/qstringmatcher.h>
#include <QtCore/qstringtokenizer.h>

#include <string_view>

namespace {

char16_t foldUnit(char16_t c) { return QChar(c).toCaseFolded().unicode(); }

} // namespace

/*!
    Constructs a matcher that finds \a pattern, with the case sensitivity
    \a cs.

    \sa setPattern(), setCaseSensitivity()
*/
QStringMatcher::QStringMatcher(const QString &pattern, Qt::CaseSensitivity cs) : m_pattern(pattern), m_cs(cs) { build(); }

/*!
    \overload

    Constructs a matcher that finds the \a len characters starting at \a uc,
    with the case sensitivity \a cs.
*/
QStringMatcher::QStringMatcher(const QChar *uc, qsizetype len, Qt::CaseSensitivity cs) : m_pattern(uc, len), m_cs(cs) { build(); }

/*!
    \overload

    Constructs a matcher that finds \a pattern, with the case sensitivity
    \a cs.
*/
QStringMatcher::QStringMatcher(QStringView pattern, Qt::CaseSensitivity cs) : m_pattern(pattern.toString()), m_cs(cs) { build(); }

void QStringMatcher::build() {
    m_key.clear();
    for (char16_t c : m_pattern.u16string())
        m_key.push_back(m_cs == Qt::CaseSensitive ? c : foldUnit(c));
    const qsizetype m = static_cast<qsizetype>(m_key.size());
    for (qsizetype &s : m_skip)
        s = m;
    for (qsizetype i = 0; i + 1 < m; ++i)
        m_skip[m_key[static_cast<std::size_t>(i)] & 0xff] = m - 1 - i;
}

/*!
    Sets the string that the matcher finds to \a pattern.

    \sa pattern()
*/
void QStringMatcher::setPattern(const QString &pattern) {
    m_pattern = pattern;
    build();
}

/*!
    Sets the case sensitivity of the matcher to \a cs.

    \sa caseSensitivity()
*/
void QStringMatcher::setCaseSensitivity(Qt::CaseSensitivity cs) {
    if (cs == m_cs)
        return;
    m_cs = cs;
    build();
}

/*!
    Searches the string \a str from index position \a from (a negative value
    counts as 0) for the pattern, and returns the index position of the first
    match, or -1 if there is none. An empty pattern matches at \a from.

    \sa pattern()
*/
qsizetype QStringMatcher::indexIn(const QString &str, qsizetype from) const { return indexIn(str.constData(), str.size(), from); }

/*!
    \overload

    Searches the view \a str from index position \a from.
*/
qsizetype QStringMatcher::indexIn(QStringView str, qsizetype from) const { return indexIn(str.data(), str.size(), from); }

/*!
    \overload

    Searches the \a length characters starting at \a str from index position
    \a from.
*/
qsizetype QStringMatcher::indexIn(const QChar *str, qsizetype length, qsizetype from) const {
    const qsizetype m = static_cast<qsizetype>(m_key.size());
    if (from < 0)
        from = 0;
    if (m == 0)
        return from > length ? -1 : from;
    const bool fold = m_cs == Qt::CaseInsensitive;
    const char16_t *key = m_key.data();
    qsizetype i = from + m - 1;
    while (i < length) {
        const char16_t last = fold ? foldUnit(str[i].unicode()) : str[i].unicode();
        if (last == key[m - 1]) {
            qsizetype j = m - 2;
            qsizetype k = i - 1;
            while (j >= 0 && (fold ? foldUnit(str[k].unicode()) : str[k].unicode()) == key[j]) {
                --j;
                --k;
            }
            if (j < 0)
                return k + 1;
        }
        i += m_skip[last & 0xff];
    }
    return -1;
}

qsizetype QtPrivate::findString(const QChar *haystack, qsizetype haystackLength, const QChar *needle, qsizetype needleLength, qsizetype from,
                                Qt::CaseSensitivity cs) {
    if (from < 0)
        from = 0;
    if (needleLength == 0)
        return from > haystackLength ? -1 : from;
    if (from + needleLength > haystackLength)
        return -1;
    if (cs == Qt::CaseSensitive) {
        const std::u16string_view h(reinterpret_cast<const char16_t *>(haystack), static_cast<std::size_t>(haystackLength));
        const std::u16string_view n(reinterpret_cast<const char16_t *>(needle), static_cast<std::size_t>(needleLength));
        const std::size_t r = h.find(n, static_cast<std::size_t>(from));
        return r == std::u16string_view::npos ? -1 : static_cast<qsizetype>(r);
    }
    for (qsizetype i = from; i + needleLength <= haystackLength; ++i) {
        qsizetype j = 0;
        while (j < needleLength && foldUnit(haystack[i + j].unicode()) == foldUnit(needle[j].unicode()))
            ++j;
        if (j == needleLength)
            return i;
    }
    return -1;
}
