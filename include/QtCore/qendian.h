// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cstring>
#include <type_traits>

namespace QtPrivate {

template <typename T> constexpr T byteSwapInteger(T value) noexcept {
    using U = std::make_unsigned_t<T>;
    U in = static_cast<U>(value);
    U out = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out = static_cast<U>((out << 8) | (in & 0xff));
        in = static_cast<U>(in >> 8);
    }
    return static_cast<T>(out);
}

constexpr bool hostIsLittleEndian() noexcept {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return false;
#else
    return true;
#endif
}

} // namespace QtPrivate

// Reverses the byte order of an integer.
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>> constexpr T qbswap(T source) noexcept {
    return sizeof(T) == 1 ? source : QtPrivate::byteSwapInteger(source);
}

// Floating-point values swap their object representation.
inline float qbswap(float source) noexcept {
    quint32 bits;
    std::memcpy(&bits, &source, sizeof bits);
    bits = qbswap(bits);
    std::memcpy(&source, &bits, sizeof bits);
    return source;
}
inline double qbswap(double source) noexcept {
    quint64 bits;
    std::memcpy(&bits, &source, sizeof bits);
    bits = qbswap(bits);
    std::memcpy(&source, &bits, sizeof bits);
    return source;
}

template <typename T> constexpr T qToBigEndian(T source) noexcept {
    return QtPrivate::hostIsLittleEndian() ? qbswap(source) : source;
}
template <typename T> constexpr T qFromBigEndian(T source) noexcept { return qToBigEndian(source); }
template <typename T> constexpr T qToLittleEndian(T source) noexcept {
    return QtPrivate::hostIsLittleEndian() ? source : qbswap(source);
}
template <typename T> constexpr T qFromLittleEndian(T source) noexcept { return qToLittleEndian(source); }

// Stores `source` at `dest` in big-endian order; `dest` need not be aligned.
template <typename T> void qToBigEndian(T source, void *dest) noexcept {
    const T v = qToBigEndian(source);
    std::memcpy(dest, &v, sizeof v);
}
template <typename T> void qToLittleEndian(T source, void *dest) noexcept {
    const T v = qToLittleEndian(source);
    std::memcpy(dest, &v, sizeof v);
}
// Reads a big-endian T from `src`; `src` need not be aligned.
template <typename T> T qFromBigEndian(const void *src) noexcept {
    T v;
    std::memcpy(&v, src, sizeof v);
    return qFromBigEndian(v);
}
template <typename T> T qFromLittleEndian(const void *src) noexcept {
    T v;
    std::memcpy(&v, src, sizeof v);
    return qFromLittleEndian(v);
}
// Converts `count` values between byte orders. The buffers may be the same.
template <typename T> void qFromBigEndian(const void *source, qsizetype count, void *dest) noexcept {
    for (qsizetype i = 0; i < count; ++i) {
        const T v = qFromBigEndian<T>(static_cast<const char *>(source) + i * static_cast<qsizetype>(sizeof(T)));
        std::memcpy(static_cast<char *>(dest) + i * static_cast<qsizetype>(sizeof(T)), &v, sizeof v);
    }
}
template <typename T> void qToBigEndian(const void *source, qsizetype count, void *dest) noexcept {
    for (qsizetype i = 0; i < count; ++i) {
        T v;
        std::memcpy(&v, static_cast<const char *>(source) + i * static_cast<qsizetype>(sizeof(T)), sizeof v);
        qToBigEndian<T>(v, static_cast<char *>(dest) + i * static_cast<qsizetype>(sizeof(T)));
    }
}
template <typename T> void qFromLittleEndian(const void *source, qsizetype count, void *dest) noexcept {
    for (qsizetype i = 0; i < count; ++i) {
        const T v = qFromLittleEndian<T>(static_cast<const char *>(source) + i * static_cast<qsizetype>(sizeof(T)));
        std::memcpy(static_cast<char *>(dest) + i * static_cast<qsizetype>(sizeof(T)), &v, sizeof v);
    }
}
template <typename T> void qToLittleEndian(const void *source, qsizetype count, void *dest) noexcept {
    for (qsizetype i = 0; i < count; ++i) {
        T v;
        std::memcpy(&v, static_cast<const char *>(source) + i * static_cast<qsizetype>(sizeof(T)), sizeof v);
        qToLittleEndian<T>(v, static_cast<char *>(dest) + i * static_cast<qsizetype>(sizeof(T)));
    }
}

namespace QtPrivate {

template <typename T, bool Big> class SpecialInteger {
public:
    SpecialInteger() = default;
    SpecialInteger(T value) noexcept { store(value); }
    SpecialInteger &operator=(T value) noexcept {
        store(value);
        return *this;
    }
    operator T() const noexcept { return load(); }

    SpecialInteger &operator+=(T v) noexcept {
        store(static_cast<T>(load() + v));
        return *this;
    }
    SpecialInteger &operator-=(T v) noexcept {
        store(static_cast<T>(load() - v));
        return *this;
    }
    SpecialInteger &operator*=(T v) noexcept {
        store(static_cast<T>(load() * v));
        return *this;
    }
    SpecialInteger &operator/=(T v) noexcept {
        store(static_cast<T>(load() / v));
        return *this;
    }
    SpecialInteger &operator%=(T v) noexcept {
        store(static_cast<T>(load() % v));
        return *this;
    }
    SpecialInteger &operator&=(T v) noexcept {
        store(static_cast<T>(load() & v));
        return *this;
    }
    SpecialInteger &operator|=(T v) noexcept {
        store(static_cast<T>(load() | v));
        return *this;
    }
    SpecialInteger &operator^=(T v) noexcept {
        store(static_cast<T>(load() ^ v));
        return *this;
    }
    SpecialInteger &operator<<=(int v) noexcept {
        store(static_cast<T>(load() << v));
        return *this;
    }
    SpecialInteger &operator>>=(int v) noexcept {
        store(static_cast<T>(load() >> v));
        return *this;
    }
    SpecialInteger &operator++() noexcept { return *this += 1; }
    SpecialInteger operator++(int) noexcept {
        const SpecialInteger old = *this;
        *this += 1;
        return old;
    }
    SpecialInteger &operator--() noexcept { return *this -= 1; }
    SpecialInteger operator--(int) noexcept {
        const SpecialInteger old = *this;
        *this -= 1;
        return old;
    }

    static constexpr T max() noexcept { return (std::numeric_limits<T>::max)(); }
    static constexpr T min() noexcept { return (std::numeric_limits<T>::min)(); }

private:
    void store(T value) noexcept { m_bytes = Big ? qToBigEndian(value) : qToLittleEndian(value); }
    T load() const noexcept { return Big ? qFromBigEndian(m_bytes) : qFromLittleEndian(m_bytes); }

    T m_bytes;
};

} // namespace QtPrivate

/*!
    \class QBEInteger
    \brief The QBEInteger class provides an integer that is stored in big-endian byte order.

    The object has the size of \c T and holds the bytes in network order,
    whatever the byte order of the host, so it can be overlaid on a binary
    format. It converts to and from \c T implicitly, and supports the compound
    assignment and increment operators:

    \code
    struct Header {
        quint16_be length;
        quint32_be id;
    };
    Header h;
    h.length = 1500;
    quint16 n = h.length;
    \endcode

    \c T is an integer type.

    \sa QLEInteger
*/
template <typename T> class QBEInteger : public QtPrivate::SpecialInteger<T, true> {
public:
    /*!
        Constructs an uninitialized integer.
    */
    QBEInteger() = default;
    /*!
        Constructs an integer holding \a value.
    */
    QBEInteger(T value) noexcept : QtPrivate::SpecialInteger<T, true>(value) {}
    /*!
        Assigns \a value and returns this object.
    */
    QBEInteger &operator=(T value) noexcept {
        QtPrivate::SpecialInteger<T, true>::operator=(value);
        return *this;
    }
};

/*!
    \class QLEInteger
    \brief The QLEInteger class provides an integer that is stored in little-endian byte order.

    It is the counterpart of QBEInteger for formats that are little-endian.
    \c T is an integer type.

    \sa QBEInteger
*/
template <typename T> class QLEInteger : public QtPrivate::SpecialInteger<T, false> {
public:
    /*!
        Constructs an uninitialized integer.
    */
    QLEInteger() = default;
    /*!
        Constructs an integer holding \a value.
    */
    QLEInteger(T value) noexcept : QtPrivate::SpecialInteger<T, false>(value) {}
    /*!
        Assigns \a value and returns this object.
    */
    QLEInteger &operator=(T value) noexcept {
        QtPrivate::SpecialInteger<T, false>::operator=(value);
        return *this;
    }
};

using qint16_be = QBEInteger<qint16>;
using quint16_be = QBEInteger<quint16>;
using qint32_be = QBEInteger<qint32>;
using quint32_be = QBEInteger<quint32>;
using qint64_be = QBEInteger<qint64>;
using quint64_be = QBEInteger<quint64>;
using qint16_le = QLEInteger<qint16>;
using quint16_le = QLEInteger<quint16>;
using qint32_le = QLEInteger<qint32>;
using quint32_le = QLEInteger<quint32>;
using qint64_le = QLEInteger<qint64>;
using quint64_le = QLEInteger<quint64>;
