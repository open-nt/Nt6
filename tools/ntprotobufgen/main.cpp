// MIT License - see LICENSE
//
// ntprotobufgen: reads .proto files (proto2 and proto3) and writes, for each, a .qpb.h and a .qpb.cpp with a C++ class
// for each message, in the form of the code that Qt's qtprotobufgen generates, for Nt Protobuf. It has a parser of its
// own, so it needs neither protoc nor libprotobuf. Only the C++ standard library is used, so that it builds as a host
// tool of a cross build.
//
//   ntprotobufgen [-I <dir>]... [--output-dir <dir>] [--root <dir>] <file.proto>...
//
// Imports are looked up in the -I directories and the directory of the importing file; only the files named on the
// command line are generated. The outputs go to <output-dir>/<path>.qpb.{h,cpp}, where <path> is the file's path under
// --root (or under the -I directory that holds it, or else its name), without .proto.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

[[noreturn]] void die(const std::string &message) {
    std::fprintf(stderr, "ntprotobufgen: %s\n", message.c_str());
    std::exit(1);
}

void warn(const std::string &message) { std::fprintf(stderr, "ntprotobufgen: warning: %s\n", message.c_str()); }

// ---------------------------------------------------------------------------------------------------------------------
// Tokens

struct Token {
    enum Kind { Identifier, Number, String, Symbol, End } kind = End;
    std::string text;
    int line = 0;
};

class Lexer {
public:
    Lexer(std::string text, std::string file) : m_text(std::move(text)), m_file(std::move(file)) {}

    std::vector<Token> tokens() {
        std::vector<Token> out;
        for (;;) {
            skipSpace();
            Token t;
            t.line = m_line;
            if (m_pos >= m_text.size()) {
                t.kind = Token::End;
                out.push_back(t);
                return out;
            }
            const char c = m_text[m_pos];
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                t.kind = Token::Identifier;
                while (m_pos < m_text.size() &&
                       (std::isalnum(static_cast<unsigned char>(m_text[m_pos])) || m_text[m_pos] == '_'))
                    t.text += m_text[m_pos++];
            } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                       ((c == '-' || c == '+' || c == '.') && m_pos + 1 < m_text.size() &&
                        (std::isdigit(static_cast<unsigned char>(m_text[m_pos + 1])) || m_text[m_pos + 1] == '.'))) {
                t.kind = Token::Number;
                t.text += m_text[m_pos++];
                while (m_pos < m_text.size()) {
                    const char d = m_text[m_pos];
                    const bool exponentSign = (d == '-' || d == '+') &&
                                              (t.text.back() == 'e' || t.text.back() == 'E') &&
                                              t.text.find_first_of("xX") == std::string::npos;
                    if (std::isalnum(static_cast<unsigned char>(d)) || d == '.' || exponentSign)
                        t.text += m_text[m_pos++];
                    else
                        break;
                }
            } else if (c == '"' || c == '\'') {
                t.kind = Token::String;
                t.text = readString(c);
            } else {
                t.kind = Token::Symbol;
                t.text = std::string(1, c);
                ++m_pos;
            }
            out.push_back(t);
        }
    }

private:
    void skipSpace() {
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if (c == '\n') {
                ++m_line;
                ++m_pos;
            } else if (std::isspace(static_cast<unsigned char>(c))) {
                ++m_pos;
            } else if (c == '/' && m_pos + 1 < m_text.size() && m_text[m_pos + 1] == '/') {
                while (m_pos < m_text.size() && m_text[m_pos] != '\n')
                    ++m_pos;
            } else if (c == '/' && m_pos + 1 < m_text.size() && m_text[m_pos + 1] == '*') {
                m_pos += 2;
                while (m_pos + 1 < m_text.size() && !(m_text[m_pos] == '*' && m_text[m_pos + 1] == '/')) {
                    if (m_text[m_pos] == '\n')
                        ++m_line;
                    ++m_pos;
                }
                m_pos += 2;
            } else {
                break;
            }
        }
    }

    std::string readString(char quote) {
        std::string s;
        ++m_pos;
        while (m_pos < m_text.size() && m_text[m_pos] != quote) {
            char c = m_text[m_pos++];
            if (c == '\\' && m_pos < m_text.size()) {
                const char e = m_text[m_pos++];
                switch (e) {
                case 'n':
                    c = '\n';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case '0':
                    c = '\0';
                    break;
                default:
                    c = e;
                    break;
                }
            }
            s += c;
        }
        if (m_pos >= m_text.size())
            die(m_file + ":" + std::to_string(m_line) + ": unterminated string");
        ++m_pos;
        return s;
    }

    std::string m_text;
    std::string m_file;
    size_t m_pos = 0;
    int m_line = 1;
};

// ---------------------------------------------------------------------------------------------------------------------
// The model of a .proto file

struct Message;
struct Enum;

enum class Label { None, Optional, Repeated, Required };

struct Field {
    std::string name;
    int number = 0;
    Label label = Label::None;
    std::string typeName; // as written, or the key/value of a map below
    bool isMap = false;
    std::string keyType;
    std::string valueType;
    int oneof = -1;
    int packed = -1; // -1 unset, 0 false, 1 true
    std::string jsonName;
    std::string defaultValue;
    bool hasDefault = false;
    // Resolved.
    Message *message = nullptr; // for a message type (or the value of a map)
    Enum *enumType = nullptr;   // for an enum type (or the value of a map)
};

struct Oneof {
    std::string name;
    std::vector<int> fields;
};

struct EnumValue {
    std::string name;
    long long number = 0;
};

struct Enum {
    std::string name;
    std::string fullName; // package.Outer.Name
    Message *parent = nullptr;
    std::vector<EnumValue> values;
    struct File *file = nullptr;
};

struct Message {
    std::string name;
    std::string fullName;
    Message *parent = nullptr;
    std::vector<Field> fields;
    std::vector<Oneof> oneofs;
    std::vector<std::unique_ptr<Message>> messages;
    std::vector<std::unique_ptr<Enum>> enums;
    struct File *file = nullptr;
    bool mapEntry = false;
};

struct File {
    fs::path path;
    std::string relative; // under the root, without .proto
    std::string syntax = "proto2";
    std::string package;
    std::vector<std::string> imports;
    std::vector<std::unique_ptr<Message>> messages;
    std::vector<std::unique_ptr<Enum>> enums;
    std::vector<File *> importFiles;
    bool proto3() const { return syntax == "proto3"; }
};

// ---------------------------------------------------------------------------------------------------------------------
// Parser

class Parser {
public:
    Parser(std::vector<Token> tokens, File *file) : m_tokens(std::move(tokens)), m_file(file) {}

    void parse() {
        while (peek().kind != Token::End) {
            const std::string word = peek().text;
            if (accept(";"))
                continue;
            if (word == "syntax") {
                next();
                expect("=");
                m_file->syntax = expectString();
                expect(";");
                if (m_file->syntax != "proto2" && m_file->syntax != "proto3")
                    fail("unsupported syntax " + m_file->syntax);
            } else if (word == "edition") {
                fail("editions are not supported; use syntax = \"proto3\" or \"proto2\"");
            } else if (word == "package") {
                next();
                m_file->package = fullIdentifier();
                expect(";");
            } else if (word == "import") {
                next();
                if (peek().text == "public" || peek().text == "weak")
                    next();
                m_file->imports.push_back(expectString());
                expect(";");
            } else if (word == "option") {
                skipOption();
            } else if (word == "message") {
                next();
                m_file->messages.push_back(parseMessage(nullptr));
            } else if (word == "enum") {
                next();
                m_file->enums.push_back(parseEnum(nullptr));
            } else if (word == "service" || word == "extend") {
                warn(m_file->path.string() + ": " + word + " is ignored");
                next();
                fullIdentifier();
                skipBlock();
            } else {
                fail("unexpected '" + word + "'");
            }
        }
    }

private:
    const Token &peek(int ahead = 0) const {
        const size_t i = std::min(m_pos + size_t(ahead), m_tokens.size() - 1);
        return m_tokens[i];
    }
    const Token &next() { return m_tokens[std::min(m_pos++, m_tokens.size() - 1)]; }
    bool accept(const std::string &symbol) {
        if (peek().text == symbol && peek().kind != Token::String) {
            next();
            return true;
        }
        return false;
    }
    [[noreturn]] void fail(const std::string &message) const {
        die(m_file->path.string() + ":" + std::to_string(peek().line) + ": " + message);
    }
    void expect(const std::string &symbol) {
        if (!accept(symbol))
            fail("expected '" + symbol + "', found '" + peek().text + "'");
    }
    std::string expectIdentifier() {
        if (peek().kind != Token::Identifier)
            fail("expected a name, found '" + peek().text + "'");
        return next().text;
    }
    std::string expectString() {
        if (peek().kind != Token::String)
            fail("expected a string");
        std::string s = next().text;
        while (peek().kind == Token::String) // adjacent strings are joined
            s += next().text;
        return s;
    }
    long long expectInteger() {
        if (peek().kind != Token::Number)
            fail("expected a number, found '" + peek().text + "'");
        const std::string text = next().text;
        try {
            return std::stoll(text, nullptr, 0);
        } catch (...) {
            fail("invalid number " + text);
        }
    }
    std::string fullIdentifier() {
        std::string name;
        if (accept("."))
            name = ".";
        name += expectIdentifier();
        while (accept("."))
            name += "." + expectIdentifier();
        return name;
    }
    void skipBlock() {
        expect("{");
        int depth = 1;
        while (depth > 0) {
            if (peek().kind == Token::End)
                fail("unbalanced braces");
            const Token &t = next();
            if (t.kind == Token::Symbol && t.text == "{")
                ++depth;
            else if (t.kind == Token::Symbol && t.text == "}")
                --depth;
        }
    }
    // A constant: a name, a number, a string, or an aggregate in braces, which is skipped.
    std::string constant() {
        if (peek().text == "{" && peek().kind == Token::Symbol) {
            skipBlock();
            return std::string();
        }
        if (peek().kind == Token::String)
            return expectString();
        if (peek().text == "-" && peek(1).kind == Token::Identifier) { // -inf
            next();
            return "-" + next().text;
        }
        return next().text;
    }
    std::string optionName() {
        std::string name;
        if (accept("(")) {
            name = "(" + fullIdentifier() + ")";
            expect(")");
        } else {
            name = expectIdentifier();
        }
        while (accept("."))
            name += "." + expectIdentifier();
        return name;
    }
    void skipOption() {
        next(); // option
        optionName();
        expect("=");
        constant();
        expect(";");
    }
    void fieldOptions(Field &field) {
        if (!accept("["))
            return;
        do {
            const std::string name = optionName();
            expect("=");
            const std::string value = constant();
            if (name == "packed")
                field.packed = value == "true" ? 1 : 0;
            else if (name == "json_name")
                field.jsonName = value;
            else if (name == "default") {
                field.defaultValue = value;
                field.hasDefault = true;
            }
        } while (accept(","));
        expect("]");
    }
    void reserved() {
        while (!accept(";")) {
            if (peek().kind == Token::End)
                fail("unterminated reserved");
            next();
        }
    }

    std::unique_ptr<Enum> parseEnum(Message *parent) {
        auto e = std::make_unique<Enum>();
        e->name = expectIdentifier();
        e->parent = parent;
        e->file = m_file;
        e->fullName = scopeName(parent) + e->name;
        expect("{");
        while (!accept("}")) {
            if (accept(";"))
                continue;
            if (peek().text == "option") {
                skipOption();
                continue;
            }
            if (peek().text == "reserved") {
                next();
                reserved();
                continue;
            }
            EnumValue v;
            v.name = expectIdentifier();
            expect("=");
            v.number = expectInteger();
            Field ignored;
            fieldOptions(ignored);
            expect(";");
            e->values.push_back(v);
        }
        if (e->values.empty())
            fail("enum " + e->name + " has no values");
        return e;
    }

    std::string scopeName(const Message *parent) const {
        if (parent)
            return parent->fullName + ".";
        return m_file->package.empty() ? std::string() : m_file->package + ".";
    }

    Field parseField(Label label, int oneof) {
        Field f;
        f.label = label;
        f.oneof = oneof;
        if (peek().text == "map" && peek(1).text == "<") {
            next();
            expect("<");
            f.isMap = true;
            f.keyType = fullIdentifier();
            expect(",");
            f.valueType = fullIdentifier();
            expect(">");
            f.label = Label::Repeated;
        } else {
            f.typeName = fullIdentifier();
        }
        f.name = expectIdentifier();
        expect("=");
        f.number = int(expectInteger());
        fieldOptions(f);
        expect(";");
        return f;
    }

    std::unique_ptr<Message> parseMessage(Message *parent) {
        auto m = std::make_unique<Message>();
        m->name = expectIdentifier();
        m->parent = parent;
        m->file = m_file;
        m->fullName = scopeName(parent) + m->name;
        expect("{");
        while (!accept("}")) {
            const std::string word = peek().text;
            if (accept(";"))
                continue;
            if (word == "message") {
                next();
                m->messages.push_back(parseMessage(m.get()));
            } else if (word == "enum") {
                next();
                m->enums.push_back(parseEnum(m.get()));
            } else if (word == "option") {
                skipOption();
            } else if (word == "reserved" || word == "extensions") {
                next();
                reserved();
            } else if (word == "extend") {
                warn(m_file->path.string() + ": extend is ignored");
                next();
                fullIdentifier();
                skipBlock();
            } else if (word == "oneof") {
                next();
                Oneof o;
                o.name = expectIdentifier();
                const int index = int(m->oneofs.size());
                expect("{");
                while (!accept("}")) {
                    if (accept(";"))
                        continue;
                    if (peek().text == "option") {
                        skipOption();
                        continue;
                    }
                    o.fields.push_back(int(m->fields.size()));
                    m->fields.push_back(parseField(Label::None, index));
                }
                m->oneofs.push_back(o);
            } else {
                Label label = Label::None;
                if (word == "optional")
                    label = Label::Optional;
                else if (word == "repeated")
                    label = Label::Repeated;
                else if (word == "required")
                    label = Label::Required;
                if (label != Label::None)
                    next();
                if (peek().text == "group") {
                    warn(m_file->path.string() + ": groups are not supported; skipped");
                    while (!accept("{"))
                        next();
                    --m_pos;
                    skipBlock();
                    continue;
                }
                m->fields.push_back(parseField(label, -1));
            }
        }
        return m;
    }

    std::vector<Token> m_tokens;
    size_t m_pos = 0;
    File *m_file;
};

// ---------------------------------------------------------------------------------------------------------------------
// Scalar types

struct ScalarInfo {
    const char *proto;
    const char *cpp;  // the QtProtobuf type
    const char *kind; // QtProtobufPrivate::Scalar
    const char *list; // the QtProtobuf list
    int jsonKind;     // 0 32-bit int, 1 int64, 2 uint64, 3 float, 4 double, 5 bool
    const char *minimum;
    const char *maximum;
};

const ScalarInfo kScalars[] = {
    {"double", "double", "Double", "QtProtobuf::doubleList", 4, "", ""},
    {"float", "float", "Float", "QtProtobuf::floatList", 3, "", ""},
    {"int32", "QtProtobuf::int32", "Int32", "QtProtobuf::int32List", 0, "-2147483648LL", "2147483647LL"},
    {"int64", "QtProtobuf::int64", "Int64", "QtProtobuf::int64List", 1, "", ""},
    {"uint32", "QtProtobuf::uint32", "UInt32", "QtProtobuf::uint32List", 0, "0", "4294967295LL"},
    {"uint64", "QtProtobuf::uint64", "UInt64", "QtProtobuf::uint64List", 2, "", ""},
    {"sint32", "QtProtobuf::sint32", "SInt32", "QtProtobuf::sint32List", 0, "-2147483648LL", "2147483647LL"},
    {"sint64", "QtProtobuf::sint64", "SInt64", "QtProtobuf::sint64List", 1, "", ""},
    {"fixed32", "QtProtobuf::fixed32", "Fixed32", "QtProtobuf::fixed32List", 0, "0", "4294967295LL"},
    {"fixed64", "QtProtobuf::fixed64", "Fixed64", "QtProtobuf::fixed64List", 2, "", ""},
    {"sfixed32", "QtProtobuf::sfixed32", "SFixed32", "QtProtobuf::sfixed32List", 0, "-2147483648LL", "2147483647LL"},
    {"sfixed64", "QtProtobuf::sfixed64", "SFixed64", "QtProtobuf::sfixed64List", 1, "", ""},
    {"bool", "bool", "Bool", "QtProtobuf::boolList", 5, "", ""},
};

const ScalarInfo *scalarOf(const std::string &type) {
    for (const ScalarInfo &s : kScalars)
        if (type == s.proto)
            return &s;
    return nullptr;
}

bool isString(const std::string &t) { return t == "string"; }
bool isBytes(const std::string &t) { return t == "bytes"; }

// ---------------------------------------------------------------------------------------------------------------------
// Loading and resolving

class Loader {
public:
    explicit Loader(std::vector<fs::path> includes) : m_includes(std::move(includes)) {}

    File *load(const fs::path &path, const fs::path &root) {
        const fs::path canonical = fs::weakly_canonical(path);
        auto it = m_files.find(canonical.string());
        if (it != m_files.end())
            return it->second.get();
        std::ifstream in(canonical);
        if (!in)
            die("cannot read " + path.string());
        std::stringstream text;
        text << in.rdbuf();
        auto file = std::make_unique<File>();
        file->path = canonical;
        file->relative = relativeName(canonical, root);
        File *raw = file.get();
        m_files[canonical.string()] = std::move(file);
        Parser(Lexer(text.str(), path.string()).tokens(), raw).parse();
        for (const std::string &import : raw->imports)
            raw->importFiles.push_back(load(findImport(import, canonical.parent_path()), fs::path()));
        index(raw);
        return raw;
    }

    // Resolves the field types of file and its imports.
    void resolve(File *file) {
        for (auto &m : file->messages)
            resolveMessage(m.get());
    }

private:
    std::string relativeName(const fs::path &file, const fs::path &root) const {
        std::vector<fs::path> roots;
        if (!root.empty())
            roots.push_back(fs::weakly_canonical(root));
        for (const fs::path &i : m_includes)
            roots.push_back(fs::weakly_canonical(i));
        for (const fs::path &r : roots) {
            const fs::path rel = file.lexically_relative(r);
            if (!rel.empty() && rel.native()[0] != '.') {
                fs::path noExt = rel;
                noExt.replace_extension();
                return noExt.generic_string();
            }
        }
        return file.stem().string();
    }

    fs::path findImport(const std::string &name, const fs::path &from) const {
        std::vector<fs::path> dirs = m_includes;
        dirs.push_back(from);
        for (const fs::path &d : dirs) {
            const fs::path candidate = d / name;
            if (fs::exists(candidate))
                return candidate;
        }
        die("cannot find import \"" + name + "\" (add its directory with -I)");
    }

    void index(File *file) {
        for (auto &m : file->messages)
            indexMessage(m.get());
        for (auto &e : file->enums)
            m_enums[e->fullName] = e.get();
    }
    void indexMessage(Message *m) {
        m_messages[m->fullName] = m;
        for (auto &n : m->messages)
            indexMessage(n.get());
        for (auto &e : m->enums)
            m_enums[e->fullName] = e.get();
    }

    // Looks name up as protoc does: from the innermost scope outwards.
    void lookup(const std::string &name, const std::string &scope, Message **message, Enum **enumType) {
        *message = nullptr;
        *enumType = nullptr;
        if (!name.empty() && name[0] == '.') {
            const std::string full = name.substr(1);
            if (m_messages.count(full))
                *message = m_messages[full];
            else if (m_enums.count(full))
                *enumType = m_enums[full];
            return;
        }
        std::string s = scope;
        for (;;) {
            const std::string full = s.empty() ? name : s + "." + name;
            if (m_messages.count(full)) {
                *message = m_messages[full];
                return;
            }
            if (m_enums.count(full)) {
                *enumType = m_enums[full];
                return;
            }
            if (s.empty())
                return;
            const size_t dot = s.rfind('.');
            s = dot == std::string::npos ? std::string() : s.substr(0, dot);
        }
    }

    void resolveType(const std::string &type, const std::string &scope, Field &f, const Message *m) {
        if (scalarOf(type) || isString(type) || isBytes(type))
            return;
        lookup(type, scope, &f.message, &f.enumType);
        if (!f.message && !f.enumType)
            die(m->file->path.string() + ": unknown type " + type + " of field " + m->name + "." + f.name);
    }

    void resolveMessage(Message *m) {
        for (Field &f : m->fields) {
            if (f.isMap) {
                if (!scalarOf(f.keyType) && !isString(f.keyType))
                    die(m->file->path.string() + ": invalid map key type " + f.keyType);
                if (f.keyType == "float" || f.keyType == "double")
                    die(m->file->path.string() + ": a map key cannot be " + f.keyType);
                resolveType(f.valueType, m->fullName, f, m);
            } else {
                resolveType(f.typeName, m->fullName, f, m);
            }
        }
        for (auto &n : m->messages)
            resolveMessage(n.get());
    }

    std::vector<fs::path> m_includes;
    std::map<std::string, std::unique_ptr<File>> m_files;
    std::map<std::string, Message *> m_messages;
    std::map<std::string, Enum *> m_enums;
};

// ---------------------------------------------------------------------------------------------------------------------
// Names

const std::set<std::string> kKeywords = {
    "alignas",     "alignof",   "and",         "asm",          "auto",      "bool",     "break",    "case",
    "catch",       "char",      "class",       "const",        "constexpr", "continue", "default",  "delete",
    "do",          "double",    "else",        "enum",         "explicit",  "export",   "extern",   "false",
    "float",       "for",       "friend",      "goto",         "if",        "inline",   "int",      "long",
    "mutable",     "namespace", "new",         "noexcept",     "not",       "nullptr",  "operator", "or",
    "private",     "protected", "public",      "register",     "return",    "short",    "signed",   "sizeof",
    "static",      "struct",    "switch",      "template",     "this",      "throw",    "true",     "try",
    "typedef",     "typeid",    "typename",    "union",        "unsigned",  "using",    "virtual",  "void",
    "volatile",    "while",     "xor",         "signals",      "slots",     "emit",     "dptr",     "serialize",
    "deserialize", "property",  "setProperty", "registerTypes"};

// The JSON name of a field: underscores dropped and the letter after each capitalised.
std::string jsonNameOf(const std::string &name) {
    std::string out;
    bool upper = false;
    for (char c : name) {
        if (c == '_') {
            upper = true;
            continue;
        }
        out += upper ? char(std::toupper(static_cast<unsigned char>(c))) : c;
        upper = false;
    }
    return out;
}

std::string camel(const std::string &name) {
    std::string out = jsonNameOf(name);
    if (!out.empty())
        out[0] = char(std::tolower(static_cast<unsigned char>(out[0])));
    if (kKeywords.count(out))
        out += "_";
    return out;
}

std::string capital(const std::string &name) {
    std::string out = jsonNameOf(name);
    if (!out.empty())
        out[0] = char(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

std::string nsOf(const std::string &package) {
    std::string out;
    std::stringstream ss(package);
    std::string part;
    while (std::getline(ss, part, '.')) {
        if (!out.empty())
            out += "::";
        out += part;
    }
    return out;
}

// The namespace that the nested types of m are in, relative to the package: A_QtProtobufNested::B_QtProtobufNested.
std::string nestedNs(const Message *m) {
    std::string chain;
    for (const Message *p = m; p; p = p->parent)
        chain = p->name + "_QtProtobufNested" + (chain.empty() ? "" : "::" + chain);
    return chain;
}

std::string parentPath(const Message *parent) { return parent ? nestedNs(parent) + "::" : std::string(); }

std::string qualifiedMessage(const Message *m) {
    const std::string ns = nsOf(m->file->package);
    return "::" + (ns.empty() ? "" : ns + "::") + parentPath(m->parent) + m->name;
}

std::string qualifiedEnum(const Enum *e) {
    const std::string ns = nsOf(e->file->package);
    return "::" + (ns.empty() ? "" : ns + "::") + parentPath(e->parent) + e->name;
}

std::string mangle(const std::string &fullName) {
    std::string out;
    for (char c : fullName)
        out += c == '.' ? '_' : c;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Field types in C++

struct FieldCpp {
    std::string element; // the type of one value
    std::string storage; // the type of the member
    std::string param;   // the parameter type of the setter
    bool byReference = false;
};

std::string elementType(const std::string &type, const Field &f, bool valueOfMap) {
    (void)valueOfMap;
    if (const ScalarInfo *s = scalarOf(type))
        return s->cpp;
    if (isString(type))
        return "QString";
    if (isBytes(type))
        return "QByteArray";
    if (f.message)
        return qualifiedMessage(f.message);
    if (f.enumType)
        return qualifiedEnum(f.enumType);
    return "void";
}

FieldCpp cppOf(const Field &f) {
    FieldCpp c;
    if (f.isMap) {
        const std::string key = elementType(f.keyType, f, false);
        const std::string value = elementType(f.valueType, f, true);
        c.element = value;
        c.storage = "QHash<" + key + ", " + value + ">";
        c.param = "const " + c.storage + " &";
        c.byReference = true;
        return c;
    }
    c.element = elementType(f.typeName, f, false);
    if (f.label == Label::Repeated) {
        const ScalarInfo *s = scalarOf(f.typeName);
        c.storage = s ? s->list : "QList<" + c.element + ">";
        c.param = "const " + c.storage + " &";
        c.byReference = true;
        return c;
    }
    c.storage = c.element;
    if (f.message || isString(f.typeName) || isBytes(f.typeName)) {
        c.param = "const " + c.element + " &";
        c.byReference = f.message != nullptr;
    } else {
        c.param = c.element;
    }
    return c;
}

bool isPacked(const File *file, const Field &f) {
    if (f.label != Label::Repeated || f.isMap)
        return false;
    if (!scalarOf(f.typeName) && !f.enumType)
        return false;
    if (f.packed >= 0)
        return f.packed == 1;
    return file->proto3();
}

// Whether a singular field tracks whether it was set: proto3 optional, proto2 optional and required, messages, oneofs.
bool hasPresence(const File *file, const Field &f) {
    if (f.label == Label::Repeated)
        return false;
    if (f.oneof >= 0)
        return false; // the case of the oneof tells
    if (f.message)
        return true;
    if (file->proto3())
        return f.label == Label::Optional;
    return true;
}

// The default value of a singular field, as a C++ expression.
std::string defaultOf(const Field &f) {
    const FieldCpp c = cppOf(f);
    if (f.hasDefault) {
        if (isString(f.typeName))
            return "QStringLiteral(" + [&] {
                std::string s = "\"";
                for (char ch : f.defaultValue) {
                    if (ch == '"' || ch == '\\')
                        s += '\\';
                    s += ch;
                }
                return s + "\"";
            }() + ")";
        if (isBytes(f.typeName)) {
            std::string s = "QByteArray(\"";
            for (char ch : f.defaultValue) {
                char buffer[8];
                std::snprintf(buffer, sizeof buffer, "\\x%02x", static_cast<unsigned char>(ch));
                s += buffer;
            }
            return s + "\", " + std::to_string(f.defaultValue.size()) + ")";
        }
        if (f.enumType)
            return c.element + "::" + f.defaultValue;
        if (f.typeName == "float" || f.typeName == "double") {
            if (f.defaultValue == "inf")
                return "std::numeric_limits<" + c.element + ">::infinity()";
            if (f.defaultValue == "-inf")
                return "-std::numeric_limits<" + c.element + ">::infinity()";
            if (f.defaultValue == "nan")
                return "std::numeric_limits<" + c.element + ">::quiet_NaN()";
            return c.element + "(" + f.defaultValue + ")";
        }
        if (f.typeName == "bool")
            return f.defaultValue;
        return c.element + "(" + f.defaultValue + (f.typeName.find("64") != std::string::npos ? "LL" : "") + ")";
    }
    if (f.enumType)
        return c.element + "(" + std::to_string(f.enumType->values.front().number) + ")";
    if (scalarOf(f.typeName))
        return c.element + "()";
    return c.element + "()";
}

// ---------------------------------------------------------------------------------------------------------------------
// Generator

class Generator {
public:
    Generator(File *file, std::string headerName) : m_file(file), m_headerName(std::move(headerName)) {}

    std::string header() {
        std::ostringstream h;
        std::string guard;
        for (char c : m_file->relative + "_QPB_H")
            guard +=
                std::isalnum(static_cast<unsigned char>(c)) ? char(std::toupper(static_cast<unsigned char>(c))) : '_';
        h << "// Generated by ntprotobufgen from " << m_file->path.filename().string() << ". Do not edit.\n";
        h << "#ifndef " << guard << "\n#define " << guard << "\n\n";
        h << "#include <QtCore/qbytearray.h>\n#include <QtCore/qhash.h>\n#include <QtCore/qlist.h>\n"
             "#include <QtCore/qshareddata.h>\n#include <QtCore/qstring.h>\n#include <QtProtobuf/qprotobufmessage.h>\n"
             "#include <QtProtobuf/qtprotobuftypes.h>\n\n#include <cstdint>\n";
        for (const File *import : m_file->importFiles)
            h << "#include \"" << import->relative << ".qpb.h\"\n";
        h << "\n";
        const std::string ns = nsOf(m_file->package);
        if (!ns.empty())
            h << "namespace " << ns << " {\n\n";
        // Declarations of every class, then the enums, then the classes, so that each can refer to every other.
        for (auto &m : m_file->messages)
            declare(h, m.get());
        h << "\n";
        for (auto &e : m_file->enums)
            enumDefinition(h, e.get(), "");
        for (auto &m : m_file->messages)
            nestedEnums(h, m.get());
        for (auto &m : m_file->messages)
            classes(h, m.get());
        if (!ns.empty())
            h << "} // namespace " << ns << "\n";
        h << "\n#endif // " << guard << "\n";
        return h.str();
    }

    std::string source() {
        std::ostringstream s;
        s << "// Generated by ntprotobufgen from " << m_file->path.filename().string() << ". Do not edit.\n";
        s << "#include \"" << m_headerName << "\"\n\n";
        s << "#include <QtCore/qjsonarray.h>\n#include <QtCore/qjsonobject.h>\n#include "
             "<QtProtobuf/qprotobufcodec_p.h>\n\n"
             "#include <algorithm>\n#include <limits>\n#include <utility>\n\n";
        s << "namespace {\n\n";
        // The JSON names of every enum that a field of this file has, wherever it is declared.
        std::vector<const Enum *> enums;
        for (auto &e : m_file->enums)
            enums.push_back(e.get());
        for (auto &m : m_file->messages)
            collectEnums(m.get(), enums);
        for (const Enum *e : enums)
            enumHelpers(s, e);
        s << "} // namespace\n\n";
        for (auto &m : m_file->messages)
            implementation(s, m.get());
        return s.str();
    }

private:
    // --- header --------------------------------------------------------------------------------------------------

    void openNested(std::ostream &h, const Message *parent) {
        if (parent)
            h << "namespace " << nestedNs(parent) << " {\n";
    }
    void closeNested(std::ostream &h, const Message *parent) {
        if (parent)
            h << "} // namespace " << nestedNs(parent) << "\n";
    }

    void declare(std::ostream &h, const Message *m) {
        openNested(h, m->parent);
        h << "class " << m->name << ";\n";
        h << "using " << m->name << "Repeated = QList<" << m->name << ">;\n";
        closeNested(h, m->parent);
        for (auto &n : m->messages)
            declare(h, n.get());
    }

    void enumDefinition(std::ostream &h, const Enum *e, const std::string &) {
        openNested(h, e->parent);
        h << "enum class " << e->name << " : int32_t {\n";
        std::set<long long> seen;
        for (const EnumValue &v : e->values)
            h << "    " << v.name << " = " << v.number << ",\n";
        h << "};\n";
        h << "using " << e->name << "Repeated = QList<" << e->name << ">;\n";
        closeNested(h, e->parent);
        h << "\n";
    }

    void nestedEnums(std::ostream &h, const Message *m) {
        for (auto &e : m->enums)
            enumDefinition(h, e.get(), "");
        for (auto &n : m->messages)
            nestedEnums(h, n.get());
    }

    void classes(std::ostream &h, const Message *m) {
        for (auto &n : m->messages) // nested classes first: the outer class names them
            classes(h, n.get());
        openNested(h, m->parent);
        const std::string data = m->name + "_QtProtobufData";
        h << "class " << data << ";\n";
        h << "class " << m->name << " : public QProtobufMessage {\npublic:\n";
        for (auto &n : m->messages)
            h << "    using " << n->name << " = " << nestedNs(m) << "::" << n->name << ";\n";
        for (auto &e : m->enums)
            h << "    using " << e->name << " = " << nestedNs(m) << "::" << e->name << ";\n";
        h << "    enum QtProtobufFieldEnum {\n";
        for (const Field &f : m->fields)
            h << "        " << capital(f.name) << "ProtoFieldNumber = " << f.number << ",\n";
        h << "    };\n";
        for (const Oneof &o : m->oneofs) {
            h << "    enum class " << capital(o.name)
              << "Fields {\n        UninitializedField = QtProtobuf::InvalidFieldNumber,\n";
            for (int i : o.fields)
                h << "        " << capital(m->fields.at(size_t(i)).name) << " = " << m->fields.at(size_t(i)).number
                  << ",\n";
            h << "    };\n";
        }
        h << "\n    " << m->name << "();\n    ~" << m->name << "() override;\n";
        h << "    " << m->name << "(const " << m->name << " &other);\n";
        h << "    " << m->name << " &operator=(const " << m->name << " &other);\n";
        h << "    " << m->name << "(" << m->name << " &&other) noexcept;\n";
        h << "    " << m->name << " &operator=(" << m->name << " &&other) noexcept;\n";
        h << "    bool operator==(const " << m->name << " &other) const;\n";
        h << "    bool operator!=(const " << m->name << " &other) const { return !(*this == other); }\n\n";
        for (const Field &f : m->fields) {
            const FieldCpp c = cppOf(f);
            const std::string name = camel(f.name);
            const std::string cap = capital(f.name);
            if (c.byReference) {
                h << "    const " << c.storage << " &" << name << "() const &;\n";
                h << "    " << c.storage << " &mut" << cap << "() &;\n";
            } else {
                h << "    " << c.storage << " " << name << "() const;\n";
            }
            h << "    void set" << cap << "(" << c.param << (c.param.back() == '&' ? "" : " ") << name << ");\n";
            if (c.byReference)
                h << "    void set" << cap << "(" << c.storage << " &&" << name << ");\n";
            if (hasPresence(m_file, f) || f.oneof >= 0)
                h << "    bool has" << cap << "() const;\n";
            if (hasPresence(m_file, f))
                h << "    void clear" << cap << "();\n";
        }
        for (const Oneof &o : m->oneofs) {
            h << "    " << capital(o.name) << "Fields " << camel(o.name) << "Field() const;\n";
            h << "    void clear" << capital(o.name) << "();\n";
        }
        h << "\n    const QtProtobufPrivate::QProtobufPropertyOrdering *propertyOrdering() const override;\n";
        h << "    static void registerTypes();\n";
        h << "\nprotected:\n";
        h << "    void ntClear() override;\n    void ntEncode(QtProtobufPrivate::Writer &writer) const override;\n"
             "    bool ntDecode(QtProtobufPrivate::Reader &reader) override;\n"
             "    void ntToJson(QJsonObject &object) const override;\n"
             "    bool ntFromJson(const QJsonObject &object, QString *error) override;\n"
             "    QVariant ntProperty(QAnyStringView name) const override;\n"
             "    bool ntSetProperty(QAnyStringView name, const QVariant &value) override;\n";
        h << "\nprivate:\n    QSharedDataPointer<" << data << "> dptr;\n};\n";
        closeNested(h, m->parent);
        h << "\n";
    }

    // --- source --------------------------------------------------------------------------------------------------

    void enumHelpers(std::ostream &s, const Enum *e) {
        const std::string t = qualifiedEnum(e);
        const std::string id = mangle(e->fullName);
        s << "QJsonValue enumToJson_" << id << "(" << t << " value) {\n    switch (qint32(value)) {\n";
        std::set<long long> seen;
        for (const EnumValue &v : e->values) {
            if (!seen.insert(v.number).second)
                continue; // an alias
            s << "    case " << v.number << ":\n        return QJsonValue(QStringLiteral(\"" << v.name << "\"));\n";
        }
        s << "    default:\n        return QJsonValue(double(qint32(value)));\n    }\n}\n\n";
        s << "bool enumFromJson_" << id << "(const QJsonValue &json, " << t << " *value) {\n";
        s << "    if (json.isString()) {\n        const QString name = json.toString();\n";
        for (const EnumValue &v : e->values)
            s << "        if (name == QLatin1String(\"" << v.name << "\")) {\n            *value = " << t << "("
              << v.number << ");\n            return true;\n        }\n";
        s << "        return false;\n    }\n";
        s << "    qint64 number = 0;\n    if (!QtProtobufPrivate::Json::toInteger(json, &number, -2147483648LL, "
             "2147483647LL))\n"
             "        return false;\n    *value = "
          << t << "(qint32(number));\n    return true;\n}\n\n";
    }

    static void addEnum(std::vector<const Enum *> &enums, const Enum *e) {
        if (e && std::find(enums.begin(), enums.end(), e) == enums.end())
            enums.push_back(e);
    }
    void collectEnums(const Message *m, std::vector<const Enum *> &enums) {
        for (auto &e : m->enums)
            addEnum(enums, e.get());
        for (const Field &f : m->fields)
            addEnum(enums, f.enumType);
        for (auto &n : m->messages)
            collectEnums(n.get(), enums);
    }

    // The Writer call for one value of type (proto) in field number, given a C++ expression.
    std::string writeValue(const std::string &type, const Field &f, int number, const std::string &expr,
                           const std::string &force, const std::string &writer) {
        const std::string n = std::to_string(number);
        if (const ScalarInfo *sc = scalarOf(type)) {
            std::string v = expr;
            if (type != "float" && type != "double")
                v = "qint64(" + expr + ")";
            return writer + ".scalar(" + n + ", QtProtobufPrivate::Scalar::" + sc->kind + ", " + v + ", " + force +
                   ");";
        }
        if (isString(type))
            return writer + ".string(" + n + ", " + expr + ", " + force + ");";
        if (isBytes(type))
            return writer + ".bytes(" + n + ", " + expr + ", " + force + ");";
        if (f.enumType)
            return writer + ".scalar(" + n + ", QtProtobufPrivate::Scalar::Int32, qint64(qint32(" + expr + ")), " +
                   force + ");";
        return writer + ".message(" + n + ", " + expr + ");";
    }

    // Statements that read one value of type from reader into the variable target (declared).
    std::string readValue(const std::string &type, const Field &f, const std::string &target,
                          const std::string &reader) {
        if (const ScalarInfo *sc = scalarOf(type)) {
            if (type == "float")
                return target + " = " + reader + ".floatValue();";
            if (type == "double")
                return target + " = " + reader + ".doubleValue();";
            if (type == "bool")
                return target + " = " + reader + ".boolean();";
            return target + " = " + std::string(sc->cpp) + "(" + reader +
                   ".integer(QtProtobufPrivate::Scalar::" + sc->kind + "));";
        }
        if (isString(type))
            return target + " = " + reader + ".string();";
        if (isBytes(type))
            return target + " = " + reader + ".bytes();";
        if (f.enumType)
            return target + " = " + qualifiedEnum(f.enumType) + "(qint32(" + reader +
                   ".integer(QtProtobufPrivate::Scalar::Int32)));";
        return reader + ".message(&" + target + ");";
    }

    // A JSON value from a C++ expression of type.
    std::string toJson(const std::string &type, const Field &f, const std::string &expr) {
        if (const ScalarInfo *sc = scalarOf(type)) {
            switch (sc->jsonKind) {
            case 0:
                return "QJsonValue(double(" + expr + "))";
            case 1:
                return "QtProtobufPrivate::Json::fromInt64(" + expr + ")";
            case 2:
                return "QtProtobufPrivate::Json::fromUInt64(" + expr + ")";
            case 3:
                return "QtProtobufPrivate::Json::fromFloat(" + expr + ")";
            case 4:
                return "QtProtobufPrivate::Json::fromDouble(" + expr + ")";
            default:
                return "QJsonValue(bool(" + expr + "))";
            }
        }
        if (isString(type))
            return "QJsonValue(" + expr + ")";
        if (isBytes(type))
            return "QtProtobufPrivate::Json::fromBytes(" + expr + ")";
        if (f.enumType)
            return "enumToJson_" + mangle(f.enumType->fullName) + "(" + expr + ")";
        return "QtProtobufPrivate::Json::fromMessage(" + expr + ")";
    }

    // Statements that parse json (a QJsonValue) into the variable target, or fail with message.
    std::string fromJson(const std::string &type, const Field &f, const std::string &json, const std::string &target,
                         const std::string &what) {
        const std::string fail = "{ *error = QStringLiteral(\"" + what + " has an invalid value\"); return false; }";
        if (const ScalarInfo *sc = scalarOf(type)) {
            switch (sc->jsonKind) {
            case 0:
                return "{ qint64 v = 0; if (!QtProtobufPrivate::Json::toInteger(" + json + ", &v, " + sc->minimum +
                       ", " + sc->maximum + ")) " + fail + " " + target + " = " + sc->cpp + "(v); }";
            case 1:
                return "{ qint64 v = 0; if (!QtProtobufPrivate::Json::toInteger(" + json +
                       ", &v, std::numeric_limits<qint64>::min(), std::numeric_limits<qint64>::max())) " + fail + " " +
                       target + " = v; }";
            case 2:
                return "{ quint64 v = 0; if (!QtProtobufPrivate::Json::toUInt64(" + json + ", &v)) " + fail + " " +
                       target + " = v; }";
            case 3:
                return "if (!QtProtobufPrivate::Json::toFloat(" + json + ", &" + target + ")) " + fail;
            case 4:
                return "if (!QtProtobufPrivate::Json::toDouble(" + json + ", &" + target + ")) " + fail;
            default:
                return "if (!QtProtobufPrivate::Json::toBool(" + json + ", &" + target + ")) " + fail;
            }
        }
        if (isString(type))
            return "if (!QtProtobufPrivate::Json::toString(" + json + ", &" + target + ")) " + fail;
        if (isBytes(type))
            return "if (!QtProtobufPrivate::Json::toBytes(" + json + ", &" + target + ")) " + fail;
        if (f.enumType)
            return "if (!enumFromJson_" + mangle(f.enumType->fullName) + "(" + json + ", &" + target + ")) " + fail;
        return "if (!QtProtobufPrivate::Json::toMessage(" + json + ", &" + target + ", error)) return false;";
    }

    std::string mapKeyFromString(const std::string &keyType, const std::string &text, const std::string &target,
                                 const std::string &what) {
        const std::string fail = "{ *error = QStringLiteral(\"" + what + " has an invalid key\"); return false; }";
        if (isString(keyType))
            return target + " = " + text + ";";
        if (keyType == "bool")
            return "if (" + text + " == QLatin1String(\"true\")) " + target + " = true; else if (" + text +
                   " == QLatin1String(\"false\")) " + target + " = false; else " + fail;
        const ScalarInfo *sc = scalarOf(keyType);
        if (sc->jsonKind == 2)
            return "{ bool ok = false; " + target + " = " + text + ".toULongLong(&ok); if (!ok) " + fail + " }";
        return "{ bool ok = false; const qint64 v = " + text + ".toLongLong(&ok); if (!ok) " + fail + " " + target +
               " = " + sc->cpp + "(v); }";
    }

    std::string mapKeyToString(const std::string &keyType, const std::string &expr) {
        if (isString(keyType))
            return expr;
        if (keyType == "bool")
            return "(" + expr + " ? QStringLiteral(\"true\") : QStringLiteral(\"false\"))";
        return "QString::number(" + expr + ")";
    }

    // The statement that puts a field of the data d back to its default.
    std::string resetOf(const Field &f) {
        const std::string name = "d->" + camel(f.name);
        if (f.message && f.label != Label::Repeated && !f.isMap)
            return name + ".reset();";
        if (f.label == Label::Repeated || f.isMap)
            return name + ".clear();";
        return name + " = " + defaultOf(f) + ";";
    }

    void implementation(std::ostream &s, const Message *m) {
        for (auto &n : m->messages)
            implementation(s, n.get());
        const std::string cls = qualifiedMessage(m).substr(2); // no leading ::, which would join the return type
        const std::string pkg = nsOf(m->file->package);
        const std::string data =
            "::" + (pkg.empty() ? std::string() : pkg + "::") + parentPath(m->parent) + m->name + "_QtProtobufData";
        const std::string id = mangle(m->fullName);
        // The data.
        const std::string ns = nsOf(m->file->package);
        if (!ns.empty())
            s << "namespace " << ns << " {\n";
        openNested(s, m->parent);
        s << "class " << m->name << "_QtProtobufData : public QSharedData {\npublic:\n";
        for (const Field &f : m->fields) {
            const FieldCpp c = cppOf(f);
            const bool singular = f.label != Label::Repeated && !f.isMap;
            if (singular && f.message) {
                s << "    QtProtobufPrivate::MessageField<" << c.storage << "> " << camel(f.name) << ";\n";
                if (hasPresence(m_file, f))
                    s << "    bool has_" << camel(f.name) << " = false;\n";
                continue;
            }
            s << "    " << c.storage << " " << camel(f.name);
            if (singular && !f.message && !isString(f.typeName) && !isBytes(f.typeName))
                s << " = " << defaultOf(f);
            else if (singular && f.hasDefault)
                s << " = " << defaultOf(f);
            s << ";\n";
            if (hasPresence(m_file, f))
                s << "    bool has_" << camel(f.name) << " = false;\n";
        }
        for (const Oneof &o : m->oneofs)
            s << "    " << cls << "::" << capital(o.name) << "Fields " << camel(o.name) << "Case = " << cls
              << "::" << capital(o.name) << "Fields::UninitializedField;\n";
        s << "};\n";
        closeNested(s, m->parent);
        if (!ns.empty())
            s << "} // namespace " << ns << "\n";
        s << "\n";

        // Ordering and registration.
        s << "namespace {\nQProtobufMessage *create_" << id << "() { return new " << cls << "; }\n";
        s << "const QtProtobufPrivate::QProtobufPropertyOrdering ordering_" << id << " = {\"" << m->fullName
          << "\", &create_" << id << "};\n";
        s << "const QtProtobufPrivate::MessageRegistration registration_" << id << "(&ordering_" << id << ");\n}\n\n";

        // Special members.
        s << cls << "::" << m->name << "() : dptr(new " << data << ") {}\n";
        s << cls << "::~" << m->name << "() = default;\n";
        s << cls << "::" << m->name << "(const " << m->name << " &other) = default;\n";
        s << cls << " &" << cls << "::operator=(const " << m->name << " &other) = default;\n";
        s << cls << "::" << m->name << "(" << m->name << " &&other) noexcept = default;\n";
        s << cls << " &" << cls << "::operator=(" << m->name << " &&other) noexcept = default;\n";
        s << "bool " << cls << "::operator==(const " << m->name
          << " &other) const {\n    if (dptr == other.dptr)\n        return true;\n    return true";
        for (const Field &f : m->fields) {
            s << "\n        && dptr->" << camel(f.name) << " == other.dptr->" << camel(f.name);
            if (hasPresence(m_file, f))
                s << "\n        && dptr->has_" << camel(f.name) << " == other.dptr->has_" << camel(f.name);
        }
        for (const Oneof &o : m->oneofs)
            s << "\n        && dptr->" << camel(o.name) << "Case == other.dptr->" << camel(o.name) << "Case";
        s << ";\n}\n";
        s << "const QtProtobufPrivate::QProtobufPropertyOrdering *" << cls
          << "::propertyOrdering() const { return &ordering_" << id << "; }\n";
        s << "void " << cls << "::registerTypes() {}\n";
        s << "void " << cls << "::ntClear() {\n    QProtobufMessage::ntClear();\n    dptr = QSharedDataPointer<" << data
          << ">(new " << data << ");\n}\n\n";

        // Accessors.
        for (size_t i = 0; i < m->fields.size(); ++i) {
            const Field &f = m->fields[i];
            const FieldCpp c = cppOf(f);
            const std::string name = camel(f.name);
            const std::string cap = capital(f.name);
            const bool presence = hasPresence(m_file, f);
            std::string setCase;
            if (f.oneof >= 0) {
                const Oneof &o = m->oneofs[size_t(f.oneof)];
                setCase = "    if (d->" + camel(o.name) + "Case != " + cls + "::" + capital(o.name) + "Fields::" + cap +
                          ") {\n";
                for (int other : o.fields)
                    if (size_t(other) != i)
                        setCase += "        " + resetOf(m->fields[size_t(other)]) + "\n";
                setCase += "        d->" + camel(o.name) + "Case = " + cls + "::" + capital(o.name) + "Fields::" + cap +
                           ";\n    }\n";
            }
            const std::string mark = presence ? "    d->has_" + name + " = true;\n" : "";
            const bool boxed = f.message && f.label != Label::Repeated && !f.isMap;
            const std::string get = boxed ? ".get()" : "";
            const std::string mut = boxed ? ".mut()" : "";
            if (c.byReference) {
                s << "const " << c.storage << " &" << cls << "::" << name << "() const & { return dptr->" << name << get
                  << "; }\n";
                s << c.storage << " &" << cls << "::mut" << cap << "() & {\n    auto *d = dptr.data();\n"
                  << setCase << mark << "    return d->" << name << mut << ";\n}\n";
            } else {
                s << c.storage << " " << cls << "::" << name << "() const { return dptr->" << name << "; }\n";
            }
            s << "void " << cls << "::set" << cap << "(" << c.param << (c.param.back() == '&' ? "" : " ") << name
              << ") {\n    auto *d = dptr.data();\n"
              << setCase << mark << "    d->" << name << mut << " = " << name << ";\n}\n";
            if (c.byReference)
                s << "void " << cls << "::set" << cap << "(" << c.storage << " &&" << name
                  << ") {\n    auto *d = dptr.data();\n"
                  << setCase << mark << "    d->" << name << mut << " = std::move(" << name << ");\n}\n";
            if (presence) {
                s << "bool " << cls << "::has" << cap << "() const { return dptr->has_" << name << "; }\n";
                s << "void " << cls << "::clear" << cap << "() {\n    auto *d = dptr.data();\n    d->has_" << name
                  << " = false;\n    " << resetOf(f) << "\n}\n";
            }
            if (f.oneof >= 0) {
                const Oneof &o = m->oneofs[size_t(f.oneof)];
                s << "bool " << cls << "::has" << cap << "() const { return dptr->" << camel(o.name)
                  << "Case == " << cls << "::" << capital(o.name) << "Fields::" << cap << "; }\n";
            }
        }
        for (const Oneof &o : m->oneofs) {
            s << cls << "::" << capital(o.name) << "Fields " << cls << "::" << camel(o.name)
              << "Field() const { return dptr->" << camel(o.name) << "Case; }\n";
            s << "void " << cls << "::clear" << capital(o.name) << "() {\n    auto *d = dptr.data();\n";
            for (int i : o.fields)
                s << "    " << resetOf(m->fields[size_t(i)]) << "\n";
            s << "    d->" << camel(o.name) << "Case = " << cls << "::" << capital(o.name)
              << "Fields::UninitializedField;\n}\n";
        }
        s << "\n";

        // Fields in the order of their numbers, as encoders write them.
        std::vector<const Field *> ordered;
        for (const Field &f : m->fields)
            ordered.push_back(&f);
        std::sort(ordered.begin(), ordered.end(), [](const Field *a, const Field *b) { return a->number < b->number; });

        // Encode.
        s << "void " << cls << "::ntEncode(QtProtobufPrivate::Writer &writer) const {\n";
        if (m->fields.empty())
            s << "    (void)writer;\n";
        else
            s << "    const auto *d = dptr.constData();\n";
        for (const Field *fp : ordered) {
            const Field &f = *fp;
            const std::string name = camel(f.name);
            const std::string value =
                "d->" + name + (f.message && f.label != Label::Repeated && !f.isMap ? ".get()" : "");
            if (f.isMap) {
                const FieldCpp c = cppOf(f);
                s << "    {\n        auto keys = " << value
                  << ".keys();\n        std::sort(keys.begin(), keys.end());\n";
                s << "        for (const auto &key : keys) {\n            QByteArray entry;\n"
                     "            QtProtobufPrivate::Writer e(&entry);\n";
                s << "            " << writeValue(f.keyType, f, 1, "key", "true", "e") << "\n";
                s << "            " << writeValue(f.valueType, f, 2, value + ".value(key)", "true", "e") << "\n";
                s << "            writer.bytes(" << f.number << ", entry, true);\n        }\n    }\n";
                (void)c;
            } else if (f.label == Label::Repeated) {
                if (scalarOf(f.typeName) || f.enumType) {
                    const ScalarInfo *sc = scalarOf(f.typeName);
                    s << "    writer.repeated(" << f.number << ", " << value
                      << ", QtProtobufPrivate::Scalar::" << (sc ? sc->kind : "Int32") << ", "
                      << (isPacked(m_file, f) ? "true" : "false") << ");\n";
                } else {
                    s << "    for (const auto &item : " << value << ")\n        "
                      << writeValue(f.typeName, f, f.number, "item", "true", "writer") << "\n";
                }
            } else {
                std::string condition;
                std::string force = "false";
                if (f.oneof >= 0) {
                    const Oneof &o = m->oneofs[size_t(f.oneof)];
                    condition = "d->" + camel(o.name) + "Case == " + cls + "::" + capital(o.name) +
                                "Fields::" + capital(f.name);
                    force = "true";
                } else if (hasPresence(m_file, f)) {
                    condition = "d->has_" + name;
                    force = "true";
                }
                if (!condition.empty())
                    s << "    if (" << condition << ")\n    ";
                s << "    " << writeValue(f.typeName, f, f.number, value, force, "writer") << "\n";
            }
        }
        s << "}\n\n";

        // Decode.
        s << "bool " << cls
          << "::ntDecode(QtProtobufPrivate::Reader &reader) {\n    int field = 0;\n"
             "    QtProtobuf::WireTypes wire = QtProtobuf::WireTypes::Unknown;\n    while (reader.next(&field, &wire)) "
             "{\n";
        s << "        switch (field) {\n";
        for (const Field &f : m->fields) {
            const FieldCpp c = cppOf(f);
            const std::string name = camel(f.name);
            const std::string cap = capital(f.name);
            s << "        case " << f.number << ": {\n";
            if (f.isMap) {
                const std::string keyCpp = elementType(f.keyType, f, false);
                s << "            QtProtobufPrivate::Reader e = reader.entry();\n            " << keyCpp
                  << " key{};\n            " << c.element << " value{};\n";
                s << "            int ef = 0;\n            QtProtobuf::WireTypes ew = "
                     "QtProtobuf::WireTypes::Unknown;\n";
                s << "            while (e.next(&ef, &ew)) {\n                if (ef == 1) {\n                    "
                  << readValue(f.keyType, f, "key", "e")
                  << "\n                } else if (ef == 2) {\n                    "
                  << readValue(f.valueType, f, "value", "e")
                  << "\n                } else {\n                    e.skip();\n                }\n            }\n";
                s << "            if (e.failed())\n                return reader.fail(e.error(), e.errorString());\n";
                s << "            mut" << cap << "().insert(key, value);\n";
            } else if (f.label == Label::Repeated) {
                if (scalarOf(f.typeName) || f.enumType) {
                    const ScalarInfo *sc = scalarOf(f.typeName);
                    s << "            reader.repeated(&mut" << cap
                      << "(), QtProtobufPrivate::Scalar::" << (sc ? sc->kind : "Int32") << ");\n";
                } else {
                    s << "            " << c.element << " item{};\n            "
                      << readValue(f.typeName, f, "item", "reader")
                      << "\n            if (!reader.failed())\n                mut" << cap
                      << "().append(std::move(item));\n";
                }
            } else if (f.message) {
                s << "            reader.message(&mut" << cap << "());\n";
            } else {
                s << "            " << c.storage << " value{};\n            "
                  << readValue(f.typeName, f, "value", "reader")
                  << "\n            if (!reader.failed())\n                set" << cap << "(std::move(value));\n";
            }
            s << "            break;\n        }\n";
        }
        s << "        default:\n            reader.unknown(this);\n            break;\n        }\n";
        s << "        if (reader.failed())\n            return false;\n    }\n    return !reader.failed();\n}\n\n";

        // JSON.
        s << "void " << cls << "::ntToJson(QJsonObject &object) const {\n";
        if (m->fields.empty())
            s << "    (void)object;\n";
        else
            s << "    const auto *d = dptr.constData();\n";
        for (const Field *fp : ordered) {
            const Field &f = *fp;
            const std::string name = camel(f.name);
            const std::string value =
                "d->" + name + (f.message && f.label != Label::Repeated && !f.isMap ? ".get()" : "");
            const std::string json = f.jsonName.empty() ? jsonNameOf(f.name) : f.jsonName;
            if (f.isMap) {
                s << "    if (!" << value << ".isEmpty()) {\n        QJsonObject map;\n        for (auto it = " << value
                  << ".cbegin(); it != " << value << ".cend(); ++it)\n            map.insert("
                  << mapKeyToString(f.keyType, "it.key()") << ", " << toJson(f.valueType, f, "it.value()") << ");\n";
                s << "        object.insert(QLatin1String(\"" << json << "\"), map);\n    }\n";
            } else if (f.label == Label::Repeated) {
                s << "    if (!" << value
                  << ".isEmpty()) {\n        QJsonArray array;\n        for (const auto &item : " << value
                  << ")\n            array.append(" << toJson(f.typeName, f, "item") << ");\n";
                s << "        object.insert(QLatin1String(\"" << json << "\"), array);\n    }\n";
            } else {
                std::string condition;
                if (f.oneof >= 0) {
                    const Oneof &o = m->oneofs[size_t(f.oneof)];
                    condition = "d->" + camel(o.name) + "Case == " + cls + "::" + capital(o.name) +
                                "Fields::" + capital(f.name);
                } else if (hasPresence(m_file, f)) {
                    condition = "d->has_" + name;
                } else if (isString(f.typeName) || isBytes(f.typeName)) {
                    condition = "!" + value + ".isEmpty()";
                } else if (f.enumType) {
                    condition = "qint32(" + value + ") != 0";
                } else if (f.typeName == "float" || f.typeName == "double") {
                    condition = value + " != 0 || std::signbit(" + value + ")";
                } else {
                    condition = value + " != 0";
                }
                s << "    if (" << condition << ")\n        object.insert(QLatin1String(\"" << json << "\"), "
                  << toJson(f.typeName, f, value) << ");\n";
            }
        }
        s << "}\n\n";

        s << "bool " << cls << "::ntFromJson(const QJsonObject &object, QString *error) {\n";
        if (m->fields.empty())
            s << "    (void)object;\n    (void)error;\n";
        else
            s << "    QJsonValue json;\n";
        for (const Field &f : m->fields) {
            const FieldCpp c = cppOf(f);
            const std::string name = camel(f.name);
            const std::string cap = capital(f.name);
            const std::string jsonName = f.jsonName.empty() ? jsonNameOf(f.name) : f.jsonName;
            const std::string what = m->name + "." + f.name;
            s << "    if (QtProtobufPrivate::Json::find(object, \"" << jsonName << "\", \"" << f.name
              << "\", &json)) {\n";
            if (f.isMap) {
                const std::string keyCpp = elementType(f.keyType, f, false);
                s << "        if (!json.isObject()) {\n            *error = QStringLiteral(\"" << what
                  << " must be an object\");\n            return false;\n        }\n";
                s << "        const QJsonObject map = json.toObject();\n        for (auto it = map.begin(); it != "
                     "map.end(); ++it) {\n";
                s << "            " << keyCpp << " key{};\n            "
                  << mapKeyFromString(f.keyType, "it.key()", "key", what) << "\n";
                s << "            " << c.element << " value{};\n            "
                  << fromJson(f.valueType, f, "it.value()", "value", what) << "\n";
                s << "            mut" << cap << "().insert(key, value);\n        }\n";
            } else if (f.label == Label::Repeated) {
                s << "        if (!json.isArray()) {\n            *error = QStringLiteral(\"" << what
                  << " must be an array\");\n            return false;\n        }\n";
                s << "        const QJsonArray array = json.toArray();\n        for (const QJsonValue item : array) "
                     "{\n";
                s << "            " << c.element << " value{};\n            "
                  << fromJson(f.typeName, f, "item", "value", what) << "\n            mut" << cap
                  << "().append(std::move(value));\n        }\n";
            } else {
                s << "        " << c.storage << " value{};\n        " << fromJson(f.typeName, f, "json", "value", what)
                  << "\n        set" << cap << "(std::move(value));\n";
            }
            s << "    }\n";
        }
        s << "    return true;\n}\n\n";

        // Properties.
        s << "QVariant " << cls
          << "::ntProperty(QAnyStringView name) const {\n    const QString n = name.toString();\n";
        for (const Field &f : m->fields) {
            if (f.label == Label::Repeated || f.isMap || f.message)
                continue;
            const std::string name = camel(f.name);
            std::string v = "dptr->" + name;
            if (f.enumType)
                v = "int(" + v + ")";
            else if (const ScalarInfo *sc = scalarOf(f.typeName)) {
                if (sc->jsonKind == 1)
                    v = "qlonglong(" + v + ")";
                else if (sc->jsonKind == 2)
                    v = "qulonglong(" + v + ")";
                else if (std::string(sc->kind) == "UInt32" || std::string(sc->kind) == "Fixed32")
                    v = "uint(" + v + ")";
                else if (sc->jsonKind == 0)
                    v = "int(" + v + ")";
            }
            s << "    if (n == QLatin1String(\"" << name << "\"))\n        return QVariant(" << v << ");\n";
        }
        s << "    return QVariant();\n}\n\n";
        s << "bool " << cls
          << "::ntSetProperty(QAnyStringView name, const QVariant &value) {\n"
             "    const QString n = name.toString();\n    bool ok = false;\n    (void)ok;\n    if (!value.isValid())\n "
             "       return false;\n";
        for (const Field &f : m->fields) {
            if (f.label == Label::Repeated || f.isMap || f.message)
                continue;
            const FieldCpp c = cppOf(f);
            const std::string name = camel(f.name);
            const std::string cap = capital(f.name);
            s << "    if (n == QLatin1String(\"" << name << "\")) {\n";
            if (isString(f.typeName))
                s << "        set" << cap << "(value.toString());\n        return true;\n";
            else if (isBytes(f.typeName))
                s << "        set" << cap << "(value.toByteArray());\n        return true;\n";
            else if (f.typeName == "bool")
                s << "        set" << cap << "(value.toBool());\n        return true;\n";
            else if (f.typeName == "float" || f.typeName == "double")
                s << "        const double v = value.toDouble(&ok);\n        if (!ok)\n            return false;\n     "
                     "   set"
                  << cap << "(" << c.element << "(v));\n        return true;\n";
            else if (f.enumType || (scalarOf(f.typeName) && scalarOf(f.typeName)->jsonKind != 2))
                s << "        const qlonglong v = value.toLongLong(&ok);\n        if (!ok)\n            return "
                     "false;\n        set"
                  << cap << "(" << c.element << "(v));\n        return true;\n";
            else
                s << "        const qulonglong v = value.toULongLong(&ok);\n        if (!ok)\n            return "
                     "false;\n        set"
                  << cap << "(" << c.element << "(v));\n        return true;\n";
            s << "    }\n";
        }
        s << "    return false;\n}\n\n";
    }

    File *m_file;
    std::string m_headerName;
};

} // namespace

int main(int argc, char **argv) {
    std::vector<fs::path> includes;
    std::vector<fs::path> inputs;
    fs::path outputDir = ".";
    fs::path root;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&](const std::string &option) {
            if (arg.size() > option.size())
                return arg.substr(option.size());
            if (i + 1 >= argc)
                die(option + " needs a value");
            return std::string(argv[++i]);
        };
        if (arg.rfind("-I", 0) == 0)
            includes.emplace_back(value("-I"));
        else if (arg == "--output-dir" || arg == "-o")
            outputDir = value(arg);
        else if (arg == "--root")
            root = value(arg);
        else if (arg == "--help" || arg == "-h") {
            std::printf("usage: ntprotobufgen [-I <dir>]... [--output-dir <dir>] [--root <dir>] <file.proto>...\n");
            return 0;
        } else if (arg == "--version") {
            std::printf("ntprotobufgen %s\n", NT_VERSION_STRING);
            return 0;
        } else if (!arg.empty() && arg[0] == '-')
            die("unknown option " + arg);
        else
            inputs.emplace_back(arg);
    }
    if (inputs.empty())
        die("no .proto file given");
    Loader loader(includes);
    for (const fs::path &input : inputs) {
        File *file = loader.load(input, root);
        loader.resolve(file);
        const fs::path base = outputDir / file->relative;
        fs::create_directories(base.parent_path());
        const std::string headerName = fs::path(file->relative).filename().string() + ".qpb.h";
        Generator generator(file, headerName);
        const auto write = [](const fs::path &path, const std::string &text) {
            // Unchanged files are left alone, so that what includes them is not rebuilt.
            std::ifstream old(path);
            if (old) {
                std::stringstream current;
                current << old.rdbuf();
                if (current.str() == text)
                    return;
            }
            std::ofstream out(path);
            if (!out)
                die("cannot write " + path.string());
            out << text;
        };
        write(base.string() + ".qpb.h", generator.header());
        write(base.string() + ".qpb.cpp", generator.source());
    }
    return 0;
}
