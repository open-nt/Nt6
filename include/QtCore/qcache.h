// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>

#include <list>
#include <utility>

// Least-recently-used object cache. It owns the objects inserted into it.
/*!
    \class QCache
    \brief The QCache class is a template class that provides a cache.

    QCache<Key, T> stores objects of type \c T* under keys of type \c Key and
    owns them. Each object has a cost, and the cache keeps the total cost
    within maxCost() by deleting the least recently used objects. Looking up
    an object with object() marks it as the most recently used.

    \code
    QCache<QString, QImage> cache(10000);

    QImage *loadImage(const QString &name)
    {
        if (QImage *image = cache.object(name))
            return image;
        auto *image = new QImage(name);
        cache.insert(name, image, image->sizeInBytes());
        return image;
    }
    \endcode

    The key type needs the requirements of QHash keys. A QCache cannot be
    copied.

    \differences
    There is no \c emplace(). The cache is not copyable, and objects are
    reached only through object(), operator[]() and take().
    \enddifferences

    \sa QContiguousCache, QHash
*/
template <typename Key, typename T>
class QCache {
public:
    /*!
    Constructs a cache whose maximum total cost is \a maxCost.

    \sa setMaxCost()
    */
    explicit QCache(qsizetype maxCost = 100) : m_max(maxCost) {}
    /*!
    Destroys the cache and deletes all the objects it contains.
    */
    ~QCache() { clear(); }
    QCache(const QCache &) = delete;
    QCache &operator=(const QCache &) = delete;

    /*!
    Returns the maximum allowed total cost of the cache.

    \sa setMaxCost(), totalCost()
    */
    qsizetype maxCost() const noexcept { return m_max; }
    /*!
    Sets the maximum allowed total cost of the cache to \a cost. If the current
    total cost is greater, the least recently used objects are deleted until the
    total cost fits.

    \sa maxCost(), totalCost()
    */
    void setMaxCost(qsizetype cost) { m_max = cost; trim(m_max); }
    /*!
    Returns the total cost of the objects currently in the cache. This is never
    more than maxCost().

    \sa maxCost(), insert()
    */
    qsizetype totalCost() const noexcept { return m_total; }
    /*!
    Returns the number of objects in the cache.

    \sa isEmpty(), count()
    */
    qsizetype size() const noexcept { return static_cast<qsizetype>(m_order.size()); }
    /*!
    Returns the number of objects in the cache; the same as size().
    */
    qsizetype count() const noexcept { return size(); }
    /*!
    Returns \c true if the cache contains no objects; otherwise returns \c false.

    \sa size()
    */
    bool isEmpty() const noexcept { return m_order.empty(); }
    /*!
    Returns \c true if the cache contains an object associated with \a key;
    otherwise returns \c false. The object is not marked as recently used.

    \sa object()
    */
    bool contains(const Key &key) const { return m_index.contains(key); }

    /*!
    Returns the keys of all objects in the cache, from the most recently used to
    the least recently used.

    \sa object()
    */
    QList<Key> keys() const {
        QList<Key> r;
        for (const Node &n : m_order)
            r.append(n.key);
        return r;
    }

    // Takes ownership. Deletes `object` and returns false if `cost` is too high.
    /*!
    Inserts \a object into the cache under \a key with the given \a cost,
    replacing any object already stored under that key. The cache takes ownership
    of \a object. If the total cost would exceed maxCost(), the least recently
    used objects are deleted to make room.

    Returns \c true if the object was inserted. If \a cost is greater than
    maxCost(), \a object is deleted immediately and the function returns
    \c false.

    \sa object(), remove(), take()
    */
    bool insert(const Key &key, T *object, qsizetype cost = 1) {
        remove(key);
        if (cost > m_max) {
            delete object;
            return false;
        }
        trim(m_max - cost);
        m_order.push_front(Node{key, object, cost});
        m_index.insert(key, m_order.begin());
        m_total += cost;
        return true;
    }
    /*!
    Returns the object associated with \a key and marks it as the most recently
    used, or \c nullptr if there is no such object. The cache keeps ownership of
    the returned object.

    \sa insert(), contains(), take()
    */
    T *object(const Key &key) const {
        auto it = m_index.find(key);
        if (it == m_index.end())
            return nullptr;
        m_order.splice(m_order.begin(), m_order, *it);
        return m_order.front().object;
    }
    /*!
    Returns the object associated with \a key, or \c nullptr if there is none;
    the same as object().
    */
    T *operator[](const Key &key) const { return object(key); }
    /*!
    Deletes the object associated with \a key. Returns \c true if an object was
    removed; otherwise returns \c false.

    \sa take(), clear()
    */
    bool remove(const Key &key) {
        T *o = take(key);
        delete o;
        return o != nullptr;
    }
    // Returns the object without deleting it; the caller owns it.
    /*!
    Removes the object associated with \a key from the cache and returns it, or
    returns \c nullptr if there is none. The caller becomes the owner of the
    returned object.

    \sa remove(), insert()
    */
    T *take(const Key &key) {
        auto it = m_index.find(key);
        if (it == m_index.end())
            return nullptr;
        return unlink(*it);
    }
    /*!
    Deletes all objects in the cache.

    \sa remove()
    */
    void clear() {
        for (Node &n : m_order)
            delete n.object;
        m_order.clear();
        m_index.clear();
        m_total = 0;
    }

private:
    struct Node {
        Key key;
        T *object;
        qsizetype cost;
    };
    using NodeList = std::list<Node>;

    T *unlink(typename NodeList::iterator it) {
        T *o = it->object;
        m_total -= it->cost;
        m_index.remove(it->key);
        m_order.erase(it);
        return o;
    }
    void trim(qsizetype limit) {
        while (m_total > limit && !m_order.empty())
            delete unlink(std::prev(m_order.end()));
    }

    mutable NodeList m_order; // most recently used first
    QHash<Key, typename NodeList::iterator> m_index;
    qsizetype m_max;
    qsizetype m_total = 0;
};
