// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>
#include <QtCore/qutf8stringview.h>

#include <string>
#include <string_view>
#include <type_traits>

/*!
    \class QAnyStringView
    \brief The QAnyStringView class provides a unified view on UTF-8, UTF-16 or Latin-1 strings.

    A QAnyStringView can refer to text in any of the three encodings Qt uses
    for strings, without copying it, so the viewed data must outlive the view.
    It is meant for function parameters that only read text: the caller passes
    a QString, a QStringView, a \c{u""} literal, a QLatin1StringView, a QByteArray, a
    \c std::string or a \c{""} literal (UTF-8) as it is. Use visit() to get
    the text in its own encoding, or toString() to get a QString.

    \code
    void log(QAnyStringView message);

    log("a UTF-8 literal");
    log(u"a UTF-16 literal");
    log(QLatin1StringView("Latin-1"));
    log(QString("a QString"));
    \endcode

    The size is a number of code units in the encoding of the data, not of
    characters. A view is \e null if it was constructed without data, and
    empty if its size is 0.

    \differences
    The view stores its encoding in a separate member, so it is larger than
    Qt's, which packs it into the size. There is no \c char8_t (C++17), no
    \c char32_t overloads, no \c fromArray(), no constructors for arbitrary
    ranges of characters, and no iterators. at() and operator[]() give the
    code unit at an index, and for UTF-8 that is one byte, not a character.
    The comparison operators compare code point by code point and convert to
    UTF-16 when the two views differ in encoding. QString, QByteArray and the
    other classes of Nt do not take QAnyStringView arguments; call toString()
    when a QString is needed. A QAnyStringView built from a QChar refers to
    that QChar, which must outlive the view.
    \enddifferences

    \sa QUtf8StringView, QStringView, QLatin1StringView
*/
class QAnyStringView {
public:
    /*!
        \enum QAnyStringView::Encoding

        This enum names the encodings a QAnyStringView can hold.

        \value Utf8 The data is UTF-8.
        \value Latin1 The data is ISO 8859-1.
        \value Utf16 The data is UTF-16.
    */
    enum class Encoding { Utf8, Latin1, Utf16 };

    /*!
        The element type of the view, for STL compatibility: the type of
        one code unit as an integer.
    */
    using value_type = char32_t;
    /*!
        The size type, for STL compatibility.
    */
    using size_type = qsizetype;

    /*!
        Constructs a null view in UTF-8.

        \sa isNull()
    */
    constexpr QAnyStringView() noexcept = default;
    /*!
        \overload

        Constructs a null view in UTF-8.
    */
    constexpr QAnyStringView(std::nullptr_t) noexcept {}
    /*!
        \overload

        Constructs a view of the null-terminated UTF-8 string \a str. The view
        is null if \a str is \c nullptr.
    */
    constexpr QAnyStringView(const char *str) noexcept
        : m_data(str), m_size(str ? static_cast<qsizetype>(std::char_traits<char>::length(str)) : 0) {}
    /*!
        \overload

        Constructs a view of the first \a len bytes of the UTF-8 string
        \a str.
    */
    constexpr QAnyStringView(const char *str, qsizetype len) noexcept : m_data(str), m_size(len) {}
    /*!
        \overload

        Constructs a view of the null-terminated UTF-16 string \a str. The
        view is null if \a str is \c nullptr.
    */
    QAnyStringView(const char16_t *str) noexcept
        : m_data(str), m_size(str ? static_cast<qsizetype>(std::char_traits<char16_t>::length(str)) : 0), m_encoding(Encoding::Utf16) {}
    /*!
        \overload

        Constructs a view of the first \a len code units of the UTF-16 string
        \a str.
    */
    QAnyStringView(const char16_t *str, qsizetype len) noexcept : m_data(str), m_size(len), m_encoding(Encoding::Utf16) {}
    /*!
        \overload

        Constructs a view of the first \a len characters of \a str.
    */
    QAnyStringView(const QChar *str, qsizetype len) noexcept : m_data(str), m_size(len), m_encoding(Encoding::Utf16) {}
    /*!
        \overload

        Constructs a view of the single character \a c. The view refers to
        \a c, which must outlive it.
    */
    QAnyStringView(const QChar &c) noexcept : m_data(&c), m_size(1), m_encoding(Encoding::Utf16) {}
    /*!
        \overload

        Constructs a view of the characters of \a str, in UTF-16. The view is
        null if \a str is null.
    */
    QAnyStringView(const QString &str) noexcept : m_data(str.isNull() ? nullptr : str.constData()), m_size(str.size()), m_encoding(Encoding::Utf16) {}
    /*!
        \overload

        Constructs a view of the characters of \a str, in UTF-16.
    */
    QAnyStringView(QStringView str) noexcept : m_data(str.data()), m_size(str.size()), m_encoding(Encoding::Utf16) {}
    /*!
        \overload

        Constructs a view of the Latin-1 text \a str.
    */
    QAnyStringView(QLatin1StringView str) noexcept : m_data(str.data()), m_size(str.size()), m_encoding(Encoding::Latin1) {}
    /*!
        \overload

        Constructs a view of the UTF-8 text \a str.
    */
    QAnyStringView(QUtf8StringView str) noexcept : m_data(str.data()), m_size(str.size()) {}
    /*!
        \overload

        Constructs a view of the bytes of \a ba, which are taken as UTF-8.
        The view is null if \a ba is null.
    */
    QAnyStringView(const QByteArray &ba) noexcept : m_data(ba.isNull() ? nullptr : ba.constData()), m_size(ba.size()) {}
    /*!
        \overload

        Constructs a view of the bytes of \a ba, which are taken as UTF-8.
    */
    QAnyStringView(QByteArrayView ba) noexcept : m_data(ba.data()), m_size(ba.size()) {}
    /*!
        \overload

        Constructs a view of the bytes of \a s, which are taken as UTF-8.
    */
    QAnyStringView(const std::string &s) noexcept : m_data(s.data()), m_size(static_cast<qsizetype>(s.size())) {}
    /*!
        \overload

        Constructs a view of the bytes of \a s, which are taken as UTF-8.
    */
    QAnyStringView(std::string_view s) noexcept : m_data(s.data()), m_size(static_cast<qsizetype>(s.size())) {}
    /*!
        \overload

        Constructs a view of the UTF-16 code units of \a s.
    */
    QAnyStringView(const std::u16string &s) noexcept : m_data(s.data()), m_size(static_cast<qsizetype>(s.size())), m_encoding(Encoding::Utf16) {}
    /*!
        \overload

        Constructs a view of the UTF-16 code units of \a s.
    */
    QAnyStringView(std::u16string_view s) noexcept : m_data(s.data()), m_size(static_cast<qsizetype>(s.size())), m_encoding(Encoding::Utf16) {}

    /*!
        Calls \a f with the text of the view as a QUtf8StringView, a
        QLatin1StringView or a QStringView, depending on its encoding, and
        returns what \a f returns. All three calls must return the same type,
        so \a f is usually a generic lambda.

        \code
        qsizetype n = view.visit([](auto v) { return v.size(); });
        \endcode
    */
    template <typename F>
    decltype(auto) visit(F &&f) const {
        switch (m_encoding) {
        case Encoding::Latin1:
            return std::forward<F>(f)(QLatin1StringView(static_cast<const char *>(m_data), m_size));
        case Encoding::Utf16:
            return std::forward<F>(f)(QStringView(static_cast<const QChar *>(m_data), m_size));
        case Encoding::Utf8:
            break;
        }
        return std::forward<F>(f)(QUtf8StringView(static_cast<const char *>(m_data), m_size));
    }

    /*!
        Returns a QString holding the text of the view. The result is null if
        the view is null.
    */
    QString toString() const {
        if (!m_data)
            return QString();
        switch (m_encoding) {
        case Encoding::Latin1:
            return QString(QLatin1StringView(static_cast<const char *>(m_data), m_size));
        case Encoding::Utf16:
            return QString(static_cast<const QChar *>(m_data), m_size);
        case Encoding::Utf8:
            break;
        }
        return QString::fromUtf8(static_cast<const char *>(m_data), m_size);
    }
    /*!
        Returns the encoding of the viewed text.

        \sa isUtf8(), isUtf16(), isLatin1()
    */
    constexpr Encoding encoding() const noexcept { return m_encoding; }
    /*!
        Returns \c true if the viewed text is UTF-8.

        \sa isUtf16(), isLatin1()
    */
    constexpr bool isUtf8() const noexcept { return m_encoding == Encoding::Utf8; }
    /*!
        Returns \c true if the viewed text is UTF-16.

        \sa isUtf8(), isLatin1()
    */
    constexpr bool isUtf16() const noexcept { return m_encoding == Encoding::Utf16; }
    /*!
        Returns \c true if the viewed text is Latin-1.

        \sa isUtf8(), isUtf16()
    */
    constexpr bool isLatin1() const noexcept { return m_encoding == Encoding::Latin1; }
    /*!
        Returns a pointer to the first code unit of the view. It points to
        \c char for UTF-8 and Latin-1 and to \c char16_t for UTF-16. The data
        is not necessarily null-terminated.

        \sa isUtf16(), size()
    */
    constexpr const void *data() const noexcept { return m_data; }
    /*!
        Returns the number of code units in the view.

        \sa sizeInBytes(), isEmpty()
    */
    constexpr qsizetype size() const noexcept { return m_size; }
    /*!
        Returns the number of code units in the view. This is the same as
        size().
    */
    constexpr qsizetype length() const noexcept { return m_size; }
    /*!
        Returns the number of bytes the viewed text occupies.

        \sa size()
    */
    constexpr qsizetype sizeInBytes() const noexcept { return m_encoding == Encoding::Utf16 ? m_size * 2 : m_size; }
    /*!
        Returns \c true if the view has no data; otherwise returns \c false.

        \sa isEmpty()
    */
    constexpr bool isNull() const noexcept { return m_data == nullptr; }
    /*!
        Returns \c true if the view has no code units; otherwise returns
        \c false.

        \sa isNull(), size()
    */
    constexpr bool isEmpty() const noexcept { return m_size == 0; }
    /*!
        Returns \c true if the view has no code units. This is the same as
        isEmpty(), provided for STL compatibility.
    */
    constexpr bool empty() const noexcept { return m_size == 0; }
    /*!
        Returns the code unit at index position \a n as an integer. \a n must
        be a valid index; no bounds checking is done. For UTF-8 this is one
        byte, not a character.
    */
    char32_t at(qsizetype n) const {
        if (m_encoding == Encoding::Utf16)
            return static_cast<const char16_t *>(m_data)[n];
        return static_cast<unsigned char>(static_cast<const char *>(m_data)[n]);
    }
    /*!
        Returns the code unit at index position \a n. This is the same as
        at().
    */
    char32_t operator[](qsizetype n) const { return at(n); }
    /*!
        Returns the first code unit of the view. The view must not be empty.

        \sa back()
    */
    char32_t front() const { return at(0); }
    /*!
        Returns the last code unit of the view. The view must not be empty.

        \sa front()
    */
    char32_t back() const { return at(m_size - 1); }

    /*!
        Returns a view of \a len code units starting at index position
        \a pos. If \a len is negative or extends past the end, the view
        extends to the end. A negative \a pos is clamped to 0 and reduces
        \a len accordingly. If \a pos is past the end, an empty view is
        returned. For UTF-8 the cut may fall inside a multibyte sequence.

        \sa left(), right(), sliced()
    */
    QAnyStringView mid(qsizetype pos, qsizetype len = -1) const {
        if (pos < 0) {
            if (len >= 0)
                len = qMax<qsizetype>(len + pos, 0);
            pos = 0;
        }
        if (pos >= m_size)
            return withRange(m_data ? pos0(m_size) : nullptr, 0);
        return withRange(pos0(pos), len < 0 || len > m_size - pos ? m_size - pos : len);
    }
    /*!
        Returns the first \a n code units of the view, or the whole view if
        \a n is larger than size().

        \sa right(), mid()
    */
    QAnyStringView left(qsizetype n) const { return mid(0, qMax<qsizetype>(n, 0)); }
    /*!
        Returns the last \a n code units of the view, or the whole view if
        \a n is larger than size().

        \sa left(), mid()
    */
    QAnyStringView right(qsizetype n) const { return n >= m_size ? *this : mid(m_size - qMax<qsizetype>(n, 0)); }
    /*!
        Returns a view of the first \a n code units. \a n must not exceed
        size().

        \sa last()
    */
    QAnyStringView first(qsizetype n) const { return withRange(m_data, n); }
    /*!
        Returns a view of the last \a n code units. \a n must not exceed
        size().

        \sa first()
    */
    QAnyStringView last(qsizetype n) const { return withRange(pos0(m_size - n), n); }
    /*!
        Returns a view of the part starting at index position \a pos and
        extending to the end. \a pos must not exceed size().

        \sa mid()
    */
    QAnyStringView sliced(qsizetype pos) const { return withRange(pos0(pos), m_size - pos); }
    /*!
        \overload

        Returns a view of \a n code units starting at index position \a pos.
        The range must lie within the view.
    */
    QAnyStringView sliced(qsizetype pos, qsizetype n) const { return withRange(pos0(pos), n); }
    /*!
        Returns the view with the last \a n code units removed. \a n must not
        exceed size().

        \sa chop()
    */
    QAnyStringView chopped(qsizetype n) const { return withRange(m_data, m_size - n); }
    /*!
        Truncates the view to \a n code units. \a n must not exceed size().

        \sa chop()
    */
    void truncate(qsizetype n) noexcept { m_size = n; }
    /*!
        Removes \a n code units from the end of the view. \a n must not exceed
        size().

        \sa truncate(), chopped()
    */
    void chop(qsizetype n) noexcept { m_size -= n; }

    /*!
        Compares the text of this view with the text of \a other, code point
        by code point, and returns a negative integer, zero or a positive
        integer if this view is less than, equal to or greater than \a other.
        The case sensitivity \a cs selects whether case is ignored.
    */
    int compare(QAnyStringView other, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        if (m_encoding == other.m_encoding && m_encoding != Encoding::Utf8 && cs == Qt::CaseSensitive)
            return compareUnits(other);
        if (m_encoding == Encoding::Utf8 && other.m_encoding == Encoding::Utf8 && cs == Qt::CaseSensitive)
            return compareUnits(other);
        return toString().compare(other.toString(), cs);
    }

    friend bool operator==(QAnyStringView a, QAnyStringView b) { return a.compare(b) == 0; }
    friend bool operator!=(QAnyStringView a, QAnyStringView b) { return a.compare(b) != 0; }
    friend bool operator<(QAnyStringView a, QAnyStringView b) { return a.compare(b) < 0; }
    friend bool operator>(QAnyStringView a, QAnyStringView b) { return a.compare(b) > 0; }
    friend bool operator<=(QAnyStringView a, QAnyStringView b) { return a.compare(b) <= 0; }
    friend bool operator>=(QAnyStringView a, QAnyStringView b) { return a.compare(b) >= 0; }

private:
    const void *pos0(qsizetype n) const noexcept {
        if (m_encoding == Encoding::Utf16)
            return static_cast<const char16_t *>(m_data) + n;
        return static_cast<const char *>(m_data) + n;
    }
    QAnyStringView withRange(const void *p, qsizetype n) const noexcept {
        QAnyStringView r(*this);
        r.m_data = p;
        r.m_size = n;
        return r;
    }
    int compareUnits(const QAnyStringView &o) const {
        const qsizetype n = qMin(m_size, o.m_size);
        for (qsizetype i = 0; i < n; ++i) {
            const char32_t a = at(i);
            const char32_t b = o.at(i);
            if (a != b)
                return a < b ? -1 : 1;
        }
        return m_size == o.m_size ? 0 : (m_size < o.m_size ? -1 : 1);
    }

    const void *m_data = nullptr;
    qsizetype m_size = 0;
    Encoding m_encoding = Encoding::Utf8;
};

/*!
    \internal
*/
inline std::size_t qHash(QAnyStringView v, std::size_t seed = 0) noexcept { return qHash(v.toString(), seed); }
