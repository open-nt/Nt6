// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cstddef>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

// The hashing functions of Nt: qHash() for the built-in types, qHashBits(), qHashRange(),
// qHashRangeCommutative() and qHashMulti(), which QHash, QSet and QMultiHash use through ADL.
// Hash values are not stable across Nt versions or runs; do not store them.

// Returns a hash of an integer or enumeration key, mixed with seed.
template <typename T, typename = std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>>>
constexpr std::size_t qHash(T key, std::size_t seed = 0) noexcept {
    std::size_t v = 0;
    if constexpr (std::is_enum_v<T>)
        v = static_cast<std::size_t>(static_cast<std::underlying_type_t<T>>(key));
    else
        v = static_cast<std::size_t>(key);
    // A 64-bit finalizer (from MurmurHash3), so that nearby keys spread over the buckets.
    v ^= v >> 33;
    v *= 0xff51afd7ed558ccdULL;
    v ^= v >> 33;
    return v ^ seed;
}

// Returns a hash of a floating-point key; 0.0 and -0.0 hash the same.
inline std::size_t qHash(double key, std::size_t seed = 0) noexcept {
    if (key == 0.0)
        key = 0.0;
    unsigned long long bits = 0;
    std::memcpy(&bits, &key, sizeof bits);
    return qHash(bits, seed);
}

// Returns a hash of a float key; 0.0f and -0.0f hash the same.
inline std::size_t qHash(float key, std::size_t seed = 0) noexcept { return qHash(static_cast<double>(key), seed); }

// Returns a hash of a pointer key.
template <typename T> std::size_t qHash(const T *key, std::size_t seed = 0) noexcept {
    return qHash(reinterpret_cast<quintptr>(key), seed);
}

// Returns a hash of nullptr.
inline std::size_t qHash(std::nullptr_t, std::size_t seed = 0) noexcept { return qHash(quintptr(0), seed); }

// Returns a hash of a pair, from both members; declared here so that the detection below sees it.
template <typename T1, typename T2> std::size_t qHash(const std::pair<T1, T2> &key, std::size_t seed = 0);

namespace QtPrivate {

template <typename K, typename = void> struct HasQHash : std::false_type {};
template <typename K>
struct HasQHash<K, std::void_t<decltype(qHash(std::declval<const K &>(), std::size_t{}))>> : std::true_type {};

template <typename T, typename = void> struct HasStdHash : std::false_type {};
template <typename T>
struct HasStdHash<T, std::void_t<decltype(std::hash<T>{}(std::declval<const T &>()))>> : std::true_type {};

// True if a value of T can be hashed with qHash() or std::hash.
template <typename T> inline constexpr bool IsHashable = HasQHash<T>::value || HasStdHash<T>::value;

// Mixes `value` into `seed`, so that the order of the values matters.
constexpr std::size_t hashCombine(std::size_t seed, std::size_t value) noexcept {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

template <typename T> std::size_t hashOne(const T &value, std::size_t seed) {
    if constexpr (HasQHash<T>::value)
        return qHash(value, seed);
    else
        return hashCombine(seed, std::hash<T>{}(value));
}

} // namespace QtPrivate

// Returns a hash of the len bytes at p, mixed with seed (FNV-1a).
inline std::size_t qHashBits(const void *p, std::size_t len, std::size_t seed = 0) noexcept {
    const auto *bytes = static_cast<const unsigned char *>(p);
    std::size_t h = 0xcbf29ce484222325ULL ^ seed;
    for (std::size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

// Returns a hash of the values in [first, last), which depends on their order.
template <typename InputIterator>
std::size_t qHashRange(InputIterator first, InputIterator last, std::size_t seed = 0) {
    std::size_t h = seed;
    for (; first != last; ++first)
        h = QtPrivate::hashCombine(h, QtPrivate::hashOne(*first, 0));
    return h;
}

// Returns a hash of the values in [first, last) that does not depend on their order.
template <typename InputIterator>
std::size_t qHashRangeCommutative(InputIterator first, InputIterator last, std::size_t seed = 0) {
    std::size_t sum = 0;
    for (; first != last; ++first)
        sum += QtPrivate::hashOne(*first, 0);
    return QtPrivate::hashCombine(seed, sum);
}

// Returns a hash of args, in order, mixed with seed.
template <typename... Args> std::size_t qHashMulti(std::size_t seed, const Args &...args) {
    std::size_t h = seed;
    ((h = QtPrivate::hashCombine(h, QtPrivate::hashOne(args, 0))), ...);
    return h;
}

template <typename T1, typename T2> std::size_t qHash(const std::pair<T1, T2> &key, std::size_t seed) {
    return qHashMulti(seed, key.first, key.second);
}
