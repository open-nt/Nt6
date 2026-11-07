// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <vector>

template <typename T>
class QList {
    static_assert(!std::is_same_v<T, bool>, "QList<bool> is not supported yet");

public:
    using Container = std::vector<T>;
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using reverse_iterator = typename Container::reverse_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = T;
    using reference = T &;
    using const_reference = const T &;
    using size_type = qsizetype;
    using difference_type = qsizetype;
    using pointer = T *;
    using const_pointer = const T *;
    using Iterator = iterator;
    using ConstIterator = const_iterator;

    QList() = default;
    explicit QList(qsizetype size) : d(static_cast<std::size_t>(size)) {}
    QList(qsizetype size, const T &value) : d(static_cast<std::size_t>(size), value) {}
    QList(std::initializer_list<T> args) : d(args) {}
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QList(It first, It last) : d(first, last) {}

    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    qsizetype count() const noexcept { return size(); }
    qsizetype length() const noexcept { return size(); }
    qsizetype capacity() const noexcept { return static_cast<qsizetype>(d.capacity()); }
    bool isEmpty() const noexcept { return d.empty(); }
    bool empty() const noexcept { return d.empty(); }
    void clear() { d.clear(); }
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    void squeeze() { d.shrink_to_fit(); }
    void resize(qsizetype n) { d.resize(static_cast<std::size_t>(n)); }
    void resize(qsizetype n, const T &v) { d.resize(static_cast<std::size_t>(n), v); }
    void fill(const T &v, qsizetype n = -1) {
        if (n >= 0)
            d.assign(static_cast<std::size_t>(n), v);
        else
            std::fill(d.begin(), d.end(), v);
    }

    T &operator[](qsizetype i) { return d[static_cast<std::size_t>(i)]; }
    const T &operator[](qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    const T &at(qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    T value(qsizetype i) const { return (i >= 0 && i < size()) ? d[static_cast<std::size_t>(i)] : T(); }
    T value(qsizetype i, const T &def) const { return (i >= 0 && i < size()) ? d[static_cast<std::size_t>(i)] : def; }
    T &first() { return d.front(); }
    const T &first() const { return d.front(); }
    T &last() { return d.back(); }
    const T &last() const { return d.back(); }
    T &front() { return d.front(); }
    const T &front() const { return d.front(); }
    T &back() { return d.back(); }
    const T &back() const { return d.back(); }
    T *data() { return d.data(); }
    const T *data() const { return d.data(); }
    const T *constData() const { return d.data(); }

    void append(const T &t) { d.push_back(t); }
    void append(T &&t) { d.push_back(std::move(t)); }
    void append(const QList &l) { d.insert(d.end(), l.d.begin(), l.d.end()); }
    void push_back(const T &t) { d.push_back(t); }
    void push_back(T &&t) { d.push_back(std::move(t)); }
    void prepend(const T &t) { d.insert(d.begin(), t); }
    void push_front(const T &t) { prepend(t); }
    template <typename... Args>
    T &emplaceBack(Args &&...args) { d.emplace_back(std::forward<Args>(args)...); return d.back(); }
    template <typename... Args>
    T &emplace_back(Args &&...args) { return emplaceBack(std::forward<Args>(args)...); }
    iterator insert(qsizetype i, const T &t) { return d.insert(d.begin() + i, t); }
    iterator insert(qsizetype i, qsizetype n, const T &t) { return d.insert(d.begin() + i, static_cast<std::size_t>(n), t); }
    iterator insert(const_iterator before, const T &t) { return d.insert(before, t); }
    void replace(qsizetype i, const T &t) { d[static_cast<std::size_t>(i)] = t; }
    void remove(qsizetype i, qsizetype n = 1) { d.erase(d.begin() + i, d.begin() + i + n); }
    void removeAt(qsizetype i) { d.erase(d.begin() + i); }
    void removeFirst() { d.erase(d.begin()); }
    void removeLast() { d.pop_back(); }
    void pop_back() { d.pop_back(); }
    void pop_front() { removeFirst(); }
    iterator erase(const_iterator pos) { return d.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return d.erase(first, last); }
    T takeAt(qsizetype i) { T t = std::move(d[static_cast<std::size_t>(i)]); removeAt(i); return t; }
    T takeFirst() { return takeAt(0); }
    T takeLast() { T t = std::move(d.back()); d.pop_back(); return t; }
    bool removeOne(const T &t) {
        auto it = std::find(d.begin(), d.end(), t);
        if (it == d.end())
            return false;
        d.erase(it);
        return true;
    }
    qsizetype removeAll(const T &t) {
        const auto before = d.size();
        d.erase(std::remove(d.begin(), d.end(), t), d.end());
        return static_cast<qsizetype>(before - d.size());
    }
    template <typename Pred>
    qsizetype removeIf(Pred p) {
        const auto before = d.size();
        d.erase(std::remove_if(d.begin(), d.end(), p), d.end());
        return static_cast<qsizetype>(before - d.size());
    }
    void move(qsizetype from, qsizetype to) {
        if (from == to)
            return;
        T t = std::move(d[static_cast<std::size_t>(from)]);
        d.erase(d.begin() + from);
        d.insert(d.begin() + to, std::move(t));
    }
    void swapItemsAt(qsizetype i, qsizetype j) { std::swap(d[static_cast<std::size_t>(i)], d[static_cast<std::size_t>(j)]); }
    void swap(QList &o) noexcept { d.swap(o.d); }

    bool contains(const T &t) const { return std::find(d.begin(), d.end(), t) != d.end(); }
    qsizetype indexOf(const T &t, qsizetype from = 0) const {
        if (from < 0)
            from = qMax<qsizetype>(0, from + size());
        for (qsizetype i = from; i < size(); ++i)
            if (d[static_cast<std::size_t>(i)] == t)
                return i;
        return -1;
    }
    qsizetype lastIndexOf(const T &t, qsizetype from = -1) const {
        qsizetype i = (from < 0) ? size() + from : qMin(from, size() - 1);
        for (; i >= 0; --i)
            if (d[static_cast<std::size_t>(i)] == t)
                return i;
        return -1;
    }
    qsizetype count(const T &t) const { return static_cast<qsizetype>(std::count(d.begin(), d.end(), t)); }
    bool startsWith(const T &t) const { return !d.empty() && d.front() == t; }
    bool endsWith(const T &t) const { return !d.empty() && d.back() == t; }

    QList mid(qsizetype pos, qsizetype len = -1) const {
        if (pos < 0) pos = 0;
        if (pos >= size()) return {};
        const qsizetype end = (len < 0 || pos + len > size()) ? size() : pos + len;
        return QList(d.begin() + pos, d.begin() + end);
    }
    QList first(qsizetype n) const { return mid(0, n); }
    QList last(qsizetype n) const { return n >= size() ? *this : mid(size() - n, n); }
    QList sliced(qsizetype pos, qsizetype n) const { return mid(pos, n); }
    QList sliced(qsizetype pos) const { return mid(pos); }
    QList chopped(qsizetype n) const { return mid(0, size() - n); }
    void chop(qsizetype n) { d.resize(d.size() - static_cast<std::size_t>(n)); }
    void reverse() { std::reverse(d.begin(), d.end()); }
    const std::vector<T> &toStdVector() const { return d; }

    iterator begin() { return d.begin(); }
    iterator end() { return d.end(); }
    const_iterator begin() const noexcept { return d.begin(); }
    const_iterator end() const noexcept { return d.end(); }
    const_iterator cbegin() const noexcept { return d.cbegin(); }
    const_iterator cend() const noexcept { return d.cend(); }
    const_iterator constBegin() const noexcept { return d.cbegin(); }
    const_iterator constEnd() const noexcept { return d.cend(); }
    reverse_iterator rbegin() { return d.rbegin(); }
    reverse_iterator rend() { return d.rend(); }
    const_reverse_iterator rbegin() const noexcept { return d.rbegin(); }
    const_reverse_iterator rend() const noexcept { return d.rend(); }

    QList &operator+=(const QList &l) { append(l); return *this; }
    QList &operator+=(const T &t) { append(t); return *this; }
    QList &operator<<(const T &t) { append(t); return *this; }
    QList &operator<<(const QList &l) { append(l); return *this; }
    friend QList operator+(const QList &a, const QList &b) { QList r(a); r.append(b); return r; }
    friend bool operator==(const QList &a, const QList &b) { return a.d == b.d; }
    friend bool operator!=(const QList &a, const QList &b) { return a.d != b.d; }
    friend bool operator<(const QList &a, const QList &b) { return a.d < b.d; }

private:
    Container d;
};

template <typename T>
using QVector = QList<T>;

template <typename T>
class QQueue : public QList<T> {
public:
    using QList<T>::QList;
    void enqueue(const T &t) { this->append(t); }
    T dequeue() { return this->takeFirst(); }
    T &head() { return this->first(); }
    const T &head() const { return this->first(); }
};

template <typename T>
class QStack : public QList<T> {
public:
    using QList<T>::QList;
    void push(const T &t) { this->append(t); }
    T pop() { return this->takeLast(); }
    T &top() { return this->last(); }
    const T &top() const { return this->last(); }
};
