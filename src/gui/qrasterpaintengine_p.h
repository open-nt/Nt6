// MIT License - see LICENSE
#pragma once

#include <QtGui/qpaintengine.h>

#include <vector>

// Software engine that paints on a QImage in one of its 32-bit formats. No antialiasing and no
// transforms: the painter hands over device coordinates.
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
    Type type() const override { return Raster; }

private:
    // Premultiplied color of `color` at the current opacity.
    QRgb premultiplied(const QColor &color) const;
    void fillRect(const QRect &rect, QRgb premult);
    void fillSpan(int y, int x0, int x1, QRgb premult);
    void blend(uint *dst, QRgb premult) const;
    void strokeSquare(int cx, int cy, int width, QRgb premult);
    void strokeRect(const QRect &rect, int width, QRgb premult);

    QImage *m_image = nullptr;
    QPaintEngineState m_state;
    std::vector<QRect> m_clip; // device space; the image rectangle when there is no clip
};
