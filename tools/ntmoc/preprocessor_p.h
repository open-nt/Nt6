// MIT License - see LICENSE
//
// The preprocessor of ntmoc. Like Qt's moc, ntmoc has to know which branch of each #if is compiled,
// or it would put slots and properties that the compiler never sees into the meta-object. This
// preprocessor evaluates the conditionals and blanks the lines of the branches that are not compiled,
// keeping newlines. It leaves the code as it is, except for a macro of the program's own that
// declares something, or whose expansion has a word for the meta-object in it, such as Q_PROPERTY or
// signals: that one is expanded where it is used, on as many lines as it took.
//
// It follows the C preprocessor where that is possible:
//
//  * #if and #elif expressions with defined, the arithmetic, bitwise, logical, relational and
//    conditional operators, integer and character literals, and object-like and function-like
//    macros (with # and ##). An identifier that is not a macro is 0.
//  * #ifdef, #ifndef, #elifdef and #elifndef, #else and #endif.
//  * #define and #undef, also in the files it includes.
//  * #include, #include_next and #import, searched next to the including file and in the -I
//    directories; files that are not found are skipped, as moc does.
//  * #pragma once, #pragma push_macro("NAME") and #pragma pop_macro("NAME"). Other pragmas,
//    including moc's qt_no_included_check, and #error, #warning and #line are accepted and ignored.
//  * __has_include and __has_include_next.
//
// Where it cannot know the answer it keeps every branch, which is what ntmoc always did: the
// condition uses __has_cpp_attribute and the like, a function-like macro that is not defined (often
// one from a header that was not found), a __has_include file that was not found, or a macro that
// was defined in such an uncertain branch.

#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace ntmoc {

enum class Cond { False, True, Unknown };

class Preprocessor {
public:
    // The -I directories, searched in order.
    std::vector<std::string> includeDirs;
    // The files read through #include and --include, for the dependency file.
    std::vector<std::string> dependencies;

    Preprocessor() {
        define("Q_MOC_RUN");
        define("__cplusplus=201703L");
    }

    // Defines a macro given as NAME, NAME=VALUE or NAME(ARGS)=VALUE, as with -D.
    void define(const std::string &def) {
        const std::size_t eq = def.find('=');
        std::string line = def.substr(0, eq) + " " + (eq == std::string::npos ? std::string("1") : def.substr(eq + 1));
        defineLine(line, true);
    }

    // Removes the macro called \a name, as with -U.
    void undefine(const std::string &name) { macros_.erase(name); }

    // Reads the file at \a path for its macros only, as with --include. Returns false if it cannot be read.
    bool includeFile(const std::string &path) {
        const std::string *text = readFile(normalize(path));
        if (!text)
            return false;
        addDependency(normalize(path));
        process(*text, normalize(path), Cond::True, false);
        return true;
    }

    // Returns \a text, the contents of the file at \a path, with the lines of the branches that are not
    // compiled replaced by spaces.
    std::string run(const std::string &text, const std::string &path) {
        return process(text, normalize(path), Cond::True, true);
    }

private:
    struct Macro {
        bool function = false;
        bool variadic = false;
        bool uncertain = false; // defined or undefined in a branch whose condition is unknown
        std::vector<std::string> params;
        std::vector<std::string> body;
    };

    struct Value {
        long long v = 0;
        bool unknown = false;
    };

    std::map<std::string, Macro> macros_;
    std::map<std::string, std::vector<std::optional<Macro>>> pushed_;
    std::set<std::string> once_;
    std::map<std::string, std::string> files_;
    int depth_ = 0;

    static bool identStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
    static bool identChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
    static bool isIdent(const std::string &t) { return !t.empty() && identStart(t[0]); }

    static std::string trim(const std::string &s) {
        const std::size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return {};
        const std::size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static std::string normalize(const std::string &path) {
        std::error_code ec;
        std::filesystem::path p = std::filesystem::absolute(path, ec);
        if (ec)
            p = path;
        return p.lexically_normal().string();
    }

    void addDependency(const std::string &path) {
        for (const std::string &d : dependencies)
            if (d == path)
                return;
        dependencies.push_back(path);
    }

    const std::string *readFile(const std::string &path) {
        const auto it = files_.find(path);
        if (it != files_.end())
            return &it->second;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
            return nullptr;
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return nullptr;
        std::stringstream buf;
        buf << in.rdbuf();
        return &(files_[path] = buf.str());
    }

    // Returns \a s with comments replaced by spaces, keeping newlines. Literals are skipped, not blanked,
    // so that #include "file" keeps its name.
    static std::string blankComments(const std::string &s) {
        std::string out = s;
        const std::size_t n = s.size();
        for (std::size_t i = 0; i < n;) {
            const char c = s[i];
            if (c == '/' && i + 1 < n && s[i + 1] == '/') {
                while (i < n && s[i] != '\n') {
                    // A line comment continues on the next line after a backslash.
                    if (s[i] == '\\' && i + 1 < n && s[i + 1] == '\n') {
                        out[i] = ' ';
                        i += 2;
                        continue;
                    }
                    out[i++] = ' ';
                }
            } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
                std::size_t j = s.find("*/", i + 2);
                j = j == std::string::npos ? n : j + 2;
                for (; i < j; ++i)
                    if (out[i] != '\n')
                        out[i] = ' ';
            } else if (c == '"' && i > 0 && s[i - 1] == 'R') {
                const std::size_t open = s.find('(', i);
                std::size_t j = n;
                if (open != std::string::npos) {
                    const std::string close = ")" + s.substr(i + 1, open - i - 1) + "\"";
                    const std::size_t e = s.find(close, open);
                    if (e != std::string::npos)
                        j = e + close.size();
                }
                i = j;
            } else if (c == '"' || (c == '\'' && !(i > 0 && std::isalnum(static_cast<unsigned char>(s[i - 1]))))) {
                std::size_t j = i + 1;
                while (j < n && s[j] != c && s[j] != '\n')
                    j += s[j] == '\\' ? 2u : 1u;
                i = j < n && s[j] == c ? j + 1 : j;
            } else {
                ++i;
            }
        }
        return out;
    }

    static std::vector<std::string> tokenize(const std::string &s) {
        static const char *const three[] = {"...", "<<=", ">>="};
        static const char *const two[] = {"##", "&&", "||", "==", "!=", "<=", ">=", "<<", ">>", "->", "::", "++", "--"};
        std::vector<std::string> t;
        const std::size_t n = s.size();
        for (std::size_t i = 0; i < n;) {
            const char c = s[i];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++i;
                continue;
            }
            std::size_t j = i + 1;
            if (identStart(c)) {
                while (j < n && identChar(s[j]))
                    ++j;
                // An encoding prefix belongs to the literal that follows it.
                if (j < n && (s[j] == '"' || s[j] == '\'')) {
                    const std::string prefix = s.substr(i, j - i);
                    if (prefix == "L" || prefix == "u" || prefix == "U" || prefix == "u8")
                        goto literal;
                }
            } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                       (c == '.' && j < n && std::isdigit(static_cast<unsigned char>(s[j])))) {
                while (j < n && (identChar(s[j]) || s[j] == '.' || s[j] == '\'' ||
                                 ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E' ||
                                                                   s[j - 1] == 'p' || s[j - 1] == 'P'))))
                    ++j;
            } else if (c == '"' || c == '\'') {
                j = i;
            literal:
                const char q = s[j];
                ++j;
                while (j < n && s[j] != q)
                    j += s[j] == '\\' ? 2u : 1u;
                j = j < n ? j + 1 : n;
            } else {
                bool found = false;
                for (const char *p : three)
                    if (s.compare(i, 3, p) == 0) {
                        j = i + 3;
                        found = true;
                        break;
                    }
                if (!found)
                    for (const char *p : two)
                        if (s.compare(i, 2, p) == 0) {
                            j = i + 2;
                            break;
                        }
            }
            t.push_back(s.substr(i, j - i));
            i = j;
        }
        return t;
    }

    // Parses "NAME body" or "NAME(params) body" and defines the macro.
    void defineLine(const std::string &line, bool certain) {
        std::size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            ++i;
        const std::size_t start = i;
        while (i < line.size() && identChar(line[i]))
            ++i;
        if (i == start)
            return;
        const std::string name = line.substr(start, i - start);
        Macro m;
        m.uncertain = !certain;
        if (i < line.size() && line[i] == '(') {
            m.function = true;
            const std::size_t close = line.find(')', i);
            if (close == std::string::npos)
                return;
            bool afterParam = false;
            for (const std::string &p : tokenize(line.substr(i + 1, close - i - 1))) {
                if (p == "...") {
                    // NAME... is a named variadic parameter (GNU); a lone ... is __VA_ARGS__.
                    m.variadic = true;
                    if (!afterParam)
                        m.params.push_back("__VA_ARGS__");
                    afterParam = false;
                } else if (p == ",") {
                    afterParam = false;
                } else {
                    m.params.push_back(p);
                    afterParam = true;
                }
            }
            i = close + 1;
        }
        m.body = tokenize(line.substr(i));
        macros_[name] = m;
    }

    static std::string stringify(const std::vector<std::string> &tokens) {
        std::string s = "\"";
        for (std::size_t k = 0; k < tokens.size(); ++k) {
            if (k)
                s += ' ';
            for (const char c : tokens[k]) {
                if (c == '"' || c == '\\')
                    s += '\\';
                s += c;
            }
        }
        return s + "\"";
    }

    std::vector<std::string> substitute(const Macro &m, const std::vector<std::vector<std::string>> &args,
                                        const std::set<std::string> &hide, bool &unknown, int depth) {
        auto paramIndex = [&](const std::string &t) -> int {
            for (std::size_t k = 0; k < m.params.size(); ++k)
                if (m.params[k] == t)
                    return static_cast<int>(k);
            return -1;
        };
        auto argOf = [&](int k) -> std::vector<std::string> {
            const auto idx = static_cast<std::size_t>(k);
            if (m.variadic && idx + 1 == m.params.size()) {
                std::vector<std::string> all;
                for (std::size_t a = idx; a < args.size(); ++a) {
                    if (a > idx)
                        all.push_back(",");
                    all.insert(all.end(), args[a].begin(), args[a].end());
                }
                return all;
            }
            return idx < args.size() ? args[idx] : std::vector<std::string>();
        };
        std::vector<std::string> out;
        std::vector<std::size_t> pastes; // positions in out where ## joins out[p - 1] and out[p]
        const std::vector<std::string> &b = m.body;
        for (std::size_t k = 0; k < b.size(); ++k) {
            if (b[k] == "#" && k + 1 < b.size() && paramIndex(b[k + 1]) >= 0) {
                out.push_back(stringify(argOf(paramIndex(b[k + 1]))));
                ++k;
                continue;
            }
            if (b[k] == "##") {
                pastes.push_back(out.size());
                continue;
            }
            const int p = paramIndex(b[k]);
            if (p < 0) {
                out.push_back(b[k]);
                continue;
            }
            const bool raw = (k > 0 && b[k - 1] == "##") || (k + 1 < b.size() && b[k + 1] == "##");
            const std::vector<std::string> arg = raw ? argOf(p) : expand(argOf(p), hide, unknown, depth + 1);
            out.insert(out.end(), arg.begin(), arg.end());
        }
        // Paste from the back so that earlier positions stay valid.
        for (auto it = pastes.rbegin(); it != pastes.rend(); ++it) {
            const std::size_t p = *it;
            if (p == 0 || p >= out.size())
                continue;
            out[p - 1] += out[p];
            out.erase(out.begin() + static_cast<std::ptrdiff_t>(p));
        }
        return out;
    }

    // Collects the parenthesized argument list that starts at in[i] == "(". Returns false if it does not close.
    static bool collectArgs(const std::vector<std::string> &in, std::size_t &i,
                            std::vector<std::vector<std::string>> &args) {
        int nest = 0;
        args.assign(1, {});
        for (++i; i < in.size(); ++i) {
            const std::string &t = in[i];
            if (t == "(")
                ++nest;
            if (t == ")" && nest-- == 0)
                return true;
            if (t == "," && nest == 0)
                args.emplace_back();
            else
                args.back().push_back(t);
        }
        return false;
    }

    std::vector<std::string> expand(const std::vector<std::string> &in, const std::set<std::string> &hide,
                                    bool &unknown, int depth) {
        std::vector<std::string> out;
        if (depth > 64) {
            unknown = true;
            return out;
        }
        for (std::size_t i = 0; i < in.size(); ++i) {
            const std::string &t = in[i];
            if (!isIdent(t)) {
                out.push_back(t);
                continue;
            }
            const bool call = i + 1 < in.size() && in[i + 1] == "(";
            if (t == "defined") {
                std::size_t k = i + 1;
                const bool paren = k < in.size() && in[k] == "(";
                if (paren)
                    ++k;
                if (k >= in.size() || !isIdent(in[k])) {
                    unknown = true;
                    return out;
                }
                const Cond c = isDefined(in[k]);
                if (c == Cond::Unknown)
                    unknown = true;
                out.push_back(c == Cond::True ? "1" : "0");
                i = paren ? k + 1 : k;
                continue;
            }
            if ((t == "__has_include" || t == "__has_include_next") && call) {
                std::vector<std::vector<std::string>> args;
                if (!collectArgs(in, ++i, args)) {
                    unknown = true;
                    return out;
                }
                std::string header;
                for (const std::string &a : args.front())
                    header += a;
                if (header.size() < 2 || !(header[0] == '"' || header[0] == '<')) {
                    bool u = false;
                    header.clear();
                    for (const std::string &a : expand(args.front(), hide, u, depth + 1))
                        header += a;
                }
                if (!resolve(header, currentFile_, t == "__has_include_next").empty())
                    out.push_back("1");
                else {
                    unknown = true;
                    out.push_back("0");
                }
                continue;
            }
            if (t.rfind("__has_", 0) == 0 && call) {
                // __has_cpp_attribute, __has_builtin, __has_feature and the like: compiler knowledge.
                std::vector<std::vector<std::string>> args;
                collectArgs(in, ++i, args);
                unknown = true;
                out.push_back("0");
                continue;
            }
            const auto it = macros_.find(t);
            if (it == macros_.end() || hide.count(t)) {
                if (call && it == macros_.end())
                    unknown = true; // a function-like macro from a header that was not read
                out.push_back(t);
                continue;
            }
            const Macro &m = it->second;
            if (m.uncertain)
                unknown = true;
            std::set<std::string> inner = hide;
            inner.insert(t);
            if (!m.function) {
                const std::vector<std::string> e = expand(m.body, inner, unknown, depth + 1);
                out.insert(out.end(), e.begin(), e.end());
                continue;
            }
            if (!call) {
                out.push_back(t);
                continue;
            }
            std::vector<std::vector<std::string>> args;
            if (!collectArgs(in, ++i, args)) {
                unknown = true;
                return out;
            }
            const std::vector<std::string> body = substitute(m, args, inner, unknown, depth);
            // Rescan the result together with the rest of the line, as a function-like macro may
            // expand to the name of another one whose arguments follow.
            std::vector<std::string> rest(body);
            rest.insert(rest.end(), in.begin() + static_cast<std::ptrdiff_t>(i) + 1, in.end());
            const std::vector<std::string> e = expand(rest, inner, unknown, depth + 1);
            out.insert(out.end(), e.begin(), e.end());
            return out;
        }
        return out;
    }

    Cond isDefined(const std::string &name) const {
        if (name == "__has_include" || name == "__has_include_next" || name == "__has_cpp_attribute")
            return Cond::True;
        const auto it = macros_.find(name);
        if (it == macros_.end())
            return Cond::False;
        return it->second.uncertain ? Cond::Unknown : Cond::True;
    }

    // Recursive descent over the expanded tokens of a condition.
    struct Parser {
        const std::vector<std::string> &t;
        std::size_t i = 0;
        bool error = false;

        bool accept(const char *s) {
            if (i < t.size() && t[i] == s) {
                ++i;
                return true;
            }
            return false;
        }

        static Value number(const std::string &text) {
            Value v;
            std::string s;
            for (const char c : text)
                if (c != '\'')
                    s += c;
            if (!s.empty() && (s[0] == '\'' || s.back() == '\'')) {
                v.unknown = true;
                return v;
            }
            while (!s.empty() && (s.back() == 'u' || s.back() == 'U' || s.back() == 'l' || s.back() == 'L' ||
                                  s.back() == 'z' || s.back() == 'Z'))
                s.pop_back();
            int base = 10;
            std::size_t k = 0;
            if (s.size() > 1 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
                base = 16;
                k = 2;
            } else if (s.size() > 1 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
                base = 2;
                k = 2;
            } else if (s.size() > 1 && s[0] == '0') {
                base = 8;
                k = 1;
            }
            unsigned long long r = 0;
            if (k >= s.size() && base != 8) {
                v.unknown = true;
                return v;
            }
            for (; k < s.size(); ++k) {
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[k])));
                int d = -1;
                if (c >= '0' && c <= '9')
                    d = c - '0';
                else if (c >= 'a' && c <= 'f')
                    d = c - 'a' + 10;
                if (d < 0 || d >= base) {
                    v.unknown = true;
                    return v;
                }
                r = r * static_cast<unsigned long long>(base) + static_cast<unsigned long long>(d);
            }
            v.v = static_cast<long long>(r);
            return v;
        }

        static Value character(const std::string &text) {
            Value v;
            const std::size_t q = text.find('\'');
            const std::string body = text.substr(q + 1, text.size() - q - 2);
            if (body.size() == 1) {
                v.v = static_cast<unsigned char>(body[0]);
            } else if (body.size() == 2 && body[0] == '\\') {
                switch (body[1]) {
                case 'n': v.v = '\n'; break;
                case 't': v.v = '\t'; break;
                case 'r': v.v = '\r'; break;
                case '0': v.v = 0; break;
                default: v.v = static_cast<unsigned char>(body[1]); break;
                }
            } else {
                v.unknown = true;
            }
            return v;
        }

        Value primary() {
            if (i >= t.size()) {
                error = true;
                return {};
            }
            const std::string &s = t[i++];
            if (s == "(") {
                Value v = conditional();
                if (!accept(")"))
                    error = true;
                return v;
            }
            if (s == "!") {
                Value v = primary();
                v.v = !v.v;
                return v;
            }
            if (s == "~") {
                Value v = primary();
                v.v = ~v.v;
                return v;
            }
            if (s == "-") {
                Value v = primary();
                v.v = static_cast<long long>(0ULL - static_cast<unsigned long long>(v.v));
                return v;
            }
            if (s == "+")
                return primary();
            if (std::isdigit(static_cast<unsigned char>(s[0])))
                return number(s);
            if (s.find('\'') != std::string::npos && s.back() == '\'')
                return character(s);
            if (s == "true")
                return {1, false};
            if (identStart(s[0]))
                return {0, false}; // an identifier that is not a macro
            error = true;
            return {};
        }

        static Value combine(const Value &a, const Value &b, long long r) { return {r, a.unknown || b.unknown}; }

        Value multiplicative() {
            Value a = primary();
            for (;;) {
                if (accept("*")) {
                    const Value b = primary();
                    a = combine(a, b,
                                static_cast<long long>(static_cast<unsigned long long>(a.v) *
                                                       static_cast<unsigned long long>(b.v)));
                } else if (i < t.size() && (t[i] == "/" || t[i] == "%")) {
                    const bool div = t[i++] == "/";
                    const Value b = primary();
                    if (b.v == 0 || (a.v == (-0x7fffffffffffffffLL - 1) && b.v == -1)) {
                        a = {0, true};
                        continue;
                    }
                    a = combine(a, b, div ? a.v / b.v : a.v % b.v);
                } else {
                    return a;
                }
            }
        }
        Value additive() {
            Value a = multiplicative();
            for (;;) {
                if (accept("+")) {
                    const Value b = multiplicative();
                    a = combine(a, b,
                                static_cast<long long>(static_cast<unsigned long long>(a.v) +
                                                       static_cast<unsigned long long>(b.v)));
                } else if (accept("-")) {
                    const Value b = multiplicative();
                    a = combine(a, b,
                                static_cast<long long>(static_cast<unsigned long long>(a.v) -
                                                       static_cast<unsigned long long>(b.v)));
                } else {
                    return a;
                }
            }
        }
        Value shift() {
            Value a = additive();
            for (;;) {
                const bool left = i < t.size() && t[i] == "<<";
                if (!left && !(i < t.size() && t[i] == ">>"))
                    return a;
                ++i;
                const Value b = additive();
                if (b.v < 0 || b.v >= 64) {
                    a = {0, true};
                    continue;
                }
                // Shift the bits as unsigned; a negative value shifted right keeps its sign (~(~x >> n)),
                // without the implementation-defined right shift of a negative signed value.
                const auto bits = static_cast<unsigned long long>(a.v);
                unsigned long long r = 0;
                if (left)
                    r = bits << b.v;
                else if (a.v < 0)
                    r = ~(~bits >> b.v);
                else
                    r = bits >> b.v;
                a = combine(a, b, static_cast<long long>(r));
            }
        }
        Value relational() {
            Value a = shift();
            for (;;) {
                if (accept("<=")) {
                    const Value b = shift();
                    a = combine(a, b, a.v <= b.v);
                } else if (accept(">=")) {
                    const Value b = shift();
                    a = combine(a, b, a.v >= b.v);
                } else if (accept("<")) {
                    const Value b = shift();
                    a = combine(a, b, a.v < b.v);
                } else if (accept(">")) {
                    const Value b = shift();
                    a = combine(a, b, a.v > b.v);
                } else {
                    return a;
                }
            }
        }
        Value equality() {
            Value a = relational();
            for (;;) {
                if (accept("==")) {
                    const Value b = relational();
                    a = combine(a, b, a.v == b.v);
                } else if (accept("!=")) {
                    const Value b = relational();
                    a = combine(a, b, a.v != b.v);
                } else {
                    return a;
                }
            }
        }
        Value bitAnd() {
            Value a = equality();
            while (accept("&")) {
                const Value b = equality();
                a = combine(a, b, a.v & b.v);
            }
            return a;
        }
        Value bitXor() {
            Value a = bitAnd();
            while (accept("^")) {
                const Value b = bitAnd();
                a = combine(a, b, a.v ^ b.v);
            }
            return a;
        }
        Value bitOr() {
            Value a = bitXor();
            while (accept("|")) {
                const Value b = bitXor();
                a = combine(a, b, a.v | b.v);
            }
            return a;
        }
        Value logicalAnd() {
            Value a = bitOr();
            while (accept("&&")) {
                const Value b = bitOr();
                // A known false side decides the result whatever the other one is.
                if ((!a.unknown && !a.v) || (!b.unknown && !b.v))
                    a = {0, false};
                else
                    a = combine(a, b, 1);
            }
            return a;
        }
        Value logicalOr() {
            Value a = logicalAnd();
            while (accept("||")) {
                const Value b = logicalAnd();
                if ((!a.unknown && a.v) || (!b.unknown && b.v))
                    a = {1, false};
                else
                    a = combine(a, b, 0);
            }
            return a;
        }
        Value conditional() {
            const Value c = logicalOr();
            if (!accept("?"))
                return c;
            const Value a = conditional();
            if (!accept(":"))
                error = true;
            const Value b = conditional();
            if (c.unknown)
                return {a.v, true};
            return c.v ? a : b;
        }
    };

    Cond evaluate(const std::string &expr) {
        bool unknown = false;
        const std::vector<std::string> tokens = expand(tokenize(expr), {}, unknown, 0);
        Parser p{tokens};
        const Value v = p.conditional();
        if (p.error || p.i != tokens.size())
            return Cond::Unknown;
        // The expansion flags an unknown part even where the result does not depend on it (0 && F(1)):
        // the parser cannot tell, so such a condition keeps every branch.
        if (v.unknown || unknown)
            return Cond::Unknown;
        return v.v ? Cond::True : Cond::False;
    }

    Cond ifdefCondition(const std::string &rest, bool negate) {
        const std::vector<std::string> t = tokenize(rest);
        if (t.empty() || !isIdent(t[0]))
            return Cond::Unknown;
        const Cond c = isDefined(t[0]);
        if (c == Cond::Unknown || !negate)
            return c;
        return c == Cond::True ? Cond::False : Cond::True;
    }

    // Finds \a header ("name" or <name>) for a file included from \a from. Returns an empty string if not found.
    std::string resolve(const std::string &header, const std::string &from, bool next) const {
        if (header.size() < 3)
            return {};
        const bool quoted = header[0] == '"';
        if (!quoted && header[0] != '<')
            return {};
        const std::string name = header.substr(1, header.size() - 2);
        std::error_code ec;
        if (!name.empty() && std::filesystem::path(name).is_absolute())
            return std::filesystem::is_regular_file(name, ec) ? normalize(name) : std::string();
        const std::string fromDir = std::filesystem::path(from).parent_path().string();
        if (quoted && !next) {
            const std::string p = fromDir + "/" + name;
            if (std::filesystem::is_regular_file(p, ec))
                return normalize(p);
        }
        bool skipping = next;
        for (const std::string &dir : includeDirs) {
            if (skipping) {
                if (normalize(dir) == normalize(fromDir))
                    skipping = false;
                continue;
            }
            const std::string p = dir + "/" + name;
            if (std::filesystem::is_regular_file(p, ec))
                return normalize(p);
        }
        if (skipping) {
            // The including file is not in an include directory: search them all, as compilers do.
            for (const std::string &dir : includeDirs) {
                const std::string p = dir + "/" + name;
                if (std::filesystem::is_regular_file(p, ec) && normalize(p) != from)
                    return normalize(p);
            }
        }
        return {};
    }

    std::string headerName(const std::string &rest) {
        const std::string r = trim(rest);
        if (!r.empty() && (r[0] == '"' || r[0] == '<')) {
            const std::size_t close = r.find(r[0] == '"' ? '"' : '>', 1);
            return close == std::string::npos ? std::string() : r.substr(0, close + 1);
        }
        bool unknown = false;
        std::string h;
        for (const std::string &tok : expand(tokenize(r), {}, unknown, 0))
            h += tok;
        return h;
    }

    void pragma(const std::string &rest, const std::string &path) {
        const std::vector<std::string> t = tokenize(rest);
        if (t.empty())
            return;
        if (t[0] == "once") {
            once_.insert(path);
        } else if ((t[0] == "push_macro" || t[0] == "pop_macro") && t.size() >= 4 && t[1] == "(" &&
                   t[2].size() >= 2 && t[2].front() == '"') {
            const std::string name = t[2].substr(1, t[2].size() - 2);
            if (t[0] == "push_macro") {
                const auto it = macros_.find(name);
                pushed_[name].push_back(it == macros_.end() ? std::nullopt : std::optional<Macro>(it->second));
            } else if (!pushed_[name].empty()) {
                const std::optional<Macro> m = pushed_[name].back();
                pushed_[name].pop_back();
                if (m)
                    macros_[name] = *m;
                else
                    macros_.erase(name);
            }
        }
        // Other pragmas (qt_no_included_check, GCC diagnostic, pack, ...) do not affect the meta-object.
    }

    static Cond both(Cond a, Cond b) {
        if (a == Cond::False || b == Cond::False)
            return Cond::False;
        if (a == Cond::True && b == Cond::True)
            return Cond::True;
        return Cond::Unknown;
    }

    std::string currentFile_;

    // The words that the scanner makes the meta-object from.
    static const std::set<std::string> &mocWords() {
        static const std::set<std::string> words = {"Q_OBJECT",          "Q_GADGET",
                                                    "Q_NAMESPACE",       "Q_NAMESPACE_EXPORT",
                                                    "Q_PROPERTY",        "Q_ENUM",
                                                    "Q_ENUM_NS",         "Q_FLAG",
                                                    "Q_FLAG_NS",         "Q_ENUMS",
                                                    "Q_FLAGS",           "Q_CLASSINFO",
                                                    "Q_PLUGIN_METADATA", "Q_DECLARE_FLAGS",
                                                    "Q_INTERFACES",      "Q_INVOKABLE",
                                                    "Q_SIGNAL",          "Q_SLOT",
                                                    "Q_SCRIPTABLE",      "Q_REVISION",
                                                    "Q_SIGNALS",         "Q_SLOTS",
                                                    "signals",           "slots",
                                                    "QML_ELEMENT",       "QML_NAMED_ELEMENT",
                                                    "QML_SINGLETON",     "QML_ANONYMOUS",
                                                    "QML_UNCREATABLE"};
        return words;
    }

    // The tokens as source text.
    static std::string join(const std::vector<std::string> &tokens) {
        std::string text;
        std::string before;
        for (const std::string &token : tokens) {
            const bool word = identChar(token[0]) || token[0] == '"' || token[0] == '\'';
            const bool opens = before == "::" || before == "(" || before == "<" || before == "~";
            if (!before.empty() && ((word && !opens) || before == "," || before.back() == token[0]))
                text += ' ';
            text += token;
            before = token;
        }
        return text;
    }

    // Writes to \a expanded the part of \a out from \a copied on, with each use between \a from and \a to of a macro
    // that declares something, or brings in a word of the meta-object, replaced by its expansion. \a s is \a out
    // without its comments. A macro that stands for a type or a value is left as it is written.
    void expandMocMacros(const std::string &s, std::size_t from, std::size_t to, const std::string &out,
                         std::size_t &copied, std::string &expanded) {
        for (std::size_t i = from; i < to;) {
            const char c = s[i];
            if (c == '"' || (c == '\'' && !(i > 0 && std::isalnum(static_cast<unsigned char>(s[i - 1]))))) {
                i = skipLiteral(s, i);
                continue;
            }
            if (!identChar(c)) {
                ++i;
                continue;
            }
            std::size_t j = i;
            while (j < to && identChar(s[j]))
                ++j;
            const std::string name = s.substr(i, j - i);
            const std::size_t start = i;
            i = j;
            // The framework's own macros stay: the scanner knows those that matter.
            if (!identStart(c) || name.rfind("Q_", 0) == 0 || name.rfind("QT_", 0) == 0 || name.rfind("QML_", 0) == 0 ||
                mocWords().count(name))
                continue;
            const auto it = macros_.find(name);
            if (it == macros_.end())
                continue;
            std::size_t end = j;
            if (it->second.function) {
                std::size_t k = j;
                while (k < to && std::isspace(static_cast<unsigned char>(s[k])))
                    ++k;
                if (k >= to || s[k] != '(')
                    continue;
                int nest = 0;
                while (k < to) {
                    if (s[k] == '"' || s[k] == '\'') {
                        k = skipLiteral(s, k);
                        continue;
                    }
                    if (s[k] == '(')
                        ++nest;
                    else if (s[k] == ')' && --nest == 0)
                        break;
                    ++k;
                }
                if (k >= to)
                    continue;
                end = k + 1;
            }
            bool unknown = false;
            const std::vector<std::string> result =
                expand(tokenize(s.substr(start, end - start)), mocWords(), unknown, 0);
            bool wanted = false;
            for (const std::string &token : result)
                wanted = wanted || token == ";" || mocWords().count(token) != 0;
            if (!wanted)
                continue;
            expanded.append(out, copied, start - copied);
            expanded += ' ' + join(result) + ' ';
            for (std::size_t k = start; k < end; ++k)
                if (s[k] == '\n')
                    expanded += '\n';
            copied = end;
            i = end;
        }
    }

    // The index after the string or character literal that begins at \a i.
    static std::size_t skipLiteral(const std::string &s, std::size_t i) {
        const char quote = s[i];
        std::size_t j = i + 1;
        while (j < s.size() && s[j] != quote && s[j] != '\n')
            j += s[j] == '\\' ? 2u : 1u;
        return j < s.size() && s[j] == quote ? j + 1 : i + 1;
    }

    std::string process(const std::string &text, const std::string &path, Cond base, bool blank) {
        struct Frame {
            Cond parent;
            Cond state;
            bool taken;      // a branch was certainly taken
            bool maybeTaken; // a branch may have been taken
        };
        std::string out = blank ? text : std::string();
        const std::string s = blankComments(text);
        std::vector<Frame> stack;
        Cond state = base;
        const std::string savedFile = currentFile_;
        currentFile_ = path;
        ++depth_;

        auto branch = [&](Frame &f, Cond c) {
            if (f.parent == Cond::False || f.taken) {
                f.state = Cond::False;
                return;
            }
            Cond b = c;
            if (c != Cond::False && f.maybeTaken)
                b = Cond::Unknown;
            f.state = both(f.parent, b);
            f.taken = f.taken || f.state == Cond::True;
            f.maybeTaken = f.maybeTaken || c != Cond::False;
        };

        std::size_t pos = 0;
        const std::size_t n = s.size();
        // For the file that is scanned: what has been written to \a expanded, up to copied, and where the code that
        // has not been looked at for macros begins. The macros of a stretch of code are those defined before it.
        std::string expanded;
        std::size_t copied = 0, code = 0;
        auto expandCode = [&](std::size_t to) {
            if (blank && to > code)
                expandMocMacros(s, code, std::min(to, n), out, copied, expanded);
        };
        while (pos < n) {
            // A logical line: physical lines joined at a trailing backslash.
            std::size_t end = pos;
            for (;;) {
                end = s.find('\n', end);
                if (end == std::string::npos) {
                    end = n;
                    break;
                }
                std::size_t k = end;
                if (k > pos && s[k - 1] == '\r')
                    --k;
                if (k > pos && s[k - 1] == '\\') {
                    ++end;
                    continue;
                }
                break;
            }
            std::size_t k = pos;
            while (k < end && (s[k] == ' ' || s[k] == '\t'))
                ++k;
            const Cond lineState = state;
            if (k < end && s[k] == '#') {
                if (lineState != Cond::False)
                    expandCode(pos);
                code = end + 1;
                std::string line = s.substr(k + 1, end - k - 1);
                for (std::size_t c = 0; c + 1 < line.size(); ++c)
                    if (line[c] == '\\' && (line[c + 1] == '\n' || line[c + 1] == '\r'))
                        line[c] = ' ';
                for (char &c : line)
                    if (c == '\n' || c == '\r')
                        c = ' ';
                std::size_t w = 0;
                while (w < line.size() && (line[w] == ' ' || line[w] == '\t'))
                    ++w;
                const std::size_t ws = w;
                while (w < line.size() && identChar(line[w]))
                    ++w;
                const std::string directive = line.substr(ws, w - ws);
                const std::string rest = line.substr(w);
                if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
                    Frame f{state, Cond::False, false, false};
                    if (state != Cond::False) {
                        const Cond c = directive == "if" ? evaluate(rest) : ifdefCondition(rest, directive == "ifndef");
                        branch(f, c);
                    }
                    stack.push_back(f);
                    state = f.state;
                } else if ((directive == "elif" || directive == "elifdef" || directive == "elifndef") &&
                           !stack.empty()) {
                    Frame &f = stack.back();
                    if (f.parent != Cond::False && !f.taken) {
                        const Cond c =
                            directive == "elif" ? evaluate(rest) : ifdefCondition(rest, directive == "elifndef");
                        branch(f, c);
                    } else {
                        f.state = Cond::False;
                    }
                    state = f.state;
                } else if (directive == "else" && !stack.empty()) {
                    branch(stack.back(), Cond::True);
                    state = stack.back().state;
                } else if (directive == "endif" && !stack.empty()) {
                    state = stack.back().parent;
                    stack.pop_back();
                } else if (state != Cond::False) {
                    const bool certain = state == Cond::True;
                    if (directive == "define") {
                        defineLine(rest, certain);
                    } else if (directive == "undef") {
                        const std::vector<std::string> t = tokenize(rest);
                        if (!t.empty()) {
                            if (certain)
                                macros_.erase(t[0]);
                            else if (macros_.count(t[0]))
                                macros_[t[0]].uncertain = true;
                        }
                    } else if (directive == "include" || directive == "include_next" || directive == "import") {
                        const std::string found = resolve(headerName(rest), path, directive == "include_next");
                        if (!found.empty() && !once_.count(found) && depth_ < 200) {
                            if (const std::string *contents = readFile(found)) {
                                addDependency(found);
                                const std::string copy = *contents;
                                process(copy, found, state, false);
                            }
                        }
                    } else if (directive == "pragma") {
                        pragma(rest, path);
                    }
                    // #error, #warning, #line, #ident and unknown directives are ignored.
                }
            }
            // Blank a disabled line. The directives themselves are blanked by the scanner.
            if (blank && lineState == Cond::False) {
                for (std::size_t c = pos; c < end; ++c)
                    if (out[c] != '\n')
                        out[c] = ' ';
                code = end + 1;
            }
            pos = end + 1;
            if (lineState == Cond::False && state != Cond::False)
                code = pos;
        }
        if (state != Cond::False)
            expandCode(n);
        if (blank && !expanded.empty())
            out = expanded + out.substr(copied);
        --depth_;
        currentFile_ = savedFile;
        return out;
    }
};

} // namespace ntmoc
