// MIT License - see LICENSE
#include <QtWidgets/qcolormap.h>

/*!
    \class QColormap
    \brief The QColormap class maps colors to the pixel values of the screen.

    Every screen of Nt is direct color with 32 bits per pixel, so a pixel value is the ARGB value of the color:

    \code
    const uint pixel = QColormap::instance().pixel(QColor(Qt::red));
    \endcode

    \differences
    Only the Direct mode exists; \c size() is -1 and \c colormap() is empty. There is a single instance for all
    screens, and \c initialize() and \c cleanup() do nothing.
    \enddifferences

    \sa QColor
*/

/*!
    \enum QColormap::Mode

    How the screen stores colors.

    \value Direct Every pixel holds its color.
    \value Indexed Pixels are indexes into a table of colors.
    \value Gray Pixels are shades of gray.
*/

/*!
    \fn void QColormap::initialize()

    Does nothing.
*/

/*!
    \fn void QColormap::cleanup()

    Does nothing.
*/

/*!
    Returns the color map of the \a screen, which is the same for all screens.
*/
QColormap QColormap::instance(int screen) {
    (void)screen;
    return QColormap();
}

/*!
    \fn QColormap::Mode QColormap::mode() const

    Returns how the screen stores colors: always Direct.
*/

/*!
    \fn int QColormap::depth() const

    Returns the number of bits of a pixel: 32.
*/

/*!
    \fn int QColormap::size() const

    Returns the number of colors in the table, or -1 for a direct color screen.
*/

/*!
    Returns the pixel value of \a color: its ARGB value.
*/
uint QColormap::pixel(const QColor &color) const { return color.rgba(); }

/*!
    Returns the color of the \a pixel value.
*/
QColor QColormap::colorAt(uint pixel) const { return QColor::fromRgba(pixel); }

/*!
    \fn QList<QColor> QColormap::colormap() const

    Returns the table of colors, which is empty for a direct color screen.
*/
