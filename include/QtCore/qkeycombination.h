// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>

/*!
    \class QKeyCombination
    \inmodule NtCore
    \since 6.0
    \brief The QKeyCombination class stores a combination of a key with optional modifiers.

    A key combination is a Qt::Key with the Qt::KeyboardModifiers held with it, such as Ctrl+S. It is what a
    QKeySequence holds, and what its \c operator[] returns:

    \code
    const QKeyCombination combination = QKeySequence(Qt::CTRL | Qt::Key_S)[0];
    combination.key();               // Qt::Key_S
    combination.keyboardModifiers(); // Qt::ControlModifier
    \endcode

    \differences
    There is no constructor from Qt::Modifiers, and \c{Qt::CTRL | Qt::Key_S} gives an \c int rather than a
    QKeyCombination; both convert to one another.
    \enddifferences

    \sa QKeySequence
*/
class QKeyCombination {
public:
    /*!
        Constructs a combination of \a key without modifiers.
    */
    constexpr QKeyCombination(Qt::Key key = Qt::Key_unknown) noexcept // NOLINT(google-explicit-constructor)
        : m_combined(static_cast<int>(key)) {}
    /*!
        Constructs a combination of \a key with \a modifiers.
    */
    constexpr explicit QKeyCombination(Qt::KeyboardModifiers modifiers, Qt::Key key = Qt::Key_unknown) noexcept
        : m_combined(modifiers.toInt() | static_cast<int>(key)) {}

    /*!
        Returns the modifiers of the combination.
    */
    constexpr Qt::KeyboardModifiers keyboardModifiers() const noexcept {
        return Qt::KeyboardModifiers::fromInt(m_combined & kModifierMask);
    }
    /*!
        Returns the key of the combination.
    */
    constexpr Qt::Key key() const noexcept { return static_cast<Qt::Key>(m_combined & ~kModifierMask); }

    /*!
        Returns the combination that \a combined, a key ORed with modifiers, stands for.
    */
    static constexpr QKeyCombination fromCombined(int combined) noexcept {
        QKeyCombination c;
        c.m_combined = combined;
        return c;
    }
    /*!
        Returns the key ORed with the modifiers.
    */
    constexpr int toCombined() const noexcept { return m_combined; }
    /*!
        Returns toCombined(), so that code written for an \c int key keeps working.
    */
    constexpr operator int() const noexcept { return m_combined; } // NOLINT(google-explicit-constructor)

    /*!
        Returns \c true if \a lhs and \a rhs have the same key and modifiers.
    */
    friend constexpr bool operator==(QKeyCombination lhs, QKeyCombination rhs) noexcept {
        return lhs.m_combined == rhs.m_combined;
    }
    /*!
        Returns \c true if \a lhs and \a rhs differ.
    */
    friend constexpr bool operator!=(QKeyCombination lhs, QKeyCombination rhs) noexcept { return !(lhs == rhs); }
    /*!
        \overload
        Returns \c true if \a lhs is the combination \a rhs, a key ORed with modifiers or a Qt::Key.
    */
    friend constexpr bool operator==(QKeyCombination lhs, int rhs) noexcept { return lhs.m_combined == rhs; }
    /*!
        \overload
        Returns \c true if \a lhs, a key ORed with modifiers or a Qt::Key, is the combination \a rhs.
    */
    friend constexpr bool operator==(int lhs, QKeyCombination rhs) noexcept { return lhs == rhs.m_combined; }
    /*!
        \overload
        Returns \c true if \a lhs is not the combination \a rhs.
    */
    friend constexpr bool operator!=(QKeyCombination lhs, int rhs) noexcept { return !(lhs == rhs); }
    /*!
        \overload
        Returns \c true if \a lhs is not the combination \a rhs.
    */
    friend constexpr bool operator!=(int lhs, QKeyCombination rhs) noexcept { return !(lhs == rhs); }

private:
    static constexpr int kModifierMask = static_cast<int>(0xfe000000u);
    int m_combined;
};
