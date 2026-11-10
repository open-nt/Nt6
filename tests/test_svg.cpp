// MIT License - see LICENSE
//
// Tests for Nt SVG and Nt SVG Widgets on the offscreen platform: the XML reader, paths and the rasterizer
// through their private headers, and QSvgRenderer, QSvgGenerator and QSvgWidget through pixels.
#include <QtCore/qbuffer.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qimage.h>
#include <QtGui/qpaintengine.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstylehints.h>
#include <QtSvg/QSvgGenerator>
#include <QtSvg/QSvgRenderer>
#include <QtSvg/QtSvg>
#include <QtSvgWidgets/QSvgWidget>
#include <QtSvgWidgets/QtSvgWidgets>
#include <QtWidgets/qapplication.h>

#include "qsvgraster_p.h"
#include "qsvgxml_p.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace QtSvgPrivate;

static QByteArray doc(const char *rootAttributes, const char *body) {
    return QByteArray("<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' ") +
           rootAttributes + ">" + body + "</svg>";
}

static QByteArray doc(const char *rootAttributes, const std::string &body) { return doc(rootAttributes, body.c_str()); }

// Draws a document on a white image of the given size.
static QImage draw(QSvgRenderer &renderer, int width, int height) {
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    renderer.render(&painter);
    return image;
}

static QImage draw(const QByteArray &svg, int width, int height) {
    QSvgRenderer renderer(svg);
    CHECK(renderer.isValid());
    if (!renderer.isValid())
        std::printf("  %s\n", svg.constData());
    return draw(renderer, width, height);
}

static bool rgb(QRgb pixel, int r, int g, int b, int tolerance = 2) {
    const bool ok = std::abs(qRed(pixel) - r) <= tolerance && std::abs(qGreen(pixel) - g) <= tolerance &&
                    std::abs(qBlue(pixel) - b) <= tolerance;
    if (!ok)
        std::printf("  pixel is %d,%d,%d, expected %d,%d,%d\n", qRed(pixel), qGreen(pixel), qBlue(pixel), r, g, b);
    return ok;
}

static bool isWhite(QRgb pixel) { return rgb(pixel, 255, 255, 255, 0); }

static bool rectIs(const QRectF &r, double x, double y, double w, double h, double tolerance = 1e-6) {
    const bool ok = std::fabs(r.x() - x) <= tolerance && std::fabs(r.y() - y) <= tolerance &&
                    std::fabs(r.width() - w) <= tolerance && std::fabs(r.height() - h) <= tolerance;
    if (!ok)
        std::printf("  rectangle is %g,%g %gx%g, expected %g,%g %gx%g\n", r.x(), r.y(), r.width(), r.height(), x, y, w, h);
    return ok;
}

static int occurrences(const QByteArray &in, const char *what) {
    int count = 0;
    for (qsizetype at = in.indexOf(what); at >= 0; at = in.indexOf(what, at + 1))
        ++count;
    return count;
}

static void testXml() {
    const auto parse = [](const std::string &s) { return parseXml(s.data(), s.size()); };
    auto root = parse("\xef\xbb\xbf<?xml version='1.0'?>\n<!-- c --><!DOCTYPE svg [ <!ENTITY a \"b>\"> ]>\n"
                      "<svg:svg a='1' b=\"x &lt;&amp;&gt;&quot;&apos; &#65;&#x42; &unknown;\" xlink:href='#h'>"
                      "t1<!-- c --><g/><![CDATA[<raw>&amp;]]><?pi?>t2<a:b>in</a:b></svg:svg>\n<!-- end -->\n");
    CHECK(root != nullptr);
    if (root) {
        CHECK(root->name == "svg");
        CHECK(root->attribute("a") && *root->attribute("a") == "1");
        CHECK(root->attribute("b") && *root->attribute("b") == "x <&>\"' AB &unknown;");
        CHECK(root->attribute("href") && *root->attribute("href") == "#h");
        CHECK(root->attribute("missing") == nullptr);
        CHECK(root->children.size() == 4);
        if (root->children.size() == 4) {
            CHECK(root->children[0]->name == "#text" && root->children[0]->text == "t1");
            CHECK(root->children[1]->name == "g" && root->children[1]->children.empty());
            CHECK(root->children[2]->name == "#text" && root->children[2]->text == "<raw>&amp;t2");
            CHECK(root->children[3]->name == "a:b");
        }
    }
    CHECK(parse("<a><b></b></a>") != nullptr);
    CHECK(parse("<a b='1' />") != nullptr);
    // Not well formed.
    for (const char *bad :
         {"", "text", "<a>", "<a></b>", "<a><b></a></b>", "<a b></a>", "<a b=1></a>", "<a b='1' b='2'/>",
          "<a b='1'c='2'/>", "<a b='<'/>", "<a/><b/>", "<a/>trailing", "<a><!-- open</a>", "<a><![CDATA[ open</a>",
          "<a b='open/>", "<!DOCTYPE a [", "<", "<a", "<a/", "<a><?pi</a>", "</a>"})
        CHECK(parse(bad) == nullptr);
    // Character references that are not characters are replaced.
    root = parse("<a b='&#0;&#xd800;&#x110000;&#;&#xZ;'/>");
    CHECK(root && *root->attribute("b") == "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd&#;&#xZ;");
    // Nesting has a limit, so that nothing recurses without one.
    std::string deep;
    for (int i = 0; i < 100000; ++i)
        deep += "<g>";
    CHECK(parse(deep) == nullptr);
    std::string closed;
    for (int i = 0; i < 200; ++i)
        closed += "<g>";
    for (int i = 0; i < 200; ++i)
        closed += "</g>";
    CHECK(parse(closed) != nullptr);
}

static void testNumbers() {
    const auto scan = [](const std::string &s, double *v) {
        const char *end = scanNumber(s.data(), s.data() + s.size(), v);
        return end ? static_cast<int>(end - s.data()) : -1;
    };
    double v = 0.0;
    CHECK(scan("12", &v) == 2 && v == 12.0);
    CHECK(scan("-1.5e2x", &v) == 6 && v == -150.0);
    CHECK(scan("+.25", &v) == 4 && v == 0.25);
    CHECK(scan("3.", &v) == 2 && v == 3.0);
    CHECK(scan("1em", &v) == 1 && v == 1.0);
    CHECK(scan("1e-3", &v) == 4 && std::fabs(v - 0.001) < 1e-12);
    CHECK(scan("0.5.5", &v) == 3 && v == 0.5);
    for (const char *bad : {"", ".", "-", "e5", "inf", "nan", "1e99999", " 1"})
        CHECK(scan(bad, &v) == -1);
}

static void testPaths() {
    const auto contours = [](const char *d) { return Path::fromSvg(d, std::strlen(d)).flatten(0.01); };

    auto c = contours("M1 2 L3 4 5 6 H7 V8 h-1 v-1 l1,1 z");
    CHECK(c.size() == 1);
    if (c.size() == 1) {
        CHECK(c[0].closed);
        CHECK(c[0].points.size() == 8);
        CHECK(c[0].points[0].x == 1.0 && c[0].points[0].y == 2.0);
        CHECK(c[0].points[3].x == 7.0 && c[0].points[3].y == 6.0);
        CHECK(c[0].points[4].x == 7.0 && c[0].points[4].y == 8.0);
        CHECK(c[0].points[7].x == 7.0 && c[0].points[7].y == 8.0);
    }
    // Further pairs after a move are lines; relative moves; numbers that run together.
    c = contours("m1 1 2 0 0 2 m10 10 1 1");
    CHECK(c.size() == 2 && c[0].points.size() == 3 && !c[0].closed);
    CHECK(c.size() == 2 && c[1].points[0].x == 13.0 && c[1].points[0].y == 13.0);
    c = contours("M.5.5-1-1L1e1-2");
    CHECK(c.size() == 1 && c[0].points.size() == 3 && c[0].points[1].x == -1.0 && c[0].points[2].x == 10.0);
    // After a close, drawing continues from the start of the subpath.
    c = contours("M0 0 L10 0 L10 10 Z L-5 -5");
    CHECK(c.size() == 2 && c[1].points.size() == 2 && c[1].points[0].x == 0.0 && c[1].points[1].x == -5.0);
    // A lone move draws nothing.
    CHECK(contours("M5 5").empty());
    CHECK(contours("M5 5 M6 6 L7 7").size() == 1);
    CHECK(contours("M5 5 z").size() == 1);
    // Errors keep what came before them.
    c = contours("M0 0 L1 1 L2");
    CHECK(c.size() == 1 && c[0].points.size() == 2);
    c = contours("M0 0 L1 1 X 5 5 L 9 9");
    CHECK(c.size() == 1 && c[0].points.size() == 2);
    CHECK(contours("L1 1").empty());
    CHECK(contours("").empty());
    CHECK(contours("M0 0 1").empty());

    // Curves end where they should and stay near the curve.
    c = contours("M0 0 C0 10 10 10 10 0 S20 -10 20 0 Q25 10 30 0 T40 0");
    CHECK(c.size() == 1 && c[0].points.size() > 20);
    if (c.size() == 1) {
        CHECK(std::fabs(c[0].points.back().x - 40.0) < 1e-9 && std::fabs(c[0].points.back().y) < 1e-9);
        double top = 0.0, bottom = 0.0;
        for (const Pt &p : c[0].points) {
            top = std::max(top, p.y);
            bottom = std::min(bottom, p.y);
        }
        CHECK(std::fabs(top - 7.5) < 0.02);    // the first curve peaks at 3/4 of its control points
        CHECK(std::fabs(bottom + 7.5) < 0.02); // and S mirrors it
    }
    // An arc: half a circle of radius 10 from (0,0) to (20,0), with flags that run into the numbers.
    for (const char *d : {"M0 0 A10 10 0 0 1 20 0", "M0 0a10 10 0 0120 0"}) {
        c = contours(d);
        CHECK(c.size() == 1);
        if (c.size() != 1)
            continue;
        double worst = 0.0, top = 0.0;
        for (const Pt &p : c[0].points) {
            worst = std::max(worst, std::fabs(std::hypot(p.x - 10.0, p.y) - 10.0));
            top = std::min(top, p.y);
        }
        CHECK(worst < 0.02);
        CHECK(std::fabs(top + 10.0) < 0.02); // sweep 1 goes clockwise on the screen: over the top
        CHECK(std::fabs(c[0].points.back().x - 20.0) < 1e-9);
    }
    // The other sweep goes below; radii that are too small are scaled up; a zero radius is a line.
    c = contours("M0 0 A10 10 0 0 0 20 0");
    CHECK(c.size() == 1 && c[0].points[c[0].points.size() / 2].y > 9.0);
    c = contours("M0 0 A1 1 0 0 1 20 0");
    CHECK(c.size() == 1 && std::fabs(c[0].points[c[0].points.size() / 2].y + 10.0) < 0.05);
    c = contours("M0 0 A0 5 0 0 1 20 0");
    CHECK(c.size() == 1 && c[0].points.size() == 2);
    CHECK(contours("M0 0 A10 10 0 2 1 20 0").empty()); // a flag is 0 or 1

    Path rect;
    rect.addRect(1, 2, 10, 20, 0, 0);
    CHECK(rect.flatten(0.1).size() == 1 && rect.flatten(0.1)[0].points.size() == 4);
    Path round;
    round.addRect(0, 0, 10, 10, 50, 50); // the radii are at most half the sides
    Box box;
    const auto roundContours = round.flatten(0.01);
    for (const Pt &p : roundContours[0].points)
        box.add(p);
    CHECK(std::fabs(box.x0) < 1e-9 && std::fabs(box.x1 - 10.0) < 1e-9 && std::fabs(box.height() - 10.0) < 1e-9);

    Mat m = Mat::translate(10, 20) * Mat::scaling(2, 3);
    CHECK(m.map({1, 1}).x == 12.0 && m.map({1, 1}).y == 23.0);
    Mat inverse;
    CHECK(m.invert(&inverse));
    CHECK(std::fabs(inverse.map({12, 23}).x - 1.0) < 1e-12 && std::fabs(inverse.map({12, 23}).y - 1.0) < 1e-12);
    CHECK(!Mat::scaling(0, 1).invert(&inverse));
    CHECK(std::fabs(Mat::scaling(2, 8).scale() - 4.0) < 1e-12);
}

static Polygons square(double x0, double y0, double x1, double y1) {
    Polygons p;
    const Pt pts[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    p.add(pts, 4);
    return p;
}

static void testRaster() {
    const auto alphaAt = [](const Surface &s, int x, int y) {
        return static_cast<int>(s.pixels[static_cast<std::size_t>(y * s.width + x)] >> 24u);
    };
    Shader white;
    white.color = 0xffffffffu;
    {
        // Coverage is exact for edges along and across pixels.
        Surface s(8, 8);
        s.fill(square(1.5, 2.0, 5.25, 6.5), FillRule::NonZero, white);
        CHECK(alphaAt(s, 0, 3) == 0);
        CHECK(std::abs(alphaAt(s, 1, 3) - 128) <= 1);
        CHECK(alphaAt(s, 2, 3) == 255 && alphaAt(s, 4, 3) == 255);
        CHECK(std::abs(alphaAt(s, 5, 3) - 64) <= 1);
        CHECK(alphaAt(s, 6, 3) == 0);
        CHECK(alphaAt(s, 3, 1) == 0 && alphaAt(s, 3, 2) == 255);
        CHECK(std::abs(alphaAt(s, 3, 6) - 128) <= 1);
        CHECK(std::abs(alphaAt(s, 1, 6) - 64) <= 1);
        CHECK(alphaAt(s, 3, 7) == 0);
    }
    {
        // A diagonal edge: half of the square, and half of each pixel on the diagonal.
        Surface s(4, 4);
        Polygons p;
        const Pt pts[3] = {{0, 0}, {4, 4}, {0, 4}};
        p.add(pts, 3);
        s.fill(p, FillRule::NonZero, white);
        CHECK(std::abs(alphaAt(s, 1, 1) - 128) <= 1);
        CHECK(alphaAt(s, 0, 2) == 255 && alphaAt(s, 3, 1) == 0);
    }
    {
        // What lies outside the surface is cut off on every side, and still counts for what is inside.
        Surface s(4, 4);
        s.fill(square(-100, -100, 100, 100), FillRule::NonZero, white);
        for (int i = 0; i < 16; ++i)
            CHECK(s.pixels[static_cast<std::size_t>(i)] == 0xffffffffu);
        Surface t(4, 4);
        t.fill(square(-3.5, 1, 2, 3), FillRule::NonZero, white);
        CHECK(alphaAt(t, 0, 1) == 255 && alphaAt(t, 1, 2) == 255 && alphaAt(t, 2, 1) == 0 && alphaAt(t, 0, 0) == 0);
        Surface u(4, 4);
        u.fill(square(2, 1, 1e9, 3), FillRule::NonZero, white);
        CHECK(alphaAt(u, 1, 1) == 0 && alphaAt(u, 2, 1) == 255 && alphaAt(u, 3, 2) == 255);
        Surface w(4, 4);
        Polygons p;
        const Pt pts[3] = {{-10, -10}, {22, 2}, {2, 22}}; // a triangle over all corners
        p.add(pts, 3);
        w.fill(p, FillRule::NonZero, white);
        CHECK(alphaAt(w, 0, 0) == 255 && alphaAt(w, 3, 3) == 255 && alphaAt(w, 3, 0) == 255 && alphaAt(w, 0, 3) == 255);
        Surface none(4, 4);
        none.fill(square(10, 10, 20, 20), FillRule::NonZero, white);
        none.fill(square(-20, -20, -10, -10), FillRule::NonZero, white);
        none.fill(square(1, 1, 1, 3), FillRule::NonZero, white);
        none.fill(Polygons(), FillRule::NonZero, white);
        for (int i = 0; i < 16; ++i)
            CHECK(none.pixels[static_cast<std::size_t>(i)] == 0u);
    }
    {
        // Two squares wound the same way: the overlap is filled once, or not at all.
        Polygons both = square(0, 0, 6, 6);
        const Polygons inner = square(2, 2, 4, 4);
        both.points.insert(both.points.end(), inner.points.begin(), inner.points.end());
        both.ends.push_back(both.points.size());
        Surface nonZero(6, 6), evenOdd(6, 6);
        nonZero.fill(both, FillRule::NonZero, white);
        evenOdd.fill(both, FillRule::EvenOdd, white);
        CHECK(alphaAt(nonZero, 3, 3) == 255 && alphaAt(nonZero, 1, 1) == 255);
        CHECK(alphaAt(evenOdd, 3, 3) == 0 && alphaAt(evenOdd, 1, 1) == 255);
    }
    {
        // Polygons::add() winds everything one way, whichever way it was given.
        Polygons p;
        const Pt forward[3] = {{0, 0}, {4, 0}, {0, 4}}, backward[3] = {{0, 0}, {0, 4}, {4, 0}};
        p.add(forward, 3);
        p.add(backward, 3);
        p.add(forward, 2); // not a polygon
        CHECK(p.ends.size() == 2 && p.points.size() == 6);
        CHECK(p.points[3].x == 4.0 && p.points[4].y == 4.0 && p.points[5].x == 0.0 && p.points[5].y == 0.0);
    }
    {
        // Blending and layers.
        Surface s(2, 1);
        Shader red;
        red.color = 0xffff0000u;
        s.fill(square(0, 0, 2, 1), FillRule::NonZero, red);
        Shader halfBlue;
        halfBlue.color = 0x80000080u; // premultiplied
        s.fill(square(0, 0, 1, 1), FillRule::NonZero, halfBlue);
        CHECK(rgb(s.pixels[0], 127, 0, 128, 2) && (s.pixels[0] >> 24u) == 255u);
        CHECK(s.pixels[1] == 0xffff0000u);
        Surface layer(2, 1), mask(2, 1), target(2, 1);
        layer.pixels = {0xff00ff00u, 0xff00ff00u};
        mask.pixels = {0xffffffffu, 0u};
        target.pixels = {0xffff0000u, 0xffff0000u};
        target.composite(layer, 0.5, &mask);
        CHECK(rgb(target.pixels[0], 127, 128, 0, 2));
        CHECK(target.pixels[1] == 0xffff0000u);
        target.composite(layer, 1.0, nullptr);
        CHECK(target.pixels[1] == 0xff00ff00u);
        Surface other(3, 1);
        other.pixels = {0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
        target.composite(other, 1.0, nullptr); // sizes differ: nothing happens
        CHECK(target.pixels[1] == 0xff00ff00u);
    }
    {
        // Gradients: the table, and the three ways to continue outside it.
        Shader g;
        g.gradient = true;
        g.p1 = {0, 0};
        g.p2 = {10, 0};
        g.setStops({{0.0, 0, 0, 0, 1}, {0.5, 1, 0, 0, 1}, {1.0, 1, 1, 1, 1}}, 1.0);
        CHECK(rgb(g.at(0, 0), 0, 0, 0));
        CHECK(rgb(g.at(2.5, 3), 128, 0, 0, 3));
        CHECK(rgb(g.at(5, 0), 255, 0, 0, 3));
        CHECK(rgb(g.at(7.5, 0), 255, 128, 128, 3));
        CHECK(rgb(g.at(-5, 0), 0, 0, 0) && rgb(g.at(50, 0), 255, 255, 255));
        g.spread = Spread::Repeat;
        CHECK(rgb(g.at(12.5, 0), 128, 0, 0, 3));
        g.spread = Spread::Reflect;
        CHECK(rgb(g.at(12.5, 0), 255, 128, 128, 3) && rgb(g.at(-2.5, 0), 128, 0, 0, 3));
        g.setStops({{0.0, 1, 1, 1, 1}, {1.0, 1, 1, 1, 0}}, 0.5);
        CHECK((g.at(0, 0) >> 24u) == 128u && (g.at(0, 0) & 0xffu) == 128u); // premultiplied
        Shader r;
        r.gradient = r.radial = true;
        r.p1 = r.p2 = {0, 0};
        r.radius = 10;
        r.setStops({{0.0, 0, 0, 0, 1}, {1.0, 1, 1, 1, 1}}, 1.0);
        CHECK(rgb(r.at(0, 0), 0, 0, 0) && rgb(r.at(3, 4), 128, 128, 128, 3) && rgb(r.at(0, 20), 255, 255, 255));
        r.p2 = {5, 0}; // the focal point: black there, and the rings are off center
        CHECK(rgb(r.at(5, 0), 0, 0, 0, 3) && rgb(r.at(10, 0), 255, 255, 255, 3));
        CHECK(rgb(r.at(7.5, 0), 128, 128, 128, 3) && rgb(r.at(-2.5, 0), 128, 128, 128, 3));
        Shader empty;
        empty.gradient = true;
        CHECK(empty.at(1, 1) == 0u);
    }
}

static void testLoad() {
    QSvgRenderer empty;
    CHECK(!empty.isValid());
    CHECK(!empty.defaultSize().isValid());
    CHECK(empty.viewBoxF().isNull());
    CHECK(!empty.elementExists("a"));
    CHECK(empty.boundsOnElement("a").isNull());
    CHECK(!empty.animated() && empty.animationDuration() == 0 && empty.currentFrame() == 0);
    CHECK(empty.framesPerSecond() == 30 && empty.isAnimationEnabled());
    empty.setFramesPerSecond(60);
    empty.setFramesPerSecond(-1);
    empty.setCurrentFrame(5);
    empty.setAnimationEnabled(false);
    CHECK(empty.framesPerSecond() == 60 && empty.currentFrame() == 0 && !empty.isAnimationEnabled());
    CHECK(empty.options() == QtSvg::Options());
    empty.setOptions(QtSvg::Tiny12FeaturesOnly | QtSvg::AssumeTrustedSource);
    CHECK(empty.options().testFlag(QtSvg::Tiny12FeaturesOnly));
    CHECK(empty.aspectRatioMode() == Qt::IgnoreAspectRatio);
    // Rendering nothing does nothing.
    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        empty.render(&painter);
        empty.render(&painter, "a");
        empty.render(nullptr);
        QPainter idle;
        empty.render(&idle);
    }
    CHECK(isWhite(image.pixel(1, 1)));

    CHECK(!empty.load(QByteArray()));
    CHECK(!empty.load(QByteArray("not xml")));
    CHECK(!empty.load(QByteArray("<html/>")));
    CHECK(!empty.load(QByteArray("<svg><g></svg>")));
    CHECK(!empty.load(QString("/nonexistent/file.svg")));
    CHECK(empty.load(QByteArray("<svg/>")));
    CHECK(empty.isValid());
    CHECK(empty.defaultSize() == QSize(100, 100));
    CHECK(!empty.load(QByteArray("<svg")));
    CHECK(!empty.isValid());

    // The size and the view box.
    {
        QSvgRenderer r(doc("width='40' height='30' viewBox='5 6 80 60'", ""));
        CHECK(r.defaultSize() == QSize(40, 30));
        CHECK(r.viewBoxF() == QRectF(5, 6, 80, 60) && r.viewBox() == QRect(5, 6, 80, 60));
    }
    const auto size = [](const char *attributes) { return QSvgRenderer(doc(attributes, "")).defaultSize(); };
    CHECK(size("width='2in' height='1in'") == QSize(192, 96));
    CHECK(size("width='25.4mm' height='2.54cm'") == QSize(96, 96));
    CHECK(size("width='72pt' height='6pc'") == QSize(96, 96));
    CHECK(size("width='10px' height='2em'") == QSize(10, 32));
    CHECK(size("viewBox='0 0 30 40'") == QSize(30, 40));
    CHECK(size("viewBox='0,0,30,40' width='100%' height='100%'") == QSize(30, 40));
    CHECK(size("viewBox='0 0 10 20' width='100'") == QSize(100, 200));
    CHECK(size("viewBox='0 0 10 20' height='100'") == QSize(50, 100));
    CHECK(QSvgRenderer(doc("width='40' height='30'", "")).viewBoxF() == QRectF(0, 0, 40, 30));
    // Without either, the bounds of what is drawn.
    {
        QSvgRenderer r(doc("", "<rect x='10' y='10' width='30' height='20'/>"));
        CHECK(r.viewBoxF() == QRectF(10, 10, 30, 20) && r.defaultSize() == QSize(30, 20));
    }
    // A view box that is not one is ignored, as are sizes that are not.
    CHECK(QSvgRenderer(doc("viewBox='0 0 0 10' width='7' height='8'", "")).viewBoxF() == QRectF(0, 0, 7, 8));
    CHECK(size("viewBox='0 0 10' width='-7' height='x'") == QSize(100, 100));

    // From a file.
    const QString path = QDir::tempPath() + "/nt_svg_test_load.svg";
    {
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(doc("width='12' height='34'", ""));
    }
    QSvgRenderer fromFile(path);
    CHECK(fromFile.isValid() && fromFile.defaultSize() == QSize(12, 34));
    QSvgRenderer later;
    CHECK(later.load(path) && later.defaultSize() == QSize(12, 34));
    QFile::remove(path);
    QObject parent;
    auto *child = new QSvgRenderer(QByteArray("<svg/>"), &parent);
    CHECK(child->parent() == &parent && child->isValid());
    CHECK(child->inherits("QSvgRenderer"));
    CHECK(child->property("framesPerSecond").toInt() == 30);
}

static void testShapes() {
    QImage i = draw(doc("width='20' height='20'", "<rect x='5' y='5' width='10' height='10' fill='#ff0000'/>"), 20, 20);
    CHECK(rgb(i.pixel(5, 5), 255, 0, 0) && rgb(i.pixel(14, 14), 255, 0, 0));
    CHECK(isWhite(i.pixel(4, 10)) && isWhite(i.pixel(15, 10)) && isWhite(i.pixel(10, 4)) && isWhite(i.pixel(10, 15)));
    // Scaled to the device: twice the size.
    i = draw(doc("width='20' height='20'", "<rect x='5' y='5' width='10' height='10' fill='#ff0000'/>"), 40, 40);
    CHECK(rgb(i.pixel(10, 10), 255, 0, 0) && rgb(i.pixel(29, 29), 255, 0, 0) && isWhite(i.pixel(9, 20)) &&
          isWhite(i.pixel(30, 20)));
    // Antialiasing: half a pixel is half the color.
    i = draw(doc("width='20' height='20'", "<rect x='5.5' y='5' width='10' height='10' fill='#ff0000'/>"), 20, 20);
    CHECK(rgb(i.pixel(5, 10), 255, 127, 127, 2) && rgb(i.pixel(15, 10), 255, 127, 127, 2));
    // The default fill is black, and there is no stroke.
    i = draw(doc("width='20' height='20'", "<rect x='5' y='5' width='10' height='10'/>"), 20, 20);
    CHECK(rgb(i.pixel(10, 10), 0, 0, 0) && isWhite(i.pixel(4, 4)));
    // A stroke lies on the outline, half on each side.
    i = draw(doc("width='20' height='20'",
                 "<rect x='5' y='5' width='10' height='10' fill='none' stroke='blue' stroke-width='2'/>"),
             20, 20);
    CHECK(rgb(i.pixel(4, 10), 0, 0, 255) && rgb(i.pixel(5, 10), 0, 0, 255) && rgb(i.pixel(15, 15), 0, 0, 255));
    CHECK(isWhite(i.pixel(3, 10)) && isWhite(i.pixel(6, 10)) && isWhite(i.pixel(10, 10)));
    CHECK(rgb(i.pixel(4, 4), 0, 0, 255)); // the miter of the corner
    // Rounded corners; one radius gives the other.
    i = draw(doc("width='20' height='20'", "<rect width='20' height='20' rx='10' fill='red'/>"), 20, 20);
    CHECK(isWhite(i.pixel(0, 0)) && isWhite(i.pixel(19, 19)) && rgb(i.pixel(10, 0), 255, 0, 0, 30) &&
          rgb(i.pixel(4, 4), 255, 0, 0));
    // Circle, ellipse, line, polyline, polygon.
    i = draw(doc("width='40' height='20'",
                 "<circle cx='10' cy='10' r='8' fill='green'/><ellipse cx='30' cy='10' rx='9' ry='4' fill='blue'/>"),
             40, 20);
    CHECK(rgb(i.pixel(10, 10), 0, 128, 0) && rgb(i.pixel(3, 10), 0, 128, 0) && isWhite(i.pixel(3, 3)) &&
          isWhite(i.pixel(0, 10)));
    CHECK(rgb(i.pixel(22, 10), 0, 0, 255) && rgb(i.pixel(30, 7), 0, 0, 255) && isWhite(i.pixel(30, 4)));
    i = draw(doc("width='20' height='20'", "<line x1='2' y1='10' x2='18' y2='10' stroke='black' stroke-width='4'/>"
                                           "<line x1='2' y1='2' x2='18' y2='2'/>"),
             20, 20);
    CHECK(rgb(i.pixel(10, 8), 0, 0, 0) && rgb(i.pixel(10, 11), 0, 0, 0) && isWhite(i.pixel(10, 7)) &&
          isWhite(i.pixel(1, 10)) && isWhite(i.pixel(10, 2)));
    i = draw(doc("width='20' height='20'",
                 "<polygon points='0,0 20,0 20,20' fill='red'/><polyline points='0,4 0,20 16,20' fill='blue'/>"),
             20, 20);
    CHECK(rgb(i.pixel(15, 5), 255, 0, 0) && rgb(i.pixel(3, 15), 0, 0, 255) && isWhite(i.pixel(7, 9)));
    // Shapes without a size draw nothing.
    i = draw(doc("width='20' height='20'",
                 "<rect width='0' height='10'/><rect width='-5' height='10'/><circle r='0'/><ellipse rx='5'/>"
                 "<polygon points='1'/><path/><path d=''/>"),
             20, 20);
    CHECK(isWhite(i.pixel(0, 0)) && isWhite(i.pixel(2, 2)));
    // Percentages are taken of the view box.
    i = draw(doc("viewBox='0 0 200 100'", "<rect x='50%' y='50%' width='25%' height='50%' fill='red'/>"), 20, 10);
    CHECK(rgb(i.pixel(10, 5), 255, 0, 0) && rgb(i.pixel(14, 9), 255, 0, 0) && isWhite(i.pixel(9, 5)) &&
          isWhite(i.pixel(15, 5)) && isWhite(i.pixel(10, 4)));
}

static void testPaint() {
    const auto pixel = [](const std::string &body) {
        return draw(doc("width='20' height='20'", body), 20, 20).pixel(10, 10);
    };
    const std::string full = "<rect width='20' height='20' ";
    // Colors.
    CHECK(rgb(pixel(full + "fill='#0f0'/>"), 0, 255, 0));
    CHECK(rgb(pixel(full + "fill='#0F08'/>"), 119, 255, 119, 3));
    CHECK(rgb(pixel(full + "fill='#00ff0080'/>"), 127, 255, 127, 3));
    CHECK(rgb(pixel(full + "fill='rgb(10, 20,30)'/>"), 10, 20, 30));
    CHECK(rgb(pixel(full + "fill='rgb(100%,50%,0%)'/>"), 255, 128, 0));
    CHECK(rgb(pixel(full + "fill='rgba(255,0,0,0.5)'/>"), 255, 127, 127, 3));
    CHECK(rgb(pixel(full + "fill='AliceBlue'/>"), 240, 248, 255));
    CHECK(rgb(pixel(full + "fill='yellowgreen'/>"), 154, 205, 50));
    CHECK(rgb(pixel(full + "fill='lightgoldenrodyellow'/>"), 250, 250, 210));
    CHECK(rgb(pixel(full + "fill='darkgrey'/>"), 169, 169, 169));
    CHECK(rgb(pixel(full + "fill='mediumspringgreen'/>"), 0, 250, 154));
    CHECK(rgb(pixel(full + "fill='nocolor'/>"), 0, 0, 0)); // not a color: the default
    CHECK(rgb(pixel(full + "fill='#12'/>"), 0, 0, 0));
    CHECK(isWhite(pixel(full + "fill='none'/>")));
    CHECK(isWhite(pixel(full + "fill='transparent'/>")));
    CHECK(rgb(pixel("<g color='red'>" + full + "fill='currentColor'/></g>"), 255, 0, 0));
    // Opacity, of the fill and of the element.
    CHECK(rgb(pixel(full + "fill='red' fill-opacity='0.5'/>"), 255, 127, 127, 3));
    CHECK(rgb(pixel(full + "fill='red' fill-opacity='50%'/>"), 255, 127, 127, 3));
    CHECK(rgb(pixel(full + "fill='red' opacity='0.5' fill-opacity='0.5'/>"), 255, 191, 191, 3));
    CHECK(isWhite(pixel(full + "fill='red' opacity='0'/>")));
    CHECK(rgb(pixel("<line x1='0' y1='10' x2='20' y2='10' stroke='red' stroke-width='6' stroke-opacity='.5'/>"), 255,
              127, 127, 3));
    // A group is blended as a whole: its parts do not show through each other.
    CHECK(rgb(pixel("<g opacity='0.5'>" + full + "fill='red'/>" + full + "fill='red'/></g>"), 255, 127, 127, 3));
    CHECK(rgb(pixel("<g opacity='0.5'>" + full + "fill='blue'/>" + full + "fill='red'/></g>"), 255, 127, 127, 3));
    // So is a shape with both a fill and a stroke: the stroke hides the fill under it.
    CHECK(rgb(pixel("<rect width='20' height='11' fill='blue' stroke='red' stroke-width='4' opacity='0.5'/>"), 255, 127,
              127, 3));
    // A stroke that crosses itself is painted once.
    CHECK(rgb(pixel("<path d='M0 10 H20 M10 0 V20' stroke='red' stroke-width='6' stroke-opacity='0.5' fill='none'/>"),
              255, 127, 127, 3));
    // Inheritance, and what does not inherit.
    CHECK(rgb(pixel("<g fill='red'><g>" + full + "/></g></g>"), 255, 0, 0));
    CHECK(rgb(pixel("<g fill='red'>" + full + "fill='inherit'/></g>"), 255, 0, 0));
    CHECK(rgb(pixel("<g fill='red'>" + full + "fill='lime'/></g>"), 0, 255, 0));
    CHECK(rgb(pixel("<g opacity='0.5'/>" + full + "fill='red'/>"), 255, 0, 0));
    CHECK(isWhite(pixel("<g display='none'>" + full + "/></g>")));
    CHECK(isWhite(pixel(full + "display='none'/>")));
    CHECK(isWhite(pixel("<g visibility='hidden'>" + full + "/></g>")));
    CHECK(rgb(pixel("<g visibility='hidden'>" + full + "visibility='visible'/></g>"), 0, 0, 0));
    // The style attribute, and style sheets: attributes lose to rules, rules to the style attribute, and
    // an id is worth more than a class, a class more than an element name, whatever the order.
    CHECK(rgb(pixel(full + "fill='red' style='fill: blue; stroke:none'/>"), 0, 0, 255));
    CHECK(rgb(pixel(full + "style='fill:#0f0 !important;'/>"), 0, 255, 0));
    const std::string css = "<style type='text/css'><![CDATA[ /* a comment */ #x { fill: #f00 } .a, .b { fill: #00f; }\n"
                            " rect { fill: #0f0 } g > rect { fill: black } rect.a.wide { fill: #ff0 }\n"
                            " @media print { rect { fill: black } } circle:hover { fill: black } ]]></style>"
                            "<style type='text/x-other'>rect { fill: black }</style>";
    CHECK(rgb(pixel(css + full + "/>"), 0, 255, 0));
    CHECK(rgb(pixel(css + full + "fill='red'/>"), 0, 255, 0));
    CHECK(rgb(pixel(css + full + "class='a'/>"), 0, 0, 255));
    CHECK(rgb(pixel(css + full + "class='c  b'/>"), 0, 0, 255));
    CHECK(rgb(pixel(css + full + "class='wide a'/>"), 255, 255, 0));
    CHECK(rgb(pixel(css + full + "class='a' id='x'/>"), 255, 0, 0));
    CHECK(rgb(pixel(css + full + "class='a' id='x' style='fill:white'/>"), 255, 255, 255));
    CHECK(rgb(pixel(css + "<circle cx='10' cy='10' r='9' class='b'/>"), 0, 0, 255));
    CHECK(rgb(pixel(css + "<circle cx='10' cy='10' r='9'/>"), 0, 0, 0));
    // The fill rules.
    const std::string rings = "<path d='M0 0 H20 V20 H0 Z M5 5 H15 V15 H5 Z' fill='red' ";
    CHECK(rgb(pixel(rings + "/>"), 255, 0, 0));
    CHECK(isWhite(pixel(rings + "fill-rule='evenodd'/>")));
    CHECK(isWhite(pixel("<path d='M0 0 H20 V20 H0 Z M5 5 V15 H15 V5 Z' fill='red'/>"))); // wound the other way
}

static void testGradients() {
    const std::string stops = "<stop offset='0' stop-color='#000'/><stop offset='1' stop-color='#fff'/>";
    const auto row = [](const std::string &defs, const char *fill = "url(#g)") {
        return draw(doc("width='100' height='10'",
                        "<defs>" + defs + "</defs><rect width='100' height='10' fill='" + fill + "'/>"),
                    100, 10);
    };
    const auto gray = [](QRgb p, int level, int tolerance = 4) { return rgb(p, level, level, level, tolerance); };
    // Along the bounding box by default.
    QImage i = row("<linearGradient id='g'>" + stops + "</linearGradient>");
    CHECK(gray(i.pixel(0, 5), 1) && gray(i.pixel(50, 5), 129) && gray(i.pixel(99, 5), 254));
    // Percentages, stop offsets in percent, colors and opacity from the style attribute.
    i = row("<linearGradient id='g' x1='25%' x2='75%'><stop offset='0%' style='stop-color:red'/>"
            "<stop offset='100%' style='stop-color:blue;stop-opacity:0.5'/></linearGradient>");
    CHECK(rgb(i.pixel(10, 5), 255, 0, 0) && rgb(i.pixel(50, 5), 158, 65, 162, 6) && rgb(i.pixel(90, 5), 127, 127, 255, 3));
    // User space, a transformation, and vertical.
    i = row("<linearGradient id='g' gradientUnits='userSpaceOnUse' x1='0' x2='10' "
            "gradientTransform='translate(20) scale(4 1)'>" +
            stops + "</linearGradient>");
    CHECK(gray(i.pixel(10, 5), 0) && gray(i.pixel(40, 5), 131) && gray(i.pixel(70, 5), 255));
    i = row("<linearGradient id='g' x2='0' y2='1'>" + stops + "</linearGradient>");
    CHECK(gray(i.pixel(10, 0), 13) && gray(i.pixel(90, 0), 13) && gray(i.pixel(50, 9), 242));
    // Spreading.
    i = row("<linearGradient id='g' x2='0.25' spreadMethod='repeat'>" + stops + "</linearGradient>");
    CHECK(gray(i.pixel(12, 5), 128, 6) && gray(i.pixel(37, 5), 128, 6) && gray(i.pixel(27, 5), 25, 6));
    i = row("<linearGradient id='g' x2='0.25' spreadMethod='reflect'>" + stops + "</linearGradient>");
    CHECK(gray(i.pixel(27, 5), 230, 6) && gray(i.pixel(49, 5), 5, 6));
    // A gradient that refers to another takes its stops and what it does not set.
    i = row("<linearGradient id='base' x2='0.5' spreadMethod='reflect'>" + stops +
            "</linearGradient><linearGradient id='mid' xlink:href='#base'/><linearGradient id='g' href='#mid'/>");
    CHECK(gray(i.pixel(25, 5), 130) && gray(i.pixel(50, 5), 252) && gray(i.pixel(75, 5), 125));
    // One stop is a color; none is nothing; a missing gradient is the fallback, or nothing.
    i = row("<linearGradient id='g'><stop offset='0.5' stop-color='red'/></linearGradient>");
    CHECK(rgb(i.pixel(5, 5), 255, 0, 0) && rgb(i.pixel(95, 5), 255, 0, 0));
    CHECK(isWhite(row("<linearGradient id='g'/>").pixel(50, 5)));
    CHECK(isWhite(row("", "url(#missing)").pixel(50, 5)));
    CHECK(rgb(row("", "url(#missing) blue").pixel(50, 5), 0, 0, 255));
    CHECK(rgb(row("", "url(  \"#missing\" ) #f00").pixel(50, 5), 255, 0, 0));
    CHECK(isWhite(row("", "url(#missing) none").pixel(50, 5)));
    CHECK(rgb(row("<g color='lime'/>", "url(#missing) currentColor").pixel(50, 5), 0, 0, 0)); // the color is black here
    // Offsets never go back.
    i = row("<linearGradient id='g'><stop offset='0.5' stop-color='red'/><stop offset='0.2' stop-color='blue'/>"
            "<stop offset='7' stop-color='lime'/></linearGradient>");
    CHECK(rgb(i.pixel(40, 5), 255, 0, 0) && rgb(i.pixel(52, 5), 0, 13, 242, 12) && rgb(i.pixel(99, 5), 0, 252, 3, 6));
    // References that go around end.
    i = row("<linearGradient id='g' href='#h'>" + stops + "</linearGradient><linearGradient id='h' href='#g'/>");
    CHECK(gray(i.pixel(50, 5), 129));
    CHECK(isWhite(row("<linearGradient id='g' href='#g'/>").pixel(50, 5)));

    // Radial: from the center out.
    const auto disc = [&](const std::string &attributes) {
        return draw(doc("width='100' height='100'", "<radialGradient id='g' " + attributes + ">" + stops +
                                                        "</radialGradient><rect width='100' height='100' fill='url(#g)'/>"),
                    100, 100);
    };
    i = disc("");
    CHECK(gray(i.pixel(50, 50), 3) && gray(i.pixel(75, 50), 130, 5) && gray(i.pixel(50, 25), 125, 5));
    CHECK(gray(i.pixel(1, 50), 247, 5) && gray(i.pixel(2, 2), 255));
    i = disc("cx='0' cy='0' r='100%'");
    CHECK(gray(i.pixel(0, 0), 2) && gray(i.pixel(50, 0), 129) && gray(i.pixel(30, 40), 129));
    i = disc("gradientUnits='userSpaceOnUse' cx='50' cy='50' r='20' fx='60' fy='50'");
    CHECK(gray(i.pixel(60, 50), 15, 10) && gray(i.pixel(72, 50), 255) && gray(i.pixel(45, 50), 123, 8));
    // A focal point outside the circle is brought to its edge.
    i = disc("fx='5'");
    CHECK(qRed(i.pixel(95, 50)) < 80 && qRed(i.pixel(3, 50)) > 220);
    // A gradient on a stroke, and a bounding box without an area, which paints nothing.
    i = draw(doc("width='100' height='10'", "<linearGradient id='g'>" + stops +
                                                "</linearGradient><path d='M0 5 H100 M0 5 V6' stroke='url(#g)' stroke-width='10'/>"),
             100, 10);
    CHECK(gray(i.pixel(50, 5), 129));
    i = draw(doc("width='100' height='10'", "<linearGradient id='g'>" + stops +
                                                "</linearGradient><line x1='0' y1='5' x2='100' y2='5' stroke='url(#g)' stroke-width='10'/>"),
             100, 10);
    CHECK(isWhite(i.pixel(50, 5)));
}

static void testStrokes() {
    const auto drawn = [](const std::string &body) { return draw(doc("width='40' height='20'", body), 40, 20); };
    const auto black = [](QRgb p) { return rgb(p, 0, 0, 0); };
    // Caps.
    const std::string capped = "<line x1='10' y1='10' x2='30' y2='10' stroke='black' stroke-width='6' ";
    QImage i = drawn(capped + "/>");
    CHECK(isWhite(i.pixel(9, 10)) && black(i.pixel(10, 10)) && black(i.pixel(29, 12)) && isWhite(i.pixel(30, 10)));
    i = drawn(capped + "stroke-linecap='square'/>");
    CHECK(black(i.pixel(7, 7)) && black(i.pixel(32, 12)) && isWhite(i.pixel(6, 10)) && isWhite(i.pixel(33, 10)));
    i = drawn(capped + "stroke-linecap='round'/>");
    CHECK(qRed(i.pixel(7, 10)) < 100 && black(i.pixel(8, 9)) && qRed(i.pixel(7, 7)) > 200 && isWhite(i.pixel(6, 6)) &&
          isWhite(i.pixel(6, 10)));
    // A dot has a cap or nothing.
    CHECK(isWhite(drawn("<path d='M10 10 z' stroke='black' stroke-width='6'/>").pixel(10, 10)));
    CHECK(black(drawn("<path d='M10 10 z' stroke='black' stroke-width='6' stroke-linecap='round'/>").pixel(10, 10)));
    i = drawn("<path d='M10 10 L10 10' stroke='black' stroke-width='6' stroke-linecap='square'/>");
    CHECK(black(i.pixel(7, 7)) && black(i.pixel(12, 12)) && isWhite(i.pixel(13, 10)));
    CHECK(isWhite(drawn("<path d='M10 10' stroke='black' stroke-width='6' stroke-linecap='round'/>").pixel(10, 10)));
    // Joins: a right angle at (30,4).
    const std::string corner = "<path d='M5 4 H30 V18' fill='none' stroke='black' stroke-width='8' ";
    i = drawn(corner + "/>");
    CHECK(black(i.pixel(33, 0)) && black(i.pixel(30, 2)));
    i = drawn(corner + "stroke-linejoin='bevel'/>");
    CHECK(isWhite(i.pixel(33, 0)) && black(i.pixel(30, 2)) && black(i.pixel(30, 4)));
    i = drawn(corner + "stroke-linejoin='round'/>");
    CHECK(isWhite(i.pixel(33, 0)) && black(i.pixel(31, 2)));
    i = drawn(corner + "stroke-miterlimit='1'/>");
    CHECK(isWhite(i.pixel(33, 0))); // the miter is too long for the limit: a bevel
    // A sharp turn back, in both directions, with every join.
    for (const char *join : {"miter", "round", "bevel"}) {
        i = drawn(std::string("<path d='M5 5 H30 L5 9 M35 18 V12 L33 18' fill='none' stroke='black' stroke-width='2' "
                              "stroke-linejoin='") + join + "'/>");
        CHECK(black(i.pixel(20, 4)) && black(i.pixel(29, 5)) && isWhite(i.pixel(20, 12)));
    }
    // A closed path has a join at its start too, and no caps.
    i = drawn("<path d='M10 5 H30 V15 H10 Z' fill='none' stroke='black' stroke-width='4' stroke-linecap='round'/>");
    CHECK(black(i.pixel(8, 3)) && isWhite(i.pixel(7, 10)) && isWhite(i.pixel(20, 10)));
    // Dashes.
    const std::string line = "<line x1='0' y1='10' x2='40' y2='10' stroke='black' stroke-width='4' ";
    i = drawn(line + "stroke-dasharray='10'/>");
    CHECK(black(i.pixel(5, 10)) && isWhite(i.pixel(15, 10)) && black(i.pixel(25, 10)) && isWhite(i.pixel(35, 10)));
    CHECK(black(i.pixel(9, 10)) && isWhite(i.pixel(10, 10)));
    i = drawn(line + "stroke-dasharray='10, 5' stroke-dashoffset='5'/>");
    CHECK(black(i.pixel(2, 10)) && isWhite(i.pixel(7, 10)) && black(i.pixel(12, 10)) && isWhite(i.pixel(22, 10)));
    i = drawn(line + "stroke-dasharray='10 5' stroke-dashoffset='-5'/>");
    CHECK(isWhite(i.pixel(2, 10)) && black(i.pixel(7, 10)) && isWhite(i.pixel(17, 10)));
    i = drawn(line + "stroke-dasharray='4 2 2'/>"); // odd: repeated to 4 2 2 4 2 2
    CHECK(black(i.pixel(2, 10)) && isWhite(i.pixel(5, 10)) && black(i.pixel(7, 10)) && isWhite(i.pixel(9, 10)) &&
          black(i.pixel(13, 10)) && isWhite(i.pixel(15, 10)));
    // Patterns that are not patterns give a solid line.
    for (const char *pattern : {"none", "0", "0 0", "5 -1", "x"}) {
        i = drawn(line + "stroke-dasharray='" + pattern + "'/>");
        CHECK(black(i.pixel(15, 10)) && black(i.pixel(7, 10)));
    }
    // Dashes with caps, around a closed shape, and a pattern far too fine to draw, which still ends.
    i = drawn(line + "stroke-dasharray='0 10' stroke-linecap='round'/>");
    CHECK(black(i.pixel(10, 10)) && black(i.pixel(20, 9)) && isWhite(i.pixel(15, 10)));
    i = drawn("<rect x='5' y='5' width='30' height='10' fill='none' stroke='black' stroke-width='2' stroke-dasharray='5'/>");
    CHECK(black(i.pixel(7, 5)) && isWhite(i.pixel(12, 5)) && black(i.pixel(17, 5)));
    i = drawn(line + "stroke-dasharray='0.0000001'/>");
    CHECK(i.width() == 40);
    // No width, no stroke; the width scales with the transformation.
    CHECK(isWhite(drawn("<line x1='0' y1='10' x2='40' y2='10' stroke='black' stroke-width='0'/>").pixel(20, 10)));
    i = drawn("<g transform='scale(1 4)'><line x1='0' y1='2.5' x2='40' y2='2.5' stroke='black' stroke-width='1'/></g>");
    CHECK(black(i.pixel(20, 8)) && black(i.pixel(20, 11)) && isWhite(i.pixel(20, 7)) && isWhite(i.pixel(20, 12)));
    // A curve is stroked evenly: a ring is as wide all the way around.
    i = draw(doc("width='40' height='40'", "<circle cx='20' cy='20' r='14' fill='none' stroke='black' stroke-width='6'/>"), 40, 40);
    CHECK(black(i.pixel(20, 4)) && black(i.pixel(34, 20)) && black(i.pixel(10, 10)) && black(i.pixel(29, 29)));
    CHECK(isWhite(i.pixel(20, 20)) && isWhite(i.pixel(20, 10)) && isWhite(i.pixel(20, 1)) && isWhite(i.pixel(13, 13)) && isWhite(i.pixel(7, 7)));
}

static void testStructure() {
    const auto drawn = [](const std::string &body) { return draw(doc("width='40' height='40'", body), 40, 40); };
    const auto black = [](QRgb p) { return rgb(p, 0, 0, 0); };
    // Transformations.
    QImage i = drawn("<rect width='5' height='5' transform='translate(10 10) scale(2)'/>");
    CHECK(black(i.pixel(10, 10)) && black(i.pixel(19, 19)) && isWhite(i.pixel(9, 15)) && isWhite(i.pixel(20, 15)));
    i = drawn("<rect width='10' height='4' transform='rotate(90 10 10)'/>");
    CHECK(black(i.pixel(18, 5)) && black(i.pixel(16, 9)) && isWhite(i.pixel(5, 2)) && isWhite(i.pixel(15, 5)));
    i = drawn("<rect width='10' height='10' transform='matrix(1 0 0 1 20 20)'/>");
    CHECK(black(i.pixel(25, 25)) && isWhite(i.pixel(15, 15)));
    i = drawn("<rect width='10' height='10' transform='skewX(45)'/>");
    CHECK(black(i.pixel(14, 8)) && isWhite(i.pixel(2, 8)) && black(i.pixel(2, 0)));
    i = drawn("<rect width='10' height='10' transform='skewY(45), translate(5)'/>");
    CHECK(black(i.pixel(10, 15)) && isWhite(i.pixel(10, 4)));
    i = drawn("<g transform='translate(10,0)'><g transform='scale(2)'><rect x='5' width='5' height='5'/></g></g>");
    CHECK(black(i.pixel(25, 5)) && isWhite(i.pixel(15, 5)));
    // A list with an error stops there; a transformation to nothing draws nothing.
    i = drawn("<rect width='10' height='10' transform='translate(20) bogus(1) translate(0 20)'/>");
    CHECK(black(i.pixel(25, 5)) && isWhite(i.pixel(25, 25)));
    CHECK(isWhite(drawn("<rect width='10' height='10' transform='scale(0)'/>").pixel(0, 0)));

    // Definitions are drawn where they are used, with what they inherit there.
    i = drawn("<defs><rect id='r' width='5' height='5' fill='green'/><g id='pair'><rect id='q' width='4' height='4'/>"
              "<rect x='6' width='4' height='4' fill='red'/></g></defs>"
              "<use xlink:href='#r' x='10' y='10'/><use href='#pair' transform='translate(20 20)' fill='blue'/>"
              "<use href='#q' y='30' fill='red' opacity='0.5'/><use href='#missing'/><use/>");
    CHECK(isWhite(i.pixel(2, 2)) && rgb(i.pixel(12, 12), 0, 128, 0));
    CHECK(rgb(i.pixel(22, 22), 0, 0, 255) && rgb(i.pixel(28, 22), 255, 0, 0));
    CHECK(rgb(i.pixel(2, 32), 255, 127, 127, 3));
    // A symbol is not drawn by itself; what is drawn anyway can be used again.
    i = drawn("<symbol id='s'><circle cx='5' cy='5' r='5'/></symbol><rect id='r' x='20' width='5' height='5'/>"
              "<use href='#s' x='10' y='10'/><use href='#r' y='20'/>");
    CHECK(isWhite(i.pixel(5, 5)) && black(i.pixel(15, 15)) && black(i.pixel(22, 2)) && black(i.pixel(22, 22)));
    // A nested document fits its view box into its viewport, centered.
    i = drawn("<svg x='10' y='10' width='20' height='10' viewBox='0 0 2 2'><rect width='2' height='2' fill='red'/></svg>");
    CHECK(rgb(i.pixel(20, 15), 255, 0, 0) && rgb(i.pixel(16, 11), 255, 0, 0) && isWhite(i.pixel(13, 15)) &&
          isWhite(i.pixel(26, 15)) && isWhite(i.pixel(20, 21)));
    // Links are groups; elements that are not known are not drawn, nor what is in them.
    i = drawn("<a href='https://example.org'><rect width='5' height='5'/></a><switch><rect x='10' width='5' height='5'/></switch>"
              "<mask><rect x='20' width='5' height='5'/></mask><foo><rect x='20' width='5' height='5'/></foo>"
              "<title>t</title><metadata><x:y xmlns:x='x'/></metadata><svg:rect xmlns:svg='http://www.w3.org/2000/svg' "
              "y='10' width='5' height='5'/>");
    CHECK(black(i.pixel(2, 2)) && black(i.pixel(12, 2)) && isWhite(i.pixel(22, 2)) && black(i.pixel(2, 12)));

    // Clipping.
    i = drawn("<clipPath id='c'><circle cx='20' cy='20' r='10'/></clipPath>"
              "<rect width='40' height='40' fill='red' clip-path='url(#c)'/>");
    CHECK(rgb(i.pixel(20, 20), 255, 0, 0) && rgb(i.pixel(12, 20), 255, 0, 0) && isWhite(i.pixel(5, 5)) &&
          isWhite(i.pixel(11, 11)) && isWhite(i.pixel(9, 20)));
    i = drawn("<clipPath id='c' clipPathUnits='objectBoundingBox'><rect width='0.5' height='1'/></clipPath>"
              "<g clip-path='url(#c)' opacity='0.5'><rect x='10' y='10' width='20' height='20' fill='red'/></g>");
    CHECK(rgb(i.pixel(15, 20), 255, 127, 127, 3) && isWhite(i.pixel(25, 20)));
    i = drawn("<clipPath id='c' transform='translate(20)'><rect width='10' height='40' transform='translate(5)'/>"
              "<use href='#r'/></clipPath><defs><rect id='r' y='30' width='20' height='10'/></defs>"
              "<g transform='translate(0 -5)' clip-path='url(#c)'><rect width='40' height='45' fill='red'/></g>");
    CHECK(rgb(i.pixel(30, 10), 255, 0, 0) && isWhite(i.pixel(22, 10)) && isWhite(i.pixel(36, 10)) && rgb(i.pixel(22, 30), 255, 0, 0));
    i = drawn("<clipPath id='c'><path d='M0 0 H40 V40 H0 Z M10 10 H30 V30 H10 Z' clip-rule='evenodd'/></clipPath>"
              "<rect width='40' height='40' clip-path='url(#c)'/>");
    CHECK(black(i.pixel(5, 5)) && isWhite(i.pixel(20, 20)));
    // An empty clip path hides everything; a missing one is ignored.
    CHECK(isWhite(drawn("<clipPath id='c'/><rect width='40' height='40' clip-path='url(#c)'/>").pixel(20, 20)));
    CHECK(black(drawn("<rect width='40' height='40' clip-path='url(#nope)'/>").pixel(20, 20)));
}

static void testText() {
    QFont font;
    font.setPixelSize(20);
    if (QFontMetrics(font).horizontalAdvance(QString("XX")) <= 0) {
        std::puts("no fonts: text is not tested");
        return;
    }
    const auto ink = [](const QImage &image, int x0, int x1) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = x0; x < x1; ++x)
                count += qGreen(image.pixel(x, y)) < 128 ? 1 : 0;
        }
        return count;
    };
    const auto drawn = [](const std::string &body) { return draw(doc("width='120' height='40'", body), 120, 40); };
    QImage i = drawn("<text x='10' y='30' font-size='24'>HH</text>");
    CHECK(ink(i, 0, 120) > 40 && ink(i, 0, 10) == 0);
    int below = 0;
    for (int x = 0; x < 120; ++x)
        below += qGreen(i.pixel(x, 35)) < 128 ? 1 : 0;
    CHECK(below == 0); // nothing below the baseline
    // The anchor moves the text; the scale of the device enlarges it.
    const QImage end = drawn("<text x='110' y='30' font-size='24' text-anchor='end'>HH</text>");
    CHECK(ink(end, 110, 120) == 0 && ink(end, 60, 110) > 40 && ink(end, 0, 20) == 0);
    const QImage middle = drawn("<text x='60' y='30' font-size='24' style='text-anchor:middle'>HH</text>");
    CHECK(ink(middle, 30, 60) > 20 && ink(middle, 60, 90) > 20 && std::abs(ink(middle, 30, 60) - ink(middle, 60, 90)) < 30);
    const QImage big = draw(doc("width='120' height='40'", "<text x='10' y='30' font-size='24'>HH</text>"), 240, 80);
    CHECK(ink(big, 0, 240) > 3 * ink(i, 0, 120));
    // Runs, colors, positions of their own, and white space.
    i = drawn("<text x='5' y='15' font-size='12' fill='blue'>  a\n  <tspan fill='red' font-weight='bold'>HH</tspan>"
              "<tspan x='5' y='35' font-style='italic'>HH</tspan> </text>");
    bool red = false, blue = false, lower = false;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 120; ++x) {
            const QRgb p = i.pixel(x, y);
            red = red || (qRed(p) > 200 && qBlue(p) < 80);
            blue = blue || (qBlue(p) > 200 && qRed(p) < 80);
            lower = lower || (y > 22 && x < 40 && qBlue(p) > 200 && qRed(p) < 80);
        }
    }
    CHECK(red && blue && lower);
    // Text has bounds, and nothing is drawn of text that is hidden or has no fill.
    QSvgRenderer r(doc("width='120' height='40'", "<text id='t' x='10' y='30' font-size='24'>HH</text><text id='e'></text>"));
    const QRectF bounds = r.boundsOnElement("t");
    CHECK(bounds.x() == 10.0 && bounds.width() > 15.0 && bounds.y() < 15.0 && bounds.y() + bounds.height() >= 30.0);
    CHECK(r.boundsOnElement("e").isNull());
    CHECK(ink(drawn("<text x='10' y='30' fill='none'>HH</text>"), 0, 120) == 0);
    CHECK(ink(drawn("<g visibility='hidden'><text x='10' y='30'>HH</text></g>"), 0, 120) == 0);
    CHECK(ink(drawn("<text x='10' y='30' opacity='0'>HH</text>"), 0, 120) == 0);
    CHECK(ink(drawn("<text x='10' y='30'><tspan display='none'>HH</tspan></text>"), 0, 120) == 0);
}

static void testRenderBounds() {
    const QByteArray red = doc("viewBox='0 0 10 10'", "<rect width='10' height='10' fill='red'/>");
    QSvgRenderer r(red);
    // Stretched by default.
    QImage i = draw(r, 40, 20);
    CHECK(rgb(i.pixel(0, 0), 255, 0, 0) && rgb(i.pixel(39, 19), 255, 0, 0));
    r.setAspectRatioMode(Qt::KeepAspectRatio);
    CHECK(r.aspectRatioMode() == Qt::KeepAspectRatio);
    i = draw(r, 40, 20);
    CHECK(isWhite(i.pixel(9, 10)) && rgb(i.pixel(10, 0), 255, 0, 0) && rgb(i.pixel(29, 19), 255, 0, 0) && isWhite(i.pixel(30, 10)));
    r.setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    i = draw(r, 40, 20);
    CHECK(rgb(i.pixel(0, 0), 255, 0, 0) && rgb(i.pixel(39, 19), 255, 0, 0));
    // Into a rectangle: nothing is drawn outside it, even when expanding.
    QImage image(40, 40, QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        r.render(&painter, QRectF(10, 10, 20, 10));
        r.render(&painter, QRectF());
        r.render(&painter, QRectF(0, 0, -5, 5));
    }
    CHECK(rgb(image.pixel(10, 10), 255, 0, 0) && rgb(image.pixel(29, 19), 255, 0, 0));
    CHECK(isWhite(image.pixel(9, 15)) && isWhite(image.pixel(30, 15)) && isWhite(image.pixel(20, 9)) && isWhite(image.pixel(20, 20)));
    // Bounds between pixels are antialiased, and the translation of the painter applies.
    r.setAspectRatioMode(Qt::IgnoreAspectRatio);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.translate(10, 0);
        r.render(&painter, QRectF(2.5, 2.5, 5, 5));
    }
    CHECK(rgb(image.pixel(12, 5), 255, 127, 127, 3) && rgb(image.pixel(13, 5), 255, 0, 0) && rgb(image.pixel(17, 5), 255, 127, 127, 3));
    CHECK(rgb(image.pixel(12, 2), 255, 191, 191, 3) && isWhite(image.pixel(11, 5)) && isWhite(image.pixel(3, 5)));
    // The view box can be changed, and a new document resets it.
    QSvgRenderer quarters(doc("viewBox='0 0 10 10'", "<rect width='5' height='5' fill='red'/><rect x='5' y='5' width='5' height='5' fill='blue'/>"));
    quarters.setViewBox(QRectF(5, 5, 5, 5));
    CHECK(quarters.viewBoxF() == QRectF(5, 5, 5, 5));
    CHECK(rgb(draw(quarters, 10, 10).pixel(2, 2), 0, 0, 255));
    quarters.setViewBox(QRect(0, 0, 5, 5));
    CHECK(rgb(draw(quarters, 10, 10).pixel(8, 8), 255, 0, 0));
    quarters.setViewBox(QRectF());
    CHECK(isWhite(draw(quarters, 10, 10).pixel(2, 2))); // an empty view box shows nothing
    CHECK(quarters.load(red) && quarters.viewBoxF() == QRectF(0, 0, 10, 10));
    // On a transparent image the result keeps its alpha.
    QImage clear(10, 10, QImage::Format_ARGB32_Premultiplied);
    clear.fill(Qt::transparent);
    {
        QSvgRenderer half(doc("viewBox='0 0 10 10'", "<rect width='5' height='10' fill='red' fill-opacity='0.5'/>"));
        QPainter painter(&clear);
        half.render(&painter);
    }
    CHECK(qAlpha(clear.pixel(8, 5)) == 0 && std::abs(qAlpha(clear.pixel(2, 5)) - 128) <= 2);
    // A size that makes no sense draws nothing, and is not allocated either.
    QImage tiny(4, 4, QImage::Format_RGB32);
    tiny.fill(Qt::white);
    {
        QPainter painter(&tiny);
        r.render(&painter, QRectF(0, 0, 1e9, 1e9));
        r.render(&painter, QRectF(0, 0, 40000, 2));
        r.render(&painter, QRectF(1e300, 1e300, 1e300, 1e300));
    }
    CHECK(isWhite(tiny.pixel(1, 1)));
}

static void testElements() {
    QSvgRenderer r(doc("width='200' height='100'",
                       "<g id='grp' transform='translate(100,0)'><rect id='a' x='10' y='20' width='30' height='40' fill='red'/>"
                       "<rect id='s' x='10' y='20' width='30' height='40' stroke='black' stroke-width='2'/>"
                       "<circle id='c' cx='20' cy='20' r='10' transform='scale(2)'/></g>"
                       "<defs><rect id='d' width='8' height='8' fill='blue'/></defs><g id='empty'/>"
                       "<g fill='lime'><rect id='inherits' width='5' height='5'/></g><rect id='a' width='1' height='1'/>"));
    CHECK(r.elementExists("a") && r.elementExists("grp") && r.elementExists("d") && r.elementExists("empty"));
    CHECK(!r.elementExists("nope") && !r.elementExists(""));
    CHECK(rectIs(r.boundsOnElement("a"), 10, 20, 30, 40)); // the first of two with the same id
    CHECK(rectIs(r.boundsOnElement("s"), 9, 19, 32, 42));
    CHECK(rectIs(r.boundsOnElement("c"), 20, 20, 40, 40));
    CHECK(rectIs(r.boundsOnElement("grp"), 109, 19, 51, 42));
    CHECK(rectIs(r.boundsOnElement("d"), 0, 0, 8, 8));
    CHECK(r.boundsOnElement("empty").isNull() && r.boundsOnElement("nope").isNull());

    QImage image(40, 40, QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        r.render(&painter, "a", QRectF(0, 0, 10, 20));
        r.render(&painter, "d", QRectF(20, 0, 20, 10));
        r.render(&painter, "inherits", QRectF(0, 30, 10, 10));
        r.render(&painter, "nope", QRectF(20, 20, 10, 10));
        r.render(&painter, "empty", QRectF(20, 20, 10, 10));
    }
    CHECK(rgb(image.pixel(0, 0), 255, 0, 0) && rgb(image.pixel(9, 19), 255, 0, 0) && isWhite(image.pixel(10, 10)) && isWhite(image.pixel(5, 20)));
    CHECK(rgb(image.pixel(20, 0), 0, 0, 255) && rgb(image.pixel(39, 9), 0, 0, 255) && isWhite(image.pixel(30, 10)));
    CHECK(rgb(image.pixel(5, 35), 0, 255, 0));
    CHECK(isWhite(image.pixel(25, 25)));
    // Without bounds, the element fills the device.
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        r.render(&painter, "a");
    }
    CHECK(rgb(image.pixel(0, 0), 255, 0, 0) && rgb(image.pixel(39, 39), 255, 0, 0));
}

static void testRobustness() {
    QImage image(20, 20, QImage::Format_RGB32);
    const auto survive = [&](const QByteArray &svg) {
        QSvgRenderer r(svg);
        image.fill(Qt::white);
        QPainter painter(&image);
        r.render(&painter);
        (void)r.boundsOnElement("a");
        return r.isValid();
    };
    // References to themselves and to each other.
    CHECK(survive(doc("", "<use id='a' href='#a'/>")));
    CHECK(survive(doc("", "<g id='a'><use href='#b'/></g><g id='b'><use href='#a'/><rect width='5' height='5'/></g>")));
    CHECK(survive(doc("", "<clipPath id='a'><use href='#a'/><rect width='5' height='5' clip-path='url(#a)'/></clipPath>"
                          "<rect width='20' height='20' clip-path='url(#a)'/>")));
    // A tree of references to a million groups stops at the limit, so what follows it is not
    // drawn; a trusted document is drawn whatever it takes.
    std::string bomb = "<g id='l0'/>";
    for (int level = 1; level <= 6; ++level) {
        bomb += "<g id='l" + std::to_string(level) + "'>";
        for (int n = 0; n < 10; ++n)
            bomb += "<use href='#l" + std::to_string(level - 1) + "'/>";
        bomb += "</g>";
    }
    bomb += "<rect width='20' height='20' fill='red'/>";
    CHECK(survive(doc("width='20' height='20'", bomb)));
    CHECK(isWhite(image.pixel(10, 10)));
    {
        QSvgRenderer trusted(doc("width='20' height='20'", bomb));
        trusted.setOptions(QtSvg::AssumeTrustedSource);
        CHECK(rgb(draw(trusted, 20, 20).pixel(10, 10), 255, 0, 0));
        trusted.setOptions(QtSvg::NoOption);
        CHECK(isWhite(draw(trusted, 20, 20).pixel(10, 10)));
    }
    // Groups nested deeply, with and without opacity: within the limit of the reader, past that of the tree.
    std::string nested, close;
    for (int n = 0; n < 150; ++n) {
        nested += n % 2 ? "<g opacity='0.9'>" : "<g>";
        close += "</g>";
    }
    CHECK(survive(doc("width='20' height='20'", nested + "<rect width='20' height='20'/>" + close)));
    // Numbers without sense.
    CHECK(survive(doc("width='1e300' height='1e300' viewBox='0 0 1e-300 1e300'", "<rect width='1e300' height='1e300'/>")));
    CHECK(survive(doc("viewBox='0 0 20 20'", "<path id='a' d='M-1e30 -1e30 L1e30 1e30 L1e30 -1e30 Z A1e308 1e308 0 0 0 5 5'/>")));
    CHECK(survive(doc("viewBox='0 0 20 20'", "<circle id='a' cx='10' cy='10' r='1e20' stroke='red' stroke-width='1e20' "
                                             "stroke-dasharray='1e-20 1e20'/>")));
    CHECK(survive(doc("viewBox='0 0 20 20'", "<rect width='20' height='20' transform='matrix(1e200 1e200 1e200 1e200 0 0) "
                                             "scale(1e200) rotate(1e30) skewX(90)'/>")));
    CHECK(survive(doc("viewBox='0 0 20 20'", "<linearGradient id='g' x1='1e300' x2='-1e300' gradientTransform='scale(1e-200)'>"
                                             "<stop/><stop offset='1'/></linearGradient><rect id='a' width='20' height='20' fill='url(#g)'/>")));
    CHECK(survive(doc("viewBox='0 0 20 20'", "<text id='a' x='1e300' y='nan' font-size='1e300' transform='scale(1e100)'>x</text>")));
    CHECK(survive(doc("viewBox='0 0 20 20'", "<svg width='1e-300' height='0' viewBox='0 0 1 1'><rect width='1' height='1'/></svg>")));
    // Style sheets and attributes that end early.
    CHECK(survive(doc("", "<style>rect { fill: red</style><style>/* open</style><style>}{}{{</style>"
                          "<rect width='5' height='5' style=':;;:;fill' transform='translate(' fill='url(' class=' '/>")));
    // Every prefix of a document either fails to load or draws.
    const QByteArray whole = doc("width='20' height='20'",
                                 "<defs><linearGradient id='g'><stop offset='0'/></linearGradient></defs>"
                                 "<path d='M0 0 C1 2 3 4 5 6 A5 5 0 0 1 9 9z' fill='url(#g)' stroke-dasharray='1 2'/>"
                                 "<text x='1' y='9'>a<tspan dx='1'>b</tspan></text>");
    for (qsizetype n = 0; n < whole.size(); ++n)
        (void)survive(whole.left(n));
    CHECK(survive(whole));
}

static QByteArray generate(QSvgGenerator &generator, void (*paint)(QPainter &)) {
    QBuffer buffer;
    generator.setOutputDevice(&buffer);
    {
        QPainter painter(&generator);
        CHECK(painter.isActive());
        paint(painter);
    }
    CHECK(!buffer.isOpen()); // opened by the generator, so closed by it
    const QByteArray out = buffer.data();
    generator.setOutputDevice(nullptr);
    return out;
}

static void paintScene(QPainter &painter) {
    painter.fillRect(0, 0, 60, 40, QColor(240, 240, 200));
    painter.setPen(QPen(QColor(0, 0, 160), 3));
    painter.setBrush(QColor(200, 60, 60));
    painter.drawRect(5, 5, 20, 12);
    painter.setPen(QColor(0, 120, 0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(30, 5, 10, 10);
    painter.drawLine(2, 30, 40, 30);
    painter.drawLine(50, 2, 50, 38);
    painter.drawPoint(45, 20);
    painter.setPen(QPen(QColor(0, 0, 0), 2));
    painter.drawLine(4, 34, 30, 34);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(90, 90, 250));
    painter.drawRect(42, 24, 6, 6);
    painter.setOpacity(0.5);
    painter.fillRect(52, 30, 8, 8, QColor(255, 0, 0));
    painter.setOpacity(1.0);
    painter.translate(1, 1);
    painter.fillRect(54, 0, 4, 4, QColor(0, 0, 0, 128));
}

static void testGenerator() {
    QSvgGenerator generator;
    CHECK(generator.svgVersion() == QSvgGenerator::SvgVersion::SvgTiny12);
    CHECK(generator.resolution() == 72 && !generator.size().isValid() && generator.viewBoxF().isNull());
    CHECK(generator.title().isEmpty() && generator.description().isEmpty() && generator.fileName().isEmpty());
    CHECK(generator.outputDevice() == nullptr);
    // Nowhere to write to: painting does not begin.
    {
        QPainter painter;
        CHECK(!painter.begin(&generator));
    }
    generator.setFileName("/nonexistent-directory/out.svg");
    {
        QPainter painter;
        CHECK(!painter.begin(&generator));
    }
    generator.setSize(QSize(60, 40));
    generator.setTitle("A <title> & more");
    generator.setDescription("\"desc\"");
    generator.setResolution(96);
    generator.setResolution(0);
    CHECK(generator.title() == "A <title> & more" && generator.description() == "\"desc\"");
    CHECK(generator.resolution() == 96 && generator.size() == QSize(60, 40));
    CHECK(generator.width() == 60 && generator.height() == 40 && generator.logicalDpiX() == 96 && generator.logicalDpiY() == 96);
    CHECK(generator.widthMM() == 16 && generator.heightMM() == 11 && generator.depth() == 32);
    CHECK(generator.paintEngine() && generator.paintEngine()->type() == QPaintEngine::User);

    const QByteArray svg = generate(generator, paintScene);
    CHECK(svg.startsWith("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n<svg width=\"15.875mm\" height=\"10.583mm\" viewBox=\"0 0 60 40\""));
    CHECK(svg.contains("version=\"1.2\" baseProfile=\"tiny\""));
    CHECK(svg.contains("<title>A &lt;title&gt; &amp; more</title>") && svg.contains("<desc>&quot;desc&quot;</desc>"));
    CHECK(svg.contains("<rect x=\"0\" y=\"0\" width=\"60\" height=\"40\" fill=\"#f0f0c8\"/>"));
    CHECK(svg.contains("<rect x=\"5.5\" y=\"5.5\" width=\"20\" height=\"12\" fill=\"none\" stroke=\"#0000a0\" stroke-width=\"3\"/>"));
    CHECK(svg.contains("<line x1=\"2.5\" y1=\"30.5\" x2=\"40.5\" y2=\"30.5\" stroke=\"#007800\" stroke-width=\"1\" stroke-linecap=\"square\"/>"));
    CHECK(svg.contains("fill=\"#ff0000\" fill-opacity=\"0.5\"") && svg.contains("fill=\"#000000\" fill-opacity=\"0.502\""));
    CHECK(svg.contains("<rect x=\"55\" y=\"1\" width=\"4\" height=\"4\"")); // the translation of the painter
    CHECK(svg.endsWith("</svg>\n") && !svg.contains("clipPath"));

    // What the generator writes, the renderer draws as the raster engine painted it.
    QImage painted(60, 40, QImage::Format_RGB32);
    painted.fill(Qt::white);
    {
        QPainter painter(&painted);
        paintScene(painter);
    }
    QSvgRenderer renderer(svg);
    CHECK(renderer.isValid() && renderer.viewBoxF() == QRectF(0, 0, 60, 40) && renderer.defaultSize() == QSize(60, 40));
    const QImage rendered = draw(renderer, 60, 40);
    int different = 0;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 60; ++x) {
            const QRgb a = painted.pixel(x, y), b = rendered.pixel(x, y);
            if (std::abs(qRed(a) - qRed(b)) > 2 || std::abs(qGreen(a) - qGreen(b)) > 2 || std::abs(qBlue(a) - qBlue(b)) > 2) {
                if (++different < 5)
                    std::printf("  (%d,%d) painted %08x rendered %08x\n", x, y, a, b);
            }
        }
    }
    CHECK(different == 0);

    // Ellipses lie inside their rectangle, outline and all.
    QSvgGenerator shapes;
    shapes.setSize(QSize(40, 20));
    const QByteArray ellipses = generate(shapes, [](QPainter &painter) {
        painter.setPen(QPen(QColor(0, 0, 0), 2));
        painter.setBrush(QColor(255, 0, 0));
        painter.drawEllipse(0, 0, 20, 20);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(20, 5, 20, 10);
        painter.setPen(QPen(QColor(0, 0, 255), 6));
        painter.drawEllipse(0, 0, 4, 4);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(0, 0, 40, 20);
    });
    CHECK(ellipses.contains("<ellipse cx=\"10\" cy=\"10\" rx=\"9\" ry=\"9\" fill=\"#ff0000\" stroke=\"#000000\" stroke-width=\"2\"/>"));
    CHECK(ellipses.contains("<ellipse cx=\"30\" cy=\"10\" rx=\"10\" ry=\"5\" fill=\"#ff0000\"/>"));
    CHECK(ellipses.contains("<ellipse cx=\"2\" cy=\"2\" rx=\"2\" ry=\"2\" fill=\"#0000ff\"/>"));
    CHECK(occurrences(ellipses, "<ellipse") == 3);
    const QImage round = draw(ellipses, 40, 20);
    CHECK(rgb(round.pixel(10, 10), 255, 0, 0) && rgb(round.pixel(10, 1), 0, 0, 0, 30) && isWhite(round.pixel(19, 1)));

    // Images and text are embedded as PNG, and SVG 1.1 clips.
    QSvgGenerator pictures(QSvgGenerator::SvgVersion::Svg11);
    CHECK(pictures.svgVersion() == QSvgGenerator::SvgVersion::Svg11);
    pictures.setSize(QSize(80, 40));
    pictures.setViewBox(QRectF(0, 0, 40, 20));
    CHECK(pictures.viewBox() == QRect(0, 0, 40, 20) && pictures.viewBoxF() == QRectF(0, 0, 40, 20));
    pictures.setViewBox(QRect(0, 0, 80, 40));
    const QByteArray withImages = generate(pictures, [](QPainter &painter) {
        QImage image(2, 2, QImage::Format_ARGB32);
        image.setPixel(0, 0, qRgba(255, 0, 0, 255));
        image.setPixel(1, 0, qRgba(0, 255, 0, 128));
        image.setPixel(0, 1, qRgba(0, 0, 255, 255));
        image.setPixel(1, 1, qRgba(0, 0, 0, 0));
        painter.drawImage(QRect(4, 4, 8, 8), image);
        painter.setOpacity(0.25);
        painter.drawImage(QPoint(20, 4), image, QRect(1, 0, 1, 2));
        painter.drawImage(QPoint(0, 0), QImage());
        painter.setOpacity(1.0);
        painter.setPen(QColor(10, 20, 30));
        painter.drawText(QPoint(4, 30), "Text");
        painter.setClipRect(QRect(2, 2, 10, 10));
        painter.fillRect(0, 0, 80, 40, QColor(1, 2, 3));
        painter.fillRect(0, 0, 5, 5, QColor(1, 2, 3));
        painter.setClipRegion(QRegion(QRect(0, 0, 4, 4)).united(QRect(10, 10, 4, 4)));
        painter.fillRect(0, 0, 80, 40, QColor(4, 5, 6));
        painter.setClipping(false);
        painter.fillRect(70, 30, 10, 10, QColor(7, 8, 9));
    });
    CHECK(withImages.contains("version=\"1.1\"") && withImages.contains("viewBox=\"0 0 80 40\""));
    // The 2 by 2 image: the PNG signature and header, in base64.
    CHECK(withImages.contains("<image x=\"4\" y=\"4\" width=\"8\" height=\"8\" preserveAspectRatio=\"none\" "
                              "xlink:href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0k"));
    CHECK(withImages.contains("<image x=\"20\" y=\"4\" width=\"1\" height=\"2\" preserveAspectRatio=\"none\" opacity=\"0.25\""));
    QFont font;
    const bool fonts = QFontMetrics(font).horizontalAdvance(QString("Text")) > 0;
    CHECK(fonts ? occurrences(withImages, "<image") >= 3 : occurrences(withImages, "<image") == 2);
    CHECK(withImages.contains("<clipPath id=\"clip1\"><rect x=\"2\" y=\"2\" width=\"10\" height=\"10\"/></clipPath>\n<g clip-path=\"url(#clip1)\">"));
    CHECK(occurrences(withImages, "<clipPath") == 2 && occurrences(withImages, "</g>") == 2 && occurrences(withImages, "<g ") == 2);
    CHECK(withImages.indexOf("fill=\"#070809\"") > withImages.lastIndexOf("</g>"));
    const QImage clipped = draw(withImages, 80, 40);
    CHECK(rgb(clipped.pixel(1, 1), 4, 5, 6) && rgb(clipped.pixel(8, 8), 1, 2, 3) && rgb(clipped.pixel(11, 11), 4, 5, 6));
    CHECK(isWhite(clipped.pixel(14, 14)) && isWhite(clipped.pixel(40, 20)) && rgb(clipped.pixel(75, 35), 7, 8, 9));
    // SVG Tiny has no clipping.
    QSvgGenerator tinyClip;
    const QByteArray unclipped = generate(tinyClip, [](QPainter &painter) {
        painter.setClipRect(QRect(2, 2, 10, 10));
        painter.fillRect(0, 0, 5, 5, QColor(1, 2, 3));
    });
    CHECK(!unclipped.contains("clip") && unclipped.contains("<rect"));

    // To a file, and to a device that is already open, which stays open.
    const QString path = QDir::tempPath() + "/nt_svg_test_out.svg";
    QSvgGenerator toFile;
    toFile.setFileName(path);
    CHECK(toFile.fileName() == path);
    {
        QPainter painter(&toFile);
        CHECK(painter.isActive());
        toFile.setFileName("ignored-while-painting.svg");
        toFile.setOutputDevice(nullptr);
        painter.fillRect(0, 0, 3, 3, Qt::red);
    }
    CHECK(toFile.fileName() == path);
    QSvgRenderer fromFile(path);
    CHECK(fromFile.isValid());
    CHECK(fromFile.viewBoxF() == QRectF(0, 0, 3, 3)); // no size: the bounds of what was painted
    QFile::remove(path);
    QBuffer open;
    CHECK(open.open(QIODevice::WriteOnly));
    QSvgGenerator toOpen;
    toOpen.setOutputDevice(&open);
    CHECK(toOpen.outputDevice() == &open);
    {
        QPainter painter(&toOpen);
        CHECK(painter.isActive());
    }
    CHECK(open.isOpen() && open.data().endsWith(" version=\"1.2\" baseProfile=\"tiny\">\n</svg>\n"));
    open.close();
    CHECK(open.open(QIODevice::ReadOnly));
    {
        QPainter painter;
        CHECK(!painter.begin(&toOpen)); // not writable
    }
}

static void testWidget() {
    QSvgWidget widget;
    CHECK(widget.renderer() && !widget.renderer()->isValid());
    CHECK(widget.sizeHint() == QSize(128, 64));
    CHECK(widget.renderer()->parent() == &widget);
    widget.setOptions(QtSvg::Tiny12FeaturesOnly);
    CHECK(widget.options() == QtSvg::Options(QtSvg::Tiny12FeaturesOnly));
    CHECK(widget.renderer()->options() == QtSvg::Options(QtSvg::Tiny12FeaturesOnly));
    widget.load(doc("width='30' height='20'", "<rect width='15' height='20' fill='red'/><rect x='15' width='15' height='20' fill='blue'/>"));
    CHECK(widget.renderer()->isValid() && widget.sizeHint() == QSize(30, 20));
    widget.resize(60, 40);
    QImage image(60, 40, QImage::Format_RGB32);
    image.fill(Qt::white);
    widget.render(&image);
    CHECK(rgb(image.pixel(5, 5), 255, 0, 0) && rgb(image.pixel(29, 39), 255, 0, 0) && rgb(image.pixel(30, 0), 0, 0, 255) &&
          rgb(image.pixel(59, 39), 0, 0, 255));
    widget.renderer()->setAspectRatioMode(Qt::KeepAspectRatio);
    widget.resize(60, 80);
    QImage tall(60, 80, QImage::Format_RGB32);
    tall.fill(Qt::white);
    widget.render(&tall);
    const QRgb background = tall.pixel(5, 5);
    CHECK(qRed(background) == qBlue(background)); // the widget's own background, not the document
    CHECK(rgb(tall.pixel(5, 40), 255, 0, 0) && rgb(tall.pixel(55, 40), 0, 0, 255) && tall.pixel(55, 75) == background);
    widget.load(QByteArray("broken"));
    CHECK(!widget.renderer()->isValid() && widget.sizeHint() == QSize(128, 64));
    widget.render(&tall);
    CHECK(tall.pixel(5, 40) == background);

    const QString path = QDir::tempPath() + "/nt_svg_test_widget.svg";
    {
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(doc("width='7' height='9'", ""));
    }
    QWidget parent;
    auto *child = new QSvgWidget(path, &parent);
    CHECK(child->parentWidget() == &parent && child->sizeHint() == QSize(7, 9));
    child->load(QString("/nonexistent/file.svg"));
    CHECK(!child->renderer()->isValid());
    child->load(path);
    CHECK(child->renderer()->isValid());
    QFile::remove(path);
    CHECK(child->inherits("QSvgWidget") && child->inherits("QWidget"));
}

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    testXml();
    testNumbers();
    testPaths();
    testRaster();
    testLoad();
    testShapes();
    testPaint();
    testGradients();
    testStrokes();
    testStructure();
    testText();
    testRenderBounds();
    testElements();
    testRobustness();
    testGenerator();
    testWidget();
    if (g_failures == 0)
        std::puts("all svg tests passed");
    return g_failures == 0 ? 0 : 1;
}
