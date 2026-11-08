// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>

class QPen {
public:
    QPen() noexcept = default;
    QPen(Qt::PenStyle style) noexcept : m_style(style) {}
    QPen(const QColor &color) noexcept : m_brush(color) {}
    QPen(const QBrush &brush, qreal width, Qt::PenStyle style = Qt::SolidLine) noexcept
        : m_brush(brush), m_width(width), m_style(style) {}

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

    friend bool operator==(const QPen &a, const QPen &b) noexcept {
        return a.m_style == b.m_style && a.m_width == b.m_width && a.m_brush == b.m_brush;
    }
    friend bool operator!=(const QPen &a, const QPen &b) noexcept { return !(a == b); }

private:
    QBrush m_brush{Qt::black, Qt::SolidPattern};
    qreal m_width = 1.0;
    Qt::PenStyle m_style = Qt::SolidLine;
};
