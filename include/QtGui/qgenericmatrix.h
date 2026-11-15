// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

/*!
    \class QGenericMatrix
    \brief The QGenericMatrix class is a matrix of N columns and M rows of T, stored column by column.

    QMatrix3x3 is the 3 by 3 matrix of float that QMatrix4x4::normalMatrix() returns and that
    QOpenGLShaderProgram::setUniformValue() takes.

    \differences
    Only construction, element access, identity and transposition are provided; there is no arithmetic.
    \enddifferences
*/
template <int N, int M, typename T> class QGenericMatrix {
public:
    /*! Constructs an identity matrix (ones on the diagonal, zeros elsewhere). */
    QGenericMatrix() { setToIdentity(); }
    /*! Constructs a matrix from the N times M \a values, given row by row. */
    explicit QGenericMatrix(const T *values) {
        for (int col = 0; col < N; ++col)
            for (int row = 0; row < M; ++row)
                m[col][row] = values[row * N + col];
    }
    /*! Returns the element at \a row and \a column. */
    const T &operator()(int row, int column) const { return m[column][row]; }
    /*! Returns a reference to the element at \a row and \a column. */
    T &operator()(int row, int column) { return m[column][row]; }
    /*! Returns \c true if the matrix is the identity. */
    bool isIdentity() const {
        for (int col = 0; col < N; ++col)
            for (int row = 0; row < M; ++row)
                if (m[col][row] != (row == col ? T(1) : T(0)))
                    return false;
        return true;
    }
    /*! Makes the matrix the identity. */
    void setToIdentity() {
        for (int col = 0; col < N; ++col)
            for (int row = 0; row < M; ++row)
                m[col][row] = row == col ? T(1) : T(0);
    }
    /*! Returns the matrix with its rows and columns swapped. */
    QGenericMatrix<M, N, T> transposed() const {
        QGenericMatrix<M, N, T> result;
        for (int col = 0; col < N; ++col)
            for (int row = 0; row < M; ++row)
                result(col, row) = m[col][row];
        return result;
    }
    /*! Returns the elements, column by column. */
    T *data() { return &m[0][0]; }
    /*! \overload */
    const T *data() const { return &m[0][0]; }
    /*! Returns the elements, column by column. */
    const T *constData() const { return &m[0][0]; }

private:
    T m[static_cast<unsigned>(N)][static_cast<unsigned>(M)];
};

using QMatrix2x2 = QGenericMatrix<2, 2, float>;
using QMatrix3x3 = QGenericMatrix<3, 3, float>;
