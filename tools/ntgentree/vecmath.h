// MIT License - see LICENSE
//
// Small vector maths and a seeded random number generator for ntgentree. The generator is
// SplitMix64, so that the same seed gives the same plant on every platform and compiler.
#pragma once

#include <cmath>
#include <cstdint>

namespace gentree {

constexpr float kPi = 3.14159265358979f;
constexpr float kGoldenAngle = 2.39996323f; // radians, 137.5 degrees

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline Vec3 &operator+=(Vec3 &a, Vec3 b) { return a = a + b; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
    const float l = length(a);
    return l > 1e-12f ? a * (1.0f / l) : Vec3{0, 1, 0};
}
inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Any unit vector perpendicular to the unit vector d.
inline Vec3 perpendicular(Vec3 d) {
    const Vec3 a = std::fabs(d.y) < 0.9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    return normalize(cross(d, a));
}

// Rotates v by angle radians around the unit vector axis (Rodrigues' formula).
inline Vec3 rotate(Vec3 v, Vec3 axis, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}

class Rng {
public:
    explicit Rng(std::uint64_t seed = 1) : m_state(seed) {}

    std::uint64_t next() {
        m_state += 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    // Uniform in [0, 1).
    float uniform() { return static_cast<float>(next() >> 40) * (1.0f / 16777216.0f); }
    float range(float lo, float hi) { return lo + (hi - lo) * uniform(); }
    // Uniform in [-v, v].
    float sym(float v) { return range(-v, v); }
    // Standard normal by the Box-Muller transform.
    float normal() {
        const float u = std::fmax(uniform(), 1e-7f);
        return std::sqrt(-2.0f * std::log(u)) * std::cos(2.0f * kPi * uniform());
    }
    Vec3 unitVector() {
        const float z = range(-1.0f, 1.0f);
        const float a = range(0.0f, 2.0f * kPi);
        const float r = std::sqrt(std::fmax(0.0f, 1.0f - z * z));
        return {r * std::cos(a), z, r * std::sin(a)};
    }

private:
    std::uint64_t m_state;
};

} // namespace gentree
