// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qkeyvalueiterator.h>
#include <QtCore/qlist.h>

#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

// Returns a hash of the items of list, in order, mixed with seed.
template <typename T, typename = std::enable_if_t<QtPrivate::IsHashable<T>>>
std::size_t qHash(const QList<T> &list, std::size_t seed = 0) {
    return qHashRange(list.begin(), list.end(), seed);
}

// Uses qHash(key, seed) when available (found via ADL), std::hash otherwise.
template <typename K>
struct QtHasher {
    std::size_t operator()(const K &k) const noexcept {
        if constexpr (QtPrivate::HasQHash<K>::value)
            return qHash(k, 0);
        else
            return std::hash<K>{}(k);
    }
};

namespace QtPrivate {

// Qt-style iterator: key()/value() accessors and `*it` yielding the value.
template <typename It, typename ValueRef>
class AssocIterator {
public:
    using iterator_category = typename std::iterator_traits<It>::iterator_category;
    using value_type = std::remove_cv_t<std::remove_reference_t<ValueRef>>;
    using difference_type = std::ptrdiff_t;
    using pointer = std::remove_reference_t<ValueRef> *;
    using reference = ValueRef;

    AssocIterator() = default;
    explicit AssocIterator(It i) : it(i) {}
    template <typename It2, typename V2,
              typename = std::enable_if_t<std::is_convertible_v<It2, It> && std::is_convertible_v<V2, ValueRef>>>
    AssocIterator(const AssocIterator<It2, V2> &o) : it(o.base()) {}

    const auto &key() const { return it->first; }
    ValueRef value() const { return it->second; }
    ValueRef operator*() const { return it->second; }
    pointer operator->() const { return &it->second; }
    AssocIterator &operator++() { ++it; return *this; }
    AssocIterator operator++(int) { auto t = *this; ++it; return t; }
    AssocIterator &operator--() { --it; return *this; }
    AssocIterator operator--(int) { auto t = *this; --it; return t; }
    friend bool operator==(const AssocIterator &a, const AssocIterator &b) { return a.it == b.it; }
    friend bool operator!=(const AssocIterator &a, const AssocIterator &b) { return a.it != b.it; }
    It base() const { return it; }

private:
    It it{};
};

// Shared implementation of QMap/QHash and their multi variants.
template <typename Container, typename K, typename V, bool Multi>
class AssocContainer {
public:
    using key_type = K;
    using mapped_type = V;
    using iterator = AssocIterator<typename Container::iterator, V &>;
    using const_iterator = AssocIterator<typename Container::const_iterator, const V &>;
    using Iterator = iterator;
    using ConstIterator = const_iterator;
    using size_type = qsizetype;

    AssocContainer() = default;
    AssocContainer(std::initializer_list<std::pair<K, V>> init) {
        for (const auto &p : init)
            insert(p.first, p.second);
    }

    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    qsizetype count() const noexcept { return size(); }
    bool isEmpty() const noexcept { return d.empty(); }
    void clear() { d.clear(); }
    bool contains(const K &k) const { return d.find(k) != d.end(); }

    V value(const K &k) const { auto it = d.find(k); return it == d.end() ? V() : it->second; }
    V value(const K &k, const V &def) const { auto it = d.find(k); return it == d.end() ? def : it->second; }
    template <bool M = Multi, typename = std::enable_if_t<!M>>
    V &operator[](const K &k) { return d[k]; }
    template <bool M = Multi, typename = std::enable_if_t<!M>>
    V operator[](const K &k) const { return value(k); }

    iterator insert(const K &k, const V &v) {
        if constexpr (Multi) {
            return iterator(d.emplace(k, v));
        } else {
            auto it = d.find(k);
            if (it != d.end()) {
                it->second = v;
                return iterator(it);
            }
            return iterator(d.emplace(k, v).first);
        }
    }
    void insert(const AssocContainer &other) {
        for (const auto &p : other.d)
            insert(p.first, p.second);
    }
    template <typename... Args>
    iterator emplace(const K &k, Args &&...args) { return insert(k, V(std::forward<Args>(args)...)); }
    template <bool M = Multi, typename = std::enable_if_t<M>>
    iterator replace(const K &k, const V &v) {
        auto it = d.find(k);
        if (it != d.end()) {
            it->second = v;
            return iterator(it);
        }
        return insert(k, v);
    }

    qsizetype remove(const K &k) { return static_cast<qsizetype>(d.erase(k)); }
    template <typename Pred>
    qsizetype removeIf(Pred pred) {
        qsizetype n = 0;
        for (auto it = d.begin(); it != d.end();) {
            if (pred(iterator(it))) {
                it = d.erase(it);
                ++n;
            } else {
                ++it;
            }
        }
        return n;
    }
    V take(const K &k) {
        auto it = d.find(k);
        if (it == d.end())
            return V();
        V v = std::move(it->second);
        d.erase(it);
        return v;
    }
    iterator erase(const_iterator pos) { return iterator(d.erase(pos.base())); }
    iterator erase(iterator pos) { return iterator(d.erase(pos.base())); }

    K key(const V &v, const K &def = K()) const {
        for (const auto &p : d)
            if (p.second == v)
                return p.first;
        return def;
    }
    QList<K> keys() const {
        QList<K> r;
        for (const auto &p : d)
            r.append(p.first);
        return r;
    }
    QList<K> keys(const V &v) const {
        QList<K> r;
        for (const auto &p : d)
            if (p.second == v)
                r.append(p.first);
        return r;
    }
    QList<V> values() const {
        QList<V> r;
        for (const auto &p : d)
            r.append(p.second);
        return r;
    }
    template <bool M = Multi, typename = std::enable_if_t<M>>
    QList<V> values(const K &k) const {
        QList<V> r;
        auto range = d.equal_range(k);
        for (auto it = range.first; it != range.second; ++it)
            r.append(it->second);
        return r;
    }
    template <bool M = Multi, typename = std::enable_if_t<M>>
    qsizetype count(const K &k) const { return static_cast<qsizetype>(d.count(k)); }

    iterator find(const K &k) { return iterator(d.find(k)); }
    const_iterator find(const K &k) const { return const_iterator(d.find(k)); }
    const_iterator constFind(const K &k) const { return const_iterator(d.find(k)); }
    iterator begin() { return iterator(d.begin()); }
    iterator end() { return iterator(d.end()); }
    const_iterator begin() const { return const_iterator(d.begin()); }
    const_iterator end() const { return const_iterator(d.end()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    const_iterator constBegin() const { return begin(); }
    const_iterator constEnd() const { return end(); }

    using key_value_iterator = QKeyValueIterator<const K &, V &, iterator>;
    using const_key_value_iterator = QKeyValueIterator<const K &, const V &, const_iterator>;
    key_value_iterator keyValueBegin() { return key_value_iterator(begin()); }
    key_value_iterator keyValueEnd() { return key_value_iterator(end()); }
    const_key_value_iterator keyValueBegin() const { return const_key_value_iterator(begin()); }
    const_key_value_iterator keyValueEnd() const { return const_key_value_iterator(end()); }
    const_key_value_iterator constKeyValueBegin() const { return keyValueBegin(); }
    const_key_value_iterator constKeyValueEnd() const { return keyValueEnd(); }
    auto asKeyValueRange() & { return KeyValueRange<AssocContainer &, key_value_iterator>(*this); }
    auto asKeyValueRange() const & { return KeyValueRange<const AssocContainer &, const_key_value_iterator>(*this); }
    auto asKeyValueRange() && { return KeyValueRange<AssocContainer, key_value_iterator>(std::move(*this)); }

    void swap(AssocContainer &o) noexcept { d.swap(o.d); }
    friend bool operator==(const AssocContainer &a, const AssocContainer &b) { return a.d == b.d; }
    friend bool operator!=(const AssocContainer &a, const AssocContainer &b) { return !(a == b); }

protected:
    Container d;
};

} // namespace QtPrivate
