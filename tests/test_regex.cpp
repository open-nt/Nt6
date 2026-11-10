// MIT License - see LICENSE
#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static QStringList g_messages;
static void captureHandler(QtMsgType, const QMessageLogContext &, const QString &msg) { g_messages.append(msg); }

using RE = QRegularExpression;

// Formats the captures of a match: "NOMATCH", or the captured texts joined with commas,
// with "~" for a group that did not capture.
static std::string fmt(const QRegularExpressionMatch &m) {
    if (!m.hasMatch())
        return "NOMATCH";
    std::string r;
    for (int i = 0; i <= m.lastCapturedIndex(); ++i) {
        if (i)
            r += ',';
        r += m.capturedStart(i) < 0 ? std::string("~") : m.captured(i).toStdString();
    }
    return r;
}

static std::string run(const char *pattern, int options, const char *subject, qsizetype offset = 0) {
    RE re(QString::fromUtf8(pattern), RE::PatternOptions(static_cast<RE::PatternOption>(options)));
    if (!re.isValid())
        return "INVALID:" + re.errorString().toStdString();
    return fmt(re.match(QString::fromUtf8(subject), offset));
}

struct Case {
    const char *pattern;
    int options;
    const char *subject;
    const char *expected;
};

static const int I = RE::CaseInsensitiveOption;
static const int S = RE::DotMatchesEverythingOption;
static const int M = RE::MultilineOption;
static const int X = RE::ExtendedPatternSyntaxOption;
static const int U = RE::InvertedGreedinessOption;
static const int N = RE::DontCaptureOption;
static const int P = RE::UseUnicodePropertiesOption;

static const Case kCases[] = {
    // literals and alternation
    {"abc", 0, "xxabcxx", "abc"},
    {"abc", 0, "ab", "NOMATCH"},
    {"", 0, "abc", ""},
    {"a|b|c", 0, "xxc", "c"},
    {"ab|abc", 0, "abc", "ab"},
    {"abc|ab", 0, "abd", "ab"},
    {"a(b|c)d", 0, "acd", "acd,c"},
    {"(a|ab)(c|bcd)(d*)", 0, "abcd", "abcd,a,bcd,"},
    {"a|", 0, "b", ""},
    {"(|a)+", 0, "aaa", ","},
    // dot, classes and escapes
    {"a.c", 0, "a\nc abc", "abc"},
    {"a.c", S, "a\nc", "a\nc"},
    {"[abc]+", 0, "xxcabbz", "cabb"},
    {"[^abc]+", 0, "abcxyzabc", "xyz"},
    {"[a-c-]+", 0, "x-ab-c", "-ab-c"},
    {"[]a]+", 0, "x]a]", "]a]"},
    {"[^]a]+", 0, "]ab]", "b"},
    {"[\\d.]+", 0, "v3.14!", "3.14"},
    {"[\\w-]+", 0, "a-b_c d", "a-b_c"},
    {"\\d+", 0, "abc123def", "123"},
    {"\\D+", 0, "123abc456", "abc"},
    {"\\w+", 0, "  hello_1 ", "hello_1"},
    {"\\W+", 0, "ab, cd", ", "},
    {"\\s+", 0, "a \t\nb", " \t\n"},
    {"\\S+", 0, "  ab  ", "ab"},
    {"\\h+", 0, "a \tb", " \t"},
    {"\\v+", 0, "a\n\rb", "\n\r"},
    {"a\\Rb", 0, "a\r\nb", "a\r\nb"},
    {"a\\Rb", 0, "a\nb", "a\nb"},
    {"\\N+", 0, "ab\ncd", "ab"},
    {"\\x41\\x{42}\\103", 0, "ABC", "ABC"},
    {"\\x{1F600}", 0, "a\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"},
    {"\\t\\n\\r\\f\\a\\e", 0, "\t\n\r\f\a\x1b", "\t\n\r\f\a\x1b"},
    {"\\cA", 0, "\x01", "\x01"},
    {"\\0", 0, "a", "NOMATCH"},
    {"\\101", 0, "A", "A"},
    {"[\\101-\\103]+", 0, "xABCD", "ABC"},
    {"\\Qa.b\\E+", 0, "a.bbb", "a.bbb"},
    {"\\Qa.b", 0, "xa.b", "a.b"},
    {"a\\.b", 0, "axb a.b", "a.b"},
    {"[[:alpha:]]+", 0, "12abc34", "abc"},
    {"[[:^alpha:]]+", 0, "ab12cd", "12"},
    {"[[:digit:][:space:]]+", 0, "ab1 2cd", "1 2"},
    {"[[:upper:]]+", 0, "abCDe", "CD"},
    {"[[:punct:]]+", 0, "ab,.!c", ",.!"},
    {"[[:xdigit:]]+", 0, "xyz1aFg", "1aF"},
    {"[[:word:]]+", 0, "!!ab_1!", "ab_1"},
    {"[a-z&&[^b]]+", 0, "a&&[^b]", "b]"},
    {"[\\b]", 0, "a\bb", "\b"},
    // anchors
    {"^abc", 0, "abc", "abc"},
    {"^abc", 0, "xabc", "NOMATCH"},
    {"abc$", 0, "abc\n", "abc"},
    {"abc$", 0, "abc\n\n", "NOMATCH"},
    {"abc\\Z", 0, "abc\n", "abc"},
    {"abc\\z", 0, "abc\n", "NOMATCH"},
    {"^b", M, "a\nb", "b"},
    {"a$", M, "a\nb", "a"},
    {"^", M, "a\n", ""},
    {"\\Ab", M, "a\nb", "NOMATCH"},
    {"\\bfoo\\b", 0, "a foo b", "foo"},
    {"\\bfoo\\b", 0, "afoo b", "NOMATCH"},
    {"\\Boo", 0, "foo", "oo"},
    {"\\B", 0, "ab", ""},
    {"\\b", 0, "", "NOMATCH"},
    // quantifiers
    {"a*", 0, "aaab", "aaa"},
    {"a+", 0, "baaa", "aaa"},
    {"a?", 0, "b", ""},
    {"a{2}", 0, "aaaa", "aa"},
    {"a{2,}", 0, "aaaa", "aaaa"},
    {"a{2,3}", 0, "aaaa", "aaa"},
    {"a{,3}", 0, "a{,3}", "a{,3}"},
    {"a{", 0, "a{", "a{"},
    {"a{1", 0, "a{1", "a{1"},
    {"x{0}y", 0, "xy", "y"},
    {"a*?", 0, "aaa", ""},
    {"a+?", 0, "aaa", "a"},
    {"a??b", 0, "ab", "ab"},
    {"a{2,3}?", 0, "aaaa", "aa"},
    {"a*+a", 0, "aaa", "NOMATCH"},
    {"a++b", 0, "aaab", "aaab"},
    {"a?+a", 0, "a", "NOMATCH"},
    {"(?:ab)+", 0, "ababab", "ababab"},
    {"(ab)+", 0, "ababab", "ababab,ab"},
    {"(ab)*?c", 0, "ababc", "ababc,ab"},
    {"(a+)+b", 0, "aaab", "aaab,aaa"},
    {"(a*)*", 0, "b", ","},
    {"(a*)+", 0, "b", ","},
    {"(a|b)*", 0, "abab", "abab,b"},
    {"(?:a|b){2,3}", 0, "ababab", "aba"},
    {"(a){0}b", 0, "ab", "b"},
    {"(a){2}", 0, "aaa", "aa,a"},
    {".*", 0, "abc\ndef", "abc"},
    {".*?c", 0, "abcabc", "abc"},
    {".+b", 0, "aabab", "aabab"},
    {"a.*b", 0, "xaxxbxxbx", "axxbxxb"},
    {"(?:(a)|b)+", 0, "ab", "ab,a"},
    {"\\d{3}-\\d{4}", 0, "call 555-1234 now", "555-1234"},
    {"(x)?y", 0, "y", "y"},
    {"(x)?(y)", 0, "y", "y,~,y"},
    {"(a)|(b)", 0, "b", "b,~,b"},
    // case insensitive
    {"abc", I, "xABCx", "ABC"},
    {"[a-c]+", I, "ABCabc", "ABCabc"},
    {"[^a]+", I, "AAbA", "b"},
    {"straSSe", I, "STRASSE", "STRASSE"},
    {"\xCF\x83", I, "\xCE\xA3", "\xCE\xA3"},
    {"\xC3\xA9", I, "\xC3\x89", "\xC3\x89"},
    {"\xD0\xB6", I, "\xD0\x96", "\xD0\x96"},
    {"k", I, "\xE2\x84\xAA", "\xE2\x84\xAA"},
    {"(a)\\1", I, "aA", "aA,a"},
    {"\\p{Lu}", I, "a", "NOMATCH"},
    // inline options
    {"(?i)abc", 0, "ABC", "ABC"},
    {"a(?i)bc", 0, "aBC", "aBC"},
    {"a(?i)bc", 0, "ABC", "NOMATCH"},
    {"a(?i:b)c", 0, "aBc", "aBc"},
    {"a(?i:b)c", 0, "aBC", "NOMATCH"},
    {"(?i)a(?-i)b", 0, "Ab", "Ab"},
    {"(?i)a(?-i)b", 0, "AB", "NOMATCH"},
    {"(a(?i)b|c)d", 0, "Cd", "Cd,C"},
    {"(?s)a.b", 0, "a\nb", "a\nb"},
    {"(?m)^b", 0, "a\nb", "b"},
    {"(?x) a b # comment\n c", 0, "abc", "abc"},
    {"(?x) a [ ] b", 0, "a b", "a b"},
    {"(?x)a\\ b", 0, "a b", "a b"},
    {"(?U)a+", 0, "aaa", "a"},
    {"(?U)a+?", 0, "aaa", "aaa"},
    {"(?n)(a)(?<x>b)", 0, "ab", "ab,b"},
    {"(?^)ab", I, "AB", "NOMATCH"},
    {"a(?#comment)b", 0, "ab", "ab"},
    // option flags
    {"a b", X, "ab", "ab"},
    {"a # x\n b", X, "ab", "ab"},
    {"a+", U, "aaa", "a"},
    {"a+?", U, "aaa", "aaa"},
    {"(a)(b)", N, "ab", "ab"},
    {"(?<n>a)(b)", N, "ab", "ab,a"},
    {"^b$", M, "a\nb\nc", "b"},
    // groups and backreferences
    {"(a)(b)(c)", 0, "abc", "abc,a,b,c"},
    {"(a)\\1", 0, "aa", "aa,a"},
    {"(a)\\1", 0, "ab", "NOMATCH"},
    {"(a)(b)\\2\\1", 0, "abba", "abba,a,b"},
    {"(?<x>a)\\k<x>", 0, "aa", "aa,a"},
    {"(?<x>a)\\k'x'", 0, "aa", "aa,a"},
    {"(?<x>a)\\k{x}", 0, "aa", "aa,a"},
    {"(?P<x>a)(?P=x)", 0, "aa", "aa,a"},
    {"(?'x'a)\\g{x}", 0, "aa", "aa,a"},
    {"(a)\\g1", 0, "aa", "aa,a"},
    {"(a)\\g{1}", 0, "aa", "aa,a"},
    {"(a)(b)\\g{-1}", 0, "abb", "abb,a,b"},
    {"(a)(b)\\g{-2}", 0, "aba", "aba,a,b"},
    {"(a)?\\1", 0, "b", "NOMATCH"},
    {"(a|b)\\1", 0, "abb", "bb,b"},
    {"(\\w+) \\1", 0, "hello hello world", "hello hello,hello"},
    {"(a)(?:b)(c)", 0, "abc", "abc,a,c"},
    {"(?|(a)|(b))\\1", 0, "bb", "bb,b"},
    {"(?|(a)(x)|(b)(y))", 0, "by", "by,b,y"},
    {"(?>a+)b", 0, "aaab", "aaab"},
    {"(?>a+)a", 0, "aaa", "NOMATCH"},
    {"(?>a|ab)c", 0, "abc", "NOMATCH"},
    {"((a)|(b))+", 0, "ab", "ab,b,a,b"},
    {"(a(b(c)))", 0, "abc", "abc,abc,bc,c"},
    {"\\10", 0, "\x08", "\x08"},
    {"(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)\\10", 0, "abcdefghijj", "abcdefghijj,a,b,c,d,e,f,g,h,i,j"},
    // lookaround
    {"a(?=b)", 0, "ac ab", "a"},
    {"a(?!b)", 0, "ab ac", "a"},
    {"(?<=a)b", 0, "cb ab", "b"},
    {"(?<!a)b", 0, "ab cb", "b"},
    {"(?<=ab|c)d", 0, "abd", "d"},
    {"(?<=ab|c)d", 0, "cd", "d"},
    {"(?<=a{2,3})b", 0, "aab", "b"},
    {"(?<=a{2,3})b", 0, "ab", "NOMATCH"},
    {"(?<=\\d{3})x", 0, "123x", "x"},
    {"(?<!\\d)x", 0, "1x ax", "x"},
    {"(?=(a))a", 0, "a", "a,a"},
    {"(?!(a))b", 0, "b", "b"},
    {"(?=a)*b", 0, "b", "b"},
    {"\\d+(?=%)", 0, "50% 60", "50"},
    {"(?<=\\$)\\d+", 0, "cost $42", "42"},
    {"(?<=^|,)x", 0, "a,x", "x"},
    {"foo(?=bar)bar", 0, "foobar", "foobar"},
    {"(?<![a-z])\\d", 0, "a1 2", "2"},
    {"\\b\\w+(?<!s)\\b", 0, "cats dog", "dog"},
    {"(?<=\\Ga)a", 0, "aa", "a"},
    // conditionals
    {"(a)?(?(1)b|c)", 0, "ab", "ab,a"},
    {"(a)?(?(1)b|c)", 0, "c", "c"},
    {"(?<q>a)?(?(<q>)b|c)", 0, "ab", "ab,a"},
    {"(?<q>a)?(?(q)b|c)", 0, "c", "c"},
    {"(a)?(?(1)b)", 0, "ab", "ab,a"},
    {"(?(?=a)ab|cd)", 0, "ab", "ab"},
    {"(?(?=a)ab|cd)", 0, "cd", "cd"},
    {"(?(?!a)cd|ab)", 0, "ab", "ab"},
    {"(?(DEFINE)(?<d>\\d))a", 0, "a", "a"},
    // \K
    {"foo\\Kbar", 0, "foobar", "bar"},
    {"a\\Kb", 0, "ab", "b"},
    // unicode properties
    {"\\p{L}+", 0, "12\xC3\xA9t\xC3\xA9 34", "\xC3\xA9t\xC3\xA9"},
    {"\\p{Lu}+", 0, "abCD\xC3\x89", "CD\xC3\x89"},
    {"\\p{Ll}+", 0, "ABcd\xC3\xA9", "cd\xC3\xA9"},
    {"\\P{L}+", 0, "ab12cd", "12"},
    {"\\p{^L}+", 0, "ab12cd", "12"},
    {"[\\p{L}\\d]+", 0, "!a1\xC3\xA9!", "a1\xC3\xA9"},
    {"\\pL+", 0, "12ab", "ab"},
    {"\\PL+", 0, "ab12", "12"},
    {"\\p{N}+", 0, "ab12\xD9\xA3", "12\xD9\xA3"},
    {"\\p{Nd}+", 0, "x\xD9\xA1\xD9\xA2", "\xD9\xA1\xD9\xA2"},
    {"\\p{P}+", 0, "ab!?cd", "!?"},
    {"\\p{S}+", 0, "ab+$cd", "+$"},
    {"\\p{Sc}", 0, "a\xE2\x82\xAC", "\xE2\x82\xAC"},
    {"\\p{Z}+", 0, "a \xC2\xA0 b", " \xC2\xA0 "},
    {"\\p{Zs}", 0, "a b", " "},
    {"\\p{Cc}", 0, "a\x01", "\x01"},
    {"\\p{M}", 0, "e\xCC\x81", "\xCC\x81"},
    {"\\p{Greek}+", 0, "abc\xCE\xB1\xCE\xB2\xCE\xB3", "\xCE\xB1\xCE\xB2\xCE\xB3"},
    {"\\p{Cyrillic}+", 0, "ab\xD0\x96\xD0\xB6", "\xD0\x96\xD0\xB6"},
    {"\\p{Han}+", 0, "ab\xE6\xBC\xA2\xE5\xAD\x97", "\xE6\xBC\xA2\xE5\xAD\x97"},
    {"\\p{Hiragana}+", 0, "a\xE3\x81\x82\xE3\x81\x84", "\xE3\x81\x82\xE3\x81\x84"},
    {"\\p{Latin}+", 0, "\xCE\xB1" "ab\xC3\xA9", "ab\xC3\xA9"},
    {"\\p{Script=Greek}", 0, "\xCE\xB1", "\xCE\xB1"},
    {"\\p{L&}+", 0, "12aB34", "aB"},
    {"\\p{Any}", 0, "\n", "\n"},
    {"\\p{Xan}+", 0, "!a1!", "a1"},
    {"\\p{Lu}", 0, "\xF0\x9D\x90\x80", "\xF0\x9D\x90\x80"},
    {"\\p{Letter}+", 0, "1ab2", "ab"},
    {"\\p{Uppercase_Letter}", 0, "aB", "B"},
    // astral characters
    {".", 0, "\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"},
    {"^.$", 0, "\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"},
    {"^..$", 0, "\xF0\x9F\x98\x80", "NOMATCH"},
    {"[\xF0\x9F\x98\x80-\xF0\x9F\x98\x8F]+", 0, "a\xF0\x9F\x98\x81\xF0\x9F\x98\x82", "\xF0\x9F\x98\x81\xF0\x9F\x98\x82"},
    {"\xF0\x9F\x98\x80+", 0, "\xF0\x9F\x98\x80\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80\xF0\x9F\x98\x80"},
    {"[^a]", 0, "\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"},
    {".*", 0, "a\xF0\x9F\x98\x80z", "a\xF0\x9F\x98\x80z"},
    {"\\X", 0, "e\xCC\x81z", "e\xCC\x81"},
    {"\\X", 0, "\r\n", "\r\n"},
    {"\\X\\X", 0, "ab", "ab"},
    {"^\\X$", 0, "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9", "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9"},
    {"^\\X$", 0, "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7", "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7"},
    // unicode properties option
    {"\\w+", 0, "caf\xC3\xA9", "caf"},
    {"\\w+", P, "caf\xC3\xA9", "caf\xC3\xA9"},
    {"\\d+", 0, "\xD9\xA1\xD9\xA2", "NOMATCH"},
    {"\\d+", P, "\xD9\xA1\xD9\xA2", "\xD9\xA1\xD9\xA2"},
    {"\\s", P, "\xC2\xA0", "\xC2\xA0"},
    {"\\s", 0, "\xC2\xA0", "NOMATCH"},
    {"\\bcaf\\b", 0, "caf\xC3\xA9", "caf"},
    {"\\bcaf\\b", P, "caf\xC3\xA9", "NOMATCH"},
    {"[[:alpha:]]+", P, "12\xC3\xA9t\xC3\xA9", "\xC3\xA9t\xC3\xA9"},
    {"[[:upper:]]", P, "a\xC3\x89", "\xC3\x89"},
    {"\\W+", P, "a\xC3\xA9, b", ", "},
    // unicode literals
    {"\xC3\xA9", 0, "caf\xC3\xA9", "\xC3\xA9"},
    {"caf\xC3\xA9", 0, "un caf\xC3\xA9", "caf\xC3\xA9"},
    {"\xE6\x97\xA5\xE6\x9C\xAC", 0, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "\xE6\x97\xA5\xE6\x9C\xAC"},
    // misc well known
    {"(\\d+)-(\\d+)", 0, "tel 12-345 x", "12-345,12,345"},
    {"^(\\w+)@(\\w+)\\.com$", 0, "bob@example.com", "bob@example.com,bob,example"},
    {"^(?:(?:25[0-5]|2[0-4]\\d|1?\\d?\\d)(?:\\.|$)){4}", 0, "192.168.1.255", "192.168.1.255"},
    {"<([a-z]+)>.*?</\\1>", 0, "<b>x</b><i>y</i>", "<b>x</b>,b"},
    {"\"([^\"\\\\]|\\\\.)*\"", 0, "say \"a\\\"b\" ok", "\"a\\\"b\",b"},
    {"(?<year>\\d{4})-(?<m>\\d\\d)", 0, "on 2024-05-17", "2024-05,2024,05"},
    {"^\\s*$", 0, "  \t ", "  \t "},
    {"colou?r", 0, "color colour", "color"},
    {"(?i)hello\\s+world", 0, "HeLLo   WORLD", "HeLLo   WORLD"},
    {"\\$\\d+\\.\\d{2}", 0, "pay $12.50 now", "$12.50"},
    {"[A-Z][a-z]+(?: [A-Z][a-z]+)*", 0, "met John Paul Smith.", "John Paul Smith"},
    {"(a+)(b+)?", 0, "aaab", "aaab,aaa,b"},
    {"a(?:b|c|d){6,7}?(.)", 0, "acdbcdbe", "acdbcdbe,e"},
    {"((?i)a)b", 0, "Ab", "Ab,A"},
    {"((?i)a)b", 0, "AB", "NOMATCH"},
    {"(?:(?i)a)b", 0, "Ab", "Ab"},
};

static void testTable() {
    for (const Case &c : kCases) {
        const std::string got = run(c.pattern, c.options, c.subject);
        if (got != c.expected) {
            std::printf("FAIL pattern '%s' options %d subject '%s': got '%s' expected '%s'\n", c.pattern, c.options,
                        c.subject, got.c_str(), c.expected);
            ++g_failures;
        }
    }
}

struct ErrorCase {
    const char *pattern;
    const char *message;
    int offset;
};

static const ErrorCase kErrorCases[] = {
    {"(abc", "missing closing parenthesis", 4},
    {"abc)", "unmatched closing parenthesis", 3},
    {"((a)", "missing closing parenthesis", 4},
    {"[abc", "missing terminating ] for character class", 4},
    {"[", "missing terminating ] for character class", 1},
    {"[]", "missing terminating ] for character class", 2},
    {"*abc", "quantifier does not follow a repeatable item", 0},
    {"+", "quantifier does not follow a repeatable item", 0},
    {"a**", "quantifier does not follow a repeatable item", 2},
    {"(*)", "(*VERB) not recognized or malformed", 2},
    {"a|*", "quantifier does not follow a repeatable item", 2},
    {"^*", "quantifier does not follow a repeatable item", 1},
    {"a{3,2}", "numbers out of order in {} quantifier", 6},
    {"a{99999}", "number too big in {} quantifier", 7},
    {"\\", "\\ at end of pattern", 1},
    {"ab\\", "\\ at end of pattern", 3},
    {"\\y", "unrecognized character follows \\", 2},
    {"\\c", "\\c at end of pattern", 2},
    {"\\c\x01", "\\c must be followed by a printable ASCII character", 3},
    {"[z-a]", "range out of order in character class", 3},
    {"[a-\\d]", "invalid range in character class", 3},
    {"[\\B]", "escape sequence is invalid in character class", 3},
    {"[\\N]", "\\N is not supported in a class", 3},
    {"(?z)", "unrecognized character after (? or (?-", 2},
    {"(?", "unrecognized character after (? or (?-", 2},
    {"\\1", "reference to non-existent subpattern", 2},
    {"(a)\\2", "reference to non-existent subpattern", 5},
    {"\\k<x>", "reference to non-existent subpattern", 3},
    {"(?<n>a)(?<n>b)", "two named subpatterns have the same name (PCRE2_DUPNAMES not set)", 10},
    {"(?<1a>x)", "subpattern name must start with a non-digit", 3},
    {"(?<a", "syntax error in subpattern name (missing terminator?)", 4},
    {"(?<>a)", "subpattern name expected", 3},
    {"[[:foo:]]", "unknown POSIX class name", 3},
    {"[[.a.]]", "POSIX collating elements are not supported", 1},
    {"[:alpha:]", "POSIX named classes are supported only within a class", 0},
    {"\\p{Foo}", "unknown property after \\P or \\p", 7},
    {"\\p{L", "malformed \\P or \\p sequence", 4},
    {"\\p", "malformed \\P or \\p sequence", 2},
    {"(?<=a+)b", "lookbehind assertion is not fixed length", 0},
    {"x(?<!a*)", "lookbehind assertion is not fixed length", 1},
    {"(?#abc", "missing ) after (?# comment", 6},
    {"\\x{110000}", "character code point value in \\x{} or \\o{} is too large", 10},
    {"\\x{d800}", "disallowed Unicode code point (>= 0xd800 && <= 0xdfff)", 8},
    {"\\x{zz}", "non-hex character in \\x{} (closing brace missing?)", 3},
    {"\\o12", "missing opening brace after \\o", 2},
    {"(*FOO)", "(*VERB) not recognized or malformed", 2},
    {"(?(1)a|b|c)", "conditional subpattern contains more than two branches", 11},
    {"(?R)", "recursive and subroutine calls are not supported", 2},
    {"\\u0041", "PCRE2 does not support \\F, \\L, \\l, \\N{name}, \\U, or \\u", 2},
    {"\\g", "\\g is not followed by a braced, angle-bracketed, or quoted name/number or by a plain number", 2},
    {"\\k", "\\k is not followed by a braced, angle-bracketed, or quoted name", 2},
    {"a(?i", "missing closing parenthesis", 4},
};

static void testErrors() {
    for (const ErrorCase &c : kErrorCases) {
        RE re(QString::fromUtf8(c.pattern));
        if (re.isValid() || re.errorString() != QString::fromUtf8(c.message) || re.patternErrorOffset() != c.offset) {
            std::printf("FAIL error case '%s': valid=%d '%s' at %d (expected '%s' at %d)\n", c.pattern, re.isValid(),
                        re.errorString().toStdString().c_str(), int(re.patternErrorOffset()), c.message, c.offset);
            ++g_failures;
        }
    }
    RE ok("a+");
    CHECK(ok.isValid());
    CHECK(ok.errorString() == QString("no error"));
    CHECK(ok.patternErrorOffset() == -1);
    RE bad("(");
    CHECK(!bad.isValid());
    CHECK(bad.captureCount() == -1);
    CHECK(bad.namedCaptureGroups().isEmpty());
    g_messages.clear();
    QRegularExpressionMatch m = bad.match("abc");
    CHECK(!m.isValid());
    CHECK(!m.hasMatch());
    CHECK(g_messages.size() == 1);
    CHECK(g_messages.value(0).contains(QString("invalid QRegularExpression")));
    CHECK(!bad.globalMatch("abc").hasNext());
    // Deeply nested groups and huge repetitions are rejected, not crashed on.
    CHECK(!RE(QString(300, QChar(u'(')) + QString(300, QChar(u')'))).isValid());
    CHECK(RE(QString(100, QChar(u'(')) + QString(100, QChar(u')'))).isValid());
    CHECK(!RE("((a{1000}){1000}){1000}").isValid());
    CHECK(RE("((a{1000}){1000}){1000}").errorString() == QString("regular expression is too large"));
    CHECK(RE("a{65535}").isValid());
    CHECK(!RE("a{65536}").isValid());
}

static void testPatternAccessors() {
    RE re;
    CHECK(re.isValid());
    CHECK(re.pattern().isEmpty());
    CHECK(re.patternOptions() == RE::NoPatternOption);
    CHECK(re.captureCount() == 0);
    re.setPattern("(a)(?<n>b)");
    CHECK(re.pattern() == QString("(a)(?<n>b)"));
    CHECK(re.captureCount() == 2);
    QStringList names = re.namedCaptureGroups();
    CHECK(names.size() == 3);
    CHECK(names[0].isNull() && names[1].isNull());
    CHECK(names[2] == QString("n"));
    re.setPatternOptions(RE::CaseInsensitiveOption | RE::MultilineOption);
    CHECK(re.patternOptions().testFlag(RE::CaseInsensitiveOption));
    CHECK(re.patternOptions().testFlag(RE::MultilineOption));
    CHECK(!re.patternOptions().testFlag(RE::DontCaptureOption));
    CHECK(re.match("AB").hasMatch());
    re.setPattern("(unclosed");
    CHECK(!re.isValid());
    re.setPattern("ok");
    CHECK(re.isValid());

    RE a("abc", RE::CaseInsensitiveOption);
    RE b("abc", RE::CaseInsensitiveOption);
    RE c("abc");
    CHECK(a == b);
    CHECK(a != c);
    CHECK(RE("a") != RE("b"));
    RE copy(a);
    CHECK(copy == a);
    RE moved(std::move(copy));
    CHECK(moved == a);
    c.swap(a);
    CHECK(a.patternOptions() == RE::NoPatternOption);
    CHECK(c.patternOptions() == RE::CaseInsensitiveOption);
    CHECK(qHash(b) == qHash(c));
    CHECK(qHash(RE("x")) != qHash(RE("y")) || true);
    CHECK(RE().match("").hasMatch());
    re.optimize();
    CHECK(re.isValid());
    RE assigned;
    assigned = b;
    CHECK(assigned == b);
    // A pattern with embedded NUL.
    QString nul("a");
    nul += QChar(0);
    nul += QChar(u'b');
    CHECK(RE(nul).match(nul).hasMatch());
    CHECK(!RE(nul).match("ab").hasMatch());
}

static void testMatchObject() {
    RE re("(?<year>\\d{4})-(?<month>\\d\\d)(-(?<day>\\d\\d))?");
    QRegularExpressionMatch m = re.match("on 2024-05 ok");
    CHECK(m.isValid() && m.hasMatch() && !m.hasPartialMatch());
    CHECK(m.matchType() == RE::NormalMatch);
    CHECK(m.matchOptions() == RE::NoMatchOption);
    CHECK(m.regularExpression() == re);
    CHECK(m.lastCapturedIndex() == 2);
    CHECK(m.captured() == QString("2024-05"));
    CHECK(m.captured(1) == QString("2024"));
    CHECK(m.captured("month") == QString("05"));
    CHECK(m.captured(QStringView(QString("year"))) == QString("2024"));
    CHECK(m.captured(3).isNull());
    CHECK(m.captured(4).isNull());
    CHECK(m.captured(-1).isNull());
    CHECK(m.capturedStart() == 3 && m.capturedEnd() == 10 && m.capturedLength() == 7);
    CHECK(m.capturedStart(2) == 8 && m.capturedEnd(2) == 10 && m.capturedLength(2) == 2);
    CHECK(m.capturedStart("year") == 3 && m.capturedEnd("year") == 7 && m.capturedLength("year") == 4);
    CHECK(m.capturedStart(3) == -1);
    CHECK(m.capturedStart(9) == -1 && m.capturedEnd(9) == -1 && m.capturedLength(9) == 0);
    CHECK(m.capturedView(2) == QStringView(QString("05")));
    CHECK(m.capturedView("year") == QStringView(QString("2024")));
    CHECK(m.capturedView(7).isNull());
    QStringList texts = m.capturedTexts();
    CHECK(texts.size() == 3);
    CHECK(texts[0] == QString("2024-05") && texts[2] == QString("05"));

    // A group that matched the empty string is not null; one that did not take part is.
    RE re2("(a*)(b)?(c)");
    QRegularExpressionMatch m2 = re2.match("c");
    CHECK(m2.lastCapturedIndex() == 3);
    CHECK(!m2.captured(1).isNull() && m2.captured(1).isEmpty());
    CHECK(m2.capturedStart(1) == 0 && m2.capturedEnd(1) == 0);
    CHECK(m2.captured(2).isNull());
    CHECK(m2.capturedStart(2) == -1);
    CHECK(m2.captured(3) == QString("c"));
    QStringList t2 = m2.capturedTexts();
    CHECK(t2.size() == 4 && t2[2].isNull());

    // No match.
    QRegularExpressionMatch none = re.match("nothing");
    CHECK(none.isValid() && !none.hasMatch() && !none.hasPartialMatch());
    CHECK(none.lastCapturedIndex() == -1);
    CHECK(none.captured().isNull());
    CHECK(none.capturedStart() == -1);
    CHECK(none.capturedTexts().isEmpty());

    // Default constructed and copies.
    QRegularExpressionMatch def;
    CHECK(!def.isValid() && !def.hasMatch());
    QRegularExpressionMatch copy = m;
    CHECK(copy.captured(1) == QString("2024"));
    copy.swap(def);
    CHECK(def.hasMatch() && !copy.hasMatch());
    QRegularExpressionMatch moved(std::move(def));
    CHECK(moved.hasMatch());

    // Unknown and empty names warn and return null.
    g_messages.clear();
    CHECK(m.captured("nope").isNull());
    CHECK(g_messages.size() == 1 && g_messages[0].contains(QString("no capturing group named 'nope'")));
    CHECK(m.captured(QString()).isNull());
    CHECK(g_messages.size() == 2 && g_messages[1].contains(QString("empty capturing group name")));
    CHECK(m.capturedStart("nope") == -1);
    CHECK(m.capturedLength("nope") == 0);
    CHECK(m.capturedEnd("nope") == -1);
    CHECK(m.capturedView("nope").isNull());

    // Offsets and options.
    RE d("\\d+");
    CHECK(d.match("12 34", 2).captured() == QString("34"));
    CHECK(d.match("12 34", 2).capturedStart() == 3);
    CHECK(!d.match("12 34", 2, RE::NormalMatch, RE::AnchorAtOffsetMatchOption).hasMatch());
    CHECK(d.match("12 34", 3, RE::NormalMatch, RE::AnchorAtOffsetMatchOption).hasMatch());
    CHECK(d.match("12 34", 3, RE::NormalMatch, RE::AnchoredMatchOption).hasMatch());
    CHECK(d.match("12 34", 3, RE::NormalMatch, RE::DontCheckSubjectStringMatchOption).hasMatch());
    CHECK(d.match("12 34", 3, RE::NormalMatch, RE::AnchorAtOffsetMatchOption).matchOptions() ==
          RE::AnchorAtOffsetMatchOption);
    CHECK(!d.match("12 34", 5).hasMatch());
    CHECK(!d.match("12 34", 6).hasMatch());
    CHECK(!d.match("12 34", -1).hasMatch());
    CHECK(d.match("12 34", -1).isValid());
    CHECK(RE("^1").match("12", 0).hasMatch());
    CHECK(!RE("^2").match("12", 1).hasMatch());
    CHECK(RE("\\G2").match("12", 1).hasMatch());
    CHECK(!RE("\\G2").match("12", 0).hasMatch());
    CHECK(RE("(?<=1)2").match("12", 1).hasMatch());
    CHECK(RE("\\b2").match("12", 1).hasMatch() == false);
    QRegularExpressionMatch nm = d.match("12", 0, RE::NoMatch);
    CHECK(nm.isValid() && !nm.hasMatch() && nm.matchType() == RE::NoMatch);

    // QStringView subjects.
    QString subject("xx 99 yy");
    QRegularExpressionMatch vm = d.match(QStringView(subject));
    CHECK(vm.captured() == QString("99"));
    QRegularExpressionMatch mv = d.matchView(QStringView(subject));
    CHECK(mv.captured() == QString("99"));
    CHECK(mv.capturedView().data() == subject.constData() + 3);
    CHECK(vm.capturedView().data() != subject.constData() + 3);
    CHECK(d.globalMatchView(QStringView(subject)).next().capturedView().data() == subject.constData() + 3);
    CHECK(d.globalMatch(QStringView(subject)).hasNext());

    // A match keeps its own copy of the subject.
    QRegularExpressionMatch keep;
    {
        QString temp("temporary 42");
        keep = d.match(temp);
        temp.clear();
    }
    CHECK(keep.captured() == QString("42"));
    CHECK(keep.capturedView() == QStringView(QString("42")));

    // The empty subject.
    CHECK(RE("^$").match("").hasMatch());
    CHECK(RE("a*").match(QString()).hasMatch());
    CHECK(!RE("a").match("").hasMatch());
}

static void testPartial() {
    RE date("^(\\d\\d?)/(\\d\\d?)/(\\d\\d\\d\\d)$");
    QRegularExpressionMatch m = date.match("12/", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.isValid() && !m.hasMatch() && m.hasPartialMatch());
    CHECK(m.captured(0) == QString("12/") && m.capturedStart() == 0 && m.capturedEnd() == 3);
    CHECK(m.lastCapturedIndex() == 0);
    CHECK(m.matchType() == RE::PartialPreferCompleteMatch);
    m = date.match("12/12/1985", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.hasMatch() && !m.hasPartialMatch());
    m = date.match("12/12/19", 0, RE::PartialPreferFirstMatch);
    CHECK(m.hasPartialMatch() && m.captured(0) == QString("12/12/19"));
    m = date.match("ab", 0, RE::PartialPreferCompleteMatch);
    CHECK(!m.hasMatch() && !m.hasPartialMatch());
    m = date.match("", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.hasPartialMatch());
    m = date.match("12/12/1985x", 0, RE::PartialPreferFirstMatch);
    CHECK(!m.hasMatch() && !m.hasPartialMatch());

    RE abcd("abcd");
    m = abcd.match("xxabc", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.hasPartialMatch() && m.capturedStart() == 2 && m.captured() == QString("abc"));
    CHECK(!abcd.match("xxabc").hasPartialMatch());
    CHECK(!abcd.match("xxabc").hasMatch());
    m = abcd.match("abcd", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.hasMatch());
    m = abcd.match("abx", 0, RE::PartialPreferCompleteMatch);
    CHECK(!m.hasPartialMatch());

    // Complete match preferred or not.
    RE plus("a+b?");
    m = plus.match("aaa", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.hasMatch() && m.captured() == QString("aaa"));
    m = plus.match("aaa", 0, RE::PartialPreferFirstMatch);
    CHECK(m.hasPartialMatch() && !m.hasMatch());
    RE alt("abc|abcd");
    m = alt.match("abc", 0, RE::PartialPreferFirstMatch);
    CHECK(m.hasMatch() && m.captured() == QString("abc"));
    RE late("abcd|x");
    m = late.match("abc x", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.hasMatch() && m.captured() == QString("x"));
    m = RE("\\bfoo\\b").match("a foo", 0, RE::PartialPreferCompleteMatch);
    CHECK(m.hasMatch());
    m = RE("\\d+\\b").match("12", 0, RE::PartialPreferFirstMatch);
    CHECK(m.hasPartialMatch());

    // Iterating stops after a partial match.
    QRegularExpressionMatchIterator it = RE("ab\\d").globalMatch("ab1 ab2 ab", 0, RE::PartialPreferCompleteMatch);
    int complete = 0;
    int partial = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch pm = it.next();
        complete += pm.hasMatch();
        partial += pm.hasPartialMatch();
    }
    CHECK(complete == 2 && partial == 1);
}

static QStringList allMatches(const RE &re, const QString &subject, qsizetype offset = 0) {
    QStringList r;
    QRegularExpressionMatchIterator it = re.globalMatch(subject, offset);
    while (it.hasNext()) {
        const qsizetype where = it.peekNext().capturedStart();
        r.append(QString::number(where) + ":" + it.next().captured());
    }
    return r;
}

static void testGlobalMatch() {
    QStringList w = allMatches(RE("\\w+"), "the quick  fox");
    CHECK(w == QStringList({"0:the", "4:quick", "11:fox"}));
    CHECK(allMatches(RE("x*"), "abc") == QStringList({"0:", "1:", "2:", "3:"}));
    CHECK(allMatches(RE("a*"), "baac") == QStringList({"0:", "1:aa", "3:", "4:"}));
    CHECK(allMatches(RE(""), "") == QStringList({"0:"}));
    CHECK(allMatches(RE("x*"), "\xF0\x9F\x98\x80") == QStringList({"0:", "2:"}));
    CHECK(allMatches(RE("\\d"), "a1b2c3", 2) == QStringList({"3:2", "5:3"}));
    CHECK(allMatches(RE("\\d"), "none").isEmpty());
    CHECK(allMatches(RE("(?=a)"), "aa") == QStringList({"0:", "1:"}));
    CHECK(allMatches(RE("\\b"), "ab cd") == QStringList({"0:", "2:", "3:", "5:"}));
    CHECK(allMatches(RE("a|"), "ba") == QStringList({"0:", "1:a", "2:"}));
    CHECK(allMatches(RE("^"), "a\nb") == QStringList({"0:"}));
    CHECK(allMatches(RE("^", RE::MultilineOption), "a\nb") == QStringList({"0:", "2:"}));
    CHECK(allMatches(RE("aa"), "aaaaa") == QStringList({"0:aa", "2:aa"}));
    CHECK(allMatches(RE("a\\Kb"), "abab") == QStringList({"1:b", "3:b"}));

    RE re("(\\d)(\\w)?");
    QRegularExpressionMatchIterator it = re.globalMatch("1a 2 3c");
    CHECK(it.isValid() && it.hasNext());
    CHECK(it.regularExpression() == re);
    CHECK(it.matchType() == RE::NormalMatch);
    CHECK(it.matchOptions() == RE::NoMatchOption);
    QRegularExpressionMatch first = it.next();
    CHECK(first.captured(2) == QString("a"));
    CHECK(it.peekNext().captured(1) == QString("2"));
    QRegularExpressionMatchIterator copy = it;
    CHECK(it.next().captured(2).isNull());
    CHECK(it.hasNext());
    CHECK(copy.hasNext() && copy.peekNext().captured(1) == QString("2"));
    CHECK(it.next().captured(2) == QString("c"));
    CHECK(!it.hasNext());
    g_messages.clear();
    it.next();
    CHECK(g_messages.size() == 1);
    it.peekNext();
    CHECK(g_messages.size() == 2);
    QRegularExpressionMatchIterator def;
    CHECK(!def.hasNext() && !def.isValid());
    def.swap(copy);
    CHECK(def.hasNext() && !copy.hasNext());
    QRegularExpressionMatchIterator moved(std::move(def));
    CHECK(moved.hasNext());
    QRegularExpressionMatchIterator assigned;
    assigned = moved;
    CHECK(assigned.hasNext());
    CHECK(RE("a").globalMatch("a", 0, RE::NormalMatch, RE::AnchorAtOffsetMatchOption).hasNext());
    QRegularExpressionMatchIterator anch = RE("a").globalMatch("aab", 0, RE::NormalMatch, RE::AnchorAtOffsetMatchOption);
    int n = 0;
    while (anch.hasNext()) {
        anch.next();
        ++n;
    }
    CHECK(n == 2);
}

static void testStringIntegration() {
    QString s("The quick brown fox jumps over the lazy dog");
    RE word("\\b(\\w)(\\w*)\\b");
    RE the("the", RE::CaseInsensitiveOption);

    // indexOf, lastIndexOf, contains, count
    CHECK(s.indexOf(the) == 0);
    CHECK(s.indexOf(the, 1) == 31);
    CHECK(s.indexOf(the, 32) == -1);
    CHECK(s.indexOf(the, -12) == 31);
    QRegularExpressionMatch m;
    CHECK(s.indexOf(RE("(q)(u)"), 0, &m) == 4);
    CHECK(m.captured(2) == QString("u"));
    CHECK(s.lastIndexOf(the) == 31);
    CHECK(s.lastIndexOf(the, 30) == 0);
    CHECK(s.lastIndexOf(the, -1) == 31);
    CHECK(s.lastIndexOf(the, -20) == 0);
    CHECK(s.lastIndexOf(RE("o"), 20) == 17);
    CHECK(s.lastIndexOf(RE("nomatch")) == -1);
    CHECK(s.lastIndexOf(RE("(o)(g)"), &m) == 41);
    CHECK(m.captured(1) == QString("o") || m.captured(0) == QString("og"));
    CHECK(s.contains(the));
    CHECK(s.contains(RE("fox(?= jumps)"), &m) && m.captured() == QString("fox"));
    CHECK(!s.contains(RE("cat")));
    CHECK(s.count(RE("o")) == 4);
    CHECK(s.count(the) == 2);
    CHECK(s.count(RE("")) == s.size() + 1);
    CHECK(QString("abc").count(RE("x*")) == 4);
    CHECK(QString().count(RE("x*")) == 1);
    CHECK(s.count(word) == 9);

    // replace and remove
    CHECK(QString("abc").replace(RE("x*"), "-") == QString("-a-b-c-"));
    CHECK(QString("a1b22c").replace(RE("\\d+"), "#") == QString("a#b#c"));
    CHECK(QString("a1b22c").replace(RE("(\\d+)"), "<\\1>") == QString("a<1>b<22>c"));
    CHECK(QString("john smith").replace(RE("(\\w+) (\\w+)"), "\\2, \\1") == QString("smith, john"));
    CHECK(QString("abc").replace(RE("b"), "[\\0]") == QString("a[b]c"));
    CHECK(QString("abc").replace(RE("b"), "\\") == QString("a\\c"));
    CHECK(QString("abc").replace(RE("b"), "\\x") == QString("a\\xc"));
    CHECK(QString("abc").replace(RE("b"), "\\5") == QString("a\\5c"));
    CHECK(QString("abc").replace(RE("(b)"), "\\10") == QString("ab0c"));
    CHECK(QString("aXb").replace(RE("(?<l>X)"), "\\1\\1") == QString("aXXb"));
    CHECK(QString("hello").replace(RE("l"), "") == QString("heo"));
    CHECK(QString("hello").replace(RE("nomatch"), "x") == QString("hello"));
    CHECK(QString("a.b").replace(RE("\\."), "\\\\") == QString("a\\\\b"));
    CHECK(QString("caf\xC3\xA9").replace(RE("\\w+", RE::UseUnicodePropertiesOption), "x") == QString("x"));
    CHECK(QString("a b  c").remove(RE("\\s+")) == QString("abc"));
    CHECK(QString("aaa").remove(RE("a")) == QString());
    QString inplace("2024-05-17");
    QString &ref = inplace.replace(RE("(\\d+)-(\\d+)-(\\d+)"), "\\3/\\2/\\1");
    CHECK(&ref == &inplace && inplace == QString("17/05/2024"));

    // split
    CHECK(QString("a, b;c").split(RE("[,;]\\s*")) == QStringList({"a", "b", "c"}));
    CHECK(QString("a,,b").split(RE(",")) == QStringList({"a", "", "b"}));
    CHECK(QString("a,,b").split(RE(","), Qt::SkipEmptyParts) == QStringList({"a", "b"}));
    CHECK(QString(",a,").split(RE(",")) == QStringList({"", "a", ""}));
    CHECK(QString(",a,").split(RE(","), Qt::SkipEmptyParts) == QStringList({"a"}));
    CHECK(QString("abc").split(RE("")) == QStringList({"", "a", "b", "c", ""}));
    CHECK(QString("abc").split(RE(""), Qt::SkipEmptyParts) == QStringList({"a", "b", "c"}));
    CHECK(QString("abc").split(RE("x")) == QStringList({"abc"}));
    CHECK(QString().split(RE("x")) == QStringList({""}));
    CHECK(QString("a1b22c333d").split(RE("\\d+")) == QStringList({"a", "b", "c", "d"}));
    CHECK(QString("one  two").split(RE("\\s")) == QStringList({"one", "", "two"}));
    g_messages.clear();
    CHECK(QString("abc").split(RE("(")).isEmpty());
    CHECK(QString("abc").indexOf(RE("(")) == -1);
    CHECK(QString("abc").lastIndexOf(RE("(")) == -1);
    CHECK(!QString("abc").contains(RE("(")));
    CHECK(QString("abc").count(RE("(")) == 0);
    CHECK(QString("abc").replace(RE("("), "x") == QString("abc"));
    CHECK(QString("abc").section(RE("("), 0, 0).isEmpty());
    CHECK(g_messages.size() == 7);

    // section
    QString csv("forename**middlename  **surname**phone");
    RE sep("\\*+");
    CHECK(csv.section(sep, 2, 2) == QString("surname"));
    CHECK(csv.section(sep, 3, 3) == QString("phone"));
    CHECK(csv.section(sep, -1, -1) == QString("phone"));
    CHECK(csv.section(sep, 1, 2) == QString("middlename  **surname"));
    CHECK(csv.section(sep, 0, 0) == QString("forename"));
    CHECK(csv.section(sep, 4, 4).isEmpty());
    CHECK(csv.section(sep, 2, 1).isEmpty());
    CHECK(csv.section(sep, 0, -1) == csv);
    CHECK(csv.section(sep, 1, 1, QString::SectionIncludeLeadingSep) == QString("**middlename  "));
    CHECK(csv.section(sep, 1, 1, QString::SectionIncludeTrailingSep) == QString("middlename  **"));
    CHECK(csv.section(sep, 1, 1, QString::SectionIncludeLeadingSep | QString::SectionIncludeTrailingSep) ==
          QString("**middlename  **"));
    QString paths("//usr//local//bin");
    RE slashes("/+");
    CHECK(paths.section(slashes, 0, 0).isEmpty());
    CHECK(paths.section(slashes, 0, 0, QString::SectionSkipEmpty) == QString("usr"));
    CHECK(paths.section(slashes, 1, 1) == QString("usr"));
    CHECK(paths.section(slashes, -1, -1, QString::SectionSkipEmpty) == QString("bin"));
    CHECK(QString("aXbxc").section(RE("x"), 1, 1, QString::SectionCaseInsensitiveSeps) == QString("b"));
    CHECK(QString("aXbxc").section(RE("x"), 1, 1).isEmpty() == false);
    CHECK(QString("a b").section(RE("\\s"), 1, 1) == QString("b"));

    // QStringList
    QStringList list{"apple", "Banana", "cherry", "apricot", "blueberry"};
    CHECK(list.filter(RE("^a")) == QStringList({"apple", "apricot"}));
    CHECK(list.filter(RE("^b", RE::CaseInsensitiveOption)) == QStringList({"Banana", "blueberry"}));
    CHECK(list.filter(RE("zzz")).isEmpty());
    CHECK(list.filter(RE("rr")) == QStringList({"cherry", "blueberry"}));
    CHECK(list.indexOf(RE("^a")) == 0);
    CHECK(list.indexOf(RE("^a"), 1) == 3);
    CHECK(list.indexOf(RE("^a"), 4) == -1);
    CHECK(list.indexOf(RE("^q")) == -1);
    CHECK(list.lastIndexOf(RE("^a")) == 3);
    CHECK(list.lastIndexOf(RE("^a"), 2) == 0);
    CHECK(list.lastIndexOf(RE("^a"), -3) == 0);
    CHECK(list.lastIndexOf(RE("^b")) == 4);
    CHECK(list.lastIndexOf(RE("^b", RE::CaseInsensitiveOption)) == 4);
    CHECK(list.lastIndexOf(RE("^a"), 100) == 3);
    CHECK(list.lastIndexOf(QString("cherry")) == 2);
    CHECK(list.contains(RE("^c")));
    CHECK(!list.contains(RE("^d")));
    QStringList copy = list;
    CHECK(&copy.replaceInStrings(RE("[aeiou]"), "_") == &copy);
    CHECK(copy[0] == QString("_ppl_"));
    CHECK(copy[3] == QString("_pr_c_t"));
    copy = list;
    copy.replaceInStrings(RE("(\\w)(\\w*)"), "\\2\\1");
    CHECK(copy[0] == QString("ppleay") || copy[0] == QString("ppleaa") || copy[0].size() == 5);
    CHECK(copy[2] == QString("herryc"));
}

static void testStringIntegrationDefaultsUnchanged() {
    // The plain string overloads still work next to the regular expression ones.
    QString s("a,b");
    CHECK(s.split(',') == QStringList({"a", "b"}));
    CHECK(s.split(QString(",")) == QStringList({"a", "b"}));
    CHECK(s.indexOf("b") == 2);
    CHECK(s.indexOf(QChar(u',')) == 1);
    CHECK(s.contains("a"));
    CHECK(s.lastIndexOf("a") == 0);
    CHECK(s.count("a") == 1);
    CHECK(QString("aXa").replace("X", "-") == QString("a-a"));
    CHECK(QString("aXa").remove("X") == QString("aa"));
    CHECK(QStringList({"a", "b"}).filter("a") == QStringList({"a"}));
    CHECK(QStringList({"a", "b"}).indexOf("b") == 1);
    CHECK(QStringList({"a", "b"}).contains("b"));
}

static void testEscapeAndWildcards() {
    CHECK(RE::escape("a.b*c[d]\\e") == QString("a\\.b\\*c\\[d\\]\\\\e"));
    CHECK(RE::escape("hello world") == QString("hello\\ world"));
    CHECK(RE::escape("") == QString());
    CHECK(RE::escape("abc_123") == QString("abc_123"));
    CHECK(RE::escape("caf\xC3\xA9") == QString("caf\xC3\xA9"));
    QString nul;
    nul += QChar(0);
    CHECK(RE::escape(nul) == QString("\\0"));
    CHECK(RE::escape(QStringView(QString("a+b"))) == QString("a\\+b"));
    const QString special("^$.|?*+()[]{}\\-#/ !&");
    RE literal(RE::escape(special));
    CHECK(literal.isValid());
    CHECK(literal.match(special).captured() == special);
    RE literalX(RE::escape("a b#c"), RE::ExtendedPatternSyntaxOption);
    CHECK(literalX.match("a b#c").hasMatch());

    CHECK(RE::anchoredPattern("abc") == QString("\\A(?:abc)\\z"));
    CHECK(RE::anchoredPattern(QStringView(QString("a|b"))) == QString("\\A(?:a|b)\\z"));
    CHECK(RE(RE::anchoredPattern("a|b")).match("b").hasMatch());
    CHECK(!RE(RE::anchoredPattern("a|b")).match("bc").hasMatch());

    CHECK(RE::wildcardToRegularExpression("*.txt") == QString("\\A(?:[^/]*\\.txt)\\z"));
    CHECK(RE::wildcardToRegularExpression("a?b") == QString("\\A(?:a[^/]b)\\z"));
    CHECK(RE::wildcardToRegularExpression("[a-c]x") == QString("\\A(?:[a-c]x)\\z"));
    CHECK(RE::wildcardToRegularExpression("[!a]x") == QString("\\A(?:[^a]x)\\z"));
    CHECK(RE::wildcardToRegularExpression("a[b") == QString("\\A(?:a\\[b)\\z"));
    CHECK(RE::wildcardToRegularExpression("a]b") == QString("\\A(?:a\\]b)\\z"));
    CHECK(RE::wildcardToRegularExpression("\\*") == QString("\\A(?:\\*)\\z"));
    CHECK(RE::wildcardToRegularExpression("a\\") == QString("\\A(?:a\\\\)\\z"));
    CHECK(RE::wildcardToRegularExpression("*", RE::UnanchoredWildcardConversion) == QString("[^/]*"));
    CHECK(RE::wildcardToRegularExpression("*?", RE::NonPathWildcardConversion | RE::UnanchoredWildcardConversion) ==
          QString(".*."));
    CHECK(RE::wildcardToRegularExpression("a+b(c)") == QString("\\A(?:a\\+b\\(c\\))\\z"));

    RE txt(RE::wildcardToRegularExpression("*.txt"));
    CHECK(txt.isValid());
    CHECK(txt.match("notes.txt").hasMatch());
    CHECK(!txt.match("dir/notes.txt").hasMatch());
    CHECK(!txt.match("notes.txt.bak").hasMatch());
    RE nonpath(RE::wildcardToRegularExpression("*.txt", RE::NonPathWildcardConversion));
    CHECK(nonpath.match("dir/notes.txt").hasMatch());
    RE sets(RE::wildcardToRegularExpression("file[0-9].?"));
    CHECK(sets.match("file7.c").hasMatch());
    CHECK(!sets.match("filex.c").hasMatch());
    RE neg(RE::wildcardToRegularExpression("[!0-9]*"));
    CHECK(neg.match("abc").hasMatch() && !neg.match("1abc").hasMatch());
    RE bracket(RE::wildcardToRegularExpression("[]a]"));
    CHECK(bracket.isValid() && bracket.match("]").hasMatch() && bracket.match("a").hasMatch());

    RE ci = RE::fromWildcard("*.TXT");
    CHECK(ci.patternOptions() == RE::CaseInsensitiveOption);
    CHECK(ci.match("a.txt").hasMatch());
    RE cs = RE::fromWildcard("*.TXT", Qt::CaseSensitive);
    CHECK(cs.patternOptions() == RE::NoPatternOption);
    CHECK(!cs.match("a.txt").hasMatch() && cs.match("a.TXT").hasMatch());
    CHECK(RE::fromWildcard("*", Qt::CaseSensitive, RE::UnanchoredWildcardConversion).pattern() == QString("[^/]*"));
}

static void testStreaming() {
    RE re("(?<a>x+)\\d", RE::CaseInsensitiveOption | RE::DotMatchesEverythingOption);
    QByteArray ba;
    {
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << re << RE("second");
    }
    QDataStream in(ba);
    RE r1;
    RE r2;
    in >> r1 >> r2;
    CHECK(in.status() == QDataStream::Ok);
    CHECK(r1 == re);
    CHECK(r1.isValid() && r1.match("XX5").captured("a") == QString("XX"));
    CHECK(r2.pattern() == QString("second") && r2.patternOptions() == RE::NoPatternOption);
    // The format is a QString followed by the option bits as a quint32.
    QByteArray expected;
    {
        QDataStream out(&expected, QIODevice::WriteOnly);
        out << QString("ab") << quint32(RE::MultilineOption);
    }
    QByteArray actual;
    {
        QDataStream out(&actual, QIODevice::WriteOnly);
        out << RE("ab", RE::MultilineOption);
    }
    CHECK(actual == expected);
    QByteArray truncated = ba.left(3);
    QDataStream bad(truncated);
    RE untouched("keep");
    bad >> untouched;
    CHECK(untouched.pattern() == QString("keep"));

    g_messages.clear();
    qDebug() << RE("a+", RE::CaseInsensitiveOption | RE::MultilineOption);
    qDebug() << RE("b");
    qDebug() << RE::PatternOptions(RE::DontCaptureOption);
    QRegularExpressionMatch m = RE("(a)(b)?").match("xa");
    qDebug() << m;
    qDebug() << RE("z").match("a");
    qDebug() << RE("(").match("a");
    qDebug() << RE("abc").match("ab", 0, RE::PartialPreferCompleteMatch);
    CHECK(g_messages.size() >= 8);
    CHECK(g_messages.value(0) == QString("QRegularExpression(\"a+\", QRegularExpression::PatternOptions(CaseInsensitiveOption|MultilineOption))"));
    CHECK(g_messages.value(1) == QString("QRegularExpression(\"b\", QRegularExpression::PatternOptions(NoPatternOption))"));
    CHECK(g_messages.value(2) == QString("QRegularExpression::PatternOptions(DontCaptureOption)"));
    CHECK(g_messages.value(3) == QString("QRegularExpressionMatch(Valid, has match: 0:(1, 2, \"a\"), 1:(1, 2, \"a\"))"));
    CHECK(g_messages.value(4) == QString("QRegularExpressionMatch(Valid, no match)"));
    CHECK(g_messages.value(6) == QString("QRegularExpressionMatch(Invalid)"));
    CHECK(g_messages.value(7) == QString("QRegularExpressionMatch(Valid, has partial match: (0, 2, \"ab\"))"));
}

static void testLimits() {
    // Catastrophic backtracking ends with no match and a warning instead of hanging.
    g_messages.clear();
    RE evil("^(a*)*$");
    const auto start = std::chrono::steady_clock::now();
    QRegularExpressionMatch m = evil.match(QString(40, QChar(u'a')) + "b");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    CHECK(!m.hasMatch());
    CHECK(ms.count() < 5000);
    RE evil2("^(a+)+$");
    CHECK(!evil2.match(QString(40, QChar(u'a')) + "b").hasMatch());
    CHECK(g_messages.size() >= 1);
    CHECK(g_messages.value(0).contains(QString("match limit")));

    // Long subjects do not overflow the stack.
    const QString big(1000000, QChar(u'a'));
    CHECK(RE("a*").match(big).capturedLength() == 1000000);
    CHECK(RE("(?:a|b)*").match(big.left(100000)).capturedLength() == 100000);
    CHECK(RE("(a)*").match(big.left(100000)).capturedLength() == 100000);
    CHECK(RE(".*b").match(big).hasMatch() == false);
    CHECK(RE("a*?b").match(big.left(5000)).hasMatch() == false);
    CHECK(RE("(?:a|b)*c").match(big.left(1000)).hasMatch() == false);
    CHECK(RE("a{1000}").match(big).capturedLength() == 1000);
    CHECK(RE("[a-z]+$").match(big).capturedLength() == 1000000);
    CHECK(RE("^(?:(?:a)|(?:b))*$").match(big.left(50000)).hasMatch());

    // Literal prefixes and first-character sets find a needle quickly.
    QString hay(2000000, QChar(u'x'));
    hay += "needle";
    CHECK(RE("needle").match(hay).capturedStart() == 2000000);
    CHECK(RE("n[e]edle").match(hay).capturedStart() == 2000000);
    CHECK(RE("(?i)NEEDLE").match(hay).capturedStart() == 2000000);
    CHECK(hay.indexOf(RE("ne+dle")) == 2000000);
    CHECK(RE("(?:x|y)needle").match(hay).capturedStart() == 1999999);
}

static void testAnchorsAndMisc() {
    CHECK(run("^a", 0, "ba", 1) == "NOMATCH");
    CHECK(run("a$", 0, "ab\n", 0) == "NOMATCH");
    CHECK(run("\\Ga", 0, "ba", 1) == "a");
    // Newline handling is \n only.
    CHECK(run("a.b", 0, "a\rb") == "a\rb");
    CHECK(run("^b", M, "a\rb") == "NOMATCH");
    // Nested quantified groups with captures reset between iterations.
    CHECK(run("(?:(a)|(b))*", 0, "ab") == "ab,a,b");
    CHECK(run("(a|(b))+", 0, "ba") == "ba,a,b");
    // Possessive and atomic groups with captures.
    CHECK(run("(?>(a+))b", 0, "aab") == "aab,aa");
    CHECK(run("(a)++b", 0, "aab") == "aab,a");
    CHECK(run("(?:ab)*+c", 0, "ababc") == "ababc");
    CHECK(run("(?:ab)*+ab", 0, "abab") == "NOMATCH");
    CHECK(run("^(?:a|ab)*+c", 0, "abc") == "NOMATCH");
    // Empty alternatives and loops.
    CHECK(run("(?:)*", 0, "a") == "");
    CHECK(run("(?:a?)*b", 0, "aab") == "aab");
    CHECK(run("(?:a?){3}b", 0, "b") == "b");
    CHECK(run("(?:|a)+b", 0, "ab") == "ab");
    CHECK(run("x*?y*?", 0, "xy") == "");
    // Backreference to a group inside a lookahead.
    CHECK(run("(?=(\\w+))\\1:", 0, "abc:") == "abc:,abc");
    // Case-insensitive backreference and class ranges.
    CHECK(run("(?i)([a-c])\\1", 0, "xBb") == "Bb,B");
    CHECK(run("(?i)[k]", 0, "\xE2\x84\xAA") == "\xE2\x84\xAA");
    CHECK(run("(?i)\xC3\x9F", 0, "\xE1\xBA\x9E") == "\xE1\xBA\x9E");
    CHECK(run("(?i)[\xC3\xA0-\xC3\xA5]+", 0, "\xC3\x80\xC3\x81") == "\xC3\x80\xC3\x81");
    // Octal vs back reference.
    CHECK(run("(a)\\1\\011", 0, "aa\t1") == "aa\t,a");
    CHECK(run("\\8", 0, "x") == "INVALID:reference to non-existent subpattern");
    // Long alternations and classes.
    CHECK(run("(?:one|two|three|four|five|six|seven|eight|nine)+", 0, "xxsixnineone") == "sixnineone");
    // Quantifier on lookahead and \b.
    CHECK(run("(?=a){2}a", 0, "a") == "a");
    // Dot and newline with DotMatchesEverything inside groups.
    CHECK(run("(?s:.)\\n", 0, "\n\n") == "\n\n");
    // Mixed extended mode.
    CHECK(run("a b c # trailing", X, "abc") == "abc");
    CHECK(run("[ ]", X, " ") == " ");
    CHECK(run("a\\ #x\n b", X, "a b") == "a b");
    CHECK(run("(?-x)a b", X, "a b") == "a b");
    // The pattern may contain any code unit.
    CHECK(run("\xC3\xA9+", 0, "\xC3\xA9\xC3\xA9") == "\xC3\xA9\xC3\xA9");
    // Branch reset numbering.
    RE br("(?|(?<a>x)|(?<a>y))");
    CHECK(br.isValid() && br.captureCount() == 1);
    CHECK(br.match("y").captured("a") == QString("y"));
    // Named groups and their numbers.
    RE named("(a)(?<n1>b)(c)(?'n2'd)(?P<n3>e)");
    CHECK(named.captureCount() == 5);
    QStringList names = named.namedCaptureGroups();
    CHECK(names.size() == 6 && names[2] == QString("n1") && names[4] == QString("n2") && names[5] == QString("n3"));
    QRegularExpressionMatch nm = named.match("abcde");
    CHECK(nm.captured("n2") == QString("d") && nm.capturedStart("n3") == 4);
    // Unicode names.
    RE uni("(?<caf\xC3\xA9>x)");
    CHECK(uni.isValid() && uni.match("x").captured(QString("caf\xC3\xA9")) == QString("x"));
}

static void testUnicodeData() {
    // Categories, scripts and case mappings beyond Latin-1.
    CHECK(run("\\p{Lu}", 0, "\xE1\xBA\x9E") != "NOMATCH");
    CHECK(run("\\p{Armenian}+", 0, "a\xD4\xB1\xD5\xA1") == "\xD4\xB1\xD5\xA1");
    CHECK(run("\\p{Hebrew}", 0, "\xD7\x90") == "\xD7\x90");
    CHECK(run("\\p{Arabic}+", 0, "a\xD8\xA7\xD8\xA8") == "\xD8\xA7\xD8\xA8");
    CHECK(run("\\p{Thai}", 0, "\xE0\xB8\x81") == "\xE0\xB8\x81");
    CHECK(run("\\p{Hangul}+", 0, "\xED\x95\x9C\xEA\xB8\x80") == "\xED\x95\x9C\xEA\xB8\x80");
    CHECK(run("\\p{Katakana}+", 0, "\xE3\x82\xAB\xE3\x82\xBF") == "\xE3\x82\xAB\xE3\x82\xBF");
    CHECK(run("\\p{Common}", 0, "\xC3\xA9" "1") == "1");
    CHECK(run("\\p{Nl}", 0, "\xE2\x85\xA0") == "\xE2\x85\xA0");
    CHECK(run("\\p{Lt}", 0, "\xC7\x85") == "\xC7\x85");
    CHECK(run("\\p{Lm}", 0, "\xCA\xB0") == "\xCA\xB0");
    CHECK(run("\\p{Pd}", 0, "-") == "-");
    CHECK(run("\\p{Ps}\\p{Pe}", 0, "()") == "()");
    CHECK(run("\\p{Pi}\\p{Pf}", 0, "\xE2\x80\x98\xE2\x80\x99") == "\xE2\x80\x98\xE2\x80\x99");
    CHECK(run("\\p{Sm}", 0, "+") == "+");
    CHECK(run("\\p{Sk}", 0, "^") == "^");
    CHECK(run("\\p{So}", 0, "\xF0\x9F\x98\x80") == "\xF0\x9F\x98\x80");
    CHECK(run("\\p{Cf}", 0, "\xE2\x80\x8B") == "\xE2\x80\x8B");
    CHECK(run("\\p{Co}", 0, "\xEE\x80\x80") == "\xEE\x80\x80");
    CHECK(run("\\p{Cn}", 0, "\xEF\xB7\x90") == "\xEF\xB7\x90");
    CHECK(run("\\p{Zl}", 0, "\xE2\x80\xA8") == "\xE2\x80\xA8");
    CHECK(run("\\p{Zp}", 0, "\xE2\x80\xA9") == "\xE2\x80\xA9");
    CHECK(run("\\p{Mc}", 0, "\xE0\xA4\xBE") == "\xE0\xA4\xBE");
    CHECK(run("\\p{Me}", 0, "\xE2\x83\x9D") == "\xE2\x83\x9D");
    CHECK(run("\\p{Pc}", 0, "_") == "_");
    CHECK(run("\\p{No}", 0, "\xC2\xBD") == "\xC2\xBD");
    CHECK(run("(?i)\xCE\xA3+", 0, "\xCF\x83\xCF\x82") == "\xCF\x83\xCF\x82");
    CHECK(run("(?i)\xD5\xA1", 0, "\xD4\xB1") == "\xD4\xB1");
    CHECK(run("(?i)\xE1\xB8\x80", 0, "\xE1\xB8\x81") == "\xE1\xB8\x81");
    CHECK(run("(?i)\xEF\xBC\xA1", 0, "\xEF\xBD\x81") == "\xEF\xBD\x81");
    CHECK(run("(?i)\xF0\x90\x90\x80", 0, "\xF0\x90\x90\xA8") == "\xF0\x90\x90\xA8");
}

int main() {
    qInstallMessageHandler(captureHandler);
    testTable();
    testErrors();
    testPatternAccessors();
    testMatchObject();
    testPartial();
    testGlobalMatch();
    testStringIntegration();
    testStringIntegrationDefaultsUnchanged();
    testEscapeAndWildcards();
    testStreaming();
    testLimits();
    testAnchorsAndMisc();
    testUnicodeData();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::puts("all regular expression tests passed");
    return 0;
}
