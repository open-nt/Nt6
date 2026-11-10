// MIT License - see LICENSE
#include "qsvgxml_p.h"

#include <cstdlib>
#include <cstring>

namespace QtSvgPrivate {

const std::string *XmlElement::attribute(const char *key) const {
    for (const auto &a : attributes) {
        if (a.first == key)
            return &a.second;
    }
    return nullptr;
}

namespace {

// Deeper documents are refused, so that parsing and the passes over the tree cannot exhaust the stack.
const int kMaxDepth = 256;

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

bool isNameChar(char c) {
    const auto u = static_cast<unsigned char>(c);
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '_' || u == '-' ||
           u == '.' || u == ':' || u >= 0x80;
}

void appendUtf8(std::string &out, unsigned long code) {
    if (code == 0 || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
        code = 0xfffd;
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xc0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xe0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (code & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (code & 0x3f));
    }
}

class Parser {
public:
    Parser(const char *data, std::size_t size) : m_p(data), m_end(data + size) {}

    std::unique_ptr<XmlElement> document() {
        if (m_end - m_p >= 3 && std::memcmp(m_p, "\xef\xbb\xbf", 3) == 0)
            m_p += 3;
        if (!skipMisc() || m_p >= m_end || *m_p != '<')
            return nullptr;
        auto root = std::make_unique<XmlElement>();
        if (!element(*root, 0))
            return nullptr;
        if (!skipMisc() || m_p != m_end)
            return nullptr;
        return root;
    }

private:
    bool starts(const char *token) const {
        const std::size_t n = std::strlen(token);
        return static_cast<std::size_t>(m_end - m_p) >= n && std::memcmp(m_p, token, n) == 0;
    }

    bool skipUntil(const char *token) {
        const std::size_t n = std::strlen(token);
        for (; static_cast<std::size_t>(m_end - m_p) >= n; ++m_p) {
            if (std::memcmp(m_p, token, n) == 0) {
                m_p += n;
                return true;
            }
        }
        return false;
    }

    void skipSpace() {
        while (m_p < m_end && isSpace(*m_p))
            ++m_p;
    }

    // Skips a document type declaration, with its internal subset.
    bool skipDoctype() {
        int depth = 0;
        char quote = 0;
        for (; m_p < m_end; ++m_p) {
            const char c = *m_p;
            if (quote != 0) {
                if (c == quote)
                    quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == '[') {
                ++depth;
            } else if (c == ']') {
                --depth;
            } else if (c == '>' && depth <= 0) {
                ++m_p;
                return true;
            }
        }
        return false;
    }

    // Skips white space, comments, processing instructions and the document type between elements.
    bool skipMisc() {
        for (;;) {
            skipSpace();
            if (starts("<!--")) {
                if (!skipUntil("-->"))
                    return false;
            } else if (starts("<?")) {
                if (!skipUntil("?>"))
                    return false;
            } else if (starts("<!DOCTYPE")) {
                if (!skipDoctype())
                    return false;
            } else {
                return true;
            }
        }
    }

    bool name(std::string &out) {
        const char *start = m_p;
        while (m_p < m_end && isNameChar(*m_p))
            ++m_p;
        out.assign(start, m_p);
        return !out.empty();
    }

    // Appends the text from `from` to `to`, with the character and predefined entity references replaced.
    static void unescape(const char *from, const char *to, std::string &out) {
        while (from < to) {
            const char *amp = static_cast<const char *>(std::memchr(from, '&', static_cast<std::size_t>(to - from)));
            if (!amp) {
                out.append(from, to);
                return;
            }
            out.append(from, amp);
            const char *semi = static_cast<const char *>(std::memchr(amp, ';', static_cast<std::size_t>(to - amp)));
            if (!semi || semi - amp > 12) {
                out += '&';
                from = amp + 1;
                continue;
            }
            const std::string ref(amp + 1, semi);
            if (ref == "lt") {
                out += '<';
            } else if (ref == "gt") {
                out += '>';
            } else if (ref == "amp") {
                out += '&';
            } else if (ref == "quot") {
                out += '"';
            } else if (ref == "apos") {
                out += '\'';
            } else if (ref.size() > 1 && ref[0] == '#') {
                const bool hex = ref[1] == 'x';
                char *stop = nullptr;
                const unsigned long code = std::strtoul(ref.c_str() + (hex ? 2 : 1), &stop, hex ? 16 : 10);
                if (*stop != '\0' || stop == ref.c_str() + (hex ? 2 : 1))
                    out.append(amp, semi + 1);
                else
                    appendUtf8(out, code);
            } else {
                // An entity of the document type, which is not read: kept as written.
                out.append(amp, semi + 1);
            }
            from = semi + 1;
        }
    }

    bool attributes(XmlElement &e) {
        for (;;) {
            const char *before = m_p;
            skipSpace();
            if (m_p >= m_end)
                return false;
            if (*m_p == '>' || *m_p == '/')
                return true;
            if (m_p == before)
                return false; // attributes are separated by white space
            std::string key;
            if (!name(key))
                return false;
            skipSpace();
            if (m_p >= m_end || *m_p != '=')
                return false;
            ++m_p;
            skipSpace();
            if (m_p >= m_end || (*m_p != '"' && *m_p != '\''))
                return false;
            const char quote = *m_p++;
            const char *start = m_p;
            while (m_p < m_end && *m_p != quote) {
                if (*m_p == '<')
                    return false;
                ++m_p;
            }
            if (m_p >= m_end)
                return false;
            std::string value;
            unescape(start, m_p, value);
            ++m_p;
            if (key.compare(0, 6, "xlink:") == 0)
                key.erase(0, 6);
            if (e.attribute(key.c_str()))
                return false; // an attribute may appear once
            e.attributes.emplace_back(std::move(key), std::move(value));
        }
    }

    void addText(XmlElement &parent, std::string text) {
        if (text.empty())
            return;
        if (!parent.children.empty() && parent.children.back()->name == "#text") {
            parent.children.back()->text += text;
            return;
        }
        auto node = std::make_unique<XmlElement>();
        node->name = "#text";
        node->text = std::move(text);
        parent.children.push_back(std::move(node));
    }

    // Parses an element; the parser is at its "<".
    bool element(XmlElement &e, int depth) {
        if (depth > kMaxDepth)
            return false;
        ++m_p;
        std::string tag;
        if (!name(tag))
            return false;
        e.name = tag.compare(0, 4, "svg:") == 0 ? tag.substr(4) : tag;
        if (!attributes(e))
            return false;
        if (*m_p == '/') {
            ++m_p;
            if (m_p >= m_end || *m_p != '>')
                return false;
            ++m_p;
            return true;
        }
        ++m_p; // '>'
        for (;;) {
            const char *start = m_p;
            while (m_p < m_end && *m_p != '<')
                ++m_p;
            if (m_p >= m_end)
                return false;
            if (m_p > start) {
                std::string text;
                unescape(start, m_p, text);
                addText(e, std::move(text));
            }
            if (starts("<!--")) {
                if (!skipUntil("-->"))
                    return false;
            } else if (starts("<![CDATA[")) {
                m_p += 9;
                const char *data = m_p;
                if (!skipUntil("]]>"))
                    return false;
                addText(e, std::string(data, m_p - 3));
            } else if (starts("<?")) {
                if (!skipUntil("?>"))
                    return false;
            } else if (starts("</")) {
                m_p += 2;
                std::string closing;
                if (!name(closing) || closing != tag)
                    return false;
                skipSpace();
                if (m_p >= m_end || *m_p != '>')
                    return false;
                ++m_p;
                return true;
            } else {
                auto child = std::make_unique<XmlElement>();
                if (!element(*child, depth + 1))
                    return false;
                e.children.push_back(std::move(child));
            }
        }
    }

    const char *m_p;
    const char *m_end;
};

} // namespace

std::unique_ptr<XmlElement> parseXml(const char *data, std::size_t size) { return Parser(data, size).document(); }

} // namespace QtSvgPrivate
