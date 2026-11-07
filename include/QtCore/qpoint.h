// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QPointF;

class QPoint {
public:
    constexpr QPoint() noexcept = default;
    constexpr QPoint(int xpos, int ypos) noexcept : xp(xpos), yp(ypos) {}

    constexpr bool isNull() const noexcept { return xp == 0 && yp == 0; }
    constexpr int x() const noexcept { return xp; }
    constexpr int y() const noexcept { return yp; }
    constexpr void setX(int x) noexcept { xp = x; }
    constexpr void setY(int y) noexcept { yp = y; }
    constexpr int &rx() noexcept { return xp; }
    constexpr int &ry() noexcept { return yp; }
    constexpr int manhattanLength() const { return qAbs(xp) + qAbs(yp); }
    constexpr QPoint transposed() const noexcept { return QPoint(yp, xp); }
    constexpr QPointF toPointF() const noexcept;

    constexpr QPoint &operator+=(const QPoint &p) { xp += p.xp; yp += p.yp; return *this; }
    constexpr QPoint &operator-=(const QPoint &p) { xp -= p.xp; yp -= p.yp; return *this; }
    constexpr QPoint &operator*=(int f) { xp *= f; yp *= f; return *this; }
    QPoint &operator*=(double f) { xp = qRound(xp * f); yp = qRound(yp * f); return *this; }
    QPoint &operator/=(double d) { xp = qRound(xp / d); yp = qRound(yp / d); return *this; }

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

class QPointF {
public:
    constexpr QPointF() noexcept = default;
    constexpr QPointF(const QPoint &p) noexcept : xp(p.x()), yp(p.y()) {}
    constexpr QPointF(double xpos, double ypos) noexcept : xp(xpos), yp(ypos) {}

    bool isNull() const noexcept { return qFuzzyIsNull(xp) && qFuzzyIsNull(yp); }
    constexpr double x() const noexcept { return xp; }
    constexpr double y() const noexcept { return yp; }
    constexpr void setX(double x) noexcept { xp = x; }
    constexpr void setY(double y) noexcept { yp = y; }
    constexpr double &rx() noexcept { return xp; }
    constexpr double &ry() noexcept { return yp; }
    constexpr double manhattanLength() const { return qAbs(xp) + qAbs(yp); }
    constexpr QPointF transposed() const noexcept { return QPointF(yp, xp); }
    QPoint toPoint() const { return QPoint(qRound(xp), qRound(yp)); }

    constexpr QPointF &operator+=(const QPointF &p) { xp += p.xp; yp += p.yp; return *this; }
    constexpr QPointF &operator-=(const QPointF &p) { xp -= p.xp; yp -= p.yp; return *this; }
    constexpr QPointF &operator*=(double f) { xp *= f; yp *= f; return *this; }
    constexpr QPointF &operator/=(double d) { xp /= d; yp /= d; return *this; }

    static constexpr double dotProduct(const QPointF &a, const QPointF &b) { return a.xp * b.xp + a.yp * b.yp; }

    friend bool operator==(const QPointF &a, const QPointF &b) {
        return (qFuzzyIsNull(a.xp - b.xp) || qFuzzyCompare(a.xp, b.xp)) &&
               (qFuzzyIsNull(a.yp - b.yp) || qFuzzyCompare(a.yp, b.yp));
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
