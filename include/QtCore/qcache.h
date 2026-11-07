// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>

#include <list>
#include <utility>

// Least-recently-used object cache. It owns the objects inserted into it.
template <typename Key, typename T>
class QCache {
public:
    explicit QCache(qsizetype maxCost = 100) : m_max(maxCost) {}
    ~QCache() { clear(); }
    QCache(const QCache &) = delete;
    QCache &operator=(const QCache &) = delete;

    qsizetype maxCost() const noexcept { return m_max; }
    void setMaxCost(qsizetype cost) { m_max = cost; trim(m_max); }
    qsizetype totalCost() const noexcept { return m_total; }
    qsizetype size() const noexcept { return static_cast<qsizetype>(m_order.size()); }
    qsizetype count() const noexcept { return size(); }
    bool isEmpty() const noexcept { return m_order.empty(); }
    bool contains(const Key &key) const { return m_index.contains(key); }

    QList<Key> keys() const {
        QList<Key> r;
        for (const Node &n : m_order)
            r.append(n.key);
        return r;
    }

    // Takes ownership. Deletes `object` and returns false if `cost` is too high.
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
    T *object(const Key &key) const {
        auto it = m_index.find(key);
        if (it == m_index.end())
            return nullptr;
        m_order.splice(m_order.begin(), m_order, *it);
        return m_order.front().object;
    }
    T *operator[](const Key &key) const { return object(key); }
    bool remove(const Key &key) {
        T *o = take(key);
        delete o;
        return o != nullptr;
    }
    // Returns the object without deleting it; the caller owns it.
    T *take(const Key &key) {
        auto it = m_index.find(key);
        if (it == m_index.end())
            return nullptr;
        return unlink(*it);
    }
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
