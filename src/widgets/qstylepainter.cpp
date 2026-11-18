// MIT License - see LICENSE
#include <QtWidgets/qstylepainter.h>

#include <QtWidgets/qwidget.h>

/*!
    \class QStylePainter
    \brief The QStylePainter class is a convenience class for drawing QStyle elements inside a widget.

    A QStylePainter is a QPainter on a widget that also knows the style of the widget, so that the elements of
    the style are drawn with one call, without passing the painter and the widget each time:

    \code
    void Badge::paintEvent(QPaintEvent *) {
        QStylePainter painter(this);
        QStyleOptionButton option;
        option.initFrom(this);
        option.text = text();
        painter.drawControl(QStyle::CE_PushButton, option);
    }
    \endcode

    The painter starts with the font of the widget and a pen of its text color.

    \differences
    There is no \c drawComplexControl(), as Nt's QStyle has no complex controls.
    \enddifferences

    \sa QStyle, QStyleOption, QPainter
*/

/*!
    Constructs a style painter that is not active. Call begin() to start painting.
*/
QStylePainter::QStylePainter() = default;

/*!
    Constructs a style painter that paints on \a widget with its style. Use it in the paint event of \a widget.
*/
QStylePainter::QStylePainter(QWidget *widget) { begin(widget, widget); }

/*!
    Constructs a style painter that paints on \a device with the style of \a widget, for example to draw the
    look of a widget into a pixmap.
*/
QStylePainter::QStylePainter(QPaintDevice *device, QWidget *widget) { begin(device, widget); }

/*!
    Starts painting on \a widget with its style, and returns \c true if the painter is active.
*/
bool QStylePainter::begin(QWidget *widget) { return begin(widget, widget); }

/*!
    Starts painting on \a device with the style of \a widget, and returns \c true if the painter is active.
    The painter takes the font of \a widget and a pen of its text color.
*/
bool QStylePainter::begin(QPaintDevice *device, QWidget *widget) {
    m_widget = widget;
    m_style = widget ? widget->style() : nullptr;
    if (!QPainter::begin(device))
        return false;
    if (widget) {
        setFont(widget->font());
        setPen(widget->palette().color(QPalette::WindowText));
    }
    return true;
}

/*!
    Draws the primitive element \a element of the style with the state and rectangle of \a option.

    \sa QStyle::drawPrimitive()
*/
void QStylePainter::drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption &option) {
    if (m_style)
        m_style->drawPrimitive(element, &option, this, m_widget);
}

/*!
    Draws the control element \a element of the style with the state and rectangle of \a option.

    \sa QStyle::drawControl()
*/
void QStylePainter::drawControl(QStyle::ControlElement element, const QStyleOption &option) {
    if (m_style)
        m_style->drawControl(element, &option, this, m_widget);
}

/*!
    Draws \a text in \a rectangle, aligned as \a flags says, in the color of the role \a textRole of \a palette,
    enabled or not as \a enabled says.

    \sa QStyle::drawItemText()
*/
void QStylePainter::drawItemText(const QRect &rectangle, int flags, const QPalette &palette, bool enabled,
                                 const QString &text, QPalette::ColorRole textRole) {
    if (m_style)
        m_style->drawItemText(this, rectangle, flags, palette, enabled, text, textRole);
}

/*!
    Draws \a pixmap in \a rectangle, aligned as \a flags says.

    \sa QStyle::drawItemPixmap()
*/
void QStylePainter::drawItemPixmap(const QRect &rectangle, int flags, const QPixmap &pixmap) {
    if (m_style)
        m_style->drawItemPixmap(this, rectangle, flags, pixmap);
}

/*!
    \fn QStyle *QStylePainter::style() const

    Returns the style that the painter draws with: that of the widget it was begun with.
*/

/*!
    \since 6.0

    Draws the complex control \a cc as \a option describes it, with the style of the widget.
*/
void QStylePainter::drawComplexControl(QStyle::ComplexControl cc, const QStyleOptionComplex &option) {
    m_style->drawComplexControl(cc, &option, this, m_widget);
}
