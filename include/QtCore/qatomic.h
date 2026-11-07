// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <atomic>

template <typename T>
class QAtomicInteger {
public:
    constexpr QAtomicInteger(T v = 0) noexcept : a(v) {}
    QAtomicInteger(const QAtomicInteger &o) noexcept : a(o.loadRelaxed()) {}
    QAtomicInteger &operator=(const QAtomicInteger &o) noexcept { a.store(o.loadRelaxed()); return *this; }
    QAtomicInteger &operator=(T v) noexcept { a.store(v); return *this; }

    T loadRelaxed() const noexcept { return a.load(std::memory_order_relaxed); }
    T loadAcquire() const noexcept { return a.load(std::memory_order_acquire); }
    T load() const noexcept { return a.load(); }
    void storeRelaxed(T v) noexcept { a.store(v, std::memory_order_relaxed); }
    void storeRelease(T v) noexcept { a.store(v, std::memory_order_release); }
    void store(T v) noexcept { a.store(v); }
    operator T() const noexcept { return a.load(); }

    bool ref() noexcept { return a.fetch_add(1) + 1 != 0; }
    bool deref() noexcept { return a.fetch_sub(1) - 1 != 0; }
    bool testAndSetOrdered(T expected, T desired) noexcept { return a.compare_exchange_strong(expected, desired); }
    bool testAndSetRelaxed(T expected, T desired) noexcept { return a.compare_exchange_strong(expected, desired, std::memory_order_relaxed); }
    T fetchAndStoreOrdered(T v) noexcept { return a.exchange(v); }
    T fetchAndAddOrdered(T v) noexcept { return a.fetch_add(v); }
    T fetchAndAddRelaxed(T v) noexcept { return a.fetch_add(v, std::memory_order_relaxed); }
    T fetchAndSubOrdered(T v) noexcept { return a.fetch_sub(v); }
    T fetchAndAndOrdered(T v) noexcept { return a.fetch_and(v); }
    T fetchAndOrOrdered(T v) noexcept { return a.fetch_or(v); }
    T fetchAndXorOrdered(T v) noexcept { return a.fetch_xor(v); }
    T operator++() noexcept { return a.fetch_add(1) + 1; }
    T operator++(int) noexcept { return a.fetch_add(1); }
    T operator--() noexcept { return a.fetch_sub(1) - 1; }
    T operator--(int) noexcept { return a.fetch_sub(1); }
    T operator+=(T v) noexcept { return a.fetch_add(v) + v; }
    T operator-=(T v) noexcept { return a.fetch_sub(v) - v; }

    static constexpr bool isReferenceCountingNative() noexcept { return true; }

private:
    std::atomic<T> a;
};

using QAtomicInt = QAtomicInteger<int>;

template <typename T>
class QAtomicPointer {
public:
    constexpr QAtomicPointer(T *p = nullptr) noexcept : a(p) {}
    T *loadRelaxed() const noexcept { return a.load(std::memory_order_relaxed); }
    T *loadAcquire() const noexcept { return a.load(std::memory_order_acquire); }
    void storeRelaxed(T *p) noexcept { a.store(p, std::memory_order_relaxed); }
    void storeRelease(T *p) noexcept { a.store(p, std::memory_order_release); }
    T *load() const noexcept { return a.load(); }
    void store(T *p) noexcept { a.store(p); }
    bool testAndSetOrdered(T *expected, T *desired) noexcept { return a.compare_exchange_strong(expected, desired); }
    T *fetchAndStoreOrdered(T *p) noexcept { return a.exchange(p); }
    T *fetchAndAddOrdered(qptrdiff n) noexcept { return a.fetch_add(n); }
    operator T *() const noexcept { return a.load(); }
    QAtomicPointer &operator=(T *p) noexcept { a.store(p); return *this; }

private:
    std::atomic<T *> a;
};
