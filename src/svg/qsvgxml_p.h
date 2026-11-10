// MIT License - see LICENSE
//
// A small XML reader for Nt SVG: elements, attributes and character data as a tree. It checks that the
// document is well formed as far as it reads it, and knows nothing of namespaces beyond their prefixes.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace QtSvgPrivate {

struct XmlElement {
    // The name without the prefix "svg:". Character data is a child with the name "#text".
    std::string name;
    std::string text;
    // The names lose the prefix "xlink:", so that both spellings of href are found.
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<std::unique_ptr<XmlElement>> children;

    const std::string *attribute(const char *key) const;
};

// Parses a document and returns its root element, or null if it is not well formed.
std::unique_ptr<XmlElement> parseXml(const char *data, std::size_t size);

} // namespace QtSvgPrivate
