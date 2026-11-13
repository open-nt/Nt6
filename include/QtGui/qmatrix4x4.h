// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtGui/qquaternion.h>
#include <QtGui/qvectornd.h>

class QTransform;

class QMatrix4x4 {
public:
    constexpr QMatrix4x4() noexcept
        : m{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}} {}
    explicit QMatrix4x4(const float *values);
    constexpr QMatrix4x4(float m11, float m12, float m13, float m14, float m21, float m22, float m23, float m24,
                         float m31, float m32, float m33, float m34, float m41, float m42, float m43, float m44)
        : m{{m11, m21, m31, m41}, {m12, m22, m32, m42}, {m13, m23, m33, m43}, {m14, m24, m34, m44}} {}
    explicit QMatrix4x4(const QTransform &transform);

    constexpr const float &operator()(int row, int column) const { return m[column][row]; }
    constexpr float &operator()(int row, int column) { return m[column][row]; }

    QVector4D column(int index) const { return QVector4D(m[index][0], m[index][1], m[index][2], m[index][3]); }
    void setColumn(int index, const QVector4D &value);
    QVector4D row(int index) const { return QVector4D(m[0][index], m[1][index], m[2][index], m[3][index]); }
    void setRow(int index, const QVector4D &value);

    bool isAffine() const { return m[0][3] == 0.0f && m[1][3] == 0.0f && m[2][3] == 0.0f && m[3][3] == 1.0f; }
    bool isIdentity() const;
    void setToIdentity() { *this = QMatrix4x4(); }
    void fill(float value);

    double determinant() const;
    QMatrix4x4 inverted(bool *invertible = nullptr) const;
    QMatrix4x4 transposed() const;

    QMatrix4x4 &operator+=(const QMatrix4x4 &other);
    QMatrix4x4 &operator-=(const QMatrix4x4 &other);
    QMatrix4x4 &operator*=(const QMatrix4x4 &other);
    QMatrix4x4 &operator*=(float factor);
    QMatrix4x4 &operator/=(float divisor);
    bool operator==(const QMatrix4x4 &other) const;
    bool operator!=(const QMatrix4x4 &other) const { return !(*this == other); }

    friend QMatrix4x4 operator+(QMatrix4x4 m1, const QMatrix4x4 &m2) { return m1 += m2; }
    friend QMatrix4x4 operator-(QMatrix4x4 m1, const QMatrix4x4 &m2) { return m1 -= m2; }
    friend QMatrix4x4 operator*(const QMatrix4x4 &m1, const QMatrix4x4 &m2);
    friend QMatrix4x4 operator-(QMatrix4x4 matrix) { return matrix *= -1.0f; }
    friend QMatrix4x4 operator*(float factor, QMatrix4x4 matrix) { return matrix *= factor; }
    friend QMatrix4x4 operator*(QMatrix4x4 matrix, float factor) { return matrix *= factor; }
    friend QMatrix4x4 operator/(QMatrix4x4 matrix, float divisor) { return matrix /= divisor; }
    friend QVector3D operator*(const QMatrix4x4 &matrix, const QVector3D &vector) { return matrix.map(vector); }
    friend QVector4D operator*(const QMatrix4x4 &matrix, const QVector4D &vector) { return matrix.map(vector); }
    friend QVector4D operator*(const QVector4D &vector, const QMatrix4x4 &matrix);
    friend QPoint operator*(const QMatrix4x4 &matrix, const QPoint &point) { return matrix.map(point); }
    friend QPointF operator*(const QMatrix4x4 &matrix, const QPointF &point) { return matrix.map(point); }

    friend bool qFuzzyCompare(const QMatrix4x4 &m1, const QMatrix4x4 &m2) noexcept;

    void scale(const QVector3D &vector);
    void scale(float x, float y);
    void scale(float x, float y, float z);
    void scale(float factor);
    void translate(const QVector3D &vector);
    void translate(float x, float y);
    void translate(float x, float y, float z);
    void rotate(float angle, const QVector3D &vector);
    void rotate(float angle, float x, float y, float z = 0.0f);
    void rotate(const QQuaternion &quaternion);

    void ortho(const QRect &rect);
    void ortho(const QRectF &rect);
    void ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    void frustum(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    void perspective(float verticalAngle, float aspectRatio, float nearPlane, float farPlane);
    void lookAt(const QVector3D &eye, const QVector3D &center, const QVector3D &up);
    void viewport(const QRectF &rect);
    void viewport(float left, float bottom, float width, float height, float nearPlane = 0.0f, float farPlane = 1.0f);
    void flipCoordinates();

    void copyDataTo(float *values) const;

    QTransform toTransform() const;
    QTransform toTransform(float distanceToPlane) const;

    QPoint map(const QPoint &point) const;
    QPointF map(const QPointF &point) const;
    QVector3D map(const QVector3D &point) const;
    QVector3D mapVector(const QVector3D &vector) const;
    QVector4D map(const QVector4D &point) const;
    QRect mapRect(const QRect &rect) const;
    QRectF mapRect(const QRectF &rect) const;

    float *data() { return &m[0][0]; }
    const float *data() const { return &m[0][0]; }
    const float *constData() const { return &m[0][0]; }

    void optimize() {}

private:
    float m[4][4]; // m[column][row], the layout OpenGL expects
};
