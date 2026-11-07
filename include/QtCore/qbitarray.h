// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <algorithm>
#include <utility>
#include <vector>

class QBitArray;

class QBitRef {
public:
    operator bool() const;
    bool operator!() const { return !bool(*this); }
    QBitRef &operator=(const QBitRef &v) { return *this = bool(v); }
    QBitRef &operator=(bool v);

private:
    friend class QBitArray;
    QBitRef(QBitArray &a, qsizetype i) : arr(a), idx(i) {}
    QBitArray &arr;
    qsizetype idx;
};

class QBitArray {
public:
    QBitArray() = default;
    explicit QBitArray(qsizetype size, bool value = false) : d(static_cast<std::size_t>(size), value) , m_null(false) {}

    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    qsizetype count() const noexcept { return size(); }
    qsizetype count(bool on) const { return static_cast<qsizetype>(std::count(d.begin(), d.end(), on)); }
    bool isEmpty() const noexcept { return d.empty(); }
    bool isNull() const noexcept { return m_null; }
    void resize(qsizetype size) { d.resize(static_cast<std::size_t>(size), false); m_null = false; }
    void truncate(qsizetype pos) { if (pos < size()) resize(pos); }
    void clear() { d.clear(); m_null = true; }
    void swap(QBitArray &o) noexcept { d.swap(o.d); std::swap(m_null, o.m_null); }

    bool testBit(qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    bool at(qsizetype i) const { return testBit(i); }
    void setBit(qsizetype i) { setBit(i, true); }
    void setBit(qsizetype i, bool v) { d[static_cast<std::size_t>(i)] = v; }
    void clearBit(qsizetype i) { setBit(i, false); }
    bool toggleBit(qsizetype i) { const bool old = testBit(i); setBit(i, !old); return old; }

    bool fill(bool v, qsizetype size = -1) {
        if (size >= 0)
            resize(size);
        std::fill(d.begin(), d.end(), v);
        return true;
    }
    void fill(bool v, qsizetype first, qsizetype last) {
        std::fill(d.begin() + first, d.begin() + last, v);
    }

    bool operator[](qsizetype i) const { return testBit(i); }
    QBitRef operator[](qsizetype i) { return QBitRef(*this, i); }

    QBitArray &operator&=(const QBitArray &o) { return combine(o, 0); }
    QBitArray &operator|=(const QBitArray &o) { return combine(o, 1); }
    QBitArray &operator^=(const QBitArray &o) { return combine(o, 2); }
    QBitArray operator~() const {
        QBitArray r = *this;
        r.d.flip();
        return r;
    }

    friend bool operator==(const QBitArray &a, const QBitArray &b) { return a.d == b.d; }
    friend bool operator!=(const QBitArray &a, const QBitArray &b) { return a.d != b.d; }
    friend QBitArray operator&(QBitArray a, const QBitArray &b) { a &= b; return a; }
    friend QBitArray operator|(QBitArray a, const QBitArray &b) { a |= b; return a; }
    friend QBitArray operator^(QBitArray a, const QBitArray &b) { a ^= b; return a; }

private:
    // The shorter operand is treated as if padded with zero bits.
    QBitArray &combine(const QBitArray &o, int op) {
        if (o.size() > size())
            resize(o.size());
        for (qsizetype i = 0; i < size(); ++i) {
            const bool b = i < o.size() && o.testBit(i);
            const bool a = testBit(i);
            setBit(i, op == 0 ? (a && b) : op == 1 ? (a || b) : (a != b));
        }
        return *this;
    }

    std::vector<bool> d;
    bool m_null = true;
};

inline QBitRef::operator bool() const { return static_cast<const QBitArray &>(arr).testBit(idx); }
inline QBitRef &QBitRef::operator=(bool v) { arr.setBit(idx, v); return *this; }
