// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtPositioning/qgeoshape.h>

class QGeoRectangle : public QGeoShape {
    Q_GADGET
    Q_PROPERTY(QGeoCoordinate bottomLeft READ bottomLeft WRITE setBottomLeft)
    Q_PROPERTY(QGeoCoordinate bottomRight READ bottomRight WRITE setBottomRight)
    Q_PROPERTY(QGeoCoordinate topLeft READ topLeft WRITE setTopLeft)
    Q_PROPERTY(QGeoCoordinate topRight READ topRight WRITE setTopRight)
    Q_PROPERTY(QGeoCoordinate center READ center WRITE setCenter)
    Q_PROPERTY(double height READ height WRITE setHeight)
    Q_PROPERTY(double width READ width WRITE setWidth)

public:
    QGeoRectangle();
    QGeoRectangle(const QGeoCoordinate &center, double degreesWidth, double degreesHeight);
    QGeoRectangle(const QGeoCoordinate &topLeft, const QGeoCoordinate &bottomRight);
    QGeoRectangle(const QList<QGeoCoordinate> &coordinates);
    QGeoRectangle(const QGeoShape &other);

    void setTopLeft(const QGeoCoordinate &topLeft) { m_a = topLeft; }
    QGeoCoordinate topLeft() const { return m_a; }
    void setTopRight(const QGeoCoordinate &topRight);
    QGeoCoordinate topRight() const;
    void setBottomLeft(const QGeoCoordinate &bottomLeft);
    QGeoCoordinate bottomLeft() const;
    void setBottomRight(const QGeoCoordinate &bottomRight) { m_b = bottomRight; }
    QGeoCoordinate bottomRight() const { return m_b; }

    void setCenter(const QGeoCoordinate &center);
    QGeoCoordinate center() const;
    void setWidth(double degreesWidth);
    double width() const;
    void setHeight(double degreesHeight);
    double height() const;

    using QGeoShape::contains;
    bool contains(const QGeoRectangle &rectangle) const;
    bool intersects(const QGeoRectangle &rectangle) const;

    void translate(double degreesLatitude, double degreesLongitude);
    QGeoRectangle translated(double degreesLatitude, double degreesLongitude) const;
    void extendRectangle(const QGeoCoordinate &coordinate);
    QGeoRectangle united(const QGeoRectangle &rectangle) const;
    QGeoRectangle operator|(const QGeoRectangle &rectangle) const { return united(rectangle); }
    QGeoRectangle &operator|=(const QGeoRectangle &rectangle);

    QString toString() const;
};

Q_DECLARE_METATYPE(QGeoRectangle)
