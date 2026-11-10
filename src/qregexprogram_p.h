// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// The regular expression engine behind QRegularExpression: a pattern compiler producing a Program
// and a backtracking interpreter over UTF-16 subjects.
namespace ntregex {

// Pattern option bits; they have the values of QRegularExpression::PatternOption.
enum PatternBits {
    OptCaseless = 0x01,
    OptDotAll = 0x02,
    OptMultiline = 0x04,
    OptExtended = 0x08,
    OptUngreedy = 0x10,
    OptNoCapture = 0x20,
    OptUcp = 0x40
};

enum ClassItemKind : std::uint8_t {
    KDigit,
    KWord,
    KSpace,
    KHSpace,
    KVSpace,
    KPosix,
    KCategory,
    KScript,
    KAny,
    KWordPlus
};

enum PosixClass : std::uint32_t {
    PAlpha,
    PDigit,
    PAlnum,
    PUpper,
    PLower,
    PSpace,
    PBlank,
    PPunct,
    PPrint,
    PGraph,
    PCntrl,
    PXdigit,
    PWord,
    PAscii
};

struct ClassItem {
    ClassItemKind kind;
    bool negated;
    std::uint32_t arg;
};

// A character class: ranges of code points plus class escapes and properties.
struct CharSet {
    std::vector<std::pair<char32_t, char32_t>> ranges;
    std::vector<ClassItem> items;
    bool negated = false;
    bool caseless = false;
    bool ucp = false;
    std::uint64_t ascii[2] = {0, 0};

    bool matches(char32_t c) const {
        if (c < 128)
            return ((ascii[c >> 6] >> (c & 63)) & 1) != 0;
        return evaluate(c);
    }
    bool evaluate(char32_t c) const;
    void finish();

private:
    bool matchesPositive(char32_t c, bool caseVariant) const;
    bool matchesItems(char32_t c, bool caseVariant) const;
};

enum class Op : std::uint8_t {
    Match,
    Char,
    CharI,
    Str,
    Any,
    AnyAll,
    AnyUnit,
    Set,
    Rep,
    Split,
    Jmp,
    Open,
    Close,
    Assert,
    Backref,
    Look,
    LookEnd,
    Atomic,
    AtomicEnd,
    SetMark,
    Progress,
    CondGroup,
    Keep,
    Grapheme,
    Fail
};

enum AssertKind {
    AssertStartSubject,
    AssertStartLine,
    AssertEndSubjectNl,
    AssertEndLine,
    AssertEndSubject,
    AssertWordBoundary,
    AssertNotWordBoundary,
    AssertMatchStart
};

enum RepMode { RepGreedy, RepLazy, RepPossessive };

struct Inst {
    Op op = Op::Fail;
    Op kind = Op::Fail; // Rep: the single-character matcher being repeated
    std::uint8_t mode = 0;
    std::int32_t a = 0;
    std::int32_t b = 0;
    std::int32_t c = 0;
    std::int32_t d = 0;
    std::int32_t e = 0;
    std::int32_t f = -1;
};

struct Program {
    std::vector<Inst> code;
    std::vector<CharSet> sets;
    std::vector<std::u16string> strings;
    std::vector<std::u16string> groupNames; // captureCount + 1 entries, [0] is empty
    int captureCount = 0;
    int registerCount = 0;
    bool anchored = false;
    bool hasPrefix = false;
    std::u16string prefix;
    bool hasFirst = false;
    std::uint64_t firstAscii[2] = {0, 0};
    bool firstNonAscii = true;
    bool hasRequired = false;
    char16_t required = 0; // a code unit every match must contain
    int dotStar = 0;       // 1: the pattern starts with .*, 2: with a dot-all .*
};

struct CompileError {
    int code = 0;
    qsizetype offset = 0;
};

// Compiles a pattern; returns null and fills \a error when the pattern is invalid.
std::shared_ptr<const Program> compilePattern(const char16_t *pattern, qsizetype length, int options,
                                              CompileError &error);
const char *errorMessage(int code);

struct MatchRequest {
    const char16_t *subject = nullptr;
    qsizetype length = 0;
    qsizetype offset = 0;
    bool anchored = false;
    bool notEmptyAtStart = false;
    int partial = 0; // 0 none, 1 prefer a complete match, 2 prefer the first (partial) match
};

enum class ExecStatus { NoMatch, Match, Partial, LimitExceeded };

// Searches the subject. On Match or Partial, \a ovector holds the offsets of group 0 and the groups
// (2 * (captureCount + 1) entries, -1 for unset groups).
ExecStatus execute(const Program &program, const MatchRequest &request, std::vector<qsizetype> &ovector);

} // namespace ntregex
