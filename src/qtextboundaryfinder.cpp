// MIT License - see LICENSE
#include <QtCore/qtextboundaryfinder.h>

#include "qunicode_p.h"

#include <algorithm>

namespace U = QtPrivate::Unicode;

namespace {

struct Unit {
    qsizetype start; // UTF-16 index of the first code point
    char32_t cp;     // the first code point
};

// ---------------------------------------------------------------- grapheme clusters

enum class Hangul { None, L, V, T, LV, LVT };

Hangul hangulType(char32_t c) {
    if (c >= 0x1100 && c <= 0x115f)
        return Hangul::L;
    if (c >= 0x1160 && c <= 0x11a7)
        return Hangul::V;
    if (c >= 0x11a8 && c <= 0x11ff)
        return Hangul::T;
    if (c >= 0xac00 && c <= 0xd7a3)
        return (c - 0xac00) % 28 == 0 ? Hangul::LV : Hangul::LVT;
    return Hangul::None;
}

bool isControl(char32_t c) {
    if (c == u'\r' || c == u'\n')
        return true;
    const U::Category k = U::category(c);
    return k == U::Other_Control || k == U::Separator_Line || k == U::Separator_Paragraph;
}

bool isExtend(char32_t c) {
    const U::Category k = U::category(c);
    return k == U::Mark_NonSpacing || k == U::Mark_Enclosing || k == U::Mark_SpacingCombining || c == 0x200c || c == 0x200d ||
           (c >= 0x1f3fb && c <= 0x1f3ff) || (c >= 0xe0020 && c <= 0xe007f);
}

bool isRegionalIndicator(char32_t c) { return c >= 0x1f1e6 && c <= 0x1f1ff; }

bool isPictographic(char32_t c) {
    return (c >= 0x1f000 && c <= 0x1faff) || (c >= 0x2600 && c <= 0x27bf) || c == 0x00a9 || c == 0x00ae || (c >= 0x2190 && c <= 0x21ff);
}

// Returns, for each code-unit position 0..n, whether a grapheme cluster boundary is there.
std::vector<bool> graphemeBoundaries(const char16_t *s, qsizetype n) {
    std::vector<bool> b(static_cast<std::size_t>(n) + 1, false);
    b[0] = true;
    b[static_cast<std::size_t>(n)] = true;
    int riRun = 0; // regional indicators in a row before the current position
    char32_t prev = 0;
    bool havePrev = false;
    for (qsizetype i = 0; i < n;) {
        int len = 1;
        const char32_t c = U::codePointAt(s, n, i, &len);
        if (havePrev) {
            bool brk = true;
            const Hangul hp = hangulType(prev), hc = hangulType(c);
            if (prev == u'\r' && c == u'\n')
                brk = false;
            else if (isControl(prev) || isControl(c))
                brk = true;
            else if (hp == Hangul::L && (hc == Hangul::L || hc == Hangul::V || hc == Hangul::LV || hc == Hangul::LVT))
                brk = false;
            else if ((hp == Hangul::LV || hp == Hangul::V) && (hc == Hangul::V || hc == Hangul::T))
                brk = false;
            else if ((hp == Hangul::LVT || hp == Hangul::T) && hc == Hangul::T)
                brk = false;
            else if (isExtend(c))
                brk = false;
            else if (prev == 0x200d && isPictographic(c))
                brk = false;
            else if (isRegionalIndicator(prev) && isRegionalIndicator(c) && riRun % 2 == 1)
                brk = false;
            b[static_cast<std::size_t>(i)] = brk;
        }
        riRun = isRegionalIndicator(c) ? riRun + 1 : 0;
        prev = c;
        havePrev = true;
        i += len;
    }
    return b;
}

std::vector<Unit> clusters(const char16_t *s, qsizetype n, const std::vector<bool> &gb, bool absorbFormat) {
    std::vector<Unit> units;
    for (qsizetype i = 0; i < n; ++i) {
        if (!gb[static_cast<std::size_t>(i)])
            continue;
        int len = 1;
        const char32_t c = U::codePointAt(s, n, i, &len);
        // Format characters (such as a soft hyphen) join the preceding unit for words and sentences.
        if (absorbFormat && !units.empty() && U::category(c) == U::Other_Format && c != 0x200b)
            continue;
        units.push_back({i, c});
    }
    return units;
}

// ---------------------------------------------------------------- words

enum class WordClass { Other, CR, LF, Newline, ALetter, Numeric, Katakana, Hiragana, Ideographic, ExtendNumLet, MidLetter, MidNum, MidNumLet, SingleQuote, Space };

WordClass wordClass(char32_t c) {
    if (c == u'\r')
        return WordClass::CR;
    if (c == u'\n')
        return WordClass::LF;
    if (c == 0x0b || c == 0x0c || c == 0x85 || c == 0x2028 || c == 0x2029)
        return WordClass::Newline;
    if (c == u'\'')
        return WordClass::SingleQuote;
    if (c == u'.' || c == 0x2018 || c == 0x2019 || c == 0x2024 || c == 0xfe52 || c == 0xff07 || c == 0xff0e)
        return WordClass::MidNumLet;
    if (c == u':' || c == 0xb7 || c == 0x387 || c == 0x5f4 || c == 0x2027 || c == 0xfe13 || c == 0xfe55 || c == 0xff1a)
        return WordClass::MidLetter;
    if (c == u',' || c == u';' || c == 0x37e || c == 0x589 || c == 0x60c || c == 0x60d || c == 0x66c || c == 0x2044 || c == 0xfe10 ||
        c == 0xfe14 || c == 0xfe50 || c == 0xfe54 || c == 0xff0c || c == 0xff1b)
        return WordClass::MidNum;
    if ((c >= 0x30a0 && c <= 0x30ff) || (c >= 0x31f0 && c <= 0x31ff) || (c >= 0xff66 && c <= 0xff9f) || (c >= 0x3031 && c <= 0x3035))
        return WordClass::Katakana;
    if (c >= 0x3040 && c <= 0x309f)
        return WordClass::Hiragana;
    if ((c >= 0x3400 && c <= 0x4dbf) || (c >= 0x4e00 && c <= 0x9fff) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0x20000 && c <= 0x3134f))
        return WordClass::Ideographic;
    const U::Category k = U::category(c);
    if (k == U::Punctuation_Connector)
        return WordClass::ExtendNumLet;
    if (k == U::Number_DecimalDigit)
        return WordClass::Numeric;
    if (k == U::Separator_Space || c == u'\t')
        return WordClass::Space;
    if (U::isLetter(c) || k == U::Number_Letter)
        return (c >= 0x0e00 && c <= 0x0eff) ? WordClass::Other : WordClass::ALetter;
    return WordClass::Other;
}

bool isWordUnit(WordClass k) {
    return k == WordClass::ALetter || k == WordClass::Numeric || k == WordClass::Katakana || k == WordClass::Hiragana ||
           k == WordClass::Ideographic || k == WordClass::ExtendNumLet;
}

// Returns, for each unit boundary index j (between unit j-1 and unit j), whether the text breaks.
std::vector<bool> wordBreaks(const std::vector<WordClass> &w) {
    const std::size_t n = w.size();
    std::vector<bool> brk(n + 1, true);
    auto at = [&](std::ptrdiff_t i) { return i >= 0 && static_cast<std::size_t>(i) < n ? w[static_cast<std::size_t>(i)] : WordClass::Other; };
    auto isAH = [](WordClass k) { return k == WordClass::ALetter; };
    auto isMidL = [](WordClass k) { return k == WordClass::MidLetter || k == WordClass::MidNumLet || k == WordClass::SingleQuote; };
    auto isMidN = [](WordClass k) { return k == WordClass::MidNum || k == WordClass::MidNumLet || k == WordClass::SingleQuote; };
    for (std::size_t j = 1; j < n; ++j) {
        const auto i = static_cast<std::ptrdiff_t>(j);
        const WordClass a = at(i - 1), b = at(i);
        bool br = true;
        if (a == WordClass::CR && b == WordClass::LF)
            br = false;
        else if (a == WordClass::CR || a == WordClass::LF || a == WordClass::Newline || b == WordClass::CR || b == WordClass::LF ||
                 b == WordClass::Newline)
            br = true;
        else if (a == WordClass::Space && b == WordClass::Space)
            br = false;
        else if (isAH(a) && isAH(b))
            br = false;
        else if (isAH(a) && isMidL(b) && isAH(at(i + 1)))
            br = false;
        else if (isAH(at(i - 2)) && isMidL(a) && isAH(b))
            br = false;
        else if ((a == WordClass::Numeric || isAH(a)) && (b == WordClass::Numeric || isAH(b)))
            br = false;
        else if (at(i - 2) == WordClass::Numeric && isMidN(a) && b == WordClass::Numeric)
            br = false;
        else if (a == WordClass::Numeric && isMidN(b) && at(i + 1) == WordClass::Numeric)
            br = false;
        else if (a == WordClass::Katakana && b == WordClass::Katakana)
            br = false;
        else if ((isAH(a) || a == WordClass::Numeric || a == WordClass::Katakana || a == WordClass::ExtendNumLet) && b == WordClass::ExtendNumLet)
            br = false;
        else if (a == WordClass::ExtendNumLet && (isAH(b) || b == WordClass::Numeric || b == WordClass::Katakana))
            br = false;
        brk[j] = br;
    }
    return brk;
}

// ---------------------------------------------------------------- sentences

enum class SentenceClass { Other, Sep, CR, LF, Sp, Lower, Upper, OLetter, Numeric, ATerm, STerm, Close, SContinue };

SentenceClass sentenceClass(char32_t c) {
    if (c == u'\r')
        return SentenceClass::CR;
    if (c == u'\n')
        return SentenceClass::LF;
    if (c == 0x85 || c == 0x2028 || c == 0x2029)
        return SentenceClass::Sep;
    if (c == u'.' || c == 0x2024 || c == 0xfe52 || c == 0xff0e)
        return SentenceClass::ATerm;
    if (c == u'!' || c == u'?' || c == 0x589 || c == 0x61f || c == 0x6d4 || c == 0x964 || c == 0x965 || c == 0x203c || c == 0x203d ||
        c == 0x2047 || c == 0x2048 || c == 0x2049 || c == 0x3002 || c == 0xfe56 || c == 0xfe57 || c == 0xff01 || c == 0xff1f || c == 0xff61)
        return SentenceClass::STerm;
    if (c == u',' || c == u'-' || c == u':' || c == u';' || c == 0x55d || c == 0x60c || c == 0x60d || c == 0x7f8 || c == 0x1802 ||
        c == 0x1808 || c == 0x2013 || c == 0x2014 || c == 0x3001 || c == 0xfe10 || c == 0xfe11 || c == 0xfe13 || c == 0xfe31 || c == 0xfe32 ||
        c == 0xfe50 || c == 0xfe51 || c == 0xfe55 || c == 0xfe58 || c == 0xfe63 || c == 0xff0c || c == 0xff0d || c == 0xff1a || c == 0xff1b ||
        c == 0xff64)
        return SentenceClass::SContinue;
    const U::Category k = U::category(c);
    if (c == u'"' || c == u'\'' || k == U::Punctuation_Open || k == U::Punctuation_Close || k == U::Punctuation_InitialQuote ||
        k == U::Punctuation_FinalQuote)
        return SentenceClass::Close;
    if (U::isSpace(c))
        return SentenceClass::Sp;
    if (k == U::Letter_Lowercase)
        return SentenceClass::Lower;
    if (k == U::Letter_Uppercase || k == U::Letter_Titlecase)
        return SentenceClass::Upper;
    if (U::isLetter(c) || k == U::Number_Letter)
        return SentenceClass::OLetter;
    if (k == U::Number_DecimalDigit)
        return SentenceClass::Numeric;
    return SentenceClass::Other;
}

std::vector<bool> sentenceBreaks(const std::vector<SentenceClass> &w) {
    const std::size_t n = w.size();
    std::vector<bool> brk(n + 1, false);
    auto isSep = [](SentenceClass k) { return k == SentenceClass::Sep || k == SentenceClass::CR || k == SentenceClass::LF; };
    std::size_t i = 0;
    while (i < n) {
        const SentenceClass k = w[i];
        if (isSep(k)) {
            std::size_t j = i + 1;
            if (k == SentenceClass::CR && j < n && w[j] == SentenceClass::LF)
                ++j;
            if (j < n)
                brk[j] = true;
            i = j;
            continue;
        }
        if (k != SentenceClass::ATerm && k != SentenceClass::STerm) {
            ++i;
            continue;
        }
        std::size_t j = i + 1;
        if (k == SentenceClass::ATerm) {
            if (j < n && w[j] == SentenceClass::Numeric) { // 3.14
                i = j;
                continue;
            }
            if (i > 0 && (w[i - 1] == SentenceClass::Upper || w[i - 1] == SentenceClass::Lower) && j < n && w[j] == SentenceClass::Upper) {
                i = j; // U.S.A.
                continue;
            }
        }
        while (j < n && w[j] == SentenceClass::Close)
            ++j;
        while (j < n && w[j] == SentenceClass::Sp)
            ++j;
        if (j < n && (w[j] == SentenceClass::SContinue || w[j] == SentenceClass::ATerm || w[j] == SentenceClass::STerm)) {
            i = j;
            continue;
        }
        if (k == SentenceClass::ATerm) {
            std::size_t m = j;
            while (m < n && w[m] != SentenceClass::OLetter && w[m] != SentenceClass::Upper && w[m] != SentenceClass::Lower && !isSep(w[m]) &&
                   w[m] != SentenceClass::ATerm && w[m] != SentenceClass::STerm)
                ++m;
            if (m < n && w[m] == SentenceClass::Lower) { // "etc. and so on"
                i = m;
                continue;
            }
        }
        if (j < n && isSep(w[j])) {
            const bool crlf = w[j] == SentenceClass::CR && j + 1 < n && w[j + 1] == SentenceClass::LF;
            j += crlf ? 2 : 1;
        }
        if (j < n)
            brk[j] = true;
        i = j;
    }
    return brk;
}

// ---------------------------------------------------------------- lines

enum class LineClass { AL, BK, CR, LF, NL, SP, ZW, GL, WJ, CM, BA, HY, B2, OP, CL, QU, EX, IS, SY, ID, NU, PO, PR, IN };

LineClass lineClass(char32_t c) {
    switch (c) {
    case 0x0b:
    case 0x0c:
    case 0x2028:
    case 0x2029:
        return LineClass::BK;
    case u'\r':
        return LineClass::CR;
    case u'\n':
        return LineClass::LF;
    case 0x85:
        return LineClass::NL;
    case u' ':
        return LineClass::SP;
    case 0x200b:
        return LineClass::ZW;
    case 0x2060:
    case 0xfeff:
        return LineClass::WJ;
    case 0xa0:
    case 0x202f:
    case 0x2007:
    case 0x2011:
    case 0x0f0c:
        return LineClass::GL;
    case u'\t':
    case 0xad:
    case 0x2010:
    case 0x2012:
    case 0x2013:
    case 0x1680:
    case 0x205f:
        return LineClass::BA;
    case u'-':
        return LineClass::HY;
    case 0x2014:
        return LineClass::B2;
    case u'"':
    case u'\'':
        return LineClass::QU;
    case u'!':
    case u'?':
        return LineClass::EX;
    case u',':
    case u'.':
    case u':':
    case u';':
        return LineClass::IS;
    case u'/':
        return LineClass::SY;
    case u'%':
    case 0xa2:
    case 0xb0:
    case 0x2030:
    case 0x2031:
    case 0x2032:
    case 0x2033:
    case 0x2103:
        return LineClass::PO;
    case u'$':
    case u'+':
    case u'\\':
    case 0xa3:
    case 0xa5:
    case 0xb1:
    case 0x20ac:
    case 0x2116:
        return LineClass::PR;
    case 0x2024:
    case 0x2025:
    case 0x2026:
        return LineClass::IN;
    case 0x200d:
        return LineClass::CM;
    default:
        break;
    }
    if (c >= 0x2000 && c <= 0x200a && c != 0x2007)
        return LineClass::BA;
    const U::Category k = U::category(c);
    if (k == U::Mark_NonSpacing || k == U::Mark_SpacingCombining || k == U::Mark_Enclosing)
        return LineClass::CM;
    if (k == U::Punctuation_Open)
        return LineClass::OP;
    if (k == U::Punctuation_Close)
        return LineClass::CL;
    if (k == U::Punctuation_InitialQuote || k == U::Punctuation_FinalQuote)
        return LineClass::QU;
    if (k == U::Number_DecimalDigit)
        return LineClass::NU;
    if ((c >= 0x2e80 && c <= 0x9fff && !(c >= 0x3001 && c <= 0x3002)) || (c >= 0xac00 && c <= 0xd7a3) || (c >= 0xf900 && c <= 0xfaff) ||
        (c >= 0xff01 && c <= 0xff60 && k != U::Punctuation_Close && k != U::Punctuation_Other) || (c >= 0x20000 && c <= 0x3fffd) ||
        (c >= 0x1f000 && c <= 0x1faff))
        return LineClass::ID;
    if (c == 0x3001 || c == 0x3002 || c == 0xff0c || c == 0xff0e)
        return LineClass::CL;
    return LineClass::AL;
}

} // namespace

/*!
    Constructs an invalid finder.

    \sa isValid()
*/
QTextBoundaryFinder::QTextBoundaryFinder() = default;

/*!
    Constructs a finder of boundaries of \a type in \a string. The position
    is at the start of the string.
*/
QTextBoundaryFinder::QTextBoundaryFinder(BoundaryType type, const QString &string) : m_type(type), m_string(string), m_valid(true) {
    compute();
}

/*!
    \overload
    Constructs a finder of boundaries of \a type in the text viewed by
    \a string. The text is copied. \a buffer and \a bufferSize are ignored.
*/
QTextBoundaryFinder::QTextBoundaryFinder(BoundaryType type, QStringView string, unsigned char *buffer, qsizetype bufferSize)
    : QTextBoundaryFinder(type, string.toString()) {
    Q_UNUSED(buffer)
    Q_UNUSED(bufferSize)
}

/*!
    \overload
    Constructs a finder of boundaries of \a type in the \a length characters
    at \a chars. The text is copied. \a buffer and \a bufferSize are ignored.
*/
QTextBoundaryFinder::QTextBoundaryFinder(BoundaryType type, const QChar *chars, qsizetype length, unsigned char *buffer, qsizetype bufferSize)
    : QTextBoundaryFinder(type, QString(chars, length)) {
    Q_UNUSED(buffer)
    Q_UNUSED(bufferSize)
}

void QTextBoundaryFinder::compute() {
    const qsizetype n = m_string.size();
    const char16_t *s = m_string.utf16();
    m_reasons.assign(static_cast<std::size_t>(n) + 1, 0);
    const std::vector<bool> gb = graphemeBoundaries(s, n);
    auto set = [&](qsizetype pos, unsigned short r) { m_reasons[static_cast<std::size_t>(pos)] = r; };
    const unsigned short item = BreakOpportunity | StartOfItem | EndOfItem;

    switch (m_type) {
    case Grapheme:
        for (qsizetype p = 0; p <= n; ++p)
            if (gb[static_cast<std::size_t>(p)])
                set(p, item);
        break;
    case Word: {
        const std::vector<Unit> units = clusters(s, n, gb, true);
        std::vector<WordClass> classes;
        for (const Unit &u : units)
            classes.push_back(wordClass(u.cp));
        const std::vector<bool> brk = wordBreaks(classes);
        for (std::size_t j = 0; j <= units.size(); ++j) {
            if (j > 0 && j < units.size() && !brk[j])
                continue;
            const qsizetype pos = j < units.size() ? units[j].start : n;
            unsigned short r = BreakOpportunity;
            if (j < units.size() && isWordUnit(classes[j]))
                r |= StartOfItem;
            if (j > 0 && isWordUnit(classes[j - 1]))
                r |= EndOfItem;
            set(pos, r);
        }
        if (n == 0)
            set(0, BreakOpportunity);
        break;
    }
    case Sentence: {
        const std::vector<Unit> units = clusters(s, n, gb, true);
        std::vector<SentenceClass> classes;
        for (const Unit &u : units)
            classes.push_back(sentenceClass(u.cp));
        const std::vector<bool> brk = sentenceBreaks(classes);
        set(0, BreakOpportunity | StartOfItem);
        for (std::size_t j = 1; j < units.size(); ++j)
            if (brk[j])
                set(units[j].start, item);
        set(n, static_cast<unsigned short>(n == 0 ? BreakOpportunity : BreakOpportunity | EndOfItem));
        break;
    }
    case Line: {
        const std::vector<Unit> units = clusters(s, n, gb, false);
        std::vector<LineClass> cls;
        for (const Unit &u : units) {
            LineClass k = lineClass(u.cp);
            if (k == LineClass::CM)
                k = LineClass::AL; // a combining mark that starts a cluster
            cls.push_back(k);
        }
        auto isHardBreak = [](LineClass k) { return k == LineClass::BK || k == LineClass::LF || k == LineClass::NL || k == LineClass::CR; };
        for (std::size_t j = 1; j <= units.size(); ++j) {
            const qsizetype pos = j < units.size() ? units[j].start : n;
            const LineClass a = cls[j - 1];
            if (j == units.size()) {
                set(pos, BreakOpportunity | MandatoryBreak);
                break;
            }
            const LineClass b = cls[j];
            bool brk = false, mandatory = false;
            // The class before any run of spaces, for the rules that look past spaces.
            std::size_t k = j - 1;
            while (k > 0 && cls[k] == LineClass::SP)
                --k;
            const LineClass beforeSpaces = cls[k];
            if (a == LineClass::CR && b == LineClass::LF)
                brk = false;
            else if (isHardBreak(a))
                brk = mandatory = true;
            else if (isHardBreak(b) || b == LineClass::SP || b == LineClass::ZW)
                brk = false;
            else if (beforeSpaces == LineClass::ZW)
                brk = true;
            else if (a == LineClass::WJ || b == LineClass::WJ || a == LineClass::GL)
                brk = false;
            else if (b == LineClass::GL)
                brk = a == LineClass::SP || a == LineClass::BA || a == LineClass::HY;
            else if (b == LineClass::CL || b == LineClass::EX || b == LineClass::IS || b == LineClass::SY)
                brk = false;
            else if (beforeSpaces == LineClass::OP)
                brk = false;
            else if (beforeSpaces == LineClass::B2 && b == LineClass::B2)
                brk = false;
            else if (a == LineClass::SP)
                brk = true;
            else if (a == LineClass::QU || b == LineClass::QU)
                brk = false;
            else if (b == LineClass::BA || b == LineClass::HY || b == LineClass::IN)
                brk = false;
            else if ((a == LineClass::AL && b == LineClass::NU) || (a == LineClass::NU && b == LineClass::AL))
                brk = false;
            else if (a == LineClass::PR && (b == LineClass::AL || b == LineClass::ID || b == LineClass::NU || b == LineClass::OP))
                brk = false;
            else if ((a == LineClass::AL || a == LineClass::NU || a == LineClass::CL) && b == LineClass::PO)
                brk = false;
            else if ((a == LineClass::HY || a == LineClass::IS || a == LineClass::SY || a == LineClass::NU || a == LineClass::PO) && b == LineClass::NU)
                brk = false;
            else if (a == LineClass::AL && b == LineClass::AL)
                brk = false;
            else if (a == LineClass::IS && b == LineClass::AL)
                brk = false;
            else if ((a == LineClass::AL || a == LineClass::NU) && b == LineClass::OP)
                brk = false;
            else if (a == LineClass::CL && (b == LineClass::AL || b == LineClass::NU))
                brk = false;
            else
                brk = true;
            if (!brk)
                continue;
            unsigned short r = BreakOpportunity;
            if (mandatory)
                r |= MandatoryBreak;
            else if (pos > 0 && s[pos - 1] == 0xad)
                r |= SoftHyphen;
            set(pos, r);
        }
        if (n == 0)
            set(0, BreakOpportunity | MandatoryBreak);
        break;
    }
    }
    if (m_type == Grapheme && n >= 0) {
        m_reasons[0] = static_cast<unsigned short>(m_reasons[0] & ~EndOfItem);
        m_reasons[static_cast<std::size_t>(n)] = static_cast<unsigned short>(m_reasons[static_cast<std::size_t>(n)] & ~StartOfItem);
    }
}

/*!
    Moves the finder to the start of the string, position 0.

    \sa toEnd()
*/
void QTextBoundaryFinder::toStart() { m_pos = 0; }

/*!
    Moves the finder to the end of the string.

    \sa toStart()
*/
void QTextBoundaryFinder::toEnd() { m_pos = m_string.size(); }

/*!
    Sets the current position to \a position, clamped to the range from 0 to
    the length of the string.

    \sa position()
*/
void QTextBoundaryFinder::setPosition(qsizetype position) { m_pos = std::clamp<qsizetype>(position, 0, m_string.size()); }

/*!
    Moves to the next boundary and returns its position. Returns -1, and
    makes the position -1, if there is none.

    \sa toPreviousBoundary()
*/
qsizetype QTextBoundaryFinder::toNextBoundary() {
    if (!m_valid || m_pos < 0 || m_pos >= m_string.size()) {
        m_pos = -1;
        return -1;
    }
    ++m_pos;
    while (m_pos < m_string.size() && m_reasons[static_cast<std::size_t>(m_pos)] == 0)
        ++m_pos;
    return m_pos;
}

/*!
    Moves to the previous boundary and returns its position. Returns -1, and
    makes the position -1, if there is none.

    \sa toNextBoundary()
*/
qsizetype QTextBoundaryFinder::toPreviousBoundary() {
    if (!m_valid || m_pos <= 0 || m_pos > m_string.size()) {
        m_pos = -1;
        return -1;
    }
    --m_pos;
    while (m_pos > 0 && m_reasons[static_cast<std::size_t>(m_pos)] == 0)
        --m_pos;
    if (m_reasons[static_cast<std::size_t>(m_pos)] == 0) {
        m_pos = -1;
        return -1;
    }
    return m_pos;
}

/*!
    Returns \c true if the current position is a boundary; otherwise returns
    \c false.

    \sa boundaryReasons()
*/
bool QTextBoundaryFinder::isAtBoundary() const { return boundaryReasons() != NotAtBoundary; }

/*!
    Returns the reasons why the current position is a boundary, or
    NotAtBoundary.

    \sa isAtBoundary()
*/
QTextBoundaryFinder::BoundaryReasons QTextBoundaryFinder::boundaryReasons() const {
    if (!m_valid || m_pos < 0 || m_pos > m_string.size())
        return NotAtBoundary;
    return BoundaryReasons::fromInt(m_reasons[static_cast<std::size_t>(m_pos)]);
}
