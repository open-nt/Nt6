// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qstring.h>

class QDebug;

class QGeoCoordinate {
    Q_GADGET
    Q_PROPERTY(double latitude READ latitude WRITE setLatitude)
    Q_PROPERTY(double longitude READ longitude WRITE setLongitude)
    Q_PROPERTY(double altitude READ altitude WRITE setAltitude)
    Q_PROPERTY(bool isValid READ isValid)

public:
    enum CoordinateType { InvalidCoordinate, Coordinate2D, Coordinate3D };
    Q_ENUM(CoordinateType)

    enum CoordinateFormat {
        Degrees,
        DegreesWithHemisphere,
        DegreesMinutes,
        DegreesMinutesWithHemisphere,
        DegreesMinutesSeconds,
        DegreesMinutesSecondsWithHemisphere
    };
    Q_ENUM(CoordinateFormat)

    QGeoCoordinate();
    QGeoCoordinate(double latitude, double longitude);
    QGeoCoordinate(double latitude, double longitude, double altitude);

    bool isValid() const;
    CoordinateType type() const;

    void setLatitude(double latitude) { m_latitude = latitude; }
    double latitude() const { return m_latitude; }
    void setLongitude(double longitude) { m_longitude = longitude; }
    double longitude() const { return m_longitude; }
    void setAltitude(double altitude) { m_altitude = altitude; }
    double altitude() const { return m_altitude; }

    qreal distanceTo(const QGeoCoordinate &other) const;
    qreal azimuthTo(const QGeoCoordinate &other) const;
    QGeoCoordinate atDistanceAndAzimuth(qreal distance, qreal azimuth, qreal distanceUp = 0.0) const;

    QString toString(CoordinateFormat format = DegreesMinutesSecondsWithHemisphere) const;

    friend bool operator==(const QGeoCoordinate &lhs, const QGeoCoordinate &rhs) { return lhs.equals(rhs); }
    friend bool operator!=(const QGeoCoordinate &lhs, const QGeoCoordinate &rhs) { return !lhs.equals(rhs); }

private:
    bool equals(const QGeoCoordinate &other) const;

    double m_latitude;
    double m_longitude;
    double m_altitude;
};

QDebug operator<<(QDebug debug, const QGeoCoordinate &coordinate);

Q_DECLARE_METATYPE(QGeoCoordinate)
