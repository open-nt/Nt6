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
    The \c is...Native() and \c is...WaitFree() queries report whether
    \c{std::atomic<T>} is always lock-free, rather than the processor's
    instructions.
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
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using relaxed memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetRelaxed(T expected, T desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with relaxed memory ordering.
    */
    bool testAndSetRelaxed(T expected, T desired, T &currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using relaxed memory ordering,
        and returns the previous value.
    */
    T fetchAndStoreRelaxed(T v) noexcept { return a.exchange(v, std::memory_order_relaxed); }
    /*!
        Atomically adds \a v to the value, using relaxed memory ordering, and returns
        the previous value.
    */
    T fetchAndAddRelaxed(T v) noexcept { return a.fetch_add(v, std::memory_order_relaxed); }
    /*!
        Atomically subtracts \a v from the value, using relaxed memory ordering, and returns
        the previous value.
    */
    T fetchAndSubRelaxed(T v) noexcept { return a.fetch_sub(v, std::memory_order_relaxed); }
    /*!
        Atomically performs a bitwise AND of the value with \a v, using relaxed memory ordering, and returns
        the previous value.
    */
    T fetchAndAndRelaxed(T v) noexcept { return a.fetch_and(v, std::memory_order_relaxed); }
    /*!
        Atomically performs a bitwise OR of the value with \a v, using relaxed memory ordering, and returns
        the previous value.
    */
    T fetchAndOrRelaxed(T v) noexcept { return a.fetch_or(v, std::memory_order_relaxed); }
    /*!
        Atomically performs a bitwise XOR of the value with \a v, using relaxed memory ordering, and returns
        the previous value.
    */
    T fetchAndXorRelaxed(T v) noexcept { return a.fetch_xor(v, std::memory_order_relaxed); }
    /*!
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using acquire memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetAcquire(T expected, T desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_acquire, std::memory_order_acquire);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with acquire memory ordering.
    */
    bool testAndSetAcquire(T expected, T desired, T &currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_acquire, std::memory_order_acquire);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using acquire memory ordering,
        and returns the previous value.
    */
    T fetchAndStoreAcquire(T v) noexcept { return a.exchange(v, std::memory_order_acquire); }
    /*!
        Atomically adds \a v to the value, using acquire memory ordering, and returns
        the previous value.
    */
    T fetchAndAddAcquire(T v) noexcept { return a.fetch_add(v, std::memory_order_acquire); }
    /*!
        Atomically subtracts \a v from the value, using acquire memory ordering, and returns
        the previous value.
    */
    T fetchAndSubAcquire(T v) noexcept { return a.fetch_sub(v, std::memory_order_acquire); }
    /*!
        Atomically performs a bitwise AND of the value with \a v, using acquire memory ordering, and returns
        the previous value.
    */
    T fetchAndAndAcquire(T v) noexcept { return a.fetch_and(v, std::memory_order_acquire); }
    /*!
        Atomically performs a bitwise OR of the value with \a v, using acquire memory ordering, and returns
        the previous value.
    */
    T fetchAndOrAcquire(T v) noexcept { return a.fetch_or(v, std::memory_order_acquire); }
    /*!
        Atomically performs a bitwise XOR of the value with \a v, using acquire memory ordering, and returns
        the previous value.
    */
    T fetchAndXorAcquire(T v) noexcept { return a.fetch_xor(v, std::memory_order_acquire); }
    /*!
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using release memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetRelease(T expected, T desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with release memory ordering.
    */
    bool testAndSetRelease(T expected, T desired, T &currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using release memory ordering,
        and returns the previous value.
    */
    T fetchAndStoreRelease(T v) noexcept { return a.exchange(v, std::memory_order_release); }
    /*!
        Atomically adds \a v to the value, using release memory ordering, and returns
        the previous value.
    */
    T fetchAndAddRelease(T v) noexcept { return a.fetch_add(v, std::memory_order_release); }
    /*!
        Atomically subtracts \a v from the value, using release memory ordering, and returns
        the previous value.
    */
    T fetchAndSubRelease(T v) noexcept { return a.fetch_sub(v, std::memory_order_release); }
    /*!
        Atomically performs a bitwise AND of the value with \a v, using release memory ordering, and returns
        the previous value.
    */
    T fetchAndAndRelease(T v) noexcept { return a.fetch_and(v, std::memory_order_release); }
    /*!
        Atomically performs a bitwise OR of the value with \a v, using release memory ordering, and returns
        the previous value.
    */
    T fetchAndOrRelease(T v) noexcept { return a.fetch_or(v, std::memory_order_release); }
    /*!
        Atomically performs a bitwise XOR of the value with \a v, using release memory ordering, and returns
        the previous value.
    */
    T fetchAndXorRelease(T v) noexcept { return a.fetch_xor(v, std::memory_order_release); }
    /*!
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using sequentially consistent memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetOrdered(T expected, T desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with sequentially consistent memory ordering.
    */
    bool testAndSetOrdered(T expected, T desired, T &currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using sequentially consistent memory ordering,
        and returns the previous value.
    */
    T fetchAndStoreOrdered(T v) noexcept { return a.exchange(v, std::memory_order_seq_cst); }
    /*!
        Atomically adds \a v to the value, using sequentially consistent memory ordering, and returns
        the previous value.
    */
    T fetchAndAddOrdered(T v) noexcept { return a.fetch_add(v, std::memory_order_seq_cst); }
    /*!
        Atomically subtracts \a v from the value, using sequentially consistent memory ordering, and returns
        the previous value.
    */
    T fetchAndSubOrdered(T v) noexcept { return a.fetch_sub(v, std::memory_order_seq_cst); }
    /*!
        Atomically performs a bitwise AND of the value with \a v, using sequentially consistent memory ordering, and returns
        the previous value.
    */
    T fetchAndAndOrdered(T v) noexcept { return a.fetch_and(v, std::memory_order_seq_cst); }
    /*!
        Atomically performs a bitwise OR of the value with \a v, using sequentially consistent memory ordering, and returns
        the previous value.
    */
    T fetchAndOrOrdered(T v) noexcept { return a.fetch_or(v, std::memory_order_seq_cst); }
    /*!
        Atomically performs a bitwise XOR of the value with \a v, using sequentially consistent memory ordering, and returns
        the previous value.
    */
    T fetchAndXorOrdered(T v) noexcept { return a.fetch_xor(v, std::memory_order_seq_cst); }
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
        Atomically performs a bitwise AND with \a v and returns the new value.
    */
    T operator&=(T v) noexcept { return a.fetch_and(v) & v; }
    /*!
        Atomically performs a bitwise OR with \a v and returns the new value.
    */
    T operator|=(T v) noexcept { return a.fetch_or(v) | v; }
    /*!
        Atomically performs a bitwise XOR with \a v and returns the new value.
    */
    T operator^=(T v) noexcept { return a.fetch_xor(v) ^ v; }

    /*!
        Returns \c true; reference counting with ref() and deref() is always
        natively supported.
    */
    static constexpr bool isReferenceCountingNative() noexcept { return true; }
    /*!
        Returns \c true if the operations on this type are lock-free
        (\c{std::atomic<T>::is_always_lock_free}).
    */
    static constexpr bool isReferenceCountingWaitFree() noexcept { return std::atomic<T>::is_always_lock_free; }
    /*!
        Returns \c true if the operations on this type are lock-free
        (\c{std::atomic<T>::is_always_lock_free}).
    */
    static constexpr bool isTestAndSetNative() noexcept { return std::atomic<T>::is_always_lock_free; }
    /*!
        Returns \c true if the operations on this type are lock-free
        (\c{std::atomic<T>::is_always_lock_free}).
    */
    static constexpr bool isTestAndSetWaitFree() noexcept { return std::atomic<T>::is_always_lock_free; }
    /*!
        Returns \c true if the operations on this type are lock-free
        (\c{std::atomic<T>::is_always_lock_free}).
    */
    static constexpr bool isFetchAndStoreNative() noexcept { return std::atomic<T>::is_always_lock_free; }
    /*!
        Returns \c true if the operations on this type are lock-free
        (\c{std::atomic<T>::is_always_lock_free}).
    */
    static constexpr bool isFetchAndStoreWaitFree() noexcept { return std::atomic<T>::is_always_lock_free; }
    /*!
        Returns \c true if the operations on this type are lock-free
        (\c{std::atomic<T>::is_always_lock_free}).
    */
    static constexpr bool isFetchAndAddNative() noexcept { return std::atomic<T>::is_always_lock_free; }
    /*!
        Returns \c true if the operations on this type are lock-free
        (\c{std::atomic<T>::is_always_lock_free}).
    */
    static constexpr bool isFetchAndAddWaitFree() noexcept { return std::atomic<T>::is_always_lock_free; }

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
    There are no \c is...Native() or \c is...WaitFree() queries.
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
        Constructs a copy of \a other, loading its pointer atomically.
    */
    QAtomicPointer(const QAtomicPointer &other) noexcept : a(other.loadRelaxed()) {}
    /*!
        Atomically stores the pointer of \a other and returns a reference to
        this object.
    */
    QAtomicPointer &operator=(const QAtomicPointer &other) noexcept {
        a.store(other.loadRelaxed());
        return *this;
    }
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
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using relaxed memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetRelaxed(T *expected, T *desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with relaxed memory ordering.
    */
    bool testAndSetRelaxed(T *expected, T *desired, T *&currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using relaxed memory ordering,
        and returns the previous value.
    */
    T *fetchAndStoreRelaxed(T *v) noexcept { return a.exchange(v, std::memory_order_relaxed); }
    /*!
        Atomically adds \a v to the pointer, counted in elements of \c T,, using relaxed memory ordering, and returns
        the previous value.
    */
    T *fetchAndAddRelaxed(qptrdiff v) noexcept { return a.fetch_add(v, std::memory_order_relaxed); }
    /*!
        Atomically subtracts \a v from the pointer, counted in elements of \c T,, using relaxed memory ordering, and returns
        the previous value.
    */
    T *fetchAndSubRelaxed(qptrdiff v) noexcept { return a.fetch_sub(v, std::memory_order_relaxed); }
    /*!
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using acquire memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetAcquire(T *expected, T *desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_acquire, std::memory_order_acquire);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with acquire memory ordering.
    */
    bool testAndSetAcquire(T *expected, T *desired, T *&currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_acquire, std::memory_order_acquire);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using acquire memory ordering,
        and returns the previous value.
    */
    T *fetchAndStoreAcquire(T *v) noexcept { return a.exchange(v, std::memory_order_acquire); }
    /*!
        Atomically adds \a v to the pointer, counted in elements of \c T,, using acquire memory ordering, and returns
        the previous value.
    */
    T *fetchAndAddAcquire(qptrdiff v) noexcept { return a.fetch_add(v, std::memory_order_acquire); }
    /*!
        Atomically subtracts \a v from the pointer, counted in elements of \c T,, using acquire memory ordering, and returns
        the previous value.
    */
    T *fetchAndSubAcquire(qptrdiff v) noexcept { return a.fetch_sub(v, std::memory_order_acquire); }
    /*!
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using release memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetRelease(T *expected, T *desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with release memory ordering.
    */
    bool testAndSetRelease(T *expected, T *desired, T *&currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using release memory ordering,
        and returns the previous value.
    */
    T *fetchAndStoreRelease(T *v) noexcept { return a.exchange(v, std::memory_order_release); }
    /*!
        Atomically adds \a v to the pointer, counted in elements of \c T,, using release memory ordering, and returns
        the previous value.
    */
    T *fetchAndAddRelease(qptrdiff v) noexcept { return a.fetch_add(v, std::memory_order_release); }
    /*!
        Atomically subtracts \a v from the pointer, counted in elements of \c T,, using release memory ordering, and returns
        the previous value.
    */
    T *fetchAndSubRelease(qptrdiff v) noexcept { return a.fetch_sub(v, std::memory_order_release); }
    /*!
        Atomically compares the value with \a expected and, if they are
        equal, replaces it with \a desired, using sequentially consistent memory ordering.
        Returns \c true if the value was replaced; otherwise returns \c false.
    */
    bool testAndSetOrdered(T *expected, T *desired) noexcept {
        return a.compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
    }
    /*!
        \overload
        Like the other overload, and stores the value found in \a currentValue
        whether or not it was replaced, comparing with \a expected and
        storing \a desired with sequentially consistent memory ordering.
    */
    bool testAndSetOrdered(T *expected, T *desired, T *&currentValue) noexcept {
        const bool replaced =
            a.compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
        currentValue = expected;
        return replaced;
    }
    /*!
        Atomically replaces the value with \a v, using sequentially consistent memory ordering,
        and returns the previous value.
    */
    T *fetchAndStoreOrdered(T *v) noexcept { return a.exchange(v, std::memory_order_seq_cst); }
    /*!
        Atomically adds \a v to the pointer, counted in elements of \c T,, using sequentially consistent memory ordering, and returns
        the previous value.
    */
    T *fetchAndAddOrdered(qptrdiff v) noexcept { return a.fetch_add(v, std::memory_order_seq_cst); }
    /*!
        Atomically subtracts \a v from the pointer, counted in elements of \c T,, using sequentially consistent memory ordering, and returns
        the previous value.
    */
    T *fetchAndSubOrdered(qptrdiff v) noexcept { return a.fetch_sub(v, std::memory_order_seq_cst); }
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
