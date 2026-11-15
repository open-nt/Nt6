// MIT License - see LICENSE
#include <QtGui/qfont.h>

#include "qfont_p.h"

#include <QtGui/qguiapplication.h>

#include <cmath>
#include <mutex>

namespace {

std::mutex g_defaultMutex;

QFont &storedDefault() {
    static QFont *font = [] {
        auto *f = new QFont(QString::fromLatin1("Sans Serif"), 10);
        return f;
    }();
    return *font;
}

// Qt's weights before Qt 6 run from 0 to 99.
int fromLegacyWeight(int weight) {
    if (weight < 0)
        return QFont::Normal;
    if (weight > 99)
        return weight;
    if (weight < 12)
        return QFont::Thin;
    if (weight < 25)
        return QFont::ExtraLight;
    if (weight < 50)
        return QFont::Light;
    if (weight < 57)
        return QFont::Normal;
    if (weight < 63)
        return QFont::Medium;
    if (weight < 75)
        return QFont::DemiBold;
    if (weight < 81)
        return QFont::Bold;
    if (weight < 87)
        return QFont::ExtraBold;
    return QFont::Black;
}

} // namespace

/*!
    \class QFont
    \brief The QFont class specifies a font used for drawing text.

    A font has a family, a size, a weight and an italic flag. Nt looks the font up
    with fontconfig when text is drawn or measured, so any installed family
    works, and a family that is not installed gets the closest match:

    \code
    QFont font("Noto Sans", 12);
    font.setBold(true);
    QPainter painter(&image);
    painter.setFont(font);
    painter.drawText(10, 30, "Hello");
    \endcode

    Sizes are in points, converted to pixels at 96 dots per inch, or in pixels
    with setPixelSize().

    \differences
    There is no underline, strike-out, hinting preference or \c resolve(). Letter and word spacing are applied
    to the advances of single characters, not to shaped clusters. The style hint only chooses a generic fontconfig
    family (\c monospace, \c serif, \c sans-serif, \c cursive, \c fantasy)
    that is tried after the families of the font, and the style strategy is
    remembered but not used. The font is not found until it is used.
    \enddifferences

    \sa QFontMetrics, QPainter::drawText(), QGuiApplication::font()
*/

/*!
    \enum QFont::Weight

    This enum gives the weight of a font, using the OpenType values.

    \value Thin 100.
    \value ExtraLight 200.
    \value Light 300.
    \value Normal 400.
    \value Medium 500.
    \value DemiBold 600.
    \value Bold 700.
    \value ExtraBold 800.
    \value Black 900.
*/

/*!
    \enum QFont::StyleHint

    The kind of font to fall back to when none of the families is installed.

    \value Helvetica A sans-serif font.
    \value SansSerif The same as Helvetica.
    \value Times A serif font.
    \value Serif The same as Times.
    \value Courier A monospaced font.
    \value TypeWriter The same as Courier.
    \value OldEnglish A decorative font.
    \value Decorative The same as OldEnglish.
    \value System The font of the system.
    \value AnyStyle No preference; the default.
    \value Cursive A cursive font.
    \value Monospace A monospaced font.
    \value Fantasy A fantasy font.
*/

/*!
    \enum QFont::StyleStrategy

    How fonts are matched and drawn. Nt keeps the value but does not act on it.

    \value PreferDefault The default.
    \value PreferBitmap Prefer bitmap fonts.
    \value PreferDevice Prefer device fonts.
    \value PreferOutline Prefer outline fonts.
    \value ForceOutline Use only outline fonts.
    \value PreferMatch Prefer an exact match.
    \value PreferQuality Prefer the best quality.
    \value PreferAntialias Prefer antialiased drawing.
    \value NoAntialias Do not antialias.
    \value NoSubpixelAntialias Do not use subpixel antialiasing.
    \value PreferNoShaping Skip shaping when the font allows it.
    \value NoFontMerging Do not fall back to other fonts for missing characters.
*/

/*!
    \enum QFont::Stretch

    How wide the glyphs are, in percent of the normal width.

    \value AnyStretch No preference; the default.
    \value UltraCondensed 50.
    \value ExtraCondensed 62.
    \value Condensed 75.
    \value SemiCondensed 87.
    \value Unstretched 100.
    \value SemiExpanded 112.
    \value Expanded 125.
    \value ExtraExpanded 150.
    \value UltraExpanded 200.
*/

/*!
    Returns the families the font is looked up in, in order: those set with setFamilies(), or the family() alone.

    \sa setFamilies(), family()
*/
QStringList QFont::families() const {
    if (!m_families.isEmpty())
        return m_families;
    return m_family.isEmpty() ? QStringList() : QStringList{m_family};
}

/*!
    Sets the families to try, in order, to \a families. family() becomes the first of them.

    \sa families(), setFamily()
*/
void QFont::setFamilies(const QStringList &families) {
    m_families = families;
    m_family = families.isEmpty() ? QString() : families.first();
}

/*!
    Sets the style hint to \a hint and the style strategy to \a strategy.

    \sa styleHint(), styleStrategy()
*/
void QFont::setStyleHint(StyleHint hint, StyleStrategy strategy) {
    m_styleHint = static_cast<int>(hint);
    m_styleStrategy = static_cast<int>(strategy);
}

/*!
    \fn QFont::StyleHint QFont::styleHint() const

    Returns the style hint, AnyStyle by default.

    \sa setStyleHint()
*/

/*!
    \fn QFont::StyleStrategy QFont::styleStrategy() const

    Returns the style strategy, PreferDefault by default.

    \sa setStyleStrategy()
*/

/*!
    \fn void QFont::setStyleStrategy(StyleStrategy strategy)

    Sets the style strategy to \a strategy.

    \sa styleStrategy()
*/

/*!
    \fn int QFont::stretch() const

    Returns the stretch factor, a QFont::Stretch value or a percentage; AnyStretch by default.

    \sa setStretch()
*/

/*!
    Sets the stretch factor to \a factor, a percentage from 1 to 4000 or AnyStretch. A font family that has a face of
    that width is drawn with it; the glyphs are not stretched artificially.

    \sa stretch()
*/
void QFont::setStretch(int factor) {
    if (factor < 0 || factor > 4000)
        return;
    m_stretch = factor;
}

/*!
    \enum QFont::SpacingType

    How setLetterSpacing() changes the space between letters.

    \value PercentageSpacing The spacing is a percentage of each advance; 100 leaves it unchanged.
    \value AbsoluteSpacing The spacing is a number of pixels added to each advance; 0 leaves it unchanged.
*/

/*!
    \fn qreal QFont::letterSpacing() const

    Returns the letter spacing: 100 (percent) unless setLetterSpacing() changed it.

    \sa letterSpacingType(), setLetterSpacing()
*/

/*!
    \fn QFont::SpacingType QFont::letterSpacingType() const

    Returns how letterSpacing() is applied; PercentageSpacing by default.
*/

/*!
    Sets the space between letters to \a spacing, a percentage of each advance or a number of pixels added to
    it, as \a type says.

    \sa setWordSpacing()
*/
void QFont::setLetterSpacing(SpacingType type, qreal spacing) {
    m_letterSpacingType = static_cast<int>(type);
    m_letterSpacing = spacing;
}

/*!
    \fn qreal QFont::wordSpacing() const

    Returns the pixels added to the advance of each space; 0 by default.
*/

/*!
    \fn void QFont::setWordSpacing(qreal spacing)

    Adds \a spacing pixels to the advance of each space, which may be negative.
*/

/*!
    Returns \c true if setFixedPitch() asked for a font whose glyphs all have the same width.

    \sa setFixedPitch()
*/
bool QFont::fixedPitch() const { return m_styleHint == Monospace || m_styleHint == Courier; }

/*!
    Asks for a font whose glyphs all have the same width if \a enable: the style hint becomes Monospace, so a
    monospaced family is used when the families of the font are not installed or not monospaced. Turning it off
    resets the hint to AnyStyle.

    \sa fixedPitch(), setStyleHint()
*/
void QFont::setFixedPitch(bool enable) {
    if (enable)
        m_styleHint = Monospace;
    else if (fixedPitch())
        m_styleHint = AnyStyle;
}

/*!
    Returns the font as a comma-separated description: family, point size, pixel size, style hint, weight, style
    (0 upright, 1 italic), underline, strike-out and fixed pitch, as Qt writes them. fromString() reads it back.

    \sa fromString()
*/
QString QFont::toString() const {
    const QStringList fields{m_family,
                             QString::number(m_pointSize),
                             QString::number(m_pixelSize),
                             QString::number(m_styleHint),
                             QString::number(m_weight),
                             QString::number(m_italic ? 1 : 0),
                             QStringLiteral("0"),
                             QStringLiteral("0"),
                             QString::number(fixedPitch() ? 1 : 0),
                             QStringLiteral("0")};
    return fields.join(QLatin1Char(','));
}

/*!
    Sets the font from \a description, as toString() writes it, and returns \c true if it could be read. Only the
    family, sizes, style hint, weight and style are used; a description with just a family and a point size works
    too.

    \sa toString()
*/
bool QFont::fromString(const QString &description) {
    const QStringList fields = description.split(QLatin1Char(','));
    if (fields.isEmpty() || fields.first().isEmpty())
        return false;
    QFont font = *this;
    font.setFamily(fields.at(0).trimmed());
    if (fields.size() >= 2) {
        bool ok = false;
        const double points = fields.at(1).toDouble(&ok);
        if (ok && points > 0)
            font.setPointSizeF(points);
    }
    if (fields.size() >= 3) {
        bool ok = false;
        const int pixels = fields.at(2).toInt(&ok);
        if (ok && pixels > 0)
            font.setPixelSize(pixels);
    }
    if (fields.size() >= 4)
        font.m_styleHint = fields.at(3).toInt();
    if (fields.size() >= 5) {
        bool ok = false;
        const int weight = fields.at(4).toInt(&ok);
        if (ok)
            font.m_weight = weight > 0 && weight < 100 ? fromLegacyWeight(weight) : weight;
    }
    if (fields.size() >= 6)
        font.m_italic = fields.at(5).toInt() != 0;
    *this = font;
    return true;
}

/*!
    Constructs a font that is a copy of the application font, which is the one
    QGuiApplication::setFont() set, and otherwise \c{"Sans Serif"} at 10 points.

    \sa QGuiApplication::font()
*/
QFont::QFont() { *this = QGuiApplication::font(); }

/*!
    Constructs a font of the given \a family, \a pointSize, \a weight and
    \a italic flag. A negative \a pointSize leaves the size at the default. The
    \a weight is a QFont::Weight value, or a Qt 5 style value from 0 to 99
    (50 is normal and 75 bold); -1 stands for Normal.
*/
QFont::QFont(const QString &family, int pointSize, int weight, bool italic)
    : m_family(family), m_pointSize(pointSize > 0 ? pointSize : -1.0), m_weight(fromLegacyWeight(weight)),
      m_italic(italic) {}

/*!
    \fn QString QFont::family() const

    Returns the family name.

    \sa setFamily()
*/

/*!
    \fn void QFont::setFamily(const QString &family)

    Sets the family name to \a family, replacing the list setFamilies() set. An empty name or \c{"Sans Serif"} stands for the
    default sans-serif font.

    \sa family()
*/

/*!
    Returns the size in points, rounded, or -1 if the size is in pixels.

    \sa pointSizeF(), setPointSize()
*/
int QFont::pointSize() const { return m_pointSize > 0 ? static_cast<int>(std::lround(m_pointSize)) : -1; }

/*!
    Sets the size to \a pointSize points. The size in pixels is forgotten.

    \sa pointSize()
*/
void QFont::setPointSize(int pointSize) { setPointSizeF(pointSize); }

/*!
    \fn qreal QFont::pointSizeF() const

    Returns the size in points, or -1 if the size is in pixels.

    \sa setPointSizeF()
*/

/*!
    Sets the size to \a pointSize points. The size in pixels is forgotten.

    \sa pointSizeF()
*/
void QFont::setPointSizeF(qreal pointSize) {
    if (pointSize <= 0)
        return;
    m_pointSize = pointSize;
    m_pixelSize = -1;
}

/*!
    \fn int QFont::pixelSize() const

    Returns the size in pixels, or -1 if the size is in points.

    \sa setPixelSize()
*/

/*!
    Sets the size to \a pixelSize pixels. The size in points is forgotten.

    \sa pixelSize()
*/
void QFont::setPixelSize(int pixelSize) {
    if (pixelSize <= 0)
        return;
    m_pixelSize = pixelSize;
    m_pointSize = -1.0;
}

/*!
    \fn QFont::Weight QFont::weight() const

    Returns the weight.

    \sa setWeight()
*/

/*!
    \fn void QFont::setWeight(Weight weight)

    Sets the weight to \a weight.

    \sa weight()
*/

/*!
    \fn bool QFont::bold() const

    Returns \c true if the weight is Bold or heavier.

    \sa setBold()
*/

/*!
    \fn void QFont::setBold(bool enable)

    Sets the weight to Bold if \a enable is \c true, and to Normal otherwise.

    \sa bold()
*/

/*!
    \fn bool QFont::italic() const

    Returns \c true if the font is italic.

    \sa setItalic()
*/

/*!
    \fn void QFont::setItalic(bool enable)

    Makes the font italic if \a enable is \c true.

    \sa italic()
*/

namespace QtPrivate {

QFont applicationFont() {
    std::lock_guard<std::mutex> lock(g_defaultMutex);
    return storedDefault();
}

void setApplicationFont(const QFont &font) {
    std::lock_guard<std::mutex> lock(g_defaultMutex);
    storedDefault() = font;
}

} // namespace QtPrivate
