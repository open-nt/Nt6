// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <atomic>

/*!
    \class QAtomicInteger
    \brief The QAtomicInteger class provides platform-independent atomic operations on integers.

    QAtomicInteger<T> wraps a \c{std::atomic<T>} and offers the Qt-style
    load, store, compare-and-swap and fetch-and-modify operations. Functions
    with \e Ordered in their name use sequentially consistent ordering; those
    with \e Relaxed, \e Acquire or \e Release use the corresponding memory
    order. Plain \c load(), \c store(), the conversion operator and the
    arithmetic operators are sequentially consistent.

    QAtomicInt is the common instantiation for \c int, and is typically used
    as a thread-safe reference count:

    \code
    QAtomicInt refCount(1);

    void retain() { refCount.ref(); }
    void release() { if (!refCount.deref()) delete this; }
    \endcode

    \differences
    Only the Ordered variants of most operations are provided. There are no
    \c fetchAndStoreRelaxed(), \c fetchAndStoreAcquire(),
    \c fetchAndStoreRelease(), \c testAndSetAcquire(), \c testAndSetRelease(),
    \c fetchAndSubRelaxed() or similar relaxed/acquire/release forms beyond
    those documented here, no \c testAndSet overloads that report the current
    value, and no \c is...Native() queries other than
    isReferenceCountingNative().
    \enddifferences

    \sa QAtomicPointer
*/
template <typename T>
class QAtomicInteger {
public:
    /*!
        Constructs an atomic integer holding \a v.
    */
    constexpr QAtomicInteger(T v = 0) noexcept : a(v) {}
    /*!
        Constructs a copy of \a o. The value is read with relaxed ordering; the
        copy is not atomic with respect to other threads.
    */
    QAtomicInteger(const QAtomicInteger &o) noexcept : a(o.loadRelaxed()) {}
    /*!
        Assigns the value of \a o to this integer, reading it with relaxed
        ordering, and returns a reference to this object.
    */
    QAtomicInteger &operator=(const QAtomicInteger &o) noexcept { a.store(o.loadRelaxed()); return *this; }
    /*!
        Atomically stores \a v and returns a reference to this object.
    */
    QAtomicInteger &operator=(T v) noexcept { a.store(v); return *this; }

    /*!
        Atomically loads and returns the value with relaxed memory ordering.

        \sa loadAcquire(), load()
    */
    T loadRelaxed() const noexcept { return a.load(std::memory_order_relaxed); }
    /*!
        Atomically loads and returns the value with acquire memory ordering.

        \sa loadRelaxed(), load()
    */
    T loadAcquire() const noexcept { return a.load(std::memory_order_acquire); }
    /*!
        Atomically loads and returns the value with sequentially consistent
        ordering.

        \sa loadRelaxed(), loadAcquire()
    */
    T load() const noexcept { return a.load(); }
    /*!
        Atomically stores \a v with relaxed memory ordering.

        \sa storeRelease(), store()
    */
    void storeRelaxed(T v) noexcept { a.store(v, std::memory_order_relaxed); }
    /*!
        Atomically stores \a v with release memory ordering.

        \sa storeRelaxed(), store()
    */
    void storeRelease(T v) noexcept { a.store(v, std::memory_order_release); }
    /*!
        Atomically stores \a v with sequentially consistent ordering.

        \sa storeRelaxed(), storeRelease()
    */
    void store(T v) noexcept { a.store(v); }
    /*!
        Atomically loads and returns the value, as load() does.
    */
    operator T() const noexcept { return a.load(); }

    /*!
        Atomically increments the value by one. Returns \c true if the new
        value is not zero; otherwise returns \c false.

        \sa deref()
    */
    bool ref() noexcept { return a.fetch_add(1) + 1 != 0; }
    /*!
        Atomically decrements the value by one. Returns \c true if the new
        value is not zero; otherwise returns \c false.

        \sa ref()
    */
    bool deref() noexcept { return a.fetch_sub(1) - 1 != 0; }
    /*!
        Atomically compares the value with \a expected and, if they are equal,
        replaces it with \a desired, using sequentially consistent ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.

        \sa testAndSetRelaxed()
    */
    bool testAndSetOrdered(T expected, T desired) noexcept { return a.compare_exchange_strong(expected, desired); }
    /*!
        Atomically compares the value with \a expected and, if they are equal,
        replaces it with \a desired, using relaxed ordering. Returns \c true if
        the value was replaced; otherwise returns \c false.

        \sa testAndSetOrdered()
    */
    bool testAndSetRelaxed(T expected, T desired) noexcept { return a.compare_exchange_strong(expected, desired, std::memory_order_relaxed); }
    /*!
        Atomically replaces the value with \a v and returns the previous value.
    */
    T fetchAndStoreOrdered(T v) noexcept { return a.exchange(v); }
    /*!
        Atomically adds \a v to the value and returns the previous value.

        \sa fetchAndAddRelaxed(), fetchAndSubOrdered()
    */
    T fetchAndAddOrdered(T v) noexcept { return a.fetch_add(v); }
    /*!
        Atomically adds \a v to the value with relaxed ordering and returns the
        previous value.

        \sa fetchAndAddOrdered()
    */
    T fetchAndAddRelaxed(T v) noexcept { return a.fetch_add(v, std::memory_order_relaxed); }
    /*!
        Atomically subtracts \a v from the value and returns the previous
        value.

        \sa fetchAndAddOrdered()
    */
    T fetchAndSubOrdered(T v) noexcept { return a.fetch_sub(v); }
    /*!
        Atomically performs a bitwise AND of the value with \a v and returns
        the previous value.
    */
    T fetchAndAndOrdered(T v) noexcept { return a.fetch_and(v); }
    /*!
        Atomically performs a bitwise OR of the value with \a v and returns the
        previous value.
    */
    T fetchAndOrOrdered(T v) noexcept { return a.fetch_or(v); }
    /*!
        Atomically performs a bitwise XOR of the value with \a v and returns
        the previous value.
    */
    T fetchAndXorOrdered(T v) noexcept { return a.fetch_xor(v); }
    /*!
        Atomically increments the value and returns the new value (prefix
        form).
    */
    T operator++() noexcept { return a.fetch_add(1) + 1; }
    /*!
        \overload

        Atomically increments the value and returns the previous value
        (postfix form).
    */
    T operator++(int) noexcept { return a.fetch_add(1); }
    /*!
        Atomically decrements the value and returns the new value (prefix
        form).
    */
    T operator--() noexcept { return a.fetch_sub(1) - 1; }
    /*!
        \overload

        Atomically decrements the value and returns the previous value
        (postfix form).
    */
    T operator--(int) noexcept { return a.fetch_sub(1); }
    /*!
        Atomically adds \a v to the value and returns the new value.
    */
    T operator+=(T v) noexcept { return a.fetch_add(v) + v; }
    /*!
        Atomically subtracts \a v from the value and returns the new value.
    */
    T operator-=(T v) noexcept { return a.fetch_sub(v) - v; }

    /*!
        Returns \c true; reference counting with ref() and deref() is always
        natively supported.
    */
    static constexpr bool isReferenceCountingNative() noexcept { return true; }

private:
    std::atomic<T> a;
};

using QAtomicInt = QAtomicInteger<int>;

/*!
    \class QAtomicPointer
    \brief The QAtomicPointer class is a template class that provides platform-independent atomic operations on pointers.

    QAtomicPointer<T> wraps a \c{std::atomic<T *>}. It is used for lock-free
    publication of pointers between threads:

    \code
    QAtomicPointer<Config> current;

    void publish(Config *c) { current.storeRelease(c); }
    Config *get() { return current.loadAcquire(); }
    \endcode

    \differences
    Only a subset of the Qt operations is provided: there are no
    \c fetchAndStoreRelaxed(), \c fetchAndAddRelaxed(), \c testAndSetRelaxed(),
    \c testAndSetAcquire() or \c testAndSetRelease() functions, no
    \c testAndSet overloads that report the current value, and no copy
    constructor or copy assignment.
    \enddifferences

    \sa QAtomicInteger
*/
template <typename T>
class QAtomicPointer {
public:
    /*!
        Constructs an atomic pointer holding \a p.
    */
    constexpr QAtomicPointer(T *p = nullptr) noexcept : a(p) {}
    /*!
        Atomically loads and returns the pointer with relaxed memory ordering.

        \sa loadAcquire(), load()
    */
    T *loadRelaxed() const noexcept { return a.load(std::memory_order_relaxed); }
    /*!
        Atomically loads and returns the pointer with acquire memory ordering.

        \sa loadRelaxed(), load()
    */
    T *loadAcquire() const noexcept { return a.load(std::memory_order_acquire); }
    /*!
        Atomically stores \a p with relaxed memory ordering.

        \sa storeRelease(), store()
    */
    void storeRelaxed(T *p) noexcept { a.store(p, std::memory_order_relaxed); }
    /*!
        Atomically stores \a p with release memory ordering.

        \sa storeRelaxed(), store()
    */
    void storeRelease(T *p) noexcept { a.store(p, std::memory_order_release); }
    /*!
        Atomically loads and returns the pointer with sequentially consistent
        ordering.

        \sa loadRelaxed(), loadAcquire()
    */
    T *load() const noexcept { return a.load(); }
    /*!
        Atomically stores \a p with sequentially consistent ordering.

        \sa storeRelaxed(), storeRelease()
    */
    void store(T *p) noexcept { a.store(p); }
    /*!
        Atomically compares the pointer with \a expected and, if they are
        equal, replaces it with \a desired. Returns \c true if the pointer was
        replaced; otherwise returns \c false.
    */
    bool testAndSetOrdered(T *expected, T *desired) noexcept { return a.compare_exchange_strong(expected, desired); }
    /*!
        Atomically replaces the pointer with \a p and returns the previous
        pointer.
    */
    T *fetchAndStoreOrdered(T *p) noexcept { return a.exchange(p); }
    /*!
        Atomically advances the pointer by \a n elements of type \c T and
        returns the previous pointer.
    */
    T *fetchAndAddOrdered(qptrdiff n) noexcept { return a.fetch_add(n); }
    /*!
        Atomically loads and returns the pointer, as load() does.
    */
    operator T *() const noexcept { return a.load(); }
    /*!
        Atomically stores \a p and returns a reference to this object.
    */
    QAtomicPointer &operator=(T *p) noexcept { a.store(p); return *this; }

private:
    std::atomic<T *> a;
};
