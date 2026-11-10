// MIT License - see LICENSE
#include "qregexprogram_p.h"
#include "qregexunicode_p.h"

#include <algorithm>
#include <cstring>

namespace ntregex {

namespace {

struct ErrorEntry {
    int code;
    const char *text;
};

const ErrorEntry kErrors[] = {
    {101, "\\ at end of pattern"},
    {102, "\\c at end of pattern"},
    {103, "unrecognized character follows \\"},
    {104, "numbers out of order in {} quantifier"},
    {105, "number too big in {} quantifier"},
    {106, "missing terminating ] for character class"},
    {107, "escape sequence is invalid in character class"},
    {108, "range out of order in character class"},
    {109, "quantifier does not follow a repeatable item"},
    {111, "unrecognized character after (? or (?-"},
    {112, "POSIX named classes are supported only within a class"},
    {113, "POSIX collating elements are not supported"},
    {114, "missing closing parenthesis"},
    {115, "reference to non-existent subpattern"},
    {118, "missing ) after (?# comment"},
    {119, "parentheses are too deeply nested"},
    {120, "regular expression is too large"},
    {122, "unmatched closing parenthesis"},
    {124, "missing closing parenthesis for condition"},
    {125, "lookbehind assertion is not fixed length"},
    {127, "conditional subpattern contains more than two branches"},
    {128, "assertion expected after (?( or (?(?C)"},
    {129, "digit expected after (?+ or (?-"},
    {130, "unknown POSIX class name"},
    {134, "character code point value in \\x{} or \\o{} is too large"},
    {137, "PCRE2 does not support \\F, \\L, \\l, \\N{name}, \\U, or \\u"},
    {141, "unrecognized character after (?P"},
    {142, "syntax error in subpattern name (missing terminator?)"},
    {143, "two named subpatterns have the same name (PCRE2_DUPNAMES not set)"},
    {144, "subpattern name must start with a non-digit"},
    {146, "malformed \\P or \\p sequence"},
    {147, "unknown property after \\P or \\p"},
    {148, "subpattern name is too long (maximum 32 code units)"},
    {150, "invalid range in character class"},
    {155, "missing opening brace after \\o"},
    {157, "\\g is not followed by a braced, angle-bracketed, or quoted name/number or by a plain number"},
    {154, "DEFINE subpattern contains more than one branch"},
    {160, "(*VERB) not recognized or malformed"},
    {161, "subpattern number is too big"},
    {162, "subpattern name expected"},
    {164, "non-octal character in \\o{} (closing brace missing?)"},
    {167, "non-hex character in \\x{} (closing brace missing?)"},
    {168, "\\c must be followed by a printable ASCII character"},
    {169, "\\k is not followed by a braced, angle-bracketed, or quoted name"},
    {171, "\\N is not supported in a class"},
    {173, "disallowed Unicode code point (>= 0xd800 && <= 0xdfff)"},
    {178, "digits missing in \\x{} or \\o{} or \\N{U+}"},
    {197, "too many capturing groups (maximum 65535)"},
    {900, "recursive and subroutine calls are not supported"},
};

constexpr int kMaxNesting = 250;
constexpr int kMaxGroups = 65535;
constexpr int kMaxRepeat = 65535;
constexpr std::size_t kMaxProgram = 200000;

struct ParseError {
    int code;
    qsizetype offset;
};

bool isAsciiAlnum(char32_t c) {
    return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
}
bool isNameChar(char32_t c) { return isAsciiAlnum(c) || c == U'_'; }
int addDigit(int num, char16_t d) { return std::min(num * 10 + static_cast<int>(d - u'0'), 100000); }
bool isHex(char32_t c) { return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'f') || (c >= U'A' && c <= U'F'); }
int hexValue(char32_t c) {
    if (c <= U'9')
        return static_cast<int>(c - U'0');
    return static_cast<int>((c | 0x20) - U'a') + 10;
}

enum class NT : std::uint8_t {
    Empty,
    Char,
    Any,
    AnyUnit,
    Set,
    Seq,
    Alt,
    Group,
    Repeat,
    Assert,
    Backref,
    Look,
    Atomic,
    Cond,
    Keep,
    Grapheme
};

struct Node {
    NT type = NT::Empty;
    char32_t cp = 0;
    bool caseless = false;
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    std::vector<int> kids;
    qsizetype offset = 0;
};

struct Flags {
    bool i = false;
    bool m = false;
    bool s = false;
    bool x = false;
    bool U = false;
    bool n = false;
};

struct LenRange {
    int min;
    int max; // -1 when unbounded
};

constexpr int kLenCap = 1000000;

int satAdd(int a, int b) { return std::min(a + b, kLenCap); }

LenRange lenRange(const std::vector<Node> &nodes, int id) {
    const Node &nd = nodes[static_cast<std::size_t>(id)];
    switch (nd.type) {
    case NT::Char:
    case NT::Any:
    case NT::AnyUnit:
    case NT::Set:
        return {1, 1};
    case NT::Seq: {
        LenRange r{0, 0};
        for (int k : nd.kids) {
            const LenRange c = lenRange(nodes, k);
            r.min = satAdd(r.min, c.min);
            r.max = (r.max < 0 || c.max < 0) ? -1 : satAdd(r.max, c.max);
        }
        return r;
    }
    case NT::Alt: {
        LenRange r{kLenCap, 0};
        for (int k : nd.kids) {
            const LenRange c = lenRange(nodes, k);
            r.min = std::min(r.min, c.min);
            r.max = (r.max < 0 || c.max < 0) ? -1 : std::max(r.max, c.max);
        }
        return r;
    }
    case NT::Group:
    case NT::Atomic:
        return lenRange(nodes, nd.kids[0]);
    case NT::Repeat: {
        const LenRange c = lenRange(nodes, nd.kids[0]);
        LenRange r;
        r.min = static_cast<int>(std::min<long long>(static_cast<long long>(c.min) * nd.a, kLenCap));
        if (nd.b < 0)
            r.max = c.max == 0 ? 0 : -1;
        else if (c.max < 0)
            r.max = c.max;
        else
            r.max = static_cast<int>(std::min<long long>(static_cast<long long>(c.max) * nd.b, kLenCap));
        return r;
    }
    case NT::Cond: {
        LenRange r{kLenCap, 0};
        const std::size_t first = nd.a == 1 ? 1 : 0;
        for (std::size_t k = first; k < nd.kids.size(); ++k) {
            const LenRange c = lenRange(nodes, nd.kids[k]);
            r.min = std::min(r.min, c.min);
            r.max = (r.max < 0 || c.max < 0) ? -1 : std::max(r.max, c.max);
        }
        if (nd.kids.size() == first + 1 && nd.a != 2)
            r.min = 0;
        return r;
    }
    case NT::Backref:
    case NT::Grapheme:
        return {0, -1};
    default:
        return {0, 0};
    }
}

struct PropertyName {
    const char *name;
    std::uint32_t mask;
};

const PropertyName kCategoryProps[] = {
    {"l", kMaskL},
    {"letter", kMaskL},
    {"l&", catBit(Ll) | catBit(Lu) | catBit(Lt)},
    {"casedletter", catBit(Ll) | catBit(Lu) | catBit(Lt)},
    {"lu", catBit(Lu)},
    {"uppercaseletter", catBit(Lu)},
    {"ll", catBit(Ll)},
    {"lowercaseletter", catBit(Ll)},
    {"lt", catBit(Lt)},
    {"titlecaseletter", catBit(Lt)},
    {"lm", catBit(Lm)},
    {"modifierletter", catBit(Lm)},
    {"lo", catBit(Lo)},
    {"otherletter", catBit(Lo)},
    {"m", kMaskM},
    {"mark", kMaskM},
    {"combiningmark", kMaskM},
    {"mn", catBit(Mn)},
    {"nonspacingmark", catBit(Mn)},
    {"mc", catBit(Mc)},
    {"spacingmark", catBit(Mc)},
    {"me", catBit(Me)},
    {"enclosingmark", catBit(Me)},
    {"n", kMaskN},
    {"number", kMaskN},
    {"nd", catBit(Nd)},
    {"decimalnumber", catBit(Nd)},
    {"nl", catBit(Nl)},
    {"letternumber", catBit(Nl)},
    {"no", catBit(No)},
    {"othernumber", catBit(No)},
    {"p", kMaskP},
    {"punctuation", kMaskP},
    {"pc", catBit(Pc)},
    {"connectorpunctuation", catBit(Pc)},
    {"pd", catBit(Pd)},
    {"dashpunctuation", catBit(Pd)},
    {"ps", catBit(Ps)},
    {"openpunctuation", catBit(Ps)},
    {"pe", catBit(Pe)},
    {"closepunctuation", catBit(Pe)},
    {"pi", catBit(Pi)},
    {"initialpunctuation", catBit(Pi)},
    {"pf", catBit(Pf)},
    {"finalpunctuation", catBit(Pf)},
    {"po", catBit(Po)},
    {"otherpunctuation", catBit(Po)},
    {"s", kMaskS},
    {"symbol", kMaskS},
    {"sm", catBit(Sm)},
    {"mathsymbol", catBit(Sm)},
    {"sc", catBit(Sc)},
    {"currencysymbol", catBit(Sc)},
    {"sk", catBit(Sk)},
    {"modifiersymbol", catBit(Sk)},
    {"so", catBit(So)},
    {"othersymbol", catBit(So)},
    {"z", kMaskZ},
    {"separator", kMaskZ},
    {"zs", catBit(Zs)},
    {"spaceseparator", catBit(Zs)},
    {"zl", catBit(Zl)},
    {"lineseparator", catBit(Zl)},
    {"zp", catBit(Zp)},
    {"paragraphseparator", catBit(Zp)},
    {"c", kMaskC},
    {"other", kMaskC},
    {"cc", catBit(Cc)},
    {"control", catBit(Cc)},
    {"cf", catBit(Cf)},
    {"format", catBit(Cf)},
    {"cs", catBit(Cs)},
    {"surrogate", catBit(Cs)},
    {"co", catBit(Co)},
    {"privateuse", catBit(Co)},
    {"cn", catBit(Cn)},
    {"unassigned", catBit(Cn)},
};

bool isHorizontalSpace(char32_t c) {
    return c == 0x09 || c == 0x20 || c == 0xa0 || c == 0x1680 || c == 0x180e || (c >= 0x2000 && c <= 0x200a) ||
           c == 0x202f || c == 0x205f || c == 0x3000;
}
bool isVerticalSpace(char32_t c) { return (c >= 0x0a && c <= 0x0d) || c == 0x85 || c == 0x2028 || c == 0x2029; }
bool isAsciiWord(char32_t c) { return isAsciiAlnum(c) || c == U'_'; }

bool posixMatches(std::uint32_t cls, char32_t c, bool ucp) {
    if (!ucp || c < 0x80) {
        if (c >= 0x80)
            return false;
        switch (cls) {
        case PAlpha:
            return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
        case PDigit:
            return c >= U'0' && c <= U'9';
        case PAlnum:
            return isAsciiAlnum(c);
        case PUpper:
            return c >= U'A' && c <= U'Z';
        case PLower:
            return c >= U'a' && c <= U'z';
        case PSpace:
            return c == U' ' || (c >= 9 && c <= 13);
        case PBlank:
            return c == U' ' || c == U'\t';
        case PPunct:
            return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
        case PPrint:
            return c >= 32 && c <= 126;
        case PGraph:
            return c >= 33 && c <= 126;
        case PCntrl:
            return c < 32 || c == 127;
        case PXdigit:
            return isHex(c);
        case PWord:
            return isAsciiWord(c);
        default:
            return true;
        }
    }
    const std::uint32_t bit = catBit(categoryOf(c));
    switch (cls) {
    case PAlpha:
        return (bit & kMaskL) != 0;
    case PDigit:
        return (bit & catBit(Nd)) != 0;
    case PAlnum:
        return (bit & (kMaskL | kMaskN)) != 0;
    case PUpper:
        return (bit & catBit(Lu)) != 0;
    case PLower:
        return (bit & catBit(Ll)) != 0;
    case PSpace:
        return isUnicodeSpace(c);
    case PBlank:
        return isHorizontalSpace(c);
    case PPunct:
        return (bit & kMaskP) != 0 || (c < 256 && (bit & kMaskS) != 0);
    case PPrint:
        return (bit & (kMaskL | kMaskM | kMaskN | kMaskP | kMaskS | catBit(Zs))) != 0;
    case PGraph:
        return (bit & (kMaskL | kMaskM | kMaskN | kMaskP | kMaskS)) != 0;
    case PCntrl:
        return (bit & catBit(Cc)) != 0;
    case PXdigit:
        return false;
    case PWord:
        return isUnicodeWordChar(c);
    default:
        return false;
    }
}

} // namespace

const char *errorMessage(int code) {
    for (const ErrorEntry &e : kErrors)
        if (e.code == code)
            return e.text;
    return "internal error";
}

bool CharSet::matchesItems(char32_t c, bool caseVariant) const {
    for (const ClassItem &it : items) {
        if (caseVariant && it.kind != KPosix)
            continue;
        bool r = false;
        switch (it.kind) {
        case KDigit:
            r = (ucp || it.arg == 1) ? (c < 0x80 ? (c >= U'0' && c <= U'9') : categoryOf(c) == Nd)
                                     : (c >= U'0' && c <= U'9');
            break;
        case KWord:
            r = (ucp || it.arg == 1) ? isUnicodeWordChar(c) : isAsciiWord(c);
            break;
        case KSpace:
            r = (ucp || it.arg == 1) ? isUnicodeSpace(c) : (c == U' ' || (c >= 9 && c <= 13));
            break;
        case KHSpace:
            r = isHorizontalSpace(c);
            break;
        case KVSpace:
            r = isVerticalSpace(c);
            break;
        case KPosix:
            r = posixMatches(it.arg, c, ucp);
            break;
        case KCategory:
            r = (catBit(categoryOf(c)) & it.arg) != 0;
            break;
        case KScript:
            r = scriptOf(c) == static_cast<int>(it.arg);
            break;
        case KAny:
            r = true;
            break;
        case KWordPlus:
            r = isUnicodeWordChar(c);
            break;
        }
        if (r != it.negated)
            return true;
    }
    return false;
}

bool CharSet::matchesPositive(char32_t c, bool caseVariant) const {
    for (const auto &r : ranges)
        if (c >= r.first && c <= r.second)
            return true;
    return matchesItems(c, caseVariant);
}

bool CharSet::evaluate(char32_t c) const {
    bool hit = matchesPositive(c, false);
    if (!hit && caseless) {
        const char32_t lo = toLowerSimple(c);
        const char32_t up = toUpperSimple(c);
        const char32_t fo = foldCase(c);
        if (lo != c)
            hit = matchesPositive(lo, true);
        if (!hit && up != c)
            hit = matchesPositive(up, true);
        if (!hit && fo != c && fo != lo)
            hit = matchesPositive(fo, true);
        if (!hit) {
            // Other members of the case-equivalence class, such as U+212A for k.
            if (fo == U'k')
                hit = matchesPositive(0x212a, true);
            else if (fo == U's')
                hit = matchesPositive(0x17f, true);
        }
    }
    return hit != negated;
}

void CharSet::finish() {
    ascii[0] = ascii[1] = 0;
    for (char32_t c = 0; c < 128; ++c)
        if (evaluate(c))
            ascii[c >> 6] |= std::uint64_t(1) << (c & 63);
}

namespace {

class Parser {
public:
    Parser(const char16_t *pattern, qsizetype length, int options)
        : p(pattern), n(length), ucp((options & OptUcp) != 0) {
        base.i = (options & OptCaseless) != 0;
        base.s = (options & OptDotAll) != 0;
        base.m = (options & OptMultiline) != 0;
        base.x = (options & OptExtended) != 0;
        base.U = (options & OptUngreedy) != 0;
        base.n = (options & OptNoCapture) != 0;
        prog = std::make_shared<Program>();
        prog->groupNames.emplace_back();
    }

    std::shared_ptr<Program> prog;
    std::vector<Node> nodes;
    int root = 0;
    int groupCount = 0;
    int registerCount = 0;

    void parse() {
        Flags f = base;
        root = parseAlternation(f, 0, false);
        if (i < n)
            throw ParseError{122, i};
        for (const Pending &pd : pendingNames) {
            int g = -1;
            for (std::size_t k = 0; k < prog->groupNames.size(); ++k)
                if (!pd.name.empty() && prog->groupNames[k] == pd.name) {
                    g = static_cast<int>(k);
                    break;
                }
            if (g < 0)
                throw ParseError{115, pd.offset};
            Node &target = nodes[static_cast<std::size_t>(pd.node)];
            if (target.type == NT::Cond)
                target.b = g;
            else
                target.a = g;
        }
        for (const PendingNum &pd : pendingNums)
            if (pd.number > groupCount)
                throw ParseError{115, pd.offset};
        prog->captureCount = groupCount;
    }

private:
    struct Pending {
        int node;
        std::u16string name;
        qsizetype offset;
    };
    struct PendingNum {
        int number;
        qsizetype offset;
    };

    const char16_t *p;
    qsizetype n;
    qsizetype i = 0;
    bool ucp;
    Flags base;
    bool inQuote = false;
    std::vector<Pending> pendingNames;
    std::vector<PendingNum> pendingNums;

    char32_t peek(qsizetype k = 0) const { return i + k < n ? p[i + k] : 0; }
    bool at(char32_t c, qsizetype k = 0) const { return i + k < n && p[i + k] == c; }

    int make(NT t) {
        nodes.emplace_back();
        nodes.back().type = t;
        nodes.back().offset = i;
        return static_cast<int>(nodes.size()) - 1;
    }
    Node &node(int id) { return nodes[static_cast<std::size_t>(id)]; }

    char32_t readCp() {
        const char16_t c = p[i++];
        if ((c & 0xfc00) == 0xd800 && i < n && (p[i] & 0xfc00) == 0xdc00) {
            const char32_t r = 0x10000 + ((static_cast<char32_t>(c) - 0xd800) << 10) + (p[i] - 0xdc00);
            ++i;
            return r;
        }
        return c;
    }

    int makeChar(char32_t cp, const Flags &f) {
        const int id = make(NT::Char);
        node(id).cp = cp;
        // ASCII characters other than letters have no other case; for the rest it is not worth asking.
        node(id).caseless = f.i && (cp >= 0x80 || (cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z'));
        return id;
    }

    int makeSet(CharSet &&s) {
        s.finish();
        prog->sets.push_back(std::move(s));
        const int id = make(NT::Set);
        node(id).a = static_cast<int>(prog->sets.size()) - 1;
        return id;
    }

    CharSet newSet(const Flags &f) const {
        CharSet s;
        s.caseless = f.i;
        s.ucp = ucp;
        return s;
    }

    int makeItemSet(ClassItemKind kind, bool neg, std::uint32_t arg, const Flags &f) {
        CharSet s = newSet(f);
        s.items.push_back({kind, neg, arg});
        return makeSet(std::move(s));
    }

    void skipExtended(const Flags &f) {
        if (!f.x || inQuote)
            return;
        while (i < n) {
            const char16_t c = p[i];
            if (c == U' ' || (c >= 9 && c <= 13)) {
                ++i;
            } else if (c == U'#') {
                while (i < n && p[i] != U'\n')
                    ++i;
            } else {
                break;
            }
        }
    }

    int parseAlternation(Flags &f, int depth, bool branchReset) {
        std::vector<int> alts;
        const int resetBase = groupCount;
        int maxGroup = groupCount;
        for (;;) {
            alts.push_back(parseSeq(f, depth));
            maxGroup = std::max(maxGroup, groupCount);
            if (at(U'|')) {
                ++i;
                if (branchReset)
                    groupCount = resetBase;
                continue;
            }
            break;
        }
        if (branchReset)
            groupCount = maxGroup;
        if (alts.size() == 1)
            return alts[0];
        const int id = make(NT::Alt);
        node(id).kids = alts;
        return id;
    }

    // Parses a quantifier at the current position. Returns false (position unchanged) if none.
    bool parseQuantifier(const Flags &f, int &mn, int &mx, int &mode) {
        const char16_t c = i < n ? p[i] : 0;
        if (c == U'*') {
            mn = 0;
            mx = -1;
            ++i;
        } else if (c == U'+') {
            mn = 1;
            mx = -1;
            ++i;
        } else if (c == U'?') {
            mn = 0;
            mx = 1;
            ++i;
        } else if (c == U'{') {
            qsizetype j = i + 1;
            qsizetype v1 = 0;
            qsizetype d1 = 0;
            while (j < n && p[j] >= U'0' && p[j] <= U'9') {
                v1 = std::min<qsizetype>(v1 * 10 + (p[j] - U'0'), 1000000);
                ++j;
                ++d1;
            }
            if (d1 == 0)
                return false;
            qsizetype v2 = v1;
            if (j < n && p[j] == U',') {
                ++j;
                qsizetype d2 = 0;
                v2 = 0;
                while (j < n && p[j] >= U'0' && p[j] <= U'9') {
                    v2 = std::min<qsizetype>(v2 * 10 + (p[j] - U'0'), 1000000);
                    ++j;
                    ++d2;
                }
                if (d2 == 0)
                    v2 = -1;
            }
            if (j >= n || p[j] != U'}')
                return false;
            if (v1 > kMaxRepeat || v2 > kMaxRepeat)
                throw ParseError{105, j};
            if (v2 >= 0 && v2 < v1)
                throw ParseError{104, j + 1};
            mn = static_cast<int>(v1);
            mx = static_cast<int>(v2);
            i = j + 1;
        } else {
            return false;
        }
        mode = f.U ? RepLazy : RepGreedy;
        if (at(U'?')) {
            mode = f.U ? RepGreedy : RepLazy;
            ++i;
        } else if (at(U'+')) {
            mode = RepPossessive;
            ++i;
        }
        return true;
    }

    bool quantifierAhead() const {
        const char16_t c = i < n ? p[i] : 0;
        if (c == U'*' || c == U'+' || c == U'?')
            return true;
        if (c != U'{')
            return false;
        qsizetype j = i + 1;
        qsizetype d = 0;
        while (j < n && p[j] >= U'0' && p[j] <= U'9') {
            ++j;
            ++d;
        }
        if (d == 0)
            return false;
        if (j < n && p[j] == U',') {
            ++j;
            while (j < n && p[j] >= U'0' && p[j] <= U'9')
                ++j;
        }
        return j < n && p[j] == U'}';
    }

    static bool repeatable(NT t) { return t != NT::Assert && t != NT::Keep; }

    int parseSeq(Flags &f, int depth) {
        std::vector<int> items;
        for (;;) {
            skipExtended(f);
            if (i >= n)
                break;
            if (inQuote) {
                if (at(U'\\') && at(U'E', 1)) {
                    i += 2;
                    inQuote = false;
                    continue;
                }
            } else if (at(U'|') || at(U')')) {
                if (at(U')') && depth == 0)
                    throw ParseError{122, i};
                break;
            }
            int atom;
            if (inQuote)
                atom = makeChar(readCp(), f);
            else
                atom = parseAtom(f, depth);
            if (atom < 0)
                continue;
            if (inQuote) {
                if (at(U'\\') && at(U'E', 1)) {
                    i += 2;
                    inQuote = false;
                } else {
                    items.push_back(atom);
                    continue;
                }
            }
            skipExtended(f);
            int mn = 0;
            int mx = 0;
            int mode = 0;
            const qsizetype qpos = i;
            if (quantifierAhead()) {
                if (!repeatable(node(atom).type))
                    throw ParseError{109, qpos};
                parseQuantifier(f, mn, mx, mode);
                const int r = make(NT::Repeat);
                node(r).kids.push_back(atom);
                node(r).a = mn;
                node(r).b = mx;
                node(r).c = mode;
                atom = r;
            }
            items.push_back(atom);
        }
        if (items.empty())
            return make(NT::Empty);
        if (items.size() == 1)
            return items[0];
        const int s = make(NT::Seq);
        node(s).kids = items;
        return s;
    }

    int parseAtom(Flags &f, int depth) {
        const char16_t c = p[i];
        switch (c) {
        case U'(':
            return parseGroup(f, depth);
        case U'[':
            ++i;
            return parseClass(f);
        case U'.': {
            ++i;
            const int id = make(NT::Any);
            node(id).a = f.s ? 1 : 0;
            return id;
        }
        case U'^': {
            ++i;
            const int id = make(NT::Assert);
            node(id).a = f.m ? AssertStartLine : AssertStartSubject;
            return id;
        }
        case U'$': {
            ++i;
            const int id = make(NT::Assert);
            node(id).a = f.m ? AssertEndLine : AssertEndSubjectNl;
            return id;
        }
        case U'*':
        case U'+':
        case U'?':
            throw ParseError{109, i};
        case U'{':
            if (quantifierAhead())
                throw ParseError{109, i};
            ++i;
            return makeChar(U'{', f);
        case U'\\':
            ++i;
            return parseEscape(f);
        default:
            return makeChar(readCp(), f);
        }
    }

    // --- names -----------------------------------------------------------------------------

    std::u16string parseName(char16_t terminator) {
        std::u16string name;
        if (i >= n)
            throw ParseError{162, i};
        if (p[i] >= U'0' && p[i] <= U'9')
            throw ParseError{144, i};
        while (i < n && (isNameChar(p[i]) || p[i] >= 0x80))
            name.push_back(p[i++]);
        if (name.empty())
            throw ParseError{162, i};
        if (name.size() > 32)
            throw ParseError{148, i};
        if (i >= n || p[i] != terminator)
            throw ParseError{142, i};
        ++i;
        return name;
    }

    int newGroupNumber() {
        if (groupCount >= kMaxGroups)
            throw ParseError{197, i};
        return ++groupCount;
    }

    void nameGroup(int number, const std::u16string &name, qsizetype offset) {
        if (prog->groupNames.size() <= static_cast<std::size_t>(number))
            prog->groupNames.resize(static_cast<std::size_t>(number) + 1);
        for (std::size_t k = 0; k < prog->groupNames.size(); ++k)
            if (prog->groupNames[k] == name && static_cast<int>(k) != number)
                throw ParseError{143, offset};
        prog->groupNames[static_cast<std::size_t>(number)] = name;
    }

    int makeBackrefByNumber(int number, const Flags &f, qsizetype endOffset) {
        const int id = make(NT::Backref);
        node(id).a = number;
        node(id).caseless = f.i;
        pendingNums.push_back({number, endOffset});
        return id;
    }

    int makeBackrefByName(const std::u16string &name, const Flags &f, qsizetype offset) {
        const int id = make(NT::Backref);
        node(id).caseless = f.i;
        pendingNames.push_back({id, name, offset});
        return id;
    }

    // --- groups ----------------------------------------------------------------------------

    int finishGroup(Flags &inner, int depth, NT type, int number, bool branchReset, qsizetype start) {
        if (depth + 1 > kMaxNesting)
            throw ParseError{119, start};
        const int body = parseAlternation(inner, depth + 1, branchReset);
        if (!at(U')'))
            throw ParseError{114, n};
        ++i;
        const int id = make(type);
        node(id).a = number;
        node(id).kids.push_back(body);
        node(id).offset = start;
        return id;
    }

    int parseGroup(Flags &f, int depth) {
        const qsizetype start = i;
        ++i; // (
        if (at(U'*'))
            throw ParseError{160, i + 1};
        if (!at(U'?')) {
            Flags inner = f;
            int number = -1;
            if (!f.n)
                number = newGroupNumber();
            const int id = finishGroup(inner, depth, NT::Group, number, false, start);
            return id;
        }
        ++i; // ?
        if (i >= n)
            throw ParseError{111, i};
        const char16_t c = p[i];
        Flags inner = f;
        switch (c) {
        case U'#':
            while (i < n && p[i] != U')')
                ++i;
            if (i >= n)
                throw ParseError{118, n};
            ++i;
            return -1;
        case U':':
            ++i;
            return finishGroup(inner, depth, NT::Group, -1, false, start);
        case U'|':
            ++i;
            return finishGroup(inner, depth, NT::Group, -1, true, start);
        case U'>':
            ++i;
            return finishGroup(inner, depth, NT::Atomic, 0, false, start);
        case U'=':
        case U'!': {
            ++i;
            const int id = finishGroup(inner, depth, NT::Look, 0, false, start);
            node(id).b = c == U'!' ? 1 : 0;
            return id;
        }
        case U'<':
            if (at(U'=', 1) || at(U'!', 1)) {
                const bool neg = p[i + 1] == U'!';
                i += 2;
                const int id = finishGroup(inner, depth, NT::Look, 1, false, start);
                node(id).b = neg ? 1 : 0;
                checkLookbehind(id, start);
                return id;
            }
            ++i;
            return parseNamedGroup(inner, depth, U'>', start);
        case U'\'':
            ++i;
            return parseNamedGroup(inner, depth, U'\'', start);
        case U'P':
            ++i;
            if (at(U'<')) {
                ++i;
                return parseNamedGroup(inner, depth, U'>', start);
            }
            if (at(U'=')) {
                ++i;
                const qsizetype nameStart = i;
                const std::u16string name = parseName(U')');
                return makeBackrefByName(name, f, nameStart);
            }
            if (at(U'>'))
                throw ParseError{900, i};
            throw ParseError{141, i};
        case U'(':
            return parseConditional(f, depth, start);
        case U'R':
        case U'&':
            throw ParseError{900, i};
        default:
            break;
        }
        if ((c >= U'0' && c <= U'9') || ((c == U'+' || c == U'-') && peek(1) >= U'0' && peek(1) <= U'9'))
            throw ParseError{900, i};
        // Option setting.
        bool on = true;
        Flags nf = f;
        if (at(U'^')) {
            nf.i = nf.m = nf.s = nf.x = nf.n = false;
            ++i;
        }
        for (;;) {
            if (i >= n)
                throw ParseError{114, n};
            const char16_t o = p[i];
            if (o == U')' || o == U':')
                break;
            switch (o) {
            case U'-':
                on = false;
                break;
            case U'i':
                nf.i = on;
                break;
            case U'm':
                nf.m = on;
                break;
            case U's':
                nf.s = on;
                break;
            case U'x':
                nf.x = on;
                break;
            case U'n':
                nf.n = on;
                break;
            case U'U':
                nf.U = on;
                break;
            default:
                throw ParseError{111, i};
            }
            ++i;
        }
        if (p[i] == U')') {
            ++i;
            f = nf;
            return -1;
        }
        ++i; // :
        return finishGroup(nf, depth, NT::Group, -1, false, start);
    }

    int parseNamedGroup(Flags &inner, int depth, char16_t terminator, qsizetype start) {
        const qsizetype nameStart = i;
        const std::u16string name = parseName(terminator);
        const int number = newGroupNumber();
        nameGroup(number, name, nameStart);
        return finishGroup(inner, depth, NT::Group, number, false, start);
    }

    int parseConditional(Flags &f, int depth, qsizetype start) {
        // At the "(" of the condition.
        Flags inner = f;
        const int cond = make(NT::Cond);
        node(cond).offset = start;
        if (at(U'(', 0) && at(U'?', 1)) {
            const int look = parseGroup(inner, depth + 1);
            if (look < 0 || node(look).type != NT::Look)
                throw ParseError{128, i};
            node(cond).a = 1;
            node(cond).kids.push_back(look);
        } else {
            ++i; // (
            if (depth + 1 > kMaxNesting)
                throw ParseError{119, start};
            const char16_t c = i < n ? p[i] : 0;
            if (c >= U'0' && c <= U'9') {
                int num = 0;
                while (i < n && p[i] >= U'0' && p[i] <= U'9') {
                    num = addDigit(num, p[i]);
                    ++i;
                }
                if (!at(U')'))
                    throw ParseError{124, i};
                ++i;
                node(cond).b = num;
                pendingNums.push_back({num, i});
            } else if (c == U'+' || c == U'-') {
                const bool neg = c == U'-';
                ++i;
                if (!(peek() >= U'0' && peek() <= U'9'))
                    throw ParseError{129, i};
                int num = 0;
                while (i < n && p[i] >= U'0' && p[i] <= U'9') {
                    num = addDigit(num, p[i]);
                    ++i;
                }
                if (!at(U')'))
                    throw ParseError{124, i};
                ++i;
                const int target = neg ? groupCount + 1 - num : groupCount + num;
                if (target < 1)
                    throw ParseError{115, i};
                node(cond).b = target;
                pendingNums.push_back({target, i});
            } else if (c == U'<' || c == U'\'') {
                ++i;
                const qsizetype nameStart = i;
                const std::u16string name = parseName(c == U'<' ? U'>' : U'\'');
                if (!at(U')'))
                    throw ParseError{124, i};
                ++i;
                pendingNames.push_back({cond, name, nameStart});
                node(cond).c = 1; // the group number is stored in a by the resolver
            } else {
                std::u16string name;
                const qsizetype nameStart = i;
                while (i < n && isNameChar(p[i]))
                    name.push_back(p[i++]);
                if (!at(U')') || name.empty())
                    throw ParseError{124, i};
                ++i;
                if (name == u"DEFINE") {
                    node(cond).a = 2;
                } else if (name == u"R" || name.rfind(u"R&", 0) == 0 || (name[0] == u'R' && name.size() > 1) ||
                           name.rfind(u"VERSION", 0) == 0) {
                    throw ParseError{900, nameStart};
                } else {
                    pendingNames.push_back({cond, name, nameStart});
                    node(cond).c = 1;
                }
            }
        }
        // The branches. A condition's group body ends at the closing parenthesis.
        const int body = parseAlternation(inner, depth + 1, false);
        if (!at(U')'))
            throw ParseError{114, n};
        ++i;
        const Node &bn = node(body);
        if (bn.type == NT::Alt) {
            if (bn.kids.size() > 2)
                throw ParseError{127, i};
            const std::vector<int> kids = bn.kids;
            for (int k : kids)
                node(cond).kids.push_back(k);
        } else {
            node(cond).kids.push_back(body);
        }
        if (node(cond).a == 2 && node(cond).kids.size() > 1)
            throw ParseError{154, i};
        if (node(cond).a != 2 && node(cond).kids.size() == (node(cond).a == 1 ? 2U : 1U))
            node(cond).kids.push_back(make(NT::Empty));
        return cond;
    }

    void checkLookbehind(int id, qsizetype start) {
        const LenRange r = lenRange(nodes, node(id).kids[0]);
        if (r.max < 0)
            throw ParseError{125, start};
        node(id).c = r.min;
        node(id).d = r.max;
    }


    // --- escapes ---------------------------------------------------------------------------

    char32_t parseHexEscape() {
        if (at(U'{')) {
            ++i;
            std::uint32_t v = 0;
            int digits = 0;
            while (i < n && isHex(p[i])) {
                v = v * 16 + static_cast<std::uint32_t>(hexValue(p[i]));
                if (v > 0x10ffff)
                    v = 0x110000;
                ++i;
                ++digits;
            }
            if (i >= n || p[i] != U'}')
                throw ParseError{167, i};
            ++i;
            if (digits == 0)
                throw ParseError{178, i};
            if (v > 0x10ffff)
                throw ParseError{134, i};
            if (v >= 0xd800 && v <= 0xdfff)
                throw ParseError{173, i};
            return v;
        }
        std::uint32_t v = 0;
        for (int k = 0; k < 2 && i < n && isHex(p[i]); ++k)
            v = v * 16 + static_cast<std::uint32_t>(hexValue(p[i++]));
        return v;
    }

    char32_t parseOctalBraced() {
        if (!at(U'{'))
            throw ParseError{155, i};
        ++i;
        std::uint32_t v = 0;
        int digits = 0;
        while (i < n && p[i] >= U'0' && p[i] <= U'7') {
            v = v * 8 + static_cast<std::uint32_t>(p[i] - U'0');
            if (v > 0x10ffff)
                v = 0x110000;
            ++i;
            ++digits;
        }
        if (i >= n || p[i] != U'}')
            throw ParseError{164, i};
        ++i;
        if (digits == 0)
            throw ParseError{178, i};
        if (v > 0x10ffff)
            throw ParseError{134, i};
        if (v >= 0xd800 && v <= 0xdfff)
            throw ParseError{173, i};
        return v;
    }

    // Handles the escapes that denote a single character; \a c has been consumed.
    char32_t parseCharEscape(char16_t c) {
        switch (c) {
        case U'a':
            return 7;
        case U'e':
            return 0x1b;
        case U'f':
            return 12;
        case U'n':
            return 10;
        case U'r':
            return 13;
        case U't':
            return 9;
        case U'0': {
            std::uint32_t v = 0;
            for (int k = 0; k < 2 && i < n && p[i] >= U'0' && p[i] <= U'7'; ++k)
                v = v * 8 + static_cast<std::uint32_t>(p[i++] - U'0');
            return v;
        }
        case U'x':
            return parseHexEscape();
        case U'o':
            return parseOctalBraced();
        case U'c': {
            if (i >= n)
                throw ParseError{102, n};
            const char16_t ch = p[i++];
            if (ch < 32 || ch > 126)
                throw ParseError{168, i};
            const char16_t up = (ch >= U'a' && ch <= U'z') ? static_cast<char16_t>(ch - 32) : ch;
            return static_cast<char32_t>(up ^ 0x40);
        }
        case U'F':
        case U'L':
        case U'l':
        case U'U':
        case U'u':
            throw ParseError{137, i};
        default:
            break;
        }
        if (isAsciiAlnum(c))
            throw ParseError{103, i};
        if ((c & 0xfc00) == 0xd800 && i < n && (p[i] & 0xfc00) == 0xdc00) {
            const char32_t r = 0x10000 + ((static_cast<char32_t>(c) - 0xd800) << 10) + (p[i] - 0xdc00);
            ++i;
            return r;
        }
        return c;
    }

    int parseProperty(bool negated, const Flags &f) {
        std::string name;
        bool neg = negated;
        if (at(U'{')) {
            ++i;
            if (at(U'^')) {
                neg = !neg;
                ++i;
            }
            while (i < n && p[i] != U'}') {
                const char16_t c = p[i++];
                if (c > 0x7f)
                    throw ParseError{147, n};
                if (c == U' ' || c == U'_' || c == U'-')
                    continue;
                name.push_back(static_cast<char>((c >= U'A' && c <= U'Z') ? c + 32 : c));
            }
            if (i >= n)
                throw ParseError{146, n};
            ++i;
        } else {
            if (i >= n)
                throw ParseError{146, n};
            const char16_t c = p[i++];
            if (c > 0x7f)
                throw ParseError{147, i};
            name.push_back(static_cast<char>((c >= U'A' && c <= U'Z') ? c + 32 : c));
        }
        if (name.empty())
            throw ParseError{146, i};
        for (const char *prefix : {"script=", "sc=", "scriptextensions=", "scx=", "generalcategory=", "gc=",
                                   "category="}) {
            const std::size_t len = std::strlen(prefix);
            if (name.compare(0, len, prefix) == 0) {
                name.erase(0, len);
                break;
            }
        }
        if (name == "any")
            return makeItemSet(KAny, neg, 0, f);
        if (name == "xan")
            return makeItemSet(KCategory, neg, kMaskL | kMaskN, f);
        if (name == "xwd")
            return makeItemSet(KWordPlus, neg, 0, f);
        if (name == "xsp" || name == "xps")
            return makeItemSet(KSpace, neg, 1, f);
        for (const PropertyName &pn : kCategoryProps)
            if (name == pn.name)
                return makeItemSet(KCategory, neg, pn.mask, f);
        const int script = scriptByName(name.c_str());
        if (script >= 0)
            return makeItemSet(KScript, neg, static_cast<std::uint32_t>(script), f);
        throw ParseError{147, i};
    }

    int parseBackrefAfterG(const Flags &f) {
        // After "\g".
        if (at(U'{')) {
            ++i;
            const qsizetype nameStart = i;
            bool neg = false;
            bool signedNum = false;
            if (at(U'-') || at(U'+')) {
                neg = at(U'-');
                signedNum = true;
                ++i;
            }
            if (peek() >= U'0' && peek() <= U'9') {
                int num = 0;
                while (i < n && p[i] >= U'0' && p[i] <= U'9') {
                    num = addDigit(num, p[i]);
                    ++i;
                }
                if (!at(U'}'))
                    throw ParseError{157, i};
                ++i;
                return numberedRef(num, signedNum, neg, f);
            }
            if (signedNum)
                throw ParseError{129, i};
            i = nameStart;
            const std::u16string name = parseName(U'}');
            return makeBackrefByName(name, f, nameStart);
        }
        if (at(U'<') || at(U'\''))
            throw ParseError{900, i};
        bool neg = false;
        bool signedNum = false;
        if (at(U'-') || at(U'+')) {
            neg = at(U'-');
            signedNum = true;
            ++i;
        }
        if (!(peek() >= U'0' && peek() <= U'9'))
            throw ParseError{157, i};
        int num = 0;
        while (i < n && p[i] >= U'0' && p[i] <= U'9') {
            num = addDigit(num, p[i]);
            ++i;
        }
        return numberedRef(num, signedNum, neg, f);
    }

    int numberedRef(int num, bool relative, bool neg, const Flags &f) {
        int target = num;
        if (relative)
            target = neg ? groupCount + 1 - num : groupCount + num;
        if (target < 1)
            throw ParseError{115, i};
        return makeBackrefByNumber(target, f, i);
    }

    int parseEscape(Flags &f) {
        if (i >= n)
            throw ParseError{101, n};
        const char16_t c = p[i++];
        switch (c) {
        case U'd':
        case U'D':
            return makeItemSet(KDigit, c == U'D', 0, f);
        case U'w':
        case U'W':
            return makeItemSet(KWord, c == U'W', 0, f);
        case U's':
        case U'S':
            return makeItemSet(KSpace, c == U'S', 0, f);
        case U'h':
        case U'H':
            return makeItemSet(KHSpace, c == U'H', 0, f);
        case U'v':
        case U'V':
            return makeItemSet(KVSpace, c == U'V', 0, f);
        case U'p':
        case U'P':
            return parseProperty(c == U'P', f);
        case U'R': {
            const int crlf = make(NT::Seq);
            Flags plain;
            const int cr = makeChar(13, plain);
            const int lf = makeChar(10, plain);
            node(crlf).kids = {cr, lf};
            CharSet s = newSet(plain);
            s.caseless = false;
            s.ranges = {{0x0a, 0x0d}, {0x85, 0x85}, {0x2028, 0x2029}};
            const int set = makeSet(std::move(s));
            const int alt = make(NT::Alt);
            node(alt).kids = {crlf, set};
            const int at2 = make(NT::Atomic);
            node(at2).kids = {alt};
            return at2;
        }
        case U'N': {
            if (at(U'{')) {
                if (at(U'U', 1) && at(U'+', 2)) {
                    i += 3;
                    std::uint32_t v = 0;
                    int digits = 0;
                    while (i < n && isHex(p[i])) {
                        v = v * 16 + static_cast<std::uint32_t>(hexValue(p[i++]));
                        if (v > 0x10ffff)
                            v = 0x110000;
                        ++digits;
                    }
                    if (i >= n || p[i] != U'}')
                        throw ParseError{167, i};
                    ++i;
                    if (digits == 0)
                        throw ParseError{178, i};
                    if (v > 0x10ffff)
                        throw ParseError{134, i};
                    if (v >= 0xd800 && v <= 0xdfff)
                        throw ParseError{173, i};
                    return makeChar(v, f);
                }
                throw ParseError{137, i};
            }
            const int id = make(NT::Any);
            node(id).a = 0;
            return id;
        }
        case U'X':
            return make(NT::Grapheme);
        case U'C':
            return make(NT::AnyUnit);
        case U'b':
        case U'B':
        case U'A':
        case U'z':
        case U'Z':
        case U'G': {
            const int id = make(NT::Assert);
            switch (c) {
            case U'b':
                node(id).a = AssertWordBoundary;
                break;
            case U'B':
                node(id).a = AssertNotWordBoundary;
                break;
            case U'A':
                node(id).a = AssertStartSubject;
                break;
            case U'z':
                node(id).a = AssertEndSubject;
                break;
            case U'Z':
                node(id).a = AssertEndSubjectNl;
                break;
            default:
                node(id).a = AssertMatchStart;
                break;
            }
            node(id).b = ucp ? 1 : 0;
            return id;
        }
        case U'K':
            return make(NT::Keep);
        case U'Q':
            inQuote = true;
            return -1;
        case U'E':
            return -1;
        case U'g':
            return parseBackrefAfterG(f);
        case U'k': {
            char16_t term = 0;
            if (at(U'<'))
                term = U'>';
            else if (at(U'\''))
                term = U'\'';
            else if (at(U'{'))
                term = U'}';
            else
                throw ParseError{169, i};
            ++i;
            const qsizetype nameStart = i;
            const std::u16string name = parseName(term);
            return makeBackrefByName(name, f, nameStart);
        }
        default:
            break;
        }
        if (c >= U'1' && c <= U'9') {
            const qsizetype digitsStart = i - 1;
            int num = 0;
            qsizetype j = digitsStart;
            while (j < n && p[j] >= U'0' && p[j] <= U'9') {
                num = addDigit(num, p[j]);
                ++j;
            }
            if (num < 10 || c >= U'8' || num <= groupCount) {
                i = j;
                return makeBackrefByNumber(num, f, i);
            }
            std::uint32_t v = 0;
            qsizetype k = digitsStart;
            for (int d = 0; d < 3 && k < n && p[k] >= U'0' && p[k] <= U'7'; ++d)
                v = v * 8 + static_cast<std::uint32_t>(p[k++] - U'0');
            if (k == digitsStart)
                throw ParseError{103, i};
            i = k;
            return makeChar(v, f);
        }
        return makeChar(parseCharEscape(c), f);
    }

    // --- character classes -----------------------------------------------------------------

    struct ClassAtom {
        bool isItem = false;
        char32_t cp = 0;
    };

    bool tryPosix(CharSet &set) {
        // At "[" inside a class; the next character is ':', '.' or '='.
        const char16_t kind = p[i + 1];
        qsizetype j = i + 2;
        if (kind == U':' && j < n && p[j] == U'^')
            ++j;
        qsizetype nameStart = j;
        if (kind == U':') {
            while (j < n && p[j] >= U'a' && p[j] <= U'z')
                ++j;
        } else {
            while (j + 1 < n && p[j] != U']' && !(p[j] == kind && p[j + 1] == U']'))
                ++j;
        }
        if (j + 1 < n && p[j] == kind && p[j + 1] == U']') {
            if (kind != U':')
                throw ParseError{113, i};
            const bool neg = p[i + 2] == U'^';
            std::string name;
            for (qsizetype k = nameStart; k < j; ++k)
                name.push_back(static_cast<char>(p[k]));
            static const std::pair<const char *, PosixClass> names[] = {
                {"alpha", PAlpha}, {"digit", PDigit}, {"alnum", PAlnum}, {"upper", PUpper}, {"lower", PLower},
                {"space", PSpace}, {"blank", PBlank}, {"punct", PPunct}, {"print", PPrint}, {"graph", PGraph},
                {"cntrl", PCntrl}, {"xdigit", PXdigit}, {"word", PWord}, {"ascii", PAscii}};
            for (const auto &nm : names) {
                if (name == nm.first) {
                    set.items.push_back({KPosix, neg, nm.second});
                    i = j + 2;
                    return true;
                }
            }
            throw ParseError{130, nameStart};
        }
        return false;
    }

    int parseClass(const Flags &f) {
        CharSet set = newSet(f);
        // "[:alpha:]" outside a class is an error in PCRE.
        if (at(U':')) {
            qsizetype j = i + 1;
            while (j < n && ((p[j] >= U'a' && p[j] <= U'z') || p[j] == U'^'))
                ++j;
            if (j + 1 < n && p[j] == U':' && p[j + 1] == U']' && j > i + 1)
                throw ParseError{112, i - 1};
        }
        if (at(U'^')) {
            set.negated = true;
            ++i;
        }
        bool first = true;
        bool quote = false;
        for (;;) {
            if (i >= n)
                throw ParseError{106, n};
            if (quote) {
                if (at(U'\\') && at(U'E', 1)) {
                    i += 2;
                    quote = false;
                    continue;
                }
            } else if (p[i] == U']' && !first) {
                ++i;
                break;
            }
            first = false;
            ClassAtom lo;
            if (quote) {
                lo.cp = readCp();
            } else {
                if (!parseClassAtom(set, lo, quote))
                    continue;
            }
            if (lo.isItem)
                continue;
            if (!quote && at(U'-') && i + 1 < n && p[i + 1] != U']') {
                const qsizetype dash = i;
                ++i;
                ClassAtom hi;
                const qsizetype hiStart = i;
                bool dummyQuote = false;
                if (at(U'\\') && at(U'Q', 1)) {
                    // "a-\Q..." : treat the dash literally.
                    i = dash;
                    set.ranges.emplace_back(lo.cp, lo.cp);
                    continue;
                }
                if (i >= n)
                    throw ParseError{106, n};
                if (!parseClassAtom(set, hi, dummyQuote))
                    throw ParseError{150, i};
                if (hi.isItem)
                    throw ParseError{150, hiStart};
                if (hi.cp < lo.cp)
                    throw ParseError{108, hiStart};
                set.ranges.emplace_back(lo.cp, hi.cp);
                continue;
            }
            set.ranges.emplace_back(lo.cp, lo.cp);
        }
        return makeSet(std::move(set));
    }

    // Parses one class element. Returns false if it produced nothing (a \Q or \E).
    bool parseClassAtom(CharSet &set, ClassAtom &out, bool &quote) {
        const char16_t c = p[i];
        if (c == U'[' && i + 1 < n && (p[i + 1] == U':' || p[i + 1] == U'.' || p[i + 1] == U'=')) {
            if (tryPosix(set)) {
                out.isItem = true;
                return true;
            }
        }
        if (c != U'\\') {
            out.cp = readCp();
            return true;
        }
        ++i;
        if (i >= n)
            throw ParseError{101, n};
        const char16_t e = p[i++];
        switch (e) {
        case U'd':
        case U'D':
            set.items.push_back({KDigit, e == U'D', 0});
            out.isItem = true;
            return true;
        case U'w':
        case U'W':
            set.items.push_back({KWord, e == U'W', 0});
            out.isItem = true;
            return true;
        case U's':
        case U'S':
            set.items.push_back({KSpace, e == U'S', 0});
            out.isItem = true;
            return true;
        case U'h':
        case U'H':
            set.items.push_back({KHSpace, e == U'H', 0});
            out.isItem = true;
            return true;
        case U'v':
        case U'V':
            set.items.push_back({KVSpace, e == U'V', 0});
            out.isItem = true;
            return true;
        case U'p':
        case U'P': {
            Flags f;
            f.i = set.caseless;
            const int sid = parseProperty(e == U'P', f);
            const CharSet &ps = prog->sets[static_cast<std::size_t>(node(sid).a)];
            set.items.push_back(ps.items[0]);
            prog->sets.pop_back();
            nodes.pop_back();
            out.isItem = true;
            return true;
        }
        case U'b':
            out.cp = 8;
            return true;
        case U'Q':
            quote = true;
            return false;
        case U'E':
            return false;
        case U'B':
        case U'R':
        case U'X':
            throw ParseError{107, i};
        case U'N':
            throw ParseError{171, i};
        case U'8':
        case U'9':
            out.cp = e;
            return true;
        default:
            break;
        }
        if (e >= U'1' && e <= U'7') {
            std::uint32_t v = static_cast<std::uint32_t>(e - U'0');
            for (int k = 0; k < 2 && i < n && p[i] >= U'0' && p[i] <= U'7'; ++k)
                v = v * 8 + static_cast<std::uint32_t>(p[i++] - U'0');
            out.cp = v;
            return true;
        }
        out.cp = parseCharEscape(e);
        return true;
    }
};

struct First {
    std::uint64_t ascii[2] = {0, 0};
    bool nonAscii = false;
    bool known = true;

    void add(char32_t c) {
        if (c < 128)
            ascii[c >> 6] |= std::uint64_t(1) << (c & 63);
        else
            nonAscii = true;
    }
};

class Emitter {
public:
    explicit Emitter(Parser &parser) : ps(parser), nodes(parser.nodes), prog(*parser.prog) {}

    void run() {
        genNode(ps.root);
        Inst m;
        m.op = Op::Match;
        push(m);
        analyse();
    }

private:
    Parser &ps;
    std::vector<Node> &nodes;
    Program &prog;

    const Node &node(int id) const { return nodes[static_cast<std::size_t>(id)]; }

    int push(const Inst &in) {
        if (prog.code.size() >= kMaxProgram)
            throw ParseError{120, 0};
        prog.code.push_back(in);
        return static_cast<int>(prog.code.size()) - 1;
    }
    int here() const { return static_cast<int>(prog.code.size()); }
    Inst &at(int idx) { return prog.code[static_cast<std::size_t>(idx)]; }

    int pushOp(Op op, int a = 0, int b = 0) {
        Inst in;
        in.op = op;
        in.a = a;
        in.b = b;
        return push(in);
    }

    bool singleMatcher(int id, Op &kind, int &arg) const {
        const Node &nd = node(id);
        switch (nd.type) {
        case NT::Char:
            if (nd.caseless) {
                kind = Op::CharI;
                arg = static_cast<int>(foldCase(nd.cp));
            } else {
                kind = Op::Char;
                arg = static_cast<int>(nd.cp);
            }
            return true;
        case NT::Any:
            kind = nd.a != 0 ? Op::AnyAll : Op::Any;
            arg = 0;
            return true;
        case NT::AnyUnit:
            kind = Op::AnyUnit;
            arg = 0;
            return true;
        case NT::Set:
            kind = Op::Set;
            arg = nd.a;
            return true;
        default:
            return false;
        }
    }

    void emitSeq(const std::vector<int> &kids) {
        std::u16string lit;
        int count = 0;
        char32_t last = 0;
        auto flush = [&]() {
            if (count == 0)
                return;
            if (count == 1) {
                pushOp(Op::Char, static_cast<int>(last));
            } else {
                prog.strings.push_back(lit);
                pushOp(Op::Str, static_cast<int>(prog.strings.size()) - 1);
            }
            lit.clear();
            count = 0;
        };
        for (int k : kids) {
            const Node &nd = node(k);
            if (nd.type == NT::Char && !nd.caseless) {
                if (nd.cp >= 0x10000) {
                    lit.push_back(static_cast<char16_t>(0xd800 + ((nd.cp - 0x10000) >> 10)));
                    lit.push_back(static_cast<char16_t>(0xdc00 + ((nd.cp - 0x10000) & 0x3ff)));
                } else {
                    lit.push_back(static_cast<char16_t>(nd.cp));
                }
                ++count;
                last = nd.cp;
            } else {
                flush();
                genNode(k);
            }
        }
        flush();
    }

    void emitAlt(const std::vector<int> &kids) {
        std::vector<int> jumps;
        for (std::size_t k = 0; k < kids.size(); ++k) {
            if (k + 1 < kids.size()) {
                const int sp = pushOp(Op::Split);
                at(sp).a = here();
                genNode(kids[k]);
                jumps.push_back(pushOp(Op::Jmp));
                at(sp).b = here();
            } else {
                genNode(kids[k]);
            }
        }
        for (int j : jumps)
            at(j).a = here();
    }

    bool nullable(int id) const { return lenRange(nodes, id).min == 0; }

    void emitLoopBody(int child, bool lazy) {
        // Emits "child; Split(loop, exit)" with a progress guard when the child can match empty.
        const int loop = here();
        int reg = -1;
        if (nullable(child)) {
            reg = prog.registerCount++;
            pushOp(Op::SetMark, reg);
        }
        genNode(child);
        int progress = -1;
        if (reg >= 0)
            progress = pushOp(Op::Progress, 0, reg);
        const int sp = pushOp(Op::Split);
        const int exit = here();
        if (lazy) {
            at(sp).a = exit;
            at(sp).b = loop;
        } else {
            at(sp).a = loop;
            at(sp).b = exit;
        }
        if (progress >= 0)
            at(progress).a = exit;
    }

    void emitRepeatBody(int child, int mn, int mx, bool lazy) {
        if (mx == 0)
            return;
        if (mx < 0) {
            if (mn == 0) {
                const int sp = pushOp(Op::Split);
                const int bodyStart = here();
                emitLoopBody(child, lazy);
                const int exit = here();
                if (lazy) {
                    at(sp).a = exit;
                    at(sp).b = bodyStart;
                } else {
                    at(sp).a = bodyStart;
                    at(sp).b = exit;
                }
            } else {
                for (int k = 0; k < mn - 1; ++k)
                    genNode(child);
                emitLoopBody(child, lazy);
            }
            return;
        }
        for (int k = 0; k < mn; ++k)
            genNode(child);
        std::vector<int> splits;
        for (int k = 0; k < mx - mn; ++k) {
            splits.push_back(pushOp(Op::Split));
            genNode(child);
        }
        const int exit = here();
        for (int sp : splits) {
            if (lazy) {
                at(sp).a = exit;
                at(sp).b = sp + 1;
            } else {
                at(sp).a = sp + 1;
                at(sp).b = exit;
            }
        }
    }

    void emitRepeat(const Node &nd) {
        const int child = nd.kids[0];
        Op kind = Op::Fail;
        int arg = 0;
        if (singleMatcher(child, kind, arg)) {
            Inst r;
            r.op = Op::Rep;
            r.kind = kind;
            r.a = arg;
            r.b = nd.a;
            r.c = nd.b;
            r.mode = static_cast<std::uint8_t>(nd.c);
            push(r);
            return;
        }
        if (nd.c == RepPossessive) {
            const int at0 = pushOp(Op::Atomic);
            emitRepeatBody(child, nd.a, nd.b, false);
            pushOp(Op::AtomicEnd);
            at(at0).c = here();
            return;
        }
        emitRepeatBody(child, nd.a, nd.b, nd.c == RepLazy);
    }

    // Emits a lookaround; \a elseTarget receives the index of the instruction whose f field must
    // be patched with the "assertion failed" target when used as a condition.
    int emitLook(const Node &nd) {
        Inst in;
        in.op = Op::Look;
        in.a = nd.a;
        in.b = nd.b;
        in.d = nd.c;
        in.e = nd.d;
        const int idx = push(in);
        genNode(nd.kids[0]);
        pushOp(Op::LookEnd);
        at(idx).c = here();
        return idx;
    }

    void emitCond(const Node &nd) {
        if (nd.a == 2) {
            const int j = pushOp(Op::Jmp);
            genNode(nd.kids[0]);
            at(j).a = here();
            return;
        }
        int test;
        std::size_t yes;
        if (nd.a == 1) {
            test = emitLook(node(nd.kids[0]));
            yes = 1;
        } else {
            test = pushOp(Op::CondGroup, nd.b);
            yes = 0;
        }
        genNode(nd.kids[yes]);
        const int j = pushOp(Op::Jmp);
        const int elsePc = here();
        if (nd.a == 1)
            at(test).f = elsePc;
        else
            at(test).b = elsePc;
        genNode(nd.kids[yes + 1]);
        at(j).a = here();
    }

    void genNode(int id) {
        const Node &nd = node(id);
        switch (nd.type) {
        case NT::Empty:
            break;
        case NT::Char:
            if (nd.caseless)
                pushOp(Op::CharI, static_cast<int>(foldCase(nd.cp)));
            else
                pushOp(Op::Char, static_cast<int>(nd.cp));
            break;
        case NT::Any:
            pushOp(nd.a != 0 ? Op::AnyAll : Op::Any);
            break;
        case NT::AnyUnit:
            pushOp(Op::AnyUnit);
            break;
        case NT::Set:
            pushOp(Op::Set, nd.a);
            break;
        case NT::Seq:
            emitSeq(nd.kids);
            break;
        case NT::Alt:
            emitAlt(nd.kids);
            break;
        case NT::Group:
            if (nd.a > 0) {
                pushOp(Op::Open, nd.a);
                genNode(nd.kids[0]);
                pushOp(Op::Close, nd.a);
            } else {
                genNode(nd.kids[0]);
            }
            break;
        case NT::Repeat:
            emitRepeat(nd);
            break;
        case NT::Assert:
            pushOp(Op::Assert, nd.a, nd.b);
            break;
        case NT::Backref:
            pushOp(Op::Backref, nd.a, nd.caseless ? 1 : 0);
            break;
        case NT::Look:
            emitLook(nd);
            break;
        case NT::Atomic: {
            const int idx = pushOp(Op::Atomic);
            genNode(nd.kids[0]);
            pushOp(Op::AtomicEnd);
            at(idx).c = here();
            break;
        }
        case NT::Cond:
            emitCond(nd);
            break;
        case NT::Keep:
            pushOp(Op::Keep);
            break;
        case NT::Grapheme:
            pushOp(Op::Grapheme);
            break;
        }
    }

    // --- start-of-match optimisations ------------------------------------------------------

    bool firstOf(int id, First &acc) const {
        const Node &nd = node(id);
        switch (nd.type) {
        case NT::Char:
            acc.add(nd.cp);
            if (nd.caseless) {
                acc.add(toLowerSimple(nd.cp));
                acc.add(toUpperSimple(nd.cp));
                acc.add(foldCase(nd.cp));
                acc.nonAscii = true;
            }
            return false;
        case NT::Any:
        case NT::AnyUnit:
            acc.ascii[0] = acc.ascii[1] = ~std::uint64_t(0);
            if (nd.type == NT::Any && nd.a == 0)
                acc.ascii[0] &= ~(std::uint64_t(1) << 10);
            acc.nonAscii = true;
            return false;
        case NT::Set: {
            const CharSet &s = prog.sets[static_cast<std::size_t>(nd.a)];
            acc.ascii[0] |= s.ascii[0];
            acc.ascii[1] |= s.ascii[1];
            acc.nonAscii = true;
            return false;
        }
        case NT::Seq:
            for (int k : nd.kids)
                if (!firstOf(k, acc))
                    return false;
            return true;
        case NT::Alt: {
            bool any = false;
            for (int k : nd.kids)
                any = firstOf(k, acc) || any;
            return any;
        }
        case NT::Group:
        case NT::Atomic:
            return firstOf(nd.kids[0], acc);
        case NT::Repeat: {
            const bool r = firstOf(nd.kids[0], acc);
            return r || nd.a == 0;
        }
        case NT::Backref:
        case NT::Cond:
        case NT::Grapheme:
            acc.known = false;
            return true;
        default:
            return true;
        }
    }

    bool anchoredStart(int id) const {
        const Node &nd = node(id);
        switch (nd.type) {
        case NT::Assert:
            return nd.a == AssertStartSubject;
        case NT::Seq:
            return !nd.kids.empty() && anchoredStart(nd.kids[0]);
        case NT::Alt:
            for (int k : nd.kids)
                if (!anchoredStart(k))
                    return false;
            return true;
        case NT::Group:
        case NT::Atomic:
            return anchoredStart(nd.kids[0]);
        case NT::Repeat:
            return nd.a >= 1 && anchoredStart(nd.kids[0]);
        default:
            return false;
        }
    }

    bool hasBackrefs() const {
        for (const Node &nd : nodes)
            if (nd.type == NT::Backref || nd.type == NT::Cond)
                return true;
        return false;
    }

    // Returns 1 or 2 if the pattern starts with a greedy or lazy .* (2 when it matches newlines too).
    int leadingDotStar(int id) const {
        const Node &nd = node(id);
        switch (nd.type) {
        case NT::Seq:
            return nd.kids.empty() ? 0 : leadingDotStar(nd.kids[0]);
        case NT::Group:
            return leadingDotStar(nd.kids[0]);
        case NT::Repeat: {
            const Node &child = node(nd.kids[0]);
            if (child.type == NT::Any && nd.a == 0 && nd.b < 0 && nd.c != RepPossessive)
                return child.a != 0 ? 2 : 1;
            return 0;
        }
        default:
            return 0;
        }
    }

    void analyse() {
        prog.anchored = anchoredStart(ps.root);
        if (!hasBackrefs())
            prog.dotStar = leadingDotStar(ps.root);
        {
            const Node &root = node(ps.root);
            std::vector<int> items;
            if (root.type == NT::Seq)
                items = root.kids;
            else
                items.push_back(ps.root);
            for (int k : items) {
                const Node *nd = &node(k);
                if (nd->type == NT::Repeat && nd->a >= 1)
                    nd = &node(nd->kids[0]);
                if (nd->type == NT::Char && !nd->caseless && nd->cp < 0xd800) {
                    prog.hasRequired = true;
                    prog.required = static_cast<char16_t>(nd->cp);
                }
            }
        }
        First acc;
        const bool canBeEmpty = firstOf(ps.root, acc);
        if (!canBeEmpty && acc.known) {
            prog.hasFirst = true;
            prog.firstAscii[0] = acc.ascii[0];
            prog.firstAscii[1] = acc.ascii[1];
            prog.firstNonAscii = acc.nonAscii;
        }
        const Node &root = node(ps.root);
        std::vector<int> lead;
        if (root.type == NT::Seq)
            lead = root.kids;
        else
            lead.push_back(ps.root);
        for (int k : lead) {
            const Node &nd = node(k);
            if (nd.type != NT::Char || nd.caseless)
                break;
            if (nd.cp >= 0x10000) {
                prog.prefix.push_back(static_cast<char16_t>(0xd800 + ((nd.cp - 0x10000) >> 10)));
                prog.prefix.push_back(static_cast<char16_t>(0xdc00 + ((nd.cp - 0x10000) & 0x3ff)));
            } else {
                prog.prefix.push_back(static_cast<char16_t>(nd.cp));
            }
        }
        prog.hasPrefix = !prog.prefix.empty();
    }
};

} // namespace

std::shared_ptr<const Program> compilePattern(const char16_t *pattern, qsizetype length, int options,
                                              CompileError &error) {
    try {
        Parser parser(pattern, length, options);
        parser.parse();
        Emitter emitter(parser);
        emitter.run();
        parser.prog->groupNames.resize(static_cast<std::size_t>(parser.prog->captureCount) + 1);
        return parser.prog;
    } catch (const ParseError &e) {
        error.code = e.code;
        error.offset = e.offset;
        return nullptr;
    }
}

} // namespace ntregex
