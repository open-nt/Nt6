// MIT License - see LICENSE
#pragma once

#include <QtCore/qassoc_p.h>

#include <map>

template <typename K, typename V>
class QMap : public QtPrivate::AssocContainer<std::map<K, V>, K, V, false> {
    using Base = QtPrivate::AssocContainer<std::map<K, V>, K, V, false>;

public:
    using Base::Base;
    QMap() = default;

    const K &firstKey() const { return this->d.begin()->first; }
    const K &lastKey() const { return this->d.rbegin()->first; }
    V &first() { return this->d.begin()->second; }
    const V &first() const { return this->d.begin()->second; }
    V &last() { return this->d.rbegin()->second; }
    const V &last() const { return this->d.rbegin()->second; }
    typename Base::iterator lowerBound(const K &k) { return typename Base::iterator(this->d.lower_bound(k)); }
    typename Base::const_iterator lowerBound(const K &k) const { return typename Base::const_iterator(this->d.lower_bound(k)); }
    typename Base::iterator upperBound(const K &k) { return typename Base::iterator(this->d.upper_bound(k)); }
    typename Base::const_iterator upperBound(const K &k) const { return typename Base::const_iterator(this->d.upper_bound(k)); }
    V takeFirst() { return this->take(firstKey()); }
};

template <typename K, typename V>
class QMultiMap : public QtPrivate::AssocContainer<std::multimap<K, V>, K, V, true> {
    using Base = QtPrivate::AssocContainer<std::multimap<K, V>, K, V, true>;

public:
    using Base::Base;
    QMultiMap() = default;

    QList<K> uniqueKeys() const {
        QList<K> r;
        for (auto it = this->d.begin(); it != this->d.end(); it = this->d.upper_bound(it->first))
            r.append(it->first);
        return r;
    }
    const K &firstKey() const { return this->d.begin()->first; }
    const K &lastKey() const { return this->d.rbegin()->first; }
    typename Base::iterator lowerBound(const K &k) { return typename Base::iterator(this->d.lower_bound(k)); }
    typename Base::iterator upperBound(const K &k) { return typename Base::iterator(this->d.upper_bound(k)); }
};
