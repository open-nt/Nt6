// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <string_view>

// A read-only, non-owning view of UTF-16 text.
class QStringView {
public:
    using const_iterator = const QChar *;
    using iterator = const_iterator;
    using value_type = QChar;

    constexpr QStringView() noexcept = default;
    constexpr QStringView(std::nullptr_t) noexcept {}
    constexpr QStringView(const QChar *str, qsizetype len) noexcept : p(str), n(len) {}
    QStringView(const char16_t *str, qsizetype len) noexcept : p(reinterpret_cast<const QChar *>(str)), n(len) {}
    QStringView(const char16_t *str) noexcept
        : p(reinterpret_cast<const QChar *>(str)), n(str ? static_cast<qsizetype>(std::char_traits<char16_t>::length(str)) : 0) {}
    QStringView(const QString &s) noexcept : p(s.isNull() ? nullptr : s.constData()), n(s.size()) {}
    QStringView(std::u16string_view s) noexcept : p(reinterpret_cast<const QChar *>(s.data())), n(static_cast<qsizetype>(s.size())) {}

    QString toString() const { return p ? QString(p, n) : QString(); }
    QByteArray toUtf8() const { return toString().toUtf8(); }
    QByteArray toLatin1() const { return toString().toLatin1(); }
    constexpr const QChar *data() const noexcept { return p; }
    constexpr const QChar *constData() const noexcept { return p; }
    const char16_t *utf16() const noexcept { return reinterpret_cast<const char16_t *>(p); }
    constexpr qsizetype size() const noexcept { return n; }
    constexpr qsizetype length() const noexcept { return n; }
    constexpr bool isNull() const noexcept { return p == nullptr; }
    constexpr bool isEmpty() const noexcept { return n == 0; }
    constexpr bool empty() const noexcept { return n == 0; }
    QChar at(qsizetype i) const { return p[i]; }
    QChar operator[](qsizetype i) const { return p[i]; }
    QChar front() const { return p[0]; }
    QChar back() const { return p[n - 1]; }
    QChar first() const { return front(); }
    QChar last() const { return back(); }
    const_iterator begin() const noexcept { return p; }
    const_iterator end() const noexcept { return p + n; }

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
    QStringView left(qsizetype c) const { return mid(0, qMax<qsizetype>(c, 0)); }
    QStringView right(qsizetype c) const { return c >= n ? *this : mid(n - qMax<qsizetype>(c, 0)); }
    QStringView first(qsizetype c) const { return QStringView(p, c); }
    QStringView last(qsizetype c) const { return QStringView(p + n - c, c); }
    QStringView sliced(qsizetype pos) const { return QStringView(p + pos, n - pos); }
    QStringView sliced(qsizetype pos, qsizetype c) const { return QStringView(p + pos, c); }
    QStringView chopped(qsizetype c) const { return QStringView(p, n - c); }
    void truncate(qsizetype c) { n = c; }
    void chop(qsizetype c) { n -= c; }
    QStringView trimmed() const {
        qsizetype b = 0, e = n;
        while (b < e && p[b].isSpace())
            ++b;
        while (e > b && p[e - 1].isSpace())
            --e;
        return QStringView(p + b, e - b);
    }

    bool startsWith(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().startsWith(s.toString(), cs); }
    bool endsWith(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().endsWith(s.toString(), cs); }
    qsizetype indexOf(QStringView s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().indexOf(s.toString(), from, cs); }
    qsizetype lastIndexOf(QStringView s, qsizetype from = -1, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return toString().lastIndexOf(s.toString(), from, cs); }
    bool contains(QStringView s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const { return indexOf(s, 0, cs) != -1; }
    int toInt(bool *ok = nullptr, int base = 10) const { return toString().toInt(ok, base); }
    double toDouble(bool *ok = nullptr) const { return toString().toDouble(ok); }

    friend bool operator==(QStringView a, QStringView b) noexcept {
        return a.n == b.n && std::char_traits<char16_t>::compare(a.utf16(), b.utf16(), static_cast<std::size_t>(a.n)) == 0;
    }
    friend bool operator!=(QStringView a, QStringView b) noexcept { return !(a == b); }
    friend bool operator<(QStringView a, QStringView b) noexcept {
        return std::u16string_view(a.utf16(), static_cast<std::size_t>(a.n)) < std::u16string_view(b.utf16(), static_cast<std::size_t>(b.n));
    }

private:
    const QChar *p = nullptr;
    qsizetype n = 0;
};
