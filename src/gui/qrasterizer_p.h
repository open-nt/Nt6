// MIT License - see LICENSE
//
// The geometry and the antialiased scanline rasterizer of Nt GUI: paths of lines and Bezier curves, strokes
// with caps, joins and dashes, and fills with colors and gradients, on a premultiplied ARGB surface. QPainter
// draws paths and transformed shapes with it; Nt SVG and Nt Charts draw with it directly.
#pragma once

#include <QtCore/qglobal.h>

#include <cstdint>
#include <vector>

namespace QtRasterPrivate {

struct Pt {
    double x = 0.0;
    double y = 0.0;
};

// An affine transformation: x' = a*x + c*y + e, y' = b*x + d*y + f.
struct Mat {
    double a = 1.0, b = 0.0, c = 0.0, d = 1.0, e = 0.0, f = 0.0;

    Pt map(Pt p) const { return {a * p.x + c * p.y + e, b * p.x + d * p.y + f}; }
    // The transformation that applies `first` and then this one.
    Mat operator*(const Mat &first) const;
    bool invert(Mat *out) const;
    // The factor by which lengths grow, averaged over the directions.
    double scale() const;
    static Mat translate(double tx, double ty) { return {1.0, 0.0, 0.0, 1.0, tx, ty}; }
    static Mat scaling(double sx, double sy) { return {sx, 0.0, 0.0, sy, 0.0, 0.0}; }
};

// Reads a decimal number at `p`, whatever the locale: an optional sign, digits with an optional fraction, and
// an exponent if digits follow the "e". Returns the end of the number, or null if there is none.
const char *scanNumber(const char *p, const char *end, double *out);

struct Box {
    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
    bool valid = false;

    void add(Pt p);
    void add(const Box &other);
    double width() const { return valid ? x1 - x0 : 0.0; }
    double height() const { return valid ? y1 - y0 : 0.0; }
};

// A path of lines and cubic Bézier curves in user space.
class Path {
public:
    enum Verb : unsigned char { Move, Line, Cubic, Close };

    void moveTo(Pt p);
    void lineTo(Pt p);
    void cubicTo(Pt c1, Pt c2, Pt p);
    void quadTo(Pt c, Pt p);
    // An elliptical arc from the current point, with the parameters of the SVG path command A.
    void arcTo(double rx, double ry, double rotationDegrees, bool largeArc, bool sweep, Pt p);
    void close();
    void addRect(double x, double y, double w, double h, double rx, double ry);
    void addEllipse(double cx, double cy, double rx, double ry);
    bool isEmpty() const { return m_verbs.empty(); }
    Pt currentPoint() const { return m_current; }

    // Parses the data of an SVG path. Stops at the first error and keeps what came before it, as SVG asks.
    static Path fromSvg(const char *data, std::size_t size);

    struct Contour {
        std::vector<Pt> points;
        bool closed = false;
    };
    // The path as polylines that stay within `tolerance` of the curves.
    std::vector<Contour> flatten(double tolerance) const;

private:
    std::vector<Verb> m_verbs;
    std::vector<Pt> m_points;
    Pt m_start;
    Pt m_current;
};

enum class FillRule { NonZero, EvenOdd };
enum class LineCap { Butt, Round, Square };
enum class LineJoin { Miter, Round, Bevel };

struct StrokeStyle {
    double width = 1.0;
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
    double miterLimit = 4.0;
    std::vector<double> dashes;
    double dashOffset = 0.0;
};

// Closed polygons, each wound the same way, so that filling them with the non-zero rule gives their union.
struct Polygons {
    std::vector<Pt> points;
    std::vector<std::size_t> ends; // one past the last point of each polygon

    void add(const Pt *pts, std::size_t count);
    void clear() {
        points.clear();
        ends.clear();
    }
};

// The outline of the stroke of `contours`, transformed by `m`. `tolerance` is in user space.
void strokeContours(const std::vector<Path::Contour> &contours, const StrokeStyle &style, const Mat &m,
                    double tolerance, Polygons *out);
// The contours as polygons transformed by `m`.
void fillContours(const std::vector<Path::Contour> &contours, const Mat &m, Polygons *out);

struct GradientStop {
    double offset = 0.0;
    double r = 0.0, g = 0.0, b = 0.0, a = 1.0; // not premultiplied, 0 to 1
};

enum class Spread { Pad, Reflect, Repeat };

// What a shape is filled with: one color, or a gradient in device space.
struct Shader {
    bool gradient = false;
    std::uint32_t color = 0; // premultiplied ARGB
    bool radial = false;
    bool conical = false; // p1 the center, angle the direction where it starts, counterclockwise
    double angle = 0.0;
    Mat toGradient; // from device space to the space of the gradient
    Pt p1, p2;      // linear: the gradient vector; radial: p1 the center, p2 the focal point
    double radius = 0.0;
    Spread spread = Spread::Pad;
    std::vector<std::uint32_t> table; // premultiplied ARGB along the gradient

    void setStops(const std::vector<GradientStop> &stops, double opacity);
    std::uint32_t at(double x, double y) const;
};

// A premultiplied ARGB image.
struct Surface {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;

    Surface() = default;
    Surface(int w, int h) : width(w), height(h), pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h)) {}

    // Fills the polygons with the shader, antialiased.
    void fill(const Polygons &polygons, FillRule rule, const Shader &shader);
    // Blends `layer` over this surface with the given opacity. With a mask, the alpha of the mask scales it too.
    void composite(const Surface &layer, double opacity, const Surface *mask);
};

} // namespace QtRasterPrivate
