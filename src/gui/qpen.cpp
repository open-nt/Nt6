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

    \image painter/pens.png Lines with flat, square and round caps and dashes, and arcs of a gauge

    \snippet gui/painting.cpp pens

    Paths, transformed shapes and floating-point coordinates are drawn with the cap and join styles,
    dashes and miter limit of the pen; drawing with integer coordinates and no transformation other than
    a translation draws plain lines and outlines of whole pixels.

    \differences
    A pen draws with the color of its brush; gradients are used for filling, not for pens. A width of 0
    draws a cosmetic line one pixel wide.
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
    \fn QPen::QPen(const QBrush &brush, qreal width, Qt::PenStyle style, Qt::PenCapStyle cap, Qt::PenJoinStyle join)
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

/*!
    \fn Qt::PenCapStyle QPen::capStyle() const

    Returns how the ends of lines are drawn; Qt::SquareCap by default.
*/

/*!
    \fn void QPen::setCapStyle(Qt::PenCapStyle style)

    Sets how the ends of lines are drawn to \a style.
*/

/*!
    \fn Qt::PenJoinStyle QPen::joinStyle() const

    Returns how the segments of lines meet; Qt::BevelJoin by default.
*/

/*!
    \fn void QPen::setJoinStyle(Qt::PenJoinStyle style)

    Sets how the segments of lines meet to \a style.
*/

/*!
    \fn qreal QPen::miterLimit() const

    Returns how far a miter join may reach, in units of the pen width; 2 by default.
*/

/*!
    \fn void QPen::setMiterLimit(qreal limit)

    Sets how far a miter join may reach to \a limit pen widths.
*/

/*!
    Returns the dashes and gaps of the pen, in units of its width: the pattern of its style, or the one set
    with setDashPattern().
*/
QList<qreal> QPen::dashPattern() const {
    switch (m_style) {
    case Qt::DashLine:
        return {4, 2};
    case Qt::DotLine:
        return {1, 2};
    case Qt::DashDotLine:
        return {4, 2, 1, 2};
    case Qt::DashDotDotLine:
        return {4, 2, 1, 2, 1, 2};
    case Qt::CustomDashLine:
        return m_dashes;
    default:
        return {};
    }
}

/*!
    Sets the dashes and gaps of the pen to \a pattern, alternating, in units of its width, and makes its
    style Qt::CustomDashLine. A pattern with an odd number of entries is ignored.
*/
void QPen::setDashPattern(const QList<qreal> &pattern) {
    if (pattern.isEmpty() || pattern.size() % 2 != 0)
        return;
    m_dashes = pattern;
    m_style = Qt::CustomDashLine;
}

/*!
    \fn qreal QPen::dashOffset() const

    Returns where in the dash pattern lines start, in units of the pen width.
*/

/*!
    \fn void QPen::setDashOffset(qreal offset)

    Sets where in the dash pattern lines start to \a offset.
*/

/*!
    \fn bool QPen::isCosmetic() const

    Returns \c true if the width of the pen is in device pixels, not affected by transformations: if it was
    set cosmetic or has a width of 0. Otherwise returns \c false.
*/

/*!
    \fn void QPen::setCosmetic(bool cosmetic)

    Makes the width of the pen in device pixels if \a cosmetic is \c true.
*/

/*!
    \fn bool QPen::isSolid() const

    Returns \c true if the style is Qt::SolidLine; otherwise returns \c false.
*/
