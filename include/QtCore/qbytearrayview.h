// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>

#include <iterator>
#include <string_view>

// A read-only, non-owning view of bytes.
/*!
    \class QByteArrayView
    \brief The QByteArrayView class provides a view on an array of bytes with a read-only subset of the QByteArray API.

    A QByteArrayView refers to a contiguous range of bytes without owning or
    copying it, so the viewed data must outlive the view. It is cheap to copy
    and is meant for function parameters that only read bytes, accepting a
    QByteArray, a string literal or a \c std::string_view alike.

    A view is \e null if it was constructed without data (or from a null
    QByteArray), and empty if its size is 0. A null view is also empty.

    \differences
    The number conversions copy the bytes into a QByteArray first, so they
    allocate. There is no constructor from a container of bytes other than
    QByteArray and \c std::string_view; use fromArray() for a literal array.
    first(), last(), sliced(), chopped(), slice(), truncate() and chop() do
    not check their arguments.
    \enddifferences

    \sa QByteArray, QStringView
*/
class QByteArrayView {
public:
    /*!
        A read-only iterator over the bytes of the view. It is a plain \c{const char *}.
    */
    using const_iterator = const char *;
    /*!
        The iterator type, for STL compatibility. It is the same as const_iterator.
    */
    using iterator = const_iterator;
    /*!
        The element type of the view, for STL compatibility.
    */
    using value_type = char;

    /*!
        Constructs a null view.

        \sa isNull()
    */
    constexpr QByteArrayView() noexcept = default;
    /*!
        \overload

        Constructs a null view.
    */
    constexpr QByteArrayView(std::nullptr_t) noexcept {}
    /*!
        \overload

        Constructs a view of the \a len bytes starting at \a data.
    */
    constexpr QByteArrayView(const char *data, qsizetype len) noexcept : p(data), n(len) {}
    /*!
        \overload

        Constructs a view of the null-terminated string \a data, not including
        the terminator. The view is null if \a data is \c nullptr.
    */
    QByteArrayView(const char *data) noexcept : p(data), n(data ? static_cast<qsizetype>(std::char_traits<char>::length(data)) : 0) {}
    /*!
        \overload

        Constructs a view of the contents of the byte array \a b. The view is
        null if \a b is null. It is invalidated when \a b is modified or
        destroyed.
    */
    QByteArrayView(const QByteArray &b) noexcept : p(b.isNull() ? nullptr : b.constData()), n(b.size()) {}
    /*!
        \overload

        Constructs a view of the characters of the string view \a s.
    */
    QByteArrayView(std::string_view s) noexcept : p(s.data()), n(static_cast<qsizetype>(s.size())) {}

    /*!
        Returns a copy of the viewed bytes as a QByteArray. The result is null
        if the view is null.
    */
    QByteArray toByteArray() const { return p ? QByteArray(p, n) : QByteArray(); }
    /*!
        Returns a pointer to the first byte of the view. The data is not
        necessarily null-terminated.

        \sa constData()
    */
    constexpr const char *data() const noexcept { return p; }
    /*!
        Returns a pointer to the first byte of the view. This is the same as
        data().
    */
    constexpr const char *constData() const noexcept { return p; }
    /*!
        Returns the number of bytes in the view.

        \sa isEmpty()
    */
    constexpr qsizetype size() const noexcept { return n; }
    /*!
        Returns the number of bytes in the view. This is the same as size().
    */
    constexpr qsizetype length() const noexcept { return n; }
    /*!
        Returns \c true if the view has no data (it was default-constructed or
        built from a null array or pointer); otherwise returns \c false.

        \sa isEmpty()
    */
    constexpr bool isNull() const noexcept { return p == nullptr; }
    /*!
        Returns \c true if the view has no bytes; otherwise returns \c false.

        \sa isNull(), size()
    */
    constexpr bool isEmpty() const noexcept { return n == 0; }
    /*!
        Returns \c true if the view has no bytes. This is the same as
        isEmpty(), provided for STL compatibility.
    */
    constexpr bool empty() const noexcept { return n == 0; }
    /*!
        Returns the byte at index position \a i. \a i must be a valid index;
        no bounds checking is done.

        \sa front(), back()
    */
    constexpr char at(qsizetype i) const { return p[i]; }
    /*!
        Returns the byte at index position \a i. This is the same as at().
    */
    constexpr char operator[](qsizetype i) const { return p[i]; }
    /*!
        Returns the first byte of the view. The view must not be empty.

        \sa back()
    */
    constexpr char front() const { return p[0]; }
    /*!
        Returns the last byte of the view. The view must not be empty.

        \sa front()
    */
    constexpr char back() const { return p[n - 1]; }
    /*!
        Returns an iterator pointing to the first byte of the view.

        \sa end()
    */
    constexpr const_iterator begin() const noexcept { return p; }
    /*!
        Returns an iterator pointing just after the last byte of the view.

        \sa begin()
    */
    constexpr const_iterator end() const noexcept { return p + n; }

    /*!
        Returns a view of \a len bytes starting at index position \a pos. If
        \a len is negative or extends past the end, the view extends to the
        end. A negative \a pos is clamped to 0 and reduces \a len
        accordingly. If \a pos is past the end, an empty view is returned.

        \sa left(), right(), sliced()
    */
    QByteArrayView mid(qsizetype pos, qsizetype len = -1) const {
        if (pos < 0) {
            if (len >= 0)
                len = qMax<qsizetype>(len + pos, 0);
            pos = 0;
        }
        if (pos >= n)
            return QByteArrayView(p ? p + n : nullptr, 0);
        return QByteArrayView(p + pos, len < 0 || len > n - pos ? n - pos : len);
    }
    /*!
        Returns the first \a c bytes of the view, or the whole view if \a c is
        larger than size().

        \sa right(), mid(), first()
    */
    QByteArrayView left(qsizetype c) const { return mid(0, qMax<qsizetype>(c, 0)); }
    /*!
        Returns the last \a c bytes of the view, or the whole view if \a c is
        larger than size().

        \sa left(), mid(), last()
    */
    QByteArrayView right(qsizetype c) const { return c >= n ? *this : mid(n - qMax<qsizetype>(c, 0)); }
    /*!
        Returns a view of the first \a c bytes. \a c must not exceed size();
        no bounds checking is done.

        \sa last(), left()
    */
    constexpr QByteArrayView first(qsizetype c) const { return QByteArrayView(p, c); }
    /*!
        Returns a view of the last \a c bytes. \a c must not exceed size(); no
        bounds checking is done.

        \sa first(), right()
    */
    constexpr QByteArrayView last(qsizetype c) const { return QByteArrayView(p + n - c, c); }
    /*!
        Returns a view of the part starting at index position \a pos and
        extending to the end. \a pos must not exceed size(); no bounds
        checking is done.

        \sa mid(), chopped()
    */
    constexpr QByteArrayView sliced(qsizetype pos) const { return QByteArrayView(p + pos, n - pos); }
    /*!
        \overload

        Returns a view of \a c bytes starting at index position \a pos. The
        range must lie within the view; no bounds checking is done.
    */
    constexpr QByteArrayView sliced(qsizetype pos, qsizetype c) const { return QByteArrayView(p + pos, c); }
    /*!
        Returns the view with the last \a c bytes removed. \a c must not exceed
        size(); no bounds checking is done.

        \sa sliced()
    */
    constexpr QByteArrayView chopped(qsizetype c) const { return QByteArrayView(p, n - c); }

    /*!
        Returns \c true if the view starts with \a s; otherwise returns
        \c false.

        \sa endsWith()
    */
    bool startsWith(QByteArrayView s) const { return s.n <= n && view().substr(0, static_cast<std::size_t>(s.n)) == s.view(); }
    /*!
        Returns \c true if the view ends with \a s; otherwise returns
        \c false.

        \sa startsWith()
    */
    bool endsWith(QByteArrayView s) const { return s.n <= n && view().substr(static_cast<std::size_t>(n - s.n)) == s.view(); }
    /*!
        \overload

        Returns \c true if the view starts with the byte \a c; otherwise
        returns \c false.
    */
    bool startsWith(char c) const { return n > 0 && p[0] == c; }
    /*!
        \overload

        Returns \c true if the view ends with the byte \a c; otherwise returns
        \c false.
    */
    bool endsWith(char c) const { return n > 0 && p[n - 1] == c; }
    /*!
        Returns the index position of the first occurrence of \a s in the
        view, searching forward from index position \a from, or -1 if it is
        not found.

        \sa lastIndexOf(), contains()
    */
    qsizetype indexOf(QByteArrayView s, qsizetype from = 0) const {
        const auto r = view().find(s.view(), static_cast<std::size_t>(qMax<qsizetype>(from, 0)));
        return r == std::string_view::npos ? -1 : static_cast<qsizetype>(r);
    }
    /*!
        \overload

        Returns the index position of the first occurrence of the byte \a c in
        the view, searching forward from index position \a from, or -1 if it
        is not found.
    */
    qsizetype indexOf(char c, qsizetype from = 0) const { return indexOf(QByteArrayView(&c, 1), from); }
    /*!
        Returns the index position of the last occurrence of \a s in the view,
        searching backward from index position \a from, or -1 if it is not
        found. A negative \a from searches from the end of the view; the match
        must begin at or before \a from.

        \sa indexOf(), contains()
    */
    qsizetype lastIndexOf(QByteArrayView s, qsizetype from = -1) const {
        const auto r = view().rfind(s.view(), from < 0 ? std::string_view::npos : static_cast<std::size_t>(from));
        return r == std::string_view::npos ? -1 : static_cast<qsizetype>(r);
    }
    /*!
        \overload

        Returns the index position of the last occurrence of the byte \a c in
        the view, searching backward from index position \a from, or -1 if it
        is not found.
    */
    qsizetype lastIndexOf(char c, qsizetype from = -1) const { return lastIndexOf(QByteArrayView(&c, 1), from); }
    /*!
        Returns \c true if the view contains \a s; otherwise returns \c false.

        \sa indexOf()
    */
    bool contains(QByteArrayView s) const { return indexOf(s) != -1; }
    /*!
        \overload

        Returns \c true if the view contains the byte \a c; otherwise returns
        \c false.
    */
    bool contains(char c) const { return indexOf(c) != -1; }
    /*!
        Compares the view with \a s byte by byte and returns a negative
        integer, zero or a positive integer if the view is less than, equal to
        or greater than \a s.
    */
    int compare(QByteArrayView s) const { return view().compare(s.view()); }
    /*!
        Returns a const iterator pointing to the first byte of the view. This
        is the same as begin().

        \sa cend()
    */
    const_iterator cbegin() const noexcept { return p; }
    /*!
        Returns a const iterator pointing just after the last byte of the
        view. This is the same as end().

        \sa cbegin()
    */
    const_iterator cend() const noexcept { return p + n; }

    /*!
        A read-only reverse iterator over the bytes of the view.
    */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    /*!
        The reverse iterator type, for STL compatibility. It is the same as const_reverse_iterator.
    */
    using reverse_iterator = const_reverse_iterator;
    /*!
        The type of sizes and indexes, for STL compatibility.
    */
    using size_type = qsizetype;
    /*!
        The type of the distance between two iterators, for STL compatibility.
    */
    using difference_type = qptrdiff;

    /*!
        Returns a view of the whole array \a data, including a terminating
        null if there is one: \c{fromArray("ab")} has three bytes, where
        \c{QByteArrayView("ab")} has two.
    */
    template <std::size_t N> static constexpr QByteArrayView fromArray(const char (&data)[N]) noexcept {
        return QByteArrayView(data, static_cast<qsizetype>(N));
    }

    /*!
        Returns a reverse iterator pointing to the last byte of the view.

        \sa rend()
    */
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    /*!
        Returns a reverse iterator pointing just before the first byte of the view.

        \sa rbegin()
    */
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    /*!
        Returns a reverse iterator pointing to the last byte of the view. This is the same as rbegin().
    */
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    /*!
        Returns a reverse iterator pointing just before the first byte of the view. This is the same as rend().
    */
    const_reverse_iterator crend() const noexcept { return rend(); }

    /*!
        \since 6.8
        Returns the largest number of bytes a view can refer to.
    */
    static constexpr qsizetype max_size() noexcept { return static_cast<qsizetype>(~std::size_t(0) >> 1); }
    /*!
        \since 6.8
        Returns the largest number of bytes a view can refer to. This is the
        same as max_size().
    */
    static constexpr qsizetype maxSize() noexcept { return max_size(); }

    /*!
        Returns the number of occurrences of \a s in the view, which may
        overlap. An empty \a s is found size() + 1 times.

        \sa indexOf(), contains()
    */
    qsizetype count(QByteArrayView s) const noexcept {
        if (s.n == 0)
            return n + 1;
        qsizetype found = 0;
        for (auto at = view().find(s.view()); at != std::string_view::npos; at = view().find(s.view(), at + 1))
            ++found;
        return found;
    }
    /*!
        \overload

        Returns the number of bytes in the view that equal \a c.
    */
    qsizetype count(char c) const noexcept {
        qsizetype found = 0;
        for (qsizetype i = 0; i < n; ++i)
            found += p[i] == c ? 1 : 0;
        return found;
    }

    /*!
        Returns the view without the whitespace at its start and its end.
        Whitespace means the ASCII characters space, \c{'\t'}, \c{'\n'},
        \c{'\v'}, \c{'\f'} and \c{'\r'}, as for QByteArray::trimmed().
    */
    constexpr QByteArrayView trimmed() const noexcept {
        qsizetype b = 0;
        qsizetype e = n;
        while (b < e && isSpace(p[b]))
            ++b;
        while (e > b && isSpace(p[e - 1]))
            --e;
        return QByteArrayView(p + b, e - b);
    }

    /*!
        Shortens the view to its first \a len bytes. \a len must not exceed
        size(); no bounds checking is done.

        \sa chop(), first()
    */
    constexpr void truncate(qsizetype len) { n = len; }
    /*!
        Removes \a len bytes from the end of the view. \a len must not exceed
        size(); no bounds checking is done.

        \sa truncate(), chopped()
    */
    constexpr void chop(qsizetype len) { n -= len; }
    /*!
        \since 6.8
        Changes the view to start at index position \a pos and to extend to
        its end, and returns a reference to it. \a pos must not exceed size().

        \sa sliced()
    */
    constexpr QByteArrayView &slice(qsizetype pos) {
        *this = sliced(pos);
        return *this;
    }
    /*!
        \since 6.8
        \overload

        Changes the view to the \a len bytes that start at index position
        \a pos, and returns a reference to it. The range must lie within the
        view.
    */
    constexpr QByteArrayView &slice(qsizetype pos, qsizetype len) {
        *this = sliced(pos, len);
        return *this;
    }

    /*!
        \since 6.3
        Returns \c true if the view holds well-formed UTF-8; otherwise returns
        \c false. An empty view is valid.
    */
    bool isValidUtf8() const noexcept { return QtPrivate::isValidUtf8(p, n); }

    /*!
        \overload

        Compares the view with \a s and returns a negative integer, zero or a
        positive integer if the view is less than, equal to or greater than
        \a s. With Qt::CaseInsensitive as \a cs, the ASCII letters compare
        equal whatever their case.
    */
    int compare(QByteArrayView s, Qt::CaseSensitivity cs) const noexcept {
        if (cs == Qt::CaseSensitive)
            return compare(s);
        const qsizetype common = n < s.n ? n : s.n;
        for (qsizetype i = 0; i < common; ++i) {
            const int a = foldAscii(p[i]);
            const int b = foldAscii(s.p[i]);
            if (a != b)
                return a - b;
        }
        return n == s.n ? 0 : (n < s.n ? -1 : 1);
    }

    /*!
        Returns the view converted to a \c short in base \a base, which is
        between 2 and 36, or 0 to detect the base from a \c{0x}, \c{0b} or
        \c{0} prefix. Returns 0 if the conversion fails or the value does not
        fit. If \a ok is not \c nullptr, \c{*ok} is set to whether the
        conversion succeeded.

        \sa QByteArray::toShort()
    */
    short toShort(bool *ok = nullptr, int base = 10) const { return toByteArray().toShort(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned short} in base \a base,
        as toShort() does, and sets \c{*ok} if \a ok is not \c nullptr.
    */
    ushort toUShort(bool *ok = nullptr, int base = 10) const { return toByteArray().toUShort(ok, base); }
    /*!
        Returns the view converted to an \c int in base \a base, as toShort()
        does, and sets \c{*ok} if \a ok is not \c nullptr.

        \sa QByteArray::toInt()
    */
    int toInt(bool *ok = nullptr, int base = 10) const { return toByteArray().toInt(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned int} in base \a base, as
        toShort() does, and sets \c{*ok} if \a ok is not \c nullptr.
    */
    uint toUInt(bool *ok = nullptr, int base = 10) const { return toByteArray().toUInt(ok, base); }
    /*!
        Returns the view converted to a \c long in base \a base, as toShort()
        does, and sets \c{*ok} if \a ok is not \c nullptr.
    */
    long toLong(bool *ok = nullptr, int base = 10) const { return toByteArray().toLong(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned long} in base \a base, as
        toShort() does, and sets \c{*ok} if \a ok is not \c nullptr.
    */
    ulong toULong(bool *ok = nullptr, int base = 10) const { return toByteArray().toULong(ok, base); }
    /*!
        Returns the view converted to a \c{long long} in base \a base, as
        toShort() does, and sets \c{*ok} if \a ok is not \c nullptr.
    */
    qlonglong toLongLong(bool *ok = nullptr, int base = 10) const { return toByteArray().toLongLong(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned long long} in base
        \a base, as toShort() does, and sets \c{*ok} if \a ok is not
        \c nullptr.
    */
    qulonglong toULongLong(bool *ok = nullptr, int base = 10) const { return toByteArray().toULongLong(ok, base); }
    /*!
        Returns the view converted to a \c float, or 0 if it is not a number.
        If \a ok is not \c nullptr, \c{*ok} is set to whether the conversion
        succeeded.

        \sa toDouble()
    */
    float toFloat(bool *ok = nullptr) const { return toByteArray().toFloat(ok); }
    /*!
        Returns the view converted to a \c double, or 0 if it is not a number.
        The text is read in the C locale. If \a ok is not \c nullptr,
        \c{*ok} is set to whether the conversion succeeded.

        \sa QByteArray::toDouble()
    */
    double toDouble(bool *ok = nullptr) const { return toByteArray().toDouble(ok); }

    friend bool operator==(QByteArrayView a, QByteArrayView b) noexcept { return a.view() == b.view(); }
    friend bool operator!=(QByteArrayView a, QByteArrayView b) noexcept { return a.view() != b.view(); }
    friend bool operator<(QByteArrayView a, QByteArrayView b) noexcept { return a.view() < b.view(); }
    friend bool operator>(QByteArrayView a, QByteArrayView b) noexcept { return a.view() > b.view(); }
    friend bool operator<=(QByteArrayView a, QByteArrayView b) noexcept { return a.view() <= b.view(); }
    friend bool operator>=(QByteArrayView a, QByteArrayView b) noexcept { return a.view() >= b.view(); }

private:
    static constexpr bool isSpace(char c) noexcept { return c == ' ' || (c >= '\t' && c <= '\r'); }
    static constexpr int foldAscii(char c) noexcept {
        const auto u = static_cast<unsigned char>(c);
        return u >= 'A' && u <= 'Z' ? u + ('a' - 'A') : u;
    }
    std::string_view view() const noexcept { return std::string_view(p ? p : "", static_cast<std::size_t>(n)); }

    const char *p = nullptr;
    qsizetype n = 0;
};
