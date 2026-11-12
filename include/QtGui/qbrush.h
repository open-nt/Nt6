// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpair.h>
#include <QtCore/qpoint.h>
#include <QtGui/qcolor.h>
#include <QtGui/qtransform.h>

#include <memory>

using QGradientStop = QPair<qreal, QColor>;
using QGradientStops = QList<QGradientStop>;

class QGradient {
public:
    enum Type { LinearGradient, RadialGradient, ConicalGradient, NoGradient };
    enum Spread { PadSpread, ReflectSpread, RepeatSpread };
    enum CoordinateMode { LogicalMode, StretchToDeviceMode, ObjectBoundingMode, ObjectMode };

    QGradient() = default;

    Type type() const { return m_type; }
    void setSpread(Spread spread) { m_spread = spread; }
    Spread spread() const { return m_spread; }
    void setColorAt(qreal position, const QColor &color);
    void setStops(const QGradientStops &stops);
    QGradientStops stops() const;
    CoordinateMode coordinateMode() const { return m_mode; }
    void setCoordinateMode(CoordinateMode mode) { m_mode = mode; }

    bool operator==(const QGradient &other) const;
    bool operator!=(const QGradient &other) const { return !(*this == other); }

protected:
    /*! \internal */
    Type m_type = NoGradient;
    /*! \internal */
    Spread m_spread = PadSpread;
    /*! \internal */
    CoordinateMode m_mode = LogicalMode;
    /*! \internal */
    QGradientStops m_stops;
    /*! \internal
        Linear: start and final stop; radial: center, focal point, radius, focal radius; conical: center, angle.
    */
    qreal m_data[6] = {0, 0, 0, 0, 0, 0};
};

class QLinearGradient : public QGradient {
public:
    QLinearGradient();
    QLinearGradient(const QPointF &start, const QPointF &finalStop);
    QLinearGradient(qreal x1, qreal y1, qreal x2, qreal y2);

    QPointF start() const { return QPointF(m_data[0], m_data[1]); }
    void setStart(const QPointF &start);
    void setStart(qreal x, qreal y) { setStart(QPointF(x, y)); }
    QPointF finalStop() const { return QPointF(m_data[2], m_data[3]); }
    void setFinalStop(const QPointF &stop);
    void setFinalStop(qreal x, qreal y) { setFinalStop(QPointF(x, y)); }
};

class QRadialGradient : public QGradient {
public:
    QRadialGradient();
    QRadialGradient(const QPointF &center, qreal radius, const QPointF &focalPoint);
    QRadialGradient(qreal cx, qreal cy, qreal radius, qreal fx, qreal fy);
    QRadialGradient(const QPointF &center, qreal radius);
    QRadialGradient(qreal cx, qreal cy, qreal radius);

    QPointF center() const { return QPointF(m_data[0], m_data[1]); }
    void setCenter(const QPointF &center);
    void setCenter(qreal x, qreal y) { setCenter(QPointF(x, y)); }
    QPointF focalPoint() const { return QPointF(m_data[2], m_data[3]); }
    void setFocalPoint(const QPointF &focalPoint);
    void setFocalPoint(qreal x, qreal y) { setFocalPoint(QPointF(x, y)); }
    qreal radius() const { return m_data[4]; }
    void setRadius(qreal radius) { m_data[4] = radius; }
    qreal centerRadius() const { return m_data[4]; }
    void setCenterRadius(qreal radius) { m_data[4] = radius; }
    qreal focalRadius() const { return m_data[5]; }
    void setFocalRadius(qreal radius) { m_data[5] = radius; }
};

class QConicalGradient : public QGradient {
public:
    QConicalGradient();
    QConicalGradient(const QPointF &center, qreal angle);
    QConicalGradient(qreal cx, qreal cy, qreal angle);

    QPointF center() const { return QPointF(m_data[0], m_data[1]); }
    void setCenter(const QPointF &center);
    void setCenter(qreal x, qreal y) { setCenter(QPointF(x, y)); }
    qreal angle() const { return m_data[2]; }
    void setAngle(qreal angle) { m_data[2] = angle; }
};

class QBrush {
public:
    QBrush() noexcept = default;
    QBrush(Qt::BrushStyle style) noexcept : m_style(style) {}
    QBrush(const QColor &color, Qt::BrushStyle style = Qt::SolidPattern) noexcept : m_color(color), m_style(style) {}
    QBrush(Qt::GlobalColor color, Qt::BrushStyle style = Qt::SolidPattern) noexcept : m_color(color), m_style(style) {}
    QBrush(const QGradient &gradient);

    Qt::BrushStyle style() const noexcept { return m_style; }
    void setStyle(Qt::BrushStyle style) noexcept { m_style = style; }
    const QColor &color() const noexcept { return m_color; }
    void setColor(const QColor &color) noexcept { m_color = color; }
    void setColor(Qt::GlobalColor color) noexcept { m_color = QColor(color); }
    const QGradient *gradient() const noexcept { return m_gradient.get(); }
    QTransform transform() const { return m_transform; }
    void setTransform(const QTransform &matrix) { m_transform = matrix; }
    bool isOpaque() const;

    friend bool operator==(const QBrush &a, const QBrush &b) noexcept {
        if (a.m_style != b.m_style || a.m_color != b.m_color || !(a.m_transform == b.m_transform))
            return false;
        if (!a.m_gradient || !b.m_gradient)
            return a.m_gradient == b.m_gradient;
        return *a.m_gradient == *b.m_gradient;
    }
    friend bool operator!=(const QBrush &a, const QBrush &b) noexcept { return !(a == b); }

private:
    QColor m_color{Qt::black};
    Qt::BrushStyle m_style = Qt::NoBrush;
    std::shared_ptr<const QGradient> m_gradient;
    QTransform m_transform;
};
