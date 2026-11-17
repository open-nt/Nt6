// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QVariant;

class QFont {
public:
    operator QVariant() const;
    enum Weight {
        Thin = 100,
        ExtraLight = 200,
        Light = 300,
        Normal = 400,
        Medium = 500,
        DemiBold = 600,
        Bold = 700,
        ExtraBold = 800,
        Black = 900
    };

    enum StyleHint {
        Helvetica,
        SansSerif = Helvetica,
        Times,
        Serif = Times,
        Courier,
        TypeWriter = Courier,
        OldEnglish,
        Decorative = OldEnglish,
        System,
        AnyStyle,
        Cursive,
        Monospace,
        Fantasy
    };

    enum StyleStrategy {
        PreferDefault = 0x0001,
        PreferBitmap = 0x0002,
        PreferDevice = 0x0004,
        PreferOutline = 0x0008,
        ForceOutline = 0x0010,
        PreferMatch = 0x0020,
        PreferQuality = 0x0040,
        PreferAntialias = 0x0080,
        NoAntialias = 0x0100,
        NoSubpixelAntialias = 0x0800,
        PreferNoShaping = 0x1000,
        NoFontMerging = 0x8000
    };

    enum HintingPreference {
        PreferDefaultHinting = 0,
        PreferNoHinting = 1,
        PreferVerticalHinting = 2,
        PreferFullHinting = 3
    };

    enum Stretch {
        AnyStretch = 0,
        UltraCondensed = 50,
        ExtraCondensed = 62,
        Condensed = 75,
        SemiCondensed = 87,
        Unstretched = 100,
        SemiExpanded = 112,
        Expanded = 125,
        ExtraExpanded = 150,
        UltraExpanded = 200
    };

    enum SpacingType { PercentageSpacing, AbsoluteSpacing };
    /*!
        \enum QFont::Style
        \since 6.0

        The slant of a font.

        \value StyleNormal Upright.
        \value StyleItalic Italic.
        \value StyleOblique Slanted; Nt draws it italic.
    */
    enum Style { StyleNormal, StyleItalic, StyleOblique };

    QFont();
    explicit QFont(const QString &family, int pointSize = -1, int weight = -1, bool italic = false);

    QString family() const { return m_family; }
    void setFamily(const QString &family) {
        m_family = family;
        m_families.clear();
    }
    QStringList families() const;
    void setFamilies(const QStringList &families);
    StyleHint styleHint() const { return static_cast<StyleHint>(m_styleHint); }
    StyleStrategy styleStrategy() const { return static_cast<StyleStrategy>(m_styleStrategy); }
    void setStyleHint(StyleHint hint, StyleStrategy strategy = PreferDefault);
    void setStyleStrategy(StyleStrategy strategy) { m_styleStrategy = static_cast<int>(strategy); }
    HintingPreference hintingPreference() const { return static_cast<HintingPreference>(m_hinting); }
    void setHintingPreference(HintingPreference hintingPreference) { m_hinting = static_cast<int>(hintingPreference); }
    int stretch() const { return m_stretch; }
    void setStretch(int factor);
    bool fixedPitch() const;
    void setFixedPitch(bool enable);
    qreal letterSpacing() const { return m_letterSpacing; }
    SpacingType letterSpacingType() const { return static_cast<SpacingType>(m_letterSpacingType); }
    void setLetterSpacing(SpacingType type, qreal spacing);
    qreal wordSpacing() const { return m_wordSpacing; }
    void setWordSpacing(qreal spacing) { m_wordSpacing = spacing; }
    QString toString() const;
    bool fromString(const QString &description);
    int pointSize() const;
    void setPointSize(int pointSize);
    qreal pointSizeF() const { return m_pointSize; }
    void setPointSizeF(qreal pointSize);
    int pixelSize() const { return m_pixelSize; }
    void setPixelSize(int pixelSize);
    Weight weight() const { return static_cast<Weight>(m_weight); }
    void setWeight(Weight weight) { m_weight = static_cast<int>(weight); }
    bool bold() const { return m_weight >= Bold; }
    void setBold(bool enable) { m_weight = enable ? Bold : Normal; }
    bool italic() const { return m_italic; }
    void setItalic(bool enable) { m_italic = enable; }
    Style style() const { return m_italic ? StyleItalic : StyleNormal; }
    void setStyle(Style style) { m_italic = style != StyleNormal; }
    bool underline() const { return m_underline; }
    void setUnderline(bool enable) { m_underline = enable; }
    bool overline() const { return m_overline; }
    void setOverline(bool enable) { m_overline = enable; }
    bool strikeOut() const { return m_strikeOut; }
    void setStrikeOut(bool enable) { m_strikeOut = enable; }

    friend bool operator==(const QFont &a, const QFont &b) noexcept {
        return a.m_family == b.m_family && a.m_families == b.m_families && a.m_pointSize == b.m_pointSize &&
               a.m_pixelSize == b.m_pixelSize && a.m_weight == b.m_weight && a.m_italic == b.m_italic &&
               a.m_styleHint == b.m_styleHint && a.m_styleStrategy == b.m_styleStrategy && a.m_stretch == b.m_stretch &&
               a.m_letterSpacingType == b.m_letterSpacingType && a.m_letterSpacing == b.m_letterSpacing &&
               a.m_wordSpacing == b.m_wordSpacing && a.m_hinting == b.m_hinting && a.m_underline == b.m_underline &&
               a.m_overline == b.m_overline && a.m_strikeOut == b.m_strikeOut;
    }
    friend bool operator!=(const QFont &a, const QFont &b) noexcept { return !(a == b); }

private:
    QString m_family;
    QStringList m_families;
    qreal m_pointSize = -1.0;
    int m_pixelSize = -1;
    int m_weight = Normal;
    bool m_italic = false;
    bool m_underline = false;
    bool m_overline = false;
    bool m_strikeOut = false;
    int m_styleHint = AnyStyle;
    int m_styleStrategy = PreferDefault;
    int m_stretch = AnyStretch;
    int m_letterSpacingType = PercentageSpacing;
    qreal m_letterSpacing = 100.0; // percent of the advance, or pixels added to it
    qreal m_wordSpacing = 0.0;     // pixels added to the advance of a space
    int m_hinting = PreferDefaultHinting;
};
