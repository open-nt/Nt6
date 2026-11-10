// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QPointF;

/*!
    \class QPoint
    \brief The QPoint class defines a point in the plane using integer precision.

    A point is specified by an x coordinate and a y coordinate, both
    stored as \c int. The default point is the origin (0, 0). Use
    QPointF for floating-point coordinates.

    Points can be added, subtracted and scaled with the usual operators.
    Multiplying or dividing by a \c double rounds each coordinate to the
    nearest integer.

    \differences
    There are no
    \c operator*() or \c operator/() overloads taking \c float or \c qreal
    other than \c double, and no platform conversions such as
    \c toCGPoint().
    \enddifferences

    \sa QPointF, QLine
*/
class QPoint {
public:
    /*!
        Constructs a null point, that is, a point with coordinates (0, 0).

        \sa isNull()
    */
    constexpr QPoint() noexcept = default;
    /*!
        Constructs a point with coordinates (\a xpos, \a ypos).
    */
    constexpr QPoint(int xpos, int ypos) noexcept : xp(xpos), yp(ypos) {}

    /*!
        Returns \c true if both the x and y coordinates are 0; otherwise
        returns \c false.
    */
    constexpr bool isNull() const noexcept { return xp == 0 && yp == 0; }
    /*!
        Returns the x coordinate of this point.

        \sa setX(), rx()
    */
    constexpr int x() const noexcept { return xp; }
    /*!
        Returns the y coordinate of this point.

        \sa setY(), ry()
    */
    constexpr int y() const noexcept { return yp; }
    /*!
        Sets the x coordinate of this point to the given \a x coordinate.

        \sa x(), setY()
    */
    constexpr void setX(int x) noexcept { xp = x; }
    /*!
        Sets the y coordinate of this point to the given \a y coordinate.

        \sa y(), setX()
    */
    constexpr void setY(int y) noexcept { yp = y; }
    /*!
        Returns a reference to the x coordinate of this point, allowing it
        to be modified directly.

        \sa x(), setX()
    */
    constexpr int &rx() noexcept { return xp; }
    /*!
        Returns a reference to the y coordinate of this point, allowing it
        to be modified directly.

        \sa y(), setY()
    */
    constexpr int &ry() noexcept { return yp; }
    /*!
        Returns the sum of the absolute values of x() and y(), the
        distance of the point from the origin measured along the axes.
    */
    constexpr int manhattanLength() const { return qAbs(xp) + qAbs(yp); }
    /*!
        Returns a point with the x and y coordinates exchanged.

        \sa QPointF::transposed()
    */
    constexpr QPoint transposed() const noexcept { return QPoint(yp, xp); }
    /*!
        Returns this point as a QPointF.

        \sa QPointF::toPoint()
    */
    constexpr QPointF toPointF() const noexcept;

    /*!
        Adds the given \a p to this point and returns a reference to this
        point.
    */
    constexpr QPoint &operator+=(const QPoint &p) { xp += p.xp; yp += p.yp; return *this; }
    /*!
        Subtracts the given \a p from this point and returns a reference to
        this point.
    */
    constexpr QPoint &operator-=(const QPoint &p) { xp -= p.xp; yp -= p.yp; return *this; }
    /*!
        Multiplies this point's coordinates by the given integer factor \a f
        and returns a reference to this point.
    */
    constexpr QPoint &operator*=(int f) { xp *= f; yp *= f; return *this; }
    /*!
        \overload

        Multiplies this point's coordinates by the given \a f and rounds
        each result to the nearest integer.
    */
    QPoint &operator*=(double f) { xp = qRound(xp * f); yp = qRound(yp * f); return *this; }
    /*!
        Divides both coordinates of this point by the given divisor \a d,
        rounds each result to the nearest integer, and returns a reference
        to this point.
    */
    QPoint &operator/=(double d) { xp = qRound(xp / d); yp = qRound(yp / d); return *this; }

    /*!
        Returns the dot product of \a a and \a b.
    */
    static constexpr int dotProduct(const QPoint &a, const QPoint &b) { return a.xp * b.xp + a.yp * b.yp; }

    friend constexpr bool operator==(const QPoint &a, const QPoint &b) noexcept { return a.xp == b.xp && a.yp == b.yp; }
    friend constexpr bool operator!=(const QPoint &a, const QPoint &b) noexcept { return !(a == b); }
    friend constexpr QPoint operator+(const QPoint &a, const QPoint &b) noexcept { return QPoint(a.xp + b.xp, a.yp + b.yp); }
    friend constexpr QPoint operator-(const QPoint &a, const QPoint &b) noexcept { return QPoint(a.xp - b.xp, a.yp - b.yp); }
    friend constexpr QPoint operator*(const QPoint &p, int f) noexcept { return QPoint(p.xp * f, p.yp * f); }
    friend constexpr QPoint operator*(int f, const QPoint &p) noexcept { return p * f; }
    friend QPoint operator*(const QPoint &p, double f) { return QPoint(qRound(p.xp * f), qRound(p.yp * f)); }
    friend QPoint operator*(double f, const QPoint &p) { return p * f; }
    friend constexpr QPoint operator+(const QPoint &p) noexcept { return p; }
    friend constexpr QPoint operator-(const QPoint &p) noexcept { return QPoint(-p.xp, -p.yp); }
    friend QPoint operator/(const QPoint &p, double d) { return QPoint(qRound(p.xp / d), qRound(p.yp / d)); }

private:
    int xp = 0;
    int yp = 0;
};

/*!
    \class QPointF
    \brief The QPointF class defines a point in the plane using floating point precision.

    A point is specified by an x coordinate and a y coordinate, both
    stored as \c double. Use QPoint for integer coordinates.

    Equality comparison is fuzzy: two coordinates are considered equal if
    they are equal within the precision of \c double, as determined by
    qFuzzyCompare() and qFuzzyIsNull().

    \differences
    Coordinates are always \c double, which is what \c qreal is in Nt. There
    are no platform conversions such as \c toCGPoint().
    \enddifferences

    \sa QPoint, QLineF
*/
class QPointF {
public:
    /*!
        Constructs a null point, that is, a point with coordinates (0.0, 0.0).

        \sa isNull()
    */
    constexpr QPointF() noexcept = default;
    /*!
        Constructs a copy of the given integer point \a p.

        \sa toPoint(), QPoint::toPointF()
    */
    constexpr QPointF(const QPoint &p) noexcept : xp(p.x()), yp(p.y()) {}
    /*!
        Constructs a point with coordinates (\a xpos, \a ypos).
    */
    constexpr QPointF(double xpos, double ypos) noexcept : xp(xpos), yp(ypos) {}

    /*!
        Returns \c true if both the x and y coordinates are fuzzily equal to
        0; otherwise returns \c false.
    */
    bool isNull() const noexcept { return qFuzzyIsNull(xp) && qFuzzyIsNull(yp); }
    /*!
        Returns the x coordinate of this point.

        \sa setX(), rx()
    */
    constexpr double x() const noexcept { return xp; }
    /*!
        Returns the y coordinate of this point.

        \sa setY(), ry()
    */
    constexpr double y() const noexcept { return yp; }
    /*!
        Sets the x coordinate of this point to the given \a x coordinate.

        \sa x(), setY()
    */
    constexpr void setX(double x) noexcept { xp = x; }
    /*!
        Sets the y coordinate of this point to the given \a y coordinate.

        \sa y(), setX()
    */
    constexpr void setY(double y) noexcept { yp = y; }
    /*!
        Returns a reference to the x coordinate of this point, allowing it
        to be modified directly.

        \sa x(), setX()
    */
    constexpr double &rx() noexcept { return xp; }
    /*!
        Returns a reference to the y coordinate of this point, allowing it
        to be modified directly.

        \sa y(), setY()
    */
    constexpr double &ry() noexcept { return yp; }
    /*!
        Returns the sum of the absolute values of x() and y(), the
        distance of the point from the origin measured along the axes.
    */
    constexpr double manhattanLength() const { return qAbs(xp) + qAbs(yp); }
    /*!
        Returns a point with the x and y coordinates exchanged.

        \sa QPoint::transposed()
    */
    constexpr QPointF transposed() const noexcept { return QPointF(yp, xp); }
    /*!
        Returns a QPoint with each coordinate rounded to the nearest
        integer.

        \sa QPoint::toPointF()
    */
    QPoint toPoint() const { return QPoint(qRound(xp), qRound(yp)); }

    /*!
        Adds the given \a p to this point and returns a reference to this
        point.
    */
    constexpr QPointF &operator+=(const QPointF &p) { xp += p.xp; yp += p.yp; return *this; }
    /*!
        Subtracts the given \a p from this point and returns a reference to
        this point.
    */
    constexpr QPointF &operator-=(const QPointF &p) { xp -= p.xp; yp -= p.yp; return *this; }
    /*!
        Multiplies this point's coordinates by the given factor \a f and
        returns a reference to this point.
    */
    constexpr QPointF &operator*=(double f) { xp *= f; yp *= f; return *this; }
    /*!
        Divides both coordinates of this point by the given divisor \a d
        and returns a reference to this point.
    */
    constexpr QPointF &operator/=(double d) { xp /= d; yp /= d; return *this; }

    /*!
        Returns the dot product of \a a and \a b.
    */
    static constexpr double dotProduct(const QPointF &a, const QPointF &b) { return a.xp * b.xp + a.yp * b.yp; }

    friend bool operator==(const QPointF &a, const QPointF &b) {
        return (a.xp == 0. || b.xp == 0. ? qFuzzyIsNull(a.xp - b.xp) : qFuzzyCompare(a.xp, b.xp)) &&
               (a.yp == 0. || b.yp == 0. ? qFuzzyIsNull(a.yp - b.yp) : qFuzzyCompare(a.yp, b.yp));
    }
    friend bool operator!=(const QPointF &a, const QPointF &b) { return !(a == b); }
    friend constexpr QPointF operator+(const QPointF &a, const QPointF &b) { return QPointF(a.xp + b.xp, a.yp + b.yp); }
    friend constexpr QPointF operator-(const QPointF &a, const QPointF &b) { return QPointF(a.xp - b.xp, a.yp - b.yp); }
    friend constexpr QPointF operator*(const QPointF &p, double f) { return QPointF(p.xp * f, p.yp * f); }
    friend constexpr QPointF operator*(double f, const QPointF &p) { return p * f; }
    friend constexpr QPointF operator+(const QPointF &p) { return p; }
    friend constexpr QPointF operator-(const QPointF &p) { return QPointF(-p.xp, -p.yp); }
    friend constexpr QPointF operator/(const QPointF &p, double d) { return QPointF(p.xp / d, p.yp / d); }

private:
    double xp = 0;
    double yp = 0;
};

constexpr QPointF QPoint::toPointF() const noexcept { return QPointF(*this); }
