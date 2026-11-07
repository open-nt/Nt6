// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <functional>

/*!
    \class QLatin1Char
    \brief The QLatin1Char class provides an 8-bit ASCII/Latin-1 character.

    QLatin1Char wraps a single \c char so that it can be passed where a
    QChar is expected without being ambiguous with other integer types. It is
    converted to a QChar by interpreting the byte as a Latin-1 code point.

    \differences
    Only the constructor, toLatin1() and unicode() are provided. There are no
    comparison operators.
    \enddifferences

    \sa QChar, QLatin1StringView
*/
class QLatin1Char {
public:
    /*!
        Constructs a Latin-1 character for \a c.
    */
    constexpr explicit QLatin1Char(char c) noexcept : ch(c) {}
    /*!
        Returns the Latin-1 character value as a \c char.

        \sa unicode()
    */
    constexpr char toLatin1() const noexcept { return ch; }
    /*!
        Returns the Unicode code unit for this character, in the range 0 to 255.

        \sa toLatin1()
    */
    constexpr char16_t unicode() const noexcept { return static_cast<unsigned char>(ch); }

private:
    char ch;
};

class QChar {
public:
    /*!
        \enum QChar::SpecialCharacter

        Names for commonly used code points.

        \value Null U+0000, the null character.
        \value Tabulation U+0009, horizontal tabulation.
        \value LineFeed U+000A, line feed.
        \value CarriageReturn U+000D, carriage return.
        \value Space U+0020, the space character.
        \value Nbsp U+00A0, the no-break space.
        \value SoftHyphen U+00AD, the soft hyphen.
        \value ReplacementCharacter U+FFFD, used in place of invalid or unrepresentable input.
        \value ObjectReplacementCharacter U+FFFC, the object replacement character.
        \value ByteOrderMark U+FEFF, the byte order mark.
        \value LastValidCodePoint U+10FFFF, the last valid Unicode code point.
    */
    enum SpecialCharacter {
        Null = 0x0000,
        Tabulation = 0x0009,
        LineFeed = 0x000a,
        CarriageReturn = 0x000d,
        Space = 0x0020,
        Nbsp = 0x00a0,
        SoftHyphen = 0x00ad,
        ReplacementCharacter = 0xfffd,
        ObjectReplacementCharacter = 0xfffc,
        ByteOrderMark = 0xfeff,
        LastValidCodePoint = 0x10ffff
    };

    /*!
        Constructs a null character (U+0000).

        \sa isNull()
    */
    constexpr QChar() noexcept = default;
    /*!
        Constructs a character with the UTF-16 code unit \a c.
    */
    constexpr QChar(char16_t c) noexcept : ucs(c) {}
    /*!
        \overload

        Constructs a character from the Latin-1 byte \a c. The byte is
        zero-extended, not interpreted as a signed value.
    */
    constexpr QChar(char c) noexcept : ucs(static_cast<unsigned char>(c)) {}
    /*!
        \overload

        Constructs a character with the code unit \a c.
    */
    constexpr QChar(uchar c) noexcept : ucs(c) {}
    /*!
        \overload

        Constructs a character with the code unit \a c, reinterpreted as an
        unsigned 16-bit value.
    */
    constexpr QChar(short c) noexcept : ucs(static_cast<char16_t>(c)) {}
    /*!
        \overload

        Constructs a character with the code unit \a c.
    */
    constexpr QChar(ushort c) noexcept : ucs(c) {}
    /*!
        \overload

        Constructs a character from \a c, truncated to 16 bits.
    */
    constexpr QChar(int c) noexcept : ucs(static_cast<char16_t>(c)) {}
    /*!
        \overload

        Constructs a character from \a c, truncated to 16 bits.
    */
    constexpr QChar(uint c) noexcept : ucs(static_cast<char16_t>(c)) {}
    /*!
        \overload

        Constructs a character with the code point of the special character \a s.
    */
    constexpr QChar(SpecialCharacter s) noexcept : ucs(static_cast<char16_t>(s)) {}
    /*!
        \overload

        Constructs a character from the Latin-1 character \a c.
    */
    constexpr QChar(QLatin1Char c) noexcept : ucs(c.unicode()) {}

    /*!
        Returns the UTF-16 code unit of this character.

        \sa cell(), row()
    */
    constexpr char16_t unicode() const noexcept { return ucs; }
    /*!
        \overload

        Returns a reference to the UTF-16 code unit of this character, which
        can be used to modify it.
    */
    constexpr char16_t &unicode() noexcept { return ucs; }
    /*!
        Returns the low byte of the code unit (the "cell" within its row).

        \sa row(), unicode()
    */
    constexpr uchar cell() const noexcept { return static_cast<uchar>(ucs & 0xff); }
    /*!
        Returns the high byte of the code unit (the "row" of its plane).

        \sa cell(), unicode()
    */
    constexpr uchar row() const noexcept { return static_cast<uchar>(ucs >> 8); }
    /*!
        Returns \c true if this is the null character (U+0000); otherwise
        returns \c false.
    */
    constexpr bool isNull() const noexcept { return ucs == 0; }
    /*!
        Returns the Latin-1 representation of this character, or \c '\\0' if
        it is outside the range 0 to 255.

        \sa fromLatin1(), unicode()
    */
    constexpr char toLatin1() const noexcept { return ucs > 0xff ? '\0' : static_cast<char>(ucs); }
    /*!
        Converts the Latin-1 byte \a c to a QChar.

        \sa toLatin1()
    */
    static constexpr QChar fromLatin1(char c) noexcept { return QChar(static_cast<unsigned char>(c)); }

    /*!
        Returns \c true if this is an ASCII digit (\c 0 to \c 9); otherwise
        returns \c false. Digits from other scripts are not recognized.

        \sa isNumber(), digitValue()
    */
    bool isDigit() const noexcept { return ucs >= u'0' && ucs <= u'9'; }
    /*!
        Returns \c true if this is a number. Only the ASCII digits count as
        numbers, so this is the same as isDigit().

        \sa isDigit(), isLetterOrNumber()
    */
    bool isNumber() const noexcept { return isDigit(); }
    bool isLetter() const noexcept;
    /*!
        Returns \c true if this is a letter or a number; otherwise returns
        \c false.

        \sa isLetter(), isNumber()
    */
    bool isLetterOrNumber() const noexcept { return isLetter() || isNumber(); }
    bool isSpace() const noexcept;
    bool isUpper() const noexcept;
    bool isLower() const noexcept;
    bool isPunct() const noexcept;
    /*!
        Returns \c true if this is a printable character, that is, not a C0 or
        C1 control character and not U+007F; otherwise returns \c false.
    */
    bool isPrint() const noexcept { return ucs >= 0x20 && ucs != 0x7f && !(ucs >= 0x80 && ucs < 0xa0); }
    /*!
        Returns \c true if this is a UTF-16 high surrogate (U+D800 to U+DBFF);
        otherwise returns \c false.

        \sa isLowSurrogate(), isSurrogate()
    */
    bool isHighSurrogate() const noexcept { return (ucs & 0xfc00) == 0xd800; }
    /*!
        Returns \c true if this is a UTF-16 low surrogate (U+DC00 to U+DFFF);
        otherwise returns \c false.

        \sa isHighSurrogate(), isSurrogate()
    */
    bool isLowSurrogate() const noexcept { return (ucs & 0xfc00) == 0xdc00; }
    /*!
        Returns \c true if this is a UTF-16 surrogate (U+D800 to U+DFFF);
        otherwise returns \c false.

        \sa isHighSurrogate(), isLowSurrogate()
    */
    bool isSurrogate() const noexcept { return (ucs & 0xf800) == 0xd800; }
    /*!
        Returns the numeric value of this character if it is an ASCII digit;
        otherwise returns -1.

        \sa isDigit()
    */
    int digitValue() const noexcept { return isDigit() ? ucs - u'0' : -1; }
    QChar toLower() const noexcept;
    QChar toUpper() const noexcept;
    /*!
        Returns the case folded equivalent of this character. This is the same
        as toLower().

        \sa toLower()
    */
    QChar toCaseFolded() const noexcept { return toLower(); }

    friend constexpr bool operator==(QChar a, QChar b) noexcept { return a.ucs == b.ucs; }
    friend constexpr bool operator!=(QChar a, QChar b) noexcept { return a.ucs != b.ucs; }
    friend constexpr bool operator<(QChar a, QChar b) noexcept { return a.ucs < b.ucs; }
    friend constexpr bool operator>(QChar a, QChar b) noexcept { return a.ucs > b.ucs; }
    friend constexpr bool operator<=(QChar a, QChar b) noexcept { return a.ucs <= b.ucs; }
    friend constexpr bool operator>=(QChar a, QChar b) noexcept { return a.ucs >= b.ucs; }

private:
    char16_t ucs = 0;
};

static_assert(sizeof(QChar) == sizeof(char16_t), "QChar must be 16 bits");

template <>
struct std::hash<QChar> {
    std::size_t operator()(QChar c) const noexcept { return std::hash<char16_t>{}(c.unicode()); }
};
