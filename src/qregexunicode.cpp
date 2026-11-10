// MIT License - see LICENSE
#include "qregexunicode_p.h"

#include <cstring>

namespace ntregex {

Category categoryOf(char32_t c) {
    if (c >= 0x110000)
        return Cn;
    int lo = 0;
    int hi = kCategoryRunCount - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (kCategoryRuns[mid].start <= c)
            lo = mid;
        else
            hi = mid - 1;
    }
    return static_cast<Category>(kCategoryRuns[lo].category);
}

int scriptOf(char32_t c) {
    if (c >= 0x110000)
        return kUnknownScript;
    int lo = 0;
    int hi = kScriptRunCount - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (kScriptRuns[mid].start <= c)
            lo = mid;
        else
            hi = mid - 1;
    }
    return kScriptRuns[lo].script;
}

int scriptByName(const char *normalized) {
    for (int i = 0; i < kScriptNameCount; ++i)
        if (std::strcmp(kScriptNames[i].name, normalized) == 0)
            return kScriptNames[i].id;
    return -1;
}

static char32_t mapCase(const CaseRange *ranges, int count, char32_t c) {
    int lo = 0;
    int hi = count - 1;
    int found = -1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (ranges[mid].lo <= c) {
            found = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    if (found < 0)
        return c;
    const CaseRange &r = ranges[found];
    if (c > r.hi || (c - r.lo) % r.stride != 0)
        return c;
    return static_cast<char32_t>(static_cast<std::int32_t>(c) + r.delta);
}

char32_t toLowerSimple(char32_t c) {
    if (c < 0x80)
        return (c >= U'A' && c <= U'Z') ? c + 32 : c;
    return mapCase(kLowerRanges, kLowerRangeCount, c);
}

char32_t toUpperSimple(char32_t c) {
    if (c < 0x80)
        return (c >= U'a' && c <= U'z') ? c - 32 : c;
    return mapCase(kUpperRanges, kUpperRangeCount, c);
}

char32_t foldCase(char32_t c) {
    if (c < 0x80)
        return (c >= U'A' && c <= U'Z') ? c + 32 : c;
    return toLowerSimple(toUpperSimple(c));
}

bool isUnicodeWordChar(char32_t c) {
    if (c < 0x80)
        return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_';
    const std::uint32_t bit = catBit(categoryOf(c));
    return (bit & (kMaskL | kMaskN)) != 0;
}

bool isUnicodeSpace(char32_t c) {
    if (c < 0x80)
        return c == U' ' || (c >= 9 && c <= 13);
    if (c == 0x85)
        return true;
    return (catBit(categoryOf(c)) & kMaskZ) != 0;
}

} // namespace ntregex
