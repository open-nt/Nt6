// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

namespace QtPrivate {
// A literal 0 is the only argument that converts to this type, so `order < 0` compiles and `order < 1` does not.
struct CompareAgainstLiteralZero {
    constexpr CompareAgainstLiteralZero(CompareAgainstLiteralZero *) noexcept {}
};
} // namespace QtPrivate

/*!
    \class QPartialOrdering
    \brief The QPartialOrdering class represents the result of a comparison
    that allows for unordered results.

    A comparison of two values gives one of Less, Equivalent, Greater or
    Unordered; the last happens for values such as a NaN that have no order
    relative to anything. The result is compared with a literal \c 0, the same
    way as the result of \c std::partial_ordering:

    \code
    QPartialOrdering order = compare(a, b);
    if (order < 0)
        qDebug() << "a comes first";
    else if (order == QPartialOrdering::Unordered)
        qDebug() << "a and b cannot be compared";
    \endcode

    Every comparison against \c 0 is \c false for Unordered, except \c !=.

    \differences
    There are no conversions to and from \c std::partial_ordering, because Nt
    is built as C++17 and that type is C++20. The \c Qt::partial_ordering,
    \c Qt::weak_ordering and \c Qt::strong_ordering classes are not provided.
    \enddifferences
*/
class QPartialOrdering {
public:
    /*!
        \variable QPartialOrdering::Less

        The left operand is ordered before the right one.
    */
    static const QPartialOrdering Less;
    /*!
        \variable QPartialOrdering::Equivalent

        The operands are equivalent.
    */
    static const QPartialOrdering Equivalent;
    /*!
        \variable QPartialOrdering::Greater

        The left operand is ordered after the right one.
    */
    static const QPartialOrdering Greater;
    /*!
        \variable QPartialOrdering::Unordered

        The operands have no order relative to each other.
    */
    static const QPartialOrdering Unordered;

    /*!
        Returns \c true if \a lhs and \a rhs are the same result.
    */
    friend constexpr bool operator==(QPartialOrdering lhs, QPartialOrdering rhs) noexcept {
        return lhs.m_order == rhs.m_order;
    }
    /*!
        Returns \c true if \a lhs and \a rhs are different results.
    */
    friend constexpr bool operator!=(QPartialOrdering lhs, QPartialOrdering rhs) noexcept {
        return lhs.m_order != rhs.m_order;
    }
    /*!
        Returns \c true if \a o is Equivalent. The second operand is the literal \c 0.
    */
    friend constexpr bool operator==(QPartialOrdering o, QtPrivate::CompareAgainstLiteralZero) noexcept {
        return o.m_order == 0;
    }
    /*!
        Returns \c true unless \a o is Equivalent. The second operand is the literal \c 0.
    */
    friend constexpr bool operator!=(QPartialOrdering o, QtPrivate::CompareAgainstLiteralZero) noexcept {
        return o.m_order != 0;
    }
    /*!
        Returns \c true if \a o is Less. The second operand is the literal \c 0.
    */
    friend constexpr bool operator<(QPartialOrdering o, QtPrivate::CompareAgainstLiteralZero) noexcept {
        return o.m_order == -1;
    }
    /*!
        Returns \c true if \a o is Less or Equivalent. The second operand is the literal \c 0.
    */
    friend constexpr bool operator<=(QPartialOrdering o, QtPrivate::CompareAgainstLiteralZero) noexcept {
        return o.m_order == -1 || o.m_order == 0;
    }
    /*!
        Returns \c true if \a o is Greater. The second operand is the literal \c 0.
    */
    friend constexpr bool operator>(QPartialOrdering o, QtPrivate::CompareAgainstLiteralZero) noexcept {
        return o.m_order == 1;
    }
    /*!
        Returns \c true if \a o is Greater or Equivalent. The second operand is the literal \c 0.
    */
    friend constexpr bool operator>=(QPartialOrdering o, QtPrivate::CompareAgainstLiteralZero) noexcept {
        return o.m_order == 1 || o.m_order == 0;
    }
    /*!
        Returns \c true if \a o is Greater. The first operand is the literal \c 0.
    */
    friend constexpr bool operator<(QtPrivate::CompareAgainstLiteralZero, QPartialOrdering o) noexcept {
        return o.m_order == 1;
    }
    /*!
        Returns \c true if \a o is Greater or Equivalent. The first operand is the literal \c 0.
    */
    friend constexpr bool operator<=(QtPrivate::CompareAgainstLiteralZero, QPartialOrdering o) noexcept {
        return o.m_order == 1 || o.m_order == 0;
    }
    /*!
        Returns \c true if \a o is Less. The first operand is the literal \c 0.
    */
    friend constexpr bool operator>(QtPrivate::CompareAgainstLiteralZero, QPartialOrdering o) noexcept {
        return o.m_order == -1;
    }
    /*!
        Returns \c true if \a o is Less or Equivalent. The first operand is the literal \c 0.
    */
    friend constexpr bool operator>=(QtPrivate::CompareAgainstLiteralZero, QPartialOrdering o) noexcept {
        return o.m_order == -1 || o.m_order == 0;
    }

private:
    constexpr explicit QPartialOrdering(signed char order) noexcept : m_order(order) {}

    signed char m_order;
};

inline constexpr QPartialOrdering QPartialOrdering::Less{-1};
inline constexpr QPartialOrdering QPartialOrdering::Equivalent{0};
inline constexpr QPartialOrdering QPartialOrdering::Greater{1};
inline constexpr QPartialOrdering QPartialOrdering::Unordered{2};
