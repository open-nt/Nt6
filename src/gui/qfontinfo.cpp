// MIT License - see LICENSE
#include <QtGui/qfontinfo.h>

#include "qfontengine_p.h"

#include <cmath>

/*!
    \class QFontInfo
    \brief The QFontInfo class provides general information about fonts.

    A QFontInfo describes the font that is used for a QFont: its size in
    pixels and points, and whether its family was found.

    \code
    QFontInfo info(QFont("Noto Sans", 12));
    int pixels = info.pixelSize(); // 16 at 96 dots per inch
    \endcode

    \differences
    The family, style and weight are those requested, not those of the face
    that fontconfig picked; exactMatch() only checks that the family is
    installed. Sizes are at 96 dots per inch.
    \enddifferences

    \sa QFont, QFontMetrics
*/

/*!
    \fn QFontInfo::QFontInfo(const QFont &font)
    \since 6.0

    Constructs information about \a font.
*/

/*!
    \fn QFontInfo::QFontInfo(const QFontInfo &other)
    \since 6.0

    Constructs a copy of \a other.
*/

/*!
    \fn QFontInfo &QFontInfo::operator=(const QFontInfo &other)
    \since 6.0

    Assigns \a other to this font info and returns a reference to it.
*/

/*!
    \fn QFontInfo::~QFontInfo()
    \since 6.0

    Destroys the font info.
*/

/*!
    \since 6.0

    Returns the family of the font: the first of its families, or
    \c "Sans Serif" if it names none.
*/
QString QFontInfo::family() const {
    const QStringList families = m_font.families();
    if (!families.isEmpty() && !families.first().isEmpty())
        return families.first();
    if (!m_font.family().isEmpty())
        return m_font.family();
    return QStringLiteral("Sans Serif");
}

/*!
    \since 6.0

    Returns the style name of the font, such as \c "Bold Italic".
*/
QString QFontInfo::styleName() const {
    const bool isBold = m_font.bold();
    const bool isItalic = m_font.italic();
    if (isBold && isItalic)
        return QStringLiteral("Bold Italic");
    if (isBold)
        return QStringLiteral("Bold");
    if (isItalic)
        return QStringLiteral("Italic");
    return QStringLiteral("Regular");
}

/*!
    \since 6.0

    Returns the size of the font in pixels.
*/
int QFontInfo::pixelSize() const { return static_cast<int>(std::lround(QFontEngine::pixelSizeOf(m_font))); }

/*!
    \since 6.0

    Returns the size of the font in points, rounded.
*/
int QFontInfo::pointSize() const { return static_cast<int>(std::lround(pointSizeF())); }

/*!
    \since 6.0

    Returns the size of the font in points.
*/
qreal QFontInfo::pointSizeF() const { return QFontEngine::pixelSizeOf(m_font) * 72.0 / 96.0; }

/*!
    \fn bool QFontInfo::italic() const
    \since 6.0

    Returns \c true if the font is italic.
*/

/*!
    \fn QFont::Style QFontInfo::style() const
    \since 6.0

    Returns the slant of the font.
*/

/*!
    \fn int QFontInfo::weight() const
    \since 6.0

    Returns the weight of the font, as a QFont::Weight value.
*/

/*!
    \fn bool QFontInfo::bold() const
    \since 6.0

    Returns \c true if the font is bold.
*/

/*!
    \fn bool QFontInfo::underline() const
    \since 6.0

    Returns \c true if the font is underlined.
*/

/*!
    \fn bool QFontInfo::overline() const
    \since 6.0

    Returns \c true if the font has a line over it.
*/

/*!
    \fn bool QFontInfo::strikeOut() const
    \since 6.0

    Returns \c true if the font is struck out.
*/

/*!
    \fn bool QFontInfo::fixedPitch() const
    \since 6.0

    Returns \c true if the font has a fixed pitch.
*/

/*!
    \fn QFont::StyleHint QFontInfo::styleHint() const
    \since 6.0

    Returns the style hint of the font.
*/

/*!
    \since 6.0

    Returns \c true if the family of the font is installed or was added as an
    application font. Without fontconfig, returns \c false.
*/
bool QFontInfo::exactMatch() const { return QFontEngine::families().contains(family(), Qt::CaseInsensitive); }
