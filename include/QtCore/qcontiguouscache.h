// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <deque>
#include <limits>

// A bounded window over a logically contiguous range of indexes.
template <typename T>
class QContiguousCache {
public:
    explicit QContiguousCache(qsizetype capacity = 0) : m_cap(capacity) {}

    qsizetype capacity() const noexcept { return m_cap; }
    qsizetype count() const noexcept { return size(); }
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    bool isEmpty() const noexcept { return d.empty(); }
    bool isFull() const noexcept { return size() == m_cap; }
    qsizetype available() const noexcept { return m_cap - size(); }
    qsizetype firstIndex() const noexcept { return m_first; }
    qsizetype lastIndex() const noexcept { return m_first + size() - 1; }
    bool containsIndex(qsizetype i) const noexcept { return i >= m_first && i <= lastIndex(); }
    bool areIndexesValid() const noexcept {
        return m_first >= 0 && m_first < std::numeric_limits<qsizetype>::max() - m_cap;
    }
    void normalizeIndexes() {
        if (!areIndexesValid())
            m_first = 0;
    }

    void setCapacity(qsizetype c) {
        while (size() > c)
            removeFirst();
        m_cap = c;
    }
    void clear() { d.clear(); m_first = 0; }

    void append(const T &v) {
        if (m_cap <= 0)
            return;
        if (isFull())
            removeFirst();
        d.push_back(v);
    }
    void prepend(const T &v) {
        if (m_cap <= 0)
            return;
        if (isFull())
            d.pop_back();
        d.push_front(v);
        --m_first;
    }
    // An index outside the contiguous range discards the current contents.
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

    const T &at(qsizetype i) const { return d[static_cast<std::size_t>(i - m_first)]; }
    T &operator[](qsizetype i) {
        if (!containsIndex(i))
            insert(i, T());
        return d[static_cast<std::size_t>(i - m_first)];
    }
    const T &operator[](qsizetype i) const { return at(i); }
    T &first() { return d.front(); }
    const T &first() const { return d.front(); }
    T &last() { return d.back(); }
    const T &last() const { return d.back(); }

    void removeFirst() {
        d.pop_front();
        ++m_first;
    }
    void removeLast() { d.pop_back(); }
    T takeFirst() {
        T t = std::move(d.front());
        removeFirst();
        return t;
    }
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
