// MIT License - see LICENSE
#include <QtWidgets/qstyle.h>

/*!
    \class QStyle
    \brief The QStyle class is an abstract base class that encapsulates the look and feel of a GUI.

    Widgets do not draw themselves with fixed colors and sizes; they ask the style. A style
    draws the primitive elements (frames, panels) and controls (buttons) from a QStyleOption that
    describes the state and rectangle, and answers questions about sizes. Nt has one style, the
    default, which is like Qt's Fusion style, and QApplication::setStyle() installs another one.

    \code
    QStyleOptionButton option;
    option.initFrom(button);
    option.text = button->text();
    button->style()->drawControl(QStyle::CE_PushButton, &option, &painter, button);
    \endcode

    \differences
    The element set is small: the push button, the check box, the line edit panel and the
    focus frame. There are no complex controls, \c polish(), \c standardIcon(), \c styleHint(), \c hitTestComplexControl(),
    style sheets or \c QStyleFactory, and no \c QStyleOption subclasses beyond the button and the frame.
    \enddifferences

    \sa QStyleOption, QApplication::setStyle(), QWidget::setStyle()
*/

/*!
    \enum QStyle::StateFlag

    This enum describes the state of an element that is drawn. The State type is a typedef
    for \c{QFlags<StateFlag>}.

    \value State_None Nothing is set.
    \value State_Enabled The element is enabled.
    \value State_Raised The element is raised: not pressed.
    \value State_Sunken The element is pressed.
    \value State_Off A checkable element is not checked.
    \value State_On A checkable element is checked.
    \value State_HasFocus The element has the keyboard focus.
    \value State_MouseOver The mouse is over the element.
    \value State_Active The window of the element is active.
*/

/*!
    \enum QStyle::PrimitiveElement

    This enum names the elements that a style draws with drawPrimitive().

    \value PE_Frame A generic frame.
    \value PE_FrameFocusRect The rectangle that shows the keyboard focus.
    \value PE_PanelButtonCommand The background of a push button.
    \value PE_IndicatorCheckBox The indicator of a check box.
    \value PE_PanelLineEdit The background of a line edit.
    \value PE_FrameLineEdit The frame of a line edit.
*/

/*!
    \enum QStyle::ControlElement

    This enum names the elements that a style draws with drawControl().

    \value CE_PushButton A push button: bevel and label.
    \value CE_PushButtonBevel The bevel of a push button.
    \value CE_PushButtonLabel The label of a push button.
    \value CE_CheckBox A check box: indicator and label.
    \value CE_CheckBoxLabel The label of a check box.
*/

/*!
    \enum QStyle::SubElement

    This enum names the parts of an element that subElementRect() finds.

    \value SE_CheckBoxIndicator The indicator of a check box.
    \value SE_CheckBoxContents The area of a check box that has the label.
    \value SE_PushButtonContents The area of a push button that has the label.
*/

/*!
    \enum QStyle::PixelMetric

    This enum names the sizes that a style provides to pixelMetric().

    \value PM_ButtonMargin The margin between the edge of a button and its label.
    \value PM_DefaultFrameWidth The width of the frame of a widget.
    \value PM_IndicatorWidth The width of a check box indicator.
    \value PM_IndicatorHeight The height of a check box indicator.
    \value PM_CheckBoxLabelSpacing The space between a check box indicator and its label.
    \value PM_LayoutLeftMargin The default left margin of a layout.
    \value PM_LayoutTopMargin The default top margin of a layout.
    \value PM_LayoutRightMargin The default right margin of a layout.
    \value PM_LayoutBottomMargin The default bottom margin of a layout.
    \value PM_LayoutHorizontalSpacing The default horizontal spacing of a layout.
    \value PM_LayoutVerticalSpacing The default vertical spacing of a layout.
*/

/*!
    \enum QStyle::ContentsType

    This enum names the widgets whose size sizeFromContents() computes.

    \value CT_PushButton A push button.
    \value CT_CheckBox A check box.
    \value CT_LineEdit A line edit.
*/

/*!
    Destroys the style.
*/
QStyle::~QStyle() = default;

/*!
    \fn void QStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const

    Draws \a element as \a option describes, with \a painter. \a widget is the widget that is drawn, and can be \c nullptr.
*/

/*!
    \fn void QStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const

    Draws \a element as \a option describes, with \a painter. \a widget is the widget that is drawn, and can be \c nullptr.
*/

/*!
    \fn QRect QStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const

    Returns the rectangle of \a element for the widget that \a option describes. \a widget is that widget, and may be \c nullptr.
*/

/*!
    \fn int QStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const

    Returns the value of \a metric, in pixels. \a option and \a widget may be \c nullptr.
*/

/*!
    \fn QSize QStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const

    Returns the size that a widget of \a type needs for contents of \a contentsSize: the contents plus the space that the style spends
    on frames and margins. \a option and \a widget describe the widget and may be \c nullptr.
*/

/*!
    Returns the palette that the application uses for its widgets when it has not been given one with
    QApplication::setPalette(). The default is the light palette of QPalette. A style that follows the
    color scheme of the desktop returns a dark palette when QStyleHints::colorScheme() is
    Qt::ColorScheme::Dark.

    \sa QApplication::palette(), QStyleHints::colorScheme()
*/
QPalette QStyle::standardPalette() const { return QPalette(); }

/*!
    \fn QStyle::QStyle()

    Constructs a style.
*/

#include "moc_qstyle.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
