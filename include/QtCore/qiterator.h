// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>

#include <iterator>

// Java-style iterators. The iterator sits between items: next() returns the
// item it jumps over.

template <typename T, typename C = QList<T>>
class QListIterator {
public:
    explicit QListIterator(const C &container) : c(&container), i(c->begin()) {}
    QListIterator &operator=(const C &container) { c = &container; i = c->begin(); return *this; }

    void toFront() { i = c->begin(); }
    void toBack() { i = c->end(); }
    bool hasNext() const { return i != c->end(); }
    bool hasPrevious() const { return i != c->begin(); }
    const T &next() { return *i++; }
    const T &previous() { return *--i; }
    const T &peekNext() const { return *i; }
    const T &peekPrevious() const { auto p = i; return *--p; }
    bool findNext(const T &v) {
        while (hasNext())
            if (next() == v)
                return true;
        return false;
    }
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

template <typename T>
class QMutableListIterator {
public:
    explicit QMutableListIterator(QList<T> &list) : c(&list) {}

    void toFront() { i = 0; item = -1; }
    void toBack() { i = c->size(); item = -1; }
    bool hasNext() const { return i < c->size(); }
    bool hasPrevious() const { return i > 0; }
    T &next() { item = i++; return (*c)[item]; }
    T &previous() { item = --i; return (*c)[item]; }
    T &peekNext() const { return (*c)[i]; }
    T &peekPrevious() const { return (*c)[i - 1]; }
    T &value() const { return (*c)[item]; }
    void setValue(const T &v) { if (item >= 0) (*c)[item] = v; }
    void remove() {
        if (item < 0)
            return;
        c->removeAt(item);
        if (item < i)
            --i;
        item = -1;
    }
    void insert(const T &v) { c->insert(i++, v); item = -1; }
    bool findNext(const T &v) {
        while (hasNext())
            if (next() == v)
                return true;
        return false;
    }
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

template <typename C, typename K, typename V, bool Bidirectional>
class QAssocConstIterator {
    using It = typename C::const_iterator;

public:
    using Item = It;
    explicit QAssocConstIterator(const C &container) : c(&container), i(c->begin()), n(i) {}
    QAssocConstIterator &operator=(const C &container) { c = &container; toFront(); return *this; }

    void toFront() { i = c->begin(); n = i; }
    void toBack() { i = c->end(); n = i; }
    bool hasNext() const { return i != c->end(); }
    It next() { n = i++; return n; }
    It peekNext() const { return i; }
    const K &key() const { return n.key(); }
    const V &value() const { return n.value(); }
    bool findNext(const V &v) {
        while (hasNext())
            if (*next() == v)
                return true;
        return false;
    }
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    bool hasPrevious() const { return i != c->begin(); }
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    It previous() { n = --i; return n; }
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    It peekPrevious() const { auto p = i; return --p; }

private:
    const C *c;
    It i, n;
};

template <typename C, typename K, typename V, bool Bidirectional>
class QAssocMutableIterator {
    using It = typename C::iterator;

public:
    explicit QAssocMutableIterator(C &container) : c(&container), i(c->begin()), n(c->end()) {}

    void toFront() { i = c->begin(); n = c->end(); }
    void toBack() { i = c->end(); n = c->end(); }
    bool hasNext() const { return i != c->end(); }
    It next() { n = i++; return n; }
    const K &key() const { return n.key(); }
    V &value() const { return n.value(); }
    void setValue(const V &v) { if (n != c->end()) n.value() = v; }
    void remove() {
        if (n == c->end())
            return;
        const bool nextIsCurrent = i == n; // set by previous()
        It after = c->erase(n);
        if (nextIsCurrent)
            i = after;
        n = c->end();
    }
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    bool hasPrevious() const { return i != c->begin(); }
    template <bool B = Bidirectional, typename = std::enable_if_t<B>>
    It previous() { n = --i; return n; }

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

template <typename T>
class QSetIterator {
public:
    explicit QSetIterator(const QSet<T> &set) : c(&set), i(c->begin()) {}
    void toFront() { i = c->begin(); }
    bool hasNext() const { return i != c->end(); }
    const T &next() { return *i++; }
    const T &peekNext() const { return *i; }
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
