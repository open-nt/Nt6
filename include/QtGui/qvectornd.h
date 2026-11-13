// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qpoint.h>

class QMatrix4x4;
class QRect;
class QVector3D;
class QVector4D;

class QVector2D {
public:
    constexpr QVector2D() noexcept : v{0.0f, 0.0f} {}
    constexpr QVector2D(float xpos, float ypos) noexcept : v{xpos, ypos} {}
    constexpr explicit QVector2D(QPoint point) noexcept : v{float(point.x()), float(point.y())} {}
    constexpr explicit QVector2D(QPointF point) noexcept : v{float(point.x()), float(point.y())} {}
    constexpr explicit QVector2D(QVector3D vector) noexcept;
    constexpr explicit QVector2D(QVector4D vector) noexcept;

    constexpr bool isNull() const noexcept { return v[0] == 0.0f && v[1] == 0.0f; }

    constexpr float x() const noexcept { return v[0]; }
    constexpr float y() const noexcept { return v[1]; }
    constexpr void setX(float x) noexcept { v[0] = x; }
    constexpr void setY(float y) noexcept { v[1] = y; }

    constexpr float &operator[](int i) { return v[i]; }
    constexpr float operator[](int i) const { return v[i]; }

    float length() const noexcept;
    constexpr float lengthSquared() const noexcept { return v[0] * v[0] + v[1] * v[1]; }
    QVector2D normalized() const noexcept;
    void normalize() noexcept;
    float distanceToPoint(QVector2D point) const noexcept;
    float distanceToLine(QVector2D point, QVector2D direction) const noexcept;

    constexpr QVector2D &operator+=(QVector2D vector) noexcept {
        v[0] += vector.v[0];
        v[1] += vector.v[1];
        return *this;
    }
    constexpr QVector2D &operator-=(QVector2D vector) noexcept {
        v[0] -= vector.v[0];
        v[1] -= vector.v[1];
        return *this;
    }
    constexpr QVector2D &operator*=(float factor) noexcept {
        v[0] *= factor;
        v[1] *= factor;
        return *this;
    }
    constexpr QVector2D &operator*=(QVector2D vector) noexcept {
        v[0] *= vector.v[0];
        v[1] *= vector.v[1];
        return *this;
    }
    constexpr QVector2D &operator/=(float divisor) {
        v[0] /= divisor;
        v[1] /= divisor;
        return *this;
    }
    constexpr QVector2D &operator/=(QVector2D vector) {
        v[0] /= vector.v[0];
        v[1] /= vector.v[1];
        return *this;
    }

    static constexpr float dotProduct(QVector2D v1, QVector2D v2) noexcept {
        return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1];
    }

    friend constexpr bool operator==(QVector2D v1, QVector2D v2) noexcept {
        return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1];
    }
    friend constexpr bool operator!=(QVector2D v1, QVector2D v2) noexcept { return !(v1 == v2); }
    friend constexpr QVector2D operator+(QVector2D v1, QVector2D v2) noexcept { return v1 += v2; }
    friend constexpr QVector2D operator-(QVector2D v1, QVector2D v2) noexcept { return v1 -= v2; }
    friend constexpr QVector2D operator*(float factor, QVector2D vector) noexcept { return vector *= factor; }
    friend constexpr QVector2D operator*(QVector2D vector, float factor) noexcept { return vector *= factor; }
    friend constexpr QVector2D operator*(QVector2D v1, QVector2D v2) noexcept { return v1 *= v2; }
    friend constexpr QVector2D operator-(QVector2D vector) noexcept { return QVector2D(-vector.v[0], -vector.v[1]); }
    friend constexpr QVector2D operator/(QVector2D vector, float divisor) { return vector /= divisor; }
    friend constexpr QVector2D operator/(QVector2D vector, QVector2D divisor) { return vector /= divisor; }

    friend bool qFuzzyCompare(QVector2D v1, QVector2D v2) noexcept;

    constexpr QVector3D toVector3D() const noexcept;
    constexpr QVector4D toVector4D() const noexcept;
    QPoint toPoint() const noexcept { return QPoint(qRound(double(v[0])), qRound(double(v[1]))); }
    constexpr QPointF toPointF() const noexcept { return QPointF(qreal(v[0]), qreal(v[1])); }

private:
    float v[2];
    friend class QVector3D;
    friend class QVector4D;
};

class QVector3D {
public:
    constexpr QVector3D() noexcept : v{0.0f, 0.0f, 0.0f} {}
    constexpr QVector3D(float xpos, float ypos, float zpos) noexcept : v{xpos, ypos, zpos} {}
    constexpr explicit QVector3D(QPoint point) noexcept : v{float(point.x()), float(point.y()), 0.0f} {}
    constexpr explicit QVector3D(QPointF point) noexcept : v{float(point.x()), float(point.y()), 0.0f} {}
    constexpr QVector3D(QVector2D vector, float zpos) noexcept : v{vector.v[0], vector.v[1], zpos} {}
    constexpr explicit QVector3D(QVector2D vector) noexcept : v{vector.v[0], vector.v[1], 0.0f} {}
    constexpr explicit QVector3D(QVector4D vector) noexcept;

    constexpr bool isNull() const noexcept { return v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f; }

    constexpr float x() const noexcept { return v[0]; }
    constexpr float y() const noexcept { return v[1]; }
    constexpr float z() const noexcept { return v[2]; }
    constexpr void setX(float x) noexcept { v[0] = x; }
    constexpr void setY(float y) noexcept { v[1] = y; }
    constexpr void setZ(float z) noexcept { v[2] = z; }

    constexpr float &operator[](int i) { return v[i]; }
    constexpr float operator[](int i) const { return v[i]; }

    float length() const noexcept;
    constexpr float lengthSquared() const noexcept { return v[0] * v[0] + v[1] * v[1] + v[2] * v[2]; }
    QVector3D normalized() const noexcept;
    void normalize() noexcept;

    constexpr QVector3D &operator+=(QVector3D vector) noexcept {
        v[0] += vector.v[0];
        v[1] += vector.v[1];
        v[2] += vector.v[2];
        return *this;
    }
    constexpr QVector3D &operator-=(QVector3D vector) noexcept {
        v[0] -= vector.v[0];
        v[1] -= vector.v[1];
        v[2] -= vector.v[2];
        return *this;
    }
    constexpr QVector3D &operator*=(float factor) noexcept {
        v[0] *= factor;
        v[1] *= factor;
        v[2] *= factor;
        return *this;
    }
    constexpr QVector3D &operator*=(QVector3D vector) noexcept {
        v[0] *= vector.v[0];
        v[1] *= vector.v[1];
        v[2] *= vector.v[2];
        return *this;
    }
    constexpr QVector3D &operator/=(float divisor) {
        v[0] /= divisor;
        v[1] /= divisor;
        v[2] /= divisor;
        return *this;
    }
    constexpr QVector3D &operator/=(QVector3D vector) {
        v[0] /= vector.v[0];
        v[1] /= vector.v[1];
        v[2] /= vector.v[2];
        return *this;
    }

    static constexpr float dotProduct(QVector3D v1, QVector3D v2) noexcept {
        return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1] + v1.v[2] * v2.v[2];
    }
    static constexpr QVector3D crossProduct(QVector3D v1, QVector3D v2) noexcept {
        return QVector3D(v1.v[1] * v2.v[2] - v1.v[2] * v2.v[1], v1.v[2] * v2.v[0] - v1.v[0] * v2.v[2],
                         v1.v[0] * v2.v[1] - v1.v[1] * v2.v[0]);
    }
    static QVector3D normal(QVector3D v1, QVector3D v2) noexcept;
    static QVector3D normal(QVector3D v1, QVector3D v2, QVector3D v3) noexcept;

    QVector3D project(const QMatrix4x4 &modelView, const QMatrix4x4 &projection, const QRect &viewport) const;
    QVector3D unproject(const QMatrix4x4 &modelView, const QMatrix4x4 &projection, const QRect &viewport) const;

    float distanceToPoint(QVector3D point) const noexcept;
    float distanceToPlane(QVector3D plane, QVector3D normal) const noexcept;
    float distanceToPlane(QVector3D plane1, QVector3D plane2, QVector3D plane3) const noexcept;
    float distanceToLine(QVector3D point, QVector3D direction) const noexcept;

    friend constexpr bool operator==(QVector3D v1, QVector3D v2) noexcept {
        return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1] && v1.v[2] == v2.v[2];
    }
    friend constexpr bool operator!=(QVector3D v1, QVector3D v2) noexcept { return !(v1 == v2); }
    friend constexpr QVector3D operator+(QVector3D v1, QVector3D v2) noexcept { return v1 += v2; }
    friend constexpr QVector3D operator-(QVector3D v1, QVector3D v2) noexcept { return v1 -= v2; }
    friend constexpr QVector3D operator*(float factor, QVector3D vector) noexcept { return vector *= factor; }
    friend constexpr QVector3D operator*(QVector3D vector, float factor) noexcept { return vector *= factor; }
    friend constexpr QVector3D operator*(QVector3D v1, QVector3D v2) noexcept { return v1 *= v2; }
    friend constexpr QVector3D operator-(QVector3D vector) noexcept {
        return QVector3D(-vector.v[0], -vector.v[1], -vector.v[2]);
    }
    friend constexpr QVector3D operator/(QVector3D vector, float divisor) { return vector /= divisor; }
    friend constexpr QVector3D operator/(QVector3D vector, QVector3D divisor) { return vector /= divisor; }

    friend bool qFuzzyCompare(QVector3D v1, QVector3D v2) noexcept;

    constexpr QVector2D toVector2D() const noexcept { return QVector2D(v[0], v[1]); }
    constexpr QVector4D toVector4D() const noexcept;
    QPoint toPoint() const noexcept { return QPoint(qRound(double(v[0])), qRound(double(v[1]))); }
    constexpr QPointF toPointF() const noexcept { return QPointF(qreal(v[0]), qreal(v[1])); }

private:
    float v[3];
    friend class QVector2D;
    friend class QVector4D;
};

class QVector4D {
public:
    constexpr QVector4D() noexcept : v{0.0f, 0.0f, 0.0f, 0.0f} {}
    constexpr QVector4D(float xpos, float ypos, float zpos, float wpos) noexcept : v{xpos, ypos, zpos, wpos} {}
    constexpr explicit QVector4D(QPoint point) noexcept : v{float(point.x()), float(point.y()), 0.0f, 0.0f} {}
    constexpr explicit QVector4D(QPointF point) noexcept : v{float(point.x()), float(point.y()), 0.0f, 0.0f} {}
    constexpr explicit QVector4D(QVector2D vector) noexcept : v{vector.v[0], vector.v[1], 0.0f, 0.0f} {}
    constexpr QVector4D(QVector2D vector, float zpos, float wpos) noexcept : v{vector.v[0], vector.v[1], zpos, wpos} {}
    constexpr explicit QVector4D(QVector3D vector) noexcept : v{vector.v[0], vector.v[1], vector.v[2], 0.0f} {}
    constexpr QVector4D(QVector3D vector, float wpos) noexcept : v{vector.v[0], vector.v[1], vector.v[2], wpos} {}

    constexpr bool isNull() const noexcept { return v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f && v[3] == 0.0f; }

    constexpr float x() const noexcept { return v[0]; }
    constexpr float y() const noexcept { return v[1]; }
    constexpr float z() const noexcept { return v[2]; }
    constexpr float w() const noexcept { return v[3]; }
    constexpr void setX(float x) noexcept { v[0] = x; }
    constexpr void setY(float y) noexcept { v[1] = y; }
    constexpr void setZ(float z) noexcept { v[2] = z; }
    constexpr void setW(float w) noexcept { v[3] = w; }

    constexpr float &operator[](int i) { return v[i]; }
    constexpr float operator[](int i) const { return v[i]; }

    float length() const noexcept;
    constexpr float lengthSquared() const noexcept { return v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]; }
    QVector4D normalized() const noexcept;
    void normalize() noexcept;

    constexpr QVector4D &operator+=(QVector4D vector) noexcept {
        v[0] += vector.v[0];
        v[1] += vector.v[1];
        v[2] += vector.v[2];
        v[3] += vector.v[3];
        return *this;
    }
    constexpr QVector4D &operator-=(QVector4D vector) noexcept {
        v[0] -= vector.v[0];
        v[1] -= vector.v[1];
        v[2] -= vector.v[2];
        v[3] -= vector.v[3];
        return *this;
    }
    constexpr QVector4D &operator*=(float factor) noexcept {
        v[0] *= factor;
        v[1] *= factor;
        v[2] *= factor;
        v[3] *= factor;
        return *this;
    }
    constexpr QVector4D &operator*=(QVector4D vector) noexcept {
        v[0] *= vector.v[0];
        v[1] *= vector.v[1];
        v[2] *= vector.v[2];
        v[3] *= vector.v[3];
        return *this;
    }
    constexpr QVector4D &operator/=(float divisor) {
        v[0] /= divisor;
        v[1] /= divisor;
        v[2] /= divisor;
        v[3] /= divisor;
        return *this;
    }
    constexpr QVector4D &operator/=(QVector4D vector) {
        v[0] /= vector.v[0];
        v[1] /= vector.v[1];
        v[2] /= vector.v[2];
        v[3] /= vector.v[3];
        return *this;
    }

    static constexpr float dotProduct(QVector4D v1, QVector4D v2) noexcept {
        return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1] + v1.v[2] * v2.v[2] + v1.v[3] * v2.v[3];
    }

    friend constexpr bool operator==(QVector4D v1, QVector4D v2) noexcept {
        return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1] && v1.v[2] == v2.v[2] && v1.v[3] == v2.v[3];
    }
    friend constexpr bool operator!=(QVector4D v1, QVector4D v2) noexcept { return !(v1 == v2); }
    friend constexpr QVector4D operator+(QVector4D v1, QVector4D v2) noexcept { return v1 += v2; }
    friend constexpr QVector4D operator-(QVector4D v1, QVector4D v2) noexcept { return v1 -= v2; }
    friend constexpr QVector4D operator*(float factor, QVector4D vector) noexcept { return vector *= factor; }
    friend constexpr QVector4D operator*(QVector4D vector, float factor) noexcept { return vector *= factor; }
    friend constexpr QVector4D operator*(QVector4D v1, QVector4D v2) noexcept { return v1 *= v2; }
    friend constexpr QVector4D operator-(QVector4D vector) noexcept {
        return QVector4D(-vector.v[0], -vector.v[1], -vector.v[2], -vector.v[3]);
    }
    friend constexpr QVector4D operator/(QVector4D vector, float divisor) { return vector /= divisor; }
    friend constexpr QVector4D operator/(QVector4D vector, QVector4D divisor) { return vector /= divisor; }

    friend bool qFuzzyCompare(QVector4D v1, QVector4D v2) noexcept;

    constexpr QVector2D toVector2D() const noexcept { return QVector2D(v[0], v[1]); }
    constexpr QVector2D toVector2DAffine() const noexcept {
        return v[3] == 0.0f ? QVector2D() : QVector2D(v[0] / v[3], v[1] / v[3]);
    }
    constexpr QVector3D toVector3D() const noexcept { return QVector3D(v[0], v[1], v[2]); }
    constexpr QVector3D toVector3DAffine() const noexcept {
        return v[3] == 0.0f ? QVector3D() : QVector3D(v[0] / v[3], v[1] / v[3], v[2] / v[3]);
    }
    QPoint toPoint() const noexcept { return QPoint(qRound(double(v[0])), qRound(double(v[1]))); }
    constexpr QPointF toPointF() const noexcept { return QPointF(qreal(v[0]), qreal(v[1])); }

private:
    float v[4];
    friend class QVector2D;
    friend class QVector3D;
};

constexpr QVector2D::QVector2D(QVector3D vector) noexcept : v{vector.v[0], vector.v[1]} {}
constexpr QVector2D::QVector2D(QVector4D vector) noexcept : v{vector.v[0], vector.v[1]} {}
constexpr QVector3D QVector2D::toVector3D() const noexcept { return QVector3D(v[0], v[1], 0.0f); }
constexpr QVector4D QVector2D::toVector4D() const noexcept { return QVector4D(v[0], v[1], 0.0f, 0.0f); }
constexpr QVector3D::QVector3D(QVector4D vector) noexcept : v{vector.v[0], vector.v[1], vector.v[2]} {}
constexpr QVector4D QVector3D::toVector4D() const noexcept { return QVector4D(v[0], v[1], v[2], 0.0f); }
