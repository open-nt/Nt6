// MIT License - see LICENSE
//
// ndoc: generates Qt-style HTML reference documentation from /*! ... */
// comments written next to the code, in headers or in source files.
//
//     /*!
//         Returns \c true if the timer is running.
//
//         \sa start(), stop()
//     */
//     bool isActive() const;
//
// Like ntmoc, it is a text scanner, not a C++ parser. It understands namespaces,
// nested classes, access/signals/slots labels, templates, overloads, enums and
// typedefs. Comment syntax is a subset of QDoc's; see tools/ndoc/README.md.
//
//     ndoc [-o outdir] [--strict] [--require-complete] <config>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr const char *kVersion = "ndoc 0.1.0";
constexpr std::size_t npos = std::string::npos;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdent(const std::string &s) { return !s.empty() && isIdentStart(s[0]); }

std::string trim(const std::string &s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
        ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    return s.substr(b, e - b);
}

std::vector<std::string> splitWords(const std::string &s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    for (std::string w; in >> w;)
        out.push_back(w);
    return out;
}

// Collapses whitespace runs to one space.
std::string collapse(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!out.empty() && out.back() != ' ')
                out += ' ';
        } else {
            out += c;
        }
    }
    return trim(out);
}

std::string lower(std::string s) {
    for (char &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool startsWith(const std::string &s, const std::string &prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), npos, suffix) == 0;
}

std::string escapeHtml(const std::string &s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

// Makes a string usable as an HTML id or file name.
std::string slug(const std::string &s) {
    std::string out;
    for (char c : lower(s)) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out += c;
        } else if (!out.empty() && out.back() != '-') {
            out += '-';
        }
    }
    while (!out.empty() && out.back() == '-')
        out.pop_back();
    return out;
}

bool readFile(const fs::path &path, std::string &out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const fs::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
    return static_cast<bool>(out);
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

class Diagnostics {
public:
    void warn(const std::string &file, int line, const std::string &message) {
        std::string text = file;
        if (line > 0)
            text += ':' + std::to_string(line);
        text += ": warning: " + message;
        // Briefs are rendered on several pages; report each problem once.
        if (m_seen.insert(text).second)
            std::cerr << text << '\n';
    }
    int count() const { return static_cast<int>(m_seen.size()); }

private:
    std::set<std::string> m_seen;
};

// ---------------------------------------------------------------------------
// Comment model
// ---------------------------------------------------------------------------

struct Doc {
    std::string file;
    int line = 0;                   // line of lines[0]
    std::vector<std::string> lines; // body, with metadata commands blanked
    std::string topic;              // class, fn, enum, page, ...; empty when attached
    std::string topicArg;
    std::string title;
    std::string brief;
    std::string since;
    std::string inModule;
    std::string inHeader;
    std::string cmakeTarget;
    std::vector<std::string> seeAlso;
    bool overload = false;
    bool reimp = false;
    bool internal = false;
    bool threadSafe = false;
    std::vector<std::string> differences;
    int differencesLine = 0;
};

bool isTopic(const std::string &cmd) {
    static const std::set<std::string> topics = {"class", "namespace", "fn",   "enum",
                                                 "typedef", "variable", "page", "module"};
    return topics.count(cmd) != 0;
}

// Splits "\word rest" from a line that starts with a command.
bool commandOf(const std::string &line, std::string &word, std::string &rest) {
    const std::string t = trim(line);
    if (t.size() < 2 || t[0] != '\\' || !std::isalpha(static_cast<unsigned char>(t[1])))
        return false;
    std::size_t i = 1;
    while (i < t.size() && isIdentChar(t[i]))
        ++i;
    word = t.substr(1, i - 1);
    rest = trim(t.substr(i));
    return true;
}

bool isMultilineMeta(const std::string &cmd) { return cmd == "brief" || cmd == "sa"; }

std::shared_ptr<Doc> parseDoc(const std::string &text, const std::string &file, int line) {
    auto doc = std::make_shared<Doc>();
    doc->file = file;
    doc->line = line;

    std::vector<std::string> raw;
    {
        std::istringstream in(text);
        for (std::string l; std::getline(in, l);)
            raw.push_back(l);
    }

    // Remove the common indent; the first line follows the comment opener.
    std::size_t indent = npos;
    for (std::size_t i = 1; i < raw.size(); ++i) {
        const std::size_t p = raw[i].find_first_not_of(" \t");
        if (p != npos)
            indent = std::min(indent, p);
    }
    for (std::size_t i = 1; i < raw.size(); ++i)
        raw[i] = indent != npos && raw[i].size() >= indent ? raw[i].substr(indent) : trim(raw[i]);

    bool inCode = false;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        std::string word, rest;
        const bool isCmd = commandOf(raw[i], word, rest);
        if (isCmd && word == "code")
            inCode = true;
        if (isCmd && word == "endcode")
            inCode = false;
        if (inCode || !isCmd)
            continue;

        bool meta = true;
        if (isTopic(word)) {
            doc->topic = word;
            doc->topicArg = rest;
        } else if (word == "title") {
            doc->title = rest;
        } else if (word == "since") {
            doc->since = rest;
        } else if (word == "inmodule") {
            doc->inModule = rest;
        } else if (word == "inheaderfile") {
            doc->inHeader = rest;
        } else if (word == "cmaketarget") {
            doc->cmakeTarget = rest;
        } else if (word == "overload") {
            doc->overload = true;
        } else if (word == "reimp") {
            doc->reimp = true;
        } else if (word == "internal") {
            doc->internal = true;
        } else if (word == "threadsafe") {
            doc->threadSafe = true;
        } else if (isMultilineMeta(word)) {
            std::string value = rest;
            raw[i].clear();
            while (i + 1 < raw.size()) {
                std::string w, r;
                if (trim(raw[i + 1]).empty() || commandOf(raw[i + 1], w, r))
                    break;
                value += ' ' + trim(raw[i + 1]);
                raw[++i].clear();
            }
            if (word == "brief") {
                doc->brief = collapse(value);
            } else {
                std::istringstream items(value);
                for (std::string item; std::getline(items, item, ',');) {
                    item = trim(item);
                    if (!item.empty() && item.back() == '.')
                        item.pop_back();
                    if (item.size() > 2 && item.front() == '{' && item.back() == '}')
                        item = item.substr(1, item.size() - 2);
                    if (!item.empty())
                        doc->seeAlso.push_back(item);
                }
            }
        } else if (word == "differences") {
            doc->differencesLine = line + static_cast<int>(i) + 1;
            raw[i].clear();
            while (++i < raw.size()) {
                std::string w, r;
                if (commandOf(raw[i], w, r) && w == "enddifferences") {
                    raw[i].clear();
                    break;
                }
                doc->differences.push_back(raw[i]);
                raw[i].clear();
            }
        } else {
            meta = false;
        }
        if (meta && i < raw.size())
            raw[i].clear();
    }
    doc->lines = std::move(raw);
    return doc;
}

// ---------------------------------------------------------------------------
// C++ model
// ---------------------------------------------------------------------------

enum class Kind { Function, Enum, Typedef, Variable };
enum class Access { Public, Protected, Private };

struct Param {
    std::string text; // as written, including any default
    std::string type; // normalised, without name or default
    std::string name;
};

struct Member {
    Kind kind = Kind::Function;
    std::string name;
    std::string tmpl;     // "template <typename T>"
    std::string ret;      // return, variable or aliased type
    std::string trailer;  // text after the parameter list, e.g. "const"
    std::vector<Param> params;
    std::vector<std::string> values; // enumerators
    Access access = Access::Public;
    bool isSignal = false;
    bool isSlot = false;
    bool isStatic = false;
    bool isVirtual = false;
    bool isExplicit = false;
    bool isConstexpr = false;
    bool isConst = false;
    bool isOverride = false;
    bool isDeleted = false;
    bool isPure = false;
    bool isCtor = false;
    bool isDtor = false;
    std::string flagsOf; // enum wrapped by a QFlags typedef
    std::string key;     // identifies an overload
    std::string file;
    int line = 0;
    std::shared_ptr<Doc> doc;
    std::string anchor;
};

struct ClassDoc {
    std::string qualName;
    std::string name;
    std::string word; // class, struct or namespace
    std::string tmpl;
    std::string parent; // enclosing class, if nested
    std::vector<std::string> bases;
    std::vector<Member> members;
    std::string file;
    int line = 0;
    std::shared_ptr<Doc> doc;
    std::string module;
    std::string href;

    bool visible() const { return doc && !doc->internal; }
};

struct ModuleDoc {
    std::string name;
    std::vector<fs::path> dirs;
    std::shared_ptr<Doc> doc;
    std::string href;
};

struct PageDoc {
    std::shared_ptr<Doc> doc;
    std::string href;
};

// An out-of-class definition carrying a comment.
struct Definition {
    std::string qualifier;
    Member member;
};

struct Model {
    std::map<std::string, std::unique_ptr<ClassDoc>> classes;
    std::vector<ModuleDoc> modules;
    std::vector<PageDoc> pages;
    std::vector<Definition> definitions;
    std::vector<std::shared_ptr<Doc>> topics;

    ClassDoc *find(const std::string &name) const {
        auto it = classes.find(name);
        return it == classes.end() ? nullptr : it->second.get();
    }
};

bool memberVisible(const Member &m) {
    return m.doc && !m.doc->internal && m.access != Access::Private && !m.isDeleted;
}

// Overrides and deleted functions do not need their own comment.
bool memberRequired(const Member &m) {
    if (m.access == Access::Private || m.isDeleted || m.isOverride)
        return false;
    return !(m.doc && m.doc->internal);
}

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

struct Token {
    std::string text;
    std::size_t pos;
    std::size_t end;
};

struct Comment {
    std::string text;
    std::size_t begin;
    std::size_t end;
    int line;
    std::shared_ptr<Doc> doc;
    bool used = false;
};

// code blanks comments, literals and preprocessor lines; display keeps literals,
// so default arguments read as written. Both preserve offsets.
struct Lexed {
    std::string display;
    std::string code;
    std::vector<Token> tokens;
    std::vector<Comment> comments;
    std::vector<std::size_t> lineStarts;

    int lineAt(std::size_t pos) const {
        auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), pos);
        return static_cast<int>(it - lineStarts.begin());
    }
};

// The ' in 1'000, as opposed to the one opening u'x'.
bool isDigitSeparator(const std::string &s, std::size_t i) {
    std::size_t b = i;
    while (b > 0 && isIdentChar(s[b - 1]))
        --b;
    return b < i && std::isdigit(static_cast<unsigned char>(s[b]));
}

Lexed lex(const std::string &s, const std::string &file) {
    Lexed out;
    out.display = s;
    out.code = s;
    const std::size_t n = s.size();
    out.lineStarts.push_back(0);
    for (std::size_t i = 0; i < n; ++i)
        if (s[i] == '\n')
            out.lineStarts.push_back(i + 1);

    auto blankBoth = [&](std::size_t from, std::size_t to) {
        for (std::size_t k = from; k < to && k < n; ++k)
            if (s[k] != '\n')
                out.display[k] = out.code[k] = ' ';
    };
    auto blankCode = [&](std::size_t from, std::size_t to) {
        for (std::size_t k = from; k < to && k < n; ++k)
            if (s[k] != '\n')
                out.code[k] = ' ';
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
            std::size_t e = s.find('\n', i);
            e = e == npos ? n : e;
            blankBoth(i, e);
            i = e;
        } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            std::size_t e = s.find("*/", i + 2);
            e = e == npos ? n : e + 2;
            if (i + 2 < n && s[i + 2] == '!') {
                Comment comment;
                const std::size_t bodyEnd = e >= 2 && e <= n && s.compare(e - 2, 2, "*/") == 0 ? e - 2 : e;
                comment.text = s.substr(i + 3, bodyEnd - i - 3);
                comment.begin = i;
                comment.end = e;
                comment.line = out.lineAt(i);
                comment.doc = parseDoc(comment.text, file, comment.line);
                out.comments.push_back(std::move(comment));
            }
            blankBoth(i, e);
            i = e;
        } else if (c == '#' && lineStart) {
            std::size_t e = i;
            while (e < n && s[e] != '\n') {
                if (s[e] == '\\' && e + 1 < n && s[e + 1] == '\n')
                    ++e;
                ++e;
            }
            blankBoth(i, e);
            i = e;
        } else if (c == '"' || (c == '\'' && !isDigitSeparator(s, i))) {
            std::size_t e = i + 1;
            while (e < n && s[e] != c && s[e] != '\n') {
                if (s[e] == '\\')
                    ++e;
                ++e;
            }
            blankCode(i, e + 1);
            i = std::min(e + 1, n);
            lineStart = false;
        } else if (c == 'R' && i + 1 < n && s[i + 1] == '"' && (i == 0 || !isIdentChar(s[i - 1]))) {
            const std::size_t open = s.find('(', i);
            const std::string delim = open == npos ? "" : s.substr(i + 2, open - i - 2);
            std::size_t e = open == npos ? npos : s.find(')' + delim + '"', open);
            e = e == npos ? n : e + delim.size() + 2;
            blankCode(i, e);
            i = e;
            lineStart = false;
        } else {
            lineStart = false;
            ++i;
        }
    }

    const std::string &code = out.code;
    for (std::size_t i = 0; i < n;) {
        if (std::isspace(static_cast<unsigned char>(code[i]))) {
            ++i;
        } else if (isIdentStart(code[i])) {
            std::size_t e = i;
            while (e < n && isIdentChar(code[e]))
                ++e;
            out.tokens.push_back({code.substr(i, e - i), i, e});
            i = e;
        } else if (std::isdigit(static_cast<unsigned char>(code[i]))) {
            std::size_t e = i;
            while (e < n && (isIdentChar(code[e]) || code[e] == '.' || code[e] == '\''))
                ++e;
            out.tokens.push_back({code.substr(i, e - i), i, e});
            i = e;
        } else {
            std::size_t len = 1;
            for (const char *op : {"...", "::", "->", "&&"}) {
                const std::size_t l = std::char_traits<char>::length(op);
                if (code.compare(i, l, op) == 0) {
                    len = l;
                    break;
                }
            }
            out.tokens.push_back({code.substr(i, len), i, i + len});
            i += len;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Scanner
// ---------------------------------------------------------------------------

bool isBuiltinType(const std::string &w) {
    static const std::set<std::string> kinds = {"void",  "bool",   "char",     "short", "int",
                                                "long",  "float",  "double",   "signed",
                                                "unsigned", "wchar_t", "char8_t", "char16_t",
                                                "char32_t", "auto"};
    return kinds.count(w) != 0;
}

bool isMacroName(const std::string &w) {
    if (w.find('_') == npos || !std::isupper(static_cast<unsigned char>(w[0])))
        return false;
    return std::all_of(w.begin(), w.end(), [](char c) {
        return std::isupper(static_cast<unsigned char>(c)) ||
               std::isdigit(static_cast<unsigned char>(c)) || c == '_';
    });
}

bool isPrivateScope(const std::string &name) {
    return name == "std" || name == "detail" || name.find("Private") != npos;
}

class Scanner {
public:
    Scanner(Lexed &lexed, Model &model, const std::string &file, bool isHeader, Diagnostics &diag)
        : L(lexed), M(model), m_file(file), m_header(isHeader), m_diag(diag) {}

    void run() {
        parseScope(0, L.tokens.size(), nullptr, "", Access::Public);
        for (Comment &c : L.comments) {
            if (c.used)
                continue;
            if (!c.doc->topic.empty())
                M.topics.push_back(c.doc);
            else
                m_diag.warn(m_file, c.line, "comment is not attached to a declaration");
        }
    }

    // Parses "signature;" as used by \fn. Returns false if it is not a function.
    bool parseSignature(Member &m, std::string &qualifier) {
        std::size_t i = 0;
        if (i < L.tokens.size() && T(i) == "template") {
            const std::size_t close = matchAngle(i + 1);
            m.tmpl = text(i, close);
            i = close + 1;
        }
        std::size_t next = 0;
        return parseFunction(i, L.tokens.size(), m, qualifier, next);
    }

private:
    const std::string &T(std::size_t i) const {
        static const std::string empty;
        return i < L.tokens.size() ? L.tokens[i].text : empty;
    }

    std::string text(std::size_t first, std::size_t last) const {
        const std::size_t b = L.tokens[first].pos;
        const std::size_t e = L.tokens[last].end;
        return collapse(L.display.substr(b, e - b));
    }

    std::size_t match(std::size_t i, const char *open, const char *close) const {
        int depth = 0;
        for (std::size_t k = i; k < L.tokens.size(); ++k) {
            if (T(k) == open) {
                ++depth;
            } else if (T(k) == close && --depth == 0) {
                return k;
            }
        }
        return L.tokens.empty() ? 0 : L.tokens.size() - 1;
    }
    std::size_t matchParen(std::size_t i) const { return match(i, "(", ")"); }
    std::size_t matchBrace(std::size_t i) const { return match(i, "{", "}"); }

    std::size_t matchAngle(std::size_t i) const {
        int depth = 0;
        for (std::size_t k = i; k < L.tokens.size(); ++k) {
            if (T(k) == "(") {
                k = matchParen(k);
            } else if (T(k) == "<") {
                ++depth;
            } else if (T(k) == ">" && --depth == 0) {
                return k;
            }
        }
        return L.tokens.size() - 1;
    }

    // Skips to just past the ';' or the body ending a declaration.
    std::size_t skipDeclaration(std::size_t i, std::size_t end) const {
        for (; i < end; ++i) {
            if (T(i) == ";")
                return i + 1;
            if (T(i) == "(") {
                i = matchParen(i);
            } else if (T(i) == "{") {
                i = matchBrace(i);
                if (T(i + 1) == ";")
                    ++i;
                return i + 1;
            }
        }
        return end;
    }

    // The comment directly above token i, if it has no topic of its own.
    std::shared_ptr<Doc> takeDoc(std::size_t i) {
        if (i >= L.tokens.size())
            return nullptr;
        const std::size_t pos = L.tokens[i].pos;
        const std::size_t prevEnd = i == 0 ? 0 : L.tokens[i - 1].end;
        for (auto it = L.comments.rbegin(); it != L.comments.rend(); ++it) {
            if (it->end > pos)
                continue;
            if (it->begin < prevEnd || !it->doc->topic.empty())
                return nullptr;
            it->used = true;
            return it->doc;
        }
        return nullptr;
    }

    ClassDoc &namespaceRecord(const std::string &qual, const std::string &name) {
        auto &slot = M.classes[qual];
        if (!slot) {
            slot = std::make_unique<ClassDoc>();
            slot->qualName = qual;
            slot->name = name;
            slot->word = "namespace";
            slot->file = m_file;
        }
        return *slot;
    }

    void parseScope(std::size_t i, std::size_t end, ClassDoc *cls, const std::string &ns,
                    Access access) {
        bool signalSection = false;
        bool slotSection = false;

        while (i < end) {
            const std::string &t = T(i);

            if (t == ";" || t == "}") {
                ++i;
                continue;
            }

            if (cls) {
                if ((t == "public" || t == "protected" || t == "private") && T(i + 1) == ":") {
                    access = t == "public" ? Access::Public
                             : t == "protected" ? Access::Protected
                                                : Access::Private;
                    signalSection = slotSection = false;
                    i += 2;
                    continue;
                }
                if ((t == "public" || t == "protected" || t == "private") &&
                    (T(i + 1) == "slots" || T(i + 1) == "Q_SLOTS") && T(i + 2) == ":") {
                    access = t == "public" ? Access::Public
                             : t == "protected" ? Access::Protected
                                                : Access::Private;
                    signalSection = false;
                    slotSection = true;
                    i += 3;
                    continue;
                }
                if ((t == "signals" || t == "Q_SIGNALS") && T(i + 1) == ":") {
                    access = Access::Public;
                    signalSection = true;
                    slotSection = false;
                    i += 2;
                    continue;
                }
            }

            std::shared_ptr<Doc> doc = takeDoc(i);

            if (t == "Q_DECLARE_FLAGS" && T(i + 1) == "(" && T(i + 3) == "," && T(i + 5) == ")") {
                Member m;
                m.kind = Kind::Typedef;
                m.name = T(i + 2);
                m.ret = "QFlags<" + T(i + 4) + ">";
                m.flagsOf = T(i + 4);
                m.access = access;
                addMember(cls, ns, std::move(m), doc, i);
                i += 6;
                continue;
            }
            if (isMacroName(t)) {
                ++i;
                if (T(i) == "(")
                    i = matchParen(i) + 1;
                continue;
            }
            if (t == "[" && T(i + 1) == "[") {
                while (i < end && T(i) != "]")
                    ++i;
                i += 2;
                continue;
            }

            std::string tmpl;
            const std::size_t declStart = i;
            std::size_t b = i;
            if (t == "template") {
                const std::size_t close = matchAngle(i + 1);
                tmpl = text(i, close);
                b = close + 1;
            }
            const std::string &first = T(b);

            if (first == "namespace") {
                std::size_t k = b + 1;
                std::string name;
                while (k < end && T(k) != "{" && T(k) != ";" && T(k) != "=") {
                    if (isIdent(T(k)))
                        name += (name.empty() ? "" : "::") + T(k);
                    ++k;
                }
                if (T(k) != "{") {
                    i = skipDeclaration(k, end);
                    continue;
                }
                const std::size_t close = matchBrace(k);
                if (!name.empty() && !isPrivateScope(name))
                    parseScope(k + 1, close, nullptr, ns.empty() ? name : ns + "::" + name,
                               Access::Public);
                i = close + 1;
            } else if (first == "friend" || first == "static_assert") {
                i = skipDeclaration(b, end);
            } else if (first == "using") {
                if (T(b + 1) == "namespace" || T(b + 2) != "=") {
                    i = skipDeclaration(b, end);
                    continue;
                }
                const std::size_t e = skipDeclaration(b, end);
                Member m;
                m.kind = Kind::Typedef;
                m.name = T(b + 1);
                m.tmpl = tmpl;
                m.ret = text(b + 3, e - 2);
                m.access = access;
                addMember(cls, ns, std::move(m), doc, declStart);
                i = e;
            } else if (first == "typedef") {
                const std::size_t e = skipDeclaration(b, end);
                Member m;
                m.kind = Kind::Typedef;
                std::size_t nameAt = e - 2;
                for (std::size_t k = b; k + 2 < e; ++k)
                    if (T(k) == "(" && T(k + 1) == "*" && isIdent(T(k + 2)))
                        nameAt = k + 2;
                m.name = T(nameAt);
                m.ret = text(b + 1, nameAt == e - 2 ? e - 3 : e - 2);
                m.access = access;
                addMember(cls, ns, std::move(m), doc, declStart);
                i = e;
            } else if (first == "enum") {
                i = parseEnum(b, end, cls, ns, access, doc, declStart);
            } else if (first == "class" || first == "struct" || first == "union") {
                i = parseClass(b, end, cls, ns, access, doc, tmpl, declStart);
            } else {
                Member m;
                m.tmpl = tmpl;
                m.access = access;
                m.isSignal = signalSection;
                m.isSlot = slotSection;
                std::string qualifier;
                std::size_t next = b;
                if (parseFunction(b, end, m, qualifier, next)) {
                    if (qualifier.empty()) {
                        addMember(cls, ns, std::move(m), doc, declStart);
                    } else {
                        m.file = m_file;
                        m.line = L.lineAt(L.tokens[declStart].pos);
                        m.doc = doc;
                        if (doc)
                            M.definitions.push_back({ns.empty() ? qualifier : ns + "::" + qualifier,
                                                     std::move(m)});
                    }
                    i = next;
                } else {
                    i = parseVariable(b, end, cls, ns, access, doc, declStart);
                }
            }
        }
    }

    void addMember(ClassDoc *cls, const std::string &ns, Member m, std::shared_ptr<Doc> doc,
                   std::size_t declStart) {
        m.file = m_file;
        m.line = L.lineAt(L.tokens[declStart].pos);
        m.doc = doc;

        ClassDoc *owner = cls;
        if (!owner && m_header && !ns.empty()) {
            const std::size_t sep = ns.rfind("::");
            owner = &namespaceRecord(ns, sep == npos ? ns : ns.substr(sep + 2));
        }
        if (!owner) {
            if (doc)
                m_diag.warn(m_file, m.line, "global declarations cannot be documented yet");
            return;
        }
        if (m.access == Access::Private) {
            if (doc)
                m_diag.warn(m_file, m.line, "documented member '" + m.name + "' is private");
            return;
        }
        m.key = memberKey(m, owner->qualName);
        for (const Member &existing : owner->members)
            if (existing.key == m.key)
                return; // the other side of an #ifdef
        owner->members.push_back(std::move(m));
    }

    std::size_t parseEnum(std::size_t b, std::size_t end, ClassDoc *cls, const std::string &ns,
                          Access access, std::shared_ptr<Doc> doc, std::size_t declStart) {
        std::size_t k = b + 1;
        if (T(k) == "class" || T(k) == "struct")
            ++k;
        Member m;
        m.kind = Kind::Enum;
        m.access = access;
        if (isIdent(T(k)))
            m.name = T(k);
        while (k < end && T(k) != "{" && T(k) != ";")
            ++k;
        if (T(k) != "{")
            return k + 1;
        const std::size_t close = matchBrace(k);
        bool expectName = true;
        int depth = 0;
        for (std::size_t p = k + 1; p < close; ++p) {
            const std::string &v = T(p);
            if (v == "(" || v == "{") {
                ++depth;
            } else if (v == ")" || v == "}") {
                --depth;
            } else if (depth == 0 && v == ",") {
                expectName = true;
            } else if (expectName) {
                if (isIdent(v))
                    m.values.push_back(v);
                expectName = false;
            }
        }
        if (!m.name.empty())
            addMember(cls, ns, std::move(m), doc, declStart);
        return skipDeclaration(close + 1, end);
    }

    std::size_t parseClass(std::size_t b, std::size_t end, ClassDoc *cls, const std::string &ns,
                           Access access, std::shared_ptr<Doc> doc, const std::string &tmpl,
                           std::size_t declStart) {
        std::size_t k = b + 1;
        std::string name;
        std::size_t specialization = 0;
        while (k < end && T(k) != "{" && T(k) != ";" && T(k) != ":" && T(k) != "(" && T(k) != "=") {
            if (T(k) == "<") {
                specialization = k;
                k = matchAngle(k);
            } else if (isIdent(T(k)) && !isMacroName(T(k)) && T(k) != "final") {
                name = T(k);
            }
            ++k;
        }
        if (T(k) == ";")
            return k + 1;
        if (T(k) == "(" || T(k) == "=")
            return skipDeclaration(k, end); // e.g. "struct tm *f();"

        std::vector<std::string> bases;
        if (T(k) == ":") {
            ++k;
            std::string current;
            for (; k < end && T(k) != "{"; ++k) {
                const std::string &v = T(k);
                if (v == "<") {
                    k = matchAngle(k);
                } else if (v == ",") {
                    if (!current.empty())
                        bases.push_back(current);
                    current.clear();
                } else if (v != "public" && v != "protected" && v != "private" && v != "virtual") {
                    current += v;
                }
            }
            if (!current.empty())
                bases.push_back(current);
        }
        if (T(k) != "{")
            return k + 1;
        const std::size_t close = matchBrace(k);
        const std::size_t after = skipDeclaration(close + 1, end);

        // Source-local classes, specialisations and private details are not API.
        if (!m_header || specialization != 0 || name.empty() || access == Access::Private)
            return after;

        std::string qual = name;
        if (cls)
            qual = cls->qualName + "::" + name;
        else if (!ns.empty())
            qual = ns + "::" + name;

        auto &slot = M.classes[qual];
        if (!slot)
            slot = std::make_unique<ClassDoc>();
        ClassDoc *rec = slot.get();
        rec->qualName = qual;
        rec->name = name;
        rec->word = T(b);
        rec->tmpl = tmpl;
        rec->parent = cls ? cls->qualName : std::string();
        rec->bases = std::move(bases);
        rec->file = m_file;
        rec->line = L.lineAt(L.tokens[declStart].pos);
        if (doc)
            rec->doc = doc;
        parseScope(k + 1, close, rec, ns,
                   T(b) == "class" ? Access::Private : Access::Public);
        return after;
    }

    std::size_t parseVariable(std::size_t b, std::size_t end, ClassDoc *cls, const std::string &ns,
                              Access access, std::shared_ptr<Doc> doc, std::size_t declStart) {
        std::size_t k = b;
        std::size_t nameAt = npos;
        int angle = 0;
        for (; k < end; ++k) {
            const std::string &v = T(k);
            if (v == "<") {
                ++angle;
            } else if (v == ">") {
                --angle;
            } else if (angle <= 0 && (v == ";" || v == "=" || v == "{" || v == "[" || v == ":")) {
                break;
            } else if (isIdent(v)) {
                nameAt = k;
            }
        }
        const std::size_t next = skipDeclaration(k, end);
        if (nameAt == npos || nameAt == b)
            return next;

        Member m;
        m.kind = Kind::Variable;
        m.name = T(nameAt);
        m.access = access;
        std::size_t typeBegin = b;
        for (; typeBegin < nameAt; ++typeBegin) {
            const std::string &v = T(typeBegin);
            if (v == "static") {
                m.isStatic = true;
            } else if (v == "constexpr") {
                m.isConstexpr = true;
            } else if (v != "inline" && v != "mutable") {
                break;
            }
        }
        if (typeBegin < nameAt)
            m.ret = text(typeBegin, nameAt - 1);
        if (cls || (m_header && !ns.empty()))
            addMember(cls, ns, std::move(m), doc, declStart);
        return next;
    }

    std::vector<Param> parseParams(std::size_t open, std::size_t close) const {
        std::vector<Param> out;
        std::size_t begin = open + 1;
        int depth = 0;
        for (std::size_t k = open + 1; k <= close; ++k) {
            const std::string &v = T(k);
            if (k < close && (v == "(" || v == "<" || v == "[" || v == "{")) {
                ++depth;
            } else if (k < close && (v == ")" || v == ">" || v == "]" || v == "}")) {
                --depth;
            } else if (k == close || (v == "," && depth == 0)) {
                if (k > begin)
                    out.push_back(parseParam(begin, k));
                begin = k + 1;
            }
        }
        if (out.size() == 1 && out[0].type == "void")
            out.clear();
        return out;
    }

    Param parseParam(std::size_t begin, std::size_t end) const {
        Param p;
        // Runs up to the delimiter, as a literal at the end is not a token.
        p.text = collapse(L.display.substr(L.tokens[begin].pos, L.tokens[end].pos - L.tokens[begin].pos));
        std::size_t typeEnd = end;
        int depth = 0;
        for (std::size_t k = begin; k < end; ++k) {
            const std::string &v = T(k);
            if (v == "(" || v == "<") {
                ++depth;
            } else if (v == ")" || v == ">") {
                --depth;
            } else if (v == "=" && depth == 0) {
                typeEnd = k;
                break;
            }
        }

        std::size_t nameAt = npos;
        for (std::size_t k = begin; k + 2 < typeEnd; ++k)
            if (T(k) == "(" && (T(k + 1) == "*" || T(k + 1) == "&"))
                nameAt = isIdent(T(k + 2)) ? k + 2 : npos;
        for (std::size_t k = begin; nameAt == npos && k + 3 < typeEnd; ++k)
            if (T(k) == "(" && isIdent(T(k + 1)) && T(k + 2) == "::" && T(k + 3) == "*") {
                nameAt = k + 4 < typeEnd && isIdent(T(k + 4)) ? k + 4 : npos;
                break;
            }
        if (nameAt == npos && typeEnd > begin) {
            const std::size_t last = typeEnd - 1;
            std::size_t significant = 0;
            for (std::size_t k = begin; k < typeEnd; ++k)
                if (T(k) != "const" && T(k) != "volatile" && T(k) != "struct" && T(k) != "class")
                    ++significant;
            if (isIdent(T(last)) && significant >= 2 && !isBuiltinType(T(last)) &&
                T(last - 1) != "::" && T(last) != "const")
                nameAt = last;
        }
        if (nameAt != npos)
            p.name = T(nameAt);
        for (std::size_t k = begin; k < typeEnd; ++k) {
            if (k == nameAt)
                continue;
            if (!p.type.empty() && isIdent(p.type.substr(p.type.size() - 1)) && isIdent(T(k)))
                p.type += ' ';
            p.type += T(k);
        }
        return p;
    }

    bool parseFunction(std::size_t b, std::size_t end, Member &m, std::string &qualifier,
                       std::size_t &next) const {
        std::size_t open = npos;
        std::size_t operatorAt = npos;
        int angle = 0;
        for (std::size_t p = b; p < end; ++p) {
            const std::string &t = T(p);
            if (t == "operator") {
                operatorAt = p;
                std::size_t q = p + 1;
                if (T(q) == "(" && T(q + 1) == ")")
                    q += 2;
                while (q < end && T(q) != "(")
                    ++q;
                open = q;
                break;
            }
            if ((t == "decltype" || t == "alignas" || t == "__attribute__" || t == "noexcept") &&
                T(p + 1) == "(") {
                p = matchParen(p + 1);
            } else if (t == "(") {
                if (angle == 0) {
                    if (p == b || !isIdent(T(p - 1)))
                        return false;
                    open = p;
                    break;
                }
                p = matchParen(p);
            } else if (t == "<" && p > b && isIdent(T(p - 1))) {
                ++angle;
            } else if (t == ">" && angle > 0) {
                --angle;
            } else if (angle == 0 && (t == ";" || t == "=" || t == "{" || t == "}")) {
                return false;
            }
        }
        if (open == npos || open >= end)
            return false;

        std::size_t nameBegin = operatorAt != npos ? operatorAt : open - 1;
        std::string name;
        if (operatorAt != npos) {
            name = "operator";
            for (std::size_t k = operatorAt + 1; k < open; ++k) {
                if (isIdent(T(k)))
                    name += ' ';
                name += T(k);
            }
        } else {
            name = T(nameBegin);
        }
        if (nameBegin > b && T(nameBegin - 1) == "~") {
            --nameBegin;
            name = "~" + name;
        }
        while (nameBegin >= b + 2 && T(nameBegin - 1) == "::") {
            std::size_t q = nameBegin - 2;
            if (T(q) == ">") {
                int depth = 0;
                for (;; --q) {
                    if (T(q) == ">") {
                        ++depth;
                    } else if (T(q) == "<" && --depth == 0) {
                        break;
                    }
                    if (q == b)
                        break;
                }
                if (q == b)
                    break;
                --q;
            }
            if (!isIdent(T(q)))
                break;
            qualifier = qualifier.empty() ? T(q) : T(q) + "::" + qualifier;
            nameBegin = q;
        }
        m.name = name;
        m.kind = Kind::Function;

        std::size_t retBegin = b;
        for (; retBegin < nameBegin; ++retBegin) {
            const std::string &t = T(retBegin);
            if (t == "virtual") {
                m.isVirtual = true;
            } else if (t == "static") {
                m.isStatic = true;
            } else if (t == "explicit") {
                m.isExplicit = true;
            } else if (t == "constexpr") {
                m.isConstexpr = true;
            } else if (t != "inline" && t != "extern" && !isMacroName(t)) {
                break;
            }
        }
        // Keeps the qualifier out of the return type.
        std::size_t retEnd = nameBegin;
        if (retEnd > retBegin)
            m.ret = text(retBegin, retEnd - 1);

        const std::size_t close = matchParen(open);
        m.params = parseParams(open, close);

        std::size_t q = close + 1;
        std::string trailer;
        for (;;) {
            const std::string &t = T(q);
            if (t == "const") {
                m.isConst = true;
                trailer += " const";
                ++q;
            } else if (t == "&" || t == "&&") {
                trailer += " " + t;
                ++q;
            } else if (t == "volatile" || t == "final") {
                ++q;
            } else if (t == "noexcept") {
                ++q;
                if (T(q) == "(")
                    q = matchParen(q) + 1;
            } else if (t == "override") {
                m.isOverride = true;
                ++q;
            } else if (t == "->") {
                const std::size_t from = q;
                while (q < end && T(q) != "{" && T(q) != ";" && T(q) != "=")
                    ++q;
                trailer += " " + text(from, q - 1);
            } else if (t == "=") {
                if (T(q + 1) == "delete")
                    m.isDeleted = true;
                if (T(q + 1) == "0")
                    m.isPure = true;
                q += 2;
            } else {
                break;
            }
        }
        m.trailer = trim(trailer);

        if (T(q) == ":") {
            ++q;
            while (q < end) {
                while (q < end && T(q) != "(" && T(q) != "{")
                    ++q;
                q = (T(q) == "(" ? matchParen(q) : matchBrace(q)) + 1;
                if (T(q) != ",")
                    break;
                ++q;
            }
        }
        if (T(q) == "{") {
            next = matchBrace(q) + 1;
        } else if (T(q) == ";") {
            next = q + 1;
        } else {
            next = skipDeclaration(q, end);
        }
        return true;
    }

public:
    // Stable identity of a declaration: name, parameter types and constness.
    static std::string memberKey(const Member &m, const std::string &owner) {
        std::string key = m.name;
        if (m.kind != Kind::Function)
            return key;
        std::string ownerPrefix = owner + "::";
        key += '(';
        for (std::size_t i = 0; i < m.params.size(); ++i) {
            std::string type = m.params[i].type;
            for (std::size_t p; (p = type.find(ownerPrefix)) != npos;)
                type.erase(p, ownerPrefix.size());
            key += (i ? "," : "") + type;
        }
        key += m.isConst ? ") const" : ")";
        return key;
    }

private:
    Lexed &L;
    Model &M;
    std::string m_file;
    bool m_header;
    Diagnostics &m_diag;
};

// ---------------------------------------------------------------------------
// Binding comments to declarations
// ---------------------------------------------------------------------------

Member *findOverload(ClassDoc &cls, const Member &wanted) {
    const std::string key = Scanner::memberKey(wanted, cls.qualName);
    for (Member &m : cls.members)
        if (m.kind == Kind::Function && m.key == key)
            return &m;
    // Spelling can differ between declaration and definition; accept a
    // unique candidate with the same name and arity.
    Member *only = nullptr;
    for (Member &m : cls.members) {
        if (m.kind != Kind::Function || m.name != wanted.name ||
            m.params.size() != wanted.params.size() || m.isConst != wanted.isConst)
            continue;
        if (only)
            return nullptr;
        only = &m;
    }
    return only;
}

void assignDoc(Member &m, const std::shared_ptr<Doc> &doc, Diagnostics &diag) {
    if (m.doc && m.doc != doc)
        diag.warn(doc->file, doc->line,
                  "'" + m.name + "' is already documented at " + m.doc->file + ":" +
                      std::to_string(m.doc->line));
    m.doc = doc;
}

void bindDefinitions(Model &M, Diagnostics &diag) {
    for (Definition &def : M.definitions) {
        ClassDoc *cls = M.find(def.qualifier);
        Member *m = cls ? findOverload(*cls, def.member) : nullptr;
        if (!m) {
            diag.warn(def.member.file, def.member.line,
                      "comment on '" + def.qualifier + "::" + def.member.name +
                          "' matches no public declaration");
            continue;
        }
        assignDoc(*m, def.member.doc, diag);
    }
}

void bindTopicToMember(Model &M, const std::shared_ptr<Doc> &doc, Kind kind, Diagnostics &diag) {
    const std::string &arg = doc->topicArg;
    const std::size_t sep = arg.rfind("::");
    ClassDoc *cls = sep == npos ? nullptr : M.find(arg.substr(0, sep));
    if (cls) {
        const std::string name = arg.substr(sep + 2);
        for (Member &m : cls->members) {
            if (m.kind == kind && m.name == name) {
                assignDoc(m, doc, diag);
                return;
            }
        }
    }
    diag.warn(doc->file, doc->line, "\\" + doc->topic + " names unknown '" + arg + "'");
}

void bindTopics(Model &M, Diagnostics &diag) {
    for (const std::shared_ptr<Doc> &doc : M.topics) {
        const std::string &topic = doc->topic;
        const std::string &arg = doc->topicArg;
        if (topic == "class" || topic == "namespace") {
            if (ClassDoc *cls = M.find(arg)) {
                if (cls->doc)
                    diag.warn(doc->file, doc->line, "'" + arg + "' is already documented");
                cls->doc = doc;
            } else {
                diag.warn(doc->file, doc->line, "\\" + topic + " names unknown '" + arg + "'");
            }
        } else if (topic == "fn") {
            Lexed lexed = lex(arg + ";", doc->file);
            Model scratch;
            Diagnostics quiet;
            Scanner scanner(lexed, scratch, doc->file, false, quiet);
            Member wanted;
            std::string qualifier;
            ClassDoc *cls = nullptr;
            if (scanner.parseSignature(wanted, qualifier))
                cls = M.find(qualifier);
            Member *m = cls ? findOverload(*cls, wanted) : nullptr;
            if (m)
                assignDoc(*m, doc, diag);
            else
                diag.warn(doc->file, doc->line, "\\fn matches no public declaration: " + arg);
        } else if (topic == "enum") {
            bindTopicToMember(M, doc, Kind::Enum, diag);
        } else if (topic == "typedef") {
            bindTopicToMember(M, doc, Kind::Typedef, diag);
        } else if (topic == "variable") {
            bindTopicToMember(M, doc, Kind::Variable, diag);
        } else if (topic == "module") {
            auto it = std::find_if(M.modules.begin(), M.modules.end(),
                                   [&](const ModuleDoc &m) { return m.name == arg; });
            if (it == M.modules.end())
                diag.warn(doc->file, doc->line, "\\module names unknown '" + arg + "'");
            else
                it->doc = doc;
        } else if (topic == "page") {
            M.pages.push_back({doc, arg});
        }
    }
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

struct Config {
    fs::path baseDir;
    std::string project = "Project";
    std::string version;
    std::string description;
    std::string footer;
    fs::path logo;
    fs::path output = "html";
    std::vector<fs::path> sources;
    std::vector<fs::path> snippets;
    std::vector<fs::path> images;
    std::vector<std::pair<std::string, std::vector<fs::path>>> modules;
};

bool loadConfig(const fs::path &path, Config &cfg) {
    std::string text;
    if (!readFile(path, text)) {
        std::cerr << "ndoc: cannot read " << path.string() << '\n';
        return false;
    }
    cfg.baseDir = fs::absolute(path).parent_path();
    const auto resolve = [&](const std::string &p) { return (cfg.baseDir / p).lexically_normal(); };

    std::istringstream in(text);
    int number = 0;
    for (std::string line; std::getline(in, line);) {
        ++number;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#')
            continue;
        const std::size_t eq = t.find('=');
        if (eq == npos) {
            std::cerr << path.string() << ':' << number << ": expected key = value\n";
            return false;
        }
        const std::string key = trim(t.substr(0, eq));
        const std::string value = trim(t.substr(eq + 1));
        if (key == "project") {
            cfg.project = value;
        } else if (key == "version") {
            cfg.version = value;
        } else if (key == "description") {
            cfg.description = value;
        } else if (key == "footer") {
            cfg.footer = value;
        } else if (key == "logo") {
            cfg.logo = resolve(value);
        } else if (key == "output") {
            cfg.output = resolve(value);
        } else if (key == "sources") {
            for (const std::string &w : splitWords(value))
                cfg.sources.push_back(resolve(w));
        } else if (key == "snippets") {
            for (const std::string &w : splitWords(value))
                cfg.snippets.push_back(resolve(w));
        } else if (key == "images") {
            for (const std::string &w : splitWords(value))
                cfg.images.push_back(resolve(w));
        } else if (startsWith(key, "module ")) {
            std::vector<fs::path> dirs;
            for (const std::string &w : splitWords(value))
                dirs.push_back(resolve(w));
            cfg.modules.emplace_back(trim(key.substr(7)), std::move(dirs));
        } else {
            std::cerr << path.string() << ':' << number << ": unknown key '" << key << "'\n";
            return false;
        }
    }
    return true;
}

std::string displayPath(const fs::path &p) { return fs::proximate(p).generic_string(); }

std::vector<fs::path> collectSources(const Config &cfg) {
    std::vector<fs::path> files;
    const auto wanted = [](const fs::path &p) {
        const std::string name = p.filename().string();
        return (p.extension() == ".h" || p.extension() == ".cpp") && !endsWith(name, "_p.h");
    };
    for (const fs::path &source : cfg.sources) {
        if (fs::is_directory(source)) {
            for (const auto &entry : fs::recursive_directory_iterator(source))
                if (entry.is_regular_file() && wanted(entry.path()))
                    files.push_back(entry.path());
        } else if (fs::exists(source)) {
            files.push_back(source);
        } else {
            std::cerr << "ndoc: no such source: " << source.string() << '\n';
        }
    }
    // Headers go first, so declarations exist when definitions are matched.
    std::sort(files.begin(), files.end(), [](const fs::path &a, const fs::path &b) {
        const bool ah = a.extension() == ".h", bh = b.extension() == ".h";
        return ah != bh ? ah : a < b;
    });
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

// ---------------------------------------------------------------------------
// Link resolution and page addresses
// ---------------------------------------------------------------------------

std::string operatorSlug(const std::string &name) {
    std::string out = "operator";
    for (std::size_t i = 8; i < name.size(); ++i) {
        const char c = name[i];
        if (isIdentChar(c)) {
            out += c;
        } else if (c == ' ') {
            out += '-';
        } else {
            static const char *hex = "0123456789abcdef";
            if (out.back() != '-' && !std::isxdigit(static_cast<unsigned char>(out.back())))
                out += '-';
            out += hex[(static_cast<unsigned char>(c) >> 4) & 15];
            out += hex[static_cast<unsigned char>(c) & 15];
        }
    }
    return out;
}

class Site {
public:
    Site(Model &model, const Config &config, Diagnostics &diag)
        : M(model), cfg(config), m_diag(diag) {}

    Model &M;
    const Config &cfg;

    void prepare() {
        for (ModuleDoc &mod : M.modules)
            mod.href = slug(mod.name) + "-module.html";
        for (auto &entry : M.classes) {
            ClassDoc &cls = *entry.second;
            cls.href = slug(cls.qualName) + ".html";
            if (cls.doc && !cls.doc->inModule.empty()) {
                cls.module = cls.doc->inModule;
            } else {
                cls.module = moduleForFile(cls.file);
            }
            assignAnchors(cls);
            promoteFlagsTypedefs(cls);
        }
        for (const PageDoc &page : M.pages)
            registerDocTargets(*page.doc, page.href);
        for (const ModuleDoc &mod : M.modules)
            if (mod.doc)
                registerDocTargets(*mod.doc, mod.href);
        for (const auto &entry : M.classes)
            if (entry.second->doc)
                registerDocTargets(*entry.second->doc, entry.second->href);
    }

    const std::map<std::string, std::string> &targets() const { return m_targets; }

    // Looks through the class, its enclosing classes and its bases.
    const Member *findMember(const ClassDoc *cls, const std::string &name,
                             const std::string &args, int depth = 0) const {
        if (!cls || depth > 16)
            return nullptr;
        const Member *fallback = nullptr;
        for (const Member &m : cls->members) {
            if (m.name != name || !memberVisible(m))
                continue;
            if (args.empty())
                return &m;
            if (!fallback)
                fallback = &m;
            std::string have;
            for (std::size_t i = 0; i < m.params.size(); ++i)
                have += (i ? "," : "") + m.params[i].type;
            if (have == args)
                return &m;
        }
        if (fallback)
            return fallback;
        for (const std::string &base : cls->bases)
            if (const Member *m = findMember(M.find(base), name, args, depth + 1))
                return m;
        if (!cls->parent.empty())
            return findMember(M.find(cls->parent), name, args, depth + 1);
        return nullptr;
    }

    const ClassDoc *ownerOf(const ClassDoc *cls, const Member *member, int depth = 0) const {
        if (!cls || depth > 16)
            return nullptr;
        for (const Member &m : cls->members)
            if (&m == member)
                return cls;
        for (const std::string &base : cls->bases)
            if (const ClassDoc *c = ownerOf(M.find(base), member, depth + 1))
                return c;
        return cls->parent.empty() ? nullptr : ownerOf(M.find(cls->parent), member, depth + 1);
    }

    std::string memberHref(const ClassDoc *start, const Member *m) const {
        const ClassDoc *owner = ownerOf(start, m);
        return owner ? owner->href + "#" + m->anchor : std::string();
    }

    bool resolve(const std::string &rawTarget, const ClassDoc *ctx, std::string &href) const {
        std::string target = trim(rawTarget);
        auto direct = m_targets.find(target);
        if (direct != m_targets.end()) {
            href = direct->second;
            return true;
        }

        std::string args;
        const std::size_t paren = target.find('(');
        const bool isOperator = target.find("operator") != npos;
        if (paren != npos && !isOperator) {
            const std::size_t close = target.rfind(')');
            if (close != npos && close > paren) {
                for (char c : target.substr(paren + 1, close - paren - 1))
                    if (c != ' ')
                        args += c;
            }
            target = target.substr(0, paren);
        }

        if (const ClassDoc *cls = M.find(target)) {
            if (cls->visible()) {
                href = cls->href;
                return true;
            }
            return false;
        }

        std::string ownerName, name = target;
        const std::size_t sep = target.rfind("::");
        if (sep != npos) {
            ownerName = target.substr(0, sep);
            name = target.substr(sep + 2);
        }
        std::vector<const ClassDoc *> owners;
        if (!ownerName.empty()) {
            owners.push_back(M.find(ownerName));
            if (ctx)
                owners.push_back(M.find(ctx->qualName + "::" + ownerName));
        } else {
            owners.push_back(ctx);
        }
        for (const ClassDoc *owner : owners) {
            if (!owner)
                continue;
            if (const Member *m = findMember(owner, name, args)) {
                href = memberHref(owner, m);
                return !href.empty();
            }
            if (const std::string enumHref = enumValueHref(owner, name); !enumHref.empty()) {
                href = enumHref;
                return true;
            }
        }
        // A nested class named from inside its owner.
        if (ctx) {
            if (const ClassDoc *nested = M.find(ctx->qualName + "::" + target)) {
                if (nested->visible()) {
                    href = nested->href;
                    return true;
                }
            }
        }
        return false;
    }

    void warn(const Doc &doc, int offset, const std::string &message) {
        m_diag.warn(doc.file, doc.line + offset, message);
    }
    void warn(const std::string &file, int line, const std::string &message) {
        m_diag.warn(file, line, message);
    }

private:
    std::string enumValueHref(const ClassDoc *cls, const std::string &value, int depth = 0) const {
        if (!cls || depth > 16)
            return {};
        for (const Member &m : cls->members) {
            if (m.kind != Kind::Enum || !memberVisible(m))
                continue;
            if (std::find(m.values.begin(), m.values.end(), value) != m.values.end())
                return cls->href + "#" + m.anchor;
        }
        for (const std::string &base : cls->bases)
            if (const std::string href = enumValueHref(M.find(base), value, depth + 1); !href.empty())
                return href;
        return {};
    }

    std::string moduleForFile(const std::string &file) const {
        const fs::path absolute = fs::absolute(file).lexically_normal();
        std::string best;
        std::size_t bestLength = 0;
        for (const ModuleDoc &mod : M.modules) {
            for (const fs::path &dir : mod.dirs) {
                const std::string prefix = dir.generic_string() + "/";
                if (startsWith(absolute.generic_string(), prefix) && prefix.size() > bestLength) {
                    best = mod.name;
                    bestLength = prefix.size();
                }
            }
        }
        return best;
    }

    static void assignAnchors(ClassDoc &cls) {
        std::map<std::string, int> seen;
        for (Member &m : cls.members) {
            std::string base;
            if (m.isDtor || startsWith(m.name, "~")) {
                base = "dtor." + m.name.substr(1);
            } else if (startsWith(m.name, "operator")) {
                base = operatorSlug(m.name);
            } else {
                base = m.name;
            }
            if (m.kind != Kind::Function)
                base += m.kind == Kind::Enum ? "-enum" : m.kind == Kind::Typedef ? "-typedef" : "-var";
            const int n = ++seen[base];
            m.anchor = n == 1 ? base : base + "-" + std::to_string(n);
        }
    }

    // A QFlags typedef is documented by its enum's comment.
    static void promoteFlagsTypedefs(ClassDoc &cls) {
        for (Member &m : cls.members) {
            if (m.flagsOf.empty() || m.doc)
                continue;
            for (const Member &e : cls.members) {
                if (e.kind == Kind::Enum && e.name == m.flagsOf && e.doc) {
                    auto doc = std::make_shared<Doc>();
                    doc->file = e.doc->file;
                    doc->line = e.doc->line;
                    doc->internal = e.doc->internal;
                    doc->lines.push_back("The \\c " + m.name + " type is a typedef for \\c{QFlags<" +
                                         e.name + ">}. It stores an OR combination of " + e.name +
                                         " values.");
                    m.doc = doc;
                }
            }
        }
    }

    void registerDocTargets(const Doc &doc, const std::string &href) {
        if (!doc.title.empty())
            addTarget(doc.title, href);
        addTarget(href, href);
        for (const std::string &line : doc.lines) {
            std::string word, rest;
            if (!commandOf(line, word, rest))
                continue;
            if (word == "target")
                addTarget(rest, href + "#" + slug(rest));
            else if (word == "section1" || word == "section2")
                addTarget(rest, href + "#" + slug(rest));
        }
    }

    void addTarget(const std::string &name, const std::string &href) { m_targets[name] = href; }

    Diagnostics &m_diag;
    std::map<std::string, std::string> m_targets;
};

// ---------------------------------------------------------------------------
// Comment markup to HTML
// ---------------------------------------------------------------------------

struct RenderContext {
    const ClassDoc *cls = nullptr;
    const Member *member = nullptr;
    std::string self;                 // href of the entity being described
    std::set<std::string> *seenParams = nullptr;
};

class Renderer {
public:
    explicit Renderer(Site &site) : S(site) {}

    // Plain text with automatic links.
    std::string autolink(const std::string &text, const RenderContext &ctx) const {
        std::string out;
        std::size_t i = 0;
        while (i < text.size()) {
            if (!isIdentStart(text[i]) || (i > 0 && isIdentChar(text[i - 1]))) {
                out += escapeHtml(std::string(1, text[i]));
                ++i;
                continue;
            }
            std::size_t e = i;
            while (e < text.size() && (isIdentChar(text[e]) || (text[e] == ':' && e + 1 < text.size() &&
                                                                 text[e + 1] == ':') ||
                                       (text[e] == ':' && text[e - 1] == ':')))
                ++e;
            std::string word = text.substr(i, e - i);
            const bool call = text.compare(e, 2, "()") == 0;
            if (call) {
                word += "()";
                e += 2;
            }
            const bool candidate = call || word.find("::") != npos || S.M.find(word);
            std::string href;
            if (candidate && S.resolve(word, ctx.cls, href) && href != ctx.self) {
                out += "<a href=\"" + escapeHtml(href) + "\">" + escapeHtml(word) + "</a>";
            } else {
                out += escapeHtml(word);
            }
            i = e;
        }
        return out;
    }

    std::string inlineMarkup(const std::string &text, const Doc &doc, int offset,
                             const RenderContext &ctx) const {
        std::string out;
        std::string plain;
        const auto flush = [&]() {
            out += autolink(plain, ctx);
            plain.clear();
        };

        std::size_t i = 0;
        while (i < text.size()) {
            if (text[i] != '\\') {
                plain += text[i++];
                continue;
            }
            if (i + 1 < text.size() && !std::isalpha(static_cast<unsigned char>(text[i + 1]))) {
                plain += text[i + 1];
                i += 2;
                continue;
            }
            std::size_t e = i + 1;
            while (e < text.size() && isIdentChar(text[e]))
                ++e;
            const std::string cmd = text.substr(i + 1, e - i - 1);
            i = e;
            flush();

            const auto argument = [&]() {
                std::string arg;
                if (i < text.size() && text[i] == '{') {
                    arg = braced(text, i);
                    return arg;
                }
                while (i < text.size() && text[i] == ' ')
                    ++i;
                std::size_t b = i;
                int parens = 0;
                while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) {
                    if (text[i] == '(')
                        ++parens;
                    if (text[i] == ')')
                        --parens;
                    ++i;
                }
                arg = text.substr(b, i - b);
                while (!arg.empty() && (std::string(".,;:!?").find(arg.back()) != npos ||
                                        (arg.back() == ')' && parens < 0))) {
                    if (arg.back() == ')')
                        ++parens;
                    arg.pop_back();
                    --i;
                }
                return arg;
            };

            if (cmd == "a") {
                const std::string arg = argument();
                out += "<i>" + escapeHtml(arg) + "</i>";
                if (ctx.member && ctx.member->kind == Kind::Function) {
                    bool known = false;
                    for (const Param &p : ctx.member->params)
                        known = known || p.name == arg;
                    if (!known)
                        S.warn(doc, offset, "\\a names unknown parameter '" + arg + "'");
                    if (ctx.seenParams)
                        ctx.seenParams->insert(arg);
                }
            } else if (cmd == "c" || cmd == "tt") {
                out += "<code>" + escapeHtml(argument()) + "</code>";
            } else if (cmd == "e" || cmd == "i") {
                out += "<em>" + autolink(argument(), ctx) + "</em>";
            } else if (cmd == "b") {
                out += "<strong>" + autolink(argument(), ctx) + "</strong>";
            } else if (cmd == "br") {
                out += "<br>";
            } else if (cmd == "l") {
                const std::string target = argument();
                std::string label = target;
                if (i < text.size() && text[i] == '{')
                    label = braced(text, i);
                out += link(target, label, doc, offset, ctx);
            } else {
                S.warn(doc, offset, "unknown command \\" + cmd);
            }
        }
        flush();
        return out;
    }

    std::string link(const std::string &target, const std::string &label, const Doc &doc,
                     int offset, const RenderContext &ctx) const {
        if (startsWith(target, "http://") || startsWith(target, "https://") ||
            startsWith(target, "mailto:"))
            return "<a href=\"" + escapeHtml(target) + "\">" + escapeHtml(label) + "</a>";
        std::string href;
        if (S.resolve(target, ctx.cls, href))
            return "<a href=\"" + escapeHtml(href) + "\">" + escapeHtml(label) + "</a>";
        S.warn(doc, offset, "can't link to '" + target + "'");
        return escapeHtml(label);
    }

    std::string brief(const Doc &doc, const RenderContext &ctx) const {
        return inlineMarkup(doc.brief, doc, 0, ctx);
    }

    std::string blocks(const std::vector<std::string> &lines, int firstLine, const Doc &doc,
                       const RenderContext &ctx, const Member *enumMember = nullptr,
                       std::vector<std::pair<std::string, std::string>> *toc = nullptr) const {
        std::string out;
        std::string para;
        int paraOffset = 0;
        std::string paraOpen = "<p>";
        std::string paraClose = "</p>";

        struct Open {
            std::string kind;  // ol, ul, table
            bool item = false;
            bool row = false;
            bool header = false;
        };
        std::vector<Open> stack;
        std::vector<std::string> pendingValues;
        std::set<std::string> documentedValues;
        bool valueTableOpen = false;

        const int base = firstLine - doc.line;
        const auto flush = [&]() {
            if (!trim(para).empty())
                out += paraOpen + inlineMarkup(trim(para), doc, base + paraOffset, ctx) + paraClose + "\n";
            para.clear();
            paraOpen = "<p>";
            paraClose = "</p>";
        };
        const auto closeValues = [&]() {
            if (valueTableOpen) {
                out += "</tbody></table>\n";
                valueTableOpen = false;
            }
        };
        const auto closeCell = [&](Open &top) {
            if (top.item) {
                out += top.kind == "table" ? (top.header ? "</th>" : "</td>") : "</li>\n";
                top.item = false;
            }
        };
        const auto closeRow = [&](Open &top) {
            closeCell(top);
            if (top.row) {
                out += "</tr>\n";
                top.row = false;
            }
        };

        for (std::size_t i = 0; i < lines.size(); ++i) {
            const int offset = base + static_cast<int>(i);
            std::string word, rest;
            if (!commandOf(lines[i], word, rest) || isInlineStart(word)) {
                if (trim(lines[i]).empty()) {
                    flush();
                } else {
                    if (para.empty())
                        paraOffset = static_cast<int>(i);
                    para += lines[i] + ' ';
                }
                continue;
            }

            if (word == "code") {
                flush();
                closeValues();
                std::vector<std::string> code;
                bool closed = false;
                while (++i < lines.size()) {
                    std::string w, r;
                    if (commandOf(lines[i], w, r) && w == "endcode") {
                        closed = true;
                        break;
                    }
                    code.push_back(lines[i]);
                }
                if (!closed)
                    S.warn(doc, offset, "\\code without \\endcode");
                out += codeBlock(code);
            } else if (word == "snippet") {
                flush();
                closeValues();
                out += snippet(rest, doc, offset);
            } else if (word == "image") {
                flush();
                closeValues();
                out += image(rest, doc, offset);
            } else if (word == "section1" || word == "section2") {
                flush();
                closeValues();
                const std::string id = slug(rest);
                const char *tag = word == "section1" ? "h2" : "h3";
                out += std::string("<") + tag + " id=\"" + id + "\">" + escapeHtml(rest) + "</" + tag + ">\n";
                if (toc && word == "section1")
                    toc->emplace_back(id, rest);
            } else if (word == "target") {
                flush();
                out += "<a id=\"" + slug(rest) + "\"></a>\n";
            } else if (word == "note" || word == "warning") {
                flush();
                paraOpen = word == "note" ? "<p class=\"note\"><strong>Note:</strong> "
                                          : "<p class=\"warning\"><strong>Warning:</strong> ";
                paraOffset = static_cast<int>(i);
                para = rest + ' ';
            } else if (word == "list") {
                flush();
                closeValues();
                const bool numbered = !rest.empty();
                stack.push_back({numbered ? "ol" : "ul"});
                out += numbered ? "<ol>\n" : "<ul>\n";
            } else if (word == "table") {
                flush();
                closeValues();
                stack.push_back({"table"});
                out += "<table class=\"data\">\n";
            } else if (word == "header" || word == "row") {
                flush();
                if (stack.empty() || stack.back().kind != "table") {
                    S.warn(doc, offset, "\\" + word + " outside \\table");
                    continue;
                }
                closeRow(stack.back());
                stack.back().row = true;
                stack.back().header = word == "header";
                out += "<tr>";
            } else if (word == "li") {
                flush();
                if (stack.empty()) {
                    S.warn(doc, offset, "\\li outside \\list or \\table");
                    continue;
                }
                closeCell(stack.back());
                stack.back().item = true;
                if (stack.back().kind == "table")
                    out += stack.back().header ? "<th>" : "<td>";
                else
                    out += "<li>";
                paraOffset = static_cast<int>(i);
                para = rest + ' ';
            } else if (word == "endlist" || word == "endtable") {
                flush();
                if (stack.empty()) {
                    S.warn(doc, offset, "\\" + word + " without an open block");
                    continue;
                }
                closeRow(stack.back());
                out += word == "endlist" ? "</" + stack.back().kind + ">\n" : "</table>\n";
                stack.pop_back();
            } else if (word == "value" || word == "omitvalue") {
                flush();
                const std::size_t sp = rest.find_first_of(" \t");
                const std::string name = rest.substr(0, sp);
                std::string description = sp == npos ? "" : trim(rest.substr(sp));
                if (enumMember) {
                    documentedValues.insert(name);
                    if (std::find(enumMember->values.begin(), enumMember->values.end(), name) ==
                        enumMember->values.end())
                        S.warn(doc, offset, "\\" + word + " names unknown value '" + name + "'");
                } else {
                    S.warn(doc, offset, "\\" + word + " outside an enum");
                }
                if (word == "omitvalue")
                    continue;
                while (i + 1 < lines.size() && !trim(lines[i + 1]).empty()) {
                    std::string w, r;
                    if (commandOf(lines[i + 1], w, r) && !isInlineStart(w))
                        break;
                    description += ' ' + trim(lines[++i]);
                }
                if (!valueTableOpen) {
                    out += "<table class=\"data\"><thead><tr><th>Constant</th><th>Description</th></tr></thead><tbody>\n";
                    valueTableOpen = true;
                }
                const std::string prefix = ctx.cls ? ctx.cls->qualName + "::" : "";
                out += "<tr><td><code>" + escapeHtml(prefix + name) + "</code></td><td>" +
                       inlineMarkup(description, doc, offset, ctx) + "</td></tr>\n";
            } else {
                S.warn(doc, offset, "unknown command \\" + word);
            }
        }
        flush();
        closeValues();
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            S.warn(doc, 0, "unterminated \\list or \\table");
            closeRow(*it);
            out += it->kind == "table" ? "</table>\n" : "</" + it->kind + ">\n";
        }
        if (enumMember) {
            for (const std::string &value : enumMember->values)
                if (documentedValues.count(value) == 0)
                    S.warn(doc, 0, "enum value '" + value + "' is not documented");
        }
        return out;
    }

    std::string seeAlso(const Doc &doc, const RenderContext &ctx) const {
        if (doc.seeAlso.empty())
            return {};
        std::string out = "<p class=\"see-also\"><strong>See also</strong> ";
        for (std::size_t i = 0; i < doc.seeAlso.size(); ++i) {
            if (i)
                out += i + 1 == doc.seeAlso.size() ? " and " : ", ";
            out += link(doc.seeAlso[i], doc.seeAlso[i], doc, 0, ctx);
        }
        return out + ".</p>\n";
    }

private:
    // Inline commands may begin a line of running text.
    static bool isInlineStart(const std::string &word) {
        static const std::set<std::string> words = {"a", "c", "tt", "e", "i", "b", "l", "br"};
        return words.count(word) != 0;
    }

    static std::string braced(const std::string &text, std::size_t &i) {
        int depth = 0;
        const std::size_t begin = i + 1;
        for (; i < text.size(); ++i) {
            if (text[i] == '{') {
                ++depth;
            } else if (text[i] == '}' && --depth == 0) {
                return text.substr(begin, i++ - begin);
            }
        }
        return text.substr(begin);
    }

    static std::string codeBlock(std::vector<std::string> code) {
        while (!code.empty() && trim(code.back()).empty())
            code.pop_back();
        while (!code.empty() && trim(code.front()).empty())
            code.erase(code.begin());
        std::size_t indent = npos;
        for (const std::string &line : code)
            if (!trim(line).empty())
                indent = std::min(indent, line.find_first_not_of(" \t"));
        std::string body;
        for (const std::string &line : code)
            body += (indent != npos && line.size() >= indent ? line.substr(indent) : trim(line)) + "\n";
        return "<pre><code>" + escapeHtml(body) + "</code></pre>\n";
    }

    std::string image(const std::string &arguments, const Doc &doc, int offset) const {
        const std::string name = arguments.substr(0, arguments.find_first_of(" \t"));
        if (name.empty()) {
            S.warn(doc, offset, "\\image needs a file");
            return {};
        }
        std::string alt = trim(arguments.substr(name.size()));
        bool found = false;
        for (const fs::path &dir : S.cfg.images)
            found = found || fs::exists(dir / name);
        if (!found) {
            S.warn(doc, offset, "can't find image file '" + name + "'");
            return {};
        }
        return "<p class=\"figure\"><img src=\"images/" + escapeHtml(name) + "\" alt=\"" +
               escapeHtml(alt) + "\"></p>\n";
    }

    std::string snippet(const std::string &arguments, const Doc &doc, int offset) const {
        const std::vector<std::string> words = splitWords(arguments);
        if (words.empty()) {
            S.warn(doc, offset, "\\snippet needs a file");
            return {};
        }
        fs::path found;
        for (const fs::path &dir : S.cfg.snippets) {
            if (fs::exists(dir / words[0])) {
                found = dir / words[0];
                break;
            }
        }
        std::string text;
        if (found.empty() || !readFile(found, text)) {
            S.warn(doc, offset, "can't find snippet file '" + words[0] + "'");
            return {};
        }
        std::istringstream in(text);
        std::vector<std::string> code;
        bool inside = words.size() == 1;
        bool matched = words.size() == 1;
        const std::string marker = words.size() > 1 ? "[" + words[1] + "]" : "";
        for (std::string line; std::getline(in, line);) {
            const std::string t = trim(line);
            if (startsWith(t, "//! [")) {
                if (t == "//! " + marker) {
                    inside = !inside;
                    matched = true;
                }
                continue;
            }
            if (inside)
                code.push_back(line);
        }
        if (!matched) {
            S.warn(doc, offset, "no snippet " + marker + " in '" + words[0] + "'");
            return {};
        }
        return codeBlock(code);
    }

    Site &S;
};

// ---------------------------------------------------------------------------
// Page generation
// ---------------------------------------------------------------------------

constexpr const char *kStylesheet = R"css(:root {
  color-scheme: light dark;
  --bg: #ffffff; --surface: #f4f7f6; --text: #1d2b28; --muted: #5b6b67;
  --border: #d8e2df; --accent: #137a63; --code-bg: #f0f4f3;
  --note: #e6f6f1; --warn: #fdf1e3; --warn-line: #c97a1a;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #12191a; --surface: #192324; --text: #e3ecea; --muted: #9db0ab;
    --border: #2c3a3a; --accent: #4fd8b6; --code-bg: #1d2829;
    --note: #17332d; --warn: #3a2a15; --warn-line: #e0a050;
  }
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.6 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
code, pre { font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; font-size: 0.9em; }
code { background: var(--code-bg); padding: 0.1em 0.3em; border-radius: 3px; }
pre { background: var(--code-bg); padding: 12px 16px; border-radius: 6px; overflow-x: auto; }
pre code { background: none; padding: 0; }
header.site { display: flex; align-items: center; gap: 12px; padding: 10px 24px;
  background: var(--surface); border-bottom: 1px solid var(--border); }
header.site img { height: 32px; width: 32px; }
header.site .name { font-weight: 600; color: var(--text); }
header.site .version { color: var(--muted); font-size: 0.875em; }
nav.crumbs { padding: 8px 24px; font-size: 0.875em; color: var(--muted);
  border-bottom: 1px solid var(--border); }
.layout { display: flex; gap: 40px; max-width: 1180px; margin: 0 auto; padding: 24px; }
main { flex: 1; min-width: 0; }
aside.toc { width: 220px; flex: none; order: 2; font-size: 0.875em; position: sticky;
  top: 16px; align-self: flex-start; }
aside.toc h2 { font-size: 0.75em; letter-spacing: 0.06em; text-transform: uppercase;
  color: var(--muted); margin: 0 0 8px; border: 0; padding: 0; }
aside.toc ul { list-style: none; margin: 0; padding: 0; }
aside.toc li { padding: 3px 0; }
h1 { font-size: 2em; line-height: 1.2; margin: 0 0 8px; }
h2 { font-size: 1.35em; margin: 32px 0 12px; padding-bottom: 6px;
  border-bottom: 1px solid var(--border); }
h3.member { font-size: 1em; font-weight: 600; margin: 28px 0 8px; padding: 8px 12px;
  background: var(--surface); border-radius: 4px; overflow-wrap: anywhere; }
h3.member .tags { color: var(--muted); font-weight: 400; font-size: 0.85em; }
.template { display: block; color: var(--muted); font-weight: 400; font-size: 0.9em; }
.brief { font-size: 1.05em; }
table { border-collapse: collapse; width: 100%; margin: 12px 0; }
.scroll { overflow-x: auto; }
table.info { width: auto; }
table.info th { text-align: left; padding: 4px 16px 4px 0; color: var(--muted); font-weight: 500;
  vertical-align: top; white-space: nowrap; }
table.info td { padding: 4px 0; }
table.summary td { padding: 4px 8px; vertical-align: top; border-bottom: 1px solid var(--border); }
table.summary td.ret { width: 30%; text-align: right; color: var(--muted); }
table.data th, table.data td { padding: 6px 10px; border: 1px solid var(--border);
  text-align: left; vertical-align: top; }
table.data th { background: var(--surface); }
.note, .warning { padding: 8px 12px; border-radius: 4px; }
.note { background: var(--note); }
.warning { background: var(--warn); border-left: 3px solid var(--warn-line); }
.figure img { max-width: 100%; height: auto; }
.differences { background: var(--surface); border-left: 3px solid var(--accent);
  padding: 4px 16px; border-radius: 4px; }
td p, li p { margin: 0; }
footer.site { border-top: 1px solid var(--border); color: var(--muted); font-size: 0.8125em;
  padding: 16px 24px; margin-top: 32px; }
@media (max-width: 860px) {
  .layout { flex-direction: column; padding: 16px; gap: 16px; }
  aside.toc { width: auto; position: static; order: 0; }
  header.site, nav.crumbs, footer.site { padding-left: 16px; padding-right: 16px; }
  table.summary td.ret { width: auto; }
}
)css";

using TocEntries = std::vector<std::pair<std::string, std::string>>;
using Crumbs = std::vector<std::pair<std::string, std::string>>;

class Generator {
public:
    Generator(Site &site, Diagnostics &diag) : S(site), R(site), m_diag(diag) {}

    bool run() {
        fs::create_directories(S.cfg.output / "style");
        if (!writeFile(S.cfg.output / "style" / "ndoc.css", kStylesheet))
            return false;
        if (!S.cfg.logo.empty()) {
            fs::create_directories(S.cfg.output / "images");
            std::error_code ec;
            fs::copy_file(S.cfg.logo, S.cfg.output / "images" / S.cfg.logo.filename(),
                          fs::copy_options::overwrite_existing, ec);
            if (ec)
                m_diag.warn(S.cfg.logo.string(), 0, "can't copy logo: " + ec.message());
        }

        for (const fs::path &dir : S.cfg.images) {
            fs::create_directories(S.cfg.output / "images");
            std::error_code ec;
            fs::copy(dir, S.cfg.output / "images",
                     fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
            if (ec)
                m_diag.warn(dir.string(), 0, "can't copy images: " + ec.message());
        }

        std::vector<const ClassDoc *> visible;
        for (const auto &entry : S.M.classes)
            if (entry.second->visible())
                visible.push_back(entry.second.get());

        bool ok = writeLanding(visible);
        ok = writeClassIndex(visible) && ok;
        for (const ModuleDoc &mod : S.M.modules)
            ok = writeModule(mod, visible) && ok;
        for (const PageDoc &page : S.M.pages)
            if (page.href != "index.html")
                ok = writeOverview(page) && ok;
        for (const ClassDoc *cls : visible) {
            ok = writeClass(*cls) && ok;
            ok = writeMembers(*cls) && ok;
        }
        return ok;
    }

private:
    const ModuleDoc *moduleOf(const ClassDoc &cls) const {
        for (const ModuleDoc &mod : S.M.modules)
            if (mod.name == cls.module)
                return &mod;
        return nullptr;
    }

    std::string layout(const std::string &title, const Crumbs &crumbs, const TocEntries &toc,
                       const std::string &body) const {
        const Config &cfg = S.cfg;
        std::string html = "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
                           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
        html += "<title>" + escapeHtml(title) + " | " + escapeHtml(cfg.project) + " " +
                escapeHtml(cfg.version) + "</title>\n";
        html += "<link rel=\"stylesheet\" href=\"style/ndoc.css\">\n</head>\n<body>\n";
        html += "<header class=\"site\">";
        if (!cfg.logo.empty())
            html += "<a href=\"index.html\"><img src=\"images/" +
                    escapeHtml(cfg.logo.filename().string()) + "\" alt=\"\"></a>";
        html += "<a class=\"name\" href=\"index.html\">" + escapeHtml(cfg.project) + "</a>";
        html += "<span class=\"version\">" + escapeHtml(cfg.version) + "</span></header>\n";
        html += "<nav class=\"crumbs\"><a href=\"index.html\">" + escapeHtml(cfg.project) + "</a>";
        for (const auto &crumb : crumbs) {
            html += " &rsaquo; ";
            html += crumb.second.empty()
                        ? escapeHtml(crumb.first)
                        : "<a href=\"" + escapeHtml(crumb.second) + "\">" + escapeHtml(crumb.first) + "</a>";
        }
        html += "</nav>\n<div class=\"layout\">\n";
        if (!toc.empty()) {
            html += "<aside class=\"toc\"><h2>Contents</h2><ul>\n";
            for (const auto &entry : toc)
                html += "<li><a href=\"#" + entry.first + "\">" + escapeHtml(entry.second) + "</a></li>\n";
            html += "</ul></aside>\n";
        }
        html += "<main>\n" + body + "</main>\n</div>\n";
        html += "<footer class=\"site\">" + escapeHtml(cfg.footer) + "</footer>\n</body>\n</html>\n";
        return html;
    }

    bool write(const std::string &href, const std::string &html) {
        if (writeFile(S.cfg.output / href, html))
            return true;
        std::cerr << "ndoc: cannot write " << (S.cfg.output / href).string() << '\n';
        return false;
    }

    std::string classTable(const std::vector<const ClassDoc *> &classes) const {
        std::string out = "<div class=\"scroll\"><table class=\"summary\">\n";
        for (const ClassDoc *cls : classes) {
            RenderContext ctx;
            ctx.cls = cls;
            ctx.self = cls->href;
            out += "<tr><td><a href=\"" + cls->href + "\">" + escapeHtml(cls->qualName) + "</a></td><td>" +
                   R.brief(*cls->doc, ctx) + "</td></tr>\n";
        }
        return out + "</table></div>\n";
    }

    bool writeLanding(const std::vector<const ClassDoc *> &visible) {
        std::string body;
        const PageDoc *custom = nullptr;
        for (const PageDoc &page : S.M.pages)
            if (page.href == "index.html")
                custom = &page;
        TocEntries toc;
        RenderContext ctx;
        ctx.self = "index.html";
        const std::string title = custom && !custom->doc->title.empty()
                                      ? custom->doc->title
                                      : S.cfg.project + " Documentation";
        body += "<h1>" + escapeHtml(title) + "</h1>\n";
        if (!S.cfg.description.empty())
            body += "<p class=\"brief\">" + escapeHtml(S.cfg.description) + "</p>\n";
        if (custom)
            body += R.blocks(custom->doc->lines, custom->doc->line, *custom->doc, ctx, nullptr, &toc);

        body += "<h2 id=\"modules\">Modules</h2>\n<div class=\"scroll\"><table class=\"summary\">\n";
        toc.emplace_back("modules", "Modules");
        for (const ModuleDoc &mod : S.M.modules) {
            const std::string name = mod.doc && !mod.doc->title.empty() ? mod.doc->title : mod.name;
            body += "<tr><td><a href=\"" + mod.href + "\">" + escapeHtml(name) + "</a></td><td>" +
                    (mod.doc ? R.brief(*mod.doc, ctx) : std::string()) + "</td></tr>\n";
        }
        body += "</table></div>\n";

        std::vector<const PageDoc *> overviews;
        for (const PageDoc &page : S.M.pages)
            if (page.href != "index.html")
                overviews.push_back(&page);
        if (!overviews.empty()) {
            std::sort(overviews.begin(), overviews.end(), [](const PageDoc *a, const PageDoc *b) {
                return a->doc->title < b->doc->title;
            });
            toc.emplace_back("overviews", "Overviews");
            body += "<h2 id=\"overviews\">Overviews</h2>\n<div class=\"scroll\"><table class=\"summary\">\n";
            for (const PageDoc *page : overviews)
                body += "<tr><td><a href=\"" + escapeHtml(page->href) + "\">" + escapeHtml(page->doc->title) +
                        "</a></td><td>" + R.brief(*page->doc, ctx) + "</td></tr>\n";
            body += "</table></div>\n";
        }
        body += "<p><a href=\"classes.html\">All classes (" + std::to_string(visible.size()) + ")</a></p>\n";
        return write("index.html", layout(title, {}, toc, body));
    }

    bool writeClassIndex(const std::vector<const ClassDoc *> &visible) {
        const std::string body = "<h1>All Classes</h1>\n" + classTable(visible);
        return write("classes.html", layout("All Classes", {{"All Classes", ""}}, {}, body));
    }

    bool writeModule(const ModuleDoc &mod, const std::vector<const ClassDoc *> &visible) {
        const std::string title = mod.doc && !mod.doc->title.empty() ? mod.doc->title : mod.name;
        TocEntries toc;
        std::string body = "<h1>" + escapeHtml(title) + "</h1>\n";
        RenderContext ctx;
        ctx.self = mod.href;
        std::string details;
        if (mod.doc) {
            body += "<p class=\"brief\">" + R.brief(*mod.doc, ctx) + "</p>\n";
            if (!mod.doc->cmakeTarget.empty()) {
                body += "<h2 id=\"using\">Using the Module</h2>\n<pre><code>target_link_libraries(app PRIVATE " +
                        escapeHtml(mod.doc->cmakeTarget) + ")</code></pre>\n";
                toc.emplace_back("using", "Using the Module");
            }
            details = R.blocks(mod.doc->lines, mod.doc->line, *mod.doc, ctx, nullptr, &toc);
        } else {
            m_diag.warn(mod.name, 0, "module has no \\module comment");
        }
        body += details;

        std::vector<const ClassDoc *> classes;
        for (const ClassDoc *cls : visible)
            if (cls->module == mod.name)
                classes.push_back(cls);
        body += "<h2 id=\"classes\">Classes</h2>\n" + classTable(classes);
        toc.emplace_back("classes", "Classes");
        if (mod.doc)
            body += R.seeAlso(*mod.doc, ctx);
        return write(mod.href, layout(title, {{title, ""}}, toc, body));
    }

    bool writeOverview(const PageDoc &page) {
        const Doc &doc = *page.doc;
        RenderContext ctx;
        ctx.self = page.href;
        TocEntries toc;
        const std::string content = R.blocks(doc.lines, doc.line, doc, ctx, nullptr, &toc);
        std::string body = "<h1>" + escapeHtml(doc.title) + "</h1>\n";
        if (!doc.brief.empty())
            body += "<p class=\"brief\">" + R.brief(doc, ctx) + "</p>\n";
        body += content + R.seeAlso(doc, ctx);
        return write(page.href, layout(doc.title, {{doc.title, ""}}, toc, body));
    }

    // Shows a declaration with known classes linked and parameter names marked.
    std::string decorate(const std::string &text, const std::set<std::string> &names,
                         const RenderContext &ctx) const {
        std::string out;
        std::size_t i = 0;
        while (i < text.size()) {
            if (!isIdentStart(text[i]) || (i > 0 && isIdentChar(text[i - 1]))) {
                out += escapeHtml(std::string(1, text[i++]));
                continue;
            }
            std::size_t e = i;
            while (e < text.size() && (isIdentChar(text[e]) ||
                                       (text.compare(e, 2, "::") == 0 && e + 2 < text.size() &&
                                        isIdentStart(text[e + 2])) ||
                                       (text[e] == ':' && text[e - 1] == ':')))
                ++e;
            const std::string word = text.substr(i, e - i);
            std::string href;
            if (names.count(word)) {
                out += "<i>" + escapeHtml(word) + "</i>";
            } else if (S.resolve(word, ctx.cls, href) && word.find("::") == npos && isClass(word, ctx) &&
                       href != ctx.self) {
                out += "<a href=\"" + escapeHtml(href) + "\">" + escapeHtml(word) + "</a>";
            } else if (word.find("::") != npos && S.resolve(word, ctx.cls, href)) {
                out += "<a href=\"" + escapeHtml(href) + "\">" + escapeHtml(word) + "</a>";
            } else {
                out += escapeHtml(word);
            }
            i = e;
        }
        return out;
    }

    bool isClass(const std::string &word, const RenderContext &ctx) const {
        if (S.M.find(word))
            return true;
        return ctx.cls && S.M.find(ctx.cls->qualName + "::" + word);
    }

    std::string paramsHtml(const Member &m, const RenderContext &ctx) const {
        std::set<std::string> names;
        std::string text;
        for (std::size_t i = 0; i < m.params.size(); ++i) {
            if (!m.params[i].name.empty())
                names.insert(m.params[i].name);
            text += (i ? ", " : "") + m.params[i].text;
        }
        return "(" + decorate(text, names, ctx) + ")";
    }

    std::string summaryRow(const ClassDoc &cls, const Member &m) const {
        RenderContext ctx;
        ctx.cls = &cls;
        ctx.self = cls.href;
        std::string left;
        if (m.kind == Kind::Enum) {
            left = "enum";
        } else if (m.kind == Kind::Typedef) {
            left = "typedef";
        } else {
            if (m.isVirtual)
                left += "virtual ";
            left += decorate(m.ret, {}, ctx);
        }
        std::string right = "<a href=\"#" + m.anchor + "\">" + escapeHtml(m.name) + "</a>";
        if (m.kind == Kind::Function) {
            right += paramsHtml(m, ctx);
            if (!m.trailer.empty())
                right += " " + escapeHtml(m.trailer);
        }
        return "<tr><td class=\"ret\">" + left + "</td><td>" + right + "</td></tr>\n";
    }

    std::vector<std::string> tagsOf(const Member &m) const {
        std::vector<std::string> tags;
        if (m.isSignal)
            tags.push_back("signal");
        if (m.isSlot)
            tags.push_back("slot");
        if (m.isStatic)
            tags.push_back("static");
        if (m.isVirtual)
            tags.push_back("virtual");
        if (m.isExplicit)
            tags.push_back("explicit");
        if (m.isConstexpr)
            tags.push_back("constexpr");
        if (m.isOverride)
            tags.push_back("override");
        if (m.access == Access::Protected)
            tags.push_back("protected");
        return tags;
    }

    std::string memberDetails(const ClassDoc &cls, const Member &m) const {
        RenderContext ctx;
        ctx.cls = &cls;
        ctx.member = &m;
        ctx.self = cls.href + "#" + m.anchor;
        std::set<std::string> seen;
        ctx.seenParams = &seen;

        std::string heading = "<h3 class=\"member\" id=\"" + m.anchor + "\">";
        if (!m.tmpl.empty())
            heading += "<span class=\"template\">" + escapeHtml(m.tmpl) + "</span>";
        const std::vector<std::string> tags = tagsOf(m);
        if (!tags.empty()) {
            heading += "<span class=\"tags\">[";
            for (std::size_t i = 0; i < tags.size(); ++i)
                heading += (i ? ", " : "") + tags[i];
            heading += "]</span> ";
        }
        if (m.kind == Kind::Enum) {
            heading += "enum " + escapeHtml(cls.qualName + "::" + m.name);
        } else if (m.kind == Kind::Typedef) {
            heading += "typedef " + escapeHtml(cls.qualName + "::" + m.name);
        } else {
            if (!m.ret.empty())
                heading += decorate(m.ret, {}, ctx) + " ";
            heading += escapeHtml(cls.qualName + "::" + m.name);
            if (m.kind == Kind::Function) {
                heading += paramsHtml(m, ctx);
                if (!m.trailer.empty())
                    heading += " " + escapeHtml(m.trailer);
            }
        }
        heading += "</h3>\n";

        const Doc &doc = *m.doc;
        std::string body;
        if (m.isOverride) {
            for (const std::string &base : cls.bases) {
                std::string href;
                if (S.resolve(base + "::" + m.name, nullptr, href)) {
                    body += "<p>Reimplements <a href=\"" + escapeHtml(href) + "\">" +
                            escapeHtml(base + "::" + m.name + "()") + "</a>.</p>\n";
                    break;
                }
            }
        }
        if (doc.overload)
            body += "<p>This is an overloaded function.</p>\n";
        body += R.blocks(doc.lines, doc.line, doc, ctx, m.kind == Kind::Enum ? &m : nullptr);
        if (!doc.since.empty())
            body += "<p>This function was introduced in " + escapeHtml(doc.since) + ".</p>\n";
        if (doc.threadSafe)
            body += "<p class=\"note\"><strong>Note:</strong> This function is thread-safe.</p>\n";
        body += R.seeAlso(doc, ctx);

        if (m.kind == Kind::Function && !doc.overload && !doc.reimp) {
            for (const Param &p : m.params)
                if (!p.name.empty() && seen.count(p.name) == 0)
                    S.warn(doc, 0, "undocumented parameter '" + p.name + "' in " + cls.qualName +
                                       "::" + m.name + "()");
        }
        return heading + body;
    }

    static bool byName(const Member *a, const Member *b) {
        const auto rank = [](const Member *m) { return m->isCtor ? 0 : m->isDtor ? 1 : 2; };
        if (rank(a) != rank(b))
            return rank(a) < rank(b);
        return lower(a->name) < lower(b->name);
    }

    std::string headerLine(const ClassDoc &cls) const {
        if (cls.doc->inHeader.size())
            return cls.doc->inHeader;
        std::string top = cls.qualName.substr(0, cls.qualName.find("::"));
        const fs::path file = fs::path(cls.file);
        if (fs::exists(file.parent_path() / top))
            return top;
        return file.parent_path().filename().string() + "/" + file.filename().string();
    }

    bool writeClass(const ClassDoc &cls) {
        const Doc &doc = *cls.doc;
        RenderContext ctx;
        ctx.cls = &cls;
        ctx.self = cls.href;
        const bool isNs = cls.word == "namespace";
        const std::string title = cls.qualName + (isNs ? " Namespace" : cls.word == "struct" ? " Struct" : " Class");

        std::string body = "<h1>" + escapeHtml(title) + "</h1>\n";
        if (!cls.tmpl.empty())
            body += "<p><code>" + escapeHtml(cls.tmpl + " " + cls.word + " " + cls.name) + "</code></p>\n";
        body += "<p class=\"brief\">" + R.brief(doc, ctx) + " <a href=\"#details\">More&hellip;</a></p>\n";

        body += "<table class=\"info\">\n";
        const auto infoRow = [&](const std::string &label, const std::string &value) {
            body += "<tr><th>" + label + "</th><td>" + value + "</td></tr>\n";
        };
        infoRow("Header", "<code>#include &lt;" + escapeHtml(headerLine(cls)) + "&gt;</code>");
        const ModuleDoc *mod = moduleOf(cls);
        if (mod) {
            if (mod->doc && !mod->doc->cmakeTarget.empty())
                infoRow("CMake", "<code>target_link_libraries(app PRIVATE " +
                                     escapeHtml(mod->doc->cmakeTarget) + ")</code>");
            const std::string name = mod->doc && !mod->doc->title.empty() ? mod->doc->title : mod->name;
            infoRow("Module", "<a href=\"" + mod->href + "\">" + escapeHtml(name) + "</a>");
        }
        if (!doc.since.empty())
            infoRow("Since", escapeHtml(doc.since));
        if (!cls.parent.empty())
            infoRow("Nested in", decorate(cls.parent, {}, ctx));
        if (!cls.bases.empty()) {
            std::string names;
            for (std::size_t i = 0; i < cls.bases.size(); ++i)
                names += (i ? ", " : "") + linkClass(cls.bases[i]);
            infoRow("Inherits", names);
        }
        std::string derived;
        for (const auto &entry : S.M.classes) {
            const ClassDoc &other = *entry.second;
            if (other.visible() && std::find(other.bases.begin(), other.bases.end(), cls.qualName) != other.bases.end())
                derived += (derived.empty() ? "" : ", ") + linkClass(other.qualName);
        }
        if (!derived.empty())
            infoRow("Inherited by", derived);
        body += "</table>\n";
        if (!isNs)
            body += "<p><a href=\"" + membersHref(cls) + "\">List of all members, including inherited members</a></p>\n";

        struct Group {
            std::string title;
            std::vector<const Member *> members;
        };
        std::vector<Group> groups = {
            {"Public Types", {}},           {"Public Functions", {}},
            {"Reimplemented Public Functions", {}},
            {"Public Slots", {}},           {"Signals", {}},
            {"Public Variables", {}},       {"Static Public Members", {}},
            {"Protected Types", {}},        {"Protected Functions", {}},
            {"Reimplemented Protected Functions", {}},
            {"Protected Slots", {}},        {"Protected Variables", {}},
            {"Static Protected Members", {}},
        };
        const auto group = [&](const char *name) -> std::vector<const Member *> & {
            for (Group &g : groups)
                if (g.title == name)
                    return g.members;
            return groups.front().members;
        };

        std::vector<const Member *> types, functions, variables;
        for (const Member &m : cls.members) {
            if (!memberVisible(m))
                continue;
            const bool pub = m.access == Access::Public;
            if (m.kind == Kind::Enum || m.kind == Kind::Typedef) {
                group(pub ? "Public Types" : "Protected Types").push_back(&m);
                types.push_back(&m);
            } else if (m.kind == Kind::Variable) {
                group(m.isStatic ? (pub ? "Static Public Members" : "Static Protected Members")
                                 : (pub ? "Public Variables" : "Protected Variables"))
                    .push_back(&m);
                variables.push_back(&m);
            } else {
                functions.push_back(&m);
                if (m.isSignal)
                    group("Signals").push_back(&m);
                else if (m.isSlot)
                    group(pub ? "Public Slots" : "Protected Slots").push_back(&m);
                else if (m.isStatic)
                    group(pub ? "Static Public Members" : "Static Protected Members").push_back(&m);
                else if (m.isOverride && !m.isDtor)
                    group(pub ? "Reimplemented Public Functions" : "Reimplemented Protected Functions").push_back(&m);
                else
                    group(pub ? "Public Functions" : "Protected Functions").push_back(&m);
            }
        }

        TocEntries toc;
        for (const Group &g : groups) {
            if (g.members.empty())
                continue;
            const std::string id = slug(g.title);
            toc.emplace_back(id, g.title);
            body += "<h2 id=\"" + id + "\">" + escapeHtml(g.title) + "</h2>\n<div class=\"scroll\"><table class=\"summary\">\n";
            for (const Member *m : g.members)
                body += summaryRow(cls, *m);
            body += "</table></div>\n";
        }

        toc.emplace_back("details", "Detailed Description");
        body += "<h2 id=\"details\">Detailed Description</h2>\n";
        TocEntries inner;
        body += R.blocks(doc.lines, doc.line, doc, ctx, nullptr, &inner);
        body += R.seeAlso(doc, ctx);
        toc.insert(toc.end(), inner.begin(), inner.end());

        if (!doc.differences.empty()) {
            toc.emplace_back("differences", "Differences from Qt");
            body += "<h2 id=\"differences\">Differences from Qt</h2>\n<div class=\"differences\">\n";
            body += R.blocks(doc.differences, doc.differencesLine, doc, ctx);
            body += "</div>\n";
        }

        const auto detail = [&](const char *id, const char *heading, std::vector<const Member *> list) {
            if (list.empty())
                return;
            std::stable_sort(list.begin(), list.end(), byName);
            toc.emplace_back(id, heading);
            body += std::string("<h2 id=\"") + id + "\">" + heading + "</h2>\n";
            for (const Member *m : list)
                body += memberDetails(cls, *m);
        };
        detail("types", "Member Type Documentation", types);
        detail("functions", isNs ? "Function Documentation" : "Member Function Documentation", functions);
        detail("variables", "Member Variable Documentation", variables);

        Crumbs crumbs;
        if (mod)
            crumbs.emplace_back(mod->doc && !mod->doc->title.empty() ? mod->doc->title : mod->name, mod->href);
        crumbs.emplace_back(cls.qualName, "");
        return write(cls.href, layout(title, crumbs, toc, body));
    }

    std::string linkClass(const std::string &name) const {
        const ClassDoc *cls = S.M.find(name);
        if (cls && cls->visible())
            return "<a href=\"" + cls->href + "\">" + escapeHtml(name) + "</a>";
        return escapeHtml(name);
    }

    static std::string membersHref(const ClassDoc &cls) {
        return slug(cls.qualName) + "-members.html";
    }

    void collectMembers(const ClassDoc &cls, std::vector<std::pair<const Member *, const ClassDoc *>> &out,
                        std::set<std::string> &seen, int depth) const {
        if (depth > 16)
            return;
        for (const Member &m : cls.members)
            if (memberVisible(m) && seen.insert(m.key).second)
                out.emplace_back(&m, &cls);
        for (const std::string &base : cls.bases)
            if (const ClassDoc *b = S.M.find(base))
                collectMembers(*b, out, seen, depth + 1);
    }

    bool writeMembers(const ClassDoc &cls) {
        if (cls.word == "namespace")
            return true;
        std::vector<std::pair<const Member *, const ClassDoc *>> all;
        std::set<std::string> seen;
        collectMembers(cls, all, seen, 0);
        std::stable_sort(all.begin(), all.end(), [](const auto &a, const auto &b) {
            return lower(a.first->name) < lower(b.first->name);
        });
        const std::string title = "List of All Members for " + cls.qualName;
        std::string body = "<h1>" + escapeHtml(title) + "</h1>\n<p>This is the complete list of members for <a href=\"" +
                           cls.href + "\">" + escapeHtml(cls.qualName) + "</a>, including inherited members.</p>\n";
        body += "<div class=\"scroll\"><table class=\"summary\">\n";
        for (const auto &entry : all) {
            const Member &m = *entry.first;
            const ClassDoc &owner = *entry.second;
            RenderContext ctx;
            ctx.cls = &owner;
            std::string row = "<a href=\"" + owner.href + "#" + m.anchor + "\">" + escapeHtml(m.name) + "</a>";
            if (m.kind == Kind::Function)
                row += paramsHtml(m, ctx);
            body += "<tr><td>" + row + "</td><td>" + (&owner == &cls ? "" : "from " + linkClass(owner.qualName)) + "</td></tr>\n";
        }
        body += "</table></div>\n";
        return write(membersHref(cls), layout(title, {{cls.qualName, cls.href}, {"All members", ""}}, {}, body));
    }

    Site &S;
    Renderer R;
    Diagnostics &m_diag;
};

// ---------------------------------------------------------------------------
// Coverage
// ---------------------------------------------------------------------------

struct Coverage {
    int classes = 0;
    int classesDocumented = 0;
    int members = 0;
    int membersDocumented = 0;

    bool complete() const { return classes == classesDocumented && members == membersDocumented; }
};

Coverage writeCoverage(const Model &M, const fs::path &file) {
    Coverage total;
    std::string report = "# Undocumented public API\n";
    for (const auto &entry : M.classes) {
        const ClassDoc &cls = *entry.second;
        if (cls.doc && cls.doc->internal)
            continue;
        const bool isNamespace = cls.word == "namespace";
        if (!isNamespace || cls.doc) {
            ++total.classes;
            total.classesDocumented += cls.doc ? 1 : 0;
        } else if (cls.members.empty()) {
            continue;
        } else {
            ++total.classes;
        }
        std::string missing;
        for (const Member &m : cls.members) {
            if (!memberRequired(m))
                continue;
            ++total.members;
            if (m.doc) {
                ++total.membersDocumented;
            } else {
                std::string item = m.name;
                if (m.kind == Kind::Function) {
                    item += '(';
                    for (std::size_t i = 0; i < m.params.size(); ++i)
                        item += (i ? ", " : "") + m.params[i].type;
                    item += m.isConst ? ") const" : ")";
                }
                missing += "    " + item + "\n";
            }
        }
        if (!cls.doc || !missing.empty())
            report += "\n" + cls.qualName + (cls.doc ? "" : " (class undocumented)") + "\n" + missing;
    }
    writeFile(file, report);
    return total;
}

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------

void markSpecialMembers(Model &M) {
    for (auto &entry : M.classes) {
        for (Member &m : entry.second->members) {
            m.isCtor = m.kind == Kind::Function && m.name == entry.second->name;
            m.isDtor = m.kind == Kind::Function && startsWith(m.name, "~");
        }
    }
}

int usage(std::ostream &out, int code) {
    out << "Usage: ndoc [options] <config>\n\n"
           "Generates Qt-style HTML documentation from /*! */ comments.\n\n"
           "Options:\n"
           "  -o <dir>             Output directory, overriding the config file\n"
           "  --strict             Exit non-zero on any warning\n"
           "  --require-complete   Exit non-zero if any public API is undocumented\n"
           "  -v, --version        Print the version\n"
           "  -h, --help           Print this help\n";
    return code;
}

} // namespace

int main(int argc, char **argv) {
    std::string configPath, outputDir;
    bool strict = false, requireComplete = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
            return usage(std::cout, 0);
        if (arg == "-v" || arg == "--version") {
            std::cout << kVersion << '\n';
            return 0;
        }
        if (arg == "--strict") {
            strict = true;
        } else if (arg == "--require-complete") {
            requireComplete = true;
        } else if (arg == "-o" && i + 1 < argc) {
            outputDir = argv[++i];
        } else if (!arg.empty() && arg[0] != '-' && configPath.empty()) {
            configPath = arg;
        } else {
            return usage(std::cerr, 2);
        }
    }
    if (configPath.empty())
        return usage(std::cerr, 2);

    Config cfg;
    if (!loadConfig(configPath, cfg))
        return 2;
    cfg.output = outputDir.empty() ? fs::absolute(cfg.output) : fs::absolute(outputDir);

    Model model;
    for (const auto &entry : cfg.modules) {
        ModuleDoc mod;
        mod.name = entry.first;
        mod.dirs = entry.second;
        model.modules.push_back(std::move(mod));
    }

    Diagnostics diag;
    const std::vector<fs::path> files = collectSources(cfg);
    if (files.empty()) {
        std::cerr << "ndoc: no source files\n";
        return 2;
    }
    for (const fs::path &file : files) {
        std::string text;
        if (!readFile(file, text)) {
            std::cerr << "ndoc: cannot read " << file.string() << '\n';
            return 2;
        }
        const std::string name = displayPath(file);
        Lexed lexed = lex(text, name);
        Scanner(lexed, model, name, file.extension() == ".h", diag).run();
    }

    markSpecialMembers(model);
    bindDefinitions(model, diag);
    bindTopics(model, diag);

    Site site(model, cfg, diag);
    site.prepare();
    Generator generator(site, diag);
    if (!generator.run())
        return 2;

    const Coverage coverage = writeCoverage(model, cfg.output / "coverage.txt");
    std::cout << "ndoc: " << coverage.classesDocumented << "/" << coverage.classes << " classes, "
              << coverage.membersDocumented << "/" << coverage.members << " members documented, "
              << diag.count() << " warning(s); wrote " << displayPath(cfg.output) << '\n';

    if (strict && diag.count() != 0)
        return 1;
    if (requireComplete && !coverage.complete())
        return 1;
    return 0;
}
