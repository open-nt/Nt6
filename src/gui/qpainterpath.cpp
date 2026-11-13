// MIT License - see LICENSE
#include <QtGui/qpainterpath.h>
#include <QtGui/qpolygon.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr qreal kPi = 3.14159265358979323846;

// Points along a cubic Bezier curve, enough to stay within a fraction of a unit of it.
// The points are taken by value: p0 is often the last point of `out`, which appending moves.
void flattenCubic(QPointF p0, QPointF c1, QPointF c2, QPointF p3, QPolygonF *out) {
    const qreal len = std::hypot(c1.x() - p0.x(), c1.y() - p0.y()) + std::hypot(c2.x() - c1.x(), c2.y() - c1.y()) +
                      std::hypot(p3.x() - c2.x(), p3.y() - c2.y());
    const int n = std::clamp(static_cast<int>(std::ceil(std::sqrt(std::max(len, 0.0)) * 2.0)), 2, 256);
    for (int i = 1; i <= n; ++i) {
        const qreal t = static_cast<qreal>(i) / n, u = 1.0 - t;
        const qreal a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        out->append(QPointF(a * p0.x() + b * c1.x() + c * c2.x() + d * p3.x(),
                            a * p0.y() + b * c1.y() + c * c2.y() + d * p3.y()));
    }
}

// Winding number of the closed polygon around `p`.
int winding(const QPolygonF &poly, const QPointF &p, int *crossings) {
    int w = 0;
    const qsizetype n = poly.size();
    for (qsizetype i = 0; i < n; ++i) {
        const QPointF &a = poly.at(i), &b = poly.at((i + 1) % n);
        if (a.y() == b.y())
            continue;
        if ((a.y() <= p.y()) != (b.y() <= p.y())) {
            const qreal x = a.x() + (p.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
            if (x > p.x()) {
                w += b.y() > a.y() ? 1 : -1;
                ++*crossings;
            }
        }
    }
    return w;
}

bool segmentsCross(QPointF a, QPointF b, QPointF c, QPointF d) {
    const auto cross = [](QPointF o, QPointF p, QPointF q) {
        return (p.x() - o.x()) * (q.y() - o.y()) - (p.y() - o.y()) * (q.x() - o.x());
    };
    const qreal d1 = cross(c, d, a), d2 = cross(c, d, b), d3 = cross(a, b, c), d4 = cross(a, b, d);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

} // namespace

// ================================================================================================================

/*!
    \class QPolygon
    \brief The QPolygon class is a list of points with integer coordinates.

    \differences
    There are no \c setPoints() and \c putPoints() taking variadic coordinates. united(), intersected()
    and subtracted() return polygons that are closed by repeating their first point, and several separate
    areas one after the other.
    \enddifferences

    \sa QPolygonF, QPainter::drawPolygon()
*/

/*!
    \fn QPolygon::QPolygon()
    Constructs an empty polygon.
*/
/*!
    \fn QPolygon::QPolygon(const QList<QPoint> &points)
    Constructs a polygon of \a points.
*/
/*!
    \fn QPolygon::QPolygon(std::initializer_list<QPoint> points)
    Constructs a polygon of \a points.
*/
/*!
    \fn QPolygon::QPolygon(qsizetype size)
    Constructs a polygon of \a size points at the origin.
*/

/*!
    Constructs the polygon of the corners of \a rectangle, with the first corner repeated at the end if
    \a closed is \c true.
*/
QPolygon::QPolygon(const QRect &rectangle, bool closed) {
    append(rectangle.topLeft());
    append(rectangle.topRight());
    append(rectangle.bottomRight());
    append(rectangle.bottomLeft());
    if (closed)
        append(rectangle.topLeft());
}

/*!
    \fn QPoint QPolygon::point(qsizetype index) const
    Returns the point at \a index.
*/
/*!
    \fn void QPolygon::setPoint(qsizetype index, const QPoint &point)
    Sets the point at \a index to \a point.
*/
/*!
    \fn void QPolygon::setPoint(qsizetype index, int x, int y)
    \overload
    Sets the point at \a index to (\a x, \a y).
*/

/*!
    Copies \a count points of \a from, starting at \a fromIndex, to this polygon at \a index, growing it if
    needed.
*/
void QPolygon::putPoints(qsizetype index, qsizetype count, const QPolygon &from, qsizetype fromIndex) {
    if (index + count > size())
        resize(index + count);
    for (qsizetype i = 0; i < count && fromIndex + i < from.size(); ++i)
        (*this)[index + i] = from.at(fromIndex + i);
}

/*!
    Returns the smallest rectangle that contains the points.
*/
QRect QPolygon::boundingRect() const {
    if (isEmpty())
        return QRect();
    int x0 = first().x(), x1 = x0, y0 = first().y(), y1 = y0;
    for (const QPoint &p : *this) {
        x0 = std::min(x0, p.x());
        x1 = std::max(x1, p.x());
        y0 = std::min(y0, p.y());
        y1 = std::max(y1, p.y());
    }
    return QRect(QPoint(x0, y0), QPoint(x1, y1));
}

/*!
    Moves each point by \a dx and \a dy.
*/
void QPolygon::translate(int dx, int dy) {
    for (QPoint &p : *this)
        p += QPoint(dx, dy);
}

/*!
    \fn void QPolygon::translate(const QPoint &offset)
    \overload
    Moves each point by \a offset.
*/

/*!
    Returns a copy moved by \a dx and \a dy.
*/
QPolygon QPolygon::translated(int dx, int dy) const {
    QPolygon p = *this;
    p.translate(dx, dy);
    return p;
}

/*!
    \fn QPolygon QPolygon::translated(const QPoint &offset) const
    \overload
    Returns a copy moved by \a offset.
*/

/*!
    Returns \c true if \a point is inside the polygon, closed, with \a fillRule; otherwise returns \c false.
*/
bool QPolygon::containsPoint(const QPoint &point, Qt::FillRule fillRule) const {
    return toPolygonF().containsPoint(QPointF(point), fillRule);
}

/*!
    Returns the polygon with real coordinates.
*/
QPolygonF QPolygon::toPolygonF() const { return QPolygonF(*this); }

/*!
    \class QPolygonF
    \brief The QPolygonF class is a list of points with real coordinates.

    \differences
    united(), intersected() and subtracted() return several separate areas one after the other in one
    polygon, each closed by repeating its first point.
    \enddifferences

    \sa QPolygon, QPainter::drawPolygon(), QPainterPath::addPolygon()
*/

/*!
    \fn QPolygonF::QPolygonF()
    Constructs an empty polygon.
*/
/*!
    \fn QPolygonF::QPolygonF(const QList<QPointF> &points)
    Constructs a polygon of \a points.
*/
/*!
    \fn QPolygonF::QPolygonF(std::initializer_list<QPointF> points)
    Constructs a polygon of \a points.
*/
/*!
    \fn QPolygonF::QPolygonF(qsizetype size)
    Constructs a polygon of \a size points at the origin.
*/

/*!
    Constructs the closed polygon of the corners of \a rectangle: five points, the first repeated.
*/
QPolygonF::QPolygonF(const QRectF &rectangle) {
    append(rectangle.topLeft());
    append(rectangle.topRight());
    append(rectangle.bottomRight());
    append(rectangle.bottomLeft());
    append(rectangle.topLeft());
}

/*!
    Constructs a polygon of the points of \a polygon.
*/
QPolygonF::QPolygonF(const QPolygon &polygon) {
    reserve(polygon.size());
    for (const QPoint &p : polygon)
        append(QPointF(p));
}

/*!
    \fn bool QPolygonF::isClosed() const
    Returns \c true if the last point is the first one; otherwise returns \c false.
*/

/*!
    Returns the smallest rectangle that contains the points.
*/
QRectF QPolygonF::boundingRect() const {
    if (isEmpty())
        return QRectF();
    qreal x0 = first().x(), x1 = x0, y0 = first().y(), y1 = y0;
    for (const QPointF &p : *this) {
        x0 = std::min(x0, p.x());
        x1 = std::max(x1, p.x());
        y0 = std::min(y0, p.y());
        y1 = std::max(y1, p.y());
    }
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

/*!
    Moves each point by \a dx and \a dy.
*/
void QPolygonF::translate(qreal dx, qreal dy) {
    for (QPointF &p : *this)
        p += QPointF(dx, dy);
}

/*!
    \fn void QPolygonF::translate(const QPointF &offset)
    \overload
    Moves each point by \a offset.
*/

/*!
    Returns a copy moved by \a dx and \a dy.
*/
QPolygonF QPolygonF::translated(qreal dx, qreal dy) const {
    QPolygonF p = *this;
    p.translate(dx, dy);
    return p;
}

/*!
    \fn QPolygonF QPolygonF::translated(const QPointF &offset) const
    \overload
    Returns a copy moved by \a offset.
*/

/*!
    Returns \c true if \a point is inside the polygon, closed, with \a fillRule; otherwise returns \c false.
*/
bool QPolygonF::containsPoint(const QPointF &point, Qt::FillRule fillRule) const {
    if (size() < 3)
        return false;
    int crossings = 0;
    const int w = winding(*this, point, &crossings);
    return fillRule == Qt::WindingFill ? w != 0 : (crossings % 2) == 1;
}

/*!
    Returns the polygon with its points rounded to integers.
*/
QPolygon QPolygonF::toPolygon() const {
    QPolygon out;
    out.reserve(size());
    for (const QPointF &p : *this)
        out.append(p.toPoint());
    return out;
}

// ================================================================================================================

/*!
    \class QPainterPath
    \brief The QPainterPath class is a shape made of lines and curves, for QPainter to fill and stroke.

    A path is a list of subpaths. Each starts with moveTo() and goes on with lineTo(), cubicTo(),
    quadTo() and arcTo(); closeSubpath() joins its end to its start. Shapes are added whole with addRect(),
    addEllipse(), addRoundedRect() and addPolygon():

    \code
    QPainterPath petal;
    petal.moveTo(0, 0);
    petal.cubicTo(20, -30, 60, -30, 80, 0);
    petal.cubicTo(60, 30, 20, 30, 0, 0);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(100, 100);
    for (int i = 0; i < 8; ++i) {
        painter.rotate(45);
        painter.fillPath(petal, QColor(200, 80, 120));
    }
    \endcode

    \image painter/flower.png A flower of Bezier petals

    The fill rule decides which parts of a path that crosses itself are inside.

    Paths can be combined as areas with united(), intersected() and subtracted(), or the operators
    \c |, \c & and \c -. QPainterPathStroker turns the stroke of a path into a path to fill.

    \differences
    There are no toReversed(), addText() or addRegion(). Lengths and points along the path are measured on
    its curves divided into short lines. united(), intersected(), subtracted() and simplified() divide
    curves into lines too; intersects() and contains() taking a path compare areas, so paths that only touch
    do not intersect.
    \enddifferences

    \sa QPainter::drawPath(), QPainter::fillPath(), QPainterPathStroker
*/

/*!
    \enum QPainterPath::ElementType

    \value MoveToElement The start of a subpath.
    \value LineToElement The end of a line.
    \value CurveToElement The first control point of a cubic curve.
    \value CurveToDataElement The second control point, or the end, of a cubic curve.
*/

/*!
    \class QPainterPath::Element
    \brief The QPainterPath::Element class is one point of a path.

    \differences
    None.
    \enddifferences
*/

/*!
    \variable QPainterPath::Element::x
    The x coordinate.
*/
/*!
    \variable QPainterPath::Element::y
    The y coordinate.
*/
/*!
    \variable QPainterPath::Element::type
    What the point is.
*/
/*!
    \fn bool QPainterPath::Element::isMoveTo() const
    Returns \c true if the point starts a subpath; otherwise returns \c false.
*/
/*!
    \fn bool QPainterPath::Element::isLineTo() const
    Returns \c true if the point ends a line; otherwise returns \c false.
*/
/*!
    \fn bool QPainterPath::Element::isCurveTo() const
    Returns \c true if the point is the first control point of a curve; otherwise returns \c false.
*/
/*!
    \fn QPainterPath::Element::operator QPointF() const
    Returns the point.
*/
/*!
    \fn bool QPainterPath::Element::operator==(const QPainterPath::Element &e) const
    Returns \c true if this point and \a e are equal.
*/
/*!
    \fn bool QPainterPath::Element::operator!=(const QPainterPath::Element &e) const
    Returns \c true if this point and \a e differ.
*/

/*!
    \fn QPainterPath::QPainterPath()
    Constructs an empty path.
*/

/*!
    Constructs a path that starts at \a startPoint.
*/
QPainterPath::QPainterPath(const QPointF &startPoint) { moveTo(startPoint); }

void QPainterPath::ensureStart() {
    if (m_elements.isEmpty()) {
        m_elements.append(Element{0, 0, MoveToElement});
    } else if (m_closed) {
        const QPointF start = currentPosition();
        m_elements.append(Element{start.x(), start.y(), MoveToElement});
    }
    m_closed = false;
}

/*!
    Starts a new subpath at \a point.
*/
void QPainterPath::moveTo(const QPointF &point) {
    m_closed = false;
    if (!m_elements.isEmpty() && m_elements.last().type == MoveToElement)
        m_elements.last() = Element{point.x(), point.y(), MoveToElement};
    else
        m_elements.append(Element{point.x(), point.y(), MoveToElement});
}

/*!
    \fn void QPainterPath::moveTo(qreal x, qreal y)
    \overload
    Starts a new subpath at (\a x, \a y).
*/

/*!
    Adds a line from the current position to \a endPoint.
*/
void QPainterPath::lineTo(const QPointF &endPoint) {
    ensureStart();
    m_elements.append(Element{endPoint.x(), endPoint.y(), LineToElement});
}

/*!
    \fn void QPainterPath::lineTo(qreal x, qreal y)
    \overload
    Adds a line to (\a x, \a y).
*/

/*!
    Adds a cubic Bezier curve from the current position to \a endPoint, with the control points \a c1 and
    \a c2.
*/
void QPainterPath::cubicTo(const QPointF &c1, const QPointF &c2, const QPointF &endPoint) {
    ensureStart();
    m_elements.append(Element{c1.x(), c1.y(), CurveToElement});
    m_elements.append(Element{c2.x(), c2.y(), CurveToDataElement});
    m_elements.append(Element{endPoint.x(), endPoint.y(), CurveToDataElement});
}

/*!
    \fn void QPainterPath::cubicTo(qreal c1X, qreal c1Y, qreal c2X, qreal c2Y, qreal endPointX, qreal endPointY)
    \overload
    Adds a cubic curve to (\a endPointX, \a endPointY) with the control points (\a c1X, \a c1Y) and
    (\a c2X, \a c2Y).
*/

/*!
    Adds a quadratic Bezier curve from the current position to \a endPoint, with the control point \a c.
*/
void QPainterPath::quadTo(const QPointF &c, const QPointF &endPoint) {
    ensureStart();
    const QPointF p0 = currentPosition();
    cubicTo(p0 + (c - p0) * (2.0 / 3.0), endPoint + (c - endPoint) * (2.0 / 3.0), endPoint);
}

/*!
    \fn void QPainterPath::quadTo(qreal cx, qreal cy, qreal endPointX, qreal endPointY)
    \overload
    Adds a quadratic curve to (\a endPointX, \a endPointY) with the control point (\a cx, \a cy).
*/

/*!
    Adds an arc of the ellipse that fits in \a rectangle, from \a startAngle for \a sweepLength degrees.
    Angles are counterclockwise from 3 o'clock. A line joins the current position to the start of the arc.
*/
void QPainterPath::arcTo(const QRectF &rectangle, qreal startAngle, qreal sweepLength) {
    const QPointF c = rectangle.center();
    const qreal rx = rectangle.width() / 2.0, ry = rectangle.height() / 2.0;
    const auto at = [&](qreal deg) {
        const qreal a = deg * kPi / 180.0;
        return QPointF(c.x() + rx * std::cos(a), c.y() - ry * std::sin(a));
    };
    const QPointF start = at(startAngle);
    if (m_elements.isEmpty())
        moveTo(start);
    else
        lineTo(start);
    if (sweepLength == 0.0 || !std::isfinite(sweepLength))
        return;
    sweepLength = std::clamp(sweepLength, -3600.0, 3600.0);
    const int segments = static_cast<int>(std::ceil(std::fabs(sweepLength) / 90.0));
    const qreal step = sweepLength / segments;
    for (int i = 0; i < segments; ++i) {
        const qreal a0 = (startAngle + i * step) * kPi / 180.0, a1 = (startAngle + (i + 1) * step) * kPi / 180.0;
        const qreal k = 4.0 / 3.0 * std::tan((a1 - a0) / 4.0);
        const QPointF p0(c.x() + rx * std::cos(a0), c.y() - ry * std::sin(a0));
        const QPointF p1(c.x() + rx * std::cos(a1), c.y() - ry * std::sin(a1));
        const QPointF d0(-rx * std::sin(a0), -ry * std::cos(a0)), d1(-rx * std::sin(a1), -ry * std::cos(a1));
        cubicTo(p0 + d0 * k, p1 - d1 * k, p1);
    }
}

/*!
    \fn void QPainterPath::arcTo(qreal x, qreal y, qreal w, qreal h, qreal startAngle, qreal arcLength)
    \overload
    Adds an arc of the ellipse in the rectangle at (\a x, \a y) of \a w by \a h, from \a startAngle for
    \a arcLength degrees.
*/

/*!
    Starts a new subpath at the point at \a angle degrees of the ellipse that fits in \a rectangle.
*/
void QPainterPath::arcMoveTo(const QRectF &rectangle, qreal angle) {
    const qreal a = angle * kPi / 180.0;
    moveTo(QPointF(rectangle.center().x() + rectangle.width() / 2.0 * std::cos(a),
                   rectangle.center().y() - rectangle.height() / 2.0 * std::sin(a)));
}

/*!
    \fn void QPainterPath::arcMoveTo(qreal x, qreal y, qreal w, qreal h, qreal angle)
    \overload
    Starts a new subpath on the ellipse in the rectangle at (\a x, \a y) of \a w by \a h, at \a angle.
*/

/*!
    Closes the current subpath with a line to its start, and starts a new one there.
*/
void QPainterPath::closeSubpath() {
    if (m_elements.isEmpty())
        return;
    qsizetype start = m_elements.size() - 1;
    while (start > 0 && m_elements.at(start).type != MoveToElement)
        --start;
    const Element s = m_elements.at(start);
    const Element &last = m_elements.last();
    if (start == m_elements.size() - 1 || m_closed)
        return;
    if (last.x != s.x || last.y != s.y)
        m_elements.append(Element{s.x, s.y, LineToElement});
    m_closed = true;
}

/*!
    Adds \a rectangle as a closed subpath.
*/
void QPainterPath::addRect(const QRectF &rectangle) {
    moveTo(rectangle.topLeft());
    lineTo(rectangle.topRight());
    lineTo(rectangle.bottomRight());
    lineTo(rectangle.bottomLeft());
    closeSubpath();
}

/*!
    \fn void QPainterPath::addRect(qreal x, qreal y, qreal width, qreal height)
    \overload
    Adds the rectangle at (\a x, \a y) of \a width by \a height.
*/

/*!
    Adds the ellipse that fits in \a boundingRectangle as a closed subpath.
*/
void QPainterPath::addEllipse(const QRectF &boundingRectangle) {
    arcMoveTo(boundingRectangle, 0);
    arcTo(boundingRectangle, 0, 360);
    closeSubpath();
}

/*!
    \fn void QPainterPath::addEllipse(qreal x, qreal y, qreal width, qreal height)
    \overload
    Adds the ellipse in the rectangle at (\a x, \a y) of \a width by \a height.
*/

/*!
    \fn void QPainterPath::addEllipse(const QPointF &center, qreal rx, qreal ry)
    \overload
    Adds the ellipse at \a center with the radii \a rx and \a ry.
*/

/*!
    Adds \a rect with corners rounded by \a xRadius and \a yRadius, in units or, with \a mode
    Qt::RelativeSize, in percent of half the width and height.
*/
void QPainterPath::addRoundedRect(const QRectF &rect, qreal xRadius, qreal yRadius, Qt::SizeMode mode) {
    const QRectF r = rect.normalized();
    qreal rx = xRadius, ry = yRadius;
    if (mode == Qt::RelativeSize) {
        rx = r.width() / 2.0 * xRadius / 100.0;
        ry = r.height() / 2.0 * yRadius / 100.0;
    }
    rx = std::min(rx, r.width() / 2.0);
    ry = std::min(ry, r.height() / 2.0);
    if (rx <= 0 || ry <= 0) {
        addRect(r);
        return;
    }
    const qreal x = r.x(), y = r.y(), w = r.width(), h = r.height();
    arcMoveTo(QRectF(x + w - 2 * rx, y, 2 * rx, 2 * ry), 90);
    arcTo(QRectF(x, y, 2 * rx, 2 * ry), 90, 90);
    arcTo(QRectF(x, y + h - 2 * ry, 2 * rx, 2 * ry), 180, 90);
    arcTo(QRectF(x + w - 2 * rx, y + h - 2 * ry, 2 * rx, 2 * ry), 270, 90);
    arcTo(QRectF(x + w - 2 * rx, y, 2 * rx, 2 * ry), 0, 90);
    closeSubpath();
}

/*!
    \fn void QPainterPath::addRoundedRect(qreal x, qreal y, qreal w, qreal h, qreal xRadius, qreal yRadius, Qt::SizeMode mode)
    \overload
    Adds the rectangle at (\a x, \a y) of \a w by \a h with corners rounded by \a xRadius and \a yRadius in
    the given \a mode.
*/

/*!
    Adds \a polygon as a subpath; it is closed only if its last point is its first.
*/
void QPainterPath::addPolygon(const QPolygonF &polygon) {
    if (polygon.isEmpty())
        return;
    moveTo(polygon.first());
    for (qsizetype i = 1; i < polygon.size(); ++i)
        lineTo(polygon.at(i));
}

/*!
    Adds the subpaths of \a path.
*/
void QPainterPath::addPath(const QPainterPath &path) {
    if (path.m_elements.isEmpty())
        return;
    if (!m_elements.isEmpty() && m_elements.last().type == MoveToElement)
        m_elements.removeLast();
    m_elements.append(path.m_elements);
    m_closed = path.m_closed;
}

/*!
    Adds \a path, joining its first subpath to the current position with a line.
*/
void QPainterPath::connectPath(const QPainterPath &path) {
    if (path.m_elements.isEmpty())
        return;
    if (m_elements.isEmpty()) {
        addPath(path);
        return;
    }
    const Element &first = path.m_elements.first();
    lineTo(QPointF(first.x, first.y));
    for (qsizetype i = 1; i < path.m_elements.size(); ++i)
        m_elements.append(path.m_elements.at(i));
}

/*!
    Returns the end of the last subpath.
*/
QPointF QPainterPath::currentPosition() const {
    return m_elements.isEmpty() ? QPointF() : QPointF(m_elements.last().x, m_elements.last().y);
}

/*!
    Returns \c true if the path has nothing but a start point; otherwise returns \c false.
*/
bool QPainterPath::isEmpty() const {
    return m_elements.isEmpty() || (m_elements.size() == 1 && m_elements.first().type == MoveToElement);
}

/*!
    \fn int QPainterPath::elementCount() const
    Returns the number of points of the path, control points included.
*/
/*!
    \fn QPainterPath::Element QPainterPath::elementAt(int index) const
    Returns the point at \a index.
*/

/*!
    Moves the point at \a index to (\a x, \a y).
*/
void QPainterPath::setElementPositionAt(int index, qreal x, qreal y) {
    if (index < 0 || index >= m_elements.size())
        return;
    m_elements[index].x = x;
    m_elements[index].y = y;
}

/*!
    Removes all points.
*/
void QPainterPath::clear() {
    m_elements.clear();
    m_closed = false;
}

/*!
    \fn void QPainterPath::reserve(int size)
    Reserves room for \a size points.
*/
/*!
    \fn int QPainterPath::capacity() const
    Returns the number of points there is room for.
*/
/*!
    \fn Qt::FillRule QPainterPath::fillRule() const
    Returns the fill rule; Qt::OddEvenFill by default.
*/
/*!
    \fn void QPainterPath::setFillRule(Qt::FillRule fillRule)
    Sets the fill rule to \a fillRule.
*/
/*!
    \fn void QPainterPath::swap(QPainterPath &other)
    Swaps this path with \a other.
*/

/*!
    Returns the subpaths as polylines, mapped by \a matrix: curves are divided into short lines.
*/
QList<QPolygonF> QPainterPath::toSubpathPolygons(const QTransform &matrix) const {
    QList<QPolygonF> out;
    QPolygonF current;
    for (qsizetype i = 0; i < m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        const QPointF p = matrix.map(QPointF(e.x, e.y));
        switch (e.type) {
        case MoveToElement:
            if (current.size() > 1)
                out.append(current);
            current = QPolygonF();
            current.append(p);
            break;
        case LineToElement:
            current.append(p);
            break;
        case CurveToElement:
            if (i + 2 < m_elements.size() && !current.isEmpty()) {
                const QPointF c2 = matrix.map(QPointF(m_elements.at(i + 1).x, m_elements.at(i + 1).y));
                const QPointF end = matrix.map(QPointF(m_elements.at(i + 2).x, m_elements.at(i + 2).y));
                flattenCubic(current.last(), p, c2, end, &current);
                i += 2;
            }
            break;
        case CurveToDataElement:
            break;
        }
    }
    if (current.size() > 1)
        out.append(current);
    return out;
}

/*!
    Returns the subpaths as closed polygons, mapped by \a matrix.
*/
QList<QPolygonF> QPainterPath::toFillPolygons(const QTransform &matrix) const {
    QList<QPolygonF> polys = toSubpathPolygons(matrix);
    for (QPolygonF &p : polys)
        if (!p.isClosed())
            p.append(p.first());
    return polys;
}

/*!
    Returns all subpaths as one closed polygon, mapped by \a matrix.
*/
QPolygonF QPainterPath::toFillPolygon(const QTransform &matrix) const {
    QPolygonF all;
    for (const QPolygonF &p : toFillPolygons(matrix))
        all.append(p);
    return all;
}

/*!
    Returns the smallest rectangle that contains the path, curves included.
*/
QRectF QPainterPath::boundingRect() const {
    QPolygonF all;
    for (const QPolygonF &p : toSubpathPolygons())
        all.append(p);
    if (all.isEmpty())
        return m_elements.isEmpty() ? QRectF() : QRectF(currentPosition(), QSizeF(0, 0));
    return all.boundingRect();
}

/*!
    Returns the smallest rectangle that contains all the points of the path, control points included.
*/
QRectF QPainterPath::controlPointRect() const {
    QPolygonF points;
    for (const Element &e : m_elements)
        points.append(QPointF(e.x, e.y));
    return points.boundingRect();
}

/*!
    Returns \c true if \a point is inside the path, by its fill rule; otherwise returns \c false.
*/
bool QPainterPath::contains(const QPointF &point) const {
    int w = 0, crossings = 0;
    for (const QPolygonF &p : toFillPolygons())
        w += winding(p, point, &crossings);
    return m_fillRule == Qt::WindingFill ? w != 0 : (crossings % 2) == 1;
}

/*!
    \overload

    Returns \c true if \a rectangle is inside the path; otherwise returns \c false.
*/
bool QPainterPath::contains(const QRectF &rectangle) const {
    const QRectF r = rectangle.normalized();
    for (const QPointF &c : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()})
        if (!contains(c))
            return false;
    // No edge of the path may cross into the rectangle.
    const QPointF corners[4] = {r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft()};
    for (const QPolygonF &p : toFillPolygons())
        for (qsizetype i = 0; i + 1 < p.size(); ++i) {
            if (r.contains(p.at(i)) && !(p.at(i).x() == r.left() || p.at(i).x() == r.right() ||
                                         p.at(i).y() == r.top() || p.at(i).y() == r.bottom()))
                return false;
            for (int k = 0; k < 4; ++k)
                if (segmentsCross(p.at(i), p.at(i + 1), corners[k], corners[(k + 1) % 4]))
                    return false;
        }
    return true;
}

/*!
    Returns \c true if the path and \a rectangle overlap; otherwise returns \c false.
*/
bool QPainterPath::intersects(const QRectF &rectangle) const {
    const QRectF r = rectangle.normalized();
    if (contains(r.center()))
        return true;
    const QPointF corners[4] = {r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft()};
    for (const QPolygonF &p : toFillPolygons()) {
        for (qsizetype i = 0; i + 1 < p.size(); ++i) {
            if (r.contains(p.at(i)))
                return true;
            for (int k = 0; k < 4; ++k)
                if (segmentsCross(p.at(i), p.at(i + 1), corners[k], corners[(k + 1) % 4]))
                    return true;
        }
    }
    for (const QPointF &c : corners)
        if (contains(c))
            return true;
    return false;
}

/*!
    Moves each point by \a dx and \a dy.
*/
void QPainterPath::translate(qreal dx, qreal dy) {
    for (Element &e : m_elements) {
        e.x += dx;
        e.y += dy;
    }
}

/*!
    \fn void QPainterPath::translate(const QPointF &offset)
    \overload
    Moves each point by \a offset.
*/

/*!
    Returns a copy moved by \a dx and \a dy.
*/
QPainterPath QPainterPath::translated(qreal dx, qreal dy) const {
    QPainterPath p = *this;
    p.translate(dx, dy);
    return p;
}

/*!
    \fn QPainterPath QPainterPath::translated(const QPointF &offset) const
    \overload
    Returns a copy moved by \a offset.
*/

/*!
    Returns the length of the path.
*/
qreal QPainterPath::length() const {
    qreal len = 0.0;
    for (const QPolygonF &p : toSubpathPolygons())
        for (qsizetype i = 1; i < p.size(); ++i)
            len += std::hypot(p.at(i).x() - p.at(i - 1).x(), p.at(i).y() - p.at(i - 1).y());
    return len;
}

/*!
    Returns where along the path, from 0 to 1, the length \a len is reached.
*/
qreal QPainterPath::percentAtLength(qreal len) const {
    const qreal total = length();
    if (total <= 0.0)
        return 0.0;
    return std::clamp(len / total, 0.0, 1.0);
}

/*!
    Returns the point at the fraction \a t, from 0 to 1, of the length of the path.
*/
QPointF QPainterPath::pointAtPercent(qreal t) const {
    const QList<QPolygonF> polys = toSubpathPolygons();
    const qreal target = std::clamp(t, 0.0, 1.0) * length();
    qreal walked = 0.0;
    for (const QPolygonF &p : polys)
        for (qsizetype i = 1; i < p.size(); ++i) {
            const qreal seg = std::hypot(p.at(i).x() - p.at(i - 1).x(), p.at(i).y() - p.at(i - 1).y());
            if (walked + seg >= target && seg > 0) {
                const qreal k = (target - walked) / seg;
                return p.at(i - 1) + (p.at(i) - p.at(i - 1)) * k;
            }
            walked += seg;
        }
    return currentPosition();
}

/*!
    Returns the angle, in degrees counterclockwise from 3 o'clock, of the path at the fraction \a t of its
    length.
*/
qreal QPainterPath::angleAtPercent(qreal t) const {
    const QPointF a = pointAtPercent(std::max(0.0, t - 0.001)), b = pointAtPercent(std::min(1.0, t + 0.001));
    const qreal deg = std::atan2(-(b.y() - a.y()), b.x() - a.x()) * 180.0 / kPi;
    return deg < 0 ? deg + 360.0 : deg;
}

/*!
    Returns the slope, dy/dx, of the path at the fraction \a t of its length.
*/
qreal QPainterPath::slopeAtPercent(qreal t) const {
    const QPointF a = pointAtPercent(std::max(0.0, t - 0.001)), b = pointAtPercent(std::min(1.0, t + 0.001));
    const qreal dx = b.x() - a.x();
    return dx == 0.0 ? std::numeric_limits<qreal>::infinity() : (b.y() - a.y()) / dx;
}

/*!
    Returns \c true if \a path has the same points and fill rule; otherwise returns \c false.
*/
bool QPainterPath::operator==(const QPainterPath &path) const {
    return m_fillRule == path.m_fillRule && m_elements == path.m_elements;
}

/*!
    \fn bool QPainterPath::operator!=(const QPainterPath &path) const
    Returns \c true if \a path differs from this one.
*/
