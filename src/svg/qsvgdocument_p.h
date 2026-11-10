// MIT License - see LICENSE
//
// The tree of an SVG document, as QSvgRenderer keeps it, and the code that draws it.
#pragma once

#include "qsvgraster_p.h"

#include <QtCore/qbytearray.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace QtSvgPrivate {

struct XmlElement;

struct Rgba {
    double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
};

struct PaintSpec {
    enum Kind { None, Color, CurrentColor, Url };
    Kind kind = None;
    Rgba color;
    std::string url;          // the id of a gradient
    Kind fallback = None;     // what to use if the id is not found: None, Color or CurrentColor
};

// The properties that children inherit.
struct Style {
    PaintSpec fill{PaintSpec::Color, {}, {}, PaintSpec::None};
    PaintSpec stroke;
    double fillOpacity = 1.0;
    double strokeOpacity = 1.0;
    FillRule fillRule = FillRule::NonZero;
    FillRule clipRule = FillRule::NonZero;
    StrokeStyle pen;
    Rgba color;
    bool visible = true;
    std::string fontFamily;
    double fontSize = 16.0;
    bool bold = false;
    bool italic = false;
    int anchor = 0; // 0 start, 1 middle, 2 end
};

// The properties that an element sets.
struct StyleDelta {
    enum Bit : unsigned {
        Fill = 1u << 0,
        Stroke = 1u << 1,
        FillOpacity = 1u << 2,
        StrokeOpacity = 1u << 3,
        FillRuleBit = 1u << 4,
        ClipRule = 1u << 5,
        StrokeWidth = 1u << 6,
        Cap = 1u << 7,
        Join = 1u << 8,
        MiterLimit = 1u << 9,
        Dashes = 1u << 10,
        DashOffset = 1u << 11,
        ColorBit = 1u << 12,
        Visibility = 1u << 13,
        FontFamily = 1u << 14,
        FontSize = 1u << 15,
        FontWeight = 1u << 16,
        FontStyle = 1u << 17,
        Anchor = 1u << 18
    };
    unsigned set = 0;
    Style value;
    // These are not inherited.
    double opacity = 1.0;
    bool display = true;
    std::string clipPath;

    void applyTo(Style &style) const;
};

struct TextRun {
    std::string text;
    bool hasX = false, hasY = false;
    double x = 0.0, y = 0.0, dx = 0.0, dy = 0.0;
    StyleDelta style;
};

struct Node {
    enum Type { Group, Shape, Text, Use };
    Type type = Group;
    std::string id;
    Mat transform;
    StyleDelta style;
    Node *parent = nullptr;
    // False for defs, symbol and clipPath: they and what is in them are drawn only where something refers to it.
    bool rendered = true;
    bool clipBoxUnits = false; // clipPath: its coordinates are fractions of the bounding box
    Path path;                 // Shape
    std::string href;          // Use
    std::vector<TextRun> runs; // Text
    std::vector<std::unique_ptr<Node>> children;
};

struct Length {
    double value = 0.0;
    bool percent = false;
    bool set = false;
};

struct Gradient {
    bool radial = false;
    bool boxUnits = true;
    bool unitsSet = false;
    Mat transform;
    bool transformSet = false;
    Spread spread = Spread::Pad;
    bool spreadSet = false;
    Length x1, y1, x2, y2;     // linear
    Length cx, cy, r, fx, fy;  // radial
    std::vector<GradientStop> stops;
    std::string href;
};

class Document {
public:
    Document();
    ~Document();

    // Reads a document; returns false if it is not well-formed XML or its root is not an svg element.
    bool parse(const QByteArray &data);

    // The size the document asks for, in pixels.
    double width() const { return m_width; }
    double height() const { return m_height; }
    Box viewBox() const { return m_viewBox; }

    // How many elements one drawing may visit, counting each use of an element. A document that asks for more
    // is drawn as far as that goes.
    static constexpr long long DefaultBudget = 500000;
    void setBudget(long long elements) { m_budget = elements; }

    const Node *find(const std::string &id) const;
    // The bounds of a node with its own transformation, without those of its ancestors.
    Box bounds(const Node *node) const;

    // Draws the document, or one node of it, with the transformation `m` from user space to the surface.
    void render(Surface *surface, const Mat &m, const Node *only = nullptr) const;

    struct Sheet;

private:
    struct Context;
    void build(const XmlElement &e, Node *parent, const Sheet &sheet, int depth);
    void buildText(const XmlElement &e, Node *node, const Sheet &sheet, const StyleDelta &inherited, bool *space,
                   int depth);
    void addGradient(const XmlElement &e, const Sheet &sheet);
    void resolveGradients();
    Style inheritedStyle(const Node *node) const;
    void renderNode(const Node &node, const Mat &m, const Style &inherited, Context &ctx, bool referenced) const;
    void drawContent(const Node &node, const Mat &m, const Style &style, double opacity, Context &ctx) const;
    void drawShape(const Node &node, const Mat &m, const Style &style, double opacity, Context &ctx) const;
    void drawText(const Node &node, const Mat &m, const Style &style, double opacity, Context &ctx) const;
    void drawClip(const Node &clip, const Mat &m, Context &ctx, int depth) const;
    bool shaderFor(const PaintSpec &paint, const Style &style, double opacity, const Box &box, const Mat &m,
                   Shader *out) const;

    std::unique_ptr<Node> m_root;
    std::map<std::string, Node *> m_ids;
    std::map<std::string, Gradient> m_gradients;
    double m_width = 0.0;
    double m_height = 0.0;
    Box m_viewBox;
    double m_viewportWidth = 100.0;
    double m_viewportHeight = 100.0;
    long long m_budget = DefaultBudget;
};

} // namespace QtSvgPrivate
