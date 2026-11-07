// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qstring.h>

#include <string>
#include <string_view>

/*!
    \class QUtf8StringView
    \brief The QUtf8StringView class is a view on a UTF-8 encoded string.

    A QUtf8StringView refers to a contiguous range of UTF-8 code units
    without owning or copying them, so the viewed data must outlive the view.
    It is meant for function parameters that accept text from any 8-bit source
    (a literal, a QByteArray or a \c std::string) and treat it as UTF-8.

    The size is a number of bytes (code units), not of characters.

    \differences
    The storage type is always \c char, because Nt requires only C++17 and has
    no \c char8_t. Only the construction, size and slicing functions, element
    access, toString() and the equality and ordering operators are provided;
    the other search and conversion functions of Qt are not. The comparison
    operators compare the UTF-8 bytes, which orders text as code point order.
    \enddifferences

    \sa QAnyStringView, QStringView, QByteArrayView
*/
class QUtf8StringView {
public:
    /*!
        The type of one code unit as stored: \c char.
    */
    using storage_type = char;
    /*!
        The element type of the view, for STL compatibility.
    */
    using value_type = const char;
    /*!
        A read-only iterator over the code units of the view.
    */
    using const_iterator = const char *;
    /*!
        The iterator type, for STL compatibility. It is the same as const_iterator.
    */
    using iterator = const_iterator;
    /*!
        The size type, for STL compatibility.
    */
    using size_type = qsizetype;

    /*!
        Constructs a null view.

        \sa isNull()
    */
    constexpr QUtf8StringView() noexcept = default;
    /*!
        \overload

        Constructs a null view.
    */
    constexpr QUtf8StringView(std::nullptr_t) noexcept {}
    /*!
        \overload

        Constructs a view of the first \a len bytes of \a str.
    */
    constexpr QUtf8StringView(const char *str, qsizetype len) noexcept : m_data(str), m_size(len) {}
    /*!
        \overload

        Constructs a view of the null-terminated UTF-8 string \a str. The view
        is null if \a str is \c nullptr.
    */
    constexpr QUtf8StringView(const char *str) noexcept : m_data(str), m_size(str ? static_cast<qsizetype>(std::char_traits<char>::length(str)) : 0) {}
    /*!
        \overload

        Constructs a view of the bytes of \a ba. The view is null if \a ba is
        null.
    */
    QUtf8StringView(const QByteArray &ba) noexcept : m_data(ba.isNull() ? nullptr : ba.constData()), m_size(ba.size()) {}
    /*!
        \overload

        Constructs a view of the bytes of \a ba.
    */
    QUtf8StringView(QByteArrayView ba) noexcept : m_data(ba.data()), m_size(ba.size()) {}
    /*!
        \overload

        Constructs a view of the bytes of \a s.
    */
    QUtf8StringView(const std::string &s) noexcept : m_data(s.data()), m_size(static_cast<qsizetype>(s.size())) {}
    /*!
        \overload

        Constructs a view of the bytes of \a s.
    */
    QUtf8StringView(std::string_view s) noexcept : m_data(s.data()), m_size(static_cast<qsizetype>(s.size())) {}

    /*!
        Returns a QString decoded from the viewed UTF-8 text. The result is
        null if the view is null.

        \sa QString::fromUtf8()
    */
    QString toString() const { return m_data ? QString::fromUtf8(m_data, m_size) : QString(); }
    /*!
        Returns a pointer to the first byte of the view. The data is not
        necessarily null-terminated.

        \sa size()
    */
    constexpr const char *data() const noexcept { return m_data; }
    /*!
        Returns a pointer to the first byte of the view. This is the same as
        data().
    */
    constexpr const char *utf8() const noexcept { return m_data; }
    /*!
        Returns the number of bytes in the view.

        \sa isEmpty()
    */
    constexpr qsizetype size() const noexcept { return m_size; }
    /*!
        Returns the number of bytes in the view. This is the same as size().
    */
    constexpr qsizetype length() const noexcept { return m_size; }
    /*!
        Returns \c true if the view has no data; otherwise returns \c false.

        \sa isEmpty()
    */
    constexpr bool isNull() const noexcept { return m_data == nullptr; }
    /*!
        Returns \c true if the view has no bytes; otherwise returns \c false.

        \sa isNull(), size()
    */
    constexpr bool isEmpty() const noexcept { return m_size == 0; }
    /*!
        Returns \c true if the view has no bytes. This is the same as
        isEmpty(), provided for STL compatibility.
    */
    constexpr bool empty() const noexcept { return m_size == 0; }
    /*!
        Returns the byte at index position \a n. \a n must be a valid index;
        no bounds checking is done.
    */
    constexpr char at(qsizetype n) const { return m_data[n]; }
    /*!
        Returns the byte at index position \a n. This is the same as at().
    */
    constexpr char operator[](qsizetype n) const { return m_data[n]; }
    /*!
        Returns the first byte of the view. The view must not be empty.

        \sa back()
    */
    constexpr char front() const { return m_data[0]; }
    /*!
        Returns the last byte of the view. The view must not be empty.

        \sa front()
    */
    constexpr char back() const { return m_data[m_size - 1]; }
    /*!
        Returns an iterator pointing to the first byte of the view.

        \sa end()
    */
    constexpr const_iterator begin() const noexcept { return m_data; }
    /*!
        Returns an iterator pointing just after the last byte of the view.

        \sa begin()
    */
    constexpr const_iterator end() const noexcept { return m_data + m_size; }
    /*!
        Returns a const iterator pointing to the first byte of the view. This
        is the same as begin().
    */
    constexpr const_iterator cbegin() const noexcept { return m_data; }
    /*!
        Returns a const iterator pointing just after the last byte of the
        view. This is the same as end().
    */
    constexpr const_iterator cend() const noexcept { return m_data + m_size; }

    /*!
        Returns a view of \a len bytes starting at index position \a pos. If
        \a len is negative or extends past the end, the view extends to the
        end. A negative \a pos is clamped to 0 and reduces \a len accordingly.
        If \a pos is past the end, an empty view is returned. The cut may fall
        inside a multibyte sequence.

        \sa left(), right(), sliced()
    */
    QUtf8StringView mid(qsizetype pos, qsizetype len = -1) const {
        if (pos < 0) {
            if (len >= 0)
                len = qMax<qsizetype>(len + pos, 0);
            pos = 0;
        }
        if (pos >= m_size)
            return QUtf8StringView(m_data ? m_data + m_size : nullptr, 0);
        return QUtf8StringView(m_data + pos, len < 0 || len > m_size - pos ? m_size - pos : len);
    }
    /*!
        Returns the first \a n bytes of the view, or the whole view if \a n is
        larger than size().

        \sa right(), mid()
    */
    QUtf8StringView left(qsizetype n) const { return mid(0, qMax<qsizetype>(n, 0)); }
    /*!
        Returns the last \a n bytes of the view, or the whole view if \a n is
        larger than size().

        \sa left(), mid()
    */
    QUtf8StringView right(qsizetype n) const { return n >= m_size ? *this : mid(m_size - qMax<qsizetype>(n, 0)); }
    /*!
        Returns a view of the first \a n bytes. \a n must not exceed size().

        \sa last()
    */
    constexpr QUtf8StringView first(qsizetype n) const { return QUtf8StringView(m_data, n); }
    /*!
        Returns a view of the last \a n bytes. \a n must not exceed size().

        \sa first()
    */
    constexpr QUtf8StringView last(qsizetype n) const { return QUtf8StringView(m_data + m_size - n, n); }
    /*!
        Returns a view of the part starting at index position \a pos and
        extending to the end. \a pos must not exceed size().

        \sa mid()
    */
    constexpr QUtf8StringView sliced(qsizetype pos) const { return QUtf8StringView(m_data + pos, m_size - pos); }
    /*!
        \overload

        Returns a view of \a n bytes starting at index position \a pos. The
        range must lie within the view.
    */
    constexpr QUtf8StringView sliced(qsizetype pos, qsizetype n) const { return QUtf8StringView(m_data + pos, n); }
    /*!
        Returns the view with the last \a n bytes removed. \a n must not exceed
        size().

        \sa chop()
    */
    constexpr QUtf8StringView chopped(qsizetype n) const { return QUtf8StringView(m_data, m_size - n); }
    /*!
        Truncates the view to \a n bytes. \a n must not exceed size().

        \sa chop()
    */
    void truncate(qsizetype n) noexcept { m_size = n; }
    /*!
        Removes \a n bytes from the end of the view. \a n must not exceed
        size().

        \sa truncate(), chopped()
    */
    void chop(qsizetype n) noexcept { m_size -= n; }

    friend bool operator==(QUtf8StringView a, QUtf8StringView b) noexcept {
        return a.m_size == b.m_size && (a.m_size == 0 || std::char_traits<char>::compare(a.m_data, b.m_data, static_cast<std::size_t>(a.m_size)) == 0);
    }
    friend bool operator!=(QUtf8StringView a, QUtf8StringView b) noexcept { return !(a == b); }
    friend bool operator<(QUtf8StringView a, QUtf8StringView b) noexcept {
        return std::string_view(a.m_data ? a.m_data : "", static_cast<std::size_t>(a.m_size)) < std::string_view(b.m_data ? b.m_data : "", static_cast<std::size_t>(b.m_size));
    }
    friend bool operator>(QUtf8StringView a, QUtf8StringView b) noexcept { return b < a; }
    friend bool operator<=(QUtf8StringView a, QUtf8StringView b) noexcept { return !(b < a); }
    friend bool operator>=(QUtf8StringView a, QUtf8StringView b) noexcept { return !(a < b); }

private:
    const char *m_data = nullptr;
    qsizetype m_size = 0;
};
