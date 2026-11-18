// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>

class QDebug;

class QGeoSatelliteInfo {
    Q_GADGET
    Q_PROPERTY(SatelliteSystem satelliteSystem READ satelliteSystem)
    Q_PROPERTY(int satelliteIdentifier READ satelliteIdentifier)
    Q_PROPERTY(int signalStrength READ signalStrength)

public:
    enum Attribute { Elevation, Azimuth };
    Q_ENUM(Attribute)

    enum SatelliteSystem {
        Undefined = 0x00,
        GPS = 0x01,
        GLONASS = 0x02,
        GALILEO = 0x03,
        BEIDOU = 0x04,
        QZSS = 0x05,
        Multiple = 0xFF,
        CustomType = 0x100
    };
    Q_ENUM(SatelliteSystem)

    QGeoSatelliteInfo() = default;

    void setSatelliteSystem(SatelliteSystem system) { m_system = system; }
    SatelliteSystem satelliteSystem() const { return m_system; }
    void setSatelliteIdentifier(int satId) { m_identifier = satId; }
    int satelliteIdentifier() const { return m_identifier; }
    void setSignalStrength(int signalStrength) { m_signalStrength = signalStrength; }
    int signalStrength() const { return m_signalStrength; }

    void setAttribute(Attribute attribute, qreal value) { m_attributes.insert(attribute, value); }
    qreal attribute(Attribute attribute) const;
    void removeAttribute(Attribute attribute) { m_attributes.remove(attribute); }
    bool hasAttribute(Attribute attribute) const { return m_attributes.contains(attribute); }

    friend bool operator==(const QGeoSatelliteInfo &lhs, const QGeoSatelliteInfo &rhs) {
        return lhs.m_system == rhs.m_system && lhs.m_identifier == rhs.m_identifier &&
               lhs.m_signalStrength == rhs.m_signalStrength && lhs.m_attributes == rhs.m_attributes;
    }
    friend bool operator!=(const QGeoSatelliteInfo &lhs, const QGeoSatelliteInfo &rhs) { return !(lhs == rhs); }

private:
    SatelliteSystem m_system = Undefined;
    int m_identifier = -1;
    int m_signalStrength = -1;
    QHash<int, qreal> m_attributes;
};

QDebug operator<<(QDebug debug, const QGeoSatelliteInfo &info);

Q_DECLARE_METATYPE(QGeoSatelliteInfo)
