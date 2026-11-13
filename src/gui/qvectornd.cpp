// MIT License - see LICENSE
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qvectornd.h>

#include "qmath3d_p.h"

#include <cmath>

using QtPrivate::fuzzyCompare;
using QtPrivate::fuzzyIsNull;

namespace {

// Lengths are summed in double so that large coordinates do not overflow.
float lengthOf(const float *v, int n) noexcept {
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += double(v[i]) * double(v[i]);
    return float(std::sqrt(sum));
}

} // namespace

/*!
    \class QVector2D
    \brief The QVector2D class represents a vector or vertex in 2D space.

    A QVector2D holds two \c float coordinates. Vectors add, subtract and scale with the usual
    operators, and \c{v1 * v2} multiplies coordinate by coordinate:

    \code
    QVector2D a(3, 4);
    float len = a.length();                       // 5
    QVector2D unit = a.normalized();              // (0.6, 0.8)
    float d = QVector2D::dotProduct(a, unit);     // 5
    \endcode

    \c operator==() compares exactly; \c qFuzzyCompare() allows for rounding.

    \differences
    There is no QDataStream or QDebug streaming and no QVariant conversion.
    \enddifferences

    \sa QVector3D, QVector4D
*/

/*!
    \class QVector3D
    \brief The QVector3D class represents a vector or vertex in 3D space.

    A QVector3D holds three \c float coordinates. Besides arithmetic it has the dot and cross products,
    normals, distances to points, lines and planes, and project() and unproject() for window coordinates:

    \code
    QVector3D n = QVector3D::normal(QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(0, 1, 0));
    // n == (0, 0, 1)
    \endcode

    QMatrix4x4 and QQuaternion transform vectors.

    \differences
    There is no QDataStream or QDebug streaming and no QVariant conversion.
    \enddifferences

    \sa QVector2D, QVector4D, QMatrix4x4, QQuaternion
*/

/*!
    \class QVector4D
    \brief The QVector4D class represents a vector or vertex in 4D space.

    A QVector4D holds four \c float coordinates, usually homogeneous coordinates or an RGBA color.
    toVector3DAffine() divides by w.

    \differences
    There is no QDataStream or QDebug streaming and no QVariant conversion.
    \enddifferences

    \sa QVector2D, QVector3D, QMatrix4x4
*/

/*!
    Returns the length of the vector from the origin.

    \sa lengthSquared(), normalized()
*/
float QVector2D::length() const noexcept { return lengthOf(v, 2); }

/*!
    Returns the vector scaled to length 1. A vector whose length is already about 1 is returned
    unchanged, and a vector too short to normalize gives a null vector.

    \sa length(), normalize()
*/
QVector2D QVector2D::normalized() const noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f))
        return *this;
    if (fuzzyIsNull(len))
        return QVector2D();
    return *this / len;
}

/*!
    Scales the vector to length 1, in place. A vector too short to normalize is left unchanged.

    \sa normalized()
*/
void QVector2D::normalize() noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f) || fuzzyIsNull(len))
        return;
    *this /= len;
}

/*!
    Returns the distance from this vertex to \a point.
*/
float QVector2D::distanceToPoint(QVector2D point) const noexcept { return (*this - point).length(); }

/*!
    Returns the distance from this vertex to the line through \a point along \a direction, which must
    be a unit vector. A null \a direction gives the distance to \a point.
*/
float QVector2D::distanceToLine(QVector2D point, QVector2D direction) const noexcept {
    if (direction.isNull())
        return (*this - point).length();
    const QVector2D p = point + dotProduct(*this - point, direction) * direction;
    return (*this - p).length();
}

bool qFuzzyCompare(QVector2D v1, QVector2D v2) noexcept {
    return fuzzyCompare(v1.v[0], v2.v[0]) && fuzzyCompare(v1.v[1], v2.v[1]);
}

/*!
    Returns the length of the vector from the origin.

    \sa lengthSquared(), normalized()
*/
float QVector3D::length() const noexcept { return lengthOf(v, 3); }

/*!
    Returns the vector scaled to length 1. A vector whose length is already about 1 is returned
    unchanged, and a vector too short to normalize gives a null vector.

    \sa length(), normalize()
*/
QVector3D QVector3D::normalized() const noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f))
        return *this;
    if (fuzzyIsNull(len))
        return QVector3D();
    return *this / len;
}

/*!
    Scales the vector to length 1, in place. A vector too short to normalize is left unchanged.

    \sa normalized()
*/
void QVector3D::normalize() noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f) || fuzzyIsNull(len))
        return;
    *this /= len;
}

/*!
    Returns the unit normal of the plane spanned by \a v1 and \a v2: their cross product, normalized.

    \sa crossProduct()
*/
QVector3D QVector3D::normal(QVector3D v1, QVector3D v2) noexcept { return crossProduct(v1, v2).normalized(); }

/*!
    \overload

    Returns the unit normal of the triangle \a v1, \a v2, \a v3, facing the side from which the
    vertices run counterclockwise.
*/
QVector3D QVector3D::normal(QVector3D v1, QVector3D v2, QVector3D v3) noexcept {
    return crossProduct(v2 - v1, v3 - v1).normalized();
}

/*!
    Returns the window coordinates of this point, transformed by \a modelView and \a projection and
    mapped into \a viewport. The z coordinate is the depth, from 0 at the near plane to 1 at the far plane.

    \sa unproject()
*/
QVector3D QVector3D::project(const QMatrix4x4 &modelView, const QMatrix4x4 &projection, const QRect &viewport) const {
    QVector4D tmp = projection * modelView * QVector4D(*this, 1.0f);
    if (fuzzyIsNull(tmp.w()))
        tmp.setW(1.0f);
    tmp /= tmp.w();
    tmp = tmp * 0.5f + QVector4D(0.5f, 0.5f, 0.5f, 0.5f);
    tmp.setX(tmp.x() * float(viewport.width()) + float(viewport.x()));
    tmp.setY(tmp.y() * float(viewport.height()) + float(viewport.y()));
    return tmp.toVector3D();
}

/*!
    Returns the object coordinates of this point in window coordinates: the inverse of project() with
    the same \a modelView, \a projection and \a viewport.

    \sa project()
*/
QVector3D QVector3D::unproject(const QMatrix4x4 &modelView, const QMatrix4x4 &projection, const QRect &viewport) const {
    const QMatrix4x4 inverse = (projection * modelView).inverted();
    QVector4D tmp(*this, 1.0f);
    tmp.setX((tmp.x() - float(viewport.x())) / float(viewport.width()));
    tmp.setY((tmp.y() - float(viewport.y())) / float(viewport.height()));
    tmp = tmp * 2.0f - QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
    QVector4D obj = inverse * tmp;
    if (fuzzyIsNull(obj.w()))
        obj.setW(1.0f);
    obj /= obj.w();
    return obj.toVector3D();
}

/*!
    Returns the distance from this vertex to \a point.
*/
float QVector3D::distanceToPoint(QVector3D point) const noexcept { return (*this - point).length(); }

/*!
    Returns the signed distance from this vertex to the plane through \a plane with unit \a normal.
    It is positive on the side \a normal points to.
*/
float QVector3D::distanceToPlane(QVector3D plane, QVector3D normal) const noexcept {
    return dotProduct(*this - plane, normal);
}

/*!
    \overload

    Returns the signed distance from this vertex to the plane through \a plane1, \a plane2 and
    \a plane3. It is positive on the side from which the three points run counterclockwise.
*/
float QVector3D::distanceToPlane(QVector3D plane1, QVector3D plane2, QVector3D plane3) const noexcept {
    return dotProduct(*this - plane1, normal(plane2 - plane1, plane3 - plane1));
}

/*!
    Returns the distance from this vertex to the line through \a point along \a direction, which must
    be a unit vector. A null \a direction gives the distance to \a point.
*/
float QVector3D::distanceToLine(QVector3D point, QVector3D direction) const noexcept {
    if (direction.isNull())
        return (*this - point).length();
    const QVector3D p = point + dotProduct(*this - point, direction) * direction;
    return (*this - p).length();
}

bool qFuzzyCompare(QVector3D v1, QVector3D v2) noexcept {
    return fuzzyCompare(v1.v[0], v2.v[0]) && fuzzyCompare(v1.v[1], v2.v[1]) && fuzzyCompare(v1.v[2], v2.v[2]);
}

/*!
    Returns the length of the vector from the origin.

    \sa lengthSquared(), normalized()
*/
float QVector4D::length() const noexcept { return lengthOf(v, 4); }

/*!
    Returns the vector scaled to length 1. A vector whose length is already about 1 is returned
    unchanged, and a vector too short to normalize gives a null vector.

    \sa length(), normalize()
*/
QVector4D QVector4D::normalized() const noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f))
        return *this;
    if (fuzzyIsNull(len))
        return QVector4D();
    return *this / len;
}

/*!
    Scales the vector to length 1, in place. A vector too short to normalize is left unchanged.

    \sa normalized()
*/
void QVector4D::normalize() noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f) || fuzzyIsNull(len))
        return;
    *this /= len;
}

bool qFuzzyCompare(QVector4D v1, QVector4D v2) noexcept {
    return fuzzyCompare(v1.v[0], v2.v[0]) && fuzzyCompare(v1.v[1], v2.v[1]) && fuzzyCompare(v1.v[2], v2.v[2]) &&
           fuzzyCompare(v1.v[3], v2.v[3]);
}

/*!
    \fn QVector2D::QVector2D()

    Constructs a null vector, with all coordinates 0.
*/

/*!
    \fn QVector2D::QVector2D(float xpos, float ypos)

    Constructs a vector with coordinates (\a xpos, \a ypos).
*/

/*!
    \fn QVector2D::QVector2D(QPoint point)

    Constructs a vector with the x and y coordinates of \a point.
*/

/*!
    \fn QVector2D::QVector2D(QPointF point)

    Constructs a vector with the x and y coordinates of \a point.
*/

/*!
    \fn QVector2D::QVector2D(QVector3D vector)

    Constructs a vector from the first 2 coordinates of the 3-D \a vector.
*/

/*!
    \fn QVector2D::QVector2D(QVector4D vector)

    Constructs a vector from the first 2 coordinates of the 4-D \a vector.
*/

/*!
    \fn bool QVector2D::isNull() const

    Returns \c true if every coordinate is 0.0 (or -0.0); otherwise returns \c false.
*/

/*!
    \fn float QVector2D::x() const

    Returns the x coordinate.

    \sa setX()
*/

/*!
    \fn void QVector2D::setX(float x)

    Sets the x coordinate to \a x.

    \sa x()
*/

/*!
    \fn float QVector2D::y() const

    Returns the y coordinate.

    \sa setY()
*/

/*!
    \fn void QVector2D::setY(float y)

    Sets the y coordinate to \a y.

    \sa y()
*/

/*!
    \fn float &QVector2D::operator[](int i)

    Returns a reference to coordinate \a i, where 0 is x, 1 is y and so on.
*/

/*!
    \fn float QVector2D::operator[](int i) const

    Returns coordinate \a i, where 0 is x, 1 is y and so on.
*/

/*!
    \fn float QVector2D::lengthSquared() const

    Returns the squared length of the vector, which is cheaper to compute than length().

    \sa length(), dotProduct()
*/

/*!
    \fn QVector2D &QVector2D::operator+=(QVector2D vector)

    Adds \a vector to this vector, coordinate by coordinate, and returns a reference to this vector.
*/

/*!
    \fn QVector2D &QVector2D::operator-=(QVector2D vector)

    Subtracts \a vector from this vector, coordinate by coordinate, and returns a reference to this vector.
*/

/*!
    \fn QVector2D &QVector2D::operator*=(float factor)

    Multiplies every coordinate by \a factor and returns a reference to this vector.
*/

/*!
    \fn QVector2D &QVector2D::operator*=(QVector2D vector)

    \overload

    Multiplies each coordinate by the matching coordinate of \a vector and returns a reference to this vector.
*/

/*!
    \fn QVector2D &QVector2D::operator/=(float divisor)

    Divides every coordinate by \a divisor and returns a reference to this vector.
*/

/*!
    \fn QVector2D &QVector2D::operator/=(QVector2D vector)

    \overload

    Divides each coordinate by the matching coordinate of \a vector and returns a reference to this vector.
*/

/*!
    \fn float QVector2D::dotProduct(QVector2D v1, QVector2D v2)

    Returns the dot product of \a v1 and \a v2.
*/

/*!
    \fn QVector3D QVector2D::toVector3D() const

    Returns this vector as a QVector3D, with the remaining coordinates 0.
*/

/*!
    \fn QVector4D QVector2D::toVector4D() const

    Returns this vector as a QVector4D, with the remaining coordinates 0.
*/

/*!
    \fn QPoint QVector2D::toPoint() const

    Returns the x and y coordinates as a QPoint, rounded to the nearest integers.

    \sa toPointF()
*/

/*!
    \fn QPointF QVector2D::toPointF() const

    Returns the x and y coordinates as a QPointF.

    \sa toPoint()
*/

/*!
    \fn QVector3D::QVector3D()

    Constructs a null vector, with all coordinates 0.
*/

/*!
    \fn QVector3D::QVector3D(float xpos, float ypos, float zpos)

    Constructs a vector with coordinates (\a xpos, \a ypos, \a zpos).
*/

/*!
    \fn QVector3D::QVector3D(QPoint point)

    Constructs a vector with the x and y coordinates of \a point and the remaining coordinates 0.
*/

/*!
    \fn QVector3D::QVector3D(QPointF point)

    Constructs a vector with the x and y coordinates of \a point and the remaining coordinates 0.
*/

/*!
    \fn QVector3D::QVector3D(QVector2D vector)

    Constructs a vector from the coordinates of the 2-D \a vector; the remaining coordinates are 0.
*/

/*!
    \fn QVector3D::QVector3D(QVector4D vector)

    Constructs a vector from the first 3 coordinates of the 4-D \a vector.
*/

/*!
    \fn QVector3D::QVector3D(QVector2D vector, float zpos)

    Constructs a vector from the x and y coordinates of \a vector and the z coordinate \a zpos.
*/

/*!
    \fn bool QVector3D::isNull() const

    Returns \c true if every coordinate is 0.0 (or -0.0); otherwise returns \c false.
*/

/*!
    \fn float QVector3D::x() const

    Returns the x coordinate.

    \sa setX()
*/

/*!
    \fn void QVector3D::setX(float x)

    Sets the x coordinate to \a x.

    \sa x()
*/

/*!
    \fn float QVector3D::y() const

    Returns the y coordinate.

    \sa setY()
*/

/*!
    \fn void QVector3D::setY(float y)

    Sets the y coordinate to \a y.

    \sa y()
*/

/*!
    \fn float QVector3D::z() const

    Returns the z coordinate.

    \sa setZ()
*/

/*!
    \fn void QVector3D::setZ(float z)

    Sets the z coordinate to \a z.

    \sa z()
*/

/*!
    \fn float &QVector3D::operator[](int i)

    Returns a reference to coordinate \a i, where 0 is x, 1 is y and so on.
*/

/*!
    \fn float QVector3D::operator[](int i) const

    Returns coordinate \a i, where 0 is x, 1 is y and so on.
*/

/*!
    \fn float QVector3D::lengthSquared() const

    Returns the squared length of the vector, which is cheaper to compute than length().

    \sa length(), dotProduct()
*/

/*!
    \fn QVector3D &QVector3D::operator+=(QVector3D vector)

    Adds \a vector to this vector, coordinate by coordinate, and returns a reference to this vector.
*/

/*!
    \fn QVector3D &QVector3D::operator-=(QVector3D vector)

    Subtracts \a vector from this vector, coordinate by coordinate, and returns a reference to this vector.
*/

/*!
    \fn QVector3D &QVector3D::operator*=(float factor)

    Multiplies every coordinate by \a factor and returns a reference to this vector.
*/

/*!
    \fn QVector3D &QVector3D::operator*=(QVector3D vector)

    \overload

    Multiplies each coordinate by the matching coordinate of \a vector and returns a reference to this vector.
*/

/*!
    \fn QVector3D &QVector3D::operator/=(float divisor)

    Divides every coordinate by \a divisor and returns a reference to this vector.
*/

/*!
    \fn QVector3D &QVector3D::operator/=(QVector3D vector)

    \overload

    Divides each coordinate by the matching coordinate of \a vector and returns a reference to this vector.
*/

/*!
    \fn float QVector3D::dotProduct(QVector3D v1, QVector3D v2)

    Returns the dot product of \a v1 and \a v2.
*/

/*!
    \fn QVector2D QVector3D::toVector2D() const

    Returns the first 2 coordinates as a QVector2D.
*/

/*!
    \fn QVector4D QVector3D::toVector4D() const

    Returns this vector as a QVector4D, with the remaining coordinates 0.
*/

/*!
    \fn QPoint QVector3D::toPoint() const

    Returns the x and y coordinates as a QPoint, rounded to the nearest integers.

    \sa toPointF()
*/

/*!
    \fn QPointF QVector3D::toPointF() const

    Returns the x and y coordinates as a QPointF.

    \sa toPoint()
*/

/*!
    \fn QVector3D QVector3D::crossProduct(QVector3D v1, QVector3D v2)

    Returns the cross product of \a v1 and \a v2, a vector perpendicular to both.

    \sa normal()
*/

/*!
    \fn QVector4D::QVector4D()

    Constructs a null vector, with all coordinates 0.
*/

/*!
    \fn QVector4D::QVector4D(float xpos, float ypos, float zpos, float wpos)

    Constructs a vector with coordinates (\a xpos, \a ypos, \a zpos, \a wpos).
*/

/*!
    \fn QVector4D::QVector4D(QPoint point)

    Constructs a vector with the x and y coordinates of \a point and the remaining coordinates 0.
*/

/*!
    \fn QVector4D::QVector4D(QPointF point)

    Constructs a vector with the x and y coordinates of \a point and the remaining coordinates 0.
*/

/*!
    \fn QVector4D::QVector4D(QVector2D vector)

    Constructs a vector from the coordinates of the 2-D \a vector; the remaining coordinates are 0.
*/

/*!
    \fn QVector4D::QVector4D(QVector3D vector)

    Constructs a vector from the coordinates of the 3-D \a vector; the remaining coordinates are 0.
*/

/*!
    \fn QVector4D::QVector4D(QVector2D vector, float zpos, float wpos)

    Constructs a vector from the x and y coordinates of \a vector, \a zpos and \a wpos.
*/

/*!
    \fn QVector4D::QVector4D(QVector3D vector, float wpos)

    Constructs a vector from the coordinates of \a vector and the w coordinate \a wpos.
*/

/*!
    \fn bool QVector4D::isNull() const

    Returns \c true if every coordinate is 0.0 (or -0.0); otherwise returns \c false.
*/

/*!
    \fn float QVector4D::x() const

    Returns the x coordinate.

    \sa setX()
*/

/*!
    \fn void QVector4D::setX(float x)

    Sets the x coordinate to \a x.

    \sa x()
*/

/*!
    \fn float QVector4D::y() const

    Returns the y coordinate.

    \sa setY()
*/

/*!
    \fn void QVector4D::setY(float y)

    Sets the y coordinate to \a y.

    \sa y()
*/

/*!
    \fn float QVector4D::z() const

    Returns the z coordinate.

    \sa setZ()
*/

/*!
    \fn void QVector4D::setZ(float z)

    Sets the z coordinate to \a z.

    \sa z()
*/

/*!
    \fn float QVector4D::w() const

    Returns the w coordinate.

    \sa setW()
*/

/*!
    \fn void QVector4D::setW(float w)

    Sets the w coordinate to \a w.

    \sa w()
*/

/*!
    \fn float &QVector4D::operator[](int i)

    Returns a reference to coordinate \a i, where 0 is x, 1 is y and so on.
*/

/*!
    \fn float QVector4D::operator[](int i) const

    Returns coordinate \a i, where 0 is x, 1 is y and so on.
*/

/*!
    \fn float QVector4D::lengthSquared() const

    Returns the squared length of the vector, which is cheaper to compute than length().

    \sa length(), dotProduct()
*/

/*!
    \fn QVector4D &QVector4D::operator+=(QVector4D vector)

    Adds \a vector to this vector, coordinate by coordinate, and returns a reference to this vector.
*/

/*!
    \fn QVector4D &QVector4D::operator-=(QVector4D vector)

    Subtracts \a vector from this vector, coordinate by coordinate, and returns a reference to this vector.
*/

/*!
    \fn QVector4D &QVector4D::operator*=(float factor)

    Multiplies every coordinate by \a factor and returns a reference to this vector.
*/

/*!
    \fn QVector4D &QVector4D::operator*=(QVector4D vector)

    \overload

    Multiplies each coordinate by the matching coordinate of \a vector and returns a reference to this vector.
*/

/*!
    \fn QVector4D &QVector4D::operator/=(float divisor)

    Divides every coordinate by \a divisor and returns a reference to this vector.
*/

/*!
    \fn QVector4D &QVector4D::operator/=(QVector4D vector)

    \overload

    Divides each coordinate by the matching coordinate of \a vector and returns a reference to this vector.
*/

/*!
    \fn float QVector4D::dotProduct(QVector4D v1, QVector4D v2)

    Returns the dot product of \a v1 and \a v2.
*/

/*!
    \fn QVector2D QVector4D::toVector2D() const

    Returns the first 2 coordinates as a QVector2D.
*/

/*!
    \fn QVector3D QVector4D::toVector3D() const

    Returns the first 3 coordinates as a QVector3D.
*/

/*!
    \fn QVector2D QVector4D::toVector2DAffine() const

    Returns x and y divided by w, or a null vector if w is 0.
*/

/*!
    \fn QVector3D QVector4D::toVector3DAffine() const

    Returns x, y and z divided by w, or a null vector if w is 0.
*/

/*!
    \fn QPoint QVector4D::toPoint() const

    Returns the x and y coordinates as a QPoint, rounded to the nearest integers.

    \sa toPointF()
*/

/*!
    \fn QPointF QVector4D::toPointF() const

    Returns the x and y coordinates as a QPointF.

    \sa toPoint()
*/
