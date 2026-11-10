// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <deque>
#include <utility>
#include <limits>

// A bounded window over a logically contiguous range of indexes.
/*!
    \class QContiguousCache
    \brief The QContiguousCache class is a template class that provides an efficient cache for a contiguous range of indexes.

    QContiguousCache<T> holds a window of at most capacity() items that are
    addressed by an integer index, such as the visible rows of a very long
    list. The window covers the indexes firstIndex() through lastIndex().
    Appending or prepending items slides the window and evicts items from the
    opposite end once the cache is full.

    \code
    QContiguousCache<QString> rows(100);
    rows.insert(5000, "row 5000");
    rows.append("row 5001");
    QString s = rows.at(5001);
    \endcode

    \differences
    QContiguousCache is implemented on top of \c std::deque and is not
    implicitly shared, so copying it copies every item, and detach() does
    nothing.
    \enddifferences

    \sa QCache, QList
*/
template <typename T>
class QContiguousCache {
public:
    /*!
    Constructs a cache that can hold up to \a capacity items. The cache is
    initially empty and its first index is 0.

    \sa setCapacity()
    */
    explicit QContiguousCache(qsizetype capacity = 0) : m_cap(capacity) {}

    /*!
    Returns the maximum number of items the cache can hold.

    \sa setCapacity(), size()
    */
    qsizetype capacity() const noexcept { return m_cap; }
    /*!
    Returns the number of items in the cache; the same as size().
    */
    qsizetype count() const noexcept { return size(); }
    /*!
    Returns the number of items currently in the cache.

    \sa capacity(), isEmpty()
    */
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    /*!
    Returns \c true if the cache contains no items; otherwise returns \c false.

    \sa size()
    */
    bool isEmpty() const noexcept { return d.empty(); }
    /*!
    Returns \c true if the cache holds as many items as its capacity; otherwise
    returns \c false.

    \sa available(), capacity()
    */
    bool isFull() const noexcept { return size() == m_cap; }
    /*!
    Returns the number of items that can still be added before the cache is full.

    \sa isFull(), capacity()
    */
    qsizetype available() const noexcept { return m_cap - size(); }
    /*!
    Returns the index of the first item in the cache. The cache covers the
    indexes from firstIndex() to lastIndex().

    \sa lastIndex(), containsIndex()
    */
    qsizetype firstIndex() const noexcept { return m_first; }
    /*!
    Returns the index of the last item in the cache. If the cache is empty, the
    result is firstIndex() - 1.

    \sa firstIndex(), containsIndex()
    */
    qsizetype lastIndex() const noexcept { return m_first + size() - 1; }
    /*!
    Returns \c true if the cache contains an item at index \a i; otherwise
    returns \c false.

    \sa firstIndex(), lastIndex()
    */
    bool containsIndex(qsizetype i) const noexcept { return i >= m_first && i <= lastIndex(); }
    /*!
    Returns \c true if the indexes of the cache are far enough from the limits of
    \c qsizetype that further items can be added safely; otherwise returns
    \c false.

    \sa normalizeIndexes()
    */
    bool areIndexesValid() const noexcept {
        return m_first >= 0 && m_first < std::numeric_limits<qsizetype>::max() - m_cap;
    }
    /*!
    Moves the indexes as close to zero as possible while keeping each item's
    index modulo capacity() unchanged. Use this when areIndexesValid() returns
    \c false.

    \sa areIndexesValid(), firstIndex()
    */
    void normalizeIndexes() {
        if (m_cap > 0)
            m_first = ((m_first % m_cap) + m_cap) % m_cap;
    }

    /*!
    Sets the capacity of the cache to \a c. If the cache holds more than \a c
    items, the items at the front are removed until it fits.

    \sa capacity()
    */
    void setCapacity(qsizetype c) {
        while (size() > c)
            removeFirst();
        m_cap = c;
    }
    /*!
    Removes all items from the cache and resets the first index to 0.

    \sa isEmpty()
    */
    void clear() { d.clear(); m_first = 0; }
    /*!
        \since 4.8
        Swaps this cache with \a other.
    */
    void swap(QContiguousCache &other) noexcept {
        d.swap(other.d);
        std::swap(m_cap, other.m_cap);
        std::swap(m_first, other.m_first);
    }
    /*!
        Does nothing: the cache is never shared.
    */
    void detach() {}

    /*!
    Appends \a v after the last item. If the cache is full, the first item is
    removed to make room, and firstIndex() advances. Does nothing if the capacity
    is 0 or less.

    \sa prepend(), insert()
    */
    void append(const T &v) {
        if (m_cap <= 0)
            return;
        if (isFull())
            removeFirst();
        d.push_back(v);
    }
    /*!
    Prepends \a v before the first item, giving it the index firstIndex() - 1. If
    the cache is full, the last item is removed to make room. Does nothing if the
    capacity is 0 or less.

    \sa append(), insert()
    */
    void prepend(const T &v) {
        if (m_cap <= 0)
            return;
        if (isFull())
            d.pop_back();
        d.push_front(v);
        --m_first;
    }
    // An index outside the contiguous range discards the current contents.
    /*!
    Inserts \a v at index \a pos. If \a pos is inside the cache, the item at
    that index is replaced. If \a pos is just after the last item or just before
    the first, the item is appended or prepended. Any other index discards the
    current contents and starts a new range at \a pos.

    \sa append(), prepend(), containsIndex()
    */
    void insert(qsizetype pos, const T &v) {
        if (m_cap <= 0)
            return;
        if (containsIndex(pos)) {
            d[static_cast<std::size_t>(pos - m_first)] = v;
        } else if (pos == lastIndex() + 1 && !isEmpty()) {
            append(v);
        } else if (pos == m_first - 1 && !isEmpty()) {
            prepend(v);
        } else {
            d.clear();
            d.push_back(v);
            m_first = pos;
        }
    }

    /*!
    Returns a reference to the item at index \a i. The index must satisfy
    containsIndex().
    */
    const T &at(qsizetype i) const { return d[static_cast<std::size_t>(i - m_first)]; }
    /*!
    Returns a reference to the item at index \a i. If the cache does not contain
    that index, a default-constructed item is inserted there first, as insert()
    would, which may discard the current contents.

    \sa at(), insert()
    */
    T &operator[](qsizetype i) {
        if (!containsIndex(i))
            insert(i, T());
        return d[static_cast<std::size_t>(i - m_first)];
    }
    /*!
    \overload

    Returns a reference to the item at index \a i, which must satisfy
    containsIndex(); the same as at().
    */
    const T &operator[](qsizetype i) const { return at(i); }
    /*!
    Returns a reference to the first item. The cache must not be empty.

    \sa last(), firstIndex()
    */
    T &first() { return d.front(); }
    /*!
    \overload
    */
    const T &first() const { return d.front(); }
    /*!
    Returns a reference to the last item. The cache must not be empty.

    \sa first(), lastIndex()
    */
    T &last() { return d.back(); }
    /*!
    \overload
    */
    const T &last() const { return d.back(); }

    /*!
    Removes the first item. The first index advances by one. The cache must not
    be empty.

    \sa removeLast(), takeFirst()
    */
    void removeFirst() {
        d.pop_front();
        ++m_first;
    }
    /*!
    Removes the last item. The cache must not be empty.

    \sa removeFirst(), takeLast()
    */
    void removeLast() { d.pop_back(); }
    /*!
    Removes the first item and returns it. The cache must not be empty.

    \sa removeFirst(), takeLast()
    */
    T takeFirst() {
        T t = std::move(d.front());
        removeFirst();
        return t;
    }
    /*!
    Removes the last item and returns it. The cache must not be empty.

    \sa removeLast(), takeFirst()
    */
    T takeLast() {
        T t = std::move(d.back());
        removeLast();
        return t;
    }

    friend bool operator==(const QContiguousCache &a, const QContiguousCache &b) {
        return a.m_first == b.m_first && a.m_cap == b.m_cap && a.d == b.d;
    }
    friend bool operator!=(const QContiguousCache &a, const QContiguousCache &b) { return !(a == b); }

private:
    std::deque<T> d;
    qsizetype m_cap;
    qsizetype m_first = 0;
};
