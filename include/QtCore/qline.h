// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>

class QLine {
public:
    constexpr QLine() noexcept = default;
    constexpr QLine(const QPoint &p1, const QPoint &p2) noexcept : a(p1), b(p2) {}
    constexpr QLine(int x1, int y1, int x2, int y2) noexcept : a(x1, y1), b(x2, y2) {}

    constexpr bool isNull() const noexcept { return a == b; }
    constexpr QPoint p1() const noexcept { return a; }
    constexpr QPoint p2() const noexcept { return b; }
    constexpr int x1() const noexcept { return a.x(); }
    constexpr int y1() const noexcept { return a.y(); }
    constexpr int x2() const noexcept { return b.x(); }
    constexpr int y2() const noexcept { return b.y(); }
    constexpr int dx() const noexcept { return b.x() - a.x(); }
    constexpr int dy() const noexcept { return b.y() - a.y(); }
    constexpr QPoint center() const noexcept {
        return QPoint(static_cast<int>((qint64(a.x()) + b.x()) / 2), static_cast<int>((qint64(a.y()) + b.y()) / 2));
    }
    constexpr void setP1(const QPoint &p) noexcept { a = p; }
    constexpr void setP2(const QPoint &p) noexcept { b = p; }
    constexpr void setPoints(const QPoint &p, const QPoint &q) noexcept { a = p; b = q; }
    constexpr void setLine(int x1, int y1, int x2, int y2) noexcept { a = QPoint(x1, y1); b = QPoint(x2, y2); }
    constexpr void translate(const QPoint &p) noexcept { a += p; b += p; }
    constexpr void translate(int dx, int dy) noexcept { translate(QPoint(dx, dy)); }
    constexpr QLine translated(const QPoint &p) const noexcept { return QLine(a + p, b + p); }
    constexpr QLine translated(int dx, int dy) const noexcept { return translated(QPoint(dx, dy)); }

    friend constexpr bool operator==(const QLine &l, const QLine &r) noexcept { return l.a == r.a && l.b == r.b; }
    friend constexpr bool operator!=(const QLine &l, const QLine &r) noexcept { return !(l == r); }

private:
    QPoint a, b;
};

class QLineF {
public:
    enum IntersectionType { NoIntersection, BoundedIntersection, UnboundedIntersection };

    constexpr QLineF() noexcept = default;
    constexpr QLineF(const QPointF &p1, const QPointF &p2) noexcept : a(p1), b(p2) {}
    constexpr QLineF(double x1, double y1, double x2, double y2) noexcept : a(x1, y1), b(x2, y2) {}
    constexpr QLineF(const QLine &l) noexcept : a(l.p1()), b(l.p2()) {}

    static QLineF fromPolar(double length, double angle) {
        const double r = angle * M_PI / 180.0;
        return QLineF(0, 0, std::cos(r) * length, -std::sin(r) * length);
    }

    bool isNull() const { return a == b; }
    constexpr QPointF p1() const noexcept { return a; }
    constexpr QPointF p2() const noexcept { return b; }
    constexpr double x1() const noexcept { return a.x(); }
    constexpr double y1() const noexcept { return a.y(); }
    constexpr double x2() const noexcept { return b.x(); }
    constexpr double y2() const noexcept { return b.y(); }
    constexpr double dx() const noexcept { return b.x() - a.x(); }
    constexpr double dy() const noexcept { return b.y() - a.y(); }
    constexpr QPointF center() const noexcept { return QPointF(0.5 * a.x() + 0.5 * b.x(), 0.5 * a.y() + 0.5 * b.y()); }
    double length() const { return std::hypot(dx(), dy()); }
    void setLength(double len) {
        const double l = length();
        if (l > 0)
            b = QPointF(a.x() + len * dx() / l, a.y() + len * dy() / l);
    }
    // Degrees counter-clockwise from the positive x axis, in [0, 360).
    double angle() const {
        const double theta = std::atan2(-dy(), dx()) * 180.0 / M_PI;
        const double t = theta < 0 ? theta + 360.0 : theta;
        return t >= 360.0 ? 0.0 : t;
    }
    void setAngle(double angle) {
        const double l = length();
        const double r = angle * M_PI / 180.0;
        b = QPointF(a.x() + std::cos(r) * l, a.y() - std::sin(r) * l);
    }
    double angleTo(const QLineF &o) const {
        if (isNull() || o.isNull())
            return 0;
        const double delta = o.angle() - angle();
        const double n = delta < 0 ? delta + 360.0 : delta;
        return qFuzzyCompare(n, 360.0) ? 0.0 : n;
    }
    QLineF unitVector() const {
        const double l = length();
        return QLineF(a, QPointF(a.x() + dx() / l, a.y() + dy() / l));
    }
    constexpr QLineF normalVector() const noexcept { return QLineF(a, a + QPointF(dy(), -dx())); }
    constexpr QPointF pointAt(double t) const noexcept { return QPointF(a.x() + dx() * t, a.y() + dy() * t); }
    QLine toLine() const { return QLine(a.toPoint(), b.toPoint()); }

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

    constexpr void setP1(const QPointF &p) noexcept { a = p; }
    constexpr void setP2(const QPointF &p) noexcept { b = p; }
    constexpr void setPoints(const QPointF &p, const QPointF &q) noexcept { a = p; b = q; }
    constexpr void setLine(double x1, double y1, double x2, double y2) noexcept { a = QPointF(x1, y1); b = QPointF(x2, y2); }
    constexpr void translate(const QPointF &p) noexcept { a += p; b += p; }
    constexpr void translate(double dx, double dy) noexcept { translate(QPointF(dx, dy)); }
    constexpr QLineF translated(const QPointF &p) const noexcept { return QLineF(a + p, b + p); }
    constexpr QLineF translated(double dx, double dy) const noexcept { return translated(QPointF(dx, dy)); }

    friend bool operator==(const QLineF &l, const QLineF &r) { return l.a == r.a && l.b == r.b; }
    friend bool operator!=(const QLineF &l, const QLineF &r) { return !(l == r); }

private:
    QPointF a, b;
};
