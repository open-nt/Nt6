// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qtemporarydir.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

using namespace Qt::StringLiterals;

// Returns captured(0) of the first match, or "<none>".
static QString first(const QString &pattern, const QString &subject, QRegularExpression::PatternOptions o = {}) {
    const QRegularExpressionMatch m = QRegularExpression(pattern, o).match(subject);
    return m.hasMatch() ? m.captured() : u"<none>"_s;
}

static QStringList all(const QString &pattern, const QString &subject) {
    QStringList r;
    for (const QRegularExpressionMatch &m : QRegularExpression(pattern).globalMatch(subject))
        r << m.captured();
    return r;
}

static void testRegexBasics() {
    CHECK(first(u"b+"_s, u"abbbc"_s) == u"bbb"_s);
    CHECK(first(u"b+?"_s, u"abbbc"_s) == u"b"_s);
    CHECK(first(u"a.c"_s, u"xa\ncabc"_s) == u"abc"_s);
    CHECK(first(u"a.c"_s, u"a\nc"_s, QRegularExpression::DotMatchesEverythingOption) == u"a\nc"_s);
    CHECK(first(u"(?s)a.c"_s, u"a\nc"_s) == u"a\nc"_s);
    CHECK(first(u"colou?r"_s, u"the color red"_s) == u"color"_s);
    CHECK(first(u"\\d{2,3}"_s, u"a1b2345"_s) == u"234"_s);
    CHECK(first(u"\\d{2,}"_s, u"12345"_s) == u"12345"_s);
    CHECK(first(u"x{,2}y"_s, u"xxxy"_s) == u"xxy"_s);
    CHECK(first(u"a{2}"_s, u"aaa"_s) == u"aa"_s);
    CHECK(first(u"a{"_s, u"a{"_s) == u"a{"_s);   // not a quantifier: a literal brace
    CHECK(first(u"cat|dog"_s, u"hotdog"_s) == u"dog"_s);
    CHECK(first(u"[a-c]+"_s, u"xxabcabd"_s) == u"abcab"_s);
    CHECK(first(u"[^a-c]+"_s, u"abcxyz"_s) == u"xyz"_s);
    CHECK(first(u"[]a]+"_s, u"x]a]"_s) == u"]a]"_s);
    CHECK(first(u"[a-]+"_s, u"x-a-"_s) == u"-a-"_s);
    CHECK(first(u"[[:digit:][:upper:]]+"_s, u"abC9Dx"_s) == u"C9D"_s);
    CHECK(first(u"[[:^alpha:]]+"_s, u"ab12cd"_s) == u"12"_s);
    CHECK(first(u"\\w+"_s, u"  hello_1 "_s) == u"hello_1"_s);
    CHECK(first(u"\\s+"_s, u"a \t b"_s) == u" \t "_s);
    CHECK(first(u"\\bis\\b"_s, u"this is"_s) == u"is"_s);
    CHECK(QRegularExpression(u"\\bis\\b"_s).match(u"this is"_s).capturedStart() == 5);
    CHECK(first(u"\\Bis"_s, u"this is"_s) == u"is"_s);
    CHECK(QRegularExpression(u"\\Bis"_s).match(u"this is"_s).capturedStart() == 2);
    CHECK(first(u"HELLO"_s, u"say hello"_s, QRegularExpression::CaseInsensitiveOption) == u"hello"_s);
    CHECK(first(u"(?i)straße"_s, u"STRASSE Straße"_s) == u"Straße"_s);
    CHECK(first(u"(?i:a)b"_s, u"Ab AB"_s) == u"Ab"_s);
    CHECK(first(u"[a-z]+"_s, u"ÄBC"_s, QRegularExpression::CaseInsensitiveOption) == u"BC"_s);
    CHECK(first(u"\\x41\\x{42}\\103"_s, u"ABC"_s) == u"ABC"_s); // octal: there are not 103 groups
    CHECK(first(u"\\x41\\x{42}\\0103"_s, u"AB\u00083"_s) == u"AB\u00083"_s);
    CHECK(first(u"[\\101-\\103]+"_s, u"xABCD"_s) == u"ABC"_s);
    CHECK(first(u"\\Qa.b\\E+"_s, u"a.bbb axb"_s) == u"a.bbb"_s);
    CHECK(first(u"a\\.b"_s, u"axb a.b"_s) == u"a.b"_s);
    CHECK(first(u"\\t\\n"_s, u"x\t\ny"_s) == u"\t\n"_s);
    CHECK(first(u"a b # comment\n c"_s, u"abc"_s, QRegularExpression::ExtendedPatternSyntaxOption) == u"abc"_s);
    CHECK(first(u"(?x) a [ ] b"_s, u"a b"_s) == u"a b"_s);
    CHECK(first(u"a(?#comment)b"_s, u"ab"_s) == u"ab"_s);
    CHECK(first(u"\\N+"_s, u"ab\ncd"_s, QRegularExpression::DotMatchesEverythingOption) == u"ab"_s);
    CHECK(first(u"a\\Rb"_s, u"a\r\nb"_s) == u"a\r\nb"_s);
    CHECK(first(u"\\h+"_s, u"a \t\u00a0b"_s) == u" \t\u00a0"_s);
    CHECK(first(u"\\X"_s, u"e\u0301x"_s) == u"e\u0301"_s);
    CHECK(first(u""_s, u"abc"_s).isEmpty());
    // Characters outside the BMP are single characters.
    CHECK(first(u"a.b"_s, u"a\U0001F600b"_s) == u"a\U0001F600b"_s);
    CHECK(first(u"[\U0001F600-\U0001F64F]+"_s, u"x\U0001F600\U0001F601y"_s) == u"\U0001F600\U0001F601"_s);
    CHECK(first(u"^.{3}$"_s, u"a\U0001F600b"_s) == u"a\U0001F600b"_s);
}

static void testRegexGroupsAndAnchors() {
    QRegularExpression re(u"(\\d+)-(?<month>\\d+)(?:-(\\d+))?"_s);
    CHECK(re.isValid() && re.captureCount() == 3);
    CHECK((re.namedCaptureGroups() == QStringList{QString(), QString(), u"month"_s, QString()}));
    QRegularExpressionMatch m = re.match(u"on 2024-06"_s);
    CHECK(m.hasMatch() && m.captured(1) == u"2024"_s && m.captured(u"month"_s) == u"06"_s);
    CHECK(m.lastCapturedIndex() == 2 && !m.hasCaptured(3) && m.hasCaptured(u"month"_s));
    CHECK(m.captured(3).isNull() && m.capturedStart(3) == -1 && m.capturedLength(3) == 0 && m.capturedEnd(3) == -1);
    CHECK(m.capturedStart(0) == 3 && m.capturedEnd(0) == 10 && m.capturedStart(u"month"_s) == 8);
    CHECK((m.capturedTexts() == QStringList{u"2024-06"_s, u"2024"_s, u"06"_s}));
    CHECK(m.capturedView(1) == u"2024" && m.captured(u"nope"_s).isNull());
    m = re.match(u"2024-06-30"_s);
    CHECK(m.lastCapturedIndex() == 3 && m.captured(3) == u"30"_s);

    CHECK(first(u"(?P<x>a)(?P=x)"_s, u"aa"_s) == u"aa"_s);
    CHECK(first(u"(?'q'b)\\k'q'\\k<q>\\k{q}\\g{q}"_s, u"bbbbb"_s) == u"bbbbb"_s);
    CHECK(first(u"(a)(b)\\g{-1}\\g1\\2"_s, u"abbab"_s) == u"abbab"_s);
    CHECK(first(u"(?i)(a)\\1"_s, u"aA"_s) == u"aA"_s);
    CHECK(first(u"(a)|b\\1"_s, u"b"_s) == u"<none>"_s); // a reference to an unset group fails
    CHECK(QRegularExpression(u"(a)(?:b)(c)"_s).captureCount() == 2);
    CHECK(QRegularExpression(u"(a)(b)"_s, QRegularExpression::DontCaptureOption).captureCount() == 0);
    CHECK(QRegularExpression(u"(a)(?<n>b)"_s, QRegularExpression::DontCaptureOption).captureCount() == 1);
    // The last iteration of a repeated group is captured.
    m = QRegularExpression(u"(\\w)+"_s).match(u"abc"_s);
    CHECK(m.captured(1) == u"c"_s);

    CHECK(first(u"^b"_s, u"a\nb"_s) == u"<none>"_s);
    CHECK(first(u"^b"_s, u"a\nb"_s, QRegularExpression::MultilineOption) == u"b"_s);
    CHECK(first(u"a$"_s, u"a\nb"_s) == u"<none>"_s);
    CHECK(first(u"a$"_s, u"a\nb"_s, QRegularExpression::MultilineOption) == u"a"_s);
    CHECK(first(u"b$"_s, u"ab\n"_s) == u"b"_s);
    CHECK(first(u"b\\z"_s, u"ab\n"_s) == u"<none>"_s);
    CHECK(first(u"b\\Z"_s, u"ab\n"_s) == u"b"_s);
    CHECK(first(u"\\Ab"_s, u"ab"_s) == u"<none>"_s);
    CHECK(QRegularExpression(u"^b"_s).match(u"ab"_s, 1).hasMatch() == false);
    CHECK(QRegularExpression(u"\\Gb"_s).match(u"ab"_s, 1).hasMatch());
    CHECK(QRegularExpression(u"b"_s).match(u"abab"_s, -1).capturedStart() == 3);
    CHECK(!QRegularExpression(u"b"_s).match(u"ab"_s, 0, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption).hasMatch());
    CHECK(QRegularExpression(u"b"_s).match(u"ab"_s, 1, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption).hasMatch());

    // Lookaround, atomic groups, possessive quantifiers, \K.
    CHECK(first(u"\\w+(?=;)"_s, u"ab cd;"_s) == u"cd"_s);
    CHECK(first(u"\\b\\w+\\b(?!;)"_s, u"ab; cd"_s) == u"cd"_s);
    CHECK(first(u"(?<=\\$)\\d+"_s, u"5 $42"_s) == u"42"_s);
    CHECK(first(u"(?<!\\$)\\b\\d+"_s, u"$5 42"_s) == u"42"_s);
    CHECK(first(u"(?<=ab|x)c"_s, u"xc"_s) == u"c"_s);
    CHECK(first(u"(?<=\U0001F600)x"_s, u"\U0001F600x"_s) == u"x"_s);
    CHECK(first(u"(?>a+)b"_s, u"aaab"_s) == u"aaab"_s);
    CHECK(first(u"(?>a+)ab"_s, u"aaab"_s) == u"<none>"_s);
    CHECK(first(u"a++ab"_s, u"aaab"_s) == u"<none>"_s);
    CHECK(first(u"a*+b"_s, u"aaab"_s) == u"aaab"_s);
    CHECK(first(u"(?:ab)++c"_s, u"ababc"_s) == u"ababc"_s);
    CHECK(first(u"foo\\Kbar"_s, u"foobar"_s) == u"bar"_s);
    CHECK(QRegularExpression(u"foo\\Kbar"_s).match(u"foobar"_s).capturedStart() == 3);
    CHECK(first(u"(?U)a+"_s, u"aaa"_s) == u"a"_s);
    CHECK(first(u"a+?"_s, u"aaa"_s, QRegularExpression::InvertedGreedinessOption) == u"aaa"_s);
    // Backtracking into groups and alternations.
    CHECK(first(u"(a|ab)(c|bcd)(d*)"_s, u"abcd"_s) == u"abcd"_s);
    CHECK(first(u"^(\\w+)\\s+\\1$"_s, u"hello hello"_s) == u"hello hello"_s);
    CHECK(first(u"(x+x+)+y"_s, u"xxxxxxxxxxy"_s) == u"xxxxxxxxxxy"_s);
    CHECK(first(u"(?:a|b)*c"_s, u"ababbac"_s) == u"ababbac"_s);
    CHECK(first(u"(a*)*b"_s, u"aab"_s) == u"aab"_s);
    CHECK(first(u"(a?){3}"_s, u"aa"_s) == u"aa"_s);
}

static void testRegexUnicode() {
    const auto ucp = QRegularExpression::UseUnicodePropertiesOption;
    CHECK(first(u"\\w+"_s, u"çà va"_s) == u"va"_s);
    CHECK(first(u"\\w+"_s, u"çà va"_s, ucp) == u"çà"_s);
    CHECK(first(u"\\d+"_s, u"x٣٤y"_s) == u"<none>"_s);
    CHECK(first(u"\\d+"_s, u"x٣٤y"_s, ucp) == u"٣٤"_s);
    CHECK(first(u"\\p{Lu}+"_s, u"abÄÖc"_s) == u"ÄÖ"_s);
    CHECK(first(u"\\p{L}+"_s, u"12日本語!"_s) == u"日本語"_s);
    CHECK(first(u"\\pN+"_s, u"ab½3c"_s) == u"½3"_s);
    CHECK(first(u"\\P{L}+"_s, u"ab12!c"_s) == u"12!"_s);
    CHECK(first(u"\\p{^L}+"_s, u"ab12!c"_s) == u"12!"_s);
    CHECK(first(u"[\\p{Sc}\\d]+"_s, u"cost €12"_s) == u"€12"_s);
    CHECK(first(u"\\p{Any}"_s, u"\n"_s) == u"\n"_s);
    CHECK(first(u"[[:alpha:]]+"_s, u"éa"_s) == u"a"_s);
    CHECK(first(u"[[:alpha:]]+"_s, u"éa"_s, ucp) == u"éa"_s);
    CHECK(first(u"\\bé"_s, u"aé é"_s, ucp) == u"é"_s);
    CHECK(QRegularExpression(u"\\bé"_s, ucp).match(u"aé é"_s).capturedStart() == 3);
    CHECK(first(u"ΣΊΣΥΦΟΣ"_s, u"σίσυφοσ"_s, QRegularExpression::CaseInsensitiveOption) == u"σίσυφοσ"_s);
    CHECK(first(u"ж+"_s, u"ЖжЖ"_s, QRegularExpression::CaseInsensitiveOption) == u"ЖжЖ"_s);
}

static void testRegexErrors() {
    struct Bad {
        QString pattern;
        const char *message;
        qsizetype offset;
    };
    const Bad bad[] = {
        {u"(ab"_s, "missing closing parenthesis", 3},
        {u"ab)"_s, "unmatched closing parenthesis", 2},
        {u"*a"_s, "quantifier does not follow a repeatable item", 0},
        {u"a**"_s, "quantifier does not follow a repeatable item", 2},
        {u"[ab"_s, "missing terminating ] for character class", 0},
        {u"[z-a]"_s, "range out of order in character class", 4},
        {u"ab\\"_s, "\\ at end of pattern", 3},
        {u"\\1(a)\\2"_s, "reference to non-existent subpattern", 5},
        {u"(?<=a+)b"_s, "lookbehind assertion is not fixed length", 0},
        {u"a{3,2}"_s, "numbers out of order in {} quantifier", 5},
        {u"(?<n>a)(?<n>b)"_s, "two named subpatterns have the same name (PCRE2_DUPNAMES not set)", 10},
        {u"\\p{Nope}"_s, "unknown property after \\P or \\p", 8},
        {u"[[:nope:]]"_s, "unknown POSIX class name", 1},
        {u"(?z)"_s, "unrecognized character after (? or (?-", 2},
        {u"\\k<missing>"_s, "reference to non-existent subpattern", 3},
        {u"\\y"_s, "unrecognized character follows \\", 1},
    };
    for (const Bad &b : bad) {
        const QRegularExpression re(b.pattern);
        if (re.isValid() || re.errorString() != QString::fromUtf8(b.message) || re.patternErrorOffset() != b.offset) {
            std::printf("  pattern %s: valid=%d error='%s' offset=%lld\n", b.pattern.toUtf8().constData(), re.isValid(),
                        re.errorString().toUtf8().constData(), static_cast<long long>(re.patternErrorOffset()));
            CHECK(!"unexpected result for an invalid pattern");
        }
        CHECK(re.captureCount() == -1 && re.namedCaptureGroups().isEmpty());
    }
    const QRegularExpression good(u"a(b)"_s);
    CHECK(good.isValid() && good.errorString() == u"no error"_s && good.patternErrorOffset() == -1);
    CHECK(!QRegularExpression(u"(?R)"_s).isValid() && !QRegularExpression(u"(?(1)a|b)"_s).isValid());
    CHECK(!QRegularExpression(QString(300, u'(') + QString(300, u')')).isValid());
    const QRegularExpressionMatch m = QRegularExpression(u"("_s).match(u"x"_s);
    CHECK(!m.isValid() && !m.hasMatch() && m.lastCapturedIndex() == -1);
}

static void testRegexPartialAndGlobal() {
    const QRegularExpression date(u"\\d\\d \\w{3} \\d{4}"_s);
    QRegularExpressionMatch m = date.match(u"due 21 Jan 20"_s, 0, QRegularExpression::PartialPreferCompleteMatch);
    CHECK(!m.hasMatch() && m.hasPartialMatch() && m.captured() == u"21 Jan 20"_s && m.capturedStart() == 4);
    CHECK(m.matchType() == QRegularExpression::PartialPreferCompleteMatch);
    m = date.match(u"due 21 Jan 20"_s);
    CHECK(!m.hasMatch() && !m.hasPartialMatch());
    // A complete match later in the subject wins over an earlier partial one...
    m = QRegularExpression(u"abc|ab"_s).match(u"xab"_s, 0, QRegularExpression::PartialPreferCompleteMatch);
    CHECK(m.hasMatch() && m.captured() == u"ab"_s);
    // ...unless the first match is preferred.
    m = QRegularExpression(u"abc|ab"_s).match(u"xab"_s, 0, QRegularExpression::PartialPreferFirstMatch);
    CHECK(m.hasPartialMatch() && m.captured() == u"ab"_s);
    m = QRegularExpression(u"abc"_s).match(u"abc"_s, 0, QRegularExpression::PartialPreferFirstMatch);
    CHECK(m.hasMatch());
    m = QRegularExpression(u"abc"_s).match(u"abc"_s, 0, QRegularExpression::NoMatch);
    CHECK(m.isValid() && !m.hasMatch() && m.matchType() == QRegularExpression::NoMatch);

    CHECK((all(u"\\d+"_s, u"a1b22c333"_s) == QStringList{u"1"_s, u"22"_s, u"333"_s}));
    CHECK((all(u"x*"_s, u"axb"_s) == QStringList{u""_s, u"x"_s, u""_s, u""_s}));
    CHECK((all(u""_s, u"a\U0001F600"_s) == QStringList{u""_s, u""_s, u""_s}));
    CHECK((all(u"\\b"_s, u"ab cd"_s).size() == 4));
    QRegularExpressionMatchIterator it = QRegularExpression(u"\\w+"_s).globalMatch(u"one two"_s);
    CHECK(it.isValid() && it.hasNext() && it.peekNext().captured() == u"one"_s);
    QRegularExpressionMatchIterator copy = it;
    CHECK(it.next().captured() == u"one"_s && it.next().captured() == u"two"_s && !it.hasNext());
    CHECK(copy.hasNext() && copy.next().captured() == u"one"_s); // copies advance independently
    CHECK(QRegularExpression(u"\\w+"_s).globalMatch(u"a b c"_s, 2).next().captured() == u"b"_s);
    CHECK(!QRegularExpression(u"("_s).globalMatch(u"x"_s).isValid());
    CHECK(!QRegularExpressionMatchIterator().hasNext());
}

static void testRegexStatics() {
    CHECK(QRegularExpression::escape(u"a.b*c_1 (x)"_s) == u"a\\.b\\*c_1\\ \\(x\\)"_s);
    CHECK(QRegularExpression::escape(QString(QChar(0))) == u"\\0"_s);
    CHECK(QRegularExpression(QRegularExpression::escape(u"[a+b]? \U0001F600"_s)).match(u"x[a+b]? \U0001F600"_s).capturedStart() == 1);
    CHECK(QRegularExpression::anchoredPattern(u"a|b"_s) == u"\\A(?:a|b)\\z"_s);
    CHECK(QRegularExpression::wildcardToRegularExpression(u"*.txt"_s) == u"\\A(?:[^/]*\\.txt)\\z"_s);
    const QRegularExpression wc = QRegularExpression::fromWildcard(u"*.TXT"_s);
    CHECK(wc.match(u"notes.txt"_s).hasMatch() && !wc.match(u"dir/notes.txt"_s).hasMatch() && !wc.match(u"notes.txt.bak"_s).hasMatch());
    CHECK(QRegularExpression::fromWildcard(u"*.txt"_s, Qt::CaseInsensitive, QRegularExpression::NonPathWildcardConversion)
              .match(u"dir/notes.txt"_s)
              .hasMatch());
    CHECK(!QRegularExpression::fromWildcard(u"*.TXT"_s, Qt::CaseSensitive).match(u"notes.txt"_s).hasMatch());
    CHECK(QRegularExpression::fromWildcard(u"file?.[ch]"_s).match(u"file1.c"_s).hasMatch());
    CHECK(!QRegularExpression::fromWildcard(u"file?.[!ch]"_s).match(u"file1.c"_s).hasMatch());
    CHECK(QRegularExpression::fromWildcard(u"[]]"_s).match(u"]"_s).hasMatch());
    CHECK(QRegularExpression::fromWildcard(u"a[b"_s).match(u"a[b"_s).hasMatch());
    CHECK(QRegularExpression::wildcardToRegularExpression(u"a*"_s, QRegularExpression::UnanchoredWildcardConversion) == u"a[^/]*"_s);

    QRegularExpression a(u"x+"_s), b(u"x+"_s), c(u"x+"_s, QRegularExpression::CaseInsensitiveOption);
    CHECK(a == b && a != c && qHash(a) == qHash(b));
    b.setPattern(u"y"_s);
    CHECK(a != b && a.pattern() == u"x+"_s && b.pattern() == u"y"_s);
    c.setPatternOptions(QRegularExpression::NoPatternOption);
    CHECK(a == c && c.patternOptions() == QRegularExpression::NoPatternOption);
    a.swap(b);
    CHECK(a.pattern() == u"y"_s);
    a.optimize();
    QRegularExpression empty;
    CHECK(empty.isValid() && empty.pattern().isEmpty() && empty.match(u"abc"_s).hasMatch());
}

static void testRegexStringApi() {
    const QString s = u"one 22 three 4444"_s;
    QRegularExpressionMatch m;
    CHECK(s.contains(QRegularExpression(u"\\d+"_s), &m) && m.captured() == u"22"_s);
    CHECK(s.indexOf(QRegularExpression(u"\\d+"_s)) == 4 && s.indexOf(QRegularExpression(u"\\d+"_s), 6) == 13);
    CHECK(s.indexOf(QRegularExpression(u"z"_s)) == -1);
    CHECK(s.lastIndexOf(QRegularExpression(u"\\d+"_s), &m) == 16 && m.captured() == u"4"_s);
    CHECK(s.lastIndexOf(QRegularExpression(u"\\d+"_s), 10) == 5);
    CHECK(s.count(QRegularExpression(u"\\d\\d"_s)) == 4); // overlapping
    QString r = s;
    r.replace(QRegularExpression(u"(\\w)(\\w*)"_s), u"\\2\\1"_s);
    CHECK(r == u"neo 22 hreet 4444"_s);
    r = u"a1b2"_s;
    CHECK(r.replace(QRegularExpression(u"\\d"_s), u"<\\0\\\\>"_s) == u"a<1\\\\>b<2\\\\>"_s); // "\\\\" stays as it is
    r = u"abc"_s;
    CHECK(r.replace(QRegularExpression(u"(a)(b)(c)"_s), u"\\3\\21\\12"_s) == u"cb1a2"_s);
    r = u"x  y   z"_s;
    CHECK(r.remove(QRegularExpression(u"\\s+"_s)) == u"xyz"_s);
    CHECK((u"a, b,,c"_s.split(QRegularExpression(u",\\s*"_s)) == QStringList{u"a"_s, u"b"_s, u""_s, u"c"_s}));
    CHECK((u"a, b,,c"_s.split(QRegularExpression(u",\\s*"_s), Qt::SkipEmptyParts) == QStringList{u"a"_s, u"b"_s, u"c"_s}));
    CHECK((u"abc"_s.split(QRegularExpression(u""_s)) == QStringList{u""_s, u"a"_s, u"b"_s, u"c"_s, u""_s}));

    const QStringView v(s);
    CHECK(v.contains(QRegularExpression(u"three"_s)) && v.indexOf(QRegularExpression(u"t"_s)) == 7);
    CHECK(v.lastIndexOf(QRegularExpression(u"e"_s)) == 11 && v.lastIndexOf(QRegularExpression(u"e"_s), 10) == 10 && v.count(QRegularExpression(u"e"_s)) == 3);

    QStringList list{u"apple"_s, u"banana"_s, u"cherry"_s, u"apple pie"_s};
    CHECK((list.filter(QRegularExpression(u"^a"_s)) == QStringList{u"apple"_s, u"apple pie"_s}));
    CHECK(list.indexOf(QRegularExpression(u"apple"_s)) == 0 && list.indexOf(QRegularExpression(u"apple"_s), 1) == -1);
    CHECK(list.indexOf(QRegularExpression(u"apple.*"_s), 1) == 3 && list.lastIndexOf(QRegularExpression(u"a.*"_s)) == 3);
    CHECK(list.lastIndexOf(QRegularExpression(u"a.*"_s), 2) == 0 && list.lastIndexOf(u"banana"_s) == 1);
    list.replaceInStrings(QRegularExpression(u"a"_s), u"4"_s);
    CHECK(list.at(1) == u"b4n4n4"_s);
}

static void testRegexLimits() {
    // Single-character loops do not recurse, so long subjects are fine.
    const QString longText(200000, u'a');
    CHECK(QRegularExpression(u"^a*$"_s).match(longText).hasMatch());
    CHECK(QRegularExpression(u"^.*b"_s).match(longText + u"b"_s).capturedLength() == 200001);
    CHECK(QRegularExpression(u"(?:ab)+"_s).match(QString(u"ab"_s).repeated(500)).capturedLength() == 1000);
    // Too much recursion fails cleanly instead of crashing.
    const QRegularExpressionMatch m = QRegularExpression(u"(?:a|b)+$"_s).match(QString(400000, u'a'));
    CHECK(m.isValid());
}

static const uchar kGermanQm[] = {
    0x3c, 0xb8, 0x64, 0x18, 0xca, 0xef, 0x9c, 0x95, 0xcd, 0x21, 0x1c, 0xbf, 0x60, 0xa1, 0xbd, 0xdd, 0xa7, 0x00, 0x00,
    0x00, 0x05, 0x64, 0x65, 0x5f, 0x44, 0x45, 0x42, 0x00, 0x00, 0x00, 0x20, 0x00, 0x05, 0x66, 0xbe, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x05, 0x66, 0xbe, 0x00, 0x00, 0x00, 0xb0, 0x00, 0x4a, 0x36, 0x95, 0x00, 0x00, 0x00, 0x7e, 0x0d, 0x00,
    0xbf, 0x39, 0x00, 0x00, 0x00, 0x30, 0x69, 0x00, 0x00, 0x00, 0xdb, 0x03, 0x00, 0x00, 0x00, 0x12, 0x00, 0x41, 0x00,
    0x75, 0x00, 0x66, 0x00, 0x6d, 0x00, 0x61, 0x00, 0x63, 0x00, 0x68, 0x00, 0x65, 0x00, 0x6e, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x4f, 0x70, 0x65, 0x6e, 0x07, 0x00, 0x00, 0x00, 0x05, 0x4f, 0x74, 0x68, 0x65,
    0x72, 0x01, 0x03, 0x00, 0x00, 0x00, 0x10, 0x00, 0x25, 0x00, 0x6e, 0x00, 0x20, 0x00, 0x44, 0x00, 0x61, 0x00, 0x74,
    0x00, 0x65, 0x00, 0x69, 0x03, 0x00, 0x00, 0x00, 0x14, 0x00, 0x25, 0x00, 0x6e, 0x00, 0x20, 0x00, 0x44, 0x00, 0x61,
    0x00, 0x74, 0x00, 0x65, 0x00, 0x69, 0x00, 0x65, 0x00, 0x6e, 0x08, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x0a, 0x25, 0x6e, 0x20, 0x66, 0x69, 0x6c, 0x65, 0x28, 0x73, 0x29, 0x07, 0x00, 0x00, 0x00, 0x06, 0x57, 0x69, 0x6e,
    0x64, 0x6f, 0x77, 0x01, 0x03, 0x00, 0x00, 0x00, 0x12, 0x00, 0x53, 0x00, 0x63, 0x00, 0x68, 0x00, 0x6c, 0x00, 0x69,
    0x00, 0x65, 0x00, 0xdf, 0x00, 0x65, 0x00, 0x6e, 0x08, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x43,
    0x6c, 0x6f, 0x73, 0x65, 0x07, 0x00, 0x00, 0x00, 0x06, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x01, 0x03, 0x00, 0x00,
    0x00, 0x0c, 0x00, 0xd6, 0x00, 0x66, 0x00, 0x66, 0x00, 0x6e, 0x00, 0x65, 0x00, 0x6e, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x04, 0x4f, 0x70, 0x65, 0x6e, 0x07, 0x00, 0x00, 0x00, 0x06, 0x57, 0x69, 0x6e, 0x64, 0x6f,
    0x77, 0x01, 0x88, 0x00, 0x00, 0x00, 0x02, 0x01, 0x01};
static const uchar kPolishQm[] = {
    0x3c, 0xb8, 0x64, 0x18, 0xca, 0xef, 0x9c, 0x95, 0xcd, 0x21, 0x1c, 0xbf, 0x60, 0xa1, 0xbd, 0xdd, 0xa7, 0x00,
    0x00, 0x00, 0x05, 0x70, 0x6c, 0x5f, 0x50, 0x4c, 0x42, 0x00, 0x00, 0x00, 0x08, 0x0d, 0x00, 0xbf, 0x39, 0x00,
    0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x5f, 0x03, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x25, 0x00, 0x6e, 0x00,
    0x20, 0x00, 0x70, 0x00, 0x6c, 0x00, 0x69, 0x00, 0x6b, 0x03, 0x00, 0x00, 0x00, 0x10, 0x00, 0x25, 0x00, 0x6e,
    0x00, 0x20, 0x00, 0x70, 0x00, 0x6c, 0x00, 0x69, 0x00, 0x6b, 0x00, 0x69, 0x03, 0x00, 0x00, 0x00, 0x12, 0x00,
    0x25, 0x00, 0x6e, 0x00, 0x20, 0x00, 0x70, 0x00, 0x6c, 0x00, 0x69, 0x00, 0x6b, 0x00, 0xf3, 0x00, 0x77, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0a, 0x25, 0x6e, 0x20, 0x66, 0x69, 0x6c, 0x65, 0x28, 0x73,
    0x29, 0x07, 0x00, 0x00, 0x00, 0x06, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x01, 0x88, 0x00, 0x00, 0x00, 0x0a,
    0x01, 0x01, 0xff, 0x14, 0x02, 0x04, 0xfd, 0x2c, 0x0a, 0x13};

// ------------------------------------------------------------------ views, matcher, converters, tokenizer

static void testMatcherAndViews() {
    QStringMatcher m(u"needle"_s);
    CHECK(m.indexIn(u"haystack with a needle in it"_s) == 16 && m.indexIn(u"no"_s) == -1);
    CHECK(m.indexIn(u"needle needle"_s, 1) == 7 && m.indexIn(QStringView(u"xneedle")) == 1);
    m.setCaseSensitivity(Qt::CaseInsensitive);
    CHECK(m.indexIn(u"A NEEDLE"_s) == 2 && m.caseSensitivity() == Qt::CaseInsensitive);
    m.setPattern(u"Ä"_s);
    CHECK(m.indexIn(u"xxä"_s) == 2 && m.pattern() == u"Ä"_s && m.patternView() == u"Ä");
    CHECK(QStringMatcher().indexIn(u"abc"_s, 1) == 1 && QStringMatcher(u"c"_s).indexIn(u"abc"_s, -1) == 2);
    const QChar raw[] = {u'b', u'c'};
    CHECK(QStringMatcher(raw, 2).indexIn(u"abcbc"_s, 2) == 3);

    QUtf8StringView u8("Grüße");
    CHECK(u8.size() == 7 && u8.toString() == u"Grüße"_s && u8.isValidUtf8() && !u8.isNull());
    CHECK(u8.first(2) == QUtf8StringView("Gr") && u8.last(2) == QUtf8StringView("ße").last(2) &&
          u8.sliced(1, 1).front() == 'r');
    CHECK(!QUtf8StringView("\xc3\x28").isValidUtf8() && !QUtf8StringView("\xed\xa0\x80").isValidUtf8() &&
          !QUtf8StringView("\xe2\x82").isValidUtf8());
    CHECK(QUtf8StringView("abc") < QUtf8StringView("abd") &&
          QUtf8StringView("ABC").compare("abc", Qt::CaseInsensitive) == 0);
    CHECK(QUtf8StringView().isNull() && QUtf8StringView(QByteArray()).isNull() &&
          QUtf8StringView(std::string("x")).size() == 1);

    const QString s = u"text"_s;
    const QAnyStringView a(s), b("text"), c(QLatin1StringView("text")), d(u"te");
    CHECK(a == b && b == c && a != d && d < a && a.size() == 4 && a.size_bytes() == 8 && b.size_bytes() == 4);
    CHECK(c.toString() == u"text"_s && a.front() == u't' && b.back() == u't' && a.sliced(1, 2).toString() == u"ex"_s);
    CHECK(b.chopped(1).toString() == u"tex"_s && QAnyStringView::compare(u"A", "a", Qt::CaseInsensitive) == 0);
    CHECK(QAnyStringView().isNull() && QAnyStringView("").isEmpty() && !QAnyStringView("").isNull());
    const qsizetype units = b.visit([](auto view) { return view.size(); });
    CHECK(units == 4);
    const QChar ch(u'q');
    CHECK(QAnyStringView(ch).toString() == u"q"_s);
}

static void testConverters() {
    QStringEncoder toUtf8(QStringConverter::Utf8);
    CHECK(toUtf8.isValid() && QByteArray(toUtf8.name()) == "UTF-8" &&
          toUtf8(u"aé€"_s) == QByteArray("a\xc3\xa9\xe2\x82\xac"));
    CHECK(QStringEncoder(QStringConverter::Utf16BE)(u"A€"_s) == QByteArray("\x00\x41\x20\xac", 4));
    CHECK(QStringEncoder(QStringConverter::Utf16LE, QStringConverter::Flag::WriteBom)(u"A"_s) ==
          QByteArray("\xff\xfe\x41\x00", 4));
    CHECK(QStringEncoder(QStringConverter::Utf32BE)(u"\U0001F600"_s) == QByteArray("\x00\x01\xf6\x00", 4));
    QStringEncoder latin1(QStringConverter::Latin1);
    CHECK(latin1(u"aé€"_s) == QByteArray("a\xe9?") && latin1.hasError());
    latin1.resetState();
    CHECK(!latin1.hasError());
    // A surrogate pair split between two calls is joined.
    QStringEncoder pieces(QStringConverter::Utf8);
    const QString smile = u"\U0001F600"_s;
    QByteArray joined = pieces(QStringView(smile).first(1));
    joined += pieces(QStringView(smile).sliced(1));
    CHECK(joined == QByteArray("\xf0\x9f\x98\x80") && !pieces.hasError());
    CHECK(QStringEncoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless)(QStringView(smile).first(1)) ==
          QByteArray("\xef\xbf\xbd"));
    char buffer[64];
    QStringEncoder enc(QStringConverter::Utf8);
    CHECK(enc.requiredSpace(3) >= 9 && enc.appendToBuffer(buffer, u"abc") - buffer == 3);

    QStringDecoder fromUtf8(QStringConverter::Utf8);
    QString text = fromUtf8(QByteArray("\xef\xbb\xbfGr\xc3"));
    text += fromUtf8(QByteArray("\xbc\xc3\x9f"
                                "e"));
    CHECK(text == u"Grüße"_s && !fromUtf8.hasError());
    QStringDecoder bad(QStringConverter::Utf8);
    CHECK(bad(QByteArray("a\xff"
                         "b\xc0\x80")) == u"a�b��"_s &&
          bad.hasError());
    CHECK(QStringDecoder(QStringConverter::Utf8, QStringConverter::Flag::ConvertInvalidToNull)(QByteArray("\xff")) ==
          QString(QChar(0)));
    CHECK(QStringDecoder(QStringConverter::Utf8,
                         QStringConverter::Flag::ConvertInitialBom)(QByteArray("\xef\xbb\xbfx")) == u"﻿x"_s);
    CHECK(QStringDecoder(QStringConverter::Utf16)(QByteArray("\xfe\xff\x00\x41", 4)) == u"A"_s);
    CHECK(QStringDecoder(QStringConverter::Utf16)(QByteArray("\xff\xfe\x41\x00", 4)) == u"A"_s);
    CHECK(QStringDecoder(QStringConverter::Utf32LE)(QByteArray("\x00\xf6\x01\x00", 4)) == u"\U0001F600"_s);
    CHECK(QStringDecoder(QStringConverter::Latin1)(QByteArray("\xe9")) == u"é"_s);
    QStringDecoder split16(QStringConverter::Utf16BE);
    QString s16 = split16(QByteArray("\x00", 1));
    s16 += split16(QByteArray("\x41\xd8\x3d", 3));
    s16 += split16(QByteArray("\xde\x00", 2));
    CHECK(s16 == u"A\U0001F600"_s);
    QStringDecoder stateless(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    CHECK(stateless(QByteArray("\xc3")) == u"�"_s);
    char16_t out[16];
    QStringDecoder dec(QStringConverter::Utf8);
    CHECK(dec.appendToBuffer(out, QByteArray("hi")) - out == 2 && out[1] == u'i');

    CHECK(QStringConverter::encodingForName("utf8") == QStringConverter::Utf8);
    CHECK(QStringConverter::encodingForName("ISO-8859-1") == QStringConverter::Latin1);
    CHECK(QStringConverter::encodingForName(u"UTF-16le") == QStringConverter::Utf16LE);
    CHECK(!QStringConverter::encodingForName("Shift_JIS") && !QStringDecoder("Shift_JIS").isValid());
    CHECK(QStringDecoder("latin1")(QByteArray("\xe9")) == u"é"_s &&
          QByteArray(QStringEncoder("locale").name()) == "Locale");
    CHECK(QByteArray(QStringConverter::nameForEncoding(QStringConverter::Utf32BE)) == "UTF-32BE");
    CHECK(QStringConverter::encodingForData(QByteArray("\xff\xfe\x00\x00", 4)) == QStringConverter::Utf32LE);
    CHECK(QStringConverter::encodingForData(QByteArray("\xef\xbb\xbf")) == QStringConverter::Utf8);
    CHECK(QStringConverter::encodingForData(QByteArray("\x00<", 2), u'<') == QStringConverter::Utf16BE);
    CHECK(!QStringConverter::encodingForData(QByteArray("abc")));
    CHECK(QStringConverter::encodingForHtml(QByteArray("<html><head><meta charset=\"ISO-8859-1\">")) ==
          QStringConverter::Latin1);
    CHECK(QStringConverter::encodingForHtml(QByteArray("<META http-equiv=x content='text/html; charset=utf-8'>")) ==
          QStringConverter::Utf8);
    CHECK(QStringConverter::encodingForHtml(QByteArray("<p>plain</p>")) == QStringConverter::Utf8);
    CHECK(!QStringConverter::encodingForHtml(QByteArray("<meta charset=koi8-r>")));
    CHECK(QStringDecoder::decoderForHtml(QByteArray("<meta charset=latin1>"))(QByteArray("\xe9")) == u"é"_s);
    CHECK(QStringConverter::availableCodecs().contains(u"UTF-8"_s));
    QStringDecoder moved = std::move(dec);
    CHECK(moved.isValid() && moved(QByteArray("ok")) == u"ok"_s);
}

static void testTokenizer() {
    const QString line = u"a,b,,c"_s;
    QStringList parts;
    for (QStringView part : QStringTokenizer{line, u','})
        parts << part.toString();
    CHECK((parts == QStringList{u"a"_s, u"b"_s, u""_s, u"c"_s}));
    parts.clear();
    for (QStringView part : qTokenize(line, u',', Qt::SkipEmptyParts))
        parts << part.toString();
    CHECK((parts == QStringList{u"a"_s, u"b"_s, u"c"_s}));
    // Views from toContainer() point into the haystack, which must outlive them.
    const QString dashed = u"x--y--z"_s;
    const QList<QStringView> views = qTokenize(dashed, u"--").toContainer();
    CHECK(views.size() == 3 && views.at(2) == u"z");
    // A temporary haystack is kept alive by the tokenizer.
    CHECK(qTokenize(QString(u"p;q"_s), u';').toContainer().at(1) == u"q");
    CHECK(QStringTokenizer(line, QLatin1StringView(","), Qt::CaseInsensitive).toContainer().size() == 4);
    CHECK(qTokenize(u"aXbxc"_s, u'x', Qt::CaseInsensitive).toContainer().size() == 3);
    CHECK(qTokenize(u"ab"_s, u"").toContainer().size() == 4);
    CHECK(qTokenize(QString(), u',').toContainer().size() == 1);
    CHECK(qTokenize(QString(), u',', Qt::SkipEmptyParts).toContainer().isEmpty());
    auto tok = qTokenize(u"1 2"_s, u' ');
    auto it = tok.begin();
    CHECK(*it == u"1" && it->size() == 1);
    ++it;
    CHECK(it != tok.end() && *it++ == u"2" && it == tok.end());
}

// ------------------------------------------------------------------ QLocale

static void testLocaleNumbers() {
    const QLocale c = QLocale::c();
    CHECK(c.name() == u"C"_s && c.language() == QLocale::C && c.toString(1234567) == u"1234567"_s);
    CHECK(c.toString(-1.5) == u"-1.5"_s && c.toString(1e10, 'e', 2) == u"1.00e+10"_s && c.toDouble(u"2.5e3") == 2500.0);
    CHECK(c.numberOptions() == QLocale::OmitGroupSeparator);

    const QLocale us(QLocale::English, QLocale::UnitedStates);
    CHECK(us.name() == u"en_US"_s && us.bcp47Name() == u"en"_s && us.toString(1234567) == u"1,234,567"_s);
    CHECK(us.toString(-1234.5, 'f', 2) == u"-1,234.50"_s && us.toString(qulonglong(1000)) == u"1,000"_s);
    CHECK(us.toString(std::numeric_limits<qlonglong>::min()) == u"-9,223,372,036,854,775,808"_s);
    bool ok = false;
    CHECK(us.toInt(u"1,234", &ok) == 1234 && ok && us.toDouble(u" -1,234.5 ", &ok) == -1234.5 && ok);
    CHECK(us.toInt(u"12,34", &ok) == 0 && !ok); // misplaced separator
    CHECK(us.toInt(u"1,2345", &ok) == 0 && !ok);
    CHECK(us.toShort(u"40000", &ok) == 0 && !ok && us.toUInt(u"-1", &ok) == 0 && !ok);
    CHECK(us.toULongLong(u"18,446,744,073,709,551,615") == std::numeric_limits<qulonglong>::max());

    const QLocale de(u"de_DE.UTF-8"_s);
    CHECK(de.language() == QLocale::German && de.territory() == QLocale::Germany && de.decimalPoint() == u","_s &&
          de.groupSeparator() == u"."_s);
    CHECK(de.toString(1234567.891, 'f', 2) == u"1.234.567,89"_s && de.toDouble(u"1.234,5") == 1234.5 &&
          de.toFloat(u"0,25") == 0.25f);
    QLocale strict = de;
    strict.setNumberOptions(QLocale::RejectGroupSeparator | QLocale::OmitGroupSeparator);
    CHECK(strict.toString(1234) == u"1234"_s && strict.toInt(u"1.234", &ok) == 0 && !ok);
    QLocale exp = c;
    exp.setNumberOptions(QLocale::OmitLeadingZeroInExponent);
    CHECK(exp.toString(1e5, 'e', 1) == u"1.0e+5"_s);
    exp.setNumberOptions(QLocale::IncludeTrailingZeroesAfterDot);
    CHECK(exp.toString(1.5, 'g', 4) == u"1.500"_s);
    exp.setNumberOptions(QLocale::RejectTrailingZeroesAfterDot);
    CHECK(exp.toDouble(u"1.50", &ok) == 0.0 && !ok && exp.toDouble(u"1.5", &ok) == 1.5 && ok);
    CHECK(c.toString(0.1, 'g', QLocale::FloatingPointShortest) == u"0.1"_s);
    CHECK(c.toString(1.0 / 3, 'g', QLocale::FloatingPointShortest) == u"0.3333333333333333"_s);
    CHECK(c.toString(1234.5, 'f', QLocale::FloatingPointShortest) == u"1234.5"_s);

    const QLocale sv(QLocale::Swedish);
    CHECK(sv.toString(-1234) == u"−1 234"_s && sv.toInt(u"−1 234") == -1234 && sv.negativeSign() == u"−"_s);
    const QLocale es(QLocale::Spanish, QLocale::Spain);
    CHECK(es.toString(1234) == u"1234"_s && es.toString(12345) == u"12.345"_s); // groups only from five digits
    const QLocale ch(QLocale::German, QLocale::Switzerland);
    CHECK(ch.toString(1234.5, 'f', 1) == u"1’234.5"_s);
    CHECK(c.toString(std::numeric_limits<double>::infinity()) == u"inf"_s && std::isnan(c.toDouble(u"nan")));
}

static void testLocaleNamesAndDates() {
    const QLocale us(u"en_US"_s), gb(u"en-GB"_s), de(QLocale::German), fr(QLocale::French, QLocale::Canada);
    CHECK(gb.bcp47Name() == u"en-GB"_s && gb.name(QLocale::TagSeparator::Dash) == u"en-GB"_s &&
          fr.name() == u"fr_CA"_s);
    CHECK(QLocale(u"xx_YY"_s) == QLocale::c() && QLocale(QLocale::Welsh) == QLocale::c());
    CHECK(QLocale(QLocale::German, QLocale::UnitedStates).territory() == QLocale::Germany);
    CHECK(QLocale(u"zh_Hant_TW"_s).script() == QLocale::TraditionalHanScript &&
          QLocale(u"zh"_s).territory() == QLocale::China);
    CHECK(de.nativeLanguageName() == u"Deutsch"_s && de.nativeTerritoryName() == u"Deutschland"_s &&
          de.nativeCountryName() == u"Deutschland"_s);
    CHECK(QLocale::languageToString(QLocale::German) == u"German"_s &&
          QLocale::territoryToString(QLocale::Spain) == u"Spain"_s);
    CHECK(QLocale::scriptToString(QLocale::CyrillicScript) == u"Cyrillic"_s &&
          QLocale::countryToString(QLocale::Japan) == u"Japan"_s);
    CHECK(QLocale::languageToCode(QLocale::Japanese) == u"ja"_s && QLocale::territoryToCode(QLocale::Japan) == u"JP"_s);
    CHECK(QLocale::scriptToCode(QLocale::LatinScript) == u"Latn"_s &&
          QLocale::codeToScript(u"cyrl") == QLocale::CyrillicScript);
    CHECK(QLocale::codeToLanguage(u"DE") == QLocale::German && QLocale::codeToTerritory(u"de") == QLocale::Germany);
    CHECK(QLocale::codeToLanguage(u"zz") == QLocale::AnyLanguage &&
          QLocale::codeToTerritory(u"ZZ") == QLocale::AnyTerritory);
    CHECK((us.uiLanguages() == QStringList{u"en-US"_s, u"en"_s}) && us.collation() == us);
    CHECK(QLocale::matchingLocales(QLocale::English, QLocale::AnyScript, QLocale::AnyTerritory).size() == 4);
    CHECK(QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::Canada).size() == 2);
    CHECK(QLocale::matchingLocales(QLocale::C, QLocale::AnyScript, QLocale::AnyTerritory).at(0) == QLocale::c());

    CHECK(us.firstDayOfWeek() == Qt::Sunday && de.firstDayOfWeek() == Qt::Monday && us.weekdays().size() == 5);
    CHECK(us.measurementSystem() == QLocale::ImperialUSSystem && gb.measurementSystem() == QLocale::ImperialUKSystem &&
          de.measurementSystem() == QLocale::MetricSystem && de.textDirection() == Qt::LeftToRight);
    CHECK(us.monthName(1) == u"January"_s && us.monthName(1, QLocale::ShortFormat) == u"Jan"_s &&
          us.monthName(13).isEmpty());
    CHECK(us.dayName(7) == u"Sunday"_s && us.dayName(1, QLocale::NarrowFormat) == u"M"_s &&
          us.standaloneMonthName(2, QLocale::NarrowFormat) == u"F"_s);
    const QLocale ru(QLocale::Russian), pl(QLocale::Polish);
    CHECK(ru.monthName(1) == u"января"_s && ru.standaloneMonthName(1) == u"январь"_s &&
          pl.standaloneMonthName(3) == u"marzec"_s);
    CHECK(QLocale(QLocale::Japanese).monthName(12) == u"12月"_s && QLocale(QLocale::Chinese).monthName(1) == u"一月"_s);
    CHECK(us.amText() == u"AM"_s && QLocale(QLocale::Swedish).pmText() == u"em"_s);

    const QDate date(2024, 5, 1);
    const QTime time(14, 5, 9, 120);
    CHECK(us.toString(date) == u"Wednesday, May 1, 2024"_s && us.toString(date, QLocale::ShortFormat) == u"5/1/24"_s);
    CHECK(de.toString(date) == u"Mittwoch, 1. Mai 2024"_s && de.toString(date, QLocale::ShortFormat) == u"01.05.24"_s);
    CHECK(ru.toString(date) == u"среда, 1 мая 2024 г."_s);
    CHECK(QLocale(QLocale::Spanish).toString(date) == u"miércoles, 1 de mayo de 2024"_s);
    CHECK(QLocale(QLocale::Japanese).toString(date) == u"2024年5月1日水曜日"_s);
    CHECK(us.toString(time, QLocale::ShortFormat) == u"2:05 PM"_s &&
          de.toString(time, QLocale::ShortFormat) == u"14:05"_s);
    CHECK(us.toString(time, u"hh:mm:ss.zzz ap"_s) == u"02:05:09.120 pm"_s &&
          us.toString(time, u"H 'o''clock' z"_s) == u"14 o'clock 12"_s);
    CHECK(QLocale::c().toString(date) == u"Wednesday, 1 May 2024"_s);
    const QDateTime dt(date, time, Qt::UTC);
    CHECK(us.toString(dt, QLocale::LongFormat) == u"Wednesday, May 1, 2024 2:05:09 PM UTC"_s);
    CHECK(us.toString(dt, u"yyyy-MM-dd HH:mm t"_s) == u"2024-05-01 14:05 UTC"_s);
    CHECK(us.dateTimeFormat(QLocale::ShortFormat) == u"M/d/yy h:mm AP"_s && de.timeFormat() == u"HH:mm:ss t"_s);

    CHECK(us.toDate(u"Wednesday, May 1, 2024"_s) == date &&
          us.toDate(u"5/1/24"_s, QLocale::ShortFormat) == QDate(1924, 5, 1));
    CHECK(de.toDate(u"1. mai 2024"_s, u"d. MMMM yyyy"_s) == date &&
          de.toDate(u"31.02.2024"_s, u"dd.MM.yyyy"_s).isNull());
    CHECK(ru.toDate(u"1 январь 2024"_s, u"d MMMM yyyy"_s) == QDate(2024, 1, 1));
    CHECK(us.toTime(u"2:05 PM"_s, QLocale::ShortFormat) == QTime(14, 5) &&
          us.toTime(u"12:30 am"_s, u"h:mm ap"_s) == QTime(0, 30));
    CHECK(!us.toTime(u"13:00 PM"_s, u"h:mm AP"_s).isValid() && !us.toDate(u"May 1"_s, u"MMMM d, yyyy"_s).isValid());
    CHECK(us.toDateTime(u"2024-05-01 14:05 UTC+02:00"_s, u"yyyy-MM-dd HH:mm t"_s).offsetFromUtc() == 7200);
    CHECK(us.toDateTime(u"2024-05-01 14:05:09.12"_s, u"yyyy-MM-dd HH:mm:ss.z"_s).time() == time);
}

static void testLocaleMisc() {
    const QLocale us(QLocale::English), de(QLocale::German), ja(QLocale::Japanese), c = QLocale::c();
    CHECK(us.toCurrencyString(1234.5) == u"$1,234.50"_s && us.toCurrencyString(-3) == u"-$3"_s);
    CHECK(de.toCurrencyString(1234.5) == u"1.234,50 €"_s && ja.toCurrencyString(1234.6) == u"￥1,235"_s);
    CHECK(us.toCurrencyString(2.5, u"EUR "_s, 1) == u"EUR 2.5"_s && c.toCurrencyString(1234.5) == u"1234.50"_s);
    CHECK(de.currencySymbol() == u"€"_s && de.currencySymbol(QLocale::CurrencyIsoCode) == u"EUR"_s &&
          us.currencySymbol(QLocale::CurrencyDisplayName) == u"US Dollar"_s);
    CHECK(us.formattedDataSize(1023) == u"1,023 bytes"_s && us.formattedDataSize(1536) == u"1.50 KiB"_s);
    CHECK(us.formattedDataSize(1500000, 1, QLocale::DataSizeSIFormat) == u"1.5 MB"_s);
    CHECK(us.formattedDataSize(2048, 0, QLocale::DataSizeTraditionalFormat) == u"2 kB"_s &&
          de.formattedDataSize(1536) == u"1,50 KiB"_s);
    CHECK(us.quoteString(u"hi") == u"“hi”"_s && de.quoteString(u"hi") == u"„hi“"_s &&
          de.quoteString(u"x", QLocale::AlternateQuotation) == u"‚x‘"_s);
    const QStringList three{u"a"_s, u"b"_s, u"c"_s};
    CHECK(us.createSeparatedList(three) == u"a, b, and c"_s &&
          QLocale(QLocale::English, QLocale::UnitedKingdom).createSeparatedList(three) == u"a, b and c"_s);
    CHECK(de.createSeparatedList({u"a"_s, u"b"_s}) == u"a und b"_s && us.createSeparatedList({}).isEmpty() &&
          us.createSeparatedList({u"x"_s}) == u"x"_s);
    CHECK(us.toUpper(u"straße ǆ ω"_s) == u"STRAßE Ǆ Ω"_s && us.toLower(u"ΣΑΣ İ"_s) == u"σασ İ"_s);
    CHECK(us.percent() == u"%"_s && us.zeroDigit() == u"0"_s && us.positiveSign() == u"+"_s &&
          us.exponential() == u"e"_s);
    QLocale a = us, b = de;
    a.swap(b);
    CHECK(a == de && b == us && qHash(a) == qHash(QLocale(QLocale::German)));
    QLocale::setDefault(de);
    CHECK(QLocale() == de && QLocale().toString(1.5) == u"1,5"_s);
    QLocale::setDefault(c);
    CHECK(QLocale() == c);
}

// ------------------------------------------------------------------ QCollator

static void testCollator() {
    QCollator en(QLocale(QLocale::English));
    QStringList words{u"banana"_s, u"Apple"_s, u"apple"_s, u"cherry"_s, u"Äpfel"_s, u"apfel"_s};
    std::sort(words.begin(), words.end(), en);
    CHECK((words == QStringList{u"apfel"_s, u"Äpfel"_s, u"apple"_s, u"Apple"_s, u"banana"_s, u"cherry"_s}));
    CHECK(en.compare(u"a"_s, u"A"_s) < 0 && en.compare(u"resume"_s, u"résumé"_s) < 0 &&
          en.compare(u"résumé"_s, u"resumes"_s) < 0);
    en.setCaseSensitivity(Qt::CaseInsensitive);
    CHECK(en.compare(u"a"_s, u"A"_s) == 0 && en.caseSensitivity() == Qt::CaseInsensitive);
    CHECK(en.compare(u"file10"_s, u"file2"_s) < 0);
    en.setNumericMode(true);
    CHECK(en.numericMode() && en.compare(u"file10"_s, u"file2"_s) > 0 && en.compare(u"file02"_s, u"file2"_s) != 0);
    CHECK(en.compare(u"v1.10"_s, u"v1.9"_s) > 0 && en(u"x9"_s, u"x10"_s));
    CHECK(en.compare(u"ab"_s, u"a-c"_s) > 0);
    en.setIgnorePunctuation(true);
    CHECK(en.ignorePunctuation() && en.compare(u"a-b"_s, u"ab"_s) == 0 && en.compare(u"a b"_s, u"a-b"_s) == 0);
    CHECK(en.compare(u"straße"_s, u"strasse"_s) > 0 && en.compare(u"straße"_s, u"strasse"_s) != 0 &&
          en.compare(u"strasse"_s, u"strast"_s) < 0);

    QCollator sv(QLocale(QLocale::Swedish));
    CHECK(sv.compare(u"ö"_s, u"z"_s) > 0 && sv.compare(u"å"_s, u"ä"_s) < 0 && en.compare(u"ö"_s, u"z"_s) < 0);
    CHECK(QCollator(QLocale(QLocale::Spanish)).compare(u"ñu"_s, u"nz"_s) > 0);
    CHECK(QCollator(QLocale(QLocale::Polish)).compare(u"łza"_s, u"lz"_s) > 0);
    CHECK(QCollator(QLocale(QLocale::Russian)).compare(u"ёж"_s, u"ель"_s) != 0);
    CHECK(QCollator().compare(u"Ω"_s, u"z"_s) > 0);

    const QCollatorSortKey k1 = sv.sortKey(u"ä"_s), k2 = sv.sortKey(u"z"_s);
    CHECK(k1.compare(k2) > 0 && k2 < k1 && k1.compare(k1) == 0);
    QCollator copy = sv;
    copy.setLocale(QLocale(QLocale::English));
    CHECK(copy.locale().language() == QLocale::English && sv.locale().language() == QLocale::Swedish);
    copy.swap(sv);
    CHECK(sv.locale().language() == QLocale::English);
    const QChar a[] = {u'a'}, b[] = {u'b'};
    CHECK(sv.compare(a, 1, b, 1) < 0 && QCollator::defaultCompare(u"a", u"b") < 0 &&
          QCollator::defaultSortKey(u"a") < QCollator::defaultSortKey(u"b"));
    CHECK(QString::localeAwareCompare(u"apple"_s, u"Banana"_s) < 0);
}

// ------------------------------------------------------------------ QTextBoundaryFinder

static QStringList segments(QTextBoundaryFinder::BoundaryType type, const QString &text, bool itemsOnly) {
    QTextBoundaryFinder f(type, text);
    QStringList out;
    qsizetype start = 0;
    for (qsizetype end; (end = f.toNextBoundary()) != -1; start = end) {
        if (!itemsOnly || (f.boundaryReasons() & QTextBoundaryFinder::EndOfItem))
            out << text.mid(start, end - start);
    }
    return out;
}

static void testBoundaryFinder() {
    using F = QTextBoundaryFinder;
    CHECK((segments(F::Grapheme, u"éa\r\n\U0001F600"_s, false) ==
           QStringList{u"é"_s, u"a"_s, u"\r\n"_s, u"\U0001F600"_s}));
    CHECK(segments(F::Grapheme, u"\U0001F1E9\U0001F1EA\U0001F1EB\U0001F1F7"_s, false).size() == 2); // two flags
    CHECK(segments(F::Grapheme, u"\U0001F469‍\U0001F4BB"_s, false).size() == 1);
    CHECK(segments(F::Grapheme, u"각"_s, false).size() == 1); // a decomposed Hangul syllable
    CHECK((segments(F::Word, u"Hello, world! It's 3.14 o'clock"_s, true) ==
           QStringList{u"Hello"_s, u"world"_s, u"It's"_s, u"3.14"_s, u"o'clock"_s}));
    CHECK((segments(F::Word, u"a  b"_s, false) == QStringList{u"a"_s, u"  "_s, u"b"_s}));
    CHECK((segments(F::Word, u"snake_case x2 カタカナ漢字"_s, true) ==
           QStringList{u"snake_case"_s, u"x2"_s, u"カタカナ"_s, u"漢"_s, u"字"_s}));
    CHECK((segments(F::Sentence, u"Hello there. How are you? Fine!"_s, false) ==
           QStringList{u"Hello there. "_s, u"How are you? "_s, u"Fine!"_s}));
    // UAX #29 has no abbreviation list, so "Mr." before a capital ends a sentence.
    CHECK((segments(F::Sentence, u"Mr. Smith arrived. etc. and more. Pi is 3.14 ok."_s, false) ==
           QStringList{u"Mr. "_s, u"Smith arrived. etc. and more. "_s, u"Pi is 3.14 ok."_s}));
    CHECK((segments(F::Sentence, u"\"Stop!\" he said.\nNext"_s, false) ==
           QStringList{u"\"Stop!\" "_s, u"he said.\n"_s, u"Next"_s}));
    CHECK((segments(F::Line, u"The quick (brown) fox-like jump."_s, false) ==
           QStringList{u"The "_s, u"quick "_s, u"(brown) "_s, u"fox-"_s, u"like "_s, u"jump."_s}));
    CHECK((segments(F::Line, u"a b c"_s, false) == QStringList{u"a b "_s, u"c"_s}));
    CHECK((segments(F::Line, u"-5 $10 100%"_s, false) == QStringList{u"-5 "_s, u"$10 "_s, u"100%"_s}));
    CHECK(segments(F::Line, u"漢字かな"_s, false).size() == 4);

    F f(F::Line, u"one\ntwo co­op"_s);
    f.setPosition(4);
    CHECK(f.isAtBoundary() && (f.boundaryReasons() & F::MandatoryBreak));
    f.setPosition(11);
    CHECK((f.boundaryReasons() & F::SoftHyphen) && f.position() == 11);
    f.toEnd();
    CHECK(f.position() == 13 && (f.boundaryReasons() & F::MandatoryBreak) && f.toNextBoundary() == -1 &&
          f.position() == -1);
    f.toEnd();
    CHECK(f.toPreviousBoundary() == 11 && f.toPreviousBoundary() == 8 && f.toPreviousBoundary() == 4 &&
          f.toPreviousBoundary() == -1);
    f.toStart();
    CHECK(f.position() == 0 && !f.isAtBoundary());
    f.setPosition(100);
    CHECK(f.position() == 13);

    F w(F::Word, u" hi "_s);
    CHECK(w.isValid() && w.type() == F::Word && w.string() == u" hi "_s);
    w.setPosition(1);
    CHECK(w.boundaryReasons() == (F::BreakOpportunity | F::StartOfItem));
    w.setPosition(3);
    CHECK(w.boundaryReasons() == (F::BreakOpportunity | F::EndOfItem));
    w.setPosition(2);
    CHECK(w.boundaryReasons() == F::NotAtBoundary);
    F g(F::Grapheme, QStringView(u"ab"));
    CHECK(g.boundaryReasons() == (F::BreakOpportunity | F::StartOfItem));
    g.toEnd();
    CHECK(g.boundaryReasons() == (F::BreakOpportunity | F::EndOfItem));
    const QChar chars[] = {u'x', u'.'};
    F s(F::Sentence, chars, 2);
    CHECK(s.toNextBoundary() == 2);
    CHECK(!F().isValid() && F().toNextBoundary() == -1);
}

// ------------------------------------------------------------------ QTranslator

static void testTranslator() {
    QTranslator de;
    CHECK(de.isEmpty() && de.translate("Window", "Open").isNull());
    CHECK(de.load(kGermanQm, int(sizeof kGermanQm)) && !de.isEmpty() && de.language() == u"de_DE"_s &&
          de.filePath().isEmpty());
    CHECK(de.translate("Window", "Open") == u"Öffnen"_s && de.translate("Other", "Open") == u"Aufmachen"_s);
    // "Close" is unique in its context, so lrelease drops the disambiguation "menu": both find it.
    CHECK(de.translate("Window", "Close", "menu") == u"Schließen"_s &&
          de.translate("Window", "Close") == u"Schließen"_s);
    CHECK(de.translate("Nowhere", "Open").isNull() && de.translate("Window", "Missing").isNull());
    CHECK(de.translate("Window", "%n file(s)", nullptr, 1) == u"%n Datei"_s &&
          de.translate("Window", "%n file(s)", nullptr, 3) == u"%n Dateien"_s);
    CHECK(!de.load(reinterpret_cast<const uchar *>("not a qm file"), 13) && de.isEmpty());

    QTranslator pl;
    CHECK(pl.load(kPolishQm, int(sizeof kPolishQm)));
    const int counts[] = {1, 2, 4, 5, 12, 22, 25, 112, 0};
    const char *const expected[] = {"plik",  "pliki",  "pliki",  "plików", "plików",
                                    "pliki", "plików", "plików", "plików"};
    for (std::size_t k = 0; k < std::size(counts); ++k)
        CHECK(pl.translate("Window", "%n file(s)", nullptr, counts[k]) == u"%n "_s + QString::fromUtf8(expected[k]));

    CHECK(QCoreApplication::translate("Window", "Open") == u"Open"_s);
    CHECK(!QCoreApplication::installTranslator(nullptr) && !QCoreApplication::installTranslator(&de));
    CHECK(de.load(kGermanQm, int(sizeof kGermanQm)) && QCoreApplication::installTranslator(&de) &&
          QCoreApplication::installTranslator(&pl));
    CHECK(QCoreApplication::translate("Window", "Open") == u"Öffnen"_s);
    CHECK(QCoreApplication::translate("Window", "%n file(s)", nullptr, 5) ==
          u"5 plików"_s); // the last installed is searched first
    CHECK(QCoreApplication::translate("Window", "Untranslated %n", nullptr, 7) == u"Untranslated 7"_s);
    CHECK(QCoreApplication::removeTranslator(&pl) && !QCoreApplication::removeTranslator(&pl));
    CHECK(QCoreApplication::translate("Window", "%n file(s)", nullptr, 5) == u"5 Dateien"_s);
    {
        QTranslator temporary;
        CHECK(temporary.load(kPolishQm, int(sizeof kPolishQm)) && QCoreApplication::installTranslator(&temporary));
    } // the destructor uninstalls it
    CHECK(QCoreApplication::translate("Window", "%n file(s)", nullptr, 2) == u"2 Dateien"_s);
    CHECK(QObject::tr("Open") == u"Open"_s);
    QCoreApplication::removeTranslator(&de);

    // Loading from files, with the fallback search.
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString dir = QFileInfo(tmp.path()).canonicalFilePath();
    {
        QFile f(dir + u"/app_pl.qm"_s);
        CHECK(f.open(QIODevice::WriteOnly) &&
              f.write(reinterpret_cast<const char *>(kPolishQm), qint64(sizeof kPolishQm)) == qint64(sizeof kPolishQm));
    }
    QTranslator file;
    CHECK(file.load(u"app_pl_PL"_s, dir) && file.filePath() == dir + u"/app_pl.qm"_s && file.language() == u"pl_PL"_s);
    CHECK(!file.load(u"other_pl"_s, dir) && file.isEmpty());
    CHECK(file.load(QLocale(QLocale::Polish), u"app"_s, u"_"_s, dir) && !file.isEmpty());
    CHECK(!file.load(QLocale(QLocale::German), u"app"_s, u"_"_s, dir));
    CHECK(file.load(dir + u"/app_pl.qm"_s));
}

// ------------------------------------------------------------------ Unicode in QChar and QString

static void testUnicodeCharAndString() {
    CHECK(QChar(u'Ж').isLetter() && QChar(u'Ж').isUpper() && QChar(u'ж').isLower() && QChar(u'Ж').toLower() == u'ж');
    CHECK(QChar(u'ا').isLetter() && QChar(u'क').isLetter() && QChar(u'ก').isLetter() && !QChar(u'·').isLetter());
    CHECK(QChar(u'Ǆ').isUpper() && QChar(u'ǅ').isTitleCase() && QChar(u'ǆ').toTitleCase() == u'ǅ' &&
          QChar(u'ǆ').toUpper() == u'Ǆ');
    CHECK(QChar(u'٣').isDigit() && QChar(u'٣').digitValue() == 3 && QChar(u'७').digitValue() == 7 &&
          QChar(u'x').digitValue() == -1);
    CHECK(QChar(u'½').isNumber() && !QChar(u'½').isDigit() && QChar(u'Ⅻ').isNumber() &&
          QChar(u'Ⅻ').category() == QChar::Number_Letter);
    CHECK(QChar(u'́').isMark() && QChar(u'€').isSymbol() && QChar(u'«').isPunct() && QChar(u'　').isSpace() &&
          QChar(u' ').isSpace());
    CHECK(QChar(u'A').category() == QChar::Letter_Uppercase && QChar(u'​').category() == QChar::Other_Format &&
          QChar(u'\x7f').category() == QChar::Other_Control && QChar(0xe000).category() == QChar::Other_PrivateUse);
    CHECK(!QChar(u'\x07').isPrint() && QChar(u'ä').isPrint() && !QChar(0x0378).isPrint() &&
          QChar(0xfffe).isNonCharacter());
    CHECK(QChar(u'ẞ').toCaseFolded() == u'ß' && QChar(u'Σ').toCaseFolded() == u'σ' &&
          QChar(u'ς').toCaseFolded() == u'σ');
    CHECK(QChar(u'ß').toUpper() == u'ß'); // the simple mapping has no single-character uppercase
    // Code points outside the BMP, through the static overloads.
    CHECK(QChar::isLetter(char32_t(0x1D400)) && QChar::isUpper(char32_t(0x1D400)) &&
          QChar::category(char32_t(0x1F600)) == QChar::Symbol_Other);
    CHECK(QChar::toLower(char32_t(0x10400)) == 0x10428 && QChar::toUpper(char32_t(0x10428)) == 0x10400 &&
          QChar::isDigit(char32_t(0x1D7CE)));
    CHECK(QChar::digitValue(char32_t(0x1D7D0)) == 2 && QChar::isSpace(char32_t(' ')) &&
          !QChar::isLetter(char32_t(0x110000)));
    CHECK(QChar::requiresSurrogates(0x1F600) && !QChar::requiresSurrogates(0xFFFF) &&
          QChar::highSurrogate(0x1F600) == 0xd83d && QChar::lowSurrogate(0x1F600) == 0xde00 &&
          QChar::surrogateToUcs4(char16_t(0xd83d), char16_t(0xde00)) == 0x1F600);
    CHECK(QChar::surrogateToUcs4(QChar(0xd83d), QChar(0xde00)) == 0x1F600 && QChar::isHighSurrogate(char32_t(0xd800)) &&
          QChar::isLowSurrogate(char32_t(0xdfff)) && QChar::isSurrogate(char32_t(0xdabc)) &&
          !QChar::isSurrogate(char32_t(0xe000)));

    CHECK(u"straße"_s.toUpper() == u"STRASSE"_s && u"ﬁ"_s.toUpper() == u"FI"_s && u"İ"_s.toLower() == u"i̇"_s);
    CHECK(u"ΣΊΣΥΦΟΣ"_s.toLower() == u"σίσυφοσ"_s && u"ǆemal"_s.toUpper() == u"ǄEMAL"_s);
    CHECK(u"Straße"_s.toCaseFolded() == u"strasse"_s && u"ΣΑΣ"_s.toCaseFolded() == u"σασ"_s);
    CHECK(u"\U00010400x"_s.toLower() == u"\U00010428x"_s && u"\U00010428"_s.toUpper() == u"\U00010400"_s);
    CHECK(QString().toUpper().isNull() && QString(u""_s).toLower().isEmpty());
    // Case-insensitive search uses simple folding and keeps positions.
    CHECK(u"ПРИВЕТ мир"_s.indexOf(u"привет"_s, 0, Qt::CaseInsensitive) == 0 &&
          u"aΣb"_s.indexOf(u"ς"_s, 0, Qt::CaseInsensitive) == 1);
    CHECK(u"x\U00010400y"_s.indexOf(u"\U00010428y"_s, 0, Qt::CaseInsensitive) == 1);
    CHECK(QString::compare(u"ÄÖÜ"_s, u"äöü"_s, Qt::CaseInsensitive) == 0 &&
          u"ǅ"_s.compare(u"ǆ"_s, Qt::CaseInsensitive) == 0);
    // Number parsers still accept only ASCII digits.
    CHECK(QVersionNumber::fromString(u"١.٢"_s).isNull() && !QDate::fromString(u"٢٠٢٤-01-01"_s, Qt::ISODate).isValid());
    CHECK(u"\\1"_s.isEmpty() == false);
}

static void testArgLocalized() {
    const QLocale saved;
    QLocale::setDefault(QLocale(QLocale::German));
    CHECK(u"%1 and %L1"_s.arg(1234567) == u"1234567 and 1.234.567"_s);
    CHECK(u"%L1"_s.arg(qulonglong(1000)) == u"1.000"_s && u"%L1"_s.arg(1234.5, 0, 'f', 1) == u"1.234,5"_s);
    CHECK(u"%L1 %2"_s.arg(1000).arg(u"x"_s) == u"1.000 x"_s && u"%L1"_s.arg(255, 0, 16) == u"ff"_s);
    CHECK(u"[%L1]"_s.arg(1234, 8) == u"[   1.234]"_s);
    QLocale::setDefault(QLocale::c());
    CHECK(u"%L1"_s.arg(1234567) == u"1234567"_s);
    QLocale::setDefault(saved);
}

static void testTextStreamEncodings() {
    QByteArray out;
    {
        QTextStream ts(&out, QIODevice::WriteOnly);
        CHECK(ts.encoding() == QStringConverter::Utf8 && ts.autoDetectUnicode() && !ts.generateByteOrderMark());
        ts.setEncoding(QStringConverter::Utf16LE);
        ts << Qt::bom << u"Aé"_s;
        CHECK(ts.generateByteOrderMark() && ts.encoding() == QStringConverter::Utf16LE);
    }
    CHECK(out == QByteArray("\xff\xfe\x41\x00\xe9\x00", 6));
    {
        // The byte order mark is detected and selects UTF-16LE.
        QTextStream in(out);
        CHECK(in.readAll() == u"Aé"_s && in.encoding() == QStringConverter::Utf16LE);
    }
    {
        QTextStream in(out);
        in.setAutoDetectUnicode(false);
        in.setEncoding(QStringConverter::Latin1);
        CHECK(in.readAll().size() == 6);
    }
    QByteArray latin;
    {
        QTextStream ts(&latin, QIODevice::WriteOnly);
        ts.setEncoding(QStringConverter::Latin1);
        ts << u"café €"_s;
    }
    CHECK(latin == QByteArray("caf\xe9 ?"));
    {
        QTextStream in(latin);
        in.setEncoding(QStringConverter::Latin1);
        CHECK(in.readLine() == u"café ?"_s);
    }
    // A UTF-8 byte order mark is skipped, and positions count bytes of the encoding.
    QByteArray utf8("\xef\xbb\xbfGr\xc3\xbc\xc3\x9f"
                    "e\nnext");
    {
        QTextStream in(utf8);
        CHECK(in.readLine() == u"Grüße"_s && in.pos() == 11 && in.readLine() == u"next"_s);
    }
    QByteArray utf32("\x00\x00\xfe\xff\x00\x01\xf6\x00\x00\x00\x00\x41", 12);
    {
        QTextStream in(utf32);
        CHECK(in.readAll() == u"\U0001F600A"_s && in.encoding() == QStringConverter::Utf32BE);
    }
    QBuffer device;
    device.open(QIODevice::ReadWrite);
    {
        QTextStream ts(&device);
        ts.setGenerateByteOrderMark(true);
        ts << u"x"_s;
        ts.flush();
        ts << u"y"_s; // no second byte order mark
    }
    CHECK(device.data() == QByteArray("\xef\xbb\xbfxy"));
}

static void testNormalization() {
    const QString composed = u"é"_s;   // é
    const QString decomposed = u"é"_s; // e + combining acute
    CHECK(decomposed.normalized(QString::NormalizationForm_C) == composed);
    CHECK(composed.normalized(QString::NormalizationForm_D) == decomposed);
    // Marks are put in canonical order: dot below (220) before acute (230).
    CHECK(u"q̣́"_s.normalized(QString::NormalizationForm_D) == u"q̣́"_s);
    CHECK(u"ḍ̇"_s.normalized(QString::NormalizationForm_C) == u"ḍ̇"_s);
    // Compatibility forms fold presentation variants.
    CHECK(u"ﬁ"_s.normalized(QString::NormalizationForm_KC) == u"fi"_s);
    CHECK(u"ﬁ"_s.normalized(QString::NormalizationForm_C) == u"ﬁ"_s);
    CHECK(u"x²"_s.normalized(QString::NormalizationForm_KD) == u"x2"_s);
    // Singletons and exclusions: Ångström sign → Å; U+0958 stays decomposed.
    CHECK(u"Å"_s.normalized(QString::NormalizationForm_C) == u"Å"_s);
    CHECK(u"क़"_s.normalized(QString::NormalizationForm_C) == u"क़"_s);
    // Hangul syllables decompose and recompose by formula.
    CHECK(u"한"_s.normalized(QString::NormalizationForm_D) == u"한"_s);
    CHECK(u"한"_s.normalized(QString::NormalizationForm_C) == u"한"_s);
    // Characters outside the BMP.
    CHECK(u"\U0001D15E"_s.normalized(QString::NormalizationForm_C) == u"\U0001D157\U0001D165"_s);
    CHECK(QString().normalized(QString::NormalizationForm_C).isNull() &&
          u"plain ascii"_s.normalized(QString::NormalizationForm_KC) == u"plain ascii"_s);

    CHECK(QChar(u'é').decomposition() == decomposed && QChar(u'é').decompositionTag() == QChar::Canonical);
    CHECK(QChar(u'½').decomposition() == u"1⁄2"_s && QChar(u'½').decompositionTag() == QChar::Fraction);
    CHECK(QChar(u' ').decompositionTag() == QChar::NoBreak && QChar(u'a').decompositionTag() == QChar::NoDecomposition);
    CHECK(QChar(u'a').decomposition().isEmpty());
    CHECK(QChar(u'한').decomposition() == u"한"_s); // LVT -> LV + T
    CHECK(QChar::decomposition(char32_t(0x1D15E)) == u"\U0001D157\U0001D165"_s);
    CHECK(QChar(u'́').combiningClass() == 230 && QChar(u'̣').combiningClass() == 220 &&
          QChar(u'a').combiningClass() == 0);
    CHECK(QChar::combiningClass(char32_t(0x1D165)) == 216);
}

static void testStringConversions() {
    const QString s = u"aé\U0001F600"_s;
    CHECK(s.toStdU32String() == U"aé\U0001F600");
    CHECK(QString::fromStdU32String(U"x\U0001F600") == u"x\U0001F600"_s);
    CHECK(s.toUcs4() == QList<uint>({0x61, 0xe9, 0x1F600}));
    const char32_t ucs4[] = {0x41, 0x1F600, 0x110000, 0};
    CHECK(QString::fromUcs4(ucs4) == u"A\U0001F600�"_s);
    CHECK(QString::fromUcs4(ucs4, 1) == u"A"_s);
    CHECK(s.toStdWString() == L"aé\U0001F600");
    CHECK(QString::fromStdWString(L"w\U0001F600") == u"w\U0001F600"_s);
    wchar_t buf[8] = {};
    const qsizetype n = s.toWCharArray(buf);
    CHECK(QString::fromWCharArray(buf, n) == s && QString::fromWCharArray(L"zz") == u"zz"_s);
    // Unpaired surrogates.
    const QString broken(QChar(char16_t(0xD800)));
    CHECK(!broken.isValidUtf16() && s.isValidUtf16() && QString().isValidUtf16());
    CHECK(!QString(QChar(char16_t(0xDC00))).isValidUtf16());
    CHECK(broken.toStdU32String() == U"�");

    CHECK(u"<a href=\"x\">&</a>"_s.toHtmlEscaped() == u"&lt;a href=&quot;x&quot;&gt;&amp;&lt;/a&gt;"_s);
    CHECK(u"ABC 1!"_s.isUpper() && !u"Abc"_s.isUpper() && u"été"_s.isLower() && !u"Été"_s.isLower());

    QString t = u"hello world"_s;
    CHECK(t.slice(6) == u"world"_s);
    t = u"hello world"_s;
    CHECK(t.slice(1, 4) == u"ello"_s);
    CHECK(!QString().nullTerminated().isNull() && QString().nullTerminated().unicode()[0] == QChar(u'\0'));
    QString digits = u"a1b2c3"_s;
    CHECK(digits.removeIf([](QChar c) { return c.isDigit(); }) == u"abc"_s);
    CHECK(QString().assign(3, QChar(u'x')) == u"xxx"_s);
    const std::u16string src = u"iter";
    CHECK(QString().assign(src.begin(), src.end()) == u"iter"_s);
    QString any;
    CHECK(any.assign("utf-8 \xc3\xa9") == u"utf-8 \u00e9"_s && any.assign(u"utf-16") == u"utf-16"_s);
    CHECK(any.assign(QLatin1StringView("latin")) == u"latin"_s && any.assign(u"copy"_s) == u"copy"_s);
    CHECK(!QString().assign(QAnyStringView()).isNull());
    QString raw;
    CHECK(raw.setUnicode(s.unicode(), 2) == u"aé"_s && raw.setUtf16(u"xyz", 2) == u"xy"_s);
    CHECK(raw.setUnicode(nullptr, 5).size() == 5 && raw.setRawData(s.constData(), 1) == u"a"_s);
}

static void testStringViewsAndDebug() {
    // QLatin1StringView
    const QLatin1StringView l1("  Hello, World  ");
    CHECK(l1.trimmed() == QLatin1StringView("Hello, World") && l1.length() == 16 && !l1.empty());
    const QLatin1StringView hw = l1.trimmed();
    CHECK(hw.first(5) == QLatin1StringView("Hello") && hw.last(5) == QLatin1StringView("World"));
    CHECK(hw.sliced(7) == QLatin1StringView("World") && hw.sliced(7, 3) == QLatin1StringView("Wor"));
    CHECK(hw.mid(7, 100) == QLatin1StringView("World") && hw.left(5) == QLatin1StringView("Hello") &&
          hw.right(5) == QLatin1StringView("World"));
    CHECK(hw.front() == QChar(u'H') && hw.back() == QChar(u'd') && hw.chopped(7) == QLatin1StringView("Hello"));
    CHECK(hw.startsWith(u"hello"_s, Qt::CaseInsensitive) && hw.endsWith(QChar(u'd')) && hw.contains(u"o, W"_s));
    CHECK(hw.indexOf(QChar(u'o')) == 4 && hw.lastIndexOf(QChar(u'o')) == 8 && hw.count(QChar(u'l')) == 3);
    CHECK(hw.compare(u"hello, world"_s, Qt::CaseInsensitive) == 0 && hw.toString() == u"Hello, World"_s);
    CHECK(QLatin1StringView("abc") < QLatin1StringView("abd") && QLatin1StringView("\xe9") > QLatin1StringView("z"));
    CHECK(QLatin1StringView("abc") == "abc" && QLatin1StringView("abc") != QLatin1StringView("ab"));
    CHECK(u"abc"_s == QLatin1StringView("abc") && QLatin1StringView("abc") == u"abc"_s);
    std::string collected(hw.begin(), hw.end());
    CHECK(collected == "Hello, World");

    // QStringView
    const QString text = u"a,b,,c"_s;
    const QStringView v(text);
    const QList<QStringView> parts = v.split(QChar(u','));
    CHECK(parts.size() == 4 && parts.at(0) == u"a" && parts.at(2).isEmpty() && parts.at(3) == u"c");
    CHECK(parts.at(3).data() == text.constData() + 5); // views into the same characters
    CHECK(v.split(u",", Qt::SkipEmptyParts).size() == 3);
    CHECK(QStringView(u"aXbxc").split(u"x", Qt::KeepEmptyParts, Qt::CaseInsensitive).size() == 3);
    CHECK(v.count(QChar(u',')) == 3 && v.count(u",,") == 1 && v.count(QLatin1StringView(",")) == 3);
    CHECK(v.startsWith(QLatin1StringView("a,")) && v.endsWith(QLatin1StringView(",c")) &&
          v.contains(QLatin1StringView(",,")));
    CHECK(v.indexOf(QLatin1StringView(",,")) == 3 && v.lastIndexOf(QLatin1StringView(",")) == 4);
    CHECK(QStringView(u"abc").compare(QLatin1StringView("ABC"), Qt::CaseInsensitive) == 0 &&
          QStringView(u"b").compare(QChar(u'a')) > 0);
    QString reversed;
    for (auto it = QStringView(u"abc").rbegin(); it != QStringView(u"abc").rend(); ++it)
        reversed += *it;
    CHECK(reversed == u"cba"_s);
    bool ok = false;
    CHECK(QStringView(u"-12").toShort(&ok) == -12 && ok && QStringView(u"70000").toShort(&ok) == 0 && !ok);
    CHECK(QStringView(u"65535").toUShort(&ok) == 65535 && ok && QStringView(u"-1").toULong(&ok) == 0 && !ok);
    CHECK(QStringView(u"123456").toLong(&ok) == 123456 && ok && QStringView(u"1.5").toFloat(&ok) == 1.5f && ok);
    CHECK(u"4000000000"_s.toULong(&ok) == 4000000000UL && ok);
    CHECK(QStringView(u"é").toLocal8Bit() == "\xc3\xa9" &&
          QStringView(u"A\U0001F600").toUcs4() == QList<uint>({0x41, 0x1F600}));
    CHECK(QStringView(u"ABC").isUpper() && QStringView(u"abc").isLower() && QStringView(u"ok").isValidUtf16());

    // QDebug manipulators.
    QString out;
    {
        QDebug d(&out);
        d << Qt::hex << Qt::showbase << 255 << Qt::dec << 255 << Qt::uppercasedigits << Qt::hex << 255;
    }
    CHECK(out == u"0xff 255 0xFF "_s); // a QDebug writing to a string keeps the trailing space
    out.clear();
    {
        QDebug d(&out);
        d.nospace() << qSetFieldWidth(5) << 42 << qSetFieldWidth(0) << "|" << qSetPadChar(u'0') << qSetFieldWidth(4)
                    << 7;
    }
    CHECK(out == u"   42|0007"_s);
    out.clear();
    {
        QDebug d(&out);
        d << Qt::fixed << qSetRealNumberPrecision(2) << 3.14159 << Qt::forcesign << 1;
    }
    CHECK(out == u"3.14 +1 "_s);
    out.clear();
    {
        QDebug d(&out);
        {
            QDebugStateSaver saver(d);
            d << Qt::hex << 16;
        }
        d << 16 << 0.5;
    }
    CHECK(out == u"10 16 0.5 "_s);

    // QTextStream with a locale.
    QString s;
    QTextStream ts(&s);
    ts.setLocale(QLocale(QLocale::German));
    CHECK(ts.locale() == QLocale(QLocale::German));
    ts << 1234567 << ' ' << 1234.5 << ' ' << -1000 << ' ' << Qt::hex << 255;
    CHECK(s == u"1.234.567 1.234,5 -1.000 ff"_s);
    QString input = u"1.234.567 2.500,25 12"_s;
    QTextStream in(&input);
    in.setLocale(QLocale(QLocale::German));
    int big = 0;
    double real = 0;
    int small = 0;
    in >> big >> real >> small;
    CHECK(big == 1234567 && real == 2500.25 && small == 12);
    QLocale noGroups(QLocale::German);
    noGroups.setNumberOptions(QLocale::OmitGroupSeparator);
    QString plain;
    QTextStream ps(&plain);
    ps.setLocale(noGroups);
    ps << 1234567 << ' ' << 0.5;
    CHECK(plain == u"1234567 0,5"_s);
    ps.setLocale(QLocale::c());
    ps << ' ' << 1234.5;
    CHECK(plain.endsWith(u" 1234.5"_s));
}

static void testUnicodeProperties() {
    CHECK(QChar(u'a').direction() == QChar::DirL && QChar(u'א').direction() == QChar::DirR);
    CHECK(QChar(u'ب').direction() == QChar::DirAL && QChar(u'1').direction() == QChar::DirEN);
    CHECK(QChar(u' ').direction() == QChar::DirWS && QChar(0x0300).direction() == QChar::DirNSM);
    CHECK(QChar::direction(char32_t(0x2067)) == QChar::DirRLI && QChar::direction(char32_t(0x2069)) == QChar::DirPDI);
    CHECK(QChar::direction(U'\U00010900') == QChar::DirR); // PHOENICIAN LETTER ALF

    CHECK(QChar(u'(').hasMirrored() && QChar(u'(').mirroredChar() == QChar(u')'));
    CHECK(QChar(u'«').mirroredChar() == QChar(u'»') && QChar(u'a').mirroredChar() == QChar(u'a'));
    CHECK(!QChar(u'a').hasMirrored());
    CHECK(QChar(0x2211).hasMirrored() &&
          QChar(0x2211).mirroredChar() == QChar(0x2211)); // N-ARY SUMMATION: no glyph pair
    CHECK(QChar::mirroredChar(U'\U0001D6DB') == U'\U0001D6DB');

    CHECK(QChar(0x0628).joiningType() == QChar::Joining_Dual && QChar(0x0627).joiningType() == QChar::Joining_Right);
    CHECK(QChar(0x200d).joiningType() == QChar::Joining_Causing &&
          QChar(0x0301).joiningType() == QChar::Joining_Transparent);
    CHECK(QChar(u'a').joiningType() == QChar::Joining_None &&
          QChar(0x00ad).joiningType() == QChar::Joining_Transparent);
    CHECK(QChar(0xa872).joiningType() == QChar::Joining_Left); // PHAGS-PA SUPERFIXED LETTER RA

    CHECK(QChar(u'a').script() == QChar::Script_Latin && QChar(u'α').script() == QChar::Script_Greek);
    CHECK(QChar(0x0300).script() == QChar::Script_Inherited && QChar(u'1').script() == QChar::Script_Common);
    CHECK(QChar(0x4e00).script() == QChar::Script_Han && QChar(0x0915).script() == QChar::Script_Devanagari);
    CHECK(QChar::script(U'\U0001F600') == QChar::Script_Common && QChar(0xe000).script() == QChar::Script_Unknown);
    CHECK(QChar::script(char32_t(0x110000)) == QChar::Script_Unknown &&
          QChar::script(U'\U0001E900') == QChar::Script_Adlam);
    CHECK(QChar(0xa840).script() == QChar::Script_PhagsPa && QChar(0x07ca).script() == QChar::Script_Nko);

    CHECK(QChar(u'a').unicodeVersion() == QChar::Unicode_1_1 && QChar(0x20ac).unicodeVersion() == QChar::Unicode_2_1_2);
    CHECK(QChar::unicodeVersion(U'\U0001F600') == QChar::Unicode_6_1 &&
          QChar(0x0378).unicodeVersion() == QChar::Unicode_Unassigned);
    CHECK(QChar::unicodeVersion(U'\U0001FAE9') == QChar::Unicode_16_0);
    CHECK(QChar::currentUnicodeVersion() == QChar::Unicode_16_0);

    const auto smile = QChar::fromUcs4(U'\U0001F600');
    CHECK(smile.size() == 2 && QStringView(smile) == u"\U0001F600"_s && smile.end() - smile.begin() == 2);
    CHECK(QChar::fromUcs4(U'a').size() == 1 && QStringView(QChar::fromUcs4(U'é')) == u"é"_s);
    CHECK(QChar(QChar::ParagraphSeparator).unicode() == 0x2029 && QChar(QChar::LineSeparator).unicode() == 0x2028);
    CHECK(QChar(QChar::ByteOrderSwapped).unicode() == 0xfffe && QChar(QChar::VisualTabCharacter).unicode() == 0x2192);

    CHECK(!u"abc"_s.isRightToLeft() && u"שלום"_s.isRightToLeft() && !QString().isRightToLeft());
    CHECK(u"123 שלום abc"_s.isRightToLeft());                    // digits and spaces are not strong
    const QChar rli(0x2067);                                     // RIGHT-TO-LEFT ISOLATE
    const QChar pdi(0x2069);                                     // POP DIRECTIONAL ISOLATE
    CHECK(!(rli + u"שלום"_s + pdi + u" abc"_s).isRightToLeft()); // the isolate is skipped
    CHECK(!(rli + u"שלום"_s).isRightToLeft());                   // an unterminated isolate hides the rest
    CHECK(u"\U00010900"_s.isRightToLeft());                      // a strong character outside the BMP
    CHECK(QStringView(u"مرحبا").isRightToLeft() && !QStringView(u"hi").isRightToLeft());
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testStringViewsAndDebug();
    testUnicodeProperties();
    testNormalization();
    testStringConversions();
    testRegexBasics();
    testRegexGroupsAndAnchors();
    testRegexUnicode();
    testRegexErrors();
    testRegexPartialAndGlobal();
    testRegexStatics();
    testRegexStringApi();
    testRegexLimits();
    testMatcherAndViews();
    testConverters();
    testTokenizer();
    testLocaleNumbers();
    testLocaleNamesAndDates();
    testLocaleMisc();
    testCollator();
    testBoundaryFinder();
    testTranslator();
    testUnicodeCharAndString();
    testArgLocalized();
    testTextStreamEncodings();
    if (g_failures == 0)
        std::puts("all text tests passed");
    return g_failures == 0 ? 0 : 1;
}
