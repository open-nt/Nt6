// MIT License - see LICENSE
#pragma once

#include <QtCore/qalgorithms.h>
#include <QtCore/qglobal.h>
#include <QtCore/qnumeric.h>

#include <cmath>
#include <limits>
#include <type_traits>

#ifndef M_E
#  define M_E (2.7182818284590452354)
#endif
#ifndef M_LOG2E
#  define M_LOG2E (1.4426950408889634074)
#endif
#ifndef M_LOG10E
#  define M_LOG10E (0.43429448190325182765)
#endif
#ifndef M_LN2
#  define M_LN2 (0.69314718055994530942)
#endif
#ifndef M_LN10
#  define M_LN10 (2.30258509299404568402)
#endif
#ifndef M_PI
#  define M_PI (3.14159265358979323846)
#endif
#ifndef M_PI_2
#  define M_PI_2 (1.57079632679489661923)
#endif
#ifndef M_PI_4
#  define M_PI_4 (0.78539816339744830962)
#endif
#ifndef M_1_PI
#  define M_1_PI (0.31830988618379067154)
#endif
#ifndef M_2_PI
#  define M_2_PI (0.63661977236758134308)
#endif
#ifndef M_2_SQRTPI
#  define M_2_SQRTPI (1.12837916709551257390)
#endif
#ifndef M_SQRT2
#  define M_SQRT2 (1.41421356237309504880)
#endif
#ifndef M_SQRT1_2
#  define M_SQRT1_2 (0.70710678118654752440)
#endif

// The functions of <cmath> under their Qt names. qCeil() and qFloor() return an int.
template <typename T> inline int qCeil(T v) { return static_cast<int>(std::ceil(v)); }
template <typename T> inline int qFloor(T v) { return static_cast<int>(std::floor(v)); }
template <typename T> inline auto qFabs(T v) { return std::fabs(v); }
template <typename T> inline auto qSin(T v) { return std::sin(v); }
template <typename T> inline auto qCos(T v) { return std::cos(v); }
template <typename T> inline auto qTan(T v) { return std::tan(v); }
template <typename T> inline auto qAcos(T v) { return std::acos(v); }
template <typename T> inline auto qAsin(T v) { return std::asin(v); }
template <typename T> inline auto qAtan(T v) { return std::atan(v); }
template <typename T1, typename T2> inline auto qAtan2(T1 y, T2 x) { return std::atan2(y, x); }
template <typename T> inline auto qSqrt(T v) { return std::sqrt(v); }
template <typename T> inline auto qLn(T v) { return std::log(v); }
template <typename T> inline auto qExp(T v) { return std::exp(v); }
template <typename T1, typename T2> inline auto qPow(T1 x, T2 y) { return std::pow(x, y); }
template <typename Tx, typename Ty> inline auto qHypot(Tx x, Ty y) { return std::hypot(x, y); }
template <typename Tx, typename Ty, typename Tz> inline auto qHypot(Tx x, Ty y, Tz z) { return std::hypot(x, y, z); }

// Qt reads these from a table; here they are as exact as qSin() and qCos().
inline qreal qFastSin(qreal x) { return std::sin(x); }
inline qreal qFastCos(qreal x) { return std::cos(x); }

constexpr float qDegreesToRadians(float degrees) { return degrees * static_cast<float>(M_PI / 180); }
constexpr double qDegreesToRadians(double degrees) { return degrees * (M_PI / 180); }
constexpr long double qDegreesToRadians(long double degrees) { return degrees * (static_cast<long double>(M_PI) / 180); }
template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
constexpr double qDegreesToRadians(T degrees) {
    return qDegreesToRadians(static_cast<double>(degrees));
}
constexpr float qRadiansToDegrees(float radians) { return radians * static_cast<float>(180 / M_PI); }
constexpr double qRadiansToDegrees(double radians) { return radians * (180 / M_PI); }
constexpr long double qRadiansToDegrees(long double radians) { return radians * (180 / static_cast<long double>(M_PI)); }

// The smallest power of two that is greater than v: 8 for 4, 5 and 7, and 1 for 0. The result is 0 where that
// power does not fit the type, and for a negative v. A type narrower than int is promoted first.
template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
constexpr auto qNextPowerOfTwo(T v) -> decltype(+v) {
    using Result = decltype(+v);
    using Unsigned = std::make_unsigned_t<Result>;
    if (std::is_signed_v<Result> && static_cast<std::make_signed_t<Result>>(v) < 0)
        return 0;
    const auto u = static_cast<Unsigned>(v);
    const uint zeros = qCountLeadingZeroBits(u);
    const uint width = static_cast<uint>(std::numeric_limits<Unsigned>::digits);
    // The power takes the top bit: it does not fit a signed type, and is past the end when there are no zeros.
    if (zeros == 0 || (std::is_signed_v<Result> && zeros == 1))
        return 0;
    return static_cast<Result>(Unsigned(1) << (width - zeros));
}
