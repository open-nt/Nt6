// MIT License - see LICENSE
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qtransform.h>

#include "qmath3d_p.h"

#include <algorithm>
#include <cmath>

using QtPrivate::fuzzyCompare;

/*!
    \class QMatrix4x4
    \brief The QMatrix4x4 class represents a 4x4 transformation matrix in 3D space.

    A QMatrix4x4 starts as the identity. translate(), scale(), rotate() and the projection functions
    multiply it on the right, so the transformation added last applies to a vertex first, as in OpenGL:

    \code
    QMatrix4x4 projection;
    projection.perspective(45.0f, aspect, 0.1f, 100.0f);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0, 2, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    QMatrix4x4 model;
    model.translate(0, 0.5f, 0);
    model.rotate(30.0f, 0, 1, 0);
    QVector3D p = projection * view * model * QVector3D(1, 0, 0);
    \endcode

    \c{operator()(row, column)} reads and writes an element. The 16-value constructors take values in
    row-major order, but data() is column-major, the layout \c glUniformMatrix4fv() expects.
    Multiplying a QVector3D or QPointF divides by w; mapVector() applies only the rotation and scale.

    \differences
    There are no QGenericMatrix conversions, \c normalMatrix() or \c toGenericMatrix(), since there is no
    QMatrix3x3. There is no QDataStream or QDebug streaming and no QVariant conversion. The matrix keeps
    no flags for special cases, so optimize() does nothing and every product is a full 4x4 one.
    \enddifferences

    \sa QVector3D, QQuaternion, QTransform
*/

/*!
    \fn QMatrix4x4::QMatrix4x4()

    Constructs the identity matrix.
*/

/*!
    \fn QMatrix4x4::QMatrix4x4(float m11, float m12, float m13, float m14, float m21, float m22, float m23, float m24, float m31, float m32, float m33, float m34, float m41, float m42, float m43, float m44)

    Constructs a matrix from 16 values in row-major order: \a m11, \a m12, \a m13 and \a m14 are the
    first row, \a m21, \a m22, \a m23 and \a m24 the second, \a m31, \a m32, \a m33 and \a m34 the third
    and \a m41, \a m42, \a m43 and \a m44 the fourth.
*/

/*!
    \fn const float &QMatrix4x4::operator()(int row, int column) const

    Returns a reference to the element at \a row and \a column, both from 0 to 3.
*/

/*!
    \fn float &QMatrix4x4::operator()(int row, int column)

    Returns a modifiable reference to the element at \a row and \a column, both from 0 to 3.
*/

/*!
    \fn QVector4D QMatrix4x4::column(int index) const

    Returns column \a index, from 0 to 3.

    \sa setColumn(), row()
*/

/*!
    \fn QVector4D QMatrix4x4::row(int index) const

    Returns row \a index, from 0 to 3.

    \sa setRow(), column()
*/

/*!
    \fn bool QMatrix4x4::isAffine() const

    Returns \c true if the last row is (0, 0, 0, 1), so the matrix has no projection; otherwise returns \c false.
*/

/*!
    \fn void QMatrix4x4::setToIdentity()

    Sets the matrix to the identity.

    \sa isIdentity()
*/

/*!
    \fn bool QMatrix4x4::operator!=(const QMatrix4x4 &other) const

    Returns \c true if any element differs from the one in \a other; otherwise returns \c false.
*/

/*!
    \fn float *QMatrix4x4::data()

    Returns a pointer to the 16 elements, in column-major order.

    \sa constData(), copyDataTo()
*/

/*!
    \fn const float *QMatrix4x4::data() const

    \overload
*/

/*!
    \fn const float *QMatrix4x4::constData() const

    Returns a pointer to the 16 elements, in column-major order, the layout \c glUniformMatrix4fv() expects.

    \sa data()
*/

/*!
    \fn void QMatrix4x4::optimize()

    Does nothing: Nt keeps no flags for special matrices. It exists for source compatibility.
*/

/*!
    Constructs a matrix from the 16 floats at \a values, in row-major order.

    \sa copyDataTo()
*/
QMatrix4x4::QMatrix4x4(const float *values) : QMatrix4x4() {
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            m[col][row] = values[row * 4 + col];
}

/*!
    Constructs a matrix that does in the plane z = 0 what \a transform does, leaving z unchanged.

    \sa toTransform()
*/
QMatrix4x4::QMatrix4x4(const QTransform &transform)
    : QMatrix4x4(float(transform.m11()), float(transform.m21()), 0.0f, float(transform.m31()), float(transform.m12()),
                 float(transform.m22()), 0.0f, float(transform.m32()), 0.0f, 0.0f, 1.0f, 0.0f, float(transform.m13()),
                 float(transform.m23()), 0.0f, float(transform.m33())) {}

/*!
    Sets column \a index, from 0 to 3, to \a value.

    \sa column()
*/
void QMatrix4x4::setColumn(int index, const QVector4D &value) {
    for (int i = 0; i < 4; ++i)
        m[index][i] = value[i];
}

/*!
    Sets row \a index, from 0 to 3, to \a value.

    \sa row()
*/
void QMatrix4x4::setRow(int index, const QVector4D &value) {
    for (int i = 0; i < 4; ++i)
        m[i][index] = value[i];
}

/*!
    Returns \c true if this is the identity matrix; otherwise returns \c false.
*/
bool QMatrix4x4::isIdentity() const { return *this == QMatrix4x4(); }

/*!
    Sets every element to \a value.
*/
void QMatrix4x4::fill(float value) {
    for (auto &col : m)
        for (float &e : col)
            e = value;
}

/*!
    Returns the determinant, computed in \c double.

    \sa inverted()
*/
double QMatrix4x4::determinant() const {
    double a[4][4];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            a[r][c] = double(m[c][r]);
    const double s0 = a[0][0] * a[1][1] - a[1][0] * a[0][1];
    const double s1 = a[0][0] * a[1][2] - a[1][0] * a[0][2];
    const double s2 = a[0][0] * a[1][3] - a[1][0] * a[0][3];
    const double s3 = a[0][1] * a[1][2] - a[1][1] * a[0][2];
    const double s4 = a[0][1] * a[1][3] - a[1][1] * a[0][3];
    const double s5 = a[0][2] * a[1][3] - a[1][2] * a[0][3];
    const double c5 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
    const double c4 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
    const double c3 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
    const double c2 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
    const double c1 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
    const double c0 = a[2][0] * a[3][1] - a[3][0] * a[2][1];
    return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
}

/*!
    Returns the inverse of the matrix. If the matrix cannot be inverted, returns the identity and sets
    \c{*invertible} to \c false; otherwise sets it to \c true. \a invertible may be \c nullptr.

    \sa determinant()
*/
QMatrix4x4 QMatrix4x4::inverted(bool *invertible) const {
    double a[4][4];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            a[r][c] = double(m[c][r]);
    const double s0 = a[0][0] * a[1][1] - a[1][0] * a[0][1];
    const double s1 = a[0][0] * a[1][2] - a[1][0] * a[0][2];
    const double s2 = a[0][0] * a[1][3] - a[1][0] * a[0][3];
    const double s3 = a[0][1] * a[1][2] - a[1][1] * a[0][2];
    const double s4 = a[0][1] * a[1][3] - a[1][1] * a[0][3];
    const double s5 = a[0][2] * a[1][3] - a[1][2] * a[0][3];
    const double c5 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
    const double c4 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
    const double c3 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
    const double c2 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
    const double c1 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
    const double c0 = a[2][0] * a[3][1] - a[3][0] * a[2][1];
    const double det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    if (det == 0.0 || !std::isfinite(det)) {
        if (invertible)
            *invertible = false;
        return QMatrix4x4();
    }
    const double d = 1.0 / det;
    double b[4][4];
    b[0][0] = (a[1][1] * c5 - a[1][2] * c4 + a[1][3] * c3) * d;
    b[0][1] = (-a[0][1] * c5 + a[0][2] * c4 - a[0][3] * c3) * d;
    b[0][2] = (a[3][1] * s5 - a[3][2] * s4 + a[3][3] * s3) * d;
    b[0][3] = (-a[2][1] * s5 + a[2][2] * s4 - a[2][3] * s3) * d;
    b[1][0] = (-a[1][0] * c5 + a[1][2] * c2 - a[1][3] * c1) * d;
    b[1][1] = (a[0][0] * c5 - a[0][2] * c2 + a[0][3] * c1) * d;
    b[1][2] = (-a[3][0] * s5 + a[3][2] * s2 - a[3][3] * s1) * d;
    b[1][3] = (a[2][0] * s5 - a[2][2] * s2 + a[2][3] * s1) * d;
    b[2][0] = (a[1][0] * c4 - a[1][1] * c2 + a[1][3] * c0) * d;
    b[2][1] = (-a[0][0] * c4 + a[0][1] * c2 - a[0][3] * c0) * d;
    b[2][2] = (a[3][0] * s4 - a[3][1] * s2 + a[3][3] * s0) * d;
    b[2][3] = (-a[2][0] * s4 + a[2][1] * s2 - a[2][3] * s0) * d;
    b[3][0] = (-a[1][0] * c3 + a[1][1] * c1 - a[1][2] * c0) * d;
    b[3][1] = (a[0][0] * c3 - a[0][1] * c1 + a[0][2] * c0) * d;
    b[3][2] = (-a[3][0] * s3 + a[3][1] * s1 - a[3][2] * s0) * d;
    b[3][3] = (a[2][0] * s3 - a[2][1] * s1 + a[2][2] * s0) * d;
    QMatrix4x4 inv;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            inv.m[c][r] = float(b[r][c]);
    if (invertible)
        *invertible = true;
    return inv;
}

/*!
    Returns the transpose of the matrix, with rows and columns swapped.
*/
QMatrix4x4 QMatrix4x4::transposed() const {
    QMatrix4x4 t;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            t.m[c][r] = m[r][c];
    return t;
}

/*!
    Adds \a other to this matrix, element by element, and returns a reference to it.
*/
QMatrix4x4 &QMatrix4x4::operator+=(const QMatrix4x4 &other) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            m[c][r] += other.m[c][r];
    return *this;
}

/*!
    Subtracts \a other from this matrix, element by element, and returns a reference to it.
*/
QMatrix4x4 &QMatrix4x4::operator-=(const QMatrix4x4 &other) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            m[c][r] -= other.m[c][r];
    return *this;
}

/*!
    Sets this matrix to its product with \a other, on the right, and returns a reference to it.
*/
QMatrix4x4 &QMatrix4x4::operator*=(const QMatrix4x4 &other) { return *this = *this * other; }

/*!
    \overload

    Multiplies every element by \a factor and returns a reference to this matrix.
*/
QMatrix4x4 &QMatrix4x4::operator*=(float factor) {
    for (auto &col : m)
        for (float &e : col)
            e *= factor;
    return *this;
}

/*!
    Divides every element by \a divisor and returns a reference to this matrix.
*/
QMatrix4x4 &QMatrix4x4::operator/=(float divisor) {
    for (auto &col : m)
        for (float &e : col)
            e /= divisor;
    return *this;
}

/*!
    Returns \c true if every element equals the one in \a other exactly; otherwise returns \c false.
    Use \c qFuzzyCompare() to allow for rounding.
*/
bool QMatrix4x4::operator==(const QMatrix4x4 &other) const {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (m[c][r] != other.m[c][r])
                return false;
    return true;
}

QMatrix4x4 operator*(const QMatrix4x4 &m1, const QMatrix4x4 &m2) {
    QMatrix4x4 p;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            p.m[c][r] =
                m1.m[0][r] * m2.m[c][0] + m1.m[1][r] * m2.m[c][1] + m1.m[2][r] * m2.m[c][2] + m1.m[3][r] * m2.m[c][3];
    return p;
}

QVector4D operator*(const QVector4D &vector, const QMatrix4x4 &matrix) {
    QVector4D out;
    for (int c = 0; c < 4; ++c)
        out[c] = vector.x() * matrix.m[c][0] + vector.y() * matrix.m[c][1] + vector.z() * matrix.m[c][2] +
                 vector.w() * matrix.m[c][3];
    return out;
}

bool qFuzzyCompare(const QMatrix4x4 &m1, const QMatrix4x4 &m2) noexcept {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!fuzzyCompare(m1.m[c][r], m2.m[c][r]))
                return false;
    return true;
}

/*!
    Multiplies this matrix by a scale of \a vector along x, y and z.
*/
void QMatrix4x4::scale(const QVector3D &vector) { scale(vector.x(), vector.y(), vector.z()); }

/*!
    \overload

    Multiplies this matrix by a scale of \a x and \a y, leaving z unchanged.
*/
void QMatrix4x4::scale(float x, float y) { scale(x, y, 1.0f); }

/*!
    \overload

    Multiplies this matrix by a scale of \a x, \a y and \a z.
*/
void QMatrix4x4::scale(float x, float y, float z) {
    for (int r = 0; r < 4; ++r) {
        m[0][r] *= x;
        m[1][r] *= y;
        m[2][r] *= z;
    }
}

/*!
    \overload

    Multiplies this matrix by a uniform scale of \a factor.
*/
void QMatrix4x4::scale(float factor) { scale(factor, factor, factor); }

/*!
    Multiplies this matrix by a translation of \a vector.
*/
void QMatrix4x4::translate(const QVector3D &vector) { translate(vector.x(), vector.y(), vector.z()); }

/*!
    \overload

    Multiplies this matrix by a translation of \a x and \a y.
*/
void QMatrix4x4::translate(float x, float y) { translate(x, y, 0.0f); }

/*!
    \overload

    Multiplies this matrix by a translation of \a x, \a y and \a z.
*/
void QMatrix4x4::translate(float x, float y, float z) {
    for (int r = 0; r < 4; ++r)
        m[3][r] += m[0][r] * x + m[1][r] * y + m[2][r] * z;
}

/*!
    Multiplies this matrix by a rotation of \a angle degrees counterclockwise about \a vector.
*/
void QMatrix4x4::rotate(float angle, const QVector3D &vector) { rotate(angle, vector.x(), vector.y(), vector.z()); }

/*!
    \overload

    Multiplies this matrix by a rotation of \a angle degrees counterclockwise about the axis (\a x,
    \a y, \a z). Multiples of 90 degrees are exact.
*/
void QMatrix4x4::rotate(float angle, float x, float y, float z) {
    if (angle == 0.0f)
        return;
    const double len = std::sqrt(double(x) * double(x) + double(y) * double(y) + double(z) * double(z));
    if (len == 0.0)
        return;
    const float ux = float(double(x) / len), uy = float(double(y) / len), uz = float(double(z) / len);
    float s = 0.0f, c = 1.0f;
    QtPrivate::exactSinCos(angle, &s, &c);
    const float ic = 1.0f - c;
    const QMatrix4x4 rot(ux * ux * ic + c, ux * uy * ic - uz * s, ux * uz * ic + uy * s, 0.0f, uy * ux * ic + uz * s,
                         uy * uy * ic + c, uy * uz * ic - ux * s, 0.0f, ux * uz * ic - uy * s, uy * uz * ic + ux * s,
                         uz * uz * ic + c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    *this *= rot;
}

/*!
    \overload

    Multiplies this matrix by the rotation \a quaternion, which should be a unit quaternion.
*/
void QMatrix4x4::rotate(const QQuaternion &quaternion) {
    const float x = quaternion.x(), y = quaternion.y(), z = quaternion.z(), w = quaternion.scalar();
    const QMatrix4x4 rot(1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y - w * z), 2.0f * (x * z + w * y), 0.0f,
                         2.0f * (x * y + w * z), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z - w * x), 0.0f,
                         2.0f * (x * z - w * y), 2.0f * (y * z + w * x), 1.0f - 2.0f * (x * x + y * y), 0.0f, 0.0f,
                         0.0f, 0.0f, 1.0f);
    *this *= rot;
}

/*!
    Multiplies this matrix by an orthographic projection of \a rect onto the window, with y down as in
    widget coordinates and the depth range -1 to 1.
*/
void QMatrix4x4::ortho(const QRect &rect) {
    ortho(float(rect.x()), float(rect.x() + rect.width()), float(rect.y() + rect.height()), float(rect.y()), -1.0f,
          1.0f);
}

/*!
    \overload
*/
void QMatrix4x4::ortho(const QRectF &rect) {
    ortho(float(rect.left()), float(rect.right()), float(rect.bottom()), float(rect.top()), -1.0f, 1.0f);
}

/*!
    \overload

    Multiplies this matrix by an orthographic projection of the box from \a left to \a right, \a bottom
    to \a top and \a nearPlane to \a farPlane onto clip space. A box with no extent leaves the matrix unchanged.
*/
void QMatrix4x4::ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    if (left == right || bottom == top || nearPlane == farPlane)
        return;
    const float w = right - left, h = top - bottom, d = farPlane - nearPlane;
    *this *= QMatrix4x4(2.0f / w, 0.0f, 0.0f, -(left + right) / w, 0.0f, 2.0f / h, 0.0f, -(top + bottom) / h, 0.0f,
                        0.0f, -2.0f / d, -(nearPlane + farPlane) / d, 0.0f, 0.0f, 0.0f, 1.0f);
}

/*!
    Multiplies this matrix by a perspective projection of the frustum whose near face runs from \a left
    to \a right and \a bottom to \a top at \a nearPlane, out to \a farPlane, as \c glFrustum() does.

    \sa perspective()
*/
void QMatrix4x4::frustum(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    if (left == right || bottom == top || nearPlane == farPlane)
        return;
    const float w = right - left, h = top - bottom, d = farPlane - nearPlane;
    *this *= QMatrix4x4(2.0f * nearPlane / w, 0.0f, (left + right) / w, 0.0f, 0.0f, 2.0f * nearPlane / h,
                        (top + bottom) / h, 0.0f, 0.0f, 0.0f, -(nearPlane + farPlane) / d,
                        -2.0f * nearPlane * farPlane / d, 0.0f, 0.0f, -1.0f, 0.0f);
}

/*!
    Multiplies this matrix by a perspective projection with a vertical field of view of
    \a verticalAngle degrees, the width-to-height \a aspectRatio, and the clipping planes \a nearPlane
    and \a farPlane, which must both be positive.

    \sa frustum(), lookAt()
*/
void QMatrix4x4::perspective(float verticalAngle, float aspectRatio, float nearPlane, float farPlane) {
    if (nearPlane == farPlane || aspectRatio == 0.0f)
        return;
    const double radians = double(verticalAngle) / 2.0 * 3.14159265358979323846 / 180.0;
    const double sine = std::sin(radians);
    if (sine == 0.0)
        return;
    const float cotan = float(std::cos(radians) / sine);
    const float d = farPlane - nearPlane;
    *this *= QMatrix4x4(cotan / aspectRatio, 0.0f, 0.0f, 0.0f, 0.0f, cotan, 0.0f, 0.0f, 0.0f, 0.0f,
                        -(nearPlane + farPlane) / d, -2.0f * nearPlane * farPlane / d, 0.0f, 0.0f, -1.0f, 0.0f);
}

/*!
    Multiplies this matrix by a viewing transformation from \a eye toward \a center, with \a up
    pointing up on the screen. An \a eye at \a center leaves the matrix unchanged.

    \sa perspective()
*/
void QMatrix4x4::lookAt(const QVector3D &eye, const QVector3D &center, const QVector3D &up) {
    const QVector3D forward = (center - eye).normalized();
    if (forward.isNull())
        return;
    const QVector3D side = QVector3D::crossProduct(forward, up).normalized();
    const QVector3D upVector = QVector3D::crossProduct(side, forward);
    *this *= QMatrix4x4(side.x(), side.y(), side.z(), 0.0f, upVector.x(), upVector.y(), upVector.z(), 0.0f,
                        -forward.x(), -forward.y(), -forward.z(), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    translate(-eye);
}

/*!
    Multiplies this matrix by the transformation from normalized device coordinates to the window
    \a rect, with depth from 0 to 1.
*/
void QMatrix4x4::viewport(const QRectF &rect) {
    viewport(float(rect.x()), float(rect.y()), float(rect.width()), float(rect.height()));
}

/*!
    \overload

    Multiplies this matrix by the transformation from normalized device coordinates to the window
    \a width by \a height at (\a left, \a bottom), with depth from \a nearPlane to \a farPlane.
*/
void QMatrix4x4::viewport(float left, float bottom, float width, float height, float nearPlane, float farPlane) {
    const float w2 = width / 2.0f, h2 = height / 2.0f;
    *this *= QMatrix4x4(w2, 0.0f, 0.0f, left + w2, 0.0f, h2, 0.0f, bottom + h2, 0.0f, 0.0f,
                        (farPlane - nearPlane) / 2.0f, (nearPlane + farPlane) / 2.0f, 0.0f, 0.0f, 0.0f, 1.0f);
}

/*!
    Flips the y and z axes, to change between coordinates with y up and with y down.
*/
void QMatrix4x4::flipCoordinates() { scale(1.0f, -1.0f, -1.0f); }

/*!
    Copies the 16 elements to \a values, in row-major order.

    \sa data()
*/
void QMatrix4x4::copyDataTo(float *values) const {
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            values[row * 4 + col] = m[col][row];
}

/*!
    Returns the 2D transformation this matrix makes in the plane z = 0, dropping z.

    \sa QMatrix4x4(const QTransform &)
*/
QTransform QMatrix4x4::toTransform() const {
    return QTransform(qreal(m[0][0]), qreal(m[0][1]), qreal(m[0][3]), qreal(m[1][0]), qreal(m[1][1]), qreal(m[1][3]),
                      qreal(m[3][0]), qreal(m[3][1]), qreal(m[3][3]));
}

/*!
    \overload

    Returns the 2D transformation this matrix makes, seen in perspective from \a distanceToPlane in
    front of the plane z = 0, as QTransform::rotate() does about the x and y axes. A distance of 0 is
    the same as toTransform().
*/
QTransform QMatrix4x4::toTransform(float distanceToPlane) const {
    if (distanceToPlane == 0.0f)
        return toTransform();
    const float inv = 1.0f / distanceToPlane;
    return QTransform(qreal(m[0][0]), qreal(m[0][1]), qreal(m[0][3] - m[0][2] * inv), qreal(m[1][0]), qreal(m[1][1]),
                      qreal(m[1][3] - m[1][2] * inv), qreal(m[3][0]), qreal(m[3][1]), qreal(m[3][3] - m[3][2] * inv));
}

/*!
    Returns \a point mapped by this matrix and rounded to integers.
*/
QPoint QMatrix4x4::map(const QPoint &point) const { return map(QPointF(point)).toPoint(); }

/*!
    \overload

    Returns \a point, taken as (x, y, 0, 1), mapped by this matrix and divided by w.
*/
QPointF QMatrix4x4::map(const QPointF &point) const {
    const QVector3D v = map(QVector3D(point));
    return v.toPointF();
}

/*!
    \overload

    Returns \a point, taken as (x, y, z, 1), mapped by this matrix and divided by w.

    \sa mapVector()
*/
QVector3D QMatrix4x4::map(const QVector3D &point) const {
    const QVector4D v = map(QVector4D(point, 1.0f));
    if (v.w() == 1.0f || v.w() == 0.0f)
        return v.toVector3D();
    return v.toVector3D() / v.w();
}

/*!
    Returns \a vector mapped by the upper-left 3x3 part of this matrix: rotated and scaled, but not
    translated or projected. Use it for directions.

    \sa map()
*/
QVector3D QMatrix4x4::mapVector(const QVector3D &vector) const {
    return QVector3D(m[0][0] * vector.x() + m[1][0] * vector.y() + m[2][0] * vector.z(),
                     m[0][1] * vector.x() + m[1][1] * vector.y() + m[2][1] * vector.z(),
                     m[0][2] * vector.x() + m[1][2] * vector.y() + m[2][2] * vector.z());
}

/*!
    \overload

    Returns \a point mapped by this matrix, with no division by w.
*/
QVector4D QMatrix4x4::map(const QVector4D &point) const {
    QVector4D out;
    for (int r = 0; r < 4; ++r)
        out[r] = m[0][r] * point.x() + m[1][r] * point.y() + m[2][r] * point.z() + m[3][r] * point.w();
    return out;
}

/*!
    Returns the bounding rectangle of \a rect mapped by this matrix, rounded to integers.
*/
QRect QMatrix4x4::mapRect(const QRect &rect) const { return mapRect(QRectF(rect)).toRect(); }

/*!
    \overload

    Returns the bounding rectangle of the four corners of \a rect mapped by this matrix.
*/
QRectF QMatrix4x4::mapRect(const QRectF &rect) const {
    const QPointF corners[4] = {map(rect.topLeft()), map(rect.topRight()), map(rect.bottomLeft()),
                                map(rect.bottomRight())};
    qreal left = corners[0].x(), right = left, top = corners[0].y(), bottom = top;
    for (const QPointF &p : corners) {
        left = std::min(left, p.x());
        right = std::max(right, p.x());
        top = std::min(top, p.y());
        bottom = std::max(bottom, p.y());
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}
