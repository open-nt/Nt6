// MIT License - see LICENSE
#include "qtwinatmosphere.h"

#include "qtwinrotation_p.h"

#include <algorithm>
#include <cmath>

namespace QtTwin {

namespace {

constexpr double kDegrees = 3.14159265358979323846 / 180.0;
constexpr double kEarthRadius = 6356766.0; // the radius the ISA uses for geopotential height
constexpr double kGamma = 1.4;

// The layers of the International Standard Atmosphere (ISO 2533, ICAO Doc 7488) up to 86 km: base
// geopotential height in metres and temperature lapse rate in kelvin per metre.
struct Layer {
    double base;
    double lapse;
};
constexpr Layer kLayers[] = {{0.0, -0.0065},    {11000.0, 0.0},     {20000.0, 0.001},  {32000.0, 0.0028},
                             {47000.0, 0.0},    {51000.0, -0.0028}, {71000.0, -0.002}, {84852.0, 0.0}};
constexpr int kLayerCount = static_cast<int>(sizeof kLayers / sizeof kLayers[0]);

} // namespace

/*!
    \class QtTwin::AirState
    \inmodule NtTwin
    \brief The AirState struct is the state of the air at one height: temperature, pressure,
    density and the speed of sound.

    \differences
    Qt has no atmosphere model; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::Atmosphere::at()
*/

/*!
    \variable QtTwin::AirState::temperature
    The temperature in kelvin.
*/

/*!
    \variable QtTwin::AirState::pressure
    The static pressure in pascals.
*/

/*!
    \variable QtTwin::AirState::density
    The density in kilograms per cubic metre.
*/

/*!
    \variable QtTwin::AirState::speedOfSound
    The speed of sound in metres per second.
*/

/*!
    \class QtTwin::Atmosphere
    \inmodule NtTwin
    \brief The Atmosphere class is the International Standard Atmosphere, with the sea-level
    pressure and temperature of the day.

    The air thins with height, and with it the drag on anything in flight and the lift that keeps
    an aircraft up. Atmosphere gives the temperature, pressure, density and speed of sound at any
    height up to 86 km, following the layers of the International Standard Atmosphere (ISO 2533).
    The default is the standard day; pass the day's sea-level pressure (QNH) and temperature to
    shift it:

    \code
    const QtTwin::Atmosphere today(100900.0, 283.15);        // 1009 hPa, 10 degrees Celsius
    const QtTwin::AirState air = today.at(1500.0);           // density about 1.05 kg/m^3
    const double mach = speed / air.speedOfSound;
    \endcode

    The height is geometric height above mean sea level; QtTwin::GeoPosition::height is
    ellipsoidal, and the two differ by the geoid undulation, which does not matter for the
    atmosphere.

    \differences
    Qt has no atmosphere model; this class is part of Nt Twin. There is no humidity and no
    turbulence; above 86 km the air is treated as at 86 km.
    \enddifferences

    \sa QtTwin::WindModel, QtTwin::PhysicsEngine
*/

/*!
    \variable QtTwin::Atmosphere::standardPressure
    The sea-level pressure of the standard day: 101325 Pa.
*/

/*!
    \variable QtTwin::Atmosphere::standardTemperature
    The sea-level temperature of the standard day: 288.15 K (15 degrees Celsius).
*/

/*!
    \variable QtTwin::Atmosphere::standardDensity
    The sea-level density of the standard day: 1.225 kg/m^3.
*/

/*!
    \variable QtTwin::Atmosphere::gasConstant
    The specific gas constant of dry air: 287.05287 J/(kg K).
*/

/*!
    \variable QtTwin::Atmosphere::maximumHeight
    The height the model covers: 86000 m.
*/

/*!
    \fn constexpr QtTwin::Atmosphere::Atmosphere()
    Constructs the standard atmosphere.
*/

/*!
    \fn constexpr QtTwin::Atmosphere::Atmosphere(double seaLevelPressure, double seaLevelTemperature)
    Constructs the atmosphere of a day with \a seaLevelPressure in pascals and
    \a seaLevelTemperature in kelvin at sea level. The standard lapse rates apply above.
*/

/*!
    \fn constexpr double QtTwin::Atmosphere::seaLevelPressure() const
    Returns the sea-level pressure in pascals.
*/

/*!
    \fn constexpr double QtTwin::Atmosphere::seaLevelTemperature() const
    Returns the sea-level temperature in kelvin.
*/

/*!
    Returns the state of the air at \a height metres above mean sea level.
*/
AirState Atmosphere::at(double height) const {
    const double h = geopotentialHeight(std::clamp(height, -5000.0, maximumHeight));
    double t = m_temperature;
    double p = m_pressure;
    int layer = 0;
    while (layer + 1 < kLayerCount && kLayers[layer + 1].base <= h)
        ++layer;
    // Integrate the hydrostatic equation through the layers below, then within the current one.
    for (int i = 0; i <= layer; ++i) {
        const double top = i == layer ? h : kLayers[i + 1].base;
        const double dh = top - kLayers[i].base;
        const double lapse = kLayers[i].lapse;
        if (std::fabs(lapse) > 1e-12) {
            const double tTop = t + lapse * dh;
            p *= std::pow(tTop / t, -Gravity::standard / (lapse * gasConstant));
            t = tTop;
        } else {
            p *= std::exp(-Gravity::standard * dh / (gasConstant * t));
        }
    }
    AirState s;
    s.temperature = t;
    s.pressure = p;
    s.density = p / (gasConstant * t);
    s.speedOfSound = std::sqrt(kGamma * gasConstant * t);
    return s;
}

/*!
    \fn double QtTwin::Atmosphere::temperature(double height) const
    Returns the temperature in kelvin at \a height metres.
*/

/*!
    \fn double QtTwin::Atmosphere::pressure(double height) const
    Returns the pressure in pascals at \a height metres.
*/

/*!
    \fn double QtTwin::Atmosphere::density(double height) const
    Returns the density in kilograms per cubic metre at \a height metres.
*/

/*!
    \fn double QtTwin::Atmosphere::speedOfSound(double height) const
    Returns the speed of sound in metres per second at \a height metres.
*/

/*!
    Returns the height in metres at which the air has the static \a pressure in pascals: what a
    barometric altimeter set to this atmosphere's sea-level pressure reads.
*/
double Atmosphere::pressureAltitude(double pressure) const {
    if (pressure >= m_pressure)
        return 0.0;
    double low = 0.0, high = maximumHeight;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (low + high);
        if (at(mid).pressure > pressure)
            low = mid;
        else
            high = mid;
    }
    return 0.5 * (low + high);
}

/*!
    Returns the geopotential height of the geometric \a height in metres, as the standard
    atmosphere's tables are indexed.
*/
double Atmosphere::geopotentialHeight(double height) { return kEarthRadius * height / (kEarthRadius + height); }

/*!
    \class QtTwin::WindLayer
    \inmodule NtTwin
    \brief The WindLayer struct is the wind at one height.

    \differences
    Qt has no wind model; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::WindModel
*/

/*!
    \variable QtTwin::WindLayer::height
    The height in metres above mean sea level.
*/

/*!
    \variable QtTwin::WindLayer::velocity
    The velocity of the air in metres per second, as east, north and up components.
*/

/*!
    \fn constexpr QtTwin::WindLayer::WindLayer()
    Constructs calm air at sea level.
*/

/*!
    \fn constexpr QtTwin::WindLayer::WindLayer(double h, const Vector3d &eastNorthUp)
    Constructs the layer at height \a h with the air moving at \a eastNorthUp metres per second.
*/

/*!
    Returns the layer at \a height with wind of \a speed metres per second blowing from the compass
    bearing \a directionFrom in degrees, as weather reports give it: a wind from 270 blows towards
    the east.
*/
WindLayer WindLayer::fromSpeedAndDirection(double height, double speed, double directionFrom) {
    const double to = (directionFrom + 180.0) * kDegrees;
    return WindLayer(height, Vector3d(speed * std::sin(to), speed * std::cos(to), 0.0));
}

/*!
    \class QtTwin::WindModel
    \inmodule NtTwin
    \brief The WindModel class gives the wind at any height from a profile of layers.

    A WindModel holds the wind at a number of heights and interpolates between them. Below the
    lowest layer and above the highest it keeps that layer's wind. With no layers the air is calm.

    \code
    QtTwin::WindModel wind(8.0, 240.0);                                          // 8 m/s from the south-west at every height
    wind.addLayer(QtTwin::WindLayer::fromSpeedAndDirection(500.0, 15.0, 260.0)); // stronger and veering aloft
    const QtTwin::Vector3d air = wind.at(250.0);                                 // east, north, up
    \endcode

    QtTwin::PhysicsEngine takes the wind from the environment and applies the drag of the air
    moving relative to the body, so a projectile or a falling drone drifts downwind.

    \differences
    Qt has no wind model; this class is part of Nt Twin. The wind varies only with height, not with
    position or time; there are no gusts.
    \enddifferences

    \sa QtTwin::Atmosphere, QtTwin::Environment
*/

/*!
    \fn QtTwin::WindModel::WindModel()
    Constructs calm air.
*/

/*!
    Constructs a wind of \a eastNorthUp metres per second at every height.
*/
WindModel::WindModel(const Vector3d &eastNorthUp) { addLayer(WindLayer(0.0, eastNorthUp)); }

/*!
    Constructs a wind of \a speed metres per second from the bearing \a directionFrom in degrees at
    every height.
*/
WindModel::WindModel(double speed, double directionFrom) {
    addLayer(WindLayer::fromSpeedAndDirection(0.0, speed, directionFrom));
}

/*!
    \fn bool QtTwin::WindModel::isCalm() const
    Returns \c true if the model has no layers, so the wind is zero everywhere.
*/

/*!
    \fn QList<QtTwin::WindLayer> QtTwin::WindModel::layers() const
    Returns the layers, lowest first.
*/

/*!
    \fn void QtTwin::WindModel::clear()
    Removes all layers, leaving calm air.
*/

/*!
    Adds \a layer, replacing any layer at the same height.
*/
void WindModel::addLayer(const WindLayer &layer) {
    auto it = std::lower_bound(m_layers.begin(), m_layers.end(), layer,
                               [](const WindLayer &a, const WindLayer &b) { return a.height < b.height; });
    if (it != m_layers.end() && it->height == layer.height)
        *it = layer;
    else
        m_layers.insert(it, layer);
}

/*!
    Returns the wind at \a height metres as east, north and up components in metres per second.
*/
Vector3d WindModel::at(double height) const {
    if (m_layers.isEmpty())
        return Vector3d();
    if (height <= m_layers.first().height)
        return m_layers.first().velocity;
    if (height >= m_layers.last().height)
        return m_layers.last().velocity;
    for (qsizetype i = 1; i < m_layers.size(); ++i) {
        const WindLayer &a = m_layers.at(i - 1);
        const WindLayer &b = m_layers.at(i);
        if (height <= b.height) {
            const double t = (height - a.height) / (b.height - a.height);
            return a.velocity + (b.velocity - a.velocity) * t;
        }
    }
    return m_layers.last().velocity;
}

/*!
    Returns the wind at the position \a ecef as a vector in Earth-centred, Earth-fixed coordinates,
    in metres per second.
*/
Vector3d WindModel::atEcef(const Vector3d &ecef) const {
    if (m_layers.isEmpty())
        return Vector3d();
    const GeoPosition geo = Ellipsoid::wgs84().fromEcef(ecef);
    const Vector3d enu = at(geo.height);
    const Rotation::Matrix ned = Rotation::nedAxes(geo.latitude, geo.longitude);
    return Rotation::row(ned, 1) * enu.x + Rotation::row(ned, 0) * enu.y + Rotation::row(ned, 2) * -enu.z;
}

/*!
    Returns the speed in metres per second of the wind \a eastNorthUp, ignoring its vertical part.
*/
double WindModel::speed(const Vector3d &eastNorthUp) {
    return std::sqrt(eastNorthUp.x * eastNorthUp.x + eastNorthUp.y * eastNorthUp.y);
}

/*!
    Returns the compass bearing in degrees the wind \a eastNorthUp blows from, 0 to 360.
*/
double WindModel::directionFrom(const Vector3d &eastNorthUp) {
    if (eastNorthUp.x == 0.0 && eastNorthUp.y == 0.0)
        return 0.0;
    double from = std::atan2(-eastNorthUp.x, -eastNorthUp.y) / kDegrees;
    if (from < 0.0)
        from += 360.0;
    return from;
}

/*!
    \class QtTwin::Gravity
    \inmodule NtTwin
    \brief The Gravity class is the normal gravity of the WGS 84 ellipsoid.

    Gravity is not 9.81 everywhere: it is about 9.780 m/s^2 at the equator and 9.832 m/s^2 at the
    poles, and falls by about 3 mm/s^2 for every kilometre of height. magnitude() gives Somigliana's
    closed formula with the height correction of the WGS 84 standard, and at() gives it as a vector
    pointing down the ellipsoid's normal, for integrating the motion of a body in Earth-centred,
    Earth-fixed coordinates.

    \differences
    Qt has no gravity model; this class is part of Nt Twin. The model is the normal gravity of the
    ellipsoid, without local anomalies or the Earth's rotation beyond what normal gravity already
    includes.
    \enddifferences

    \sa QtTwin::PhysicsEngine
*/

/*!
    \variable QtTwin::Gravity::standard
    Standard gravity: 9.80665 m/s^2.
*/

/*!
    Returns the magnitude of normal gravity in metres per second squared at geodetic \a latitude in
    degrees and \a height metres above the ellipsoid.
*/
double Gravity::magnitude(double latitude, double height) {
    constexpr double ge = 9.7803253359;    // gravity at the equator
    constexpr double k = 0.00193185265241; // Somigliana's constant
    constexpr double e2 = 0.00669437999014;
    constexpr double a = 6378137.0;
    constexpr double f = 1.0 / 298.257223563;
    constexpr double m = 0.00344978650684; // omega^2 a^2 b / GM
    const double s2 = std::sin(latitude * kDegrees) * std::sin(latitude * kDegrees);
    const double g0 = ge * (1.0 + k * s2) / std::sqrt(1.0 - e2 * s2);
    return g0 * (1.0 - 2.0 / a * (1.0 + f + m - 2.0 * f * s2) * height + 3.0 * height * height / (a * a));
}

/*!
    Returns the acceleration of gravity at the position \a ecef as a vector in Earth-centred,
    Earth-fixed coordinates, in metres per second squared.
*/
Vector3d Gravity::at(const Vector3d &ecef) { return at(Ellipsoid::wgs84().fromEcef(ecef)); }

/*!
    \overload
    Returns the acceleration of gravity at \a position as a vector in Earth-centred, Earth-fixed
    coordinates.
*/
Vector3d Gravity::at(const GeoPosition &position) {
    const Vector3d up = Ellipsoid::wgs84().surfaceNormal(position.latitude, position.longitude);
    return up * -magnitude(position.latitude, position.height);
}

} // namespace QtTwin
