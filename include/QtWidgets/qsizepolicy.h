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
    enum ControlType {
        DefaultType = 0x00000001,
        ButtonBox = 0x00000002,
        CheckBox = 0x00000004,
        ComboBox = 0x00000008,
        Frame = 0x00000010,
        GroupBox = 0x00000020,
        Label = 0x00000040,
        Line = 0x00000080,
        LineEdit = 0x00000100,
        PushButton = 0x00000200,
        RadioButton = 0x00000400,
        Slider = 0x00000800,
        SpinBox = 0x00001000,
        TabWidget = 0x00002000,
        ToolButton = 0x00004000
    };
    Q_DECLARE_FLAGS(ControlTypes, ControlType)

    constexpr QSizePolicy() noexcept = default;
    constexpr QSizePolicy(Policy horizontal, Policy vertical) noexcept : m_horizontal(horizontal), m_vertical(vertical) {}
    constexpr QSizePolicy(Policy horizontal, Policy vertical, ControlType type) noexcept
        : m_horizontal(horizontal), m_vertical(vertical), m_controlType(type) {}

    constexpr Policy horizontalPolicy() const noexcept { return m_horizontal; }
    constexpr Policy verticalPolicy() const noexcept { return m_vertical; }
    constexpr void setHorizontalPolicy(Policy policy) noexcept { m_horizontal = policy; }
    constexpr void setVerticalPolicy(Policy policy) noexcept { m_vertical = policy; }
    constexpr int horizontalStretch() const noexcept { return m_horizontalStretch; }
    constexpr int verticalStretch() const noexcept { return m_verticalStretch; }
    constexpr void setHorizontalStretch(int stretch) noexcept { m_horizontalStretch = stretch; }
    constexpr void setVerticalStretch(int stretch) noexcept { m_verticalStretch = stretch; }
    constexpr bool hasHeightForWidth() const noexcept { return m_heightForWidth; }
    constexpr void setHeightForWidth(bool b) noexcept { m_heightForWidth = b; }
    constexpr bool hasWidthForHeight() const noexcept { return m_widthForHeight; }
    constexpr void setWidthForHeight(bool b) noexcept { m_widthForHeight = b; }
    constexpr bool retainSizeWhenHidden() const noexcept { return m_retainSizeWhenHidden; }
    constexpr void setRetainSizeWhenHidden(bool retainSize) noexcept { m_retainSizeWhenHidden = retainSize; }
    constexpr ControlType controlType() const noexcept { return m_controlType; }
    constexpr void setControlType(ControlType type) noexcept { m_controlType = type; }
    constexpr void transpose() noexcept { *this = transposed(); }
    constexpr QSizePolicy transposed() const noexcept {
        QSizePolicy t(m_vertical, m_horizontal);
        t.m_horizontalStretch = m_verticalStretch;
        t.m_verticalStretch = m_horizontalStretch;
        t.m_heightForWidth = m_widthForHeight;
        t.m_widthForHeight = m_heightForWidth;
        t.m_retainSizeWhenHidden = m_retainSizeWhenHidden;
        t.m_controlType = m_controlType;
        return t;
    }
    constexpr Qt::Orientations expandingDirections() const noexcept {
        Qt::Orientations o;
        if (static_cast<int>(m_vertical) & ExpandFlag)
            o |= Qt::Vertical;
        if (static_cast<int>(m_horizontal) & ExpandFlag)
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
    bool m_heightForWidth = false;
    bool m_widthForHeight = false;
    bool m_retainSizeWhenHidden = false;
    ControlType m_controlType = DefaultType;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSizePolicy::ControlTypes)
