// MIT License - see LICENSE
#pragma once

// Float comparisons shared by QVector2D/3D/4D, QQuaternion and QMatrix4x4.

#include <cmath>

namespace QtPrivate {

// True if f is within 0.00001 of zero, as Qt's qFuzzyIsNull(float).
inline bool fuzzyIsNull(float f) noexcept { return std::fabs(f) <= 0.00001f; }

// True if a and b differ by a relative 0.00001, or are both near zero.
inline bool fuzzyCompare(float a, float b) noexcept {
    return fuzzyIsNull(a - b) || std::fabs(a - b) * 100000.0f <= std::fmin(std::fabs(a), std::fabs(b));
}

// The sine and cosine of angle degrees, exact for multiples of 90.
inline void exactSinCos(float angle, float *s, float *c) noexcept {
    const float a = std::fmod(angle, 360.0f);
    if (a == 0.0f) {
        *s = 0.0f;
        *c = 1.0f;
    } else if (a == 90.0f || a == -270.0f) {
        *s = 1.0f;
        *c = 0.0f;
    } else if (a == 180.0f || a == -180.0f) {
        *s = 0.0f;
        *c = -1.0f;
    } else if (a == 270.0f || a == -90.0f) {
        *s = -1.0f;
        *c = 0.0f;
    } else {
        const double radians = double(a) * 3.14159265358979323846 / 180.0;
        *s = float(std::sin(radians));
        *c = float(std::cos(radians));
    }
}

} // namespace QtPrivate
