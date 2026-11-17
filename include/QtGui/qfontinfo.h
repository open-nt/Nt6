// MIT License - see LICENSE
#pragma once

#include <QtGui/qfont.h>

class QFontInfo {
public:
    explicit QFontInfo(const QFont &font) : m_font(font) {}
    QFontInfo(const QFontInfo &other) = default;
    QFontInfo &operator=(const QFontInfo &other) = default;
    ~QFontInfo() = default;

    QString family() const;
    QString styleName() const;
    int pixelSize() const;
    int pointSize() const;
    qreal pointSizeF() const;
    bool italic() const { return m_font.italic(); }
    QFont::Style style() const { return m_font.style(); }
    int weight() const { return static_cast<int>(m_font.weight()); }
    bool bold() const { return m_font.bold(); }
    bool underline() const { return m_font.underline(); }
    bool overline() const { return m_font.overline(); }
    bool strikeOut() const { return m_font.strikeOut(); }
    bool fixedPitch() const { return m_font.fixedPitch(); }
    QFont::StyleHint styleHint() const { return m_font.styleHint(); }
    bool exactMatch() const;

private:
    QFont m_font;
};
