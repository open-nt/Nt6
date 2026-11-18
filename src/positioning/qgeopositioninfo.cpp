// MIT License - see LICENSE
#include "qgeopositioninfo.h"
#include "qgeosatelliteinfo.h"

#include <QtCore/qdebug.h>
#include <QtCore/qnumeric.h>

/*!
    \class QGeoPositionInfo
    \inmodule NtPositioning
    \brief The QGeoPositionInfo class holds a position fix: a coordinate, a time and optional
    measurements.

    A QGeoPositionInfoSource delivers one of these for each fix. Besides the coordinate and its UTC
    timestamp it may carry the attributes a receiver reports, such as ground speed and direction
    of travel, or the estimated horizontal accuracy.

    \code
    connect(source, &QGeoPositionInfoSource::positionUpdated, [](const QGeoPositionInfo &info) {
        if (info.hasAttribute(QGeoPositionInfo::GroundSpeed))
            qDebug() << info.coordinate() << info.attribute(QGeoPositionInfo::GroundSpeed) << "m/s";
    });
    \endcode

    \differences
    There is no QDataStream operator.
    \enddifferences

    \sa QGeoPositionInfoSource
*/

/*!
    \enum QGeoPositionInfo::Attribute

    \value Direction The direction of travel in degrees clockwise from true north.
    \value GroundSpeed The speed over ground in metres per second.
    \value VerticalSpeed The vertical speed in metres per second, positive upwards.
    \value MagneticVariation The angle in degrees between true north and magnetic north, positive
           when magnetic north is east of true north.
    \value HorizontalAccuracy The accuracy of the latitude and longitude in metres.
    \value VerticalAccuracy The accuracy of the altitude in metres.
    \value DirectionAccuracy The accuracy of the direction in degrees.
*/

/*!
    Constructs an invalid position: no coordinate and no timestamp.
*/
QGeoPositionInfo::QGeoPositionInfo() = default;

/*!
    Constructs a position at \a coordinate, measured at \a updateTime.
*/
QGeoPositionInfo::QGeoPositionInfo(const QGeoCoordinate &coordinate, const QDateTime &updateTime)
    : m_timestamp(updateTime), m_coordinate(coordinate) {}

/*!
    Returns \c true if both the timestamp and the coordinate are valid; otherwise returns \c false.
*/
bool QGeoPositionInfo::isValid() const { return m_timestamp.isValid() && m_coordinate.isValid(); }

/*!
    \fn void QGeoPositionInfo::setTimestamp(const QDateTime &timestamp)
    Sets the time of the fix to \a timestamp.
*/

/*!
    \fn QDateTime QGeoPositionInfo::timestamp() const
    Returns the time of the fix, in UTC for the sources of Nt Positioning.
*/

/*!
    \fn void QGeoPositionInfo::setCoordinate(const QGeoCoordinate &coordinate)
    Sets the coordinate to \a coordinate.
*/

/*!
    \fn QGeoCoordinate QGeoPositionInfo::coordinate() const
    Returns the coordinate.
*/

/*!
    \fn void QGeoPositionInfo::setAttribute(Attribute attribute, qreal value)
    Sets \a attribute to \a value.
*/

/*!
    Returns the value of \a attribute, or NaN if it has not been set.
*/
qreal QGeoPositionInfo::attribute(Attribute attribute) const { return m_attributes.value(attribute, qQNaN()); }

/*!
    \fn void QGeoPositionInfo::removeAttribute(Attribute attribute)
    Removes \a attribute.
*/

/*!
    \fn bool QGeoPositionInfo::hasAttribute(Attribute attribute) const
    Returns \c true if \a attribute has been set; otherwise returns \c false.
*/

bool QGeoPositionInfo::equals(const QGeoPositionInfo &other) const {
    return m_timestamp == other.m_timestamp && m_coordinate == other.m_coordinate && m_attributes == other.m_attributes;
}

// Writes the time, coordinate and attributes to the debug stream.
QDebug operator<<(QDebug debug, const QGeoPositionInfo &info) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "QGeoPositionInfo(" << info.timestamp() << ", " << info.coordinate();
    static const char *const names[] = {"Direction",         "GroundSpeed",        "VerticalSpeed",
                                        "MagneticVariation", "HorizontalAccuracy", "VerticalAccuracy",
                                        "DirectionAccuracy"};
    for (int i = QGeoPositionInfo::Direction; i <= QGeoPositionInfo::DirectionAccuracy; ++i) {
        const auto attribute = static_cast<QGeoPositionInfo::Attribute>(i);
        if (info.hasAttribute(attribute))
            debug << ", " << names[i] << '=' << info.attribute(attribute);
    }
    debug << ')';
    return debug;
}

/*!
    \class QGeoSatelliteInfo
    \inmodule NtPositioning
    \brief The QGeoSatelliteInfo class describes one satellite seen by a receiver.

    It identifies the satellite by its system and identifier (the PRN for GPS) and holds the
    signal strength and, as attributes, the elevation and azimuth at which the receiver sees it.

    \differences
    There is no QDataStream operator.
    \enddifferences

    \sa QGeoSatelliteInfoSource, QNmeaSatelliteInfoSource
*/

/*!
    \enum QGeoSatelliteInfo::Attribute

    \value Elevation The elevation of the satellite above the horizon, in degrees.
    \value Azimuth The azimuth of the satellite, in degrees clockwise from true north.
*/

/*!
    \enum QGeoSatelliteInfo::SatelliteSystem

    \value Undefined Not set.
    \value GPS The US Global Positioning System.
    \value GLONASS The Russian GLONASS system.
    \value GALILEO The European Galileo system.
    \value BEIDOU The Chinese BeiDou system.
    \value QZSS The Japanese Quasi-Zenith Satellite System.
    \value Multiple Several systems; used by sources that merge them.
    \value CustomType The first value free for systems not listed here.
*/

/*!
    \fn QGeoSatelliteInfo::QGeoSatelliteInfo()
    Constructs satellite information with no system, identifier -1 and signal strength -1.
*/

/*!
    \fn void QGeoSatelliteInfo::setSatelliteSystem(SatelliteSystem system)
    Sets the satellite system to \a system.
*/

/*!
    \fn QGeoSatelliteInfo::SatelliteSystem QGeoSatelliteInfo::satelliteSystem() const
    Returns the satellite system.
*/

/*!
    \fn void QGeoSatelliteInfo::setSatelliteIdentifier(int satId)
    Sets the identifier of the satellite within its system to \a satId.
*/

/*!
    \fn int QGeoSatelliteInfo::satelliteIdentifier() const
    Returns the identifier of the satellite within its system, or -1 if it is not set.
*/

/*!
    \fn void QGeoSatelliteInfo::setSignalStrength(int signalStrength)
    Sets the signal strength to \a signalStrength, in dB-Hz.
*/

/*!
    \fn int QGeoSatelliteInfo::signalStrength() const
    Returns the signal strength in dB-Hz, or -1 if it is not known.
*/

/*!
    \fn void QGeoSatelliteInfo::setAttribute(Attribute attribute, qreal value)
    Sets \a attribute to \a value.
*/

/*!
    Returns the value of \a attribute, or NaN if it has not been set.
*/
qreal QGeoSatelliteInfo::attribute(Attribute attribute) const { return m_attributes.value(attribute, qQNaN()); }

/*!
    \fn void QGeoSatelliteInfo::removeAttribute(Attribute attribute)
    Removes \a attribute.
*/

/*!
    \fn bool QGeoSatelliteInfo::hasAttribute(Attribute attribute) const
    Returns \c true if \a attribute has been set; otherwise returns \c false.
*/

// Writes the system, identifier, signal strength and attributes to the debug stream.
QDebug operator<<(QDebug debug, const QGeoSatelliteInfo &info) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "QGeoSatelliteInfo(system=" << int(info.satelliteSystem())
                    << ", id=" << info.satelliteIdentifier() << ", signal=" << info.signalStrength();
    if (info.hasAttribute(QGeoSatelliteInfo::Elevation))
        debug << ", elevation=" << info.attribute(QGeoSatelliteInfo::Elevation);
    if (info.hasAttribute(QGeoSatelliteInfo::Azimuth))
        debug << ", azimuth=" << info.attribute(QGeoSatelliteInfo::Azimuth);
    debug << ')';
    return debug;
}

#include "moc_qgeosatelliteinfo.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
