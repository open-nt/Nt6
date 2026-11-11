// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <array>
#include <cstddef>
#include <iterator>
#include <type_traits>

/*!
    \namespace q20
    \inmodule QtCore
    \brief The q20 namespace holds C++20 library facilities for use from C++17.

    \differences
    Only dynamic_extent is provided.
    \enddifferences
*/
namespace q20 {
/*!
    \variable q20::dynamic_extent

    A constant of type \c std::size_t that is the default extent of QSpan and
    signals that the number of elements is not part of the type.
*/
inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);
}

/*!
    \class QSpan
    \brief The QSpan class provides a non-owning view of a contiguous sequence of elements.

    A QSpan holds a pointer and an element count. It does not own or copy the
    elements, so the sequence it refers to must outlive the span. Spans are
    cheap to copy and are normally passed by value. A \c{QSpan<const T>}
    gives read-only access, and a \c{QSpan<T>} lets the elements be modified.

    A QSpan can be constructed from a pointer and a count, a pair of
    pointers, a C array, a \c std::array, or any container providing
    \c data() and \c size() such as QList or \c std::vector:

    \code
    QList<int> values = {1, 2, 3, 4};
    QSpan<const int> view(values);
    int sum = 0;
    for (int v : view.first(2))
        sum += v;
    \endcode

    \differences
    The extent is not enforced: every QSpan stores its size at run time, and
    first(), last() and the other slicing functions always return a
    dynamic-extent \c{QSpan<T>}, as do the free functions \c as_bytes() and
    \c as_writable_bytes(). Indexes and counts are not bounds-checked.
    \enddifferences
*/
template <typename T, std::size_t E = q20::dynamic_extent>
class QSpan {
public:
    /*!
        The type of the elements, \c T, including any \c const qualifier.
    */
    using element_type = T;
    /*!
        The type of the elements with any \c const or \c volatile qualifier
        removed.
    */
    using value_type = std::remove_cv_t<T>;
    /*!
        The signed type used for sizes and indexes, \c qsizetype.
    */
    using size_type = qsizetype;
    /*!
        The signed type used for the distance between iterators,
        \c qptrdiff.
    */
    using difference_type = qptrdiff;
    /*!
        A pointer to an element, \c{T *}.
    */
    using pointer = T *;
    /*!
        A pointer to a constant element, \c{const T *}.
    */
    using const_pointer = const T *;
    /*!
        A reference to an element, \c{T &}.
    */
    using reference = T &;
    /*!
        A reference to a constant element, \c{const T &}.
    */
    using const_reference = const T &;
    /*!
        The iterator type, a plain pointer \c{T *}.
    */
    using iterator = T *;
    /*!
        The constant iterator type, a plain pointer \c{const T *}.
    */
    using const_iterator = const T *;
    /*!
        The reverse iterator type.
    */
    using reverse_iterator = std::reverse_iterator<iterator>;
    /*!
        The constant reverse iterator type.
    */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    /*!
        The extent given as the second template argument, or
        \c q20::dynamic_extent. It does not constrain the size of the span.
    */
    static constexpr std::size_t extent = E;

    /*!
        Constructs an empty span that refers to no elements.
    */
    constexpr QSpan() noexcept = default;
    /*!
        Constructs a span over \a count elements starting at \a first.
    */
    constexpr QSpan(T *first, qsizetype count) noexcept : p(first), n(count) {}
    /*!
        \overload

        Constructs a span over the elements in the range from \a first up to,
        but not including, \a last.
    */
    template <typename End, typename = std::enable_if_t<std::is_pointer_v<End> && std::is_convertible_v<End, T *>>>
    constexpr QSpan(T *first, End last) noexcept : p(first), n(last - first) {}
    /*!
        \overload

        Constructs a span over the elements of the C array \a array.
    */
    template <std::size_t N>
    constexpr QSpan(T (&array)[N]) noexcept : p(array), n(N) {}
    /*!
        \overload

        Constructs a span over the elements of the \c std::array \a array.
    */
    template <typename U, std::size_t N, typename = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr QSpan(std::array<U, N> &array) noexcept : p(array.data()), n(N) {}
    /*!
        \overload

        Constructs a span over the elements of the constant \c std::array
        \a array.
    */
    template <typename U, std::size_t N, typename = std::enable_if_t<std::is_convertible_v<const U (*)[], T (*)[]>>>
    constexpr QSpan(const std::array<U, N> &array) noexcept : p(array.data()), n(N) {}
    /*!
        \overload

        Constructs a span over the elements of the container \a c, which must
        provide \c data() and \c size().
    */
    template <typename C,
              typename = std::enable_if_t<
                  !std::is_array_v<std::remove_reference_t<C>> &&
                  std::is_convertible_v<std::remove_pointer_t<decltype(std::declval<C &>().data())> (*)[], T (*)[]>>,
              typename = decltype(std::declval<C &>().size())>
    constexpr QSpan(C &&c) noexcept : p(c.data()), n(static_cast<qsizetype>(c.size())) {}
    /*!
        \overload

        Constructs a span from the span \a other, whose element type is
        convertible to \c T, for example from \c{QSpan<int>} to
        \c{QSpan<const int>}.
    */
    template <typename U, std::size_t F, typename = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
    constexpr QSpan(QSpan<U, F> other) noexcept : p(other.data()), n(other.size()) {}

    /*!
        Returns the number of elements in the span.

        \sa size_bytes(), isEmpty()
    */
    constexpr qsizetype size() const noexcept { return n; }
    /*!
        Returns the number of elements in the span; the same as size().
    */
    constexpr qsizetype count() const noexcept { return n; }
    /*!
        Returns the number of elements in the span; the same as size().
    */
    constexpr qsizetype length() const noexcept { return n; }
    /*!
        Returns the size of the viewed elements in bytes.

        \sa size()
    */
    constexpr qsizetype size_bytes() const noexcept { return n * static_cast<qsizetype>(sizeof(T)); }
    /*!
        Returns \c true if the span has no elements; otherwise returns
        \c false.

        \sa size()
    */
    constexpr bool isEmpty() const noexcept { return n == 0; }
    /*!
        Returns \c true if the span has no elements; the same as isEmpty().
    */
    constexpr bool empty() const noexcept { return n == 0; }
    /*!
        Returns a pointer to the first element.
    */
    constexpr T *data() const noexcept { return p; }
    /*!
        Returns a reference to the element at index \a i. The index must be
        valid; it is not checked.

        \sa at()
    */
    constexpr T &operator[](qsizetype i) const { return p[i]; }
    /*!
        Returns a reference to the element at index \a i. The index must be
        valid; it is not checked.

        This is equivalent to the subscript operator.
    */
    constexpr T &at(qsizetype i) const { return p[i]; }
    /*!
        Returns a reference to the first element. The span must not be empty.

        \sa back()
    */
    constexpr T &front() const { return p[0]; }
    /*!
        Returns a reference to the last element. The span must not be empty.

        \sa front()
    */
    constexpr T &back() const { return p[n - 1]; }
    /*!
        Returns an iterator to the first element.

        \sa end()
    */
    constexpr iterator begin() const noexcept { return p; }
    /*!
        Returns an iterator to the position after the last element.

        \sa begin()
    */
    constexpr iterator end() const noexcept { return p + n; }
    /*!
        Returns a constant iterator to the first element.

        \sa cend()
    */
    constexpr const_iterator cbegin() const noexcept { return p; }
    /*!
        Returns a constant iterator to the position after the last element.

        \sa cbegin()
    */
    constexpr const_iterator cend() const noexcept { return p + n; }
    /*!
        Returns a reverse iterator to the last element.

        \sa rend()
    */
    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    /*!
        Returns a reverse iterator to the position before the first element.

        \sa rbegin()
    */
    constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
    /*!
        Returns a const reverse iterator pointing to the last element of the span.

        \sa crend(), rbegin()
    */
    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    /*!
        Returns a const reverse iterator pointing just before the first element of the span.

        \sa crbegin(), rend()
    */
    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    /*!
        Returns a span over the first \a c elements. \a c must not exceed
        size().

        \sa last(), subspan()
    */
    constexpr QSpan<T> first(qsizetype c) const { return QSpan<T>(p, c); }
    /*!
        Returns a span over the last \a c elements. \a c must not exceed
        size().

        \sa first(), subspan()
    */
    constexpr QSpan<T> last(qsizetype c) const { return QSpan<T>(p + n - c, c); }
    /*!
        Returns a span over the elements from index \a pos to the end.

        \sa first(), last(), sliced()
    */
    constexpr QSpan<T> subspan(qsizetype pos) const { return QSpan<T>(p + pos, n - pos); }
    /*!
        \overload

        Returns a span over \a c elements starting at index \a pos.
    */
    constexpr QSpan<T> subspan(qsizetype pos, qsizetype c) const { return QSpan<T>(p + pos, c); }
    /*!
        Returns a span over the elements from index \a pos to the end; the
        same as subspan().
    */
    constexpr QSpan<T> sliced(qsizetype pos) const { return subspan(pos); }
    /*!
        \overload

        Returns a span over \a c elements starting at index \a pos; the same
        as subspan().
    */
    constexpr QSpan<T> sliced(qsizetype pos, qsizetype c) const { return subspan(pos, c); }
    /*!
        Returns a span over all but the last \a c elements.

        \sa first(), last()
    */
    constexpr QSpan<T> chopped(qsizetype c) const { return first(n - c); }
    /*!
        \since 6.9
        Removes the last \a c elements from the span.

        \sa chopped(), slice()
    */
    constexpr void chop(qsizetype c) { n -= c; }
    /*!
        \since 6.9
        Removes the elements before index \a pos from the span.

        \sa sliced(), chop()
    */
    constexpr void slice(qsizetype pos) {
        p += pos;
        n -= pos;
    }
    /*!
        \overload
        \since 6.9
        Makes the span refer to \a c elements starting at index \a pos.
    */
    constexpr void slice(qsizetype pos, qsizetype c) {
        p += pos;
        n = c;
    }

private:
    T *p = nullptr;
    qsizetype n = 0;
};

template <typename T, std::size_t N>
QSpan(T (&)[N]) -> QSpan<T>;
template <typename C>
QSpan(C &) -> QSpan<std::remove_pointer_t<decltype(std::declval<C &>().data())>>;

// Returns the storage of the elements of s as read-only bytes.
template <typename T, std::size_t E> QSpan<const std::byte> as_bytes(QSpan<T, E> s) noexcept {
    return QSpan<const std::byte>(reinterpret_cast<const std::byte *>(s.data()), s.size_bytes());
}

// Returns the storage of the elements of s as writable bytes; s must be a span of non-const elements.
template <typename T, std::size_t E, typename = std::enable_if_t<!std::is_const_v<T>>>
QSpan<std::byte> as_writable_bytes(QSpan<T, E> s) noexcept {
    return QSpan<std::byte>(reinterpret_cast<std::byte *>(s.data()), s.size_bytes());
}
