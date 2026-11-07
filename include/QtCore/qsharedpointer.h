// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <memory>

template <typename T>
class QWeakPointer;

/*!
    \class QSharedPointer
    \brief The QSharedPointer class holds a strong reference to a shared pointer.

    QSharedPointer\<T\> shares ownership of an object through atomic
    reference counting. The object is deleted when the last QSharedPointer
    referring to it is destroyed or reset. QSharedPointer inherits from
    \c{std::shared_ptr<T>}, so the standard interface (\c get(), \c reset(),
    \c use_count(), \c operator*() and so on) is also available.

    \differences
    QSharedPointer is a thin subclass of \c{std::shared_ptr<T>}, so
    reference counting and the deleter follow the standard library. There is
    no \c objectCast(), \c qSharedPointerObjectCast(),
    \c qSharedPointerConstCast() or \c qHash(), and no custom-deleter
    \c reset() overload.
    \enddifferences

    \sa QWeakPointer, QEnableSharedFromThis
*/
template <typename T>
class QSharedPointer : public std::shared_ptr<T> {
    using Base = std::shared_ptr<T>;

public:
    /*!
        Constructs a null shared pointer.
    */
    QSharedPointer() noexcept = default;
    /*!
        Constructs a null shared pointer.
    */
    QSharedPointer(std::nullptr_t) noexcept {}
    /*!
        Constructs a shared pointer that takes ownership of \a p, which is
        deleted with \c delete when the last reference goes away.
    */
    explicit QSharedPointer(T *p) : Base(p) {}
    /*!
        \overload
        Constructs a shared pointer that takes ownership of \a p and calls
        the deleter \a d to destroy it when the last reference goes away.
    */
    template <typename Deleter>
    QSharedPointer(T *p, Deleter d) : Base(p, d) {}
    /*!
        \overload
        Constructs a shared pointer from the \c{std::shared_ptr} \a b,
        sharing its ownership.
    */
    QSharedPointer(const Base &b) : Base(b) {}
    /*!
        \overload
        Constructs a shared pointer from \a o, whose pointer type \c{X *}
        must be implicitly convertible to \c{T *}.
    */
    template <typename X, typename = std::enable_if_t<std::is_convertible_v<X *, T *>>>
    QSharedPointer(const QSharedPointer<X> &o) : Base(o) {}
    /*!
        Constructs a shared pointer from the weak pointer \a w. The result is
        null if the object \a w referred to has already been destroyed.

        \sa QWeakPointer::toStrongRef()
    */
    QSharedPointer(const QWeakPointer<T> &w);

    /*!
        Returns the pointer to the shared object, or \c nullptr.
    */
    T *data() const noexcept { return this->get(); }
    /*!
        Returns \c true if this pointer is null; otherwise returns
        \c false.
    */
    bool isNull() const noexcept { return this->get() == nullptr; }
    /*!
        Releases this reference and makes the pointer null.
    */
    void clear() noexcept { this->reset(); }
    /*!
        Returns a QWeakPointer that observes the same object without
        keeping it alive.

        \sa QWeakPointer::toStrongRef()
    */
    QWeakPointer<T> toWeakRef() const;
    /*!
        Returns a shared pointer to the same object converted to \c{X *}
        with \c static_cast. The reference count is shared with this pointer.
    */
    template <typename X>
    QSharedPointer<X> staticCast() const { return QSharedPointer<X>(std::static_pointer_cast<X>(static_cast<const Base &>(*this))); }
    /*!
        Returns a shared pointer to the same object converted to \c{X *}
        with \c dynamic_cast. The result is null if the cast fails.
    */
    template <typename X>
    QSharedPointer<X> dynamicCast() const { return QSharedPointer<X>(std::dynamic_pointer_cast<X>(static_cast<const Base &>(*this))); }
    /*!
        Returns a shared pointer to the same object converted to \c{X *}
        with \c const_cast. The reference count is shared with this pointer.
    */
    template <typename X>
    QSharedPointer<X> constCast() const { return QSharedPointer<X>(std::const_pointer_cast<X>(static_cast<const Base &>(*this))); }

    /*!
        Creates a new \c T constructed from \a args and returns a shared
        pointer to it, allocating the object and its reference count together.
    */
    template <typename... Args>
    static QSharedPointer create(Args &&...args) { return QSharedPointer(std::make_shared<T>(std::forward<Args>(args)...)); }
};

/*!
    \class QWeakPointer
    \brief The QWeakPointer class holds a weak reference to a shared pointer.

    A QWeakPointer\<T\> observes an object owned by QSharedPointer objects
    without keeping it alive. Call toStrongRef() to obtain a QSharedPointer,
    which is null if the object has been destroyed. QWeakPointer inherits from
    \c{std::weak_ptr<T>}.

    \differences
    QWeakPointer is a thin subclass of \c{std::weak_ptr<T>}. It cannot observe
    a QObject directly, and has no \c data(), \c{operator bool()} or
    \c{operator==()}.
    \enddifferences

    \sa QSharedPointer
*/
template <typename T>
class QWeakPointer : public std::weak_ptr<T> {
    using Base = std::weak_ptr<T>;

public:
    /*!
        Constructs a null weak pointer.
    */
    QWeakPointer() noexcept = default;
    /*!
        Constructs a weak pointer that observes the object held by \a s.
    */
    QWeakPointer(const QSharedPointer<T> &s) noexcept : Base(s) {}
    /*!
        \overload
        Constructs a weak pointer from the \c{std::weak_ptr} \a b.
    */
    QWeakPointer(const Base &b) noexcept : Base(b) {}

    /*!
        Returns \c true if the observed object has been destroyed or the
        pointer is null; otherwise returns \c false.
    */
    bool isNull() const noexcept { return this->expired(); }
    /*!
        Makes the weak pointer null.
    */
    void clear() noexcept { this->reset(); }
    /*!
        Returns a QSharedPointer to the observed object, or a null pointer
        if it has been destroyed.

        \sa QSharedPointer::toWeakRef()
    */
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

/*!
    \class QEnableSharedFromThis
    \brief The QEnableSharedFromThis class lets an object create shared pointers to itself.

    Derive \c T from QEnableSharedFromThis\<T\> so that sharedFromThis() can
    return a QSharedPointer that shares ownership with the existing
    QSharedPointer managing the object. The object must already be owned by
    a QSharedPointer when sharedFromThis() is called.

    \differences
    QEnableSharedFromThis is a thin subclass of
    \c{std::enable_shared_from_this<T>}.
    \enddifferences

    \sa QSharedPointer
*/
template <typename T>
class QEnableSharedFromThis : public std::enable_shared_from_this<T> {
public:
    /*!
        Returns a QSharedPointer to this object. Calling it on an object not
        owned by a shared pointer throws \c{std::bad_weak_ptr}.
    */
    QSharedPointer<T> sharedFromThis() { return QSharedPointer<T>(this->shared_from_this()); }
    /*!
        \overload
        Returns a QSharedPointer to this const object.
    */
    QSharedPointer<const T> sharedFromThis() const { return QSharedPointer<const T>(this->shared_from_this()); }
};
