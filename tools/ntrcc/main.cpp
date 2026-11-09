// MIT License - see LICENSE
//
// ntrcc: the resource compiler of Nt. It reads Qt resource collection files (.qrc) and writes the
// files they list as one resource tree, either as C++ source that registers the tree when the
// program starts, or as a binary .rcc file for QResource::registerResource(). The tree format is
// described in src/qresource_p.h.
//
// The command line follows the subset of Qt's rcc that CMake's AUTORCC and qt_add_resources() use.
// Data is never compressed; the compression options are accepted and ignored.

#include <sys/stat.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

#ifndef NT_VERSION_STRING
#define NT_VERSION_STRING "unknown"
#endif
constexpr const char *kVersion = "rcc 6.0.0 (Nt ntrcc " NT_VERSION_STRING ")";

// Keep in step with src/qresource_p.h.
constexpr char kMagic[4] = {'N', 't', 'R', 'C'};
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint32_t kHeaderSize = 36;
constexpr std::uint32_t kNodeSize = 40;
constexpr std::uint32_t kDirectoryFlag = 1;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------------------------
// A small XML reader: elements, attributes, text, comments, CDATA, character and the predefined
// entity references. It is enough for .qrc files, which have no namespaces or DTDs.

struct Element {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::string text;
    std::vector<Element> children;
    int line = 0;
};

void appendUtf8(std::string &out, unsigned long cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

class XmlReader {
public:
    XmlReader(std::string text, std::string file) : m_s(std::move(text)), m_file(std::move(file)) {}

    Element document() {
        if (m_s.compare(0, 3, "\xEF\xBB\xBF") == 0)
            advance(3);
        skipMisc();
        if (!at("<"))
            fail("expected an element");
        Element root = element();
        skipMisc();
        if (m_i < m_s.size())
            fail("unexpected content after the document element");
        return root;
    }

private:
    [[noreturn]] void fail(const std::string &message) const {
        throw Error(m_file + ":" + std::to_string(m_line) + ": " + message);
    }

    bool at(const char *text) const { return m_s.compare(m_i, std::strlen(text), text) == 0; }

    void advance(std::size_t n) {
        for (std::size_t k = 0; k < n && m_i < m_s.size(); ++k, ++m_i)
            if (m_s[m_i] == '\n')
                ++m_line;
    }

    void skipTo(const char *end) {
        const std::size_t pos = m_s.find(end, m_i);
        if (pos == std::string::npos)
            fail(std::string("missing '") + end + "'");
        advance(pos + std::strlen(end) - m_i);
    }

    void skipSpace() {
        while (m_i < m_s.size() && std::strchr(" \t\r\n", m_s[m_i]) && m_s[m_i] != '\0')
            advance(1);
    }

    void skipMisc() {
        for (;;) {
            skipSpace();
            if (at("<?"))
                skipTo("?>");
            else if (at("<!--"))
                skipTo("-->");
            else if (at("<!DOCTYPE")) {
                const std::size_t gt = m_s.find('>', m_i), bracket = m_s.find('[', m_i);
                skipTo(bracket < gt ? "]>" : ">");
            } else
                return;
        }
    }

    static bool nameChar(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == ':' ||
               (static_cast<unsigned char>(c) & 0x80);
    }

    std::string name() {
        const std::size_t start = m_i;
        while (m_i < m_s.size() && nameChar(m_s[m_i]))
            advance(1);
        if (m_i == start)
            fail("expected a name");
        return m_s.substr(start, m_i - start);
    }

    void reference(std::string &out) {
        const std::size_t semi = m_s.find(';', m_i);
        if (semi == std::string::npos || semi - m_i > 12)
            fail("unterminated entity reference");
        const std::string ref = m_s.substr(m_i + 1, semi - m_i - 1);
        if (ref == "amp")
            out += '&';
        else if (ref == "lt")
            out += '<';
        else if (ref == "gt")
            out += '>';
        else if (ref == "quot")
            out += '"';
        else if (ref == "apos")
            out += '\'';
        else if (ref.size() > 1 && ref[0] == '#') {
            const bool hex = ref[1] == 'x' || ref[1] == 'X';
            const std::string digits = ref.substr(hex ? 2 : 1);
            char *end = nullptr;
            const unsigned long cp = std::strtoul(digits.c_str(), &end, hex ? 16 : 10);
            if (digits.empty() || *end != '\0' || cp == 0 || cp > 0x10FFFF)
                fail("invalid character reference '&" + ref + ";'");
            appendUtf8(out, cp);
        } else {
            fail("unknown entity '&" + ref + ";'");
        }
        advance(semi + 1 - m_i);
    }

    Element element() {
        Element e;
        e.line = m_line;
        advance(1); // '<'
        e.name = name();
        for (;;) {
            skipSpace();
            if (at("/>")) {
                advance(2);
                return e;
            }
            if (at(">")) {
                advance(1);
                break;
            }
            const std::string key = name();
            skipSpace();
            if (!at("="))
                fail("expected '=' after attribute '" + key + "'");
            advance(1);
            skipSpace();
            if (!at("\"") && !at("'"))
                fail("expected a quoted value for attribute '" + key + "'");
            const char quote = m_s[m_i];
            advance(1);
            std::string value;
            while (m_i < m_s.size() && m_s[m_i] != quote) {
                if (m_s[m_i] == '&')
                    reference(value);
                else if (m_s[m_i] == '<')
                    fail("'<' in an attribute value");
                else {
                    value += m_s[m_i];
                    advance(1);
                }
            }
            if (m_i >= m_s.size())
                fail("unterminated attribute value");
            advance(1);
            e.attributes[key] = value;
        }
        for (;;) {
            if (m_i >= m_s.size())
                fail("element '" + e.name + "' is not closed");
            if (at("</")) {
                advance(2);
                const std::string end = name();
                if (end != e.name)
                    fail("expected </" + e.name + ">, found </" + end + ">");
                skipSpace();
                if (!at(">"))
                    fail("expected '>'");
                advance(1);
                return e;
            }
            if (at("<!--")) {
                skipTo("-->");
            } else if (at("<![CDATA[")) {
                advance(9);
                const std::size_t end = m_s.find("]]>", m_i);
                if (end == std::string::npos)
                    fail("unterminated CDATA section");
                e.text += m_s.substr(m_i, end - m_i);
                advance(end + 3 - m_i);
            } else if (at("<?")) {
                skipTo("?>");
            } else if (at("<")) {
                e.children.push_back(element());
            } else if (m_s[m_i] == '&') {
                reference(e.text);
            } else {
                e.text += m_s[m_i];
                advance(1);
            }
        }
    }

    std::string m_s;
    std::string m_file;
    std::size_t m_i = 0;
    int m_line = 1;
};

// ---------------------------------------------------------------------------------------------
// The resources described by the .qrc files.

struct File {
    std::string locale;
    fs::path source;
    std::int64_t lastModified = 0;
};

struct Directory {
    std::map<std::string, Directory> dirs;
    std::map<std::string, std::map<std::string, File>> files; // name -> locale -> file
};

struct Options {
    std::vector<std::string> inputs;
    std::string output;
    std::string name;
    std::string root;
    std::string depfile;
    bool binary = false;
    bool list = false;
    bool listMapping = false;
};

std::string trim(const std::string &s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return std::string();
    const std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Splits a resource path into components, resolving "." and ".." lexically from the root.
std::vector<std::string> components(const std::string &path) {
    std::vector<std::string> out;
    std::string part;
    std::istringstream in(path);
    while (std::getline(in, part, '/')) {
        if (part.empty() || part == ".")
            continue;
        if (part == "..") {
            if (!out.empty())
                out.pop_back();
            continue;
        }
        out.push_back(part);
    }
    return out;
}

std::string joinPath(const std::vector<std::string> &parts) {
    std::string s;
    for (const std::string &p : parts)
        s += '/' + p;
    return s.empty() ? std::string("/") : s;
}

// "fr-FR" and "fr_FR" both name the French of France; an empty or "C" language means every locale.
std::string normalizeLocale(std::string lang, const std::string &country) {
    std::replace(lang.begin(), lang.end(), '-', '_');
    if (lang == "C")
        lang.clear();
    if (!lang.empty() && !country.empty() && lang.find('_') == std::string::npos)
        lang += '_' + country;
    return lang;
}

std::int64_t modificationTime(const fs::path &path) {
    struct stat st {};
    if (::stat(path.string().c_str(), &st) != 0)
        return 0;
    std::int64_t ms = static_cast<std::int64_t>(st.st_mtime) * 1000;
    // Reproducible builds clamp the time stamps to SOURCE_DATE_EPOCH.
    if (const char *epoch = std::getenv("SOURCE_DATE_EPOCH")) {
        char *end = nullptr;
        const long long seconds = std::strtoll(epoch, &end, 10);
        if (end != epoch && *end == '\0' && seconds >= 0)
            ms = std::min<std::int64_t>(ms, static_cast<std::int64_t>(seconds) * 1000);
    }
    return ms;
}

class Collection {
public:
    void read(const std::string &qrcFile, const std::string &rootOption) {
        std::ifstream in(qrcFile, std::ios::binary);
        if (!in)
            throw Error("cannot open '" + qrcFile + "'");
        std::stringstream buffer;
        buffer << in.rdbuf();
        const Element rcc = XmlReader(buffer.str(), qrcFile).document();
        if (rcc.name != "RCC")
            throw Error(qrcFile + ":" + std::to_string(rcc.line) + ": expected <RCC>, found <" + rcc.name + ">");
        const fs::path base = fs::absolute(fs::path(qrcFile)).parent_path();
        for (const Element &resource : rcc.children) {
            if (resource.name != "qresource")
                throw Error(where(qrcFile, resource) + "unexpected <" + resource.name + "> in <RCC>");
            const auto attr = [&](const char *key) {
                const auto it = resource.attributes.find(key);
                return it == resource.attributes.end() ? std::string() : it->second;
            };
            const std::string prefix = attr("prefix");
            const std::string locale = normalizeLocale(attr("lang"), attr("country"));
            for (const Element &file : resource.children) {
                if (file.name != "file")
                    throw Error(where(qrcFile, file) + "unexpected <" + file.name + "> in <qresource>");
                const std::string text = trim(file.text);
                if (text.empty())
                    throw Error(where(qrcFile, file) + "<file> names no file");
                const auto alias = file.attributes.find("alias");
                const std::string name = alias == file.attributes.end() ? text : alias->second;
                const fs::path source = (base / fs::path(text)).lexically_normal();
                const std::vector<std::string> target = components(rootOption + '/' + prefix + '/' + name);
                if (fs::is_directory(source)) {
                    addDirectory(source, target, locale);
                } else if (fs::is_regular_file(source)) {
                    add(target, locale, source);
                } else {
                    throw Error(where(qrcFile, file) + "cannot find file '" + text + "'");
                }
            }
        }
    }

    const Directory &root() const { return m_root; }
    const std::vector<std::pair<std::string, fs::path>> &mapping() const { return m_mapping; }

private:
    static std::string where(const std::string &file, const Element &e) {
        return file + ":" + std::to_string(e.line) + ": ";
    }

    void addDirectory(const fs::path &dir, const std::vector<std::string> &target, const std::string &locale) {
        std::vector<fs::path> entries;
        for (const fs::directory_entry &e : fs::directory_iterator(dir)) {
            const std::string n = e.path().filename().string();
            if (!n.empty() && n[0] != '.')
                entries.push_back(e.path());
        }
        std::sort(entries.begin(), entries.end());
        for (const fs::path &p : entries) {
            std::vector<std::string> child = target;
            child.push_back(p.filename().string());
            if (fs::is_directory(p))
                addDirectory(p, child, locale);
            else if (fs::is_regular_file(p))
                add(child, locale, p);
        }
    }

    void add(const std::vector<std::string> &target, const std::string &locale, const fs::path &source) {
        if (target.empty())
            throw Error("'" + source.string() + "' has an empty resource name");
        Directory *dir = &m_root;
        for (std::size_t i = 0; i + 1 < target.size(); ++i)
            dir = &dir->dirs[target[i]];
        std::map<std::string, File> &variants = dir->files[target.back()];
        if (variants.count(locale)) {
            std::cerr << "ntrcc: warning: duplicate resource " << joinPath(target) << ", '" << source.string()
                      << "' is ignored\n";
            return;
        }
        File f;
        f.locale = locale;
        f.source = source;
        f.lastModified = modificationTime(source);
        variants[locale] = f;
        m_mapping.emplace_back(joinPath(target), source);
    }

    Directory m_root;
    std::vector<std::pair<std::string, fs::path>> m_mapping;
};

// ---------------------------------------------------------------------------------------------
// Writing the tree.

void put32(std::vector<unsigned char> &out, std::size_t at, std::uint64_t v) {
    if (v > 0xFFFFFFFFu)
        throw Error("the resources are larger than 4 GiB");
    for (int k = 0; k < 4; ++k)
        out[at + static_cast<std::size_t>(k)] = static_cast<unsigned char>(v >> (24 - 8 * k));
}

void put64(std::vector<unsigned char> &out, std::size_t at, std::int64_t value) {
    const auto v = static_cast<std::uint64_t>(value);
    put32(out, at, v >> 32);
    put32(out, at + 4, v & 0xFFFFFFFFu);
}

std::vector<unsigned char> readFile(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw Error("cannot read '" + path.string() + "'");
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<unsigned char> serialize(const Directory &root) {
    struct Node {
        std::string name;
        std::string locale;
        const Directory *dir = nullptr;
        const File *file = nullptr;
        std::uint64_t first = 0;
        std::uint64_t count = 0;
    };
    std::vector<Node> nodes(1);
    nodes[0].dir = &root;
    std::string strings;
    std::map<std::string, std::uint64_t> stringOffsets;
    const auto intern = [&](const std::string &s) {
        const auto it = stringOffsets.find(s);
        if (it != stringOffsets.end())
            return it->second;
        const std::uint64_t at = strings.size();
        strings += s;
        stringOffsets.emplace(s, at);
        return at;
    };
    std::vector<unsigned char> data;

    // Breadth first, so that the children of a directory are consecutive and follow it.
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].dir)
            continue;
        const Directory &dir = *nodes[i].dir;
        std::vector<Node> children;
        for (const auto &d : dir.dirs) {
            Node n;
            n.name = d.first;
            n.dir = &d.second;
            children.push_back(n);
        }
        for (const auto &f : dir.files) {
            for (const auto &variant : f.second) {
                Node n;
                n.name = f.first;
                n.locale = variant.first;
                n.file = &variant.second;
                children.push_back(n);
            }
        }
        std::stable_sort(children.begin(), children.end(), [](const Node &a, const Node &b) {
            return a.name != b.name ? a.name < b.name : a.locale < b.locale;
        });
        nodes[i].first = nodes.size();
        nodes[i].count = children.size();
        nodes.insert(nodes.end(), children.begin(), children.end());
    }

    std::vector<std::pair<std::uint64_t, std::uint64_t>> names(nodes.size()), locales(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        names[i] = {intern(nodes[i].name), nodes[i].name.size()};
        locales[i] = {intern(nodes[i].locale), nodes[i].locale.size()};
        if (nodes[i].file) {
            const std::vector<unsigned char> bytes = readFile(nodes[i].file->source);
            nodes[i].first = data.size();
            nodes[i].count = bytes.size();
            data.insert(data.end(), bytes.begin(), bytes.end());
        }
    }

    const std::uint64_t nodeTable = kHeaderSize;
    const std::uint64_t stringTable = nodeTable + nodes.size() * kNodeSize;
    const std::uint64_t dataOffset = stringTable + strings.size();
    const std::uint64_t total = dataOffset + data.size();
    if (total > 0xFFFFFFFFu)
        throw Error("the resources are larger than 4 GiB");
    std::vector<unsigned char> out(static_cast<std::size_t>(dataOffset));
    std::memcpy(out.data(), kMagic, sizeof kMagic);
    put32(out, 4, kFormatVersion);
    put32(out, 8, total);
    put32(out, 12, nodes.size());
    put32(out, 16, nodeTable);
    put32(out, 20, stringTable);
    put32(out, 24, strings.size());
    put32(out, 28, dataOffset);
    put32(out, 32, data.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const std::size_t at = static_cast<std::size_t>(nodeTable) + i * kNodeSize;
        put32(out, at, names[i].first);
        put32(out, at + 4, names[i].second);
        put32(out, at + 8, nodes[i].dir ? kDirectoryFlag : 0);
        put32(out, at + 12, locales[i].first);
        put32(out, at + 16, locales[i].second);
        put32(out, at + 20, nodes[i].first);
        put32(out, at + 24, nodes[i].count);
        put32(out, at + 28, 0);
        put64(out, at + 32, nodes[i].file ? nodes[i].file->lastModified : 0);
    }
    std::memcpy(out.data() + stringTable, strings.data(), strings.size());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

std::string identifier(const std::string &name) {
    std::string out;
    for (const char c : name)
        out += std::isalnum(static_cast<unsigned char>(c)) || c == '_' ? c : '_';
    if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0])))
        out.insert(out.begin(), '_');
    return out;
}

std::string cppSource(const std::vector<unsigned char> &tree, const std::string &name, const Options &opts) {
    std::ostringstream o;
    o << "// Resource tree compiled by ntrcc " NT_VERSION_STRING " from";
    for (const std::string &in : opts.inputs)
        o << ' ' << fs::path(in).filename().string();
    o << ".\n// Do not edit: changes are lost when the file is generated again.\n\n";
    o << "#include <QtCore/qresource.h>\n\n";
    o << "namespace {\n\nconst unsigned char ntrcc_resource_tree[] = {";
    static const char hex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < tree.size(); ++i) {
        o << (i % 16 == 0 ? "\n    " : " ") << "0x" << hex[tree[i] >> 4] << hex[tree[i] & 15] << ',';
    }
    o << "\n};\n\n} // namespace\n\n";
    o << "int qInitResources_" << name << "();\n";
    o << "int qInitResources_" << name << "() {\n"
      << "    QResource::registerResource(ntrcc_resource_tree);\n    return 1;\n}\n\n";
    o << "int qCleanupResources_" << name << "();\n";
    o << "int qCleanupResources_" << name << "() {\n"
      << "    QResource::unregisterResource(ntrcc_resource_tree);\n    return 1;\n}\n\n";
    o << "namespace {\n\n"
      << "// Registers the resources while the program starts and unregisters them when it ends.\n"
      << "struct ntrcc_initializer {\n"
      << "    ntrcc_initializer() { qInitResources_" << name << "(); }\n"
      << "    ~ntrcc_initializer() { qCleanupResources_" << name << "(); }\n"
      << "    ntrcc_initializer(const ntrcc_initializer &) = delete;\n"
      << "    ntrcc_initializer &operator=(const ntrcc_initializer &) = delete;\n"
      << "} ntrcc_initializer_instance;\n\n"
      << "} // namespace\n";
    return o.str();
}

void writeOutput(const std::string &path, const std::string &bytes) {
    if (path.empty() || path == "-") {
        std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        std::cout.flush();
        if (!std::cout)
            throw Error("cannot write the output");
        return;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out)
        throw Error("cannot write '" + path + "'");
}

std::string makeEscape(const std::string &path) {
    std::string out;
    for (const char c : path) {
        if (c == ' ' || c == '#' || c == '\\')
            out += '\\';
        else if (c == '$')
            out += '$';
        out += c;
    }
    return out;
}

void usage(std::ostream &out) {
    out << "Usage: ntrcc [options] <inputs.qrc>...\n"
           "Compiles Qt resource collection files into C++ source or a binary .rcc file.\n\n"
           "Options:\n"
           "  -o, --output <file>     Write to <file> instead of standard output.\n"
           "  --name <name>           Name of the initialization functions, used by Q_INIT_RESOURCE().\n"
           "  --root <path>           Prefix every resource path with <path>.\n"
           "  --binary                Write a binary .rcc file for QResource::registerResource().\n"
           "  --list                  List the files the inputs refer to.\n"
           "  --list-mapping          List each resource path and the file it comes from.\n"
           "  -d, --depfile <file>    Also write a Makefile dependency file.\n"
           "  --no-compress, --compress <level>, --threshold <n>, --compress-algo <algo>,\n"
           "  --no-zstd, --format-version <n>, --no-namespace\n"
           "                          Accepted for compatibility with rcc; data is never compressed.\n"
           "  -v, --version           Show the version.\n"
           "  -h, --help              Show this help.\n";
}

Options parseArguments(int argc, char **argv) {
    Options opts;
    const std::set<std::string> ignoredFlags = {"no-compress", "no-zstd", "no-namespace", "verbose"};
    const std::set<std::string> ignoredValues = {"compress", "threshold", "compress-algo", "format-version",
                                                 "namespace", "pass"};
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.size() > 1 && a[0] == '-' && a != "-") {
            // rcc accepts single and double dashes for its long options, and "--opt=value".
            std::string key = a.substr(a[1] == '-' ? 2 : 1);
            std::string value;
            bool hasValue = false;
            const std::size_t eq = key.find('=');
            if (eq != std::string::npos) {
                value = key.substr(eq + 1);
                key.erase(eq);
                hasValue = true;
            }
            const auto take = [&]() {
                if (hasValue)
                    return value;
                if (i + 1 >= argc)
                    throw Error("option '" + a + "' needs a value");
                return std::string(argv[++i]);
            };
            if (key == "o" || key == "output")
                opts.output = take();
            else if (key == "name")
                opts.name = take();
            else if (key == "root")
                opts.root = take();
            else if (key == "d" || key == "depfile")
                opts.depfile = take();
            else if (key == "binary")
                opts.binary = true;
            else if (key == "list")
                opts.list = true;
            else if (key == "list-mapping")
                opts.listMapping = true;
            else if (key == "v" || key == "version") {
                std::cout << kVersion << '\n';
                std::exit(0);
            } else if (key == "h" || key == "help") {
                usage(std::cout);
                std::exit(0);
            } else if (ignoredFlags.count(key)) {
            } else if (ignoredValues.count(key)) {
                take();
            } else {
                throw Error("unknown option '" + a + "'");
            }
        } else {
            opts.inputs.push_back(a);
        }
    }
    if (opts.inputs.empty())
        throw Error("no input files");
    if (!opts.root.empty() && opts.root[0] != '/')
        throw Error("--root must start with '/'");
    return opts;
}

} // namespace

int main(int argc, char **argv) {
    try {
        const Options opts = parseArguments(argc, argv);
        Collection collection;
        for (const std::string &input : opts.inputs)
            collection.read(input, opts.root);

        if (opts.list || opts.listMapping) {
            std::string out;
            for (const auto &m : collection.mapping()) {
                if (opts.listMapping)
                    out += m.first + '\t';
                out += fs::absolute(m.second).lexically_normal().string() + '\n';
            }
            writeOutput(opts.output, out);
            return 0;
        }

        const std::vector<unsigned char> tree = serialize(collection.root());
        if (opts.binary) {
            writeOutput(opts.output, std::string(tree.begin(), tree.end()));
        } else {
            const std::string name =
                identifier(opts.name.empty() ? fs::path(opts.inputs.front()).stem().string() : opts.name);
            writeOutput(opts.output, cppSource(tree, name, opts));
        }

        if (!opts.depfile.empty()) {
            if (opts.output.empty() || opts.output == "-")
                throw Error("--depfile needs --output");
            std::string dep = makeEscape(opts.output) + ':';
            for (const std::string &input : opts.inputs)
                dep += ' ' + makeEscape(fs::absolute(input).lexically_normal().string());
            for (const auto &m : collection.mapping())
                dep += ' ' + makeEscape(fs::absolute(m.second).lexically_normal().string());
            writeOutput(opts.depfile, dep + '\n');
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "ntrcc: " << e.what() << '\n';
        return 1;
    }
}
