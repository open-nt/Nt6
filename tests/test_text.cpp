// MIT License - see LICENSE
#include <QtCore>

#include <cstdio>
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

int main() {
    testRegexBasics();
    testRegexGroupsAndAnchors();
    testRegexUnicode();
    testRegexErrors();
    testRegexPartialAndGlobal();
    testRegexStatics();
    testRegexStringApi();
    testRegexLimits();
    if (g_failures == 0)
        std::puts("all text tests passed");
    return g_failures == 0 ? 0 : 1;
}
