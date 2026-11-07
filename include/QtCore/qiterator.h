// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>

#include <iterator>

// Java-style iterators. The iterator sits between items: next() returns the
// item it jumps over.

/*!
    \class QListIterator
    \brief The QListIterator class provides a Java-style const iterator for QList.

    QListIterator<T> iterates over a list without modifying it. The iterator
    points between items, not at them: next() returns the item it jumps over
    and moves forward, and previous() moves back and returns the item it jumps
    over. A new iterator is positioned before the first item.

    \code
    QList<QString> list = {"A", "B", "C"};
    QListIterator<QString> it(list);
    while (it.hasNext())
        qDebug() << it.next();
    \endcode

    The iterator refers to the container it was created from, which must not
    be destroyed or modified while the iterator is in use. QVectorIterator is
    an alias of this class.

    \differences
    The second template argument selects the container type and defaults to
    QList<T>. An iterator is assigned from a container, not from another
    iterator.
    \enddifferences

    \sa QMutableListIterator, QSetIterator
*/
template <typename T, typename C = QList<T>>
class QListIterator {
public:
    /*!
    Constructs an iterator for traversing \a container. The iterator is set to be
    at the front of the list, before the first item.

    \sa toFront()
    */
    explicit QListIterator(const C &container) : c(&container), i(c->begin()) {}
    /*!
    Makes the iterator operate on \a container and moves it to the front of the
    list. Returns a reference to this iterator.
    */
    QListIterator &operator=(const C &container) { c = &container; i = c->begin(); return *this; }

    /*!
    Moves the iterator to the front of the list, before the first item.

    \sa toBack(), next()
    */
    void toFront() { i = c->begin(); }
    /*!
    Moves the iterator to the back of the list, after the last item.

    \sa toFront(), previous()
    */
    void toBack() { i = c->end(); }
    /*!
    Returns \c true if there is at least one item ahead of the iterator;
    otherwise returns \c false.

    \sa hasPrevious(), next()
    */
    bool hasNext() const { return i != c->end(); }
    /*!
    Returns \c true if there is at least one item behind the iterator; otherwise
    returns \c false.

    \sa hasNext(), previous()
    */
    bool hasPrevious() const { return i != c->begin(); }
    /*!
    Returns the next item and advances the iterator by one position. Calling this
    function when the iterator is at the back of the list is undefined.

    \sa hasNext(), peekNext(), previous()
    */
    const T &next() { return *i++; }
    /*!
    Returns the previous item and moves the iterator back by one position.
    Calling this function when the iterator is at the front of the list is
    undefined.

    \sa hasPrevious(), peekPrevious(), next()
    */
    const T &previous() { return *--i; }
    /*!
    Returns the next item without moving the iterator. Calling this function when
    the iterator is at the back of the list is undefined.

    \sa hasNext(), next(), peekPrevious()
    */
    const T &peekNext() const { return *i; }
    /*!
    Returns the previous item without moving the iterator. Calling this function
    when the iterator is at the front of the list is undefined.

    \sa hasPrevious(), previous(), peekNext()
    */
    const T &peekPrevious() const { auto p = i; return *--p; }
    /*!
    Searches for \a v forward from the iterator position. Returns \c true if an
    item equal to \a v is found, leaving the iterator just after it; otherwise
    returns \c false and leaves the iterator at the back of the list.

    \sa findPrevious()
    */
    bool findNext(const T &v) {
        while (hasNext())
            if (next() == v)
                return true;
        return false;
    }
    /*!
    Searches for \a v backward from the iterator position. Returns \c true if an
    item equal to \a v is found, leaving the iterator just before it; otherwise
    returns \c false and leaves the iterator at the front of the list.

    \sa findNext()
    */
    bool findPrevious(const T &v) {
        while (hasPrevious())
            if (previous() == v)
                return true;
        return false;
    }

private:
    const C *c;
    typename C::const_iterator i;
};

/*!
    \class QMutableListIterator
    \brief The QMutableListIterator class provides a Java-style non-const iterator for QList.

    QMutableListIterator<T> iterates over a list and can modify it. Like
    QListIterator it points between items; in addition, value(), setValue()
    and remove() act on the item that was last jumped over by next() or
    previous(), and insert() adds an item at the iterator position.

    \code
    QList<int> list = {1, 2, 3, 4};
    QMutableListIterator<int> it(list);
    while (it.hasNext()) {
        if (it.next() % 2 == 0)
            it.remove();
    }
    \endcode

    The iterator is index-based, so inserting or removing items through it
    keeps it valid. Modifying the list by other means while an iterator is in
    use is not supported. QMutableVectorIterator is an alias of this class.

    \differences
    The container type is always QList<T>.
    \enddifferences

    \sa QListIterator, QAssocMutableIterator
*/
template <typename T>
class QMutableListIterator {
public:
    /*!
    Constructs an iterator for traversing \a list. The iterator is set to be at
    the front of the list, before the first item.

    \sa toFront()
    */
    explicit QMutableListIterator(QList<T> &list) : c(&list) {}
    /*!
    Makes the iterator operate on \a list and moves it to the front of the list.
    Returns a reference to this iterator.
    */
    QMutableListIterator &operator=(QList<T> &list) { c = &list; toFront(); return *this; }

    /*!
    Moves the iterator to the front of the list, before the first item.

    \sa toBack(), next()
    */
    void toFront() { i = 0; item = -1; }
    /*!
    Moves the iterator to the back of the list, after the last item.

    \sa toFront(), previous()
    */
    void toBack() { i = c->size(); item = -1; }
    /*!
    Returns \c true if there is at least one item ahead of the iterator;
    otherwise returns \c false.

    \sa hasPrevious(), next()
    */
    bool hasNext() const { return i < c->size(); }
    /*!
    Returns \c true if there is at least one item behind the iterator; otherwise
    returns \c false.

    \sa hasNext(), previous()
    */
    bool hasPrevious() const { return i > 0; }
    /*!
    Returns a reference to the next item and advances the iterator by one
    position. Calling this function when the iterator is at the back of the list
    is undefined.

    \sa hasNext(), peekNext(), previous()
    */
    T &next() { item = i++; return (*c)[item]; }
    /*!
    Returns a reference to the previous item and moves the iterator back by one
    position. Calling this function when the iterator is at the front of the list
    is undefined.

    \sa hasPrevious(), peekPrevious(), next()
    */
    T &previous() { item = --i; return (*c)[item]; }
    /*!
    Returns a reference to the next item without moving the iterator. Calling
    this function when the iterator is at the back of the list is undefined.

    \sa hasNext(), next(), peekPrevious()
    */
    T &peekNext() const { return (*c)[i]; }
    /*!
    Returns a reference to the previous item without moving the iterator. Calling
    this function when the iterator is at the front of the list is undefined.

    \sa hasPrevious(), previous(), peekNext()
    */
    T &peekPrevious() const { return (*c)[i - 1]; }
    /*!
    Returns a reference to the item that was last jumped over by next(),
    previous() or insert(). The iterator must have jumped over an item, and the
    item must not have been removed.

    \sa setValue(), remove()
    */
    T &value() const { return (*c)[item]; }
    /*!
    Replaces the item that was last jumped over by next(), previous() or insert()
    with \a v. Does nothing if there is no such item.

    \sa value(), remove()
    */
    void setValue(const T &v) { if (item >= 0) (*c)[item] = v; }
    /*!
    Removes the item that was last jumped over by next() or previous(). Does
    nothing if there is no such item, or if it has already been removed.

    \sa setValue(), insert()
    */
    void remove() {
        if (item < 0)
            return;
        c->removeAt(item);
        if (item < i)
            --i;
        item = -1;
    }
    /*!
    Inserts \a v at the iterator position and advances the iterator past it, so
    the next call to next() returns the item that followed the insertion point.

    \sa remove(), setValue()
    */
    void insert(const T &v) { c->insert(i, v); item = i++; }
    /*!
    Searches for \a v forward from the iterator position. Returns \c true if an
    item equal to \a v is found, leaving the iterator just after it; otherwise
    returns \c false and leaves the iterator at the back of the list.

    \sa findPrevious()
    */
    bool findNext(const T &v) {
        while (hasNext())
            if (next() == v)
                return true;
        return false;
    }
    /*!
    Searches for \a v backward from the iterator position. Returns \c true if an
    item equal to \a v is found, leaving the iterator just before it; otherwise
    returns \c false and leaves the iterator at the front of the list.

    \sa findNext()
    */
    bool findPrevious(const T &v) {
        while (hasPrevious())
            if (previous() == v)
                return true;
        return false;
    }

private:
    QList<T> *c;
    qsizetype i = 0;
    qsizetype item = -1; // the item last jumped over
};

template <typename T>
using QVectorIterator = QListIterator<T>;
template <typename T>
using QMutableVectorIterator = QMutableListIterator<T>;

/*!
    \class QAssocConstIterator
    \brief The QAssocConstIterator class provides the Java-style const iterator behind QMapIterator, QMultiMapIterator and QHashIterator.

    QAssocConstIterator<C, K, V, Bidirectional> iterates over an associative
    container \c C with key type \c K and value type \c V without modifying
    it. It points between items: next() returns the item it jumps over, and
    key() and value() refer to that item. Use the aliases QMapIterator,
    QMultiMapIterator and QHashIterator rather than naming this class.

    \code
    QMap<QString, int> map = {{"a", 1}, {"b", 2}};
    QMapIterator<QString, int> it(map);
    while (it.hasNext()) {
        it.next();
        qDebug() << it.key() << it.value();
    }
    \endcode

    When \c Bidirectional is \c false, as for QHashIterator, the functions that
    move backward (hasPrevious(), previous(), peekPrevious() and
    findPrevious()) are not available.

    \differences
    next(), peekNext(), previous() and peekPrevious() return a const iterator
    of the container rather than a key-value item, and the iterator type is
    exposed as Item.
    \enddifferences

    \sa QAssocMutableIterator, QListIterator
*/
template <typename C, typename K, typename V, bool Bidirectional>
class QAssocConstIterator {
    using It = typename C::const_iterator;

public:
    /*!
    The type returned by next(), peekNext(), previous() and peekPrevious(): a
    const iterator of the container that provides key() and value().
    */
    using Item = It;
    /*!
    Constructs an iterator for traversing \a container. The iterator is set to be
    at the front of the container, before the first item.

    \sa toFront()
    */
    explicit QAssocConstIterator(const C &container) : c(&container), i(c->begin()), n(i) {}
    /*!
    Makes the iterator operate on \a container and moves it to the front.
    Returns a reference to this iterator.
    */
    QAssocConstIterator &operator=(const C &container) { c = &container; toFront(); return *this; }

    /*!
    Moves the iterator to the front of the container, before the first item.

    \sa toBack(), next()
    */
    void toFront() { i = c->begin(); n = i; }
    /*!
    Moves the iterator to the back of the container, after the last item.

    \sa toFront(), previous()
    */
    void toBack() { i = c->end(); n = i; }
    /*!
    Returns \c true if there is at least one item ahead of the iterator;
    otherwise returns \c false.

    \sa hasPrevious(), next()
    */
    bool hasNext() const { return i != c->end(); }
    /*!
    Returns the next item and advances the iterator by one position. The key and
    value of the returned item are then available from key() and value(). Calling
    this function when the iterator is at the back is undefined.

    \sa hasNext(), peekNext(), previous()
    */
    It next() { n = i++; return n; }
    /*!
    Returns the next item without moving the iterator. Calling this function when
    the iterator is at the back is undefined.

    \sa hasNext(), next(), peekPrevious()
    */
    It peekNext() const { return i; }
    /*!
    Returns the key of the item that was last jumped over by next() or
    previous().

    \sa value()
    */
    const K &key() const { return n.key(); }
    /*!
    Returns the value of the item that was last jumped over by next() or
    previous().

    \sa key()
    */
    const V &value() const { return n.value(); }
    /*!
    Searches for the value \a v forward from the iterator position. Returns
    \c true if an item with that value is found, leaving the iterator just after
    it so that key() and value() describe it; otherwise returns \c false and
    leaves the iterator at the back.

    \sa findPrevious()
    */
    bool findNext(const V &v) {
        while (i != c->end())
            if (*(n = i++) == v)
                return true;
        n = i;
        return false;
    }
    /*!
    Returns \c true if there is at least one item behind the iterator; otherwise
    returns \c false. Only available when the container is bidirectional.

    \sa hasNext(), previous()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    bool hasPrevious() const { return i != c->begin(); }
    /*!
    Returns the previous item and moves the iterator back by one position. Only
    available when the container is bidirectional. Calling this function when the
    iterator is at the front is undefined.

    \sa hasPrevious(), peekPrevious(), next()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    It previous() { n = --i; return n; }
    /*!
    Returns the previous item without moving the iterator. Only available when
    the container is bidirectional. Calling this function when the iterator is at
    the front is undefined.

    \sa hasPrevious(), previous(), peekNext()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    It peekPrevious() const { auto p = i; return --p; }
    /*!
    Searches for the value \a v backward from the iterator position. Returns
    \c true if an item with that value is found, leaving the iterator just before
    it so that key() and value() describe it; otherwise returns \c false and
    leaves the iterator at the front. Only available when the container is
    bidirectional.

    \sa findNext()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    bool findPrevious(const V &v) {
        while (i != c->begin())
            if (*(n = --i) == v)
                return true;
        n = i;
        return false;
    }

private:
    const C *c;
    It i, n;
};

/*!
    \class QAssocMutableIterator
    \brief The QAssocMutableIterator class provides the Java-style non-const iterator behind QMutableMapIterator, QMutableMultiMapIterator and QMutableHashIterator.

    QAssocMutableIterator<C, K, V, Bidirectional> iterates over an associative
    container \c C and can modify it. Like QAssocConstIterator it points
    between items; key(), value(), setValue() and remove() act on the item that
    was last jumped over. Use the aliases QMutableMapIterator,
    QMutableMultiMapIterator and QMutableHashIterator rather than naming this
    class.

    \code
    QMap<QString, int> map = {{"a", 1}, {"b", 2}};
    QMutableMapIterator<QString, int> it(map);
    while (it.hasNext()) {
        it.next();
        if (it.value() < 2)
            it.remove();
    }
    \endcode

    When \c Bidirectional is \c false, as for QMutableHashIterator, the
    functions that move backward (hasPrevious(), previous(), peekPrevious() and
    findPrevious()) are not available.

    \differences
    next(), peekNext(), previous() and peekPrevious() return an iterator of
    the container rather than a key-value item. There is no assignment from a
    container.
    \enddifferences

    \sa QAssocConstIterator, QMutableListIterator
*/
template <typename C, typename K, typename V, bool Bidirectional>
class QAssocMutableIterator {
    using It = typename C::iterator;

public:
    /*!
    Constructs an iterator for traversing \a container. The iterator is set to be
    at the front of the container, before the first item.

    \sa toFront()
    */
    explicit QAssocMutableIterator(C &container) : c(&container), i(c->begin()), n(c->end()) {}

    /*!
    Moves the iterator to the front of the container, before the first item.

    \sa toBack(), next()
    */
    void toFront() { i = c->begin(); n = c->end(); }
    /*!
    Moves the iterator to the back of the container, after the last item.

    \sa toFront(), previous()
    */
    void toBack() { i = c->end(); n = c->end(); }
    /*!
    Returns \c true if there is at least one item ahead of the iterator;
    otherwise returns \c false.

    \sa hasPrevious(), next()
    */
    bool hasNext() const { return i != c->end(); }
    /*!
    Returns the next item and advances the iterator by one position. The key and
    value of the returned item are then available from key() and value(). Calling
    this function when the iterator is at the back is undefined.

    \sa hasNext(), peekNext(), previous()
    */
    It next() { n = i++; return n; }
    /*!
    Returns the key of the item that was last jumped over by next() or
    previous().

    \sa value()
    */
    const K &key() const { return n.key(); }
    /*!
    Returns a reference to the value of the item that was last jumped over by
    next() or previous().

    \sa key(), setValue()
    */
    V &value() const { return n.value(); }
    /*!
    Replaces the value of the item that was last jumped over with \a v. Does
    nothing if there is no such item.

    \sa value(), remove()
    */
    void setValue(const V &v) { if (n != c->end()) n.value() = v; }
    /*!
    Removes the item that was last jumped over by next() or previous(). The
    iterator then sits at the item that followed the removed one. Does nothing if
    there is no such item.

    \sa setValue()
    */
    void remove() {
        if (n == c->end())
            return;
        i = c->erase(n); // the iterator now sits on the item that followed the removed one
        n = c->end();
    }
    /*!
    Returns the next item without moving the iterator. Calling this function when
    the iterator is at the back is undefined.

    \sa hasNext(), next(), peekPrevious()
    */
    It peekNext() const { return i; }
    /*!
    Searches for the value \a v forward from the iterator position. Returns
    \c true if an item with that value is found, leaving the iterator just after
    it so that key() and value() describe it; otherwise returns \c false and
    leaves the iterator at the back.

    \sa findPrevious()
    */
    bool findNext(const V &v) {
        while (i != c->end())
            if (*(n = i++) == v)
                return true;
        n = c->end();
        return false;
    }
    /*!
    Returns \c true if there is at least one item behind the iterator; otherwise
    returns \c false. Only available when the container is bidirectional.

    \sa hasNext(), previous()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    bool hasPrevious() const { return i != c->begin(); }
    /*!
    Returns the previous item and moves the iterator back by one position. Only
    available when the container is bidirectional. Calling this function when the
    iterator is at the front is undefined.

    \sa hasPrevious(), peekPrevious(), next()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    It previous() { n = --i; return n; }
    /*!
    Returns the previous item without moving the iterator. Only available when
    the container is bidirectional. Calling this function when the iterator is at
    the front is undefined.

    \sa hasPrevious(), previous(), peekNext()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    It peekPrevious() const { auto p = i; return --p; }
    /*!
    Searches for the value \a v backward from the iterator position. Returns
    \c true if an item with that value is found, leaving the iterator just before
    it so that key() and value() describe it; otherwise returns \c false and
    leaves the iterator at the front. Only available when the container is
    bidirectional.

    \sa findNext()
    */
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    bool findPrevious(const V &v) {
        while (i != c->begin())
            if (*(n = --i) == v)
                return true;
        n = c->end();
        return false;
    }

private:
    C *c;
    It i, n;
};

template <typename K, typename V>
using QMapIterator = QAssocConstIterator<QMap<K, V>, K, V, true>;
template <typename K, typename V>
using QMutableMapIterator = QAssocMutableIterator<QMap<K, V>, K, V, true>;
template <typename K, typename V>
using QMultiMapIterator = QAssocConstIterator<QMultiMap<K, V>, K, V, true>;
template <typename K, typename V>
using QMutableMultiMapIterator = QAssocMutableIterator<QMultiMap<K, V>, K, V, true>;
template <typename K, typename V>
using QHashIterator = QAssocConstIterator<QHash<K, V>, K, V, false>;
template <typename K, typename V>
using QMutableHashIterator = QAssocMutableIterator<QHash<K, V>, K, V, false>;

/*!
    \class QSetIterator
    \brief The QSetIterator class provides a Java-style const iterator for QSet.

    QSetIterator<T> iterates forward over a set without modifying it. The
    iterator points between values: next() returns the value it jumps over. A
    new iterator is positioned before the first value.

    \code
    QSet<int> set = {1, 2, 3};
    QSetIterator<int> it(set);
    while (it.hasNext())
        qDebug() << it.next();
    \endcode

    \differences
    The iterator moves forward only: there is no \c toBack(), \c hasPrevious(),
    \c previous(), \c peekPrevious() or \c findPrevious(), and it cannot be
    assigned a new set. There is no mutable counterpart.
    \enddifferences

    \sa QListIterator, QSet
*/
template <typename T>
class QSetIterator {
public:
    /*!
    Constructs an iterator for traversing \a set. The iterator is set to be at
    the front of the set, before the first value.

    \sa toFront()
    */
    explicit QSetIterator(const QSet<T> &set) : c(&set), i(c->begin()) {}
    /*!
    Moves the iterator to the front of the set, before the first value.

    \sa next()
    */
    void toFront() { i = c->begin(); }
    /*!
    Returns \c true if there is at least one value ahead of the iterator;
    otherwise returns \c false.

    \sa next()
    */
    bool hasNext() const { return i != c->end(); }
    /*!
    Returns the next value and advances the iterator by one position. Calling
    this function when the iterator is at the back of the set is undefined.

    \sa hasNext(), peekNext()
    */
    const T &next() { return *i++; }
    /*!
    Returns the next value without moving the iterator. Calling this function
    when the iterator is at the back of the set is undefined.

    \sa hasNext(), next()
    */
    const T &peekNext() const { return *i; }
    /*!
    Searches for \a v forward from the iterator position. Returns \c true if a
    value equal to \a v is found, leaving the iterator just after it; otherwise
    returns \c false and leaves the iterator at the back of the set.
    */
    bool findNext(const T &v) {
        while (hasNext())
            if (next() == v)
                return true;
        return false;
    }

private:
    const QSet<T> *c;
    typename QSet<T>::const_iterator i;
};
