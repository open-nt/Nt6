// MIT License - see LICENSE
#include <QtWidgets/qstyleoption.h>
#include <cmath>

#include <QtWidgets/qwidget.h>

/*!
    \class QStyleOption
    \brief The QStyleOption class stores the parameters that QStyle functions use to draw.

    An option carries the rectangle to draw in, the state, the palette and the font metrics. Use
    initFrom() to set them from a widget. Subclasses add the data that an element needs, such as a text.

    \c{qstyleoption_cast<T>()} casts an option to the subclass \c T if its type is that of \c T, and returns
    \c nullptr otherwise, as a style does with the option it is given; a cast to QStyleOptionComplex succeeds for
    every complex option:

    \code
    if (const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option))
        drawTicks(painter, slider);
    \endcode

    \differences
    initFrom() leaves \c direction left to right, as Nt has no right-to-left layout of widgets.
    \enddifferences

    \sa QStyle, QStyleOptionButton
*/

/*!
    \enum QStyleOption::OptionType

    This enum identifies the kind of an option.

    \value SO_Default A plain option.
    \value SO_FocusRect A QStyleOptionFocusRect.
    \value SO_ToolBox A QStyleOptionToolBox.
    \value SO_DockWidget A QStyleOptionDockWidget.
    \value SO_TabWidgetFrame A QStyleOptionTabWidgetFrame.
    \value SO_TabBarBase A QStyleOptionTabBarBase.
    \value SO_RubberBand A QStyleOptionRubberBand.
    \value SO_ToolBar A QStyleOptionToolBar.
    \value SO_Complex A QStyleOptionComplex; the types of the complex options come after it.
    \value SO_ToolButton A QStyleOptionToolButton.
    \value SO_TitleBar A QStyleOptionTitleBar.
    \value SO_SizeGrip A QStyleOptionSizeGrip.
    \value SO_CustomBase The first type of an option of the application's own.
    \value SO_ComplexCustomBase The first type of a complex option of the application's own.
    \value SO_Button A QStyleOptionButton.
    \value SO_Frame A QStyleOptionFrame.
    \value SO_Slider A QStyleOptionSlider.
    \value SO_ProgressBar A QStyleOptionProgressBar.
    \value SO_Tab A QStyleOptionTab.
    \value SO_ComboBox A QStyleOptionComboBox.
    \value SO_SpinBox A QStyleOptionSpinBox.
    \value SO_GroupBox A QStyleOptionGroupBox.
    \value SO_Header A QStyleOptionHeader.
    \value SO_MenuItem A QStyleOptionMenuItem.
    \value SO_ViewItem A QStyleOptionViewItem.
    \value SO_GraphicsItem A QStyleOptionGraphicsItem.
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
    // Nt has no right-to-left layout of widgets yet.
    direction = Qt::LeftToRight;
    styleObject = const_cast<QWidget *>(widget); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's member
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
    \value HasMenu The button has a menu.
    \value DefaultButton The button is the default button.
    \value AutoDefaultButton The button becomes the default button when it has the focus.
    \value CommandLinkButton The button is a command link button.
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
    \variable QStyleOptionButton::icon
    \since 6.0

    The icon of the button, drawn before the text in iconSize.
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
    \variable QStyleOptionSlider::pageStep

    The part of the range that fits in a page. A scroll bar makes its handle that part of the groove long; a slider
    does not use it and leaves it at 0.
*/

/*!
    \variable QStyleOptionSlider::tickMarks

    Whether a dial draws its notches. A slider does not use it.
*/

/*!
    \variable QStyleOptionSlider::dialWrapping

    Whether a dial goes all the way round, so that its smallest and largest value are at the same place. A slider does
    not use it.
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
    \variable QStyleOptionComboBox::currentIcon
    \since 6.0

    The icon of the current item, drawn before its text in iconSize.
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
    \class QStyleOptionHeader
    \brief The QStyleOptionHeader class describes the parameters for drawing one section of a header.

    \sa QStyleOption, QHeaderView
*/

/*!
    \fn QStyleOptionHeader::QStyleOptionHeader()

    Constructs an option for a header section.
*/

/*!
    \variable QStyleOptionHeader::orientation

    The direction of the header: horizontal for the column headers of a table, vertical for the row headers.
*/

/*!
    \variable QStyleOptionHeader::text

    The text of the section.
*/

/*!
    \variable QStyleOptionHeader::textAlignment

    How the text is placed in the section, centered by default.
*/

/*!
    \variable QStyleOptionHeader::lastSection

    Whether this is the last section of the header, which has no separator after it.
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

/*!
    \class QStyleOptionMenuItem
    \brief The QStyleOptionMenuItem class describes the parameters for drawing an entry of a menu, or a title of a menu bar.

    \sa QStyleOption, QMenu, QMenuBar
*/

/*!
    \enum QStyleOptionMenuItem::MenuItemType

    This enum says what kind of entry a menu has.

    \value Normal An entry that triggers an action.
    \value DefaultItem The default entry of the menu.
    \value Separator A line between groups of entries.
    \value SubMenu An entry that opens another menu.
    \value Scroller The scroller of a menu that is too high.
    \value TearOff The tear-off entry.
    \value Margin The margin of the menu.
    \value EmptyArea The area of the menu that has no entries.
*/

/*!
    \enum QStyleOptionMenuItem::CheckType

    This enum says whether an entry of a menu has a check box.

    \value NotCheckable No check box.
    \value Exclusive A check box of an action in an exclusive group; Fusion draws it as it does for a non-exclusive one.
    \value NonExclusive A check box that is on or off by itself.
*/

/*!
    \fn QStyleOptionMenuItem::QStyleOptionMenuItem()

    Constructs an option for an entry of a menu.
*/

/*!
    \variable QStyleOptionMenuItem::menuItemType

    What kind of entry this is.
*/

/*!
    \variable QStyleOptionMenuItem::checkType

    Whether the entry has a check box.
*/

/*!
    \variable QStyleOptionMenuItem::checked

    Whether the check box is on.
*/

/*!
    \variable QStyleOptionMenuItem::text

    The text of the entry. A \c{&} marks the mnemonic, and a tab separates the text from the shortcut that is shown
    at the right.
*/

/*!
    \variable QStyleOptionMenuItem::icon
    \since 6.0

    The icon of the entry, drawn in the column of the check box when the entry is not checked.
*/

/*!
    \variable QStyleOptionMenuItem::font

    The font of the text.
*/

/*!
    \variable QStyleOptionMenuItem::reservedShortcutWidth

    The width of the widest shortcut of the entries before this one in the menu, which a style adds to the width of an
    entry that has a shortcut.
*/

/*!
    \enum QStyleOption::StyleOptionType

    \value Type The type of the option, \c SO_Default. qstyleoption_cast() compares it.
*/

/*!
    \enum QStyleOption::StyleOptionVersion

    The version of the options.

    \value Version The version, 1.
*/

/*!
    \fn QStyleOption::QStyleOption(int optionVersion, int optionType)

    Constructs an option of the type \a optionType and the version \a optionVersion.
*/
/*!
    \fn QStyleOption::QStyleOption(const QStyleOption &other)

    Constructs a copy of \a other.
*/
/*!
    \fn QStyleOption &QStyleOption::operator=(const QStyleOption &other)

    Makes the option a copy of \a other and returns it.
*/

/*!
    \variable QStyleOption::version

    The version of the option, 1.
*/

/*!
    \variable QStyleOption::direction

    The layout direction of the widget; left to right.
*/

/*!
    \variable QStyleOption::styleObject

    The widget that the option was made for by initFrom(), or \c nullptr.
*/

/*!
    \class QStyleOptionFocusRect
    \brief The QStyleOptionFocusRect class describes the parameters for drawing a focus rectangle.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionFocusRect::StyleOptionType

    \value Type The type of the option, \c SO_FocusRect. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionFocusRect::QStyleOptionFocusRect()

    Constructs an option for a focus rectangle.
*/

/*!
    \variable QStyleOptionFocusRect::backgroundColor

    The color behind the focus rectangle.
*/

/*!
    \enum QStyleOptionButton::StyleOptionType

    \value Type The type of the option, \c SO_Button. qstyleoption_cast() compares it.
*/

/*!
    \variable QStyleOptionButton::iconSize

    The size of the icon of the button.
*/

/*!
    \enum QStyleOptionFrame::StyleOptionType

    \value Type The type of the option, \c SO_Frame. qstyleoption_cast() compares it.
*/

/*!
    \enum QStyleOptionFrame::FrameFeature

    This enum describes how a frame is drawn. The FrameFeatures type is a typedef for \c{QFlags<FrameFeature>}.

    \value None A plain frame.
    \value Flat A flat frame.
    \value Rounded A frame with rounded corners.
*/

/*!
    \variable QStyleOptionFrame::midLineWidth

    The width of the line in the middle of the frame, 0 by default.
*/

/*!
    \variable QStyleOptionFrame::features

    How the frame is drawn.
*/

/*!
    \variable QStyleOptionFrame::frameShape

    The shape of the frame, a QFrame::Shape.
*/

/*!
    \class QStyleOptionTabWidgetFrame
    \brief The QStyleOptionTabWidgetFrame class describes the parameters for drawing the frame around the pages of a tab widget.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionTabWidgetFrame::StyleOptionType

    \value Type The type of the option, \c SO_TabWidgetFrame. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionTabWidgetFrame::QStyleOptionTabWidgetFrame()

    Constructs an option for the frame of a tab widget.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::lineWidth

    The width of the frame line.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::midLineWidth

    The width of the line in the middle of the frame.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::shape

    The shape of the tabs, a QTabBar::Shape.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::tabBarSize

    The size of the tab bar.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::rightCornerWidgetSize

    The size of the right corner widget.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::leftCornerWidgetSize

    The size of the left corner widget.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::tabBarRect

    The rectangle of the tab bar.
*/

/*!
    \variable QStyleOptionTabWidgetFrame::selectedTabRect

    The rectangle of the selected tab.
*/

/*!
    \class QStyleOptionTabBarBase
    \brief The QStyleOptionTabBarBase class describes the base of a tab bar, the line under its tabs.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionTabBarBase::StyleOptionType

    \value Type The type of the option, \c SO_TabBarBase. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionTabBarBase::QStyleOptionTabBarBase()

    Constructs an option for the base of a tab bar.
*/

/*!
    \variable QStyleOptionTabBarBase::shape

    The shape of the tabs, a QTabBar::Shape.
*/

/*!
    \variable QStyleOptionTabBarBase::tabBarRect

    The rectangle of the tab bar.
*/

/*!
    \variable QStyleOptionTabBarBase::selectedTabRect

    The rectangle of the selected tab.
*/

/*!
    \variable QStyleOptionTabBarBase::documentMode

    Whether the tab bar is in document mode.
*/

/*!
    \class QStyleOptionComplex
    \brief The QStyleOptionComplex class is the base of the options of complex controls.

    A complex control, such as a slider or a spin box, has parts that QStyle::subControlRect() finds and
    QStyle::hitTestComplexControl() tells apart.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionComplex::StyleOptionType

    \value Type The type of the option, \c SO_Complex. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionComplex::QStyleOptionComplex(int optionType)

    Constructs a complex option of the type \a optionType.
*/

/*!
    \variable QStyleOptionComplex::subControls

    The parts of the control that are drawn; all of them by default.
*/

/*!
    \variable QStyleOptionComplex::activeSubControls

    The parts of the control that are pressed or under the mouse; none by default.
*/

/*!
    \enum QStyleOptionSlider::StyleOptionType

    \value Type The type of the option, \c SO_Slider. qstyleoption_cast() compares it.
*/

/*!
    \variable QStyleOptionSlider::tickPosition

    Where the tick marks of a slider are, a QSlider::TickPosition.
*/

/*!
    \variable QStyleOptionSlider::tickInterval

    The distance between two tick marks, in values; 0 lets the style choose.
*/

/*!
    \variable QStyleOptionSlider::upsideDown

    Whether the smallest value is at the right or the bottom.
*/

/*!
    \variable QStyleOptionSlider::sliderValue

    The value of the slider, which can differ from sliderPosition while the handle is dragged.
*/

/*!
    \variable QStyleOptionSlider::singleStep

    The step of the arrow keys.
*/

/*!
    \variable QStyleOptionSlider::notchTarget

    The distance between the notches of a dial that the style aims for, in pixels.
*/

/*!
    \variable QStyleOptionSlider::keyboardModifiers

    The modifier keys that are held.
*/

/*!
    \enum QStyleOptionProgressBar::StyleOptionType

    \value Type The type of the option, \c SO_ProgressBar. qstyleoption_cast() compares it.
*/

/*!
    \variable QStyleOptionProgressBar::textAlignment

    How the text of the bar is aligned.
*/

/*!
    \variable QStyleOptionProgressBar::invertedAppearance

    Whether the bar grows from the other end.
*/

/*!
    \variable QStyleOptionProgressBar::bottomToTop

    Whether the text of a vertical bar reads from the bottom up.
*/

/*!
    \enum QStyleOptionTab::StyleOptionType

    \value Type The type of the option, \c SO_Tab. qstyleoption_cast() compares it.
*/

/*!
    \enum QStyleOptionTab::TabPosition

    Where a tab is in its tab bar.

    \value Beginning The first tab.
    \value Middle A tab between others.
    \value End The last tab.
    \value OnlyOneTab The only tab.
    \value Moving A tab that is being dragged.
*/

/*!
    \enum QStyleOptionTab::SelectedPosition

    Whether a tab is next to the selected one.

    \value NotAdjacent Neither neighbour is selected.
    \value NextIsSelected The next tab is selected.
    \value PreviousIsSelected The previous tab is selected.
*/

/*!
    \enum QStyleOptionTab::CornerWidget

    Which corner widgets the tab widget has. The CornerWidgets type is a typedef for \c{QFlags<CornerWidget>}.

    \value NoCornerWidgets None.
    \value LeftCornerWidget One on the left.
    \value RightCornerWidget One on the right.
*/

/*!
    \enum QStyleOptionTab::TabFeature

    What a tab has. The TabFeatures type is a typedef for \c{QFlags<TabFeature>}.

    \value None Nothing special.
    \value HasFrame The tab has a frame.
    \value MinimumSizeHint The size is asked for the minimum size hint.
*/

/*!
    \variable QStyleOptionTab::shape

    The shape of the tab, a QTabBar::Shape.
*/

/*!
    \variable QStyleOptionTab::row

    The row of the tab.
*/

/*!
    \variable QStyleOptionTab::position

    Where the tab is in its tab bar.
*/

/*!
    \variable QStyleOptionTab::selectedPosition

    Whether the tab is next to the selected one.
*/

/*!
    \variable QStyleOptionTab::cornerWidgets

    Which corner widgets the tab widget has.
*/

/*!
    \variable QStyleOptionTab::iconSize

    The size of the icon of the tab.
*/

/*!
    \variable QStyleOptionTab::icon
    \since 6.0

    The icon of the tab, drawn before its text.
*/

/*!
    \variable QStyleOptionTab::documentMode

    Whether the tab bar is in document mode.
*/

/*!
    \variable QStyleOptionTab::leftButtonSize

    The size of the widget on the left of the tab.
*/

/*!
    \variable QStyleOptionTab::rightButtonSize

    The size of the widget on the right of the tab.
*/

/*!
    \variable QStyleOptionTab::features

    What the tab has.
*/

/*!
    \variable QStyleOptionTab::tabIndex

    The index of the tab, or -1.
*/

/*!
    \class QStyleOptionToolBar
    \brief The QStyleOptionToolBar class describes the parameters for drawing a tool bar.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionToolBar::StyleOptionType

    \value Type The type of the option, \c SO_ToolBar. qstyleoption_cast() compares it.
*/

/*!
    \enum QStyleOptionToolBar::ToolBarPosition

    Where a tool bar is among the others.

    \value Beginning The first.
    \value Middle Between others.
    \value End The last.
    \value OnlyOne The only one.
*/

/*!
    \enum QStyleOptionToolBar::ToolBarFeature

    What a tool bar can do. The ToolBarFeatures type is a typedef for \c{QFlags<ToolBarFeature>}.

    \value None Nothing special.
    \value Movable It can be moved.
*/

/*!
    \fn QStyleOptionToolBar::QStyleOptionToolBar()

    Constructs an option for a tool bar.
*/

/*!
    \variable QStyleOptionToolBar::positionOfLine

    Where the line of the tool bar is among the lines of tool bars.
*/

/*!
    \variable QStyleOptionToolBar::positionWithinLine

    Where the tool bar is in its line.
*/

/*!
    \variable QStyleOptionToolBar::toolBarArea

    The area of the main window that the tool bar is in.
*/

/*!
    \variable QStyleOptionToolBar::features

    What the tool bar can do.
*/

/*!
    \variable QStyleOptionToolBar::lineWidth

    The width of the frame line.
*/

/*!
    \variable QStyleOptionToolBar::midLineWidth

    The width of the line in the middle of the frame.
*/

/*!
    \class QStyleOptionToolBox
    \brief The QStyleOptionToolBox class describes the parameters for drawing the title of a page of a tool box.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionToolBox::StyleOptionType

    \value Type The type of the option, \c SO_ToolBox. qstyleoption_cast() compares it.
*/

/*!
    \enum QStyleOptionToolBox::TabPosition

    Where a title is among the others.

    \value Beginning The first.
    \value Middle Between others.
    \value End The last.
    \value OnlyOneTab The only one.
*/

/*!
    \enum QStyleOptionToolBox::SelectedPosition

    Whether a title is next to the open page.

    \value NotAdjacent It is not.
    \value NextIsSelected The next page is open.
    \value PreviousIsSelected The previous page is open.
*/

/*!
    \fn QStyleOptionToolBox::QStyleOptionToolBox()

    Constructs an option for the title of a page of a tool box.
*/

/*!
    \variable QStyleOptionToolBox::text

    The title.
*/

/*!
    \variable QStyleOptionToolBox::position

    Where the title is among the others.
*/

/*!
    \variable QStyleOptionToolBox::selectedPosition

    Whether the title is next to the open page.
*/

/*!
    \class QStyleOptionDockWidget
    \brief The QStyleOptionDockWidget class describes the parameters for drawing the title bar of a dock widget.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionDockWidget::StyleOptionType

    \value Type The type of the option, \c SO_DockWidget. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionDockWidget::QStyleOptionDockWidget()

    Constructs an option for a dock widget.
*/

/*!
    \variable QStyleOptionDockWidget::title

    The title of the dock widget.
*/

/*!
    \variable QStyleOptionDockWidget::closable

    Whether it can be closed.
*/

/*!
    \variable QStyleOptionDockWidget::movable

    Whether it can be moved.
*/

/*!
    \variable QStyleOptionDockWidget::floatable

    Whether it can float.
*/

/*!
    \variable QStyleOptionDockWidget::verticalTitleBar

    Whether its title bar is vertical.
*/

/*!
    \class QStyleOptionRubberBand
    \brief The QStyleOptionRubberBand class describes the parameters for drawing a rubber band.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionRubberBand::StyleOptionType

    \value Type The type of the option, \c SO_RubberBand. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionRubberBand::QStyleOptionRubberBand()

    Constructs an option for a rubber band.
*/

/*!
    \variable QStyleOptionRubberBand::shape

    The shape of the rubber band, a QRubberBand::Shape.
*/

/*!
    \variable QStyleOptionRubberBand::opaque

    Whether the rubber band is opaque.
*/

/*!
    \enum QStyleOptionComboBox::StyleOptionType

    \value Type The type of the option, \c SO_ComboBox. qstyleoption_cast() compares it.
*/

/*!
    \variable QStyleOptionComboBox::editable

    Whether the combo box can be edited.
*/

/*!
    \variable QStyleOptionComboBox::popupRect

    Where the popup opens.
*/

/*!
    \variable QStyleOptionComboBox::frame

    Whether the combo box has a frame.
*/

/*!
    \variable QStyleOptionComboBox::iconSize

    The size of the icon of the current item.
*/

/*!
    \variable QStyleOptionComboBox::textAlignment

    How the text is aligned.
*/

/*!
    \enum QStyleOptionSpinBox::StyleOptionType

    \value Type The type of the option, \c SO_SpinBox. qstyleoption_cast() compares it.
*/

/*!
    \variable QStyleOptionSpinBox::buttonSymbols

    The symbols on the buttons, a QAbstractSpinBox::ButtonSymbols.
*/

/*!
    \variable QStyleOptionSpinBox::frame

    Whether the spin box has a frame.
*/

/*!
    \class QStyleOptionToolButton
    \brief The QStyleOptionToolButton class describes the parameters for drawing a tool button.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionToolButton::StyleOptionType

    \value Type The type of the option, \c SO_ToolButton. qstyleoption_cast() compares it.
*/

/*!
    \enum QStyleOptionToolButton::ToolButtonFeature

    What a tool button has. The ToolButtonFeatures type is a typedef for \c{QFlags<ToolButtonFeature>}.

    \value None Nothing special.
    \value Arrow An arrow instead of text.
    \value Menu A menu button beside it.
    \value MenuButtonPopup The same as Menu.
    \value PopupDelay A menu that opens after the button is held.
    \value HasMenu A menu.
*/

/*!
    \fn QStyleOptionToolButton::QStyleOptionToolButton()

    Constructs an option for a tool button.
*/

/*!
    \variable QStyleOptionToolButton::features

    What the tool button has.
*/

/*!
    \variable QStyleOptionToolButton::icon
    \since 6.0

    The icon of the button.
*/

/*!
    \variable QStyleOptionToolButton::text

    The text of the button.
*/

/*!
    \variable QStyleOptionToolButton::iconSize

    The size of the icon.
*/

/*!
    \variable QStyleOptionToolButton::font

    The font of the text.
*/

/*!
    \variable QStyleOptionToolButton::arrowType

    Which way the arrow points, for a button with an arrow.
*/

/*!
    \variable QStyleOptionToolButton::toolButtonStyle

    Whether the button shows its icon, its text or both.
*/

/*!
    \variable QStyleOptionToolButton::pos

    The position of the button.
*/

/*!
    \class QStyleOptionTitleBar
    \brief The QStyleOptionTitleBar class describes the parameters for drawing a title bar.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionTitleBar::StyleOptionType

    \value Type The type of the option, \c SO_TitleBar. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionTitleBar::QStyleOptionTitleBar()

    Constructs an option for a title bar.
*/

/*!
    \variable QStyleOptionTitleBar::text

    The title.
*/

/*!
    \variable QStyleOptionTitleBar::titleBarState

    The state of the window, a Qt::WindowStates.
*/

/*!
    \variable QStyleOptionTitleBar::titleBarFlags

    The flags of the window, which say which buttons it has.
*/

/*!
    \enum QStyleOptionGroupBox::StyleOptionType

    \value Type The type of the option, \c SO_GroupBox. qstyleoption_cast() compares it.
*/

/*!
    \variable QStyleOptionGroupBox::features

    How the frame is drawn.
*/

/*!
    \variable QStyleOptionGroupBox::textAlignment

    How the title is aligned.
*/

/*!
    \variable QStyleOptionGroupBox::textColor

    The color of the title.
*/

/*!
    \variable QStyleOptionGroupBox::lineWidth

    The width of the frame line.
*/

/*!
    \variable QStyleOptionGroupBox::midLineWidth

    The width of the line in the middle of the frame.
*/

/*!
    \class QStyleOptionSizeGrip
    \brief The QStyleOptionSizeGrip class describes the parameters for drawing a size grip.

    \sa QStyleOption, QStyle
*/

/*!
    \enum QStyleOptionSizeGrip::StyleOptionType

    \value Type The type of the option, \c SO_SizeGrip. qstyleoption_cast() compares it.
*/

/*!
    \fn QStyleOptionSizeGrip::QStyleOptionSizeGrip()

    Constructs an option for a size grip.
*/

/*!
    \variable QStyleOptionSizeGrip::corner

    The corner that the grip is in, a Qt::Corner; the bottom right one by default.
*/

/*!
    \enum QStyleOptionHeader::StyleOptionType

    \value Type The type of the option, \c SO_Header. qstyleoption_cast() compares it.
*/

/*!
    \enum QStyleOptionHeader::SectionPosition

    Where a section is in its header.

    \value Beginning The first section.
    \value Middle A section between others.
    \value End The last section.
    \value OnlyOneSection The only section.
*/

/*!
    \enum QStyleOptionHeader::SelectedPosition

    Whether a section is next to one that is selected.

    \value NotAdjacent Neither neighbour is selected.
    \value NextIsSelected The next section is selected.
    \value PreviousIsSelected The previous section is selected.
    \value NextAndPreviousAreSelected Both neighbours are selected.
*/

/*!
    \variable QStyleOptionHeader::section

    The logical index of the section.
*/

/*!
    \variable QStyleOptionHeader::iconAlignment

    How the icon of the section is aligned.
*/

/*!
    \variable QStyleOptionHeader::position

    Where the section is in its header.
*/

/*!
    \variable QStyleOptionHeader::selectedPosition

    Whether the section is next to one that is selected.
*/

/*!
    \enum QStyleOptionMenuItem::StyleOptionType

    \value Type The type of the option, \c SO_MenuItem. qstyleoption_cast() compares it.
*/

/*!
    \variable QStyleOptionMenuItem::menuHasCheckableItems

    Whether the menu has entries with check boxes.
*/

/*!
    \variable QStyleOptionMenuItem::menuRect

    The rectangle of the whole menu.
*/

/*!
    \variable QStyleOptionMenuItem::maxIconWidth

    The width of the widest icon of the menu.
*/

/*!
    \class QStyleOptionGraphicsItem
    \brief The QStyleOptionGraphicsItem class describes what a QGraphicsItem paints.

    QGraphicsScene passes it to QGraphicsItem::paint(): rect is the bounding rectangle, state says whether the item
    is enabled, selected, focused or under the mouse, and exposedRect is the part that needs painting.

    \differences
    The palette is that of the view's viewport. There is no \c matrix or \c levelOfDetail member.
    \enddifferences
*/

/*!
    Returns how many device pixels one unit of item coordinates covers under \a worldTransform: the square root of
    the area a unit square maps to. Items draw less detail when it is small.
*/
qreal QStyleOptionGraphicsItem::levelOfDetailFromTransform(const QTransform &worldTransform) {
    if (worldTransform.type() <= QTransform::TxTranslate)
        return 1.0;
    const QRectF unit = worldTransform.mapRect(QRectF(0, 0, 1, 1));
    return std::sqrt(unit.width() * unit.height());
}

/*!
    \class QStyleHintReturn
    \inmodule NtWidgets
    \since 6.0
    \brief The QStyleHintReturn class is what QStyle::styleHint() gives back beyond its number.

    A style fills it in for the hints that return more than a number, such as a mask. Find out the kind with
    qstyleoption_cast().

    \differences
    The styles of Nt fill in none yet.
    \enddifferences

    \sa QStyleHintReturnMask, QStyleHintReturnVariant
*/

/*!
    \class QStyleHintReturnMask
    \inmodule NtWidgets
    \since 6.0
    \brief The QStyleHintReturnMask class is a style hint that gives back a region.

    \differences
    None.
    \enddifferences
*/

/*!
    \class QStyleHintReturnVariant
    \inmodule NtWidgets
    \since 6.0
    \brief The QStyleHintReturnVariant class is a style hint that gives back a QVariant.

    \differences
    None.
    \enddifferences
*/
