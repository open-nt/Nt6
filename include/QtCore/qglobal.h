// MIT License - see LICENSE
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

using qint8 = std::int8_t;
using quint8 = std::uint8_t;
using qint16 = std::int16_t;
using quint16 = std::uint16_t;
using qint32 = std::int32_t;
using quint32 = std::uint32_t;
using qint64 = std::int64_t;
using quint64 = std::uint64_t;
using qlonglong = long long;
using qulonglong = unsigned long long;
using qsizetype = std::ptrdiff_t;
using qptrdiff = std::ptrdiff_t;
using qintptr = std::intptr_t;
using quintptr = std::uintptr_t;
using qreal = double;
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using ulong = unsigned long;

#define Q_LIKELY(x) (x)
#define Q_UNLIKELY(x) (x)

#define QT_VERSION_MAJOR 6
#define QT_VERSION_MINOR 0
#define QT_VERSION_PATCH 0
#define QT_VERSION_CHECK(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#define QT_VERSION QT_VERSION_CHECK(QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH)

#define Q_UNUSED(x) (void)(x);
#define Q_ASSERT(cond) assert(cond)
#define Q_DISABLE_COPY(Class)      \
    Class(const Class &) = delete; \
    Class &operator=(const Class &) = delete;

// Without moc these are markers only.
#define Q_OBJECT
#define Q_GADGET
#define Q_NAMESPACE
#define Q_SIGNALS public
#define Q_SLOTS
#define Q_EMIT
#define Q_PROPERTY(...)
#define Q_ENUM(...)
#define Q_FLAG(...)
#define Q_INVOKABLE
#define Q_DECLARE_METATYPE(...)
#define Q_INTERFACES(...)

#ifndef QT_NO_KEYWORDS
#  define signals public
#  define slots
#  define emit
#  define foreach(variable, container) for (variable : container)
#  define forever for (;;)
#endif
#define Q_FOREACH(variable, container) for (variable : container)

template <typename T>
constexpr const T &qMin(const T &a, const T &b) { return (b < a) ? b : a; }
template <typename T>
constexpr const T &qMax(const T &a, const T &b) { return (a < b) ? b : a; }
template <typename T>
constexpr const T &qBound(const T &lo, const T &v, const T &hi) { return qMax(lo, qMin(hi, v)); }
template <typename T>
constexpr T qAbs(const T &t) { return t >= 0 ? t : -t; }
template <typename T>
inline void qSwap(T &a, T &b) { using std::swap; swap(a, b); }
inline int qRound(double d) { return static_cast<int>(d >= 0.0 ? d + 0.5 : d - 0.5); }
inline qint64 qRound64(double d) { return static_cast<qint64>(d >= 0.0 ? d + 0.5 : d - 0.5); }
inline bool qFuzzyIsNull(double d) { return std::fabs(d) <= 0.000000000001; }
inline bool qFuzzyCompare(double p1, double p2) {
    return std::fabs(p1 - p2) * 1000000000000. <= std::fmin(std::fabs(p1), std::fabs(p2));
}

// moc-style cast; works on any polymorphic type.
template <typename T, typename Obj>
inline T qobject_cast(Obj *o) { return dynamic_cast<T>(o); }
template <typename T, typename Obj>
inline T qobject_cast(const Obj *o) { return dynamic_cast<T>(o); }

template <typename... Args>
struct QOverload {
    template <typename R, typename T>
    constexpr auto operator()(R (T::*ptr)(Args...)) const noexcept -> decltype(ptr) { return ptr; }
    template <typename R, typename T>
    constexpr auto operator()(R (T::*ptr)(Args...) const) const noexcept -> decltype(ptr) { return ptr; }
};
template <typename... Args>
inline constexpr QOverload<Args...> qOverload = {};
