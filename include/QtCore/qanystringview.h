// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>
#include <QtCore/qutf8stringview.h>

#include <string>
#include <string_view>

/*!
    \class QAnyStringView
    \since 6.0
    \brief The QAnyStringView class provides a unified view on Latin-1, UTF-8 or UTF-16 strings.

    A QAnyStringView refers to text in one of three encodings without owning
    or converting it: a QLatin1StringView, a QUtf8StringView (including plain
    C strings and QByteArray) or a QStringView (including QString). It suits
    function parameters that only read text and accept all of them:

    \code
    void setName(QAnyStringView name) { m_name = name.toString(); }

    setName("ascii");
    setName(u"utf-16");
    setName(QLatin1StringView("latin-1"));
    \endcode

    Use visit() to work on the underlying view directly. Sizes count code
    units of the viewed encoding.

    \differences
    The text classes of Nt take QString arguments, so QAnyStringView is mainly
    for user code. Comparisons convert both sides to QString, so they
    allocate. There is no constructor taking a callable or a \c char8_t
    string.
    \enddifferences

    \sa QStringView, QUtf8StringView, QLatin1StringView
*/
class QAnyStringView {
public:
    /*! \typedef QAnyStringView::size_type

        Alias for qsizetype. */
    using size_type = qsizetype;

    /*!
        Constructs a null view.
    */
    constexpr QAnyStringView() noexcept = default;
    /*!
        Constructs a null view.
    */
    constexpr QAnyStringView(std::nullptr_t) noexcept {}
    /*!
        Constructs a view of the UTF-16 view \a str.
    */
    QAnyStringView(QStringView str) noexcept : p(str.data()), n(str.size()), tag(Utf16) {}
    /*!
        Constructs a view of the characters of \a str.
    */
    QAnyStringView(const QString &str) noexcept : QAnyStringView(QStringView(str)) {}
    /*!
        Constructs a view of the Latin-1 view \a str.
    */
    constexpr QAnyStringView(QLatin1StringView str) noexcept : p(str.data()), n(str.size()), tag(Latin1) {}
    /*!
        Constructs a view of the UTF-8 view \a str.
    */
    constexpr QAnyStringView(QUtf8StringView str) noexcept : p(str.data()), n(str.size()), tag(Utf8) {}
    /*!
        Constructs a view of the null-terminated UTF-8 string \a str.
    */
    constexpr QAnyStringView(const char *str) noexcept : QAnyStringView(QUtf8StringView(str)) {}
    /*!
        Constructs a view of the \a len UTF-8 bytes at \a str.
    */
    constexpr QAnyStringView(const char *str, qsizetype len) noexcept : p(str), n(len), tag(Utf8) {}
    /*!
        Constructs a view of the UTF-8 bytes of \a str.
    */
    QAnyStringView(const QByteArray &str) noexcept : QAnyStringView(QUtf8StringView(str)) {}
    /*!
        Constructs a view of the UTF-8 bytes of \a str.
    */
    QAnyStringView(const std::string &str) noexcept : QAnyStringView(QUtf8StringView(str)) {}
    /*!
        Constructs a view of the null-terminated UTF-16 string \a str.
    */
    QAnyStringView(const char16_t *str) noexcept : QAnyStringView(QStringView(str)) {}
    /*!
        Constructs a view of the \a len UTF-16 code units at \a str.
    */
    QAnyStringView(const char16_t *str, qsizetype len) noexcept : QAnyStringView(QStringView(str, len)) {}
    /*!
        Constructs a view of the UTF-16 code units of \a str.
    */
    QAnyStringView(std::u16string_view str) noexcept : QAnyStringView(QStringView(str)) {}
    /*!
        Constructs a view of the single character \a c. The view refers to
        \a c, which must outlive it.
    */
    QAnyStringView(const QChar &c) noexcept : p(&c), n(1), tag(Utf16) {}

    /*!
        Calls \a v with the underlying view, a QLatin1StringView, a
        QUtf8StringView or a QStringView, and returns its result.
    */
    template <typename Visitor>
    decltype(auto) visit(Visitor &&v) const {
        if (tag == Latin1)
            return std::forward<Visitor>(v)(QLatin1StringView(static_cast<const char *>(p), n));
        if (tag == Utf16)
            return std::forward<Visitor>(v)(QStringView(static_cast<const QChar *>(p), n));
        return std::forward<Visitor>(v)(QUtf8StringView(static_cast<const char *>(p), n));
    }

    /*!
        Returns the viewed text converted to a QString.
    */
    QString toString() const {
        if (!p)
            return QString();
        if (tag == Latin1)
            return QString::fromLatin1(static_cast<const char *>(p), n);
        if (tag == Utf16)
            return QString(static_cast<const QChar *>(p), n);
        return QString::fromUtf8(static_cast<const char *>(p), n);
    }
    /*!
        Returns a pointer to the first code unit of the view.
    */
    constexpr const void *data() const noexcept { return p; }
    /*!
        Returns the number of code units in the view.
    */
    constexpr qsizetype size() const noexcept { return n; }
    /*!
        Same as size().
    */
    constexpr qsizetype length() const noexcept { return n; }
    /*!
        Returns the size of the view in bytes.
    */
    constexpr qsizetype size_bytes() const noexcept { return tag == Utf16 ? n * 2 : n; }
    /*!
        Returns \c true if the view is null; otherwise returns \c false.
    */
    constexpr bool isNull() const noexcept { return p == nullptr; }
    /*!
        Returns \c true if the view is empty; otherwise returns \c false.
    */
    constexpr bool isEmpty() const noexcept { return n == 0; }
    /*!
        Same as isEmpty(). Provided for STL compatibility.
    */
    constexpr bool empty() const noexcept { return n == 0; }
    /*!
        Returns the first character. For a UTF-8 view of a non-ASCII text
        this is the first character decoded. The view must not be empty.
    */
    QChar front() const { return tag == Utf16 ? static_cast<const QChar *>(p)[0] : toString().front(); }
    /*!
        Returns the last character. The view must not be empty.
    */
    QChar back() const { return tag == Utf16 ? static_cast<const QChar *>(p)[n - 1] : toString().back(); }

    /*!
        \since 6.5
        Returns a view of the first \a count code units.
    */
    QAnyStringView first(qsizetype count) const { return slice(0, count); }
    /*!
        \since 6.5
        Returns a view of the last \a count code units.
    */
    QAnyStringView last(qsizetype count) const { return slice(n - count, count); }
    /*!
        \since 6.5
        Returns a view of the code units from \a pos to the end.
    */
    QAnyStringView sliced(qsizetype pos) const { return slice(pos, n - pos); }
    /*!
        \overload
        \since 6.5
        Returns a view of \a count code units from \a pos.
    */
    QAnyStringView sliced(qsizetype pos, qsizetype count) const { return slice(pos, count); }
    /*!
        \since 6.5
        Returns a view without the last \a count code units.
    */
    QAnyStringView chopped(qsizetype count) const { return slice(0, n - count); }
    /*!
        Returns a view of the first \a count code units, or the whole view if
        \a count is negative or larger than size().

        \sa first(), right(), mid()
    */
    QAnyStringView left(qsizetype count) const { return slice(0, count < 0 || count > n ? n : count); }
    /*!
        Returns a view of the last \a count code units, or the whole view if
        \a count is negative or larger than size().

        \sa last(), left(), mid()
    */
    QAnyStringView right(qsizetype count) const {
        const qsizetype c = count < 0 || count > n ? n : count;
        return slice(n - c, c);
    }
    /*!
        Returns a view of \a count code units from index \a pos, or of the
        code units from \a pos to the end if \a count is negative or reaches
        past the end. Returns an empty view if \a pos is out of range.

        \sa sliced(), left(), right()
    */
    QAnyStringView mid(qsizetype pos, qsizetype count = -1) const {
        if (pos < 0 || pos > n)
            return slice(0, 0);
        if (count < 0 || count > n - pos)
            count = n - pos;
        return slice(pos, count);
    }
    /*!
        \since 6.8
        Returns the largest number of code units a view can refer to.
    */
    static constexpr qsizetype max_size() noexcept { return static_cast<qsizetype>(~std::size_t(0) >> 3); }
    /*!
        \since 6.5
        Shortens the view to \a count code units.
    */
    void truncate(qsizetype count) { n = count; }
    /*!
        \since 6.5
        Removes \a count code units from the end of the view.
    */
    void chop(qsizetype count) { n -= count; }

    /*!
        Compares \a lhs with \a rhs, using the case sensitivity \a cs, and
        returns a negative number, zero or a positive number if \a lhs is less
        than, equal to or greater than \a rhs.

        \sa equal()
    */
    static int compare(QAnyStringView lhs, QAnyStringView rhs, Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        return lhs.toString().compare(rhs.toString(), cs);
    }
    /*!
        Returns \c true if \a lhs and \a rhs hold the same text, whatever their
        encodings; otherwise returns \c false.

        \sa compare()
    */
    static bool equal(QAnyStringView lhs, QAnyStringView rhs) {
        if (lhs.tag == rhs.tag && lhs.n == rhs.n && lhs.tag != Utf16)
            return lhs.n == 0 || std::char_traits<char>::compare(static_cast<const char *>(lhs.p), static_cast<const char *>(rhs.p), static_cast<std::size_t>(lhs.n)) == 0;
        return lhs.toString() == rhs.toString();
    }

    friend bool operator==(QAnyStringView a, QAnyStringView b) { return equal(a, b); }
    friend bool operator!=(QAnyStringView a, QAnyStringView b) { return !equal(a, b); }
    friend bool operator<(QAnyStringView a, QAnyStringView b) { return compare(a, b) < 0; }
    friend bool operator<=(QAnyStringView a, QAnyStringView b) { return compare(a, b) <= 0; }
    friend bool operator>(QAnyStringView a, QAnyStringView b) { return compare(a, b) > 0; }
    friend bool operator>=(QAnyStringView a, QAnyStringView b) { return compare(a, b) >= 0; }

private:
    enum Tag : unsigned char { Utf8, Latin1, Utf16 };
    QAnyStringView slice(qsizetype pos, qsizetype count) const {
        QAnyStringView r = *this;
        r.p = tag == Utf16 ? static_cast<const void *>(static_cast<const QChar *>(p) + pos) : static_cast<const void *>(static_cast<const char *>(p) + pos);
        r.n = count;
        return r;
    }

    const void *p = nullptr;
    qsizetype n = 0;
    Tag tag = Utf8;
};
