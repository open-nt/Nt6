// MIT License - see LICENSE
#include "qjson_p.h"

#include <charconv>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>

namespace {

constexpr int kNestingLimit = 1024;
constexpr qint64 kMaxExactInteger = qint64(1) << 53;

// Decodes one UTF-8 sequence starting at p and advances p past it. Rejects overlong forms, surrogates and
// code points above U+10FFFF.
bool scanUtf8(const char *&p, const char *end, char32_t *out) {
    const auto b0 = static_cast<unsigned char>(*p);
    int extra = 0;
    char32_t cp = 0;
    char32_t min = 0;
    if (b0 < 0x80) {
        *out = b0;
        ++p;
        return true;
    }
    if (b0 >= 0xC2 && b0 <= 0xDF) {
        extra = 1;
        cp = b0 & 0x1Fu;
        min = 0x80;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        extra = 2;
        cp = b0 & 0x0Fu;
        min = 0x800;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        extra = 3;
        cp = b0 & 0x07u;
        min = 0x10000;
    } else {
        return false;
    }
    if (end - p <= extra)
        return false;
    for (int i = 1; i <= extra; ++i) {
        const auto b = static_cast<unsigned char>(p[i]);
        if ((b & 0xC0) != 0x80)
            return false;
        cp = (cp << 6) | (b & 0x3Fu);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return false;
    p += extra + 1;
    *out = cp;
    return true;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

class Parser {
public:
    Parser(const char *begin, const char *endOfInput) : head(begin), json(begin), end(endOfInput) {}

    bool parse(QJsonValue *root) {
        eatBOM();
        const char token = nextToken();
        if (token == '[') {
            if (!parseArray(root))
                return false;
        } else if (token == '{') {
            if (!parseObject(root))
                return false;
        } else {
            lastError = QJsonParseError::IllegalValue;
            return false;
        }
        eatSpace();
        if (json < end) {
            lastError = QJsonParseError::GarbageAtEnd;
            return false;
        }
        return true;
    }

    QJsonParseError::ParseError lastError = QJsonParseError::NoError;
    qsizetype offset() const { return json - head; }

private:
    const char *head;
    const char *json;
    const char *end;
    int nesting = 0;

    void eatBOM() {
        if (end - json >= 3 && static_cast<unsigned char>(json[0]) == 0xEF &&
            static_cast<unsigned char>(json[1]) == 0xBB && static_cast<unsigned char>(json[2]) == 0xBF)
            json += 3;
    }

    // Skips white space and returns whether any input is left.
    bool eatSpace() {
        while (json < end && (*json == ' ' || *json == '\t' || *json == '\n' || *json == '\r'))
            ++json;
        return json < end;
    }

    // Consumes the next character, and returns it if it is structural; returns 0 at the end of the input or for
    // any other character.
    char nextToken() {
        if (!eatSpace())
            return 0;
        char token = *json++;
        switch (token) {
        case '[':
        case '{':
        case ':':
        case ',':
        case ']':
        case '}':
            eatSpace();
            break;
        case '"':
            break;
        default:
            token = 0;
            break;
        }
        return token;
    }

    bool parseObject(QJsonValue *out) {
        if (++nesting > kNestingLimit) {
            lastError = QJsonParseError::DeepNesting;
            return false;
        }
        std::vector<QJsonObjectPrivate::Entry> entries;
        // The first token is either a key or the closing brace.
        char token = nextToken();
        while (token == '"') {
            QJsonObjectPrivate::Entry entry;
            if (!parseString(&entry.key))
                return false;
            token = nextToken();
            if (token != ':') {
                lastError = QJsonParseError::MissingNameSeparator;
                return false;
            }
            if (!eatSpace()) {
                lastError = QJsonParseError::UnterminatedObject;
                return false;
            }
            if (!parseValue(&entry.value))
                return false;
            entries.push_back(std::move(entry));
            token = nextToken();
            if (token != ',')
                break;
            token = nextToken();
            if (token == '}') {
                lastError = QJsonParseError::MissingObject;
                return false;
            }
        }
        if (token != '}') {
            lastError = QJsonParseError::UnterminatedObject;
            return false;
        }
        --nesting;
        *out = QJsonValue(QJsonPrivateAccess::makeObjectSorting(std::move(entries)));
        return true;
    }

    bool parseArray(QJsonValue *out) {
        if (++nesting > kNestingLimit) {
            lastError = QJsonParseError::DeepNesting;
            return false;
        }
        std::vector<QJsonValue> items;
        if (!eatSpace()) {
            lastError = QJsonParseError::UnterminatedArray;
            return false;
        }
        if (*json == ']') {
            nextToken();
        } else {
            while (true) {
                if (!eatSpace()) {
                    lastError = QJsonParseError::UnterminatedArray;
                    return false;
                }
                QJsonValue v;
                if (!parseValue(&v))
                    return false;
                items.push_back(std::move(v));
                const char token = nextToken();
                if (token == ']')
                    break;
                if (token != ',') {
                    lastError =
                        eatSpace() ? QJsonParseError::MissingValueSeparator : QJsonParseError::UnterminatedArray;
                    return false;
                }
            }
        }
        --nesting;
        *out = QJsonValue(QJsonPrivateAccess::makeArray(std::move(items)));
        return true;
    }

    bool parseLiteral(const char *word, std::size_t len) {
        if (static_cast<std::size_t>(end - json) < len || std::memcmp(json, word, len) != 0) {
            lastError = QJsonParseError::IllegalValue;
            return false;
        }
        json += len;
        return true;
    }

    // json points at the first character of the value, and is not at the end of the input.
    bool parseValue(QJsonValue *out) {
        switch (*json) {
        case 'n':
            if (!parseLiteral("null", 4))
                return false;
            *out = QJsonValue();
            return true;
        case 't':
            if (!parseLiteral("true", 4))
                return false;
            *out = QJsonValue(true);
            return true;
        case 'f':
            if (!parseLiteral("false", 5))
                return false;
            *out = QJsonValue(false);
            return true;
        case '"': {
            ++json;
            QString s;
            if (!parseString(&s))
                return false;
            *out = QJsonValue(s);
            return true;
        }
        case '[':
            ++json;
            return parseArray(out);
        case '{':
            ++json;
            return parseObject(out);
        case ']':
            ++json;
            lastError = QJsonParseError::MissingObject;
            return false;
        default:
            return parseNumber(out);
        }
    }

    bool parseNumber(QJsonValue *out) {
        const char *start = json;
        bool isInt = true;
        if (json < end && *json == '-')
            ++json;
        if (json < end && *json == '0') {
            ++json;
        } else if (json < end && *json >= '1' && *json <= '9') {
            while (json < end && *json >= '0' && *json <= '9')
                ++json;
        } else if (json == start) {
            lastError = QJsonParseError::IllegalValue;
            return false;
        } else if (json >= end) {
            lastError = QJsonParseError::TerminationByNumber;
            return false;
        } else {
            lastError = QJsonParseError::IllegalNumber;
            return false;
        }
        const char *intEnd = json;
        if (json < end && *json == '.') {
            isInt = false;
            ++json;
            if (json < end && !(*json >= '0' && *json <= '9')) {
                lastError = QJsonParseError::IllegalNumber;
                return false;
            }
            while (json < end && *json >= '0' && *json <= '9')
                ++json;
        }
        if (json < end && (*json == 'e' || *json == 'E')) {
            isInt = false;
            ++json;
            if (json < end && (*json == '+' || *json == '-'))
                ++json;
            if (json < end && !(*json >= '0' && *json <= '9')) {
                lastError = QJsonParseError::IllegalNumber;
                return false;
            }
            while (json < end && *json >= '0' && *json <= '9')
                ++json;
        }
        if (json >= end) {
            lastError = QJsonParseError::TerminationByNumber;
            return false;
        }

        if (isInt) {
            const bool negative = *start == '-';
            const char *digits = negative ? start + 1 : start;
            if (intEnd - digits <= 18) {
                long long n = 0;
                for (const char *p = digits; p < intEnd; ++p)
                    n = n * 10 + (*p - '0');
                n = negative ? -n : n;
                if (n >= -kMaxExactInteger && n <= kMaxExactInteger) {
                    *out = QJsonValue(static_cast<qint64>(n));
                    return true;
                }
            }
        }
        return convertDouble(start, json, out);
    }

    bool convertDouble(const char *first, const char *last, QJsonValue *out) {
        double d = 0;
        const auto r = std::from_chars(first, last, d);
        if (r.ec == std::errc::result_out_of_range) {
            if (!convertTiny(first, last, &d)) {
                lastError = QJsonParseError::IllegalNumber;
                return false;
            }
        } else if (r.ec != std::errc() || r.ptr != last || !std::isfinite(d)) {
            lastError = QJsonParseError::IllegalNumber;
            return false;
        }
        *out = QJsonValue(d);
        return true;
    }

    // from_chars reports a range error both for numbers that are too large and for ones that are too small, and
    // some implementations do so for subnormal results too. This tells which it was, from the decimal exponent of
    // the first significant digit, and converts the small ones with strtod. It is given the digits without the
    // decimal point, so the C locale's point does not matter.
    static bool convertTiny(const char *first, const char *last, double *out) {
        const char *p = first;
        const bool negative = *p == '-';
        if (negative)
            ++p;
        std::string digits;
        long long fractionDigits = 0;
        bool seenDot = false;
        for (; p < last && *p != 'e' && *p != 'E'; ++p) {
            if (*p == '.') {
                seenDot = true;
                continue;
            }
            digits.push_back(*p);
            if (seenDot)
                ++fractionDigits;
        }
        long long exponent = 0;
        if (p < last) {
            ++p;
            const bool negativeExponent = p < last && *p == '-';
            if (p < last && (*p == '+' || *p == '-'))
                ++p;
            for (; p < last; ++p) {
                if (exponent < 100000)
                    exponent = exponent * 10 + (*p - '0');
            }
            if (negativeExponent)
                exponent = -exponent;
        }
        const std::size_t firstSignificant = digits.find_first_not_of('0');
        if (firstSignificant == std::string::npos) {
            *out = 0;
            return true;
        }
        // The value is 0.DIGITS x 10^magnitude.
        const long long magnitude =
            static_cast<long long>(digits.size() - firstSignificant) - fractionDigits + exponent;
        if (magnitude > 0)
            return false;
        const std::string text =
            (negative ? "-" : "") + digits.substr(firstSignificant) + "e" + std::to_string(exponent - fractionDigits);
        *out = std::strtod(text.c_str(), nullptr);
        return true;
    }

    // json is just past the opening quote. Leaves it just past the closing quote.
    bool parseString(QString *out) {
        std::u16string s;
        while (true) {
            if (json >= end) {
                lastError = QJsonParseError::UnterminatedString;
                return false;
            }
            const auto c = static_cast<unsigned char>(*json);
            if (c == '"') {
                ++json;
                break;
            }
            if (c < 0x20) {
                lastError = QJsonParseError::IllegalValue;
                return false;
            }
            if (c == '\\') {
                if (!parseEscape(&s))
                    return false;
                continue;
            }
            char32_t cp = 0;
            if (!scanUtf8(json, end, &cp)) {
                lastError = QJsonParseError::IllegalUTF8String;
                return false;
            }
            if (cp >= 0x10000) {
                cp -= 0x10000;
                s.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
                s.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
            } else {
                s.push_back(static_cast<char16_t>(cp));
            }
        }
        *out = QString::fromStdU16String(s);
        return true;
    }

    // json is at the backslash.
    bool parseEscape(std::u16string *s) {
        ++json;
        if (json >= end) {
            lastError = QJsonParseError::UnterminatedString;
            return false;
        }
        const char c = *json++;
        switch (c) {
        case '"':
        case '\\':
        case '/':
            s->push_back(static_cast<char16_t>(c));
            return true;
        case 'b':
            s->push_back(u'\b');
            return true;
        case 'f':
            s->push_back(u'\f');
            return true;
        case 'n':
            s->push_back(u'\n');
            return true;
        case 'r':
            s->push_back(u'\r');
            return true;
        case 't':
            s->push_back(u'\t');
            return true;
        case 'u': {
            if (end - json < 4) {
                lastError = QJsonParseError::IllegalEscapeSequence;
                return false;
            }
            unsigned unit = 0;
            for (int i = 0; i < 4; ++i) {
                const int v = hexValue(json[i]);
                if (v < 0) {
                    lastError = QJsonParseError::IllegalEscapeSequence;
                    return false;
                }
                unit = unit * 16 + static_cast<unsigned>(v);
            }
            json += 4;
            s->push_back(static_cast<char16_t>(unit));
            return true;
        }
        default:
            lastError = QJsonParseError::IllegalEscapeSequence;
            return false;
        }
    }
};

} // namespace

namespace QJsonPrivate {

QJsonValue parse(const QByteArray &json, QJsonParseError *error) {
    if (error) {
        error->error = QJsonParseError::NoError;
        error->offset = 0;
    }
    if (json.size() > INT_MAX) {
        if (error)
            error->error = QJsonParseError::DocumentTooLarge;
        return QJsonValue();
    }
    Parser parser(json.constData(), json.constData() + json.size());
    QJsonValue root;
    if (parser.parse(&root))
        return root;
    if (error) {
        error->error = parser.lastError;
        error->offset = static_cast<int>(parser.offset());
    }
    return QJsonValue();
}

} // namespace QJsonPrivate
