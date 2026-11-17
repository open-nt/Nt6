// MIT License - see LICENSE
#include <QtGui/qregion.h>

#include <algorithm>

/*!
    \class QRegion
    \brief The QRegion class specifies a clip region for a painter.

    A region is a set of integer rectangles. The window system reports the
    damaged part of a window as a region, and QPainter clips to one.

    \differences
    A region is stored as a list of rectangles that may overlap, and it is not
    simplified. The rectangles that rects() returns are therefore not the
    minimal decomposition Qt gives, but they cover the same pixels. There are
    no ellipse or polygon regions, \c subtracted() or \c xored(), and no
    QVariant conversion.
    \enddifferences

    \sa QPainter, QRect
*/

/*!
    \fn QRegion::QRegion()

    Constructs an empty region.
*/

/*!
    Constructs a region that covers \a rect. A null or empty \a rect gives an
    empty region.
*/
QRegion::QRegion(const QRect &rect) {
    if (!rect.isEmpty())
        m_rects.push_back(rect);
}

/*!
    \overload

    Constructs a region that covers the rectangle at (\a x, \a y) with the
    size \a w by \a h.
*/
QRegion::QRegion(int x, int y, int w, int h) : QRegion(QRect(x, y, w, h)) {}

/*!
    \fn bool QRegion::isEmpty() const

    Returns \c true if the region covers no pixels.
*/

/*!
    \fn bool QRegion::isNull() const

    Returns \c true if the region covers no pixels. This is the same as
    isEmpty().
*/

/*!
    Returns the smallest rectangle that contains the region, or an empty
    rectangle for an empty region.
*/
QRect QRegion::boundingRect() const noexcept {
    QRect r;
    for (const QRect &part : m_rects)
        r = r.isNull() ? part : r.united(part);
    return r;
}

/*!
    \fn std::vector<QRect> QRegion::rects() const

    Returns the rectangles that make up the region.
*/

/*!
    \fn int QRegion::rectCount() const

    Returns the number of rectangles that rects() returns.
*/

/*!
    Returns \c true if \a point lies inside the region.
*/
bool QRegion::contains(const QPoint &point) const noexcept {
    return std::any_of(m_rects.begin(), m_rects.end(), [&](const QRect &r) { return r.contains(point); });
}

/*!
    Returns \c true if \a rect overlaps the region.
*/
bool QRegion::intersects(const QRect &rect) const noexcept {
    return std::any_of(m_rects.begin(), m_rects.end(), [&](const QRect &r) { return r.intersects(rect); });
}

/*!
    Returns the region that covers this region and \a rect.

    \sa intersected()
*/
QRegion QRegion::united(const QRect &rect) const {
    QRegion r = *this;
    r += rect;
    return r;
}

/*!
    \overload

    Returns the region that covers this region and \a other.
*/
QRegion QRegion::united(const QRegion &other) const {
    QRegion r = *this;
    r += other;
    return r;
}

/*!
    Returns the part of this region that lies inside \a rect.

    \sa united()
*/
QRegion QRegion::intersected(const QRect &rect) const {
    QRegion r;
    for (const QRect &part : m_rects) {
        const QRect i = part.intersected(rect);
        if (!i.isEmpty())
            r.m_rects.push_back(i);
    }
    return r;
}

/*!
    \overload

    Returns the part of this region that lies inside \a other.
*/
QRegion QRegion::intersected(const QRegion &other) const {
    QRegion r;
    for (const QRect &part : other.m_rects)
        r += intersected(part);
    return r;
}

/*!
    \since 6.0

    Returns the part of this region that is not in \a other.

    \sa intersected(), xored()
*/
QRegion QRegion::subtracted(const QRegion &other) const {
    std::vector<QRect> parts = m_rects;
    for (const QRect &cut : other.m_rects) {
        std::vector<QRect> left;
        for (const QRect &r : parts) {
            const QRect i = r.intersected(cut);
            if (i.isEmpty()) {
                left.push_back(r);
                continue;
            }
            // What is left of r: the bands above and below the cut, and the pieces beside it.
            if (i.top() > r.top())
                left.emplace_back(QPoint(r.left(), r.top()), QPoint(r.right(), i.top() - 1));
            if (i.bottom() < r.bottom())
                left.emplace_back(QPoint(r.left(), i.bottom() + 1), QPoint(r.right(), r.bottom()));
            if (i.left() > r.left())
                left.emplace_back(QPoint(r.left(), i.top()), QPoint(i.left() - 1, i.bottom()));
            if (i.right() < r.right())
                left.emplace_back(QPoint(i.right() + 1, i.top()), QPoint(r.right(), i.bottom()));
        }
        parts = std::move(left);
    }
    QRegion result;
    for (const QRect &r : parts)
        result += r;
    return result;
}

/*!
    \since 6.0

    Returns the parts that are in this region or in \a other, but not in both.

    \sa subtracted()
*/
QRegion QRegion::xored(const QRegion &other) const { return subtracted(other).united(other.subtracted(*this)); }

/*!
    \fn QRegion QRegion::operator-(const QRegion &other) const
    \since 6.0

    Returns subtracted(\a other).
*/

/*!
    \fn QRegion QRegion::operator^(const QRegion &other) const
    \since 6.0

    Returns xored(\a other).
*/

/*!
    \fn QRegion &QRegion::operator-=(const QRegion &other)
    \since 6.0

    Takes \a other away from the region and returns a reference to it.
*/

/*!
    Returns a copy of the region moved by \a dx horizontally and \a dy
    vertically.

    \sa translate()
*/
QRegion QRegion::translated(int dx, int dy) const {
    QRegion r = *this;
    r.translate(dx, dy);
    return r;
}

/*!
    \fn QRegion QRegion::translated(const QPoint &offset) const
    \overload

    Returns a copy of the region moved by \a offset.
*/

/*!
    Moves the region by \a dx horizontally and \a dy vertically.

    \sa translated()
*/
void QRegion::translate(int dx, int dy) {
    for (QRect &r : m_rects)
        r.translate(dx, dy);
}

/*!
    Adds \a rect to the region and returns a reference to it.
*/
QRegion &QRegion::operator+=(const QRect &rect) {
    if (!rect.isEmpty() &&
        std::none_of(m_rects.begin(), m_rects.end(), [&](const QRect &r) { return r.contains(rect); }))
        m_rects.push_back(rect);
    return *this;
}

/*!
    \overload

    Adds the rectangles of \a other to the region and returns a reference to it.
*/
QRegion &QRegion::operator+=(const QRegion &other) {
    for (const QRect &r : other.m_rects)
        *this += r;
    return *this;
}

/*!
    \fn QRegion QRegion::operator+(const QRect &rect) const

    Returns united(\a rect).
*/

/*!
    \fn QRegion QRegion::operator+(const QRegion &other) const
    \overload

    Returns united(\a other).
*/

/*!
    \fn QRegion QRegion::operator&(const QRect &rect) const

    Returns intersected(\a rect).
*/

/*!
    \fn QRegion QRegion::operator&(const QRegion &other) const
    \overload

    Returns intersected(\a other).
*/

/*!
    Intersects the region with \a rect and returns a reference to it.
*/
QRegion &QRegion::operator&=(const QRect &rect) {
    *this = intersected(rect);
    return *this;
}
