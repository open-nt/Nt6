// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>

/*!
    \class QLine
    \brief The QLine class provides a two-dimensional vector using integer precision.

    A QLine describes a finite line (or line segment) by its two end
    points, stored as QPoint values. Use QLineF for floating-point
    coordinates and for operations such as intersection and angles.

    \differences
    None.
    \enddifferences

    \sa QLineF, QPoint
*/
class QLine {
public:
    /*!
        Constructs a null line, with both end points at (0, 0).

        \sa isNull()
    */
    constexpr QLine() noexcept = default;
    /*!
        Constructs a line object that represents the line between \a p1 and
        \a p2.
    */
    constexpr QLine(const QPoint &p1, const QPoint &p2) noexcept : a(p1), b(p2) {}
    /*!
        Constructs a line object that represents the line between (\a x1,
        \a y1) and (\a x2, \a y2).
    */
    constexpr QLine(int x1, int y1, int x2, int y2) noexcept : a(x1, y1), b(x2, y2) {}

    /*!
        Returns \c true if the line is not set up with valid start and end
        points, that is, if the start point equals the end point; otherwise
        returns \c false.
    */
    constexpr bool isNull() const noexcept { return a == b; }
    /*!
        Returns the line's start point.

        \sa setP1(), x1(), y1(), p2()
    */
    constexpr QPoint p1() const noexcept { return a; }
    /*!
        Returns the line's end point.

        \sa setP2(), x2(), y2(), p1()
    */
    constexpr QPoint p2() const noexcept { return b; }
    /*!
        Returns the x coordinate of the line's start point.

        \sa p1(), y1(), x2()
    */
    constexpr int x1() const noexcept { return a.x(); }
    /*!
        Returns the y coordinate of the line's start point.

        \sa p1(), x1(), y2()
    */
    constexpr int y1() const noexcept { return a.y(); }
    /*!
        Returns the x coordinate of the line's end point.

        \sa p2(), y2(), x1()
    */
    constexpr int x2() const noexcept { return b.x(); }
    /*!
        Returns the y coordinate of the line's end point.

        \sa p2(), x2(), y1()
    */
    constexpr int y2() const noexcept { return b.y(); }
    /*!
        Returns the horizontal component of the line's vector, that is,
        x2() - x1().

        \sa dy()
    */
    constexpr int dx() const noexcept { return b.x() - a.x(); }
    /*!
        Returns the vertical component of the line's vector, that is,
        y2() - y1().

        \sa dx()
    */
    constexpr int dy() const noexcept { return b.y() - a.y(); }
    /*!
        Returns the center point of this line, the average of the two end
        points with each coordinate truncated to an integer.
    */
    constexpr QPoint center() const noexcept {
        return QPoint(static_cast<int>((qint64(a.x()) + b.x()) / 2), static_cast<int>((qint64(a.y()) + b.y()) / 2));
    }
    /*!
        Sets the start point of this line to \a p.

        \sa setP2(), p1()
    */
    constexpr void setP1(const QPoint &p) noexcept { a = p; }
    /*!
        Sets the end point of this line to \a p.

        \sa setP1(), p2()
    */
    constexpr void setP2(const QPoint &p) noexcept { b = p; }
    /*!
        Sets the start point of this line to \a p and the end point to
        \a q.

        \sa setP1(), setP2(), setLine()
    */
    constexpr void setPoints(const QPoint &p, const QPoint &q) noexcept { a = p; b = q; }
    /*!
        Sets this line to the start point (\a x1, \a y1) and end point
        (\a x2, \a y2).

        \sa setP1(), setP2(), setPoints()
    */
    constexpr void setLine(int x1, int y1, int x2, int y2) noexcept { a = QPoint(x1, y1); b = QPoint(x2, y2); }
    /*!
        Translates this line by the given offset \a p.

        \sa translated()
    */
    constexpr void translate(const QPoint &p) noexcept { a += p; b += p; }
    /*!
        \overload

        Translates this line the distance specified by \a dx and \a dy.
    */
    constexpr void translate(int dx, int dy) noexcept { translate(QPoint(dx, dy)); }
    /*!
        Returns this line translated by the given offset \a p.

        \sa translate()
    */
    constexpr QLine translated(const QPoint &p) const noexcept { return QLine(a + p, b + p); }
    /*!
        \overload

        Returns this line translated the distance specified by \a dx and
        \a dy.
    */
    constexpr QLine translated(int dx, int dy) const noexcept { return translated(QPoint(dx, dy)); }

    friend constexpr bool operator==(const QLine &l, const QLine &r) noexcept { return l.a == r.a && l.b == r.b; }
    friend constexpr bool operator!=(const QLine &l, const QLine &r) noexcept { return !(l == r); }

private:
    QPoint a, b;
};

/*!
    \class QLineF
    \brief The QLineF class provides a two-dimensional vector using floating point precision.

    A QLineF describes a finite line (or line segment) by its two end
    points, stored as QPointF values. Angles are measured in degrees,
    counter-clockwise from the positive x axis; since the y axis points
    down, a positive angle turns the line upwards on screen.

    \differences
    Coordinates are always \c double, which is what \c qreal is in Nt.
    \enddifferences

    \sa QLine, QPointF
*/
class QLineF {
public:
    /*!
        \enum QLineF::IntersectionType

        Describes the type of intersection found by intersects().

        \value NoIntersection The lines do not intersect, because they are
               parallel.
        \value BoundedIntersection The lines intersect at a point that lies
               within both line segments.
        \value UnboundedIntersection The lines would intersect if extended
               indefinitely, but the intersection point lies outside at
               least one of the segments.

        \sa intersects()
    */
    enum IntersectionType { NoIntersection, BoundedIntersection, UnboundedIntersection };

    /*!
        Constructs a null line, with both end points at (0, 0).

        \sa isNull()
    */
    constexpr QLineF() noexcept = default;
    /*!
        Constructs a line object that represents the line between \a p1 and
        \a p2.
    */
    constexpr QLineF(const QPointF &p1, const QPointF &p2) noexcept : a(p1), b(p2) {}
    /*!
        Constructs a line object that represents the line between (\a x1,
        \a y1) and (\a x2, \a y2).
    */
    constexpr QLineF(double x1, double y1, double x2, double y2) noexcept : a(x1, y1), b(x2, y2) {}
    /*!
        Construct a QLineF object from the given integer-based line \a l.

        \sa toLine()
    */
    constexpr QLineF(const QLine &l) noexcept : a(l.p1()), b(l.p2()) {}

    /*!
        Returns a QLineF with the given \a length and \a angle, starting at
        the origin.

        The \a angle is in degrees, measured counter-clockwise from the
        positive x axis.

        \sa setLength(), setAngle()
    */
    static QLineF fromPolar(double length, double angle) {
        const double r = angle * M_PI / 180.0;
        return QLineF(0, 0, std::cos(r) * length, -std::sin(r) * length);
    }

    /*!
        Returns \c true if the start point equals the end point; otherwise
        returns \c false.
    */
    bool isNull() const { return a == b; }
    /*!
        Returns the line's start point.

        \sa setP1(), x1(), y1(), p2()
    */
    constexpr QPointF p1() const noexcept { return a; }
    /*!
        Returns the line's end point.

        \sa setP2(), x2(), y2(), p1()
    */
    constexpr QPointF p2() const noexcept { return b; }
    /*!
        Returns the x coordinate of the line's start point.

        \sa p1(), y1(), x2()
    */
    constexpr double x1() const noexcept { return a.x(); }
    /*!
        Returns the y coordinate of the line's start point.

        \sa p1(), x1(), y2()
    */
    constexpr double y1() const noexcept { return a.y(); }
    /*!
        Returns the x coordinate of the line's end point.

        \sa p2(), y2(), x1()
    */
    constexpr double x2() const noexcept { return b.x(); }
    /*!
        Returns the y coordinate of the line's end point.

        \sa p2(), x2(), y1()
    */
    constexpr double y2() const noexcept { return b.y(); }
    /*!
        Returns the horizontal component of the line's vector, that is,
        x2() - x1().

        \sa dy()
    */
    constexpr double dx() const noexcept { return b.x() - a.x(); }
    /*!
        Returns the vertical component of the line's vector, that is,
        y2() - y1().

        \sa dx()
    */
    constexpr double dy() const noexcept { return b.y() - a.y(); }
    /*!
        Returns the center point of this line, the midpoint between the end
        points.
    */
    constexpr QPointF center() const noexcept { return QPointF(0.5 * a.x() + 0.5 * b.x(), 0.5 * a.y() + 0.5 * b.y()); }
    /*!
        Returns the length of the line.

        \sa setLength()
    */
    double length() const { return std::hypot(dx(), dy()); }
    /*!
        Sets the length of the line to the given \a len, keeping the start
        point and the direction. Does nothing if the line has zero length.

        \sa length(), unitVector()
    */
    void setLength(double len) {
        const double l = length();
        if (l > 0)
            b = QPointF(a.x() + len * dx() / l, a.y() + len * dy() / l);
    }
    // Degrees counter-clockwise from the positive x axis, in [0, 360).
    /*!
        Returns the angle of the line in degrees, measured counter-clockwise
        from the positive x axis, in the range [0, 360).

        \sa setAngle(), angleTo()
    */
    double angle() const {
        const double theta = std::atan2(-dy(), dx()) * 180.0 / M_PI;
        const double t = theta < 0 ? theta + 360.0 : theta;
        return t >= 360.0 ? 0.0 : t;
    }
    /*!
        Sets the angle of the line to the given \a angle in degrees,
        measured counter-clockwise from the positive x axis. The start point
        and the length of the line are kept.

        \sa angle(), length()
    */
    void setAngle(double angle) {
        const double l = length();
        const double r = angle * M_PI / 180.0;
        b = QPointF(a.x() + std::cos(r) * l, a.y() - std::sin(r) * l);
    }
    /*!
        Returns the angle in degrees, in the range [0, 360), by which this
        line must be rotated counter-clockwise to have the direction of the
        \a o line. Returns 0 if either line is null.

        \sa angle()
    */
    double angleTo(const QLineF &o) const {
        if (isNull() || o.isNull())
            return 0;
        const double delta = o.angle() - angle();
        const double n = delta < 0 ? delta + 360.0 : delta;
        return qFuzzyCompare(n, 360.0) ? 0.0 : n;
    }
    /*!
        Returns the unit vector for this line, that is, a line starting at
        the same point with a length of 1.0 and the same direction. If this
        line has zero length, it is returned unchanged.

        \sa normalVector(), setLength()
    */
    QLineF unitVector() const {
        const double l = length();
        if (qFuzzyIsNull(l))
            return *this;
        return QLineF(a, QPointF(a.x() + dx() / l, a.y() + dy() / l));
    }
    /*!
        Returns a line that is perpendicular to this line, starts at the
        same point and has the same length.

        \sa unitVector()
    */
    constexpr QLineF normalVector() const noexcept { return QLineF(a, a + QPointF(dy(), -dx())); }
    /*!
        Returns the point at the parameterized position \a t, where 0.0 is
        the start point, 1.0 is the end point, and other values lie on the
        extension of the line.
    */
    constexpr QPointF pointAt(double t) const noexcept { return QPointF(a.x() + dx() * t, a.y() + dy() * t); }
    /*!
        Returns an integer-based copy of this line, with the end points
        rounded to the nearest integer.

        \sa QLineF(const QLine &)
    */
    QLine toLine() const { return QLine(a.toPoint(), b.toPoint()); }

    /*!
        Returns a value indicating whether this line and the line \a l
        intersect, and if so how.

        If the lines are not parallel and \a point is not \c nullptr, it is
        set to the intersection point, even when the intersection is
        unbounded. If the lines are parallel, \a point is left unchanged.

        \sa IntersectionType
    */
    IntersectionType intersects(const QLineF &l, QPointF *point = nullptr) const {
        const QPointF p = QPointF(dx(), dy()), q = QPointF(l.dx(), l.dy());
        const double denom = p.y() * q.x() - p.x() * q.y();
        if (denom == 0.0 || !std::isfinite(denom))
            return NoIntersection;
        const QPointF c = a - l.a;
        const double na = (q.y() * c.x() - q.x() * c.y()) / denom;
        const double nb = (p.x() * c.y() - p.y() * c.x()) / (-denom);
        if (point)
            *point = a + p * na;
        return (na >= 0 && na <= 1 && nb >= 0 && nb <= 1) ? BoundedIntersection : UnboundedIntersection;
    }

    /*!
        Sets the start point of this line to \a p.

        \sa setP2(), p1()
    */
    constexpr void setP1(const QPointF &p) noexcept { a = p; }
    /*!
        Sets the end point of this line to \a p.

        \sa setP1(), p2()
    */
    constexpr void setP2(const QPointF &p) noexcept { b = p; }
    /*!
        Sets the start point of this line to \a p and the end point to
        \a q.

        \sa setP1(), setP2(), setLine()
    */
    constexpr void setPoints(const QPointF &p, const QPointF &q) noexcept { a = p; b = q; }
    /*!
        Sets this line to the start point (\a x1, \a y1) and end point
        (\a x2, \a y2).

        \sa setP1(), setP2(), setPoints()
    */
    constexpr void setLine(double x1, double y1, double x2, double y2) noexcept { a = QPointF(x1, y1); b = QPointF(x2, y2); }
    /*!
        Translates this line by the given offset \a p.

        \sa translated()
    */
    constexpr void translate(const QPointF &p) noexcept { a += p; b += p; }
    /*!
        \overload

        Translates this line the distance specified by \a dx and \a dy.
    */
    constexpr void translate(double dx, double dy) noexcept { translate(QPointF(dx, dy)); }
    /*!
        Returns this line translated by the given offset \a p.

        \sa translate()
    */
    constexpr QLineF translated(const QPointF &p) const noexcept { return QLineF(a + p, b + p); }
    /*!
        \overload

        Returns this line translated the distance specified by \a dx and
        \a dy.
    */
    constexpr QLineF translated(double dx, double dy) const noexcept { return translated(QPointF(dx, dy)); }

    friend bool operator==(const QLineF &l, const QLineF &r) { return l.a == r.a && l.b == r.b; }
    friend bool operator!=(const QLineF &l, const QLineF &r) { return !(l == r); }

private:
    QPointF a, b;
};
