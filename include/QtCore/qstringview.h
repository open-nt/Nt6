// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <string_view>

// A read-only, non-owning view of UTF-16 text.
/*!
    \class QStringView
    \brief The QStringView class provides a unified view on UTF-16 strings with a read-only subset of the QString API.

    A QStringView refers to a contiguous range of QChars without owning or
    copying it, so the viewed data must outlive the view. It is cheap to copy
    and is meant for function parameters that only read text, accepting a
    QString, a \c{u""} literal or a \c std::u16string_view alike.

    A view is \e null if it was constructed without data (or from a null
    QString), and empty if its size is 0. A null view is also empty.

    \differences
    Only part of the Qt API is provided: construction, element access,
    slicing, trimmed(), startsWith(), endsWith(), indexOf(), lastIndexOf(),
    contains(), compare(), a few number conversions (toInt(), toUInt(),
    toLongLong(), toULongLong() and toDouble()), the QRegularExpression
    searches and the comparison operators.
    The searching, comparison and conversion functions copy the view into a
    QString first, so they allocate. \c split(), \c count(), \c toLocal8Bit(),
    \c toUcs4(), the other \c toXxx() number conversions, the reverse
    iterators, \c isRightToLeft() and the QLatin1StringView overloads are not
    provided. first(), last(), sliced(),
    chopped(), truncate() and chop() do not check their arguments.
    \enddifferences

    \sa QString, QStringList, QLatin1StringView
*/
class QStringView {
public:
    /*!
        A read-only iterator over the characters of the view. It is a plain \c{const QChar *}.
    */
    using const_iterator = const QChar *;
    /*!
        The iterator type, for STL compatibility. It is the same as const_iterator.
    */
    using iterator = const_iterator;
    /*!
        The element type of the view, for STL compatibility.
    */
    using value_type = QChar;

    /*!
        Constructs a null view.

        \sa isNull()
    */
    constexpr QStringView() noexcept = default;
    /*!
        \overload

        Constructs a null view.
    */
    constexpr QStringView(std::nullptr_t) noexcept {}
    /*!
        \overload

        Constructs a view of the \a len characters starting at \a str.
    */
    constexpr QStringView(const QChar *str, qsizetype len) noexcept : p(str), n(len) {}
    /*!
        \overload

        Constructs a view of the \a len UTF-16 code units starting at \a str.
    */
    QStringView(const char16_t *str, qsizetype len) noexcept : p(reinterpret_cast<const QChar *>(str)), n(len) {}
    /*!
        \overload

        Constructs a view of the null-terminated UTF-16 string \a str. The
        view is null if \a str is \c nullptr.
    */
    QStringView(const char16_t *str) noexcept
        : p(reinterpret_cast<const QChar *>(str)), n(str ? static_cast<qsizetype>(std::char_traits<char16_t>::length(str)) : 0) {}
    /*!
        \overload

        Constructs a view of the characters of the string \a s. The view is
        null if \a s is null. It is invalidated when \a s is modified or
        destroyed.
    */
    QStringView(const QString &s) noexcept : p(s.isNull() ? nullptr : s.constData()), n(s.size()) {}
    /*!
        \overload

        Constructs a view of the UTF-16 string view \a s.
    */
    QStringView(std::u16string_view s) noexcept : p(reinterpret_cast<const QChar *>(s.data())), n(static_cast<qsizetype>(s.size())) {}

    /*!
        Returns a copy of the viewed characters as a QString. The result is
        null if the view is null.

        \sa toUtf8(), toLatin1()
    */
    QString toString() const { return p ? QString(p, n) : QString(); }
    /*!
        Returns the viewed characters as a UTF-8 encoded byte array.

        \sa QString::toUtf8()
    */
    QByteArray toUtf8() const { return toString().toUtf8(); }
    /*!
        Returns the viewed characters as a Latin-1 encoded byte array.
        Characters outside Latin-1 are replaced by \c{'?'}.

        \sa QString::toLatin1()
    */
    QByteArray toLatin1() const { return toString().toLatin1(); }
    /*!
        Returns a pointer to the first character of the view. The data is not
        necessarily null-terminated.

        \sa constData(), utf16()
    */
    constexpr const QChar *data() const noexcept { return p; }
    /*!
        Returns a pointer to the first character of the view. This is the same
        as data().
    */
    constexpr const QChar *constData() const noexcept { return p; }
    /*!
        Returns a pointer to the first character of the view as UTF-16 code
        units. The data is not necessarily null-terminated.

        \sa data()
    */
    const char16_t *utf16() const noexcept { return reinterpret_cast<const char16_t *>(p); }
    /*!
        Returns the number of characters in the view.

        \sa isEmpty()
    */
    constexpr qsizetype size() const noexcept { return n; }
    /*!
        Returns the number of characters in the view. This is the same as
        size().
    */
    constexpr qsizetype length() const noexcept { return n; }
    /*!
        Returns \c true if the view has no data (it was default-constructed or
        built from a null string or pointer); otherwise returns \c false.

        \sa isEmpty()
    */
    constexpr bool isNull() const noexcept { return p == nullptr; }
    /*!
        Returns \c true if the view has no characters; otherwise returns
        \c false.

        \sa isNull(), size()
    */
    constexpr bool isEmpty() const noexcept { return n == 0; }
    /*!
        Returns \c true if the view has no characters. This is the same as
        isEmpty(), provided for STL compatibility.
    */
    constexpr bool empty() const noexcept { return n == 0; }
    /*!
        Returns the character at index position \a i. \a i must be a valid
        index; no bounds checking is done.

        \sa front(), back()
    */
    QChar at(qsizetype i) const { return p[i]; }
    /*!
        Returns the character at index position \a i. This is the same as
        at().
    */
    QChar operator[](qsizetype i) const { return p[i]; }
    /*!
        Returns the first character of the view. The view must not be empty.

        \sa back(), first()
    */
    QChar front() const { return p[0]; }
    /*!
        Returns the last character of the view. The view must not be empty.

        \sa front(), last()
    */
    QChar back() const { return p[n - 1]; }
    /*!
        \overload

        Returns the first character of the view. This is the same as front().
    */
    QChar first() const { return front(); }
    /*!
        \overload

        Returns the last character of the view. This is the same as back().
    */
    QChar last() const { return back(); }
    /*!
        Returns an iterator pointing to the first character of the view.

        \sa end()
    */
    const_iterator begin() const noexcept { return p; }
    /*!
        Returns an iterator pointing just after the last character of the view.

        \sa begin()
    */
    const_iterator end() const noexcept { return p + n; }

    /*!
        Returns a view of \a len characters starting at index position
        \a pos. If \a len is negative or extends past the end, the view
        extends to the end. A negative \a pos is clamped to 0 and reduces
        \a len accordingly. If \a pos is past the end, an empty view is
        returned.

        \sa left(), right(), sliced()
    */
    QStringView mid(qsizetype pos, qsizetype len = -1) const {
        if (pos < 0) {
            if (len >= 0)
                len = qMax<qsizetype>(len + pos, 0);
            pos = 0;
        }
        if (pos >= n)
            return QStringView(p ? p + n : nullptr, 0);
        return QStringView(p + pos, len < 0 || len > n - pos ? n - pos : len);
    }
    /*!
        Returns the first \a c characters of the view, or the whole view if
        \a c is larger than size().

        \sa right(), mid(), first()
    */
    QStringView left(qsizetype c) const { return mid(0, qMax<qsizetype>(c, 0)); }
    /*!
        Returns the last \a c characters of the view, or the whole view if
        \a c is larger than size().

        \sa left(), mid(), last()
    */
    QStringView right(qsizetype c) const { return c >= n ? *this : mid(n - qMax<qsizetype>(c, 0)); }
    /*!
        \overload

        Returns a view of the first \a c characters. \a c must not exceed
        size(); no bounds checking is done.
    */
    QStringView first(qsizetype c) const { return QStringView(p, c); }
    /*!
        \overload

        Returns a view of the last \a c characters. \a c must not exceed
        size(); no bounds checking is done.
    */
    QStringView last(qsizetype c) const { return QStringView(p + n - c, c); }
    /*!
        Returns a view of the part starting at index position \a pos and
        extending to the end. \a pos must not exceed size(); no bounds
        checking is done.

        \sa mid(), chopped()
    */
    QStringView sliced(qsizetype pos) const { return QStringView(p + pos, n - pos); }
    /*!
        \overload

        Returns a view of \a c characters starting at index position \a pos.
        The range must lie within the view; no bounds checking is done.
    */
    QStringView sliced(qsizetype pos, qsizetype c) const { return QStringView(p + pos, c); }
    /*!
        Returns the view with the last \a c characters removed. \a c must not
        exceed size(); no bounds checking is done.

        \sa chop(), sliced()
    */
    QStringView chopped(qsizetype c) const { return QStringView(p, n - c); }
    /*!
        Truncates the view to \a c characters. \a c must not exceed size(); no
        bounds checking is done.

        \sa chop()
    */
    void truncate(qsizetype c) { n = c; }
    /*!
        Removes \a c characters from the end of the view. \a c must not exceed
        size(); no bounds checking is done.

        \sa truncate(), chopped()
    */
    void chop(qsizetype c) { n -= c; }
    /*!
        Returns a view with white space removed from the start and the end.

        \sa QChar::isSpace(), QString::trimmed()
    */
    QStringView trimmed() const {
        qsizetype b = 0, e = n;
        while (b < e && p[b].isSpace())
            ++b;
        while (e > b && p[e - 1].isSpace())
            --e;
        return QStringView(p + b, e - b);
    }

    /*!
        Returns \c true if the view starts with \a s, using the case
        sensitivity \a cs; otherwise returns \c false.

        \sa endsWith()
    */
    bool startsWith(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().startsWith(s.toString(), cs); }
    /*!
        Returns \c true if the view ends with \a s, using the case sensitivity
        \a cs; otherwise returns \c false.

        \sa startsWith()
    */
    bool endsWith(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().endsWith(s.toString(), cs); }
    /*!
        Returns the index position of the first occurrence of \a s in the
        view, searching forward from index position \a from, or -1 if it is
        not found. \a cs selects the case sensitivity.

        \sa lastIndexOf(), contains()
    */
    qsizetype indexOf(QStringView s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().indexOf(s.toString(), from, cs); }
    /*!
        Returns the index position of the last occurrence of \a s in the view,
        searching backward from index position \a from, or -1 if it is not
        found. \a cs selects the case sensitivity.

        \sa indexOf()
    */
    qsizetype lastIndexOf(QStringView s, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().lastIndexOf(s.toString(), from, cs); }
    /*!
        Returns \c true if the view contains \a s, using the case sensitivity
        \a cs; otherwise returns \c false.

        \sa indexOf()
    */
    bool contains(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(s, 0, cs) != -1; }
    /*!
        \overload

        Returns \c true if the view starts with the character \a c, using the
        case sensitivity \a cs; otherwise returns \c false.
    */
    bool startsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return n > 0 && QStringView(&c, 1).startsWith(QStringView(p, 1), cs); }
    /*!
        \overload

        Returns \c true if the view ends with the character \a c, using the
        case sensitivity \a cs; otherwise returns \c false.
    */
    bool endsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return n > 0 && QStringView(&c, 1).startsWith(QStringView(p + n - 1, 1), cs); }
    /*!
        \overload

        Returns the index position of the first occurrence of the character
        \a c in the view, searching forward from index position \a from, or -1
        if it is not found. \a cs selects the case sensitivity.
    */
    qsizetype indexOf(QChar c, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(QStringView(&c, 1), from, cs); }
    /*!
        \overload

        Returns the index position of the last occurrence of the character
        \a c in the view, searching backward from index position \a from, or
        -1 if it is not found. \a cs selects the case sensitivity.
    */
    qsizetype lastIndexOf(QChar c, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return lastIndexOf(QStringView(&c, 1), from, cs); }
    /*!
        \overload

        Returns \c true if the view contains the character \a c, using the
        case sensitivity \a cs; otherwise returns \c false.
    */
    bool contains(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(c, 0, cs) != -1; }
    /*!
        Compares the view with \a s, using the case sensitivity \a cs, and
        returns a negative integer, zero or a positive integer if the view is
        less than, equal to or greater than \a s. See QString::compare().
    */
    int compare(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().compare(s.toString(), cs); }
    bool contains(const QRegularExpression &re, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype indexOf(const QRegularExpression &re, qsizetype from = 0, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype lastIndexOf(const QRegularExpression &re, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype lastIndexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch = nullptr) const;
    qsizetype count(const QRegularExpression &re) const;
    /*!
        Returns the view converted to an \c int using \a base. See
        QString::toInt() for how \a ok is set and which inputs are accepted.

        \sa toUInt(), toLongLong()
    */
    int toInt(bool *ok = nullptr, int base = 10) const { return toString().toInt(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned int} using \a base. See
        QString::toUInt() for how \a ok is set.

        \sa toInt()
    */
    uint toUInt(bool *ok = nullptr, int base = 10) const { return toString().toUInt(ok, base); }
    /*!
        Returns the view converted to a \c{long long} using \a base. See
        QString::toLongLong() for how \a ok is set.

        \sa toULongLong(), toInt()
    */
    qlonglong toLongLong(bool *ok = nullptr, int base = 10) const { return toString().toLongLong(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned long long} using \a base.
        See QString::toULongLong() for how \a ok is set.

        \sa toLongLong()
    */
    qulonglong toULongLong(bool *ok = nullptr, int base = 10) const { return toString().toULongLong(ok, base); }
    /*!
        Returns the view converted to a \c double. See QString::toDouble() for
        how \a ok is set and which inputs are accepted.
    */
    double toDouble(bool *ok = nullptr) const { return toString().toDouble(ok); }
    /*!
        Returns a const iterator pointing to the first character of the view.
        This is the same as begin().

        \sa cend()
    */
    const_iterator cbegin() const noexcept { return p; }
    /*!
        Returns a const iterator pointing just after the last character of the
        view. This is the same as end().

        \sa cbegin()
    */
    const_iterator cend() const noexcept { return p + n; }

    friend bool operator==(QStringView a, QStringView b) noexcept {
        return a.n == b.n && std::char_traits<char16_t>::compare(a.utf16(), b.utf16(), static_cast<std::size_t>(a.n)) == 0;
    }
    friend bool operator!=(QStringView a, QStringView b) noexcept { return !(a == b); }
    friend bool operator<(QStringView a, QStringView b) noexcept {
        return std::u16string_view(a.utf16(), static_cast<std::size_t>(a.n)) < std::u16string_view(b.utf16(), static_cast<std::size_t>(b.n));
    }
    friend bool operator>(QStringView a, QStringView b) noexcept { return b < a; }
    friend bool operator<=(QStringView a, QStringView b) noexcept { return !(b < a); }
    friend bool operator>=(QStringView a, QStringView b) noexcept { return !(a < b); }

private:
    const QChar *p = nullptr;
    qsizetype n = 0;
};
