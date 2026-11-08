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
    The element set covers the widgets of Nt: buttons, check and radio buttons, line edits, sliders, progress bars,
    tabs, combo boxes, spin boxes and group boxes. There are no complex controls (\c drawComplexControl() and
    \c QStyle::ComplexControl), so the slider, combo box, spin box and group box are drawn as control elements with a
    \c QStyleOption subclass of their own, and no \c polish(), \c standardIcon(), \c styleHint(),
    \c hitTestComplexControl(), style sheets or \c QStyleFactory.
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
    \value State_Selected The element is selected, as the current tab of a tab bar is.
    \value State_MouseOver The mouse is over the element.
    \value State_Active The window of the element is active.
    \value State_Horizontal The element is horizontal, as the handle of a splitter that divides left from right.
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
    \value PE_IndicatorRadioButton The indicator of a radio button.
    \value PE_IndicatorArrowUp An arrow that points up, as on a spin box button.
    \value PE_IndicatorArrowDown An arrow that points down, as on a combo box.
    \value PE_FrameGroupBox The frame of a group box.
    \value PE_FrameTabWidget The frame around the pages of a tab widget.
    \value PE_PanelTipLabel The background and the frame of a tool tip.
    \value PE_IndicatorArrowLeft An arrow that points left, as on a tool button.
    \value PE_IndicatorArrowRight An arrow that points right, as on a tool button.
    \value PE_PanelStatusBar The background of a status bar.
    \value PE_PanelMenu The background and the frame of a menu.
*/

/*!
    \enum QStyle::ControlElement

    This enum names the elements that a style draws with drawControl().

    \value CE_PushButton A push button: bevel and label.
    \value CE_PushButtonBevel The bevel of a push button.
    \value CE_PushButtonLabel The label of a push button.
    \value CE_CheckBox A check box: indicator and label.
    \value CE_CheckBoxLabel The label of a check box.
    \value CE_RadioButton A radio button: indicator and label.
    \value CE_RadioButtonLabel The label of a radio button.
    \value CE_Slider A slider: groove and handle, described by a QStyleOptionSlider.
    \value CE_ProgressBar A progress bar: groove, bar and text, described by a QStyleOptionProgressBar.
    \value CE_TabBarTab One tab of a tab bar, described by a QStyleOptionTab.
    \value CE_ComboBox A combo box without its popup, described by a QStyleOptionComboBox.
    \value CE_SpinBox The frame and buttons of a spin box, described by a QStyleOptionSpinBox.
    \value CE_GroupBox A group box: frame and title, described by a QStyleOptionGroupBox.
    \value CE_ScrollBar A scroll bar: groove and handle, described by a QStyleOptionSlider.
    \value CE_Header One section of a header: background and text, described by a QStyleOptionHeader.
    \value CE_Dial A dial: the face, the handle and the notches, described by a QStyleOptionSlider.
    \value CE_Splitter The handle of a splitter, which is horizontal if the state has State_Horizontal.
    \value CE_MenuItem An entry of a menu, with a QStyleOptionMenuItem.
    \value CE_MenuBarItem A title in a menu bar, with a QStyleOptionMenuItem.
    \value CE_MenuBarEmptyArea The background of a menu bar.
*/

/*!
    \enum QStyle::SubElement

    This enum names the parts of an element that subElementRect() finds.

    \value SE_CheckBoxIndicator The indicator of a check box.
    \value SE_CheckBoxContents The area of a check box that has the label.
    \value SE_PushButtonContents The area of a push button that has the label.
    \value SE_RadioButtonIndicator The indicator of a radio button.
    \value SE_RadioButtonContents The area of a radio button that has the label.
    \value SE_LineEditContents The area of a line edit in which the text is drawn.
    \value SE_SliderGroove The groove of a slider.
    \value SE_SliderHandle The handle of a slider, at the position in the QStyleOptionSlider.
    \value SE_ProgressBarContents The area of a progress bar that the bar and the text use.
    \value SE_ComboBoxArrow The part of a combo box that has the arrow.
    \value SE_SpinBoxEditField The part of a spin box in which the text is edited.
    \value SE_SpinBoxUp The button of a spin box that steps up.
    \value SE_SpinBoxDown The button of a spin box that steps down.
    \value SE_GroupBoxIndicator The check box in the title of a checkable group box.
    \value SE_GroupBoxTitle The title of a group box, with the check box if it has one.
    \value SE_GroupBoxContents The area inside the frame of a group box.
    \value SE_ScrollBarSlider The handle of a scroll bar, from the position, range and page step of a QStyleOptionSlider.
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
    \value PM_ExclusiveIndicatorWidth The width of a radio button indicator.
    \value PM_ExclusiveIndicatorHeight The height of a radio button indicator.
    \value PM_SliderThickness The width of a slider across its direction, handle included.
    \value PM_SliderLength The length of the handle of a slider.
    \value PM_SpinBoxButtonWidth The width of the buttons of a spin box.
    \value PM_ComboBoxArrowWidth The width of the part of a combo box that has the arrow.
    \value PM_ScrollBarExtent The thickness of a scroll bar: the width of a vertical one, the height of a horizontal one.
    \value PM_ScrollBarSliderMin The shortest that the handle of a scroll bar gets.
    \value PM_HeaderMargin The space on each side of the text in a section of a header.
    \value PM_HeaderDefaultSectionSizeHorizontal The width of a section of a horizontal header, or of a column of a table.
    \value PM_HeaderDefaultSectionSizeVertical The height of a section of a vertical header, or of a row of a table.
    \value PM_SplitterWidth The width of the handle of a splitter.
    \value PM_MenuBarItemSpacing The space between the titles of a menu bar, which is also the space before the first.
*/

/*!
    \enum QStyle::ContentsType

    This enum names the widgets whose size sizeFromContents() computes.

    \value CT_PushButton A push button.
    \value CT_CheckBox A check box.
    \value CT_LineEdit A line edit.
    \value CT_RadioButton A radio button.
    \value CT_ProgressBar A progress bar.
    \value CT_TabBarTab A tab of a tab bar.
    \value CT_ComboBox A combo box.
    \value CT_SpinBox A spin box.
    \value CT_ToolButton A tool button with a text.
    \value CT_MenuItem An entry of a menu: the size of its text becomes the size of the entry.
    \value CT_MenuBarItem A title of a menu bar.
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
