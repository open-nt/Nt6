// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <atomic>
#include <utility>

class QSharedData {
public:
    mutable std::atomic<int> ref{0};
    QSharedData() noexcept = default;
    QSharedData(const QSharedData &) noexcept : ref(0) {}
    QSharedData &operator=(const QSharedData &) = delete;
};

struct QAdoptSharedDataTag {
    explicit constexpr QAdoptSharedDataTag() = default;
};

// Implicitly shared (copy-on-write) pointer.
template <typename T>
class QSharedDataPointer {
public:
    QSharedDataPointer() noexcept : d(nullptr) {}
    explicit QSharedDataPointer(T *data) noexcept : d(data) { if (d) d->ref.fetch_add(1); }
    QSharedDataPointer(QAdoptSharedDataTag, T *data) noexcept : d(data) {}
    QSharedDataPointer(const QSharedDataPointer &o) noexcept : d(o.d) { if (d) d->ref.fetch_add(1); }
    QSharedDataPointer(QSharedDataPointer &&o) noexcept : d(std::exchange(o.d, nullptr)) {}
    QSharedDataPointer &operator=(const QSharedDataPointer &o) noexcept {
        QSharedDataPointer tmp(o);
        swap(tmp);
        return *this;
    }
    QSharedDataPointer &operator=(QSharedDataPointer &&o) noexcept {
        QSharedDataPointer tmp(std::move(o));
        swap(tmp);
        return *this;
    }
    ~QSharedDataPointer() { if (d && d->ref.fetch_sub(1) == 1) delete d; }

    void detach() { if (d && d->ref.load() != 1) detachHelper(); }
    T &operator*() { detach(); return *d; }
    const T &operator*() const { return *d; }
    T *operator->() { detach(); return d; }
    const T *operator->() const { return d; }
    operator bool() const noexcept { return d != nullptr; }
    T *data() { detach(); return d; }
    const T *data() const { return d; }
    const T *constData() const { return d; }
    void reset(T *p = nullptr) noexcept { QSharedDataPointer tmp(p); swap(tmp); }
    void swap(QSharedDataPointer &o) noexcept { std::swap(d, o.d); }
    friend bool operator==(const QSharedDataPointer &a, const QSharedDataPointer &b) { return a.d == b.d; }
    friend bool operator!=(const QSharedDataPointer &a, const QSharedDataPointer &b) { return a.d != b.d; }

private:
    void detachHelper() {
        T *x = new T(*d);
        x->ref.store(1);
        if (d->ref.fetch_sub(1) == 1)
            delete d;
        d = x;
    }
    T *d;
};

// Shared pointer that never detaches implicitly.
template <typename T>
class QExplicitlySharedDataPointer {
public:
    QExplicitlySharedDataPointer() noexcept : d(nullptr) {}
    explicit QExplicitlySharedDataPointer(T *data) noexcept : d(data) { if (d) d->ref.fetch_add(1); }
    QExplicitlySharedDataPointer(const QExplicitlySharedDataPointer &o) noexcept : d(o.d) { if (d) d->ref.fetch_add(1); }
    QExplicitlySharedDataPointer &operator=(const QExplicitlySharedDataPointer &o) noexcept {
        QExplicitlySharedDataPointer tmp(o);
        std::swap(d, tmp.d);
        return *this;
    }
    ~QExplicitlySharedDataPointer() { if (d && d->ref.fetch_sub(1) == 1) delete d; }

    void detach() {
        if (d && d->ref.load() != 1) {
            T *x = new T(*d);
            x->ref.store(1);
            if (d->ref.fetch_sub(1) == 1)
                delete d;
            d = x;
        }
    }
    T &operator*() const { return *d; }
    T *operator->() const noexcept { return d; }
    T *data() const noexcept { return d; }
    operator bool() const noexcept { return d != nullptr; }

private:
    T *d;
};
