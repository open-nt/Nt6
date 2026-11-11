// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <limits>
#include <type_traits>

// Deletes the objects that the pointers in [begin, end) point to. The pointers themselves stay where they are.
template <typename ForwardIterator> void qDeleteAll(ForwardIterator begin, ForwardIterator end) {
    for (; begin != end; ++begin)
        delete *begin;
}
// Deletes the objects that the pointers in the container c point to. The container keeps its (dangling) items.
template <typename Container> void qDeleteAll(const Container &c) { qDeleteAll(c.begin(), c.end()); }

namespace QtPrivate {

// The number of bits that are set, counted in parallel: pairs, then nibbles, then the bytes are summed.
constexpr uint populationCount(unsigned long long v) noexcept {
    v = v - ((v >> 1) & 0x5555555555555555ULL);
    v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
    v = (v + (v >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
    return static_cast<uint>((v * 0x0101010101010101ULL) >> 56);
}
// The bits below the lowest set bit are the trailing zeros; of zero, every bit of the type is one.
template <typename T> constexpr uint trailingZeroBits(T v) noexcept {
    const auto wide = static_cast<unsigned long long>(v);
    if (wide == 0)
        return static_cast<uint>(std::numeric_limits<T>::digits);
    return populationCount((wide & (~wide + 1)) - 1);
}
// Every bit below the highest set bit is set as well; what is left unset are the leading zeros.
template <typename T> constexpr uint leadingZeroBits(T v) noexcept {
    auto wide = static_cast<unsigned long long>(v);
    wide |= wide >> 1;
    wide |= wide >> 2;
    wide |= wide >> 4;
    wide |= wide >> 8;
    wide |= wide >> 16;
    wide |= wide >> 32;
    return static_cast<uint>(std::numeric_limits<T>::digits) - populationCount(wide);
}

} // namespace QtPrivate

// The number of bits set in v.
constexpr uint qPopulationCount(unsigned char v) noexcept { return QtPrivate::populationCount(v); }
constexpr uint qPopulationCount(unsigned short v) noexcept { return QtPrivate::populationCount(v); }
constexpr uint qPopulationCount(unsigned int v) noexcept { return QtPrivate::populationCount(v); }
constexpr uint qPopulationCount(unsigned long v) noexcept { return QtPrivate::populationCount(v); }
constexpr uint qPopulationCount(unsigned long long v) noexcept { return QtPrivate::populationCount(v); }

// The number of zero bits below the lowest bit set in v; the width of the type if v is 0.
constexpr uint qCountTrailingZeroBits(unsigned char v) noexcept { return QtPrivate::trailingZeroBits(v); }
constexpr uint qCountTrailingZeroBits(unsigned short v) noexcept { return QtPrivate::trailingZeroBits(v); }
constexpr uint qCountTrailingZeroBits(unsigned int v) noexcept { return QtPrivate::trailingZeroBits(v); }
constexpr uint qCountTrailingZeroBits(unsigned long v) noexcept { return QtPrivate::trailingZeroBits(v); }
constexpr uint qCountTrailingZeroBits(unsigned long long v) noexcept { return QtPrivate::trailingZeroBits(v); }

// The number of zero bits above the highest bit set in v; the width of the type if v is 0.
constexpr uint qCountLeadingZeroBits(unsigned char v) noexcept { return QtPrivate::leadingZeroBits(v); }
constexpr uint qCountLeadingZeroBits(unsigned short v) noexcept { return QtPrivate::leadingZeroBits(v); }
constexpr uint qCountLeadingZeroBits(unsigned int v) noexcept { return QtPrivate::leadingZeroBits(v); }
constexpr uint qCountLeadingZeroBits(unsigned long v) noexcept { return QtPrivate::leadingZeroBits(v); }
constexpr uint qCountLeadingZeroBits(unsigned long long v) noexcept { return QtPrivate::leadingZeroBits(v); }
