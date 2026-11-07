// MIT License - see LICENSE
#pragma once

#include <QtCore/qassoc_p.h>

#include <unordered_map>
#include <unordered_set>

template <typename K, typename V>
class QHash : public QtPrivate::AssocContainer<std::unordered_map<K, V, QtHasher<K>>, K, V, false> {
    using Base = QtPrivate::AssocContainer<std::unordered_map<K, V, QtHasher<K>>, K, V, false>;

public:
    using Base::Base;
    QHash() = default;
    void reserve(qsizetype n) { this->d.reserve(static_cast<std::size_t>(n)); }
    qsizetype capacity() const { return static_cast<qsizetype>(this->d.bucket_count()); }
};

template <typename K, typename V>
class QMultiHash : public QtPrivate::AssocContainer<std::unordered_multimap<K, V, QtHasher<K>>, K, V, true> {
    using Base = QtPrivate::AssocContainer<std::unordered_multimap<K, V, QtHasher<K>>, K, V, true>;

public:
    using Base::Base;
    QMultiHash() = default;
    void reserve(qsizetype n) { this->d.reserve(static_cast<std::size_t>(n)); }
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

template <typename T>
class QSet {
public:
    using Container = std::unordered_set<T, QtHasher<T>>;
    using iterator = typename Container::const_iterator;
    using const_iterator = typename Container::const_iterator;
    using value_type = T;

    QSet() = default;
    QSet(std::initializer_list<T> l) : d(l) {}
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QSet(It first, It last) : d(first, last) {}

    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    qsizetype count() const noexcept { return size(); }
    bool isEmpty() const noexcept { return d.empty(); }
    void clear() { d.clear(); }
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    bool contains(const T &t) const { return d.find(t) != d.end(); }
    bool contains(const QSet &other) const {
        for (const T &t : other.d)
            if (!contains(t))
                return false;
        return true;
    }
    iterator insert(const T &t) { return d.insert(t).first; }
    bool remove(const T &t) { return d.erase(t) > 0; }
    template <typename Pred>
    qsizetype removeIf(Pred p) {
        qsizetype n = 0;
        for (auto it = d.begin(); it != d.end();) {
            if (p(it)) { it = d.erase(it); ++n; } else { ++it; }
        }
        return n;
    }
    iterator erase(const_iterator it) { return d.erase(it); }
    iterator find(const T &t) const { return d.find(t); }
    QSet &unite(const QSet &o) { d.insert(o.d.begin(), o.d.end()); return *this; }
    QSet &intersect(const QSet &o) {
        for (auto it = d.begin(); it != d.end();)
            it = o.contains(*it) ? std::next(it) : d.erase(it);
        return *this;
    }
    QSet &subtract(const QSet &o) {
        for (const T &t : o.d)
            d.erase(t);
        return *this;
    }
    bool intersects(const QSet &o) const {
        for (const T &t : d)
            if (o.contains(t))
                return true;
        return false;
    }
    QList<T> values() const { return QList<T>(d.begin(), d.end()); }
    QList<T> toList() const { return values(); }

    const_iterator begin() const noexcept { return d.begin(); }
    const_iterator end() const noexcept { return d.end(); }
    const_iterator cbegin() const noexcept { return d.cbegin(); }
    const_iterator cend() const noexcept { return d.cend(); }
    const_iterator constBegin() const noexcept { return d.cbegin(); }
    const_iterator constEnd() const noexcept { return d.cend(); }

    QSet &operator<<(const T &t) { insert(t); return *this; }
    QSet &operator+=(const T &t) { insert(t); return *this; }
    QSet &operator|=(const QSet &o) { return unite(o); }
    QSet &operator+=(const QSet &o) { return unite(o); }
    QSet &operator&=(const QSet &o) { return intersect(o); }
    QSet &operator-=(const QSet &o) { return subtract(o); }
    friend QSet operator|(QSet a, const QSet &b) { return a.unite(b); }
    friend QSet operator&(QSet a, const QSet &b) { return a.intersect(b); }
    friend QSet operator-(QSet a, const QSet &b) { return a.subtract(b); }
    friend bool operator==(const QSet &a, const QSet &b) { return a.d == b.d; }
    friend bool operator!=(const QSet &a, const QSet &b) { return a.d != b.d; }

private:
    Container d;
};
