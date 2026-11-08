// MIT License - see LICENSE
#include <QtWidgets/qstyleoption.h>

#include <QtWidgets/qwidget.h>

/*!
    \class QStyleOption
    \brief The QStyleOption class stores the parameters that QStyle functions use to draw.

    An option carries the rectangle to draw in, the state, the palette and the font metrics. Use
    initFrom() to set them from a widget. Subclasses add the data that an element needs, such as a text.

    \differences
    The members are plain data and there is no \c version, \c direction, \c styleObject or \c operator=.
    \enddifferences

    \sa QStyle, QStyleOptionButton
*/

/*!
    \enum QStyleOption::OptionType

    This enum identifies the kind of an option.

    \value SO_Default A plain option.
    \value SO_Button A QStyleOptionButton.
    \value SO_Frame A QStyleOptionFrame.
*/

/*!
    \fn QStyleOption::QStyleOption(int optionType)

    Constructs an option of the given \a optionType.
*/

/*!
    Destroys the option.
*/
QStyleOption::~QStyleOption() = default;

/*!
    Sets the state, rectangle, palette and font metrics of the option from \a widget.
*/
void QStyleOption::initFrom(const QWidget *widget) {
    state = QStyle::State();
    if (widget->isEnabled())
        state |= QStyle::State_Enabled;
    if (widget->hasFocus())
        state |= QStyle::State_HasFocus;
    if (widget->underMouse())
        state |= QStyle::State_MouseOver;
    if (widget->isActiveWindow())
        state |= QStyle::State_Active;
    rect = widget->rect();
    palette = widget->palette();
    fontMetrics = widget->fontMetrics();
}

/*!
    \variable QStyleOption::type

    The kind of the option, a value of OptionType.
*/

/*!
    \variable QStyleOption::state

    The state of the element.
*/

/*!
    \variable QStyleOption::rect

    The rectangle to draw in.
*/

/*!
    \variable QStyleOption::palette

    The palette to draw with.
*/

/*!
    \variable QStyleOption::fontMetrics

    The metrics of the font of the widget.
*/

/*!
    \class QStyleOptionButton
    \brief The QStyleOptionButton class describes the parameters for drawing buttons.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionButton::ButtonFeature

    This enum describes the features of a button. The ButtonFeatures type is a typedef for
    \c{QFlags<ButtonFeature>}.

    \value None No special feature.
    \value Flat The button has no bevel until it is pressed.
    \value DefaultButton The button is the default button.
*/

/*!
    \fn QStyleOptionButton::QStyleOptionButton()

    Constructs an option for a button.
*/

/*!
    \variable QStyleOptionButton::features

    The features of the button.
*/

/*!
    \variable QStyleOptionButton::text

    The text of the button.
*/

/*!
    \class QStyleOptionFrame
    \brief The QStyleOptionFrame class describes the parameters for drawing a frame.

    \sa QStyleOption, QStyle
*/

/*!
    \fn QStyleOptionFrame::QStyleOptionFrame()

    Constructs an option for a frame.
*/

/*!
    \variable QStyleOptionFrame::lineWidth

    The width of the frame line, 1 by default.
*/
