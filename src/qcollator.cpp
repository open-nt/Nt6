// MIT License - see LICENSE
#include <QtCore/qcollator.h>

#include "qunicode_p.h"

#include <string>
#include <vector>

namespace U = QtPrivate::Unicode;

namespace {

// Primary weights. Letters of the Latin alphabet get kLatin + 16 * (letter - 'a'), leaving room
// for tailored letters after each base letter; other letters get kOtherLetter + code point.
constexpr std::uint32_t kSpace = 0x0100;
constexpr std::uint32_t kPunct = 0x0200;
constexpr std::uint32_t kSymbol = 0x10000;
constexpr std::uint32_t kDigit = 0x200000;
constexpr std::uint32_t kNumberLength = 0x200100;
constexpr std::uint32_t kLatin = 0x300000;
constexpr std::uint32_t kOtherLetter = 0x400000;

struct Element {
    std::uint32_t primary;
    std::uint32_t secondary; // accent
    std::uint32_t tertiary;  // case
};

std::uint32_t latin(char32_t base, int after = 0) { return kLatin + 16 * static_cast<std::uint32_t>(base - u'a') + static_cast<std::uint32_t>(after); }

// Letters that the language of the locale sorts as separate letters, and where.
bool tailored(QLocale::Language lang, char32_t lower, std::uint32_t *primary) {
    switch (lang) {
    case QLocale::Swedish:
    case QLocale::Finnish:
        if (lower == 0xe5) { *primary = latin(u'z', 1); return true; } // å
        if (lower == 0xe4 || lower == 0xe6) { *primary = latin(u'z', 2); return true; } // ä æ
        if (lower == 0xf6 || lower == 0xf8) { *primary = latin(u'z', 3); return true; } // ö ø
        return false;
    case QLocale::Danish:
    case QLocale::NorwegianBokmal:
    case QLocale::NorwegianNynorsk:
        if (lower == 0xe6 || lower == 0xe4) { *primary = latin(u'z', 1); return true; } // æ ä
        if (lower == 0xf8 || lower == 0xf6) { *primary = latin(u'z', 2); return true; } // ø ö
        if (lower == 0xe5) { *primary = latin(u'z', 3); return true; } // å
        return false;
    case QLocale::Spanish:
        if (lower == 0xf1) { *primary = latin(u'n', 1); return true; } // ñ
        return false;
    case QLocale::Polish: {
        static const std::pair<char32_t, char32_t> letters[] = {{0x105, u'a'}, {0x107, u'c'}, {0x119, u'e'}, {0x142, u'l'}, {0x144, u'n'},
                                                                {0xf3, u'o'},  {0x15b, u's'}, {0x17a, u'z'}, {0x17c, u'z'}};
        for (const auto &l : letters) {
            if (l.first == lower) {
                *primary = latin(l.second, l.first == 0x17c ? 2 : 1);
                return true;
            }
        }
        return false;
    }
    case QLocale::Czech: {
        static const std::pair<char32_t, char32_t> letters[] = {{0x10d, u'c'}, {0x159, u'r'}, {0x161, u's'}, {0x17e, u'z'}};
        for (const auto &l : letters) {
            if (l.first == lower) {
                *primary = latin(l.second, 1);
                return true;
            }
        }
        return false;
    }
    default:
        return false;
    }
}

// Latin letters without a canonical decomposition that sort as (sequences of) basic letters.
const char *latinExpansion(char32_t lower) {
    switch (lower) {
    case 0xdf: return "ss";   // ß
    case 0x1e9e: return "ss"; // ẞ
    case 0xe6: return "ae";   // æ
    case 0x153: return "oe";  // œ
    case 0xf8: return "o";    // ø
    case 0x111: return "d";   // đ
    case 0xf0: return "d";    // ð
    case 0x142: return "l";   // ł
    case 0x131: return "i";   // ı
    case 0x127: return "h";   // ħ
    case 0x167: return "t";   // ŧ
    case 0xfe: return "th";   // þ
    default: return nullptr;
    }
}

} // namespace

/*!
    Constructs a collator for the default locale.

    \sa QLocale::setDefault()
*/
QCollator::QCollator() = default;

/*!
    Constructs a collator for \a locale.
*/
QCollator::QCollator(const QLocale &locale) : m_locale(locale) {}

QByteArray QCollator::key(QStringView s) const {
    const QLocale::Language lang = m_locale.language();
    std::vector<Element> elements;
    const auto *text = reinterpret_cast<const char16_t *>(s.data());
    const qsizetype n = s.size();
    for (qsizetype i = 0; i < n;) {
        int len = 1;
        const char32_t c = U::codePointAt(text, n, i, &len);
        i += len;
        const U::Category cat = U::category(c);
        if (cat == U::Other_Control || cat == U::Other_Format)
            continue;
        if (cat <= U::Mark_Enclosing) {
            // A combining mark changes the accent of the preceding letter.
            if (!elements.empty())
                elements.back().secondary = elements.back().secondary * 3 + c;
            continue;
        }
        if (U::isSpace(c) || U::isPunct(c) || U::isSymbol(c)) {
            if (m_ignorePunctuation)
                continue;
            const std::uint32_t base = U::isSpace(c) ? kSpace : (U::isPunct(c) ? kPunct : kSymbol);
            elements.push_back({base + (base == kSymbol ? c : (c & 0xff)), 1, 1});
            continue;
        }
        if (U::isDigit(c)) {
            if (!m_numeric) {
                elements.push_back({kDigit + static_cast<std::uint32_t>(U::digitValue(c)), 1, 1});
                continue;
            }
            // A run of digits sorts by its value: first its number of significant digits, then
            // the digits. Leading zeros only break ties.
            std::vector<int> digits{U::digitValue(c)};
            while (i < n) {
                const char32_t d = U::codePointAt(text, n, i, &len);
                if (!U::isDigit(d))
                    break;
                digits.push_back(U::digitValue(d));
                i += len;
            }
            std::size_t first = 0;
            while (first + 1 < digits.size() && digits[first] == 0)
                ++first;
            const auto leadingZeros = static_cast<std::uint32_t>(first);
            elements.push_back({kNumberLength + static_cast<std::uint32_t>(digits.size() - first), 1 + leadingZeros, 1});
            for (std::size_t k = first; k < digits.size(); ++k)
                elements.push_back({kDigit + static_cast<std::uint32_t>(digits[k]), 1, 1});
            continue;
        }
        // A letter (or a number in another form).
        const char32_t lower = U::foldCase(c);
        const std::uint32_t tertiary = (U::isUpper(c) || lower != c) ? 2 : 1;
        std::uint32_t primary = 0;
        if (tailored(lang, lower, &primary)) {
            elements.push_back({primary, 1, tertiary});
            continue;
        }
        char16_t base = 0, mark = 0;
        char32_t letter = lower;
        std::uint32_t accent = 1;
        if (U::decompose(lower, &base, &mark)) {
            letter = U::foldCase(base);
            accent = 2 + mark;
            // Precomposed letters with two marks, such as ǖ, decompose once more.
            char16_t base2 = 0, mark2 = 0;
            if (U::decompose(letter, &base2, &mark2)) {
                letter = U::foldCase(base2);
                accent = accent * 3 + mark2;
            }
        }
        if (letter >= u'a' && letter <= u'z') {
            elements.push_back({latin(letter), accent, tertiary});
        } else if (const char *exp = latinExpansion(letter)) {
            for (const char *p = exp; *p; ++p)
                elements.push_back({latin(static_cast<char32_t>(*p)), accent + 1, tertiary});
        } else {
            elements.push_back({kOtherLetter + letter, accent, tertiary});
        }
    }

    std::string out;
    out.reserve(elements.size() * 12 + 8);
    auto put = [&out](std::uint32_t v) {
        out.push_back(static_cast<char>((v >> 24) & 0xff));
        out.push_back(static_cast<char>((v >> 16) & 0xff));
        out.push_back(static_cast<char>((v >> 8) & 0xff));
        out.push_back(static_cast<char>(v & 0xff));
    };
    for (const Element &e : elements)
        put(e.primary);
    put(0);
    for (const Element &e : elements)
        put(e.secondary);
    if (m_cs == Qt::CaseSensitive) {
        put(0);
        for (const Element &e : elements)
            put(e.tertiary);
    }
    return QByteArray(std::move(out));
}

/*!
    Compares \a s1 with \a s2 and returns a negative number, zero or a
    positive number if \a s1 sorts before, equal to or after \a s2 in the
    collator's locale.

    \sa sortKey()
*/
int QCollator::compare(QStringView s1, QStringView s2) const {
    const int r = key(s1).compare(key(s2));
    return r < 0 ? -1 : (r > 0 ? 1 : 0);
}

/*!
    Returns the sort key of \a string. Comparing sort keys gives the same
    result as compare() on their strings.
*/
QCollatorSortKey QCollator::sortKey(const QString &string) const { return QCollatorSortKey(key(string)); }

/*!
    \since 6.3
    Compares \a s1 with \a s2 using a collator for the default locale with
    the default settings.
*/
int QCollator::defaultCompare(QStringView s1, QStringView s2) { return QCollator().compare(s1, s2); }

/*!
    \since 6.3
    Returns the sort key of \a key for a collator with the default locale and
    settings.
*/
QCollatorSortKey QCollator::defaultSortKey(QStringView key) { return QCollator().sortKey(key.toString()); }

/*!
    Compares \a a with \a b in the collation order of the default locale and
    returns a negative number, zero or a positive number if \a a sorts
    before, equal to or after \a b.

    \sa QCollator
*/
int QString::localeAwareCompare(const QString &a, const QString &b) { return QCollator::defaultCompare(a, b); }
