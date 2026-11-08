// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>

#include <vector>

class QRegion {
public:
    QRegion() = default;
    QRegion(const QRect &rect);
    QRegion(int x, int y, int w, int h);

    bool isEmpty() const noexcept { return m_rects.empty(); }
    bool isNull() const noexcept { return m_rects.empty(); }
    QRect boundingRect() const noexcept;
    std::vector<QRect> rects() const { return m_rects; }
    int rectCount() const noexcept { return static_cast<int>(m_rects.size()); }
    bool contains(const QPoint &point) const noexcept;
    bool intersects(const QRect &rect) const noexcept;

    QRegion united(const QRect &rect) const;
    QRegion united(const QRegion &other) const;
    QRegion intersected(const QRect &rect) const;
    QRegion intersected(const QRegion &other) const;
    QRegion translated(int dx, int dy) const;
    QRegion translated(const QPoint &offset) const { return translated(offset.x(), offset.y()); }
    void translate(int dx, int dy);

    QRegion &operator+=(const QRect &rect);
    QRegion &operator+=(const QRegion &other);
    QRegion operator+(const QRect &rect) const { return united(rect); }
    QRegion operator+(const QRegion &other) const { return united(other); }
    QRegion operator&(const QRect &rect) const { return intersected(rect); }
    QRegion operator&(const QRegion &other) const { return intersected(other); }
    QRegion &operator&=(const QRect &rect);

    friend bool operator==(const QRegion &a, const QRegion &b) { return a.m_rects == b.m_rects; }
    friend bool operator!=(const QRegion &a, const QRegion &b) { return !(a == b); }

private:
    std::vector<QRect> m_rects;
};
