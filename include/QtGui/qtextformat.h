// MIT License - see LICENSE
#pragma once

#include <QtCore/qmap.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>

class QTextBlockFormat;
class QTextCharFormat;
class QTextListFormat;

class QTextFormat {
public:
    /*!
        \enum QTextFormat::FormatType

        The kinds of formats.

        \value InvalidFormat A format that was default-constructed.
        \value BlockFormat The format of a paragraph; see QTextBlockFormat.
        \value CharFormat The format of characters; see QTextCharFormat.
        \value ListFormat The format of a list; see QTextListFormat.
        \value UserFormat The first number for formats of an application.
    */
    enum FormatType { InvalidFormat = -1, BlockFormat = 1, CharFormat = 2, ListFormat = 3, UserFormat = 100 };

    /*!
        \enum QTextFormat::Property

        The properties that a format can have, by the numbers that Qt gives
        them. Nt has the ones listed here.

        \value BackgroundBrush The brush behind the text.
        \value ForegroundBrush The brush that the text is drawn with.
        \value BlockAlignment The alignment of a paragraph.
        \value BlockTopMargin The space above a paragraph, in pixels.
        \value BlockBottomMargin The space below a paragraph.
        \value BlockLeftMargin The space left of a paragraph.
        \value BlockRightMargin The space right of a paragraph.
        \value TextIndent The indentation of the first line of a paragraph.
        \value BlockIndent The number of steps that a paragraph is indented by.
        \value HeadingLevel The level of a heading, 1 to 6; 0 for a paragraph that is none.
        \value FontFamilies The families of the font, in order of preference.
        \value FontPointSize The size of the font in points.
        \value FontWeight The weight of the font.
        \value FontItalic Whether the font is italic.
        \value FontUnderline Whether the text is underlined.
        \value FontStrikeOut Whether the text is struck out.
        \value FontFixedPitch Whether the font has a fixed pitch.
        \value IsAnchor Whether the text is a link.
        \value AnchorHref The target of the link.
        \value ListStyle The style of a list; see QTextListFormat::Style.
        \value ListIndent The number of steps that a list is indented by.
        \value UserProperty The first number for properties of an application.
    */
    enum Property {
        BackgroundBrush = 0x820,
        ForegroundBrush = 0x821,
        BlockAlignment = 0x1010,
        BlockTopMargin = 0x1030,
        BlockBottomMargin = 0x1031,
        BlockLeftMargin = 0x1032,
        BlockRightMargin = 0x1033,
        TextIndent = 0x1034,
        BlockIndent = 0x1040,
        HeadingLevel = 0x1070,
        FontFamilies = 0x1FE0,
        FontPointSize = 0x2001,
        FontWeight = 0x2003,
        FontItalic = 0x2004,
        FontUnderline = 0x2005,
        FontStrikeOut = 0x2007,
        FontFixedPitch = 0x2008,
        IsAnchor = 0x2030,
        AnchorHref = 0x2031,
        ListStyle = 0x3000,
        ListIndent = 0x3001,
        UserProperty = 0x100000
    };

    QTextFormat();
    explicit QTextFormat(int type);

    void merge(const QTextFormat &other);
    bool isValid() const { return m_type != InvalidFormat; }
    bool isEmpty() const { return m_properties.isEmpty(); }
    int type() const { return m_type; }

    bool boolProperty(int propertyId) const;
    int intProperty(int propertyId) const;
    qreal doubleProperty(int propertyId) const;
    QString stringProperty(int propertyId) const;
    QBrush brushProperty(int propertyId) const;
    QVariant property(int propertyId) const;
    void setProperty(int propertyId, const QVariant &value);
    void clearProperty(int propertyId);
    bool hasProperty(int propertyId) const;
    int propertyCount() const { return static_cast<int>(m_properties.size()); }
    QMap<int, QVariant> properties() const { return m_properties; }

    bool isCharFormat() const { return m_type == CharFormat; }
    bool isBlockFormat() const { return m_type == BlockFormat; }
    bool isListFormat() const { return m_type == ListFormat; }

    QTextBlockFormat toBlockFormat() const;
    QTextCharFormat toCharFormat() const;
    QTextListFormat toListFormat() const;

    void setBackground(const QBrush &brush) { setProperty(BackgroundBrush, QVariant::fromValue(brush)); }
    QBrush background() const { return brushProperty(BackgroundBrush); }
    void clearBackground() { clearProperty(BackgroundBrush); }
    void setForeground(const QBrush &brush) { setProperty(ForegroundBrush, QVariant::fromValue(brush)); }
    QBrush foreground() const { return brushProperty(ForegroundBrush); }
    void clearForeground() { clearProperty(ForegroundBrush); }

    bool operator==(const QTextFormat &other) const;
    /*!
        Returns \c true if this format differs from \a other; otherwise
        returns \c false.
    */
    bool operator!=(const QTextFormat &other) const { return !operator==(other); }

private:
    int m_type;
    QMap<int, QVariant> m_properties;
};

class QTextCharFormat : public QTextFormat {
public:
    QTextCharFormat();

    void setFont(const QFont &font);
    QFont font() const;

    void setFontFamilies(const QStringList &families) { setProperty(FontFamilies, families); }
    QStringList fontFamilies() const { return property(FontFamilies).toStringList(); }
    void setFontPointSize(qreal size) { setProperty(FontPointSize, size); }
    qreal fontPointSize() const { return doubleProperty(FontPointSize); }
    void setFontWeight(int weight) { setProperty(FontWeight, weight); }
    int fontWeight() const { return hasProperty(FontWeight) ? intProperty(FontWeight) : int(QFont::Normal); }
    void setFontItalic(bool italic) { setProperty(FontItalic, italic); }
    bool fontItalic() const { return boolProperty(FontItalic); }
    void setFontUnderline(bool underline) { setProperty(FontUnderline, underline); }
    bool fontUnderline() const { return boolProperty(FontUnderline); }
    void setFontStrikeOut(bool strikeOut) { setProperty(FontStrikeOut, strikeOut); }
    bool fontStrikeOut() const { return boolProperty(FontStrikeOut); }
    void setFontFixedPitch(bool fixedPitch) { setProperty(FontFixedPitch, fixedPitch); }
    bool fontFixedPitch() const { return boolProperty(FontFixedPitch); }

    void setAnchor(bool anchor) { setProperty(IsAnchor, anchor); }
    bool isAnchor() const { return boolProperty(IsAnchor); }
    void setAnchorHref(const QString &value) { setProperty(AnchorHref, value); }
    QString anchorHref() const { return stringProperty(AnchorHref); }
};

class QTextBlockFormat : public QTextFormat {
public:
    QTextBlockFormat();

    void setAlignment(Qt::Alignment alignment) { setProperty(BlockAlignment, static_cast<int>(alignment)); }
    Qt::Alignment alignment() const;
    void setTopMargin(qreal margin) { setProperty(BlockTopMargin, margin); }
    qreal topMargin() const { return doubleProperty(BlockTopMargin); }
    void setBottomMargin(qreal margin) { setProperty(BlockBottomMargin, margin); }
    qreal bottomMargin() const { return doubleProperty(BlockBottomMargin); }
    void setLeftMargin(qreal margin) { setProperty(BlockLeftMargin, margin); }
    qreal leftMargin() const { return doubleProperty(BlockLeftMargin); }
    void setRightMargin(qreal margin) { setProperty(BlockRightMargin, margin); }
    qreal rightMargin() const { return doubleProperty(BlockRightMargin); }
    void setTextIndent(qreal indent) { setProperty(TextIndent, indent); }
    qreal textIndent() const { return doubleProperty(TextIndent); }
    void setIndent(int indent) { setProperty(BlockIndent, indent); }
    int indent() const { return intProperty(BlockIndent); }
    void setHeadingLevel(int level) { setProperty(HeadingLevel, level); }
    int headingLevel() const { return intProperty(HeadingLevel); }
};

class QTextListFormat : public QTextFormat {
public:
    /*!
        \enum QTextListFormat::Style

        What the items of a list are marked with.

        \value ListDisc A filled circle.
        \value ListCircle An empty circle.
        \value ListSquare A filled square.
        \value ListDecimal Numbers: 1, 2, 3.
        \value ListLowerAlpha Small letters: a, b, c.
        \value ListUpperAlpha Capital letters: A, B, C.
        \omitvalue ListStyleUndefined
    */
    enum Style {
        ListDisc = -1,
        ListCircle = -2,
        ListSquare = -3,
        ListDecimal = -4,
        ListLowerAlpha = -5,
        ListUpperAlpha = -6,
        ListStyleUndefined = 0
    };

    QTextListFormat();

    void setStyle(Style style) { setProperty(ListStyle, static_cast<int>(style)); }
    Style style() const { return static_cast<Style>(intProperty(ListStyle)); }
    void setIndent(int indent) { setProperty(ListIndent, indent); }
    int indent() const { return intProperty(ListIndent); }
};
