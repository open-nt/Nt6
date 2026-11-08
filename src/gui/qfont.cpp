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
    There is no \c setStyleHint(), \c setStretch(), underline, strike-out,
    letter or word spacing, hinting preference or \c resolve(). A QFont is a
    plain value; there is no font database, so QFontDatabase is missing and the
    available families cannot be listed. The font is not found until it is used.
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

    Sets the family name to \a family. An empty name or \c{"Sans Serif"} stands for the
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
