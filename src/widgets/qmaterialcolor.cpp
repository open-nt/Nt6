// MIT License - see LICENSE
#include "qmaterialcolor_p.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace QtPrivate {

namespace {

constexpr double pi = 3.14159265358979323846;
using Vector = std::array<double, 3>;
using Matrix = std::array<Vector, 3>;

constexpr Matrix srgbToXyz{
    {{0.4124564, 0.3575761, 0.1804375}, {0.2126729, 0.7151522, 0.0721750}, {0.0193339, 0.1191920, 0.9503041}}};
constexpr Matrix xyzToSrgb{
    {{3.2404542, -1.5371385, -0.4985314}, {-0.9692660, 1.8760108, 0.0415560}, {0.0556434, -0.2040259, 1.0572252}}};
// The CAM16 cone response matrix and its inverse.
constexpr Matrix m16{
    {{0.401288, 0.650173, -0.051461}, {-0.250268, 1.204414, 0.045854}, {-0.002079, 0.048952, 0.953127}}};
constexpr Matrix m16Inverse{{{1.86206786, -1.01125463, 0.14918677},
                             {0.38752654, 0.62144744, -0.00897398},
                             {-0.01584150, -0.03412294, 1.04996444}}};
constexpr Vector whiteD65{95.047, 100.0, 108.883};

Vector multiply(const Matrix &m, const Vector &v) {
    return {m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2], m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
            m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]};
}

double linearFromSrgb(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }

double srgbFromLinear(double c) { return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055; }

// CIELAB lightness and relative luminance, with Y from 0 to 100.
double lstarFromY(double y) {
    const double t = y / 100.0;
    return t > 216.0 / 24389.0 ? 116.0 * std::cbrt(t) - 16.0 : t * 24389.0 / 27.0;
}

double yFromLstar(double lstar) {
    const double f = (lstar + 16.0) / 116.0;
    return lstar > 8.0 ? 100.0 * f * f * f : 100.0 * lstar * 27.0 / 24389.0;
}

// What CAM16 derives from the viewing conditions once.
struct Conditions {
    double n, aw, nbb, ncb, c = 0.69, nc = 1.0, z, fl;
    Vector discount;

    Conditions() {
        const double adaptingLuminance = 200.0 / pi * yFromLstar(50.0) / 100.0;
        n = yFromLstar(50.0) / whiteD65[1];
        const double degree = std::clamp(1.0 - (1.0 / 3.6) * std::exp((-adaptingLuminance - 42.0) / 92.0), 0.0, 1.0);
        const Vector white = multiply(m16, whiteD65);
        for (std::size_t i = 0; i < 3; ++i)
            discount[i] = degree * (100.0 / white[i]) + 1.0 - degree;
        const double k = 1.0 / (5.0 * adaptingLuminance + 1.0);
        const double k4 = k * k * k * k;
        fl = 0.2 * k4 * (5.0 * adaptingLuminance) + 0.1 * (1.0 - k4) * (1.0 - k4) * std::cbrt(5.0 * adaptingLuminance);
        z = 1.48 + std::sqrt(n);
        nbb = ncb = 0.725 * std::pow(1.0 / n, 0.2);
        Vector response{};
        for (std::size_t i = 0; i < 3; ++i)
            response[i] = adapt(discount[i] * white[i]);
        aw = (2.0 * response[0] + response[1] + 0.05 * response[2]) * nbb;
    }

    // The post-adaptation non-linear response of a cone, and its inverse.
    double adapt(double component) const {
        const double f = std::pow(fl * std::abs(component) / 100.0, 0.42);
        return std::copysign(400.0 * f / (f + 27.13), component);
    }
    double unadapt(double response) const {
        const double base = std::max(0.0, 27.13 * std::abs(response) / (400.0 - std::abs(response)));
        return std::copysign(100.0 / fl * std::pow(base, 1.0 / 0.42), response);
    }
};

const Conditions &conditions() {
    static const Conditions c;
    return c;
}

// CIE XYZ of the colour with CAM16 lightness j, chroma c and hue h.
Vector xyzFromCam(double j, double c, double h) {
    const Conditions &vc = conditions();
    const double alpha = (c == 0.0 || j == 0.0) ? 0.0 : c / std::sqrt(j / 100.0);
    const double t = std::pow(alpha / std::pow(1.64 - std::pow(0.29, vc.n), 0.73), 1.0 / 0.9);
    const double radians = h * pi / 180.0;
    const double eccentricity = 0.25 * (std::cos(radians + 2.0) + 3.8);
    const double achromatic = vc.aw * std::pow(j / 100.0, 1.0 / vc.c / vc.z);
    const double p1 = eccentricity * (50000.0 / 13.0) * vc.nc * vc.ncb;
    const double p2 = achromatic / vc.nbb;
    const double sine = std::sin(radians), cosine = std::cos(radians);
    const double gamma = 23.0 * (p2 + 0.305) * t / (23.0 * p1 + 11.0 * t * cosine + 108.0 * t * sine);
    const double a = gamma * cosine, b = gamma * sine;
    const Vector response{(460.0 * p2 + 451.0 * a + 288.0 * b) / 1403.0, (460.0 * p2 - 891.0 * a - 261.0 * b) / 1403.0,
                          (460.0 * p2 - 220.0 * a - 6300.0 * b) / 1403.0};
    Vector cone{};
    for (std::size_t i = 0; i < 3; ++i)
        cone[i] = vc.unadapt(response[i]) / vc.discount[i];
    return multiply(m16Inverse, cone);
}

// The linear sRGB colour of a hue, chroma and tone, or nothing if sRGB cannot show it.
std::optional<Vector> solve(double hue, double chroma, double tone) {
    // At a given chroma and hue, the luminance rises with the CAM16 lightness: find the lightness of the tone.
    const double y = yFromLstar(tone);
    double low = 0.0, high = 100.0;
    Vector xyz{};
    for (int i = 0; i < 50; ++i) {
        const double j = (low + high) / 2.0;
        xyz = xyzFromCam(j, chroma, hue);
        if (xyz[1] < y)
            low = j;
        else
            high = j;
    }
    const Vector linear = multiply(xyzToSrgb, {xyz[0] / 100.0, xyz[1] / 100.0, xyz[2] / 100.0});
    constexpr double tolerance = 0.0005;
    const bool inGamut =
        std::all_of(linear.begin(), linear.end(), [](double v) { return v >= -tolerance && v <= 1.0 + tolerance; }) &&
        std::abs(xyz[1] - y) < 0.2;
    if (!inGamut)
        return std::nullopt;
    return linear;
}

QColor colorFromLinear(const Vector &linear) {
    int channel[3];
    for (std::size_t i = 0; i < 3; ++i)
        channel[i] = static_cast<int>(std::lround(255.0 * srgbFromLinear(std::clamp(linear[i], 0.0, 1.0))));
    return QColor(channel[0], channel[1], channel[2]);
}

} // namespace

Hct Hct::fromColor(const QColor &color) {
    const Conditions &vc = conditions();
    const Vector linear{linearFromSrgb(color.red() / 255.0), linearFromSrgb(color.green() / 255.0),
                        linearFromSrgb(color.blue() / 255.0)};
    Vector xyz = multiply(srgbToXyz, linear);
    for (double &v : xyz)
        v *= 100.0;
    const Vector cone = multiply(m16, xyz);
    Vector response{};
    for (std::size_t i = 0; i < 3; ++i)
        response[i] = vc.adapt(vc.discount[i] * cone[i]);
    const double ra = response[0], ga = response[1], ba = response[2];
    const double a = ra - 12.0 * ga / 11.0 + ba / 11.0;
    const double b = (ra + ga - 2.0 * ba) / 9.0;
    double hue = std::atan2(b, a) * 180.0 / pi;
    if (hue < 0)
        hue += 360.0;
    const double u = (20.0 * ra + 20.0 * ga + 21.0 * ba) / 20.0;
    const double p2 = (40.0 * ra + 20.0 * ga + ba) / 20.0;
    const double j = 100.0 * std::pow(p2 * vc.nbb / vc.aw, vc.c * vc.z);
    const double huePrime = hue < 20.14 ? hue + 360.0 : hue;
    const double eccentricity = 0.25 * (std::cos(huePrime * pi / 180.0 + 2.0) + 3.8);
    const double p1 = 50000.0 / 13.0 * eccentricity * vc.nc * vc.ncb;
    const double t = p1 * std::hypot(a, b) / (u + 0.305);
    const double alpha = std::pow(t, 0.9) * std::pow(1.64 - std::pow(0.29, vc.n), 0.73);
    return Hct{hue, alpha * std::sqrt(j / 100.0), lstarFromY(xyz[1])};
}

QColor Hct::toColor(double hue, double chroma, double tone) {
    hue = std::fmod(hue, 360.0);
    if (hue < 0)
        hue += 360.0;
    tone = std::clamp(tone, 0.0, 100.0);
    const double grey = yFromLstar(tone) / 100.0;
    if (chroma < 0.5 || tone < 0.01 || tone > 99.99)
        return colorFromLinear({grey, grey, grey});
    if (const std::optional<Vector> rgb = solve(hue, chroma, tone))
        return colorFromLinear(*rgb);
    // sRGB does not reach that chroma at this hue and tone: take the most that it does.
    Vector best{grey, grey, grey};
    double low = 0.0, high = chroma;
    for (int i = 0; i < 24; ++i) {
        const double middle = (low + high) / 2.0;
        if (const std::optional<Vector> rgb = solve(hue, middle, tone)) {
            best = *rgb;
            low = middle;
        } else {
            high = middle;
        }
    }
    return colorFromLinear(best);
}

} // namespace QtPrivate
