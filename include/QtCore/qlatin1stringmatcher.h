// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

/*!
    \class QLatin1StringMatcher
    \brief The QLatin1StringMatcher class searches for a Latin-1 pattern in Latin-1 and UTF-16 text.
    \since 6.5

    The matcher prepares a skip table for the pattern once, which makes it faster than
    QLatin1StringView::indexOf() for a pattern that is searched for many times:

    \code
    QLatin1StringMatcher matcher("content-length"_L1, Qt::CaseInsensitive);
    for (QLatin1StringView line : headerLines)
        if (matcher.indexIn(line) == 0)
            readLength(line);
    \endcode

    The matcher keeps a view of the pattern and not a copy, so the characters of the pattern must
    stay valid for as long as the matcher is used.

    \differences
    The matcher uses the Boyer-Moore-Horspool algorithm. Case-insensitive matching folds the letters
    of Latin-1 only; a character of a UTF-16 text that is outside Latin-1, such as the Kelvin sign,
    never matches.
    \enddifferences

    \sa QStringMatcher, QByteArrayMatcher
*/
class QLatin1StringMatcher {
public:
    /*!
        Constructs a matcher with an empty pattern. Call setPattern() to give it one.
    */
    QLatin1StringMatcher() noexcept { prepare(); }
    /*!
        Constructs a matcher that searches for \a pattern with the case sensitivity \a cs.
    */
    explicit QLatin1StringMatcher(QLatin1StringView pattern, Qt::CaseSensitivity cs = Qt::CaseSensitive) noexcept
        : m_pattern(pattern), m_cs(cs) {
        prepare();
    }

    /*!
        Sets the pattern to search for to \a pattern. The matcher views the characters and does not
        copy them.

        \sa pattern()
    */
    void setPattern(QLatin1StringView pattern) noexcept {
        m_pattern = pattern;
        prepare();
    }
    /*!
        Returns the pattern that the matcher searches for.

        \sa setPattern()
    */
    QLatin1StringView pattern() const noexcept { return m_pattern; }
    /*!
        Sets the case sensitivity of the matcher to \a cs.

        \sa caseSensitivity()
    */
    void setCaseSensitivity(Qt::CaseSensitivity cs) noexcept {
        m_cs = cs;
        prepare();
    }
    /*!
        Returns the case sensitivity of the matcher.

        \sa setCaseSensitivity()
    */
    Qt::CaseSensitivity caseSensitivity() const noexcept { return m_cs; }

    /*!
        Searches \a haystack from index position \a from (default 0) and returns the index of the
        first occurrence of the pattern, or -1 if it is not found. An empty pattern is found at
        \a from.
    */
    qsizetype indexIn(QLatin1StringView haystack, qsizetype from = 0) const noexcept {
        return find(reinterpret_cast<const uchar *>(haystack.data()), haystack.size(), from);
    }
    /*!
        \overload
        \since 6.8
        Searches the UTF-16 text \a haystack from index position \a from. The result counts UTF-16
        code units.
    */
    qsizetype indexIn(QStringView haystack, qsizetype from = 0) const noexcept {
        return find(haystack.utf16(), haystack.size(), from);
    }

private:
    // Lower case for the letters of Latin-1; the multiplication sign 0xD7 is between them and is no letter.
    static constexpr char16_t lower(char16_t c) noexcept {
        const bool upper = (c >= u'A' && c <= u'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7);
        return upper ? static_cast<char16_t>(c + 0x20) : c;
    }
    char16_t fold(char16_t c) const noexcept { return m_cs == Qt::CaseSensitive ? c : lower(c); }
    char16_t patternAt(qsizetype i) const noexcept { return fold(static_cast<uchar>(m_pattern.data()[i])); }
    void prepare() noexcept {
        const auto capped = [](qsizetype shift) { return static_cast<uchar>(shift > 255 ? 255 : shift); };
        const qsizetype m = m_pattern.size();
        for (uchar &shift : m_skip)
            shift = capped(m > 0 ? m : 1);
        for (qsizetype i = 0; i + 1 < m; ++i)
            m_skip[patternAt(i)] = capped(m - 1 - i);
    }
    // A character that is not in Latin-1 is in no pattern, so the pattern may move past it.
    template <typename Char> qsizetype find(const Char *text, qsizetype length, qsizetype from) const noexcept {
        const qsizetype m = m_pattern.size();
        if (from < 0)
            from = 0;
        if (m == 0)
            return from <= length ? from : -1;
        qsizetype pos = from;
        while (pos <= length - m) {
            qsizetype j = m - 1;
            while (j >= 0 && fold(text[pos + j]) == patternAt(j))
                --j;
            if (j < 0)
                return pos;
            const char16_t last = fold(text[pos + m - 1]);
            pos += last < 256 ? m_skip[last] : m;
        }
        return -1;
    }

    QLatin1StringView m_pattern;
    Qt::CaseSensitivity m_cs = Qt::CaseSensitive;
    uchar m_skip[256] = {};
};
