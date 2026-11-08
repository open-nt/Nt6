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
    \value SO_Slider A QStyleOptionSlider.
    \value SO_ProgressBar A QStyleOptionProgressBar.
    \value SO_Tab A QStyleOptionTab.
    \value SO_ComboBox A QStyleOptionComboBox.
    \value SO_SpinBox A QStyleOptionSpinBox.
    \value SO_GroupBox A QStyleOptionGroupBox.
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

/*!
    \class QStyleOptionSlider
    \brief The QStyleOptionSlider class describes the parameters for drawing a slider.

    \sa QStyleOption, QSlider
*/

/*!
    \fn QStyleOptionSlider::QStyleOptionSlider()

    Constructs an option for a slider.
*/

/*!
    \variable QStyleOptionSlider::orientation

    The direction in which the handle moves, horizontal by default.
*/

/*!
    \variable QStyleOptionSlider::minimum

    The smallest value of the slider.
*/

/*!
    \variable QStyleOptionSlider::maximum

    The largest value of the slider.
*/

/*!
    \variable QStyleOptionSlider::sliderPosition

    The value that the handle is at, between minimum and maximum.
*/

/*!
    \class QStyleOptionProgressBar
    \brief The QStyleOptionProgressBar class describes the parameters for drawing a progress bar.

    \sa QStyleOption, QProgressBar
*/

/*!
    \fn QStyleOptionProgressBar::QStyleOptionProgressBar()

    Constructs an option for a progress bar.
*/

/*!
    \variable QStyleOptionProgressBar::minimum

    The value that stands for no progress.
*/

/*!
    \variable QStyleOptionProgressBar::maximum

    The value that stands for completion.
*/

/*!
    \variable QStyleOptionProgressBar::progress

    The current value, between minimum and maximum.
*/

/*!
    \variable QStyleOptionProgressBar::text

    The text to show on the bar.
*/

/*!
    \variable QStyleOptionProgressBar::textVisible

    Whether the text is drawn.
*/

/*!
    \class QStyleOptionTab
    \brief The QStyleOptionTab class describes the parameters for drawing a tab of a tab bar.

    A selected tab has the State_Selected flag.

    \sa QStyleOption, QTabWidget
*/

/*!
    \fn QStyleOptionTab::QStyleOptionTab()

    Constructs an option for a tab.
*/

/*!
    \variable QStyleOptionTab::text

    The text on the tab.
*/

/*!
    \class QStyleOptionComboBox
    \brief The QStyleOptionComboBox class describes the parameters for drawing a combo box.

    The combo box is shown with its popup open if the option has the State_On flag.

    \sa QStyleOption, QComboBox
*/

/*!
    \fn QStyleOptionComboBox::QStyleOptionComboBox()

    Constructs an option for a combo box.
*/

/*!
    \variable QStyleOptionComboBox::currentText

    The text of the current item.
*/

/*!
    \class QStyleOptionSpinBox
    \brief The QStyleOptionSpinBox class describes the parameters for drawing a spin box.

    \sa QStyleOption, QSpinBox
*/

/*!
    \fn QStyleOptionSpinBox::QStyleOptionSpinBox()

    Constructs an option for a spin box.
*/

/*!
    \variable QStyleOptionSpinBox::upEnabled

    Whether the button that steps up can be used.
*/

/*!
    \variable QStyleOptionSpinBox::downEnabled

    Whether the button that steps down can be used.
*/

/*!
    \variable QStyleOptionSpinBox::upPressed

    Whether the button that steps up is pressed.
*/

/*!
    \variable QStyleOptionSpinBox::downPressed

    Whether the button that steps down is pressed.
*/

/*!
    \class QStyleOptionGroupBox
    \brief The QStyleOptionGroupBox class describes the parameters for drawing a group box.

    A checkable group box shows its state with the State_On and State_Off flags.

    \sa QStyleOption, QGroupBox
*/

/*!
    \fn QStyleOptionGroupBox::QStyleOptionGroupBox()

    Constructs an option for a group box.
*/

/*!
    \variable QStyleOptionGroupBox::text

    The title of the group box.
*/

/*!
    \variable QStyleOptionGroupBox::checkable

    Whether the title has a check box.
*/
