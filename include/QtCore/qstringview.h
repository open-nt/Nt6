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
    The searching, comparison and conversion functions copy the view into a
    QString first, so they allocate. arg() takes strings and characters only.
    first(), last(), sliced(), chopped(), slice(), truncate() and chop() do
    not check their arguments.
    \enddifferences

    \sa QString, QStringList, QLatin1StringView
*/
template <typename Haystack, typename Needle> class QStringTokenizer;

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
    /*!
        \typedef QStringView::const_reverse_iterator

        A reverse iterator over the characters.
    */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    /*!
        \typedef QStringView::reverse_iterator

        The same as const_reverse_iterator.
    */
    using reverse_iterator = const_reverse_iterator;
    /*!
        Returns a reverse iterator at the last character.
    */
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    /*!
        Returns a reverse iterator before the first character.
    */
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    /*!
        Returns a reverse iterator at the last character. Same as rbegin().
    */
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    /*!
        Returns a reverse iterator before the first character. Same as rend().
    */
    const_reverse_iterator crend() const noexcept { return rend(); }

    /*!
        \overload
        Returns \c true if the view starts with \a s, compared with \a cs.
    */
    bool startsWith(QLatin1StringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return toString().startsWith(QString(s), cs);
    }
    /*!
        \overload
        Returns \c true if the view ends with \a s, compared with \a cs.
    */
    bool endsWith(QLatin1StringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return toString().endsWith(QString(s), cs);
    }
    /*!
        \overload
        Returns the position of the first \a s at or after \a from, or -1.
        \a cs selects case sensitivity.
    */
    qsizetype indexOf(QLatin1StringView s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return toString().indexOf(QString(s), from, cs);
    }
    /*!
        \overload
        Returns the position of the last \a s at or before \a from, or -1.
        \a cs selects case sensitivity.
    */
    qsizetype lastIndexOf(QLatin1StringView s, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return toString().lastIndexOf(QString(s), from, cs);
    }
    /*!
        \overload
        Returns \c true if the view contains \a s, compared with \a cs.
    */
    bool contains(QLatin1StringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return indexOf(s, 0, cs) != -1;
    }
    /*!
        \overload
        Compares the view with \a s, using \a cs.
    */
    int compare(QLatin1StringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return toString().compare(QString(s), cs);
    }
    /*!
        \overload
        Compares the view with \a c, using \a cs.
    */
    int compare(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return compare(QStringView(&c, 1), cs); }

    /*!
        Returns how often \a s occurs in the view, possibly overlapping,
        compared with \a cs.
    */
    qsizetype count(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return toString().count(s.toString(), cs);
    }
    /*!
        \overload
        Returns how often \a c occurs, compared with \a cs.
    */
    qsizetype count(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().count(c, cs); }
    /*!
        \overload
        Returns how often \a s occurs, compared with \a cs.
    */
    qsizetype count(QLatin1StringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return toString().count(QString(s), cs);
    }

    QList<QStringView> split(QStringView sep, Qt::SplitBehavior behavior = Qt::KeepEmptyParts,
                             Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    QList<QStringView> split(QChar sep, Qt::SplitBehavior behavior = Qt::KeepEmptyParts,
                             Qt::CaseSensitivity cs = Qt::CaseSensitive) const;

    /*!
        Returns the view as UTF-8, the 8-bit encoding of Nt.
    */
    QByteArray toLocal8Bit() const { return toUtf8(); }
    /*!
        Returns the code points of the view; unpaired surrogates become U+FFFD.
    */
    QList<uint> toUcs4() const { return toString().toUcs4(); }
    /*!
        Returns \c true if the view is valid UTF-16.
    */
    bool isValidUtf16() const noexcept { return toString().isValidUtf16(); }
    /*!
        \since 6.7
        Returns \c true if the view equals its uppercase form.
    */
    bool isUpper() const { return toString().isUpper(); }
    /*!
        \since 6.7
        Returns \c true if the view equals its lowercase form.
    */
    bool isLower() const { return toString().isLower(); }
    bool isRightToLeft() const noexcept;
    /*!
        Returns the view converted to a \c short in \a base, or 0 on failure;
        \c{*ok} reports success if \a ok is not \c nullptr.
    */
    short toShort(bool *ok = nullptr, int base = 10) const { return toString().toShort(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned short} in \a base, or 0;
        \c{*ok} reports success if \a ok is not \c nullptr.
    */
    ushort toUShort(bool *ok = nullptr, int base = 10) const { return toString().toUShort(ok, base); }
    /*!
        Returns the view converted to a \c long in \a base, or 0; \c{*ok}
        reports success if \a ok is not \c nullptr.
    */
    long toLong(bool *ok = nullptr, int base = 10) const { return toString().toLong(ok, base); }
    /*!
        Returns the view converted to an \c{unsigned long} in \a base, or 0;
        \c{*ok} reports success if \a ok is not \c nullptr.
    */
    ulong toULong(bool *ok = nullptr, int base = 10) const { return toString().toULong(ok, base); }
    /*!
        Returns the view converted to a \c float, or 0; \c{*ok} reports
        success if \a ok is not \c nullptr.
    */
    float toFloat(bool *ok = nullptr) const { return toString().toFloat(ok); }

    /*!
        Returns a view of the whole array \a string, including a terminating
        null if there is one: \c{fromArray(u"ab")} has three characters, where
        \c{QStringView(u"ab")} has two.
    */
    template <std::size_t N> static QStringView fromArray(const char16_t (&string)[N]) noexcept {
        return QStringView(string, static_cast<qsizetype>(N));
    }

    /*!
        \since 6.1
        Returns an iterator pointing to the first character of the view. This is the same as begin().
    */
    const_iterator constBegin() const noexcept { return p; }
    /*!
        \since 6.1
        Returns an iterator pointing just after the last character of the view. This is the same as end().
    */
    const_iterator constEnd() const noexcept { return p + n; }

    /*!
        \since 6.8
        Returns the largest number of characters a view can refer to.
    */
    static constexpr qsizetype max_size() noexcept {
        return static_cast<qsizetype>((~std::size_t(0) >> 1) / sizeof(char16_t));
    }
    /*!
        \since 6.8
        Returns the largest number of characters a view can refer to. This is
        the same as max_size().
    */
    static constexpr qsizetype maxSize() noexcept { return max_size(); }

    /*!
        \since 6.8
        Changes the view to start at index position \a pos and to extend to
        its end, and returns a reference to it. \a pos must not exceed size().

        \sa sliced()
    */
    QStringView &slice(qsizetype pos) {
        *this = sliced(pos);
        return *this;
    }
    /*!
        \since 6.8
        \overload

        Changes the view to the \a c characters that start at index position
        \a pos, and returns a reference to it. The range must lie within the
        view.
    */
    QStringView &slice(qsizetype pos, qsizetype c) {
        *this = sliced(pos, c);
        return *this;
    }

    /*!
        Copies the view to \a array as a wide string and returns the number of
        \c wchar_t written. Where \c wchar_t has 32 bits, a surrogate pair
        becomes one element. \a array must have room for size() elements; no
        terminating null is written.

        \sa QString::toWCharArray()
    */
    qsizetype toWCharArray(wchar_t *array) const { return toString().toWCharArray(array); }

    /*!
        \since 6.4
        Compares the view with \a other as text in the current locale and
        returns a negative number, zero or a positive number if the view sorts
        before, with or after \a other.

        \sa QString::localeAwareCompare(), QCollator
    */
    int localeAwareCompare(QStringView other) const {
        return QString::localeAwareCompare(toString(), other.toString());
    }

    /*!
        Returns a string in which the place markers of this view (\c{%1} to
        \c{%99}) are replaced by \a a and \a rest: the lowest-numbered marker
        by \a a, the next one by the first of \a rest, and so on. The
        arguments are strings, views or characters. The markers are replaced
        in one pass, so a marker inside an argument is left alone.

        \code
        QStringView pattern = u"%1 of %2";
        const QString text = pattern.arg(u"3", QLatin1StringView("7")); // "3 of 7"
        \endcode

        \sa QString::arg()
    */
    template <typename A, typename... Rest> QString arg(const A &a, const Rest &...rest) const {
        const QString texts[] = {QtPrivate::argText(a), QtPrivate::argText(rest)...};
        return QtPrivate::multiArg(toString(), texts, static_cast<qsizetype>(1 + sizeof...(Rest)));
    }

    /*!
        \since 6.0
        Returns a tokenizer that splits this view at each occurrence of
        \a needle, without copying the text. \a flags may hold a
        Qt::SplitBehavior and a Qt::CaseSensitivity, in either order. Include
        \c{<QStringTokenizer>} to use this function.

        \code
        for (QStringView field : QStringView(u"a,b,,c").tokenize(u',', Qt::SkipEmptyParts))
            use(field); // "a", "b", "c"
        \endcode

        \sa QStringTokenizer, split()
    */
    template <typename Needle, typename... Flags> auto tokenize(Needle &&needle, Flags... flags) const;

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
