// MIT License - see LICENSE
//
// QGeoCoordinate. Distances and azimuths use a sphere of the Earth's mean radius, as Qt Positioning
// does, so that results match Qt; Nt Twin provides exact ellipsoidal geodesy.
#include "qgeocoordinate.h"
#include "qpositioningmath_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qnumeric.h>

#include <cmath>

namespace {

using QtPositioningPrivate::degrees;
using QtPositioningPrivate::kEarthMeanRadius;
using QtPositioningPrivate::radians;

bool sameOrBothNaN(double a, double b) { return (std::isnan(a) && std::isnan(b)) || qFuzzyCompare(a, b); }

QString angle(double value, QGeoCoordinate::CoordinateFormat format) {
    const double magnitude = std::fabs(value);
    switch (format) {
    case QGeoCoordinate::Degrees:
    case QGeoCoordinate::DegreesWithHemisphere:
        return QString::number(magnitude, 'f', 5) + QChar(0x00B0);
    case QGeoCoordinate::DegreesMinutes:
    case QGeoCoordinate::DegreesMinutesWithHemisphere: {
        int whole = static_cast<int>(magnitude);
        double minutes = (magnitude - whole) * 60.0;
        if (minutes >= 59.9995) { // would print as 60.000'
            ++whole;
            minutes = 0.0;
        }
        return QString::number(whole) + QChar(0x00B0) + QLatin1Char(' ') + QString::number(minutes, 'f', 3) +
               QLatin1Char('\'');
    }
    case QGeoCoordinate::DegreesMinutesSeconds:
    case QGeoCoordinate::DegreesMinutesSecondsWithHemisphere: {
        int whole = static_cast<int>(magnitude);
        int minutes = static_cast<int>((magnitude - whole) * 60.0);
        double seconds = (magnitude - whole - minutes / 60.0) * 3600.0;
        if (seconds >= 59.95) { // would print as 60.0"
            seconds = 0.0;
            if (++minutes == 60) {
                minutes = 0;
                ++whole;
            }
        }
        return QString::number(whole) + QChar(0x00B0) + QLatin1Char(' ') + QString::number(minutes) +
               QLatin1String("' ") + QString::number(seconds, 'f', 1) + QLatin1Char('"');
    }
    }
    return QString();
}

} // namespace

/*!
    \module NtPositioning
    \title Nt Positioning C++ Classes
    \brief Geographic coordinates and shapes, position updates and NMEA 0183 receivers.
    \cmaketarget Nt::Positioning

    Nt Positioning is the core of Qt Positioning: QGeoCoordinate, QGeoRectangle and QGeoCircle,
    position updates (QGeoPositionInfo) from a QGeoPositionInfoSource, satellite information
    (QGeoSatelliteInfo) and QNmeaPositionInfoSource, which reads a GNSS receiver or a recorded log
    in NMEA 0183. It depends only on Nt Core.

    See \l{Using Nt Positioning} for an introduction.
*/

/*!
    \class QGeoCoordinate
    \inmodule NtPositioning
    \brief The QGeoCoordinate class defines a geographical position on the surface of the Earth.

    A coordinate has a latitude and longitude in degrees (WGS84) and an optional altitude in
    metres. It is valid when both angles are in range; it is 3D when it also has an altitude.

    \code
    const QGeoCoordinate stonehenge(51.1789, -1.8262, 102.0);
    const QGeoCoordinate avebury(51.4287, -1.8544);
    const qreal metres = stonehenge.distanceTo(avebury);     // about 27.9 km
    const qreal bearing = stonehenge.azimuthTo(avebury);     // about 356 degrees
    const QGeoCoordinate ahead = stonehenge.atDistanceAndAzimuth(1000.0, 90.0);
    \endcode

    distanceTo(), azimuthTo() and atDistanceAndAzimuth() treat the Earth as a sphere of radius
    6371.0072 km, as Qt does. They are within about 0.5% of the ellipsoidal values; for exact
    geodesy, convert to QtTwin::GeoPosition and use Nt Twin.

    \differences
    There is no QDataStream operator. Equality treats longitudes of 180 and -180 degrees as the
    same.
    \enddifferences

    \sa QGeoRectangle, QGeoCircle, QGeoPositionInfo
*/

/*!
    \enum QGeoCoordinate::CoordinateType

    \value InvalidCoordinate The latitude or longitude is missing or out of range.
    \value Coordinate2D A valid coordinate without an altitude.
    \value Coordinate3D A valid coordinate with an altitude.
*/

/*!
    \enum QGeoCoordinate::CoordinateFormat

    The formats of toString(), shown for latitude -27.46758 and longitude 153.027892:

    \value Degrees \c{-27.46758°, 153.02789°}
    \value DegreesWithHemisphere \c{27.46758° S, 153.02789° E}
    \value DegreesMinutes \c{-27° 28.055', 153° 1.674'}
    \value DegreesMinutesWithHemisphere \c{27° 28.055' S, 153° 1.674' E}
    \value DegreesMinutesSeconds \c{-27° 28' 3.3", 153° 1' 40.4"}
    \value DegreesMinutesSecondsWithHemisphere \c{27° 28' 3.3" S, 153° 1' 40.4" E}
*/

/*!
    Constructs an invalid coordinate: latitude, longitude and altitude are NaN.
*/
QGeoCoordinate::QGeoCoordinate() : m_latitude(qQNaN()), m_longitude(qQNaN()), m_altitude(qQNaN()) {}

/*!
    Constructs a 2D coordinate at \a latitude and \a longitude, in degrees. The coordinate is
    invalid if either is out of range.
*/
QGeoCoordinate::QGeoCoordinate(double latitude, double longitude)
    : m_latitude(latitude), m_longitude(longitude), m_altitude(qQNaN()) {}

/*!
    Constructs a 3D coordinate at \a latitude and \a longitude, in degrees, and \a altitude metres.
*/
QGeoCoordinate::QGeoCoordinate(double latitude, double longitude, double altitude)
    : m_latitude(latitude), m_longitude(longitude), m_altitude(altitude) {}

/*!
    Returns \c true if the latitude lies in [-90, 90] and the longitude in [-180, 180]; otherwise
    returns \c false.
*/
bool QGeoCoordinate::isValid() const { return type() != InvalidCoordinate; }

/*!
    Returns whether the coordinate is invalid, 2D or 3D.
*/
QGeoCoordinate::CoordinateType QGeoCoordinate::type() const {
    if (!(m_latitude >= -90.0 && m_latitude <= 90.0) || !(m_longitude >= -180.0 && m_longitude <= 180.0))
        return InvalidCoordinate;
    return std::isnan(m_altitude) ? Coordinate2D : Coordinate3D;
}

/*!
    \fn void QGeoCoordinate::setLatitude(double latitude)
    Sets the latitude to \a latitude degrees.
*/

/*!
    \fn double QGeoCoordinate::latitude() const
    Returns the latitude in degrees, or NaN if none was set.
*/

/*!
    \fn void QGeoCoordinate::setLongitude(double longitude)
    Sets the longitude to \a longitude degrees.
*/

/*!
    \fn double QGeoCoordinate::longitude() const
    Returns the longitude in degrees, or NaN if none was set.
*/

/*!
    \fn void QGeoCoordinate::setAltitude(double altitude)
    Sets the altitude to \a altitude metres.
*/

/*!
    \fn double QGeoCoordinate::altitude() const
    Returns the altitude in metres, or NaN if the coordinate is 2D.
*/

/*!
    Returns the great-circle distance in metres from this coordinate to \a other, or 0 if either is
    invalid. Altitudes are ignored.
*/
qreal QGeoCoordinate::distanceTo(const QGeoCoordinate &other) const {
    if (!isValid() || !other.isValid())
        return 0.0;
    const double dLat = radians(other.m_latitude - m_latitude);
    const double dLon = radians(other.m_longitude - m_longitude);
    const double h = std::sin(dLat / 2) * std::sin(dLat / 2) + std::cos(radians(m_latitude)) *
                                                                   std::cos(radians(other.m_latitude)) *
                                                                   std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * kEarthMeanRadius * std::atan2(std::sqrt(h), std::sqrt(1.0 - h));
}

/*!
    Returns the initial bearing in degrees, clockwise from north in [0, 360), of the great circle from
    this coordinate to \a other, or 0 if either is invalid.
*/
qreal QGeoCoordinate::azimuthTo(const QGeoCoordinate &other) const {
    if (!isValid() || !other.isValid())
        return 0.0;
    const double lat1 = radians(m_latitude);
    const double lat2 = radians(other.m_latitude);
    const double dLon = radians(other.m_longitude - m_longitude);
    const double y = std::sin(dLon) * std::cos(lat2);
    const double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    const double azimuth = std::fmod(degrees(std::atan2(y, x)) + 360.0, 360.0);
    return azimuth >= 360.0 ? 0.0 : azimuth;
}

/*!
    Returns the coordinate reached by travelling \a distance metres along the great circle that
    starts at this coordinate with bearing \a azimuth degrees, and rising \a distanceUp metres. The
    result is invalid if this coordinate is. A 2D coordinate stays 2D.
*/
QGeoCoordinate QGeoCoordinate::atDistanceAndAzimuth(qreal distance, qreal azimuth, qreal distanceUp) const {
    if (!isValid())
        return QGeoCoordinate();
    const double lat1 = radians(m_latitude);
    const double lon1 = radians(m_longitude);
    const double d = distance / kEarthMeanRadius;
    const double az = radians(azimuth);
    const double lat2 = std::asin(std::sin(lat1) * std::cos(d) + std::cos(lat1) * std::sin(d) * std::cos(az));
    const double lon2 =
        lon1 + std::atan2(std::sin(az) * std::sin(d) * std::cos(lat1), std::cos(d) - std::sin(lat1) * std::sin(lat2));
    double lon = std::fmod(degrees(lon2) + 540.0, 360.0) - 180.0;
    if (lon < -180.0)
        lon += 360.0;
    QGeoCoordinate result(degrees(lat2), lon);
    if (!std::isnan(m_altitude))
        result.setAltitude(m_altitude + distanceUp);
    return result;
}

/*!
    Returns the coordinate as text in \a format, followed by the altitude in metres for a 3D
    coordinate, such as \c{27° 46' 12.0" S, 153° 1' 48.0" E, 28.1m}. An invalid coordinate gives an
    empty string.
*/
QString QGeoCoordinate::toString(CoordinateFormat format) const {
    if (!isValid())
        return QString();
    const bool hemisphere = format == DegreesWithHemisphere || format == DegreesMinutesWithHemisphere ||
                            format == DegreesMinutesSecondsWithHemisphere;
    QString lat = angle(m_latitude, format);
    QString lon = angle(m_longitude, format);
    if (hemisphere) {
        // The poles and the 180th meridian have no hemisphere.
        if (std::fabs(m_latitude) < 90.0 && m_latitude != 0.0)
            lat += m_latitude < 0.0 ? QLatin1String(" S") : QLatin1String(" N");
        if (std::fabs(m_longitude) < 180.0 && m_longitude != 0.0 && std::fabs(m_latitude) < 90.0)
            lon += m_longitude < 0.0 ? QLatin1String(" W") : QLatin1String(" E");
    } else {
        if (m_latitude < 0.0)
            lat.prepend(QLatin1Char('-'));
        if (m_longitude < 0.0)
            lon.prepend(QLatin1Char('-'));
    }
    QString text = lat + QLatin1String(", ") + lon;
    if (!std::isnan(m_altitude))
        text += QLatin1String(", ") + QString::number(m_altitude) + QLatin1Char('m');
    return text;
}

bool QGeoCoordinate::equals(const QGeoCoordinate &other) const {
    bool sameLongitude = sameOrBothNaN(m_longitude, other.m_longitude) ||
                         (std::fabs(m_longitude) == 180.0 && std::fabs(other.m_longitude) == 180.0);
    return sameOrBothNaN(m_latitude, other.m_latitude) && sameLongitude && sameOrBothNaN(m_altitude, other.m_altitude);
}

// Writes the coordinate to the debug stream.
QDebug operator<<(QDebug debug, const QGeoCoordinate &coordinate) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "QGeoCoordinate(";
    if (coordinate.isValid()) {
        debug << coordinate.latitude() << ", " << coordinate.longitude();
        if (coordinate.type() == QGeoCoordinate::Coordinate3D)
            debug << ", " << coordinate.altitude();
    } else {
        debug << "Invalid";
    }
    debug << ')';
    return debug;
}

#include "moc_qgeocoordinate.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
