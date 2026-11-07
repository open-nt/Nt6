// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <atomic>
#include <utility>

/*!
    \class QSharedData
    \brief The QSharedData class is a base class for shared data objects.

    Derive a class from QSharedData to hold the data of an implicitly shared
    class, and refer to it through QSharedDataPointer or
    QExplicitlySharedDataPointer. The base class provides the atomic reference
    count those pointers maintain.

    \differences
    The reference count is a public \c{std::atomic<int>} member rather than a
    \c QAtomicInt.
    \enddifferences

    \sa QSharedDataPointer, QExplicitlySharedDataPointer
*/
class QSharedData {
public:
    /*!
        \variable QSharedData::ref

        The reference count of the data object. It is managed by
        QSharedDataPointer and QExplicitlySharedDataPointer.
    */
    mutable std::atomic<int> ref{0};
    /*!
        Constructs a QSharedData object with a reference count of 0.
    */
    QSharedData() noexcept = default;
    /*!
        Constructs a copy of another QSharedData object. The reference count
        of the new object is 0, not that of the original.
    */
    QSharedData(const QSharedData &) noexcept : ref(0) {}
    QSharedData &operator=(const QSharedData &) = delete;
};

/*!
    \class QAdoptSharedDataTag
    \brief The QAdoptSharedDataTag struct tags a QSharedDataPointer constructor that adopts data.

    Pass a QAdoptSharedDataTag as the first argument to
    QSharedDataPointer::QSharedDataPointer(QAdoptSharedDataTag, T *) to take
    over a data pointer without incrementing its reference count.

    \differences
    This is an Nt-only type, not part of the public Qt API.
    \enddifferences
*/
struct QAdoptSharedDataTag {
    /*!
        Constructs a tag.
    */
    explicit constexpr QAdoptSharedDataTag() = default;
};

/*!
    \class QSharedDataPointer
    \brief The QSharedDataPointer class represents a pointer to an implicitly shared object.

    QSharedDataPointer\<T\> holds a pointer to a QSharedData-derived object and
    maintains its reference count. Copying a QSharedDataPointer shares the
    data; calling a non-const function, or the non-const \c operator->(),
    \c operator*() or data(), detaches first, making a private copy of the
    data if it is shared (copy-on-write).

    \differences
    There is no \c{operator!()}, no relational operators other than
    \c{==} and \c{!=} between two pointers, and no \c{clone()} customisation
    point or \c qHash(); detaching always copy-constructs \c T. The
    conversion to \c bool is implicit.
    \enddifferences

    \sa QSharedData, QExplicitlySharedDataPointer
*/
template <typename T>
class QSharedDataPointer {
public:
    /*!
        Constructs a null pointer.
    */
    QSharedDataPointer() noexcept : d(nullptr) {}
    /*!
        Constructs a pointer that shares \a data and increments its reference
        count. A null \a data gives a null pointer.
    */
    explicit QSharedDataPointer(T *data) noexcept : d(data) { if (d) d->ref.fetch_add(1); }
    /*!
        Constructs a pointer that adopts \a data without changing its
        reference count. The first argument is the tag that selects this
        constructor.
    */
    QSharedDataPointer(QAdoptSharedDataTag, T *data) noexcept : d(data) {}
    /*!
        Constructs a copy of \a o that shares its data and increments the
        reference count.
    */
    QSharedDataPointer(const QSharedDataPointer &o) noexcept : d(o.d) { if (d) d->ref.fetch_add(1); }
    /*!
        Move-constructs a pointer from \a o, leaving \a o null.
    */
    QSharedDataPointer(QSharedDataPointer &&o) noexcept : d(std::exchange(o.d, nullptr)) {}
    /*!
        Assigns \a o to this pointer, which then shares the data of \a o, and
        returns a reference to this pointer.
    */
    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment): copy and swap is safe for self-assignment
    QSharedDataPointer &operator=(const QSharedDataPointer &o) noexcept {
        QSharedDataPointer tmp(o);
        swap(tmp);
        return *this;
    }
    /*!
        Move-assigns \a o to this pointer, leaving \a o null, and returns a
        reference to this pointer.
    */
    QSharedDataPointer &operator=(QSharedDataPointer &&o) noexcept {
        QSharedDataPointer tmp(std::move(o));
        swap(tmp);
        return *this;
    }
    /*!
        Destroys the pointer, decrementing the reference count and deleting
        the data if this was the last reference.
    */
    ~QSharedDataPointer() { if (d && d->ref.fetch_sub(1) == 1) delete d; }

    /*!
        Detaches from shared data. If the data is shared with another
        pointer, it is copied and this pointer refers to the copy. Does
        nothing for a null pointer.
    */
    void detach() { if (d && d->ref.load() != 1) detachHelper(); }
    /*!
        Returns a reference to the data, detaching first if it is shared.
    */
    T &operator*() { detach(); return *d; }
    /*!
        \overload
        Returns a const reference to the data without detaching.
    */
    const T &operator*() const { return *d; }
    /*!
        Returns a pointer to the data, detaching first if it is shared.
    */
    T *operator->() { detach(); return d; }
    /*!
        \overload
        Returns a const pointer to the data without detaching.
    */
    const T *operator->() const { return d; }
    /*!
        Returns \c true if the pointer is not null; otherwise returns
        \c false.
    */
    operator bool() const noexcept { return d != nullptr; }
    /*!
        Returns a pointer to the data, detaching first if it is shared.

        \sa constData()
    */
    T *data() { detach(); return d; }
    /*!
        \overload
        Returns a const pointer to the data without detaching.
    */
    const T *data() const { return d; }
    /*!
        Returns a const pointer to the data without detaching.

        \sa data()
    */
    const T *constData() const { return d; }
    /*!
        Resets the pointer to share \a p, releasing the current data. The
        default is a null pointer.
    */
    void reset(T *p = nullptr) noexcept { QSharedDataPointer tmp(p); swap(tmp); }
    /*!
        Swaps this pointer with \a o. This operation is very fast and never
        fails.
    */
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

/*!
    \class QExplicitlySharedDataPointer
    \brief The QExplicitlySharedDataPointer class represents a pointer to an explicitly shared object.

    QExplicitlySharedDataPointer\<T\> shares a QSharedData-derived object like
    QSharedDataPointer does, but never detaches implicitly: modifying the data
    through any pointer modifies it for all of them. Call detach() to make a
    private copy explicitly.

    \differences
    It has no move constructor or move assignment, no \c reset(), \c swap(),
    \c constData() or \c{operator!()}, and no adopting constructor. The
    conversion to \c bool is implicit.
    \enddifferences

    \sa QSharedData, QSharedDataPointer
*/
template <typename T>
class QExplicitlySharedDataPointer {
public:
    /*!
        Constructs a null pointer.
    */
    QExplicitlySharedDataPointer() noexcept : d(nullptr) {}
    /*!
        Constructs a pointer that shares \a data and increments its reference
        count. A null \a data gives a null pointer.
    */
    explicit QExplicitlySharedDataPointer(T *data) noexcept : d(data) { if (d) d->ref.fetch_add(1); }
    /*!
        Constructs a copy of \a o that shares its data and increments the
        reference count.
    */
    QExplicitlySharedDataPointer(const QExplicitlySharedDataPointer &o) noexcept : d(o.d) { if (d) d->ref.fetch_add(1); }
    /*!
        Assigns \a o to this pointer, which then shares the data of \a o, and
        returns a reference to this pointer.
    */
    QExplicitlySharedDataPointer &operator=(const QExplicitlySharedDataPointer &o) noexcept {
        QExplicitlySharedDataPointer tmp(o);
        std::swap(d, tmp.d);
        return *this;
    }
    /*!
        Destroys the pointer, decrementing the reference count and deleting
        the data if this was the last reference.
    */
    ~QExplicitlySharedDataPointer() { if (d && d->ref.fetch_sub(1) == 1) delete d; }

    /*!
        Detaches from shared data. If the data is shared with another
        pointer, it is copied and this pointer refers to the copy. Does
        nothing for a null pointer.
    */
    void detach() {
        if (d && d->ref.load() != 1) {
            T *x = new T(*d);
            x->ref.store(1);
            if (d->ref.fetch_sub(1) == 1)
                delete d;
            d = x;
        }
    }
    /*!
        Returns a reference to the data without detaching.
    */
    T &operator*() const { return *d; }
    /*!
        Returns a pointer to the data without detaching.
    */
    T *operator->() const noexcept { return d; }
    /*!
        Returns a pointer to the data without detaching.
    */
    T *data() const noexcept { return d; }
    /*!
        Returns \c true if the pointer is not null; otherwise returns
        \c false.
    */
    operator bool() const noexcept { return d != nullptr; }

private:
    T *d;
};
