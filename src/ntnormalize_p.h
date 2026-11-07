// MIT License - see LICENSE
#pragma once

// Normalization of C++ type names and method signatures as used by the meta-object system.
// ntmoc and the run-time library share this file, so both spell types the same way.

#include <cctype>
#include <string>
#include <vector>

namespace NtMeta {

inline bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

inline bool startsWithWord(const std::string &s, std::size_t pos, const char *word) {
    std::size_t n = 0;
    while (word[n])
        ++n;
    if (s.compare(pos, n, word) != 0)
        return false;
    return pos + n == s.size() || !isIdentChar(s[pos + n]);
}

inline bool isWordStart(const std::string &s, std::size_t pos) { return pos == 0 || !isIdentChar(s[pos - 1]); }

// Replaces whole words `from` in `s` by `to`.
inline void replaceWord(std::string &s, const char *from, const char *to) {
    const std::string f = from;
    for (std::size_t pos = s.find(f); pos != std::string::npos; pos = s.find(f, pos + 1)) {
        if (isWordStart(s, pos) && startsWithWord(s, pos, from)) {
            s.replace(pos, f.size(), to);
            pos += std::string(to).size() - 1;
        }
    }
}

// Splits `a, b<c, d>, e` at top-level commas.
inline std::vector<std::string> splitTopLevel(const std::string &s) {
    std::vector<std::string> parts;
    int depth = 0;
    std::string cur;
    for (char c : s) {
        if (c == '<' || c == '(' || c == '[')
            ++depth;
        else if (c == '>' || c == ')' || c == ']')
            --depth;
        if (c == ',' && depth == 0) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty() || !parts.empty())
        parts.push_back(cur);
    return parts;
}

// Normalizes a type such as `const QString &` to `QString`, `unsigned int` to `uint`,
// and `QMap<int, QList<int> >` to `QMap<int,QList<int>>`.
inline std::string normalizeType(const std::string &input) {
    // Split into words and punctuation.
    std::vector<std::string> words;
    std::string w;
    const auto flush = [&] {
        if (!w.empty())
            words.push_back(w);
        w.clear();
    };
    for (char c : input) {
        if (c == '<' || c == '>' || c == ',' || c == '*' || c == '&' || c == '(' || c == ')' || c == '[' || c == ']') {
            flush();
            words.emplace_back(1, c);
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            flush();
        } else {
            w += c;
        }
    }
    flush();

    // Drop elaborated type specifiers and a leading ::
    std::vector<std::string> v;
    for (std::string &x : words) {
        if (x == "struct" || x == "class" || x == "enum" || x == "typename")
            continue;
        if (x.size() > 2 && x[0] == ':' && x[1] == ':' && (v.empty() || v.back() == "<" || v.back() == ","))
            x.erase(0, 2);
        v.push_back(x);
    }

    // Merge the multi-word fundamental types.
    std::vector<std::string> m;
    for (std::size_t i = 0; i < v.size();) {
        const std::string &x = v[i];
        if (x != "unsigned" && x != "signed" && x != "long" && x != "short" && x != "int" && x != "char") {
            m.push_back(x);
            ++i;
            continue;
        }
        bool isUnsigned = false, isSigned = false, isShort = false, isChar = false;
        int longs = 0;
        std::size_t j = i;
        for (; j < v.size(); ++j) {
            if (v[j] == "unsigned")
                isUnsigned = true;
            else if (v[j] == "signed")
                isSigned = true;
            else if (v[j] == "long")
                ++longs;
            else if (v[j] == "short")
                isShort = true;
            else if (v[j] == "char")
                isChar = true;
            else if (v[j] != "int")
                break;
        }
        if (isChar)
            m.push_back(isUnsigned ? "uchar" : (isSigned ? "signed char" : "char"));
        else if (isShort)
            m.push_back(isUnsigned ? "ushort" : "short");
        else if (longs >= 2)
            m.push_back(isUnsigned ? "qulonglong" : "qlonglong");
        else if (longs == 1)
            m.push_back(isUnsigned ? "ulong" : "long");
        else
            m.push_back(isUnsigned ? "uint" : "int");
        i = j;
    }

    // const goes away on values and on references to values, but stays on pointers.
    int depth = 0;
    bool hasPointer = false, hasConst = false;
    int refs = 0;
    for (const std::string &x : m) {
        if (x == "<" || x == "(" || x == "[")
            ++depth;
        else if (x == ">" || x == ")" || x == "]")
            --depth;
        else if (depth == 0 && x == "*")
            hasPointer = true;
        else if (depth == 0 && x == "const")
            hasConst = true;
        else if (depth == 0 && x == "&")
            ++refs;
    }
    const bool dropConst = !hasPointer && hasConst;
    const bool dropRef = dropConst && refs == 1;
    std::string out;
    depth = 0;
    for (const std::string &x : m) {
        if (x == "<" || x == "(" || x == "[")
            ++depth;
        else if (x == ">" || x == ")" || x == "]")
            --depth;
        else if (depth == 0 && x == "const" && dropConst)
            continue;
        else if (depth == 0 && x == "&" && dropRef)
            continue;
        const bool identLike = isIdentChar(x[0]);
        if (identLike && !out.empty() && isIdentChar(out.back()))
            out += ' ';
        out += x;
    }
    return out;
}

// Normalizes `name( const QString & , int )` to `name(QString,int)`.
inline std::string normalizeSignature(const std::string &sig) {
    const std::size_t open = sig.find('(');
    if (open == std::string::npos)
        return sig;
    const std::size_t close = sig.rfind(')');
    std::string name = sig.substr(0, open);
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
        name.pop_back();
    std::size_t b = 0;
    while (b < name.size() && std::isspace(static_cast<unsigned char>(name[b])))
        ++b;
    name.erase(0, b);
    std::string args = sig.substr(open + 1, (close == std::string::npos ? sig.size() : close) - open - 1);
    std::string out = name + "(";
    bool first = true;
    for (const std::string &p : splitTopLevel(args)) {
        const std::string n = normalizeType(p);
        if (n.empty() || n == "void")
            continue;
        if (!first)
            out += ",";
        out += n;
        first = false;
    }
    return out + ")";
}

} // namespace NtMeta
