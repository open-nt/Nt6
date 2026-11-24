// MIT License - see LICENSE
#pragma once

#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace QtPrivate {

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
#define NT_HAVE_FP_CHARCONV 1
#endif

// std::to_chars() for a double, which the C++ library of the Android NDK (libc++ before r26) does not have. The
// fallback finds the fewest digits that read back as \a v, then lays them out as the standard does.
inline char *doubleToChars(char *first, char *last, double v, std::chars_format fmt) {
#ifdef NT_HAVE_FP_CHARCONV
    return std::to_chars(first, last, v, fmt).ptr;
#else
    char sci[64];
    int digits = 1;
    for (; digits <= 17; ++digits) {
        std::snprintf(sci, sizeof sci, "%.*e", digits - 1, v);
        if (std::strtod(sci, nullptr) == v)
            break;
    }
    if (digits > 17)
        std::snprintf(sci, sizeof sci, "%.16e", v);
    const char *ep = std::strchr(sci, 'e');
    const int exp10 = ep ? std::atoi(ep + 1) : 0;
    char fixed[400];
    std::snprintf(fixed, sizeof fixed, "%.*f", digits - 1 - exp10 > 0 ? digits - 1 - exp10 : 0, v);
    const char *out = sci;
    if (fmt == std::chars_format::fixed)
        out = fixed;
    else if (fmt == std::chars_format::general && std::strlen(fixed) <= std::strlen(sci))
        out = fixed;
    const std::size_t n = std::strlen(out);
    if (n > static_cast<std::size_t>(last - first))
        return first;
    std::memcpy(first, out, n);
    return first + n;
#endif
}

// std::from_chars() for a double, with the same fallback reason as doubleToChars().
struct DoubleParse {
    const char *ptr;
    std::errc ec;
};

inline DoubleParse doubleFromChars(const char *first, const char *last, double &out) {
#ifdef NT_HAVE_FP_CHARCONV
    const auto r = std::from_chars(first, last, out);
    return {r.ptr, r.ec};
#else
    if (first == last || *first == '+' || *first == ' ' || (*first >= '\t' && *first <= '\r'))
        return {first, std::errc::invalid_argument};
    const std::string copy(first, last);
    errno = 0;
    char *end = nullptr;
    const double d = std::strtod(copy.c_str(), &end);
    if (end == copy.c_str())
        return {first, std::errc::invalid_argument};
    const char *stop = first + (end - copy.c_str());
    if (errno == ERANGE && (std::isinf(d) || d == 0.0))
        return {stop, std::errc::result_out_of_range};
    out = d;
    return {stop, std::errc()};
#endif
}

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
        n = static_cast<int>(doubleToChars(buf, buf + sizeof buf, v, cf) - buf);
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
