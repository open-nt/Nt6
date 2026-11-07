// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

/*!
    \class QPointer
    \brief The QPointer class is a template class that provides guarded pointers to QObject.

    A QPointer\<T\> behaves like a normal \c{T *} but becomes \c nullptr
    automatically when the QObject it points to is destroyed. \c T must
    derive from QObject.

    \code
    QPointer<QTimer> timer = new QTimer;
    delete timer.data();
    Q_ASSERT(timer.isNull());
    \endcode

    \differences
    The guard is held through a shared control block, so a QPointer may
    outlive the object but is not thread-safe against concurrent destruction.
    There are no comparison operators; compare data() instead.
    \enddifferences

    \sa QObject, QSharedPointer
*/
template <typename T>
class QPointer {
    static_assert(std::is_base_of_v<QObject, T>, "QPointer requires a QObject type");

public:
    /*!
        Constructs a null guarded pointer.
    */
    QPointer() = default;
    /*!
        Constructs a guarded pointer that tracks \a p.
    */
    QPointer(T *p) { assign(p); }
    /*!
        Constructs a copy of another guarded pointer, tracking the same
        object.
    */
    QPointer(const QPointer &) = default;
    /*!
        Assigns another guarded pointer to this one and returns a reference
        to this pointer.
    */
    QPointer &operator=(const QPointer &) = default;
    /*!
        \overload
        Makes this pointer track \a p and returns a reference to this
        pointer.
    */
    QPointer &operator=(T *p) { assign(p); return *this; }

    /*!
        Returns the guarded object, or \c nullptr if it has been destroyed or
        the pointer is null.
    */
    T *data() const { return d ? static_cast<T *>(d->obj.load()) : nullptr; }
    /*!
        Returns the guarded object. This is the same as data().
    */
    T *get() const { return data(); }
    /*!
        Returns the guarded object, for member access.
    */
    T *operator->() const { return data(); }
    /*!
        Returns a reference to the guarded object. The pointer must not be
        null.
    */
    T &operator*() const { return *data(); }
    /*!
        Returns the guarded object. This allows a QPointer to be used where a
        \c{T *} is expected.
    */
    operator T *() const { return data(); }
    /*!
        Returns \c true if the guarded object is null or has been destroyed;
        otherwise returns \c false.
    */
    bool isNull() const { return data() == nullptr; }
    /*!
        Makes the pointer null.
    */
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
