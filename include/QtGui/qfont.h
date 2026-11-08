// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>

class QFont {
public:
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

    QFont();
    explicit QFont(const QString &family, int pointSize = -1, int weight = -1, bool italic = false);

    QString family() const { return m_family; }
    void setFamily(const QString &family) { m_family = family; }
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

    friend bool operator==(const QFont &a, const QFont &b) noexcept {
        return a.m_family == b.m_family && a.m_pointSize == b.m_pointSize && a.m_pixelSize == b.m_pixelSize &&
               a.m_weight == b.m_weight && a.m_italic == b.m_italic;
    }
    friend bool operator!=(const QFont &a, const QFont &b) noexcept { return !(a == b); }

private:
    QString m_family;
    qreal m_pointSize = -1.0;
    int m_pixelSize = -1;
    int m_weight = Normal;
    bool m_italic = false;
};
