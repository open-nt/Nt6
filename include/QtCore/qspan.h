// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <array>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace q20 {
inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);
}

// A non-owning view of contiguous elements, usable from C++17.
template <typename T, std::size_t E = q20::dynamic_extent>
class QSpan {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = qsizetype;
    using difference_type = qptrdiff;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    static constexpr std::size_t extent = E;

    constexpr QSpan() noexcept = default;
    constexpr QSpan(T *first, qsizetype count) noexcept : p(first), n(count) {}
    constexpr QSpan(T *first, T *last) noexcept : p(first), n(last - first) {}
    template <std::size_t N>
    constexpr QSpan(T (&array)[N]) noexcept : p(array), n(N) {}
    template <typename U, std::size_t N, typename = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr QSpan(std::array<U, N> &array) noexcept : p(array.data()), n(N) {}
    template <typename U, std::size_t N, typename = std::enable_if_t<std::is_convertible_v<const U (*)[], T (*)[]>>>
    constexpr QSpan(const std::array<U, N> &array) noexcept : p(array.data()), n(N) {}
    template <typename C,
              typename = std::enable_if_t<
                  !std::is_array_v<std::remove_reference_t<C>> &&
                  std::is_convertible_v<std::remove_pointer_t<decltype(std::declval<C &>().data())> (*)[], T (*)[]>>,
              typename = decltype(std::declval<C &>().size())>
    constexpr QSpan(C &&c) noexcept : p(c.data()), n(static_cast<qsizetype>(c.size())) {}
    template <typename U, std::size_t F, typename = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr QSpan(QSpan<U, F> other) noexcept : p(other.data()), n(other.size()) {}

    constexpr qsizetype size() const noexcept { return n; }
    constexpr qsizetype count() const noexcept { return n; }
    constexpr qsizetype length() const noexcept { return n; }
    constexpr qsizetype size_bytes() const noexcept { return n * static_cast<qsizetype>(sizeof(T)); }
    constexpr bool isEmpty() const noexcept { return n == 0; }
    constexpr bool empty() const noexcept { return n == 0; }
    constexpr T *data() const noexcept { return p; }
    constexpr T &operator[](qsizetype i) const { return p[i]; }
    constexpr T &front() const { return p[0]; }
    constexpr T &back() const { return p[n - 1]; }
    constexpr iterator begin() const noexcept { return p; }
    constexpr iterator end() const noexcept { return p + n; }
    constexpr const_iterator cbegin() const noexcept { return p; }
    constexpr const_iterator cend() const noexcept { return p + n; }
    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

    constexpr QSpan<T> first(qsizetype c) const { return QSpan<T>(p, c); }
    constexpr QSpan<T> last(qsizetype c) const { return QSpan<T>(p + n - c, c); }
    constexpr QSpan<T> subspan(qsizetype pos) const { return QSpan<T>(p + pos, n - pos); }
    constexpr QSpan<T> subspan(qsizetype pos, qsizetype c) const { return QSpan<T>(p + pos, c); }
    constexpr QSpan<T> sliced(qsizetype pos) const { return subspan(pos); }
    constexpr QSpan<T> sliced(qsizetype pos, qsizetype c) const { return subspan(pos, c); }
    constexpr QSpan<T> chopped(qsizetype c) const { return first(n - c); }

private:
    T *p = nullptr;
    qsizetype n = 0;
};

template <typename T, std::size_t N>
QSpan(T (&)[N]) -> QSpan<T>;
template <typename C>
QSpan(C &) -> QSpan<std::remove_pointer_t<decltype(std::declval<C &>().data())>>;
