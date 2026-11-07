// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <memory>
#include <utility>

template <typename T>
struct QScopedPointerDeleter {
    static void cleanup(T *p) noexcept { delete p; }
    void operator()(T *p) const noexcept { cleanup(p); }
};

template <typename T, typename Cleanup = QScopedPointerDeleter<T>>
class QScopedPointer {
public:
    explicit QScopedPointer(T *p = nullptr) noexcept : d(p) {}
    QScopedPointer(const QScopedPointer &) = delete;
    QScopedPointer &operator=(const QScopedPointer &) = delete;

    T &operator*() const { return *d; }
    T *operator->() const noexcept { return d.get(); }
    bool operator!() const noexcept { return !d; }
    explicit operator bool() const noexcept { return static_cast<bool>(d); }
    T *data() const noexcept { return d.get(); }
    T *get() const noexcept { return d.get(); }
    bool isNull() const noexcept { return !d; }
    void reset(T *other = nullptr) noexcept { d.reset(other); }
    T *take() noexcept { return d.release(); }
    void swap(QScopedPointer &o) noexcept { d.swap(o.d); }

private:
    std::unique_ptr<T, Cleanup> d;
};

template <typename T>
struct QScopedPointerArrayDeleter {
    static void cleanup(T *p) noexcept { delete[] p; }
    void operator()(T *p) const noexcept { cleanup(p); }
};

template <typename T>
class QScopedArrayPointer : public QScopedPointer<T, QScopedPointerArrayDeleter<T>> {
public:
    explicit QScopedArrayPointer(T *p = nullptr) noexcept : QScopedPointer<T, QScopedPointerArrayDeleter<T>>(p) {}
    T &operator[](qsizetype i) const { return this->data()[i]; }
};
