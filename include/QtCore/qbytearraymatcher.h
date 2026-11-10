// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>

#include <cstddef>

namespace QtPrivate {

// The skip table of the Boyer-Moore-Horspool search for the m bytes at pattern: how far the pattern may move when
// the byte under its last position is the index. A shift is at most 255, which is short of the best but correct.
constexpr void prepareByteSkipTable(const char *pattern, qsizetype m, uchar *skip) noexcept {
    const auto capped = [](qsizetype shift) { return static_cast<uchar>(shift > 255 ? 255 : shift); };
    for (int i = 0; i < 256; ++i)
        skip[i] = capped(m > 0 ? m : 1);
    for (qsizetype i = 0; i + 1 < m; ++i)
        skip[static_cast<uchar>(pattern[i])] = capped(m - 1 - i);
}

// The index of the first occurrence at or after from, or -1.
constexpr qsizetype findBytes(const char *haystack, qsizetype length, qsizetype from, const char *pattern, qsizetype m,
                              const uchar *skip) noexcept {
    if (from < 0)
        from = 0;
    if (m == 0)
        return from <= length ? from : -1;
    qsizetype pos = from;
    while (pos <= length - m) {
        qsizetype j = m - 1;
        while (j >= 0 && haystack[pos + j] == pattern[j])
            --j;
        if (j < 0)
            return pos;
        pos += skip[static_cast<uchar>(haystack[pos + m - 1])];
    }
    return -1;
}

} // namespace QtPrivate

/*!
    \class QByteArrayMatcher
    \brief The QByteArrayMatcher class holds a sequence of bytes that can be quickly matched in a byte array.

    Searching for the same bytes in many byte arrays, or many times in one, is faster with a
    QByteArrayMatcher than with QByteArray::indexOf(), because the matcher prepares a skip table for the
    pattern once:

    \code
    QByteArrayMatcher matcher("\r\n\r\n");
    const qsizetype endOfHeaders = matcher.indexIn(request);
    \endcode

    \differences
    The matcher uses the Boyer-Moore-Horspool algorithm. There is no \c indexIn() overload that takes a
    QByteArray: the one that takes a QByteArrayView accepts a byte array too.
    \enddifferences

    \sa QStaticByteArrayMatcher, QByteArray::indexOf(), QStringMatcher
*/
class QByteArrayMatcher {
public:
    /*!
        Constructs a matcher with an empty pattern. Call setPattern() to give it one.
    */
    QByteArrayMatcher() { prepare(); }
    /*!
        Constructs a matcher that searches for \a pattern.
    */
    explicit QByteArrayMatcher(const QByteArray &pattern) : m_pattern(pattern) { prepare(); }
    /*!
        \overload
        \since 5.12
        Constructs a matcher that searches for a copy of the bytes viewed by \a pattern.
    */
    explicit QByteArrayMatcher(QByteArrayView pattern) : m_pattern(pattern.data(), pattern.size()) { prepare(); }
    /*!
        \overload
        Constructs a matcher that searches for the \a length bytes at \a pattern. If \a length is
        negative, \a pattern is taken up to its terminating zero.
    */
    explicit QByteArrayMatcher(const char *pattern, qsizetype length = -1)
        : m_pattern(length < 0 ? QByteArray(pattern) : QByteArray(pattern, length)) {
        prepare();
    }

    /*!
        Sets the bytes to search for to \a pattern.

        \sa pattern(), indexIn()
    */
    void setPattern(const QByteArray &pattern) {
        m_pattern = pattern;
        prepare();
    }
    /*!
        Returns the bytes that the matcher searches for.

        \sa setPattern()
    */
    QByteArray pattern() const { return m_pattern; }

    /*!
        Searches the \a len bytes at \a str from index position \a from (default 0) and returns the
        index of the first occurrence of the pattern, or -1 if it is not found. An empty pattern is
        found at \a from.
    */
    qsizetype indexIn(const char *str, qsizetype len, qsizetype from = 0) const {
        return QtPrivate::findBytes(str, len, from, m_pattern.constData(), m_pattern.size(), m_skip);
    }
    /*!
        \overload
        \since 6.3
        Searches the bytes viewed by \a data from index position \a from.
    */
    qsizetype indexIn(QByteArrayView data, qsizetype from = 0) const { return indexIn(data.data(), data.size(), from); }

private:
    void prepare() { QtPrivate::prepareByteSkipTable(m_pattern.constData(), m_pattern.size(), m_skip); }

    QByteArray m_pattern;
    uchar m_skip[256] = {};
};

/*!
    \class QStaticByteArrayMatcher
    \brief The QStaticByteArrayMatcher class is a QByteArrayMatcher whose pattern is known when the program is compiled.
    \since 5.9

    The skip table is computed by the compiler, so a matcher that is \c constexpr costs nothing when
    the program starts. \c N is the size of the string literal with its terminating zero; let
    qMakeStaticByteArrayMatcher() deduce it:

    \code
    static constexpr auto matcher = qMakeStaticByteArrayMatcher("needle");
    const qsizetype at = matcher.indexIn(haystack);
    \endcode

    \differences
    There is no \c indexIn() overload that takes a QByteArray: the one that takes a QByteArrayView accepts
    a byte array too.
    \enddifferences

    \sa QByteArrayMatcher
*/
template <std::size_t N> class QStaticByteArrayMatcher {
public:
    /*!
        Constructs a matcher that searches for the string literal \a pattern, without its
        terminating zero.

    */
    explicit constexpr QStaticByteArrayMatcher(const char (&pattern)[N]) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            m_pattern[i] = pattern[i];
        QtPrivate::prepareByteSkipTable(m_pattern, static_cast<qsizetype>(N) - 1, m_skip);
    }

    /*!
        Searches the \a hlen bytes at \a haystack from index position \a from (default 0) and
        returns the index of the first occurrence of the pattern, or -1 if it is not found.
    */
    constexpr qsizetype indexIn(const char *haystack, qsizetype hlen, qsizetype from = 0) const noexcept {
        return QtPrivate::findBytes(haystack, hlen, from, m_pattern, static_cast<qsizetype>(N) - 1, m_skip);
    }
    /*!
        \overload
        \since 6.3
        Searches the bytes viewed by \a haystack from index position \a from.
    */
    qsizetype indexIn(QByteArrayView haystack, qsizetype from = 0) const noexcept {
        return indexIn(haystack.data(), haystack.size(), from);
    }
    /*!
        Returns the bytes that the matcher searches for.
    */
    QByteArray pattern() const { return QByteArray(m_pattern, static_cast<qsizetype>(N) - 1); }

private:
    char m_pattern[N] = {};
    uchar m_skip[256] = {};
};

// Returns a matcher for the string literal, with the size of the literal deduced.
template <std::size_t N>
constexpr QStaticByteArrayMatcher<N> qMakeStaticByteArrayMatcher(const char (&pattern)[N]) noexcept {
    return QStaticByteArrayMatcher<N>(pattern);
}
