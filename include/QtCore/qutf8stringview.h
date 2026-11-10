// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <string>
#include <string_view>

/*!
    \class QUtf8StringView
    \since 6.0
    \brief The QUtf8StringView class provides a unified view on UTF-8 strings with a read-only subset of the QString API.

    A QUtf8StringView refers to a range of UTF-8 bytes without owning or
    copying them, so the viewed data must outlive the view. Sizes and indexes
    count bytes (code units), not characters.

    \code
    QUtf8StringView name = "Grüße";
    qsizetype bytes = name.size();   // 7
    QString text = name.toString(); // 5 characters
    \endcode

    \differences
    QUtf8StringView is a class, not an alias of the \c QBasicUtf8StringView
    template, and there is no \c char8_t support because Nt is built as C++17.
    Comparisons compare the bytes, which orders the strings by code point.
    \enddifferences

    \sa QStringView, QLatin1StringView, QAnyStringView
*/
class QUtf8StringView {
public:
    /*! \typedef QUtf8StringView::storage_type

        The type of a code unit, \c char. */
    using storage_type = char;
    /*! \typedef QUtf8StringView::value_type

        Alias for \c{const char}. */
    using value_type = const char;
    /*! \typedef QUtf8StringView::const_iterator

        A random access iterator over the bytes of the view. */
    using const_iterator = const char *;
    /*! \typedef QUtf8StringView::iterator

        Same as const_iterator; the view is read-only. */
    using iterator = const_iterator;
    /*! \typedef QUtf8StringView::size_type

        Alias for qsizetype. */
    using size_type = qsizetype;

    /*!
        Constructs a null view.

        \sa isNull()
    */
    constexpr QUtf8StringView() noexcept = default;
    /*!
        Constructs a null view.
    */
    constexpr QUtf8StringView(std::nullptr_t) noexcept {}
    /*!
        Constructs a view of the \a len bytes at \a str.
    */
    constexpr QUtf8StringView(const char *str, qsizetype len) noexcept : p(str), n(len) {}
    /*!
        Constructs a view of the null-terminated string \a str. A null
        \a str gives a null view.
    */
    constexpr QUtf8StringView(const char *str) noexcept : p(str), n(str ? static_cast<qsizetype>(std::char_traits<char>::length(str)) : 0) {}
    /*!
        Constructs a view of the bytes of \a ba. A null byte array gives a
        null view.
    */
    QUtf8StringView(const QByteArray &ba) noexcept : p(ba.isNull() ? nullptr : ba.constData()), n(ba.size()) {}
    /*!
        Constructs a view of the bytes of \a s.
    */
    constexpr QUtf8StringView(std::string_view s) noexcept : p(s.data()), n(static_cast<qsizetype>(s.size())) {}
    /*!
        Constructs a view of the bytes of \a s.
    */
    QUtf8StringView(const std::string &s) noexcept : p(s.data()), n(static_cast<qsizetype>(s.size())) {}

    /*!
        Returns the viewed text converted to a QString.
    */
    QString toString() const { return p ? QString::fromUtf8(p, n) : QString(); }
    /*!
        Returns a pointer to the first byte of the view. The data is not
        necessarily null-terminated.
    */
    constexpr const char *data() const noexcept { return p; }
    /*!
        Same as data().
    */
    constexpr const char *utf8() const noexcept { return p; }
    /*!
        Returns the number of bytes in the view.
    */
    constexpr qsizetype size() const noexcept { return n; }
    /*!
        Same as size().
    */
    constexpr qsizetype length() const noexcept { return n; }
    /*!
        Returns the size of the view in bytes, which is size().
    */
    constexpr qsizetype size_bytes() const noexcept { return n; }
    /*!
        Returns \c true if the view is null; otherwise returns \c false.
    */
    constexpr bool isNull() const noexcept { return p == nullptr; }
    /*!
        Returns \c true if the view has no bytes; otherwise returns \c false.
    */
    constexpr bool isEmpty() const noexcept { return n == 0; }
    /*!
        Same as isEmpty(). Provided for STL compatibility.
    */
    constexpr bool empty() const noexcept { return n == 0; }
    /*!
        Returns the byte at index \a i, which must be valid.
    */
    constexpr char at(qsizetype i) const { return p[i]; }
    /*!
        Same as at(\a i).
    */
    constexpr char operator[](qsizetype i) const { return p[i]; }
    /*!
        Returns the first byte. The view must not be empty.
    */
    constexpr char front() const { return p[0]; }
    /*!
        Returns the last byte. The view must not be empty.
    */
    constexpr char back() const { return p[n - 1]; }

    /*!
        Returns a view of the first \a count bytes.
    */
    constexpr QUtf8StringView first(qsizetype count) const { return QUtf8StringView(p, count); }
    /*!
        Returns a view of the last \a count bytes.
    */
    constexpr QUtf8StringView last(qsizetype count) const { return QUtf8StringView(p + n - count, count); }
    /*!
        Returns a view of the bytes from index \a pos to the end.
    */
    constexpr QUtf8StringView sliced(qsizetype pos) const { return QUtf8StringView(p + pos, n - pos); }
    /*!
        \overload
        Returns a view of \a count bytes from index \a pos.
    */
    constexpr QUtf8StringView sliced(qsizetype pos, qsizetype count) const { return QUtf8StringView(p + pos, count); }
    /*!
        Returns a view without the last \a count bytes.
    */
    constexpr QUtf8StringView chopped(qsizetype count) const { return QUtf8StringView(p, n - count); }
    /*!
        Shortens the view to \a count bytes.
    */
    constexpr void truncate(qsizetype count) { n = count; }
    /*!
        Removes \a count bytes from the end of the view.
    */
    constexpr void chop(qsizetype count) { n -= count; }

    /*!
        \since 6.3
        Returns \c true if the view holds well-formed UTF-8; otherwise returns
        \c false.
    */
    bool isValidUtf8() const noexcept { return QtPrivate::isValidUtf8(p, n); }
    /*!
        \since 6.5
        Compares this view with \a other and returns a negative number, zero
        or a positive number if it is less than, equal to or greater than
        \a other. With Qt::CaseInsensitive (\a cs), the texts are compared
        after case folding.
    */
    int compare(QUtf8StringView other, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        if (cs == Qt::CaseInsensitive)
            return toString().compare(other.toString(), cs);
        const int r = std::string_view(p ? p : "", static_cast<std::size_t>(n)).compare(std::string_view(other.p ? other.p : "", static_cast<std::size_t>(other.n)));
        return r < 0 ? -1 : (r > 0 ? 1 : 0);
    }

    /*! Returns an iterator to the first byte. */
    constexpr const_iterator begin() const noexcept { return p; }
    /*! Returns an iterator just past the last byte. */
    constexpr const_iterator end() const noexcept { return p + n; }
    /*! Same as begin(). */
    constexpr const_iterator cbegin() const noexcept { return p; }
    /*! Same as end(). */
    constexpr const_iterator cend() const noexcept { return p + n; }

    friend bool operator==(QUtf8StringView a, QUtf8StringView b) noexcept { return a.compare(b) == 0; }
    friend bool operator!=(QUtf8StringView a, QUtf8StringView b) noexcept { return a.compare(b) != 0; }
    friend bool operator<(QUtf8StringView a, QUtf8StringView b) noexcept { return a.compare(b) < 0; }
    friend bool operator<=(QUtf8StringView a, QUtf8StringView b) noexcept { return a.compare(b) <= 0; }
    friend bool operator>(QUtf8StringView a, QUtf8StringView b) noexcept { return a.compare(b) > 0; }
    friend bool operator>=(QUtf8StringView a, QUtf8StringView b) noexcept { return a.compare(b) >= 0; }

private:
    const char *p = nullptr;
    qsizetype n = 0;
};
