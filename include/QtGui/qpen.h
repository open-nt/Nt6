// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>

class QPen {
public:
    QPen() noexcept = default;
    QPen(Qt::PenStyle style) noexcept : m_style(style) {}
    QPen(const QColor &color) noexcept : m_brush(color) {}
    QPen(const QBrush &brush, qreal width, Qt::PenStyle style = Qt::SolidLine, Qt::PenCapStyle cap = Qt::SquareCap,
         Qt::PenJoinStyle join = Qt::BevelJoin) noexcept
        : m_brush(brush), m_width(width), m_style(style), m_cap(cap), m_join(join) {}

    Qt::PenStyle style() const noexcept { return m_style; }
    void setStyle(Qt::PenStyle style) noexcept { m_style = style; }
    QColor color() const noexcept { return m_brush.color(); }
    void setColor(const QColor &color) noexcept { m_brush.setColor(color); }
    QBrush brush() const noexcept { return m_brush; }
    void setBrush(const QBrush &brush) noexcept { m_brush = brush; }
    int width() const noexcept { return qRound(m_width); }
    void setWidth(int width) noexcept { m_width = width; }
    qreal widthF() const noexcept { return m_width; }
    void setWidthF(qreal width) noexcept { m_width = width; }

    Qt::PenCapStyle capStyle() const noexcept { return m_cap; }
    void setCapStyle(Qt::PenCapStyle style) noexcept { m_cap = style; }
    Qt::PenJoinStyle joinStyle() const noexcept { return m_join; }
    void setJoinStyle(Qt::PenJoinStyle style) noexcept { m_join = style; }
    qreal miterLimit() const noexcept { return m_miterLimit; }
    void setMiterLimit(qreal limit) noexcept { m_miterLimit = limit; }
    QList<qreal> dashPattern() const;
    void setDashPattern(const QList<qreal> &pattern);
    qreal dashOffset() const noexcept { return m_dashOffset; }
    void setDashOffset(qreal offset) noexcept { m_dashOffset = offset; }
    bool isCosmetic() const noexcept { return m_cosmetic || m_width == 0.0; }
    void setCosmetic(bool cosmetic) noexcept { m_cosmetic = cosmetic; }
    bool isSolid() const noexcept { return m_style == Qt::SolidLine; }

    friend bool operator==(const QPen &a, const QPen &b) noexcept {
        return a.m_style == b.m_style && a.m_width == b.m_width && a.m_brush == b.m_brush && a.m_cap == b.m_cap &&
               a.m_join == b.m_join && a.m_miterLimit == b.m_miterLimit && a.m_dashes == b.m_dashes &&
               a.m_dashOffset == b.m_dashOffset && a.m_cosmetic == b.m_cosmetic;
    }
    friend bool operator!=(const QPen &a, const QPen &b) noexcept { return !(a == b); }

private:
    QBrush m_brush{Qt::black, Qt::SolidPattern};
    qreal m_width = 1.0;
    Qt::PenStyle m_style = Qt::SolidLine;
    Qt::PenCapStyle m_cap = Qt::SquareCap;
    Qt::PenJoinStyle m_join = Qt::BevelJoin;
    qreal m_miterLimit = 2.0;
    QList<qreal> m_dashes;
    qreal m_dashOffset = 0.0;
    bool m_cosmetic = false;
};
