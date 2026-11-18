// MIT License - see LICENSE
//
// Helpers shared by the Nt Positioning sources. Not part of the public API.
#pragma once

#include <cmath>

namespace QtPositioningPrivate {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthMeanRadius = 6371007.2; // metres, as Qt Positioning

inline double radians(double degrees) { return degrees * kPi / 180.0; }
inline double degrees(double radians) { return radians * 180.0 / kPi; }

// Reduces a longitude to [-180, 180], keeping 180 as 180.
inline double normalizeLongitude(double longitude) {
    if (longitude >= -180.0 && longitude <= 180.0)
        return longitude;
    double r = std::fmod(longitude + 180.0, 360.0);
    if (r < 0.0)
        r += 360.0;
    return r - 180.0;
}

// The eastward angle from a to b in [0, 360).
inline double eastward(double a, double b) {
    double d = std::fmod(b - a, 360.0);
    if (d < 0.0)
        d += 360.0;
    return d;
}

} // namespace QtPositioningPrivate
