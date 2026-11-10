// MIT License - see LICENSE
#include <QtCore/qchar.h>

#include <QtCore/qstring.h>

#include "qnormalize_p.h"
#include "qunicode_p.h"

#include <algorithm>
#include <iterator>

namespace U = QtPrivate::Unicode;
namespace D = QtPrivate::UnicodeData;

static_assert(D::scriptCount == QChar::ScriptCount, "regenerate the QChar::Script enum with tools/gen_unicodedata.py");
static_assert(D::ageCount == QChar::Unicode_16_0, "add the new QChar::UnicodeVersion values");

namespace {
// The value of the run of a run-length table that contains `c`; each entry is a code point shifted
// left by Shift, ORed with the value.
template <unsigned Shift, std::size_t N> unsigned runValue(const std::uint32_t (&runs)[N], char32_t c) noexcept {
    if (c > 0x10ffff)
        return 0;
    constexpr std::uint32_t mask = (1u << Shift) - 1;
    const auto *it =
        std::upper_bound(std::begin(runs), std::end(runs), (static_cast<std::uint32_t>(c) << Shift) | mask);
    return it == std::begin(runs) ? 0 : *(it - 1) & mask;
}
} // namespace

/*!
    \class QChar
    \brief The QChar class provides a 16-bit Unicode code unit.

    A QChar holds one UTF-16 code unit. Characters outside the Basic
    Multilingual Plane are represented by a pair of QChar surrogates; see
    isHighSurrogate(), isLowSurrogate() and surrogateToUcs4(). QChar is the
    element type of QString.

    The classification functions (isLetter(), isSpace(), isUpper() and so on)
    and the case conversions (toLower(), toUpper(), toCaseFolded()) use the
    general categories and the simple case mappings of the Unicode database
    (version 16). Each also has a static overload that takes a full code point
    as a \c char32_t.

    \code
    QChar::isLetter(U'\U0001D400');   // true: MATHEMATICAL BOLD CAPITAL A
    QChar(u'Σ').toLower();            // σ
    QChar(u'٣').digitValue();         // 3
    \endcode

    The properties used for bidirectional text and shaping come from the
    same database: direction(), hasMirrored() and mirroredChar(),
    joiningType(), script() and unicodeVersion().

    \differences
    The numeric values of the \c Script enumerators follow Nt's own order
    (Unknown, Inherited, Common, then alphabetical) and differ from Qt's;
    use the names. Unassigned code points have the direction \c DirL rather
    than the default of their block. The deprecated \c Joining enum and
    \c joining() are not provided, and QChar has no \c char32_t
    constructor. digitValue() recognizes decimal digits only, not other
    numbers such as superscript digits.
    \enddifferences

    \sa QString, QLatin1Char
*/

/*!
    Returns the general category of the code point \a ucs4.
*/
QChar::Category QChar::category(char32_t ucs4) noexcept { return static_cast<Category>(U::category(ucs4)); }

/*!
    \overload
    Returns \c true if \a ucs4 is a decimal digit of any script.
*/
bool QChar::isDigit(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return ucs4 >= u'0' && ucs4 <= u'9';
    return U::isDigit(ucs4);
}

/*!
    \overload
    Returns \c true if \a ucs4 is a number of any kind.
*/
bool QChar::isNumber(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return ucs4 >= u'0' && ucs4 <= u'9';
    return U::isNumber(ucs4);
}

/*!
    \overload
    Returns \c true if \a ucs4 is a letter.
*/
bool QChar::isLetter(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return (ucs4 >= u'a' && ucs4 <= u'z') || (ucs4 >= u'A' && ucs4 <= u'Z');
    return U::isLetter(ucs4);
}

/*!
    \overload
    Returns \c true if \a ucs4 is a letter or a number.
*/
bool QChar::isLetterOrNumber(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return (ucs4 >= u'a' && ucs4 <= u'z') || (ucs4 >= u'A' && ucs4 <= u'Z') || (ucs4 >= u'0' && ucs4 <= u'9');
    return U::isLetterOrNumber(ucs4);
}

/*!
    \overload
    Returns \c true if \a ucs4 is white space.
*/
bool QChar::isSpace(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return ucs4 == u' ' || (ucs4 >= 0x09 && ucs4 <= 0x0d);
    return U::isSpace(ucs4);
}

/*!
    \overload
    Returns \c true if \a ucs4 is an uppercase letter.
*/
bool QChar::isUpper(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return ucs4 >= u'A' && ucs4 <= u'Z';
    return U::category(ucs4) == U::Letter_Uppercase;
}

/*!
    \overload
    Returns \c true if \a ucs4 is a lowercase letter.
*/
bool QChar::isLower(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return ucs4 >= u'a' && ucs4 <= u'z';
    return U::isLower(ucs4);
}

/*!
    \overload
    Returns \c true if \a ucs4 is a titlecase letter.
*/
bool QChar::isTitleCase(char32_t ucs4) noexcept { return ucs4 >= 0x80 && U::category(ucs4) == U::Letter_Titlecase; }

/*!
    \overload
    Returns \c true if \a ucs4 is punctuation.
*/
bool QChar::isPunct(char32_t ucs4) noexcept { return U::isPunct(ucs4); }

/*!
    \overload
    Returns \c true if \a ucs4 is a combining mark.
*/
bool QChar::isMark(char32_t ucs4) noexcept { return ucs4 >= 0x300 && U::isMark(ucs4); }

/*!
    \overload
    Returns \c true if \a ucs4 is a symbol.
*/
bool QChar::isSymbol(char32_t ucs4) noexcept { return U::isSymbol(ucs4); }

/*!
    \overload
    Returns \c true if \a ucs4 is printable: neither a control character nor
    unassigned.
*/
bool QChar::isPrint(char32_t ucs4) noexcept {
    if (ucs4 < 0x80)
        return ucs4 >= 0x20 && ucs4 != 0x7f;
    const U::Category k = U::category(ucs4);
    return k != U::Other_Control && k != U::Other_NotAssigned;
}

/*!
    \overload
    Returns the value of \a ucs4 if it is a decimal digit, or -1.
*/
int QChar::digitValue(char32_t ucs4) noexcept { return U::digitValue(ucs4); }

/*!
    \overload
    Returns the simple lowercase mapping of \a ucs4.
*/
char32_t QChar::toLower(char32_t ucs4) noexcept { return U::toLower(ucs4); }

/*!
    \overload
    Returns the simple uppercase mapping of \a ucs4.
*/
char32_t QChar::toUpper(char32_t ucs4) noexcept { return U::toUpper(ucs4); }

/*!
    \overload
    Returns the simple titlecase mapping of \a ucs4.
*/
char32_t QChar::toTitleCase(char32_t ucs4) noexcept { return U::toTitle(ucs4); }

/*!
    \overload
    Returns the simple case folding of \a ucs4.
*/
char32_t QChar::toCaseFolded(char32_t ucs4) noexcept { return U::foldCase(ucs4); }

/*!
    \overload
    Returns the tag of the decomposition mapping of \a ucs4.
*/
QChar::Decomposition QChar::decompositionTag(char32_t ucs4) noexcept {
    if (ucs4 >= U::HangulSBase && ucs4 < U::HangulSBase + U::HangulSCount)
        return Canonical;
    const auto *m = U::decompositionMapping(ucs4);
    return m ? static_cast<Decomposition>(m->tag) : NoDecomposition;
}

/*!
    Returns this character's decomposition mapping: the characters it
    decomposes into, one level deep, or an empty string if it has none.

    \sa decompositionTag(), QString::normalized()
*/
QString QChar::decomposition() const { return decomposition(char32_t(ucs)); }

/*!
    \overload
    Returns the one-level decomposition mapping of \a ucs4.
*/
QString QChar::decomposition(char32_t ucs4) {
    std::u16string out;
    if (ucs4 >= U::HangulSBase && ucs4 < U::HangulSBase + U::HangulSCount) {
        // A syllable maps to its leading consonant and vowel (LV), and an LVT syllable to LV + T.
        const char32_t s = ucs4 - U::HangulSBase;
        if (s % U::HangulTCount == 0) {
            U::appendUtf16(out, U::HangulLBase + s / U::HangulNCount);
            U::appendUtf16(out, U::HangulVBase + (s % U::HangulNCount) / U::HangulTCount);
        } else {
            U::appendUtf16(out, ucs4 - s % U::HangulTCount);
            U::appendUtf16(out, U::HangulTBase + s % U::HangulTCount);
        }
        return QString(out);
    }
    if (const auto *m = U::decompositionMapping(ucs4)) {
        for (int i = 0; i < m->length; ++i)
            U::appendUtf16(out, QtPrivate::UnicodeData::decompositionPool[m->offset + i]);
    }
    return QString(out);
}

/*!
    \overload
    Returns the canonical combining class of \a ucs4.
*/
unsigned char QChar::combiningClass(char32_t ucs4) noexcept {
    return static_cast<unsigned char>(U::combiningClass(ucs4));
}

/*!
    \overload
    Returns the bidirectional class of \a ucs4.
*/
QChar::Direction QChar::direction(char32_t ucs4) noexcept {
    return static_cast<Direction>(runValue<5>(D::directionRuns, ucs4));
}

/*!
    \overload
    Returns the joining type of \a ucs4.
*/
QChar::JoiningType QChar::joiningType(char32_t ucs4) noexcept {
    return static_cast<JoiningType>(runValue<3>(D::joiningRuns, ucs4));
}

/*!
    \overload
    Returns the script of \a ucs4; \c Script_Unknown for code points that
    are not assigned.
*/
QChar::Script QChar::script(char32_t ucs4) noexcept { return static_cast<Script>(runValue<8>(D::scriptRuns, ucs4)); }

/*!
    \overload
    Returns the Unicode version that assigned \a ucs4.
*/
QChar::UnicodeVersion QChar::unicodeVersion(char32_t ucs4) noexcept {
    return static_cast<UnicodeVersion>(runValue<5>(D::ageRuns, ucs4));
}

/*!
    Returns the version of the Unicode Character Database that Nt's tables
    were generated from.

    \sa unicodeVersion()
*/
QChar::UnicodeVersion QChar::currentUnicodeVersion() noexcept { return Unicode_16_0; }

/*!
    \overload
    Returns \c true if \a ucs4 is mirrored in right-to-left text.
*/
bool QChar::hasMirrored(char32_t ucs4) noexcept { return runValue<1>(D::mirroredRuns, ucs4) != 0; }

/*!
    \overload
    Returns the mirror image of \a ucs4, or \a ucs4 itself if it has none.
*/
char32_t QChar::mirroredChar(char32_t ucs4) noexcept {
    const auto *first = std::begin(D::mirroringPairs);
    const auto *last = std::end(D::mirroringPairs);
    const auto *it =
        std::lower_bound(first, last, ucs4, [](const D::CaseMapping &m, char32_t v) { return m.code < v; });
    return it != last && it->code == ucs4 ? it->mapped : ucs4;
}
