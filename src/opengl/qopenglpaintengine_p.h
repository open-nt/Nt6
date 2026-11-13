// MIT License - see LICENSE
#pragma once

#include <QtGui/qopengl.h>
#include <QtGui/qpaintengine.h>

#include <vector>

class QOpenGLContext;
class QOpenGLFunctions;
class QOpenGLPaintDevice;

namespace QtRasterPrivate {
struct Polygons;
}

// Paints with OpenGL into the framebuffer that is bound when painting begins. Shapes are filled with the stencil
// buffer (stencil, then cover), so any path and both fill rules work; edges are antialiased by multisampling, when the
// target has samples. Integer primitives are drawn as the same rectangles as in the raster engine, and paths are
// flattened and stroked by the same code, so both engines draw the same shapes.
class QOpenGL2PaintEngine : public QPaintEngine {
public:
    QOpenGL2PaintEngine() = default;
    ~QOpenGL2PaintEngine() override;

    bool begin(QPaintDevice *device) override;
    bool end() override;
    void updateState(const QPaintEngineState &state) override;
    void drawRects(const QRect *rects, int rectCount) override;
    void drawLines(const QLine *lines, int lineCount) override;
    void drawPoints(const QPoint *points, int pointCount) override;
    void drawEllipse(const QRect &rect) override;
    void drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) override;
    void drawTransformedImage(const QRectF &rect, const QImage &image, const QRectF &sourceRect) override;
    void drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height, qsizetype stride) override;
    void drawPath(const QPainterPath &path) override;
    Type type() const override { return OpenGL2; }

    // Draws `texture` over the whole target, blended if `blend` is set; for QOpenGLWindow's partial updates. The
    // context must be current.
    static void drawTexture(QOpenGLContext *context, GLuint texture, const QSize &targetSize, bool blend);

    struct Resources;

private:
    enum class Clip { None, Empty, Scissor, Stencil };
    enum Mode { Solid = 0, Linear = 1, Radial = 2, Conical = 3, Image = 4, Mask = 5 };

    bool prepare();
    void applyBlend();
    void setSolid(const QColor &color);
    // Sets up the brush for a fill in device coordinates; returns false if nothing would be drawn.
    bool setBrush(const QBrush &brush);
    void fillPolygons(const QtRasterPrivate::Polygons &polygons, bool evenOdd);
    void fillRects(const std::vector<QRect> &rects);
    void drawVertices(GLenum mode, const std::vector<GLfloat> &xy, const std::vector<GLfloat> *uv = nullptr);
    void uploadTexture(GLuint texture, int width, int height, const std::vector<unsigned char> &rgba, bool smooth);
    // Draws the part `source` of `image` onto the quadrilateral `corners` (top left, top right, bottom right, bottom
    // left, in device pixels), from the texture cache when the image fits in it.
    void drawImageQuad(const QImage &image, const QRectF &source, const QPointF (&corners)[4]);
    // Binds the cached texture of `image`, uploading it if needed; returns false if it is too large to cache.
    bool bindCachedImage(const QImage &image);

    QOpenGLContext *m_context = nullptr;
    QOpenGLFunctions *m_f = nullptr;
    Resources *m_res = nullptr;
    QOpenGLPaintDevice *m_device = nullptr;
    QSize m_size;
    bool m_flipped = false;
    bool m_hasStencil = true;
    QPaintEngineState m_state;
    Clip m_clip = Clip::None;
    QRegion m_clipRegion;
    bool m_clipping = false;
    bool m_clipDirty = true;
};
