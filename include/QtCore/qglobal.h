// MIT License - see LICENSE
#pragma once

#include <QtCore/qconfig.h>

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

// 64-bit integer literals of the types qint64 and quint64, whatever those are on the platform.
#define Q_INT64_C(c) static_cast<qint64>(c##LL)
#define Q_UINT64_C(c) static_cast<quint64>(c##ULL)

#define Q_LIKELY(x) (x)
#define Q_UNLIKELY(x) (x)

#define QT_VERSION_CHECK(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#define QT_VERSION QT_VERSION_CHECK(QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH)

const char *qVersion() noexcept;

#define Q_UNUSED(x) (void)(x);
#define Q_ASSERT(cond) assert(cond)
#define Q_DISABLE_COPY(Class)      \
    Class(const Class &) = delete; \
    Class &operator=(const Class &) = delete;

// Markers that ntmoc reads; they expand to nothing. Q_OBJECT, Q_GADGET, Q_NAMESPACE, Q_ENUM() and
// Q_FLAG() are in qobjectdefs.h, and Q_DECLARE_METATYPE() in qmetatype.h.
#define Q_SIGNALS public
#define Q_SLOTS
#define Q_EMIT
#define Q_PROPERTY(...)
#define Q_PRIVATE_PROPERTY(...)
#define Q_CLASSINFO(name, value)
#define Q_ENUMS(...)
#define Q_FLAGS(...)
#define Q_INVOKABLE
#define Q_SIGNAL
#define Q_SLOT
#define Q_SCRIPTABLE
#define Q_REVISION(...)
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

/*!
    \class QOverload
    \brief The QOverload struct helps to select an overloaded member function.

    Use \c qOverload to pick one overload of a member function when taking
    its address, for example when connecting a signal:

    \code
    connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), receiver, &Receiver::setValue);
    \endcode

    \c qConstOverload and \c qNonConstOverload pick only among the const or
    the non-const overloads.

    \differences
    None.
    \enddifferences
*/
template <typename... Args>
struct QOverload {
    /*!
        Returns \a ptr, the member function pointer of the overload taking
        \c Args.
    */
    template <typename R, typename T>
    constexpr auto operator()(R (T::*ptr)(Args...)) const noexcept -> decltype(ptr) { return ptr; }
    /*!
        \overload
        Returns \a ptr, the const member function pointer of the overload
        taking \c Args.
    */
    template <typename R, typename T>
    constexpr auto operator()(R (T::*ptr)(Args...) const) const noexcept -> decltype(ptr) { return ptr; }
};
template <typename... Args>
inline constexpr QOverload<Args...> qOverload = {};

namespace QtPrivate {
template <typename... Args> struct QConstOverload {
    template <typename R, typename T>
    constexpr auto operator()(R (T::*ptr)(Args...) const) const noexcept -> decltype(ptr) {
        return ptr;
    }
};
template <typename... Args> struct QNonConstOverload {
    template <typename R, typename T> constexpr auto operator()(R (T::*ptr)(Args...)) const noexcept -> decltype(ptr) {
        return ptr;
    }
};
} // namespace QtPrivate

// Selects the const overload of a member function taking Args.
template <typename... Args> inline constexpr QtPrivate::QConstOverload<Args...> qConstOverload = {};
// Selects the non-const overload of a member function taking Args.
template <typename... Args> inline constexpr QtPrivate::QNonConstOverload<Args...> qNonConstOverload = {};
