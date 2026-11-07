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
    enum Base64Option { Base64Encoding = 0, Base64UrlEncoding = 1, KeepTrailingEquals = 0, OmitTrailingEquals = 2 };
    Q_DECLARE_FLAGS(Base64Options, Base64Option)

    QByteArray() noexcept {}
    QByteArray(const char *data, qsizetype size = -1);
    QByteArray(qsizetype size, char ch) : d(static_cast<std::size_t>(size < 0 ? 0 : size), ch), m_null(false) {}
    QByteArray(const std::string &s) : d(s), m_null(false) {}
    QByteArray(std::string &&s) : d(std::move(s)), m_null(false) {}

    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    qsizetype length() const noexcept { return size(); }
    qsizetype count() const noexcept { return size(); }
    qsizetype capacity() const noexcept { return static_cast<qsizetype>(d.capacity()); }
    bool isEmpty() const noexcept { return d.empty(); }
    bool isNull() const noexcept { return m_null && d.empty(); }
    void clear() { d.clear(); m_null = true; }
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    void squeeze() { d.shrink_to_fit(); }
    void resize(qsizetype n) { d.resize(static_cast<std::size_t>(n), '\0'); m_null = false; }
    void resize(qsizetype n, char c) { d.resize(static_cast<std::size_t>(n), c); m_null = false; }
    QByteArray &fill(char c, qsizetype n = -1);

    char *data() { return d.data(); }
    const char *data() const noexcept { return d.data(); }
    const char *constData() const noexcept { return d.data(); }
    char at(qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    char &operator[](qsizetype i) { return d[static_cast<std::size_t>(i)]; }
    char operator[](qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    char front() const { return d.front(); }
    char back() const { return d.back(); }
    char *begin() { return d.data(); }
    char *end() { return d.data() + d.size(); }
    const char *begin() const noexcept { return d.data(); }
    const char *end() const noexcept { return d.data() + d.size(); }
    const char *cbegin() const noexcept { return begin(); }
    const char *cend() const noexcept { return end(); }

    QByteArray &append(const QByteArray &a) { d += a.d; m_null = false; return *this; }
    QByteArray &append(const char *s) { if (s) d += s; m_null = false; return *this; }
    QByteArray &append(const char *s, qsizetype n) { d.append(s, static_cast<std::size_t>(n)); m_null = false; return *this; }
    QByteArray &append(qsizetype n, char c) { d.append(static_cast<std::size_t>(n), c); m_null = false; return *this; }
    QByteArray &append(char c) { d.push_back(c); m_null = false; return *this; }
    void push_back(char c) { append(c); }
    void push_back(const QByteArray &a) { append(a); }
    QByteArray &prepend(const QByteArray &a) { d.insert(0, a.d); m_null = false; return *this; }
    QByteArray &prepend(const char *s) { d.insert(0, s); m_null = false; return *this; }
    QByteArray &prepend(char c) { d.insert(d.begin(), c); m_null = false; return *this; }
    QByteArray &insert(qsizetype pos, const QByteArray &a) { d.insert(static_cast<std::size_t>(pos), a.d); return *this; }
    QByteArray &insert(qsizetype pos, const char *s) { d.insert(static_cast<std::size_t>(pos), s); return *this; }
    QByteArray &insert(qsizetype pos, char c) { d.insert(d.begin() + pos, c); return *this; }
    QByteArray &remove(qsizetype pos, qsizetype len);
    QByteArray &replace(qsizetype pos, qsizetype len, const QByteArray &after);
    QByteArray &replace(const QByteArray &before, const QByteArray &after);
    QByteArray &replace(char before, const QByteArray &after) { return replace(QByteArray(1, before), after); }
    QByteArray &replace(char before, char after);
    void chop(qsizetype n) { if (n >= size()) d.clear(); else d.resize(d.size() - static_cast<std::size_t>(n)); }
    void truncate(qsizetype pos) { if (pos < size()) d.resize(static_cast<std::size_t>(pos < 0 ? 0 : pos)); }
    QByteArray &operator+=(const QByteArray &a) { return append(a); }
    QByteArray &operator+=(const char *s) { return append(s); }
    QByteArray &operator+=(char c) { return append(c); }

    QByteArray mid(qsizetype pos, qsizetype len = -1) const;
    QByteArray left(qsizetype n) const { return mid(0, n); }
    QByteArray right(qsizetype n) const { return n >= size() ? *this : mid(size() - n, n); }
    QByteArray first(qsizetype n) const { return left(n); }
    QByteArray last(qsizetype n) const { return right(n); }
    QByteArray sliced(qsizetype pos, qsizetype n) const { return mid(pos, n); }
    QByteArray sliced(qsizetype pos) const { return mid(pos); }
    QByteArray chopped(qsizetype n) const { return left(size() - n); }
    QByteArray trimmed() const;
    QByteArray simplified() const;
    QByteArray toUpper() const;
    QByteArray toLower() const;
    QByteArray repeated(qsizetype times) const;
    QByteArray leftJustified(qsizetype width, char fill = ' ', bool truncate = false) const;
    QByteArray rightJustified(qsizetype width, char fill = ' ', bool truncate = false) const;

    bool startsWith(const QByteArray &a) const { return d.compare(0, a.d.size(), a.d) == 0 && d.size() >= a.d.size(); }
    bool startsWith(char c) const { return !d.empty() && d[0] == c; }
    bool startsWith(const char *s) const { return startsWith(QByteArray(s)); }
    bool endsWith(const QByteArray &a) const { return d.size() >= a.d.size() && d.compare(d.size() - a.d.size(), a.d.size(), a.d) == 0; }
    bool endsWith(char c) const { return !d.empty() && d.back() == c; }
    bool endsWith(const char *s) const { return endsWith(QByteArray(s)); }
    qsizetype indexOf(const QByteArray &a, qsizetype from = 0) const;
    qsizetype indexOf(char c, qsizetype from = 0) const;
    qsizetype indexOf(const char *s, qsizetype from = 0) const { return indexOf(QByteArray(s), from); }
    qsizetype lastIndexOf(const QByteArray &a, qsizetype from = -1) const;
    qsizetype lastIndexOf(char c, qsizetype from = -1) const;
    bool contains(const QByteArray &a) const { return indexOf(a) != -1; }
    bool contains(char c) const { return indexOf(c) != -1; }
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
    static QByteArray number(int n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    static QByteArray number(uint n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    static QByteArray number(long n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    static QByteArray number(ulong n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    static QByteArray number(double n, char format = 'g', int precision = 6);
    QByteArray &setNum(qlonglong n, int base = 10) { return *this = number(n, base); }
    QByteArray &setNum(qulonglong n, int base = 10) { return *this = number(n, base); }
    QByteArray &setNum(int n, int base = 10) { return *this = number(n, base); }
    QByteArray &setNum(uint n, int base = 10) { return *this = number(n, base); }
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

    static QByteArray fromStdString(const std::string &s) { return QByteArray(s); }
    std::string toStdString() const { return d; }
    const std::string &stdString() const noexcept { return d; }
    static QByteArray fromRawData(const char *data, qsizetype size) { return QByteArray(data, size); }

    void swap(QByteArray &o) noexcept { d.swap(o.d); std::swap(m_null, o.m_null); }
    int compare(const QByteArray &o, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
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

inline QByteArray operator""_ba(const char *s, std::size_t n) { return QByteArray(s, static_cast<qsizetype>(n)); }
#define QByteArrayLiteral(str) QByteArray(str, static_cast<qsizetype>(sizeof(str) - 1))

#include <QtCore/qlist.h>
