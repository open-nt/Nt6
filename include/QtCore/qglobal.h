// MIT License - see LICENSE
#pragma once

#include <QtCore/qconfig.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

using qint8 = std::int8_t;
using quint8 = std::uint8_t;
using qint16 = std::int16_t;
using quint16 = std::uint16_t;
using qint32 = std::int32_t;
using quint32 = std::uint32_t;
// long long, as in Qt, whatever std::int64_t is: "%lld" then formats a qint64 on every platform.
using qint64 = long long;
using quint64 = unsigned long long;
using qlonglong = long long;
using qulonglong = unsigned long long;
namespace QtPrivate {
// The integers of Qt that are as wide as a size or a pointer.
template <std::size_t Bytes> struct IntegerForSize;
template <> struct IntegerForSize<4> {
    using Signed = qint32;
    using Unsigned = quint32;
};
template <> struct IntegerForSize<8> {
    using Signed = qint64;
    using Unsigned = quint64;
};
} // namespace QtPrivate
// As in Qt, these are qint64 and quint64 on a 64-bit platform (and qint32 and quint32 on a 32-bit one), and
// not the types that the C++ library uses for sizes, which may be long: a size then converts to what a
// qint64 converts to, and is formatted with "%lld".
using qsizetype = QtPrivate::IntegerForSize<sizeof(std::size_t)>::Signed;
using qptrdiff = QtPrivate::IntegerForSize<sizeof(void *)>::Signed;
using qintptr = QtPrivate::IntegerForSize<sizeof(void *)>::Signed;
using quintptr = QtPrivate::IntegerForSize<sizeof(void *)>::Unsigned;
using qreal = double;
// A pointer to a function of any type, as QLibrary::resolve() returns it; cast it before the call.
using QFunctionPointer = void (*)();
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

// The text of a macro argument, after the argument has been expanded.
#define QT_STRINGIFY2(x) #x
#define QT_STRINGIFY(x) QT_STRINGIFY2(x)

// Nt is not built in a namespace.
#define QT_BEGIN_NAMESPACE
#define QT_END_NAMESPACE
#define QT_USE_NAMESPACE
#define QT_PREPEND_NAMESPACE(name) ::name
#define QT_FORWARD_DECLARE_CLASS(name) class name;
#define QT_FORWARD_DECLARE_STRUCT(name) struct name;

// The keywords and attributes that Qt wrapped for old compilers.
#define Q_DECL_CONSTEXPR constexpr
#define Q_DECL_RELAXED_CONSTEXPR constexpr
#define Q_DECL_NOEXCEPT noexcept
#define Q_DECL_NOTHROW noexcept
#define Q_DECL_NOEXCEPT_EXPR(x) noexcept(x)
#define Q_DECL_OVERRIDE override
#define Q_DECL_FINAL final
#define Q_DECL_EQ_DEFAULT = default
#define Q_DECL_EQ_DELETE = delete
#define Q_NULLPTR nullptr
#define Q_DECL_UNUSED [[maybe_unused]]
#define Q_DECL_DEPRECATED [[deprecated]]
#define Q_DECL_DEPRECATED_X(text) [[deprecated(text)]]
#define Q_REQUIRED_RESULT [[nodiscard]]
#define Q_NORETURN [[noreturn]]
#define Q_FALLTHROUGH() [[fallthrough]]
#if defined(_MSC_VER)
#define Q_DECL_EXPORT __declspec(dllexport)
#define Q_DECL_IMPORT __declspec(dllimport)
#define Q_DECL_HIDDEN
#define Q_ALWAYS_INLINE __forceinline
#define Q_NEVER_INLINE __declspec(noinline)
#define NT_UNREACHABLE_IMPL() __assume(0)
#elif defined(__GNUC__) || defined(__clang__)
#define Q_DECL_EXPORT __attribute__((visibility("default")))
#define Q_DECL_IMPORT __attribute__((visibility("default")))
#define Q_DECL_HIDDEN __attribute__((visibility("hidden")))
#define Q_ALWAYS_INLINE inline __attribute__((always_inline))
#define Q_NEVER_INLINE __attribute__((noinline))
#define NT_UNREACHABLE_IMPL() __builtin_unreachable()
#else
#define Q_DECL_EXPORT
#define Q_DECL_IMPORT
#define Q_DECL_HIDDEN
#define Q_ALWAYS_INLINE inline
#define Q_NEVER_INLINE
#define NT_UNREACHABLE_IMPL() std::abort()
#endif

#define Q_UNUSED(x) (void)(x);
#define Q_ASSERT(cond) assert(cond)
// An assertion that also names the place and says what is wrong.
#define Q_ASSERT_X(cond, where, what) assert((cond) && (where) && (what))
#define Q_STATIC_ASSERT(condition) static_assert(bool(condition), #condition)
#define Q_STATIC_ASSERT_X(condition, message) static_assert(bool(condition), message)
// Code that cannot be reached: asserts in a debug build and is a hint to the optimizer otherwise.
#define Q_UNREACHABLE()                                                                                                \
    do {                                                                                                               \
        Q_ASSERT_X(false, "Q_UNREACHABLE()", "Q_UNREACHABLE was reached");                                             \
        NT_UNREACHABLE_IMPL();                                                                                         \
    } while (false)
#define Q_UNREACHABLE_RETURN(...)                                                                                      \
    do {                                                                                                               \
        Q_UNREACHABLE();                                                                                               \
        return __VA_ARGS__;                                                                                            \
    } while (false)
// Tells the optimizer that the expression holds; it must have no side effects.
#define Q_ASSUME(expr)                                                                                                 \
    do {                                                                                                               \
        const bool ntAssumed = (expr);                                                                                 \
        Q_ASSERT_X(ntAssumed, "Q_ASSUME()", "Assumption in Q_ASSUME(\"" #expr "\") was not correct");                  \
        if (!ntAssumed)                                                                                                \
            NT_UNREACHABLE_IMPL();                                                                                     \
    } while (false)
#define Q_DISABLE_COPY(Class)      \
    Class(const Class &) = delete; \
    Class &operator=(const Class &) = delete;
#define Q_DISABLE_MOVE(Class)                                                                                          \
    Class(Class &&) = delete;                                                                                          \
    Class &operator=(Class &&) = delete;
#define Q_DISABLE_COPY_MOVE(Class)                                                                                     \
    Q_DISABLE_COPY(Class)                                                                                              \
    Q_DISABLE_MOVE(Class)

inline void qt_noop() {}
// Ends the program as std::terminate() does.
[[noreturn]] inline void qTerminate() noexcept { std::terminate(); }
// Throws std::bad_alloc. Q_CHECK_PTR() calls it for a null pointer.
[[noreturn]] inline void qBadAlloc() { throw std::bad_alloc(); }
#define Q_CHECK_PTR(p)                                                                                                 \
    do {                                                                                                               \
        if (Q_UNLIKELY(!(p)))                                                                                          \
            qBadAlloc();                                                                                               \
    } while (false)

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
#define Q_MOC_INCLUDE(...)

#ifndef QT_NO_KEYWORDS
#  define signals public
#  define slots
#  define emit
#  define foreach(variable, container) for (variable : container)
#  define forever for (;;)
#endif
#define Q_FOREACH(variable, container) for (variable : container)
#define Q_FOREVER for (;;)

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
// True if the value is exactly +0.0 or -0.0.
inline bool qIsNull(double d) noexcept { return d == 0.0; }
inline bool qIsNull(float f) noexcept { return f == 0.0f; }
// The value of an enumerator as its underlying integer type.
template <typename Enum> constexpr std::underlying_type_t<Enum> qToUnderlying(Enum e) noexcept {
    return static_cast<std::underlying_type_t<Enum>>(e);
}
// Makes a std::pair, as QPair is in Qt 6.
template <typename T1, typename T2>
constexpr std::pair<std::decay_t<T1>, std::decay_t<T2>> qMakePair(T1 &&value1, T2 &&value2) {
    return std::make_pair(std::forward<T1>(value1), std::forward<T2>(value2));
}
inline bool qFuzzyCompare(double p1, double p2) {
    return std::fabs(p1 - p2) * 1000000000000. <= std::fmin(std::fabs(p1), std::fabs(p2));
}

// A const reference to an lvalue, so that a range-based for loop does not call the non-const begin().
template <typename T> constexpr std::add_const_t<T> &qAsConst(T &t) noexcept { return t; }
template <typename T> void qAsConst(const T &&) = delete;
// Sets t to newValue and returns what it was.
template <typename T, typename U = T> constexpr T qExchange(T &t, U &&newValue) {
    T old = std::move(t);
    t = std::forward<U>(newValue);
    return old;
}

// The identifier that Q_DECLARE_INTERFACE() gave an interface, or nullptr. qobject_cast() finds interfaces
// without it, as it is a dynamic_cast.
template <typename T> inline const char *qobject_interface_iid() { return nullptr; }
#define Q_DECLARE_INTERFACE(IFace, IId)                                                                                \
    template <> inline const char *qobject_interface_iid<IFace *>() { return IId; }

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
