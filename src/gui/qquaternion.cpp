// MIT License - see LICENSE
#include <QtGui/qquaternion.h>

#include "qmath3d_p.h"

#include <algorithm>
#include <cmath>

using QtPrivate::fuzzyCompare;
using QtPrivate::fuzzyIsNull;

namespace {

constexpr double kPi = 3.14159265358979323846;

float toRadians(float degrees) { return float(double(degrees) * kPi / 180.0); }
float toDegrees(float radians) { return float(double(radians) * 180.0 / kPi); }

// The quaternion of the rotation matrix r (r[row][column], orthonormal).
QQuaternion fromRotation(const float r[3][3]) {
    const float trace = r[0][0] + r[1][1] + r[2][2];
    if (trace > 0.0f) {
        const float s = 2.0f * std::sqrt(trace + 1.0f);
        return QQuaternion(0.25f * s, (r[2][1] - r[1][2]) / s, (r[0][2] - r[2][0]) / s, (r[1][0] - r[0][1]) / s);
    }
    if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
        const float s = 2.0f * std::sqrt(1.0f + r[0][0] - r[1][1] - r[2][2]);
        return QQuaternion((r[2][1] - r[1][2]) / s, 0.25f * s, (r[0][1] + r[1][0]) / s, (r[0][2] + r[2][0]) / s);
    }
    if (r[1][1] > r[2][2]) {
        const float s = 2.0f * std::sqrt(1.0f + r[1][1] - r[0][0] - r[2][2]);
        return QQuaternion((r[0][2] - r[2][0]) / s, (r[0][1] + r[1][0]) / s, 0.25f * s, (r[1][2] + r[2][1]) / s);
    }
    const float s = 2.0f * std::sqrt(1.0f + r[2][2] - r[0][0] - r[1][1]);
    return QQuaternion((r[1][0] - r[0][1]) / s, (r[0][2] + r[2][0]) / s, (r[1][2] + r[2][1]) / s, 0.25f * s);
}

} // namespace

/*!
    \class QQuaternion
    \brief The QQuaternion class represents a quaternion, a rotation in 3D space.

    A quaternion has a scalar part and a vector part, (scalar, x, y, z). A unit quaternion is a
    rotation; multiplying two applies the right-hand one first, and \c{q * v} rotates a QVector3D:

    \code
    QQuaternion yaw = QQuaternion::fromAxisAndAngle(0, 1, 0, 90);
    QVector3D v = yaw * QVector3D(1, 0, 0);       // (0, 0, -1)
    QQuaternion half = QQuaternion::slerp(QQuaternion(), yaw, 0.5f);
    \endcode

    Angles are in degrees. The Euler angles are a pitch about x, a yaw about y and a roll about z,
    applied roll first, then pitch, then yaw.

    \differences
    There is no \c toRotationMatrix() or \c fromRotationMatrix(), since there is no QMatrix3x3; use
    QMatrix4x4::rotate(), getAxes() or fromAxes(). There is no QDataStream or QDebug streaming and no
    QVariant conversion.
    \enddifferences

    \sa QMatrix4x4, QVector3D
*/

/*!
    \fn QQuaternion::QQuaternion()

    Constructs the identity quaternion (1, 0, 0, 0), which is no rotation.
*/

/*!
    \fn QQuaternion::QQuaternion(float scalar, float xpos, float ypos, float zpos)

    Constructs a quaternion with the scalar part \a scalar and the vector part (\a xpos, \a ypos, \a zpos).
*/

/*!
    \fn QQuaternion::QQuaternion(float scalar, const QVector3D &vector)

    Constructs a quaternion with the scalar part \a scalar and the vector part \a vector.
*/

/*!
    \fn QQuaternion::QQuaternion(const QVector4D &vector)

    Constructs a quaternion from \a vector: x, y and z become the vector part and w the scalar part.

    \sa toVector4D()
*/

/*!
    \fn bool QQuaternion::isNull() const

    Returns \c true if all four components are 0; otherwise returns \c false.
*/

/*!
    \fn bool QQuaternion::isIdentity() const

    Returns \c true if this is the identity quaternion (1, 0, 0, 0); otherwise returns \c false.
*/

/*!
    \fn QVector3D QQuaternion::vector() const

    Returns the vector part (x, y, z).

    \sa setVector(), scalar()
*/

/*!
    \fn void QQuaternion::setVector(const QVector3D &vector)

    Sets the vector part to \a vector.

    \sa vector(), setScalar()
*/

/*!
    \fn void QQuaternion::setVector(float x, float y, float z)

    \overload

    Sets the vector part to (\a x, \a y, \a z).
*/

/*!
    \fn float QQuaternion::x() const

    Returns the x component of the vector part.
*/

/*!
    \fn float QQuaternion::y() const

    Returns the y component of the vector part.
*/

/*!
    \fn float QQuaternion::z() const

    Returns the z component of the vector part.
*/

/*!
    \fn float QQuaternion::scalar() const

    Returns the scalar part.
*/

/*!
    \fn void QQuaternion::setX(float x)

    Sets the x component of the vector part to \a x.
*/

/*!
    \fn void QQuaternion::setY(float y)

    Sets the y component of the vector part to \a y.
*/

/*!
    \fn void QQuaternion::setZ(float z)

    Sets the z component of the vector part to \a z.
*/

/*!
    \fn void QQuaternion::setScalar(float scalar)

    Sets the scalar part to \a scalar.
*/

/*!
    \fn float QQuaternion::dotProduct(const QQuaternion &q1, const QQuaternion &q2)

    Returns the dot product of \a q1 and \a q2.
*/

/*!
    \fn float QQuaternion::lengthSquared() const

    Returns the squared length of the quaternion.

    \sa length()
*/

/*!
    \fn QQuaternion QQuaternion::conjugated() const

    Returns the conjugate, with the vector part negated. For a unit quaternion it is the inverse rotation.

    \sa inverted()
*/

/*!
    \fn QQuaternion &QQuaternion::operator+=(const QQuaternion &q)

    Adds \a q to this quaternion, component by component, and returns a reference to it.
*/

/*!
    \fn QQuaternion &QQuaternion::operator-=(const QQuaternion &q)

    Subtracts \a q from this quaternion, component by component, and returns a reference to it.
*/

/*!
    \fn QQuaternion &QQuaternion::operator*=(float factor)

    Multiplies every component by \a factor and returns a reference to this quaternion.
*/

/*!
    \fn QQuaternion &QQuaternion::operator*=(const QQuaternion &q)

    \overload

    Sets this quaternion to the product of itself and \a q, so that the rotation \a q applies first, and
    returns a reference to it.
*/

/*!
    \fn QQuaternion &QQuaternion::operator/=(float divisor)

    Divides every component by \a divisor and returns a reference to this quaternion.
*/

/*!
    \fn QVector4D QQuaternion::toVector4D() const

    Returns the quaternion as a QVector4D, with the vector part in x, y and z and the scalar part in w.
*/

/*!
    Returns the length of the quaternion.

    \sa lengthSquared(), normalized()
*/
float QQuaternion::length() const noexcept {
    return float(std::sqrt(double(wp) * double(wp) + double(xp) * double(xp) + double(yp) * double(yp) +
                           double(zp) * double(zp)));
}

/*!
    Returns the quaternion scaled to length 1. One whose length is already about 1 is returned
    unchanged, and one too short to normalize gives (0, 0, 0, 0).

    \sa normalize()
*/
QQuaternion QQuaternion::normalized() const noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f))
        return *this;
    if (fuzzyIsNull(len))
        return QQuaternion(0.0f, 0.0f, 0.0f, 0.0f);
    return *this / len;
}

/*!
    Scales the quaternion to length 1, in place. One too short to normalize is left unchanged.

    \sa normalized()
*/
void QQuaternion::normalize() noexcept {
    const float len = length();
    if (fuzzyIsNull(len - 1.0f) || fuzzyIsNull(len))
        return;
    *this /= len;
}

/*!
    Returns the inverse, so that \c{q * q.inverted()} is the identity, or (0, 0, 0, 0) for a null quaternion.

    \sa conjugated()
*/
QQuaternion QQuaternion::inverted() const noexcept {
    const float len = lengthSquared();
    if (fuzzyIsNull(len))
        return QQuaternion(0.0f, 0.0f, 0.0f, 0.0f);
    return QQuaternion(wp / len, -xp / len, -yp / len, -zp / len);
}

/*!
    Returns \a vector rotated by this quaternion, which should be a unit quaternion. It is the same
    as \c{q * vector}.
*/
QVector3D QQuaternion::rotatedVector(const QVector3D &vector) const noexcept {
    return (*this * QQuaternion(0.0f, vector) * conjugated()).vector();
}

bool qFuzzyCompare(const QQuaternion &q1, const QQuaternion &q2) noexcept {
    return fuzzyCompare(q1.wp, q2.wp) && fuzzyCompare(q1.xp, q2.xp) && fuzzyCompare(q1.yp, q2.yp) &&
           fuzzyCompare(q1.zp, q2.zp);
}

/*!
    Sets \a axis and \a angle (in degrees) to the rotation this quaternion makes. The identity gives a
    null axis and an angle of 0.

    \sa fromAxisAndAngle()
*/
void QQuaternion::getAxisAndAngle(QVector3D *axis, float *angle) const {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    getAxisAndAngle(&x, &y, &z, angle);
    if (axis)
        *axis = QVector3D(x, y, z);
}

/*!
    \overload

    Sets the axis (\a x, \a y, \a z) and \a angle (in degrees) of the rotation this quaternion makes.
*/
void QQuaternion::getAxisAndAngle(float *x, float *y, float *z, float *angle) const {
    const float len = vector().length();
    float ax = 0.0f, ay = 0.0f, az = 0.0f, a = 0.0f;
    if (!fuzzyIsNull(len)) {
        ax = xp / len;
        ay = yp / len;
        az = zp / len;
        a = toDegrees(2.0f * std::atan2(len, wp));
    }
    if (x)
        *x = ax;
    if (y)
        *y = ay;
    if (z)
        *z = az;
    if (angle)
        *angle = a;
}

/*!
    Returns the quaternion that rotates by \a angle degrees counterclockwise about \a axis (looking
    from its tip toward the origin).

    \sa getAxisAndAngle()
*/
QQuaternion QQuaternion::fromAxisAndAngle(const QVector3D &axis, float angle) {
    const QVector3D a = axis.normalized();
    const float half = toRadians(angle) / 2.0f;
    return QQuaternion(std::cos(half), a * std::sin(half)).normalized();
}

/*!
    \overload

    Returns the quaternion that rotates by \a angle degrees about the axis (\a x, \a y, \a z).
*/
QQuaternion QQuaternion::fromAxisAndAngle(float x, float y, float z, float angle) {
    return fromAxisAndAngle(QVector3D(x, y, z), angle);
}

/*!
    Returns the Euler angles of this rotation, in degrees, as (pitch, yaw, roll).

    \sa getEulerAngles(), fromEulerAngles()
*/
QVector3D QQuaternion::toEulerAngles() const {
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
    getEulerAngles(&pitch, &yaw, &roll);
    return QVector3D(pitch, yaw, roll);
}

/*!
    Sets \a pitch (about x), \a yaw (about y) and \a roll (about z) to the Euler angles of this
    rotation, in degrees. At a pitch of +-90 degrees, where yaw and roll turn about the same axis,
    the roll is 0.

    \sa toEulerAngles(), fromEulerAngles()
*/
void QQuaternion::getEulerAngles(float *pitch, float *yaw, float *roll) const {
    float x = xp, y = yp, z = zp, w = wp;
    const float len2 = lengthSquared();
    if (!fuzzyIsNull(len2 - 1.0f) && !fuzzyIsNull(len2)) {
        const float len = std::sqrt(len2);
        x /= len;
        y /= len;
        z /= len;
        w /= len;
    }
    const float xx = x * x, xy = x * y, xz = x * z, xw = x * w;
    const float yy = y * y, yz = y * z, yw = y * w;
    const float zz = z * z, zw = z * w;

    // The pitch from atan2 of its sine and cosine stays accurate near +-90 degrees, where asin does not.
    const float sinPitch = -2.0f * (yz - xw);
    const float cosPitch = std::hypot(2.0f * (xz + yw), 1.0f - 2.0f * (xx + yy));
    const float p = std::atan2(sinPitch, cosPitch);
    float yw2 = 0.0f, r = 0.0f;
    if (cosPitch > 0.00001f) {
        yw2 = std::atan2(2.0f * (xz + yw), 1.0f - 2.0f * (xx + yy));
        r = std::atan2(2.0f * (xy + zw), 1.0f - 2.0f * (xx + zz));
    } else {
        // Gimbal lock: yaw and roll turn about the same axis, so put it all in yaw.
        yw2 = std::atan2((sinPitch > 0.0f ? 2.0f : -2.0f) * (xy - zw), 1.0f - 2.0f * (yy + zz));
    }
    if (pitch)
        *pitch = toDegrees(p);
    if (yaw)
        *yaw = toDegrees(yw2);
    if (roll)
        *roll = toDegrees(r);
}

/*!
    Returns the rotation with the Euler \a angles (pitch, yaw, roll), in degrees.

    \sa toEulerAngles()
*/
QQuaternion QQuaternion::fromEulerAngles(const QVector3D &angles) {
    return fromEulerAngles(angles.x(), angles.y(), angles.z());
}

/*!
    \overload

    Returns the rotation that turns by \a roll degrees about z, then \a pitch about x, then \a yaw about y.
*/
QQuaternion QQuaternion::fromEulerAngles(float pitch, float yaw, float roll) {
    const float p = toRadians(pitch) * 0.5f, y = toRadians(yaw) * 0.5f, r = toRadians(roll) * 0.5f;
    const float c1 = std::cos(y), s1 = std::sin(y);
    const float c2 = std::cos(r), s2 = std::sin(r);
    const float c3 = std::cos(p), s3 = std::sin(p);
    const float c1c2 = c1 * c2, s1s2 = s1 * s2;
    return QQuaternion(c1c2 * c3 + s1s2 * s3, c1c2 * s3 + s1s2 * c3, s1 * c2 * c3 - c1 * s2 * s3,
                       c1 * s2 * c3 - s1 * c2 * s3);
}

/*!
    Sets \a xAxis, \a yAxis and \a zAxis to the axes of the coordinate system this rotation turns the
    standard one into: the columns of its rotation matrix.

    \sa fromAxes()
*/
void QQuaternion::getAxes(QVector3D *xAxis, QVector3D *yAxis, QVector3D *zAxis) const {
    const QQuaternion q = normalized();
    const float x = q.xp, y = q.yp, z = q.zp, w = q.wp;
    if (xAxis)
        *xAxis = QVector3D(1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y + w * z), 2.0f * (x * z - w * y));
    if (yAxis)
        *yAxis = QVector3D(2.0f * (x * y - w * z), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + w * x));
    if (zAxis)
        *zAxis = QVector3D(2.0f * (x * z + w * y), 2.0f * (y * z - w * x), 1.0f - 2.0f * (x * x + y * y));
}

/*!
    Returns the rotation that turns the standard axes into \a xAxis, \a yAxis and \a zAxis, which must
    be orthonormal and right-handed.

    \sa getAxes(), fromDirection()
*/
QQuaternion QQuaternion::fromAxes(const QVector3D &xAxis, const QVector3D &yAxis, const QVector3D &zAxis) {
    const float r[3][3] = {
        {xAxis.x(), yAxis.x(), zAxis.x()}, {xAxis.y(), yAxis.y(), zAxis.y()}, {xAxis.z(), yAxis.z(), zAxis.z()}};
    return fromRotation(r).normalized();
}

/*!
    Returns the rotation that turns the z axis toward \a direction, with the y axis as close to \a up as
    it can be. A null \a direction gives the identity; an \a up parallel to \a direction gives the
    shortest rotation from z.

    \sa fromAxes(), rotationTo()
*/
QQuaternion QQuaternion::fromDirection(const QVector3D &direction, const QVector3D &up) {
    if (direction.isNull())
        return QQuaternion();
    const QVector3D zAxis = direction.normalized();
    QVector3D xAxis = QVector3D::crossProduct(up, zAxis);
    if (fuzzyIsNull(xAxis.lengthSquared()))
        return rotationTo(QVector3D(0.0f, 0.0f, 1.0f), zAxis);
    xAxis.normalize();
    const QVector3D yAxis = QVector3D::crossProduct(zAxis, xAxis);
    return fromAxes(xAxis, yAxis, zAxis);
}

/*!
    Returns the shortest rotation that turns the direction \a from into \a to.

    \sa fromDirection()
*/
QQuaternion QQuaternion::rotationTo(const QVector3D &from, const QVector3D &to) {
    const QVector3D v0 = from.normalized();
    const QVector3D v1 = to.normalized();
    float d = QVector3D::dotProduct(v0, v1) + 1.0f;
    if (fuzzyIsNull(d)) {
        // Opposite directions: turn half way round any axis perpendicular to them.
        QVector3D axis = QVector3D::crossProduct(QVector3D(1.0f, 0.0f, 0.0f), v0);
        if (fuzzyIsNull(axis.lengthSquared()))
            axis = QVector3D::crossProduct(QVector3D(0.0f, 1.0f, 0.0f), v0);
        axis.normalize();
        return QQuaternion(0.0f, axis);
    }
    d = std::sqrt(2.0f * d);
    return QQuaternion(d * 0.5f, QVector3D::crossProduct(v0, v1) / d).normalized();
}

/*!
    Returns the spherical linear interpolation between the rotations \a q1 and \a q2 at \a t, from 0
    (\a q1) to 1 (\a q2), along the shorter arc. The rotation turns at a constant rate as \a t moves.

    \sa nlerp()
*/
QQuaternion QQuaternion::slerp(const QQuaternion &q1, const QQuaternion &q2, float t) {
    if (t <= 0.0f)
        return q1;
    if (t >= 1.0f)
        return q2;
    QQuaternion q2b = q2;
    float dot = dotProduct(q1, q2);
    if (dot < 0.0f) {
        q2b = -q2b;
        dot = -dot;
    }
    float factor1 = 1.0f - t;
    float factor2 = t;
    if (1.0f - dot > 0.0000001f) {
        const float angle = std::acos(std::min(dot, 1.0f));
        const float sinOfAngle = std::sin(angle);
        if (sinOfAngle > 0.0000001f) {
            factor1 = std::sin((1.0f - t) * angle) / sinOfAngle;
            factor2 = std::sin(t * angle) / sinOfAngle;
        }
    }
    return q1 * factor1 + q2b * factor2;
}

/*!
    Returns the normalized linear interpolation between the rotations \a q1 and \a q2 at \a t, from 0
    to 1, along the shorter arc. It is cheaper than slerp() but does not turn at a constant rate.

    \sa slerp()
*/
QQuaternion QQuaternion::nlerp(const QQuaternion &q1, const QQuaternion &q2, float t) {
    if (t <= 0.0f)
        return q1;
    if (t >= 1.0f)
        return q2;
    const QQuaternion q2b = dotProduct(q1, q2) >= 0.0f ? q2 : -q2;
    return (q1 * (1.0f - t) + q2b * t).normalized();
}
