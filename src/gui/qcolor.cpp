// MIT License - see LICENSE
#include <QtGui/qcolor.h>

#include <QtCore/qvariant.h>

#include <algorithm>
#include <cmath>

namespace {

struct NamedColor {
    const char *name;
    QRgb rgb;
};

// The names Nt knows besides #rgb forms: the CSS basic colors and a few common extras.
constexpr NamedColor kNamedColors[] = {
    {"aqua", 0xff00ffffu},       {"black", 0xff000000u},       {"blue", 0xff0000ffu},        {"brown", 0xffa52a2au},
    {"cyan", 0xff00ffffu},       {"darkblue", 0xff00008bu},    {"darkcyan", 0xff008b8bu},    {"darkgray", 0xffa9a9a9u},
    {"darkgreen", 0xff006400u},  {"darkgrey", 0xffa9a9a9u},    {"darkmagenta", 0xff8b008bu}, {"darkred", 0xff8b0000u},
    {"fuchsia", 0xffff00ffu},    {"gold", 0xffffd700u},        {"gray", 0xff808080u},        {"green", 0xff008000u},
    {"grey", 0xff808080u},       {"indigo", 0xff4b0082u},      {"lightblue", 0xffadd8e6u},   {"lightgray", 0xffd3d3d3u},
    {"lightgreen", 0xff90ee90u}, {"lightgrey", 0xffd3d3d3u},   {"lime", 0xff00ff00u},        {"magenta", 0xffff00ffu},
    {"maroon", 0xff800000u},     {"navy", 0xff000080u},        {"olive", 0xff808000u},       {"orange", 0xffffa500u},
    {"pink", 0xffffc0cbu},       {"purple", 0xff800080u},      {"red", 0xffff0000u},         {"silver", 0xffc0c0c0u},
    {"teal", 0xff008080u},       {"transparent", 0x00000000u}, {"violet", 0xffee82eeu},      {"white", 0xffffffffu},
    {"yellow", 0xffffff00u},
};

int clamp255(int v) { return std::clamp(v, 0, 255); }

int hexDigit(QChar c) {
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9')
        return u - '0';
    if (u >= 'a' && u <= 'f')
        return u - 'a' + 10;
    if (u >= 'A' && u <= 'F')
        return u - 'A' + 10;
    return -1;
}

// Parses #RGB, #RRGGBB and #AARRGGBB; returns false if `s` is not one of them.
bool parseHex(const QString &s, QRgb *out) {
    if (s.size() < 2 || s.at(0) != QChar('#'))
        return false;
    const qsizetype digits = s.size() - 1;
    if (digits != 3 && digits != 6 && digits != 8)
        return false;
    unsigned values[8] = {};
    for (qsizetype i = 0; i < digits; ++i) {
        const int d = hexDigit(s.at(i + 1));
        if (d < 0)
            return false;
        values[i] = static_cast<unsigned>(d);
    }
    if (digits == 3)
        *out =
            qRgb(static_cast<int>(values[0] * 17), static_cast<int>(values[1] * 17), static_cast<int>(values[2] * 17));
    else if (digits == 6)
        *out = qRgb(static_cast<int>(values[0] * 16 + values[1]), static_cast<int>(values[2] * 16 + values[3]),
                    static_cast<int>(values[4] * 16 + values[5]));
    else
        *out = qRgba(static_cast<int>(values[2] * 16 + values[3]), static_cast<int>(values[4] * 16 + values[5]),
                     static_cast<int>(values[6] * 16 + values[7]), static_cast<int>(values[0] * 16 + values[1]));
    return true;
}

bool lookup(const QString &name, QRgb *out) {
    if (parseHex(name, out))
        return true;
    const QByteArray key = name.toLower().toLatin1();
    for (const NamedColor &c : kNamedColors) {
        if (key == c.name) {
            *out = c.rgb;
            return true;
        }
    }
    return false;
}

} // namespace

/*!
    \class QColor
    \brief The QColor class provides colors based on RGB values.

    A color is a red, green, blue and alpha value, each from 0 to 255. It can
    be built from components, a QRgb, a Qt::GlobalColor or a name such as
    \c{"#ff8000"} or \c{"steelblue"}:

    \code
    QColor orange(255, 128, 0);
    QColor translucent = QColor::fromRgb(0, 0, 255, 128);
    QColor named("#336699");
    \endcode

    A default-constructed QColor is invalid; isValid() returns \c false.

    \differences
    A color is always stored as RGB: there is no CMYK or extended RGB, HSV and
    HSL values are worked out from the RGB ones, so that they may come back off by
    one, spec() reports \c Rgb and toHsl() returns the color as it is. Each
    component has 8 bits, so the floating-point accessors are
    quantized to 1/255. Color names are the \c #rgb, \c #rrggbb and
    \c #aarrggbb forms and a short list of the CSS basic colors, not the full
    SVG list. There are no QVariant or QDataStream conversions.
    \enddifferences

    \sa QBrush, QPen, QImage
*/

/*!
    \enum QColor::Spec

    This enum describes how a color is specified.

    \value Invalid The color is not valid.
    \value Rgb The color is specified by red, green and blue.
    \value Hsv Accepted for compatibility; colors are always reported as Rgb.
*/

/*!
    \enum QColor::NameFormat

    This enum selects the form of name().

    \value HexRgb \c #RRGGBB.
    \value HexArgb \c #AARRGGBB.
*/

/*!
    \fn QColor::QColor()

    Constructs an invalid color.

    \sa isValid()
*/

/*!
    \fn QColor::QColor(Qt::GlobalColor color)

    Constructs a color for the predefined \a color.
*/

/*!
    \fn QColor::QColor(int r, int g, int b, int a)

    Constructs a color with the components \a r, \a g, \a b and \a a, each
    clamped to the range 0 to 255.
*/

/*!
    \fn QColor::QColor(QRgb rgb)

    Constructs an opaque color from \a rgb. The alpha byte of \a rgb is
    ignored; use fromRgba() to keep it.
*/

/*!
    Constructs a color from \a name, which is a hex form or a color name. The
    color is invalid if the name is not recognized.

    \sa setNamedColor()
*/
QColor::QColor(const QString &name) { setNamedColor(name); }

/*!
    \overload

    Constructs a color from the Latin-1 string \a name.
*/
QColor::QColor(const char *name) { setNamedColor(QString::fromLatin1(name)); }

/*!
    \fn bool QColor::isValid() const

    Returns \c true if the color is valid; otherwise returns \c false.
*/

/*!
    \fn QColor::Spec QColor::spec() const

    Returns \c Rgb for a valid color and \c Invalid otherwise.
*/

/*!
    Returns the name of the color in the given \a format: \c #RRGGBB for \c HexRgb
    and \c #AARRGGBB for \c HexArgb. An invalid color gives an empty string.

    \sa setNamedColor()
*/
QString QColor::name(NameFormat format) const {
    if (!m_valid)
        return QString();
    static const char digits[] = "0123456789abcdef";
    QString s = QString::fromLatin1("#");
    auto append = [&s](int v) {
        s.append(QChar(static_cast<ushort>(digits[(v >> 4) & 0xf])));
        s.append(QChar(static_cast<ushort>(digits[v & 0xf])));
    };
    if (format == HexArgb)
        append(m_a);
    append(m_r);
    append(m_g);
    append(m_b);
    return s;
}

/*!
    Sets the color to the one called \a name: \c #rgb, \c #rrggbb,
    \c #aarrggbb or a color name, in any case. The color becomes invalid if
    \a name is not recognized.

    \sa isValidColor(), name()
*/
void QColor::setNamedColor(const QString &name) {
    QRgb rgb = 0;
    if (!lookup(name, &rgb)) {
        *this = QColor();
        return;
    }
    *this = fromRgba(rgb);
}

/*!
    Returns \c true if \a name is a color name that setNamedColor() accepts.
*/
bool QColor::isValidColor(const QString &name) {
    QRgb rgb = 0;
    return lookup(name, &rgb);
}

/*!
    Returns the color called \a name, or an invalid color.

    \sa setNamedColor()
*/
QColor QColor::fromString(const QString &name) { return QColor(name); }

/*!
    \fn int QColor::red() const

    Returns the red component, from 0 to 255.

    \sa setRed(), redF()
*/

/*!
    \fn int QColor::green() const

    Returns the green component, from 0 to 255.

    \sa setGreen(), greenF()
*/

/*!
    \fn int QColor::blue() const

    Returns the blue component, from 0 to 255.

    \sa setBlue(), blueF()
*/

/*!
    \fn int QColor::alpha() const

    Returns the alpha component, from 0 (transparent) to 255 (opaque).

    \sa setAlpha(), alphaF()
*/

/*!
    Sets the red component to \a red, clamped to 0 to 255. The color becomes
    valid.

    \sa red()
*/
void QColor::setRed(int red) {
    m_r = static_cast<unsigned char>(clamp255(red));
    m_valid = true;
}

/*!
    Sets the green component to \a green, clamped to 0 to 255. The color
    becomes valid.

    \sa green()
*/
void QColor::setGreen(int green) {
    m_g = static_cast<unsigned char>(clamp255(green));
    m_valid = true;
}

/*!
    Sets the blue component to \a blue, clamped to 0 to 255. The color becomes
    valid.

    \sa blue()
*/
void QColor::setBlue(int blue) {
    m_b = static_cast<unsigned char>(clamp255(blue));
    m_valid = true;
}

/*!
    Sets the alpha component to \a alpha, clamped to 0 to 255. The color
    becomes valid.

    \sa alpha()
*/
void QColor::setAlpha(int alpha) {
    m_a = static_cast<unsigned char>(clamp255(alpha));
    m_valid = true;
}

/*!
    \fn qreal QColor::redF() const

    Returns the red component from 0.0 to 1.0.

    \sa red(), setRedF()
*/

/*!
    \fn qreal QColor::greenF() const

    Returns the green component from 0.0 to 1.0.

    \sa green(), setGreenF()
*/

/*!
    \fn qreal QColor::blueF() const

    Returns the blue component from 0.0 to 1.0.

    \sa blue(), setBlueF()
*/

/*!
    \fn qreal QColor::alphaF() const

    Returns the alpha component from 0.0 to 1.0.

    \sa alpha(), setAlphaF()
*/

/*!
    Sets the red component to \a red, from 0.0 to 1.0.

    \sa redF()
*/
void QColor::setRedF(qreal red) { setRed(qRound(red * 255.0)); }

/*!
    Sets the green component to \a green, from 0.0 to 1.0.

    \sa greenF()
*/
void QColor::setGreenF(qreal green) { setGreen(qRound(green * 255.0)); }

/*!
    Sets the blue component to \a blue, from 0.0 to 1.0.

    \sa blueF()
*/
void QColor::setBlueF(qreal blue) { setBlue(qRound(blue * 255.0)); }

/*!
    Sets the alpha component to \a alpha, from 0.0 to 1.0.

    \sa alphaF()
*/
void QColor::setAlphaF(qreal alpha) { setAlpha(qRound(alpha * 255.0)); }

/*!
    Stores the components in \a r, \a g, \a b and, if not null, \a a.

    \sa setRgb()
*/
void QColor::getRgb(int *r, int *g, int *b, int *a) const {
    *r = m_r;
    *g = m_g;
    *b = m_b;
    if (a)
        *a = m_a;
}

/*!
    Sets the color to the components \a r, \a g, \a b and \a a.

    \sa getRgb()
*/
void QColor::setRgb(int r, int g, int b, int a) { *this = QColor(r, g, b, a); }

/*!
    \overload

    Sets the color to \a rgb, with full opacity.
*/
void QColor::setRgb(QRgb rgb) { *this = QColor(rgb); }

/*!
    Sets the color to \a rgba, including its alpha byte.

    \sa rgba()
*/
void QColor::setRgba(QRgb rgba) { *this = fromRgba(rgba); }

/*!
    \fn QRgb QColor::rgb() const

    Returns the color as an opaque QRgb; the alpha component is dropped.

    \sa rgba()
*/

/*!
    \fn QRgb QColor::rgba() const

    Returns the color as a QRgb that includes the alpha component.

    \sa rgb()
*/

/*!
    Returns the color with the components \a r, \a g, \a b and \a a.
*/
QColor QColor::fromRgb(int r, int g, int b, int a) { return QColor(r, g, b, a); }

/*!
    \overload

    Returns the opaque color for \a rgb.
*/
QColor QColor::fromRgb(QRgb rgb) { return QColor(rgb); }

/*!
    Returns the color for \a rgba, including its alpha byte.
*/
QColor QColor::fromRgba(QRgb rgba) { return QColor(qRed(rgba), qGreen(rgba), qBlue(rgba), qAlpha(rgba)); }

/*!
    Returns the color with the components \a r, \a g, \a b and \a a, each from
    0.0 to 1.0.
*/
QColor QColor::fromRgbF(qreal r, qreal g, qreal b, qreal a) {
    return QColor(qRound(r * 255.0), qRound(g * 255.0), qRound(b * 255.0), qRound(a * 255.0));
}

namespace {

struct HsvValue {
    int h; // -1 for grays
    int s;
    int v;
};

HsvValue toHsv(int r, int g, int b) {
    const int mx = std::max({r, g, b});
    const int mn = std::min({r, g, b});
    const int delta = mx - mn;
    HsvValue out{-1, mx == 0 ? 0 : delta * 255 / mx, mx};
    if (delta == 0)
        return out;
    double h;
    if (mx == r)
        h = static_cast<double>(g - b) / delta;
    else if (mx == g)
        h = 2.0 + static_cast<double>(b - r) / delta;
    else
        h = 4.0 + static_cast<double>(r - g) / delta;
    h *= 60.0;
    if (h < 0.0)
        h += 360.0;
    out.h = static_cast<int>(h) % 360;
    return out;
}

} // namespace

/*!
    Returns the hue, from 0 to 359, or -1 for a gray.

    \sa saturation(), value()
*/
int QColor::hue() const noexcept { return toHsv(m_r, m_g, m_b).h; }

/*!
    Returns the HSV saturation, from 0 to 255.

    \sa hue(), value()
*/
int QColor::saturation() const noexcept { return toHsv(m_r, m_g, m_b).s; }

/*!
    Returns the HSV value, from 0 to 255.

    \sa hue(), saturation()
*/
int QColor::value() const noexcept { return toHsv(m_r, m_g, m_b).v; }

/*!
    Returns the lightness of the color in the HSL model, from 0 (black) to 255 (white): the mean of its largest and
    smallest component.

    \sa lightnessF(), value()
*/
int QColor::lightness() const noexcept {
    const int hi = std::max({m_r, m_g, m_b});
    const int lo = std::min({m_r, m_g, m_b});
    return (hi + lo) / 2;
}

/*!
    Returns the lightness from 0.0 to 1.0.

    \sa lightness()
*/
qreal QColor::lightnessF() const noexcept { return lightness() / 255.0; }

/*!
    Returns the color with hue \a h (0 to 359), saturation \a s, value \a v
    and alpha \a a (each 0 to 255). A negative \a h gives a gray.
*/
QColor QColor::fromHsv(int h, int s, int v, int a) {
    s = clamp255(s);
    v = clamp255(v);
    if (s == 0 || h < 0)
        return QColor(v, v, v, a);
    const double hh = (h % 360) / 60.0;
    const int sector = static_cast<int>(hh);
    const double f = hh - sector;
    const int p = qRound(v * (1.0 - s / 255.0));
    const int q = qRound(v * (1.0 - s / 255.0 * f));
    const int t = qRound(v * (1.0 - s / 255.0 * (1.0 - f)));
    switch (sector) {
    case 0:
        return QColor(v, t, p, a);
    case 1:
        return QColor(q, v, p, a);
    case 2:
        return QColor(p, v, t, a);
    case 3:
        return QColor(p, q, v, a);
    case 4:
        return QColor(t, p, v, a);
    default:
        return QColor(v, p, q, a);
    }
}

namespace {

struct HslValue {
    double h; // from 0 to under 360; -1 for grays
    double s; // from 0 to 1
    double l;
};

HslValue hslOf(int r, int g, int b) {
    const int hi = std::max({r, g, b});
    const int lo = std::min({r, g, b});
    const int delta = hi - lo;
    HslValue out{-1.0, 0.0, (hi + lo) / 510.0};
    if (delta == 0)
        return out;
    out.s = static_cast<double>(delta) / (hi + lo <= 255 ? hi + lo : 510 - hi - lo);
    double h = hi == r   ? static_cast<double>(g - b) / delta
               : hi == g ? 2.0 + static_cast<double>(b - r) / delta
                         : 4.0 + static_cast<double>(r - g) / delta;
    h *= 60.0;
    out.h = h < 0.0 ? h + 360.0 : h;
    return out;
}

// The red, green and blue, from 0 to 1, of a hue in degrees (negative for a gray), saturation and lightness.
void rgbOfHsl(double h, double s, double l, double rgb[3]) {
    const double chroma = h < 0.0 ? 0.0 : (1.0 - std::fabs(2.0 * l - 1.0)) * s;
    const double sector = h < 0.0 ? 0.0 : std::fmod(h, 360.0) / 60.0;
    const double second = chroma * (1.0 - std::fabs(std::fmod(sector, 2.0) - 1.0));
    const double low = l - chroma / 2.0;
    const int at = static_cast<int>(sector) % 6;
    // Which of red, green and blue is the largest in this sector, and which the second.
    const int largest[] = {0, 1, 1, 2, 2, 0};
    const int middle[] = {1, 0, 2, 1, 0, 2};
    rgb[0] = rgb[1] = rgb[2] = low;
    rgb[largest[at]] += chroma;
    rgb[middle[at]] += second;
}

} // namespace

/*!
    Returns the hue in the HSL model, from 0 to 359, or -1 for a gray. It is the same as hue().

    \sa hslSaturation(), lightness()
*/
int QColor::hslHue() const noexcept { return hue(); }

/*!
    Returns the saturation in the HSL model, from 0 to 255.

    \sa hslHue(), lightness()
*/
int QColor::hslSaturation() const noexcept { return qRound(hslOf(m_r, m_g, m_b).s * 255.0); }

/*!
    Returns the hue in the HSL model as a part of a turn, from 0.0 to under 1.0, or -1.0 for a gray.
*/
qreal QColor::hslHueF() const noexcept {
    const double h = hslOf(m_r, m_g, m_b).h;
    return h < 0.0 ? -1.0 : h / 360.0;
}

/*!
    Returns the saturation in the HSL model, from 0.0 to 1.0.
*/
qreal QColor::hslSaturationF() const noexcept { return hslOf(m_r, m_g, m_b).s; }

/*!
    Stores the hue in \a h, the saturation in \a s, the lightness in \a l and, if it is not null, the alpha in \a a.

    \sa setHsl(), getHslF()
*/
void QColor::getHsl(int *h, int *s, int *l, int *a) const {
    *h = hslHue();
    *s = hslSaturation();
    *l = lightness();
    if (a)
        *a = m_a;
}

/*!
    Stores the hue in \a h (a part of a turn, -1 for a gray), the saturation in \a s, the lightness in \a l and, if
    it is not null, the alpha in \a a, the last three from 0.0 to 1.0.

    \sa setHslF(), getHsl()
*/
void QColor::getHslF(float *h, float *s, float *l, float *a) const {
    const HslValue value = hslOf(m_r, m_g, m_b);
    *h = value.h < 0.0 ? -1.0F : static_cast<float>(value.h / 360.0);
    *s = static_cast<float>(value.s);
    *l = static_cast<float>(value.l);
    if (a)
        *a = static_cast<float>(alphaF());
}

/*!
    Returns the color with the hue \a h (0 to 359, or -1 for a gray; 360 and more go round again), the saturation
    \a s, the lightness \a l and the alpha \a a (each 0 to 255). The color is invalid if a value is out of range.

    \sa setHsl()
*/
QColor QColor::fromHsl(int h, int s, int l, int a) {
    if (h < -1 || s < 0 || s > 255 || l < 0 || l > 255 || a < 0 || a > 255)
        return QColor();
    double rgb[3];
    rgbOfHsl(h, s / 255.0, l / 255.0, rgb);
    return QColor(qRound(rgb[0] * 255.0), qRound(rgb[1] * 255.0), qRound(rgb[2] * 255.0), a);
}

/*!
    Returns the color with the hue \a h (a part of a turn from 0.0 to 1.0, or -1.0 for a gray), the saturation \a s,
    the lightness \a l and the alpha \a a (each 0.0 to 1.0). The color is invalid if a value is out of range.

    \sa setHslF()
*/
QColor QColor::fromHslF(float h, float s, float l, float a) {
    const auto outside = [](float v) { return !(v >= 0.0F && v <= 1.0F); };
    if ((outside(h) && h != -1.0F) || outside(s) || outside(l) || outside(a))
        return QColor();
    double rgb[3];
    rgbOfHsl(h == -1.0F ? -1.0 : static_cast<double>(h) * 360.0, static_cast<double>(s), static_cast<double>(l), rgb);
    return fromRgbF(rgb[0], rgb[1], rgb[2], static_cast<double>(a));
}

/*!
    Sets the color from the hue \a h, the saturation \a s, the lightness \a l and the alpha \a a, as fromHsl()
    takes them.
*/
void QColor::setHsl(int h, int s, int l, int a) { *this = fromHsl(h, s, l, a); }

/*!
    Sets the color from the hue \a h, the saturation \a s, the lightness \a l and the alpha \a a, as fromHslF()
    takes them.
*/
void QColor::setHslF(float h, float s, float l, float a) { *this = fromHslF(h, s, l, a); }

/*!
    \fn QColor QColor::toHsl() const

    Returns the color. Nt stores every color as RGB, so there is nothing to convert; hslHue(), hslSaturation() and
    lightness() work on any color.
*/

/*!
    Returns a lighter color. A \a factor of 150 makes the value 50% higher;
    100 or less returns the color unchanged. Black becomes dark gray.

    \sa darker()
*/
QColor QColor::lighter(int factor) const {
    if (factor <= 0)
        return *this;
    if (factor < 100)
        return darker(10000 / factor);
    HsvValue c = toHsv(m_r, m_g, m_b);
    c.v = c.v * factor / 100;
    if (c.v > 255) {
        c.s -= c.v - 255;
        if (c.s < 0)
            c.s = 0;
        c.v = 255;
    }
    if (c.v == 0 && factor > 100)
        c.v = factor * 255 / 1000;
    QColor out = fromHsv(c.h, c.s, c.v, m_a);
    out.m_valid = m_valid;
    return out;
}

/*!
    Returns a darker color. A \a factor of 200 halves the value; 100 or less
    returns the color unchanged.

    \sa lighter()
*/
QColor QColor::darker(int factor) const {
    if (factor <= 0)
        return *this;
    if (factor < 100)
        return lighter(10000 / factor);
    const HsvValue c = toHsv(m_r, m_g, m_b);
    QColor out = fromHsv(c.h, c.s, c.v * 100 / factor, m_a);
    out.m_valid = m_valid;
    return out;
}

/*!
    \since 6.0

    Returns the color as a QVariant, so that it can be given where a QVariant is taken, as in
    \c{item->setData(Qt::UserRole, value)}.
*/
QColor::operator QVariant() const { return QVariant::fromValue(*this); }
