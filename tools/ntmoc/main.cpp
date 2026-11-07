// MIT License - see LICENSE
//
// ntmoc: generates the bodies of signals declared without one, e.g.
//
//     signals:
//         void valueChanged(int value);
//
// It is a text scanner, not a C++ parser. It understands namespaces, nested
// classes, access/signals labels, overloaded signals and default arguments.
// The command line follows the subset of Qt's moc that CMake's AUTOMOC uses.

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
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
};

struct Signal {
    std::string name;
    std::vector<Param> params;
};

struct ClassInfo {
    std::string qualName;
    std::vector<Signal> signals_;
};

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Blanks comments, literals and preprocessor lines, keeping offsets and newlines.
std::string stripSource(const std::string &s) {
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
    static const char *const kw[] = {"int",      "long",     "short",   "char",     "unsigned", "signed",
                                     "double",   "float",    "bool",    "void",     "auto",     "wchar_t",
                                     "char8_t",  "char16_t", "char32_t"};
    for (const char *k : kw)
        if (w == k)
            return true;
    return false;
}

bool isQualifier(const std::string &w) {
    return w == "const" || w == "volatile" || w == "struct" || w == "class" || w == "enum" ||
           w == "typename";
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

// Splits `a, b<int, 2>, c` on top-level commas; returns token index ranges.
std::vector<std::pair<std::size_t, std::size_t>> splitParams(const std::vector<Token> &t,
                                                             std::size_t from, std::size_t to) {
    std::vector<std::pair<std::size_t, std::size_t>> r;
    int depth = 0;
    std::size_t start = from;
    for (std::size_t i = from; i < to; ++i) {
        const std::string &x = t[i].text;
        if (x == "(" || x == "<" || x == "[" || x == "{")
            ++depth;
        else if (x == ")" || x == ">" || x == "]" || x == "}")
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

// Parses `void name(params)` from tokens [from, to); false if not a signal declaration.
bool parseSignal(const std::string &src, const std::vector<Token> &t, std::size_t from,
                 std::size_t to, Signal &sig) {
    while (from < to && (t[from].text == "Q_SIGNAL"))
        ++from;
    if (to - from < 4 || t[from].text != "void" || !isIdentStart(t[from + 1].text[0]) ||
        t[from + 2].text != "(")
        return false;
    // find matching ')'
    int depth = 0;
    std::size_t close = std::string::npos;
    for (std::size_t i = from + 2; i < to; ++i) {
        if (t[i].text == "(")
            ++depth;
        else if (t[i].text == ")" && --depth == 0) {
            close = i;
            break;
        }
    }
    if (close == std::string::npos)
        return false;

    sig.name = t[from + 1].text;
    sig.params.clear();
    for (auto [b, e] : splitParams(t, from + 3, close)) {
        // drop default argument
        int d = 0;
        for (std::size_t i = b; i < e; ++i) {
            const std::string &x = t[i].text;
            if (x == "(" || x == "<" || x == "[" || x == "{")
                ++d;
            else if (x == ")" || x == ">" || x == "]" || x == "}")
                --d;
            else if (x == "=" && d == 0) {
                e = i;
                break;
            }
        }
        if (e <= b)
            continue;
        if (e - b == 1 && t[b].text == "void")
            continue;

        Param p;
        std::size_t typeEnd = e;
        const Token &last = t[e - 1];
        if (e - b >= 2 && isIdentStart(last.text[0]) && !isBuiltinType(last.text) &&
            !isQualifier(last.text)) {
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
        sig.params.push_back(std::move(p));
    }
    return true;
}

struct Scope {
    enum Kind { Namespace, Class, Other } kind = Other;
    std::string name;
    bool qobject = false;
    bool inSignals = false;
    bool isTemplate = false;
    std::vector<Signal> sigs;
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

std::vector<ClassInfo> scan(const std::string &raw) {
    const std::string src = stripSource(raw);
    const std::vector<Token> t = tokenize(src);
    const std::size_t n = t.size();

    std::vector<ClassInfo> classes;
    std::vector<Scope> stack;

    auto at = [&](std::size_t i) -> const std::string & {
        static const std::string empty;
        return i < n ? t[i].text : empty;
    };

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
                s.kind = name.empty() ? Scope::Other : Scope::Namespace;
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
            if (at(j) == ":") {
                while (j < n && t[j].text != "{" && t[j].text != ";")
                    ++j;
            }
            if (at(j) == "{") {
                Scope s;
                s.kind = (cur && cur->kind == Scope::Other) ? Scope::Other : Scope::Class;
                s.name = name;
                s.isTemplate = i > 0 && at(i - 1) == ">";
                s.inSignals = false;
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
                if (s.kind == Scope::Class && s.qobject && !s.sigs.empty()) {
                    if (s.isTemplate) {
                        std::cerr << "ntmoc: warning: signals of template class " << s.name
                                  << " are not supported\n";
                    } else if (const std::string q = qualify(stack, s.name); !q.empty()) {
                        classes.push_back({q, std::move(s.sigs)});
                    }
                }
            }
            ++i;
            continue;
        }

        if (cur && cur->kind == Scope::Class) {
            if (x == "Q_OBJECT") {
                cur->qobject = true;
                ++i;
                continue;
            }
            if ((x == "signals" || x == "Q_SIGNALS") && at(i + 1) == ":") {
                cur->inSignals = true;
                i += 2;
                continue;
            }
            if (x == "public" || x == "private" || x == "protected") {
                // `public:` or `public slots:` / `public Q_SLOTS:`
                cur->inSignals = (at(i + 1) == "signals" || at(i + 1) == "Q_SIGNALS") && at(i + 2) == ":";
                i += cur->inSignals ? 3 : 1;
                continue;
            }
            if (cur->inSignals) {
                std::size_t j = i;
                int depth = 0;
                while (j < n) {
                    const std::string &y = t[j].text;
                    if (y == "(")
                        ++depth;
                    else if (y == ")")
                        --depth;
                    else if (depth == 0 && (y == ";" || y == "{" || y == "}"))
                        break;
                    ++j;
                }
                if (at(j) == ";") {
                    Signal sig;
                    if (parseSignal(src, t, i, j, sig))
                        cur->sigs.push_back(std::move(sig));
                    i = j + 1;
                } else {
                    // inline body or end of class: leave `{`/`}` to the scope handling above
                    i = (j > i) ? j : i + 1;
                }
                continue;
            }
        }
        ++i;
    }
    return classes;
}

std::string generate(const std::vector<ClassInfo> &classes, const std::string &includePath) {
    std::ostringstream o;
    o << "// Generated by ntmoc. Do not edit.\n";
    if (!includePath.empty())
        o << "#include \"" << includePath << "\"\n";
    for (const ClassInfo &c : classes) {
        o << "\n";
        for (const Signal &s : c.signals_) {
            std::string types, decl, args;
            for (std::size_t i = 0; i < s.params.size(); ++i) {
                const std::string a = "_a" + std::to_string(i);
                types += (i ? ", " : "") + s.params[i].type;
                decl += (i ? ", " : "") + s.params[i].type + " " + a;
                args += ", " + a;
            }
            o << "void " << c.qualName << "::" << s.name << "(" << decl << ")\n{\n"
              << "    emitSignal(static_cast<void (" << c.qualName << "::*)(" << types << ")>(&"
              << c.qualName << "::" << s.name << ")" << args << ");\n}\n";
        }
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
                         "Generates bodies for signals declared without one.\n"
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
    const std::string text = generate(scan(buf.str()), (isHeader && !noInclude) ? input : "");

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
