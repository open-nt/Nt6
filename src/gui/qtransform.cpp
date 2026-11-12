// MIT License - see LICENSE
#include <QtGui/qpainterpath.h>
#include <QtGui/qpolygon.h>
#include <QtGui/qtransform.h>

#include <cmath>

/*!
    \class QTransform
    \brief The QTransform class is a 2D transformation of the coordinate system.

    A transformation maps a point (x, y) to (x', y'):

    \code
    x' = m11 * x + m21 * y + dx
    y' = m22 * y + m12 * x + dy
    \endcode

    and divides by \c{m13 * x + m23 * y + m33} when it is a projection. translate(), scale(), rotate() and
    shear() change the coordinate system the way QPainter's functions of the same names do: the new
    operation applies first, then the transformation as it was.

    \code
    QTransform t;
    t.translate(50, 50);
    t.rotate(45);
    QPointF p = t.map(QPointF(10, 0));   // (57.07, 57.07)
    \endcode

    QPainter has the same functions, and saves and restores its transformation with save() and restore():

    \snippet gui/painting.cpp transforms

    \image painter/transforms.png Rounded rectangles rotated and scaled around a center

    \differences
    There is no \c quadToQuad(), \c squareToQuad(), \c quadToSquare() or QDataStream operators. Rotations
    about the x and y axes use a perspective at \a distanceToPlane as in Qt, but QPainter draws only affine
    transformations.
    \enddifferences

    \sa QPainter::setTransform(), QPainterPath
*/

/*!
    \enum QTransform::TransformationType

    What a transformation does, in increasing order of generality.

    \value TxNone The identity.
    \value TxTranslate A translation.
    \value TxScale A scale, and maybe a translation.
    \value TxRotate A rotation, and maybe a scale and a translation.
    \value TxShear A shear, or any other affine transformation.
    \value TxProject A projection.
*/

/*!
    \fn QTransform::QTransform()

    Constructs the identity transformation.
*/

/*!
    Constructs an affine transformation with the elements \a h11, \a h12, \a h21, \a h22, \a dx and \a dy.
*/
QTransform::QTransform(qreal h11, qreal h12, qreal h21, qreal h22, qreal dx, qreal dy)
    : a11(h11), a12(h12), a21(h21), a22(h22), a31(dx), a32(dy) {}

/*!
    Constructs a transformation with the elements \a h11, \a h12, \a h13, \a h21, \a h22, \a h23, \a h31,
    \a h32 and \a h33.
*/
QTransform::QTransform(qreal h11, qreal h12, qreal h13, qreal h21, qreal h22, qreal h23, qreal h31, qreal h32,
                       qreal h33)
    : a11(h11), a12(h12), a13(h13), a21(h21), a22(h22), a23(h23), a31(h31), a32(h32), a33(h33) {}

/*!
    \fn bool QTransform::isAffine() const

    Returns \c true if the transformation is not a projection; otherwise returns \c false.
*/

/*!
    \fn bool QTransform::isIdentity() const

    Returns \c true if the transformation is the identity; otherwise returns \c false.
*/

/*!
    \fn bool QTransform::isInvertible() const

    Returns \c true if the transformation can be inverted; otherwise returns \c false.
*/

/*!
    \fn bool QTransform::isScaling() const

    Returns \c true if the transformation scales, or does more; otherwise returns \c false.
*/

/*!
    \fn bool QTransform::isRotating() const

    Returns \c true if the transformation rotates, or does more; otherwise returns \c false.
*/

/*!
    \fn bool QTransform::isTranslating() const

    Returns \c true if the transformation is not the identity; otherwise returns \c false.
*/

/*!
    Returns what the transformation does.
*/
QTransform::TransformationType QTransform::type() const {
    if (!isAffine())
        return TxProject;
    if (a12 != 0.0 || a21 != 0.0) {
        // A rotation keeps the axes perpendicular and the same length.
        const qreal dot = a11 * a21 + a12 * a22;
        const qreal len1 = a11 * a11 + a12 * a12, len2 = a21 * a21 + a22 * a22;
        return std::fabs(dot) < 1e-12 && std::fabs(len1 - len2) < 1e-12 * std::max(1.0, len1) ? TxRotate : TxShear;
    }
    if (a11 != 1.0 || a22 != 1.0)
        return TxScale;
    if (a31 != 0.0 || a32 != 0.0)
        return TxTranslate;
    return TxNone;
}

/*!
    Returns the determinant of the matrix.
*/
qreal QTransform::determinant() const {
    return a11 * (a33 * a22 - a32 * a23) - a21 * (a33 * a12 - a32 * a13) + a31 * (a23 * a12 - a22 * a13);
}

/*!
    \fn qreal QTransform::m11() const
    Returns the horizontal scaling factor.
*/
/*!
    \fn qreal QTransform::m12() const
    Returns the vertical shearing factor.
*/
/*!
    \fn qreal QTransform::m13() const
    Returns the horizontal projection factor.
*/
/*!
    \fn qreal QTransform::m21() const
    Returns the horizontal shearing factor.
*/
/*!
    \fn qreal QTransform::m22() const
    Returns the vertical scaling factor.
*/
/*!
    \fn qreal QTransform::m23() const
    Returns the vertical projection factor.
*/
/*!
    \fn qreal QTransform::m31() const
    Returns the horizontal translation.
*/
/*!
    \fn qreal QTransform::m32() const
    Returns the vertical translation.
*/
/*!
    \fn qreal QTransform::m33() const
    Returns the division factor of the projection.
*/
/*!
    \fn qreal QTransform::dx() const
    Returns the horizontal translation.
*/
/*!
    \fn qreal QTransform::dy() const
    Returns the vertical translation.
*/

/*!
    Sets the elements of the matrix to \a h11, \a h12, \a h13, \a h21, \a h22, \a h23, \a h31, \a h32 and
    \a h33.
*/
void QTransform::setMatrix(qreal h11, qreal h12, qreal h13, qreal h21, qreal h22, qreal h23, qreal h31, qreal h32,
                           qreal h33) {
    *this = QTransform(h11, h12, h13, h21, h22, h23, h31, h32, h33);
}

/*!
    \fn void QTransform::reset()

    Makes the transformation the identity.
*/

/*!
    Returns the adjoint of the matrix.
*/
QTransform QTransform::adjoint() const {
    const qreal h11 = a22 * a33 - a23 * a32, h21 = a23 * a31 - a21 * a33, h31 = a21 * a32 - a22 * a31;
    const qreal h12 = a13 * a32 - a12 * a33, h22 = a11 * a33 - a13 * a31, h32 = a12 * a31 - a11 * a32;
    const qreal h13 = a12 * a23 - a13 * a22, h23 = a13 * a21 - a11 * a23, h33 = a11 * a22 - a12 * a21;
    return QTransform(h11, h12, h13, h21, h22, h23, h31, h32, h33);
}

/*!
    Returns the transposed matrix.
*/
QTransform QTransform::transposed() const { return QTransform(a11, a21, a31, a12, a22, a32, a13, a23, a33); }

/*!
    Returns the inverse of the transformation, or the identity if it has none; then \a invertible, if not
    \c nullptr, is set to \c false.
*/
QTransform QTransform::inverted(bool *invertible) const {
    const qreal det = determinant();
    if (invertible)
        *invertible = det != 0.0;
    if (det == 0.0)
        return QTransform();
    if (isAffine()) {
        const qreal inv = 1.0 / det;
        return QTransform(a22 * inv, -a12 * inv, -a21 * inv, a11 * inv, (a21 * a32 - a22 * a31) * inv,
                          (a12 * a31 - a11 * a32) * inv);
    }
    QTransform a = adjoint();
    a /= det;
    return a;
}

/*!
    Moves the coordinate system by \a dx and \a dy, and returns a reference to the transformation.
*/
QTransform &QTransform::translate(qreal dx, qreal dy) {
    *this = fromTranslate(dx, dy) * *this;
    return *this;
}

/*!
    Scales the coordinate system by \a sx horizontally and \a sy vertically, and returns a reference to the
    transformation.
*/
QTransform &QTransform::scale(qreal sx, qreal sy) {
    *this = fromScale(sx, sy) * *this;
    return *this;
}

/*!
    Shears the coordinate system by \a sh horizontally and \a sv vertically, and returns a reference to the
    transformation.
*/
QTransform &QTransform::shear(qreal sh, qreal sv) {
    *this = QTransform(1, sv, sh, 1, 0, 0) * *this;
    return *this;
}

/*!
    Rotates the coordinate system by \a a degrees clockwise on the screen about \a axis, and returns a
    reference to the transformation. Rotations about the x and y axes are projections, seen from
    \a distanceToPlane.
*/
QTransform &QTransform::rotate(qreal a, Qt::Axis axis, qreal distanceToPlane) {
    return rotateRadians(a * 3.14159265358979323846 / 180.0, axis, distanceToPlane);
}

/*!
    Rotates the coordinate system by \a a radians about \a axis, seen from \a distanceToPlane for the x and y
    axes, and returns a reference to the transformation.
*/
QTransform &QTransform::rotateRadians(qreal a, Qt::Axis axis, qreal distanceToPlane) {
    qreal s = std::sin(a), c = std::cos(a);
    // Exact values for the right angles, which are common.
    if (std::fabs(s) < 1e-15)
        s = 0.0;
    if (std::fabs(c) < 1e-15)
        c = 0.0;
    if (axis == Qt::ZAxis) {
        *this = QTransform(c, s, -s, c, 0, 0) * *this;
        return *this;
    }
    QTransform r;
    if (axis == Qt::YAxis) {
        r.a11 = c;
        r.a13 = distanceToPlane != 0.0 ? -s / distanceToPlane : 0.0;
    } else {
        r.a22 = c;
        r.a23 = distanceToPlane != 0.0 ? -s / distanceToPlane : 0.0;
    }
    *this = r * *this;
    return *this;
}

/*!
    Returns a translation by \a dx and \a dy.
*/
QTransform QTransform::fromTranslate(qreal dx, qreal dy) { return QTransform(1, 0, 0, 1, dx, dy); }

/*!
    Returns a scale by \a dx horizontally and \a dy vertically.
*/
QTransform QTransform::fromScale(qreal dx, qreal dy) { return QTransform(dx, 0, 0, dy, 0, 0); }

/*!
    Maps the point (\a x, \a y) and stores the result in \a tx and \a ty.
*/
void QTransform::map(qreal x, qreal y, qreal *tx, qreal *ty) const {
    qreal fx = a11 * x + a21 * y + a31;
    qreal fy = a12 * x + a22 * y + a32;
    if (!isAffine()) {
        qreal w = a13 * x + a23 * y + a33;
        if (w < 1e-6)
            w = 1e-6; // points behind the eye are clamped, as Qt does
        fx /= w;
        fy /= w;
    }
    *tx = fx;
    *ty = fy;
}

/*!
    \overload

    Maps the point (\a x, \a y), rounding to integers, and stores the result in \a tx and \a ty.
*/
void QTransform::map(int x, int y, int *tx, int *ty) const {
    qreal fx = 0, fy = 0;
    map(static_cast<qreal>(x), static_cast<qreal>(y), &fx, &fy);
    *tx = qRound(fx);
    *ty = qRound(fy);
}

/*!
    Returns \a point mapped by the transformation.
*/
QPointF QTransform::map(const QPointF &point) const {
    qreal x = 0, y = 0;
    map(point.x(), point.y(), &x, &y);
    return QPointF(x, y);
}

/*!
    \overload

    Returns \a point mapped by the transformation, rounded to integers.
*/
QPoint QTransform::map(const QPoint &point) const { return map(QPointF(point)).toPoint(); }

/*!
    \overload

    Returns \a line with both ends mapped.
*/
QLineF QTransform::map(const QLineF &line) const { return QLineF(map(line.p1()), map(line.p2())); }

/*!
    \overload

    Returns \a line with both ends mapped, rounded to integers.
*/
QLine QTransform::map(const QLine &line) const { return QLine(map(line.p1()), map(line.p2())); }

/*!
    \overload

    Returns \a polygon with each point mapped.
*/
QPolygonF QTransform::map(const QPolygonF &polygon) const {
    QPolygonF out;
    out.reserve(polygon.size());
    for (const QPointF &p : polygon)
        out.append(map(p));
    return out;
}

/*!
    \overload

    Returns \a polygon with each point mapped, rounded to integers.
*/
QPolygon QTransform::map(const QPolygon &polygon) const {
    QPolygon out;
    out.reserve(polygon.size());
    for (const QPoint &p : polygon)
        out.append(map(p));
    return out;
}

/*!
    \overload

    Returns \a path with each of its points mapped. Curves stay curves, which is exact for affine
    transformations.
*/
QPainterPath QTransform::map(const QPainterPath &path) const {
    QPainterPath out = path;
    for (int i = 0; i < out.elementCount(); ++i) {
        const QPointF p = map(QPointF(path.elementAt(i)));
        out.setElementPositionAt(i, p.x(), p.y());
    }
    return out;
}

/*!
    Returns the bounding rectangle of \a rect mapped by the transformation.
*/
QRectF QTransform::mapRect(const QRectF &rect) const {
    const QPointF corners[4] = {map(rect.topLeft()), map(rect.topRight()), map(rect.bottomLeft()),
                                map(rect.bottomRight())};
    qreal x0 = corners[0].x(), x1 = x0, y0 = corners[0].y(), y1 = y0;
    for (const QPointF &c : corners) {
        x0 = std::min(x0, c.x());
        x1 = std::max(x1, c.x());
        y0 = std::min(y0, c.y());
        y1 = std::max(y1, c.y());
    }
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

/*!
    \overload

    Returns the bounding rectangle of \a rect mapped by the transformation, rounded to integers.
*/
QRect QTransform::mapRect(const QRect &rect) const { return mapRect(QRectF(rect)).toRect(); }

/*!
    Returns the four corners of \a rect mapped by the transformation, as a polygon.
*/
QPolygon QTransform::mapToPolygon(const QRect &rect) const {
    const QRectF r(rect);
    return QPolygon({map(r.topLeft()).toPoint(), map(r.topRight()).toPoint(), map(r.bottomRight()).toPoint(),
                     map(r.bottomLeft()).toPoint()});
}

/*!
    Makes this transformation this one followed by \a other, and returns a reference to it.
*/
QTransform &QTransform::operator*=(const QTransform &other) {
    *this = *this * other;
    return *this;
}

/*!
    Returns this transformation followed by \a other.
*/
QTransform QTransform::operator*(const QTransform &other) const {
    const QTransform &o = other;
    return QTransform(a11 * o.a11 + a12 * o.a21 + a13 * o.a31, a11 * o.a12 + a12 * o.a22 + a13 * o.a32,
                      a11 * o.a13 + a12 * o.a23 + a13 * o.a33, a21 * o.a11 + a22 * o.a21 + a23 * o.a31,
                      a21 * o.a12 + a22 * o.a22 + a23 * o.a32, a21 * o.a13 + a22 * o.a23 + a23 * o.a33,
                      a31 * o.a11 + a32 * o.a21 + a33 * o.a31, a31 * o.a12 + a32 * o.a22 + a33 * o.a32,
                      a31 * o.a13 + a32 * o.a23 + a33 * o.a33);
}

/*!
    Multiplies each element by \a factor and returns a reference to the transformation.
*/
QTransform &QTransform::operator*=(qreal factor) {
    for (qreal *e : {&a11, &a12, &a13, &a21, &a22, &a23, &a31, &a32, &a33})
        *e *= factor;
    return *this;
}

/*!
    Divides each element by \a divisor and returns a reference to the transformation.
*/
QTransform &QTransform::operator/=(qreal divisor) {
    for (qreal *e : {&a11, &a12, &a13, &a21, &a22, &a23, &a31, &a32, &a33})
        *e /= divisor;
    return *this;
}

/*!
    Adds \a value to each element and returns a reference to the transformation.
*/
QTransform &QTransform::operator+=(qreal value) {
    for (qreal *e : {&a11, &a12, &a13, &a21, &a22, &a23, &a31, &a32, &a33})
        *e += value;
    return *this;
}

/*!
    Subtracts \a value from each element and returns a reference to the transformation.
*/
QTransform &QTransform::operator-=(qreal value) {
    for (qreal *e : {&a11, &a12, &a13, &a21, &a22, &a23, &a31, &a32, &a33})
        *e -= value;
    return *this;
}

// The global operators map with the transformation on the right, as Qt writes them.
QPointF operator*(const QPointF &point, const QTransform &matrix) { return matrix.map(point); }
QPoint operator*(const QPoint &point, const QTransform &matrix) { return matrix.map(point); }
QLineF operator*(const QLineF &line, const QTransform &matrix) { return matrix.map(line); }
QPolygonF operator*(const QPolygonF &polygon, const QTransform &matrix) { return matrix.map(polygon); }
QPainterPath operator*(const QPainterPath &path, const QTransform &matrix) { return matrix.map(path); }
