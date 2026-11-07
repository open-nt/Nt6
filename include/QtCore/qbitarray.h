// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <algorithm>
#include <utility>
#include <vector>

class QBitArray;

/*!
    \class QBitRef
    \brief The QBitRef class is an internal helper for the QBitArray class.

    A QBitRef is returned by the non-const \c operator[]() of QBitArray. It refers to one bit of the array, so that the bit can be
    both read and assigned with the subscript operator.

    \code
    QBitArray bits(8);
    bits[3] = true;
    \endcode

    \differences
    The class is the same as in Qt, but it holds a reference to the array and
    is invalidated if the array is destroyed.
    \enddifferences

    \sa QBitArray
*/
class QBitRef {
public:
    /*! Returns the value of the referenced bit. */
    operator bool() const;
    /*! Returns the inverse of the referenced bit. */
    bool operator!() const { return !bool(*this); }
    /*! Copies the value of the bit \a v to the referenced bit and returns a reference to this object. */
    QBitRef &operator=(const QBitRef &v) { return *this = bool(v); }
    /*! Sets the referenced bit to \a v and returns a reference to this object. */
    QBitRef &operator=(bool v);

private:
    friend class QBitArray;
    QBitRef(QBitArray &a, qsizetype i) : arr(a), idx(i) {}
    QBitArray &arr;
    qsizetype idx;
};

/*!
    \class QBitArray
    \brief The QBitArray class provides an array of bits.

    A QBitArray stores a sequence of bits and gives access to each of them
    by index. Bits can be tested, set, cleared and toggled individually, and
    arrays can be combined with the bitwise operators \c &, \c |, \c ^
    and \c ~.

    \code
    QBitArray bits(5);
    bits.setBit(1);
    bits.setBit(3);
    QBitArray inverted = ~bits;
    \endcode

    Like QByteArray, a QBitArray distinguishes between a null array and an
    empty one; see isNull().

    \differences
    Bits are stored in a \c{std::vector<bool>}. There is no \c toUInt32(),
    \c fromBits(), \c bits() or \c operator<<()/\c operator>>() for
    QDataStream. Indexes are not range-checked. The bit count is a qsizetype.
    \enddifferences

    \sa QByteArray
*/
class QBitArray {
public:
    /*!
        Constructs an empty, null bit array.

        \sa isNull()
    */
    QBitArray() = default;
    /*!
        Constructs a bit array containing \a size bits, each initialized to
        \a value. The array is not null.
    */
    explicit QBitArray(qsizetype size, bool value = false) : d(static_cast<std::size_t>(size), value) , m_null(false) {}

    /*! Returns the number of bits in the array. */
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    /*! Same as size(). */
    qsizetype count() const noexcept { return size(); }
    /*! \overload

        Returns the number of bits in the array whose value is \a on. */
    qsizetype count(bool on) const { return static_cast<qsizetype>(std::count(d.begin(), d.end(), on)); }
    /*! Returns \c true if the array has size 0; otherwise returns \c false. */
    bool isEmpty() const noexcept { return d.empty(); }
    /*!
        Returns \c true if this array is null; otherwise returns \c false.

        A default-constructed array, or one that has been cleared, is null. An
        array becomes non-null when it is constructed with a size or resized.

        \sa isEmpty(), clear()
    */
    bool isNull() const noexcept { return m_null; }
    /*!
        Resizes the array to \a size bits. New bits are set to \c false.

        \sa size(), truncate()
    */
    void resize(qsizetype size) { d.resize(static_cast<std::size_t>(size), false); m_null = false; }
    /*!
        Truncates the array at index \a pos. If \a pos is beyond the end of
        the array, nothing happens.

        \sa resize()
    */
    void truncate(qsizetype pos) { if (pos < size()) resize(pos); }
    /*!
        Clears the contents of the array and makes it null.

        \sa isNull(), resize()
    */
    void clear() { d.clear(); m_null = true; }
    /*! Swaps this bit array with \a o. This operation is very fast and never fails. */
    void swap(QBitArray &o) noexcept { d.swap(o.d); std::swap(m_null, o.m_null); }

    /*!
        Returns \c true if the bit at index position \a i is set; otherwise
        returns \c false. \a i must be a valid index.

        \sa setBit(), clearBit()
    */
    bool testBit(qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    /*!
        Returns the value of the bit at index position \a i.

        \sa testBit()
    */
    bool at(qsizetype i) const { return testBit(i); }
    /*!
        Sets the bit at index position \a i to \c true.

        \sa clearBit(), toggleBit()
    */
    void setBit(qsizetype i) { setBit(i, true); }
    /*!
        \overload

        Sets the bit at index position \a i to \a v.
    */
    void setBit(qsizetype i, bool v) { d[static_cast<std::size_t>(i)] = v; }
    /*!
        Sets the bit at index position \a i to \c false.

        \sa setBit(), toggleBit()
    */
    void clearBit(qsizetype i) { setBit(i, false); }
    /*!
        Inverts the bit at index position \a i and returns its previous value.

        \sa setBit(), clearBit()
    */
    bool toggleBit(qsizetype i) { const bool old = testBit(i); setBit(i, !old); return old; }

    /*!
        Sets every bit in the array to \a v and returns \c true. If \a size is
        not negative, the array is first resized to \a size bits.

        \sa resize()
    */
    bool fill(bool v, qsizetype size = -1) {
        if (size >= 0)
            resize(size);
        std::fill(d.begin(), d.end(), v);
        return true;
    }
    /*!
        \overload

        Sets the bits from index \a first up to, but not including, index
        \a last to \a v. Both indexes must be valid, with \a first not greater
        than \a last.
    */
    void fill(bool v, qsizetype first, qsizetype last) {
        std::fill(d.begin() + first, d.begin() + last, v);
    }

    /*! Returns the value of the bit at index position \a i. */
    bool operator[](qsizetype i) const { return testBit(i); }
    /*!
        \overload

        Returns a QBitRef that refers to the bit at index position \a i and
        can be assigned to.
    */
    QBitRef operator[](qsizetype i) { return QBitRef(*this, i); }

    /*!
        Performs a bitwise AND of this array with \a o and stores the result
        in this array. If \a o is longer, this array is enlarged to its size;
        the shorter operand is treated as padded with zero bits. Returns a
        reference to this array.
    */
    QBitArray &operator&=(const QBitArray &o) { return combine(o, 0); }
    /*!
        Performs a bitwise OR of this array with \a o and stores the result
        in this array. If \a o is longer, this array is enlarged to its size;
        the shorter operand is treated as padded with zero bits. Returns a
        reference to this array.
    */
    QBitArray &operator|=(const QBitArray &o) { return combine(o, 1); }
    /*!
        Performs a bitwise XOR of this array with \a o and stores the result
        in this array. If \a o is longer, this array is enlarged to its size;
        the shorter operand is treated as padded with zero bits. Returns a
        reference to this array.
    */
    QBitArray &operator^=(const QBitArray &o) { return combine(o, 2); }
    /*! Returns a copy of this array with every bit inverted. */
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
