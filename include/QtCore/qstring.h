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

class QLatin1StringView {
public:
    constexpr QLatin1StringView() noexcept = default;
    constexpr QLatin1StringView(const char *s) noexcept : p(s), n(s ? static_cast<qsizetype>(std::char_traits<char>::length(s)) : 0) {}
    constexpr QLatin1StringView(const char *s, qsizetype len) noexcept : p(s), n(len) {}
    QLatin1StringView(const QByteArray &b) noexcept : p(b.constData()), n(b.size()) {}
    constexpr const char *latin1() const noexcept { return p; }
    constexpr const char *data() const noexcept { return p; }
    constexpr qsizetype size() const noexcept { return n; }
    constexpr bool isEmpty() const noexcept { return n == 0; }
    constexpr QChar at(qsizetype i) const { return QChar::fromLatin1(p[i]); }
    constexpr QChar operator[](qsizetype i) const { return at(i); }

private:
    const char *p = "";
    qsizetype n = 0;
};
using QLatin1String = QLatin1StringView;

class QString {
public:
    enum NormalizationForm { NormalizationForm_D, NormalizationForm_C, NormalizationForm_KD, NormalizationForm_KC };
    using iterator = QChar *;
    using const_iterator = const QChar *;
    using value_type = QChar;
    using size_type = qsizetype;

    QString() noexcept {}
    QString(const char *utf8);
    QString(const char16_t *u, qsizetype size = -1);
    QString(const QChar *u, qsizetype size);
    QString(QChar c) : d(1, c.unicode()), m_null(false) {}
    QString(qsizetype size, QChar c) : d(static_cast<std::size_t>(size < 0 ? 0 : size), c.unicode()), m_null(false) {}
    QString(QLatin1StringView s);
    QString(const QByteArray &utf8);
    QString(const std::u16string &s) : d(s), m_null(false) {}

    // --- size / access ---
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    qsizetype length() const noexcept { return size(); }
    qsizetype count() const noexcept { return size(); }
    qsizetype capacity() const noexcept { return static_cast<qsizetype>(d.capacity()); }
    bool isEmpty() const noexcept { return d.empty(); }
    bool isNull() const noexcept { return m_null && d.empty(); }
    void clear() { d.clear(); m_null = true; }
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    void squeeze() { d.shrink_to_fit(); }
    void resize(qsizetype n) { d.resize(static_cast<std::size_t>(n)); m_null = false; }
    void resize(qsizetype n, QChar c) { d.resize(static_cast<std::size_t>(n), c.unicode()); m_null = false; }
    QString &fill(QChar c, qsizetype n = -1);
    void truncate(qsizetype pos) { if (pos < size()) d.resize(static_cast<std::size_t>(pos < 0 ? 0 : pos)); }
    void chop(qsizetype n) { if (n >= size()) d.clear(); else d.resize(d.size() - static_cast<std::size_t>(n)); }

    QChar at(qsizetype i) const { return QChar(d[static_cast<std::size_t>(i)]); }
    QChar operator[](qsizetype i) const { return at(i); }
    QChar &operator[](qsizetype i) { return data()[i]; }
    QChar front() const { return at(0); }
    QChar back() const { return at(size() - 1); }
    QChar *data() { return reinterpret_cast<QChar *>(d.data()); }
    const QChar *data() const { return reinterpret_cast<const QChar *>(d.data()); }
    const QChar *constData() const { return data(); }
    const char16_t *utf16() const { return d.c_str(); }
    const std::u16string &u16string() const noexcept { return d; }
    iterator begin() { return data(); }
    iterator end() { return data() + size(); }
    const_iterator begin() const { return data(); }
    const_iterator end() const { return data() + size(); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    const_iterator constBegin() const { return begin(); }
    const_iterator constEnd() const { return end(); }

    // --- modification ---
    QString &append(const QString &s) { d += s.d; m_null = false; return *this; }
    QString &append(QChar c) { d.push_back(c.unicode()); m_null = false; return *this; }
    QString &append(QLatin1StringView s) { return append(QString(s)); }
    QString &append(const char *s) { return append(QString(s)); }
    void push_back(QChar c) { append(c); }
    void push_back(const QString &s) { append(s); }
    QString &prepend(const QString &s) { d.insert(0, s.d); m_null = false; return *this; }
    QString &prepend(QChar c) { d.insert(d.begin(), c.unicode()); m_null = false; return *this; }
    QString &insert(qsizetype pos, const QString &s) { d.insert(static_cast<std::size_t>(pos), s.d); m_null = false; return *this; }
    QString &insert(qsizetype pos, QChar c) { d.insert(d.begin() + pos, c.unicode()); m_null = false; return *this; }
    QString &remove(qsizetype pos, qsizetype len);
    QString &remove(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(s, QString(), cs); }
    QString &remove(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(QString(c), QString(), cs); }
    QString &replace(qsizetype pos, qsizetype len, const QString &after);
    QString &replace(const QString &before, const QString &after, Qt::CaseSensitivity cs = Qt::CaseSensitive);
    QString &replace(QChar before, QChar after, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(QString(before), QString(after), cs); }
    QString &replace(QChar before, const QString &after, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return replace(QString(before), after, cs); }
    QString &operator+=(const QString &s) { return append(s); }
    QString &operator+=(QChar c) { return append(c); }
    QString &operator+=(const char *s) { return append(QString(s)); }

    // --- substrings ---
    QString mid(qsizetype pos, qsizetype len = -1) const;
    QString left(qsizetype n) const { return mid(0, n); }
    QString right(qsizetype n) const { return n >= size() ? *this : mid(size() - n, n); }
    QString first(qsizetype n) const { return left(n); }
    QString last(qsizetype n) const { return right(n); }
    QString sliced(qsizetype pos, qsizetype n) const { return mid(pos, n); }
    QString sliced(qsizetype pos) const { return mid(pos); }
    QString chopped(qsizetype n) const { return left(size() - n); }
    QString trimmed() const;
    QString simplified() const;
    QString toUpper() const;
    QString toLower() const;
    QString toCaseFolded() const { return toLower(); }
    QString repeated(qsizetype times) const;
    QString leftJustified(qsizetype width, QChar fill = QChar(u' '), bool truncate = false) const;
    QString rightJustified(qsizetype width, QChar fill = QChar(u' '), bool truncate = false) const;
    QString section(QChar sep, qsizetype start, qsizetype end = -1) const;
    QString section(const QString &sep, qsizetype start, qsizetype end = -1) const;

    // --- searching ---
    bool startsWith(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool startsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return startsWith(QString(c), cs); }
    bool endsWith(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    bool endsWith(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return endsWith(QString(c), cs); }
    qsizetype indexOf(const QString &s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype indexOf(QChar c, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(QString(c), from, cs); }
    qsizetype lastIndexOf(const QString &s, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype lastIndexOf(QChar c, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return lastIndexOf(QString(c), from, cs); }
    bool contains(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(s, 0, cs) != -1; }
    bool contains(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(c, 0, cs) != -1; }
    qsizetype count(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    qsizetype count(QChar c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return count(QString(c), cs); }
    QStringList split(const QString &sep, Qt::SplitBehavior behavior = Qt::KeepEmptyParts, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    QStringList split(QChar sep, Qt::SplitBehavior behavior = Qt::KeepEmptyParts, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;

    // --- formatting / numbers ---
    QString arg(const QString &a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const;
    QString arg(QChar a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const { return arg(QString(a), fieldWidth, fillChar); }
    QString arg(char a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const { return arg(QString(QChar(a)), fieldWidth, fillChar); }
    QString arg(const char *a, int fieldWidth = 0, QChar fillChar = QChar(u' ')) const { return arg(QString(a), fieldWidth, fillChar); }
    QString arg(qlonglong a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const;
    QString arg(qulonglong a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const;
    QString arg(long a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qlonglong>(a), fieldWidth, base, fillChar); }
    QString arg(ulong a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qulonglong>(a), fieldWidth, base, fillChar); }
    QString arg(int a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qlonglong>(a), fieldWidth, base, fillChar); }
    QString arg(uint a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qulonglong>(a), fieldWidth, base, fillChar); }
    QString arg(short a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qlonglong>(a), fieldWidth, base, fillChar); }
    QString arg(ushort a, int fieldWidth = 0, int base = 10, QChar fillChar = QChar(u' ')) const { return arg(static_cast<qulonglong>(a), fieldWidth, base, fillChar); }
    QString arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1, QChar fillChar = QChar(u' ')) const;
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
    static QString number(int n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    static QString number(uint n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    static QString number(long n, int base = 10) { return number(static_cast<qlonglong>(n), base); }
    static QString number(ulong n, int base = 10) { return number(static_cast<qulonglong>(n), base); }
    static QString number(double n, char format = 'g', int precision = 6);
    QString &setNum(qlonglong n, int base = 10) { return *this = number(n, base); }
    QString &setNum(qulonglong n, int base = 10) { return *this = number(n, base); }
    QString &setNum(int n, int base = 10) { return *this = number(n, base); }
    QString &setNum(uint n, int base = 10) { return *this = number(n, base); }
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
    static QString fromUtf8(const QByteArray &b) { return fromUtf8(b.constData(), b.size()); }
    static QString fromLatin1(const char *s, qsizetype size = -1);
    static QString fromLatin1(const QByteArray &b) { return fromLatin1(b.constData(), b.size()); }
    static QString fromLocal8Bit(const char *s, qsizetype size = -1) { return fromUtf8(s, size); }
    static QString fromLocal8Bit(const QByteArray &b) { return fromUtf8(b); }
    static QString fromStdString(const std::string &s) { return fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }
    static QString fromStdU16String(const std::u16string &s) { return QString(s); }
    static QString fromUtf16(const char16_t *s, qsizetype size = -1) { return QString(s, size); }
    static QString fromRawData(const QChar *u, qsizetype size) { return QString(u, size); }
    QByteArray toUtf8() const;
    QByteArray toLatin1() const;
    QByteArray toLocal8Bit() const { return toUtf8(); }
    std::string toStdString() const { return toUtf8().toStdString(); }
    std::u16string toStdU16String() const { return d; }

    int compare(const QString &o, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;
    static int compare(const QString &a, const QString &b, Qt::CaseSensitivity cs = Qt::CaseSensitive) { return a.compare(b, cs); }
    static int localeAwareCompare(const QString &a, const QString &b) { return a.compare(b); }
    bool isRightToLeft() const { return false; }
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
inline namespace Literals {
inline namespace StringLiterals {
inline QString operator""_s(const char16_t *s, std::size_t n) { return QString(s, static_cast<qsizetype>(n)); }
constexpr QLatin1StringView operator""_L1(const char *s, std::size_t n) { return QLatin1StringView(s, static_cast<qsizetype>(n)); }
inline QChar operator""_L1(char c) { return QChar(c); }
} // namespace StringLiterals
} // namespace Literals
} // namespace Qt

#include <QtCore/qstringlist.h>
