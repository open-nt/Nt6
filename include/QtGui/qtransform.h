// MIT License - see LICENSE
#pragma once

#include <QtCore/qline.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qnamespace.h>

class QPainterPath;
class QPolygon;
class QPolygonF;

class QTransform {
public:
    enum TransformationType {
        TxNone = 0x00,
        TxTranslate = 0x01,
        TxScale = 0x02,
        TxRotate = 0x04,
        TxShear = 0x08,
        TxProject = 0x10
    };

    QTransform() = default;
    QTransform(qreal h11, qreal h12, qreal h21, qreal h22, qreal dx, qreal dy);
    QTransform(qreal h11, qreal h12, qreal h13, qreal h21, qreal h22, qreal h23, qreal h31, qreal h32, qreal h33);

    bool isAffine() const { return a13 == 0.0 && a23 == 0.0 && a33 == 1.0; }
    bool isIdentity() const { return type() == TxNone; }
    bool isInvertible() const { return determinant() != 0.0; }
    bool isScaling() const { return type() >= TxScale; }
    bool isRotating() const { return type() >= TxRotate; }
    bool isTranslating() const { return type() >= TxTranslate; }
    TransformationType type() const;

    qreal determinant() const;

    qreal m11() const { return a11; }
    qreal m12() const { return a12; }
    qreal m13() const { return a13; }
    qreal m21() const { return a21; }
    qreal m22() const { return a22; }
    qreal m23() const { return a23; }
    qreal m31() const { return a31; }
    qreal m32() const { return a32; }
    qreal m33() const { return a33; }
    qreal dx() const { return a31; }
    qreal dy() const { return a32; }

    void setMatrix(qreal h11, qreal h12, qreal h13, qreal h21, qreal h22, qreal h23, qreal h31, qreal h32, qreal h33);
    void reset() { *this = QTransform(); }

    QTransform inverted(bool *invertible = nullptr) const;
    QTransform adjoint() const;
    QTransform transposed() const;

    QTransform &translate(qreal dx, qreal dy);
    QTransform &scale(qreal sx, qreal sy);
    QTransform &shear(qreal sh, qreal sv);
    QTransform &rotate(qreal a, Qt::Axis axis = Qt::ZAxis, qreal distanceToPlane = 1024.0);
    QTransform &rotateRadians(qreal a, Qt::Axis axis = Qt::ZAxis, qreal distanceToPlane = 1024.0);

    static QTransform fromTranslate(qreal dx, qreal dy);
    static QTransform fromScale(qreal dx, qreal dy);

    QPoint map(const QPoint &point) const;
    QPointF map(const QPointF &point) const;
    QLine map(const QLine &line) const;
    QLineF map(const QLineF &line) const;
    QPolygonF map(const QPolygonF &polygon) const;
    QPolygon map(const QPolygon &polygon) const;
    QPainterPath map(const QPainterPath &path) const;
    void map(qreal x, qreal y, qreal *tx, qreal *ty) const;
    void map(int x, int y, int *tx, int *ty) const;
    QRect mapRect(const QRect &rect) const;
    QRectF mapRect(const QRectF &rect) const;
    QPolygon mapToPolygon(const QRect &rect) const;

    QTransform &operator*=(const QTransform &other);
    QTransform operator*(const QTransform &other) const;
    QTransform &operator*=(qreal factor);
    QTransform &operator/=(qreal divisor);
    QTransform &operator+=(qreal value);
    QTransform &operator-=(qreal value);

    friend bool operator==(const QTransform &a, const QTransform &b) {
        return a.a11 == b.a11 && a.a12 == b.a12 && a.a13 == b.a13 && a.a21 == b.a21 && a.a22 == b.a22 &&
               a.a23 == b.a23 && a.a31 == b.a31 && a.a32 == b.a32 && a.a33 == b.a33;
    }
    friend bool operator!=(const QTransform &a, const QTransform &b) { return !(a == b); }

private:
    qreal a11 = 1.0, a12 = 0.0, a13 = 0.0;
    qreal a21 = 0.0, a22 = 1.0, a23 = 0.0;
    qreal a31 = 0.0, a32 = 0.0, a33 = 1.0;
};

QPointF operator*(const QPointF &point, const QTransform &matrix);
QPoint operator*(const QPoint &point, const QTransform &matrix);
QLineF operator*(const QLineF &line, const QTransform &matrix);
QPolygonF operator*(const QPolygonF &polygon, const QTransform &matrix);
QPainterPath operator*(const QPainterPath &path, const QTransform &matrix);
