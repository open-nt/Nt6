// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtGui/qrgb.h>

class QVariant;

class QColor {
public:
    operator QVariant() const;
    enum Spec { Invalid, Rgb, Hsv };
    enum NameFormat { HexRgb, HexArgb };

    constexpr QColor() noexcept = default;
    constexpr QColor(Qt::GlobalColor color) noexcept : QColor(FromRgba(), globalRgb(color)) {}
    constexpr QColor(int r, int g, int b, int a = 255) noexcept
        : m_r(byte(r)), m_g(byte(g)), m_b(byte(b)), m_a(byte(a)), m_valid(true) {}
    constexpr QColor(QRgb rgb) noexcept : QColor(qRed(rgb), qGreen(rgb), qBlue(rgb)) {}
    QColor(const QString &name);
    QColor(const char *name);

    constexpr bool isValid() const noexcept { return m_valid; }
    constexpr Spec spec() const noexcept { return m_valid ? Rgb : Invalid; }

    QString name(NameFormat format = HexRgb) const;
    void setNamedColor(const QString &name);
    static bool isValidColor(const QString &name);
    static QColor fromString(const QString &name);

    constexpr int red() const noexcept { return m_r; }
    constexpr int green() const noexcept { return m_g; }
    constexpr int blue() const noexcept { return m_b; }
    constexpr int alpha() const noexcept { return m_a; }
    void setRed(int red);
    void setGreen(int green);
    void setBlue(int blue);
    void setAlpha(int alpha);

    constexpr qreal redF() const noexcept { return m_r / 255.0; }
    constexpr qreal greenF() const noexcept { return m_g / 255.0; }
    constexpr qreal blueF() const noexcept { return m_b / 255.0; }
    constexpr qreal alphaF() const noexcept { return m_a / 255.0; }
    void setRedF(qreal red);
    void setGreenF(qreal green);
    void setBlueF(qreal blue);
    void setAlphaF(qreal alpha);

    void getRgb(int *r, int *g, int *b, int *a = nullptr) const;
    void setRgb(int r, int g, int b, int a = 255);
    void setRgb(QRgb rgb);
    void setRgba(QRgb rgba);
    constexpr QRgb rgb() const noexcept { return qRgb(m_r, m_g, m_b); }
    constexpr QRgb rgba() const noexcept { return qRgba(m_r, m_g, m_b, m_a); }
    static QColor fromRgb(int r, int g, int b, int a = 255);
    static QColor fromRgb(QRgb rgb);
    static QColor fromRgba(QRgb rgba);
    static QColor fromRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);

    int hue() const noexcept;
    int saturation() const noexcept;
    int value() const noexcept;
    int lightness() const noexcept;
    qreal lightnessF() const noexcept;
    static QColor fromHsv(int h, int s, int v, int a = 255);

    // HSL. The color is stored as RGB: toHsl() returns the same color.
    int hslHue() const noexcept;
    int hslSaturation() const noexcept;
    qreal hslHueF() const noexcept;
    qreal hslSaturationF() const noexcept;
    void getHsl(int *h, int *s, int *l, int *a = nullptr) const;
    void getHslF(float *h, float *s, float *l, float *a = nullptr) const;
    void setHsl(int h, int s, int l, int a = 255);
    void setHslF(float h, float s, float l, float a = 1.0F);
    static QColor fromHsl(int h, int s, int l, int a = 255);
    static QColor fromHslF(float h, float s, float l, float a = 1.0F);
    QColor toHsl() const noexcept { return *this; }

    QColor lighter(int factor = 150) const;
    QColor darker(int factor = 200) const;

    friend constexpr bool operator==(const QColor &a, const QColor &b) noexcept {
        return a.m_valid == b.m_valid && (!a.m_valid || (a.rgba() == b.rgba()));
    }
    friend constexpr bool operator!=(const QColor &a, const QColor &b) noexcept { return !(a == b); }

private:
    struct FromRgba {};
    constexpr QColor(FromRgba, QRgb rgba) noexcept : QColor(qRed(rgba), qGreen(rgba), qBlue(rgba), qAlpha(rgba)) {}
    static constexpr unsigned char byte(int value) noexcept {
        return static_cast<unsigned char>(value < 0 ? 0 : value > 255 ? 255 : value);
    }
    static constexpr QRgb globalRgb(Qt::GlobalColor color) noexcept {
        constexpr QRgb colors[] = {
            0xff000000U, // color0 (opaque black as a color)
            0xffffffffU, // color1
            0xff000000U, // black
            0xffffffffU, // white
            0xff808080U, // darkGray
            0xffa0a0a4U, // gray
            0xffc0c0c0U, // lightGray
            0xffff0000U, // red
            0xff00ff00U, // green
            0xff0000ffU, // blue
            0xff00ffffU, // cyan
            0xffff00ffU, // magenta
            0xffffff00U, // yellow
            0xff800000U, // darkRed
            0xff008000U, // darkGreen
            0xff000080U, // darkBlue
            0xff008080U, // darkCyan
            0xff800080U, // darkMagenta
            0xff808000U, // darkYellow
            0x00000000U, // transparent
        };
        return colors[static_cast<int>(color)];
    }

    unsigned char m_r = 0;
    unsigned char m_g = 0;
    unsigned char m_b = 0;
    unsigned char m_a = 255;
    bool m_valid = false;
};

class QDebug;
QDebug operator<<(QDebug debug, const QColor &color);
