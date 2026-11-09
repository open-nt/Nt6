// MIT License - see LICENSE
#include <QtCore/qchar.h>

#include "qunicode_p.h"

namespace U = QtPrivate::Unicode;

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

    \differences
    The \c script(), \c direction(), \c joiningType(), \c combiningClass(),
    \c decomposition() and \c unicodeVersion() functions, the \c Script,
    \c Direction, \c Decomposition and \c JoiningType enums and
    \c fromUcs4() are not provided, and QChar has no \c char32_t
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
