// MIT License - see LICENSE
#include <QtCore/qchar.h>

// Case mapping and classification cover ASCII, Latin-1, Latin Extended-A, Greek and
// Cyrillic only; other scripts are returned unchanged.

/*!
    \class QChar
    \brief The QChar class provides a 16-bit Unicode code unit.

    A QChar holds one UTF-16 code unit. Characters outside the Basic
    Multilingual Plane are represented by a pair of QChar surrogates; see
    isHighSurrogate() and isLowSurrogate(). QChar is the element type of
    QString.

    The classification functions (isLetter(), isSpace(), isUpper() and so on)
    and the case conversions (toLower(), toUpper()) are implemented with
    built-in tables rather than the full Unicode database.

    \differences
    Classification and case mapping cover ASCII, Latin-1, Latin Extended-A,
    Greek and Cyrillic only (isLetter() also recognizes the Japanese kana, CJK
    ideographs and Hangul syllables); characters from other scripts are
    returned unchanged. isNumber() and digitValue() recognize only the ASCII
    digits, and toCaseFolded() is the same as toLower(). The surrogate-pair
    helpers (\c requiresSurrogates(), \c fromUcs4(), \c toUcs4()), the
    \c category(), \c script() and \c decomposition() functions, \c isMark(),
    \c isSymbol() and the \c Category, \c Script and \c Direction enums are not
    provided. QChar has no \c char32_t constructor.
    \enddifferences

    \sa QString, QLatin1Char
*/

/*!
    Returns \c true if this is a letter; otherwise returns \c false.

    Classification covers ASCII, Latin-1, Latin Extended, Greek, Cyrillic,
    Hiragana, Katakana, CJK ideographs and Hangul syllables. Letters from
    other scripts are not recognized.

    \sa isLetterOrNumber(), isUpper(), isLower()
*/
bool QChar::isLetter() const noexcept {
    const char16_t c = ucs;
    if ((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'))
        return true;
    if (c < 0xaa)
        return false;
    if (c == 0xaa || c == 0xb5 || c == 0xba)
        return true;
    if (c >= 0xc0 && c <= 0x24f)
        return c != 0xd7 && c != 0xf7;
    return (c >= 0x370 && c <= 0x3ff && c != 0x37e && c != 0x387) || (c >= 0x400 && c <= 0x481) ||
           (c >= 0x48a && c <= 0x52f) || (c >= 0x3040 && c <= 0x30ff) || (c >= 0x4e00 && c <= 0x9fff) ||
           (c >= 0xac00 && c <= 0xd7a3);
}

/*!
    Returns \c true if this is a white space character, including the
    control characters U+0009 to U+000D, U+0085, the no-break space and the
    Unicode space separators; otherwise returns \c false.
*/
bool QChar::isSpace() const noexcept {
    const char16_t c = ucs;
    return c == u' ' || (c >= 0x09 && c <= 0x0d) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
           c == 0x3000;
}

/*!
    Returns \c true if this is an uppercase letter, that is a letter
    whose toLower() is a different character; otherwise returns \c false.

    \sa isLower(), toUpper()
*/
bool QChar::isUpper() const noexcept { return isLetter() && toLower().ucs != ucs; }

/*!
    Returns \c true if this is a lowercase letter, that is a letter whose
    toUpper() is a different character; otherwise returns \c false.

    \sa isUpper(), toLower()
*/
bool QChar::isLower() const noexcept { return isLetter() && toUpper().ucs != ucs; }

/*!
    Returns \c true if this is a punctuation character; otherwise returns
    \c false.

    Classification covers ASCII, a few Latin-1 characters and the General
    Punctuation block (U+2010 to U+205E).
*/
bool QChar::isPunct() const noexcept {
    const char16_t c = ucs;
    if (c < 0x80)
        return (c >= 0x21 && c <= 0x2f && c != u'$' && c != u'+') ||
               (c >= 0x3a && c <= 0x40 && c != u'<' && c != u'=' && c != u'>') ||
               (c >= 0x5b && c <= 0x60 && c != u'^' && c != u'`') || c == u'{' || c == u'}';
    return c == 0xa1 || c == 0xa7 || c == 0xab || c == 0xb6 || c == 0xb7 || c == 0xbb || c == 0xbf ||
           (c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x205e);
}

/*!
    Returns the lowercase equivalent of this character, or the character
    itself if it has none.

    Case mapping covers ASCII, Latin-1, Latin Extended-A, Greek and
    Cyrillic only.

    \sa toUpper(), toCaseFolded()
*/
QChar QChar::toLower() const noexcept {
    const char16_t c = ucs;
    if (c < 0x80)
        return (c >= u'A' && c <= u'Z') ? QChar(char16_t(c + 32)) : *this;
    if ((c >= 0xc0 && c <= 0xde && c != 0xd7))
        return QChar(char16_t(c + 32));
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14a && c <= 0x177))
        return (c % 2 == 0) ? QChar(char16_t(c + 1)) : *this;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17e))
        return (c % 2 == 1) ? QChar(char16_t(c + 1)) : *this;
    if (c >= 0x391 && c <= 0x3a9 && c != 0x3a2)
        return QChar(char16_t(c + 32));
    if (c >= 0x410 && c <= 0x42f)
        return QChar(char16_t(c + 32));
    if (c >= 0x400 && c <= 0x40f)
        return QChar(char16_t(c + 80));
    return *this;
}

/*!
    Returns the uppercase equivalent of this character, or the character
    itself if it has none.

    Case mapping covers ASCII, Latin-1, Latin Extended-A, Greek and
    Cyrillic only.

    \sa toLower()
*/
QChar QChar::toUpper() const noexcept {
    const char16_t c = ucs;
    if (c < 0x80)
        return (c >= u'a' && c <= u'z') ? QChar(char16_t(c - 32)) : *this;
    if (c >= 0xe0 && c <= 0xfe && c != 0xf7)
        return QChar(char16_t(c - 32));
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14a && c <= 0x177))
        return (c % 2 == 1) ? QChar(char16_t(c - 1)) : *this;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17e))
        return (c % 2 == 0) ? QChar(char16_t(c - 1)) : *this;
    if (c >= 0x3b1 && c <= 0x3c9 && c != 0x3c2)
        return QChar(char16_t(c - 32));
    if (c >= 0x430 && c <= 0x44f)
        return QChar(char16_t(c - 32));
    if (c >= 0x450 && c <= 0x45f)
        return QChar(char16_t(c - 80));
    return *this;
}
