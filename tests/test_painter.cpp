// MIT License - see LICENSE
//
// Tests for the vector drawing of Nt GUI: QTransform, QPainterPath, QPolygon, gradients, pens with caps,
// joins and dashes, and QPainter drawing them with floating-point coordinates, transformations and
// antialiasing, checked through pixels. The SVG generator writes paths too.
#include <QtCore/qbuffer.h>
#include <QtGui/QtGui>
#include <QtGui/qguiapplication.h>
#include <QtSvg/QSvgGenerator>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool near(qreal a, qreal b, qreal tolerance = 1e-9) { return std::fabs(a - b) <= tolerance; }

static bool nearPoint(const QPointF &a, const QPointF &b, qreal tolerance = 1e-6) {
    const bool ok = near(a.x(), b.x(), tolerance) && near(a.y(), b.y(), tolerance);
    if (!ok)
        std::printf("  point is (%g, %g), expected (%g, %g)\n", a.x(), a.y(), b.x(), b.y());
    return ok;
}

static bool isColor(QRgb p, int r, int g, int b, int tolerance = 3) {
    return std::abs(qRed(p) - r) <= tolerance && std::abs(qGreen(p) - g) <= tolerance &&
           std::abs(qBlue(p) - b) <= tolerance;
}

static bool color(QRgb p, int r, int g, int b, int tolerance = 3) {
    const bool ok = isColor(p, r, g, b, tolerance);
    if (!ok)
        std::printf("  pixel is %d,%d,%d, expected %d,%d,%d\n", qRed(p), qGreen(p), qBlue(p), r, g, b);
    return ok;
}

static bool white(QRgb p) { return isColor(p, 255, 255, 255, 0); }

static QImage paint(int w, int h, const std::function<void(QPainter &)> &draw) {
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    draw(painter);
    return image;
}

static void testTransform() {
    QTransform t;
    CHECK(t.isIdentity() && t.type() == QTransform::TxNone);
    t.translate(50, 50);
    t.rotate(90);
    CHECK(t.type() == QTransform::TxRotate);
    CHECK(nearPoint(t.map(QPointF(10, 0)), QPointF(50, 60)));
    CHECK(nearPoint(t.inverted().map(QPointF(50, 60)), QPointF(10, 0)));
    QTransform s = QTransform::fromScale(2, 3);
    CHECK(s.type() == QTransform::TxScale && nearPoint(s.map(QPointF(1, 1)), QPointF(2, 3)));
    CHECK(QTransform::fromTranslate(4, 5).type() == QTransform::TxTranslate);
    // Applied in order: scale, then translate.
    const QTransform st = s * QTransform::fromTranslate(10, 0);
    CHECK(nearPoint(st.map(QPointF(1, 1)), QPointF(12, 3)));
    CHECK(st.mapRect(QRectF(0, 0, 1, 1)) == QRectF(10, 0, 2, 3));
    QTransform r;
    r.rotate(45);
    CHECK(near(r.determinant(), 1.0, 1e-12) && r.isInvertible());
    const QRectF rotated = r.mapRect(QRectF(-1, -1, 2, 2));
    CHECK(near(rotated.width(), 2 * std::sqrt(2.0), 1e-9));
    QTransform sh;
    sh.shear(1, 0);
    CHECK(sh.type() == QTransform::TxShear && nearPoint(sh.map(QPointF(0, 1)), QPointF(1, 1)));
    bool ok = true;
    QTransform(0, 0, 0, 0, 0, 0).inverted(&ok);
    CHECK(!ok);
    QTransform p;
    p.rotate(30, Qt::YAxis);
    CHECK(!p.isAffine() && p.type() == QTransform::TxProject);
    CHECK(nearPoint(QPointF(1, 2) * QTransform::fromTranslate(1, 1), QPointF(2, 3)));
}

static void testPaths() {
    QPainterPath rect;
    rect.addRect(10, 10, 20, 10);
    CHECK(rect.boundingRect() == QRectF(10, 10, 20, 10));
    CHECK(rect.contains(QPointF(15, 15)) && !rect.contains(QPointF(5, 5)));
    CHECK(near(rect.length(), 60.0, 1e-9));
    CHECK(nearPoint(rect.pointAtPercent(0.5), QPointF(30, 20)));
    CHECK(!rect.isEmpty() && QPainterPath().isEmpty());

    QPainterPath curve;
    curve.moveTo(0, 0);
    curve.cubicTo(0, 100, 100, 100, 100, 0);
    CHECK(curve.elementCount() == 4 && curve.elementAt(1).isCurveTo());
    CHECK(curve.controlPointRect() == QRectF(0, 0, 100, 100));
    CHECK(near(curve.boundingRect().height(), 75.0, 0.5)); // the curve reaches 3/4 of its control points
    CHECK(nearPoint(curve.currentPosition(), QPointF(100, 0)));

    QPainterPath arc;
    arc.arcMoveTo(QRectF(0, 0, 100, 100), 0);
    arc.arcTo(QRectF(0, 0, 100, 100), 0, 90);
    CHECK(nearPoint(arc.elementAt(0), QPointF(100, 50)));
    CHECK(nearPoint(arc.currentPosition(), QPointF(50, 0), 1e-9));
    CHECK(near(arc.length(), 3.14159265 * 50 / 2, 0.2));

    QPainterPath circle;
    circle.addEllipse(QPointF(50, 50), 40, 40);
    CHECK(circle.contains(QPointF(50, 50)) && !circle.contains(QPointF(12, 12)));
    CHECK(circle.boundingRect().adjusted(-0.5, -0.5, 0.5, 0.5).contains(QRectF(10, 10, 80, 80)));
    CHECK(circle.contains(QRectF(40, 40, 20, 20)) && !circle.contains(QRectF(5, 5, 20, 20)));
    CHECK(circle.intersects(QRectF(0, 45, 15, 10)) && !circle.intersects(QRectF(0, 0, 10, 10)));

    // Two overlapping squares: the overlap is outside with the odd-even rule, inside with the winding rule.
    QPainterPath two;
    two.addRect(0, 0, 20, 20);
    two.addRect(10, 10, 20, 20);
    CHECK(!two.contains(QPointF(15, 15)));
    two.setFillRule(Qt::WindingFill);
    CHECK(two.contains(QPointF(15, 15)));

    QPainterPath rounded;
    rounded.addRoundedRect(QRectF(0, 0, 40, 20), 50, 50, Qt::RelativeSize);
    CHECK(!rounded.contains(QPointF(1, 1)) && rounded.contains(QPointF(20, 10)));
    QPainterPath moved = rect.translated(5, 5);
    CHECK(moved.boundingRect() == QRectF(15, 15, 20, 10));
    CHECK((rect + moved).elementCount() == rect.elementCount() + moved.elementCount());
    CHECK(QTransform::fromScale(2, 2).map(rect).boundingRect() == QRectF(20, 20, 40, 20));
    const QList<QPolygonF> polys = circle.toSubpathPolygons();
    CHECK(polys.size() == 1 && polys.first().size() > 16);

    QPolygonF tri({QPointF(0, 0), QPointF(10, 0), QPointF(0, 10)});
    CHECK(tri.containsPoint(QPointF(2, 2), Qt::OddEvenFill) && !tri.containsPoint(QPointF(8, 8), Qt::OddEvenFill));
    CHECK(tri.boundingRect() == QRectF(0, 0, 10, 10) && !tri.isClosed());
    CHECK(QPolygonF(QRectF(0, 0, 2, 2)).isClosed());
    QPolygon ip(QRect(0, 0, 5, 5));
    CHECK(ip.size() == 4 && ip.boundingRect() == QRect(0, 0, 5, 5));
    CHECK(ip.translated(1, 1).point(0) == QPoint(1, 1));
}

static void testPensAndBrushes() {
    QPen pen;
    CHECK(pen.capStyle() == Qt::SquareCap && pen.joinStyle() == Qt::BevelJoin && pen.isSolid());
    pen.setStyle(Qt::DashLine);
    CHECK(pen.dashPattern() == QList<qreal>({4, 2}));
    pen.setDashPattern({1, 3, 5, 7});
    CHECK(pen.style() == Qt::CustomDashLine && pen.dashPattern().size() == 4);
    CHECK(QPen(Qt::red, 0).isCosmetic());

    QLinearGradient g(0, 0, 100, 0);
    g.setColorAt(1.0, Qt::blue);
    g.setColorAt(0.0, Qt::red);
    g.setColorAt(0.5, Qt::green);
    CHECK(g.stops().size() == 3 && g.stops().first().second == QColor(Qt::red));
    g.setColorAt(0.5, Qt::yellow);
    CHECK(g.stops().size() == 3 && g.stops().at(1).second == QColor(Qt::yellow));
    const QBrush b(g);
    CHECK(b.style() == Qt::LinearGradientPattern && b.gradient() && b.gradient()->type() == QGradient::LinearGradient);
    CHECK(QBrush(QRadialGradient(0, 0, 5)).style() == Qt::RadialGradientPattern);
    CHECK(QBrush(QConicalGradient(0, 0, 0)).style() == Qt::ConicalGradientPattern);
    CHECK(QBrush(Qt::red).isOpaque() && !QBrush(QColor(0, 0, 0, 100)).isOpaque());
}

static void testPainting() {
    // An antialiased circle: solid inside, blended at the edge.
    const QImage circle = paint(100, 100, [](QPainter &p) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 255));
        p.drawEllipse(QPointF(50, 50), 30.3, 30.3);
    });
    CHECK(color(circle.pixel(50, 50), 0, 0, 255) && white(circle.pixel(5, 5)));
    int partial = 0;
    for (int x = 0; x < 100; ++x) {
        const QRgb px = circle.pixel(x, 50);
        if (!white(px) && !isColor(px, 0, 0, 255, 0))
            ++partial;
    }
    CHECK(partial >= 2);

    // The same circle aliased: every pixel is either background or the brush.
    const QImage aliased = paint(100, 100, [](QPainter &p) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 255));
        p.drawEllipse(QPointF(50, 50), 30.3, 30.3);
    });
    bool twoColors = true;
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 100; ++x)
            twoColors = twoColors && (white(aliased.pixel(x, y)) || isColor(aliased.pixel(x, y), 0, 0, 255, 0));
    CHECK(twoColors);

    // A rotated square.
    const QImage rotated = paint(100, 100, [](QPainter &p) {
        p.translate(50, 50);
        p.rotate(45);
        p.fillRect(QRectF(-10, -10, 20, 20), QColor(255, 0, 0));
    });
    CHECK(color(rotated.pixel(50, 50), 255, 0, 0));
    CHECK(color(rotated.pixel(50, 37), 255, 0, 0)); // a corner points up: 14 pixels above the center
    CHECK(white(rotated.pixel(39, 39)));            // where the corner of an unrotated square would be
    // Integer drawing goes through the transformation as well.
    const QImage rotatedInt = paint(100, 100, [](QPainter &p) {
        p.translate(50, 50);
        p.rotate(45);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 0, 0));
        p.drawRect(-10, -10, 20, 20);
    });
    CHECK(color(rotatedInt.pixel(50, 37), 255, 0, 0) && white(rotatedInt.pixel(39, 39)));

    // Scaling.
    const QImage scaled = paint(60, 60, [](QPainter &p) {
        p.scale(2, 2);
        p.fillRect(QRectF(5, 5, 10, 10), QColor(0, 128, 0));
    });
    CHECK(color(scaled.pixel(11, 11), 0, 128, 0) && color(scaled.pixel(28, 28), 0, 128, 0));
    CHECK(white(scaled.pixel(9, 9)) && white(scaled.pixel(31, 31)));

    // Gradients.
    QLinearGradient linear(0, 0, 100, 0);
    linear.setColorAt(0, QColor(255, 0, 0));
    linear.setColorAt(1, QColor(0, 0, 255));
    const QImage lin = paint(100, 20, [&](QPainter &p) { p.fillRect(QRectF(0, 0, 100, 20), linear); });
    CHECK(color(lin.pixel(0, 10), 255, 0, 0, 6) && color(lin.pixel(99, 10), 0, 0, 255, 6));
    CHECK(color(lin.pixel(50, 10), 127, 0, 127, 8));
    // The integer fillRect takes the gradient too.
    const QImage linInt = paint(100, 20, [&](QPainter &p) { p.fillRect(0, 0, 100, 20, QBrush(linear)); });
    CHECK(color(linInt.pixel(50, 10), 127, 0, 127, 8));
    QRadialGradient radial(QPointF(50, 50), 50);
    radial.setColorAt(0, Qt::white);
    radial.setColorAt(1, Qt::black);
    const QImage rad = paint(100, 100, [&](QPainter &p) { p.fillRect(QRectF(0, 0, 100, 100), radial); });
    CHECK(qRed(rad.pixel(50, 50)) > 240 && qRed(rad.pixel(97, 50)) < 20 && qRed(rad.pixel(0, 0)) < 5);
    QConicalGradient conical(QPointF(50, 50), 0);
    conical.setColorAt(0, QColor(0, 0, 0));
    conical.setColorAt(1, QColor(255, 255, 255));
    const QImage con = paint(100, 100, [&](QPainter &p) { p.fillRect(QRectF(0, 0, 100, 100), conical); });
    // Counterclockwise from 3 o'clock: a quarter at 12 o'clock, three quarters at 6 o'clock.
    CHECK(near(qRed(con.pixel(50, 10)), 64, 12) && near(qRed(con.pixel(50, 90)), 191, 12));
    // A gradient moves with the transformation.
    const QImage moved = paint(100, 20, [&](QPainter &p) {
        p.translate(50, 0);
        p.fillRect(QRectF(-50, 0, 100, 20), linear);
    });
    CHECK(color(moved.pixel(50, 10), 255, 0, 0, 6));

    // An arc with a thick pen and round caps: the cap reaches past the end of the arc.
    const QImage arc = paint(120, 120, [](QPainter &p) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(0, 0, 0), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(10, 10, 100, 100), 0, 90 * 16);
    });
    CHECK(color(arc.pixel(110, 60), 0, 0, 0, 10));                // the start, at 3 o'clock
    CHECK(color(arc.pixel(60, 10), 0, 0, 0, 10));                 // the end, at 12 o'clock
    CHECK(color(arc.pixel(56, 10), 0, 0, 0, 30));                 // the round cap past the end
    CHECK(white(arc.pixel(60, 110)) && white(arc.pixel(10, 60))); // the rest of the circle
    CHECK(white(arc.pixel(60, 60)));                              // the brush is not used
    const QImage flat = paint(120, 120, [](QPainter &p) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(0, 0, 0), 10, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(QRectF(10, 10, 100, 100), 0, 90 * 16);
    });
    CHECK(qRed(flat.pixel(55, 10)) > 200); // no cap

    // Dashes leave gaps along the line.
    const QImage dashed = paint(100, 10, [](QPainter &p) {
        p.setPen(QPen(QBrush(QColor(0, 0, 0)), 2, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(0, 5), QPointF(100, 5));
    });
    int gaps = 0, dashes = 0;
    for (int x = 0; x < 100; ++x)
        (white(dashed.pixel(x, 5)) ? gaps : dashes)++;
    CHECK(gaps > 20 && dashes > 40);

    // A star: the middle is outside with the odd-even rule, inside with the winding rule.
    QPolygonF star;
    for (int i = 0; i < 5; ++i) {
        const qreal a = -3.14159265 / 2 + i * 4 * 3.14159265 / 5;
        star << QPointF(50 + 40 * std::cos(a), 50 + 40 * std::sin(a));
    }
    const QImage oddEven = paint(100, 100, [&](QPainter &p) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(200, 0, 0));
        p.drawPolygon(star, Qt::OddEvenFill);
    });
    const QImage windingFill = paint(100, 100, [&](QPainter &p) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(200, 0, 0));
        p.drawPolygon(star, Qt::WindingFill);
    });
    CHECK(white(oddEven.pixel(50, 52)) && color(windingFill.pixel(50, 52), 200, 0, 0));
    CHECK(color(oddEven.pixel(50, 15), 200, 0, 0)); // a point of the star

    // A Bezier petal, rotated around a center: a flower.
    QPainterPath petal;
    petal.moveTo(0, 0);
    petal.cubicTo(10, -15, 30, -15, 40, 0);
    petal.cubicTo(30, 15, 10, 15, 0, 0);
    const QImage flower = paint(100, 100, [&](QPainter &p) {
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(50, 50);
        for (int i = 0; i < 6; ++i) {
            p.fillPath(petal, QColor(220, 60, 120));
            p.rotate(60);
        }
    });
    CHECK(color(flower.pixel(75, 50), 220, 60, 120) && color(flower.pixel(25, 50), 220, 60, 120));
    CHECK(white(flower.pixel(50, 5)) && white(flower.pixel(95, 95)));

    // Composition, opacity and clipping work for paths too.
    const QImage source = paint(20, 20, [](QPainter &p) {
        p.fillRect(QRectF(0, 0, 20, 20), QColor(255, 0, 0));
        p.setOpacity(0.5);
        p.fillRect(QRectF(0, 0, 10, 20), QColor(0, 0, 255));
        p.setOpacity(1.0);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillRect(QRectF(10, 0, 10, 10), QColor(0, 255, 0));
    });
    CHECK(color(source.pixel(5, 5), 127, 0, 127, 3) && color(source.pixel(15, 15), 255, 0, 0));
    CHECK(color(source.pixel(15, 5), 0, 0, 0)); // cleared, on an image without alpha
    const QImage clipped = paint(40, 40, [](QPainter &p) {
        p.setClipRect(QRect(0, 0, 20, 40));
        p.fillRect(QRectF(0, 0, 40, 40), QColor(0, 0, 0));
    });
    CHECK(color(clipped.pixel(10, 10), 0, 0, 0) && white(clipped.pixel(30, 10)));

    // Polylines, points and rounded rectangles with real coordinates.
    const QImage misc = paint(60, 60, [](QPainter &p) {
        p.setPen(QPen(QColor(0, 0, 0), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawPoint(QPointF(10.5, 10.5));
        const QPointF line[3] = {QPointF(20, 50), QPointF(30, 40), QPointF(40, 50)};
        p.drawPolyline(line, 3);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 160, 0));
        p.drawRoundedRect(QRectF(30, 5, 25, 20), 8, 8);
    });
    CHECK(color(misc.pixel(10, 10), 0, 0, 0) && white(misc.pixel(14, 14)));
    CHECK(color(misc.pixel(30, 40), 0, 0, 0, 40));
    CHECK(color(misc.pixel(42, 15), 0, 160, 0) && white(misc.pixel(30, 5)));

    // Integer drawing under a translation by whole pixels is unchanged: an outline one pixel wide.
    const QImage exact = paint(20, 20, [](QPainter &p) {
        p.translate(2, 2);
        p.setPen(QColor(0, 0, 0));
        p.drawRect(0, 0, 10, 10);
    });
    CHECK(color(exact.pixel(2, 2), 0, 0, 0, 0) && color(exact.pixel(12, 12), 0, 0, 0, 0) && white(exact.pixel(3, 3)) &&
          white(exact.pixel(13, 13)));
    // The painter keeps the transformation through save() and restore().
    QImage scratch(10, 10, QImage::Format_RGB32);
    QPainter p(&scratch);
    p.save();
    p.rotate(30);
    CHECK(p.transform().isRotating());
    p.restore();
    CHECK(p.transform().isIdentity());
    p.translate(QPointF(1.5, 2));
    CHECK(p.translation() == QPoint(2, 2) && near(p.deviceTransform().dx(), 1.5));
}

static void testSvgPaths() {
    QBuffer buffer;
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setSize(QSize(100, 100));
    {
        QPainter p(&generator);
        QPainterPath path;
        path.moveTo(10, 10);
        path.cubicTo(20, 0, 40, 0, 50, 10);
        p.setPen(QPen(QColor(0, 0, 255), 3, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        QLinearGradient g(0, 0, 100, 0);
        g.setColorAt(0, Qt::red);
        g.setColorAt(1, Qt::blue);
        p.setBrush(g);
        p.translate(5, 5);
        p.drawPath(path);
    }
    const QByteArray svg = buffer.data();
    CHECK(svg.contains("<path d=\"M10 10C20 0 40 0 50 10\""));
    CHECK(svg.contains("<linearGradient id=\"gradient1\"") && svg.contains("fill=\"url(#gradient1)\""));
    CHECK(svg.contains("stroke-linecap=\"round\"") && svg.contains("stroke-dasharray=\"12,6\""));
    CHECK(svg.contains("transform=\"matrix(1 0 0 1 5 5)\""));
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    testTransform();
    testPaths();
    testPensAndBrushes();
    testPainting();
    testSvgPaths();
    if (g_failures)
        std::printf("%d failure(s)\n", g_failures);
    else
        std::printf("all painter tests passed\n");
    return g_failures ? 1 : 0;
}
