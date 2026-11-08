// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtGui/qcolor.h>

class QBrush {
public:
    QBrush() noexcept = default;
    QBrush(Qt::BrushStyle style) noexcept : m_style(style) {}
    QBrush(const QColor &color, Qt::BrushStyle style = Qt::SolidPattern) noexcept : m_color(color), m_style(style) {}
    QBrush(Qt::GlobalColor color, Qt::BrushStyle style = Qt::SolidPattern) noexcept : m_color(color), m_style(style) {}

    Qt::BrushStyle style() const noexcept { return m_style; }
    void setStyle(Qt::BrushStyle style) noexcept { m_style = style; }
    const QColor &color() const noexcept { return m_color; }
    void setColor(const QColor &color) noexcept { m_color = color; }
    void setColor(Qt::GlobalColor color) noexcept { m_color = QColor(color); }

    friend bool operator==(const QBrush &a, const QBrush &b) noexcept {
        return a.m_style == b.m_style && a.m_color == b.m_color;
    }
    friend bool operator!=(const QBrush &a, const QBrush &b) noexcept { return !(a == b); }

private:
    QColor m_color{Qt::black};
    Qt::BrushStyle m_style = Qt::NoBrush;
};
