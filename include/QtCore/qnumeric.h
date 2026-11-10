// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

// Classification of floating-point numbers, and the special values.
inline bool qIsInf(double d) { return std::isinf(d); }
inline bool qIsNaN(double d) { return std::isnan(d); }
inline bool qIsFinite(double d) { return std::isfinite(d); }
inline int qFpClassify(double val) { return std::fpclassify(val); }
inline bool qIsInf(float f) { return std::isinf(f); }
inline bool qIsNaN(float f) { return std::isnan(f); }
inline bool qIsFinite(float f) { return std::isfinite(f); }
inline int qFpClassify(float val) { return std::fpclassify(val); }
constexpr double qInf() noexcept { return std::numeric_limits<double>::infinity(); }
constexpr double qQNaN() noexcept { return std::numeric_limits<double>::quiet_NaN(); }
constexpr double qSNaN() noexcept { return std::numeric_limits<double>::signaling_NaN(); }

// The number of representable values between two finite numbers; 0 if they are equal. The numbers of IEEE 754
// are ordered as their bits are, apart from the sign.
inline quint32 qFloatDistance(float a, float b) {
    static_assert(sizeof(float) == sizeof(quint32) && std::numeric_limits<float>::is_iec559);
    quint32 ua = 0, ub = 0;
    std::memcpy(&ua, &a, sizeof ua);
    std::memcpy(&ub, &b, sizeof ub);
    const quint32 sign = quint32(1) << 31;
    const quint32 ma = ua & ~sign, mb = ub & ~sign;
    if ((ua ^ ub) & sign)
        return ma + mb;
    return ma > mb ? ma - mb : mb - ma;
}
inline quint64 qFloatDistance(double a, double b) {
    static_assert(sizeof(double) == sizeof(quint64) && std::numeric_limits<double>::is_iec559);
    quint64 ua = 0, ub = 0;
    std::memcpy(&ua, &a, sizeof ua);
    std::memcpy(&ub, &b, sizeof ub);
    const quint64 sign = quint64(1) << 63;
    const quint64 ma = ua & ~sign, mb = ub & ~sign;
    if ((ua ^ ub) & sign)
        return ma + mb;
    return ma > mb ? ma - mb : mb - ma;
}

namespace QtPrivate {

// Arithmetic that reports overflow, written without compiler support. The result wraps around, as it does with
// the builtins.
template <typename T> constexpr bool addOverflowGeneric(T v1, T v2, T *r) {
    using U = std::make_unsigned_t<T>;
    *r = static_cast<T>(static_cast<U>(static_cast<U>(v1) + static_cast<U>(v2)));
    if constexpr (std::is_unsigned_v<T>) {
        return *r < v1;
    } else {
        // The signs of the operands agree and that of the result does not.
        return (v1 < 0) == (v2 < 0) && (*r < 0) != (v1 < 0);
    }
}
template <typename T> constexpr bool subOverflowGeneric(T v1, T v2, T *r) {
    using U = std::make_unsigned_t<T>;
    *r = static_cast<T>(static_cast<U>(static_cast<U>(v1) - static_cast<U>(v2)));
    if constexpr (std::is_unsigned_v<T>) {
        return v1 < v2;
    } else {
        // The signs of the operands differ and the result has not the sign of the first.
        return (v1 < 0) != (v2 < 0) && (*r < 0) != (v1 < 0);
    }
}
template <typename T> constexpr bool mulOverflowGeneric(T v1, T v2, T *r) {
    using U = std::make_unsigned_t<T>;
    // Operands narrower than int would be promoted to int, which is signed, before they are multiplied.
    using Wide = std::common_type_t<U, unsigned>;
    *r = static_cast<T>(static_cast<U>(static_cast<Wide>(static_cast<U>(v1)) * static_cast<Wide>(static_cast<U>(v2))));
    if (v1 == 0 || v2 == 0)
        return false;
    const T highest = std::numeric_limits<T>::max();
    if constexpr (std::is_unsigned_v<T>) {
        return v1 > highest / v2;
    } else {
        const T lowest = std::numeric_limits<T>::min();
        if (v1 > 0)
            return v2 > 0 ? v1 > highest / v2 : v2 < lowest / v1;
        return v2 > 0 ? v1 < lowest / v2 : v2 < highest / v1;
    }
}

} // namespace QtPrivate

// Each stores the result in *r and returns true if it does not fit the type.
template <typename T> inline bool qAddOverflow(T v1, T v2, T *r) {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>, "qAddOverflow() takes integers");
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_add_overflow(v1, v2, r);
#else
    return QtPrivate::addOverflowGeneric(v1, v2, r);
#endif
}
template <typename T> inline bool qSubOverflow(T v1, T v2, T *r) {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>, "qSubOverflow() takes integers");
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_sub_overflow(v1, v2, r);
#else
    return QtPrivate::subOverflowGeneric(v1, v2, r);
#endif
}
template <typename T> inline bool qMulOverflow(T v1, T v2, T *r) {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>, "qMulOverflow() takes integers");
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_mul_overflow(v1, v2, r);
#else
    return QtPrivate::mulOverflowGeneric(v1, v2, r);
#endif
}
