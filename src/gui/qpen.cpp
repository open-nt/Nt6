// MIT License - see LICENSE
#include <QtGui/qpen.h>

/*!
    \class QPen
    \brief The QPen class defines how QPainter draws lines and outlines.

    A pen has a color, a width and a style. The default pen is a black solid
    line one pixel wide.

    \code
    QPainter painter(&image);
    painter.setPen(QPen(Qt::red, 3));
    painter.drawLine(0, 0, 100, 100);
    \endcode

    \differences
    The only styles are Qt::NoPen and Qt::SolidLine. There are no dashes, cap
    or join styles, miter limits or cosmetic pens. A width of 0 or 1 draws a
    one pixel line.
    \enddifferences

    \sa QBrush, QPainter
*/

/*!
    \fn QPen::QPen()

    Constructs a black solid pen one pixel wide.
*/

/*!
    \fn QPen::QPen(Qt::PenStyle style)
    \overload

    Constructs a black pen with the given \a style.
*/

/*!
    \fn QPen::QPen(const QColor &color)
    \overload

    Constructs a solid pen one pixel wide with the given \a color.
*/

/*!
    \fn QPen::QPen(const QBrush &brush, qreal width, Qt::PenStyle style)
    \overload

    Constructs a pen with the given \a brush, \a width and \a style.
*/

/*!
    \fn Qt::PenStyle QPen::style() const

    Returns the pen style.

    \sa setStyle()
*/

/*!
    \fn void QPen::setStyle(Qt::PenStyle style)

    Sets the pen style to \a style.

    \sa style()
*/

/*!
    \fn QColor QPen::color() const

    Returns the pen color.

    \sa setColor()
*/

/*!
    \fn void QPen::setColor(const QColor &color)

    Sets the pen color to \a color.

    \sa color()
*/

/*!
    \fn QBrush QPen::brush() const

    Returns the brush that gives the pen its color.

    \sa setBrush()
*/

/*!
    \fn void QPen::setBrush(const QBrush &brush)

    Sets the brush of the pen to \a brush.

    \sa brush()
*/

/*!
    \fn int QPen::width() const

    Returns the pen width rounded to whole pixels.

    \sa setWidth(), widthF()
*/

/*!
    \fn void QPen::setWidth(int width)

    Sets the pen width to \a width pixels.

    \sa width()
*/

/*!
    \fn qreal QPen::widthF() const

    Returns the pen width.

    \sa setWidthF()
*/

/*!
    \fn void QPen::setWidthF(qreal width)

    Sets the pen width to \a width.

    \sa widthF()
*/
