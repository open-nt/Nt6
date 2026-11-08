// MIT License - see LICENSE
#pragma once

#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>

class QPalette {
public:
    enum ColorGroup { Active, Disabled, Inactive, NColorGroups, Current, All, Normal = Active };
    enum ColorRole {
        WindowText,
        Button,
        Light,
        Midlight,
        Dark,
        Mid,
        Text,
        BrightText,
        ButtonText,
        Base,
        Window,
        Shadow,
        Highlight,
        HighlightedText,
        Link,
        LinkVisited,
        AlternateBase,
        NoRole,
        ToolTipBase,
        ToolTipText,
        PlaceholderText,
        NColorRoles,
        Foreground = WindowText,
        Background = Window
    };

    QPalette();
    QPalette(const QColor &button);
    QPalette(const QColor &button, const QColor &window);

    ColorGroup currentColorGroup() const { return m_group; }
    void setCurrentColorGroup(ColorGroup group) { m_group = group; }

    const QBrush &brush(ColorGroup group, ColorRole role) const;
    const QBrush &brush(ColorRole role) const { return brush(Current, role); }
    void setBrush(ColorGroup group, ColorRole role, const QBrush &brush);
    void setBrush(ColorRole role, const QBrush &brush) { setBrush(All, role, brush); }
    const QColor &color(ColorGroup group, ColorRole role) const { return brush(group, role).color(); }
    const QColor &color(ColorRole role) const { return brush(Current, role).color(); }
    void setColor(ColorGroup group, ColorRole role, const QColor &color) { setBrush(group, role, QBrush(color)); }
    void setColor(ColorRole role, const QColor &color) { setBrush(All, role, QBrush(color)); }

    const QBrush &windowText() const { return brush(WindowText); }
    const QBrush &button() const { return brush(Button); }
    const QBrush &light() const { return brush(Light); }
    const QBrush &midlight() const { return brush(Midlight); }
    const QBrush &dark() const { return brush(Dark); }
    const QBrush &mid() const { return brush(Mid); }
    const QBrush &text() const { return brush(Text); }
    const QBrush &brightText() const { return brush(BrightText); }
    const QBrush &buttonText() const { return brush(ButtonText); }
    const QBrush &base() const { return brush(Base); }
    const QBrush &window() const { return brush(Window); }
    const QBrush &shadow() const { return brush(Shadow); }
    const QBrush &highlight() const { return brush(Highlight); }
    const QBrush &highlightedText() const { return brush(HighlightedText); }
    const QBrush &link() const { return brush(Link); }
    const QBrush &linkVisited() const { return brush(LinkVisited); }
    const QBrush &alternateBase() const { return brush(AlternateBase); }
    const QBrush &placeholderText() const { return brush(PlaceholderText); }

    friend bool operator==(const QPalette &a, const QPalette &b) noexcept {
        for (int g = 0; g < NColorGroups; ++g)
            for (int r = 0; r < NColorRoles; ++r)
                if (a.m_brushes[g][r] != b.m_brushes[g][r])
                    return false;
        return true;
    }
    friend bool operator!=(const QPalette &a, const QPalette &b) noexcept { return !(a == b); }

private:
    QBrush m_brushes[NColorGroups][NColorRoles];
    ColorGroup m_group = Active;
};
