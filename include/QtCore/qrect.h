// MIT License - see LICENSE
#pragma once

#include <QtCore/qmargins.h>
#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>

// A QRect stores inclusive edges: right() is left() + width() - 1.
class QRect {
public:
    constexpr QRect() noexcept = default;
    constexpr QRect(const QPoint &topLeft, const QPoint &bottomRight) noexcept
        : x1(topLeft.x()), y1(topLeft.y()), x2(bottomRight.x()), y2(bottomRight.y()) {}
    constexpr QRect(const QPoint &topLeft, const QSize &s) noexcept
        : x1(topLeft.x()), y1(topLeft.y()), x2(topLeft.x() + s.width() - 1), y2(topLeft.y() + s.height() - 1) {}
    constexpr QRect(int left, int top, int width, int height) noexcept
        : x1(left), y1(top), x2(left + width - 1), y2(top + height - 1) {}

    constexpr bool isNull() const noexcept { return x2 == x1 - 1 && y2 == y1 - 1; }
    constexpr bool isEmpty() const noexcept { return x1 > x2 || y1 > y2; }
    constexpr bool isValid() const noexcept { return x1 <= x2 && y1 <= y2; }

    constexpr int left() const noexcept { return x1; }
    constexpr int top() const noexcept { return y1; }
    constexpr int right() const noexcept { return x2; }
    constexpr int bottom() const noexcept { return y2; }
    constexpr int x() const noexcept { return x1; }
    constexpr int y() const noexcept { return y1; }
    constexpr int width() const noexcept { return x2 - x1 + 1; }
    constexpr int height() const noexcept { return y2 - y1 + 1; }
    constexpr QSize size() const noexcept { return QSize(width(), height()); }
    constexpr QPoint topLeft() const noexcept { return QPoint(x1, y1); }
    constexpr QPoint bottomRight() const noexcept { return QPoint(x2, y2); }
    constexpr QPoint topRight() const noexcept { return QPoint(x2, y1); }
    constexpr QPoint bottomLeft() const noexcept { return QPoint(x1, y2); }
    constexpr QPoint center() const noexcept {
        return QPoint(static_cast<int>((qint64(x1) + x2) / 2), static_cast<int>((qint64(y1) + y2) / 2));
    }

    constexpr void setLeft(int v) noexcept { x1 = v; }
    constexpr void setTop(int v) noexcept { y1 = v; }
    constexpr void setRight(int v) noexcept { x2 = v; }
    constexpr void setBottom(int v) noexcept { y2 = v; }
    constexpr void setX(int v) noexcept { x1 = v; }
    constexpr void setY(int v) noexcept { y1 = v; }
    constexpr void setTopLeft(const QPoint &p) noexcept { x1 = p.x(); y1 = p.y(); }
    constexpr void setBottomRight(const QPoint &p) noexcept { x2 = p.x(); y2 = p.y(); }
    constexpr void setTopRight(const QPoint &p) noexcept { x2 = p.x(); y1 = p.y(); }
    constexpr void setBottomLeft(const QPoint &p) noexcept { x1 = p.x(); y2 = p.y(); }
    constexpr void setWidth(int w) noexcept { x2 = x1 + w - 1; }
    constexpr void setHeight(int h) noexcept { y2 = y1 + h - 1; }
    constexpr void setSize(const QSize &s) noexcept { setWidth(s.width()); setHeight(s.height()); }
    constexpr void setRect(int x, int y, int w, int h) noexcept { x1 = x; y1 = y; x2 = x + w - 1; y2 = y + h - 1; }
    constexpr void setCoords(int xa, int ya, int xb, int yb) noexcept { x1 = xa; y1 = ya; x2 = xb; y2 = yb; }
    constexpr void getRect(int *x, int *y, int *w, int *h) const { *x = x1; *y = y1; *w = width(); *h = height(); }
    constexpr void getCoords(int *xa, int *ya, int *xb, int *yb) const { *xa = x1; *ya = y1; *xb = x2; *yb = y2; }

    constexpr void moveLeft(int p) noexcept { x2 += p - x1; x1 = p; }
    constexpr void moveTop(int p) noexcept { y2 += p - y1; y1 = p; }
    constexpr void moveRight(int p) noexcept { x1 += p - x2; x2 = p; }
    constexpr void moveBottom(int p) noexcept { y1 += p - y2; y2 = p; }
    constexpr void moveTo(int x, int y) noexcept { moveLeft(x); moveTop(y); }
    constexpr void moveTo(const QPoint &p) noexcept { moveTo(p.x(), p.y()); }
    constexpr void moveTopLeft(const QPoint &p) noexcept { moveTo(p); }
    constexpr void moveBottomRight(const QPoint &p) noexcept { moveRight(p.x()); moveBottom(p.y()); }
    constexpr void moveTopRight(const QPoint &p) noexcept { moveRight(p.x()); moveTop(p.y()); }
    constexpr void moveBottomLeft(const QPoint &p) noexcept { moveLeft(p.x()); moveBottom(p.y()); }
    constexpr void moveCenter(const QPoint &p) noexcept {
        const int w = x2 - x1, h = y2 - y1;
        x1 = p.x() - w / 2;
        y1 = p.y() - h / 2;
        x2 = x1 + w;
        y2 = y1 + h;
    }
    constexpr void translate(int dx, int dy) noexcept { x1 += dx; y1 += dy; x2 += dx; y2 += dy; }
    constexpr void translate(const QPoint &p) noexcept { translate(p.x(), p.y()); }
    constexpr QRect translated(int dx, int dy) const noexcept { QRect r = *this; r.translate(dx, dy); return r; }
    constexpr QRect translated(const QPoint &p) const noexcept { return translated(p.x(), p.y()); }
    constexpr QRect transposed() const noexcept { return QRect(topLeft(), size().transposed()); }
    constexpr void adjust(int a, int b, int c, int d) noexcept { x1 += a; y1 += b; x2 += c; y2 += d; }
    constexpr QRect adjusted(int a, int b, int c, int d) const noexcept { QRect r = *this; r.adjust(a, b, c, d); return r; }
    constexpr QRect marginsAdded(const QMargins &m) const noexcept { return adjusted(-m.left(), -m.top(), m.right(), m.bottom()); }
    constexpr QRect marginsRemoved(const QMargins &m) const noexcept { return adjusted(m.left(), m.top(), -m.right(), -m.bottom()); }

    constexpr QRect normalized() const noexcept {
        QRect r = *this;
        if (r.x2 < r.x1 - 1) { const int t = r.x1; r.x1 = r.x2; r.x2 = t; }
        if (r.y2 < r.y1 - 1) { const int t = r.y1; r.y1 = r.y2; r.y2 = t; }
        return r;
    }
    constexpr bool contains(const QPoint &p, bool proper = false) const noexcept {
        const QRect n = normalized();
        if (proper)
            return p.x() > n.x1 && p.x() < n.x2 && p.y() > n.y1 && p.y() < n.y2;
        return p.x() >= n.x1 && p.x() <= n.x2 && p.y() >= n.y1 && p.y() <= n.y2;
    }
    constexpr bool contains(int x, int y, bool proper = false) const noexcept { return contains(QPoint(x, y), proper); }
    constexpr bool contains(const QRect &o, bool proper = false) const noexcept {
        if (isNull() || o.isNull())
            return false;
        const QRect a = normalized(), b = o.normalized();
        if (proper)
            return b.x1 > a.x1 && b.x2 < a.x2 && b.y1 > a.y1 && b.y2 < a.y2;
        return b.x1 >= a.x1 && b.x2 <= a.x2 && b.y1 >= a.y1 && b.y2 <= a.y2;
    }
    constexpr QRect united(const QRect &o) const noexcept {
        if (isNull())
            return o;
        if (o.isNull())
            return *this;
        const QRect a = normalized(), b = o.normalized();
        QRect r;
        r.x1 = qMin(a.x1, b.x1);
        r.x2 = qMax(a.x2, b.x2);
        r.y1 = qMin(a.y1, b.y1);
        r.y2 = qMax(a.y2, b.y2);
        return r;
    }
    constexpr QRect intersected(const QRect &o) const noexcept {
        if (isNull() || o.isNull())
            return QRect();
        const QRect a = normalized(), b = o.normalized();
        if (a.x1 > b.x2 || b.x1 > a.x2 || a.y1 > b.y2 || b.y1 > a.y2)
            return QRect();
        QRect r;
        r.x1 = qMax(a.x1, b.x1);
        r.x2 = qMin(a.x2, b.x2);
        r.y1 = qMax(a.y1, b.y1);
        r.y2 = qMin(a.y2, b.y2);
        return r;
    }
    constexpr bool intersects(const QRect &o) const noexcept { return !intersected(o).isNull(); }

    constexpr QRect operator|(const QRect &o) const noexcept { return united(o); }
    constexpr QRect operator&(const QRect &o) const noexcept { return intersected(o); }
    constexpr QRect &operator|=(const QRect &o) noexcept { return *this = united(o); }
    constexpr QRect &operator&=(const QRect &o) noexcept { return *this = intersected(o); }
    constexpr QRect &operator+=(const QMargins &m) noexcept { return *this = marginsAdded(m); }
    constexpr QRect &operator-=(const QMargins &m) noexcept { return *this = marginsRemoved(m); }

    friend constexpr bool operator==(const QRect &a, const QRect &b) noexcept {
        return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2;
    }
    friend constexpr bool operator!=(const QRect &a, const QRect &b) noexcept { return !(a == b); }
    friend constexpr QRect operator+(const QRect &r, const QMargins &m) noexcept { return r.marginsAdded(m); }
    friend constexpr QRect operator+(const QMargins &m, const QRect &r) noexcept { return r.marginsAdded(m); }
    friend constexpr QRect operator-(const QRect &r, const QMargins &m) noexcept { return r.marginsRemoved(m); }

private:
    int x1 = 0, y1 = 0, x2 = -1, y2 = -1;
};

class QRectF {
public:
    constexpr QRectF() noexcept = default;
    constexpr QRectF(double left, double top, double width, double height) noexcept : xp(left), yp(top), w(width), h(height) {}
    constexpr QRectF(const QPointF &topLeft, const QSizeF &s) noexcept : xp(topLeft.x()), yp(topLeft.y()), w(s.width()), h(s.height()) {}
    constexpr QRectF(const QPointF &topLeft, const QPointF &bottomRight) noexcept
        : xp(topLeft.x()), yp(topLeft.y()), w(bottomRight.x() - topLeft.x()), h(bottomRight.y() - topLeft.y()) {}
    constexpr QRectF(const QRect &r) noexcept : xp(r.x()), yp(r.y()), w(r.width()), h(r.height()) {}

    constexpr bool isNull() const noexcept { return w == 0. && h == 0.; }
    constexpr bool isEmpty() const noexcept { return w <= 0. || h <= 0.; }
    constexpr bool isValid() const noexcept { return w > 0. && h > 0.; }

    constexpr double x() const noexcept { return xp; }
    constexpr double y() const noexcept { return yp; }
    constexpr double width() const noexcept { return w; }
    constexpr double height() const noexcept { return h; }
    constexpr double left() const noexcept { return xp; }
    constexpr double top() const noexcept { return yp; }
    constexpr double right() const noexcept { return xp + w; }
    constexpr double bottom() const noexcept { return yp + h; }
    constexpr QSizeF size() const noexcept { return QSizeF(w, h); }
    constexpr QPointF topLeft() const noexcept { return QPointF(xp, yp); }
    constexpr QPointF bottomRight() const noexcept { return QPointF(xp + w, yp + h); }
    constexpr QPointF topRight() const noexcept { return QPointF(xp + w, yp); }
    constexpr QPointF bottomLeft() const noexcept { return QPointF(xp, yp + h); }
    constexpr QPointF center() const noexcept { return QPointF(xp + w / 2, yp + h / 2); }

    constexpr void setX(double v) noexcept { setLeft(v); }
    constexpr void setY(double v) noexcept { setTop(v); }
    constexpr void setLeft(double v) noexcept { const double d = v - xp; xp += d; w -= d; }
    constexpr void setTop(double v) noexcept { const double d = v - yp; yp += d; h -= d; }
    constexpr void setRight(double v) noexcept { w = v - xp; }
    constexpr void setBottom(double v) noexcept { h = v - yp; }
    constexpr void setWidth(double v) noexcept { w = v; }
    constexpr void setHeight(double v) noexcept { h = v; }
    constexpr void setSize(const QSizeF &s) noexcept { w = s.width(); h = s.height(); }
    constexpr void setRect(double x, double y, double wd, double ht) noexcept { xp = x; yp = y; w = wd; h = ht; }
    constexpr void setCoords(double xa, double ya, double xb, double yb) noexcept { xp = xa; yp = ya; w = xb - xa; h = yb - ya; }

    constexpr void moveLeft(double p) noexcept { xp = p; }
    constexpr void moveTop(double p) noexcept { yp = p; }
    constexpr void moveRight(double p) noexcept { xp = p - w; }
    constexpr void moveBottom(double p) noexcept { yp = p - h; }
    constexpr void moveTo(double x, double y) noexcept { xp = x; yp = y; }
    constexpr void moveTo(const QPointF &p) noexcept { moveTo(p.x(), p.y()); }
    constexpr void moveCenter(const QPointF &p) noexcept { xp = p.x() - w / 2; yp = p.y() - h / 2; }
    constexpr void translate(double dx, double dy) noexcept { xp += dx; yp += dy; }
    constexpr void translate(const QPointF &p) noexcept { translate(p.x(), p.y()); }
    constexpr QRectF translated(double dx, double dy) const noexcept { return QRectF(xp + dx, yp + dy, w, h); }
    constexpr QRectF translated(const QPointF &p) const noexcept { return translated(p.x(), p.y()); }
    constexpr QRectF transposed() const noexcept { return QRectF(topLeft(), size().transposed()); }
    constexpr void adjust(double a, double b, double c, double d) noexcept { xp += a; yp += b; w += c - a; h += d - b; }
    constexpr QRectF adjusted(double a, double b, double c, double d) const noexcept { return QRectF(xp + a, yp + b, w + c - a, h + d - b); }
    constexpr QRectF marginsAdded(const QMarginsF &m) const noexcept { return adjusted(-m.left(), -m.top(), m.right(), m.bottom()); }
    constexpr QRectF marginsRemoved(const QMarginsF &m) const noexcept { return adjusted(m.left(), m.top(), -m.right(), -m.bottom()); }

    constexpr QRectF normalized() const noexcept {
        QRectF r = *this;
        if (r.w < 0) { r.xp += r.w; r.w = -r.w; }
        if (r.h < 0) { r.yp += r.h; r.h = -r.h; }
        return r;
    }
    constexpr bool contains(const QPointF &p) const noexcept {
        const QRectF n = normalized();
        return !isNull() && p.x() >= n.xp && p.x() <= n.xp + n.w && p.y() >= n.yp && p.y() <= n.yp + n.h;
    }
    constexpr bool contains(double x, double y) const noexcept { return contains(QPointF(x, y)); }
    constexpr bool contains(const QRectF &o) const noexcept {
        if (isNull() || o.isNull())
            return false;
        const QRectF a = normalized(), b = o.normalized();
        return b.xp >= a.xp && b.right() <= a.right() && b.yp >= a.yp && b.bottom() <= a.bottom();
    }
    constexpr QRectF united(const QRectF &o) const noexcept {
        if (isNull())
            return o;
        if (o.isNull())
            return *this;
        const QRectF a = normalized(), b = o.normalized();
        const double l = qMin(a.xp, b.xp), t = qMin(a.yp, b.yp);
        return QRectF(l, t, qMax(a.right(), b.right()) - l, qMax(a.bottom(), b.bottom()) - t);
    }
    constexpr QRectF intersected(const QRectF &o) const noexcept {
        if (isNull() || o.isNull())
            return QRectF();
        const QRectF a = normalized(), b = o.normalized();
        const double l = qMax(a.xp, b.xp), t = qMax(a.yp, b.yp);
        const double r = qMin(a.right(), b.right()), bt = qMin(a.bottom(), b.bottom());
        if (l >= r || t >= bt)
            return QRectF();
        return QRectF(l, t, r - l, bt - t);
    }
    constexpr bool intersects(const QRectF &o) const noexcept { return !intersected(o).isNull(); }

    QRect toRect() const {
        const int l = qRound(xp), t = qRound(yp);
        return QRect(l, t, qRound(xp + w) - l, qRound(yp + h) - t);
    }
    QRect toAlignedRect() const {
        const int l = static_cast<int>(std::floor(xp)), t = static_cast<int>(std::floor(yp));
        return QRect(l, t, static_cast<int>(std::ceil(xp + w)) - l, static_cast<int>(std::ceil(yp + h)) - t);
    }

    constexpr QRectF operator|(const QRectF &o) const noexcept { return united(o); }
    constexpr QRectF operator&(const QRectF &o) const noexcept { return intersected(o); }
    constexpr QRectF &operator|=(const QRectF &o) noexcept { return *this = united(o); }
    constexpr QRectF &operator&=(const QRectF &o) noexcept { return *this = intersected(o); }

    friend bool operator==(const QRectF &a, const QRectF &b) {
        return a.topLeft() == b.topLeft() && a.size() == b.size();
    }
    friend bool operator!=(const QRectF &a, const QRectF &b) { return !(a == b); }

private:
    double xp = 0, yp = 0, w = 0, h = 0;
};
