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
        \enum QChar::Category

        The general category of a character in the Unicode database.

        \value Mark_NonSpacing Mn, a non-spacing combining mark.
        \value Mark_SpacingCombining Mc, a spacing combining mark.
        \value Mark_Enclosing Me, an enclosing mark.
        \value Number_DecimalDigit Nd, a decimal digit.
        \value Number_Letter Nl, a letter-like number, such as a Roman numeral.
        \value Number_Other No, another number, such as a superscript digit.
        \value Separator_Space Zs, a space.
        \value Separator_Line Zl, the line separator.
        \value Separator_Paragraph Zp, the paragraph separator.
        \value Other_Control Cc, a control character.
        \value Other_Format Cf, a format character.
        \value Other_Surrogate Cs, a surrogate.
        \value Other_PrivateUse Co, a private-use character.
        \value Other_NotAssigned Cn, a code point that is not assigned.
        \value Letter_Uppercase Lu, an uppercase letter.
        \value Letter_Lowercase Ll, a lowercase letter.
        \value Letter_Titlecase Lt, a titlecase letter, such as U+01C5.
        \value Letter_Modifier Lm, a modifier letter.
        \value Letter_Other Lo, another letter, such as an ideograph.
        \value Punctuation_Connector Pc, connector punctuation, such as the underscore.
        \value Punctuation_Dash Pd, a dash.
        \value Punctuation_Open Ps, opening punctuation, such as a parenthesis.
        \value Punctuation_Close Pe, closing punctuation.
        \value Punctuation_InitialQuote Pi, an initial quotation mark.
        \value Punctuation_FinalQuote Pf, a final quotation mark.
        \value Punctuation_Other Po, other punctuation.
        \value Symbol_Math Sm, a mathematical symbol.
        \value Symbol_Currency Sc, a currency symbol.
        \value Symbol_Modifier Sk, a modifier symbol.
        \value Symbol_Other So, another symbol.
    */
    enum Category {
        Mark_NonSpacing,
        Mark_SpacingCombining,
        Mark_Enclosing,
        Number_DecimalDigit,
        Number_Letter,
        Number_Other,
        Separator_Space,
        Separator_Line,
        Separator_Paragraph,
        Other_Control,
        Other_Format,
        Other_Surrogate,
        Other_PrivateUse,
        Other_NotAssigned,
        Letter_Uppercase,
        Letter_Lowercase,
        Letter_Titlecase,
        Letter_Modifier,
        Letter_Other,
        Punctuation_Connector,
        Punctuation_Dash,
        Punctuation_Open,
        Punctuation_Close,
        Punctuation_InitialQuote,
        Punctuation_FinalQuote,
        Punctuation_Other,
        Symbol_Math,
        Symbol_Currency,
        Symbol_Modifier,
        Symbol_Other
    };

    /*!
        Returns the general category of this character.
    */
    Category category() const noexcept { return category(char32_t(ucs)); }
    /*!
        Returns \c true if this is a decimal digit of any script (category
        Number_DecimalDigit); otherwise returns \c false.

        \sa isNumber(), digitValue()
    */
    bool isDigit() const noexcept { return isDigit(char32_t(ucs)); }
    /*!
        Returns \c true if this is a number (any Number_* category); otherwise
        returns \c false.

        \sa isDigit(), isLetterOrNumber()
    */
    bool isNumber() const noexcept { return isNumber(char32_t(ucs)); }
    /*!
        Returns \c true if this is a letter (any Letter_* category); otherwise
        returns \c false.

        \sa isLetterOrNumber(), isUpper(), isLower()
    */
    bool isLetter() const noexcept { return isLetter(char32_t(ucs)); }
    /*!
        Returns \c true if this is a letter or a number; otherwise returns
        \c false.

        \sa isLetter(), isNumber()
    */
    bool isLetterOrNumber() const noexcept { return isLetterOrNumber(char32_t(ucs)); }
    /*!
        Returns \c true if this is a white space character: a Separator_*
        character, U+0009 to U+000D or U+0085; otherwise returns \c false.
    */
    bool isSpace() const noexcept { return isSpace(char32_t(ucs)); }
    /*!
        Returns \c true if this is an uppercase letter (Letter_Uppercase);
        otherwise returns \c false.

        \sa isLower(), isTitleCase(), toUpper()
    */
    bool isUpper() const noexcept { return isUpper(char32_t(ucs)); }
    /*!
        Returns \c true if this is a lowercase letter (Letter_Lowercase);
        otherwise returns \c false.

        \sa isUpper(), toLower()
    */
    bool isLower() const noexcept { return isLower(char32_t(ucs)); }
    /*!
        Returns \c true if this is a titlecase letter (Letter_Titlecase), such
        as U+01C5; otherwise returns \c false.

        \sa toTitleCase()
    */
    bool isTitleCase() const noexcept { return isTitleCase(char32_t(ucs)); }
    /*!
        Returns \c true if this is a punctuation character (any Punctuation_*
        category); otherwise returns \c false.
    */
    bool isPunct() const noexcept { return isPunct(char32_t(ucs)); }
    /*!
        Returns \c true if this is a combining mark (any Mark_* category);
        otherwise returns \c false.
    */
    bool isMark() const noexcept { return isMark(char32_t(ucs)); }
    /*!
        Returns \c true if this is a symbol (any Symbol_* category); otherwise
        returns \c false.
    */
    bool isSymbol() const noexcept { return isSymbol(char32_t(ucs)); }
    /*!
        Returns \c true if this is a printable character, that is, neither a
        control character nor an unassigned code point; otherwise returns
        \c false.
    */
    bool isPrint() const noexcept { return isPrint(char32_t(ucs)); }
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
        Returns \c true if this is a noncharacter, such as U+FFFE; otherwise
        returns \c false.
    */
    bool isNonCharacter() const noexcept { return isNonCharacter(char32_t(ucs)); }
    /*!
        Returns the value of this character if it is a decimal digit of any
        script; otherwise returns -1.

        \sa isDigit()
    */
    int digitValue() const noexcept { return digitValue(char32_t(ucs)); }
    /*!
        Returns the lowercase equivalent of this character, or the character
        itself if it has none. The mapping is the simple (one-to-one) mapping
        of Unicode.

        \sa toUpper(), toCaseFolded(), QString::toLower()
    */
    QChar toLower() const noexcept { return QChar(static_cast<char16_t>(toLower(char32_t(ucs)))); }
    /*!
        Returns the uppercase equivalent of this character, or the character
        itself if it has none. The mapping is the simple (one-to-one) mapping
        of Unicode, so U+00DF stays as it is; QString::toUpper() uses the full
        mapping.

        \sa toLower(), toTitleCase(), QString::toUpper()
    */
    QChar toUpper() const noexcept { return QChar(static_cast<char16_t>(toUpper(char32_t(ucs)))); }
    /*!
        Returns the titlecase equivalent of this character, or the character
        itself if it has none.

        \sa toUpper(), isTitleCase()
    */
    QChar toTitleCase() const noexcept { return QChar(static_cast<char16_t>(toTitleCase(char32_t(ucs)))); }
    /*!
        Returns the case folded equivalent of this character, which is used
        to compare text regardless of case.

        \sa toLower(), QString::toCaseFolded()
    */
    QChar toCaseFolded() const noexcept { return QChar(static_cast<char16_t>(toCaseFolded(char32_t(ucs)))); }

    static Category category(char32_t ucs4) noexcept;
    static bool isDigit(char32_t ucs4) noexcept;
    static bool isNumber(char32_t ucs4) noexcept;
    static bool isLetter(char32_t ucs4) noexcept;
    static bool isLetterOrNumber(char32_t ucs4) noexcept;
    static bool isSpace(char32_t ucs4) noexcept;
    static bool isUpper(char32_t ucs4) noexcept;
    static bool isLower(char32_t ucs4) noexcept;
    static bool isTitleCase(char32_t ucs4) noexcept;
    static bool isPunct(char32_t ucs4) noexcept;
    static bool isMark(char32_t ucs4) noexcept;
    static bool isSymbol(char32_t ucs4) noexcept;
    static bool isPrint(char32_t ucs4) noexcept;
    static int digitValue(char32_t ucs4) noexcept;
    static char32_t toLower(char32_t ucs4) noexcept;
    static char32_t toUpper(char32_t ucs4) noexcept;
    static char32_t toTitleCase(char32_t ucs4) noexcept;
    static char32_t toCaseFolded(char32_t ucs4) noexcept;

    /*!
        Returns \c true if \a ucs4 is a noncharacter (U+FDD0 to U+FDEF, or a
        code point ending in FFFE or FFFF); otherwise returns \c false.
    */
    static constexpr bool isNonCharacter(char32_t ucs4) noexcept {
        return ucs4 >= 0xfdd0 && (ucs4 <= 0xfdef || (ucs4 & 0xfffe) == 0xfffe);
    }
    /*!
        Returns \c true if \a ucs4 is a high surrogate; otherwise returns
        \c false.
    */
    static constexpr bool isHighSurrogate(char32_t ucs4) noexcept { return (ucs4 & 0xfffffc00) == 0xd800; }
    /*!
        Returns \c true if \a ucs4 is a low surrogate; otherwise returns
        \c false.
    */
    static constexpr bool isLowSurrogate(char32_t ucs4) noexcept { return (ucs4 & 0xfffffc00) == 0xdc00; }
    /*!
        Returns \c true if \a ucs4 is a surrogate; otherwise returns \c false.
    */
    static constexpr bool isSurrogate(char32_t ucs4) noexcept { return ucs4 - 0xd800u < 2048u; }
    /*!
        Returns \c true if \a ucs4 lies outside the Basic Multilingual Plane
        and so needs a surrogate pair in UTF-16; otherwise returns \c false.
    */
    static constexpr bool requiresSurrogates(char32_t ucs4) noexcept { return ucs4 >= 0x10000; }
    /*!
        Returns the code point of the surrogate pair \a high and \a low.
    */
    static constexpr char32_t surrogateToUcs4(char16_t high, char16_t low) noexcept {
        return (char32_t(high) << 10) + low - 0x35fdc00;
    }
    /*!
        \overload
        Returns the code point of the surrogate pair \a high and \a low.
    */
    static constexpr char32_t surrogateToUcs4(QChar high, QChar low) noexcept {
        return surrogateToUcs4(high.ucs, low.ucs);
    }
    /*!
        Returns the high surrogate of the code point \a ucs4, which must
        require surrogates.
    */
    static constexpr char16_t highSurrogate(char32_t ucs4) noexcept { return char16_t((ucs4 >> 10) + 0xd7c0); }
    /*!
        Returns the low surrogate of the code point \a ucs4, which must require
        surrogates.
    */
    static constexpr char16_t lowSurrogate(char32_t ucs4) noexcept { return char16_t(ucs4 % 0x400 + 0xdc00); }

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

namespace QtPrivate {
// Returns true for the ASCII digits 0 to 9 only, as number parsers need.
constexpr bool isAsciiDigit(QChar c) noexcept { return c.unicode() >= u'0' && c.unicode() <= u'9'; }
} // namespace QtPrivate

template <>
struct std::hash<QChar> {
    std::size_t operator()(QChar c) const noexcept { return std::hash<char16_t>{}(c.unicode()); }
};
