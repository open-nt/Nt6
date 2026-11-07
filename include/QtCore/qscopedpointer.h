// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <memory>
#include <utility>

/*!
    \class QScopedPointerDeleter
    \brief The QScopedPointerDeleter struct is the default cleanup handler of QScopedPointer.

    It deletes the object with \c delete.

    \differences
    None. It has the same interface as in Qt, including the call operator.
    \enddifferences

    \sa QScopedPointer
*/
template <typename T>
struct QScopedPointerDeleter {
    /*!
        Deletes \a p with \c delete.
    */
    static void cleanup(T *p) noexcept { delete p; }
    /*!
        Calls cleanup() on \a p.
    */
    void operator()(T *p) const noexcept { cleanup(p); }
};

/*!
    \class QScopedPointer
    \brief The QScopedPointer class stores a pointer to a dynamically allocated object and deletes it on destruction.

    A QScopedPointer owns the object it points to and deletes it, through
    the \c Cleanup handler, when it goes out of scope or is reset. It cannot
    be copied.

    \code
    QScopedPointer<QFile> file(new QFile("data.txt"));
    file->open(QIODevice::ReadOnly);
    \endcode

    \differences
    QScopedPointer is built on \c std::unique_ptr and is not movable. The
    \c Cleanup handler must be usable as a \c std::unique_ptr deleter (see
    QScopedPointerDeleter). There are no comparison operators and no
    free \c swap() function.
    \enddifferences

    \sa QScopedArrayPointer, QSharedPointer
*/
template <typename T, typename Cleanup = QScopedPointerDeleter<T>>
class QScopedPointer {
public:
    /*!
        Constructs a scoped pointer that owns \a p. The default is a null
        pointer.
    */
    explicit QScopedPointer(T *p = nullptr) noexcept : d(p) {}
    QScopedPointer(const QScopedPointer &) = delete;
    QScopedPointer &operator=(const QScopedPointer &) = delete;

    /*!
        Returns a reference to the object. The pointer must not be null.
    */
    T &operator*() const { return *d; }
    /*!
        Returns the pointer to the object.
    */
    T *operator->() const noexcept { return d.get(); }
    /*!
        Returns \c true if the pointer is null; otherwise returns \c false.
    */
    bool operator!() const noexcept { return !d; }
    /*!
        Returns \c true if the pointer is not null; otherwise returns
        \c false.
    */
    explicit operator bool() const noexcept { return static_cast<bool>(d); }
    /*!
        Returns the pointer to the object, without giving up ownership.

        \sa get(), take()
    */
    T *data() const noexcept { return d.get(); }
    /*!
        Returns the pointer to the object. This is the same as data().
    */
    T *get() const noexcept { return d.get(); }
    /*!
        Returns \c true if the pointer is null; otherwise returns \c false.
    */
    bool isNull() const noexcept { return !d; }
    /*!
        Deletes the current object and takes ownership of \a other. The
        default is a null pointer.
    */
    void reset(T *other = nullptr) noexcept { d.reset(other); }
    /*!
        Returns the pointer to the object and releases ownership, so the
        caller must delete it. The scoped pointer becomes null.
    */
    T *take() noexcept { return d.release(); }
    /*!
        Swaps this pointer with \a o.
    */
    void swap(QScopedPointer &o) noexcept { d.swap(o.d); }

private:
    std::unique_ptr<T, Cleanup> d;
};

/*!
    \class QScopedPointerArrayDeleter
    \brief The QScopedPointerArrayDeleter struct is the cleanup handler of QScopedArrayPointer.

    It deletes the array with \c{delete[]}.

    \differences
    None. It has the same interface as in Qt, including the call operator.
    \enddifferences

    \sa QScopedArrayPointer
*/
template <typename T>
struct QScopedPointerArrayDeleter {
    /*!
        Deletes the array \a p with \c{delete[]}.
    */
    static void cleanup(T *p) noexcept { delete[] p; }
    /*!
        Calls cleanup() on \a p.
    */
    void operator()(T *p) const noexcept { cleanup(p); }
};

/*!
    \class QScopedArrayPointer
    \brief The QScopedArrayPointer class stores a pointer to a dynamically allocated array and deletes it on destruction.

    QScopedArrayPointer is a QScopedPointer that cleans up with
    \c{delete[]} and provides indexed access to the elements.

    \differences
    The constructor and \c reset() accept only \c{T *}, not a pointer to a
    different type.
    \enddifferences

    \sa QScopedPointer
*/
template <typename T>
class QScopedArrayPointer : public QScopedPointer<T, QScopedPointerArrayDeleter<T>> {
public:
    /*!
        Constructs a scoped array pointer that owns the array \a p. The
        default is a null pointer.
    */
    explicit QScopedArrayPointer(T *p = nullptr) noexcept : QScopedPointer<T, QScopedPointerArrayDeleter<T>>(p) {}
    /*!
        Returns a reference to the element at index \a i. There is no bounds
        checking.
    */
    T &operator[](qsizetype i) const { return this->data()[i]; }
};
