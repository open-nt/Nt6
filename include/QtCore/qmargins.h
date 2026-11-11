// MIT License - see LICENSE
#pragma once

#include <algorithm>

#include <QtCore/qglobal.h>

/*!
    \class QMargins
    \brief The QMargins class defines the four margins of a rectangle.

    A margin is the distance between the edge of a rectangle and its
    contents. QMargins stores the left, top, right and bottom margins as
    \c int values. All margins are 0 by default. Use QMarginsF for
    floating-point margins.

    \differences
    Division by an
    integer truncates; division by a \c double rounds.
    \enddifferences

    \sa QMarginsF, QRect
*/
class QMarginsF;
class QMargins {
public:
    /*!
        Constructs margins that are all 0.

        \sa isNull()
    */
    constexpr QMargins() noexcept = default;
    /*!
        Constructs margins with the given \a left, \a top, \a right and
        \a bottom values.
    */
    constexpr QMargins(int left, int top, int right, int bottom) noexcept : l(left), t(top), r(right), b(bottom) {}

    /*!
        Returns \c true if all four margins are 0; otherwise returns
        \c false.
    */
    constexpr bool isNull() const noexcept { return l == 0 && t == 0 && r == 0 && b == 0; }
    /*!
        Returns the left margin.

        \sa setLeft()
    */
    constexpr int left() const noexcept { return l; }
    /*!
        Returns the top margin.

        \sa setTop()
    */
    constexpr int top() const noexcept { return t; }
    /*!
        Returns the right margin.

        \sa setRight()
    */
    constexpr int right() const noexcept { return r; }
    /*!
        Returns the bottom margin.

        \sa setBottom()
    */
    constexpr int bottom() const noexcept { return b; }
    /*!
        Sets the left margin to \a v.

        \sa left()
    */
    constexpr void setLeft(int v) noexcept { l = v; }
    /*!
        Sets the top margin to \a v.

        \sa top()
    */
    constexpr void setTop(int v) noexcept { t = v; }
    /*!
        Sets the right margin to \a v.

        \sa right()
    */
    constexpr void setRight(int v) noexcept { r = v; }
    /*!
        Sets the bottom margin to \a v.

        \sa bottom()
    */
    constexpr void setBottom(int v) noexcept { b = v; }

    /*!
        Adds the margins of \a m to these margins and returns a reference to
        these margins.
    */
    constexpr QMargins &operator+=(const QMargins &m) { l += m.l; t += m.t; r += m.r; b += m.b; return *this; }
    /*!
        Subtracts the margins of \a m from these margins and returns a
        reference to these margins.
    */
    constexpr QMargins &operator-=(const QMargins &m) { l -= m.l; t -= m.t; r -= m.r; b -= m.b; return *this; }
    /*!
        \overload

        Adds \a v to each margin and returns a reference to these margins.
    */
    constexpr QMargins &operator+=(int v) { l += v; t += v; r += v; b += v; return *this; }
    /*!
        \overload

        Subtracts \a v from each margin and returns a reference to these
        margins.
    */
    constexpr QMargins &operator-=(int v) { return *this += -v; }
    /*!
        Multiplies each margin by the integer factor \a f and returns a
        reference to these margins.
    */
    constexpr QMargins &operator*=(int f) { l *= f; t *= f; r *= f; b *= f; return *this; }
    /*!
        Divides each margin by the integer divisor \a d, truncating the
        results, and returns a reference to these margins.
    */
    constexpr QMargins &operator/=(int d) { l /= d; t /= d; r /= d; b /= d; return *this; }
    /*!
        \overload

        Multiplies each margin by \a f, rounds the result to the nearest
        integer, and returns a reference to these margins.
    */
    QMargins &operator*=(double f) { l = qRound(l * f); t = qRound(t * f); r = qRound(r * f); b = qRound(b * f); return *this; }
    /*!
        \overload

        Divides each margin by \a d, rounds the result to the nearest
        integer, and returns a reference to these margins.
    */
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
    friend QMargins operator*(const QMargins &x, double f) { return QMargins(qRound(x.l * f), qRound(x.t * f), qRound(x.r * f), qRound(x.b * f)); }
    friend QMargins operator*(double f, const QMargins &x) { return x * f; }
    friend QMargins operator/(const QMargins &x, double d) { return QMargins(qRound(x.l / d), qRound(x.t / d), qRound(x.r / d), qRound(x.b / d)); }
    friend constexpr QMargins operator+(const QMargins &m) noexcept { return m; }
    friend constexpr QMargins operator-(const QMargins &m) noexcept { return QMargins(-m.l, -m.t, -m.r, -m.b); }
    /*!
        \since 6.0
        Returns the margins made of the larger of each margin of \a m1 and
        \a m2.
    */
    friend constexpr QMargins operator|(const QMargins &m1, const QMargins &m2) noexcept {
        return QMargins(std::max(m1.l, m2.l), std::max(m1.t, m2.t), std::max(m1.r, m2.r), std::max(m1.b, m2.b));
    }
    /*!
        \since 6.0
        Returns the margins made of the smaller of each margin of \a m1 and
        \a m2.
    */
    friend constexpr QMargins operator&(const QMargins &m1, const QMargins &m2) noexcept {
        return QMargins(std::min(m1.l, m2.l), std::min(m1.t, m2.t), std::min(m1.r, m2.r), std::min(m1.b, m2.b));
    }
    /*!
        \since 6.4
        Returns these margins as margins with floating point accuracy.

        \sa QMarginsF::toMargins()
    */
    constexpr QMarginsF toMarginsF() const noexcept;

private:
    int l = 0, t = 0, r = 0, b = 0;
};

/*!
    \class QMarginsF
    \brief The QMarginsF class defines the four margins of a rectangle using floating point precision.

    A margin is the distance between the edge of a rectangle and its
    contents. QMarginsF stores the left, top, right and bottom margins as
    \c double values. All margins are 0 by default. Use QMargins for integer
    margins.

    \differences
    Margins are always \c double, which is what \c qreal is in Nt. Equality
    is fuzzy per margin.
    \enddifferences

    \sa QMargins, QRectF
*/
class QMarginsF {
public:
    /*!
        Constructs margins that are all 0.0.

        \sa isNull()
    */
    constexpr QMarginsF() noexcept = default;
    /*!
        Constructs margins with the given \a left, \a top, \a right and
        \a bottom values.
    */
    constexpr QMarginsF(double left, double top, double right, double bottom) noexcept : l(left), t(top), r(right), b(bottom) {}
    /*!
        Constructs margins with floating point accuracy from the integer
        margins \a m.

        \sa toMargins()
    */
    constexpr QMarginsF(const QMargins &m) noexcept : l(m.left()), t(m.top()), r(m.right()), b(m.bottom()) {}

    /*!
        Returns \c true if all four margins are fuzzily equal to 0;
        otherwise returns \c false.
    */
    bool isNull() const noexcept { return qFuzzyIsNull(l) && qFuzzyIsNull(t) && qFuzzyIsNull(r) && qFuzzyIsNull(b); }
    /*!
        Returns the left margin.

        \sa setLeft()
    */
    constexpr double left() const noexcept { return l; }
    /*!
        Returns the top margin.

        \sa setTop()
    */
    constexpr double top() const noexcept { return t; }
    /*!
        Returns the right margin.

        \sa setRight()
    */
    constexpr double right() const noexcept { return r; }
    /*!
        Returns the bottom margin.

        \sa setBottom()
    */
    constexpr double bottom() const noexcept { return b; }
    /*!
        Sets the left margin to \a v.

        \sa left()
    */
    constexpr void setLeft(double v) noexcept { l = v; }
    /*!
        Sets the top margin to \a v.

        \sa top()
    */
    constexpr void setTop(double v) noexcept { t = v; }
    /*!
        Sets the right margin to \a v.

        \sa right()
    */
    constexpr void setRight(double v) noexcept { r = v; }
    /*!
        Sets the bottom margin to \a v.

        \sa bottom()
    */
    constexpr void setBottom(double v) noexcept { b = v; }
    /*!
        Returns a QMargins with each margin rounded to the nearest integer.

        \sa QMarginsF()
    */
    QMargins toMargins() const { return QMargins(qRound(l), qRound(t), qRound(r), qRound(b)); }

    /*!
        Adds the margins of \a m to these margins and returns a reference to
        these margins.
    */
    constexpr QMarginsF &operator+=(const QMarginsF &m) { l += m.l; t += m.t; r += m.r; b += m.b; return *this; }
    /*!
        Subtracts the margins of \a m from these margins and returns a
        reference to these margins.
    */
    constexpr QMarginsF &operator-=(const QMarginsF &m) { l -= m.l; t -= m.t; r -= m.r; b -= m.b; return *this; }
    /*!
        \overload

        Adds \a v to each margin and returns a reference to these margins.
    */
    constexpr QMarginsF &operator+=(double v) { l += v; t += v; r += v; b += v; return *this; }
    /*!
        \overload

        Subtracts \a v from each margin and returns a reference to these
        margins.
    */
    constexpr QMarginsF &operator-=(double v) { return *this += -v; }
    /*!
        Multiplies each margin by the factor \a f and returns a reference to
        these margins.
    */
    constexpr QMarginsF &operator*=(double f) { l *= f; t *= f; r *= f; b *= f; return *this; }
    /*!
        Divides each margin by the divisor \a d and returns a reference to
        these margins.
    */
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
    friend constexpr QMarginsF operator+(const QMarginsF &x, double v) { return QMarginsF(x.l + v, x.t + v, x.r + v, x.b + v); }
    friend constexpr QMarginsF operator+(double v, const QMarginsF &x) { return x + v; }
    friend constexpr QMarginsF operator-(const QMarginsF &x, double v) { return x + (-v); }
    friend constexpr QMarginsF operator+(const QMarginsF &m) { return m; }
    /*!
        \since 6.0
        Returns the margins made of the larger of each margin of \a m1 and
        \a m2.
    */
    friend constexpr QMarginsF operator|(const QMarginsF &m1, const QMarginsF &m2) noexcept {
        return QMarginsF(std::max(m1.l, m2.l), std::max(m1.t, m2.t), std::max(m1.r, m2.r), std::max(m1.b, m2.b));
    }
    /*!
        \since 6.0
        Returns the margins made of the smaller of each margin of \a m1 and
        \a m2.
    */
    friend constexpr QMarginsF operator&(const QMarginsF &m1, const QMarginsF &m2) noexcept {
        return QMarginsF(std::min(m1.l, m2.l), std::min(m1.t, m2.t), std::min(m1.r, m2.r), std::min(m1.b, m2.b));
    }
    friend constexpr QMarginsF operator-(const QMarginsF &m) { return QMarginsF(-m.l, -m.t, -m.r, -m.b); }

private:
    double l = 0, t = 0, r = 0, b = 0;
};

constexpr QMarginsF QMargins::toMarginsF() const noexcept { return QMarginsF(*this); }
