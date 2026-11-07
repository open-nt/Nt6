// MIT License - see LICENSE
//
// ntmoc: the meta-object compiler of Nt. For each class that contains Q_OBJECT or Q_GADGET, and for
// each namespace that contains Q_NAMESPACE, it generates:
//
//  * the bodies of signals that are declared without one, e.g.
//
//        signals:
//            void valueChanged(int value);
//
//  * the class's staticMetaObject: its name, base class, signals, slots, Q_INVOKABLE methods and
//    constructors, Q_PROPERTY properties, Q_ENUM/Q_FLAG enumerations and Q_CLASSINFO entries;
//  * qt_static_metacall(), through which the run time invokes methods and reads and writes
//    properties.
//
// It is a text scanner, not a C++ parser. It understands namespaces, nested classes,
// access/signals/slots labels, overloaded signals and default arguments. Preprocessor
// conditionals are not evaluated, so a slot inside `#if 0` is seen. The command line follows the
// subset of Qt's moc that CMake's AUTOMOC uses.

#include "../../src/ntnormalize_p.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

#ifndef NT_VERSION_STRING
#define NT_VERSION_STRING "unknown"
#endif
constexpr const char *kVersion = "moc 6.0.0 (Nt ntmoc " NT_VERSION_STRING ")";

struct Token {
    std::string text;
    std::size_t pos;
    std::size_t end;
};

struct Param {
    std::string type;
    std::string name;
    bool hasDefault = false;
};

enum MethodKind { KindMethod = 0, KindSignal = 1, KindSlot = 2, KindConstructor = 3 };
enum Access { AccessPrivate = 0, AccessProtected = 1, AccessPublic = 2 };

struct Method {
    std::string name;
    std::string ret;
    std::vector<Param> params;
    bool privateSignal = false; // the last parameter is QPrivateSignal, which the meta-data leaves out
    MethodKind kind = KindMethod;
    Access access = AccessPrivate;
    bool isStatic = false;
    bool hasBody = false;
    bool scriptable = false; // declared with Q_SCRIPTABLE
    std::string tag;         // a tag macro in front of the return type, such as Q_NOREPLY
    int revision = 0;

    std::size_t metaParamCount() const { return params.size() - (privateSignal ? 1 : 0); }
};

struct Property {
    std::string name;
    std::string type;
    std::string read, write, reset, notify, member;
    bool constant = false, final_ = false, required = false;
    std::string designable = "true", scriptable = "true", stored = "true", user = "false";
    int revision = 0;
};

struct EnumInfo {
    std::string name;
    std::string qualName; // with the scope, for naming enumerators
    bool scoped = false;
    std::vector<std::string> keys;
};

struct EnumRef {
    std::string name;
    bool isFlag;
};

struct ClassData {
    std::string qualName;
    std::string name;
    std::vector<std::string> bases;
    std::vector<Method> methods;
    std::vector<Property> props;
    std::vector<EnumInfo> enums;
    std::vector<EnumRef> enumRefs;
    std::map<std::string, std::string> flagTypes; // Q_DECLARE_FLAGS: flags type -> enum
    std::vector<std::pair<std::string, std::string>> infos;
    bool gadget = false;
    bool qnamespace = false;
};

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Macros given with -D, and the ones given with -U.
std::map<std::string, std::string> &definedMacros() {
    static std::map<std::string, std::string> macros;
    return macros;
}

enum class Cond { False, True, Unknown };

// Evaluates the conditions that matter for the headers: 0, 1, NAME, !NAME, defined(NAME) and
// !defined(NAME), where only the macros given with -D are known. Everything else is Unknown and
// keeps both branches.
Cond evalCondition(const std::string &directive, std::string rest) {
    const auto &macros = definedMacros();
    const auto known = [&](const std::string &name) { return macros.find(name) != macros.end(); };
    const auto trim = [](std::string &x) {
        const std::size_t first = x.find_first_not_of(" \t");
        x = first == std::string::npos ? std::string() : x.substr(first);
        while (!x.empty() && std::isspace(static_cast<unsigned char>(x.back())))
            x.pop_back();
    };
    trim(rest);
    if (directive == "ifdef")
        return known(rest) ? Cond::True : Cond::Unknown;
    if (directive == "ifndef")
        return known(rest) ? Cond::False : Cond::Unknown;
    bool negate = false;
    if (!rest.empty() && rest[0] == '!') {
        negate = true;
        rest = rest.substr(1);
        trim(rest);
    }
    Cond result = Cond::Unknown;
    if (rest == "0" || rest == "false") {
        result = Cond::False;
    } else if (rest == "1" || rest == "true") {
        result = Cond::True;
    } else {
        std::string name = rest;
        if (name.rfind("defined", 0) == 0) {
            name = name.substr(7);
            trim(name);
            if (!name.empty() && name.front() == '(' && name.back() == ')') {
                name = name.substr(1, name.size() - 2);
                trim(name);
            }
            if (known(name))
                result = Cond::True;
        } else if (known(name)) {
            const std::string &value = macros.at(name);
            result = value == "0" ? Cond::False : Cond::True;
        }
        const bool simple = !name.empty() && std::all_of(name.begin(), name.end(), [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        });
        if (!simple)
            return Cond::Unknown;
    }
    if (negate && result != Cond::Unknown)
        return result == Cond::True ? Cond::False : Cond::True;
    return negate ? Cond::Unknown : result;
}

// Blanks the lines of conditional blocks that are known to be disabled (`#if 0`, and tests of the
// macros given with -D), keeping offsets and newlines. Other conditions are not evaluated: all of
// their branches are kept.
std::string blankDisabledBlocks(const std::string &s) {
    struct Frame {
        bool parentActive;
        bool active;
        bool taken; // a branch with a known true condition was seen
    };
    std::string out = s;
    std::vector<Frame> stack;
    bool active = true;
    std::size_t pos = 0;
    while (pos < s.size()) {
        std::size_t end = s.find('\n', pos);
        if (end == std::string::npos)
            end = s.size();
        std::size_t k = pos;
        while (k < end && (s[k] == ' ' || s[k] == '\t'))
            ++k;
        if (k < end && s[k] == '#') {
            ++k;
            while (k < end && (s[k] == ' ' || s[k] == '\t'))
                ++k;
            std::size_t w = k;
            while (w < end && std::isalnum(static_cast<unsigned char>(s[w])))
                ++w;
            const std::string directive = s.substr(k, w - k);
            const std::string rest = s.substr(w, end - w);
            if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
                const Cond c = evalCondition(directive, rest);
                stack.push_back({active, active && c != Cond::False, c == Cond::True});
                active = stack.back().active;
            } else if (directive == "elif" && !stack.empty()) {
                Frame &f = stack.back();
                if (f.taken) {
                    f.active = false;
                } else {
                    const Cond c = evalCondition("if", rest);
                    f.active = f.parentActive && c != Cond::False;
                    f.taken = c == Cond::True;
                }
                active = f.active;
            } else if (directive == "else" && !stack.empty()) {
                Frame &f = stack.back();
                f.active = f.parentActive && !f.taken;
                active = f.active;
            } else if (directive == "endif" && !stack.empty()) {
                active = stack.back().parentActive;
                stack.pop_back();
            }
        }
        if (!active)
            for (std::size_t i = pos; i < end; ++i)
                out[i] = ' ';
        pos = end + 1;
    }
    return out;
}

// Blanks comments, literals and preprocessor lines, keeping offsets and newlines.
std::string stripSource(const std::string &input) {
    const std::string s = blankDisabledBlocks(input);
    std::string out = s;
    const std::size_t n = s.size();
    auto blank = [&](std::size_t from, std::size_t to) {
        for (std::size_t k = from; k < to && k < n; ++k)
            if (out[k] != '\n')
                out[k] = ' ';
    };
    bool lineStart = true;
    for (std::size_t i = 0; i < n;) {
        const char c = s[i];
        if (c == '\n') {
            lineStart = true;
            ++i;
        } else if (c == ' ' || c == '\t' || c == '\r') {
            ++i;
        } else if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            std::size_t j = i;
            while (j < n && s[j] != '\n')
                ++j;
            blank(i, j);
            i = j;
        } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            std::size_t j = s.find("*/", i + 2);
            j = (j == std::string::npos) ? n : j + 2;
            blank(i, j);
            i = j;
            lineStart = false;
        } else if (c == '#' && lineStart) {
            std::size_t j = i;
            while (j < n && !(s[j] == '\n' && (j == 0 || s[j - 1] != '\\')))
                ++j;
            blank(i, j);
            i = j;
        } else if (c == '"' && i > 0 && s[i - 1] == 'R') {
            const std::size_t open = s.find('(', i);
            std::size_t j = n;
            if (open != std::string::npos) {
                const std::string close = ")" + s.substr(i + 1, open - i - 1) + "\"";
                const std::size_t e = s.find(close, open);
                if (e != std::string::npos)
                    j = e + close.size();
            }
            blank(i, j);
            i = j;
            lineStart = false;
        } else if (c == '"' || (c == '\'' && !(i > 0 && std::isdigit(static_cast<unsigned char>(s[i - 1]))))) {
            std::size_t j = i + 1;
            while (j < n && s[j] != c && s[j] != '\n')
                j += (s[j] == '\\') ? 2u : 1u;
            j = (j < n && s[j] == c) ? j + 1 : j;
            blank(i, j);
            i = j;
            lineStart = false;
        } else {
            lineStart = false;
            ++i;
        }
    }
    return out;
}

std::vector<Token> tokenize(const std::string &s) {
    std::vector<Token> t;
    const std::size_t n = s.size();
    for (std::size_t i = 0; i < n;) {
        const char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        std::size_t j = i + 1;
        if (isIdentStart(c)) {
            while (j < n && isIdentChar(s[j]))
                ++j;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            while (j < n && (isIdentChar(s[j]) || s[j] == '.' || s[j] == '\''))
                ++j;
        } else if ((c == ':' && j < n && s[j] == ':') || (c == '&' && j < n && s[j] == '&') ||
                   (c == '-' && j < n && s[j] == '>')) {
            ++j;
        }
        t.push_back({s.substr(i, j - i), i, j});
        i = j;
    }
    return t;
}

bool isBuiltinType(const std::string &w) {
    static const char *const kw[] = {"int",  "long", "short", "char",    "unsigned", "signed",   "double",  "float",
                                     "bool", "void", "auto",  "wchar_t", "char8_t",  "char16_t", "char32_t"};
    for (const char *k : kw)
        if (w == k)
            return true;
    return false;
}

bool isQualifier(const std::string &w) {
    return w == "const" || w == "volatile" || w == "struct" || w == "class" || w == "enum" || w == "typename";
}

std::string collapse(const std::string &s) {
    std::string r;
    bool space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            space = !r.empty();
        } else {
            if (space)
                r += ' ';
            space = false;
            r += c;
        }
    }
    return r;
}

std::string cString(const std::string &s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\')
            r += '\\';
        if (c == '\n') {
            r += "\\n";
            continue;
        }
        r += c;
    }
    return r + "\"";
}

// Splits `a, b<int, 2>, c` on top-level commas; returns token index ranges.
std::vector<std::pair<std::size_t, std::size_t>> splitParams(const std::vector<Token> &t, std::size_t from,
                                                             std::size_t to, bool angles = true) {
    std::vector<std::pair<std::size_t, std::size_t>> r;
    int depth = 0;
    std::size_t start = from;
    for (std::size_t i = from; i < to; ++i) {
        const std::string &x = t[i].text;
        if (x == "(" || (angles && x == "<") || x == "[" || x == "{")
            ++depth;
        else if (x == ")" || (angles && x == ">") || x == "]" || x == "}")
            --depth;
        else if (x == "," && depth == 0) {
            r.emplace_back(start, i);
            start = i + 1;
        }
    }
    if (start < to)
        r.emplace_back(start, to);
    return r;
}

// Index of the token that closes the bracket opened at `open`, or npos.
std::size_t matching(const std::vector<Token> &t, std::size_t open) {
    const std::string &o = t[open].text;
    const std::string c = o == "(" ? ")" : (o == "[" ? "]" : (o == "{" ? "}" : ">"));
    int depth = 0;
    for (std::size_t i = open; i < t.size(); ++i) {
        if (t[i].text == o)
            ++depth;
        else if (t[i].text == c && --depth == 0)
            return i;
    }
    return std::string::npos;
}

// Parses the parameters between the parentheses at tokens `open` and `close`.
std::vector<Param> parseParams(const std::string &src, const std::vector<Token> &t, std::size_t open,
                               std::size_t close) {
    std::vector<Param> params;
    for (auto [b, e] : splitParams(t, open + 1, close)) {
        bool hasDefault = false;
        int d = 0;
        for (std::size_t i = b; i < e; ++i) {
            const std::string &x = t[i].text;
            if (x == "(" || x == "<" || x == "[" || x == "{")
                ++d;
            else if (x == ")" || x == ">" || x == "]" || x == "}")
                --d;
            else if (x == "=" && d == 0) {
                e = i;
                hasDefault = true;
                break;
            }
        }
        if (e <= b)
            continue;
        if (e - b == 1 && t[b].text == "void")
            continue;

        Param p;
        p.hasDefault = hasDefault;
        // A pointer to a function: `void (*callback)(int)` names its parameter inside the parentheses.
        std::size_t fnName = 0;
        for (std::size_t i = b; i + 3 < e; ++i) {
            if (t[i].text == "(" && (t[i + 1].text == "*" || t[i + 1].text == "&") && isIdentStart(t[i + 2].text[0]) &&
                t[i + 3].text == ")") {
                fnName = i + 2;
                break;
            }
        }
        if (fnName != 0) {
            p.name = t[fnName].text;
            std::string type;
            for (std::size_t i = b; i < e; ++i) {
                if (i == fnName)
                    continue;
                if (!type.empty() && isIdentChar(type.back()) && isIdentChar(t[i].text[0]))
                    type += ' ';
                type += t[i].text;
            }
            p.type = type;
            params.push_back(std::move(p));
            continue;
        }
        std::size_t typeEnd = e;
        const Token &last = t[e - 1];
        if (e - b >= 2 && isIdentStart(last.text[0]) && !isBuiltinType(last.text) && !isQualifier(last.text)) {
            const std::string &prev = t[e - 2].text;
            bool typeOnlyBefore = true;
            for (std::size_t i = b; i + 1 < e; ++i)
                if (!isQualifier(t[i].text))
                    typeOnlyBefore = false;
            if (prev != "::" && prev != "<" && !typeOnlyBefore) {
                p.name = last.text;
                typeEnd = e - 1;
            }
        }
        p.type = collapse(src.substr(t[b].pos, t[typeEnd - 1].end - t[b].pos));
        params.push_back(std::move(p));
    }
    return params;
}

// The types in `public A, private virtual B<int, 2>`, without access or virtual.
std::vector<std::string> parseBases(const std::string &src, const std::vector<Token> &t, std::size_t from,
                                    std::size_t to) {
    std::vector<std::string> bases;
    for (auto [b, e] : splitParams(t, from, to)) {
        while (b < e &&
               (t[b].text == "public" || t[b].text == "protected" || t[b].text == "private" || t[b].text == "virtual"))
            ++b;
        if (b < e)
            bases.push_back(collapse(src.substr(t[b].pos, t[e - 1].end - t[b].pos)));
    }
    return bases;
}

// The string literals between `from` and `to` in the unstripped source.
std::vector<std::string> stringLiterals(const std::string &raw, std::size_t from, std::size_t to) {
    std::vector<std::string> r;
    for (std::size_t i = from; i < to && i < raw.size(); ++i) {
        if (raw[i] != '"')
            continue;
        std::string v;
        ++i;
        while (i < raw.size() && raw[i] != '"') {
            if (raw[i] == '\\' && i + 1 < raw.size()) {
                ++i;
                switch (raw[i]) {
                case 'n':
                    v += '\n';
                    break;
                case 't':
                    v += '\t';
                    break;
                default:
                    v += raw[i];
                    break;
                }
            } else {
                v += raw[i];
            }
            ++i;
        }
        r.push_back(std::move(v));
    }
    return r;
}

bool isMacroLike(const std::string &w) {
    if (w.size() < 2 || !std::isupper(static_cast<unsigned char>(w[0])))
        return false;
    for (char c : w)
        if (!(std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_'))
            return false;
    return true;
}

struct Scope {
    enum Kind { Namespace, Class, Other } kind = Other;
    std::string name;
    bool qobject = false;
    bool isTemplate = false;
    bool isStruct = false;
    Access access = AccessPrivate;
    int section = 0; // 1: signals, 2: slots
    ClassData data;
};

std::string qualify(const std::vector<Scope> &stack, const std::string &name) {
    std::string q;
    for (const Scope &s : stack) {
        if (s.kind == Scope::Other)
            return {};
        if (!s.name.empty())
            q += s.name + "::";
    }
    return q + name;
}

struct Scanner {
    const std::string &raw;
    std::string src;
    std::vector<Token> t;
    std::vector<ClassData> classes;
    std::vector<Scope> stack;
    std::map<std::string, EnumInfo> allEnums; // by qualified name

    explicit Scanner(const std::string &text) : raw(text), src(stripSource(text)), t(tokenize(src)) {}

    const std::string &at(std::size_t i) const {
        static const std::string empty;
        return i < t.size() ? t[i].text : empty;
    }

    void run();
    std::size_t parseEnum(std::size_t i, Scope &cur);
    std::size_t parseMacro(std::size_t i, Scope &cur);
    std::size_t parseProperty(std::size_t open, std::size_t close, Scope &cur);
    std::size_t parseMember(std::size_t i, Scope &cur);
    void closeScope(Scope &s);
};

// `enum [class|struct] Name [: base] { A, B = 2 };` at token i. Returns the index after it, or i
// if this is not an enumeration definition.
std::size_t Scanner::parseEnum(std::size_t i, Scope &cur) {
    std::size_t j = i + 1;
    bool scoped = false;
    if (at(j) == "class" || at(j) == "struct") {
        scoped = true;
        ++j;
    }
    std::string name;
    if (j < t.size() && isIdentStart(at(j)[0]) && at(j) != "final") {
        name = at(j);
        ++j;
    }
    if (at(j) == ":") {
        while (j < t.size() && at(j) != "{" && at(j) != ";")
            ++j;
    }
    if (at(j) != "{")
        return i;
    const std::size_t close = matching(t, j);
    if (close == std::string::npos)
        return i;
    EnumInfo e;
    e.name = name;
    e.scoped = scoped;
    for (auto [b, en] : splitParams(t, j + 1, close, false)) {
        std::size_t k = b;
        while (k < en && (at(k) == "[" || at(k) == "]" || at(k) == "deprecated" || at(k) == "maybe_unused"))
            ++k;
        if (k < en && isIdentStart(at(k)[0]))
            e.keys.push_back(at(k));
    }
    std::string scopeName;
    {
        std::vector<Scope> tmp(stack);
        scopeName = qualify(tmp, name);
    }
    e.qualName = scopeName;
    if (!name.empty()) {
        if (!scopeName.empty())
            allEnums[scopeName] = e;
        cur.data.enums.push_back(e);
    }
    return close + 1;
}

// Q_PROPERTY(type name READ r WRITE w ...). `open` and `close` are the parentheses.
std::size_t Scanner::parseProperty(std::size_t open, std::size_t close, Scope &cur) {
    static const std::set<std::string> keywords = {"READ",       "WRITE",      "RESET",    "NOTIFY",   "MEMBER",
                                                   "CONSTANT",   "FINAL",      "REQUIRED", "USER",     "STORED",
                                                   "DESIGNABLE", "SCRIPTABLE", "REVISION", "BINDABLE", "EDITABLE"};
    std::size_t first = close;
    int depth = 0;
    for (std::size_t i = open + 1; i < close; ++i) {
        if (at(i) == "<" || at(i) == "(")
            ++depth;
        else if (at(i) == ">" || at(i) == ")")
            --depth;
        else if (depth == 0 && keywords.count(at(i))) {
            first = i;
            break;
        }
    }
    if (first - (open + 1) < 2)
        return close + 1;
    Property p;
    p.name = at(first - 1);
    p.type = collapse(src.substr(t[open + 1].pos, t[first - 2].end - t[open + 1].pos));
    if (p.type == "const") // guards a malformed declaration
        return close + 1;
    for (std::size_t i = first; i < close;) {
        const std::string &k = at(i);
        const std::string v = i + 1 < close ? at(i + 1) : std::string();
        std::size_t next = i + 2;
        if (k == "READ")
            p.read = v;
        else if (k == "WRITE")
            p.write = v;
        else if (k == "RESET")
            p.reset = v;
        else if (k == "NOTIFY")
            p.notify = v;
        else if (k == "MEMBER")
            p.member = v;
        else if (k == "CONSTANT") {
            p.constant = true;
            next = i + 1;
        } else if (k == "FINAL") {
            p.final_ = true;
            next = i + 1;
        } else if (k == "REQUIRED") {
            p.required = true;
            next = i + 1;
        } else if (k == "REVISION") {
            if (v == "(") {
                const std::size_t c = matching(t, i + 1);
                std::vector<int> n;
                for (std::size_t m = i + 2; m < c; ++m)
                    if (std::isdigit(static_cast<unsigned char>(at(m)[0])))
                        n.push_back(std::stoi(at(m)));
                p.revision = n.size() == 2 ? ((n[0] << 8) | n[1]) : (n.size() == 1 ? (255 << 8) | n[0] : 0);
                next = c + 1;
            } else if (!v.empty() && std::isdigit(static_cast<unsigned char>(v[0]))) {
                p.revision = (255 << 8) | std::stoi(v);
            }
        } else if (k == "DESIGNABLE" || k == "SCRIPTABLE" || k == "STORED" || k == "USER" || k == "BINDABLE" ||
                   k == "EDITABLE") {
            std::string value = v;
            if (at(i + 2) == "(") {
                const std::size_t c = matching(t, i + 2);
                next = c == std::string::npos ? i + 3 : c + 1;
                value += "()";
            }
            if (k == "DESIGNABLE")
                p.designable = value;
            else if (k == "SCRIPTABLE")
                p.scriptable = value;
            else if (k == "STORED")
                p.stored = value;
            else if (k == "USER")
                p.user = value;
        }
        i = next;
    }
    cur.data.props.push_back(std::move(p));
    return close + 1;
}

// A Q_* macro that the scanner understands, at token i. Returns the index after it, or i.
std::size_t Scanner::parseMacro(std::size_t i, Scope &cur) {
    const std::string &x = t[i].text;
    if (at(i + 1) != "(")
        return i;
    const std::size_t close = matching(t, i + 1);
    if (close == std::string::npos)
        return i;
    if (x == "Q_PROPERTY")
        return parseProperty(i + 1, close, cur);
    if (x == "Q_ENUM" || x == "Q_ENUM_NS" || x == "Q_FLAG" || x == "Q_FLAG_NS" || x == "Q_ENUMS" || x == "Q_FLAGS") {
        const bool flag = x == "Q_FLAG" || x == "Q_FLAG_NS" || x == "Q_FLAGS";
        for (auto [b, e] : splitParams(t, i + 2, close))
            if (b < e)
                cur.data.enumRefs.push_back({collapse(src.substr(t[b].pos, t[e - 1].end - t[b].pos)), flag});
        return close + 1;
    }
    if (x == "Q_CLASSINFO") {
        const std::vector<std::string> lits = stringLiterals(raw, t[i + 1].pos, t[close].pos);
        if (lits.size() >= 2)
            cur.data.infos.emplace_back(lits[0], lits[1]);
        return close + 1;
    }
    if (x == "Q_DECLARE_FLAGS") {
        const auto parts = splitParams(t, i + 2, close);
        if (parts.size() == 2)
            cur.data.flagTypes[collapse(
                src.substr(t[parts[0].first].pos, t[parts[0].second - 1].end - t[parts[0].first].pos))] =
                collapse(src.substr(t[parts[1].first].pos, t[parts[1].second - 1].end - t[parts[1].first].pos));
        return close + 1;
    }
    return i;
}

// An upper-case word in front of a return type, such as Q_NOREPLY, is a tag for QMetaMethod::tag().
bool isTagMacro(const std::string &w) {
    if (w.size() < 2)
        return false;
    for (const char c : w)
        if (!(std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_'))
            return false;
    return true;
}

// A member declaration that starts at token i inside a class. Returns the index to continue at.
std::size_t Scanner::parseMember(std::size_t i, Scope &cur) {
    const std::size_t n = t.size();
    bool invokable = false, signalMark = false, slotMark = false, scriptable = false;
    int revision = 0;
    std::size_t b = i;
    // Markers in front of the declaration.
    for (;;) {
        const std::string &x = at(b);
        if (x == "Q_INVOKABLE") {
            invokable = true;
            ++b;
        } else if (x == "Q_SIGNAL") {
            signalMark = true;
            ++b;
        } else if (x == "Q_SLOT") {
            slotMark = true;
            ++b;
        } else if (x == "Q_SCRIPTABLE") {
            // Like Q_INVOKABLE, it makes a meta-method of a function outside the slot sections.
            scriptable = invokable = true;
            ++b;
        } else if (x == "Q_REVISION" && at(b + 1) == "(") {
            const std::size_t c = matching(t, b + 1);
            std::vector<int> v;
            for (std::size_t m = b + 2; m < c; ++m)
                if (std::isdigit(static_cast<unsigned char>(at(m)[0])))
                    v.push_back(std::stoi(at(m)));
            revision = v.size() == 2 ? ((v[0] << 8) | v[1]) : (v.size() == 1 ? (255 << 8) | v[0] : 0);
            b = c + 1;
        } else if (isMacroLike(x) && at(b + 1) == "(" && x != "Q_INVOKABLE") {
            // Some other macro invocation, such as Q_DISABLE_COPY(Foo), possibly without a semicolon.
            const std::size_t c = matching(t, b + 1);
            if (c == std::string::npos)
                break;
            const std::size_t next = at(c + 1) == ";" ? c + 2 : c + 1;
            if (b != i || next >= n)
                break;
            return next;
        } else {
            break;
        }
    }
    // Extent of the declaration: up to `;`, `{` or `}` outside parentheses.
    std::size_t j = b;
    int depth = 0;
    while (j < n) {
        const std::string &y = t[j].text;
        if (y == "(" || y == "[")
            ++depth;
        else if (y == ")" || y == "]")
            --depth;
        else if (depth == 0 && (y == ";" || y == "{" || y == "}"))
            break;
        ++j;
    }
    const std::size_t after = (j < n && t[j].text == ";") ? j + 1 : j;
    const std::string &first = at(b);
    if (first == "using" || first == "typedef" || first == "friend" || first == "template" ||
        first == "static_assert" || first == "class" || first == "struct" || first == "union" || first == "enum" ||
        first == "namespace" || first == "public" || first == "private" || first == "protected")
        return std::max(after, i + 1);

    const MethodKind kind = cur.section == 1 || signalMark ? KindSignal
                            : cur.section == 2 || slotMark ? KindSlot
                            : invokable                    ? KindMethod
                                                           : KindMethod;
    const bool wanted = cur.section != 0 || signalMark || slotMark || invokable;
    if (!wanted)
        return std::max(after, i + 1);

    std::size_t p = b;
    while (p < j && at(p) != "(")
        ++p;
    if (p >= j || p == b || !isIdentStart(at(p - 1)[0]))
        return std::max(after, i + 1);
    const std::size_t close = matching(t, p);
    if (close == std::string::npos || close > j)
        return std::max(after, i + 1);
    const std::string name = at(p - 1);
    if (p >= 2 && at(p - 2) == "~")
        return std::max(after, i + 1);

    Method m;
    m.name = name;
    m.revision = revision;
    m.scriptable = scriptable;
    std::string ret;
    std::size_t lead = b;
    bool isStatic = false;
    while (lead < p - 1) {
        const std::string &w = at(lead);
        if (w == "virtual" || w == "inline" || w == "explicit" || w == "constexpr" || w == "friend" || w == "extern" ||
            w == "Q_INVOKABLE" || w == "Q_SIGNAL" || w == "Q_SLOT") {
            ++lead;
        } else if (w == "Q_SCRIPTABLE") {
            m.scriptable = true;
            ++lead;
        } else if (w == "static") {
            isStatic = true;
            ++lead;
        } else if (isTagMacro(w) && lead + 1 < p - 1 && at(lead + 1) != "::" && at(lead + 1) != "<") {
            m.tag = w;
            ++lead;
        } else {
            break;
        }
    }
    if (lead < p - 1)
        ret = collapse(src.substr(t[lead].pos, t[p - 2].end - t[lead].pos));
    m.isStatic = isStatic;
    m.params = parseParams(src, t, p, close);
    if (!m.params.empty()) {
        const std::string &lt = m.params.back().type;
        if (lt == "QPrivateSignal" || (lt.size() > 15 && lt.compare(lt.size() - 15, 15, "::QPrivateSignal") == 0))
            m.privateSignal = true;
    }
    if (kind == KindSignal) {
        if (ret != "void")
            return std::max(after, i + 1);
        m.kind = KindSignal;
        m.access = AccessPublic;
        m.ret = "void";
        m.hasBody = at(j) == "{";
    } else if (ret.empty() && name == cur.name) {
        // A constructor; only Q_INVOKABLE ones are meta-methods.
        if (!invokable)
            return std::max(after, i + 1);
        m.kind = KindConstructor;
        m.access = cur.access;
    } else {
        if (ret.empty())
            return std::max(after, i + 1);
        m.kind = kind;
        m.access = cur.access;
        m.ret = ret;
    }
    cur.data.methods.push_back(std::move(m));
    return std::max(after, i + 1);
}

void Scanner::closeScope(Scope &s) {
    if (s.kind == Scope::Class && s.qobject) {
        if (s.isTemplate) {
            if (!s.data.methods.empty())
                std::cerr << "ntmoc: warning: signals and slots of template class " << s.name << " are not supported\n";
            return;
        }
        if (const std::string q = qualify(stack, s.name); !q.empty()) {
            s.data.qualName = q;
            s.data.name = s.name;
            classes.push_back(std::move(s.data));
        }
    } else if (s.kind == Scope::Namespace && s.data.qnamespace) {
        if (const std::string q = qualify(stack, s.name); !q.empty()) {
            s.data.qualName = q;
            s.data.name = s.name;
            classes.push_back(std::move(s.data));
        }
    }
}

void Scanner::run() {
    const std::size_t n = t.size();
    for (std::size_t i = 0; i < n;) {
        const std::string &x = t[i].text;
        Scope *cur = stack.empty() ? nullptr : &stack.back();

        if (x == "namespace") {
            std::size_t j = i + 1;
            std::string name;
            while (j < n && t[j].text != "{" && t[j].text != ";" && t[j].text != "=") {
                if (t[j].text != "inline")
                    name += t[j].text;
                ++j;
            }
            if (at(j) == "{") {
                Scope s;
                s.kind = (cur && cur->kind == Scope::Other) ? Scope::Other : Scope::Namespace;
                s.name = name;
                stack.push_back(std::move(s));
                i = j + 1;
            } else {
                i = j;
            }
            continue;
        }

        if ((x == "class" || x == "struct") && (i == 0 || (at(i - 1) != "enum" && at(i - 1) != "friend")) &&
            isIdentStart(at(i + 1).empty() ? ' ' : at(i + 1)[0])) {
            // class [attrs] Name [final] [: bases] {
            std::size_t j = i + 1;
            const std::string name = t[j].text;
            ++j;
            if (at(j) == "final")
                ++j;
            std::size_t basesFrom = j;
            if (at(j) == ":") {
                basesFrom = j + 1;
                while (j < n && t[j].text != "{" && t[j].text != ";")
                    ++j;
            }
            if (at(j) == "{") {
                Scope s;
                s.data.bases = parseBases(src, t, basesFrom, j);
                s.kind = (cur && cur->kind == Scope::Other) ? Scope::Other : Scope::Class;
                s.name = name;
                s.isStruct = x == "struct";
                s.access = s.isStruct ? AccessPublic : AccessPrivate;
                s.isTemplate = i > 0 && at(i - 1) == ">";
                stack.push_back(std::move(s));
                i = j + 1;
                continue;
            }
        }

        if (x == "{") {
            stack.push_back(Scope{});
            ++i;
            continue;
        }
        if (x == "}") {
            if (!stack.empty()) {
                Scope s = std::move(stack.back());
                stack.pop_back();
                closeScope(s);
            }
            ++i;
            continue;
        }

        if (cur && (cur->kind == Scope::Class || cur->kind == Scope::Namespace)) {
            const bool inClass = cur->kind == Scope::Class;
            if (x == "enum" && (i == 0 || at(i - 1) != "(")) {
                const std::size_t next = parseEnum(i, *cur);
                if (next != i) {
                    i = next;
                    continue;
                }
            }
            if (x == "Q_OBJECT" && inClass) {
                cur->qobject = true;
                ++i;
                continue;
            }
            if (x == "Q_GADGET" && inClass) {
                cur->qobject = true;
                cur->data.gadget = true;
                ++i;
                continue;
            }
            if (x == "Q_NAMESPACE" && !inClass) {
                cur->data.qnamespace = true;
                ++i;
                continue;
            }
            if (x.compare(0, 2, "Q_") == 0) {
                const std::size_t next = parseMacro(i, *cur);
                if (next != i) {
                    i = next;
                    continue;
                }
            }
            if (inClass) {
                // Access labels and signals/slots sections.
                std::size_t k = i;
                bool label = false;
                Access access = cur->access;
                if (x == "public" || x == "private" || x == "protected") {
                    access = x == "public" ? AccessPublic : (x == "protected" ? AccessProtected : AccessPrivate);
                    ++k;
                    label = true;
                }
                int section = 0;
                if (at(k) == "signals" || at(k) == "Q_SIGNALS") {
                    section = 1;
                    ++k;
                    label = true;
                } else if (at(k) == "slots" || at(k) == "Q_SLOTS") {
                    section = 2;
                    ++k;
                    label = true;
                }
                if (label && at(k) == ":") {
                    if (section != 1)
                        cur->access = access;
                    cur->section = section;
                    i = k + 1;
                    continue;
                }
                i = parseMember(i, *cur);
                continue;
            }
        }
        ++i;
    }
}

// Finds the enumeration that a Q_ENUM(name) or Q_FLAG(name) in `c` refers to.
const EnumInfo *findEnum(const ClassData &c, const std::map<std::string, EnumInfo> &all, const std::string &name) {
    const std::size_t colon = name.rfind("::");
    const std::string last = colon == std::string::npos ? name : name.substr(colon + 2);
    for (const EnumInfo &e : c.enums)
        if (e.name == last)
            return &e;
    for (const auto &kv : all)
        if (kv.first == name ||
            (kv.first.size() > name.size() && kv.first.compare(kv.first.size() - name.size(), name.size(), name) == 0 &&
             kv.first.compare(kv.first.size() - name.size() - 2, 2, "::") == 0))
            return &kv.second;
    return nullptr;
}

struct MetaEntry {
    const Method *m;
    std::size_t paramCount;
    bool cloned;
};

std::string joinTypes(const std::vector<Param> &params, std::size_t n) {
    std::string r;
    for (std::size_t i = 0; i < n; ++i)
        r += (i ? ", " : "") + params[i].type;
    return r;
}

std::string argList(const Method &m, std::size_t n, std::size_t firstArg) {
    std::string r;
    for (std::size_t i = 0; i < n; ++i)
        r += (i ? ", " : "") + std::string("QtPrivate::metaArg<") + m.params[i].type + ">(_a[" +
             std::to_string(i + firstArg) + "])";
    // A signal that ends with QPrivateSignal is called with one.
    if (m.privateSignal && n == m.metaParamCount())
        r += std::string(n ? ", " : "") + "QPrivateSignal()";
    return r;
}

// Class members, in the order of the meta-data: signals, slots, then methods, each with the
// shorter overloads that default arguments allow right after it.
std::vector<MetaEntry> methodEntries(const ClassData &c, MethodKind kind) {
    std::vector<MetaEntry> entries;
    for (const Method &m : c.methods) {
        if (m.kind != kind)
            continue;
        const std::size_t total = m.metaParamCount();
        entries.push_back({&m, total, false});
        std::size_t n = total;
        while (n > 0 && m.params[n - 1].hasDefault) {
            --n;
            entries.push_back({&m, n, true});
        }
    }
    return entries;
}

void generateSignalBodies(std::ostringstream &o, const ClassData &c) {
    for (const Method &s : c.methods) {
        if (s.kind != KindSignal || s.hasBody)
            continue;
        std::string types, decl, args;
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const std::string a = "_a" + std::to_string(i);
            types += (i ? ", " : "") + s.params[i].type;
            decl += (i ? ", " : "") + s.params[i].type + " " + a;
            args += ", " + a;
        }
        o << "void " << c.qualName << "::" << s.name << "(" << decl << ")\n{\n"
          << "    emitSignal(static_cast<void (" << c.qualName << "::*)(" << types << ")>(&" << c.qualName
          << "::" << s.name << ")" << args << ");\n}\n";
    }
}

int flagsOf(const Property &p) {
    int f = 0;
    const bool member = !p.member.empty();
    if (!p.read.empty() || member)
        f |= 0x1;
    if (!p.write.empty() || (member && !p.constant))
        f |= 0x2;
    if (!p.reset.empty())
        f |= 0x4;
    if (p.constant)
        f |= 0x8;
    if (p.final_)
        f |= 0x10;
    if (p.required)
        f |= 0x20;
    if (member)
        f |= 0x40;
    return f;
}

std::string queryBits(const std::string &v, int shift) {
    int bits = v == "true" ? 1 : (v == "false" ? 0 : 2);
    return "(" + std::to_string(bits) + "u << " + std::to_string(shift) + ")";
}

void generateMeta(std::ostringstream &o, const ClassData &c, const std::map<std::string, EnumInfo> &allEnums) {
    const std::string &cn = c.qualName;
    const bool isClass = !c.qnamespace;
    const std::vector<MetaEntry> signals_ = methodEntries(c, KindSignal);
    const std::vector<MetaEntry> slots = methodEntries(c, KindSlot);
    const std::vector<MetaEntry> methods = methodEntries(c, KindMethod);
    std::vector<MetaEntry> all;
    all.insert(all.end(), signals_.begin(), signals_.end());
    all.insert(all.end(), slots.begin(), slots.end());
    all.insert(all.end(), methods.begin(), methods.end());
    const std::vector<MetaEntry> ctors = methodEntries(c, KindConstructor);

    // Enumerations that Q_ENUM and Q_FLAG name.
    struct Enumerator {
        std::string name;
        bool isFlag;
        bool scoped;
        const EnumInfo *info;
    };
    std::vector<Enumerator> enums;
    for (const EnumRef &r : c.enumRefs) {
        std::string target = r.name;
        const auto flag = c.flagTypes.find(r.name);
        if (flag != c.flagTypes.end())
            target = flag->second;
        const EnumInfo *e = findEnum(c, allEnums, target);
        if (!e) {
            std::cerr << "ntmoc: warning: cannot find the enumeration " << r.name << " of " << cn << "\n";
            continue;
        }
        const std::size_t colon = r.name.rfind("::");
        const std::string shown = colon == std::string::npos ? r.name : r.name.substr(colon + 2);
        bool dup = false;
        for (const Enumerator &x : enums)
            dup = dup || x.name == shown;
        if (!dup)
            enums.push_back({shown, r.isFlag || flag != c.flagTypes.end(), e->scoped, e});
    }

    if (isClass) {
        // qt_static_metacall
        o << "\nvoid " << cn << "::qt_static_metacall([[maybe_unused]] QObject *_o, QMetaObject::Call _c, "
          << "[[maybe_unused]] int _id, [[maybe_unused]] void **_a)\n{\n";
        const std::string cast = c.gadget ? "reinterpret_cast<" + cn + " *>(_o)" : "static_cast<" + cn + " *>(_o)";
        o << "    [[maybe_unused]] " << cn << " *_t = " << cast << ";\n    switch (_c) {\n";

        o << "    case QMetaObject::InvokeMetaMethod:\n        switch (_id) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
            const Method &m = *all[i].m;
            const std::size_t np = all[i].paramCount;
            const std::string call = (m.isStatic ? cn + "::" : "_t->") + m.name + "(" + argList(m, np, 1) + ")";
            if (m.kind == KindSignal || m.ret == "void") {
                o << "        case " << i << ": " << call << "; break;\n";
            } else {
                o << "        case " << i << ": {\n            auto _r = " << call << ";\n"
                  << "            if (_a[0])\n                "
                     "*reinterpret_cast<std::remove_cv_t<std::remove_reference_t<"
                  << m.ret << ">> *>(_a[0]) = std::move(_r);\n        } break;\n";
            }
        }
        o << "        default: break;\n        }\n        break;\n";

        if (!ctors.empty()) {
            o << "    case QMetaObject::CreateInstance:\n        switch (_id) {\n";
            for (std::size_t i = 0; i < ctors.size(); ++i) {
                const Method &m = *ctors[i].m;
                o << "        case " << i << ": {\n            QObject *_r = new " << cn << "("
                  << argList(m, ctors[i].paramCount, 1)
                  << ");\n            *reinterpret_cast<QObject **>(_a[0]) = _r;\n        } break;\n";
            }
            o << "        default: break;\n        }\n        break;\n";
        }

        o << "    case QMetaObject::RegisterMethodArgumentMetaType:\n        switch (_id) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
            const Method &m = *all[i].m;
            o << "        case " << i << ":\n            switch (*reinterpret_cast<int *>(_a[1])) {\n";
            o << "            case -1: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType<"
              << (m.kind == KindSignal ? "void" : m.ret) << ">(); break;\n";
            for (std::size_t k = 0; k < all[i].paramCount; ++k)
                o << "            case " << k << ": *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType<"
                  << m.params[k].type << ">(); break;\n";
            o << "            default: break;\n            }\n            break;\n";
        }
        o << "        default: break;\n        }\n        break;\n";

        if (!c.props.empty()) {
            o << "    case QMetaObject::RegisterPropertyMetaType:\n        switch (_id) {\n";
            for (std::size_t i = 0; i < c.props.size(); ++i)
                o << "        case " << i << ": *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType<"
                  << c.props[i].type << ">(); break;\n";
            o << "        default: break;\n        }\n        break;\n";

            o << "    case QMetaObject::ReadProperty:\n        switch (_id) {\n";
            for (std::size_t i = 0; i < c.props.size(); ++i) {
                const Property &p = c.props[i];
                if (!p.read.empty())
                    o << "        case " << i << ": *reinterpret_cast<" << p.type << " *>(_a[0]) = _t->" << p.read
                      << "(); break;\n";
                else if (!p.member.empty())
                    o << "        case " << i << ": *reinterpret_cast<" << p.type << " *>(_a[0]) = _t->" << p.member
                      << "; break;\n";
            }
            o << "        default: break;\n        }\n        break;\n";

            o << "    case QMetaObject::WriteProperty:\n        switch (_id) {\n";
            for (std::size_t i = 0; i < c.props.size(); ++i) {
                const Property &p = c.props[i];
                if (!p.write.empty()) {
                    o << "        case " << i << ": _t->" << p.write << "(*reinterpret_cast<" << p.type
                      << " *>(_a[0])); break;\n";
                } else if (!p.member.empty() && !p.constant) {
                    o << "        case " << i << ": {\n            const auto &_v = *reinterpret_cast<" << p.type
                      << " *>(_a[0]);\n            if (QtPrivate::metaNotEqual(_t->" << p.member << ", _v)) {\n"
                      << "                _t->" << p.member << " = _v;\n";
                    if (!p.notify.empty()) {
                        // The notify signal takes the new value if it has a parameter.
                        bool withValue = false;
                        for (const Method &m : c.methods)
                            if (m.kind == KindSignal && m.name == p.notify && m.metaParamCount() >= 1) {
                                withValue = true;
                                break;
                            }
                        o << "                _t->" << p.notify << "(" << (withValue ? "_t->" + p.member : "")
                          << ");\n";
                    }
                    o << "            }\n        } break;\n";
                }
            }
            o << "        default: break;\n        }\n        break;\n";

            o << "    case QMetaObject::ResetProperty:\n        switch (_id) {\n";
            for (std::size_t i = 0; i < c.props.size(); ++i)
                if (!c.props[i].reset.empty())
                    o << "        case " << i << ": _t->" << c.props[i].reset << "(); break;\n";
            o << "        default: break;\n        }\n        break;\n";

            const char *names[4] = {"QueryPropertyDesignable", "QueryPropertyScriptable", "QueryPropertyStored",
                                    "QueryPropertyUser"};
            for (int q = 0; q < 4; ++q) {
                o << "    case QMetaObject::" << names[q] << ":\n        switch (_id) {\n";
                for (std::size_t i = 0; i < c.props.size(); ++i) {
                    const Property &p = c.props[i];
                    const std::string &v =
                        q == 0 ? p.designable : (q == 1 ? p.scriptable : (q == 2 ? p.stored : p.user));
                    if (v != "true" && v != "false") {
                        const bool isCall = v.size() > 2 && v.compare(v.size() - 2, 2, "()") == 0;
                        o << "        case " << i << ": *reinterpret_cast<bool *>(_a[0]) = static_cast<bool>(_t->" << v
                          << (isCall ? "" : "()") << "); break;\n";
                    }
                }
                o << "        default: break;\n        }\n        break;\n";
            }
        }
        o << "    default: break;\n    }\n}\n";
    }

    // staticMetaObject
    o << "\nconst QMetaObject " << cn << "::staticMetaObject = [] {\n";
    auto typesArray = [&](const std::string &id, const Method &m, std::size_t np, bool names) {
        if (np == 0)
            return std::string("nullptr");
        o << "    static const char *const " << id << "[] = {";
        for (std::size_t k = 0; k < np; ++k)
            o << (k ? ", " : "") << cString(names ? m.params[k].name : NtMeta::normalizeType(m.params[k].type));
        o << "};\n";
        return id;
    };
    auto methodTable = [&](const std::string &prefix, const std::vector<MetaEntry> &entries) {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const Method &m = *entries[i].m;
            o << "    // " << m.name << "\n";
            const std::string t = typesArray(prefix + std::to_string(i) + "_t", m, entries[i].paramCount, false);
            const std::string nm = typesArray(prefix + std::to_string(i) + "_n", m, entries[i].paramCount, true);
            (void)t;
            (void)nm;
        }
        if (entries.empty())
            return;
        o << "    static const QtPrivate::MetaMethodData " << prefix << "[] = {\n";
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const Method &m = *entries[i].m;
            const std::size_t np = entries[i].paramCount;
            const std::string ret = m.kind == KindConstructor ? std::string() : NtMeta::normalizeType(m.ret);
            o << "        {" << cString(m.name) << ", " << cString(ret) << ", "
              << (np ? prefix + std::to_string(i) + "_t" : "nullptr") << ", "
              << (np ? prefix + std::to_string(i) + "_n" : "nullptr") << ", " << np << ", " << static_cast<int>(m.kind)
              << ", " << static_cast<int>(m.access) << ", " << ((entries[i].cloned ? 1 : 0) | (m.scriptable ? 2 : 0))
              << ", " << m.revision << ", " << (m.tag.empty() ? std::string("nullptr") : cString(m.tag)) << ", ";
            if (m.kind == KindSignal) {
                o << "[]() { return QtPrivate::SignalId::of(static_cast<void (" << cn << "::*)("
                  << joinTypes(m.params, m.params.size()) << ")>(&" << cn << "::" << m.name << ")); }";
            } else {
                o << "nullptr";
            }
            o << "},\n";
        }
        o << "    };\n";
    };
    methodTable("m", all);
    methodTable("ctor", ctors);

    if (!c.props.empty()) {
        o << "    static const QtPrivate::MetaPropertyData props[] = {\n";
        for (const Property &p : c.props) {
            int notify = -1;
            for (std::size_t i = 0; i < signals_.size() && !p.notify.empty(); ++i)
                if (!signals_[i].cloned && signals_[i].m->name == p.notify) {
                    notify = static_cast<int>(i);
                    break;
                }
            o << "        {" << cString(p.name) << ", " << cString(NtMeta::normalizeType(p.type)) << ", " << flagsOf(p)
              << "u | " << queryBits(p.designable, 8) << " | " << queryBits(p.scriptable, 10) << " | "
              << queryBits(p.stored, 12) << " | " << queryBits(p.user, 14) << ", " << notify << ", " << p.revision
              << "},\n";
        }
        o << "    };\n";
    }

    for (std::size_t i = 0; i < enums.size(); ++i) {
        const EnumInfo &e = *enums[i].info;
        o << "    static const QtPrivate::MetaEnumKey enum" << i << "[] = {";
        for (const std::string &k : e.keys)
            o << "{" << cString(k) << ", static_cast<int>(" << e.qualName << "::" << k << ")}, ";
        o << "};\n";
    }
    if (!enums.empty()) {
        o << "    static const QtPrivate::MetaEnumData enums[] = {\n";
        for (std::size_t i = 0; i < enums.size(); ++i)
            o << "        {" << cString(enums[i].name) << ", " << (enums[i].isFlag ? "true" : "false") << ", "
              << (enums[i].scoped ? "true" : "false") << ", " << enums[i].info->keys.size() << ", enum" << i << "},\n";
        o << "    };\n";
    }
    if (!c.infos.empty()) {
        o << "    static const QtPrivate::MetaClassInfoData infos[] = {\n";
        for (const auto &kv : c.infos)
            o << "        {" << cString(kv.first) << ", " << cString(kv.second) << "},\n";
        o << "    };\n";
    }
    o << "    static const QtPrivate::MetaObjectData data = {\n        " << cString(cn) << ",\n        "
      << (all.empty() ? "nullptr" : "m") << ", " << all.size() << ",\n        " << (ctors.empty() ? "nullptr" : "ctor")
      << ", " << ctors.size() << ",\n        " << (c.props.empty() ? "nullptr" : "props") << ", " << c.props.size()
      << ",\n        " << (enums.empty() ? "nullptr" : "enums") << ", " << enums.size() << ",\n        "
      << (c.infos.empty() ? "nullptr" : "infos") << ", " << c.infos.size() << "};\n";
    if (isClass && !c.gadget) {
        const std::string base = c.bases.empty() ? std::string() : c.bases.front();
        o << "    return QMetaObject{{"
          << (base.empty() ? "QtPrivate::registerMetaObject(typeid(" + cn + "), &" + cn +
                                 "::staticMetaObject, nullptr, "
                                 "[](const QObject *o) { return dynamic_cast<const " +
                                 cn + " *>(o) != nullptr; })"
                           : "QtPrivate::registerMetaObject(typeid(" + cn + "), &" + cn + "::staticMetaObject, &" +
                                 base + "::staticMetaObject, [](const QObject *o) { return dynamic_cast<const " + cn +
                                 " *>(o) != nullptr; })")
          << ", &data, &" << cn << "::qt_static_metacall}};\n";
    } else if (isClass) {
        o << "    QtPrivate::registerMetaObject(typeid(" << cn << "), &" << cn
          << "::staticMetaObject, nullptr, nullptr);\n"
          << "    return QMetaObject{{nullptr, &data, &" << cn << "::qt_static_metacall}};\n";
    } else {
        o << "    return QMetaObject{{nullptr, &data, nullptr}};\n";
    }
    o << "}();\n";
}

std::string generate(const std::vector<ClassData> &classes, const std::map<std::string, EnumInfo> &allEnums,
                     const std::string &includePath) {
    std::ostringstream o;
    o << "// Generated by ntmoc. Do not edit.\n";
    if (!includePath.empty())
        o << "#include \"" << includePath << "\"\n";
    o << "#include <QtCore/qmetaobject.h>\n#include <QtCore/qobject.h>\n\n#include <type_traits>\n#include "
         "<typeinfo>\n#include <utility>\n";
    for (const ClassData &c : classes) {
        o << "\n";
        generateSignalBodies(o, c);
        generateMeta(o, c, allEnums);
    }
    return o.str();
}

std::vector<std::string> expandResponseFiles(int argc, char **argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.size() > 1 && a[0] == '@') {
            std::ifstream f(a.substr(1));
            std::string line;
            while (std::getline(f, line)) {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
                    line = line.substr(1, line.size() - 2);
                if (!line.empty())
                    args.push_back(line);
            }
        } else {
            args.push_back(a);
        }
    }
    return args;
}

} // namespace

int main(int argc, char **argv) {
    const std::vector<std::string> args = expandResponseFiles(argc, argv);

    std::string output, input;
    bool depFile = false, noInclude = false;
    // Options that consume a value (separate or attached) and are otherwise ignored.
    const std::vector<std::string> valued = {"-I", "-D", "-U", "-F", "-f", "-p", "-b", "-M",
                                              "--include", "--dep-file-path", "--dep-file-rule-name",
                                              "--output-json", "-X"};

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string &a = args[i];
        if (a == "-v" || a == "--version") {
            std::cout << kVersion << "\n";
            return 0;
        }
        if (a == "-h" || a == "--help") {
            std::cout << "Usage: ntmoc [options] -o <output.cpp> <input>\n"
                         "Generates the meta-object of each Q_OBJECT and Q_GADGET class and\n"
                         "Q_NAMESPACE namespace, and bodies for signals declared without one.\n"
                         "  -o <file>            output file (default: stdout)\n"
                         "  -i                   do not #include the input in the output\n"
                         "  --output-dep-file    write <output>.d\n"
                         "Other moc options are accepted and ignored.\n";
            return 0;
        }
        if (a == "-o" && i + 1 < args.size()) {
            output = args[++i];
        } else if (a.rfind("-o", 0) == 0 && a.size() > 2) {
            output = a.substr(2);
        } else if (a == "-i") {
            noInclude = true;
        } else if (a == "--output-dep-file") {
            depFile = true;
        } else if (a == "-E" || a == "-n" || a.rfind("-n", 0) == 0 || a == "--no-warnings" ||
                   a == "--no-notes" || a == "--debug-includes" || a == "--collect-includes") {
            // ignored
        } else if (!a.empty() && a[0] == '-' && a.size() > 1) {
            bool matched = false;
            const bool defineArg = a.rfind("-D", 0) == 0;
            if (defineArg) {
                const std::string def = a.size() > 2 ? a.substr(2) : (i + 1 < args.size() ? args[i + 1] : "");
                const std::size_t eq = def.find('=');
                definedMacros()[def.substr(0, eq)] = eq == std::string::npos ? "1" : def.substr(eq + 1);
            } else if (a.rfind("-U", 0) == 0) {
                definedMacros().erase(a.size() > 2 ? a.substr(2) : (i + 1 < args.size() ? args[i + 1] : ""));
            }
            for (const std::string &v : valued) {
                if (a == v) {
                    ++i;
                    matched = true;
                    break;
                }
                if (v.size() == 2 && a.rfind(v, 0) == 0) {
                    matched = true;
                    break;
                }
            }
            if (!matched)
                std::cerr << "ntmoc: warning: ignoring unknown option " << a << "\n";
        } else {
            input = a;
        }
    }

    if (input.empty()) {
        std::cerr << "ntmoc: no input file\n";
        return 1;
    }
    std::ifstream in(input, std::ios::binary);
    if (!in) {
        std::cerr << "ntmoc: cannot read " << input << "\n";
        return 1;
    }
    std::stringstream buf;
    buf << in.rdbuf();

    // Only headers are included; a .cpp input is the file that will include the output.
    const std::size_t dot = input.rfind('.');
    const std::string ext = dot == std::string::npos ? "" : input.substr(dot);
    const bool isHeader = ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".hxx" || ext == ".H";
    const std::string source = buf.str();
    Scanner scanner(source);
    scanner.run();
    const std::string text = generate(scanner.classes, scanner.allEnums, (isHeader && !noInclude) ? input : "");

    if (output.empty()) {
        std::cout << text;
        return 0;
    }
    {
        std::ofstream out(output, std::ios::binary);
        if (!out) {
            std::cerr << "ntmoc: cannot write " << output << "\n";
            return 1;
        }
        out << text;
    }
    if (depFile) {
        std::ofstream dep(output + ".d");
        dep << output << ": " << input << "\n";
    }
    return 0;
}
