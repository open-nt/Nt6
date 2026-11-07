// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cstring>
#include <limits>
#include <type_traits>

namespace QtPrivate {

inline float halfBitsToFloat(quint16 h) noexcept {
    quint32 sign = static_cast<quint32>(h & 0x8000u) << 16;
    quint32 e = (h >> 10) & 0x1f;
    quint32 m = h & 0x3ff;
    quint32 out = 0;
    if (e == 0) {
        if (m == 0) {
            out = sign;
        } else { // a subnormal half is a normal float
            int shift = 0;
            while (!(m & 0x400)) {
                m <<= 1;
                ++shift;
            }
            m &= 0x3ff;
            out = sign | (static_cast<quint32>(113 - shift) << 23) | (m << 13);
        }
    } else if (e == 0x1f) {
        out = sign | 0x7f800000u | (m << 13);
    } else {
        out = sign | ((e + 112) << 23) | (m << 13);
    }
    float f = 0;
    static_assert(sizeof f == sizeof out);
    std::memcpy(&f, &out, sizeof f);
    return f;
}

// Rounds to nearest, ties to even.
inline quint16 floatToHalfBits(float f) noexcept {
    quint32 x;
    std::memcpy(&x, &f, sizeof x);
    const quint32 sign = (x >> 16) & 0x8000u;
    const int exp = static_cast<int>((x >> 23) & 0xff);
    quint32 mant = x & 0x7fffffu;
    if (exp == 0xff) // infinity or NaN; a NaN stays a quiet NaN
        return static_cast<quint16>(sign | 0x7c00u | (mant ? (0x200u | (mant >> 13)) : 0u));
    const int e = exp - 127 + 15;
    if (e >= 0x1f)
        return static_cast<quint16>(sign | 0x7c00u);
    if (e <= 0) {
        if (e < -10)
            return static_cast<quint16>(sign);
        mant |= 0x800000u;
        const int shift = 14 - e;
        quint32 half = mant >> shift;
        const quint32 rest = mant & ((1u << shift) - 1u);
        const quint32 halfway = 1u << (shift - 1);
        if (rest > halfway || (rest == halfway && (half & 1u)))
            ++half;
        return static_cast<quint16>(sign | half);
    }
    quint32 half = (static_cast<quint32>(e) << 10) | (mant >> 13);
    const quint32 rest = mant & 0x1fffu;
    if (rest > 0x1000u || (rest == 0x1000u && (half & 1u)))
        ++half; // a carry into the exponent is correct, and reaches infinity at the top
    return static_cast<quint16>(sign | half);
}

} // namespace QtPrivate

/*!
    \class qfloat16
    \brief The qfloat16 class provides 16-bit floating point support.

    The type is an IEEE 754 binary16 number: 1 sign bit, 5 exponent bits and
    10 mantissa bits, which gives about three decimal digits and a range of
    about 6e-8 to 65504. It is meant for storing values compactly, for example
    in textures and tensors; arithmetic is done by converting to \c float and
    rounding the result back.

    \code
    qfloat16 h(1.5f);
    float f = h * 2.0f;     // 3.0f
    \endcode

    Values that are too large become infinity, and values that are too small
    become zero or a subnormal number. Conversions round to nearest, with ties
    going to the even value.

    \differences
    The conversions are always done in software; there is no use of hardware
    half-precision instructions. \c qFloatToFloat16() and
    \c qFloatFromFloat16() convert arrays one element at a time.
    \enddifferences
*/
class qfloat16 {
public:
    /*!
        Constructs a qfloat16 with the value zero.
    */
    constexpr qfloat16() noexcept : m_bits(0) {}
    /*!
        Constructs a qfloat16 from \a value, rounded to the nearest half-precision number.
    */
    qfloat16(float value) noexcept : m_bits(QtPrivate::floatToHalfBits(value)) {}
    /*!
        \overload

        Constructs a qfloat16 from the integer or \c double \a value, converted
        through \c float.
    */
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, float>>>
    qfloat16(T value) noexcept : qfloat16(static_cast<float>(value)) {}

    /*!
        Returns the value as a \c float, which is exact.
    */
    operator float() const noexcept { return QtPrivate::halfBitsToFloat(m_bits); }

    /*!
        Returns \c true if the value is positive or negative infinity.

        \sa isNaN(), isFinite()
    */
    bool isInf() const noexcept { return (m_bits & 0x7fff) == 0x7c00; }
    /*!
        Returns \c true if the value is not a number.

        \sa isInf(), isFinite()
    */
    bool isNaN() const noexcept { return (m_bits & 0x7fff) > 0x7c00; }
    /*!
        Returns \c true if the value is neither infinity nor not a number.

        \sa isInf(), isNaN()
    */
    bool isFinite() const noexcept { return (m_bits & 0x7c00) != 0x7c00; }
    /*!
        Returns \c true if the value is normal: not zero, subnormal, infinite or a NaN.
    */
    bool isNormal() const noexcept { return (m_bits & 0x7c00) != 0 && isFinite(); }
    /*!
        Returns the category of the value, one of \c FP_NAN, \c FP_INFINITE,
        \c FP_ZERO, \c FP_SUBNORMAL and \c FP_NORMAL.
    */
    int fpClassify() const noexcept {
        const unsigned exponent = (m_bits >> 10) & 0x1f, mantissa = m_bits & 0x3ffu;
        if (exponent == 0x1f)
            return mantissa ? FP_NAN : FP_INFINITE;
        if (exponent == 0)
            return mantissa ? FP_SUBNORMAL : FP_ZERO;
        return FP_NORMAL;
    }
    /*!
        Returns this value with the sign of \a sign.
    */
    qfloat16 copySign(qfloat16 sign) const noexcept {
        qfloat16 r;
        r.m_bits = static_cast<quint16>((m_bits & 0x7fff) | (sign.m_bits & 0x8000));
        return r;
    }

    /*!
        Adds \a other to this value and returns this object.
    */
    qfloat16 &operator+=(qfloat16 other) noexcept { return *this = qfloat16(float(*this) + float(other)); }
    /*!
        Subtracts \a other from this value and returns this object.
    */
    qfloat16 &operator-=(qfloat16 other) noexcept { return *this = qfloat16(float(*this) - float(other)); }
    /*!
        Multiplies this value by \a other and returns this object.
    */
    qfloat16 &operator*=(qfloat16 other) noexcept { return *this = qfloat16(float(*this) * float(other)); }
    /*!
        Divides this value by \a other and returns this object.
    */
    qfloat16 &operator/=(qfloat16 other) noexcept { return *this = qfloat16(float(*this) / float(other)); }
    /*!
        Returns this value with its sign flipped.
    */
    qfloat16 operator-() const noexcept {
        qfloat16 r;
        r.m_bits = static_cast<quint16>(m_bits ^ 0x8000);
        return r;
    }

private:
    friend constexpr qfloat16 qfloat16FromBits(quint16 bits) noexcept;
    quint16 m_bits;
};

static_assert(sizeof(qfloat16) == 2, "qfloat16 must be 16 bits");

constexpr qfloat16 qfloat16FromBits(quint16 bits) noexcept {
    qfloat16 r;
    r.m_bits = bits;
    return r;
}

inline qfloat16 operator+(qfloat16 a, qfloat16 b) noexcept { return qfloat16(float(a) + float(b)); }
inline qfloat16 operator-(qfloat16 a, qfloat16 b) noexcept { return qfloat16(float(a) - float(b)); }
inline qfloat16 operator*(qfloat16 a, qfloat16 b) noexcept { return qfloat16(float(a) * float(b)); }
inline qfloat16 operator/(qfloat16 a, qfloat16 b) noexcept { return qfloat16(float(a) / float(b)); }
inline bool operator==(qfloat16 a, qfloat16 b) noexcept { return float(a) == float(b); }
inline bool operator!=(qfloat16 a, qfloat16 b) noexcept { return float(a) != float(b); }
inline bool operator<(qfloat16 a, qfloat16 b) noexcept { return float(a) < float(b); }
inline bool operator>(qfloat16 a, qfloat16 b) noexcept { return float(a) > float(b); }
inline bool operator<=(qfloat16 a, qfloat16 b) noexcept { return float(a) <= float(b); }
inline bool operator>=(qfloat16 a, qfloat16 b) noexcept { return float(a) >= float(b); }

#define NT_QFLOAT16_MIXED(op, type, calc, result)                                                                      \
    inline result operator op(qfloat16 a, type b) noexcept { return static_cast<calc>(a) op static_cast<calc>(b); }    \
    inline result operator op(type a, qfloat16 b) noexcept { return static_cast<calc>(a) op static_cast<calc>(b); }
#define NT_QFLOAT16_MIXED_TYPE(type, calc)                                                                             \
    NT_QFLOAT16_MIXED(+, type, calc, calc)                                                                             \
    NT_QFLOAT16_MIXED(-, type, calc, calc)                                                                             \
    NT_QFLOAT16_MIXED(*, type, calc, calc)                                                                             \
    NT_QFLOAT16_MIXED(/, type, calc, calc)                                                                             \
    NT_QFLOAT16_MIXED(==, type, calc, bool)                                                                            \
    NT_QFLOAT16_MIXED(!=, type, calc, bool)                                                                            \
    NT_QFLOAT16_MIXED(<, type, calc, bool)                                                                             \
    NT_QFLOAT16_MIXED(>, type, calc, bool)                                                                             \
    NT_QFLOAT16_MIXED(<=, type, calc, bool)                                                                            \
    NT_QFLOAT16_MIXED(>=, type, calc, bool)
NT_QFLOAT16_MIXED_TYPE(float, float)
NT_QFLOAT16_MIXED_TYPE(double, double)
NT_QFLOAT16_MIXED_TYPE(int, float)
#undef NT_QFLOAT16_MIXED_TYPE
#undef NT_QFLOAT16_MIXED

inline bool qIsInf(qfloat16 f) noexcept { return f.isInf(); }
inline bool qIsNaN(qfloat16 f) noexcept { return f.isNaN(); }
inline bool qIsFinite(qfloat16 f) noexcept { return f.isFinite(); }
inline int qFpClassify(qfloat16 f) noexcept { return f.fpClassify(); }
inline bool qFuzzyIsNull(qfloat16 f) noexcept { return float(f) > -0.001f && float(f) < 0.001f; }
inline bool qFuzzyCompare(qfloat16 a, qfloat16 b) noexcept {
    const float x = a, y = b;
    return std::fabs(x - y) * 102.5f <= std::fmin(std::fabs(x), std::fabs(y));
}

// Converts `length` values; the arrays must not overlap.
void qFloatToFloat16(qfloat16 *out, const float *in, qsizetype length) noexcept;
void qFloatFromFloat16(float *out, const qfloat16 *in, qsizetype length) noexcept;

namespace std {
template <> class numeric_limits<qfloat16> : public numeric_limits<float> {
public:
    static constexpr bool is_specialized = true;
    static constexpr int digits = 11;
    static constexpr int digits10 = 3;
    static constexpr int max_digits10 = 5;
    static constexpr int radix = 2;
    static constexpr int min_exponent = -13;
    static constexpr int min_exponent10 = -4;
    static constexpr int max_exponent = 16;
    static constexpr int max_exponent10 = 4;
    static constexpr qfloat16 min() noexcept { return qfloat16FromBits(0x0400); }
    static constexpr qfloat16 lowest() noexcept { return qfloat16FromBits(0xfbff); }
    static constexpr qfloat16 max() noexcept { return qfloat16FromBits(0x7bff); }
    static constexpr qfloat16 epsilon() noexcept { return qfloat16FromBits(0x1400); }
    static constexpr qfloat16 round_error() noexcept { return qfloat16FromBits(0x3800); }
    static constexpr qfloat16 infinity() noexcept { return qfloat16FromBits(0x7c00); }
    static constexpr qfloat16 quiet_NaN() noexcept { return qfloat16FromBits(0x7e00); }
    static constexpr qfloat16 signaling_NaN() noexcept { return qfloat16FromBits(0x7d00); }
    static constexpr qfloat16 denorm_min() noexcept { return qfloat16FromBits(0x0001); }
};
} // namespace std
