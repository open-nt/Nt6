// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <utility>

// Keeps up to Prealloc elements inside the object, then moves to the heap.
/*!
    \class QVarLengthArray
    \brief The QVarLengthArray class provides a low-level variable-length array.

    QVarLengthArray<T, Prealloc> is a resizable array that stores its first
    \c Prealloc items inside the object itself and switches to heap memory
    only when it grows beyond that. For short-lived, small arrays, such as
    temporary buffers in a function, this avoids a heap allocation.

    \code
    QVarLengthArray<int, 16> values;
    for (int i = 0; i < 10; ++i)
        values.append(i * i);
    \endcode

    \differences
    Indexes are not range-checked except by value(). The relational operators
    compare items with \c{operator<()} only. There is no \c qHash() overload.
    \enddifferences

    \sa QList
*/
template <typename T, qsizetype Prealloc = 256>
class QVarLengthArray {
public:
    /*! \typedef QVarLengthArray::value_type

        The type of the items stored in the array, \c T. Provided for STL compatibility. */
    using value_type = T;
    /*! \typedef QVarLengthArray::iterator

        The non-const iterator type, a pointer to \c T. */
    using iterator = T *;
    /*! \typedef QVarLengthArray::const_iterator

        The const iterator type, a pointer to \c{const T}. */
    using const_iterator = const T *;
    /*! \typedef QVarLengthArray::reverse_iterator

        The non-const reverse iterator type. */
    using reverse_iterator = std::reverse_iterator<iterator>;
    /*! \typedef QVarLengthArray::const_reverse_iterator

        The const reverse iterator type. */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /*! Constructs an empty array. */
    QVarLengthArray() noexcept = default;
    /*! Constructs an array with \a size value-initialized items. */
    explicit QVarLengthArray(qsizetype size) { resize(size); }
    /*! \overload
        \since 6.4

        Constructs an array with \a size copies of \a v. */
    QVarLengthArray(qsizetype size, const T &v) { resize(size, v); }
    /*! Constructs an array from the items in the initializer list \a list. */
    QVarLengthArray(std::initializer_list<T> list) { appendRange(list.begin(), list.end()); }
    /*! Constructs an array with a copy of the items in the range [\a first, \a last). */
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QVarLengthArray(It first, It last) {
        appendRange(first, last);
    }
    /*! Constructs a copy of the array \a o. */
    QVarLengthArray(const QVarLengthArray &o) { appendRange(o.begin(), o.end()); }
    /*! Constructs an array by moving the contents of \a o, which is left empty. */
    QVarLengthArray(QVarLengthArray &&o) noexcept(std::is_nothrow_move_constructible_v<T>) { takeFrom(o); }
    /*! Destroys the array and all its items. */
    ~QVarLengthArray() { destroyAll(); }

    /*! Assigns a copy of the array \a o to this array and returns a reference to this array. */
    QVarLengthArray &operator=(const QVarLengthArray &o) {
        if (this != &o) {
            clear();
            assign(o.begin(), o.end());
        }
        return *this;
    }
    /*! \overload

        Moves the contents of \a o into this array, leaving \a o empty, and returns a reference to this array. */
    QVarLengthArray &operator=(QVarLengthArray &&o) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this != &o) {
            destroyAll();
            ptr = inlineData();
            cap = Prealloc;
            sz = 0;
            takeFrom(o);
        }
        return *this;
    }
    /*! \overload

        Replaces the contents of this array with the items in \a list and returns a reference to this array. */
    QVarLengthArray &operator=(std::initializer_list<T> list) {
        clear();
        assign(list.begin(), list.end());
        return *this;
    }

    /*!
        Returns the number of items in the array.

        \sa isEmpty(), resize()
    */
    qsizetype size() const noexcept { return sz; }
    /*! Same as size(). */
    qsizetype count() const noexcept { return sz; }
    /*! Same as size(). */
    qsizetype length() const noexcept { return sz; }
    /*!
        Returns the maximum number of items the array can hold without
        reallocating.

        \sa reserve(), squeeze()
    */
    qsizetype capacity() const noexcept { return cap; }
    /*!
        Returns \c true if the array has size 0; otherwise returns \c false.

        \sa size(), clear()
    */
    bool isEmpty() const noexcept { return sz == 0; }
    /*! Same as isEmpty(). This function is provided for STL compatibility. */
    bool empty() const noexcept { return sz == 0; }
    /*!
        Returns a pointer to the data stored in the array. The pointer stays
        valid until the array is reallocated or destroyed.

        \sa constData()
    */
    T *data() noexcept { return ptr; }
    /*! \overload

        Returns a const pointer to the data stored in the array. */
    const T *data() const noexcept { return ptr; }
    /*!
        Returns a const pointer to the data stored in the array.

        \sa data()
    */
    const T *constData() const noexcept { return ptr; }

    /*!
        Attempts to allocate memory for at least \a n items. Nothing happens
        if the capacity is already large enough.

        \sa capacity(), squeeze()
    */
    void reserve(qsizetype n) { if (n > cap) reallocate(n); }
    /*!
        Releases any memory not required to store the items. If the items fit
        in the preallocated storage, the array moves back into it.

        \sa reserve(), capacity()
    */
    void squeeze() { reallocate(qMax<qsizetype>(sz, Prealloc)); }
    /*!
        Removes all the items from the array. The capacity is unchanged.

        \sa resize()
    */
    void clear() { std::destroy(ptr, ptr + sz); sz = 0; }
    /*!
        Sets the size of the array to \a n. If \a n is larger than the
        current size, the new items are value-initialized; if it is smaller,
        items are removed from the end.

        \sa size(), reserve()
    */
    void resize(qsizetype n) {
        if (n < sz) {
            std::destroy(ptr + n, ptr + sz);
        } else {
            reserve(n);
            std::uninitialized_value_construct(ptr + sz, ptr + n);
        }
        sz = n;
    }
    /*! \overload

        Sets the size of the array to \a n. New items are copies of \a v. */
    void resize(qsizetype n, const T &v) {
        if (n < sz) {
            std::destroy(ptr + n, ptr + sz);
        } else {
            const T copy = v;
            reserve(n);
            std::uninitialized_fill(ptr + sz, ptr + n, copy);
        }
        sz = n;
    }

    /*!
        Returns a reference to the item at index position \a i, which can be
        assigned to. \a i must be a valid index.

        \sa at(), value()
    */
    T &operator[](qsizetype i) { return ptr[i]; }
    /*! \overload

        Same as at(\a i). */
    const T &operator[](qsizetype i) const { return ptr[i]; }
    /*!
        Returns the item at index position \a i. \a i must be a valid index.

        \sa value()
    */
    const T &at(qsizetype i) const { return ptr[i]; }
    /*!
        Returns a reference to the first item in the array. The array must
        not be empty.

        \sa back(), isEmpty()
    */
    T &front() { return ptr[0]; }
    /*! \overload

        Returns a const reference to the first item in the array. */
    const T &front() const { return ptr[0]; }
    /*!
        Returns a reference to the last item in the array. The array must
        not be empty.

        \sa front(), isEmpty()
    */
    T &back() { return ptr[sz - 1]; }
    /*! \overload

        Returns a const reference to the last item in the array. */
    const T &back() const { return ptr[sz - 1]; }
    /*! Same as front(). */
    T &first() { return front(); }
    /*! \overload

        Same as front() const. */
    const T &first() const { return front(); }
    /*! Same as back(). */
    T &last() { return back(); }
    /*! \overload

        Same as back() const. */
    const T &last() const { return back(); }
    /*!
        Returns the item at index position \a i, or a default-constructed
        value if \a i is out of range.

        \sa at()
    */
    T value(qsizetype i) const { return i >= 0 && i < sz ? ptr[i] : T(); }
    /*! \overload

        Returns the item at index position \a i, or \a def if \a i is out of range. */
    T value(qsizetype i, const T &def) const { return i >= 0 && i < sz ? ptr[i] : def; }

    /*!
        Appends \a v to the end of the array.

        \sa prepend(), insert(), push_back()
    */
    void append(const T &v) { emplace_back(v); }
    /*! \overload

        Moves \a v to the end of the array. */
    void append(T &&v) { emplace_back(std::move(v)); }
    /*! \overload

        Appends \a n items copied from the buffer \a buf to the end of the array. */
    void append(const T *buf, qsizetype n) {
        reserve(sz + n);
        std::uninitialized_copy(buf, buf + n, ptr + sz);
        sz += n;
    }
    /*! Same as append(\a v). This function is provided for STL compatibility. */
    void push_back(const T &v) { emplace_back(v); }
    /*! \overload

        Same as append(\a v) for an rvalue. */
    void push_back(T &&v) { emplace_back(std::move(v)); }
    /*!
        Inserts \a v at the beginning of the array.

        \sa append(), insert()
    */
    void prepend(const T &v) { insert(begin(), v); }
    /*! \overload

        Moves \a v to the beginning of the array. */
    void prepend(T &&v) { emplace(cbegin(), std::move(v)); }
    /*!
        Constructs a new item at the end of the array from \a args and
        returns a reference to it.

        \sa append()
    */
    template <typename... Args>
    T &emplace_back(Args &&...args) {
        if (sz == cap) {
            T tmp(std::forward<Args>(args)...); // an argument may live in this array
            reallocate(cap * 2);
            new (ptr + sz) T(std::move(tmp));
        } else {
            new (ptr + sz) T(std::forward<Args>(args)...);
        }
        return ptr[sz++];
    }
    /*!
        Removes the last item in the array. The array must not be empty.

        \sa removeLast(), removeFirst()
    */
    void pop_back() { std::destroy_at(ptr + --sz); }
    /*! Same as pop_back(). */
    void removeLast() { pop_back(); }

    /*!
        Inserts \a v before the item pointed to by the iterator \a before and
        returns an iterator to the inserted item. The iterator type may be
        iterator or const_iterator.

        \sa append(), prepend(), erase()
    */
    template <typename It, typename = std::enable_if_t<std::is_same_v<It, iterator> || std::is_same_v<It, const_iterator>>>
    iterator insert(It before, const T &v) {
        const qsizetype pos = before - ptr;
        T copy = v;
        emplace_back(std::move(copy));
        std::rotate(ptr + pos, ptr + sz - 1, ptr + sz);
        return ptr + pos;
    }
    /*! \overload

        Inserts \a v at index position \a i. \a i must be between 0 and size(). */
    void insert(qsizetype i, const T &v) { insert(static_cast<const_iterator>(ptr + i), v); }
    /*! \overload

        Moves \a v before the item pointed to by \a before and returns an iterator to the inserted item. */
    template <typename It,
              typename = std::enable_if_t<std::is_same_v<It, iterator> || std::is_same_v<It, const_iterator>>>
    iterator insert(It before, T &&v) {
        return emplace(before, std::move(v));
    }
    /*! \overload

        Moves \a v to index position \a i. \a i must be between 0 and size(). */
    void insert(qsizetype i, T &&v) { emplace(static_cast<const_iterator>(ptr + i), std::move(v)); }
    /*! \overload

        Inserts \a n copies of \a v before the item pointed to by \a before and returns an iterator to the first inserted item. */
    template <typename It,
              typename = std::enable_if_t<std::is_same_v<It, iterator> || std::is_same_v<It, const_iterator>>>
    iterator insert(It before, qsizetype n, const T &v) {
        const qsizetype i = before - ptr;
        insert(i, n, v);
        return ptr + i;
    }
    /*! \overload

        Inserts \a n copies of \a v at index position \a i. \a i must be between 0 and size(). */
    void insert(qsizetype i, qsizetype n, const T &v) {
        const T copy = v;
        const qsizetype old = sz;
        reserve(sz + n);
        std::uninitialized_fill(ptr + sz, ptr + sz + n, copy);
        sz += n;
        std::rotate(ptr + i, ptr + old, ptr + sz);
    }
    /*!
        Replaces the item at index position \a i with \a v. \a i must be a
        valid index.

        \sa insert(), remove()
    */
    void replace(qsizetype i, const T &v) { ptr[i] = v; }
    /*!
        Assigns \a v to every item in the array. If \a newSize is not
        negative, the array is first resized to \a newSize items.

        \sa resize()
    */
    void fill(const T &v, qsizetype newSize = -1) {
        if (newSize >= 0)
            resize(newSize);
        std::fill(ptr, ptr + sz, v);
    }
    /*!
        Removes the item at index position \a i. \a i must be a valid index.

        \sa remove(), removeFirst(), removeLast()
    */
    void removeAt(qsizetype i) { remove(i, 1); }
    /*!
        Removes the first item in the array. The array must not be empty.

        \sa removeLast(), removeAt()
    */
    void removeFirst() { remove(0, 1); }
    /*!
        \since 6.1
        Removes all items that compare equal to \a t and returns the number
        of items removed.

        \sa removeOne(), removeIf()
    */
    template <typename AT = T> qsizetype removeAll(const AT &t) {
        return removeIf([&t](const T &v) { return v == t; });
    }
    /*!
        \since 6.1
        Removes the first item that compares equal to \a t. Returns \c true
        if an item was removed; otherwise returns \c false.

        \sa removeAll()
    */
    template <typename AT = T> bool removeOne(const AT &t) {
        const qsizetype i = indexOf(t);
        if (i < 0)
            return false;
        removeAt(i);
        return true;
    }
    /*!
        \since 6.1
        Removes all items for which the predicate \a pred returns \c true
        and returns the number of items removed.

        \sa removeAll()
    */
    template <typename Predicate> qsizetype removeIf(Predicate pred) {
        T *const newEnd = std::remove_if(ptr, ptr + sz, pred);
        const qsizetype removed = (ptr + sz) - newEnd;
        std::destroy(newEnd, ptr + sz);
        sz -= removed;
        return removed;
    }
    /*!
        \since 6.6
        Replaces the contents of the array with \a n copies of \a t and
        returns a reference to the array.

        \sa fill()
    */
    QVarLengthArray &assign(qsizetype n, const T &t) {
        const T copy = t; // t may live in this array
        clear();
        reserve(n);
        std::uninitialized_fill(ptr, ptr + n, copy);
        sz = n;
        return *this;
    }
    /*! \overload
        \since 6.6

        Replaces the contents of the array with a copy of the items in the range [\a first, \a last) and returns a reference to the array. The range must not refer to this array. */
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QVarLengthArray &assign(It first, It last) {
        clear();
        appendRange(first, last);
        return *this;
    }
    /*! \overload
        \since 6.6

        Replaces the contents of the array with the items in \a list and returns a reference to the array. */
    QVarLengthArray &assign(std::initializer_list<T> list) { return assign(list.begin(), list.end()); }
    /*!
        Returns the largest number of items the array can theoretically hold.
        This function is provided for STL compatibility.
    */
    static constexpr qsizetype max_size() noexcept {
        return static_cast<qsizetype>(std::numeric_limits<qsizetype>::max() / static_cast<qsizetype>(sizeof(T)));
    }
    /*! Same as squeeze(). This function is provided for STL compatibility. */
    void shrink_to_fit() { squeeze(); }
    /*!
        Constructs a new item from \a args and inserts it before the item
        pointed to by \a before. Returns an iterator to the new item.

        \sa insert(), emplace_back()
    */
    template <typename... Args>
    iterator emplace(const_iterator before, Args &&...args) {
        const qsizetype pos = before - ptr;
        emplace_back(std::forward<Args>(args)...);
        std::rotate(ptr + pos, ptr + sz - 1, ptr + sz);
        return ptr + pos;
    }
    /*! Appends \a v to the array and returns a reference to the array. */
    QVarLengthArray &operator+=(const T &v) { append(v); return *this; }
    /*! \overload

        Moves \a v to the end of the array and returns a reference to the array. */
    QVarLengthArray &operator+=(T &&v) {
        append(std::move(v));
        return *this;
    }
    /*! Appends \a v to the array and returns a reference to the array, so that calls can be chained. */
    QVarLengthArray &operator<<(const T &v) { append(v); return *this; }
    /*! \overload

        Moves \a v to the end of the array and returns a reference to the array, so that calls can be chained. */
    QVarLengthArray &operator<<(T &&v) {
        append(std::move(v));
        return *this;
    }
    /*! \overload

        Appends the items in the initializer list \a list to the array. */
    void append(std::initializer_list<T> list) { for (const T &v : list) append(v); }
    /*!
        Removes the items in the range [\a first, \a last) and returns an
        iterator to the item that followed the range.

        \sa remove(), insert()
    */
    iterator erase(const_iterator first, const_iterator last) {
        const qsizetype pos = first - ptr, n = last - first;
        std::move(ptr + pos + n, ptr + sz, ptr + pos);
        std::destroy(ptr + sz - n, ptr + sz);
        sz -= n;
        return ptr + pos;
    }
    /*! \overload

        Removes the item pointed to by \a pos and returns an iterator to the item that followed it. */
    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
    /*!
        Removes \a n items starting at index position \a i. The range must be
        valid.

        \sa removeAt(), erase()
    */
    void remove(qsizetype i, qsizetype n = 1) { erase(ptr + i, ptr + i + n); }

    /*!
        Returns the index position of the first item equal to \a v, searching
        forward from index position \a from. Returns -1 if no item matches.
        The comparison uses \c{operator==()} between the item type and \c U.

        \sa lastIndexOf(), contains()
    */
    template <typename U>
    qsizetype indexOf(const U &v, qsizetype from = 0) const {
        for (qsizetype i = qMax<qsizetype>(from, 0); i < sz; ++i)
            if (ptr[i] == v)
                return i;
        return -1;
    }
    /*!
        Returns the index position of the last item equal to \a v, searching
        backward from index position \a from. A negative \a from starts at
        the last item. Returns -1 if no item matches.

        \sa indexOf(), contains()
    */
    template <typename U>
    qsizetype lastIndexOf(const U &v, qsizetype from = -1) const {
        for (qsizetype i = from < 0 ? sz - 1 : qMin(from, sz - 1); i >= 0; --i)
            if (ptr[i] == v)
                return i;
        return -1;
    }
    /*!
        Returns \c true if the array contains an item equal to \a v;
        otherwise returns \c false.

        \sa indexOf()
    */
    template <typename U>
    bool contains(const U &v) const { return indexOf(v) != -1; }

    /*! Returns an iterator pointing to the first item in the array. */
    iterator begin() noexcept { return ptr; }
    /*! Returns an iterator pointing just after the last item in the array. */
    iterator end() noexcept { return ptr + sz; }
    /*! \overload

        Returns a const iterator pointing to the first item in the array. */
    const_iterator begin() const noexcept { return ptr; }
    /*! \overload

        Returns a const iterator pointing just after the last item in the array. */
    const_iterator end() const noexcept { return ptr + sz; }
    /*! Returns a const iterator pointing to the first item in the array. */
    const_iterator cbegin() const noexcept { return ptr; }
    /*! Returns a const iterator pointing just after the last item in the array. */
    const_iterator cend() const noexcept { return ptr + sz; }
    /*! Same as cbegin(). */
    const_iterator constBegin() const noexcept { return ptr; }
    /*! Same as cend(). */
    const_iterator constEnd() const noexcept { return ptr + sz; }
    /*! Returns a reverse iterator pointing to the last item in the array. */
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    /*! Returns a reverse iterator pointing just before the first item in the array. */
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    /*! \overload

        Returns a const reverse iterator pointing to the last item in the array. */
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    /*! \overload

        Returns a const reverse iterator pointing just before the first item in the array. */
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    /*! Same as rbegin() const. */
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    /*! Same as rend() const. */
    const_reverse_iterator crend() const noexcept { return rend(); }

    friend bool operator==(const QVarLengthArray &a, const QVarLengthArray &b) {
        return a.sz == b.sz && std::equal(a.begin(), a.end(), b.begin());
    }
    friend bool operator!=(const QVarLengthArray &a, const QVarLengthArray &b) { return !(a == b); }
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator<(const QVarLengthArray &a, const QVarLengthArray &b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
    }
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator>(const QVarLengthArray &a, const QVarLengthArray &b) {
        return b < a;
    }
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator<=(const QVarLengthArray &a, const QVarLengthArray &b) {
        return !(b < a);
    }
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator>=(const QVarLengthArray &a, const QVarLengthArray &b) {
        return !(a < b);
    }

private:
    T *inlineData() noexcept { return reinterpret_cast<T *>(storage); }

    template <typename It> void appendRange(It first, It last) {
        for (; first != last; ++first)
            emplace_back(*first);
    }
    void reallocate(qsizetype n) {
        n = qMax<qsizetype>(n, 1);
        T *fresh = n <= Prealloc ? inlineData() : static_cast<T *>(::operator new(sizeof(T) * static_cast<std::size_t>(n), std::align_val_t(alignof(T))));
        if (fresh != ptr) {
            std::uninitialized_move(ptr, ptr + sz, fresh);
            std::destroy(ptr, ptr + sz);
            release();
            ptr = fresh;
        }
        cap = n <= Prealloc ? Prealloc : n;
    }
    void release() {
        if (ptr != inlineData())
            ::operator delete(ptr, std::align_val_t(alignof(T)));
    }
    void destroyAll() { std::destroy(ptr, ptr + sz); release(); }
    void takeFrom(QVarLengthArray &o) {
        if (o.ptr != o.inlineData()) {
            ptr = o.ptr;
            cap = o.cap;
            sz = o.sz;
            o.ptr = o.inlineData();
            o.cap = Prealloc;
            o.sz = 0;
        } else {
            std::uninitialized_move(o.ptr, o.ptr + o.sz, ptr);
            sz = o.sz;
            o.clear();
        }
    }

    alignas(T) unsigned char storage[sizeof(T) * static_cast<std::size_t>(Prealloc)];
    T *ptr = inlineData();
    qsizetype cap = Prealloc;
    qsizetype sz = 0;
};
