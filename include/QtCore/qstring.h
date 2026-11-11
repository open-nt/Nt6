// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include <algorithm>
#include <cstdarg>
#include <functional>
#include <iterator>
#include <string>
#include <type_traits>

class QAnyStringView;
class QString;
template <typename T> class QList;
using QStringList = QList<QString>;
class QRegularExpression;
class QRegularExpressionMatch;
template <typename Haystack, typename Needle> class QStringTokenizer;

/*!
    \class QLatin1StringView
    \brief The QLatin1StringView class provides a thin wrapper around a US-ASCII/Latin-1 encoded string literal.

    A QLatin1StringView refers to a sequence of Latin-1 bytes without owning
    or copying them, so the viewed memory must outlive it. It is mainly used
    to pass 8-bit literals to QString functions without an intermediate
    conversion. \c QLatin1String is an alias for this class.

    \differences
    The searching and comparison functions that take a QString convert the
    view to a QString first, so they allocate, and so do the number
    conversions. arg() takes strings and characters only. first(), last(),
    sliced(), chopped(), slice(), truncate() and chop() do not check their
    arguments.
    \enddifferences

    \sa QString, QLatin1Char
*/
class QLatin1StringView {
public:
    /*!
        Constructs an empty Latin-1 string view.
    */
    constexpr QLatin1StringView() noexcept = default;
    /*!
        Constructs a view of the null-terminated string \a s. The length is
        determined by scanning for the terminator; a null \a s gives an empty
        view.
    */
    constexpr QLatin1StringView(const char *s) noexcept : p(s), n(s ? static_cast<qsizetype>(std::char_traits<char>::length(s)) : 0) {}
    /*!
        \overload

        Constructs a view of the first \a len bytes of \a s.
    */
    constexpr QLatin1StringView(const char *s, qsizetype len) noexcept : p(s), n(len) {}
    /*!
        \overload

        Constructs a view of the contents of the byte array \a b.
    */
    QLatin1StringView(const QByteArray &b) noexcept : p(b.constData()), n(b.size()) {}
    /*!
        Returns a pointer to the first byte of the viewed string.

        \sa data()
    */
    constexpr const char *latin1() const noexcept { return p; }
    /*!
        Returns a pointer to the first byte of the viewed string. The data is
        not necessarily null-terminated.

        \sa latin1(), size()
    */
    constexpr const char *data() const noexcept { return p; }
    /*!
        Returns the number of bytes in the view.

        \sa isEmpty()
    */
    constexpr qsizetype size() const noexcept { return n; }
    /*!
        Returns \c true if the view has no characters; otherwise returns
        \c false.

        \sa size()
    */
    constexpr bool isEmpty() const noexcept { return n == 0; }
    /*!
        Returns the character at index position \a i as a QChar. \a i must be a
        valid index; no bounds checking is done.
    */
    constexpr QChar at(qsizetype i) const { return QChar::fromLatin1(p[i]); }
    /*!
        Returns the character at index position \a i. This is the same as
        at().
    */
    constexpr QChar operator[](qsizetype i) const { return at(i); }

    /*!
        \since 6.0
        Returns the number of characters, the same as size().
    */
    constexpr qsizetype length() const noexcept { return n; }
    /*!
        \since 6.0
        Returns \c true if the view is empty. Provided for STL compatibility.
    */
    constexpr bool empty() const noexcept { return n == 0; }
    /*!
        \since 5.10
        Returns the first character. The view must not be empty.
    */
    constexpr QChar front() const { return at(0); }
    /*!
        \since 5.10
        Returns the last character. The view must not be empty.
    */
    constexpr QChar back() const { return at(n - 1); }
    /*!
        \since 6.0
        Returns a view of the first \a len characters.

        \sa last(), sliced()
    */
    constexpr QLatin1StringView first(qsizetype len) const { return QLatin1StringView(p, len); }
    /*!
        \since 6.0
        Returns a view of the last \a len characters.

        \sa first(), sliced()
    */
    constexpr QLatin1StringView last(qsizetype len) const { return QLatin1StringView(p + n - len, len); }
    /*!
        \since 6.0
        Returns a view of the characters from \a pos to the end.

        \sa first(), last()
    */
    constexpr QLatin1StringView sliced(qsizetype pos) const { return QLatin1StringView(p + pos, n - pos); }
    /*!
        \since 6.0
        \overload
        Returns a view of \a len characters starting at \a pos.
    */
    constexpr QLatin1StringView sliced(qsizetype pos, qsizetype len) const { return QLatin1StringView(p + pos, len); }
    /*!
        \since 5.10
        Returns a view without the last \a len characters.
    */
    constexpr QLatin1StringView chopped(qsizetype len) const { return QLatin1StringView(p, n - len); }
    /*!
        \since 5.10
        Removes \a len characters from the end of the view.
    */
    constexpr void chop(qsizetype len) { n -= len; }
    /*!
        \since 5.10
        Shortens the view to \a len characters.
    */
    constexpr void truncate(qsizetype len) { n = len; }
    /*!
        \since 5.8
        Returns a view of \a len characters starting at \a pos, or up to the
        end if \a len is negative. Positions outside the view are clamped.
    */
    constexpr QLatin1StringView mid(qsizetype pos, qsizetype len = -1) const {
        if (pos < 0)
            pos = 0;
        if (pos > n)
            pos = n;
        if (len < 0 || len > n - pos)
            len = n - pos;
        return QLatin1StringView(p + pos, len);
    }
    /*!
        \since 5.8
        Returns a view of the first \a len characters, or the whole view.
    */
    constexpr QLatin1StringView left(qsizetype len) const { return mid(0, len < 0 ? 0 : len); }
    /*!
        \since 5.8
        Returns a view of the last \a len characters, or the whole view.
    */
    constexpr QLatin1StringView right(qsizetype len) const { return len >= n ? *this : mid(n - (len < 0 ? 0 : len)); }
    /*!
        \since 5.10
        Returns a view without the leading and trailing white space (space,
        tab, line feed, vertical tab, form feed, carriage return, U+0085 and
        U+00A0).
    */
    constexpr QLatin1StringView trimmed() const noexcept {
        qsizetype b = 0;
        qsizetype e = n;
        while (b < e && isSpace(p[b]))
            ++b;
        while (e > b && isSpace(p[e - 1]))
            --e;
        return QLatin1StringView(p + b, e - b);
    }
    /*!
        \since 5.10
        Returns a pointer to the first byte, for iteration.
    */
    constexpr const char *begin() const noexcept { return p; }
    /*!
        \since 5.10
        Returns a pointer just past the last byte.
    */
    constexpr const char *end() const noexcept { return p + n; }
    /*!
        \since 5.10
        Returns a pointer to the first byte. Same as begin().
    */
    constexpr const char *cbegin() const noexcept { return p; }
    /*!
        \since 5.10
        Returns a pointer just past the last byte. Same as end().
    */
    constexpr const char *cend() const noexcept { return p + n; }

    /*!
        A read-only iterator over the characters of the view. It is a plain \c{const char *}.
    */
    using const_iterator = const char *;
    /*!
        A read-only reverse iterator over the characters of the view.
    */
    using const_reverse_iterator = std::reverse_iterator<const char *>;
    /*!
        The reverse iterator type, for STL compatibility. It is the same as const_reverse_iterator.
    */
    using reverse_iterator = const_reverse_iterator;

    /*!
        \overload

        Constructs a view of the characters from \a first up to, but not
        including, \a last. \a last must be a pointer, so that a literal 0
        as the second argument still means a length.
    */
    template <typename End,
              typename = std::enable_if_t<std::is_pointer_v<End> && std::is_convertible_v<End, const char *>>>
    constexpr QLatin1StringView(const char *first, End last) noexcept : p(first), n(last - first) {}
    /*!
        \overload

        Constructs a view of the bytes of \a b, read as Latin-1. The view is
        null if \a b is null.
    */
    explicit QLatin1StringView(QByteArrayView b) noexcept : p(b.data()), n(b.size()) {}

    /*!
        Returns \c true if the view has no data, as a default-constructed one;
        otherwise returns \c false. A null view is also empty.

        \sa isEmpty()
    */
    constexpr bool isNull() const noexcept { return p == nullptr; }

    /*!
        Returns a reverse iterator pointing to the last character of the view.

        \sa rend()
    */
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    /*!
        Returns a reverse iterator pointing just before the first character of the view.

        \sa rbegin()
    */
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    /*!
        Returns a reverse iterator pointing to the last character of the view. This is the same as rbegin().
    */
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    /*!
        Returns a reverse iterator pointing just before the first character of the view. This is the same as rend().
    */
    const_reverse_iterator crend() const noexcept { return rend(); }

    /*!
        \since 6.8
        Returns the largest number of characters a view can refer to.
    */
    static constexpr qsizetype max_size() noexcept { return static_cast<qsizetype>(~std::size_t(0) >> 1); }
    /*!
        \since 6.8
        Returns the largest number of characters a view can refer to. This is
        the same as max_size().
    */
    static constexpr qsizetype maxSize() noexcept { return max_size(); }
    /*!
        Returns a const iterator pointing to the first character of the view. This is the same as begin().
    */
    constexpr const_iterator constBegin() const noexcept { return begin(); }
    /*!
        Returns a const iterator pointing just after the last character of the view. This is the same as end().
    */
    constexpr const_iterator constEnd() const noexcept { return end(); }
    /*!
        Returns a pointer to the Latin-1 characters of the view. This is the same as data().
    */
    constexpr const char *constData() const noexcept { return p; }

    /*!
        \since 6.8
        Changes the view to start at index position \a pos and to extend to
        its end, and returns a reference to it. \a pos must not exceed size().

        \sa sliced()
    */
    constexpr QLatin1StringView &slice(qsizetype pos) {
        *this = sliced(pos);
        return *this;
    }
    /*!
        \since 6.8
        \overload

        Changes the view to the \a len characters that start at index position
        \a pos, and returns a reference to it. The range must lie within the
        view.
    */
    constexpr QLatin1StringView &slice(qsizetype pos, qsizetype len) {
        *this = sliced(pos, len);
        return *this;
    }

    /*!
        \since 6.4
        Returns the view converted to a \c short in base \a base, as
        QString::toShort() does. Returns 0 if the conversion fails; if \a ok
        is not \c nullptr, \c{*ok} is set to whether it succeeded.
    */
    short toShort(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to an \c{unsigned short} in base \a base,
        and sets \c{*ok} if \a ok is not \c nullptr.
    */
    ushort toUShort(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to an \c int in base \a base, as
        QString::toInt() does, and sets \c{*ok} if \a ok is not \c nullptr.
    */
    int toInt(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to an \c{unsigned int} in base \a base, and
        sets \c{*ok} if \a ok is not \c nullptr.
    */
    uint toUInt(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to a \c long in base \a base, and sets
        \c{*ok} if \a ok is not \c nullptr.
    */
    long toLong(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to an \c{unsigned long} in base \a base,
        and sets \c{*ok} if \a ok is not \c nullptr.
    */
    ulong toULong(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to a \c{long long} in base \a base, and
        sets \c{*ok} if \a ok is not \c nullptr.
    */
    qlonglong toLongLong(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to an \c{unsigned long long} in base
        \a base, and sets \c{*ok} if \a ok is not \c nullptr.
    */
    qulonglong toULongLong(bool *ok = nullptr, int base = 10) const;
    /*!
        \since 6.4
        Returns the view converted to a \c float, or 0 if it is not a number,
        and sets \c{*ok} if \a ok is not \c nullptr.
    */
    float toFloat(bool *ok = nullptr) const;
    /*!
        \since 6.4
        Returns the view converted to a \c double, or 0 if it is not a number,
        and sets \c{*ok} if \a ok is not \c nullptr. The text is read in the C
        locale.
    */
    double toDouble(bool *ok = nullptr) const;

    /*!
        Returns a string in which the place markers of this view (\c{%1} to
        \c{%99}) are replaced by \a a and \a rest: the lowest-numbered marker
        by \a a, the next one by the first of \a rest, and so on. The
        arguments are strings, views or characters. The markers are replaced
        in one pass, so a marker inside an argument is left alone.

        \sa QString::arg()
    */
    template <typename A, typename... Rest> QString arg(const A &a, const Rest &...rest) const;

    QString toString() const;
    bool startsWith(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool startsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool endsWith(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool endsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype indexOf(const QString &s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype indexOf(QChar c, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype lastIndexOf(const QString &s, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype lastIndexOf(QChar c, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool contains(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool contains(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype count(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype count(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    int compare(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;

    /*!
        Returns \c true if \a a and \a b hold the same characters.
    */
    friend constexpr bool operator==(QLatin1StringView a, QLatin1StringView b) noexcept {
        return a.n == b.n && std::char_traits<char>::compare(a.p, b.p, static_cast<std::size_t>(a.n)) == 0;
    }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend constexpr bool operator!=(QLatin1StringView a, QLatin1StringView b) noexcept { return !(a == b); }
    /*!
        Returns \c true if \a a sorts before \a b by character value.
    */
    friend constexpr bool operator<(QLatin1StringView a, QLatin1StringView b) noexcept {
        return compareBytes(a, b) < 0;
    }
    /*!
        Returns \c true if \a a sorts after \a b by character value.
    */
    friend constexpr bool operator>(QLatin1StringView a, QLatin1StringView b) noexcept {
        return compareBytes(a, b) > 0;
    }
    /*!
        Returns \c true if \a a sorts before \a b or equals it.
    */
    friend constexpr bool operator<=(QLatin1StringView a, QLatin1StringView b) noexcept {
        return compareBytes(a, b) <= 0;
    }
    /*!
        Returns \c true if \a a sorts after \a b or equals it.
    */
    friend constexpr bool operator>=(QLatin1StringView a, QLatin1StringView b) noexcept {
        return compareBytes(a, b) >= 0;
    }

private:
    static constexpr bool isSpace(char c) noexcept {
        const auto u = static_cast<unsigned char>(c);
        return u == ' ' || (u >= '\t' && u <= '\r') || u == 0x85 || u == 0xA0;
    }
    static constexpr int compareBytes(QLatin1StringView a, QLatin1StringView b) noexcept {
        const qsizetype m = a.n < b.n ? a.n : b.n;
        for (qsizetype i = 0; i < m; ++i) {
            const auto x = static_cast<unsigned char>(a.p[i]);
            const auto y = static_cast<unsigned char>(b.p[i]);
            if (x != y)
                return x < y ? -1 : 1;
        }
        return a.n == b.n ? 0 : (a.n < b.n ? -1 : 1);
    }

    const char *p = nullptr;
    qsizetype n = 0;
};
using QLatin1String = QLatin1StringView;

class QString {
public:
    /*!
        \enum QString::NormalizationForm

        The Unicode normalization forms. The enum is declared for source
        compatibility; Nt provides no \c normalized() function.

        \value NormalizationForm_D Canonical decomposition.
        \value NormalizationForm_C Canonical decomposition followed by canonical composition.
        \value NormalizationForm_KD Compatibility decomposition.
        \value NormalizationForm_KC Compatibility decomposition followed by canonical composition.
    */
    enum NormalizationForm { NormalizationForm_D, NormalizationForm_C, NormalizationForm_KD, NormalizationForm_KC };
    /*!
        A mutable random-access iterator over the characters of the string. It is a plain \c{QChar *}.
    */
    using iterator = QChar *;
    /*!
        A read-only random-access iterator over the characters of the string. It is a plain \c{const QChar *}.
    */
    using const_iterator = const QChar *;
    /*!
        The reverse iterator type, for STL compatibility.
    */
    using reverse_iterator = std::reverse_iterator<iterator>;
    /*!
        The const reverse iterator type, for STL compatibility.
    */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    /*!
        The element type of the string, for STL compatibility.
    */
    using value_type = QChar;
    /*!
        The type used for sizes and indexes, for STL compatibility.
    */
    using size_type = qsizetype;

    /*!
        Constructs a null string.

        \sa isNull()
    */
    QString() noexcept {}
    QString(const char *utf8);
    QString(const char16_t *u, qsizetype size = -1);
    QString(const QChar *u, qsizetype size);
    /*!
        \overload

        Constructs a string of size 1 containing the character \a c.
    */
    QString(QChar c) : d(1, c.unicode()), m_null(false) {}
    /*!
        \overload

        Constructs a string of the given \a size with every character set to
        \a c. A negative \a size gives an empty, non-null string.
    */
    QString(qsizetype size, QChar c) : d(static_cast<std::size_t>(size < 0 ? 0 : size), c.unicode()), m_null(false) {}
    /*!
        \overload

        Constructs a string of the given \a size whose characters the caller
        is about to assign. Pass \c Qt::Uninitialized as the second argument;
        Nt sets the characters to U+0000.
    */
    QString(qsizetype size, Qt::Initialization)
        : d(static_cast<std::size_t>(size < 0 ? 0 : size), u'\0'), m_null(false) {}
    QString(QLatin1StringView s);
    QString(const QByteArray &utf8);
    /*!
        \overload

        Constructs a string holding a copy of the UTF-16 string \a s. The
        result is never null.

        \sa fromStdU16String(), toStdU16String()
    */
    QString(const std::u16string &s) : d(s), m_null(false) {}

    // --- size / access ---
    /*!
        Returns the number of characters (UTF-16 code units) in the string.

        \sa length(), isEmpty(), resize()
    */
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    /*!
        Returns the number of characters in the string. This is the same as
        size().
    */
    qsizetype length() const noexcept { return size(); }
    /*!
        \overload

        Returns the number of characters in the string. This is the same as
        size().
    */
    qsizetype count() const noexcept { return size(); }
    /*!
        Returns the number of characters the string can hold without
        reallocating.

        \sa reserve(), squeeze()
    */
    qsizetype capacity() const noexcept { return static_cast<qsizetype>(d.capacity()); }
    /*!
        Returns \c true if the string has no characters; otherwise returns
        \c false.

        \sa isNull(), size()
    */
    bool isEmpty() const noexcept { return d.empty(); }
    /*!
        Returns \c true if the string is null, that is, default-constructed
        (or cleared) and never given any content; otherwise returns \c false.
        A null string is also empty.

        \sa isEmpty(), clear()
    */
    bool isNull() const noexcept { return m_null && d.empty(); }
    /*!
        Clears the contents of the string and makes it null.

        \sa isNull(), resize()
    */
    void clear() { d.clear(); m_null = true; }
    /*!
        Ensures the string has room for at least \a n characters without
        reallocating.

        \sa capacity(), squeeze()
    */
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    /*!
        Releases any memory not needed to store the characters.

        \sa reserve(), capacity()
    */
    void squeeze() { d.shrink_to_fit(); }
    /*!
        Sets the size of the string to \a n characters. If \a n is greater
        than the current size, the new characters are set to U+0000. The
        string is no longer null afterwards.

        \sa size(), truncate()
    */
    void resize(qsizetype n) { d.resize(static_cast<std::size_t>(n)); m_null = false; }
    /*!
        \overload

        Sets the size of the string to \a n characters. If \a n is greater
        than the current size, the new characters are set to \a c.
    */
    void resize(qsizetype n, QChar c) { d.resize(static_cast<std::size_t>(n), c.unicode()); m_null = false; }
    /*!
        \since 6.8
        Sets the size of the string to \a size characters, for a caller that
        is about to assign every new character. Nt sets them to U+0000.

        \sa resize()
    */
    void resizeForOverwrite(qsizetype size) { resize(size); }
    QString &fill(QChar c, qsizetype n = -1);
    /*!
        Truncates the string at index position \a pos. If \a pos is beyond
        the end of the string, nothing happens; a negative \a pos empties
        the string.

        \sa chop(), resize(), left()
    */
    void truncate(qsizetype pos) { if (pos < size()) d.resize(static_cast<std::size_t>(pos < 0 ? 0 : pos)); }
    /*!
        Removes \a n characters from the end of the string. If \a n is
        greater than or equal to size(), the string becomes empty.

        \sa truncate(), chopped()
    */
    void chop(qsizetype n) { if (n >= size()) d.clear(); else d.resize(d.size() - static_cast<std::size_t>(n)); }

    /*!
        Returns the character at index position \a i. \a i must be a valid
        index; the behavior is undefined otherwise.

        \sa front(), back()
    */
    QChar at(qsizetype i) const { return QChar(d[static_cast<std::size_t>(i)]); }
    /*!
        \overload

        Returns the character at index position \a i. This is the same as
        at().
    */
    QChar operator[](qsizetype i) const { return at(i); }
    /*!
        \overload

        Returns a reference to the character at index position \a i, which
        can be used to modify it. \a i must be a valid index.
    */
    QChar &operator[](qsizetype i) { return data()[i]; }
    /*!
        Returns the first character of the string. The string must not be
        empty.

        \sa back(), at()
    */
    QChar front() const { return at(0); }
    /*!
        Returns the last character of the string. The string must not be
        empty.

        \sa front(), at()
    */
    QChar back() const { return at(size() - 1); }
    /*!
        Returns a pointer to the characters of the string, which can be used
        to modify them. The data is null-terminated.

        \sa constData(), utf16()
    */
    QChar *data() { return reinterpret_cast<QChar *>(d.data()); }
    /*!
        \overload

        Returns a pointer to the characters of the string. The data is
        null-terminated.
    */
    const QChar *data() const { return reinterpret_cast<const QChar *>(d.data()); }
    /*!
        Returns a pointer to the characters of the string. The data is
        null-terminated.

        \sa data(), utf16()
    */
    const QChar *constData() const { return data(); }
    /*!
        Returns the string as a null-terminated array of UTF-16 code units.

        \sa constData(), u16string()
    */
    const char16_t *utf16() const { return d.c_str(); }
    /*!
        Returns a reference to the underlying \c std::u16string.

        \sa toStdU16String(), utf16()
    */
    const std::u16string &u16string() const noexcept { return d; }
    /*!
        Returns an iterator pointing to the first character of the string.

        \sa end(), constBegin()
    */
    iterator begin() { return data(); }
    /*!
        Returns an iterator pointing just after the last character of the
        string.

        \sa begin(), constEnd()
    */
    iterator end() { return data() + size(); }
    /*!
        \overload

        Returns a const iterator pointing to the first character of the string.
    */
    const_iterator begin() const { return data(); }
    /*!
        \overload

        Returns a const iterator pointing just after the last character of
        the string.
    */
    const_iterator end() const { return data() + size(); }
    /*!
        Returns a const iterator pointing to the first character of the string.

        \sa cend(), begin()
    */
    const_iterator cbegin() const { return begin(); }
    /*!
        Returns a const iterator pointing just after the last character of
        the string.

        \sa cbegin(), end()
    */
    const_iterator cend() const { return end(); }
    /*!
        Returns a const iterator pointing to the first character of the string.

        \sa constEnd(), begin()
    */
    const_iterator constBegin() const { return begin(); }
    /*!
        Returns a const iterator pointing just after the last character of
        the string.

        \sa constBegin(), end()
    */
    const_iterator constEnd() const { return end(); }

    // --- modification ---
    /*!
        Appends the string \a s to this string and returns a reference to it.

        \sa prepend(), insert()
    */
    QString &append(const QString &s) { d += s.d; m_null = false; return *this; }
    /*!
        \overload

        Appends the character \a c to this string.
    */
    QString &append(QChar c) { d.push_back(c.unicode()); m_null = false; return *this; }
    /*!
        \overload

        Appends the Latin-1 string \a s to this string.
    */
    QString &append(QLatin1StringView s) { return append(QString(s)); }
    /*!
        \overload

        Appends the UTF-8 encoded string \a s to this string.
    */
    QString &append(const char *s) { return append(QString(s)); }
    /*!
        Appends the character \a c to this string. This function is provided
        for STL compatibility.

        \sa append()
    */
    void push_back(QChar c) { append(c); }
    /*!
        \overload

        Appends the string \a s to this string.
    */
    void push_back(const QString &s) { append(s); }
    /*!
        Prepends the string \a s to this string and returns a reference to it.

        \sa append(), insert()
    */
    QString &prepend(const QString &s) { d.insert(0, s.d); m_null = false; return *this; }
    /*!
        \overload

        Prepends the character \a c to this string.
    */
    QString &prepend(QChar c) { d.insert(d.begin(), c.unicode()); m_null = false; return *this; }
    /*!
        Inserts the string \a s at index position \a pos and returns a
        reference to this string. \a pos must be between 0 and size().

        \sa append(), prepend(), remove()
    */
    QString &insert(qsizetype pos, const QString &s) { d.insert(static_cast<std::size_t>(pos), s.d); m_null = false; return *this; }
    /*!
        \overload

        Inserts the character \a c at index position \a pos.
    */
    QString &insert(qsizetype pos, QChar c) { d.insert(d.begin() + pos, c.unicode()); m_null = false; return *this; }
    QString &remove(qsizetype pos, qsizetype len);
    /*!
        \overload

        Removes every occurrence of the string \a s from this string, using
        the case sensitivity \a cs, and returns a reference to this string.
    */
    QString &remove(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(s, QString(), cs); }
    /*!
        \overload

        Removes every occurrence of the character \a c from this string, using
        the case sensitivity \a cs.
    */
    QString &remove(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(QString(c), QString(), cs); }
    QString &replace(qsizetype pos, qsizetype len, const QString &after);
    QString &replace(const QString &before, const QString &after, Qt::CaseSensitivity cs = Qt::CaseSensitive);
    /*!
        \overload

        Replaces every occurrence of the character \a before with the
        character \a after, using the case sensitivity \a cs.
    */
    QString &replace(QChar before, QChar after, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(QString(before), QString(after), cs); }
    /*!
        \overload

        Replaces every occurrence of the character \a before with the string
        \a after, using the case sensitivity \a cs.
    */
    QString &replace(QChar before, const QString &after, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(QString(before), after, cs); }
    /*!
        Appends the string \a s to this string and returns a reference to it.

        \sa append()
    */
    QString &operator+=(const QString &s) { return append(s); }
    /*!
        \overload

        Appends the character \a c to this string.
    */
    QString &operator+=(QChar c) { return append(c); }
    /*!
        \overload

        Appends the UTF-8 encoded string \a s to this string.
    */
    QString &operator+=(const char *s) { return append(QString(s)); }

    // --- substrings ---
    QString mid(qsizetype pos, qsizetype len = -1) const;
    /*!
        Returns the first \a n characters of the string, or the whole string
        if \a n is greater than size() or negative.

        \sa right(), mid(), first()
    */
    QString left(qsizetype n) const { return mid(0, n); }
    /*!
        Returns the last \a n characters of the string, or the whole string
        if \a n is greater than size().

        \sa left(), mid(), last()
    */
    QString right(qsizetype n) const { return n >= size() ? *this : mid(size() - n, n); }
    /*!
        Returns the first \a n characters of the string. This is the same as
        left().

        \sa last(), left()
    */
    QString first(qsizetype n) const { return left(n); }
    /*!
        Returns the last \a n characters of the string. This is the same as
        right().

        \sa first(), right()
    */
    QString last(qsizetype n) const { return right(n); }
    /*!
        Returns \a n characters of the string starting at index position
        \a pos. This is the same as mid().

        \sa mid(), chopped()
    */
    QString sliced(qsizetype pos, qsizetype n) const { return mid(pos, n); }
    /*!
        \overload

        Returns the part of the string starting at index position \a pos and
        extending to the end.
    */
    QString sliced(qsizetype pos) const { return mid(pos); }
    /*!
        Returns the string with the last \a n characters removed. If \a n is
        larger than size(), the result is the whole string.

        \sa chop(), left()
    */
    QString chopped(qsizetype n) const { return left(size() - n); }
    QString trimmed() const;
    QString simplified() const;
    QString toUpper() const;
    QString toLower() const;
    QString toCaseFolded() const;
    QString repeated(qsizetype times) const;
    QString leftJustified(qsizetype width, QChar fill = QChar(u' '), bool truncate = false) const;
    QString rightJustified(qsizetype width, QChar fill = QChar(u' '), bool truncate = false) const;
    QString section(QChar sep, qsizetype start, qsizetype end = -1) const;
    QString section(const QString &sep, qsizetype start, qsizetype end = -1) const;

    // --- searching ---
    bool startsWith(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \overload

        Returns \c true if the string starts with the character \a c, using
        the case sensitivity \a cs; otherwise returns \c false.
    */
    bool startsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return startsWith(QString(c), cs); }
    bool endsWith(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \overload

        Returns \c true if the string ends with the character \a c, using the
        case sensitivity \a cs; otherwise returns \c false.
    */
    bool endsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return endsWith(QString(c), cs); }
    qsizetype indexOf(const QString &s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \overload

        Returns the index position of the first occurrence of the character
        \a c in the string, searching forward from index position \a from, or
        -1 if it is not found. \a cs selects the case sensitivity.
    */
    qsizetype indexOf(QChar c, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(QString(c), from, cs); }
    qsizetype lastIndexOf(const QString &s, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \overload

        Returns the index position of the last occurrence of the character
        \a c in the string, searching backward from index position \a from,
        or -1 if it is not found. \a cs selects the case sensitivity.
    */
    qsizetype lastIndexOf(QChar c, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return lastIndexOf(QString(c), from, cs); }
    /*!
        Returns \c true if the string contains an occurrence of the string
        \a s, using the case sensitivity \a cs; otherwise returns \c false.

        \sa indexOf(), count()
    */
    bool contains(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(s, 0, cs) != -1; }
    /*!
        \overload

        Returns \c true if the string contains the character \a c, using the
        case sensitivity \a cs; otherwise returns \c false.
    */
    bool contains(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(c, 0, cs) != -1; }
    qsizetype count(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \overload

        Returns the number of occurrences of the character \a c in the
        string, using the case sensitivity \a cs.
    */
    qsizetype count(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return count(QString(c), cs); }
    QStringList split(const QString &sep, Qt::SplitBehavior behavior = Qt::KeepEmptyParts, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    QStringList split(QChar sep, Qt::SplitBehavior behavior = Qt::KeepEmptyParts, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \since 6.0
        Returns a tokenizer that splits this string at each occurrence of
        \a needle without copying the parts: each token is a QStringView of
        this string, which must stay alive and unchanged while the tokenizer
        is used. \a flags may hold a Qt::SplitBehavior and a
        Qt::CaseSensitivity, in either order. Include \c{<QStringTokenizer>}
        to use this function.

        \sa QStringTokenizer, split()
    */
    template <typename Needle, typename... Flags> auto tokenize(Needle &&needle, Flags... flags) const &;
    /*!
        \since 6.0
        \overload

        Returns a tokenizer for a temporary string, splitting it at each
        occurrence of \a needle according to \a flags. The string is moved
        into the tokenizer, so the tokens stay valid for as long as the
        tokenizer lives.
    */
    template <typename Needle, typename... Flags> auto tokenize(Needle &&needle, Flags... flags) &&;
    bool contains(const QRegularExpression &re, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype indexOf(const QRegularExpression &re, qsizetype from = 0, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype lastIndexOf(const QRegularExpression &re, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype lastIndexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype count(const QRegularExpression &re) const;
    QString &replace(const QRegularExpression &re, const QString &after);
    QString &remove(const QRegularExpression &re);
    QStringList split(const QRegularExpression &sep, Qt::SplitBehavior behavior = Qt::KeepEmptyParts) const;

    // --- formatting / numbers ---
    QString arg(const QString &a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const;
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the character
        \a a, padded to \a fieldWidth with \a fillChar.
    */
    QString arg(QChar a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const { return arg(QString(a), fieldWidth, fillChar); }
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the character
        \a a, interpreted as Latin-1 and padded to \a fieldWidth with
        \a fillChar.
    */
    QString arg(char a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const { return arg(QString(QChar(a)), fieldWidth, fillChar); }
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the UTF-8
        encoded string \a a, padded to \a fieldWidth with \a fillChar.
    */
    QString arg(const char *a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const { return arg(QString(a), fieldWidth, fillChar); }
    QString arg(qlonglong a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const;
    QString arg(qulonglong a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const;
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the integer
        \a a written in \a base, padded to \a fieldWidth with \a fillChar.
    */
    QString arg(long a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qlonglong>(a), fieldWidth, base, fillChar); }
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the unsigned
        integer \a a written in \a base, padded to \a fieldWidth with
        \a fillChar.
    */
    QString arg(ulong a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qulonglong>(a), fieldWidth, base, fillChar); }
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the integer
        \a a written in \a base, padded to \a fieldWidth with \a fillChar.
    */
    QString arg(int a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qlonglong>(a), fieldWidth, base, fillChar); }
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the unsigned
        integer \a a written in \a base, padded to \a fieldWidth with
        \a fillChar.
    */
    QString arg(uint a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qulonglong>(a), fieldWidth, base, fillChar); }
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the integer
        \a a written in \a base, padded to \a fieldWidth with \a fillChar.
    */
    QString arg(short a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qlonglong>(a), fieldWidth, base, fillChar); }
    /*!
        \overload

        Replaces the lowest-numbered \c{%n} place marker with the unsigned
        integer \a a written in \a base, padded to \a fieldWidth with
        \a fillChar.
    */
    QString arg(ushort a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qulonglong>(a), fieldWidth, base, fillChar); }
    QString arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1, QChar fillChar = QChar(u' ')) const;
    /*!
        \overload

        Replaces the lowest-numbered place marker with \a a1, the next one
        with \a a2, and each further one with the corresponding argument of
        \a rest. Every argument is converted to a QString. The markers are
        replaced in one pass, so a marker inside an argument is left alone:
        \c{QString("%1 %2").arg("%2", "x")} is \c{"%2 x"}. An argument that
        has no marker is dropped. This overload only takes part in overload
        resolution when none of the arguments is an arithmetic type.
    */
    template <typename A1, typename A2, typename... Rest,
              typename = std::enable_if_t<!std::is_arithmetic_v<A1> && !std::is_arithmetic_v<A2> &&
                                          (!std::is_arithmetic_v<Rest> && ...)>>
    QString arg(const A1 &a1, const A2 &a2, const Rest &...rest) const;

    static QString number(qlonglong n, int base = 10);
    static QString number(qulonglong n, int base = 10);
    /*!
        \overload

        Returns a string holding the representation of the integer \a n in
        \a base.
    */
    static QString number(int n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    /*!
        \overload

        Returns a string holding the representation of the unsigned integer
        \a n in \a base.
    */
    static QString number(uint n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    /*!
        \overload

        Returns a string holding the representation of the integer \a n in
        \a base.
    */
    static QString number(long n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    /*!
        \overload

        Returns a string holding the representation of the unsigned integer
        \a n in \a base.
    */
    static QString number(ulong n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    static QString number(double n, char format = 'g', int precision = 6);
    /*!
        Sets the string to the representation of \a n in \a base and returns a
        reference to it.

        \sa number()
    */
    QString &setNum(qlonglong n, int base = 10) { return *this = number(n, base); }
    /*!
        \overload

        Sets the string to the representation of the unsigned integer \a n in
        \a base.
    */
    QString &setNum(qulonglong n, int base = 10) { return *this = number(n, base); }
    /*!
        \overload

        Sets the string to the representation of the integer \a n in \a base.
    */
    QString &setNum(int n, int base = 10) { return *this = number(n, base); }
    /*!
        \overload

        Sets the string to the representation of the unsigned integer \a n in
        \a base.
    */
    QString &setNum(uint n, int base = 10) { return *this = number(n, base); }
    /*!
        \overload

        Sets the string to the representation of the floating-point value
        \a n, formatted with \a format and \a precision as described for
        number().
    */
    QString &setNum(double n, char format = 'g', int precision = 6) { return *this = number(n, format, precision); }
    int toInt(bool *ok = nullptr, int base = 10) const;
    uint toUInt(bool *ok = nullptr, int base = 10) const;
    long toLong(bool *ok = nullptr, int base = 10) const;
    ulong toULong(bool *ok = nullptr, int base = 10) const;
    qlonglong toLongLong(bool *ok = nullptr, int base = 10) const;
    qulonglong toULongLong(bool *ok = nullptr, int base = 10) const;
    short toShort(bool *ok = nullptr, int base = 10) const;
    ushort toUShort(bool *ok = nullptr, int base = 10) const;
    double toDouble(bool *ok = nullptr) const;
    float toFloat(bool *ok = nullptr) const;

    // --- conversions ---
    static QString fromUtf8(const char *s, qsizetype size = -1);
    /*!
        \overload

        Returns a string decoded from the UTF-8 data in \a b.
    */
    static QString fromUtf8(const QByteArray &b) { return fromUtf8(b.constData(), b.size()); }
    static QString fromLatin1(const char *s, qsizetype size = -1);
    /*!
        \overload

        Returns a string decoded from the Latin-1 data in \a b.
    */
    static QString fromLatin1(const QByteArray &b) { return fromLatin1(b.constData(), b.size()); }
    /*!
        \overload
        \since 6.0

        Returns a string decoded from the Latin-1 data that the view \a b
        refers to. A \c{std::string_view} converts to such a view.
    */
    static QString fromLatin1(QByteArrayView b) { return fromLatin1(b.data(), b.size()); }
    /*!
        Returns a string decoded from the first \a size bytes of \a s, which
        is interpreted as UTF-8. If \a size is negative, \a s must be
        null-terminated. Nt always uses UTF-8 as the local 8-bit encoding.

        \sa fromUtf8(), toLocal8Bit()
    */
    static QString fromLocal8Bit(const char *s, qsizetype size = -1) { return fromUtf8(s, size); }
    /*!
        \overload

        Returns a string decoded from the UTF-8 data in \a b.
    */
    static QString fromLocal8Bit(const QByteArray &b) { return fromUtf8(b); }
    /*!
        Returns a string decoded from the UTF-8 encoded \c std::string \a s.

        \sa toStdString(), fromUtf8()
    */
    static QString fromStdString(const std::string &s) { return fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }
    /*!
        Returns a string holding a copy of the \c std::u16string \a s.

        \sa toStdU16String()
    */
    static QString fromStdU16String(const std::u16string &s) { return QString(s); }
    /*!
        Returns a string holding a copy of the first \a size code units of
        \a s. If \a size is negative, \a s must be null-terminated. The result
        is null if \a s is \c nullptr.

        \sa utf16()
    */
    static QString fromUtf16(const char16_t *s, qsizetype size = -1) { return QString(s, size); }
    /*!
        Returns a string holding a copy of the first \a size characters of
        \a u. Unlike in Qt, the data is copied, not referenced, so \a u does
        not need to stay valid.
    */
    static QString fromRawData(const QChar *u, qsizetype size) { return QString(u, size); }
    QByteArray toUtf8() const;
    QByteArray toLatin1() const;
    /*!
        Returns the string as a UTF-8 encoded byte array. Nt always uses UTF-8
        as the local 8-bit encoding.

        \sa toUtf8(), fromLocal8Bit()
    */
    QByteArray toLocal8Bit() const { return toUtf8(); }
    /*!
        Returns the string as a UTF-8 encoded \c std::string.

        \sa fromStdString()
    */
    std::string toStdString() const { return toUtf8().toStdString(); }
    /*!
        Returns a copy of the string as a \c std::u16string.

        \sa fromStdU16String(), u16string()
    */
    std::u16string toStdU16String() const { return d; }
    std::u32string toStdU32String() const;
    static QString fromStdU32String(const std::u32string &s);
    std::wstring toStdWString() const;
    static QString fromStdWString(const std::wstring &s);
    QList<uint> toUcs4() const;
    static QString fromUcs4(const char32_t *unicode, qsizetype size = -1);
    qsizetype toWCharArray(wchar_t *array) const;
    static QString fromWCharArray(const wchar_t *string, qsizetype size = -1);
    QString normalized(NormalizationForm mode) const;
    QString toHtmlEscaped() const;
    bool isValidUtf16() const noexcept;
    bool isUpper() const;
    bool isLower() const;
    /*!
        Returns a pointer to the characters of the string, followed by a
        \c{'\0'}. Same as constData().
    */
    const QChar *unicode() const { return data(); }
    /*!
        Replaces the contents with the \a size characters at \a unicode and
        returns a reference to this string. If \a unicode is \c nullptr, the
        string is only resized to \a size.

        \sa setUtf16(), setRawData()
    */
    QString &setUnicode(const QChar *unicode, qsizetype size) {
        if (unicode)
            *this = QString(unicode, size);
        else
            resize(size);
        return *this;
    }
    /*!
        Replaces the contents with the \a size UTF-16 code units at \a utf16
        and returns a reference to this string.

        \sa setUnicode()
    */
    QString &setUtf16(const char16_t *utf16, qsizetype size) {
        return setUnicode(reinterpret_cast<const QChar *>(utf16), size);
    }
    /*!
        Replaces the contents with a copy of the \a size characters at
        \a unicode and returns a reference to this string. Unlike in Qt, the
        data is copied.

        \sa fromRawData()
    */
    QString &setRawData(const QChar *unicode, qsizetype size) { return setUnicode(unicode, size); }
    /*!
        \since 6.8
        Removes the characters before index position \a pos and returns a
        reference to this string.

        \sa sliced()
    */
    QString &slice(qsizetype pos) {
        d.erase(0, static_cast<std::size_t>(pos));
        return *this;
    }
    /*!
        \overload
        \since 6.8
        Keeps the \a n characters starting at index position \a pos and
        returns a reference to this string.
    */
    QString &slice(qsizetype pos, qsizetype n) {
        d = d.substr(static_cast<std::size_t>(pos), static_cast<std::size_t>(n));
        return *this;
    }
    /*!
        \since 6.5
        Makes sure that the characters are followed by a \c{'\0'} and returns
        a reference to this string. Nt keeps strings null-terminated already,
        so this only clears the null state of a null string.

        \sa nullTerminated()
    */
    QString &nullTerminate() {
        m_null = false;
        return *this;
    }
    /*!
        \since 6.5
        Returns a copy of the string whose characters are followed by a
        \c{'\0'}.

        \sa nullTerminate()
    */
    QString nullTerminated() const {
        QString copy(*this);
        return copy.nullTerminate();
    }
    /*!
        \since 6.1
        Removes every character for which \a pred returns \c true and returns
        a reference to this string. \a pred is called with a QChar.

        \sa remove()
    */
    template <typename Predicate> QString &removeIf(Predicate pred) {
        d.erase(std::remove_if(d.begin(), d.end(), [&pred](char16_t u) { return pred(QChar(u)); }), d.end());
        return *this;
    }
    /*!
        \since 6.5
        Removes the character at index position \a pos and returns a
        reference to this string. Nothing happens if \a pos is out of range.

        \sa remove(), removeFirst(), removeLast()
    */
    QString &removeAt(qsizetype pos) { return remove(pos, 1); }
    /*!
        \since 6.5
        Removes the first character and returns a reference to this string.
        Nothing happens if the string is empty.

        \sa removeLast(), removeAt()
    */
    QString &removeFirst() { return remove(0, 1); }
    /*!
        \since 6.5
        Removes the last character and returns a reference to this string.
        Nothing happens if the string is empty.

        \sa removeFirst(), chop()
    */
    QString &removeLast() { return remove(size() - 1, 1); }
    /*!
        \since 6.1
        Removes the characters from \a first up to, but not including,
        \a last. Returns an iterator to the character that followed the last
        removed one.

        \sa remove()
    */
    iterator erase(const_iterator first, const_iterator last) {
        const qsizetype start = first - constBegin();
        remove(start, last - first);
        return begin() + start;
    }
    /*!
        \overload
        \since 6.5
        Removes the character that \a it points to and returns an iterator to
        the character after it.
    */
    iterator erase(const_iterator it) { return erase(it, it + 1); }
    /*!
        Inserts \a c at the beginning of the string. Provided for
        compatibility with the standard library.

        \sa prepend(), push_back()
    */
    void push_front(QChar c) { prepend(c); }
    /*!
        \overload
        Inserts \a s at the beginning of the string.
    */
    void push_front(const QString &s) { prepend(s); }
    /*!
        Releases memory that is not needed to hold the characters. Provided
        for compatibility with the standard library.

        \sa squeeze()
    */
    void shrink_to_fit() { squeeze(); }
    /*!
        \since 6.8
        Returns the largest number of characters a string can theoretically
        hold.

        \sa maxSize()
    */
    qsizetype max_size() const noexcept { return maxSize(); }
    /*!
        \since 6.8
        Returns the largest number of characters a string can theoretically
        hold.

        \sa max_size()
    */
    static constexpr qsizetype maxSize() noexcept {
        return static_cast<qsizetype>((~std::size_t(0) >> 1) / sizeof(char16_t)) - 1;
    }
    /*!
        Returns a reverse iterator pointing to the last character of the
        string.

        \sa rend(), crbegin()
    */
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    /*!
        \overload
    */
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    /*!
        Returns a reverse iterator pointing just before the first character
        of the string.

        \sa rbegin(), crend()
    */
    reverse_iterator rend() { return reverse_iterator(begin()); }
    /*!
        \overload
    */
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    /*!
        Returns a const reverse iterator pointing to the last character of
        the string.

        \sa rbegin(), crend()
    */
    const_reverse_iterator crbegin() const { return rbegin(); }
    /*!
        Returns a const reverse iterator pointing just before the first
        character of the string.

        \sa rend(), crbegin()
    */
    const_reverse_iterator crend() const { return rend(); }
    static QString vasprintf(const char *fmt, va_list ap) __attribute__((format(printf, 1, 0)));
    QString &assign(const QAnyStringView &v);
    /*!
        \since 6.6
        Replaces the contents with \a n copies of \a c and returns a reference
        to this string.
    */
    QString &assign(qsizetype n, QChar c) {
        d.assign(static_cast<std::size_t>(n), c.unicode());
        m_null = false;
        return *this;
    }
    /*!
        \since 6.6
        \overload
        Replaces the contents with the characters in the range [\a first,
        \a last), each of which must convert to QChar, and returns a reference
        to this string.
    */
    template <typename InputIterator, typename = typename std::iterator_traits<InputIterator>::iterator_category>
    QString &assign(InputIterator first, InputIterator last) {
        d.clear();
        for (; first != last; ++first)
            d.push_back(QChar(*first).unicode());
        m_null = false;
        return *this;
    }

    int compare(const QString &o, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \overload

        Compares the strings \a a and \a b using the case sensitivity \a cs.
    */
    static int compare(const QString &a, const QString &b, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return a.compare(b, cs); }
    static int localeAwareCompare(const QString &a, const QString &b);
    bool isRightToLeft() const noexcept;
    /*!
        Swaps this string with \a o. This operation is very fast and never
        fails.
    */
    void swap(QString &o) noexcept { d.swap(o.d); std::swap(m_null, o.m_null); }
    static QString asprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

    friend bool operator==(const QString &a, const QString &b) noexcept { return a.d == b.d; }
    friend bool operator!=(const QString &a, const QString &b) noexcept { return a.d != b.d; }
    friend bool operator<(const QString &a, const QString &b) noexcept { return a.d < b.d; }
    friend bool operator>(const QString &a, const QString &b) noexcept { return a.d > b.d; }
    friend bool operator<=(const QString &a, const QString &b) noexcept { return a.d <= b.d; }
    friend bool operator>=(const QString &a, const QString &b) noexcept { return a.d >= b.d; }
    friend QString operator+(const QString &a, const QString &b) { QString r(a); r.append(b); return r; }
    friend QString operator+(const QString &a, QChar c) { QString r(a); r.append(c); return r; }
    friend QString operator+(QChar c, const QString &a) { QString r(c); r.append(a); return r; }
    friend QString operator+(const QString &a, const char *s) { QString r(a); r.append(QString(s)); return r; }
    friend QString operator+(const char *s, const QString &a) { QString r(s); r.append(a); return r; }

private:
    std::u16string d;
    bool m_null = true;
};

/*!
    Returns the view as a QString.
*/
inline QString QLatin1StringView::toString() const { return QString(*this); }
inline short QLatin1StringView::toShort(bool *ok, int base) const { return toString().toShort(ok, base); }
inline ushort QLatin1StringView::toUShort(bool *ok, int base) const { return toString().toUShort(ok, base); }
inline int QLatin1StringView::toInt(bool *ok, int base) const { return toString().toInt(ok, base); }
inline uint QLatin1StringView::toUInt(bool *ok, int base) const { return toString().toUInt(ok, base); }
inline long QLatin1StringView::toLong(bool *ok, int base) const { return toString().toLong(ok, base); }
inline ulong QLatin1StringView::toULong(bool *ok, int base) const { return toString().toULong(ok, base); }
inline qlonglong QLatin1StringView::toLongLong(bool *ok, int base) const { return toString().toLongLong(ok, base); }
inline qulonglong QLatin1StringView::toULongLong(bool *ok, int base) const { return toString().toULongLong(ok, base); }
inline float QLatin1StringView::toFloat(bool *ok) const { return toString().toFloat(ok); }
inline double QLatin1StringView::toDouble(bool *ok) const { return toString().toDouble(ok); }
/*!
    Returns \c true if the view starts with \a s, compared with \a cs.
*/
inline bool QLatin1StringView::startsWith(const QString &s, Qt::CaseSensitivity cs) const {
    return toString().startsWith(s, cs);
}
/*!
    \overload
    Returns \c true if the view starts with \a c, compared with \a cs.
*/
inline bool QLatin1StringView::startsWith(QChar c, Qt::CaseSensitivity cs) const {
    return toString().startsWith(c, cs);
}
/*!
    Returns \c true if the view ends with \a s, compared with \a cs.
*/
inline bool QLatin1StringView::endsWith(const QString &s, Qt::CaseSensitivity cs) const {
    return toString().endsWith(s, cs);
}
/*!
    \overload
    Returns \c true if the view ends with \a c, compared with \a cs.
*/
inline bool QLatin1StringView::endsWith(QChar c, Qt::CaseSensitivity cs) const { return toString().endsWith(c, cs); }
/*!
    Returns the position of the first \a s at or after \a from, compared
    with \a cs, or -1.
*/
inline qsizetype QLatin1StringView::indexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
    return toString().indexOf(s, from, cs);
}
/*!
    \overload
    Returns the position of the first \a c at or after \a from, compared
    with \a cs, or -1.
*/
inline qsizetype QLatin1StringView::indexOf(QChar c, qsizetype from, Qt::CaseSensitivity cs) const {
    return toString().indexOf(c, from, cs);
}
/*!
    Returns the position of the last \a s at or before \a from (-1 means
    the end), compared with \a cs, or -1.
*/
inline qsizetype QLatin1StringView::lastIndexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
    return toString().lastIndexOf(s, from, cs);
}
/*!
    \overload
    Returns the position of the last \a c at or before \a from, compared
    with \a cs, or -1.
*/
inline qsizetype QLatin1StringView::lastIndexOf(QChar c, qsizetype from, Qt::CaseSensitivity cs) const {
    return toString().lastIndexOf(c, from, cs);
}
/*!
    Returns \c true if the view contains \a s, compared with \a cs.
*/
inline bool QLatin1StringView::contains(const QString &s, Qt::CaseSensitivity cs) const {
    return indexOf(s, 0, cs) != -1;
}
/*!
    \overload
    Returns \c true if the view contains \a c, compared with \a cs.
*/
inline bool QLatin1StringView::contains(QChar c, Qt::CaseSensitivity cs) const { return indexOf(c, 0, cs) != -1; }
/*!
    Returns how often \a s occurs, possibly overlapping, compared with \a cs.
*/
inline qsizetype QLatin1StringView::count(const QString &s, Qt::CaseSensitivity cs) const {
    return toString().count(s, cs);
}
/*!
    \overload
    Returns how often \a c occurs, compared with \a cs.
*/
inline qsizetype QLatin1StringView::count(QChar c, Qt::CaseSensitivity cs) const { return toString().count(c, cs); }
/*!
    Compares the view with \a s, using \a cs, and returns a negative number,
    zero or a positive number as the view sorts before, equal to or after it.
*/
inline int QLatin1StringView::compare(const QString &s, Qt::CaseSensitivity cs) const {
    return toString().compare(s, cs);
}

inline std::size_t qHash(const QString &s, std::size_t seed = 0) noexcept {
    return std::hash<std::u16string>{}(s.u16string()) ^ (seed * 0x9e3779b9u);
}

namespace QtPrivate {
// Replaces the place markers of pattern with the count strings at args, in one pass.
QString multiArg(const QString &pattern, const QString *args, qsizetype count);

template <typename T, typename = void> struct HasToString : std::false_type {};
template <typename T>
struct HasToString<T, std::void_t<decltype(std::declval<const T &>().toString())>>
    : std::is_convertible<decltype(std::declval<const T &>().toString()), QString> {};

// The text of an argument of arg(): a view is copied, anything else is converted to a QString.
template <typename T> QString argText(const T &a) {
    if constexpr (HasToString<T>::value)
        return a.toString();
    else
        return QString(a);
}
} // namespace QtPrivate

template <typename A1, typename A2, typename... Rest, typename>
QString QString::arg(const A1 &a1, const A2 &a2, const Rest &...rest) const {
    const QString texts[] = {QtPrivate::argText(a1), QtPrivate::argText(a2), QtPrivate::argText(rest)...};
    return QtPrivate::multiArg(*this, texts, qsizetype(2) + static_cast<qsizetype>(sizeof...(Rest)));
}

template <typename A, typename... Rest> QString QLatin1StringView::arg(const A &a, const Rest &...rest) const {
    const QString texts[] = {QtPrivate::argText(a), QtPrivate::argText(rest)...};
    return QtPrivate::multiArg(toString(), texts, static_cast<qsizetype>(1 + sizeof...(Rest)));
}

template <>
struct std::hash<QString> {
    std::size_t operator()(const QString &s) const noexcept { return std::hash<std::u16string>{}(s.u16string()); }
};

#define QStringLiteral(str) QString::fromUtf8("" str, static_cast<qsizetype>(sizeof(str) - 1))
#define QLatin1StringLiteral(str) QLatin1StringView(str)
#define QT_TR_NOOP(x) x
#define QT_TRANSLATE_NOOP(scope, x) x

namespace Qt {
/*!
    The Qt::Literals inline namespace holds the user-defined literal operators
    of Nt, so they are found through \c{using namespace Qt::Literals},
    \c{using namespace Qt::StringLiterals} or \c{using namespace Qt}. The
    string literals \c{_s} (a QString from a \c{u""} literal) and \c{_L1} (a
    QLatin1StringView or QChar from a Latin-1 literal) are provided.
*/
inline namespace Literals {
inline namespace StringLiterals {
inline QString operator""_s(const char16_t *s, std::size_t n) { return QString(s, static_cast<qsizetype>(n)); }
constexpr QLatin1StringView operator""_L1(const char *s, std::size_t n) { return QLatin1StringView(s, static_cast<qsizetype>(n)); }
inline QChar operator""_L1(char c) { return QChar(c); }
} // namespace StringLiterals
} // namespace Literals
} // namespace Qt

#include <QtCore/qstringlist.h>
// The views come with the string, as they do with Qt's <QString>: tokenize() returns a QStringTokenizer.
#include <QtCore/qstringview.h>
