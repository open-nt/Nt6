// MIT License - see LICENSE
#pragma once

#include <QtCore/qline.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtGui/qbrush.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpen.h>
#include <QtGui/qregion.h>
#include <QtGui/qtransform.h>

class QPaintDevice;

class QPaintEngineState {
public:
    QPen pen() const { return m_pen; }
    QBrush brush() const { return m_brush; }
    QRegion clipRegion() const { return m_clip; }
    bool hasClipping() const { return m_clipping; }
    qreal opacity() const { return m_opacity; }
    QPainter::CompositionMode compositionMode() const { return m_mode; }
    QTransform transform() const { return m_transform; }
    QPainter::RenderHints renderHints() const { return m_hints; }

private:
    friend class QPainter;
    QPen m_pen;
    QBrush m_brush;
    QRegion m_clip;
    bool m_clipping = false;
    qreal m_opacity = 1.0;
    QPainter::CompositionMode m_mode = QPainter::CompositionMode_SourceOver;
    QTransform m_transform;
    QPainter::RenderHints m_hints;
};

class QPaintEngine {
public:
    enum Type { Raster = 10, OpenGL2 = 14, User = 50, MaxUser = 100 };

    virtual ~QPaintEngine();

    virtual bool begin(QPaintDevice *device) = 0;
    virtual bool end() = 0;
    virtual void updateState(const QPaintEngineState &state) = 0;
    virtual void drawRects(const QRect *rects, int rectCount) = 0;
    virtual void drawLines(const QLine *lines, int lineCount) = 0;
    virtual void drawPoints(const QPoint *points, int pointCount) = 0;
    virtual void drawEllipse(const QRect &rect) = 0;
    virtual void drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) = 0;
    virtual void drawTransformedImage(const QRectF &rect, const QImage &image, const QRectF &sourceRect);
    // Paints the color of the pen where `mask`, a width by height array of 8-bit coverage values, is set; text uses it.
    virtual void drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height, qsizetype stride) = 0;
    virtual void drawPath(const QPainterPath &path);
    virtual Type type() const = 0;

    bool isActive() const { return m_active; }
    void setActive(bool active) { m_active = active; }
    QPaintDevice *paintDevice() const { return m_device; }
    const QPaintEngineState &state() const { return m_lastState; }

protected:
    QPaintEngine() = default;
    void setPaintDevice(QPaintDevice *device) { m_device = device; }

private:
    friend class QPainter;
    Q_DISABLE_COPY(QPaintEngine)
    QPaintDevice *m_device = nullptr;
    bool m_active = false;
    QPaintEngineState m_lastState;
};
