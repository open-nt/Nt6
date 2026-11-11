// MIT License - see LICENSE
#pragma once

#include <QtCore/qassoc_p.h>

#include <iterator>
#include <map>

/*!
    \class QMap
    \brief The QMap class is a template class that provides an ordered associative container.

    QMap<Key, T> stores key-value pairs sorted by key and looks up values in
    logarithmic time. Each key appears at most once; inserting an existing key
    replaces its value. Use QMultiMap to store several values under one key.
    The key type needs \c operator<.

    \code
    QMap<QString, int> ages;
    ages.insert("Alice", 30);
    ages["Bob"] = 25;
    for (auto it = ages.begin(); it != ages.end(); ++it)
        qDebug() << it.key() << it.value();
    \endcode

    The size, lookup, insertion, removal and iteration functions are shared
    with the other associative containers and behave as they do in Qt.
    keyValueBegin(), keyValueEnd() and \c asKeyValueRange() visit the key
    and value pairs, as described in QKeyValueIterator.

    \differences
    QMap is implemented on top of \c std::map and has no implicit sharing:
    copying a map copies all of its elements. Iterator invalidation follows
    \c std::map rather than Qt.
    \enddifferences

    \sa QMultiMap, QHash
*/
template <typename K, typename V>
class QMap : public QtPrivate::AssocContainer<std::map<K, V>, K, V, false> {
    using Base = QtPrivate::AssocContainer<std::map<K, V>, K, V, false>;

public:
    using Base::Base;
    /*!
        Constructs an empty map.
    */
    QMap() = default;

    /*!
        Returns a reference to the smallest key in the map. The map must not
        be empty.

        \sa lastKey(), first()
    */
    const K &firstKey() const { return this->d.begin()->first; }
    /*!
        Returns a reference to the largest key in the map. The map must not be
        empty.

        \sa firstKey(), last()
    */
    const K &lastKey() const { return this->d.rbegin()->first; }
    /*!
        Returns a reference to the value associated with the smallest key. The
        map must not be empty.

        \sa last(), firstKey()
    */
    V &first() { return this->d.begin()->second; }
    /*!
        \overload
    */
    const V &first() const { return this->d.begin()->second; }
    /*!
        Returns a reference to the value associated with the largest key. The
        map must not be empty.

        \sa first(), lastKey()
    */
    V &last() { return this->d.rbegin()->second; }
    /*!
        \overload
    */
    const V &last() const { return this->d.rbegin()->second; }
    /*!
        Returns an iterator to the first item whose key is not less than
        \a k, or end() if there is none.

        \sa upperBound()
    */
    typename Base::iterator lowerBound(const K &k) { return typename Base::iterator(this->d.lower_bound(k)); }
    /*!
        \overload
    */
    typename Base::const_iterator lowerBound(const K &k) const { return typename Base::const_iterator(this->d.lower_bound(k)); }
    /*!
        Returns an iterator to the first item whose key is greater than
        \a k, or end() if there is none.

        \sa lowerBound()
    */
    typename Base::iterator upperBound(const K &k) { return typename Base::iterator(this->d.upper_bound(k)); }
    /*!
        \overload
    */
    typename Base::const_iterator upperBound(const K &k) const { return typename Base::const_iterator(this->d.upper_bound(k)); }
    /*!
        Removes the item with the smallest key and returns its value. The map
        must not be empty.

        \sa first()
    */
    V takeFirst() { return this->take(firstKey()); }
    /*!
        Returns a \c std::map with the items of the map.
    */
    std::map<K, V> toStdMap() const & { return this->d; }
    /*!
        \overload
        \since 6.0
        Moves the items out of the map.
    */
    std::map<K, V> toStdMap() && { return std::move(this->d); }
};

/*!
    \class QMultiMap
    \brief The QMultiMap class is a template class that provides an ordered associative container with multiple values per key.

    QMultiMap<Key, T> is like QMap, but insert() always adds a new item, so
    several values can be stored under the same key. Items with equal keys
    are kept in insertion order. values(key) returns all values stored under a
    key, and count(key) returns how many there are.

    \code
    QMultiMap<QString, int> scores;
    scores.insert("Alice", 10);
    scores.insert("Alice", 20);
    QList<int> aliceScores = scores.values("Alice"); // {10, 20}
    \endcode

    \differences
    QMultiMap is implemented on top of \c std::multimap and is not implicitly
    shared. Values stored under one key are returned in insertion order,
    whereas Qt returns the most recently inserted first, so first() and last()
    return the value inserted first among those of the smallest and the largest
    key.
    \enddifferences

    \sa QMap, QMultiHash
*/
template <typename K, typename V>
class QMultiMap : public QtPrivate::AssocContainer<std::multimap<K, V>, K, V, true> {
    using Base = QtPrivate::AssocContainer<std::multimap<K, V>, K, V, true>;

public:
    using Base::Base;
    /*!
        Constructs an empty multi-map.
    */
    QMultiMap() = default;

    /*!
        Returns the keys of the map in ascending order, each key listed once
        even if it has several values.
    */
    QList<K> uniqueKeys() const {
        QList<K> r;
        for (auto it = this->d.begin(); it != this->d.end(); it = this->d.upper_bound(it->first))
            r.append(it->first);
        return r;
    }
    /*!
        Returns a reference to the smallest key in the map. The map must not
        be empty.

        \sa lastKey()
    */
    const K &firstKey() const { return this->d.begin()->first; }
    /*!
        Returns a reference to the largest key in the map. The map must not be
        empty.

        \sa firstKey()
    */
    const K &lastKey() const { return this->d.rbegin()->first; }
    /*!
        Returns an iterator to the first item whose key is not less than
        \a k, or end() if there is none.

        \sa upperBound()
    */
    typename Base::iterator lowerBound(const K &k) { return typename Base::iterator(this->d.lower_bound(k)); }
    /*!
        Returns an iterator to the first item whose key is greater than
        \a k, or end() if there is none.

        \sa lowerBound()
    */
    typename Base::iterator upperBound(const K &k) { return typename Base::iterator(this->d.upper_bound(k)); }
    /*!
        \overload
        Returns a const iterator to the first item whose key is not less than
        \a k, or end().
    */
    typename Base::const_iterator lowerBound(const K &k) const {
        return typename Base::const_iterator(this->d.lower_bound(k));
    }
    /*!
        \overload
        Returns a const iterator to the first item whose key is greater than
        \a k, or end().
    */
    typename Base::const_iterator upperBound(const K &k) const {
        return typename Base::const_iterator(this->d.upper_bound(k));
    }
    /*!
        Returns a reference to the first value, the one with the smallest key.
        The map must not be empty.

        \sa last(), firstKey()
    */
    V &first() { return this->d.begin()->second; }
    /*!
        \overload
    */
    const V &first() const { return this->d.begin()->second; }
    /*!
        Returns a reference to the last value, the one with the largest key.
        The map must not be empty.

        \sa first(), lastKey()
    */
    V &last() { return std::prev(this->d.end())->second; }
    /*!
        \overload
    */
    const V &last() const { return std::prev(this->d.end())->second; }
    /*!
        Returns a \c std::multimap with the items of the map.
    */
    std::multimap<K, V> toStdMultiMap() const & { return this->d; }
    /*!
        \overload
        \since 6.0
        Moves the items out of the map.
    */
    std::multimap<K, V> toStdMultiMap() && { return std::move(this->d); }
};
