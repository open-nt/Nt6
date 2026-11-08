// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include <functional>
#include <string>
#include <type_traits>

class QString;
class QStringList;
class QRegularExpression;
class QRegularExpressionMatch;

/*!
    \class QLatin1StringView
    \brief The QLatin1StringView class provides a thin wrapper around a US-ASCII/Latin-1 encoded string literal.

    A QLatin1StringView refers to a sequence of Latin-1 bytes without owning
    or copying them, so the viewed memory must outlive it. It is mainly used
    to pass 8-bit literals to QString functions without an intermediate
    conversion. \c QLatin1String is an alias for this class.

    \differences
    Only construction, data access, size() and the subscript operators are
    provided. Qt's searching, comparison, slicing and iteration functions, the
    comparison operators and the \c QLatin1StringView(QByteArrayView)
    constructor are not available.
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

private:
    const char *p = "";
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
    /*!
        Returns the case folded equivalent of the string. This is the same as
        toLower().

        \sa toLower()
    */
    QString toCaseFolded() const { return toLower(); }
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
        \a rest. Every argument is converted to a QString. This overload only
        takes part in overload resolution when none of the arguments is an
        arithmetic type.
    */
    template <typename A1, typename A2, typename... Rest,
              typename = std::enable_if_t<!std::is_arithmetic_v<A1> && !std::is_arithmetic_v<A2> &&
                                          (!std::is_arithmetic_v<Rest> && ...)>>
    QString arg(const A1 &a1, const A2 &a2, const Rest &...rest) const {
        QString s = arg(QString(a1)).arg(QString(a2));
        ((s = s.arg(QString(rest))), ...);
        return s;
    }

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

    int compare(const QString &o, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        \overload

        Compares the strings \a a and \a b using the case sensitivity \a cs.
    */
    static int compare(const QString &a, const QString &b, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return a.compare(b, cs); }
    static int localeAwareCompare(const QString &a, const QString &b);
    /*!
        Returns \c false; right-to-left text detection is not implemented.
    */
    bool isRightToLeft() const { return false; }
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

inline std::size_t qHash(const QString &s, std::size_t seed = 0) noexcept {
    return std::hash<std::u16string>{}(s.u16string()) ^ (seed * 0x9e3779b9u);
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
