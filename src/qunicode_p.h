// MIT License - see LICENSE
#pragma once

// Unicode character properties for the text classes (QRegularExpression, QCollator,
// QTextBoundaryFinder). The general categories come from the generated qunicodedata_p.h.

#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>

#include "qunicodedata_p.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>

namespace QtPrivate {
namespace Unicode {

// The order matches the generator script.
enum Category : unsigned char {
    Mark_NonSpacing, Mark_SpacingCombining, Mark_Enclosing,
    Number_DecimalDigit, Number_Letter, Number_Other,
    Separator_Space, Separator_Line, Separator_Paragraph,
    Other_Control, Other_Format, Other_Surrogate, Other_PrivateUse, Other_NotAssigned,
    Letter_Uppercase, Letter_Lowercase, Letter_Titlecase, Letter_Modifier, Letter_Other,
    Punctuation_Connector, Punctuation_Dash, Punctuation_Open, Punctuation_Close,
    Punctuation_InitialQuote, Punctuation_FinalQuote, Punctuation_Other,
    Symbol_Math, Symbol_Currency, Symbol_Modifier, Symbol_Other
};

inline Category category(char32_t c) {
    if (c > 0x10ffff)
        return Other_NotAssigned;
    const std::uint32_t key = (static_cast<std::uint32_t>(c) << 5) | 0x1f;
    const auto *first = std::begin(UnicodeData::categoryRuns);
    const auto *it = std::upper_bound(first, std::end(UnicodeData::categoryRuns), key);
    return static_cast<Category>(*(it - 1) & 0x1f);
}

inline bool isLetter(char32_t c) { const Category k = category(c); return k >= Letter_Uppercase && k <= Letter_Other; }
inline bool isMark(char32_t c) { return category(c) <= Mark_Enclosing; }
inline bool isDigit(char32_t c) { return category(c) == Number_DecimalDigit; }
inline bool isNumber(char32_t c) { const Category k = category(c); return k >= Number_DecimalDigit && k <= Number_Other; }
inline bool isPunct(char32_t c) { const Category k = category(c); return k >= Punctuation_Connector && k <= Punctuation_Other; }
inline bool isSymbol(char32_t c) { const Category k = category(c); return k >= Symbol_Math; }
inline bool isUpper(char32_t c) { const Category k = category(c); return k == Letter_Uppercase || k == Letter_Titlecase; }
inline bool isLower(char32_t c) { return category(c) == Letter_Lowercase; }
inline bool isSpace(char32_t c) {
    if (c == u'\t' || c == u'\n' || c == u'\v' || c == u'\f' || c == u'\r' || c == 0x85)
        return true;
    const Category k = category(c);
    return k >= Separator_Space && k <= Separator_Paragraph;
}
inline bool isLetterOrNumber(char32_t c) { return isLetter(c) || isNumber(c); }

// Returns the value of a decimal digit in any script, or -1.
inline int digitValue(char32_t c) {
    if (c >= u'0' && c <= u'9')
        return static_cast<int>(c - u'0');
    if (!isDigit(c))
        return -1;
    // Decimal digits are assigned in contiguous runs of whole sets of ten, each starting at zero
    // (some runs, such as the mathematical digits, hold several sets).
    char32_t start = c;
    while (start > 0 && isDigit(start - 1))
        --start;
    return static_cast<int>((c - start) % 10);
}

template <std::size_t N>
inline char32_t mapCase(const UnicodeData::CaseMapping (&table)[N], char32_t c) {
    if (c < 0x80) // fast path for ASCII; the tables agree
        return c;
    const auto *it = std::lower_bound(std::begin(table), std::end(table), c,
                                      [](const UnicodeData::CaseMapping &m, char32_t v) { return m.code < v; });
    return it != std::end(table) && it->code == c ? it->mapped : c;
}

// Simple (one-to-one) case mappings for the whole of Unicode.
inline char32_t foldCase(char32_t c) {
    if (c < 0x80)
        return c >= u'A' && c <= u'Z' ? c + 32 : c;
    return mapCase(UnicodeData::caseFoldings, c);
}
inline char32_t toLower(char32_t c) {
    if (c < 0x80)
        return c >= u'A' && c <= u'Z' ? c + 32 : c;
    return mapCase(UnicodeData::lowercaseMappings, c);
}
inline char32_t toUpper(char32_t c) {
    if (c < 0x80)
        return c >= u'a' && c <= u'z' ? c - 32 : c;
    return mapCase(UnicodeData::uppercaseMappings, c);
}
inline char32_t toTitle(char32_t c) {
    if (c < 0x80)
        return toUpper(c);
    return mapCase(UnicodeData::titlecaseMappings, c);
}

inline void appendUtf16(std::u16string &out, char32_t c) {
    if (c >= 0x10000) {
        out.push_back(static_cast<char16_t>(0xd800 + ((c - 0x10000) >> 10)));
        out.push_back(static_cast<char16_t>(0xdc00 + ((c - 0x10000) & 0x3ff)));
    } else {
        out.push_back(static_cast<char16_t>(c));
    }
}

enum class CaseConversion { Lower, Upper, Fold };

// Appends the full case mapping of c, which may be several characters (U+00DF uppercases to "SS").
inline void appendFullCase(std::u16string &out, char32_t c, CaseConversion kind) {
    if (c >= 0x80) {
        const auto *first = kind == CaseConversion::Upper   ? std::begin(UnicodeData::specialUppercase)
                            : kind == CaseConversion::Lower ? std::begin(UnicodeData::specialLowercase)
                                                            : std::begin(UnicodeData::specialCaseFoldings);
        const auto *last = kind == CaseConversion::Upper   ? std::end(UnicodeData::specialUppercase)
                           : kind == CaseConversion::Lower ? std::end(UnicodeData::specialLowercase)
                                                           : std::end(UnicodeData::specialCaseFoldings);
        const auto *it = std::lower_bound(first, last, c,
                                          [](const UnicodeData::SpecialCasing &m, char32_t v) { return m.code < v; });
        if (it != last && it->code == c) {
            for (char32_t m : it->mapped)
                if (m)
                    appendUtf16(out, m);
            return;
        }
    }
    appendUtf16(out, kind == CaseConversion::Upper ? toUpper(c)
                                                   : (kind == CaseConversion::Lower ? toLower(c) : foldCase(c)));
}

// Splits a precomposed letter into its base letter and first combining mark. Returns false (and
// leaves the outputs alone) if the character has no such decomposition.
inline bool decompose(char32_t c, char16_t *base, char16_t *mark) {
    if (c < 0xc0 || c > 0xffff)
        return false;
    const auto *first = std::begin(UnicodeData::decompositions);
    const auto *last = std::end(UnicodeData::decompositions);
    const auto *it = std::lower_bound(first, last, c, [](const UnicodeData::Decomposition &d, char32_t v) { return d.code < v; });
    if (it == last || it->code != c)
        return false;
    *base = it->base;
    *mark = it->mark;
    return true;
}

// Reads the code point at index i of the UTF-16 data s of length n and sets *len to its length
// in code units. A lone surrogate is returned as is.
inline char32_t codePointAt(const char16_t *s, qsizetype n, qsizetype i, int *len) {
    const char16_t c = s[i];
    if ((c & 0xfc00) == 0xd800 && i + 1 < n && (s[i + 1] & 0xfc00) == 0xdc00) {
        *len = 2;
        return 0x10000 + ((static_cast<char32_t>(c) - 0xd800) << 10) + (static_cast<char32_t>(s[i + 1]) - 0xdc00);
    }
    *len = 1;
    return c;
}
// Reads the code point that ends just before index i and sets *len to its length.
inline char32_t codePointBefore(const char16_t *s, qsizetype i, int *len) {
    const char16_t c = s[i - 1];
    if ((c & 0xfc00) == 0xdc00 && i >= 2 && (s[i - 2] & 0xfc00) == 0xd800) {
        *len = 2;
        return 0x10000 + ((static_cast<char32_t>(s[i - 2]) - 0xd800) << 10) + (static_cast<char32_t>(c) - 0xdc00);
    }
    *len = 1;
    return c;
}

// Returns s with the full case conversion kind applied to each code point.
inline std::u16string convertCase(const std::u16string &s, CaseConversion kind) {
    std::u16string out;
    out.reserve(s.size());
    const qsizetype n = static_cast<qsizetype>(s.size());
    for (qsizetype i = 0; i < n;) {
        int len = 1;
        const char32_t c = codePointAt(s.data(), n, i, &len);
        appendFullCase(out, c, kind);
        i += len;
    }
    return out;
}

} // namespace Unicode
} // namespace QtPrivate
