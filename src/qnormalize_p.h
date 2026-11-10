// MIT License - see LICENSE
#pragma once

// Unicode normalization (UAX #15) over the tables of qunicodedata_p.h: decomposition, canonical
// ordering and canonical composition, with Hangul syllables handled by formula.

#include "qunicode_p.h"

#include <algorithm>
#include <string>
#include <vector>

namespace QtPrivate {
namespace Unicode {

// The constants of the Hangul syllable formula (Unicode, section 3.12).
constexpr char32_t HangulSBase = 0xAC00;
constexpr char32_t HangulLBase = 0x1100;
constexpr char32_t HangulVBase = 0x1161;
constexpr char32_t HangulTBase = 0x11A7;
constexpr int HangulLCount = 19;
constexpr int HangulVCount = 21;
constexpr int HangulTCount = 28;
constexpr int HangulNCount = HangulVCount * HangulTCount;
constexpr int HangulSCount = HangulLCount * HangulNCount;

inline int combiningClass(char32_t c) {
    if (c < 0x300)
        return 0;
    const std::uint32_t key = (static_cast<std::uint32_t>(c) << 8) | 0xff;
    const auto *it =
        std::upper_bound(std::begin(UnicodeData::combiningClassRuns), std::end(UnicodeData::combiningClassRuns), key);
    return it == std::begin(UnicodeData::combiningClassRuns) ? 0 : static_cast<int>(*(it - 1) & 0xff);
}

// The one-level mapping of `c`, or nullptr if it has none (Hangul syllables included).
inline const UnicodeData::DecompositionMapping *decompositionMapping(char32_t c) {
    const auto *first = std::begin(UnicodeData::decompositionMappings);
    const auto *last = std::end(UnicodeData::decompositionMappings);
    const auto *it = std::lower_bound(
        first, last, c, [](const UnicodeData::DecompositionMapping &d, char32_t v) { return d.code < v; });
    return it != last && it->code == c ? it : nullptr;
}

// Appends the full decomposition of `c` to `out`: canonical only, or compatibility too.
inline void appendDecomposition(std::vector<char32_t> &out, char32_t c, bool compat) {
    if (c >= HangulSBase && c < HangulSBase + HangulSCount) {
        const char32_t s = c - HangulSBase;
        out.push_back(HangulLBase + s / HangulNCount);
        out.push_back(HangulVBase + (s % HangulNCount) / HangulTCount);
        if (s % HangulTCount != 0)
            out.push_back(HangulTBase + s % HangulTCount);
        return;
    }
    const UnicodeData::DecompositionMapping *m = decompositionMapping(c);
    if (!m || (m->tag != 1 && !compat)) {
        out.push_back(c);
        return;
    }
    for (int i = 0; i < m->length; ++i)
        appendDecomposition(out, UnicodeData::decompositionPool[m->offset + i], compat);
}

// The primary composite of `a` followed by `b`, or 0.
inline char32_t compose(char32_t a, char32_t b) {
    if (a >= HangulLBase && a < HangulLBase + HangulLCount && b >= HangulVBase && b < HangulVBase + HangulVCount)
        return HangulSBase + ((a - HangulLBase) * HangulVCount + (b - HangulVBase)) * HangulTCount;
    if (a >= HangulSBase && a < HangulSBase + HangulSCount && (a - HangulSBase) % HangulTCount == 0 &&
        b > HangulTBase && b < HangulTBase + HangulTCount)
        return a + (b - HangulTBase);
    const std::uint64_t key = (static_cast<std::uint64_t>(a) << 21) | b;
    const auto *first = std::begin(UnicodeData::compositions);
    const auto *last = std::end(UnicodeData::compositions);
    const auto *it = std::lower_bound(first, last, key,
                                      [](const UnicodeData::Composition &x, std::uint64_t v) { return x.pair < v; });
    return it != last && it->pair == key ? it->composite : 0;
}

// Normalizes `s` (UTF-16) into the form selected by `compat` (K forms) and `composeResult` (C forms).
inline std::u16string normalize(const std::u16string &s, bool compat, bool composeResult) {
    // Below U+00A0 every form leaves the text as it is.
    if (std::all_of(s.begin(), s.end(), [](char16_t u) { return u < 0xA0; }))
        return s;
    const auto n = static_cast<qsizetype>(s.size());
    std::vector<char32_t> cps;
    cps.reserve(s.size());
    for (qsizetype i = 0; i < n;) {
        int len = 1;
        const char32_t c = codePointAt(s.data(), n, i, &len);
        appendDecomposition(cps, c, compat);
        i += len;
    }
    // Canonical ordering: a stable sort of each run of non-starters by combining class.
    for (std::size_t i = 0; i < cps.size();) {
        if (combiningClass(cps[i]) == 0) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < cps.size() && combiningClass(cps[j]) != 0)
            ++j;
        std::stable_sort(cps.begin() + static_cast<std::ptrdiff_t>(i), cps.begin() + static_cast<std::ptrdiff_t>(j),
                         [](char32_t a, char32_t b) { return combiningClass(a) < combiningClass(b); });
        i = j;
    }
    if (composeResult && !cps.empty()) {
        // Canonical composition: each character joins the last starter unless it is blocked.
        std::size_t starter = 0;
        bool haveStarter = combiningClass(cps[0]) == 0;
        int lastClass = haveStarter ? 0 : 256;
        std::size_t outLen = 1;
        for (std::size_t i = 1; i < cps.size(); ++i) {
            const char32_t c = cps[i];
            const int cc = combiningClass(c);
            const bool blocked = outLen - 1 != starter && (lastClass == 0 || lastClass >= cc);
            if (haveStarter && !blocked) {
                if (const char32_t composite = compose(cps[starter], c)) {
                    cps[starter] = composite;
                    continue;
                }
            }
            if (cc == 0) {
                starter = outLen;
                haveStarter = true;
            }
            lastClass = cc;
            cps[outLen++] = c;
        }
        cps.resize(outLen);
    }
    std::u16string out;
    out.reserve(cps.size());
    for (char32_t c : cps)
        appendUtf16(out, c);
    return out;
}

} // namespace Unicode
} // namespace QtPrivate
