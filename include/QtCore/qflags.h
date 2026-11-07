// MIT License - see LICENSE
#pragma once

#include <type_traits>

/*!
    \class QFlags
    \brief The QFlags class stores OR combinations of enum values in a type-safe way.

    Combining plain enum values produces an \c int, which loses the type and
    accepts values from any enum. QFlags<Enum> keeps the type, so a function
    taking \c{QFlags<Enum>} rejects unrelated values at compile time.

    Declare the flags type next to the enum, then enable the \c | operator for
    bare enum values at namespace scope:

    \code
    class Door {
    public:
        enum Option { Locked = 0x1, Alarmed = 0x2 };
        Q_DECLARE_FLAGS(Options, Option)
    };
    Q_DECLARE_OPERATORS_FOR_FLAGS(Door::Options)
    \endcode

    A value-less flag, such as an enumerator equal to \c 0, can only be
    detected with testFlag() when no other flag is set.

    \differences
    The stored value is always an \c int; there is no \c toInt() or
    \c fromInt() in the public API.
    \enddifferences
*/
template <typename Enum>
class QFlags {
    static_assert(std::is_enum_v<Enum>, "QFlags requires an enum type");

public:
    /*!
        The enum type this QFlags instantiation wraps.
    */
    using enum_type = Enum;
    /*!
        The integer type that stores the flags.
    */
    using Int = int;

    /*!
        Constructs an object with no flags set.
    */
    constexpr QFlags() noexcept = default;
    /*!
        Constructs an object with the single flag \a f set.
    */
    constexpr QFlags(Enum f) noexcept : i(static_cast<Int>(f)) {}

    /*!
        Sets the flags in \a o and returns a reference to this object.
    */
    constexpr QFlags &operator|=(QFlags o) noexcept { i |= o.i; return *this; }
    /*!
        \overload

        Sets the flag \a f and returns a reference to this object.
    */
    constexpr QFlags &operator|=(Enum f) noexcept { i |= static_cast<Int>(f); return *this; }
    /*!
        Keeps only the flags also set in \a o and returns a reference to this
        object.
    */
    constexpr QFlags &operator&=(QFlags o) noexcept { i &= o.i; return *this; }
    /*!
        \overload

        Keeps only the flag \a f and returns a reference to this object.
    */
    constexpr QFlags &operator&=(Enum f) noexcept { i &= static_cast<Int>(f); return *this; }
    /*!
        Toggles the flags in \a o and returns a reference to this object.
    */
    constexpr QFlags &operator^=(QFlags o) noexcept { i ^= o.i; return *this; }

    /*!
        Returns the flags set in this object or in \a o.
    */
    constexpr QFlags operator|(QFlags o) const noexcept { return fromInt(i | o.i); }
    /*!
        \overload

        Returns the flags set in this object plus \a f.
    */
    constexpr QFlags operator|(Enum f) const noexcept { return fromInt(i | static_cast<Int>(f)); }
    /*!
        Returns the flags set in both this object and \a o.
    */
    constexpr QFlags operator&(QFlags o) const noexcept { return fromInt(i & o.i); }
    /*!
        \overload

        Returns \a f if it is set in this object; otherwise returns no flags.
    */
    constexpr QFlags operator&(Enum f) const noexcept { return fromInt(i & static_cast<Int>(f)); }
    /*!
        Returns the flags set in exactly one of this object and \a o.
    */
    constexpr QFlags operator^(QFlags o) const noexcept { return fromInt(i ^ o.i); }
    /*!
        Returns the bitwise complement of the stored flags.
    */
    constexpr QFlags operator~() const noexcept { return fromInt(~i); }

    /*!
        Returns the stored flags as an integer.
    */
    constexpr operator Int() const noexcept { return i; }
    /*!
        Returns \c true if no flag is set.
    */
    constexpr bool operator!() const noexcept { return !i; }

    /*!
        Returns \c true if every bit of \a f is set. If \a f has the value
        \c 0, returns \c true only when no flag is set.
    */
    constexpr bool testFlag(Enum f) const noexcept {
        const Int v = static_cast<Int>(f);
        return (i & v) == v && (v != 0 || i == 0);
    }
    /*!
        Sets \a f if \a on is \c true; otherwise clears it. Returns a reference
        to this object.

        \sa testFlag()
    */
    constexpr QFlags &setFlag(Enum f, bool on = true) noexcept {
        return on ? (*this |= f) : (*this &= QFlags::fromInt(~static_cast<Int>(f)));
    }

private:
    static constexpr QFlags fromInt(Int v) noexcept { QFlags q; q.i = v; return q; }
    Int i = 0;
};

/*!
    \class QFlag
    \brief The QFlag class is a helper data type for QFlags.

    It is an integer that an enum value converts to implicitly, so that
    expressions such as \c{Option1 | Option2} on unrelated enums can be
    recognized and converted back to a QFlags value. Applications rarely name
    it directly.

    \sa QFlags
*/
class QFlag {
public:
    /*!
        Constructs a QFlag with the given \a value.
    */
    constexpr QFlag(int value) noexcept : i(value) {}
    /*!
        \overload

        Constructs a QFlag with the unsigned \a value, reinterpreted as an \c int.
    */
    constexpr QFlag(unsigned int value) noexcept : i(static_cast<int>(value)) {}
    /*!
        \overload

        Constructs a QFlag with the \a value.
    */
    constexpr QFlag(short value) noexcept : i(value) {}
    /*!
        \overload

        Constructs a QFlag with the unsigned \a value.
    */
    constexpr QFlag(unsigned short value) noexcept : i(value) {}
    /*!
        Returns the value as an \c int.
    */
    constexpr operator int() const noexcept { return i; }

private:
    int i;
};

#define Q_DECLARE_FLAGS(Flags, Enum) typedef QFlags<Enum> Flags;

#define Q_DECLARE_OPERATORS_FOR_FLAGS(Flags)                                                   \
    constexpr inline QFlags<Flags::enum_type> operator|(Flags::enum_type f1,                   \
                                                        Flags::enum_type f2) noexcept {        \
        return QFlags<Flags::enum_type>(f1) | f2;                                              \
    }                                                                                          \
    constexpr inline QFlags<Flags::enum_type> operator|(Flags::enum_type f1,                   \
                                                        QFlags<Flags::enum_type> f2) noexcept {\
        return f2 | f1;                                                                        \
    }
