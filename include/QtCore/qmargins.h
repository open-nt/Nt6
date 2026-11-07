// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QMargins {
public:
    constexpr QMargins() noexcept = default;
    constexpr QMargins(int left, int top, int right, int bottom) noexcept : l(left), t(top), r(right), b(bottom) {}

    constexpr bool isNull() const noexcept { return l == 0 && t == 0 && r == 0 && b == 0; }
    constexpr int left() const noexcept { return l; }
    constexpr int top() const noexcept { return t; }
    constexpr int right() const noexcept { return r; }
    constexpr int bottom() const noexcept { return b; }
    constexpr void setLeft(int v) noexcept { l = v; }
    constexpr void setTop(int v) noexcept { t = v; }
    constexpr void setRight(int v) noexcept { r = v; }
    constexpr void setBottom(int v) noexcept { b = v; }

    constexpr QMargins &operator+=(const QMargins &m) { l += m.l; t += m.t; r += m.r; b += m.b; return *this; }
    constexpr QMargins &operator-=(const QMargins &m) { l -= m.l; t -= m.t; r -= m.r; b -= m.b; return *this; }
    constexpr QMargins &operator+=(int v) { l += v; t += v; r += v; b += v; return *this; }
    constexpr QMargins &operator-=(int v) { return *this += -v; }
    constexpr QMargins &operator*=(int f) { l *= f; t *= f; r *= f; b *= f; return *this; }
    constexpr QMargins &operator/=(int d) { l /= d; t /= d; r /= d; b /= d; return *this; }
    QMargins &operator*=(double f) { l = qRound(l * f); t = qRound(t * f); r = qRound(r * f); b = qRound(b * f); return *this; }
    QMargins &operator/=(double d) { l = qRound(l / d); t = qRound(t / d); r = qRound(r / d); b = qRound(b / d); return *this; }

    friend constexpr bool operator==(const QMargins &x, const QMargins &y) noexcept {
        return x.l == y.l && x.t == y.t && x.r == y.r && x.b == y.b;
    }
    friend constexpr bool operator!=(const QMargins &x, const QMargins &y) noexcept { return !(x == y); }
    friend constexpr QMargins operator+(const QMargins &x, const QMargins &y) noexcept { return QMargins(x.l + y.l, x.t + y.t, x.r + y.r, x.b + y.b); }
    friend constexpr QMargins operator-(const QMargins &x, const QMargins &y) noexcept { return QMargins(x.l - y.l, x.t - y.t, x.r - y.r, x.b - y.b); }
    friend constexpr QMargins operator+(const QMargins &x, int v) noexcept { return QMargins(x.l + v, x.t + v, x.r + v, x.b + v); }
    friend constexpr QMargins operator+(int v, const QMargins &x) noexcept { return x + v; }
    friend constexpr QMargins operator-(const QMargins &x, int v) noexcept { return x + (-v); }
    friend constexpr QMargins operator*(const QMargins &x, int f) noexcept { return QMargins(x.l * f, x.t * f, x.r * f, x.b * f); }
    friend constexpr QMargins operator*(int f, const QMargins &x) noexcept { return x * f; }
    friend constexpr QMargins operator/(const QMargins &x, int d) noexcept { return QMargins(x.l / d, x.t / d, x.r / d, x.b / d); }
    friend constexpr QMargins operator+(const QMargins &m) noexcept { return m; }
    friend constexpr QMargins operator-(const QMargins &m) noexcept { return QMargins(-m.l, -m.t, -m.r, -m.b); }

private:
    int l = 0, t = 0, r = 0, b = 0;
};

class QMarginsF {
public:
    constexpr QMarginsF() noexcept = default;
    constexpr QMarginsF(double left, double top, double right, double bottom) noexcept : l(left), t(top), r(right), b(bottom) {}
    constexpr QMarginsF(const QMargins &m) noexcept : l(m.left()), t(m.top()), r(m.right()), b(m.bottom()) {}

    bool isNull() const noexcept { return qFuzzyIsNull(l) && qFuzzyIsNull(t) && qFuzzyIsNull(r) && qFuzzyIsNull(b); }
    constexpr double left() const noexcept { return l; }
    constexpr double top() const noexcept { return t; }
    constexpr double right() const noexcept { return r; }
    constexpr double bottom() const noexcept { return b; }
    constexpr void setLeft(double v) noexcept { l = v; }
    constexpr void setTop(double v) noexcept { t = v; }
    constexpr void setRight(double v) noexcept { r = v; }
    constexpr void setBottom(double v) noexcept { b = v; }
    QMargins toMargins() const { return QMargins(qRound(l), qRound(t), qRound(r), qRound(b)); }

    constexpr QMarginsF &operator+=(const QMarginsF &m) { l += m.l; t += m.t; r += m.r; b += m.b; return *this; }
    constexpr QMarginsF &operator-=(const QMarginsF &m) { l -= m.l; t -= m.t; r -= m.r; b -= m.b; return *this; }
    constexpr QMarginsF &operator+=(double v) { l += v; t += v; r += v; b += v; return *this; }
    constexpr QMarginsF &operator-=(double v) { return *this += -v; }
    constexpr QMarginsF &operator*=(double f) { l *= f; t *= f; r *= f; b *= f; return *this; }
    constexpr QMarginsF &operator/=(double d) { l /= d; t /= d; r /= d; b /= d; return *this; }

    friend bool operator==(const QMarginsF &x, const QMarginsF &y) {
        return qFuzzyIsNull(x.l - y.l) && qFuzzyIsNull(x.t - y.t) && qFuzzyIsNull(x.r - y.r) && qFuzzyIsNull(x.b - y.b);
    }
    friend bool operator!=(const QMarginsF &x, const QMarginsF &y) { return !(x == y); }
    friend constexpr QMarginsF operator+(const QMarginsF &x, const QMarginsF &y) { return QMarginsF(x.l + y.l, x.t + y.t, x.r + y.r, x.b + y.b); }
    friend constexpr QMarginsF operator-(const QMarginsF &x, const QMarginsF &y) { return QMarginsF(x.l - y.l, x.t - y.t, x.r - y.r, x.b - y.b); }
    friend constexpr QMarginsF operator*(const QMarginsF &x, double f) { return QMarginsF(x.l * f, x.t * f, x.r * f, x.b * f); }
    friend constexpr QMarginsF operator*(double f, const QMarginsF &x) { return x * f; }
    friend constexpr QMarginsF operator/(const QMarginsF &x, double d) { return QMarginsF(x.l / d, x.t / d, x.r / d, x.b / d); }
    friend constexpr QMarginsF operator-(const QMarginsF &m) { return QMarginsF(-m.l, -m.t, -m.r, -m.b); }

private:
    double l = 0, t = 0, r = 0, b = 0;
};
