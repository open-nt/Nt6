// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtGui/qrgb.h>

class QColor {
public:
    enum Spec { Invalid, Rgb, Hsv };
    enum NameFormat { HexRgb, HexArgb };

    QColor() noexcept = default;
    QColor(Qt::GlobalColor color) noexcept;
    QColor(int r, int g, int b, int a = 255) noexcept;
    QColor(QRgb rgb) noexcept;
    QColor(const QString &name);
    QColor(const char *name);

    bool isValid() const noexcept { return m_valid; }
    Spec spec() const noexcept { return m_valid ? Rgb : Invalid; }

    QString name(NameFormat format = HexRgb) const;
    void setNamedColor(const QString &name);
    static bool isValidColor(const QString &name);
    static QColor fromString(const QString &name);

    int red() const noexcept { return m_r; }
    int green() const noexcept { return m_g; }
    int blue() const noexcept { return m_b; }
    int alpha() const noexcept { return m_a; }
    void setRed(int red);
    void setGreen(int green);
    void setBlue(int blue);
    void setAlpha(int alpha);

    qreal redF() const noexcept { return m_r / 255.0; }
    qreal greenF() const noexcept { return m_g / 255.0; }
    qreal blueF() const noexcept { return m_b / 255.0; }
    qreal alphaF() const noexcept { return m_a / 255.0; }
    void setRedF(qreal red);
    void setGreenF(qreal green);
    void setBlueF(qreal blue);
    void setAlphaF(qreal alpha);

    void getRgb(int *r, int *g, int *b, int *a = nullptr) const;
    void setRgb(int r, int g, int b, int a = 255);
    void setRgb(QRgb rgb);
    void setRgba(QRgb rgba);
    QRgb rgb() const noexcept { return qRgb(m_r, m_g, m_b); }
    QRgb rgba() const noexcept { return qRgba(m_r, m_g, m_b, m_a); }
    static QColor fromRgb(int r, int g, int b, int a = 255);
    static QColor fromRgb(QRgb rgb);
    static QColor fromRgba(QRgb rgba);
    static QColor fromRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);

    int hue() const noexcept;
    int saturation() const noexcept;
    int value() const noexcept;
    static QColor fromHsv(int h, int s, int v, int a = 255);

    QColor lighter(int factor = 150) const;
    QColor darker(int factor = 200) const;

    friend bool operator==(const QColor &a, const QColor &b) noexcept {
        return a.m_valid == b.m_valid && (!a.m_valid || (a.rgba() == b.rgba()));
    }
    friend bool operator!=(const QColor &a, const QColor &b) noexcept { return !(a == b); }

private:
    unsigned char m_r = 0;
    unsigned char m_g = 0;
    unsigned char m_b = 0;
    unsigned char m_a = 255;
    bool m_valid = false;
};
