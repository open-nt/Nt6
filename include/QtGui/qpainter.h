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
#include <QtGui/qpen.h>
#include <QtGui/qregion.h>

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
    void translate(int dx, int dy) { translate(QPoint(dx, dy)); }
    QPoint translation() const;
    void resetTransform();

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
