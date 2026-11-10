// MIT License - see LICENSE
#pragma once

#include <QtCore/qassoc_p.h>

#include <unordered_map>
#include <unordered_set>

/*!
    \class QHash
    \brief The QHash class is a template class that provides a hash-table-based dictionary.

    QHash<Key, T> stores key-value pairs and finds values by key in constant
    time on average. Each key appears at most once; inserting an existing key
    replaces its value. Items are stored in no particular order. Use
    QMultiHash to store several values under one key. The key type needs
    \c operator== and either a \c qHash() overload or a \c std::hash
    specialization.

    \code
    QHash<QString, int> stock;
    stock.insert("apple", 3);
    stock["pear"] = 7;
    int apples = stock.value("apple");
    \endcode

    The size, lookup, insertion, removal and iteration functions are shared
    with the other associative containers and behave as they do in Qt.
    keyValueBegin(), keyValueEnd() and \c asKeyValueRange() visit the key
    and value pairs, as described in QKeyValueIterator.

    \differences
    QHash is implemented on top of \c std::unordered_map and has no implicit
    sharing: copying a hash copies all of its elements. The iteration order
    and the value returned by capacity() depend on the standard library.
    \enddifferences

    \sa QMultiHash, QMap, QSet
*/
template <typename K, typename V>
class QHash : public QtPrivate::AssocContainer<std::unordered_map<K, V, QtHasher<K>>, K, V, false> {
    using Base = QtPrivate::AssocContainer<std::unordered_map<K, V, QtHasher<K>>, K, V, false>;

public:
    using Base::Base;
    /*!
        Constructs an empty hash.
    */
    QHash() = default;
    /*!
        Prepares the hash for holding at least \a n items without rehashing.

        \sa capacity()
    */
    void reserve(qsizetype n) { this->d.reserve(static_cast<std::size_t>(n)); }
    /*!
        Returns the number of buckets in the hash table.

        \sa reserve()
    */
    qsizetype capacity() const { return static_cast<qsizetype>(this->d.bucket_count()); }
    /*!
        Shrinks the hash table to the smallest size that holds the items.

        \sa capacity()
    */
    void squeeze() { this->d.rehash(0); }
};

/*!
    \class QMultiHash
    \brief The QMultiHash class is a template class that provides a hash-table-based multi-valued dictionary.

    QMultiHash<Key, T> is like QHash, but insert() always adds a new item, so
    several values can be stored under the same key. values(key) returns all
    values stored under a key, and count(key) returns how many there are.

    \code
    QMultiHash<QString, int> hash;
    hash.insert("a", 1);
    hash.insert("a", 2);
    qsizetype n = hash.count("a"); // 2
    \endcode

    \differences
    QMultiHash is implemented on top of \c std::unordered_multimap and is not
    implicitly shared. The order of values
    stored under one key is not specified.
    \enddifferences

    \sa QHash, QMultiMap
*/
template <typename K, typename V>
class QMultiHash : public QtPrivate::AssocContainer<std::unordered_multimap<K, V, QtHasher<K>>, K, V, true> {
    using Base = QtPrivate::AssocContainer<std::unordered_multimap<K, V, QtHasher<K>>, K, V, true>;

public:
    using Base::Base;
    /*!
        Constructs an empty multi-hash.
    */
    QMultiHash() = default;
    /*!
        Prepares the hash for holding at least \a n items without rehashing.
    */
    void reserve(qsizetype n) { this->d.reserve(static_cast<std::size_t>(n)); }
    /*!
        Returns the number of buckets in the hash table.

        \sa reserve(), squeeze()
    */
    qsizetype capacity() const { return static_cast<qsizetype>(this->d.bucket_count()); }
    /*!
        Shrinks the hash table to the smallest size that holds the items.

        \sa capacity()
    */
    void squeeze() { this->d.rehash(0); }
    /*!
        Returns the keys of the hash, each key listed once even if it has
        several values. The order of the keys is unspecified.
    */
    QList<K> uniqueKeys() const {
        QHash<K, bool> seen;
        QList<K> r;
        for (const auto &p : this->d)
            if (!seen.contains(p.first)) {
                seen.insert(p.first, true);
                r.append(p.first);
            }
        return r;
    }
};

/*!
    \class QSet
    \brief The QSet class is a template class that provides a hash-table-based set.

    QSet<T> stores unique values and tests for membership in constant time on
    average. The values are kept in no particular order. The value type needs
    \c operator== and either a \c qHash() overload or a \c std::hash
    specialization.

    \code
    QSet<QString> names;
    names << "Alice" << "Bob";
    names.insert("Alice"); // already present, no change
    bool hasBob = names.contains("Bob");
    \endcode

    \differences
    QSet is implemented on top of \c std::unordered_set and has no implicit
    sharing. Its iterators are constant. The predicate passed to removeIf() is
    called with an iterator rather than with the value. \c toList() is
    provided although it was removed from Qt 6.
    \enddifferences

    \sa QHash, QSetIterator
*/
template <typename T>
class QSet {
public:
    /*!
        The underlying \c std::unordered_set type used for storage.
    */
    using Container = std::unordered_set<T, QtHasher<T>>;
    /*!
        The iterator type. It is constant, so the values of a set cannot be
        modified through it.
    */
    using iterator = typename Container::const_iterator;
    /*!
        The constant iterator type.
    */
    using const_iterator = typename Container::const_iterator;
    /*!
        The type of the values stored in the set, \c T.
    */
    using value_type = T;

    /*!
        Constructs an empty set.
    */
    QSet() = default;
    /*!
        \overload

        Constructs a set with the values in the initializer list \a l.
    */
    QSet(std::initializer_list<T> l) : d(l) {}
    /*!
        \overload

        Constructs a set with the values in the range from \a first up to, but not
        including, \a last.
    */
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QSet(It first, It last) : d(first, last) {}

    /*!
        Returns the number of values in the set.

        \sa isEmpty(), count()
    */
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    /*!
        Returns the number of values in the set; the same as size().
    */
    qsizetype count() const noexcept { return size(); }
    /*!
        Returns \c true if the set contains no values; otherwise returns \c false.

        \sa size()
    */
    bool isEmpty() const noexcept { return d.empty(); }
    /*!
        Removes all values from the set.

        \sa remove()
    */
    void clear() { d.clear(); }
    /*!
        Prepares the set for holding at least \a n values without rehashing.
    */
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    /*!
        Returns the number of buckets in the hash table of the set.

        \sa reserve(), squeeze()
    */
    qsizetype capacity() const { return static_cast<qsizetype>(d.bucket_count()); }
    /*!
        Shrinks the hash table to the smallest size that holds the values.

        \sa capacity()
    */
    void squeeze() { d.rehash(0); }
    /*!
        Returns an iterator to the value \a value, or constEnd() if the set
        does not contain it.

        \sa find(), contains()
    */
    const_iterator constFind(const T &value) const { return d.find(value); }
    /*!
        \since 5.12
        Returns \c true if every value of this set is also in \a other.

        \sa contains(), intersects()
    */
    bool isSubsetOf(const QSet &other) const {
        if (d.size() > other.d.size())
            return false;
        for (const T &t : d)
            if (!other.contains(t))
                return false;
        return true;
    }
    /*!
        Returns \c true if the set contains the value \a t; otherwise returns
        \c false.

        \sa insert(), find()
    */
    bool contains(const T &t) const { return d.find(t) != d.end(); }
    /*!
        \overload

        Returns \c true if the set contains every value of the set \a other;
        otherwise returns \c false.
    */
    bool contains(const QSet &other) const {
        for (const T &t : other.d)
            if (!contains(t))
                return false;
        return true;
    }
    /*!
        Inserts the value \a t into the set if it is not already present, and
        returns an iterator to the value in the set.

        \sa remove(), contains()
    */
    iterator insert(const T &t) { return d.insert(t).first; }
    /*!
        Removes the value \a t from the set. Returns \c true if the value was
        present; otherwise returns \c false.

        \sa insert(), clear()
    */
    bool remove(const T &t) { return d.erase(t) > 0; }
    /*!
        Removes all values for which \a p returns \c true and returns the number
        of values removed. The predicate is called with an iterator to the value.

        \sa remove()
    */
    template <typename Pred>
    qsizetype removeIf(Pred p) {
        qsizetype n = 0;
        for (auto it = d.begin(); it != d.end();) {
            if (p(it)) { it = d.erase(it); ++n; } else { ++it; }
        }
        return n;
    }
    /*!
        Removes the value that \a it points to and returns an iterator to the
        value that followed it.

        \sa remove()
    */
    iterator erase(const_iterator it) { return d.erase(it); }
    /*!
        Returns an iterator to the value \a t, or end() if the set does not
        contain it.

        \sa contains()
    */
    iterator find(const T &t) const { return d.find(t); }
    /*!
        Inserts all values of the set \a o into this set and returns a reference
        to this set.

        \sa intersect(), subtract()
    */
    QSet &unite(const QSet &o) { d.insert(o.d.begin(), o.d.end()); return *this; }
    /*!
        Removes all values from this set that are not in the set \a o, and
        returns a reference to this set.

        \sa unite(), subtract(), intersects()
    */
    QSet &intersect(const QSet &o) {
        for (auto it = d.begin(); it != d.end();)
            it = o.contains(*it) ? std::next(it) : d.erase(it);
        return *this;
    }
    /*!
        Removes all values of the set \a o from this set and returns a reference
        to this set.

        \sa unite(), intersect()
    */
    QSet &subtract(const QSet &o) {
        for (const T &t : o.d)
            d.erase(t);
        return *this;
    }
    /*!
        Returns \c true if this set and the set \a o have at least one value in
        common; otherwise returns \c false.

        \sa intersect()
    */
    bool intersects(const QSet &o) const {
        for (const T &t : d)
            if (o.contains(t))
                return true;
        return false;
    }
    /*!
        Returns the values of the set as a QList, in unspecified order.

        \sa toList()
    */
    QList<T> values() const { return QList<T>(d.begin(), d.end()); }
    /*!
        Returns the values of the set as a QList; the same as values().
    */
    QList<T> toList() const { return values(); }

    /*!
        Returns a constant iterator to the first value in the set.

        \sa end(), constBegin()
    */
    const_iterator begin() const noexcept { return d.begin(); }
    /*!
        Returns a constant iterator to the position after the last value in the
        set.

        \sa begin(), constEnd()
    */
    const_iterator end() const noexcept { return d.end(); }
    /*!
        Returns a constant iterator to the first value in the set; the same as
        begin().
    */
    const_iterator cbegin() const noexcept { return d.cbegin(); }
    /*!
        Returns a constant iterator to the position after the last value in the
        set; the same as end().
    */
    const_iterator cend() const noexcept { return d.cend(); }
    /*!
        Returns a constant iterator to the first value in the set; the same as
        begin().
    */
    const_iterator constBegin() const noexcept { return d.cbegin(); }
    /*!
        Returns a constant iterator to the position after the last value in the
        set; the same as end().
    */
    const_iterator constEnd() const noexcept { return d.cend(); }

    /*!
        Inserts the value \a t into the set and returns a reference to this set;
        the same as insert(). The calls can be chained.
    */
    QSet &operator<<(const T &t) { insert(t); return *this; }
    /*!
        \overload

        Inserts the value \a t into the set and returns a reference to this set.
    */
    QSet &operator+=(const T &t) { insert(t); return *this; }
    /*!
        Inserts all values of the set \a o into this set and returns a reference
        to this set; the same as unite().
    */
    QSet &operator|=(const QSet &o) { return unite(o); }
    /*!
        \overload

        Inserts all values of the set \a o into this set; the same as unite().
    */
    QSet &operator+=(const QSet &o) { return unite(o); }
    /*!
        Removes all values from this set that are not in the set \a o, and
        returns a reference to this set; the same as intersect().
    */
    QSet &operator&=(const QSet &o) { return intersect(o); }
    /*!
        Removes all values of the set \a o from this set and returns a reference
        to this set; the same as subtract().
    */
    QSet &operator-=(const QSet &o) { return subtract(o); }
    friend QSet operator|(QSet a, const QSet &b) { return a.unite(b); }
    friend QSet operator&(QSet a, const QSet &b) { return a.intersect(b); }
    friend QSet operator-(QSet a, const QSet &b) { return a.subtract(b); }
    friend bool operator==(const QSet &a, const QSet &b) { return a.d == b.d; }
    friend bool operator!=(const QSet &a, const QSet &b) { return a.d != b.d; }

private:
    Container d;
};
