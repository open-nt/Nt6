// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <functional>

class QLatin1Char {
public:
    constexpr explicit QLatin1Char(char c) noexcept : ch(c) {}
    constexpr char toLatin1() const noexcept { return ch; }
    constexpr char16_t unicode() const noexcept { return static_cast<unsigned char>(ch); }

private:
    char ch;
};

class QChar {
public:
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

    constexpr QChar() noexcept = default;
    constexpr QChar(char16_t c) noexcept : ucs(c) {}
    constexpr QChar(char c) noexcept : ucs(static_cast<unsigned char>(c)) {}
    constexpr QChar(uchar c) noexcept : ucs(c) {}
    constexpr QChar(short c) noexcept : ucs(static_cast<char16_t>(c)) {}
    constexpr QChar(ushort c) noexcept : ucs(c) {}
    constexpr QChar(int c) noexcept : ucs(static_cast<char16_t>(c)) {}
    constexpr QChar(uint c) noexcept : ucs(static_cast<char16_t>(c)) {}
    constexpr QChar(SpecialCharacter s) noexcept : ucs(static_cast<char16_t>(s)) {}
    constexpr QChar(QLatin1Char c) noexcept : ucs(c.unicode()) {}

    constexpr char16_t unicode() const noexcept { return ucs; }
    constexpr char16_t &unicode() noexcept { return ucs; }
    constexpr uchar cell() const noexcept { return static_cast<uchar>(ucs & 0xff); }
    constexpr uchar row() const noexcept { return static_cast<uchar>(ucs >> 8); }
    constexpr bool isNull() const noexcept { return ucs == 0; }
    constexpr char toLatin1() const noexcept { return ucs > 0xff ? '\0' : static_cast<char>(ucs); }
    static constexpr QChar fromLatin1(char c) noexcept { return QChar(static_cast<unsigned char>(c)); }

    bool isDigit() const noexcept { return ucs >= u'0' && ucs <= u'9'; }
    bool isNumber() const noexcept { return isDigit(); }
    bool isLetter() const noexcept;
    bool isLetterOrNumber() const noexcept { return isLetter() || isNumber(); }
    bool isSpace() const noexcept;
    bool isUpper() const noexcept;
    bool isLower() const noexcept;
    bool isPunct() const noexcept;
    bool isPrint() const noexcept { return ucs >= 0x20 && ucs != 0x7f && !(ucs >= 0x80 && ucs < 0xa0); }
    bool isHighSurrogate() const noexcept { return (ucs & 0xfc00) == 0xd800; }
    bool isLowSurrogate() const noexcept { return (ucs & 0xfc00) == 0xdc00; }
    bool isSurrogate() const noexcept { return (ucs & 0xf800) == 0xd800; }
    int digitValue() const noexcept { return isDigit() ? ucs - u'0' : -1; }
    QChar toLower() const noexcept;
    QChar toUpper() const noexcept;
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
