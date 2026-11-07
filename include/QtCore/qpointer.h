// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

// Guarded pointer: becomes null when the QObject is destroyed.
template <typename T>
class QPointer {
    static_assert(std::is_base_of_v<QObject, T>, "QPointer requires a QObject type");

public:
    QPointer() = default;
    QPointer(T *p) { assign(p); }
    QPointer(const QPointer &) = default;
    QPointer &operator=(const QPointer &) = default;
    QPointer &operator=(T *p) { assign(p); return *this; }

    T *data() const { return d && d->obj ? static_cast<T *>(d->obj) : nullptr; }
    T *get() const { return data(); }
    T *operator->() const { return data(); }
    T &operator*() const { return *data(); }
    operator T *() const { return data(); }
    bool isNull() const { return data() == nullptr; }
    void clear() { d.reset(); }

private:
    void assign(T *p) {
        if (p)
            d = QtPrivate::Access::guard(p);
        else
            d.reset();
    }
    std::shared_ptr<QtPrivate::PointerData> d;
};
