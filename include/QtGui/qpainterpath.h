// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtGui/qpolygon.h>
#include <QtGui/qtransform.h>

class QPainterPath {
public:
    enum ElementType { MoveToElement, LineToElement, CurveToElement, CurveToDataElement };

    class Element {
    public:
        qreal x = 0.0;
        qreal y = 0.0;
        ElementType type = MoveToElement;

        bool isMoveTo() const { return type == MoveToElement; }
        bool isLineTo() const { return type == LineToElement; }
        bool isCurveTo() const { return type == CurveToElement; }
        operator QPointF() const { return QPointF(x, y); }
        bool operator==(const Element &e) const { return x == e.x && y == e.y && type == e.type; }
        bool operator!=(const Element &e) const { return !(*this == e); }
    };

    QPainterPath() = default;
    explicit QPainterPath(const QPointF &startPoint);

    void moveTo(const QPointF &point);
    void moveTo(qreal x, qreal y) { moveTo(QPointF(x, y)); }
    void lineTo(const QPointF &endPoint);
    void lineTo(qreal x, qreal y) { lineTo(QPointF(x, y)); }
    void cubicTo(const QPointF &c1, const QPointF &c2, const QPointF &endPoint);
    void cubicTo(qreal c1X, qreal c1Y, qreal c2X, qreal c2Y, qreal endPointX, qreal endPointY) {
        cubicTo(QPointF(c1X, c1Y), QPointF(c2X, c2Y), QPointF(endPointX, endPointY));
    }
    void quadTo(const QPointF &c, const QPointF &endPoint);
    void quadTo(qreal cx, qreal cy, qreal endPointX, qreal endPointY) {
        quadTo(QPointF(cx, cy), QPointF(endPointX, endPointY));
    }
    void arcTo(const QRectF &rectangle, qreal startAngle, qreal sweepLength);
    void arcTo(qreal x, qreal y, qreal w, qreal h, qreal startAngle, qreal arcLength) {
        arcTo(QRectF(x, y, w, h), startAngle, arcLength);
    }
    void arcMoveTo(const QRectF &rectangle, qreal angle);
    void arcMoveTo(qreal x, qreal y, qreal w, qreal h, qreal angle) { arcMoveTo(QRectF(x, y, w, h), angle); }
    void closeSubpath();

    void addRect(const QRectF &rectangle);
    void addRect(qreal x, qreal y, qreal width, qreal height) { addRect(QRectF(x, y, width, height)); }
    void addEllipse(const QRectF &boundingRectangle);
    void addEllipse(qreal x, qreal y, qreal width, qreal height) { addEllipse(QRectF(x, y, width, height)); }
    void addEllipse(const QPointF &center, qreal rx, qreal ry) {
        addEllipse(QRectF(center.x() - rx, center.y() - ry, 2 * rx, 2 * ry));
    }
    void addRoundedRect(const QRectF &rect, qreal xRadius, qreal yRadius, Qt::SizeMode mode = Qt::AbsoluteSize);
    void addRoundedRect(qreal x, qreal y, qreal w, qreal h, qreal xRadius, qreal yRadius,
                        Qt::SizeMode mode = Qt::AbsoluteSize) {
        addRoundedRect(QRectF(x, y, w, h), xRadius, yRadius, mode);
    }
    void addPolygon(const QPolygonF &polygon);
    void addPath(const QPainterPath &path);
    void connectPath(const QPainterPath &path);

    QPointF currentPosition() const;
    bool isEmpty() const;
    int elementCount() const { return static_cast<int>(m_elements.size()); }
    QPainterPath::Element elementAt(int index) const { return m_elements.at(index); }
    void setElementPositionAt(int index, qreal x, qreal y);
    void clear();
    void reserve(int size) { m_elements.reserve(size); }
    int capacity() const { return static_cast<int>(m_elements.capacity()); }

    Qt::FillRule fillRule() const { return m_fillRule; }
    void setFillRule(Qt::FillRule fillRule) { m_fillRule = fillRule; }

    QRectF boundingRect() const;
    QRectF controlPointRect() const;
    bool contains(const QPointF &point) const;
    bool contains(const QRectF &rectangle) const;
    bool intersects(const QRectF &rectangle) const;

    void translate(qreal dx, qreal dy);
    void translate(const QPointF &offset) { translate(offset.x(), offset.y()); }
    QPainterPath translated(qreal dx, qreal dy) const;
    QPainterPath translated(const QPointF &offset) const { return translated(offset.x(), offset.y()); }

    QList<QPolygonF> toSubpathPolygons(const QTransform &matrix = QTransform()) const;
    QList<QPolygonF> toFillPolygons(const QTransform &matrix = QTransform()) const;
    QPolygonF toFillPolygon(const QTransform &matrix = QTransform()) const;

    qreal length() const;
    qreal percentAtLength(qreal len) const;
    QPointF pointAtPercent(qreal t) const;
    qreal angleAtPercent(qreal t) const;
    qreal slopeAtPercent(qreal t) const;

    void swap(QPainterPath &other) noexcept {
        m_elements.swap(other.m_elements);
        std::swap(m_fillRule, other.m_fillRule);
        std::swap(m_closed, other.m_closed);
    }

    QPainterPath operator+(const QPainterPath &other) const;
    QPainterPath &operator+=(const QPainterPath &other);
    bool operator==(const QPainterPath &path) const;
    bool operator!=(const QPainterPath &path) const { return !(*this == path); }

private:
    void ensureStart();

    QList<Element> m_elements;
    Qt::FillRule m_fillRule = Qt::OddEvenFill;
    bool m_closed = false; // the last subpath is closed: the next segment starts a new one at its start
};
