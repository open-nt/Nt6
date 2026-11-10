// MIT License - see LICENSE
#include "qsvgdocument_p.h"

#include "qsvgxml_p.h"

#include <QtCore/qstring.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace QtSvgPrivate {

namespace {

// Elements nested deeper than this are not built, and references are not followed further.
const int kMaxNesting = 64;

struct NamedColor {
    const char *name;
    unsigned rgb;
};

// The color keywords of SVG 1.1, in alphabetical order.
const NamedColor kColors[] = {
    {"aliceblue", 0xf0f8ff},
    {"antiquewhite", 0xfaebd7},
    {"aqua", 0x00ffff},
    {"aquamarine", 0x7fffd4},
    {"azure", 0xf0ffff},
    {"beige", 0xf5f5dc},
    {"bisque", 0xffe4c4},
    {"black", 0x000000},
    {"blanchedalmond", 0xffebcd},
    {"blue", 0x0000ff},
    {"blueviolet", 0x8a2be2},
    {"brown", 0xa52a2a},
    {"burlywood", 0xdeb887},
    {"cadetblue", 0x5f9ea0},
    {"chartreuse", 0x7fff00},
    {"chocolate", 0xd2691e},
    {"coral", 0xff7f50},
    {"cornflowerblue", 0x6495ed},
    {"cornsilk", 0xfff8dc},
    {"crimson", 0xdc143c},
    {"cyan", 0x00ffff},
    {"darkblue", 0x00008b},
    {"darkcyan", 0x008b8b},
    {"darkgoldenrod", 0xb8860b},
    {"darkgray", 0xa9a9a9},
    {"darkgreen", 0x006400},
    {"darkgrey", 0xa9a9a9},
    {"darkkhaki", 0xbdb76b},
    {"darkmagenta", 0x8b008b},
    {"darkolivegreen", 0x556b2f},
    {"darkorange", 0xff8c00},
    {"darkorchid", 0x9932cc},
    {"darkred", 0x8b0000},
    {"darksalmon", 0xe9967a},
    {"darkseagreen", 0x8fbc8f},
    {"darkslateblue", 0x483d8b},
    {"darkslategray", 0x2f4f4f},
    {"darkslategrey", 0x2f4f4f},
    {"darkturquoise", 0x00ced1},
    {"darkviolet", 0x9400d3},
    {"deeppink", 0xff1493},
    {"deepskyblue", 0x00bfff},
    {"dimgray", 0x696969},
    {"dimgrey", 0x696969},
    {"dodgerblue", 0x1e90ff},
    {"firebrick", 0xb22222},
    {"floralwhite", 0xfffaf0},
    {"forestgreen", 0x228b22},
    {"fuchsia", 0xff00ff},
    {"gainsboro", 0xdcdcdc},
    {"ghostwhite", 0xf8f8ff},
    {"gold", 0xffd700},
    {"goldenrod", 0xdaa520},
    {"gray", 0x808080},
    {"green", 0x008000},
    {"greenyellow", 0xadff2f},
    {"grey", 0x808080},
    {"honeydew", 0xf0fff0},
    {"hotpink", 0xff69b4},
    {"indianred", 0xcd5c5c},
    {"indigo", 0x4b0082},
    {"ivory", 0xfffff0},
    {"khaki", 0xf0e68c},
    {"lavender", 0xe6e6fa},
    {"lavenderblush", 0xfff0f5},
    {"lawngreen", 0x7cfc00},
    {"lemonchiffon", 0xfffacd},
    {"lightblue", 0xadd8e6},
    {"lightcoral", 0xf08080},
    {"lightcyan", 0xe0ffff},
    {"lightgoldenrodyellow", 0xfafad2},
    {"lightgray", 0xd3d3d3},
    {"lightgreen", 0x90ee90},
    {"lightgrey", 0xd3d3d3},
    {"lightpink", 0xffb6c1},
    {"lightsalmon", 0xffa07a},
    {"lightseagreen", 0x20b2aa},
    {"lightskyblue", 0x87cefa},
    {"lightslategray", 0x778899},
    {"lightslategrey", 0x778899},
    {"lightsteelblue", 0xb0c4de},
    {"lightyellow", 0xffffe0},
    {"lime", 0x00ff00},
    {"limegreen", 0x32cd32},
    {"linen", 0xfaf0e6},
    {"magenta", 0xff00ff},
    {"maroon", 0x800000},
    {"mediumaquamarine", 0x66cdaa},
    {"mediumblue", 0x0000cd},
    {"mediumorchid", 0xba55d3},
    {"mediumpurple", 0x9370db},
    {"mediumseagreen", 0x3cb371},
    {"mediumslateblue", 0x7b68ee},
    {"mediumspringgreen", 0x00fa9a},
    {"mediumturquoise", 0x48d1cc},
    {"mediumvioletred", 0xc71585},
    {"midnightblue", 0x191970},
    {"mintcream", 0xf5fffa},
    {"mistyrose", 0xffe4e1},
    {"moccasin", 0xffe4b5},
    {"navajowhite", 0xffdead},
    {"navy", 0x000080},
    {"oldlace", 0xfdf5e6},
    {"olive", 0x808000},
    {"olivedrab", 0x6b8e23},
    {"orange", 0xffa500},
    {"orangered", 0xff4500},
    {"orchid", 0xda70d6},
    {"palegoldenrod", 0xeee8aa},
    {"palegreen", 0x98fb98},
    {"paleturquoise", 0xafeeee},
    {"palevioletred", 0xdb7093},
    {"papayawhip", 0xffefd5},
    {"peachpuff", 0xffdab9},
    {"peru", 0xcd853f},
    {"pink", 0xffc0cb},
    {"plum", 0xdda0dd},
    {"powderblue", 0xb0e0e6},
    {"purple", 0x800080},
    {"red", 0xff0000},
    {"rosybrown", 0xbc8f8f},
    {"royalblue", 0x4169e1},
    {"saddlebrown", 0x8b4513},
    {"salmon", 0xfa8072},
    {"sandybrown", 0xf4a460},
    {"seagreen", 0x2e8b57},
    {"seashell", 0xfff5ee},
    {"sienna", 0xa0522d},
    {"silver", 0xc0c0c0},
    {"skyblue", 0x87ceeb},
    {"slateblue", 0x6a5acd},
    {"slategray", 0x708090},
    {"slategrey", 0x708090},
    {"snow", 0xfffafa},
    {"springgreen", 0x00ff7f},
    {"steelblue", 0x4682b4},
    {"tan", 0xd2b48c},
    {"teal", 0x008080},
    {"thistle", 0xd8bfd8},
    {"tomato", 0xff6347},
    {"turquoise", 0x40e0d0},
    {"violet", 0xee82ee},
    {"wheat", 0xf5deb3},
    {"white", 0xffffff},
    {"whitesmoke", 0xf5f5f5},
    {"yellow", 0xffff00},
    {"yellowgreen", 0x9acd32},
};

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

std::string trimmed(const std::string &s) {
    std::size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a]))
        ++a;
    while (b > a && isSpace(s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

std::string lowered(std::string s) {
    for (char &c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

double clamp01(double v) { return v > 0.0 ? (v < 1.0 ? v : 1.0) : 0.0; }

// Reads a number at `*pos` and moves past it.
bool readNumber(const std::string &s, std::size_t *pos, double *out) {
    const char *start = s.data() + *pos;
    const char *after = scanNumber(start, s.data() + s.size(), out);
    if (!after)
        return false;
    *pos += static_cast<std::size_t>(after - start);
    return true;
}

std::vector<double> numberList(const std::string &s) {
    std::vector<double> out;
    std::size_t pos = 0;
    while (pos < s.size()) {
        if (isSpace(s[pos]) || s[pos] == ',') {
            ++pos;
            continue;
        }
        double v = 0.0;
        if (!readNumber(s, &pos, &v))
            break;
        out.push_back(v);
    }
    return out;
}

// Parses a length to pixels. A percentage is taken of `reference`, and reported through `percent`.
bool parseLength(const std::string &text, double reference, double *out, bool *percent = nullptr) {
    const std::string s = trimmed(text);
    std::size_t pos = 0;
    double v = 0.0;
    if (!readNumber(s, &pos, &v))
        return false;
    const std::string unit = lowered(trimmed(s.substr(pos)));
    if (percent)
        *percent = unit == "%";
    if (unit.empty() || unit == "px")
        *out = v;
    else if (unit == "%")
        *out = v * reference / 100.0;
    else if (unit == "pt")
        *out = v * 96.0 / 72.0;
    else if (unit == "mm")
        *out = v * 96.0 / 25.4;
    else if (unit == "cm")
        *out = v * 96.0 / 2.54;
    else if (unit == "in")
        *out = v * 96.0;
    else if (unit == "em" || unit == "pc")
        *out = v * 16.0;
    else if (unit == "ex")
        *out = v * 8.0;
    else
        return false;
    return true;
}

double lengthOr(const XmlElement &e, const char *name, double reference, double fallback) {
    const std::string *a = e.attribute(name);
    double v = 0.0;
    return a && parseLength(*a, reference, &v) ? v : fallback;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

bool parseColor(const std::string &text, Rgba *out) {
    const std::string s = lowered(trimmed(text));
    if (s.empty())
        return false;
    if (s[0] == '#') {
        int digits[8];
        const std::size_t n = s.size() - 1;
        if (n != 3 && n != 4 && n != 6 && n != 8)
            return false;
        for (std::size_t i = 0; i < n; ++i) {
            digits[i] = hexDigit(s[i + 1]);
            if (digits[i] < 0)
                return false;
        }
        double channel[4] = {0.0, 0.0, 0.0, 1.0};
        for (std::size_t i = 0; i < (n <= 4 ? n : n / 2); ++i)
            channel[i] = n <= 4 ? digits[i] * 17 / 255.0 : (digits[2 * i] * 16 + digits[2 * i + 1]) / 255.0;
        *out = {channel[0], channel[1], channel[2], channel[3]};
        return true;
    }
    if (s.compare(0, 3, "rgb") == 0) {
        const std::size_t open = s.find('('), close = s.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close < open)
            return false;
        const std::string body = s.substr(open + 1, close - open - 1);
        double channel[4] = {0.0, 0.0, 0.0, 1.0};
        std::size_t pos = 0;
        int count = 0;
        while (pos < body.size() && count < 4) {
            if (isSpace(body[pos]) || body[pos] == ',' || body[pos] == '/') {
                ++pos;
                continue;
            }
            double v = 0.0;
            if (!readNumber(body, &pos, &v))
                return false;
            const bool percent = pos < body.size() && body[pos] == '%';
            if (percent)
                ++pos;
            if (count < 3)
                channel[count] = clamp01(percent ? v / 100.0 : v / 255.0);
            else
                channel[count] = clamp01(percent ? v / 100.0 : v);
            ++count;
        }
        if (count < 3)
            return false;
        *out = {channel[0], channel[1], channel[2], channel[3]};
        return true;
    }
    if (s == "transparent") {
        *out = {0.0, 0.0, 0.0, 0.0};
        return true;
    }
    const auto *end = std::end(kColors);
    const auto *it = std::lower_bound(std::begin(kColors), end, s,
                                      [](const NamedColor &c, const std::string &key) { return key > c.name; });
    if (it == end || s != it->name)
        return false;
    *out = {((it->rgb >> 16u) & 0xffu) / 255.0, ((it->rgb >> 8u) & 0xffu) / 255.0, (it->rgb & 0xffu) / 255.0, 1.0};
    return true;
}

// The id in "url(#id)", or an empty string. `rest` gets what follows the closing parenthesis.
std::string parseUrl(const std::string &text, std::string *rest = nullptr) {
    const std::string s = trimmed(text);
    if (s.compare(0, 4, "url(") != 0)
        return {};
    const std::size_t close = s.find(')');
    if (close == std::string::npos)
        return {};
    std::string id = trimmed(s.substr(4, close - 4));
    if (id.size() >= 2 && (id.front() == '"' || id.front() == '\'') && id.back() == id.front())
        id = id.substr(1, id.size() - 2);
    if (rest)
        *rest = trimmed(s.substr(close + 1));
    return !id.empty() && id[0] == '#' ? id.substr(1) : std::string();
}

bool parsePaint(const std::string &text, PaintSpec *out) {
    const std::string s = trimmed(text);
    const std::string low = lowered(s);
    PaintSpec p;
    if (low == "none") {
        p.kind = PaintSpec::None;
    } else if (low == "currentcolor") {
        p.kind = PaintSpec::CurrentColor;
    } else if (low.compare(0, 4, "url(") == 0) {
        std::string rest;
        p.url = parseUrl(s, &rest);
        p.kind = PaintSpec::Url;
        if (lowered(rest) == "currentcolor")
            p.fallback = PaintSpec::CurrentColor;
        else if (parseColor(rest, &p.color))
            p.fallback = PaintSpec::Color;
    } else if (parseColor(s, &p.color)) {
        p.kind = PaintSpec::Color;
    } else {
        return false;
    }
    *out = p;
    return true;
}

Mat parseTransform(const std::string &s) {
    const double pi = 3.14159265358979323846;
    Mat result;
    std::size_t pos = 0;
    while (pos < s.size()) {
        if (isSpace(s[pos]) || s[pos] == ',') {
            ++pos;
            continue;
        }
        const std::size_t open = s.find('(', pos);
        const std::size_t close = open == std::string::npos ? open : s.find(')', open);
        if (close == std::string::npos)
            break;
        const std::string name = trimmed(s.substr(pos, open - pos));
        const std::vector<double> v = numberList(s.substr(open + 1, close - open - 1));
        pos = close + 1;
        Mat m;
        if (name == "matrix" && v.size() == 6) {
            m = {v[0], v[1], v[2], v[3], v[4], v[5]};
        } else if (name == "translate" && (v.size() == 1 || v.size() == 2)) {
            m = Mat::translate(v[0], v.size() == 2 ? v[1] : 0.0);
        } else if (name == "scale" && (v.size() == 1 || v.size() == 2)) {
            m = Mat::scaling(v[0], v.size() == 2 ? v[1] : v[0]);
        } else if (name == "rotate" && (v.size() == 1 || v.size() == 3)) {
            const double a = v[0] * pi / 180.0;
            m = {std::cos(a), std::sin(a), -std::sin(a), std::cos(a), 0.0, 0.0};
            if (v.size() == 3)
                m = Mat::translate(v[1], v[2]) * m * Mat::translate(-v[1], -v[2]);
        } else if (name == "skewX" && v.size() == 1) {
            m.c = std::tan(v[0] * pi / 180.0);
        } else if (name == "skewY" && v.size() == 1) {
            m.b = std::tan(v[0] * pi / 180.0);
        } else {
            break; // an error ends the list; what came before it stands
        }
        result = result * m;
    }
    return result;
}

using Declarations = std::vector<std::pair<std::string, std::string>>;

// Splits "name: value; name: value" into pairs.
void parseDeclarations(const std::string &text, Declarations *out) {
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find(';', pos);
        if (end == std::string::npos)
            end = text.size();
        const std::string item = text.substr(pos, end - pos);
        pos = end + 1;
        const std::size_t colon = item.find(':');
        if (colon == std::string::npos)
            continue;
        std::string value = trimmed(item.substr(colon + 1));
        const std::size_t important = value.find('!');
        if (important != std::string::npos)
            value = trimmed(value.substr(0, important));
        out->emplace_back(lowered(trimmed(item.substr(0, colon))), value);
    }
}

struct Rule {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    int specificity = 0;
    Declarations declarations;
};

} // namespace

// The rules of the style elements of a document. Selectors are an element name, an id and classes, in any
// combination; rules with other selectors are skipped.
struct Document::Sheet {
    std::vector<Rule> rules;

    void add(const std::string &cssIn) {
        std::string css;
        for (std::size_t i = 0; i < cssIn.size(); ++i) {
            if (cssIn.compare(i, 2, "/*") == 0) {
                const std::size_t end = cssIn.find("*/", i + 2);
                if (end == std::string::npos)
                    break;
                i = end + 1;
            } else {
                css += cssIn[i];
            }
        }
        std::size_t pos = 0;
        while (pos < css.size()) {
            const std::size_t open = css.find('{', pos);
            if (open == std::string::npos)
                break;
            // A block may hold blocks (@media); find the brace that closes it.
            std::size_t close = open + 1;
            for (int depth = 1; close < css.size(); ++close) {
                if (css[close] == '{')
                    ++depth;
                else if (css[close] == '}' && --depth == 0)
                    break;
            }
            if (close >= css.size())
                break;
            const std::string selectors = css.substr(pos, open - pos);
            Declarations declarations;
            if (selectors.find('@') == std::string::npos)
                parseDeclarations(css.substr(open + 1, close - open - 1), &declarations);
            pos = close + 1;
            if (declarations.empty())
                continue;
            std::size_t from = 0;
            while (from <= selectors.size()) {
                std::size_t comma = selectors.find(',', from);
                if (comma == std::string::npos)
                    comma = selectors.size();
                Rule rule;
                if (parseSelector(trimmed(selectors.substr(from, comma - from)), &rule)) {
                    rule.declarations = declarations;
                    rules.push_back(std::move(rule));
                }
                from = comma + 1;
            }
        }
        std::stable_sort(rules.begin(), rules.end(),
                         [](const Rule &a, const Rule &b) { return a.specificity < b.specificity; });
    }

    static bool parseSelector(const std::string &s, Rule *rule) {
        if (s.empty())
            return false;
        std::size_t pos = 0;
        char kind = 't';
        while (pos < s.size()) {
            std::size_t end = pos;
            while (end < s.size() && s[end] != '.' && s[end] != '#') {
                const char c = s[end];
                if (isSpace(c) || c == '>' || c == '+' || c == '~' || c == '[' || c == ':' || c == '(')
                    return false;
                ++end;
            }
            const std::string part = s.substr(pos, end - pos);
            if (kind == 't') {
                if (part != "*" && !part.empty()) {
                    rule->tag = part;
                    rule->specificity += 1;
                }
            } else if (part.empty()) {
                return false;
            } else if (kind == '#') {
                rule->id = part;
                rule->specificity += 100;
            } else {
                rule->classes.push_back(part);
                rule->specificity += 10;
            }
            if (end < s.size())
                kind = s[end];
            pos = end + 1;
        }
        return true;
    }

    // The declarations that apply to an element, least important first: presentation attributes, the rules
    // of the style sheet, and the style attribute.
    Declarations properties(const XmlElement &e) const {
        Declarations out;
        for (const auto &a : e.attributes) {
            if (a.first != "style" && a.first != "class" && a.first != "id")
                out.emplace_back(a.first, a.second);
        }
        if (!rules.empty()) {
            const std::string *id = e.attribute("id");
            std::vector<std::string> classes;
            if (const std::string *list = e.attribute("class")) {
                std::size_t pos = 0;
                while (pos < list->size()) {
                    std::size_t end = pos;
                    while (end < list->size() && !isSpace((*list)[end]))
                        ++end;
                    if (end > pos)
                        classes.push_back(list->substr(pos, end - pos));
                    pos = end + 1;
                }
            }
            for (const Rule &rule : rules) {
                if (!rule.tag.empty() && rule.tag != e.name)
                    continue;
                if (!rule.id.empty() && (!id || *id != rule.id))
                    continue;
                bool match = true;
                for (const std::string &c : rule.classes)
                    match = match && std::find(classes.begin(), classes.end(), c) != classes.end();
                if (match)
                    out.insert(out.end(), rule.declarations.begin(), rule.declarations.end());
            }
        }
        if (const std::string *style = e.attribute("style"))
            parseDeclarations(*style, &out);
        return out;
    }
};

namespace {

void collectStyles(const XmlElement &e, Document::Sheet *sheet, int depth);

const std::string *findProperty(const Declarations &all, const char *name) {
    const std::string *found = nullptr;
    for (const auto &d : all) {
        if (d.first == name)
            found = &d.second;
    }
    return found;
}

// Sets what the declarations say in `delta`. Unknown properties and values are ignored.
void applyDeclarations(const Declarations &all, double viewportDiagonal, StyleDelta *delta) {
    for (const auto &d : all) {
        const std::string &name = d.first;
        const std::string value = trimmed(d.second);
        const std::string low = lowered(value);
        if (low == "inherit" || value.empty())
            continue;
        Style &v = delta->value;
        double number = 0.0;
        if (name == "fill") {
            if (parsePaint(value, &v.fill))
                delta->set |= StyleDelta::Fill;
        } else if (name == "stroke") {
            if (parsePaint(value, &v.stroke))
                delta->set |= StyleDelta::Stroke;
        } else if (name == "fill-opacity" || name == "stroke-opacity" || name == "opacity") {
            bool percent = false;
            if (!parseLength(value, 1.0, &number, &percent))
                continue;
            number = clamp01(number);
            if (name == "opacity") {
                delta->opacity = number;
            } else if (name == "fill-opacity") {
                v.fillOpacity = number;
                delta->set |= StyleDelta::FillOpacity;
            } else {
                v.strokeOpacity = number;
                delta->set |= StyleDelta::StrokeOpacity;
            }
        } else if (name == "fill-rule" || name == "clip-rule") {
            if (low != "nonzero" && low != "evenodd")
                continue;
            const FillRule rule = low == "evenodd" ? FillRule::EvenOdd : FillRule::NonZero;
            if (name == "fill-rule") {
                v.fillRule = rule;
                delta->set |= StyleDelta::FillRuleBit;
            } else {
                v.clipRule = rule;
                delta->set |= StyleDelta::ClipRule;
            }
        } else if (name == "stroke-width") {
            if (parseLength(value, viewportDiagonal, &number) && number >= 0.0) {
                v.pen.width = number;
                delta->set |= StyleDelta::StrokeWidth;
            }
        } else if (name == "stroke-linecap") {
            if (low == "butt" || low == "round" || low == "square") {
                v.pen.cap = low == "butt" ? LineCap::Butt : low == "round" ? LineCap::Round : LineCap::Square;
                delta->set |= StyleDelta::Cap;
            }
        } else if (name == "stroke-linejoin") {
            if (low == "miter" || low == "round" || low == "bevel") {
                v.pen.join = low == "miter" ? LineJoin::Miter : low == "round" ? LineJoin::Round : LineJoin::Bevel;
                delta->set |= StyleDelta::Join;
            }
        } else if (name == "stroke-miterlimit") {
            std::size_t pos = 0;
            if (readNumber(value, &pos, &number) && number >= 1.0) {
                v.pen.miterLimit = number;
                delta->set |= StyleDelta::MiterLimit;
            }
        } else if (name == "stroke-dasharray") {
            v.pen.dashes.clear();
            if (low != "none") {
                std::size_t pos = 0;
                while (pos < value.size()) {
                    if (isSpace(value[pos]) || value[pos] == ',') {
                        ++pos;
                        continue;
                    }
                    std::size_t end = pos;
                    while (end < value.size() && !isSpace(value[end]) && value[end] != ',')
                        ++end;
                    if (!parseLength(value.substr(pos, end - pos), viewportDiagonal, &number)) {
                        v.pen.dashes.clear();
                        break;
                    }
                    v.pen.dashes.push_back(number);
                    pos = end;
                }
            }
            delta->set |= StyleDelta::Dashes;
        } else if (name == "stroke-dashoffset") {
            if (parseLength(value, viewportDiagonal, &number)) {
                v.pen.dashOffset = number;
                delta->set |= StyleDelta::DashOffset;
            }
        } else if (name == "color") {
            if (parseColor(value, &v.color))
                delta->set |= StyleDelta::ColorBit;
        } else if (name == "display") {
            delta->display = low != "none";
        } else if (name == "visibility") {
            v.visible = low == "visible";
            delta->set |= StyleDelta::Visibility;
        } else if (name == "clip-path") {
            delta->clipPath = parseUrl(value);
        } else if (name == "font-family") {
            v.fontFamily = value;
            delta->set |= StyleDelta::FontFamily;
        } else if (name == "font-size") {
            if (parseLength(value, 16.0, &number) && number >= 0.0) {
                v.fontSize = number;
                delta->set |= StyleDelta::FontSize;
            }
        } else if (name == "font-weight") {
            std::size_t pos = 0;
            if (low == "bold" || low == "bolder")
                v.bold = true;
            else if (readNumber(value, &pos, &number))
                v.bold = number >= 600.0;
            else
                v.bold = false;
            delta->set |= StyleDelta::FontWeight;
        } else if (name == "font-style") {
            v.italic = low == "italic" || low == "oblique";
            delta->set |= StyleDelta::FontStyle;
        } else if (name == "text-anchor") {
            v.anchor = low == "middle" ? 1 : low == "end" ? 2 : 0;
            delta->set |= StyleDelta::Anchor;
        }
    }
}

void collectStyles(const XmlElement &e, Document::Sheet *sheet, int depth) {
    if (depth > kMaxNesting)
        return;
    if (e.name == "style") {
        const std::string *type = e.attribute("type");
        if (type && !type->empty() && *type != "text/css")
            return;
        for (const auto &child : e.children) {
            if (child->name == "#text")
                sheet->add(child->text);
        }
        return;
    }
    for (const auto &child : e.children)
        collectStyles(*child, sheet, depth + 1);
}

bool parseViewBox(const XmlElement &e, Box *out) {
    const std::string *a = e.attribute("viewBox");
    if (!a)
        return false;
    const std::vector<double> v = numberList(*a);
    if (v.size() != 4 || !(v[2] > 0.0) || !(v[3] > 0.0))
        return false;
    *out = {v[0], v[1], v[0] + v[2], v[1] + v[3], true};
    return true;
}

std::uint32_t premultiplied(const Rgba &c, double opacity) {
    const double a = clamp01(c.a * opacity);
    const auto channel = [a](double v) { return static_cast<std::uint32_t>(std::lround(clamp01(v) * a * 255.0)); };
    return (static_cast<std::uint32_t>(std::lround(a * 255.0)) << 24u) | (channel(c.r) << 16u) | (channel(c.g) << 8u) |
           channel(c.b);
}

QFont fontFor(const Style &style, double scale) {
    QFont font;
    // The first family of the list, without its quotes.
    std::string family = trimmed(style.fontFamily.substr(0, style.fontFamily.find(',')));
    if (family.size() >= 2 && (family.front() == '"' || family.front() == '\'') && family.back() == family.front())
        family = family.substr(1, family.size() - 2);
    if (!family.empty())
        font.setFamily(QString::fromUtf8(family.c_str(), static_cast<qsizetype>(family.size())));
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(std::min(4000.0, style.fontSize * scale)))));
    font.setBold(style.bold);
    font.setItalic(style.italic);
    return font;
}

} // namespace

void StyleDelta::applyTo(Style &s) const {
    if (set & Fill)
        s.fill = value.fill;
    if (set & Stroke)
        s.stroke = value.stroke;
    if (set & FillOpacity)
        s.fillOpacity = value.fillOpacity;
    if (set & StrokeOpacity)
        s.strokeOpacity = value.strokeOpacity;
    if (set & FillRuleBit)
        s.fillRule = value.fillRule;
    if (set & ClipRule)
        s.clipRule = value.clipRule;
    if (set & StrokeWidth)
        s.pen.width = value.pen.width;
    if (set & Cap)
        s.pen.cap = value.pen.cap;
    if (set & Join)
        s.pen.join = value.pen.join;
    if (set & MiterLimit)
        s.pen.miterLimit = value.pen.miterLimit;
    if (set & Dashes)
        s.pen.dashes = value.pen.dashes;
    if (set & DashOffset)
        s.pen.dashOffset = value.pen.dashOffset;
    if (set & ColorBit)
        s.color = value.color;
    if (set & Visibility)
        s.visible = value.visible;
    if (set & FontFamily)
        s.fontFamily = value.fontFamily;
    if (set & FontSize)
        s.fontSize = value.fontSize;
    if (set & FontWeight)
        s.bold = value.bold;
    if (set & FontStyle)
        s.italic = value.italic;
    if (set & Anchor)
        s.anchor = value.anchor;
}

struct Document::Context {
    Surface *surface = nullptr; // null when only the bounds are wanted
    Box *bounds = nullptr;
    long long budget = 0; // elements left to draw, so that references to references cannot take forever
    int depth = 0;        // of references and layers
    bool clip = false;    // drawing a clip path: shapes are filled solid, nothing is stroked
};

Document::Document() = default;
Document::~Document() = default;

bool Document::parse(const QByteArray &data) {
    m_root.reset();
    m_ids.clear();
    m_gradients.clear();
    m_width = m_height = 0.0;
    m_viewBox = Box();
    const std::unique_ptr<XmlElement> xml = parseXml(data.constData(), static_cast<std::size_t>(data.size()));
    if (!xml || xml->name != "svg")
        return false;

    Sheet sheet;
    collectStyles(*xml, &sheet, 0);

    const bool hasViewBox = parseViewBox(*xml, &m_viewBox);
    bool widthPercent = false, heightPercent = false;
    double width = 0.0, height = 0.0;
    const std::string *wa = xml->attribute("width");
    const std::string *ha = xml->attribute("height");
    const bool hasWidth = wa && parseLength(*wa, 100.0, &width, &widthPercent) && !widthPercent && width > 0.0;
    const bool hasHeight = ha && parseLength(*ha, 100.0, &height, &heightPercent) && !heightPercent && height > 0.0;
    m_viewportWidth = hasViewBox ? m_viewBox.width() : hasWidth ? width : 100.0;
    m_viewportHeight = hasViewBox ? m_viewBox.height() : hasHeight ? height : 100.0;

    m_root = std::make_unique<Node>();
    if (const std::string *id = xml->attribute("id")) {
        m_root->id = *id;
        m_ids.emplace(*id, m_root.get());
    }
    const double diagonal = std::sqrt((m_viewportWidth * m_viewportWidth + m_viewportHeight * m_viewportHeight) / 2.0);
    applyDeclarations(sheet.properties(*xml), diagonal, &m_root->style);
    m_root->style.clipPath.clear();
    for (const auto &child : xml->children)
        build(*child, m_root.get(), sheet, 1);
    resolveGradients();

    if (!hasViewBox) {
        if (hasWidth && hasHeight) {
            m_viewBox = {0.0, 0.0, width, height, true};
        } else {
            m_viewBox = bounds(m_root.get());
            if (!m_viewBox.valid || !(m_viewBox.width() > 0.0) || !(m_viewBox.height() > 0.0))
                m_viewBox = {0.0, 0.0, hasWidth ? width : 100.0, hasHeight ? height : 100.0, true};
        }
    }
    // A missing size comes from the view box, keeping its proportions if the other one is given.
    const double aspect = m_viewBox.width() / m_viewBox.height();
    m_width = hasWidth ? width : hasHeight ? height * aspect : m_viewBox.width();
    m_height = hasHeight ? height : hasWidth ? width / aspect : m_viewBox.height();
    return true;
}

void Document::build(const XmlElement &e, Node *parent, const Sheet &sheet, int depth) {
    if (depth > kMaxNesting)
        return;
    const std::string &name = e.name;
    if (name == "linearGradient" || name == "radialGradient") {
        addGradient(e, sheet);
        return;
    }
    Node::Type type = Node::Group;
    bool container = true;
    bool rendered = true;
    if (name == "g" || name == "a" || name == "svg" || name == "switch") {
        // drawn with its children
    } else if (name == "defs" || name == "symbol" || name == "clipPath") {
        rendered = false;
    } else if (name == "use") {
        type = Node::Use;
        container = false;
    } else if (name == "text") {
        type = Node::Text;
        container = false;
    } else if (name == "path" || name == "rect" || name == "circle" || name == "ellipse" || name == "line" ||
               name == "polyline" || name == "polygon") {
        type = Node::Shape;
        container = false;
    } else {
        return; // not known, or not drawn: metadata, title, style, mask, pattern, filter, image
    }

    auto owned = std::make_unique<Node>();
    Node *node = owned.get();
    node->type = type;
    node->parent = parent;
    node->rendered = rendered;
    if (const std::string *id = e.attribute("id")) {
        node->id = *id;
        m_ids.emplace(*id, node);
    }
    if (const std::string *t = e.attribute("transform"))
        node->transform = parseTransform(*t);
    const double vw = m_viewportWidth, vh = m_viewportHeight;
    const double diagonal = std::sqrt((vw * vw + vh * vh) / 2.0);
    applyDeclarations(sheet.properties(e), diagonal, &node->style);

    if (name == "svg") {
        // A nested document: its view box is fitted into its viewport, centered.
        const double x = lengthOr(e, "x", vw, 0.0), y = lengthOr(e, "y", vh, 0.0);
        Mat m = Mat::translate(x, y);
        Box box;
        const double w = lengthOr(e, "width", vw, 0.0), h = lengthOr(e, "height", vh, 0.0);
        if (parseViewBox(e, &box) && w > 0.0 && h > 0.0) {
            const double s = std::min(w / box.width(), h / box.height());
            m = m * Mat::translate((w - box.width() * s) / 2.0, (h - box.height() * s) / 2.0) * Mat::scaling(s, s) *
                Mat::translate(-box.x0, -box.y0);
        }
        node->transform = node->transform * m;
    } else if (name == "clipPath") {
        const std::string *units = e.attribute("clipPathUnits");
        node->clipBoxUnits = units && *units == "objectBoundingBox";
    } else if (name == "use") {
        if (const std::string *href = e.attribute("href")) {
            const std::string target = trimmed(*href);
            if (!target.empty() && target[0] == '#')
                node->href = target.substr(1);
        }
        node->transform = node->transform * Mat::translate(lengthOr(e, "x", vw, 0.0), lengthOr(e, "y", vh, 0.0));
    } else if (name == "path") {
        if (const std::string *d = e.attribute("d"))
            node->path = Path::fromSvg(d->data(), d->size());
    } else if (name == "rect") {
        const double w = lengthOr(e, "width", vw, 0.0), h = lengthOr(e, "height", vh, 0.0);
        double rx = lengthOr(e, "rx", vw, -1.0), ry = lengthOr(e, "ry", vh, -1.0);
        if (rx < 0.0)
            rx = std::max(ry, 0.0);
        if (ry < 0.0)
            ry = rx;
        if (w > 0.0 && h > 0.0)
            node->path.addRect(lengthOr(e, "x", vw, 0.0), lengthOr(e, "y", vh, 0.0), w, h, rx, ry);
    } else if (name == "circle") {
        const double r = lengthOr(e, "r", diagonal, 0.0);
        if (r > 0.0)
            node->path.addEllipse(lengthOr(e, "cx", vw, 0.0), lengthOr(e, "cy", vh, 0.0), r, r);
    } else if (name == "ellipse") {
        const double rx = lengthOr(e, "rx", vw, 0.0), ry = lengthOr(e, "ry", vh, 0.0);
        if (rx > 0.0 && ry > 0.0)
            node->path.addEllipse(lengthOr(e, "cx", vw, 0.0), lengthOr(e, "cy", vh, 0.0), rx, ry);
    } else if (name == "line") {
        node->path.moveTo({lengthOr(e, "x1", vw, 0.0), lengthOr(e, "y1", vh, 0.0)});
        node->path.lineTo({lengthOr(e, "x2", vw, 0.0), lengthOr(e, "y2", vh, 0.0)});
    } else if (name == "polyline" || name == "polygon") {
        const std::string *points = e.attribute("points");
        const std::vector<double> v = points ? numberList(*points) : std::vector<double>();
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            if (i == 0)
                node->path.moveTo({v[i], v[i + 1]});
            else
                node->path.lineTo({v[i], v[i + 1]});
        }
        if (name == "polygon" && v.size() >= 4)
            node->path.close();
    } else if (name == "text") {
        bool space = true;
        buildText(e, node, sheet, StyleDelta(), &space, depth);
        if (!node->runs.empty()) {
            // The position of the text is that of its first run, unless a tspan placed it.
            TextRun &first = node->runs.front();
            const auto firstNumber = [&](const char *attribute, double *out) {
                const std::string *a = e.attribute(attribute);
                const std::vector<double> v = a ? numberList(*a) : std::vector<double>();
                if (!v.empty())
                    *out = v[0];
            };
            if (!first.hasX)
                firstNumber("x", &first.x);
            if (!first.hasY)
                firstNumber("y", &first.y);
            first.hasX = first.hasY = true;
            while (!node->runs.empty() && !node->runs.back().text.empty() && node->runs.back().text.back() == ' ')
                node->runs.back().text.pop_back();
        }
    }

    if (container) {
        for (const auto &child : e.children)
            build(*child, node, sheet, depth + 1);
    }
    parent->children.push_back(std::move(owned));
}

void Document::buildText(const XmlElement &e, Node *node, const Sheet &sheet, const StyleDelta &inherited, bool *space,
                         int depth) {
    if (depth > kMaxNesting)
        return;
    bool first = true;
    for (const auto &child : e.children) {
        TextRun run;
        run.style = inherited;
        if (first && e.name == "tspan") {
            // The position that a tspan gives applies to the first text in it.
            const auto number = [&](const char *attribute, double *out) {
                const std::string *a = e.attribute(attribute);
                const std::vector<double> v = a ? numberList(*a) : std::vector<double>();
                if (v.empty())
                    return false;
                *out = v[0];
                return true;
            };
            run.hasX = number("x", &run.x);
            run.hasY = number("y", &run.y);
            number("dx", &run.dx);
            number("dy", &run.dy);
        }
        if (child->name == "#text") {
            // White space is collapsed, also across runs.
            for (const char c : child->text) {
                if (isSpace(c)) {
                    if (!*space)
                        run.text += ' ';
                    *space = true;
                } else {
                    run.text += c;
                    *space = false;
                }
            }
            if (!run.text.empty() || run.hasX || run.hasY) {
                if (run.hasX || run.hasY)
                    *space = run.text.empty() || run.text.back() == ' ';
                node->runs.push_back(std::move(run));
                first = false;
            }
        } else if (child->name == "tspan") {
            StyleDelta delta;
            applyDeclarations(sheet.properties(*child), 100.0, &delta);
            if (!delta.display)
                continue;
            StyleDelta merged = inherited;
            merged.set |= delta.set;
            delta.applyTo(merged.value);
            buildText(*child, node, sheet, merged, space, depth + 1);
        }
    }
}

void Document::addGradient(const XmlElement &e, const Sheet &sheet) {
    const std::string *id = e.attribute("id");
    if (!id || id->empty() || m_gradients.count(*id) != 0)
        return;
    Gradient g;
    g.radial = e.name == "radialGradient";
    if (const std::string *units = e.attribute("gradientUnits")) {
        g.boxUnits = *units != "userSpaceOnUse";
        g.unitsSet = true;
    }
    if (const std::string *t = e.attribute("gradientTransform")) {
        g.transform = parseTransform(*t);
        g.transformSet = true;
    }
    if (const std::string *spread = e.attribute("spreadMethod")) {
        g.spread = *spread == "reflect" ? Spread::Reflect : *spread == "repeat" ? Spread::Repeat : Spread::Pad;
        g.spreadSet = true;
    }
    if (const std::string *href = e.attribute("href")) {
        const std::string target = trimmed(*href);
        if (!target.empty() && target[0] == '#')
            g.href = target.substr(1);
    }
    const auto length = [&](const char *name, Length *out) {
        const std::string *a = e.attribute(name);
        if (a && parseLength(*a, 100.0, &out->value, &out->percent))
            out->set = true;
    };
    length("x1", &g.x1);
    length("y1", &g.y1);
    length("x2", &g.x2);
    length("y2", &g.y2);
    length("cx", &g.cx);
    length("cy", &g.cy);
    length("r", &g.r);
    length("fx", &g.fx);
    length("fy", &g.fy);
    double last = 0.0;
    for (const auto &child : e.children) {
        if (child->name != "stop")
            continue;
        const Declarations all = sheet.properties(*child);
        GradientStop stop;
        if (const std::string *offset = findProperty(all, "offset")) {
            bool percent = false;
            double v = 0.0;
            if (parseLength(*offset, 1.0, &v, &percent))
                stop.offset = v;
        }
        // Each offset is at least the one before it.
        stop.offset = std::max(last, clamp01(stop.offset));
        last = stop.offset;
        Rgba color;
        if (const std::string *c = findProperty(all, "stop-color"))
            parseColor(*c, &color);
        if (const std::string *o = findProperty(all, "stop-opacity")) {
            double v = 1.0;
            if (parseLength(*o, 1.0, &v))
                color.a *= clamp01(v);
        }
        stop.r = color.r;
        stop.g = color.g;
        stop.b = color.b;
        stop.a = color.a;
        g.stops.push_back(stop);
    }
    m_gradients.emplace(*id, std::move(g));
}

// A gradient takes the attributes and stops it does not set from the one it refers to.
void Document::resolveGradients() {
    for (auto &entry : m_gradients) {
        Gradient &g = entry.second;
        std::string href = g.href;
        for (int hops = 0; hops < 16 && !href.empty(); ++hops) {
            const auto it = m_gradients.find(href);
            if (it == m_gradients.end() || &it->second == &g)
                break;
            const Gradient &base = it->second;
            if (!g.unitsSet && base.unitsSet) {
                g.boxUnits = base.boxUnits;
                g.unitsSet = true;
            }
            if (!g.transformSet && base.transformSet) {
                g.transform = base.transform;
                g.transformSet = true;
            }
            if (!g.spreadSet && base.spreadSet) {
                g.spread = base.spread;
                g.spreadSet = true;
            }
            if (g.stops.empty())
                g.stops = base.stops;
            if (g.radial == base.radial) {
                for (Length Gradient::*field :
                     {&Gradient::x1, &Gradient::y1, &Gradient::x2, &Gradient::y2, &Gradient::cx, &Gradient::cy,
                      &Gradient::r, &Gradient::fx, &Gradient::fy}) {
                    if (!(g.*field).set)
                        g.*field = base.*field;
                }
            }
            href = base.href;
        }
    }
}

const Node *Document::find(const std::string &id) const {
    const auto it = m_ids.find(id);
    return it == m_ids.end() ? nullptr : it->second;
}

Style Document::inheritedStyle(const Node *node) const {
    std::vector<const Node *> chain;
    for (const Node *n = node ? node->parent : nullptr; n; n = n->parent)
        chain.push_back(n);
    Style style;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        (*it)->style.applyTo(style);
    return style;
}

Box Document::bounds(const Node *node) const {
    Box box;
    if (!node)
        return box;
    Context ctx;
    ctx.bounds = &box;
    ctx.budget = m_budget;
    renderNode(*node, Mat(), inheritedStyle(node), ctx, true);
    return box;
}

void Document::render(Surface *surface, const Mat &m, const Node *only) const {
    if (!m_root || !surface || surface->width <= 0 || surface->height <= 0)
        return;
    Context ctx;
    ctx.surface = surface;
    ctx.budget = m_budget;
    if (only)
        renderNode(*only, m, inheritedStyle(only), ctx, true);
    else
        renderNode(*m_root, m, Style(), ctx, false);
}

void Document::renderNode(const Node &node, const Mat &m, const Style &inherited, Context &ctx, bool referenced) const {
    if (--ctx.budget < 0)
        return;
    if ((!node.rendered && !referenced) || !node.style.display)
        return;
    Style style = inherited;
    node.style.applyTo(style);
    const Mat cm = m * node.transform;
    if (!ctx.surface || ctx.clip) {
        drawContent(node, cm, style, 1.0, ctx);
        return;
    }
    const double opacity = node.style.opacity;
    if (opacity <= 0.0)
        return;
    const Node *clip = node.style.clipPath.empty() ? nullptr : find(node.style.clipPath);
    // What is painted in one go can take the opacity itself; anything else is painted on a layer first, so
    // that its parts do not show through each other.
    const bool single =
        node.type == Node::Text ||
        (node.type == Node::Shape && (style.fill.kind == PaintSpec::None || style.stroke.kind == PaintSpec::None));
    if (!clip && (opacity >= 1.0 || single || ctx.depth >= kMaxNesting)) {
        drawContent(node, cm, style, opacity, ctx);
        return;
    }
    if (ctx.depth >= kMaxNesting)
        return;
    Surface layer(ctx.surface->width, ctx.surface->height);
    Context inner = ctx;
    inner.surface = &layer;
    ++inner.depth;
    drawContent(node, cm, style, 1.0, inner);
    ctx.budget = inner.budget;
    if (!clip) {
        ctx.surface->composite(layer, opacity, nullptr);
        return;
    }
    Surface mask(ctx.surface->width, ctx.surface->height);
    Context masking = ctx;
    masking.surface = &mask;
    masking.clip = true;
    ++masking.depth;
    Mat km = cm;
    if (clip->clipBoxUnits) {
        Box box;
        Context measuring;
        measuring.bounds = &box;
        measuring.budget = ctx.budget;
        drawContent(node, Mat(), style, 1.0, measuring);
        km = km * Mat{box.width(), 0.0, 0.0, box.height(), box.x0, box.y0};
    }
    drawClip(*clip, km, masking);
    ctx.budget = masking.budget;
    ctx.surface->composite(layer, opacity, &mask);
}

void Document::drawClip(const Node &clip, const Mat &m, Context &ctx) const {
    Style style = inheritedStyle(&clip);
    clip.style.applyTo(style);
    const Mat cm = m * clip.transform;
    for (const auto &child : clip.children)
        renderNode(*child, cm, style, ctx, true);
}

void Document::drawContent(const Node &node, const Mat &m, const Style &style, double opacity, Context &ctx) const {
    switch (node.type) {
    case Node::Group:
        for (const auto &child : node.children)
            renderNode(*child, m, style, ctx, false);
        break;
    case Node::Use: {
        const Node *target = node.href.empty() ? nullptr : find(node.href);
        if (!target || ctx.depth >= kMaxNesting)
            break;
        ++ctx.depth;
        renderNode(*target, m, style, ctx, true);
        --ctx.depth;
        break;
    }
    case Node::Shape:
        drawShape(node, m, style, opacity, ctx);
        break;
    case Node::Text:
        drawText(node, m, style, opacity, ctx);
        break;
    }
}

bool Document::shaderFor(const PaintSpec &paint, const Style &style, double opacity, const Box &box, const Mat &m,
                         Shader *out) const {
    PaintSpec::Kind kind = paint.kind;
    Rgba color = paint.color;
    if (kind == PaintSpec::Url) {
        const auto it = m_gradients.find(paint.url);
        if (it == m_gradients.end() || it->second.stops.empty()) {
            kind = it == m_gradients.end() ? paint.fallback : PaintSpec::None;
        } else if (it->second.stops.size() == 1) {
            const GradientStop &s = it->second.stops[0];
            color = {s.r, s.g, s.b, s.a};
            kind = PaintSpec::Color;
        } else {
            const Gradient &g = it->second;
            Mat gm = m;
            if (g.boxUnits) {
                if (!(box.width() > 0.0) || !(box.height() > 0.0))
                    return false;
                gm = gm * Mat{box.width(), 0.0, 0.0, box.height(), box.x0, box.y0};
            }
            gm = gm * g.transform;
            if (!gm.invert(&out->toGradient))
                return false;
            // In the units of the bounding box, lengths are fractions of it; in user space, percentages are
            // taken of the viewport.
            const auto resolve = [&](const Length &l, double fraction, double reference) {
                if (!l.set)
                    return g.boxUnits ? fraction : fraction * reference;
                if (l.percent)
                    return g.boxUnits ? l.value / 100.0 : l.value * reference / 100.0;
                return l.value;
            };
            const double vw = m_viewportWidth, vh = m_viewportHeight;
            out->gradient = true;
            out->radial = g.radial;
            out->spread = g.spread;
            if (!g.radial) {
                out->p1 = {resolve(g.x1, 0.0, vw), resolve(g.y1, 0.0, vh)};
                out->p2 = {resolve(g.x2, 1.0, vw), resolve(g.y2, 0.0, vh)};
            } else {
                out->p1 = {resolve(g.cx, 0.5, vw), resolve(g.cy, 0.5, vh)};
                out->radius = resolve(g.r, 0.5, std::sqrt((vw * vw + vh * vh) / 2.0));
                out->p2 = {g.fx.set ? resolve(g.fx, 0.5, vw) : out->p1.x,
                           g.fy.set ? resolve(g.fy, 0.5, vh) : out->p1.y};
                // A focal point outside the circle is moved onto it.
                const double dx = out->p2.x - out->p1.x, dy = out->p2.y - out->p1.y;
                const double distance = std::hypot(dx, dy);
                if (distance > out->radius * 0.99 && distance > 0.0) {
                    const double k = out->radius * 0.99 / distance;
                    out->p2 = {out->p1.x + dx * k, out->p1.y + dy * k};
                }
            }
            out->setStops(g.stops, opacity);
            return true;
        }
    }
    if (kind == PaintSpec::None)
        return false;
    if (kind == PaintSpec::CurrentColor)
        color = style.color;
    out->gradient = false;
    out->color = premultiplied(color, opacity);
    return true;
}

void Document::drawShape(const Node &node, const Mat &m, const Style &style, double opacity, Context &ctx) const {
    if (node.path.isEmpty() || (!style.visible && ctx.surface))
        return;
    // A quarter of a pixel is not seen; the tolerance is in user space.
    const double scale = m.scale();
    const double tolerance = 0.2 / std::max(scale, 1e-6);
    const std::vector<Path::Contour> contours = node.path.flatten(tolerance);
    Polygons polygons;
    const bool stroked = style.stroke.kind != PaintSpec::None && style.pen.width > 0.0 && !ctx.clip;
    if (!ctx.surface) {
        // Bounds: the geometry and, if there is one, the stroke.
        for (const Path::Contour &c : contours) {
            for (const Pt &p : c.points)
                ctx.bounds->add(m.map(p));
        }
        if (stroked) {
            strokeContours(contours, style.pen, m, tolerance, &polygons);
            for (const Pt &p : polygons.points)
                ctx.bounds->add(p);
        }
        return;
    }
    if (ctx.clip) {
        Shader white;
        white.color = 0xffffffffu;
        fillContours(contours, m, &polygons);
        ctx.surface->fill(polygons, style.clipRule, white);
        return;
    }
    Box box;
    for (const Path::Contour &c : contours) {
        for (const Pt &p : c.points)
            box.add(p);
    }
    Shader shader;
    if (shaderFor(style.fill, style, style.fillOpacity * opacity, box, m, &shader)) {
        fillContours(contours, m, &polygons);
        ctx.surface->fill(polygons, style.fillRule, shader);
    }
    if (stroked && shaderFor(style.stroke, style, style.strokeOpacity * opacity, box, m, &shader)) {
        polygons.clear();
        strokeContours(contours, style.pen, m, tolerance, &polygons);
        ctx.surface->fill(polygons, FillRule::NonZero, shader);
    }
}

// Text is drawn by QPainter, upright: at the position and size that the transformation gives it, but neither
// rotated nor sheared. Runs follow each other; a run with a position of its own starts a new chunk, which
// text-anchor places as a whole.
void Document::drawText(const Node &node, const Mat &m, const Style &style, double opacity, Context &ctx) const {
    if (ctx.clip || node.runs.empty() || (!style.visible && ctx.surface))
        return;
    const double scale = m.scale();
    struct Placed {
        QFont font;
        QString text;
        int advance = 0;
        Style style;
    };
    double x = 0.0, y = 0.0; // the pen, in device space
    std::size_t i = 0;
    while (i < node.runs.size()) {
        std::vector<Placed> chunk;
        int total = 0;
        const TextRun &head = node.runs[i];
        const Pt origin = m.map({head.hasX ? head.x : 0.0, head.hasY ? head.y : 0.0});
        if (head.hasX)
            x = origin.x;
        if (head.hasY)
            y = origin.y;
        std::size_t j = i;
        for (; j < node.runs.size(); ++j) {
            const TextRun &run = node.runs[j];
            if (j > i && (run.hasX || run.hasY))
                break;
            Placed p;
            p.style = style;
            run.style.applyTo(p.style);
            p.font = fontFor(p.style, scale);
            p.text = QString::fromUtf8(run.text.c_str(), static_cast<qsizetype>(run.text.size()));
            p.advance = QFontMetrics(p.font).horizontalAdvance(p.text) +
                        static_cast<int>(std::clamp(run.dx * scale, -1e6, 1e6));
            if (j == i)
                y += run.dy * scale;
            total += p.advance;
            chunk.push_back(std::move(p));
        }
        i = j;
        const int anchor = chunk.empty() ? 0 : chunk.front().style.anchor;
        if (anchor == 1)
            x -= total / 2.0;
        else if (anchor == 2)
            x -= total;
        for (const Placed &p : chunk) {
            const QFontMetrics metrics(p.font);
            // Text that is nowhere near the surface is not drawn; its position may not even fit an int.
            const bool near = std::fabs(x) < 1e6 && std::fabs(y) < 1e6;
            const int px = near ? static_cast<int>(std::floor(x + 0.5)) : 0;
            const int py = near ? static_cast<int>(std::floor(y + 0.5)) : 0;
            if (!ctx.surface) {
                ctx.bounds->add(Pt{x, y - metrics.ascent()});
                ctx.bounds->add(Pt{x + p.advance, y + metrics.descent()});
            } else if (near && p.style.visible && p.style.fill.kind != PaintSpec::None && !p.text.isEmpty()) {
                Rgba color = p.style.fill.kind == PaintSpec::CurrentColor ? p.style.color : p.style.fill.color;
                if (p.style.fill.kind == PaintSpec::Url) {
                    // Text has no gradients here: it takes the first color of one.
                    const auto it = m_gradients.find(p.style.fill.url);
                    if (it != m_gradients.end() && !it->second.stops.empty()) {
                        const GradientStop &s = it->second.stops.front();
                        color = {s.r, s.g, s.b, s.a};
                    } else if (p.style.fill.fallback == PaintSpec::CurrentColor) {
                        color = p.style.color;
                    } else if (p.style.fill.fallback == PaintSpec::None) {
                        x += p.advance;
                        continue;
                    }
                }
                const auto channel = [](double v) { return static_cast<int>(std::lround(clamp01(v) * 255.0)); };
                QImage image(reinterpret_cast<uchar *>(ctx.surface->pixels.data()), ctx.surface->width,
                             ctx.surface->height, QImage::Format_ARGB32_Premultiplied);
                QPainter painter(&image);
                painter.setFont(p.font);
                painter.setPen(QColor(channel(color.r), channel(color.g), channel(color.b),
                                      channel(color.a * p.style.fillOpacity * opacity)));
                painter.drawText(QPoint(px, py), p.text);
            }
            x += p.advance;
        }
    }
}

} // namespace QtSvgPrivate
