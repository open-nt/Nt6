// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include <functional>
#include <string>

class QByteArray;
class QString;
template <typename T>
class QList;
using QByteArrayList = QList<QByteArray>;

class QByteArray {
public:
    /*!
        \enum QByteArray::Base64Option

        These options change how toBase64() encodes data and how
        fromBase64() decodes it.

        \value Base64Encoding Use the standard Base64 alphabet, with \c{+} and \c{/} as the last two characters.
        \value Base64UrlEncoding Use the URL-safe Base64 alphabet, with \c{-} and \c{_} as the last two characters.
        \value KeepTrailingEquals Keep the \c{=} padding at the end of the encoded data.
        \value OmitTrailingEquals Omit the \c{=} padding at the end of the encoded data.
    */
    enum Base64Option { Base64Encoding = 0, Base64UrlEncoding = 1, KeepTrailingEquals = 0, OmitTrailingEquals = 2 };
    /*!
        \typedef QByteArray::Base64Options

        The Base64Options type is a typedef for QFlags<Base64Option>. It
        stores an OR combination of Base64Option values.
    */
    Q_DECLARE_FLAGS(Base64Options, Base64Option)

    /*!
        Constructs a null byte array.

        \sa isNull()
    */
    QByteArray() noexcept {}
    QByteArray(const char *data, qsizetype size = -1);
    /*!
        Constructs a byte array of \a size bytes, each set to \a ch. A
        negative \a size is treated as 0.
    */
    QByteArray(qsizetype size, char ch) : d(static_cast<std::size_t>(size < 0 ? 0 : size), ch), m_null(false) {}
    /*! Constructs a byte array holding a copy of the string \a s. */
    QByteArray(const std::string &s) : d(s), m_null(false) {}
    /*! \overload

        Constructs a byte array by moving the contents of the string \a s. */
    QByteArray(std::string &&s) : d(std::move(s)), m_null(false) {}

    /*!
        Returns the number of bytes in the array.

        \sa isEmpty(), resize()
    */
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    /*! Same as size(). */
    qsizetype length() const noexcept { return size(); }
    /*!
        \overload

        Same as size().
    */
    qsizetype count() const noexcept { return size(); }
    /*!
        Returns the maximum number of bytes that can be stored in the array
        without forcing a reallocation.

        \sa reserve(), squeeze()
    */
    qsizetype capacity() const noexcept { return static_cast<qsizetype>(d.capacity()); }
    /*!
        Returns \c true if the byte array has size 0; otherwise returns
        \c false.

        \sa size(), isNull()
    */
    bool isEmpty() const noexcept { return d.empty(); }
    /*!
        Returns \c true if this byte array is null; otherwise returns
        \c false.

        An array is null if it was default constructed, constructed from a
        null \c{const char *}, or cleared, and nothing has been added to it
        since.

        \sa isEmpty(), clear()
    */
    bool isNull() const noexcept { return m_null && d.empty(); }
    /*!
        Clears the contents of the byte array and makes it null.

        \sa resize(), isNull()
    */
    void clear() { d.clear(); m_null = true; }
    /*!
        Attempts to allocate memory for at least \a n bytes, so that later
        appends do not reallocate.

        \sa capacity(), squeeze()
    */
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    /*!
        Releases any memory not required to store the array's data.

        \sa reserve(), capacity()
    */
    void squeeze() { d.shrink_to_fit(); }
    /*!
        Sets the size of the byte array to \a n bytes. If \a n is larger than
        the current size, the new bytes are set to \c{'\0'}. The array is no
        longer null afterwards.

        \sa size(), truncate(), chop()
    */
    void resize(qsizetype n) { d.resize(static_cast<std::size_t>(n), '\0'); m_null = false; }
    /*!
        \overload

        Sets the size of the byte array to \a n bytes. If \a n is larger than
        the current size, the new bytes are set to \a c.
    */
    void resize(qsizetype n, char c) { d.resize(static_cast<std::size_t>(n), c); m_null = false; }
    QByteArray &fill(char c, qsizetype n = -1);

    /*!
        Returns a pointer to the data stored in the byte array. The pointer
        can be used to modify the bytes and stays valid until the array is
        resized or destroyed. The data is terminated with a \c{'\0'}.

        \sa constData()
    */
    char *data() { return d.data(); }
    /*!
        \overload

        Returns a pointer to the const data stored in the byte array.
    */
    const char *data() const noexcept { return d.data(); }
    /*!
        Returns a pointer to the const data stored in the byte array. The
        data is terminated with a \c{'\0'}.

        \sa data()
    */
    const char *constData() const noexcept { return d.data(); }
    /*!
        Returns the byte at index position \a i. \a i must be a valid index.

        \sa front(), back()
    */
    char at(qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    /*!
        Returns a reference to the byte at index position \a i, which can be
        assigned to. \a i must be a valid index.

        \sa at()
    */
    char &operator[](qsizetype i) { return d[static_cast<std::size_t>(i)]; }
    /*!
        \overload

        Same as at(\a i).
    */
    char operator[](qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    /*!
        Returns the first byte in the array. The array must not be empty.

        \sa back(), at()
    */
    char front() const { return d.front(); }
    /*!
        Returns the last byte in the array. The array must not be empty.

        \sa front(), at()
    */
    char back() const { return d.back(); }
    /*! Returns an iterator (a \c{char *}) pointing to the first byte in the array. */
    char *begin() { return d.data(); }
    /*! Returns an iterator (a \c{char *}) pointing just after the last byte in the array. */
    char *end() { return d.data() + d.size(); }
    /*! \overload

        Returns a const iterator pointing to the first byte in the array. */
    const char *begin() const noexcept { return d.data(); }
    /*! \overload

        Returns a const iterator pointing just after the last byte in the array. */
    const char *end() const noexcept { return d.data() + d.size(); }
    /*! Returns a const iterator pointing to the first byte in the array. */
    const char *cbegin() const noexcept { return begin(); }
    /*! Returns a const iterator pointing just after the last byte in the array. */
    const char *cend() const noexcept { return end(); }

    /*!
        Appends the byte array \a a to this byte array and returns a
        reference to this byte array.

        \sa prepend(), insert()
    */
    QByteArray &append(const QByteArray &a) { d += a.d; m_null = false; return *this; }
    /*!
        \overload

        Appends the \c{'\0'}-terminated string \a s to this byte array. If
        \a s is \c nullptr nothing is appended.
    */
    QByteArray &append(const char *s) { if (s) d += s; m_null = false; return *this; }
    /*!
        \overload

        Appends the first \a n bytes of \a s to this byte array.
    */
    QByteArray &append(const char *s, qsizetype n) { d.append(s, static_cast<std::size_t>(n)); m_null = false; return *this; }
    /*!
        \overload

        Appends \a n copies of the byte \a c to this byte array.
    */
    QByteArray &append(qsizetype n, char c) { d.append(static_cast<std::size_t>(n), c); m_null = false; return *this; }
    /*!
        \overload

        Appends the byte \a c to this byte array.
    */
    QByteArray &append(char c) { d.push_back(c); m_null = false; return *this; }
    /*! Same as append(\a c). This function is provided for STL compatibility. */
    void push_back(char c) { append(c); }
    /*!
        \overload

        Same as append(\a a). This function is provided for STL
        compatibility.
    */
    void push_back(const QByteArray &a) { append(a); }
    /*!
        Prepends the byte array \a a to this byte array and returns a
        reference to this byte array.

        \sa append(), insert()
    */
    QByteArray &prepend(const QByteArray &a) { d.insert(0, a.d); m_null = false; return *this; }
    /*!
        \overload

        Prepends the \c{'\0'}-terminated string \a s to this byte array.
    */
    QByteArray &prepend(const char *s) { d.insert(0, s); m_null = false; return *this; }
    /*!
        \overload

        Prepends the byte \a c to this byte array.
    */
    QByteArray &prepend(char c) { d.insert(d.begin(), c); m_null = false; return *this; }
    /*!
        Inserts the byte array \a a at index position \a pos and returns a
        reference to this byte array. \a pos must not be greater than size().

        \sa append(), prepend(), remove()
    */
    QByteArray &insert(qsizetype pos, const QByteArray &a) { d.insert(static_cast<std::size_t>(pos), a.d); return *this; }
    /*!
        \overload

        Inserts the \c{'\0'}-terminated string \a s at index position \a pos.
    */
    QByteArray &insert(qsizetype pos, const char *s) { d.insert(static_cast<std::size_t>(pos), s); return *this; }
    /*!
        \overload

        Inserts the byte \a c at index position \a pos.
    */
    QByteArray &insert(qsizetype pos, char c) { d.insert(d.begin() + pos, c); return *this; }
    QByteArray &remove(qsizetype pos, qsizetype len);
    QByteArray &replace(qsizetype pos, qsizetype len, const QByteArray &after);
    QByteArray &replace(const QByteArray &before, const QByteArray &after);
    /*!
        \overload

        Replaces every occurrence of the byte \a before with the byte array
        \a after and returns a reference to this byte array.
    */
    QByteArray &replace(char before, const QByteArray &after) { return replace(QByteArray(1, before), after); }
    QByteArray &replace(char before, char after);
    /*!
        Removes \a n bytes from the end of the array. If \a n is greater than
        or equal to size(), the result is an empty byte array.

        \sa truncate(), resize(), chopped()
    */
    void chop(qsizetype n) { if (n >= size()) d.clear(); else d.resize(d.size() - static_cast<std::size_t>(n)); }
    /*!
        Truncates the array at index position \a pos. If \a pos is beyond the
        end of the array, nothing happens.

        \sa chop(), resize(), left()
    */
    void truncate(qsizetype pos) { if (pos < size()) d.resize(static_cast<std::size_t>(pos < 0 ? 0 : pos)); }
    /*! Appends the byte array \a a to this byte array and returns a reference to this byte array. */
    QByteArray &operator+=(const QByteArray &a) { return append(a); }
    /*! \overload

        Appends the \c{'\0'}-terminated string \a s to this byte array. */
    QByteArray &operator+=(const char *s) { return append(s); }
    /*! \overload

        Appends the byte \a c to this byte array. */
    QByteArray &operator+=(char c) { return append(c); }

    QByteArray mid(qsizetype pos, qsizetype len = -1) const;
    /*!
        Returns a byte array containing the first \a n bytes of this byte
        array. If \a n is not less than size(), the whole array is returned.

        \sa right(), first(), mid()
    */
    QByteArray left(qsizetype n) const { return mid(0, n); }
    /*!
        Returns a byte array containing the last \a n bytes of this byte
        array. If \a n is not less than size(), the whole array is returned.

        \sa left(), last(), mid()
    */
    QByteArray right(qsizetype n) const { return n >= size() ? *this : mid(size() - n, n); }
    /*!
        Returns a byte array containing the first \a n bytes of this byte
        array. Same as left().

        \sa last(), sliced()
    */
    QByteArray first(qsizetype n) const { return left(n); }
    /*!
        Returns a byte array containing the last \a n bytes of this byte
        array. Same as right().

        \sa first(), sliced()
    */
    QByteArray last(qsizetype n) const { return right(n); }
    /*!
        Returns a byte array containing \a n bytes of this byte array,
        starting at index position \a pos. Same as mid(\a pos, \a n).

        \sa first(), last(), chopped()
    */
    QByteArray sliced(qsizetype pos, qsizetype n) const { return mid(pos, n); }
    /*!
        \overload

        Returns a byte array containing the bytes of this byte array from
        index position \a pos to the end.
    */
    QByteArray sliced(qsizetype pos) const { return mid(pos); }
    /*!
        Returns a byte array containing this byte array without its last
        \a n bytes.

        \sa chop(), first()
    */
    QByteArray chopped(qsizetype n) const { return left(size() - n); }
    QByteArray trimmed() const;
    QByteArray simplified() const;
    QByteArray toUpper() const;
    QByteArray toLower() const;
    QByteArray repeated(qsizetype times) const;
    QByteArray leftJustified(qsizetype width, char fill = ' ', bool truncate = false) const;
    QByteArray rightJustified(qsizetype width, char fill = ' ', bool truncate = false) const;

    /*!
        Returns \c true if this byte array starts with the byte array \a a;
        otherwise returns \c false.

        \sa endsWith(), left()
    */
    bool startsWith(const QByteArray &a) const { return d.compare(0, a.d.size(), a.d) == 0 && d.size() >= a.d.size(); }
    /*! \overload

        Returns \c true if this byte array starts with the byte \a c. */
    bool startsWith(char c) const { return !d.empty() && d[0] == c; }
    /*! \overload

        Returns \c true if this byte array starts with the \c{'\0'}-terminated string \a s. */
    bool startsWith(const char *s) const { return startsWith(QByteArray(s)); }
    /*!
        Returns \c true if this byte array ends with the byte array \a a;
        otherwise returns \c false.

        \sa startsWith(), right()
    */
    bool endsWith(const QByteArray &a) const { return d.size() >= a.d.size() && d.compare(d.size() - a.d.size(), a.d.size(), a.d) == 0; }
    /*! \overload

        Returns \c true if this byte array ends with the byte \a c. */
    bool endsWith(char c) const { return !d.empty() && d.back() == c; }
    /*! \overload

        Returns \c true if this byte array ends with the \c{'\0'}-terminated string \a s. */
    bool endsWith(const char *s) const { return endsWith(QByteArray(s)); }
    qsizetype indexOf(const QByteArray &a, qsizetype from = 0) const;
    qsizetype indexOf(char c, qsizetype from = 0) const;
    /*! \overload

        Returns the index position of the first occurrence of the \c{'\0'}-terminated string \a s, searching forward from index position \a from. */
    qsizetype indexOf(const char *s, qsizetype from = 0) const { return indexOf(QByteArray(s), from); }
    qsizetype lastIndexOf(const QByteArray &a, qsizetype from = -1) const;
    qsizetype lastIndexOf(char c, qsizetype from = -1) const;
    /*!
        Returns \c true if this byte array contains an occurrence of the byte
        array \a a; otherwise returns \c false.

        \sa indexOf(), count()
    */
    bool contains(const QByteArray &a) const { return indexOf(a) != -1; }
    /*! \overload

        Returns \c true if this byte array contains the byte \a c. */
    bool contains(char c) const { return indexOf(c) != -1; }
    /*! \overload

        Returns \c true if this byte array contains the \c{'\0'}-terminated string \a s. */
    bool contains(const char *s) const { return indexOf(s) != -1; }
    qsizetype count(const QByteArray &a) const;
    qsizetype count(char c) const;
    QList<QByteArray> split(char sep) const;

    QByteArray toHex(char separator = '\0') const;
    static QByteArray fromHex(const QByteArray &hex);
    QByteArray toBase64(Base64Options options = Base64Encoding) const;
    static QByteArray fromBase64(const QByteArray &base64, Base64Options options = Base64Encoding);
    QByteArray toPercentEncoding(const QByteArray &exclude = QByteArray(), const QByteArray &include = QByteArray(), char percent = '%') const;
    static QByteArray fromPercentEncoding(const QByteArray &pctEncoded, char percent = '%');

    static QByteArray number(qlonglong n, int base = 10);
    static QByteArray number(qulonglong n, int base = 10);
    /*! \overload

        Returns a byte array containing the string equivalent of the number \a n in the given \a base. */
    static QByteArray number(int n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    /*! \overload

        Returns a byte array containing the string equivalent of the number \a n in the given \a base. */
    static QByteArray number(uint n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    /*! \overload

        Returns a byte array containing the string equivalent of the number \a n in the given \a base. */
    static QByteArray number(long n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    /*! \overload

        Returns a byte array containing the string equivalent of the number \a n in the given \a base. */
    static QByteArray number(ulong n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    static QByteArray number(double n, char format = 'g', int precision = 6);
    /*!
        Sets the byte array to the string equivalent of the number \a n in the
        given \a base, and returns a reference to this byte array.

        \sa number(), toLongLong()
    */
    QByteArray &setNum(qlonglong n, int base = 10) { return *this = number(n, base); }
    /*! \overload

        Sets the byte array to the string equivalent of the number \a n in the given \a base. */
    QByteArray &setNum(qulonglong n, int base = 10) { return *this = number(n, base); }
    /*! \overload

        Sets the byte array to the string equivalent of the number \a n in the given \a base. */
    QByteArray &setNum(int n, int base = 10) { return *this = number(n, base); }
    /*! \overload

        Sets the byte array to the string equivalent of the number \a n in the given \a base. */
    QByteArray &setNum(uint n, int base = 10) { return *this = number(n, base); }
    /*! \overload

        Sets the byte array to the string equivalent of the floating-point
        number \a n, formatted with the given \a format and \a precision as
        described for number(). */
    QByteArray &setNum(double n, char format = 'g', int precision = 6) { return *this = number(n, format, precision); }
    int toInt(bool *ok = nullptr, int base = 10) const;
    uint toUInt(bool *ok = nullptr, int base = 10) const;
    long toLong(bool *ok = nullptr, int base = 10) const;
    qlonglong toLongLong(bool *ok = nullptr, int base = 10) const;
    qulonglong toULongLong(bool *ok = nullptr, int base = 10) const;
    short toShort(bool *ok = nullptr, int base = 10) const;
    ushort toUShort(bool *ok = nullptr, int base = 10) const;
    double toDouble(bool *ok = nullptr) const;
    float toFloat(bool *ok = nullptr) const;

    /*!
        Returns a byte array holding a copy of the string \a s.

        \sa toStdString()
    */
    static QByteArray fromStdString(const std::string &s) { return QByteArray(s); }
    /*!
        Returns a copy of the data as a \c std::string.

        \sa fromStdString(), stdString()
    */
    std::string toStdString() const { return d; }
    /*!
        Returns a const reference to the \c std::string that stores the data,
        without copying it. The reference is invalidated when the byte array
        is modified or destroyed.

        \sa toStdString()
    */
    const std::string &stdString() const noexcept { return d; }
    /*!
        Constructs a byte array from the first \a size bytes of \a data.

        Unlike in Qt, the data is copied, not shared; this is equivalent to
        calling the QByteArray(const char *, qsizetype) constructor.
    */
    static QByteArray fromRawData(const char *data, qsizetype size) { return QByteArray(data, size); }

    /*! Swaps this byte array with \a o. This operation is very fast and never fails. */
    void swap(QByteArray &o) noexcept { d.swap(o.d); std::swap(m_null, o.m_null); }
    int compare(const QByteArray &o, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    /*!
        Returns \c true if the byte array is not empty; otherwise returns
        \c false.

        \sa isEmpty()
    */
    explicit operator bool() const noexcept { return !isEmpty(); }

    friend bool operator==(const QByteArray &a, const QByteArray &b) noexcept { return a.d == b.d; }
    friend bool operator!=(const QByteArray &a, const QByteArray &b) noexcept { return a.d != b.d; }
    friend bool operator<(const QByteArray &a, const QByteArray &b) noexcept { return a.d < b.d; }
    friend bool operator>(const QByteArray &a, const QByteArray &b) noexcept { return a.d > b.d; }
    friend bool operator<=(const QByteArray &a, const QByteArray &b) noexcept { return a.d <= b.d; }
    friend bool operator>=(const QByteArray &a, const QByteArray &b) noexcept { return a.d >= b.d; }
    friend QByteArray operator+(const QByteArray &a, const QByteArray &b) { QByteArray r(a); r.append(b); return r; }
    friend QByteArray operator+(const QByteArray &a, char c) { QByteArray r(a); r.append(c); return r; }
    friend QByteArray operator+(char c, const QByteArray &a) { QByteArray r(1, c); r.append(a); return r; }

private:
    std::string d;
    bool m_null = true;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QByteArray::Base64Options)

inline std::size_t qHash(const QByteArray &b, std::size_t seed = 0) noexcept {
    return std::hash<std::string>{}(b.stdString()) ^ (seed * 0x9e3779b9u);
}

template <>
struct std::hash<QByteArray> {
    std::size_t operator()(const QByteArray &b) const noexcept { return std::hash<std::string>{}(b.stdString()); }
};

// Returns a copy of the C string src allocated with new char[], or nullptr if
// src is nullptr. The caller deletes it with delete[]. ndoc cannot document
// global functions yet.
inline char *qstrdup(const char *src) {
    if (!src)
        return nullptr;
    const std::size_t size = std::char_traits<char>::length(src) + 1;
    char *copy = new char[size];
    std::char_traits<char>::copy(copy, src, size);
    return copy;
}

inline QByteArray operator""_ba(const char *s, std::size_t n) { return QByteArray(s, static_cast<qsizetype>(n)); }
#define QByteArrayLiteral(str) QByteArray(str, static_cast<qsizetype>(sizeof(str) - 1))

#include <QtCore/qlist.h>
