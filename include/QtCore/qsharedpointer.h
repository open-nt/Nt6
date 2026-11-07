// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <memory>

template <typename T>
class QWeakPointer;

template <typename T>
class QSharedPointer : public std::shared_ptr<T> {
    using Base = std::shared_ptr<T>;

public:
    QSharedPointer() noexcept = default;
    QSharedPointer(std::nullptr_t) noexcept {}
    explicit QSharedPointer(T *p) : Base(p) {}
    template <typename Deleter>
    QSharedPointer(T *p, Deleter d) : Base(p, d) {}
    QSharedPointer(const Base &b) : Base(b) {}
    template <typename X, typename = std::enable_if_t<std::is_convertible_v<X *, T *>>>
    QSharedPointer(const QSharedPointer<X> &o) : Base(o) {}
    QSharedPointer(const QWeakPointer<T> &w);

    T *data() const noexcept { return this->get(); }
    bool isNull() const noexcept { return this->get() == nullptr; }
    void clear() noexcept { this->reset(); }
    QWeakPointer<T> toWeakRef() const;
    template <typename X>
    QSharedPointer<X> staticCast() const { return QSharedPointer<X>(std::static_pointer_cast<X>(static_cast<const Base &>(*this))); }
    template <typename X>
    QSharedPointer<X> dynamicCast() const { return QSharedPointer<X>(std::dynamic_pointer_cast<X>(static_cast<const Base &>(*this))); }
    template <typename X>
    QSharedPointer<X> constCast() const { return QSharedPointer<X>(std::const_pointer_cast<X>(static_cast<const Base &>(*this))); }

    template <typename... Args>
    static QSharedPointer create(Args &&...args) { return QSharedPointer(std::make_shared<T>(std::forward<Args>(args)...)); }
};

template <typename T>
class QWeakPointer : public std::weak_ptr<T> {
    using Base = std::weak_ptr<T>;

public:
    QWeakPointer() noexcept = default;
    QWeakPointer(const QSharedPointer<T> &s) noexcept : Base(s) {}
    QWeakPointer(const Base &b) noexcept : Base(b) {}

    bool isNull() const noexcept { return this->expired(); }
    void clear() noexcept { this->reset(); }
    QSharedPointer<T> toStrongRef() const { return QSharedPointer<T>(Base::lock()); }
};

template <typename T>
QSharedPointer<T>::QSharedPointer(const QWeakPointer<T> &w) : Base(w.lock()) {}

template <typename T>
QWeakPointer<T> QSharedPointer<T>::toWeakRef() const { return QWeakPointer<T>(*this); }

template <typename T, typename... Args>
QSharedPointer<T> qMakeShared(Args &&...args) { return QSharedPointer<T>::create(std::forward<Args>(args)...); }

template <typename X, typename T>
QSharedPointer<X> qSharedPointerCast(const QSharedPointer<T> &p) { return p.template staticCast<X>(); }
template <typename X, typename T>
QSharedPointer<X> qSharedPointerDynamicCast(const QSharedPointer<T> &p) { return p.template dynamicCast<X>(); }

template <typename T>
class QEnableSharedFromThis : public std::enable_shared_from_this<T> {
public:
    QSharedPointer<T> sharedFromThis() { return QSharedPointer<T>(this->shared_from_this()); }
    QSharedPointer<const T> sharedFromThis() const { return QSharedPointer<const T>(this->shared_from_this()); }
};
