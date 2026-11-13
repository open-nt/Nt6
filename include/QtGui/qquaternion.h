// MIT License - see LICENSE
#pragma once

#include <QtGui/qvectornd.h>

class QQuaternion {
public:
    constexpr QQuaternion() noexcept : wp(1.0f), xp(0.0f), yp(0.0f), zp(0.0f) {}
    constexpr QQuaternion(float scalar, float xpos, float ypos, float zpos) noexcept
        : wp(scalar), xp(xpos), yp(ypos), zp(zpos) {}
    constexpr QQuaternion(float scalar, const QVector3D &vector) noexcept
        : wp(scalar), xp(vector.x()), yp(vector.y()), zp(vector.z()) {}
    constexpr explicit QQuaternion(const QVector4D &vector) noexcept
        : wp(vector.w()), xp(vector.x()), yp(vector.y()), zp(vector.z()) {}

    constexpr bool isNull() const noexcept { return wp == 0.0f && xp == 0.0f && yp == 0.0f && zp == 0.0f; }
    constexpr bool isIdentity() const noexcept { return wp == 1.0f && xp == 0.0f && yp == 0.0f && zp == 0.0f; }

    constexpr QVector3D vector() const noexcept { return QVector3D(xp, yp, zp); }
    constexpr void setVector(const QVector3D &vector) noexcept {
        xp = vector.x();
        yp = vector.y();
        zp = vector.z();
    }
    constexpr void setVector(float x, float y, float z) noexcept {
        xp = x;
        yp = y;
        zp = z;
    }

    constexpr float x() const noexcept { return xp; }
    constexpr float y() const noexcept { return yp; }
    constexpr float z() const noexcept { return zp; }
    constexpr float scalar() const noexcept { return wp; }
    constexpr void setX(float x) noexcept { xp = x; }
    constexpr void setY(float y) noexcept { yp = y; }
    constexpr void setZ(float z) noexcept { zp = z; }
    constexpr void setScalar(float scalar) noexcept { wp = scalar; }

    static constexpr float dotProduct(const QQuaternion &q1, const QQuaternion &q2) noexcept {
        return q1.wp * q2.wp + q1.xp * q2.xp + q1.yp * q2.yp + q1.zp * q2.zp;
    }

    float length() const noexcept;
    constexpr float lengthSquared() const noexcept { return wp * wp + xp * xp + yp * yp + zp * zp; }
    QQuaternion normalized() const noexcept;
    void normalize() noexcept;
    QQuaternion inverted() const noexcept;
    constexpr QQuaternion conjugated() const noexcept { return QQuaternion(wp, -xp, -yp, -zp); }
    QVector3D rotatedVector(const QVector3D &vector) const noexcept;

    constexpr QQuaternion &operator+=(const QQuaternion &q) noexcept {
        wp += q.wp;
        xp += q.xp;
        yp += q.yp;
        zp += q.zp;
        return *this;
    }
    constexpr QQuaternion &operator-=(const QQuaternion &q) noexcept {
        wp -= q.wp;
        xp -= q.xp;
        yp -= q.yp;
        zp -= q.zp;
        return *this;
    }
    constexpr QQuaternion &operator*=(float factor) noexcept {
        wp *= factor;
        xp *= factor;
        yp *= factor;
        zp *= factor;
        return *this;
    }
    constexpr QQuaternion &operator*=(const QQuaternion &q) noexcept { return *this = *this * q; }
    constexpr QQuaternion &operator/=(float divisor) {
        wp /= divisor;
        xp /= divisor;
        yp /= divisor;
        zp /= divisor;
        return *this;
    }

    friend constexpr bool operator==(const QQuaternion &q1, const QQuaternion &q2) noexcept {
        return q1.wp == q2.wp && q1.xp == q2.xp && q1.yp == q2.yp && q1.zp == q2.zp;
    }
    friend constexpr bool operator!=(const QQuaternion &q1, const QQuaternion &q2) noexcept { return !(q1 == q2); }
    friend constexpr QQuaternion operator+(QQuaternion q1, const QQuaternion &q2) noexcept { return q1 += q2; }
    friend constexpr QQuaternion operator-(QQuaternion q1, const QQuaternion &q2) noexcept { return q1 -= q2; }
    friend constexpr QQuaternion operator*(float factor, QQuaternion q) noexcept { return q *= factor; }
    friend constexpr QQuaternion operator*(QQuaternion q, float factor) noexcept { return q *= factor; }
    friend constexpr QQuaternion operator*(const QQuaternion &q1, const QQuaternion &q2) noexcept {
        return QQuaternion(q1.wp * q2.wp - q1.xp * q2.xp - q1.yp * q2.yp - q1.zp * q2.zp,
                           q1.wp * q2.xp + q1.xp * q2.wp + q1.yp * q2.zp - q1.zp * q2.yp,
                           q1.wp * q2.yp - q1.xp * q2.zp + q1.yp * q2.wp + q1.zp * q2.xp,
                           q1.wp * q2.zp + q1.xp * q2.yp - q1.yp * q2.xp + q1.zp * q2.wp);
    }
    friend constexpr QQuaternion operator-(const QQuaternion &q) noexcept {
        return QQuaternion(-q.wp, -q.xp, -q.yp, -q.zp);
    }
    friend constexpr QQuaternion operator/(QQuaternion q, float divisor) { return q /= divisor; }
    friend QVector3D operator*(const QQuaternion &q, const QVector3D &vector) noexcept {
        return q.rotatedVector(vector);
    }

    friend bool qFuzzyCompare(const QQuaternion &q1, const QQuaternion &q2) noexcept;

    constexpr QVector4D toVector4D() const noexcept { return QVector4D(xp, yp, zp, wp); }

    void getAxisAndAngle(QVector3D *axis, float *angle) const;
    void getAxisAndAngle(float *x, float *y, float *z, float *angle) const;
    static QQuaternion fromAxisAndAngle(const QVector3D &axis, float angle);
    static QQuaternion fromAxisAndAngle(float x, float y, float z, float angle);

    QVector3D toEulerAngles() const;
    void getEulerAngles(float *pitch, float *yaw, float *roll) const;
    static QQuaternion fromEulerAngles(const QVector3D &angles);
    static QQuaternion fromEulerAngles(float pitch, float yaw, float roll);

    void getAxes(QVector3D *xAxis, QVector3D *yAxis, QVector3D *zAxis) const;
    static QQuaternion fromAxes(const QVector3D &xAxis, const QVector3D &yAxis, const QVector3D &zAxis);
    static QQuaternion fromDirection(const QVector3D &direction, const QVector3D &up);
    static QQuaternion rotationTo(const QVector3D &from, const QVector3D &to);

    static QQuaternion slerp(const QQuaternion &q1, const QQuaternion &q2, float t);
    static QQuaternion nlerp(const QQuaternion &q1, const QQuaternion &q2, float t);

private:
    float wp, xp, yp, zp;
};
