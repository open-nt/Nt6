// MIT License - see LICENSE
#include <QtCore/qregularexpression.h>

#include <QtCore/qhash.h>
#include <QtCore/qlogging.h>

#include "qunicode_p.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace U = QtPrivate::Unicode;

namespace {

// ------------------------------------------------------------------ syntax tree

enum class NodeType { Empty, Char, Any, Class, Seq, Alt, Group, Repeat, Backref, Assert, Look, Atomic, ResetStart };
enum class AssertKind { LineStart, LineEnd, SubjectStart, SubjectEnd, SubjectEndOrNewline, WordBoundary, NotWordBoundary, MatchStart };
enum class RepeatMode { Greedy, Lazy, Possessive };

// Category bit masks, one bit per QtPrivate::Unicode::Category.
constexpr std::uint32_t catBit(U::Category c) { return 1u << c; }
constexpr std::uint32_t kLetters = catBit(U::Letter_Uppercase) | catBit(U::Letter_Lowercase) | catBit(U::Letter_Titlecase) |
                                   catBit(U::Letter_Modifier) | catBit(U::Letter_Other);
constexpr std::uint32_t kMarks = catBit(U::Mark_NonSpacing) | catBit(U::Mark_SpacingCombining) | catBit(U::Mark_Enclosing);
constexpr std::uint32_t kNumbers = catBit(U::Number_DecimalDigit) | catBit(U::Number_Letter) | catBit(U::Number_Other);
constexpr std::uint32_t kSeparators = catBit(U::Separator_Space) | catBit(U::Separator_Line) | catBit(U::Separator_Paragraph);
constexpr std::uint32_t kOthers = catBit(U::Other_Control) | catBit(U::Other_Format) | catBit(U::Other_Surrogate) |
                                  catBit(U::Other_PrivateUse) | catBit(U::Other_NotAssigned);
constexpr std::uint32_t kPunct = catBit(U::Punctuation_Connector) | catBit(U::Punctuation_Dash) | catBit(U::Punctuation_Open) |
                                 catBit(U::Punctuation_Close) | catBit(U::Punctuation_InitialQuote) |
                                 catBit(U::Punctuation_FinalQuote) | catBit(U::Punctuation_Other);
constexpr std::uint32_t kSymbols = catBit(U::Symbol_Math) | catBit(U::Symbol_Currency) | catBit(U::Symbol_Modifier) | catBit(U::Symbol_Other);
constexpr std::uint32_t kAll = 0x3fffffffu;

enum class Posix { Alpha, Digit, Alnum, Upper, Lower, Space, Blank, Cntrl, Graph, Print, Punct, XDigit, Word, Ascii };

struct ClassItem {
    enum Kind { Range, Digit, Word, Space, HSpace, VSpace, PosixClass, Categories } kind = Range;
    char32_t lo = 0, hi = 0;
    bool negated = false;
    std::uint32_t mask = 0; // Categories
    Posix posix = Posix::Alpha;
};

struct Node {
    explicit Node(NodeType t) : type(t) {}
    NodeType type;
    char32_t ch = 0;      // Char (folded when icase)
    bool icase = false;   // Char, Class, Backref
    bool dotAll = false;  // Any
    bool ucp = false;     // Class, Assert (word boundaries)
    bool negated = false; // Class, Look
    std::vector<ClassItem> items;
    std::vector<std::unique_ptr<Node>> kids;
    int group = 0; // Group capture index, Backref target
    int min = 0, max = -1;
    RepeatMode mode = RepeatMode::Greedy;
    AssertKind kind = AssertKind::LineStart;
    bool multiline = false;
    bool behind = false;               // Look
    std::vector<int> behindLengths;    // Look behind: the fixed length of each alternative
};
using NodePtr = std::unique_ptr<Node>;

NodePtr makeNode(NodeType t) { return std::make_unique<Node>(t); }

bool isAsciiWord(char32_t c) { return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9') || c == u'_'; }
bool isAsciiSpace(char32_t c) { return c == u' ' || (c >= 9 && c <= 13); }

bool wordChar(char32_t c, bool ucp) {
    if (!ucp)
        return isAsciiWord(c);
    return c == u'_' || U::isLetter(c) || U::isNumber(c) || U::category(c) == U::Mark_NonSpacing;
}
bool digitChar(char32_t c, bool ucp) { return ucp ? U::isDigit(c) : (c >= u'0' && c <= u'9'); }
bool spaceChar(char32_t c, bool ucp) { return ucp ? U::isSpace(c) : isAsciiSpace(c); }
bool hspaceChar(char32_t c) {
    return c == u'\t' || c == u' ' || c == 0xa0 || c == 0x1680 || c == 0x180e || (c >= 0x2000 && c <= 0x200a) || c == 0x202f ||
           c == 0x205f || c == 0x3000;
}
bool vspaceChar(char32_t c) { return (c >= 0x0a && c <= 0x0d) || c == 0x85 || c == 0x2028 || c == 0x2029; }

bool posixMatch(Posix p, char32_t c, bool ucp) {
    if (ucp) {
        switch (p) {
        case Posix::Alpha:
            return U::isLetter(c);
        case Posix::Digit:
            return U::isDigit(c);
        case Posix::Alnum:
            return U::isLetter(c) || U::isNumber(c);
        case Posix::Upper:
            return U::isUpper(c);
        case Posix::Lower:
            return U::isLower(c);
        case Posix::Space:
            return U::isSpace(c);
        case Posix::Word:
            return wordChar(c, true);
        case Posix::Punct:
            return U::isPunct(c) || (c < 128 && U::isSymbol(c));
        case Posix::Graph:
            return !U::isSpace(c) && U::category(c) != U::Other_Control && U::category(c) != U::Other_NotAssigned &&
                   U::category(c) != U::Other_Surrogate;
        case Posix::Print:
            return c == u' ' || (!U::isSpace(c) && U::category(c) != U::Other_Control && U::category(c) != U::Other_NotAssigned &&
                                 U::category(c) != U::Other_Surrogate);
        default:
            break;
        }
    }
    switch (p) {
    case Posix::Alpha:
        return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
    case Posix::Digit:
        return c >= u'0' && c <= u'9';
    case Posix::Alnum:
        return posixMatch(Posix::Alpha, c, false) || posixMatch(Posix::Digit, c, false);
    case Posix::Upper:
        return c >= u'A' && c <= u'Z';
    case Posix::Lower:
        return c >= u'a' && c <= u'z';
    case Posix::Space:
        return isAsciiSpace(c);
    case Posix::Blank:
        return c == u' ' || c == u'\t';
    case Posix::Cntrl:
        return c < 32 || c == 127;
    case Posix::Graph:
        return c > 32 && c < 127;
    case Posix::Print:
        return c >= 32 && c < 127;
    case Posix::Punct:
        return c > 32 && c < 127 && !posixMatch(Posix::Alnum, c, false);
    case Posix::XDigit:
        return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
    case Posix::Word:
        return isAsciiWord(c);
    case Posix::Ascii:
        return c < 128;
    }
    return false;
}

bool itemMatch(const ClassItem &it, char32_t c, bool ucp) {
    bool r = false;
    switch (it.kind) {
    case ClassItem::Range:
        r = c >= it.lo && c <= it.hi;
        break;
    case ClassItem::Digit:
        r = digitChar(c, ucp);
        break;
    case ClassItem::Word:
        r = wordChar(c, ucp);
        break;
    case ClassItem::Space:
        r = spaceChar(c, ucp);
        break;
    case ClassItem::HSpace:
        r = hspaceChar(c);
        break;
    case ClassItem::VSpace:
        r = vspaceChar(c);
        break;
    case ClassItem::PosixClass:
        r = posixMatch(it.posix, c, ucp);
        break;
    case ClassItem::Categories:
        r = (it.mask & catBit(U::category(c))) != 0;
        break;
    }
    return r != it.negated;
}

bool classMatch(const Node &n, char32_t c) {
    bool r = false;
    for (const ClassItem &it : n.items) {
        if (itemMatch(it, c, n.ucp) || (n.icase && it.kind == ClassItem::Range &&
                                        (itemMatch(it, U::foldCase(c), n.ucp) || itemMatch(it, U::toUpper(c), n.ucp)))) {
            r = true;
            break;
        }
    }
    return r != n.negated;
}

// Returns the length in characters that \a n always matches, or -1 if it varies.
int fixedLength(const Node &n) {
    switch (n.type) {
    case NodeType::Char:
    case NodeType::Any:
    case NodeType::Class:
        return 1;
    case NodeType::Empty:
    case NodeType::Assert:
    case NodeType::Look:
    case NodeType::ResetStart:
        return 0;
    case NodeType::Seq: {
        int total = 0;
        for (const auto &k : n.kids) {
            const int l = fixedLength(*k);
            if (l < 0)
                return -1;
            total += l;
        }
        return total;
    }
    case NodeType::Alt: {
        int len = -2;
        for (const auto &k : n.kids) {
            const int l = fixedLength(*k);
            if (l < 0 || (len != -2 && l != len))
                return -1;
            len = l;
        }
        return len < 0 ? 0 : len;
    }
    case NodeType::Group:
    case NodeType::Atomic:
        return n.kids.empty() ? 0 : fixedLength(*n.kids[0]);
    case NodeType::Repeat: {
        if (n.min != n.max)
            return -1;
        const int l = fixedLength(*n.kids[0]);
        return l < 0 ? -1 : l * n.min;
    }
    case NodeType::Backref:
        return -1;
    }
    return -1;
}

// ------------------------------------------------------------------ parser

struct Flags {
    bool icase = false, multiline = false, dotAll = false, extended = false, ungreedy = false, noCapture = false, ucp = false;
};

class Parser {
public:
    Parser(const QString &pattern, QRegularExpression::PatternOptions options) : p(pattern.utf16()), n(pattern.size()) {
        f.icase = options.testFlag(QRegularExpression::CaseInsensitiveOption);
        f.multiline = options.testFlag(QRegularExpression::MultilineOption);
        f.dotAll = options.testFlag(QRegularExpression::DotMatchesEverythingOption);
        f.extended = options.testFlag(QRegularExpression::ExtendedPatternSyntaxOption);
        f.ungreedy = options.testFlag(QRegularExpression::InvertedGreedinessOption);
        f.noCapture = options.testFlag(QRegularExpression::DontCaptureOption);
        f.ucp = options.testFlag(QRegularExpression::UseUnicodePropertiesOption);
    }

    NodePtr parse() {
        NodePtr root = parseAlternation();
        if (ok() && i < n)
            fail("unmatched closing parenthesis");
        if (ok()) {
            for (const auto &ref : pendingNameRefs) {
                const auto it = names.find(ref.first);
                if (it == names.end()) {
                    i = ref.second.second;
                    fail("reference to non-existent subpattern");
                    break;
                }
                ref.second.first->group = it->second;
            }
        }
        if (ok() && maxBackref > groupCount) {
            i = maxBackrefOffset;
            fail("reference to non-existent subpattern");
        }
        return root;
    }

    bool ok() const { return error.empty(); }
    std::string error;
    qsizetype errorOffset = -1;
    int groupCount = 0;
    std::map<std::u16string, int> names;

private:
    void fail(const char *msg) {
        if (error.empty()) {
            error = msg;
            errorOffset = std::min(i, n);
        }
    }
    bool atEnd() const { return i >= n; }
    char16_t peek(qsizetype ahead = 0) const { return i + ahead < n ? p[i + ahead] : 0; }
    bool lookingAt(const char *s) const {
        qsizetype k = 0;
        for (; s[k]; ++k)
            if (i + k >= n || p[i + k] != static_cast<char16_t>(s[k]))
                return false;
        return true;
    }
    char32_t readCodePoint() {
        int len = 1;
        const char32_t c = U::codePointAt(p, n, i, &len);
        i += len;
        return c;
    }
    void skipExtended() {
        if (!f.extended)
            return;
        while (i < n) {
            if (p[i] == u' ' || p[i] == u'\t' || p[i] == u'\n' || p[i] == u'\r' || p[i] == u'\f' || p[i] == u'\v') {
                ++i;
            } else if (p[i] == u'#') {
                while (i < n && p[i] != u'\n')
                    ++i;
            } else {
                break;
            }
        }
    }

    NodePtr parseAlternation() {
        std::vector<NodePtr> alts;
        alts.push_back(parseSequence());
        while (ok() && peek() == u'|') {
            ++i;
            alts.push_back(parseSequence());
        }
        if (alts.size() == 1)
            return std::move(alts[0]);
        NodePtr alt = makeNode(NodeType::Alt);
        alt->kids = std::move(alts);
        return alt;
    }

    NodePtr parseSequence() {
        NodePtr seq = makeNode(NodeType::Seq);
        for (;;) {
            skipExtended();
            if (!ok() || atEnd() || peek() == u'|' || peek() == u')')
                break;
            bool repeatable = true;
            NodePtr atom = parseAtom(&repeatable);
            if (!ok())
                break;
            if (!atom)
                continue; // a comment, an option setting or an empty \Q\E
            if (pendingQuoted) {
                for (auto &k : pendingQuoted->kids)
                    seq->kids.push_back(std::move(k));
                pendingQuoted.reset();
            }
            skipExtended();
            if (isQuantifierStart()) {
                if (!repeatable) {
                    fail("quantifier does not follow a repeatable item");
                    break;
                }
                atom = parseQuantifier(std::move(atom));
                if (!ok())
                    break;
            }
            seq->kids.push_back(std::move(atom));
        }
        return seq;
    }

    bool isQuantifierStart() {
        const char16_t c = peek();
        if (c == u'*' || c == u'+' || c == u'?')
            return true;
        if (c != u'{')
            return false;
        int mn = 0, mx = 0;
        qsizetype save = i;
        const bool r = readBraces(&mn, &mx, false);
        i = save;
        return r;
    }

    // Reads {n}, {n,}, {n,m} or {,m}. Returns false (without failing) if the text is not a
    // quantifier, in which case the brace is a literal.
    bool readBraces(int *mn, int *mx, bool report) {
        qsizetype k = i + 1;
        auto readNum = [&](int *out) {
            const qsizetype start = k;
            long v = 0;
            while (k < n && p[k] >= u'0' && p[k] <= u'9') {
                v = v * 10 + (p[k] - u'0');
                if (v > 65535)
                    v = 65536;
                ++k;
            }
            *out = static_cast<int>(v);
            return k > start;
        };
        int a = 0, b = -1;
        const bool hasA = readNum(&a);
        if (k < n && p[k] == u'}') {
            if (!hasA)
                return false;
            b = a;
        } else if (k < n && p[k] == u',') {
            ++k;
            const bool hasB = readNum(&b);
            if (!hasA && !hasB)
                return false;
            if (!hasA)
                a = 0;
            if (!hasB)
                b = -1;
            if (k >= n || p[k] != u'}')
                return false;
        } else {
            return false;
        }
        if (report) {
            if (a > 65535 || b > 65535) {
                i = k;
                fail("number too big in {} quantifier");
                return false;
            }
            if (b >= 0 && b < a) {
                i = k;
                fail("numbers out of order in {} quantifier");
                return false;
            }
        }
        *mn = a;
        *mx = b;
        i = k + 1;
        return true;
    }

    NodePtr parseQuantifier(NodePtr atom) {
        int mn = 0, mx = -1;
        const char16_t c = peek();
        if (c == u'*') {
            ++i;
        } else if (c == u'+') {
            ++i;
            mn = 1;
        } else if (c == u'?') {
            ++i;
            mx = 1;
        } else if (!readBraces(&mn, &mx, true)) {
            return nullptr;
        }
        RepeatMode mode = f.ungreedy ? RepeatMode::Lazy : RepeatMode::Greedy;
        if (peek() == u'?') {
            ++i;
            mode = f.ungreedy ? RepeatMode::Greedy : RepeatMode::Lazy;
        } else if (peek() == u'+') {
            ++i;
            mode = RepeatMode::Possessive;
        }
        skipExtended();
        if (isQuantifierStart() && peek() != u'{') {
            fail("quantifier does not follow a repeatable item");
            return nullptr;
        }
        NodePtr r = makeNode(NodeType::Repeat);
        r->min = mn;
        r->max = mx;
        r->mode = mode;
        r->kids.push_back(std::move(atom));
        return r;
    }

    NodePtr charNode(char32_t c) {
        NodePtr node = makeNode(NodeType::Char);
        node->icase = f.icase;
        node->ch = f.icase ? U::foldCase(c) : c;
        return node;
    }

    NodePtr classNode(const ClassItem &item, bool negated = false) {
        NodePtr node = makeNode(NodeType::Class);
        node->items.push_back(item);
        node->negated = negated;
        node->icase = f.icase;
        node->ucp = f.ucp;
        return node;
    }

    NodePtr assertNode(AssertKind k) {
        NodePtr node = makeNode(NodeType::Assert);
        node->kind = k;
        node->multiline = f.multiline;
        node->ucp = f.ucp;
        return node;
    }

    NodePtr parseAtom(bool *repeatable) {
        const char16_t c = peek();
        switch (c) {
        case u'(':
            return parseGroup(repeatable);
        case u'[':
            return parseClass();
        case u'.': {
            ++i;
            NodePtr node = makeNode(NodeType::Any);
            node->dotAll = f.dotAll;
            return node;
        }
        case u'^':
            ++i;
            *repeatable = false;
            return assertNode(AssertKind::LineStart);
        case u'$':
            ++i;
            *repeatable = false;
            return assertNode(AssertKind::LineEnd);
        case u'*':
        case u'+':
        case u'?':
            fail("quantifier does not follow a repeatable item");
            return nullptr;
        case u'{': {
            if (isQuantifierStart()) {
                fail("quantifier does not follow a repeatable item");
                return nullptr;
            }
            ++i;
            return charNode(u'{');
        }
        case u'\\':
            return parseEscape(repeatable);
        default:
            return charNode(readCodePoint());
        }
    }

    bool readName(char16_t terminator, std::u16string *name) {
        const qsizetype start = i;
        while (i < n && p[i] != terminator) {
            const char16_t ch = p[i];
            const bool valid = ch == u'_' || (ch >= u'0' && ch <= u'9') || (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z') ||
                               (ch > 127 && U::isLetterOrNumber(ch));
            if (!valid) {
                fail("syntax error in subpattern name (missing terminator?)");
                return false;
            }
            ++i;
        }
        if (i >= n) {
            fail("syntax error in subpattern name (missing terminator?)");
            return false;
        }
        if (i == start) {
            fail("subpattern name expected");
            return false;
        }
        if (p[start] >= u'0' && p[start] <= u'9') {
            i = start;
            fail("subpattern name must start with a non-digit");
            return false;
        }
        name->assign(p + start, static_cast<std::size_t>(i - start));
        ++i; // terminator
        return true;
    }

    NodePtr backrefByName(const std::u16string &name, qsizetype offset) {
        NodePtr node = makeNode(NodeType::Backref);
        node->icase = f.icase;
        pendingNameRefs.push_back({name, {node.get(), offset}});
        return node;
    }

    NodePtr backrefByNumber(int num, qsizetype offset) {
        NodePtr node = makeNode(NodeType::Backref);
        node->icase = f.icase;
        node->group = num;
        if (num > maxBackref) {
            maxBackref = num;
            maxBackrefOffset = offset;
        }
        return node;
    }

    NodePtr finishGroup(NodePtr node, const Flags &saved) {
        if (!ok())
            return nullptr;
        if (peek() != u')') {
            fail("missing closing parenthesis");
            return nullptr;
        }
        ++i;
        f = saved;
        return node;
    }

    NodePtr parseGroup(bool *repeatable) {
        const qsizetype start = i;
        if (++nesting > 250) {
            fail("parentheses are too deeply nested");
            return nullptr;
        }
        struct Unnest {
            int &level;
            ~Unnest() { --level; }
        } unnest{nesting};
        ++i; // (
        const Flags saved = f;
        if (peek() == u'*') {
            fail("backtracking control verbs and start-of-pattern options are not supported");
            return nullptr;
        }
        if (peek() != u'?') {
            if (f.noCapture) {
                NodePtr g = makeNode(NodeType::Group);
                g->kids.push_back(parseAlternation());
                return finishGroup(std::move(g), saved);
            }
            NodePtr g = makeNode(NodeType::Group);
            g->group = ++groupCount;
            g->kids.push_back(parseAlternation());
            return finishGroup(std::move(g), saved);
        }
        ++i; // ?
        const char16_t c = peek();
        if (c == u'#') {
            while (i < n && p[i] != u')')
                ++i;
            if (i >= n) {
                i = start;
                fail("missing ) after (?# comment");
                return nullptr;
            }
            ++i;
            return nullptr;
        }
        if (c == u':') {
            ++i;
            NodePtr g = makeNode(NodeType::Group);
            g->kids.push_back(parseAlternation());
            return finishGroup(std::move(g), saved);
        }
        if (c == u'>') {
            ++i;
            NodePtr g = makeNode(NodeType::Atomic);
            g->kids.push_back(parseAlternation());
            return finishGroup(std::move(g), saved);
        }
        if (c == u'=' || c == u'!' || (c == u'<' && (peek(1) == u'=' || peek(1) == u'!'))) {
            NodePtr look = makeNode(NodeType::Look);
            look->behind = c == u'<';
            if (look->behind)
                ++i;
            look->negated = peek() == u'!';
            ++i;
            NodePtr body = parseAlternation();
            if (!ok())
                return nullptr;
            if (look->behind) {
                // Each top-level alternative must have a fixed length.
                std::vector<NodePtr> alts;
                if (body->type == NodeType::Alt)
                    alts = std::move(body->kids);
                else
                    alts.push_back(std::move(body));
                for (auto &a : alts) {
                    const int len = fixedLength(*a);
                    if (len < 0) {
                        i = start;
                        fail("lookbehind assertion is not fixed length");
                        return nullptr;
                    }
                    look->behindLengths.push_back(len);
                    look->kids.push_back(std::move(a));
                }
            } else {
                look->kids.push_back(std::move(body));
            }
            *repeatable = true;
            return finishGroup(std::move(look), saved);
        }
        if (c == u'<' || c == u'\'' || (c == u'P' && peek(1) == u'<')) {
            if (c == u'P')
                ++i;
            const char16_t term = peek() == u'<' ? u'>' : u'\'';
            ++i;
            const qsizetype nameStart = i;
            std::u16string name;
            if (!readName(term, &name))
                return nullptr;
            if (names.count(name)) {
                i = nameStart;
                fail("two named subpatterns have the same name (PCRE2_DUPNAMES not set)");
                return nullptr;
            }
            NodePtr g = makeNode(NodeType::Group);
            g->group = ++groupCount;
            names[name] = g->group;
            g->kids.push_back(parseAlternation());
            return finishGroup(std::move(g), saved);
        }
        if (c == u'P' && peek(1) == u'=') {
            i += 2;
            const qsizetype offset = i;
            std::u16string name;
            if (!readName(u')', &name))
                return nullptr;
            return backrefByName(name, offset);
        }
        if (c == u'|' || c == u'(' || c == u'R' || c == u'&' || c == u'C' || (c == u'P' && peek(1) == u'>') || (c >= u'0' && c <= u'9') ||
            ((c == u'+' || c == u'-') && peek(1) >= u'0' && peek(1) <= u'9')) {
            fail("recursion, subroutine calls, conditional and branch reset groups are not supported");
            return nullptr;
        }
        // Option settings: (?imsxnU-imsxnU) or (?imsx:...)
        bool on = true;
        Flags nf = f;
        if (c == u'^') {
            ++i;
            nf.icase = nf.multiline = nf.dotAll = nf.extended = nf.ungreedy = nf.noCapture = false;
        }
        while (i < n && p[i] != u')' && p[i] != u':') {
            switch (p[i]) {
            case u'-':
                on = false;
                break;
            case u'i':
                nf.icase = on;
                break;
            case u'm':
                nf.multiline = on;
                break;
            case u's':
                nf.dotAll = on;
                break;
            case u'x':
                nf.extended = on;
                break;
            case u'U':
                nf.ungreedy = on;
                break;
            case u'n':
                nf.noCapture = on;
                break;
            case u'J':
                break;
            default:
                fail("unrecognized character after (? or (?-");
                return nullptr;
            }
            ++i;
        }
        if (i >= n) {
            fail("missing closing parenthesis");
            return nullptr;
        }
        if (p[i] == u')') {
            ++i;
            f = nf; // applies to the rest of the enclosing group
            return nullptr;
        }
        ++i; // :
        f = nf;
        NodePtr g = makeNode(NodeType::Group);
        g->kids.push_back(parseAlternation());
        return finishGroup(std::move(g), saved);
    }

    bool readHex(int maxDigits, char32_t *out) {
        char32_t v = 0;
        int count = 0;
        while (count < maxDigits && i < n) {
            const char16_t ch = p[i];
            int d = -1;
            if (ch >= u'0' && ch <= u'9')
                d = ch - u'0';
            else if (ch >= u'a' && ch <= u'f')
                d = ch - u'a' + 10;
            else if (ch >= u'A' && ch <= u'F')
                d = ch - u'A' + 10;
            if (d < 0)
                break;
            v = v * 16 + static_cast<char32_t>(d);
            ++count;
            ++i;
        }
        *out = v;
        return count > 0;
    }

    // Parses a character escape that is valid both inside and outside classes. Returns true and
    // sets *out if the escape stands for a single character.
    bool parseCharEscape(char16_t c, char32_t *out, bool inClass) {
        switch (c) {
        case u'a':
            *out = 7;
            return true;
        case u'e':
            *out = 27;
            return true;
        case u'f':
            *out = 12;
            return true;
        case u'n':
            *out = 10;
            return true;
        case u'r':
            *out = 13;
            return true;
        case u't':
            *out = 9;
            return true;
        case u'b':
            if (inClass) {
                *out = 8;
                return true;
            }
            return false;
        case u'x':
            if (peek() == u'{') {
                ++i;
                if (!readHex(8, out) || peek() != u'}' || *out > 0x10ffff) {
                    fail("character code point value in \\x{} or \\o{} is too large");
                    return true;
                }
                ++i;
            } else if (!readHex(2, out)) {
                *out = 0;
            }
            return true;
        case u'o': {
            if (peek() != u'{') {
                fail("missing opening brace after \\o");
                return true;
            }
            ++i;
            char32_t v = 0;
            while (i < n && p[i] >= u'0' && p[i] <= u'7')
                v = v * 8 + (p[i++] - u'0');
            if (peek() != u'}' || v > 0x10ffff) {
                fail("character code point value in \\x{} or \\o{} is too large");
                return true;
            }
            ++i;
            *out = v;
            return true;
        }
        case u'0':
        case u'1':
        case u'2':
        case u'3':
        case u'4':
        case u'5':
        case u'6':
        case u'7': {
            if (c != u'0' && !inClass)
                return false;
            char32_t v = static_cast<char32_t>(c - u'0');
            int count = 0;
            while (count < 2 && i < n && p[i] >= u'0' && p[i] <= u'7') {
                v = v * 8 + (p[i++] - u'0');
                ++count;
            }
            *out = v;
            return true;
        }
        case u'c': {
            if (atEnd()) {
                fail("\\c at end of pattern");
                return true;
            }
            char16_t ch = p[i++];
            if (ch >= u'a' && ch <= u'z')
                ch = static_cast<char16_t>(ch - 32);
            *out = ch ^ 0x40;
            return true;
        }
        default:
            break;
        }
        if (c == u'u' || c == u'U' || c == u'L' || c == u'l' || c == u'F') {
            fail("PCRE2 does not support \\F, \\L, \\l, \\N{name}, \\U, or \\u");
            return true;
        }
        if ((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9'))
            return false;
        *out = c;
        if ((c & 0xfc00) == 0xd800 && (peek() & 0xfc00) == 0xdc00) {
            *out = 0x10000 + ((static_cast<char32_t>(c) - 0xd800) << 10) + (peek() - 0xdc00);
            ++i;
        }
        return true;
    }

    // Parses \p{...}, \pL and their negations; i is just after the p or P.
    bool parseProperty(bool negated, ClassItem *item) {
        std::u16string name;
        if (peek() == u'{') {
            ++i;
            while (i < n && p[i] != u'}')
                name.push_back(p[i++]);
            if (i >= n) {
                fail("malformed \\P or \\p sequence");
                return false;
            }
            ++i;
        } else if (!atEnd()) {
            name.push_back(p[i++]);
        } else {
            fail("malformed \\P or \\p sequence");
            return false;
        }
        if (!name.empty() && name[0] == u'^') {
            negated = !negated;
            name.erase(0, 1);
        }
        std::string key;
        for (char16_t ch : name)
            if (ch != u' ' && ch != u'_' && ch != u'-')
                key.push_back(static_cast<char>(ch < 128 ? ch : '?'));
        struct Prop {
            const char *name;
            std::uint32_t mask;
        };
        static const Prop props[] = {
            {"Any", kAll},
            {"L", kLetters},
            {"L&", catBit(U::Letter_Uppercase) | catBit(U::Letter_Lowercase) | catBit(U::Letter_Titlecase)},
            {"LC", catBit(U::Letter_Uppercase) | catBit(U::Letter_Lowercase) | catBit(U::Letter_Titlecase)},
            {"Lu", catBit(U::Letter_Uppercase)},
            {"Ll", catBit(U::Letter_Lowercase)},
            {"Lt", catBit(U::Letter_Titlecase)},
            {"Lm", catBit(U::Letter_Modifier)},
            {"Lo", catBit(U::Letter_Other)},
            {"M", kMarks},
            {"Mn", catBit(U::Mark_NonSpacing)},
            {"Mc", catBit(U::Mark_SpacingCombining)},
            {"Me", catBit(U::Mark_Enclosing)},
            {"N", kNumbers},
            {"Nd", catBit(U::Number_DecimalDigit)},
            {"Nl", catBit(U::Number_Letter)},
            {"No", catBit(U::Number_Other)},
            {"P", kPunct},
            {"Pc", catBit(U::Punctuation_Connector)},
            {"Pd", catBit(U::Punctuation_Dash)},
            {"Ps", catBit(U::Punctuation_Open)},
            {"Pe", catBit(U::Punctuation_Close)},
            {"Pi", catBit(U::Punctuation_InitialQuote)},
            {"Pf", catBit(U::Punctuation_FinalQuote)},
            {"Po", catBit(U::Punctuation_Other)},
            {"S", kSymbols},
            {"Sm", catBit(U::Symbol_Math)},
            {"Sc", catBit(U::Symbol_Currency)},
            {"Sk", catBit(U::Symbol_Modifier)},
            {"So", catBit(U::Symbol_Other)},
            {"Z", kSeparators},
            {"Zs", catBit(U::Separator_Space)},
            {"Zl", catBit(U::Separator_Line)},
            {"Zp", catBit(U::Separator_Paragraph)},
            {"C", kOthers},
            {"Cc", catBit(U::Other_Control)},
            {"Cf", catBit(U::Other_Format)},
            {"Cs", catBit(U::Other_Surrogate)},
            {"Co", catBit(U::Other_PrivateUse)},
            {"Cn", catBit(U::Other_NotAssigned)},
            {"Xan", kLetters | kNumbers},
            {"Xwd", kLetters | kNumbers | catBit(U::Punctuation_Connector)},
        };
        for (const Prop &pr : props) {
            if (key == pr.name) {
                item->kind = ClassItem::Categories;
                item->mask = pr.mask;
                item->negated = negated;
                return true;
            }
        }
        if (key == "Xsp" || key == "Xps") {
            item->kind = ClassItem::Space;
            item->negated = negated;
            return true;
        }
        fail("unknown property after \\P or \\p");
        return false;
    }

    NodePtr parseEscape(bool *repeatable) {
        const qsizetype start = i;
        ++i; // backslash
        if (atEnd()) {
            fail("\\ at end of pattern");
            return nullptr;
        }
        const char16_t c = p[i++];
        ClassItem item;
        switch (c) {
        case u'd':
        case u'D':
            item.kind = ClassItem::Digit;
            item.negated = c == u'D';
            return classNode(item);
        case u'w':
        case u'W':
            item.kind = ClassItem::Word;
            item.negated = c == u'W';
            return classNode(item);
        case u's':
        case u'S':
            item.kind = ClassItem::Space;
            item.negated = c == u'S';
            return classNode(item);
        case u'h':
        case u'H':
            item.kind = ClassItem::HSpace;
            item.negated = c == u'H';
            return classNode(item);
        case u'v':
        case u'V':
            item.kind = ClassItem::VSpace;
            item.negated = c == u'V';
            return classNode(item);
        case u'N': {
            if (peek() == u'{') {
                fail("PCRE2 does not support \\F, \\L, \\l, \\N{name}, \\U, or \\u");
                return nullptr;
            }
            NodePtr node = makeNode(NodeType::Any);
            node->dotAll = false;
            return node;
        }
        case u'R': {
            // (?>\r\n|\v)
            NodePtr atomic = makeNode(NodeType::Atomic);
            NodePtr alt = makeNode(NodeType::Alt);
            NodePtr crlf = makeNode(NodeType::Seq);
            NodePtr cr = makeNode(NodeType::Char);
            cr->ch = u'\r';
            NodePtr lf = makeNode(NodeType::Char);
            lf->ch = u'\n';
            crlf->kids.push_back(std::move(cr));
            crlf->kids.push_back(std::move(lf));
            alt->kids.push_back(std::move(crlf));
            ClassItem v;
            v.kind = ClassItem::VSpace;
            NodePtr vnode = makeNode(NodeType::Class);
            vnode->items.push_back(v);
            alt->kids.push_back(std::move(vnode));
            atomic->kids.push_back(std::move(alt));
            return atomic;
        }
        case u'X': {
            // (?>\P{M}\p{M}*)
            NodePtr atomic = makeNode(NodeType::Atomic);
            NodePtr seq = makeNode(NodeType::Seq);
            ClassItem marks;
            marks.kind = ClassItem::Categories;
            marks.mask = kMarks;
            ClassItem nonMarks = marks;
            nonMarks.negated = true;
            NodePtr base = makeNode(NodeType::Class);
            base->items.push_back(nonMarks);
            NodePtr ext = makeNode(NodeType::Class);
            ext->items.push_back(marks);
            NodePtr rep = makeNode(NodeType::Repeat);
            rep->min = 0;
            rep->max = -1;
            rep->kids.push_back(std::move(ext));
            seq->kids.push_back(std::move(base));
            seq->kids.push_back(std::move(rep));
            atomic->kids.push_back(std::move(seq));
            return atomic;
        }
        case u'p':
        case u'P':
            if (!parseProperty(c == u'P', &item))
                return nullptr;
            return classNode(item);
        case u'b':
            *repeatable = false;
            return assertNode(AssertKind::WordBoundary);
        case u'B':
            *repeatable = false;
            return assertNode(AssertKind::NotWordBoundary);
        case u'A':
            *repeatable = false;
            return assertNode(AssertKind::SubjectStart);
        case u'z':
            *repeatable = false;
            return assertNode(AssertKind::SubjectEnd);
        case u'Z':
            *repeatable = false;
            return assertNode(AssertKind::SubjectEndOrNewline);
        case u'G':
            *repeatable = false;
            return assertNode(AssertKind::MatchStart);
        case u'K':
            *repeatable = false;
            return makeNode(NodeType::ResetStart);
        case u'Q': {
            NodePtr seq = makeNode(NodeType::Seq);
            while (i < n && !(p[i] == u'\\' && peek(1) == u'E'))
                seq->kids.push_back(charNode(readCodePoint()));
            if (i < n)
                i += 2;
            if (seq->kids.empty())
                return nullptr;
            if (seq->kids.size() == 1)
                return std::move(seq->kids[0]);
            // A quantifier after \Q...\E applies to the last character only.
            NodePtr last = std::move(seq->kids.back());
            seq->kids.pop_back();
            pendingQuoted = std::move(seq);
            return last;
        }
        case u'E':
            return nullptr;
        case u'g': {
            const qsizetype offset = i;
            bool braces = peek() == u'{';
            if (braces)
                ++i;
            bool relative = false;
            if (peek() == u'-' || peek() == u'+') {
                relative = peek() == u'-';
                if (peek() == u'+') {
                    fail("recursion, subroutine calls, conditional and branch reset groups are not supported");
                    return nullptr;
                }
                ++i;
            }
            if (peek() >= u'0' && peek() <= u'9') {
                int num = 0;
                while (i < n && p[i] >= u'0' && p[i] <= u'9')
                    num = std::min(num * 10 + (p[i++] - u'0'), 100000);
                if (braces) {
                    if (peek() != u'}') {
                        fail("\\g is not followed by a braced, angle-bracketed, or quoted name/number or by a plain number");
                        return nullptr;
                    }
                    ++i;
                }
                if (relative)
                    num = groupCount - num + 1;
                if (num <= 0) {
                    i = offset;
                    fail("reference to non-existent subpattern");
                    return nullptr;
                }
                return backrefByNumber(num, offset);
            }
            if (braces) {
                std::u16string name;
                if (!readName(u'}', &name))
                    return nullptr;
                return backrefByName(name, offset);
            }
            fail("\\g is not followed by a braced, angle-bracketed, or quoted name/number or by a plain number");
            return nullptr;
        }
        case u'k': {
            const char16_t open = peek();
            const char16_t close = open == u'<' ? u'>' : (open == u'{' ? u'}' : (open == u'\'' ? u'\'' : 0));
            if (!close) {
                fail("\\k is not followed by a braced, angle-bracketed, or quoted name");
                return nullptr;
            }
            ++i;
            const qsizetype offset = i;
            std::u16string name;
            if (!readName(close, &name))
                return nullptr;
            return backrefByName(name, offset);
        }
        default:
            break;
        }
        if (c >= u'1' && c <= u'9') {
            // As in PCRE2: a number below 10, one starting with 8 or 9, or one not larger than the
            // number of groups so far is a back reference; otherwise up to three octal digits.
            const qsizetype digitsStart = i - 1;
            int num = c - u'0';
            while (i < n && p[i] >= u'0' && p[i] <= u'9')
                num = std::min(num * 10 + (p[i++] - u'0'), 100000);
            if (num < 10 || c == u'8' || c == u'9' || num <= groupCount)
                return backrefByNumber(num, start);
            i = digitsStart;
            char32_t v = 0;
            for (int count = 0; count < 3 && i < n && p[i] >= u'0' && p[i] <= u'7'; ++count)
                v = v * 8 + (p[i++] - u'0');
            return charNode(v);
        }
        char32_t ch = 0;
        if (parseCharEscape(c, &ch, false)) {
            if (!ok())
                return nullptr;
            return charNode(ch);
        }
        i = start + 1;
        fail("unrecognized character follows \\");
        return nullptr;
    }

    NodePtr parseClass() {
        const qsizetype start = i;
        ++i; // [
        NodePtr node = makeNode(NodeType::Class);
        node->icase = f.icase;
        node->ucp = f.ucp;
        if (peek() == u'^') {
            node->negated = true;
            ++i;
        }
        bool first = true;
        bool quoting = false;
        for (;;) {
            if (atEnd()) {
                i = start;
                fail("missing terminating ] for character class");
                return nullptr;
            }
            if (quoting) {
                if (p[i] == u'\\' && peek(1) == u'E') {
                    quoting = false;
                    i += 2;
                    continue;
                }
                const char32_t c = readCodePoint();
                ClassItem r;
                r.lo = r.hi = c;
                node->items.push_back(r);
                first = false;
                continue;
            }
            if (p[i] == u']' && !first) {
                ++i;
                break;
            }
            first = false;
            // POSIX class
            if (p[i] == u'[' && (peek(1) == u':' || peek(1) == u'.' || peek(1) == u'=')) {
                const qsizetype close = [&] {
                    for (qsizetype k = i + 2; k + 1 < n; ++k)
                        if (p[k] == p[i + 1] && p[k + 1] == u']')
                            return k;
                    return qsizetype(-1);
                }();
                if (close > 0) {
                    if (peek(1) != u':') {
                        fail("POSIX collating elements are not supported");
                        return nullptr;
                    }
                    qsizetype k = i + 2;
                    bool neg = false;
                    if (p[k] == u'^') {
                        neg = true;
                        ++k;
                    }
                    std::string name;
                    for (; k < close; ++k)
                        name.push_back(static_cast<char>(p[k] < 128 ? p[k] : '?'));
                    static const std::pair<const char *, Posix> classes[] = {
                        {"alpha", Posix::Alpha}, {"digit", Posix::Digit}, {"alnum", Posix::Alnum},   {"upper", Posix::Upper},
                        {"lower", Posix::Lower}, {"space", Posix::Space}, {"blank", Posix::Blank},   {"cntrl", Posix::Cntrl},
                        {"graph", Posix::Graph}, {"print", Posix::Print}, {"punct", Posix::Punct},   {"xdigit", Posix::XDigit},
                        {"word", Posix::Word},   {"ascii", Posix::Ascii}};
                    bool found = false;
                    for (const auto &pc : classes) {
                        if (name == pc.first) {
                            ClassItem it;
                            it.kind = ClassItem::PosixClass;
                            it.posix = pc.second;
                            it.negated = neg;
                            // With case-insensitive matching, [:upper:] and [:lower:] match letters of either case.
                            if (f.icase && (pc.second == Posix::Upper || pc.second == Posix::Lower))
                                it.posix = Posix::Alpha;
                            node->items.push_back(it);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        fail("unknown POSIX class name");
                        return nullptr;
                    }
                    i = close + 2;
                    continue;
                }
            }
            // A single character, an escape, or the start of a range.
            char32_t lo = 0;
            if (p[i] == u'\\') {
                ++i;
                if (atEnd()) {
                    fail("\\ at end of pattern");
                    return nullptr;
                }
                const char16_t c = p[i++];
                ClassItem it;
                bool special = true;
                switch (c) {
                case u'd':
                case u'D':
                    it.kind = ClassItem::Digit;
                    it.negated = c == u'D';
                    break;
                case u'w':
                case u'W':
                    it.kind = ClassItem::Word;
                    it.negated = c == u'W';
                    break;
                case u's':
                case u'S':
                    it.kind = ClassItem::Space;
                    it.negated = c == u'S';
                    break;
                case u'h':
                case u'H':
                    it.kind = ClassItem::HSpace;
                    it.negated = c == u'H';
                    break;
                case u'v':
                case u'V':
                    it.kind = ClassItem::VSpace;
                    it.negated = c == u'V';
                    break;
                case u'p':
                case u'P':
                    if (!parseProperty(c == u'P', &it))
                        return nullptr;
                    break;
                case u'Q':
                    quoting = true;
                    continue;
                case u'E':
                    continue;
                default:
                    special = false;
                    break;
                }
                if (special) {
                    node->items.push_back(it);
                    continue;
                }
                if (!parseCharEscape(c, &lo, true)) {
                    i -= 1;
                    fail("unrecognized character follows \\");
                    return nullptr;
                }
                if (!ok())
                    return nullptr;
            } else {
                lo = readCodePoint();
            }
            char32_t hi = lo;
            if (peek() == u'-' && peek(1) != u']' && i + 1 < n) {
                const qsizetype save = i;
                ++i;
                if (p[i] == u'[' && peek(1) == u':') {
                    i = save; // "a-[:digit:]": the hyphen is a literal
                } else {
                    if (p[i] == u'\\') {
                        ++i;
                        if (atEnd()) {
                            fail("\\ at end of pattern");
                            return nullptr;
                        }
                        const char16_t c = p[i++];
                        if (!parseCharEscape(c, &hi, true)) {
                            fail("invalid range in character class");
                            return nullptr;
                        }
                        if (!ok())
                            return nullptr;
                    } else {
                        hi = readCodePoint();
                    }
                    if (hi < lo) {
                        fail("range out of order in character class");
                        return nullptr;
                    }
                }
            }
            ClassItem r;
            r.lo = lo;
            r.hi = hi;
            node->items.push_back(r);
        }
        return node;
    }

public:
    NodePtr pendingQuoted; // characters of a \Q...\E sequence before its last one

private:
    const char16_t *p;
    qsizetype n;
    qsizetype i = 0;
    Flags f;
    int maxBackref = 0;
    int nesting = 0;
    qsizetype maxBackrefOffset = 0;
    std::vector<std::pair<std::u16string, std::pair<Node *, qsizetype>>> pendingNameRefs;
};

// ------------------------------------------------------------------ matcher

// A non-owning reference to a continuation, called with the position where the matched part
// ends. It is cheaper than std::function because it never allocates.
class Cont {
public:
    template <typename F>
    explicit Cont(F &f) : obj(&f), fn([](void *o, qsizetype pos) { return (*static_cast<F *>(o))(pos); }) {}
    bool operator()(qsizetype pos) const { return fn(obj, pos); }

private:
    void *obj;
    bool (*fn)(void *, qsizetype);
};

// The matcher gives up (as if there were no match) when its recursion uses more stack than this.
constexpr std::ptrdiff_t kStackBudget = 2 * 1024 * 1024;

// Returns the address of the current stack frame. The frame address is used rather than the address
// of a local, which AddressSanitizer may move to a heap-allocated fake stack.
inline std::uintptr_t currentFrame() {
#if defined(__GNUC__) || defined(__clang__)
    return reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
#else
    char marker = 0;
    return reinterpret_cast<std::uintptr_t>(&marker);
#endif
}

struct Matcher {
    const char16_t *s = nullptr;
    qsizetype n = 0;
    qsizetype searchOffset = 0;
    std::vector<qsizetype> caps;
    qsizetype matchStart = 0;
    qsizetype attemptStart = 0;
    bool notEmptyAtStart = false;
    bool hardPartial = false;
    bool hitEnd = false;
    bool aborted = false;
    bool outOfStack = false;
    std::uintptr_t stackBase = 0;

    char32_t at(qsizetype pos, int *len) const { return U::codePointAt(s, n, pos, len); }
    void noteEnd() {
        if (attemptStart < n || n == 0) {
            hitEnd = true;
            if (hardPartial)
                aborted = true;
        }
    }
    bool deep() {
        const std::uintptr_t here = currentFrame();
        if (stackBase > here && static_cast<std::ptrdiff_t>(stackBase - here) > kStackBudget) {
            aborted = true;
            outOfStack = true;
            return true;
        }
        return false;
    }

    bool isWordAt(qsizetype pos, bool ucp) const {
        if (pos < 0 || pos >= n)
            return false;
        int len = 1;
        return wordChar(at(pos, &len), ucp);
    }
    bool isWordBefore(qsizetype pos, bool ucp) const {
        if (pos <= 0)
            return false;
        int len = 1;
        return wordChar(U::codePointBefore(s, pos, &len), ucp);
    }

    // Matches one character with a Char, Any or Class node. Sets *len to the code units used.
    bool single(const Node &node, qsizetype pos, int *len) {
        if (pos >= n) {
            noteEnd();
            return false;
        }
        const char32_t c = at(pos, len);
        switch (node.type) {
        case NodeType::Char:
            return node.icase ? U::foldCase(c) == node.ch : c == node.ch;
        case NodeType::Any:
            return node.dotAll || c != u'\n';
        case NodeType::Class:
            return classMatch(node, c);
        default:
            return false;
        }
    }
    static bool isSingle(const Node &node) { return node.type == NodeType::Char || node.type == NodeType::Any || node.type == NodeType::Class; }

    bool assertion(const Node &node, qsizetype pos) const {
        switch (node.kind) {
        case AssertKind::LineStart:
            return pos == 0 || (node.multiline && s[pos - 1] == u'\n' && pos < n);
        case AssertKind::LineEnd:
            if (node.multiline)
                return pos == n || s[pos] == u'\n';
            return pos == n || (pos == n - 1 && s[pos] == u'\n');
        case AssertKind::SubjectStart:
            return pos == 0;
        case AssertKind::SubjectEnd:
            return pos == n;
        case AssertKind::SubjectEndOrNewline:
            return pos == n || (pos == n - 1 && s[pos] == u'\n');
        case AssertKind::WordBoundary:
            return isWordBefore(pos, node.ucp) != isWordAt(pos, node.ucp);
        case AssertKind::NotWordBoundary:
            return isWordBefore(pos, node.ucp) == isWordAt(pos, node.ucp);
        case AssertKind::MatchStart:
            return pos == searchOffset;
        }
        return false;
    }

    bool match(const Node &node, qsizetype pos, const Cont &k) {
        if (aborted || deep())
            return false;
        switch (node.type) {
        case NodeType::Empty:
            return k(pos);
        case NodeType::Char:
        case NodeType::Any:
        case NodeType::Class: {
            int len = 1;
            return single(node, pos, &len) && k(pos + len);
        }
        case NodeType::Seq:
            return matchSeq(node, 0, pos, k);
        case NodeType::Alt:
            for (const auto &kid : node.kids) {
                if (match(*kid, pos, k))
                    return true;
                if (aborted)
                    return false;
            }
            return false;
        case NodeType::Group: {
            if (node.group == 0)
                return match(*node.kids[0], pos, k);
            const std::size_t g = static_cast<std::size_t>(node.group) * 2;
            auto close = [&](qsizetype end) {
                const qsizetype os = caps[g], oe = caps[g + 1];
                caps[g] = pos;
                caps[g + 1] = end;
                if (k(end))
                    return true;
                caps[g] = os;
                caps[g + 1] = oe;
                return false;
            };
            return match(*node.kids[0], pos, Cont(close));
        }
        case NodeType::Repeat:
            return matchRepeat(node, pos, k);
        case NodeType::Backref: {
            const std::size_t g = static_cast<std::size_t>(node.group) * 2;
            if (g + 1 >= caps.size() || caps[g] < 0)
                return false;
            const qsizetype len = caps[g + 1] - caps[g];
            for (qsizetype j = 0; j < len; ++j) {
                if (pos + j >= n) {
                    noteEnd();
                    return false;
                }
                const char16_t a = s[caps[g] + j], b = s[pos + j];
                if (a != b && !(node.icase && U::foldCase(a) == U::foldCase(b)))
                    return false;
            }
            return k(pos + len);
        }
        case NodeType::Assert:
            return assertion(node, pos) && k(pos);
        case NodeType::Look:
            return matchLook(node, pos, k);
        case NodeType::Atomic: {
            const std::vector<qsizetype> saved = caps;
            qsizetype end = -1;
            auto record = [&](qsizetype e) {
                end = e;
                return true;
            };
            if (!match(*node.kids[0], pos, Cont(record)))
                return false;
            if (k(end))
                return true;
            caps = saved;
            return false;
        }
        case NodeType::ResetStart: {
            const qsizetype old = matchStart;
            matchStart = pos;
            if (k(pos))
                return true;
            matchStart = old;
            return false;
        }
        }
        return false;
    }

    bool matchSeq(const Node &seq, std::size_t idx, qsizetype pos, const Cont &k) {
        // Single characters are matched in a loop, without recursion.
        while (idx < seq.kids.size() && isSingle(*seq.kids[idx])) {
            int len = 1;
            if (!single(*seq.kids[idx], pos, &len))
                return false;
            pos += len;
            ++idx;
        }
        if (idx == seq.kids.size())
            return k(pos);
        if (idx + 1 == seq.kids.size())
            return match(*seq.kids[idx], pos, k);
        auto next = [&](qsizetype p) { return matchSeq(seq, idx + 1, p, k); };
        return match(*seq.kids[idx], pos, Cont(next));
    }

    bool matchRepeat(const Node &node, qsizetype pos, const Cont &k) {
        const Node &body = *node.kids[0];
        const int mx = node.max;
        if (isSingle(body)) {
            qsizetype p = pos;
            int count = 0;
            if (node.mode == RepeatMode::Lazy) {
                for (;;) {
                    if (count >= node.min) {
                        if (k(p))
                            return true;
                        if (aborted)
                            return false;
                    }
                    if (mx >= 0 && count >= mx)
                        return false;
                    int len = 1;
                    if (!single(body, p, &len))
                        return false;
                    p += len;
                    ++count;
                }
            }
            while (mx < 0 || count < mx) {
                int len = 1;
                if (!single(body, p, &len))
                    break;
                p += len;
                ++count;
            }
            if (aborted || count < node.min)
                return false;
            if (node.mode == RepeatMode::Possessive)
                return k(p);
            for (;;) {
                if (k(p))
                    return true;
                if (aborted || count == node.min)
                    return false;
                int len = 1;
                U::codePointBefore(s, p, &len);
                p -= len;
                --count;
            }
        }
        if (node.mode == RepeatMode::Possessive) {
            const std::vector<qsizetype> saved = caps;
            qsizetype end = -1;
            auto record = [&](qsizetype e) {
                end = e;
                return true;
            };
            if (!repeatGeneral(node, 0, pos, Cont(record), RepeatMode::Greedy))
                return false;
            if (k(end))
                return true;
            caps = saved;
            return false;
        }
        return repeatGeneral(node, 0, pos, k, node.mode);
    }

    bool repeatGeneral(const Node &node, int count, qsizetype pos, const Cont &k, RepeatMode mode) {
        const Node &body = *node.kids[0];
        const bool canRepeat = node.max < 0 || count < node.max;
        auto again = [&](qsizetype p) {
            // An iteration that matched nothing ends the loop once the minimum is reached.
            if (p == pos && count >= node.min)
                return false;
            return repeatGeneral(node, count + 1, p, k, mode);
        };
        if (mode == RepeatMode::Lazy) {
            if (count >= node.min) {
                if (k(pos))
                    return true;
                if (aborted)
                    return false;
            }
            return canRepeat && match(body, pos, Cont(again));
        }
        if (canRepeat) {
            if (match(body, pos, Cont(again)))
                return true;
            if (aborted)
                return false;
        }
        return count >= node.min && k(pos);
    }

    bool matchLook(const Node &node, qsizetype pos, const Cont &k) {
        const std::vector<qsizetype> saved = caps;
        bool found = false;
        if (!node.behind) {
            auto accept = [](qsizetype) { return true; };
            found = match(*node.kids[0], pos, Cont(accept));
        } else {
            for (std::size_t a = 0; a < node.kids.size() && !found && !aborted; ++a) {
                qsizetype start = pos;
                bool fits = true;
                for (int c = 0; c < node.behindLengths[a]; ++c) {
                    if (start <= 0) {
                        fits = false;
                        break;
                    }
                    int len = 1;
                    U::codePointBefore(s, start, &len);
                    start -= len;
                }
                if (!fits)
                    continue;
                auto endsHere = [pos](qsizetype e) { return e == pos; };
                found = match(*node.kids[a], start, Cont(endsHere));
            }
        }
        if (aborted)
            return false;
        if (node.negated) {
            caps = saved;
            return !found && k(pos);
        }
        if (!found)
            return false;
        if (k(pos))
            return true;
        caps = saved;
        return false;
    }
};

} // namespace

// ------------------------------------------------------------------ private data

struct QRegularExpressionPrivate {
    QString pattern;
    QRegularExpression::PatternOptions options;
    bool compiled = false;
    NodePtr root;
    QString error;
    qsizetype errorOffset = -1;
    int captureCount = 0;
    QStringList names;

    void compile() {
        if (compiled)
            return;
        compiled = true;
        Parser parser(pattern, options);
        root = parser.parse();
        if (!parser.ok()) {
            root.reset();
            error = QString::fromUtf8(parser.error.c_str());
            errorOffset = parser.errorOffset;
            captureCount = 0;
            names.clear();
            return;
        }
        captureCount = parser.groupCount;
        names = QStringList();
        for (int g = 0; g <= captureCount; ++g)
            names.append(QString());
        for (const auto &entry : parser.names)
            names[entry.second] = QString(entry.first);
    }
    QRegularExpressionPrivate() = default;
    QRegularExpressionPrivate(const QRegularExpressionPrivate &o) : pattern(o.pattern), options(o.options) {}
};

struct QRegularExpressionMatchPrivate {
    QRegularExpression regex;
    QString subject;
    QRegularExpression::MatchType type = QRegularExpression::NoMatch;
    QRegularExpression::MatchOptions options;
    std::vector<qsizetype> caps;
    int lastCaptured = -1;
    bool hasMatch = false;
    bool hasPartial = false;
    bool valid = false;
};

namespace {

enum class SearchFlag { None, NotEmptyAtStartAnchored };

std::shared_ptr<QRegularExpressionMatchPrivate> doMatch(const QRegularExpression &re, const std::shared_ptr<QRegularExpressionPrivate> &d,
                                                        const QString &subject, qsizetype offset, QRegularExpression::MatchType type,
                                                        QRegularExpression::MatchOptions options, SearchFlag flag) {
    auto m = std::make_shared<QRegularExpressionMatchPrivate>();
    m->regex = re;
    m->subject = subject;
    m->type = type;
    m->options = options;
    d->compile();
    if (!d->root) {
        qWarning("QRegularExpressionPrivate::doMatch(): called on an invalid QRegularExpression object");
        return m;
    }
    m->valid = true;
    m->caps.assign(static_cast<std::size_t>(d->captureCount + 1) * 2, -1);
    const qsizetype n = subject.size();
    if (offset < 0)
        offset += n;
    if (type == QRegularExpression::NoMatch || offset < 0 || offset > n)
        return m;

    Matcher mt;
    mt.s = subject.utf16();
    mt.n = n;
    mt.searchOffset = offset;
    mt.hardPartial = type == QRegularExpression::PartialPreferFirstMatch;
    mt.stackBase = currentFrame();
    const bool anchored = options.testFlag(QRegularExpression::AnchorAtOffsetMatchOption) || flag == SearchFlag::NotEmptyAtStartAnchored;
    mt.notEmptyAtStart = flag == SearchFlag::NotEmptyAtStartAnchored;

    qsizetype firstPartial = -1;
    qsizetype end = -1;
    for (qsizetype start = offset; start <= n;) {
        mt.caps.assign(m->caps.size(), -1);
        mt.matchStart = start;
        mt.attemptStart = start;
        mt.hitEnd = false;
        mt.aborted = false;
        auto accept = [&](qsizetype e) {
            if (mt.notEmptyAtStart && e == start && mt.matchStart == start)
                return false;
            end = e;
            return true;
        };
        if (mt.match(*d->root, start, Cont(accept))) {
            mt.caps[0] = mt.matchStart;
            mt.caps[1] = end;
            m->caps = mt.caps;
            m->hasMatch = true;
            break;
        }
        if (mt.outOfStack)
            return m;
        if (mt.hitEnd && firstPartial < 0 && type != QRegularExpression::NormalMatch) {
            firstPartial = start;
            if (type == QRegularExpression::PartialPreferFirstMatch)
                break;
        }
        if (anchored)
            break;
        int len = 1;
        if (start < n)
            U::codePointAt(mt.s, n, start, &len);
        start += len;
    }
    if (!m->hasMatch && firstPartial >= 0) {
        m->hasPartial = true;
        m->caps.assign(m->caps.size(), -1);
        m->caps[0] = firstPartial;
        m->caps[1] = n;
    }
    if (m->hasMatch || m->hasPartial) {
        m->lastCaptured = 0;
        for (std::size_t g = m->caps.size() / 2; g-- > 1;) {
            if (m->caps[2 * g] >= 0) {
                m->lastCaptured = static_cast<int>(g);
                break;
            }
        }
    }
    return m;
}

} // namespace

struct QRegularExpressionMatchIteratorPrivate {
    QRegularExpressionMatch next;
    QRegularExpression::MatchType type = QRegularExpression::NoMatch;
    QRegularExpression::MatchOptions options;
    QRegularExpression regex;

    void advance() {
        const auto prev = next.d;
        if (!prev || !prev->hasMatch) {
            next = QRegularExpressionMatch();
            next.d->regex = regex;
            next.d->type = type;
            next.d->options = options;
            next.d->valid = prev ? prev->valid : false;
            return;
        }
        const QString subject = prev->subject;
        const qsizetype end = prev->caps[1];
        std::shared_ptr<QRegularExpressionMatchPrivate> m;
        if (prev->caps[0] == end) {
            m = doMatch(regex, regex.d, subject, end, type, options, SearchFlag::NotEmptyAtStartAnchored);
            if (!m->hasMatch && !m->hasPartial && end < subject.size()) {
                int len = 1;
                U::codePointAt(subject.utf16(), subject.size(), end, &len);
                m = doMatch(regex, regex.d, subject, end + len, type, options, SearchFlag::None);
            }
        } else {
            m = doMatch(regex, regex.d, subject, end, type, options, SearchFlag::None);
        }
        next = QRegularExpressionMatch(m);
    }
};

// ------------------------------------------------------------------ QRegularExpression

/*!
    Constructs a regular expression with an empty pattern, which is valid and
    matches the empty string at any position.

    \sa setPattern()
*/
QRegularExpression::QRegularExpression() : d(std::make_shared<QRegularExpressionPrivate>()) {}

/*!
    Constructs a regular expression for \a pattern with the pattern options
    \a options. The pattern is compiled when it is first used; check
    isValid() to find out whether it is well formed.
*/
QRegularExpression::QRegularExpression(const QString &pattern, PatternOptions options) : QRegularExpression() {
    d->pattern = pattern;
    d->options = options;
}

/*!
    Constructs a copy of \a re.
*/
QRegularExpression::QRegularExpression(const QRegularExpression &re) = default;

/*!
    Move-constructs a regular expression from \a re, which becomes a valid
    empty expression.
*/
QRegularExpression::QRegularExpression(QRegularExpression &&re) noexcept : d(std::move(re.d)) {
    re.d = std::make_shared<QRegularExpressionPrivate>();
}

/*!
    Destroys the regular expression.
*/
QRegularExpression::~QRegularExpression() = default;

/*!
    Assigns \a re to this object and returns a reference to it.
*/
QRegularExpression &QRegularExpression::operator=(const QRegularExpression &re) = default;

/*!
    Move-assigns \a re to this object and returns a reference to it.
*/
QRegularExpression &QRegularExpression::operator=(QRegularExpression &&re) noexcept {
    d.swap(re.d);
    return *this;
}

/*!
    Returns the pattern string of the regular expression.

    \sa setPattern(), patternOptions()
*/
QString QRegularExpression::pattern() const { return d->pattern; }

/*!
    Sets the pattern of the regular expression to \a pattern. The pattern
    options are kept.

    \sa pattern(), setPatternOptions()
*/
void QRegularExpression::setPattern(const QString &pattern) {
    if (d->pattern == pattern && d.use_count() == 1)
        return;
    auto fresh = std::make_shared<QRegularExpressionPrivate>(*d);
    fresh->pattern = pattern;
    d = std::move(fresh);
}

/*!
    Returns the pattern options of the regular expression.

    \sa setPatternOptions(), pattern()
*/
QRegularExpression::PatternOptions QRegularExpression::patternOptions() const { return d->options; }

/*!
    Sets the pattern options to \a options.

    \sa patternOptions(), setPattern()
*/
void QRegularExpression::setPatternOptions(PatternOptions options) {
    auto fresh = std::make_shared<QRegularExpressionPrivate>(*d);
    fresh->options = options;
    d = std::move(fresh);
}

/*!
    Returns \c true if the pattern is a well-formed regular expression;
    otherwise returns \c false.

    \sa errorString(), patternErrorOffset()
*/
bool QRegularExpression::isValid() const {
    d->compile();
    return d->root != nullptr;
}

/*!
    Returns the offset in the pattern string at which an error was found, or
    -1 if the pattern is valid.

    \sa errorString(), isValid()
*/
qsizetype QRegularExpression::patternErrorOffset() const {
    d->compile();
    return d->root ? -1 : d->errorOffset;
}

/*!
    Returns a description of the error found in the pattern, or
    \c{"no error"} if the pattern is valid.

    \sa isValid(), patternErrorOffset()
*/
QString QRegularExpression::errorString() const {
    d->compile();
    return d->root ? QString::fromUtf8("no error") : d->error;
}

/*!
    Returns the number of capturing groups in the pattern, or -1 if the
    pattern is invalid. Group 0, the whole match, is not counted.

    \sa namedCaptureGroups()
*/
int QRegularExpression::captureCount() const {
    d->compile();
    return d->root ? d->captureCount : -1;
}

/*!
    Returns the names of the capturing groups, indexed by group number. The
    list has captureCount() + 1 entries; the first is empty (the whole match)
    and so are the entries of unnamed groups. An invalid pattern gives an
    empty list.

    \sa captureCount(), QRegularExpressionMatch::captured()
*/
QStringList QRegularExpression::namedCaptureGroups() const {
    d->compile();
    return d->root ? d->names : QStringList();
}

/*!
    Matches the expression against \a subject, starting the search at
    \a offset, and returns the result. A negative \a offset counts from the
    end of the subject. \a matchType selects normal or partial matching and
    \a matchOptions how the match is anchored.

    \sa globalMatch(), QRegularExpressionMatch
*/
QRegularExpressionMatch QRegularExpression::match(const QString &subject, qsizetype offset, MatchType matchType, MatchOptions matchOptions) const {
    return QRegularExpressionMatch(doMatch(*this, d, subject, offset, matchType, matchOptions, SearchFlag::None));
}

/*!
    \since 6.5
    Matches the expression against the text viewed by \a subjectView,
    starting at \a offset, with the match type \a matchType and the options
    \a matchOptions.

    \sa match()
*/
QRegularExpressionMatch QRegularExpression::matchView(QStringView subjectView, qsizetype offset, MatchType matchType,
                                                      MatchOptions matchOptions) const {
    return match(subjectView.toString(), offset, matchType, matchOptions);
}

/*!
    Returns an iterator over the successive matches of the expression in
    \a subject, starting at \a offset, with the match type \a matchType and
    the options \a matchOptions.

    \sa match(), QRegularExpressionMatchIterator
*/
QRegularExpressionMatchIterator QRegularExpression::globalMatch(const QString &subject, qsizetype offset, MatchType matchType,
                                                                MatchOptions matchOptions) const {
    QRegularExpressionMatchIterator it;
    it.d->regex = *this;
    it.d->type = matchType;
    it.d->options = matchOptions;
    it.d->next = match(subject, offset, matchType, matchOptions);
    return it;
}

/*!
    \since 6.5
    Returns an iterator over the matches in the text viewed by
    \a subjectView, starting at \a offset, with the match type \a matchType
    and the options \a matchOptions.

    \sa globalMatch()
*/
QRegularExpressionMatchIterator QRegularExpression::globalMatchView(QStringView subjectView, qsizetype offset, MatchType matchType,
                                                                    MatchOptions matchOptions) const {
    return globalMatch(subjectView.toString(), offset, matchType, matchOptions);
}

/*!
    Compiles the pattern now instead of at the first match. Nt has no JIT,
    so there is nothing else to optimize.
*/
void QRegularExpression::optimize() const { d->compile(); }

/*!
    Returns \a str with every character that has a special meaning in a
    pattern escaped by a backslash, so that the result matches \a str
    literally. All characters other than ASCII letters, digits and \c{_} are
    escaped; a null character becomes \c{\0}.

    \sa wildcardToRegularExpression()
*/
QString QRegularExpression::escape(QStringView str) {
    QString r;
    r.reserve(str.size() * 2);
    for (qsizetype k = 0; k < str.size(); ++k) {
        const QChar c = str[k];
        if (c.unicode() == 0) {
            r.append(QString::fromLatin1("\\0"));
        } else if (!isAsciiWord(c.unicode())) {
            r.append(QChar(u'\\'));
            r.append(c);
            if (c.isHighSurrogate() && k + 1 < str.size() && str[k + 1].isLowSurrogate())
                r.append(str[++k]);
        } else {
            r.append(c);
        }
    }
    return r;
}

/*!
    \since 5.12
    Returns a regular expression pattern equivalent to the shell wildcard
    \a pattern: \c{*} matches any sequence of characters, \c{?} any single
    character and \c{[...]} a set of characters (\c{[!...]} negated). Unless
    \a options contains NonPathWildcardConversion, the wildcards do not match
    the path separator \c{/}; unless it contains UnanchoredWildcardConversion,
    the result is anchored to match the whole string.

    \sa fromWildcard(), escape()
*/
QString QRegularExpression::wildcardToRegularExpression(QStringView pattern, WildcardConversionOptions options) {
    const bool path = !options.testFlag(NonPathWildcardConversion);
    QString r;
    const qsizetype n = pattern.size();
    for (qsizetype k = 0; k < n; ++k) {
        const QChar c = pattern[k];
        if (c == u'*') {
            r.append(QString::fromLatin1(path ? "[^/]*" : ".*"));
        } else if (c == u'?') {
            r.append(QString::fromLatin1(path ? "[^/]" : "."));
        } else if (c == u'[') {
            // Find the end of the set; a ']' right after '[' or '[!' belongs to the set.
            qsizetype j = k + 1;
            if (j < n && (pattern[j] == u'!' || pattern[j] == u'^'))
                ++j;
            if (j < n && pattern[j] == u']')
                ++j;
            while (j < n && pattern[j] != u']')
                ++j;
            if (j >= n) {
                r.append(QString::fromLatin1("\\["));
                continue;
            }
            r.append(QChar(u'['));
            qsizetype m = k + 1;
            if (pattern[m] == u'!' || pattern[m] == u'^') {
                r.append(QChar(u'^'));
                ++m;
            }
            for (; m < j; ++m) {
                const QChar e = pattern[m];
                if (e == u'\\' || e == u'[' || e == u']' || (e == u'^' && m > k + 1))
                    r.append(QChar(u'\\'));
                r.append(e);
            }
            r.append(QChar(u']'));
            k = j;
        } else {
            const qsizetype len = c.isHighSurrogate() && k + 1 < n && pattern[k + 1].isLowSurrogate() ? 2 : 1;
            r.append(escape(pattern.sliced(k, len)));
            k += len - 1;
        }
    }
    if (options.testFlag(UnanchoredWildcardConversion))
        return r;
    return anchoredPattern(r);
}

/*!
    \since 6.0
    Returns a regular expression for the shell wildcard \a pattern, converted
    with wildcardToRegularExpression() and the options \a options, matching
    with the case sensitivity \a cs.
*/
QRegularExpression QRegularExpression::fromWildcard(QStringView pattern, Qt::CaseSensitivity cs, WildcardConversionOptions options) {
    PatternOptions po = NoPatternOption;
    if (cs == Qt::CaseInsensitive)
        po |= CaseInsensitiveOption;
    return QRegularExpression(wildcardToRegularExpression(pattern, options), po);
}

/*!
    \since 5.12
    Returns \a expression wrapped in \c{\A(?:} and \c{)\z}, so that it only
    matches a whole string.
*/
QString QRegularExpression::anchoredPattern(QStringView expression) {
    return QString::fromLatin1("\\A(?:") + expression.toString() + QString::fromLatin1(")\\z");
}

bool operator==(const QRegularExpression &a, const QRegularExpression &b) noexcept {
    return a.d == b.d || (a.d->pattern == b.d->pattern && a.d->options == b.d->options);
}

std::size_t qHash(const QRegularExpression &key, std::size_t seed) noexcept {
    return qHash(key.pattern(), seed) ^ static_cast<std::size_t>(key.patternOptions().toInt());
}

// ------------------------------------------------------------------ QRegularExpressionMatch

/*!
    Constructs a valid match object that has no match. Its match type is
    QRegularExpression::NoMatch.
*/
QRegularExpressionMatch::QRegularExpressionMatch() : d(std::make_shared<QRegularExpressionMatchPrivate>()) { d->valid = true; }

QRegularExpressionMatch::QRegularExpressionMatch(std::shared_ptr<QRegularExpressionMatchPrivate> dd) : d(std::move(dd)) {}

/*!
    Constructs a copy of \a match.
*/
QRegularExpressionMatch::QRegularExpressionMatch(const QRegularExpressionMatch &match) = default;

/*!
    Move-constructs a match from \a match, which is left without a match.
*/
QRegularExpressionMatch::QRegularExpressionMatch(QRegularExpressionMatch &&match) noexcept : d(std::move(match.d)) {
    match.d = std::make_shared<QRegularExpressionMatchPrivate>();
}

/*!
    Destroys the match.
*/
QRegularExpressionMatch::~QRegularExpressionMatch() = default;

/*!
    Assigns \a match to this object and returns a reference to it.
*/
QRegularExpressionMatch &QRegularExpressionMatch::operator=(const QRegularExpressionMatch &match) = default;

/*!
    Move-assigns \a match to this object and returns a reference to it.
*/
QRegularExpressionMatch &QRegularExpressionMatch::operator=(QRegularExpressionMatch &&match) noexcept {
    d.swap(match.d);
    return *this;
}

/*!
    Returns the regular expression that produced this match.
*/
QRegularExpression QRegularExpressionMatch::regularExpression() const { return d->regex; }

/*!
    Returns the match type used to produce this match.
*/
QRegularExpression::MatchType QRegularExpressionMatch::matchType() const { return d->type; }

/*!
    Returns the match options used to produce this match.
*/
QRegularExpression::MatchOptions QRegularExpressionMatch::matchOptions() const { return d->options; }

/*!
    Returns \c true if the expression matched completely; otherwise returns
    \c false.

    \sa hasPartialMatch()
*/
bool QRegularExpressionMatch::hasMatch() const { return d->hasMatch; }

/*!
    Returns \c true if the subject ended while the expression could still
    match (only for the partial match types); otherwise returns \c false. A
    partial match sets only group 0, from where the match started to the end
    of the subject.

    \sa hasMatch()
*/
bool QRegularExpressionMatch::hasPartialMatch() const { return d->hasPartial; }

/*!
    Returns \c true if the match was produced by a valid regular expression
    (or is a default-constructed match); otherwise returns \c false.
*/
bool QRegularExpressionMatch::isValid() const { return d->valid; }

/*!
    Returns the index of the last group that captured something, 0 if only
    the whole match did, or -1 if there was no match.

    \sa captured(), hasCaptured()
*/
int QRegularExpressionMatch::lastCapturedIndex() const { return d->lastCaptured; }

int QRegularExpressionMatch::groupForName(QStringView name) const {
    if (name.isEmpty() || !d->regex.d)
        return -1;
    const QStringList names = d->regex.namedCaptureGroups();
    for (qsizetype g = 1; g < names.size(); ++g)
        if (names.at(g) == name.toString())
            return static_cast<int>(g);
    return -1;
}

/*!
    \since 6.3
    Returns \c true if group \a nth captured something, even an empty string;
    otherwise returns \c false.

    \sa captured(), lastCapturedIndex()
*/
bool QRegularExpressionMatch::hasCaptured(int nth) const { return nth >= 0 && nth <= d->lastCaptured && capturedStart(nth) >= 0; }

/*!
    \overload
    \since 6.3
    Returns \c true if the group called \a name captured something.
*/
bool QRegularExpressionMatch::hasCaptured(QStringView name) const { return hasCaptured(groupForName(name)); }

/*!
    Returns the text captured by group \a nth, or a null QString if the group
    did not capture or does not exist. Group 0 is the whole match.

    \sa capturedView(), capturedStart(), capturedTexts()
*/
QString QRegularExpressionMatch::captured(int nth) const {
    const qsizetype start = capturedStart(nth);
    if (start < 0)
        return QString();
    return d->subject.mid(start, capturedLength(nth));
}

/*!
    \overload
    Returns the text captured by the group called \a name, or a null QString.
*/
QString QRegularExpressionMatch::captured(QStringView name) const { return captured(groupForName(name)); }

/*!
    \since 5.10
    Returns a view of the text captured by group \a nth, or a null view. The
    view refers into the match object's copy of the subject.

    \sa captured()
*/
QStringView QRegularExpressionMatch::capturedView(int nth) const {
    const qsizetype start = capturedStart(nth);
    if (start < 0)
        return QStringView();
    return QStringView(d->subject.constData() + start, capturedLength(nth));
}

/*!
    \overload
    \since 5.10
    Returns a view of the text captured by the group called \a name.
*/
QStringView QRegularExpressionMatch::capturedView(QStringView name) const { return capturedView(groupForName(name)); }

/*!
    Returns the texts captured by groups 0 to lastCapturedIndex(). Groups that
    did not capture give null strings.

    \sa captured()
*/
QStringList QRegularExpressionMatch::capturedTexts() const {
    QStringList r;
    for (int g = 0; g <= d->lastCaptured; ++g)
        r.append(captured(g));
    return r;
}

/*!
    Returns the start of the text captured by group \a nth in the subject, or
    -1 if the group did not capture.

    \sa capturedEnd(), capturedLength()
*/
qsizetype QRegularExpressionMatch::capturedStart(int nth) const {
    if (nth < 0 || nth > d->lastCaptured)
        return -1;
    return d->caps[static_cast<std::size_t>(nth) * 2];
}

/*!
    Returns the length of the text captured by group \a nth, or 0 if the group
    did not capture.

    \sa capturedStart(), capturedEnd()
*/
qsizetype QRegularExpressionMatch::capturedLength(int nth) const {
    const qsizetype start = capturedStart(nth);
    return start < 0 ? 0 : d->caps[static_cast<std::size_t>(nth) * 2 + 1] - start;
}

/*!
    Returns the position just after the text captured by group \a nth, or -1
    if the group did not capture.

    \sa capturedStart(), capturedLength()
*/
qsizetype QRegularExpressionMatch::capturedEnd(int nth) const {
    const qsizetype start = capturedStart(nth);
    return start < 0 ? -1 : d->caps[static_cast<std::size_t>(nth) * 2 + 1];
}

/*!
    \overload
    Returns the start of the text captured by the group called \a name, or -1.
*/
qsizetype QRegularExpressionMatch::capturedStart(QStringView name) const { return capturedStart(groupForName(name)); }

/*!
    \overload
    Returns the length of the text captured by the group called \a name.
*/
qsizetype QRegularExpressionMatch::capturedLength(QStringView name) const { return capturedLength(groupForName(name)); }

/*!
    \overload
    Returns the end of the text captured by the group called \a name, or -1.
*/
qsizetype QRegularExpressionMatch::capturedEnd(QStringView name) const { return capturedEnd(groupForName(name)); }

// ------------------------------------------------------------------ QRegularExpressionMatchIterator

/*!
    Constructs an invalid iterator with no matches.
*/
QRegularExpressionMatchIterator::QRegularExpressionMatchIterator() : d(std::make_shared<QRegularExpressionMatchIteratorPrivate>()) {
    d->next.d->valid = false;
}

/*!
    Constructs a copy of \a iterator. The copy advances independently.
*/
QRegularExpressionMatchIterator::QRegularExpressionMatchIterator(const QRegularExpressionMatchIterator &iterator) = default;

/*!
    Move-constructs an iterator from \a iterator.
*/
QRegularExpressionMatchIterator::QRegularExpressionMatchIterator(QRegularExpressionMatchIterator &&iterator) noexcept
    : d(std::move(iterator.d)) {
    iterator.d = std::make_shared<QRegularExpressionMatchIteratorPrivate>();
}

/*!
    Destroys the iterator.
*/
QRegularExpressionMatchIterator::~QRegularExpressionMatchIterator() = default;

/*!
    Assigns \a iterator to this object and returns a reference to it.
*/
QRegularExpressionMatchIterator &QRegularExpressionMatchIterator::operator=(const QRegularExpressionMatchIterator &iterator) = default;

/*!
    Move-assigns \a iterator to this object and returns a reference to it.
*/
QRegularExpressionMatchIterator &QRegularExpressionMatchIterator::operator=(QRegularExpressionMatchIterator &&iterator) noexcept {
    d.swap(iterator.d);
    return *this;
}

/*!
    Returns \c true if the iterator was produced by a valid regular
    expression; otherwise returns \c false.
*/
bool QRegularExpressionMatchIterator::isValid() const { return d->next.isValid(); }

/*!
    Returns \c true if there is at least one more match; otherwise returns
    \c false.

    \sa next(), peekNext()
*/
bool QRegularExpressionMatchIterator::hasNext() const { return d->next.hasMatch() || d->next.hasPartialMatch(); }

/*!
    Returns the next match and advances the iterator. There must be a next
    match.

    \sa hasNext(), peekNext()
*/
QRegularExpressionMatch QRegularExpressionMatchIterator::next() {
    if (!hasNext()) {
        qWarning("QRegularExpressionMatchIterator::next() called on an iterator already at end");
        return d->next;
    }
    if (d.use_count() > 1)
        d = std::make_shared<QRegularExpressionMatchIteratorPrivate>(*d);
    QRegularExpressionMatch current = d->next;
    d->advance();
    return current;
}

/*!
    Returns the next match without advancing the iterator. There must be a
    next match.

    \sa next()
*/
QRegularExpressionMatch QRegularExpressionMatchIterator::peekNext() const {
    if (!hasNext())
        qWarning("QRegularExpressionMatchIterator::peekNext() called on an iterator already at end");
    return d->next;
}

/*!
    Returns the regular expression that produced this iterator.
*/
QRegularExpression QRegularExpressionMatchIterator::regularExpression() const { return d->regex; }

/*!
    Returns the match type used by this iterator.
*/
QRegularExpression::MatchType QRegularExpressionMatchIterator::matchType() const { return d->type; }

/*!
    Returns the match options used by this iterator.
*/
QRegularExpression::MatchOptions QRegularExpressionMatchIterator::matchOptions() const { return d->options; }

// ------------------------------------------------------------------ QString, QStringView and QStringList

/*!
    \since 5.0
    Returns \c true if the regular expression \a re matches somewhere in this
    string; otherwise returns \c false. If \a rmatch is not \c nullptr and
    there is a match, the match is stored there.

    \sa indexOf(), QRegularExpression::match()
*/
bool QString::contains(const QRegularExpression &re, QRegularExpressionMatch *rmatch) const { return indexOf(re, 0, rmatch) != -1; }

/*!
    \since 5.0
    Returns the index position of the first match of \a re in this string,
    searching forward from \a from, or -1 if there is none. If \a rmatch is
    not \c nullptr and there is a match, the match is stored there.

    \sa lastIndexOf(), contains()
*/
qsizetype QString::indexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch) const {
    if (!re.isValid()) {
        qWarning("QString::indexOf: invalid QRegularExpression object");
        return -1;
    }
    const QRegularExpressionMatch m = re.match(*this, from);
    if (!m.hasMatch())
        return -1;
    if (rmatch)
        *rmatch = m;
    return m.capturedStart();
}

/*!
    \since 5.0
    Returns the index position of the last match of \a re in this string, or
    -1 if there is none. If \a rmatch is not \c nullptr and there is a match,
    the match is stored there.

    \sa indexOf()
*/
qsizetype QString::lastIndexOf(const QRegularExpression &re, QRegularExpressionMatch *rmatch) const { return lastIndexOf(re, size(), rmatch); }

/*!
    \overload
    \since 5.0
    Returns the index position of the last match of \a re that starts at or
    before \a from, or -1. A negative \a from counts from the end.
*/
qsizetype QString::lastIndexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch) const {
    if (!re.isValid()) {
        qWarning("QString::lastIndexOf: invalid QRegularExpression object");
        return -1;
    }
    if (from < 0)
        from += size();
    for (qsizetype start = std::min(from, size()); start >= 0; --start) {
        const QRegularExpressionMatch m = re.match(*this, start, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
        if (m.hasMatch()) {
            if (rmatch)
                *rmatch = m;
            return start;
        }
    }
    return -1;
}

/*!
    \since 5.0
    Returns the number of times \a re matches in this string. Matches may
    overlap: after each match the search continues one character after where
    the match started.

    \sa contains()
*/
qsizetype QString::count(const QRegularExpression &re) const {
    if (!re.isValid()) {
        qWarning("QString::count: invalid QRegularExpression object");
        return 0;
    }
    qsizetype n = 0;
    qsizetype index = -1;
    for (;;) {
        const QRegularExpressionMatch m = re.match(*this, index + 1);
        if (!m.hasMatch())
            break;
        ++n;
        index = m.capturedStart();
    }
    return n;
}

/*!
    \since 5.0
    Replaces every match of \a re in this string with \a after and returns a
    reference to the string. In \a after, \c{\1} to \c{\99} stand for the text
    captured by the corresponding group; a two-digit reference is used only
    if the expression has that many groups.

    \sa remove()
*/
QString &QString::replace(const QRegularExpression &re, const QString &after) {
    if (!re.isValid()) {
        qWarning("QString::replace: invalid QRegularExpression object");
        return *this;
    }
    const QString source = *this;
    const int groups = re.captureCount();
    QString result;
    qsizetype last = 0;
    bool any = false;
    for (const QRegularExpressionMatch &m : re.globalMatch(source)) {
        any = true;
        result.append(source.mid(last, m.capturedStart() - last));
        for (qsizetype k = 0; k < after.size(); ++k) {
            const QChar c = after.at(k);
            if (c == u'\\' && k + 1 < after.size() && after.at(k + 1).isDigit()) {
                int num = after.at(k + 1).digitValue();
                qsizetype used = 1;
                if (k + 2 < after.size() && after.at(k + 2).isDigit()) {
                    const int two = num * 10 + after.at(k + 2).digitValue();
                    if (two <= groups) {
                        num = two;
                        used = 2;
                    }
                }
                if (num <= groups) {
                    result.append(m.captured(num));
                    k += used;
                    continue;
                }
            }
            result.append(c);
        }
        last = m.capturedEnd();
    }
    if (!any)
        return *this;
    result.append(source.mid(last));
    return *this = result;
}

/*!
    \since 5.0
    Removes every match of \a re from this string and returns a reference to
    the string.

    \sa replace()
*/
QString &QString::remove(const QRegularExpression &re) { return replace(re, QString()); }

/*!
    \since 5.14
    Splits the string into substrings wherever \a sep matches and returns the
    list of those substrings. \a behavior selects whether empty substrings
    are kept.

    \sa QStringList::join()
*/
QStringList QString::split(const QRegularExpression &sep, Qt::SplitBehavior behavior) const {
    QStringList list;
    if (!sep.isValid()) {
        qWarning("QString::split: invalid QRegularExpression object");
        return list;
    }
    qsizetype start = 0;
    for (const QRegularExpressionMatch &m : sep.globalMatch(*this)) {
        const qsizetype end = m.capturedStart();
        if (start != end || behavior == Qt::KeepEmptyParts)
            list.append(mid(start, end - start));
        start = m.capturedEnd();
    }
    if (start != size() || behavior == Qt::KeepEmptyParts)
        list.append(mid(start));
    return list;
}

/*!
    \since 6.1
    Returns \c true if \a re matches somewhere in the viewed text; otherwise
    returns \c false. If \a rmatch is not \c nullptr, a match is stored there.
*/
bool QStringView::contains(const QRegularExpression &re, QRegularExpressionMatch *rmatch) const { return toString().contains(re, rmatch); }

/*!
    \since 6.1
    Returns the index position of the first match of \a re in the viewed text,
    searching from \a from, or -1. If \a rmatch is not \c nullptr, the match
    is stored there.
*/
qsizetype QStringView::indexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch) const {
    return toString().indexOf(re, from, rmatch);
}

/*!
    \since 6.1
    Returns the index position of the last match of \a re in the viewed text,
    or -1. If \a rmatch is not \c nullptr, the match is stored there.
*/
qsizetype QStringView::lastIndexOf(const QRegularExpression &re, QRegularExpressionMatch *rmatch) const { return toString().lastIndexOf(re, rmatch); }

/*!
    \overload
    \since 6.1
    Returns the index position of the last match of \a re that starts at or
    before \a from, or -1. If \a rmatch is not \c nullptr, the match is stored
    there.
*/
qsizetype QStringView::lastIndexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch) const {
    return toString().lastIndexOf(re, from, rmatch);
}

/*!
    \since 6.1
    Returns the number of (possibly overlapping) matches of \a re in the
    viewed text.
*/
qsizetype QStringView::count(const QRegularExpression &re) const { return toString().count(re); }

/*!
    \since 5.0
    Returns the strings of the list that \a re matches somewhere.

    \sa QString::contains()
*/
QStringList QStringList::filter(const QRegularExpression &re) const {
    QStringList r;
    for (const QString &s : *this)
        if (s.contains(re))
            r.append(s);
    return r;
}

/*!
    \overload
    \since 5.0
    Returns the index of the first string in the list, searching forward from
    \a from, that \a re matches exactly (the whole string), or -1.
*/
qsizetype QStringList::indexOf(const QRegularExpression &re, qsizetype from) const {
    const QRegularExpression exact(QRegularExpression::anchoredPattern(re.pattern()), re.patternOptions());
    if (from < 0)
        from = std::max<qsizetype>(from + size(), 0);
    for (qsizetype k = from; k < size(); ++k)
        if (exact.match(at(k)).hasMatch())
            return k;
    return -1;
}

/*!
    \overload
    \since 5.0
    Returns the index of the last string in the list, searching backward from
    \a from, that \a re matches exactly, or -1. A negative \a from counts from
    the end of the list.
*/
qsizetype QStringList::lastIndexOf(const QRegularExpression &re, qsizetype from) const {
    const QRegularExpression exact(QRegularExpression::anchoredPattern(re.pattern()), re.patternOptions());
    if (from < 0)
        from += size();
    else if (from >= size())
        from = size() - 1;
    for (qsizetype k = from; k >= 0; --k)
        if (exact.match(at(k)).hasMatch())
            return k;
    return -1;
}

/*!
    \overload
    \since 5.0
    Replaces every match of \a re with \a after in each string of the list,
    as QString::replace() does, and returns a reference to the list.
*/
QStringList &QStringList::replaceInStrings(const QRegularExpression &re, const QString &after) {
    for (QString &s : *this)
        s.replace(re, after);
    return *this;
}
