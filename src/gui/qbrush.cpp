// MIT License - see LICENSE
#include <QtGui/qbrush.h>

/*!
    \class QBrush
    \brief The QBrush class defines the fill pattern of shapes drawn by QPainter.

    A brush has a color and a style. The default brush has the style
    Qt::NoBrush, which fills nothing.

    \code
    QPainter painter(&image);
    painter.setBrush(QBrush(Qt::blue));
    painter.drawRect(10, 10, 40, 40);
    \endcode

    \differences
    The only styles are Qt::NoBrush and Qt::SolidPattern. There are no
    gradients, textures or brush transforms.
    \enddifferences

    \sa QPen, QPainter, QColor
*/

/*!
    \fn QBrush::QBrush()

    Constructs a brush with the style Qt::NoBrush and the color black.
*/

/*!
    \fn QBrush::QBrush(Qt::BrushStyle style)
    \overload

    Constructs a black brush with the given \a style.
*/

/*!
    \fn QBrush::QBrush(const QColor &color, Qt::BrushStyle style)
    \overload

    Constructs a brush with the given \a color and \a style.
*/

/*!
    \fn QBrush::QBrush(Qt::GlobalColor color, Qt::BrushStyle style)
    \overload

    Constructs a brush with the given \a color and \a style.
*/

/*!
    \fn Qt::BrushStyle QBrush::style() const

    Returns the brush style.

    \sa setStyle()
*/

/*!
    \fn void QBrush::setStyle(Qt::BrushStyle style)

    Sets the brush style to \a style.

    \sa style()
*/

/*!
    \fn const QColor &QBrush::color() const

    Returns the brush color.

    \sa setColor()
*/

/*!
    \fn void QBrush::setColor(const QColor &color)

    Sets the brush color to \a color.

    \sa color()
*/

/*!
    \fn void QBrush::setColor(Qt::GlobalColor color)
    \overload

    Sets the brush color to \a color.
*/
