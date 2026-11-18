// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qhash.h>
#include <QtCore/qmetatype.h>
#include <QtPositioning/qgeocoordinate.h>

class QDebug;

class QGeoPositionInfo {
public:
    enum Attribute {
        Direction,
        GroundSpeed,
        VerticalSpeed,
        MagneticVariation,
        HorizontalAccuracy,
        VerticalAccuracy,
        DirectionAccuracy
    };

    QGeoPositionInfo();
    QGeoPositionInfo(const QGeoCoordinate &coordinate, const QDateTime &updateTime);

    bool isValid() const;

    void setTimestamp(const QDateTime &timestamp) { m_timestamp = timestamp; }
    QDateTime timestamp() const { return m_timestamp; }
    void setCoordinate(const QGeoCoordinate &coordinate) { m_coordinate = coordinate; }
    QGeoCoordinate coordinate() const { return m_coordinate; }

    void setAttribute(Attribute attribute, qreal value) { m_attributes.insert(attribute, value); }
    qreal attribute(Attribute attribute) const;
    void removeAttribute(Attribute attribute) { m_attributes.remove(attribute); }
    bool hasAttribute(Attribute attribute) const { return m_attributes.contains(attribute); }

    friend bool operator==(const QGeoPositionInfo &lhs, const QGeoPositionInfo &rhs) { return lhs.equals(rhs); }
    friend bool operator!=(const QGeoPositionInfo &lhs, const QGeoPositionInfo &rhs) { return !lhs.equals(rhs); }

private:
    bool equals(const QGeoPositionInfo &other) const;

    QDateTime m_timestamp;
    QGeoCoordinate m_coordinate;
    QHash<int, qreal> m_attributes;
};

QDebug operator<<(QDebug debug, const QGeoPositionInfo &info);

Q_DECLARE_METATYPE(QGeoPositionInfo)
