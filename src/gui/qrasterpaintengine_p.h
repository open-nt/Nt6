// MIT License - see LICENSE
#pragma once

#include <QtGui/qpaintengine.h>

#include "qrasterizer_p.h"

#include <functional>
#include <vector>

// Software engine that paints on a QImage in one of its 32-bit formats. Rectangles, lines, points and
// ellipses come in device coordinates and are drawn on whole pixels; paths come with a transformation and are
// rasterized, antialiased if the painter asks for it.
class QRasterPaintEngine : public QPaintEngine {
public:
    QRasterPaintEngine() = default;
    ~QRasterPaintEngine() override;

    bool begin(QPaintDevice *device) override;
    bool end() override;
    void updateState(const QPaintEngineState &state) override;
    void drawRects(const QRect *rects, int rectCount) override;
    void drawLines(const QLine *lines, int lineCount) override;
    void drawPoints(const QPoint *points, int pointCount) override;
    void drawEllipse(const QRect &rect) override;
    void drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) override;
    void drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height, qsizetype stride) override;
    void drawPath(const QPainterPath &path) override;
    Type type() const override { return Raster; }

private:
    // Premultiplied color of `color` at the current opacity.
    QRgb premultiplied(const QColor &color) const;
    void fillRect(const QRect &rect, QRgb premult);
    void fillSpan(int y, int x0, int x1, QRgb premult);
    void blend(uint *dst, QRgb premult) const;
    // Paints the polygons, in device coordinates, with the brush (or the color of the pen if `pen` is set).
    void paintCoverage(const void *polygons, bool evenOdd, const QBrush &brush, bool pen, const QTransform &toDevice);

    QImage *m_image = nullptr;
    QPaintEngineState m_state;
    std::vector<QRect> m_clip; // device space; the image rectangle when there is no clip
};

namespace QtRasterPrivate {

// What drawing a path paints, as polygons in device coordinates.
struct PathGeometry {
    Polygons fill;   // the inside, to fill with the fill rule of the path
    Polygons stroke; // the outline of the stroke, to fill with the non-zero rule
    bool evenOdd = false;
};

// The lines and curves of `path`. A line back to the start of a subpath that ends it closes the subpath, so that
// its last corner gets a join when it is stroked.
Path toRasterPath(const QPainterPath &path);
// Flattens `path`, transformed by `toDevice`, into its fill (if `fill` is set) and the stroke of `pen`. Aliased
// strokes are moved half a pixel, as the raster engine draws them. Returns false if nothing would be drawn.
bool pathGeometry(const QPainterPath &path, const QTransform &toDevice, bool fill, const QPen &pen, bool antialias,
                  PathGeometry *out);
// Sets up `shader` for the gradient of `brush` in device space. Returns false if the brush has no gradient or its
// transformation cannot be inverted.
bool gradientShader(const QBrush &brush, const QTransform &toDevice, double opacity, Shader *shader);

// Receives the rectangles, in device pixels, that make up an integer primitive; `pen` tells whether a rectangle is
// painted with the pen or with the brush.
using RectSink = std::function<void(const QRect &rect, bool pen)>;
// The rectangles of the integer primitives, as the raster engine paints them.
void rectsOfRects(const QRect *rects, int rectCount, const QPen &pen, bool fill, const RectSink &sink);
void rectsOfLines(const QLine *lines, int lineCount, const QPen &pen, const RectSink &sink);
void rectsOfPoints(const QPoint *points, int pointCount, const QPen &pen, const RectSink &sink);
void rectsOfEllipse(const QRect &rect, const QPen &pen, bool fill, const RectSink &sink);

} // namespace QtRasterPrivate
