// MIT License - see LICENSE
#pragma once

#include <charconv>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace QtPrivate {

inline std::string_view trimAscii(std::string_view s) {
    auto sp = [](char c) { return c == ' ' || (c >= '\t' && c <= '\r'); };
    while (!s.empty() && sp(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && sp(s.back()))
        s.remove_suffix(1);
    return s;
}

inline bool parseSigned(std::string_view in, int base, long long &out) {
    const std::string s(trimAscii(in));
    if (s.empty() || (base != 0 && (base < 2 || base > 36)))
        return false;
    errno = 0;
    char *end = nullptr;
    out = std::strtoll(s.c_str(), &end, base);
    return errno == 0 && end == s.c_str() + s.size();
}

inline bool parseUnsigned(std::string_view in, int base, unsigned long long &out) {
    const std::string s(trimAscii(in));
    if (s.empty() || s[0] == '-' || (base != 0 && (base < 2 || base > 36)))
        return false;
    errno = 0;
    char *end = nullptr;
    out = std::strtoull(s.c_str(), &end, base);
    return errno == 0 && end == s.c_str() + s.size();
}

inline bool parseDouble(std::string_view in, double &out) {
    const std::string s(trimAscii(in));
    if (s.empty())
        return false;
    char *end = nullptr;
    errno = 0;
    out = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size() && errno != ERANGE;
}

inline std::string intToString(unsigned long long v, bool negative, int base) {
    if (base < 2 || base > 36)
        base = 10;
    std::string r;
    do {
        const int digit = static_cast<int>(v % static_cast<unsigned>(base));
        r.push_back(static_cast<char>(digit < 10 ? '0' + digit : 'a' + digit - 10));
        v /= static_cast<unsigned>(base);
    } while (v);
    if (negative)
        r.push_back('-');
    return std::string(r.rbegin(), r.rend());
}

inline std::string signedToString(long long v, int base) {
    const bool neg = v < 0;
    const unsigned long long mag = neg ? 0ULL - static_cast<unsigned long long>(v) : static_cast<unsigned long long>(v);
    return intToString(mag, neg, base);
}

// precision < 0 requests the shortest representation that round-trips.
inline std::string formatDouble(double v, char fmt, int prec) {
    const bool upper = (fmt == 'E' || fmt == 'G' || fmt == 'F');
    const char lower = static_cast<char>(upper ? fmt + 32 : fmt);
    char buf[512];
    int n;
    if (prec < 0) {
        std::chars_format cf = lower == 'f' ? std::chars_format::fixed
                               : lower == 'e' ? std::chars_format::scientific
                                              : std::chars_format::general;
        auto r = std::to_chars(buf, buf + sizeof buf, v, cf);
        n = static_cast<int>(r.ptr - buf);
    } else {
        const char f[] = {'%', '.', '*', lower == 'f' || lower == 'e' ? lower : 'g', '\0'};
        n = std::snprintf(buf, sizeof buf, f, prec, v);
    }
    std::string s(buf, static_cast<std::size_t>(n < 0 ? 0 : n));
    if (upper)
        for (char &c : s)
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - 32);
    return s;
}

} // namespace QtPrivate
