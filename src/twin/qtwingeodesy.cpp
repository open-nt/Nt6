// MIT License - see LICENSE
//
// Geodesy for Nt Twin: WGS84 geodetic <-> ECEF, local east-north-up frames and the transverse Mercator
// projection used by UTM. The ECEF to geodetic conversion is Heikkinen's closed form (1982) for the
// latitude, followed by the usual projection for the height. The transverse Mercator projection is
// Krueger's series to sixth order in the third flattening, with the coefficients of Karney,
// "Transverse Mercator with an accuracy of a few nanometers", J. Geodesy 85 (2011); the conformal
// latitude is inverted with Newton's method as in that paper.
#include "qtwingeodesy.h"

#include <algorithm>
#include <cmath>

namespace QtTwin {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegree = kPi / 180.0;

// Reduces an angle in degrees to [-180, 180).
double normalizeDegrees(double angle) {
    double r = std::fmod(angle + 180.0, 360.0);
    if (r < 0.0)
        r += 360.0;
    return r - 180.0;
}

// tau' = tan(conformal latitude) from tau = tan(latitude), for eccentricity e.
double conformalTan(double tau, double e) {
    const double tau1 = std::hypot(1.0, tau);
    const double sigma = std::sinh(e * std::atanh(e * tau / tau1));
    return tau * std::hypot(1.0, sigma) - sigma * tau1;
}

// Inverse of conformalTan(), by Newton's method.
double geodeticTan(double taup, double e) {
    const double e2m = 1.0 - e * e;
    double tau = taup / e2m;
    for (int i = 0; i < 12; ++i) {
        const double taupa = conformalTan(tau, e);
        const double dtau =
            (taup - taupa) * (1.0 + e2m * tau * tau) / (e2m * std::hypot(1.0, tau) * std::hypot(1.0, taupa));
        tau += dtau;
        if (std::fabs(dtau) < 1e-15 * std::max(1.0, std::fabs(tau)))
            break;
    }
    return tau;
}

} // namespace

// --- Vector3d ------------------------------------------------------------------------------------

/*!
    \namespace QtTwin
    \inmodule NtTwin
    \brief The QtTwin namespace holds Nt Twin: the world model of a digital twin.

    \sa {Using Nt Twin}
*/

/*!
    \module NtTwin
    \title Nt Twin C++ Classes
    \brief The world model of a digital twin: geodetic positions, local frames, map projections,
    entities and DIS.
    \cmaketarget Nt::Twin

    Nt Twin holds the state of a digital twin: where things are on the Earth, the moving entities
    in it and, in later versions, the properties and history that mirror a real site or vehicle. It
    depends only on Nt Core and Nt Positioning, so it runs headless, in tests and on servers, and feeds a renderer
    that may run elsewhere. Qt has no such module.

    Positions are kept in double precision: as WGS84 latitude, longitude and height
    (QtTwin::GeoPosition), as Earth-centred, Earth-fixed coordinates (QtTwin::Vector3d), in local
    east-north-up frames (QtTwin::LocalFrame) and on UTM grids (QtTwin::UtmZone,
    QtTwin::TransverseMercator).

    Moving entities are described by QtTwin::EntityState, predicted between updates by
    QtTwin::DeadReckoning, written as DIS 7 Entity State PDUs by QtTwin::DisEntityStatePdu, and
    drawn with the models listed in a QtTwin::ModelCatalogue. Nt Twin Network shares them over a
    network.

    See \l{Using Nt Twin} and \l{Multiplayer Sessions} for an introduction.
*/

/*!
    \class QtTwin::Vector3d
    \inmodule NtTwin
    \brief The Vector3d struct holds a vector or point in double precision.

    QVector3D stores \c float, which resolves only about half a metre at the radius of the Earth.
    Vector3d stores \c double and is what Nt Twin uses for Earth-centred, Earth-fixed (ECEF)
    coordinates, in metres, and for positions in a QtTwin::LocalFrame.

    \differences
    Qt has no double-precision vector. Vector3d is an Nt Twin type with only the operations that
    geodesy needs: no normalization, cross product or conversion to QVector3D.
    \enddifferences

    \sa QtTwin::Ellipsoid, QtTwin::LocalFrame
*/

/*!
    \variable QtTwin::Vector3d::x
    The x coordinate. In ECEF this points from the centre of the Earth to latitude 0, longitude 0.
*/

/*!
    \variable QtTwin::Vector3d::y
    The y coordinate. In ECEF this points to latitude 0, longitude 90 degrees east.
*/

/*!
    \variable QtTwin::Vector3d::z
    The z coordinate. In ECEF this points to the North Pole.
*/

/*!
    \fn constexpr QtTwin::Vector3d::Vector3d()
    Constructs a null vector.
*/

/*!
    \fn constexpr QtTwin::Vector3d::Vector3d(double xpos, double ypos, double zpos)
    Constructs a vector with the coordinates \a xpos, \a ypos and \a zpos.
*/

/*!
    \fn double QtTwin::Vector3d::length() const
    Returns the length of the vector.
*/

/*!
    \fn constexpr double QtTwin::Vector3d::dotProduct(const Vector3d &other) const
    Returns the dot product of this vector and \a other.
*/

/*!
    \fn constexpr QtTwin::Vector3d QtTwin::Vector3d::crossProduct(const Vector3d &other) const
    Returns the cross product of this vector and \a other.
*/

/*!
    \fn QtTwin::Vector3d QtTwin::Vector3d::normalized() const
    Returns this vector scaled to length 1, or the zero vector if it has no length.
*/

/*!
    \fn constexpr QtTwin::Vector3d QtTwin::Vector3d::operator+(const Vector3d &other) const
    Returns the sum of this vector and \a other.
*/

/*!
    \fn constexpr QtTwin::Vector3d QtTwin::Vector3d::operator-(const Vector3d &other) const
    Returns this vector minus \a other.
*/

/*!
    \fn constexpr QtTwin::Vector3d QtTwin::Vector3d::operator*(double factor) const
    Returns this vector multiplied by \a factor.
*/

/*!
    \fn constexpr bool QtTwin::Vector3d::operator==(const Vector3d &other) const
    Returns \c true if every coordinate equals the one of \a other; otherwise returns \c false.
*/

/*!
    \fn constexpr bool QtTwin::Vector3d::operator!=(const Vector3d &other) const
    Returns \c true if any coordinate differs from the one of \a other; otherwise returns \c false.
*/

// --- GeoPosition ---------------------------------------------------------------------------------

/*!
    \class QtTwin::GeoPosition
    \inmodule NtTwin
    \brief The GeoPosition struct holds a geodetic latitude, longitude and ellipsoidal height.

    Latitude and longitude are in degrees, positive to the north and east. The height is in metres
    above the ellipsoid, as a GNSS receiver reports it, not above mean sea level: the two differ by
    the geoid undulation, up to about 100 metres.

    \differences
    Qt Positioning's QGeoCoordinate holds the same values with an altitude whose datum Qt leaves
    open. GeoPosition always means ellipsoidal height, and has no distance or azimuth functions;
    convert with toGeoCoordinate() and fromGeoCoordinate() to use those of QGeoCoordinate.
    \enddifferences

    \sa QtTwin::Ellipsoid::toEcef()
*/

/*!
    \variable QtTwin::GeoPosition::latitude
    The geodetic latitude in degrees, from -90 to 90.
*/

/*!
    \variable QtTwin::GeoPosition::longitude
    The longitude in degrees, from -180 to 180.
*/

/*!
    \variable QtTwin::GeoPosition::height
    The height above the ellipsoid in metres.
*/

/*!
    \fn constexpr QtTwin::GeoPosition::GeoPosition()
    Constructs a position at latitude 0, longitude 0 and height 0.
*/

/*!
    \fn constexpr QtTwin::GeoPosition::GeoPosition(double lat, double lon, double h)
    Constructs a position at latitude \a lat and longitude \a lon, in degrees, and \a h metres
    above the ellipsoid.
*/

/*!
    Returns \c true if the latitude lies in [-90, 90], the longitude in [-180, 180] and all three
    values are finite; otherwise returns \c false.
*/
bool GeoPosition::isValid() const {
    return std::isfinite(latitude) && std::isfinite(longitude) && std::isfinite(height) && latitude >= -90.0 &&
           latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

/*!
    Returns the position of \a coordinate. A 2D coordinate gets height 0; an invalid coordinate gives
    an invalid position.

    The altitude of \a coordinate is taken as a height above the ellipsoid. A QGeoCoordinate from
    a QNmeaPositionInfoSource carries the altitude above mean sea level that the receiver reports
    in GGA; add the geoid undulation to it first when the difference matters.

    \sa toGeoCoordinate()
*/
GeoPosition GeoPosition::fromGeoCoordinate(const QGeoCoordinate &coordinate) {
    if (!coordinate.isValid())
        return GeoPosition(std::nan(""), std::nan(""), std::nan(""));
    return GeoPosition(coordinate.latitude(), coordinate.longitude(),
                       coordinate.type() == QGeoCoordinate::Coordinate3D ? coordinate.altitude() : 0.0);
}

/*!
    Returns the position as a 3D QGeoCoordinate whose altitude is the ellipsoidal height, or an
    invalid coordinate if the position is invalid.

    \sa fromGeoCoordinate()
*/
QGeoCoordinate GeoPosition::toGeoCoordinate() const {
    if (!isValid())
        return QGeoCoordinate();
    return QGeoCoordinate(latitude, longitude, height);
}

// --- GridPosition --------------------------------------------------------------------------------

/*!
    \class QtTwin::GridPosition
    \inmodule NtTwin
    \brief The GridPosition struct holds an easting and northing on a map projection.

    Both values are in metres and include the projection's false easting and false northing.

    \differences
    Qt has no map projections; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::TransverseMercator
*/

/*!
    \variable QtTwin::GridPosition::easting
    The easting in metres.
*/

/*!
    \variable QtTwin::GridPosition::northing
    The northing in metres.
*/

/*!
    \fn constexpr QtTwin::GridPosition::GridPosition()
    Constructs a grid position at easting 0 and northing 0.
*/

/*!
    \fn constexpr QtTwin::GridPosition::GridPosition(double e, double n)
    Constructs a grid position at easting \a e and northing \a n.
*/

// --- Ellipsoid -----------------------------------------------------------------------------------

/*!
    \class QtTwin::Ellipsoid
    \inmodule NtTwin
    \brief The Ellipsoid class converts between geodetic positions and Earth-centred coordinates.

    An ellipsoid of revolution is given by its semi-major axis and flattening. wgs84() returns the
    one used by GPS and by every source Nt Twin reads.

    \code
    const QtTwin::Ellipsoid wgs84 = QtTwin::Ellipsoid::wgs84();
    const QtTwin::Vector3d ecef = wgs84.toEcef(QtTwin::GeoPosition(51.1789, -1.8262, 160.0));
    const QtTwin::GeoPosition back = wgs84.fromEcef(ecef);
    \endcode

    fromEcef() is exact to well below a millimetre for any point outside a few kilometres from the
    centre of the Earth.

    \differences
    Qt has no geodesy. Only biaxial ellipsoids are supported, and there is no datum shift between
    ellipsoids.
    \enddifferences

    \sa QtTwin::LocalFrame
*/

/*!
    \fn constexpr QtTwin::Ellipsoid::Ellipsoid(double semiMajorAxis, double flattening)
    Constructs an ellipsoid with the given \a semiMajorAxis in metres and \a flattening.
*/

/*!
    \fn static constexpr QtTwin::Ellipsoid QtTwin::Ellipsoid::wgs84()
    Returns the WGS84 ellipsoid: semi-major axis 6378137 m, flattening 1/298.257223563.
*/

/*!
    \fn constexpr double QtTwin::Ellipsoid::semiMajorAxis() const
    Returns the semi-major (equatorial) axis in metres.
*/

/*!
    \fn constexpr double QtTwin::Ellipsoid::flattening() const
    Returns the flattening.
*/

/*!
    \fn constexpr double QtTwin::Ellipsoid::semiMinorAxis() const
    Returns the semi-minor (polar) axis in metres.
*/

/*!
    \fn constexpr double QtTwin::Ellipsoid::eccentricitySquared() const
    Returns the square of the first eccentricity.
*/

/*!
    Returns the Earth-centred, Earth-fixed coordinates of \a position, in metres.

    \sa fromEcef()
*/
Vector3d Ellipsoid::toEcef(const GeoPosition &position) const {
    const double lat = position.latitude * kDegree;
    const double lon = position.longitude * kDegree;
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double e2 = eccentricitySquared();
    const double n = m_a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    return Vector3d((n + position.height) * cosLat * std::cos(lon), (n + position.height) * cosLat * std::sin(lon),
                    (n * (1.0 - e2) + position.height) * sinLat);
}

/*!
    Returns the geodetic position of the Earth-centred, Earth-fixed point \a ecef.

    The centre of the Earth itself has no defined latitude; it is returned as latitude 0,
    longitude 0 and a height of minus the semi-major axis.

    \sa toEcef()
*/
GeoPosition Ellipsoid::fromEcef(const Vector3d &ecef) const {
    const double a = m_a;
    const double b = semiMinorAxis();
    const double e2 = eccentricitySquared();
    const double ep2 = (a * a - b * b) / (b * b);
    const double p = std::hypot(ecef.x, ecef.y);
    const double z = ecef.z;
    if (p == 0.0 && z == 0.0)
        return GeoPosition(0.0, 0.0, -a);

    // Heikkinen's closed form for the latitude.
    const double f = 54.0 * b * b * z * z;
    const double g = p * p + (1.0 - e2) * z * z - e2 * (a * a - b * b);
    const double c = e2 * e2 * f * p * p / (g * g * g);
    const double s = std::cbrt(1.0 + c + std::sqrt(c * c + 2.0 * c));
    const double k = s + 1.0 + 1.0 / s;
    const double pp = f / (3.0 * k * k * g * g);
    const double q = std::sqrt(1.0 + 2.0 * e2 * e2 * pp);
    const double r0sq = 0.5 * a * a * (1.0 + 1.0 / q) - pp * (1.0 - e2) * z * z / (q * (1.0 + q)) - 0.5 * pp * p * p;
    const double r0 = -pp * e2 * p / (1.0 + q) + std::sqrt(std::max(0.0, r0sq));
    const double dp = p - e2 * r0;
    const double v = std::sqrt(dp * dp + (1.0 - e2) * z * z);
    const double z0 = b * b * z / (a * v);
    const double lat = std::atan2(z + ep2 * z0, p);

    // The height by projecting onto the normal, which is well conditioned everywhere.
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double h = p * cosLat + z * sinLat - a * std::sqrt(1.0 - e2 * sinLat * sinLat);
    return GeoPosition(lat / kDegree, std::atan2(ecef.y, ecef.x) / kDegree, h);
}

/*!
    Returns the outward unit normal of the ellipsoid at \a latitude and \a longitude, in degrees,
    as an Earth-centred, Earth-fixed direction.
*/
Vector3d Ellipsoid::surfaceNormal(double latitude, double longitude) const {
    const double lat = latitude * kDegree;
    const double lon = longitude * kDegree;
    return Vector3d(std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat));
}

// --- LocalFrame ----------------------------------------------------------------------------------

/*!
    \class QtTwin::LocalFrame
    \inmodule NtTwin
    \brief The LocalFrame class is an east-north-up frame tangent to the ellipsoid at a point.

    A local frame has its origin at a geodetic position, x pointing east, y north and z up along
    the ellipsoid normal, all in metres. Near the origin it is the natural frame for placing
    objects and for physics; far from it the ground curves away below the xy plane (by about
    70 m at 30 km), so a large world uses one frame per tile.

    toEcefMatrix() gives the frame as the 4x4 matrix that a 3D Tiles \c transform expects.

    \code
    const QtTwin::LocalFrame frame(QtTwin::GeoPosition(51.1789, -1.8262, 100.0));
    const QtTwin::Vector3d local = frame.toLocal(QtTwin::Ellipsoid::wgs84().toEcef(somewhere));
    \endcode

    \differences
    Qt has no geodesy. The frame is always east-north-up; there is no north-east-down variant.
    \enddifferences

    \sa QtTwin::Ellipsoid
*/

/*!
    Constructs a frame at latitude 0, longitude 0 and height 0 on the WGS84 ellipsoid.
*/
LocalFrame::LocalFrame() : LocalFrame(GeoPosition()) {}

/*!
    Constructs the east-north-up frame whose origin is \a origin on \a ellipsoid.
*/
LocalFrame::LocalFrame(const GeoPosition &origin, const Ellipsoid &ellipsoid)
    : m_origin(origin), m_originEcef(ellipsoid.toEcef(origin)) {
    const double lat = origin.latitude * kDegree;
    const double lon = origin.longitude * kDegree;
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);
    m_east = Vector3d(-sinLon, cosLon, 0.0);
    m_north = Vector3d(-sinLat * cosLon, -sinLat * sinLon, cosLat);
    m_up = Vector3d(cosLat * cosLon, cosLat * sinLon, sinLat);
}

/*!
    \fn QtTwin::GeoPosition QtTwin::LocalFrame::origin() const
    Returns the geodetic position of the frame's origin.
*/

/*!
    \fn QtTwin::Vector3d QtTwin::LocalFrame::originEcef() const
    Returns the Earth-centred, Earth-fixed coordinates of the frame's origin.
*/

/*!
    \fn QtTwin::Vector3d QtTwin::LocalFrame::east() const
    Returns the frame's x axis, pointing east, as an Earth-centred, Earth-fixed unit vector.
*/

/*!
    \fn QtTwin::Vector3d QtTwin::LocalFrame::north() const
    Returns the frame's y axis, pointing north, as an Earth-centred, Earth-fixed unit vector.
*/

/*!
    \fn QtTwin::Vector3d QtTwin::LocalFrame::up() const
    Returns the frame's z axis, the ellipsoid normal, as an Earth-centred, Earth-fixed unit vector.
*/

/*!
    Returns the Earth-centred, Earth-fixed point \a ecef in this frame.

    \sa toEcef()
*/
Vector3d LocalFrame::toLocal(const Vector3d &ecef) const {
    const Vector3d d = ecef - m_originEcef;
    return Vector3d(d.dotProduct(m_east), d.dotProduct(m_north), d.dotProduct(m_up));
}

/*!
    Returns the Earth-centred, Earth-fixed coordinates of the point \a local in this frame.

    \sa toLocal()
*/
Vector3d LocalFrame::toEcef(const Vector3d &local) const {
    return m_originEcef + m_east * local.x + m_north * local.y + m_up * local.z;
}

/*!
    Returns the matrix that maps this frame to Earth-centred, Earth-fixed coordinates, as 16 values
    in column-major order. This is the layout of a 3D Tiles \c transform and of a glTF node
    \c matrix.
*/
std::array<double, 16> LocalFrame::toEcefMatrix() const {
    return {m_east.x, m_east.y, m_east.z, 0.0, m_north.x,      m_north.y,      m_north.z,      0.0,
            m_up.x,   m_up.y,   m_up.z,   0.0, m_originEcef.x, m_originEcef.y, m_originEcef.z, 1.0};
}

// --- TransverseMercator --------------------------------------------------------------------------

/*!
    \class QtTwin::TransverseMercator
    \inmodule NtTwin
    \brief The TransverseMercator class projects geodetic positions to a transverse Mercator grid.

    The transverse Mercator projection is conformal and keeps scale errors small in a band a few
    degrees either side of its central meridian. UTM and many national grids use it. The
    implementation follows Krüger's series to sixth order, as given by Karney (2011): it is accurate
    to a few nanometres within 3900 km of the central meridian and stays usable well beyond a UTM
    zone.

    \code
    const QtTwin::UtmZone zone = QtTwin::UtmZone::containing(51.1789, -1.8262);   // 30N
    const QtTwin::GridPosition grid = zone.projection().forward(51.1789, -1.8262);
    \endcode

    \differences
    Qt has no map projections. The projection is on one ellipsoid, without datum shifts, and does
    not report scale factor or meridian convergence.
    \enddifferences

    \sa QtTwin::UtmZone
*/

/*!
    Constructs the transverse Mercator projection with the given \a centralMeridian in degrees,
    \a scaleFactor on that meridian, \a falseEasting and \a falseNorthing in metres, on
    \a ellipsoid. The defaults are those of UTM.
*/
TransverseMercator::TransverseMercator(double centralMeridian, double scaleFactor, double falseEasting,
                                       double falseNorthing, const Ellipsoid &ellipsoid)
    : m_lon0(centralMeridian), m_k0(scaleFactor), m_falseEasting(falseEasting), m_falseNorthing(falseNorthing),
      m_e(std::sqrt(ellipsoid.eccentricitySquared())), m_e2(ellipsoid.eccentricitySquared()) {
    const double f = ellipsoid.flattening();
    const double n = f / (2.0 - f);
    const double n2 = n * n;
    const double n3 = n2 * n;
    const double n4 = n3 * n;
    const double n5 = n4 * n;
    const double n6 = n5 * n;
    m_rectifyingRadius = ellipsoid.semiMajorAxis() / (1.0 + n) * (1.0 + n2 / 4.0 + n4 / 64.0 + n6 / 256.0);
    m_alpha = {
        n / 2.0 - 2.0 * n2 / 3.0 + 5.0 * n3 / 16.0 + 41.0 * n4 / 180.0 - 127.0 * n5 / 288.0 + 7891.0 * n6 / 37800.0,
        13.0 * n2 / 48.0 - 3.0 * n3 / 5.0 + 557.0 * n4 / 1440.0 + 281.0 * n5 / 630.0 - 1983433.0 * n6 / 1935360.0,
        61.0 * n3 / 240.0 - 103.0 * n4 / 140.0 + 15061.0 * n5 / 26880.0 + 167603.0 * n6 / 181440.0,
        49561.0 * n4 / 161280.0 - 179.0 * n5 / 168.0 + 6601661.0 * n6 / 7257600.0,
        34729.0 * n5 / 80640.0 - 3418889.0 * n6 / 1995840.0,
        212378941.0 * n6 / 319334400.0,
    };
    m_beta = {
        n / 2.0 - 2.0 * n2 / 3.0 + 37.0 * n3 / 96.0 - n4 / 360.0 - 81.0 * n5 / 512.0 + 96199.0 * n6 / 604800.0,
        n2 / 48.0 + n3 / 15.0 - 437.0 * n4 / 1440.0 + 46.0 * n5 / 105.0 - 1118711.0 * n6 / 3870720.0,
        17.0 * n3 / 480.0 - 37.0 * n4 / 840.0 - 209.0 * n5 / 4480.0 + 5569.0 * n6 / 90720.0,
        4397.0 * n4 / 161280.0 - 11.0 * n5 / 504.0 - 830251.0 * n6 / 7257600.0,
        4583.0 * n5 / 161280.0 - 108847.0 * n6 / 3991680.0,
        20648693.0 * n6 / 638668800.0,
    };
}

/*!
    \fn double QtTwin::TransverseMercator::centralMeridian() const
    Returns the central meridian in degrees.
*/

/*!
    \fn double QtTwin::TransverseMercator::scaleFactor() const
    Returns the scale factor on the central meridian.
*/

/*!
    \fn double QtTwin::TransverseMercator::falseEasting() const
    Returns the false easting in metres.
*/

/*!
    \fn double QtTwin::TransverseMercator::falseNorthing() const
    Returns the false northing in metres.
*/

/*!
    Returns the grid position of \a latitude and \a longitude, in degrees.

    \sa inverse()
*/
GridPosition TransverseMercator::forward(double latitude, double longitude) const {
    const double lambda = normalizeDegrees(longitude - m_lon0) * kDegree;
    const double phi = latitude * kDegree;
    const double taup = conformalTan(std::tan(phi), m_e);
    const double cosLambda = std::cos(lambda);
    const double xip = std::atan2(taup, cosLambda);
    const double etap = std::asinh(std::sin(lambda) / std::hypot(taup, cosLambda));
    double xi = xip;
    double eta = etap;
    for (std::size_t j = 0; j < m_alpha.size(); ++j) {
        const double k = 2.0 * static_cast<double>(j + 1);
        xi += m_alpha[j] * std::sin(k * xip) * std::cosh(k * etap);
        eta += m_alpha[j] * std::cos(k * xip) * std::sinh(k * etap);
    }
    const double scale = m_k0 * m_rectifyingRadius;
    return GridPosition(m_falseEasting + scale * eta, m_falseNorthing + scale * xi);
}

/*!
    Returns the latitude and longitude, in degrees, of the grid \a position. The height of the
    result is 0.

    \sa forward()
*/
GeoPosition TransverseMercator::inverse(const GridPosition &position) const {
    const double scale = m_k0 * m_rectifyingRadius;
    const double xi = (position.northing - m_falseNorthing) / scale;
    const double eta = (position.easting - m_falseEasting) / scale;
    double xip = xi;
    double etap = eta;
    for (std::size_t j = 0; j < m_beta.size(); ++j) {
        const double k = 2.0 * static_cast<double>(j + 1);
        xip -= m_beta[j] * std::sin(k * xi) * std::cosh(k * eta);
        etap -= m_beta[j] * std::cos(k * xi) * std::sinh(k * eta);
    }
    const double sinhEtap = std::sinh(etap);
    const double cosXip = std::cos(xip);
    const double taup = std::sin(xip) / std::hypot(sinhEtap, cosXip);
    const double lambda = std::atan2(sinhEtap, cosXip);
    const double phi = std::atan(geodeticTan(taup, m_e));
    return GeoPosition(phi / kDegree, normalizeDegrees(m_lon0 + lambda / kDegree), 0.0);
}

// --- UtmZone -------------------------------------------------------------------------------------

/*!
    \class QtTwin::UtmZone
    \inmodule NtTwin
    \brief The UtmZone struct identifies a Universal Transverse Mercator zone and hemisphere.

    UTM divides the Earth between 80 degrees south and 84 degrees north into 60 zones, each
    6 degrees of longitude wide, with separate false northings for the two hemispheres.
    containing() applies the exceptions for south-western Norway and Svalbard.

    \code
    const QtTwin::UtmZone zone = QtTwin::UtmZone::containing(-33.8568, 151.2153);
    zone.toString();   // "56S"
    zone.epsgCode();   // 32756
    \endcode

    \differences
    Qt has no map projections. The polar regions, which use the Universal Polar Stereographic
    projection instead, have no zone.
    \enddifferences

    \sa QtTwin::TransverseMercator
*/

/*!
    \variable QtTwin::UtmZone::number
    The zone number from 1 to 60, or 0 for no zone.
*/

/*!
    \variable QtTwin::UtmZone::north
    \c true for the northern hemisphere (false northing 0), \c false for the southern (false
    northing 10000 km).
*/

/*!
    \fn constexpr QtTwin::UtmZone::UtmZone()
    Constructs an invalid zone.
*/

/*!
    \fn constexpr QtTwin::UtmZone::UtmZone(int zoneNumber, bool northern)
    Constructs zone \a zoneNumber in the northern hemisphere if \a northern is \c true, or in the
    southern hemisphere otherwise.
*/

/*!
    Returns the zone that contains \a latitude and \a longitude, in degrees, or an invalid zone if
    the latitude is south of 80 degrees south or north of 84 degrees north.
*/
UtmZone UtmZone::containing(double latitude, double longitude) {
    if (!std::isfinite(latitude) || !std::isfinite(longitude) || latitude < -80.0 || latitude > 84.0)
        return UtmZone();
    const double lon = normalizeDegrees(longitude);
    int zone = static_cast<int>(std::floor((lon + 180.0) / 6.0)) + 1;
    zone = std::clamp(zone, 1, 60);
    if (latitude >= 56.0 && latitude < 64.0 && lon >= 3.0 && lon < 12.0)
        zone = 32;
    if (latitude >= 72.0) {
        if (lon >= 0.0 && lon < 9.0)
            zone = 31;
        else if (lon >= 9.0 && lon < 21.0)
            zone = 33;
        else if (lon >= 21.0 && lon < 33.0)
            zone = 35;
        else if (lon >= 33.0 && lon < 42.0)
            zone = 37;
    }
    return UtmZone(zone, latitude >= 0.0);
}

/*!
    \fn constexpr bool QtTwin::UtmZone::isValid() const
    Returns \c true if the zone number lies between 1 and 60; otherwise returns \c false.
*/

/*!
    \fn constexpr double QtTwin::UtmZone::centralMeridian() const
    Returns the central meridian of the zone in degrees.
*/

/*!
    \fn constexpr int QtTwin::UtmZone::epsgCode() const
    Returns the EPSG code of the zone on WGS84 (32601 to 32660 in the north, 32701 to 32760 in the
    south), or 0 for an invalid zone.
*/

/*!
    Returns the transverse Mercator projection of the zone: its central meridian, scale factor
    0.9996, false easting 500 km and a false northing of 0 in the north or 10000 km in the south.
*/
TransverseMercator UtmZone::projection() const {
    return TransverseMercator(centralMeridian(), 0.9996, 500000.0, north ? 0.0 : 10000000.0);
}

/*!
    Returns the zone as a number followed by \c N for the northern or \c S for the southern
    hemisphere, such as \c "30N", or an empty string for an invalid zone.

    The letter names the hemisphere, not an MGRS latitude band.
*/
QString UtmZone::toString() const {
    if (!isValid())
        return QString();
    return QString::number(number) + (north ? QLatin1Char('N') : QLatin1Char('S'));
}

} // namespace QtTwin
