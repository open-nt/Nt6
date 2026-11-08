// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <array>

/*!
    \class QStringMatcher
    \brief The QStringMatcher class holds a sequence of characters that can be quickly matched in a Unicode string.

    Searching for the same pattern in many strings is faster with a
    QStringMatcher than with QString::indexOf(), because the matcher prepares
    a skip table for the pattern once:

    \code
    QStringMatcher matcher(u"needle");
    for (const QString &line : lines)
        if (matcher.indexIn(line) != -1)
            ++hits;
    \endcode

    \differences
    The matcher uses the Boyer-Moore-Horspool algorithm on UTF-16 code units.
    Case-insensitive matching folds each code unit with QChar::toCaseFolded(),
    so characters outside the Basic Multilingual Plane are compared exactly.
    \enddifferences

    \sa QString::indexOf(), QStringView
*/
class QStringMatcher {
public:
    /*!
        Constructs an empty matcher that matches nothing. Call setPattern() to
        give it a pattern.
    */
    QStringMatcher() noexcept { prepare(); }
    /*!
        Constructs a matcher for \a pattern with the case sensitivity \a cs.
    */
    explicit QStringMatcher(const QString &pattern, Qt::CaseSensitivity cs = Qt::CaseSensitive) : m_pattern(pattern), m_cs(cs) { prepare(); }
    /*!
        \overload
        \since 5.14
        Constructs a matcher for the characters viewed by \a pattern with the
        case sensitivity \a cs.
    */
    explicit QStringMatcher(QStringView pattern, Qt::CaseSensitivity cs = Qt::CaseSensitive) : m_pattern(pattern.toString()), m_cs(cs) { prepare(); }
    /*!
        \overload
        Constructs a matcher for the \a length characters at \a uc with the
        case sensitivity \a cs.
    */
    QStringMatcher(const QChar *uc, qsizetype length, Qt::CaseSensitivity cs = Qt::CaseSensitive) : m_pattern(uc, length), m_cs(cs) { prepare(); }

    /*!
        Sets the pattern to search for to \a pattern.

        \sa pattern(), setCaseSensitivity()
    */
    void setPattern(const QString &pattern) {
        m_pattern = pattern;
        prepare();
    }
    /*!
        Sets the case sensitivity of the matcher to \a cs.

        \sa caseSensitivity(), setPattern()
    */
    void setCaseSensitivity(Qt::CaseSensitivity cs) {
        m_cs = cs;
        prepare();
    }
    /*!
        Returns the pattern that the matcher searches for.

        \sa setPattern()
    */
    QString pattern() const { return m_pattern; }
    /*!
        \since 6.0
        Returns a view of the pattern that the matcher searches for. The view
        is valid as long as the matcher and its pattern are not changed.
    */
    QStringView patternView() const noexcept { return QStringView(m_pattern.constData(), m_pattern.size()); }
    /*!
        Returns the case sensitivity of the matcher.

        \sa setCaseSensitivity()
    */
    Qt::CaseSensitivity caseSensitivity() const noexcept { return m_cs; }

    /*!
        Searches the string \a str from index position \a from (default 0)
        and returns the index of the first occurrence of the pattern, or -1 if
        it is not found. A negative \a from counts from the end of the string.
    */
    qsizetype indexIn(const QString &str, qsizetype from = 0) const { return indexIn(str.constData(), str.size(), from); }
    /*!
        \overload
        \since 5.14
        Searches the characters viewed by \a str from index position \a from.
    */
    qsizetype indexIn(QStringView str, qsizetype from = 0) const { return indexIn(str.data(), str.size(), from); }
    /*!
        \overload
        Searches the \a length characters at \a str from index position
        \a from.
    */
    qsizetype indexIn(const QChar *str, qsizetype length, qsizetype from = 0) const {
        const qsizetype m = m_pattern.size();
        if (from < 0)
            from = qMax<qsizetype>(from + length, 0);
        if (m == 0)
            return from <= length ? from : -1;
        const char16_t *pat = m_pattern.utf16();
        const auto *s = reinterpret_cast<const char16_t *>(str);
        qsizetype pos = from;
        while (pos + m <= length) {
            qsizetype j = m - 1;
            while (j >= 0 && fold(s[pos + j]) == fold(pat[j]))
                --j;
            if (j < 0)
                return pos;
            pos += m_skip[fold(s[pos + m - 1]) & 0xff];
        }
        return -1;
    }

private:
    char16_t fold(char16_t c) const { return m_cs == Qt::CaseSensitive ? c : QChar(c).toCaseFolded().unicode(); }
    void prepare() {
        const qsizetype m = m_pattern.size();
        m_skip.fill(m > 0 ? m : 1);
        const char16_t *pat = m_pattern.utf16();
        // Several code units share a slot of the table, so a slot keeps the smallest shift.
        for (qsizetype i = 0; i + 1 < m; ++i)
            m_skip[fold(pat[i]) & 0xff] = m - 1 - i;
    }

    QString m_pattern;
    Qt::CaseSensitivity m_cs = Qt::CaseSensitive;
    std::array<qsizetype, 256> m_skip{};
};
