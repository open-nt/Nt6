// MIT License - see LICENSE
#pragma once

#include <QtPositioning/qgeoshape.h>

class QGeoCircle : public QGeoShape {
    Q_GADGET
    Q_PROPERTY(QGeoCoordinate center READ center WRITE setCenter)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius)

public:
    QGeoCircle();
    QGeoCircle(const QGeoCoordinate &center, qreal radius = -1.0);
    QGeoCircle(const QGeoShape &other);

    void setCenter(const QGeoCoordinate &center) { m_a = center; }
    QGeoCoordinate center() const { return m_a; }
    void setRadius(qreal radius) { m_radius = radius; }
    qreal radius() const { return m_radius; }

    void translate(double degreesLatitude, double degreesLongitude);
    QGeoCircle translated(double degreesLatitude, double degreesLongitude) const;
    void extendCircle(const QGeoCoordinate &coordinate);

    QString toString() const;
};

Q_DECLARE_METATYPE(QGeoCircle)
