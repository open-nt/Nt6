// MIT License - see LICENSE
//
// Rotations for Nt Twin entities: direction cosine matrices between ECEF, local north-east-down and an
// entity's body frame (x forward, y right, z down), the DIS Euler angles, and unit quaternions for
// blending. Not part of the public API.
#pragma once

#include "qtwinentity.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace QtTwin {
namespace Rotation {

// A 3x3 matrix, row-major. As a frame matrix, its rows are the axes of one frame written in another.
using Matrix = std::array<double, 9>;

inline Matrix multiply(const Matrix &a, const Matrix &b) {
    Matrix r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r[static_cast<std::size_t>(i * 3 + j)] =
                a[static_cast<std::size_t>(i * 3)] * b[static_cast<std::size_t>(j)] +
                a[static_cast<std::size_t>(i * 3 + 1)] * b[static_cast<std::size_t>(3 + j)] +
                a[static_cast<std::size_t>(i * 3 + 2)] * b[static_cast<std::size_t>(6 + j)];
    return r;
}

inline Matrix transposed(const Matrix &m) { return {m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]}; }

inline Vector3d row(const Matrix &m, int i) {
    const auto k = static_cast<std::size_t>(i * 3);
    return Vector3d(m[k], m[k + 1], m[k + 2]);
}

// The aerospace z-y-x rotation: yaw psi about z, then pitch theta about the new y, then roll phi about the
// new x. The rows of the result are the rotated frame's axes in the original frame.
inline Matrix fromEuler(double psi, double theta, double phi) {
    const double cps = std::cos(psi), sps = std::sin(psi);
    const double cth = std::cos(theta), sth = std::sin(theta);
    const double cph = std::cos(phi), sph = std::sin(phi);
    return {cth * cps,
            cth * sps,
            -sth,
            sph * sth * cps - cph * sps,
            sph * sth * sps + cph * cps,
            sph * cth,
            cph * sth * cps + sph * sps,
            cph * sth * sps - sph * cps,
            cph * cth};
}

// The inverse of fromEuler(). At pitch +-90 degrees yaw and roll are not separable; roll is then 0.
inline void toEuler(const Matrix &m, double *psi, double *theta, double *phi) {
    *theta = std::asin(std::clamp(-m[2], -1.0, 1.0));
    if (std::fabs(m[2]) > 1.0 - 1e-12) {
        *phi = 0.0;
        *psi = std::atan2(-m[3], m[4]);
    } else {
        *psi = std::atan2(m[1], m[0]);
        *phi = std::atan2(m[5], m[8]);
    }
}

// The north, east and down axes at a geodetic latitude and longitude (degrees), in ECEF.
inline Matrix nedAxes(double latitude, double longitude) {
    constexpr double kRad = 3.14159265358979323846 / 180.0;
    const double sl = std::sin(latitude * kRad), cl = std::cos(latitude * kRad);
    const double so = std::sin(longitude * kRad), co = std::cos(longitude * kRad);
    return {-sl * co, -sl * so, cl, -so, co, 0.0, -cl * co, -cl * so, -sl};
}

// The rotation by angle |axis| (radians) about axis, as an active rotation matrix (Rodrigues).
inline Matrix axisAngle(const Vector3d &axis) {
    const double angle = axis.length();
    if (angle < 1e-15)
        return {1, 0, 0, 0, 1, 0, 0, 0, 1};
    const double x = axis.x / angle, y = axis.y / angle, z = axis.z / angle;
    const double c = std::cos(angle), s = std::sin(angle), t = 1.0 - c;
    return {t * x * x + c,     t * x * y - s * z, t * x * z + s * y, t * x * y + s * z, t * y * y + c,
            t * y * z - s * x, t * x * z - s * y, t * y * z + s * x, t * z * z + c};
}

// The angle in radians of the rotation between two frame matrices.
inline double angleBetween(const Matrix &a, const Matrix &b) {
    const Matrix d = multiply(a, transposed(b));
    return std::acos(std::clamp((d[0] + d[4] + d[8] - 1.0) / 2.0, -1.0, 1.0));
}

inline Matrix bodyAxes(const EulerAngles &o) { return fromEuler(o.psi, o.theta, o.phi); }

inline EulerAngles eulerFromAxes(const Matrix &m) {
    EulerAngles e;
    toEuler(m, &e.psi, &e.theta, &e.phi);
    return e;
}

// A unit quaternion (w, x, y, z), for blending orientations.
struct Quaternion {
    double w = 1.0, x = 0.0, y = 0.0, z = 0.0;
};

inline Quaternion quaternionFromMatrix(const Matrix &m) {
    Quaternion q;
    const double trace = m[0] + m[4] + m[8];
    if (trace > 0.0) {
        const double s = std::sqrt(trace + 1.0) * 2.0;
        q = {0.25 * s, (m[5] - m[7]) / s, (m[6] - m[2]) / s, (m[1] - m[3]) / s};
    } else if (m[0] > m[4] && m[0] > m[8]) {
        const double s = std::sqrt(1.0 + m[0] - m[4] - m[8]) * 2.0;
        q = {(m[5] - m[7]) / s, 0.25 * s, (m[3] + m[1]) / s, (m[6] + m[2]) / s};
    } else if (m[4] > m[8]) {
        const double s = std::sqrt(1.0 + m[4] - m[0] - m[8]) * 2.0;
        q = {(m[6] - m[2]) / s, (m[3] + m[1]) / s, 0.25 * s, (m[7] + m[5]) / s};
    } else {
        const double s = std::sqrt(1.0 + m[8] - m[0] - m[4]) * 2.0;
        q = {(m[1] - m[3]) / s, (m[6] + m[2]) / s, (m[7] + m[5]) / s, 0.25 * s};
    }
    return q;
}

inline Matrix matrixFromQuaternion(const Quaternion &q) {
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    const double w = q.w / n, x = q.x / n, y = q.y / n, z = q.z / n;
    return {1 - 2 * (y * y + z * z), 2 * (x * y + w * z),     2 * (x * z - w * y),
            2 * (x * y - w * z),     1 - 2 * (x * x + z * z), 2 * (y * z + w * x),
            2 * (x * z + w * y),     2 * (y * z - w * x),     1 - 2 * (x * x + y * y)};
}

inline Quaternion slerp(Quaternion a, const Quaternion &b, double t) {
    double dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (dot < 0.0) {
        a = {-a.w, -a.x, -a.y, -a.z};
        dot = -dot;
    }
    if (dot > 0.9995) {
        return {a.w + (b.w - a.w) * t, a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }
    const double theta = std::acos(dot);
    const double sa = std::sin((1.0 - t) * theta) / std::sin(theta);
    const double sb = std::sin(t * theta) / std::sin(theta);
    return {a.w * sa + b.w * sb, a.x * sa + b.x * sb, a.y * sa + b.y * sb, a.z * sa + b.z * sb};
}

} // namespace Rotation
} // namespace QtTwin
