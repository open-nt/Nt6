// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>

#include <string_view>

// A read-only, non-owning view of bytes.
class QByteArrayView {
public:
    using const_iterator = const char *;
    using iterator = const_iterator;
    using value_type = char;

    constexpr QByteArrayView() noexcept = default;
    constexpr QByteArrayView(std::nullptr_t) noexcept {}
    constexpr QByteArrayView(const char *data, qsizetype len) noexcept : p(data), n(len) {}
    QByteArrayView(const char *data) noexcept : p(data), n(data ? static_cast<qsizetype>(std::char_traits<char>::length(data)) : 0) {}
    QByteArrayView(const QByteArray &b) noexcept : p(b.isNull() ? nullptr : b.constData()), n(b.size()) {}
    QByteArrayView(std::string_view s) noexcept : p(s.data()), n(static_cast<qsizetype>(s.size())) {}

    QByteArray toByteArray() const { return p ? QByteArray(p, n) : QByteArray(); }
    constexpr const char *data() const noexcept { return p; }
    constexpr const char *constData() const noexcept { return p; }
    constexpr qsizetype size() const noexcept { return n; }
    constexpr qsizetype length() const noexcept { return n; }
    constexpr bool isNull() const noexcept { return p == nullptr; }
    constexpr bool isEmpty() const noexcept { return n == 0; }
    constexpr bool empty() const noexcept { return n == 0; }
    constexpr char at(qsizetype i) const { return p[i]; }
    constexpr char operator[](qsizetype i) const { return p[i]; }
    constexpr char front() const { return p[0]; }
    constexpr char back() const { return p[n - 1]; }
    constexpr const_iterator begin() const noexcept { return p; }
    constexpr const_iterator end() const noexcept { return p + n; }

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
    QByteArrayView left(qsizetype c) const { return mid(0, qMax<qsizetype>(c, 0)); }
    QByteArrayView right(qsizetype c) const { return c >= n ? *this : mid(n - qMax<qsizetype>(c, 0)); }
    constexpr QByteArrayView first(qsizetype c) const { return QByteArrayView(p, c); }
    constexpr QByteArrayView last(qsizetype c) const { return QByteArrayView(p + n - c, c); }
    constexpr QByteArrayView sliced(qsizetype pos) const { return QByteArrayView(p + pos, n - pos); }
    constexpr QByteArrayView sliced(qsizetype pos, qsizetype c) const { return QByteArrayView(p + pos, c); }
    constexpr QByteArrayView chopped(qsizetype c) const { return QByteArrayView(p, n - c); }

    bool startsWith(QByteArrayView s) const { return s.n <= n && view().substr(0, static_cast<std::size_t>(s.n)) == s.view(); }
    bool endsWith(QByteArrayView s) const { return s.n <= n && view().substr(static_cast<std::size_t>(n - s.n)) == s.view(); }
    qsizetype indexOf(QByteArrayView s, qsizetype from = 0) const {
        const auto r = view().find(s.view(), static_cast<std::size_t>(qMax<qsizetype>(from, 0)));
        return r == std::string_view::npos ? -1 : static_cast<qsizetype>(r);
    }
    bool contains(QByteArrayView s) const { return indexOf(s) != -1; }

    friend bool operator==(QByteArrayView a, QByteArrayView b) noexcept { return a.view() == b.view(); }
    friend bool operator!=(QByteArrayView a, QByteArrayView b) noexcept { return a.view() != b.view(); }
    friend bool operator<(QByteArrayView a, QByteArrayView b) noexcept { return a.view() < b.view(); }

private:
    std::string_view view() const noexcept { return std::string_view(p ? p : "", static_cast<std::size_t>(n)); }

    const char *p = nullptr;
    qsizetype n = 0;
};
