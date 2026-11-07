// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <utility>

// Keeps up to Prealloc elements inside the object, then moves to the heap.
template <typename T, qsizetype Prealloc = 256>
class QVarLengthArray {
public:
    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    QVarLengthArray() noexcept = default;
    explicit QVarLengthArray(qsizetype size) { resize(size); }
    QVarLengthArray(std::initializer_list<T> list) { assign(list.begin(), list.end()); }
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QVarLengthArray(It first, It last) { assign(first, last); }
    QVarLengthArray(const QVarLengthArray &o) { assign(o.begin(), o.end()); }
    QVarLengthArray(QVarLengthArray &&o) noexcept(std::is_nothrow_move_constructible_v<T>) { takeFrom(o); }
    ~QVarLengthArray() { destroyAll(); }

    QVarLengthArray &operator=(const QVarLengthArray &o) {
        if (this != &o) {
            clear();
            assign(o.begin(), o.end());
        }
        return *this;
    }
    QVarLengthArray &operator=(QVarLengthArray &&o) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this != &o) {
            destroyAll();
            ptr = inlineData();
            cap = Prealloc;
            sz = 0;
            takeFrom(o);
        }
        return *this;
    }
    QVarLengthArray &operator=(std::initializer_list<T> list) {
        clear();
        assign(list.begin(), list.end());
        return *this;
    }

    qsizetype size() const noexcept { return sz; }
    qsizetype count() const noexcept { return sz; }
    qsizetype length() const noexcept { return sz; }
    qsizetype capacity() const noexcept { return cap; }
    bool isEmpty() const noexcept { return sz == 0; }
    bool empty() const noexcept { return sz == 0; }
    T *data() noexcept { return ptr; }
    const T *data() const noexcept { return ptr; }
    const T *constData() const noexcept { return ptr; }

    void reserve(qsizetype n) { if (n > cap) reallocate(n); }
    void squeeze() { reallocate(qMax<qsizetype>(sz, Prealloc)); }
    void clear() { std::destroy(ptr, ptr + sz); sz = 0; }
    void resize(qsizetype n) {
        if (n < sz) {
            std::destroy(ptr + n, ptr + sz);
        } else {
            reserve(n);
            std::uninitialized_value_construct(ptr + sz, ptr + n);
        }
        sz = n;
    }
    void resize(qsizetype n, const T &v) {
        if (n < sz) {
            std::destroy(ptr + n, ptr + sz);
        } else {
            const T copy = v;
            reserve(n);
            std::uninitialized_fill(ptr + sz, ptr + n, copy);
        }
        sz = n;
    }

    T &operator[](qsizetype i) { return ptr[i]; }
    const T &operator[](qsizetype i) const { return ptr[i]; }
    const T &at(qsizetype i) const { return ptr[i]; }
    T &front() { return ptr[0]; }
    const T &front() const { return ptr[0]; }
    T &back() { return ptr[sz - 1]; }
    const T &back() const { return ptr[sz - 1]; }
    T &first() { return front(); }
    const T &first() const { return front(); }
    T &last() { return back(); }
    const T &last() const { return back(); }
    T value(qsizetype i) const { return i >= 0 && i < sz ? ptr[i] : T(); }
    T value(qsizetype i, const T &def) const { return i >= 0 && i < sz ? ptr[i] : def; }

    void append(const T &v) { emplace_back(v); }
    void append(T &&v) { emplace_back(std::move(v)); }
    void append(const T *buf, qsizetype n) {
        reserve(sz + n);
        std::uninitialized_copy(buf, buf + n, ptr + sz);
        sz += n;
    }
    void push_back(const T &v) { emplace_back(v); }
    void push_back(T &&v) { emplace_back(std::move(v)); }
    void prepend(const T &v) { insert(begin(), v); }
    template <typename... Args>
    T &emplace_back(Args &&...args) {
        if (sz == cap) {
            T tmp(std::forward<Args>(args)...); // an argument may live in this array
            reallocate(cap * 2);
            new (ptr + sz) T(std::move(tmp));
        } else {
            new (ptr + sz) T(std::forward<Args>(args)...);
        }
        return ptr[sz++];
    }
    void pop_back() { std::destroy_at(ptr + --sz); }
    void removeLast() { pop_back(); }

    iterator insert(const_iterator before, const T &v) {
        const qsizetype pos = before - ptr;
        T copy = v;
        emplace_back(std::move(copy));
        std::rotate(ptr + pos, ptr + sz - 1, ptr + sz);
        return ptr + pos;
    }
    void insert(qsizetype i, const T &v) { insert(ptr + i, v); }
    iterator erase(const_iterator first, const_iterator last) {
        const qsizetype pos = first - ptr, n = last - first;
        std::move(ptr + pos + n, ptr + sz, ptr + pos);
        std::destroy(ptr + sz - n, ptr + sz);
        sz -= n;
        return ptr + pos;
    }
    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
    void remove(qsizetype i, qsizetype n = 1) { erase(ptr + i, ptr + i + n); }

    template <typename U>
    qsizetype indexOf(const U &v, qsizetype from = 0) const {
        for (qsizetype i = qMax<qsizetype>(from, 0); i < sz; ++i)
            if (ptr[i] == v)
                return i;
        return -1;
    }
    template <typename U>
    qsizetype lastIndexOf(const U &v, qsizetype from = -1) const {
        for (qsizetype i = from < 0 ? sz - 1 : qMin(from, sz - 1); i >= 0; --i)
            if (ptr[i] == v)
                return i;
        return -1;
    }
    template <typename U>
    bool contains(const U &v) const { return indexOf(v) != -1; }

    iterator begin() noexcept { return ptr; }
    iterator end() noexcept { return ptr + sz; }
    const_iterator begin() const noexcept { return ptr; }
    const_iterator end() const noexcept { return ptr + sz; }
    const_iterator cbegin() const noexcept { return ptr; }
    const_iterator cend() const noexcept { return ptr + sz; }
    const_iterator constBegin() const noexcept { return ptr; }
    const_iterator constEnd() const noexcept { return ptr + sz; }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    friend bool operator==(const QVarLengthArray &a, const QVarLengthArray &b) {
        return a.sz == b.sz && std::equal(a.begin(), a.end(), b.begin());
    }
    friend bool operator!=(const QVarLengthArray &a, const QVarLengthArray &b) { return !(a == b); }

private:
    T *inlineData() noexcept { return reinterpret_cast<T *>(storage); }

    template <typename It>
    void assign(It first, It last) {
        for (; first != last; ++first)
            emplace_back(*first);
    }
    void reallocate(qsizetype n) {
        n = qMax<qsizetype>(n, 1);
        T *fresh = n <= Prealloc ? inlineData() : static_cast<T *>(::operator new(sizeof(T) * static_cast<std::size_t>(n), std::align_val_t(alignof(T))));
        if (fresh != ptr) {
            std::uninitialized_move(ptr, ptr + sz, fresh);
            std::destroy(ptr, ptr + sz);
            release();
            ptr = fresh;
        }
        cap = n <= Prealloc ? Prealloc : n;
    }
    void release() {
        if (ptr != inlineData())
            ::operator delete(ptr, std::align_val_t(alignof(T)));
    }
    void destroyAll() { std::destroy(ptr, ptr + sz); release(); }
    void takeFrom(QVarLengthArray &o) {
        if (o.ptr != o.inlineData()) {
            ptr = o.ptr;
            cap = o.cap;
            sz = o.sz;
            o.ptr = o.inlineData();
            o.cap = Prealloc;
            o.sz = 0;
        } else {
            std::uninitialized_move(o.ptr, o.ptr + o.sz, ptr);
            sz = o.sz;
            o.clear();
        }
    }

    alignas(T) unsigned char storage[sizeof(T) * static_cast<std::size_t>(Prealloc)];
    T *ptr = inlineData();
    qsizetype cap = Prealloc;
    qsizetype sz = 0;
};
