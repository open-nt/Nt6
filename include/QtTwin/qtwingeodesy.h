// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtPositioning/qgeocoordinate.h>

#include <array>
#include <cmath>

namespace QtTwin {

struct Vector3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vector3d() = default;
    constexpr Vector3d(double xpos, double ypos, double zpos) : x(xpos), y(ypos), z(zpos) {}

    double length() const { return std::sqrt(x * x + y * y + z * z); }
    constexpr double dotProduct(const Vector3d &other) const { return x * other.x + y * other.y + z * other.z; }

    constexpr Vector3d operator+(const Vector3d &other) const {
        return Vector3d(x + other.x, y + other.y, z + other.z);
    }
    constexpr Vector3d operator-(const Vector3d &other) const {
        return Vector3d(x - other.x, y - other.y, z - other.z);
    }
    constexpr Vector3d operator*(double factor) const { return Vector3d(x * factor, y * factor, z * factor); }
    constexpr bool operator==(const Vector3d &other) const { return x == other.x && y == other.y && z == other.z; }
    constexpr bool operator!=(const Vector3d &other) const { return !(*this == other); }
};

struct GeoPosition {
    double latitude = 0.0;
    double longitude = 0.0;
    double height = 0.0;

    constexpr GeoPosition() = default;
    constexpr GeoPosition(double lat, double lon, double h = 0.0) : latitude(lat), longitude(lon), height(h) {}

    bool isValid() const;

    static GeoPosition fromGeoCoordinate(const QGeoCoordinate &coordinate);
    QGeoCoordinate toGeoCoordinate() const;
};

struct GridPosition {
    double easting = 0.0;
    double northing = 0.0;

    constexpr GridPosition() = default;
    constexpr GridPosition(double e, double n) : easting(e), northing(n) {}
};

class Ellipsoid {
public:
    constexpr Ellipsoid(double semiMajorAxis, double flattening) : m_a(semiMajorAxis), m_f(flattening) {}

    static constexpr Ellipsoid wgs84() { return Ellipsoid(6378137.0, 1.0 / 298.257223563); }

    constexpr double semiMajorAxis() const { return m_a; }
    constexpr double flattening() const { return m_f; }
    constexpr double semiMinorAxis() const { return m_a * (1.0 - m_f); }
    constexpr double eccentricitySquared() const { return m_f * (2.0 - m_f); }

    Vector3d toEcef(const GeoPosition &position) const;
    GeoPosition fromEcef(const Vector3d &ecef) const;
    Vector3d surfaceNormal(double latitude, double longitude) const;

private:
    double m_a;
    double m_f;
};

class LocalFrame {
public:
    LocalFrame();
    explicit LocalFrame(const GeoPosition &origin, const Ellipsoid &ellipsoid = Ellipsoid::wgs84());

    GeoPosition origin() const { return m_origin; }
    Vector3d originEcef() const { return m_originEcef; }
    Vector3d east() const { return m_east; }
    Vector3d north() const { return m_north; }
    Vector3d up() const { return m_up; }

    Vector3d toLocal(const Vector3d &ecef) const;
    Vector3d toEcef(const Vector3d &local) const;
    std::array<double, 16> toEcefMatrix() const;

private:
    GeoPosition m_origin;
    Vector3d m_originEcef;
    Vector3d m_east;
    Vector3d m_north;
    Vector3d m_up;
};

class TransverseMercator {
public:
    explicit TransverseMercator(double centralMeridian, double scaleFactor = 0.9996, double falseEasting = 500000.0,
                                double falseNorthing = 0.0, const Ellipsoid &ellipsoid = Ellipsoid::wgs84());

    double centralMeridian() const { return m_lon0; }
    double scaleFactor() const { return m_k0; }
    double falseEasting() const { return m_falseEasting; }
    double falseNorthing() const { return m_falseNorthing; }

    GridPosition forward(double latitude, double longitude) const;
    GeoPosition inverse(const GridPosition &position) const;

private:
    double m_lon0;
    double m_k0;
    double m_falseEasting;
    double m_falseNorthing;
    double m_e;
    double m_e2;
    double m_rectifyingRadius;
    std::array<double, 6> m_alpha;
    std::array<double, 6> m_beta;
};

struct UtmZone {
    int number = 0;
    bool north = true;

    constexpr UtmZone() = default;
    constexpr UtmZone(int zoneNumber, bool northern) : number(zoneNumber), north(northern) {}

    static UtmZone containing(double latitude, double longitude);

    constexpr bool isValid() const { return number >= 1 && number <= 60; }
    constexpr double centralMeridian() const { return -183.0 + 6.0 * number; }
    constexpr int epsgCode() const { return isValid() ? (north ? 32600 : 32700) + number : 0; }
    TransverseMercator projection() const;
    QString toString() const;
};

} // namespace QtTwin
