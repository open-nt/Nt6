// MIT License - see LICENSE
#pragma once

#include <QtCore/qmargins.h>
#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>

/*!
    \class QRect
    \brief The QRect class defines a rectangle in the plane using integer precision.

    A rectangle is stored as the coordinates of its top-left and
    bottom-right corners, both inclusive. For historical reasons right() is
    therefore left() + width() - 1 and bottom() is top() + height() - 1.
    Use QRectF for floating-point coordinates, where the right and bottom
    edges are exclusive.

    A rectangle with a width or height less than 1 is empty (see
    isEmpty()); a default-constructed rectangle is null (see isNull()).
    Use normalized() to obtain an equivalent rectangle with non-negative
    width and height.

    \differences
    None.
    \enddifferences

    \sa QRectF, QPoint, QSize, QMargins
*/
class QRectF;
class QRect {
public:
    /*!
        Constructs a null rectangle.

        \sa isNull()
    */
    constexpr QRect() noexcept = default;
    /*!
        Constructs a rectangle with the given \a topLeft and \a bottomRight
        corners, both included in the rectangle.
    */
    constexpr QRect(const QPoint &topLeft, const QPoint &bottomRight) noexcept
        : x1(topLeft.x()), y1(topLeft.y()), x2(bottomRight.x()), y2(bottomRight.y()) {}
    /*!
        Constructs a rectangle with the given \a topLeft corner and the
        given size \a s.
    */
    constexpr QRect(const QPoint &topLeft, const QSize &s) noexcept
        : x1(topLeft.x()), y1(topLeft.y()), x2(topLeft.x() + s.width() - 1), y2(topLeft.y() + s.height() - 1) {}
    /*!
        Constructs a rectangle with \a left, \a top as its top-left corner
        and the given \a width and \a height.
    */
    constexpr QRect(int left, int top, int width, int height) noexcept
        : x1(left), y1(top), x2(left + width - 1), y2(top + height - 1) {}

    /*!
        Returns \c true if the rectangle is a null rectangle, that is, it
        has width 0 and height 0; otherwise returns \c false.

        \sa isEmpty(), isValid()
    */
    constexpr bool isNull() const noexcept { return x2 == x1 - 1 && y2 == y1 - 1; }
    /*!
        Returns \c true if the rectangle is empty, that is, its width or
        height is less than 1; otherwise returns \c false.

        \sa isNull(), isValid()
    */
    constexpr bool isEmpty() const noexcept { return x1 > x2 || y1 > y2; }
    /*!
        Returns \c true if the rectangle is valid, that is, its width and
        height are at least 1; otherwise returns \c false.

        \sa isNull(), isEmpty(), normalized()
    */
    constexpr bool isValid() const noexcept { return x1 <= x2 && y1 <= y2; }

    /*!
        Returns the x coordinate of the rectangle's left edge. Equivalent to
        x().

        \sa setLeft(), right()
    */
    constexpr int left() const noexcept { return x1; }
    /*!
        Returns the y coordinate of the rectangle's top edge. Equivalent to
        y().

        \sa setTop(), bottom()
    */
    constexpr int top() const noexcept { return y1; }
    /*!
        Returns the x coordinate of the rectangle's right edge, which is
        left() + width() - 1.

        \sa setRight(), left()
    */
    constexpr int right() const noexcept { return x2; }
    /*!
        Returns the y coordinate of the rectangle's bottom edge, which is
        top() + height() - 1.

        \sa setBottom(), top()
    */
    constexpr int bottom() const noexcept { return y2; }
    /*!
        Returns the x coordinate of the rectangle's left edge. Equivalent to
        left().

        \sa setX(), y()
    */
    constexpr int x() const noexcept { return x1; }
    /*!
        Returns the y coordinate of the rectangle's top edge. Equivalent to
        top().

        \sa setY(), x()
    */
    constexpr int y() const noexcept { return y1; }
    /*!
        Returns the width of the rectangle.

        \sa setWidth(), height(), size()
    */
    constexpr int width() const noexcept { return x2 - x1 + 1; }
    /*!
        Returns the height of the rectangle.

        \sa setHeight(), width(), size()
    */
    constexpr int height() const noexcept { return y2 - y1 + 1; }
    /*!
        Returns the size of the rectangle.

        \sa setSize(), width(), height()
    */
    constexpr QSize size() const noexcept { return QSize(width(), height()); }
    /*!
        Returns the position of the rectangle's top-left corner.

        \sa setTopLeft(), bottomRight()
    */
    constexpr QPoint topLeft() const noexcept { return QPoint(x1, y1); }
    /*!
        Returns the position of the rectangle's bottom-right corner.

        \sa setBottomRight(), topLeft()
    */
    constexpr QPoint bottomRight() const noexcept { return QPoint(x2, y2); }
    /*!
        Returns the position of the rectangle's top-right corner.

        \sa setTopRight(), bottomLeft()
    */
    constexpr QPoint topRight() const noexcept { return QPoint(x2, y1); }
    /*!
        Returns the position of the rectangle's bottom-left corner.

        \sa setBottomLeft(), topRight()
    */
    constexpr QPoint bottomLeft() const noexcept { return QPoint(x1, y2); }
    /*!
        Returns the center point of the rectangle, with each coordinate
        truncated to an integer.

        \sa moveCenter()
    */
    constexpr QPoint center() const noexcept {
        return QPoint(static_cast<int>((qint64(x1) + x2) / 2), static_cast<int>((qint64(y1) + y2) / 2));
    }

    /*!
        Sets the left edge of the rectangle to \a v, leaving the right edge
        unchanged and so changing the width.

        \sa left(), moveLeft()
    */
    constexpr void setLeft(int v) noexcept { x1 = v; }
    /*!
        Sets the top edge of the rectangle to \a v, leaving the bottom edge
        unchanged and so changing the height.

        \sa top(), moveTop()
    */
    constexpr void setTop(int v) noexcept { y1 = v; }
    /*!
        Sets the right edge of the rectangle to \a v, leaving the left edge
        unchanged and so changing the width.

        \sa right(), moveRight()
    */
    constexpr void setRight(int v) noexcept { x2 = v; }
    /*!
        Sets the bottom edge of the rectangle to \a v, leaving the top edge
        unchanged and so changing the height.

        \sa bottom(), moveBottom()
    */
    constexpr void setBottom(int v) noexcept { y2 = v; }
    /*!
        Sets the left edge of the rectangle to \a v. Equivalent to
        setLeft().

        \sa x(), setY()
    */
    constexpr void setX(int v) noexcept { x1 = v; }
    /*!
        Sets the top edge of the rectangle to \a v. Equivalent to
        setTop().

        \sa y(), setX()
    */
    constexpr void setY(int v) noexcept { y1 = v; }
    /*!
        Sets the top-left corner of the rectangle to \a p, leaving the
        bottom-right corner unchanged.

        \sa topLeft(), moveTopLeft()
    */
    constexpr void setTopLeft(const QPoint &p) noexcept { x1 = p.x(); y1 = p.y(); }
    /*!
        Sets the bottom-right corner of the rectangle to \a p, leaving the
        top-left corner unchanged.

        \sa bottomRight(), moveBottomRight()
    */
    constexpr void setBottomRight(const QPoint &p) noexcept { x2 = p.x(); y2 = p.y(); }
    /*!
        Sets the top-right corner of the rectangle to \a p, leaving the
        bottom-left corner unchanged.

        \sa topRight(), moveTopRight()
    */
    constexpr void setTopRight(const QPoint &p) noexcept { x2 = p.x(); y1 = p.y(); }
    /*!
        Sets the bottom-left corner of the rectangle to \a p, leaving the
        top-right corner unchanged.

        \sa bottomLeft(), moveBottomLeft()
    */
    constexpr void setBottomLeft(const QPoint &p) noexcept { x1 = p.x(); y2 = p.y(); }
    /*!
        Sets the width of the rectangle to \a w, moving the right edge and
        keeping the left edge.

        \sa width(), setHeight()
    */
    constexpr void setWidth(int w) noexcept { x2 = x1 + w - 1; }
    /*!
        Sets the height of the rectangle to \a h, moving the bottom edge and
        keeping the top edge.

        \sa height(), setWidth()
    */
    constexpr void setHeight(int h) noexcept { y2 = y1 + h - 1; }
    /*!
        Sets the size of the rectangle to \a s, keeping the top-left corner.

        \sa size()
    */
    constexpr void setSize(const QSize &s) noexcept { setWidth(s.width()); setHeight(s.height()); }
    /*!
        Sets the coordinates of the rectangle's top-left corner to (\a x,
        \a y), and its size to the given \a w and \a h.

        \sa getRect(), setCoords()
    */
    constexpr void setRect(int x, int y, int w, int h) noexcept { x1 = x; y1 = y; x2 = x + w - 1; y2 = y + h - 1; }
    /*!
        Sets the coordinates of the rectangle's top-left corner to (\a xa,
        \a ya), and of its bottom-right corner to (\a xb, \a yb).

        \sa getCoords(), setRect()
    */
    constexpr void setCoords(int xa, int ya, int xb, int yb) noexcept { x1 = xa; y1 = ya; x2 = xb; y2 = yb; }
    /*!
        Extracts the position of the rectangle's top-left corner to \a x and
        \a y, and its dimensions to \a w and \a h.

        \sa setRect(), getCoords()
    */
    constexpr void getRect(int *x, int *y, int *w, int *h) const { *x = x1; *y = y1; *w = width(); *h = height(); }
    /*!
        Extracts the position of the rectangle's top-left corner to \a xa
        and \a ya, and the position of the bottom-right corner to \a xb and
        \a yb.

        \sa setCoords(), getRect()
    */
    constexpr void getCoords(int *xa, int *ya, int *xb, int *yb) const { *xa = x1; *ya = y1; *xb = x2; *yb = y2; }

    /*!
        Moves the rectangle horizontally, leaving the rectangle's left edge
        at the given \a p. The rectangle's size is unchanged.

        \sa left(), moveRight()
    */
    constexpr void moveLeft(int p) noexcept { x2 += p - x1; x1 = p; }
    /*!
        Moves the rectangle vertically, leaving the rectangle's top edge at
        the given \a p. The rectangle's size is unchanged.

        \sa top(), moveBottom()
    */
    constexpr void moveTop(int p) noexcept { y2 += p - y1; y1 = p; }
    /*!
        Moves the rectangle horizontally, leaving the rectangle's right edge
        at the given \a p. The rectangle's size is unchanged.

        \sa right(), moveLeft()
    */
    constexpr void moveRight(int p) noexcept { x1 += p - x2; x2 = p; }
    /*!
        Moves the rectangle vertically, leaving the rectangle's bottom edge
        at the given \a p. The rectangle's size is unchanged.

        \sa bottom(), moveTop()
    */
    constexpr void moveBottom(int p) noexcept { y1 += p - y2; y2 = p; }
    /*!
        Moves the rectangle, leaving the top-left corner at the given
        position (\a x, \a y). The rectangle's size is unchanged.

        \sa translate(), moveTopLeft()
    */
    constexpr void moveTo(int x, int y) noexcept { moveLeft(x); moveTop(y); }
    /*!
        \overload

        Moves the rectangle, leaving the top-left corner at the given
        position \a p.
    */
    constexpr void moveTo(const QPoint &p) noexcept { moveTo(p.x(), p.y()); }
    /*!
        Moves the rectangle, leaving the top-left corner at the given
        position \a p. The rectangle's size is unchanged.

        \sa moveTo(), setTopLeft()
    */
    constexpr void moveTopLeft(const QPoint &p) noexcept { moveTo(p); }
    /*!
        Moves the rectangle, leaving the bottom-right corner at the given
        position \a p. The rectangle's size is unchanged.

        \sa setBottomRight()
    */
    constexpr void moveBottomRight(const QPoint &p) noexcept { moveRight(p.x()); moveBottom(p.y()); }
    /*!
        Moves the rectangle, leaving the top-right corner at the given
        position \a p. The rectangle's size is unchanged.

        \sa setTopRight()
    */
    constexpr void moveTopRight(const QPoint &p) noexcept { moveRight(p.x()); moveTop(p.y()); }
    /*!
        Moves the rectangle, leaving the bottom-left corner at the given
        position \a p. The rectangle's size is unchanged.

        \sa setBottomLeft()
    */
    constexpr void moveBottomLeft(const QPoint &p) noexcept { moveLeft(p.x()); moveBottom(p.y()); }
    /*!
        Moves the rectangle, leaving the center point at the given position
        \a p. The rectangle's size is unchanged.

        \sa center()
    */
    constexpr void moveCenter(const QPoint &p) noexcept {
        const int w = x2 - x1, h = y2 - y1;
        x1 = p.x() - w / 2;
        y1 = p.y() - h / 2;
        x2 = x1 + w;
        y2 = y1 + h;
    }
    /*!
        Moves the rectangle \a dx along the x axis and \a dy along the y
        axis, relative to the current position.

        \sa translated(), moveTo()
    */
    constexpr void translate(int dx, int dy) noexcept { x1 += dx; y1 += dy; x2 += dx; y2 += dy; }
    /*!
        \overload

        Moves the rectangle by the offset \a p, relative to the current
        position.
    */
    constexpr void translate(const QPoint &p) noexcept { translate(p.x(), p.y()); }
    /*!
        Returns a copy of the rectangle that is translated \a dx along the x
        axis and \a dy along the y axis.

        \sa translate()
    */
    constexpr QRect translated(int dx, int dy) const noexcept { QRect r = *this; r.translate(dx, dy); return r; }
    /*!
        \overload

        Returns a copy of the rectangle that is translated by the offset
        \a p.
    */
    constexpr QRect translated(const QPoint &p) const noexcept { return translated(p.x(), p.y()); }
    /*!
        Returns a copy of the rectangle with its width and height
        exchanged, keeping the top-left corner.

        \sa QSize::transposed()
    */
    constexpr QRect transposed() const noexcept { return QRect(topLeft(), size().transposed()); }
    /*!
        Adds \a a, \a b, \a c and \a d to the left, top, right and bottom
        coordinates of the rectangle respectively.

        \sa adjusted(), setRect()
    */
    constexpr void adjust(int a, int b, int c, int d) noexcept { x1 += a; y1 += b; x2 += c; y2 += d; }
    /*!
        Returns a copy of the rectangle with \a a, \a b, \a c and \a d added
        to the left, top, right and bottom coordinates respectively.

        \sa adjust()
    */
    constexpr QRect adjusted(int a, int b, int c, int d) const noexcept { QRect r = *this; r.adjust(a, b, c, d); return r; }
    /*!
        Returns a rectangle grown by the margins \a m: the left and top
        edges move outwards by the left and top margins, the right and
        bottom edges by the right and bottom margins.

        \sa marginsRemoved()
    */
    constexpr QRect marginsAdded(const QMargins &m) const noexcept { return adjusted(-m.left(), -m.top(), m.right(), m.bottom()); }
    /*!
        Returns a rectangle shrunk by the margins \a m.

        \sa marginsAdded()
    */
    constexpr QRect marginsRemoved(const QMargins &m) const noexcept { return adjusted(m.left(), m.top(), -m.right(), -m.bottom()); }

    /*!
        Returns a normalized rectangle, that is, one that has a
        non-negative width and height. If the width or height is negative,
        the corresponding edges are swapped.

        \sa isValid()
    */
    constexpr QRect normalized() const noexcept {
        QRect r = *this;
        if (r.x2 < r.x1 - 1) { const int t = r.x1; r.x1 = r.x2; r.x2 = t; }
        if (r.y2 < r.y1 - 1) { const int t = r.y1; r.y1 = r.y2; r.y2 = t; }
        return r;
    }
    /*!
        Returns \c true if the given point \a p is inside or on the edge of
        the rectangle; otherwise returns \c false. If \a proper is \c true,
        the point must be strictly inside the rectangle, not on its edge.

        The rectangle is normalized before the test.

        \sa intersects()
    */
    constexpr bool contains(const QPoint &p, bool proper = false) const noexcept {
        const QRect n = normalized();
        if (proper)
            return p.x() > n.x1 && p.x() < n.x2 && p.y() > n.y1 && p.y() < n.y2;
        return p.x() >= n.x1 && p.x() <= n.x2 && p.y() >= n.y1 && p.y() <= n.y2;
    }
    /*!
        \overload

        Returns \c true if the point (\a x, \a y) is inside this rectangle,
        or on its edge unless \a proper is \c true.
    */
    constexpr bool contains(int x, int y, bool proper = false) const noexcept { return contains(QPoint(x, y), proper); }
    /*!
        \overload

        Returns \c true if the rectangle \a o is inside this rectangle;
        otherwise returns \c false. If \a proper is \c true, \a o must not
        touch the edges of this rectangle. A null rectangle never contains,
        and is never contained by, another rectangle.
    */
    constexpr bool contains(const QRect &o, bool proper = false) const noexcept {
        if (isNull() || o.isNull())
            return false;
        const QRect a = normalized(), b = o.normalized();
        if (proper)
            return b.x1 > a.x1 && b.x2 < a.x2 && b.y1 > a.y1 && b.y2 < a.y2;
        return b.x1 >= a.x1 && b.x2 <= a.x2 && b.y1 >= a.y1 && b.y2 <= a.y2;
    }
    /*!
        Returns the bounding rectangle of this rectangle and the rectangle
        \a o. If one of the rectangles is null, the other is returned.

        \sa intersected()
    */
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
    /*!
        Returns the intersection of this rectangle and the rectangle \a o.
        Returns a null rectangle if they do not intersect.

        \sa intersects(), united()
    */
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
    /*!
        Returns \c true if this rectangle intersects the rectangle \a o,
        that is, if there is at least one pixel that is within both
        rectangles; otherwise returns \c false.

        \sa intersected(), contains()
    */
    constexpr bool intersects(const QRect &o) const noexcept { return !intersected(o).isNull(); }

    /*!
        Returns the bounding rectangle of this rectangle and the rectangle
        \a o. Equivalent to united().
    */
    constexpr QRect operator|(const QRect &o) const noexcept { return united(o); }
    /*!
        Returns the intersection of this rectangle and the rectangle \a o.
        Equivalent to intersected().
    */
    constexpr QRect operator&(const QRect &o) const noexcept { return intersected(o); }
    /*!
        Sets this rectangle to the bounding rectangle of itself and the
        rectangle \a o, and returns a reference to this rectangle.
    */
    constexpr QRect &operator|=(const QRect &o) noexcept { return *this = united(o); }
    /*!
        Sets this rectangle to its intersection with the rectangle \a o, and
        returns a reference to this rectangle.
    */
    constexpr QRect &operator&=(const QRect &o) noexcept { return *this = intersected(o); }
    /*!
        Adds the margins \a m to this rectangle, as marginsAdded() does, and
        returns a reference to this rectangle.
    */
    constexpr QRect &operator+=(const QMargins &m) noexcept { return *this = marginsAdded(m); }
    /*!
        Removes the margins \a m from this rectangle, as marginsRemoved()
        does, and returns a reference to this rectangle.
    */
    constexpr QRect &operator-=(const QMargins &m) noexcept { return *this = marginsRemoved(m); }

    friend constexpr bool operator==(const QRect &a, const QRect &b) noexcept {
        return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2;
    }
    friend constexpr bool operator!=(const QRect &a, const QRect &b) noexcept { return !(a == b); }
    friend constexpr QRect operator+(const QRect &r, const QMargins &m) noexcept { return r.marginsAdded(m); }
    friend constexpr QRect operator+(const QMargins &m, const QRect &r) noexcept { return r.marginsAdded(m); }
    friend constexpr QRect operator-(const QRect &r, const QMargins &m) noexcept { return r.marginsRemoved(m); }
    /*!
        \since 6.4
        Returns this rectangle as a rectangle with floating point accuracy.

        \sa QRectF::toRect()
    */
    constexpr QRectF toRectF() const noexcept;
    /*!
        \since 6.0
        Returns a rectangle that spans the two points \a p1 and \a p2,
        including both points.
    */
    static constexpr QRect span(const QPoint &p1, const QPoint &p2) noexcept {
        return QRect(QPoint(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y())),
                     QPoint(std::max(p1.x(), p2.x()), std::max(p1.y(), p2.y())));
    }

private:
    int x1 = 0, y1 = 0, x2 = -1, y2 = -1;
};

/*!
    \class QRectF
    \brief The QRectF class defines a rectangle in the plane using floating point precision.

    A rectangle is stored as the position of its top-left corner and its
    width and height, all as \c double. Unlike QRect, the right and bottom
    edges are exclusive: right() is left() + width() and bottom() is top() +
    height(). Use QRect for integer coordinates.

    A rectangle is valid when both its width and height are greater than 0.
    Use normalized() to obtain an equivalent rectangle with non-negative
    width and height.

    \differences
    Coordinates are always \c double, which is what \c qreal is in Nt.
    \enddifferences

    \sa QRect, QPointF, QSizeF, QMarginsF
*/
class QRectF {
public:
    /*!
        Constructs a null rectangle, with zero width and height.

        \sa isNull()
    */
    constexpr QRectF() noexcept = default;
    /*!
        Constructs a rectangle with \a left, \a top as its top-left corner
        and the given \a width and \a height.
    */
    constexpr QRectF(double left, double top, double width, double height) noexcept : xp(left), yp(top), w(width), h(height) {}
    /*!
        Constructs a rectangle with the given \a topLeft corner and the
        given size \a s.
    */
    constexpr QRectF(const QPointF &topLeft, const QSizeF &s) noexcept : xp(topLeft.x()), yp(topLeft.y()), w(s.width()), h(s.height()) {}
    /*!
        Constructs a rectangle with the given \a topLeft and \a bottomRight
        corners.
    */
    constexpr QRectF(const QPointF &topLeft, const QPointF &bottomRight) noexcept
        : xp(topLeft.x()), yp(topLeft.y()), w(bottomRight.x() - topLeft.x()), h(bottomRight.y() - topLeft.y()) {}
    /*!
        Constructs a QRectF rectangle from the given integer rectangle
        \a r. The right and bottom edges of the result are exclusive, so
        its width and height equal those of \a r.

        \sa toRect()
    */
    constexpr QRectF(const QRect &r) noexcept : xp(r.x()), yp(r.y()), w(r.width()), h(r.height()) {}

    /*!
        Returns \c true if the rectangle is a null rectangle, that is, its
        width and height are both exactly 0; otherwise returns \c false.

        \sa isEmpty(), isValid()
    */
    constexpr bool isNull() const noexcept { return w == 0. && h == 0.; }
    /*!
        Returns \c true if the rectangle is empty, that is, its width or
        height is less than or equal to 0; otherwise returns \c false.

        \sa isNull(), isValid()
    */
    constexpr bool isEmpty() const noexcept { return w <= 0. || h <= 0.; }
    /*!
        Returns \c true if the rectangle is valid, that is, its width and
        height are both greater than 0; otherwise returns \c false.

        \sa isNull(), isEmpty(), normalized()
    */
    constexpr bool isValid() const noexcept { return w > 0. && h > 0.; }

    /*!
        Returns the x coordinate of the rectangle's left edge. Equivalent to
        left().

        \sa setX(), y()
    */
    constexpr double x() const noexcept { return xp; }
    /*!
        Returns the y coordinate of the rectangle's top edge. Equivalent to
        top().

        \sa setY(), x()
    */
    constexpr double y() const noexcept { return yp; }
    /*!
        Returns the width of the rectangle.

        \sa setWidth(), height()
    */
    constexpr double width() const noexcept { return w; }
    /*!
        Returns the height of the rectangle.

        \sa setHeight(), width()
    */
    constexpr double height() const noexcept { return h; }
    /*!
        Returns the x coordinate of the rectangle's left edge. Equivalent to
        x().

        \sa setLeft(), right()
    */
    constexpr double left() const noexcept { return xp; }
    /*!
        Returns the y coordinate of the rectangle's top edge. Equivalent to
        y().

        \sa setTop(), bottom()
    */
    constexpr double top() const noexcept { return yp; }
    /*!
        Returns the x coordinate of the rectangle's right edge, which is
        left() + width().

        \sa setRight(), left()
    */
    constexpr double right() const noexcept { return xp + w; }
    /*!
        Returns the y coordinate of the rectangle's bottom edge, which is
        top() + height().

        \sa setBottom(), top()
    */
    constexpr double bottom() const noexcept { return yp + h; }
    /*!
        Returns the size of the rectangle.

        \sa setSize(), width(), height()
    */
    constexpr QSizeF size() const noexcept { return QSizeF(w, h); }
    /*!
        Returns the position of the rectangle's top-left corner.

        \sa bottomRight()
    */
    constexpr QPointF topLeft() const noexcept { return QPointF(xp, yp); }
    /*!
        Returns the position of the rectangle's bottom-right corner.

        \sa topLeft()
    */
    constexpr QPointF bottomRight() const noexcept { return QPointF(xp + w, yp + h); }
    /*!
        Returns the position of the rectangle's top-right corner.

        \sa bottomLeft()
    */
    constexpr QPointF topRight() const noexcept { return QPointF(xp + w, yp); }
    /*!
        Returns the position of the rectangle's bottom-left corner.

        \sa topRight()
    */
    constexpr QPointF bottomLeft() const noexcept { return QPointF(xp, yp + h); }
    /*!
        Returns the center point of the rectangle.

        \sa moveCenter()
    */
    constexpr QPointF center() const noexcept { return QPointF(xp + w / 2, yp + h / 2); }

    /*!
        Sets the left edge of the rectangle to \a v, leaving the right edge
        unchanged and so changing the width. Equivalent to setLeft().

        \sa x(), setY()
    */
    constexpr void setX(double v) noexcept { setLeft(v); }
    /*!
        Sets the top edge of the rectangle to \a v, leaving the bottom edge
        unchanged and so changing the height. Equivalent to setTop().

        \sa y(), setX()
    */
    constexpr void setY(double v) noexcept { setTop(v); }
    /*!
        Sets the left edge of the rectangle to \a v, leaving the right edge
        unchanged and so changing the width.

        \sa left(), moveLeft()
    */
    constexpr void setLeft(double v) noexcept { const double d = v - xp; xp += d; w -= d; }
    /*!
        Sets the top edge of the rectangle to \a v, leaving the bottom edge
        unchanged and so changing the height.

        \sa top(), moveTop()
    */
    constexpr void setTop(double v) noexcept { const double d = v - yp; yp += d; h -= d; }
    /*!
        Sets the right edge of the rectangle to \a v, leaving the left edge
        unchanged and so changing the width.

        \sa right(), moveRight()
    */
    constexpr void setRight(double v) noexcept { w = v - xp; }
    /*!
        Sets the bottom edge of the rectangle to \a v, leaving the top edge
        unchanged and so changing the height.

        \sa bottom(), moveBottom()
    */
    constexpr void setBottom(double v) noexcept { h = v - yp; }
    /*!
        Sets the top-left corner to \a p, keeping the bottom-right corner.

        \sa topLeft(), moveTopLeft()
    */
    constexpr void setTopLeft(const QPointF &p) noexcept {
        setLeft(p.x());
        setTop(p.y());
    }
    /*!
        Sets the bottom-right corner to \a p, keeping the top-left corner.

        \sa bottomRight(), moveBottomRight()
    */
    constexpr void setBottomRight(const QPointF &p) noexcept {
        setRight(p.x());
        setBottom(p.y());
    }
    /*!
        Sets the top-right corner to \a p, keeping the bottom-left corner.

        \sa topRight(), moveTopRight()
    */
    constexpr void setTopRight(const QPointF &p) noexcept {
        setRight(p.x());
        setTop(p.y());
    }
    /*!
        Sets the bottom-left corner to \a p, keeping the top-right corner.

        \sa bottomLeft(), moveBottomLeft()
    */
    constexpr void setBottomLeft(const QPointF &p) noexcept {
        setLeft(p.x());
        setBottom(p.y());
    }
    /*!
        Stores the position of the top-left corner in \c{*x} and \c{*y} and the
        size in \c{*width} and \c{*height}, through \a x, \a y, \a width and
        \a height.

        \sa getCoords()
    */
    constexpr void getRect(double *x, double *y, double *width, double *height) const {
        *x = xp;
        *y = yp;
        *width = w;
        *height = h;
    }
    /*!
        Stores the top-left corner in \c{*x1} and \c{*y1} and the bottom-right
        corner in \c{*x2} and \c{*y2}, through \a x1, \a y1, \a x2 and \a y2.

        \sa getRect()
    */
    constexpr void getCoords(double *x1, double *y1, double *x2, double *y2) const {
        *x1 = xp;
        *y1 = yp;
        *x2 = xp + w;
        *y2 = yp + h;
    }
    /*!
        Sets the width of the rectangle to \a v, keeping the left edge.

        \sa width(), setHeight()
    */
    constexpr void setWidth(double v) noexcept { w = v; }
    /*!
        Sets the height of the rectangle to \a v, keeping the top edge.

        \sa height(), setWidth()
    */
    constexpr void setHeight(double v) noexcept { h = v; }
    /*!
        Sets the size of the rectangle to \a s, keeping the top-left corner.

        \sa size()
    */
    constexpr void setSize(const QSizeF &s) noexcept { w = s.width(); h = s.height(); }
    /*!
        Sets the coordinates of the rectangle's top-left corner to (\a x,
        \a y), and its size to the given \a wd and \a ht.

        \sa setCoords()
    */
    constexpr void setRect(double x, double y, double wd, double ht) noexcept { xp = x; yp = y; w = wd; h = ht; }
    /*!
        Sets the coordinates of the rectangle's top-left corner to (\a xa,
        \a ya), and of its bottom-right corner to (\a xb, \a yb).

        \sa setRect()
    */
    constexpr void setCoords(double xa, double ya, double xb, double yb) noexcept { xp = xa; yp = ya; w = xb - xa; h = yb - ya; }

    /*!
        Moves the rectangle horizontally, leaving the rectangle's left edge
        at the given \a p. The rectangle's size is unchanged.

        \sa left(), moveRight()
    */
    constexpr void moveLeft(double p) noexcept { xp = p; }
    /*!
        Moves the rectangle vertically, leaving the rectangle's top edge at
        the given \a p. The rectangle's size is unchanged.

        \sa top(), moveBottom()
    */
    constexpr void moveTop(double p) noexcept { yp = p; }
    /*!
        Moves the rectangle horizontally, leaving the rectangle's right edge
        at the given \a p. The rectangle's size is unchanged.

        \sa right(), moveLeft()
    */
    constexpr void moveRight(double p) noexcept { xp = p - w; }
    /*!
        Moves the rectangle so that its top-left corner is at \a p, keeping
        its size.
    */
    constexpr void moveTopLeft(const QPointF &p) noexcept {
        moveLeft(p.x());
        moveTop(p.y());
    }
    /*!
        Moves the rectangle so that its bottom-right corner is at \a p,
        keeping its size.
    */
    constexpr void moveBottomRight(const QPointF &p) noexcept {
        moveRight(p.x());
        yp = p.y() - h;
    }
    /*!
        Moves the rectangle so that its top-right corner is at \a p, keeping
        its size.
    */
    constexpr void moveTopRight(const QPointF &p) noexcept {
        moveRight(p.x());
        moveTop(p.y());
    }
    /*!
        Moves the rectangle so that its bottom-left corner is at \a p,
        keeping its size.
    */
    constexpr void moveBottomLeft(const QPointF &p) noexcept {
        moveLeft(p.x());
        yp = p.y() - h;
    }
    /*!
        Moves the rectangle vertically, leaving the rectangle's bottom edge
        at the given \a p. The rectangle's size is unchanged.

        \sa bottom(), moveTop()
    */
    constexpr void moveBottom(double p) noexcept { yp = p - h; }
    /*!
        Moves the rectangle, leaving the top-left corner at the given
        position (\a x, \a y). The rectangle's size is unchanged.

        \sa translate()
    */
    constexpr void moveTo(double x, double y) noexcept { xp = x; yp = y; }
    /*!
        \overload

        Moves the rectangle, leaving the top-left corner at the given
        position \a p.
    */
    constexpr void moveTo(const QPointF &p) noexcept { moveTo(p.x(), p.y()); }
    /*!
        Moves the rectangle, leaving the center point at the given position
        \a p. The rectangle's size is unchanged.

        \sa center()
    */
    constexpr void moveCenter(const QPointF &p) noexcept { xp = p.x() - w / 2; yp = p.y() - h / 2; }
    /*!
        Moves the rectangle \a dx along the x axis and \a dy along the y
        axis, relative to the current position.

        \sa translated(), moveTo()
    */
    constexpr void translate(double dx, double dy) noexcept { xp += dx; yp += dy; }
    /*!
        \overload

        Moves the rectangle by the offset \a p, relative to the current
        position.
    */
    constexpr void translate(const QPointF &p) noexcept { translate(p.x(), p.y()); }
    /*!
        Returns a copy of the rectangle that is translated \a dx along the x
        axis and \a dy along the y axis.

        \sa translate()
    */
    constexpr QRectF translated(double dx, double dy) const noexcept { return QRectF(xp + dx, yp + dy, w, h); }
    /*!
        \overload

        Returns a copy of the rectangle that is translated by the offset
        \a p.
    */
    constexpr QRectF translated(const QPointF &p) const noexcept { return translated(p.x(), p.y()); }
    /*!
        Returns a copy of the rectangle with its width and height
        exchanged, keeping the top-left corner.

        \sa QSizeF::transposed()
    */
    constexpr QRectF transposed() const noexcept { return QRectF(topLeft(), size().transposed()); }
    /*!
        Adds \a a, \a b, \a c and \a d to the left, top, right and bottom
        coordinates of the rectangle respectively.

        \sa adjusted(), setRect()
    */
    constexpr void adjust(double a, double b, double c, double d) noexcept { xp += a; yp += b; w += c - a; h += d - b; }
    /*!
        Returns a copy of the rectangle with \a a, \a b, \a c and \a d added
        to the left, top, right and bottom coordinates respectively.

        \sa adjust()
    */
    constexpr QRectF adjusted(double a, double b, double c, double d) const noexcept { return QRectF(xp + a, yp + b, w + c - a, h + d - b); }
    /*!
        Returns a rectangle grown by the margins \a m: the left and top
        edges move outwards by the left and top margins, the right and
        bottom edges by the right and bottom margins.

        \sa marginsRemoved()
    */
    constexpr QRectF marginsAdded(const QMarginsF &m) const noexcept { return adjusted(-m.left(), -m.top(), m.right(), m.bottom()); }
    /*!
        Returns a rectangle shrunk by the margins \a m.

        \sa marginsAdded()
    */
    constexpr QRectF marginsRemoved(const QMarginsF &m) const noexcept { return adjusted(m.left(), m.top(), -m.right(), -m.bottom()); }

    /*!
        Returns a normalized rectangle, that is, one that has a
        non-negative width and height. If the width or height is negative,
        the rectangle is moved so that it covers the same area with a
        positive extent.

        \sa isValid()
    */
    constexpr QRectF normalized() const noexcept {
        QRectF r = *this;
        if (r.w < 0) { r.xp += r.w; r.w = -r.w; }
        if (r.h < 0) { r.yp += r.h; r.h = -r.h; }
        return r;
    }
    /*!
        Returns \c true if the given point \a p is inside or on the edge of
        the rectangle; otherwise returns \c false. A rectangle with zero
        width or height contains nothing.

        \sa intersects()
    */
    constexpr bool contains(const QPointF &p) const noexcept {
        if (w == 0. || h == 0.)
            return false;
        const QRectF n = normalized();
        return p.x() >= n.xp && p.x() <= n.xp + n.w && p.y() >= n.yp && p.y() <= n.yp + n.h;
    }
    /*!
        \overload

        Returns \c true if the point (\a x, \a y) is inside or on the edge
        of this rectangle.
    */
    constexpr bool contains(double x, double y) const noexcept { return contains(QPointF(x, y)); }
    /*!
        \overload

        Returns \c true if the rectangle \a o is inside this rectangle;
        otherwise returns \c false. A rectangle with zero width or height
        never contains, and is never contained by, another rectangle.
    */
    constexpr bool contains(const QRectF &o) const noexcept {
        if (w == 0. || h == 0. || o.w == 0. || o.h == 0.)
            return false;
        const QRectF a = normalized(), b = o.normalized();
        return b.xp >= a.xp && b.right() <= a.right() && b.yp >= a.yp && b.bottom() <= a.bottom();
    }
    /*!
        Returns the bounding rectangle of this rectangle and the rectangle
        \a o. If one of the rectangles is null, the other is returned.

        \sa intersected()
    */
    constexpr QRectF united(const QRectF &o) const noexcept {
        if (isNull())
            return o;
        if (o.isNull())
            return *this;
        const QRectF a = normalized(), b = o.normalized();
        const double l = qMin(a.xp, b.xp), t = qMin(a.yp, b.yp);
        return QRectF(l, t, qMax(a.right(), b.right()) - l, qMax(a.bottom(), b.bottom()) - t);
    }
    /*!
        Returns the intersection of this rectangle and the rectangle \a o.
        Returns a null rectangle if they do not intersect.

        \sa intersects(), united()
    */
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
    /*!
        Returns \c true if this rectangle intersects the rectangle \a o,
        that is, if they share an area of non-zero size; otherwise returns
        \c false.

        \sa intersected(), contains()
    */
    constexpr bool intersects(const QRectF &o) const noexcept { return !intersected(o).isNull(); }

    /*!
        Returns a QRect with the coordinates of the top-left and
        bottom-right corners each rounded to the nearest integer.

        \sa toAlignedRect(), QRectF(const QRect &)
    */
    QRect toRect() const {
        const int l = qRound(xp), t = qRound(yp);
        return QRect(l, t, qRound(xp + w) - l, qRound(yp + h) - t);
    }
    /*!
        Returns the smallest QRect that contains this rectangle, rounding
        the left and top edges down and the right and bottom edges up.

        \sa toRect()
    */
    QRect toAlignedRect() const {
        const int l = static_cast<int>(std::floor(xp)), t = static_cast<int>(std::floor(yp));
        return QRect(l, t, static_cast<int>(std::ceil(xp + w)) - l, static_cast<int>(std::ceil(yp + h)) - t);
    }

    /*!
        Returns the bounding rectangle of this rectangle and the rectangle
        \a o. Equivalent to united().
    */
    constexpr QRectF operator|(const QRectF &o) const noexcept { return united(o); }
    /*!
        Returns the intersection of this rectangle and the rectangle \a o.
        Equivalent to intersected().
    */
    constexpr QRectF operator&(const QRectF &o) const noexcept { return intersected(o); }
    /*!
        Sets this rectangle to the bounding rectangle of itself and the
        rectangle \a o, and returns a reference to this rectangle.
    */
    constexpr QRectF &operator|=(const QRectF &o) noexcept { return *this = united(o); }
    /*!
        Sets this rectangle to its intersection with the rectangle \a o, and
        returns a reference to this rectangle.
    */
    constexpr QRectF &operator&=(const QRectF &o) noexcept { return *this = intersected(o); }
    /*!
        Adds the margins \a m to this rectangle, as marginsAdded() does, and
        returns a reference to this rectangle.
    */
    constexpr QRectF &operator+=(const QMarginsF &m) noexcept { return *this = marginsAdded(m); }
    /*!
        Removes the margins \a m from this rectangle, as marginsRemoved()
        does, and returns a reference to this rectangle.
    */
    constexpr QRectF &operator-=(const QMarginsF &m) noexcept { return *this = marginsRemoved(m); }

    friend bool operator==(const QRectF &a, const QRectF &b) {
        return a.topLeft() == b.topLeft() && a.size() == b.size();
    }
    friend bool operator!=(const QRectF &a, const QRectF &b) { return !(a == b); }
    friend constexpr QRectF operator+(const QRectF &r, const QMarginsF &m) noexcept { return r.marginsAdded(m); }
    friend constexpr QRectF operator+(const QMarginsF &m, const QRectF &r) noexcept { return r.marginsAdded(m); }
    friend constexpr QRectF operator-(const QRectF &r, const QMarginsF &m) noexcept { return r.marginsRemoved(m); }

private:
    double xp = 0, yp = 0, w = 0, h = 0;
};

constexpr QRectF QRect::toRectF() const noexcept { return QRectF(*this); }
