// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qline.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpen.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qpolygon.h>
#include <QtGui/qregion.h>
#include <QtGui/qtransform.h>

#include <memory>

class QPaintDevice;
class QPaintEngine;

class QPainter {
public:
    enum RenderHint { Antialiasing = 0x01, TextAntialiasing = 0x02, SmoothPixmapTransform = 0x04 };
    Q_DECLARE_FLAGS(RenderHints, RenderHint)

    enum CompositionMode { CompositionMode_SourceOver = 0, CompositionMode_Clear = 2, CompositionMode_Source = 3 };

    QPainter();
    explicit QPainter(QPaintDevice *device);
    ~QPainter();
    Q_DISABLE_COPY(QPainter)

    bool begin(QPaintDevice *device);
    bool end();
    bool isActive() const;
    QPaintDevice *device() const;
    QPaintEngine *paintEngine() const;

    void setPen(const QPen &pen);
    void setPen(const QColor &color);
    void setPen(Qt::PenStyle style);
    const QPen &pen() const;
    void setBrush(const QBrush &brush);
    void setBrush(Qt::BrushStyle style);
    const QBrush &brush() const;
    void setOpacity(qreal opacity);
    qreal opacity() const;
    void setCompositionMode(CompositionMode mode);
    CompositionMode compositionMode() const;
    void setRenderHint(RenderHint hint, bool on = true);
    void setRenderHints(RenderHints hints, bool on = true);
    RenderHints renderHints() const;

    void setFont(const QFont &font);
    const QFont &font() const;

    void save();
    void restore();
    void translate(const QPoint &offset);
    void translate(const QPointF &offset);
    void translate(qreal dx, qreal dy) { translate(QPointF(dx, dy)); }
    void scale(qreal sx, qreal sy);
    void rotate(qreal angle);
    void shear(qreal sh, qreal sv);
    QPoint translation() const;
    void resetTransform();
    void setTransform(const QTransform &transform, bool combine = false);
    const QTransform &transform() const;
    void setWorldTransform(const QTransform &matrix, bool combine = false) { setTransform(matrix, combine); }
    const QTransform &worldTransform() const { return transform(); }
    QTransform deviceTransform() const;
    QTransform combinedTransform() const { return transform(); }
    void setWorldMatrixEnabled(bool enabled);
    bool worldMatrixEnabled() const;

    void setClipRect(const QRect &rect);
    void setClipRect(int x, int y, int width, int height) { setClipRect(QRect(x, y, width, height)); }
    void setClipRegion(const QRegion &region);
    QRegion clipRegion() const;
    void setClipping(bool enable);
    bool hasClipping() const;

    void fillRect(const QRect &rect, const QBrush &brush);
    void fillRect(const QRect &rect, const QColor &color);
    void fillRect(const QRect &rect, Qt::GlobalColor color);
    void fillRect(const QRect &rect, Qt::BrushStyle style);
    void fillRect(int x, int y, int width, int height, const QBrush &brush) {
        fillRect(QRect(x, y, width, height), brush);
    }
    void fillRect(int x, int y, int width, int height, const QColor &color) {
        fillRect(QRect(x, y, width, height), color);
    }
    void fillRect(int x, int y, int width, int height, Qt::GlobalColor color) {
        fillRect(QRect(x, y, width, height), color);
    }
    void fillRect(int x, int y, int width, int height, Qt::BrushStyle style) {
        fillRect(QRect(x, y, width, height), style);
    }
    void eraseRect(const QRect &rect);
    void fillRect(const QRectF &rect, const QBrush &brush);
    void fillRect(const QRectF &rect, const QColor &color) { fillRect(rect, QBrush(color)); }
    void fillRect(const QRectF &rect, Qt::GlobalColor color) { fillRect(rect, QBrush(color)); }
    void eraseRect(const QRectF &rect);

    void drawPath(const QPainterPath &path);
    void fillPath(const QPainterPath &path, const QBrush &brush);
    void strokePath(const QPainterPath &path, const QPen &pen);

    void drawPoint(const QPointF &point);
    void drawPoints(const QPointF *points, int pointCount);
    void drawPoints(const QPoint *points, int pointCount);
    void drawPoints(const QPolygonF &points) { drawPoints(points.constData(), static_cast<int>(points.size())); }
    void drawPoints(const QPolygon &points) { drawPoints(points.constData(), static_cast<int>(points.size())); }
    void drawLine(const QLineF &line);
    void drawLine(const QPointF &p1, const QPointF &p2) { drawLine(QLineF(p1, p2)); }
    void drawLines(const QLineF *lines, int lineCount);
    void drawLines(const QLine *lines, int lineCount);
    void drawLines(const QList<QLineF> &lines) { drawLines(lines.constData(), static_cast<int>(lines.size())); }
    void drawLines(const QList<QLine> &lines) { drawLines(lines.constData(), static_cast<int>(lines.size())); }
    void drawRect(const QRectF &rect);
    void drawRects(const QRectF *rects, int rectCount);
    void drawRects(const QRect *rects, int rectCount);
    void drawRects(const QList<QRectF> &rects) { drawRects(rects.constData(), static_cast<int>(rects.size())); }
    void drawRects(const QList<QRect> &rects) { drawRects(rects.constData(), static_cast<int>(rects.size())); }
    void drawRoundedRect(const QRectF &rect, qreal xRadius, qreal yRadius, Qt::SizeMode mode = Qt::AbsoluteSize);
    void drawRoundedRect(int x, int y, int w, int h, qreal xRadius, qreal yRadius,
                         Qt::SizeMode mode = Qt::AbsoluteSize) {
        drawRoundedRect(QRectF(x, y, w, h), xRadius, yRadius, mode);
    }
    void drawRoundedRect(const QRect &rect, qreal xRadius, qreal yRadius, Qt::SizeMode mode = Qt::AbsoluteSize) {
        drawRoundedRect(QRectF(rect), xRadius, yRadius, mode);
    }
    void drawEllipse(const QRectF &rect);
    void drawEllipse(const QPointF &center, qreal rx, qreal ry) {
        drawEllipse(QRectF(center.x() - rx, center.y() - ry, 2 * rx, 2 * ry));
    }
    void drawEllipse(const QPoint &center, int rx, int ry) {
        drawEllipse(QRect(center.x() - rx, center.y() - ry, 2 * rx, 2 * ry));
    }
    void drawArc(const QRectF &rectangle, int startAngle, int spanAngle);
    void drawArc(const QRect &rectangle, int startAngle, int spanAngle) {
        drawArc(QRectF(rectangle), startAngle, spanAngle);
    }
    void drawArc(int x, int y, int width, int height, int startAngle, int spanAngle) {
        drawArc(QRectF(x, y, width, height), startAngle, spanAngle);
    }
    void drawPie(const QRectF &rectangle, int startAngle, int spanAngle);
    void drawPie(const QRect &rectangle, int startAngle, int spanAngle) {
        drawPie(QRectF(rectangle), startAngle, spanAngle);
    }
    void drawPie(int x, int y, int width, int height, int startAngle, int spanAngle) {
        drawPie(QRectF(x, y, width, height), startAngle, spanAngle);
    }
    void drawChord(const QRectF &rectangle, int startAngle, int spanAngle);
    void drawChord(const QRect &rectangle, int startAngle, int spanAngle) {
        drawChord(QRectF(rectangle), startAngle, spanAngle);
    }
    void drawChord(int x, int y, int width, int height, int startAngle, int spanAngle) {
        drawChord(QRectF(x, y, width, height), startAngle, spanAngle);
    }
    void drawPolyline(const QPointF *points, int pointCount);
    void drawPolyline(const QPoint *points, int pointCount);
    void drawPolyline(const QPolygonF &polyline) {
        drawPolyline(polyline.constData(), static_cast<int>(polyline.size()));
    }
    void drawPolyline(const QPolygon &polyline) {
        drawPolyline(polyline.constData(), static_cast<int>(polyline.size()));
    }
    void drawPolygon(const QPointF *points, int pointCount, Qt::FillRule fillRule = Qt::OddEvenFill);
    void drawPolygon(const QPoint *points, int pointCount, Qt::FillRule fillRule = Qt::OddEvenFill);
    void drawPolygon(const QPolygonF &points, Qt::FillRule fillRule = Qt::OddEvenFill) {
        drawPolygon(points.constData(), static_cast<int>(points.size()), fillRule);
    }
    void drawPolygon(const QPolygon &points, Qt::FillRule fillRule = Qt::OddEvenFill) {
        drawPolygon(points.constData(), static_cast<int>(points.size()), fillRule);
    }
    void drawConvexPolygon(const QPointF *points, int pointCount) { drawPolygon(points, pointCount, Qt::WindingFill); }
    void drawConvexPolygon(const QPoint *points, int pointCount) { drawPolygon(points, pointCount, Qt::WindingFill); }
    void drawConvexPolygon(const QPolygonF &polygon) { drawPolygon(polygon, Qt::WindingFill); }
    void drawConvexPolygon(const QPolygon &polygon) { drawPolygon(polygon, Qt::WindingFill); }
    void drawImage(const QPointF &point, const QImage &image);
    void drawImage(const QRectF &target, const QImage &image, const QRectF &source = QRectF());
    void drawPixmap(const QPointF &point, const QPixmap &pixmap) { drawImage(point, pixmap.toImage()); }
    void drawPixmap(const QRectF &target, const QPixmap &pixmap, const QRectF &source) {
        drawImage(target, pixmap.toImage(), source);
    }
    void drawText(const QPointF &position, const QString &text);
    void drawText(const QRectF &rectangle, int flags, const QString &text, QRectF *boundingRect = nullptr);

    void drawPoint(const QPoint &point);
    void drawPoint(int x, int y) { drawPoint(QPoint(x, y)); }
    void drawLine(const QLine &line);
    void drawLine(const QPoint &p1, const QPoint &p2) { drawLine(QLine(p1, p2)); }
    void drawLine(int x1, int y1, int x2, int y2) { drawLine(QLine(x1, y1, x2, y2)); }
    void drawRect(const QRect &rect);
    void drawRect(int x, int y, int width, int height) { drawRect(QRect(x, y, width, height)); }
    void drawEllipse(const QRect &rect);
    void drawEllipse(int x, int y, int width, int height) { drawEllipse(QRect(x, y, width, height)); }
    void drawImage(const QPoint &point, const QImage &image);
    void drawImage(int x, int y, const QImage &image) { drawImage(QPoint(x, y), image); }
    void drawImage(const QRect &target, const QImage &image, const QRect &source = QRect());
    void drawImage(const QPoint &point, const QImage &image, const QRect &source);
    void drawPixmap(const QPoint &point, const QPixmap &pixmap) { drawImage(point, pixmap.toImage()); }
    void drawPixmap(int x, int y, const QPixmap &pixmap) { drawImage(QPoint(x, y), pixmap.toImage()); }
    void drawPixmap(const QRect &target, const QPixmap &pixmap, const QRect &source = QRect()) {
        drawImage(target, pixmap.toImage(), source);
    }
    void drawPixmap(int x, int y, int width, int height, const QPixmap &pixmap) {
        drawImage(QRect(x, y, width, height), pixmap.toImage());
    }

    void drawText(const QPoint &position, const QString &text);
    void drawText(int x, int y, const QString &text) { drawText(QPoint(x, y), text); }
    void drawText(const QRect &rectangle, int flags, const QString &text, QRect *boundingRect = nullptr);
    void drawText(int x, int y, int width, int height, int flags, const QString &text, QRect *boundingRect = nullptr) {
        drawText(QRect(x, y, width, height), flags, text, boundingRect);
    }
    QRect boundingRect(const QRect &rectangle, int flags, const QString &text);
    QRect boundingRect(int x, int y, int w, int h, int flags, const QString &text) {
        return boundingRect(QRect(x, y, w, h), flags, text);
    }

private:
    struct Data;
    std::unique_ptr<Data> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QPainter::RenderHints)
