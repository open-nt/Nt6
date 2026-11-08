// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qnamespace.h>

class QSizePolicy {
public:
    enum PolicyFlag { GrowFlag = 1, ExpandFlag = 2, ShrinkFlag = 4, IgnoreFlag = 8 };
    enum Policy {
        Fixed = 0,
        Minimum = GrowFlag,
        Maximum = ShrinkFlag,
        Preferred = GrowFlag | ShrinkFlag,
        MinimumExpanding = GrowFlag | ExpandFlag,
        Expanding = GrowFlag | ShrinkFlag | ExpandFlag,
        Ignored = ShrinkFlag | GrowFlag | IgnoreFlag
    };

    constexpr QSizePolicy() noexcept = default;
    constexpr QSizePolicy(Policy horizontal, Policy vertical) noexcept : m_horizontal(horizontal), m_vertical(vertical) {}

    constexpr Policy horizontalPolicy() const noexcept { return m_horizontal; }
    constexpr Policy verticalPolicy() const noexcept { return m_vertical; }
    constexpr void setHorizontalPolicy(Policy policy) noexcept { m_horizontal = policy; }
    constexpr void setVerticalPolicy(Policy policy) noexcept { m_vertical = policy; }
    constexpr int horizontalStretch() const noexcept { return m_horizontalStretch; }
    constexpr int verticalStretch() const noexcept { return m_verticalStretch; }
    constexpr void setHorizontalStretch(int stretch) noexcept { m_horizontalStretch = stretch; }
    constexpr void setVerticalStretch(int stretch) noexcept { m_verticalStretch = stretch; }
    constexpr Qt::Orientations expandingDirections() const noexcept {
        Qt::Orientations o;
        if (m_vertical & ExpandFlag)
            o |= Qt::Vertical;
        if (m_horizontal & ExpandFlag)
            o |= Qt::Horizontal;
        return o;
    }

    friend constexpr bool operator==(const QSizePolicy &a, const QSizePolicy &b) noexcept {
        return a.m_horizontal == b.m_horizontal && a.m_vertical == b.m_vertical
               && a.m_horizontalStretch == b.m_horizontalStretch && a.m_verticalStretch == b.m_verticalStretch;
    }
    friend constexpr bool operator!=(const QSizePolicy &a, const QSizePolicy &b) noexcept { return !(a == b); }

private:
    Policy m_horizontal = Preferred;
    Policy m_vertical = Preferred;
    int m_horizontalStretch = 0;
    int m_verticalStretch = 0;
};
