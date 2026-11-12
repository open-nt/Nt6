// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>

#include <initializer_list>

class QPolygonF;

class QPolygon : public QList<QPoint> {
public:
    QPolygon() = default;
    QPolygon(const QList<QPoint> &points) : QList<QPoint>(points) {}
    QPolygon(std::initializer_list<QPoint> points) : QList<QPoint>(points) {}
    explicit QPolygon(qsizetype size) : QList<QPoint>(size) {}
    QPolygon(const QRect &rectangle, bool closed = false);

    QPoint point(qsizetype index) const { return at(index); }
    void setPoint(qsizetype index, const QPoint &point) { (*this)[index] = point; }
    void setPoint(qsizetype index, int x, int y) { (*this)[index] = QPoint(x, y); }
    void putPoints(qsizetype index, qsizetype count, const QPolygon &from, qsizetype fromIndex = 0);

    QRect boundingRect() const;
    void translate(int dx, int dy);
    void translate(const QPoint &offset) { translate(offset.x(), offset.y()); }
    QPolygon translated(int dx, int dy) const;
    QPolygon translated(const QPoint &offset) const { return translated(offset.x(), offset.y()); }
    bool containsPoint(const QPoint &point, Qt::FillRule fillRule) const;
    QPolygonF toPolygonF() const;
};

class QPolygonF : public QList<QPointF> {
public:
    QPolygonF() = default;
    QPolygonF(const QList<QPointF> &points) : QList<QPointF>(points) {}
    QPolygonF(std::initializer_list<QPointF> points) : QList<QPointF>(points) {}
    explicit QPolygonF(qsizetype size) : QList<QPointF>(size) {}
    QPolygonF(const QRectF &rectangle);
    QPolygonF(const QPolygon &polygon);

    bool isClosed() const { return size() > 1 && first() == last(); }
    QRectF boundingRect() const;
    void translate(qreal dx, qreal dy);
    void translate(const QPointF &offset) { translate(offset.x(), offset.y()); }
    QPolygonF translated(qreal dx, qreal dy) const;
    QPolygonF translated(const QPointF &offset) const { return translated(offset.x(), offset.y()); }
    bool containsPoint(const QPointF &point, Qt::FillRule fillRule) const;
    QPolygon toPolygon() const;
};
