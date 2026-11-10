// MIT License - see LICENSE
#include "qregexprogram_p.h"
#include "qregexunicode_p.h"

#include <algorithm>
#include <limits>
#include <string_view>

namespace ntregex {

namespace {

constexpr std::uint64_t kStepLimit = 10000000;
constexpr std::size_t kStackLimit = std::size_t(4) << 20;
constexpr qsizetype kInfinite = std::numeric_limits<qsizetype>::max() / 4;

enum BacktrackKind { BtBranch, BtGreedy, BtLazy, BtSlot, BtReg };

struct Backtrack {
    int kind;
    int pc;
    qsizetype a;
    qsizetype b;
};

bool isAsciiWordUnit(char32_t c) {
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_';
}

bool isHighSurrogate(char32_t c) { return (c & 0xfc00) == 0xd800; }
bool isLowSurrogate(char32_t c) { return (c & 0xfc00) == 0xdc00; }

enum HangulClass { HangulNone, HangulL, HangulV, HangulT, HangulLV, HangulLVT };

HangulClass hangulClass(char32_t c) {
    if ((c >= 0x1100 && c <= 0x115f) || (c >= 0xa960 && c <= 0xa97c))
        return HangulL;
    if ((c >= 0x1160 && c <= 0x11a7) || (c >= 0xd7b0 && c <= 0xd7c6))
        return HangulV;
    if ((c >= 0x11a8 && c <= 0x11ff) || (c >= 0xd7cb && c <= 0xd7fb))
        return HangulT;
    if (c >= 0xac00 && c <= 0xd7a3)
        return (c - 0xac00) % 28 == 0 ? HangulLV : HangulLVT;
    return HangulNone;
}

class Matcher {
public:
    Matcher(const Program &program, const MatchRequest &request)
        : prog(program), req(request), s(request.subject), n(request.length) {}

    ExecStatus search(std::vector<qsizetype> &out);

private:
    const Program &prog;
    const MatchRequest &req;
    const char16_t *s;
    qsizetype n;
    std::vector<qsizetype> caps;
    std::vector<qsizetype> regs;
    std::vector<Backtrack> stack;
    std::uint64_t steps = 0;
    bool stop = false;
    bool limit = false;
    bool hitEnd = false;
    bool suppressPartial = false;

    qsizetype pendingIndex(int g) const { return 2 * (static_cast<qsizetype>(prog.captureCount) + 1) + g; }

    void needMore() {
        if (suppressPartial)
            return;
        hitEnd = true;
        if (req.partial == 2)
            stop = true;
    }

    char32_t decode(qsizetype p, qsizetype &len) const {
        const char16_t u = s[p];
        if (isHighSurrogate(u) && p + 1 < n && isLowSurrogate(s[p + 1])) {
            len = 2;
            return 0x10000 + ((static_cast<char32_t>(u) - 0xd800) << 10) + (s[p + 1] - 0xdc00);
        }
        len = 1;
        return u;
    }

    qsizetype stepBack(qsizetype p, qsizetype lo) const {
        if (p - 2 >= lo && isLowSurrogate(s[p - 1]) && isHighSurrogate(s[p - 2]))
            return p - 2;
        return p - 1;
    }

    char32_t charBefore(qsizetype p) const {
        const char16_t u = s[p - 1];
        if (isLowSurrogate(u) && p >= 2 && isHighSurrogate(s[p - 2]))
            return 0x10000 + ((static_cast<char32_t>(s[p - 2]) - 0xd800) << 10) + (u - 0xdc00);
        return u;
    }

    static bool isWord(char32_t c, bool ucp) { return ucp ? isUnicodeWordChar(c) : isAsciiWordUnit(c); }

    bool stepOne(Op op, int arg, qsizetype p, qsizetype &np) {
        if (p >= n) {
            needMore();
            return false;
        }
        const char16_t u = s[p];
        switch (op) {
        case Op::AnyUnit:
            np = p + 1;
            return true;
        case Op::Char:
            if (arg < 0x10000) {
                if (u != arg)
                    return false;
                np = p + 1;
                return true;
            } else {
                qsizetype len = 1;
                if (decode(p, len) != static_cast<char32_t>(arg)) {
                    if (isHighSurrogate(u) && p + 1 >= n)
                        needMore();
                    return false;
                }
                np = p + len;
                return true;
            }
        case Op::CharI: {
            qsizetype len = 1;
            const char32_t c = u < 0x80 ? u : decode(p, len);
            if (foldCase(c) != static_cast<char32_t>(arg))
                return false;
            np = p + len;
            return true;
        }
        case Op::Any: {
            if (u == u'\n')
                return false;
            qsizetype len = 1;
            if (u >= 0xd800)
                decode(p, len);
            np = p + len;
            return true;
        }
        case Op::AnyAll: {
            qsizetype len = 1;
            if (u >= 0xd800)
                decode(p, len);
            np = p + len;
            return true;
        }
        case Op::Set: {
            qsizetype len = 1;
            const char32_t c = u < 0xd800 ? u : decode(p, len);
            if (!prog.sets[static_cast<std::size_t>(arg)].matches(c))
                return false;
            np = p + len;
            return true;
        }
        default:
            return false;
        }
    }

    bool grapheme(qsizetype p, qsizetype &np) {
        if (p >= n) {
            needMore();
            return false;
        }
        qsizetype len = 1;
        const char32_t c = decode(p, len);
        np = p + len;
        if (c == U'\r') {
            if (np < n && s[np] == u'\n')
                ++np;
            return true;
        }
        const Category cat = categoryOf(c);
        if (c == U'\n' || cat == Cc || cat == Zl || cat == Zp)
            return true;
        if (c >= 0x1f1e6 && c <= 0x1f1ff && np < n) {
            const char32_t c2 = decode(np, len);
            if (c2 >= 0x1f1e6 && c2 <= 0x1f1ff)
                np += len;
        }
        HangulClass state = hangulClass(c);
        while (np < n && state != HangulNone) {
            const char32_t c2 = decode(np, len);
            const HangulClass h = hangulClass(c2);
            bool join = false;
            switch (state) {
            case HangulL:
                join = h == HangulL || h == HangulV || h == HangulLV || h == HangulLVT;
                break;
            case HangulLV:
            case HangulV:
                join = h == HangulV || h == HangulT;
                break;
            case HangulLVT:
            case HangulT:
                join = h == HangulT;
                break;
            default:
                break;
            }
            if (!join)
                break;
            np += len;
            state = h;
        }
        while (np < n) {
            const char32_t c2 = decode(np, len);
            const Category k = categoryOf(c2);
            if (k == Mn || k == Me || k == Mc || c2 == 0x200d || (c2 >= 0x1f3fb && c2 <= 0x1f3ff)) {
                np += len;
                if (c2 == 0x200d && np < n) {
                    qsizetype l3 = 1;
                    const char32_t c3 = decode(np, l3);
                    if (categoryOf(c3) == So)
                        np += l3;
                }
            } else {
                break;
            }
        }
        return true;
    }

    void pushSlot(qsizetype idx, qsizetype value) {
        stack.push_back({BtSlot, static_cast<int>(idx), caps[static_cast<std::size_t>(idx)], 0});
        caps[static_cast<std::size_t>(idx)] = value;
    }

    bool groupSet(int g) const {
        return caps[static_cast<std::size_t>(2 * g)] >= 0 && caps[static_cast<std::size_t>(2 * g + 1)] >= 0;
    }

    bool backtrack(std::size_t base, int &pc, qsizetype &pos);
    void unwind(std::size_t base);
    void compact(std::size_t base);
    bool run(int pc, qsizetype pos, qsizetype lbTarget, qsizetype &endPos);
    bool backref(const Inst &in, qsizetype &pos);
    bool assertion(const Inst &in, qsizetype pos);
};

void Matcher::unwind(std::size_t base) {
    while (stack.size() > base) {
        const Backtrack e = stack.back();
        stack.pop_back();
        if (e.kind == BtSlot)
            caps[static_cast<std::size_t>(e.pc)] = e.a;
        else if (e.kind == BtReg)
            regs[static_cast<std::size_t>(e.pc)] = e.a;
    }
}

// Drops the choice points above \a base but keeps the undo records, making the code run since
// then atomic.
void Matcher::compact(std::size_t base) {
    std::size_t w = base;
    for (std::size_t r = base; r < stack.size(); ++r)
        if (stack[r].kind == BtSlot || stack[r].kind == BtReg)
            stack[w++] = stack[r];
    stack.resize(w);
}

bool Matcher::backtrack(std::size_t base, int &pc, qsizetype &pos) {
    const Inst *code = prog.code.data();
    while (stack.size() > base) {
        const Backtrack e = stack.back();
        stack.pop_back();
        switch (e.kind) {
        case BtSlot:
            caps[static_cast<std::size_t>(e.pc)] = e.a;
            break;
        case BtReg:
            regs[static_cast<std::size_t>(e.pc)] = e.a;
            break;
        case BtBranch:
            pc = e.pc;
            pos = e.a;
            return true;
        case BtGreedy: {
            qsizetype cur = stepBack(e.b, e.a);
            const Inst &next = code[e.pc];
            if (next.op == Op::Char && next.a < 0xd800) {
                while (cur > e.a && s[cur] != next.a)
                    cur = stepBack(cur, e.a);
            }
            if (cur > e.a)
                stack.push_back({BtGreedy, e.pc, e.a, cur});
            pc = e.pc;
            pos = cur;
            return true;
        }
        case BtLazy: {
            const Inst &rep = code[e.pc];
            qsizetype np = 0;
            if ((rep.c < 0 || e.b < rep.c) && stepOne(rep.kind, rep.a, e.a, np)) {
                stack.push_back({BtLazy, e.pc, np, e.b + 1});
                pc = e.pc + 1;
                pos = np;
                return true;
            }
            if (stop)
                return false;
            break;
        }
        default:
            break;
        }
    }
    return false;
}

bool Matcher::assertion(const Inst &in, qsizetype pos) {
    switch (in.a) {
    case AssertStartSubject:
        return pos == 0;
    case AssertStartLine:
        return pos == 0 || (s[pos - 1] == u'\n' && pos < n);
    case AssertEndSubjectNl:
        return pos == n || (pos == n - 1 && s[pos] == u'\n');
    case AssertEndLine:
        return pos == n || s[pos] == u'\n';
    case AssertEndSubject:
        return pos == n;
    case AssertWordBoundary:
    case AssertNotWordBoundary: {
        const bool ucp = in.b != 0;
        const bool before = pos > 0 && isWord(charBefore(pos), ucp);
        bool after = false;
        if (pos < n) {
            qsizetype len = 1;
            after = isWord(decode(pos, len), ucp);
        } else {
            needMore();
        }
        return (before != after) == (in.a == AssertWordBoundary);
    }
    case AssertMatchStart:
        return pos == req.offset;
    default:
        return false;
    }
}

bool Matcher::backref(const Inst &in, qsizetype &pos) {
    const int g = in.a;
    const qsizetype st = caps[static_cast<std::size_t>(2 * g)];
    const qsizetype en = caps[static_cast<std::size_t>(2 * g + 1)];
    if (st < 0 || en < 0)
        return false;
    if (in.b == 0) {
        const qsizetype len = en - st;
        const qsizetype avail = n - pos;
        const qsizetype cmp = std::min(len, avail);
        for (qsizetype k = 0; k < cmp; ++k)
            if (s[st + k] != s[pos + k])
                return false;
        if (avail < len) {
            needMore();
            return false;
        }
        pos += len;
        return true;
    }
    qsizetype i1 = st;
    qsizetype i2 = pos;
    while (i1 < en) {
        if (i2 >= n) {
            needMore();
            return false;
        }
        qsizetype l1 = 1;
        qsizetype l2 = 1;
        if (foldCase(decode(i1, l1)) != foldCase(decode(i2, l2)))
            return false;
        i1 += l1;
        i2 += l2;
    }
    pos = i2;
    return true;
}

bool Matcher::run(int pc, qsizetype pos, qsizetype lbTarget, qsizetype &endPos) {
    const std::size_t base = stack.size();
    const Inst *code = prog.code.data();
    for (;;) {
        if (++steps > kStepLimit || stack.size() > kStackLimit) {
            limit = true;
            stop = true;
        }
        if (stop) {
            stack.resize(base);
            return false;
        }
        const Inst &in = code[pc];
        bool ok = true;
        switch (in.op) {
        case Op::Match:
            if (req.notEmptyAtStart && pos == req.offset && caps[0] == pos) {
                ok = false;
                break;
            }
            endPos = pos;
            return true;
        case Op::LookEnd:
            if (lbTarget >= 0 && pos != lbTarget) {
                ok = false;
                break;
            }
            endPos = pos;
            return true;
        case Op::AtomicEnd:
            endPos = pos;
            return true;
        case Op::Char:
        case Op::CharI:
        case Op::Any:
        case Op::AnyAll:
        case Op::AnyUnit:
        case Op::Set: {
            qsizetype np = 0;
            if (stepOne(in.op, in.a, pos, np)) {
                pos = np;
                ++pc;
            } else {
                ok = false;
            }
            break;
        }
        case Op::Str: {
            const std::u16string &str = prog.strings[static_cast<std::size_t>(in.a)];
            const qsizetype len = static_cast<qsizetype>(str.size());
            const qsizetype avail = n - pos;
            const qsizetype cmp = std::min(len, avail);
            qsizetype k = 0;
            while (k < cmp && s[pos + k] == str[static_cast<std::size_t>(k)])
                ++k;
            if (k < cmp) {
                ok = false;
            } else if (avail < len) {
                needMore();
                ok = false;
            } else {
                pos += len;
                ++pc;
            }
            break;
        }
        case Op::Rep: {
            qsizetype cnt = 0;
            qsizetype cur = pos;
            qsizetype np = 0;
            if (in.mode == RepLazy) {
                while (cnt < in.b && stepOne(in.kind, in.a, cur, np)) {
                    cur = np;
                    ++cnt;
                }
                steps += static_cast<std::uint64_t>(cnt);
                if (cnt < in.b) {
                    ok = false;
                    break;
                }
                stack.push_back({BtLazy, pc, cur, cnt});
                pos = cur;
                ++pc;
                break;
            }
            const qsizetype maxc = in.c < 0 ? kInfinite : in.c;
            qsizetype minPos = pos;
            while (cnt < maxc && stepOne(in.kind, in.a, cur, np)) {
                cur = np;
                ++cnt;
                if (cnt == in.b)
                    minPos = cur;
            }
            steps += static_cast<std::uint64_t>(cnt);
            if (cnt < in.b) {
                ok = false;
                break;
            }
            if (in.mode == RepGreedy && cnt > in.b)
                stack.push_back({BtGreedy, pc + 1, minPos, cur});
            pos = cur;
            ++pc;
            break;
        }
        case Op::Split:
            stack.push_back({BtBranch, in.b, pos, 0});
            pc = in.a;
            break;
        case Op::Jmp:
            pc = in.a;
            break;
        case Op::Open:
            pushSlot(pendingIndex(in.a), pos);
            ++pc;
            break;
        case Op::Close:
            pushSlot(2 * in.a, caps[static_cast<std::size_t>(pendingIndex(in.a))]);
            pushSlot(2 * in.a + 1, pos);
            ++pc;
            break;
        case Op::Assert:
            if (assertion(in, pos))
                ++pc;
            else
                ok = false;
            break;
        case Op::Backref:
            if (backref(in, pos))
                ++pc;
            else
                ok = false;
            break;
        case Op::Look: {
            const std::size_t sbase = stack.size();
            bool matched = false;
            qsizetype dummy = 0;
            if (in.a == 0) {
                matched = run(pc + 1, pos, -1, dummy);
            } else {
                qsizetype st = pos;
                for (int len = 0; len <= in.e && !stop; ++len) {
                    if (len >= in.d) {
                        matched = run(pc + 1, st, pos, dummy);
                        if (matched)
                            break;
                    }
                    if (st == 0)
                        break;
                    st = stepBack(st, 0);
                }
            }
            if (stop)
                break;
            if (in.b != 0) {
                if (matched) {
                    unwind(sbase);
                    matched = false;
                } else {
                    matched = true;
                }
            } else if (matched) {
                compact(sbase);
            }
            if (matched) {
                pc = in.c;
            } else if (in.f >= 0) {
                pc = in.f;
            } else {
                ok = false;
            }
            break;
        }
        case Op::Atomic: {
            const std::size_t sbase = stack.size();
            qsizetype e = 0;
            if (run(pc + 1, pos, -1, e)) {
                compact(sbase);
                pos = e;
                pc = in.c;
            } else {
                ok = false;
            }
            break;
        }
        case Op::SetMark:
            stack.push_back({BtReg, in.a, regs[static_cast<std::size_t>(in.a)], 0});
            regs[static_cast<std::size_t>(in.a)] = pos;
            ++pc;
            break;
        case Op::Progress:
            if (pos == regs[static_cast<std::size_t>(in.b)])
                pc = in.a;
            else
                ++pc;
            break;
        case Op::CondGroup:
            if (groupSet(in.a))
                ++pc;
            else
                pc = in.b;
            break;
        case Op::Keep:
            pushSlot(0, pos);
            ++pc;
            break;
        case Op::Grapheme: {
            qsizetype np = 0;
            if (grapheme(pos, np)) {
                pos = np;
                ++pc;
            } else {
                ok = false;
            }
            break;
        }
        case Op::Fail:
            ok = false;
            break;
        }
        if (!ok && !backtrack(base, pc, pos))
            return false;
    }
}

ExecStatus Matcher::search(std::vector<qsizetype> &out) {
    const std::size_t groups = static_cast<std::size_t>(prog.captureCount) + 1;
    caps.assign(groups * 3, -1);
    regs.assign(static_cast<std::size_t>(prog.registerCount), -1);
    stack.reserve(256);
    const bool partial = req.partial != 0;
    qsizetype partialStart = -1;
    qsizetype start = req.offset;
    const bool single = req.anchored || prog.anchored;
    if (prog.hasRequired && !partial) {
        const std::u16string_view tail(s + start, static_cast<std::size_t>(n - start));
        if (tail.find(static_cast<char16_t>(prog.required)) == std::u16string_view::npos)
            return ExecStatus::NoMatch;
    }
    for (;;) {
        if (!single) {
            if (prog.hasPrefix && !partial) {
                const std::u16string_view hay(s + start, static_cast<std::size_t>(n - start));
                const std::size_t f = hay.find(prog.prefix);
                if (f == std::u16string_view::npos)
                    break;
                start += static_cast<qsizetype>(f);
            } else if (prog.hasFirst) {
                while (start < n) {
                    const char16_t u = s[start];
                    const bool ok = u < 128 ? ((prog.firstAscii[u >> 6] >> (u & 63)) & 1) != 0 : prog.firstNonAscii;
                    if (ok)
                        break;
                    ++start;
                }
            }
            if (start > req.offset && start < n && isLowSurrogate(s[start]) && isHighSurrogate(s[start - 1])) {
                ++start;
                continue;
            }
        }
        std::fill(caps.begin(), caps.end(), qsizetype(-1));
        caps[0] = start;
        stack.clear();
        hitEnd = false;
        // A partial match that would start after the last character is not reported.
        suppressPartial = start >= n && start > req.offset;
        qsizetype endPos = 0;
        if (run(0, start, -1, endPos)) {
            caps[1] = endPos;
            out.assign(caps.begin(), caps.begin() + static_cast<std::ptrdiff_t>(groups * 2));
            return ExecStatus::Match;
        }
        if (limit)
            return ExecStatus::LimitExceeded;
        if (stop) {
            out.assign(2, -1);
            out[0] = start;
            out[1] = n;
            return ExecStatus::Partial;
        }
        if (partial && hitEnd && partialStart < 0)
            partialStart = start;
        if (single || start >= n)
            break;
        if (prog.dotStar != 0) {
            if (prog.dotStar == 2)
                break;
            // A failed attempt covers every later start on the same line.
            while (start < n && s[start] != u'\n')
                ++start;
            if (start >= n)
                break;
            ++start;
            continue;
        }
        start += (isHighSurrogate(s[start]) && start + 1 < n && isLowSurrogate(s[start + 1])) ? 2 : 1;
    }
    if (partialStart >= 0) {
        out.assign(2, -1);
        out[0] = partialStart;
        out[1] = n;
        return ExecStatus::Partial;
    }
    return ExecStatus::NoMatch;
}

} // namespace

ExecStatus execute(const Program &program, const MatchRequest &request, std::vector<qsizetype> &ovector) {
    Matcher m(program, request);
    return m.search(ovector);
}

} // namespace ntregex
