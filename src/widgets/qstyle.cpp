// MIT License - see LICENSE
#include <QtWidgets/qstyle.h>

#include <QtCore/qcoreevent.h>
#include <QtCore/qlist.h>

#include <QtGui/qfontmetrics.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <functional>

namespace {
constexpr Qt::Alignment kHorizontal = Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter;
constexpr Qt::Alignment kVertical = Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter;
} // namespace

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
    Nt's widgets draw the slider, scroll bar, combo box, spin box, group box and dial as control elements
    (\c CE_Slider and so on, which Qt does not have) with the option of the complex control, and the default
    drawComplexControl() draws those elements. Elements, metrics and hints that Nt's widgets do not use are
    drawn with similar ones, answered as Qt's Fusion style answers them, or not drawn. There is no
    \c standardIcon(), as Nt has no \c QIcon, and no style sheets. layoutSpacing() answers the same for every pair of control types. drawItemText() does
    not draw mnemonics.
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
    \value State_NoChange A check box that is partly checked.
    \value State_DownArrow An arrow that points down.
    \value State_Top At the top, as a tab of a tab bar on the north edge.
    \value State_Bottom At the bottom.
    \value State_FocusAtBorder The focus frame is drawn at the border, not inside.
    \value State_AutoRaise The element is raised only under the mouse, as an auto-raise tool button.
    \value State_UpArrow An arrow that points up.
    \value State_Window The element is a window.
    \value State_Open An item of a tree that is expanded.
    \value State_Children An item of a tree that has children.
    \value State_Item An item of a tree that is drawn with a branch line.
    \value State_Sibling An item of a tree that has siblings below it.
    \value State_Editing The element is being edited.
    \value State_KeyboardFocusChange The focus last moved with the keyboard.
    \value State_ReadOnly The element can only be read.
    \value State_Small A small version of the element.
    \value State_Mini A very small version of the element.
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
    \value PE_FrameDefaultButton The frame around a default button.
    \value PE_FrameDockWidget The frame of a dock widget that floats.
    \value PE_FrameMenu The frame of a menu.
    \value PE_FrameStatusBarItem The frame of an item of a status bar.
    \value PE_FrameWindow The frame of a window, such as an MDI sub-window.
    \value PE_FrameButtonBevel The frame of a button bevel.
    \value PE_FrameButtonTool The frame of a tool button.
    \value PE_FrameTabBarBase The line under a row of tabs.
    \value PE_PanelButtonBevel The bevel of a button.
    \value PE_PanelButtonTool The panel of a tool button.
    \value PE_PanelMenuBar The panel of a menu bar.
    \value PE_PanelToolBar The panel of a tool bar.
    \value PE_IndicatorBranch The lines and the arrow of a branch of a tree view.
    \value PE_IndicatorButtonDropDown The part of a tool button that drops down its menu.
    \value PE_IndicatorItemViewItemCheck The check box of an item of an item view.
    \value PE_IndicatorDockWidgetResizeHandle The handle for resizing dock widgets.
    \value PE_IndicatorHeaderArrow The sort arrow of a header section.
    \value PE_IndicatorMenuCheckMark The check mark of a menu entry.
    \value PE_IndicatorProgressChunk A chunk of the bar of a progress bar.
    \value PE_IndicatorSpinDown The down symbol of a spin box.
    \value PE_IndicatorSpinMinus The minus symbol of a spin box.
    \value PE_IndicatorSpinPlus The plus symbol of a spin box.
    \value PE_IndicatorSpinUp The up symbol of a spin box.
    \value PE_IndicatorToolBarHandle The handle by which a tool bar is dragged.
    \value PE_IndicatorToolBarSeparator A separator between the items of a tool bar.
    \value PE_IndicatorTabTearLeft The mark that tabs are cut off on the left of a tab bar.
    \value PE_PanelScrollAreaCorner The corner between the two scroll bars of a scroll area.
    \value PE_Widget The background of a plain widget.
    \value PE_IndicatorColumnViewArrow The arrow of a column view.
    \value PE_IndicatorItemViewItemDrop The mark of where a dragged item would be dropped in an item view.
    \value PE_PanelItemViewItem The background of an item of an item view.
    \value PE_PanelItemViewRow The background of a row of an item view.
    \value PE_IndicatorTabClose The close button of a tab.
    \value PE_IndicatorTabTearRight The mark that tabs are cut off on the right of a tab bar.
    \value PE_CustomBase The first value for elements of a style of the application's own.
    \omitvalue PE_IndicatorTabTear
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
    \value CE_TabBarTabShape The shape of a tab, without its label.
    \value CE_TabBarTabLabel The label of a tab.
    \value CE_ProgressBarGroove The groove of a progress bar.
    \value CE_ProgressBarContents The bar of a progress bar.
    \value CE_ProgressBarLabel The text of a progress bar.
    \value CE_MenuScroller The scroller of a menu that is too high for the screen.
    \value CE_MenuVMargin The top and bottom margins of a menu.
    \value CE_MenuHMargin The left and right margins of a menu.
    \value CE_MenuTearoff The tear-off entry of a menu.
    \value CE_MenuEmptyArea The area of a menu that has no entries.
    \value CE_ToolButtonLabel The label of a tool button.
    \value CE_HeaderSection The background of a header section.
    \value CE_HeaderLabel The text of a header section.
    \value CE_ToolBoxTab The title of a page of a tool box.
    \value CE_SizeGrip A size grip.
    \value CE_RubberBand A rubber band.
    \value CE_DockWidgetTitle The title bar of a dock widget.
    \value CE_ScrollBarAddLine The button of a scroll bar that scrolls forward.
    \value CE_ScrollBarSubLine The button of a scroll bar that scrolls back.
    \value CE_ScrollBarAddPage The part of the groove of a scroll bar after the handle.
    \value CE_ScrollBarSubPage The part of the groove of a scroll bar before the handle.
    \value CE_ScrollBarSlider The handle of a scroll bar.
    \value CE_ScrollBarFirst The button of a scroll bar that scrolls to the start.
    \value CE_ScrollBarLast The button of a scroll bar that scrolls to the end.
    \value CE_FocusFrame A focus frame.
    \value CE_ComboBoxLabel The text of a combo box.
    \value CE_ToolBar A tool bar.
    \value CE_ToolBoxTabShape The shape of the title of a page of a tool box.
    \value CE_ToolBoxTabLabel The text of the title of a page of a tool box.
    \value CE_HeaderEmptyArea The part of a header after its last section.
    \value CE_ColumnViewGrip The grip of a column view.
    \value CE_ItemViewItem An item of an item view.
    \value CE_ShapedFrame A frame of a shape and shadow from a QStyleOptionFrame.
    \value CE_CustomBase The first value for elements of a style of the application's own.
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
    \value SE_PushButtonFocusRect The focus rectangle of a push button.
    \value SE_CheckBoxFocusRect The focus rectangle of a check box.
    \value SE_CheckBoxClickRect The area of a check box that reacts to the mouse.
    \value SE_RadioButtonFocusRect The focus rectangle of a radio button.
    \value SE_RadioButtonClickRect The area of a radio button that reacts to the mouse.
    \value SE_ComboBoxFocusRect The focus rectangle of a combo box.
    \value SE_SliderFocusRect The focus rectangle of a slider.
    \value SE_ProgressBarGroove The groove of a progress bar.
    \value SE_ProgressBarLabel The text of a progress bar.
    \value SE_ToolBoxTabContents The text of the title of a page of a tool box.
    \value SE_HeaderLabel The text of a header section.
    \value SE_HeaderArrow The sort arrow of a header section.
    \value SE_TabWidgetTabBar The tab bar of a tab widget.
    \value SE_TabWidgetTabPane The frame around the pages of a tab widget.
    \value SE_TabWidgetTabContents The area for the page of a tab widget.
    \value SE_TabWidgetLeftCorner The left corner widget of a tab widget.
    \value SE_TabWidgetRightCorner The right corner widget of a tab widget.
    \value SE_ItemViewItemCheckIndicator The check box of an item of an item view.
    \value SE_TabBarTearIndicatorLeft The mark that tabs are cut off on the left.
    \value SE_TreeViewDisclosureItem The arrow of an item of a tree view.
    \value SE_FrameContents The area inside a frame.
    \value SE_DockWidgetCloseButton The close button of a dock widget.
    \value SE_DockWidgetFloatButton The float button of a dock widget.
    \value SE_DockWidgetTitleBarText The text of the title bar of a dock widget.
    \value SE_DockWidgetIcon The icon of a dock widget.
    \value SE_CheckBoxLayoutItem The rectangle of a check box that a layout lines up.
    \value SE_ComboBoxLayoutItem The rectangle of a combo box that a layout lines up.
    \value SE_DateTimeEditLayoutItem The rectangle of a date and time edit that a layout lines up.
    \value SE_LabelLayoutItem The rectangle of a label that a layout lines up.
    \value SE_ProgressBarLayoutItem The rectangle of a progress bar that a layout lines up.
    \value SE_PushButtonLayoutItem The rectangle of a push button that a layout lines up.
    \value SE_RadioButtonLayoutItem The rectangle of a radio button that a layout lines up.
    \value SE_SliderLayoutItem The rectangle of a slider that a layout lines up.
    \value SE_SpinBoxLayoutItem The rectangle of a spin box that a layout lines up.
    \value SE_ToolButtonLayoutItem The rectangle of a tool button that a layout lines up.
    \value SE_FrameLayoutItem The rectangle of a frame that a layout lines up.
    \value SE_GroupBoxLayoutItem The rectangle of a group box that a layout lines up.
    \value SE_TabWidgetLayoutItem The rectangle of a tab widget that a layout lines up.
    \value SE_ItemViewItemDecoration The decoration of an item of an item view.
    \value SE_ItemViewItemText The text of an item of an item view.
    \value SE_ItemViewItemFocusRect The focus rectangle of an item of an item view.
    \value SE_TabBarTabLeftButton The widget on the left of a tab.
    \value SE_TabBarTabRightButton The widget on the right of a tab.
    \value SE_TabBarTabText The text of a tab.
    \value SE_ShapedFrameContents The area inside a frame of a shape from a QStyleOptionFrame.
    \value SE_ToolBarHandle The handle of a tool bar.
    \value SE_TabBarScrollLeftButton The button that scrolls a tab bar to the left.
    \value SE_TabBarScrollRightButton The button that scrolls a tab bar to the right.
    \value SE_TabBarTearIndicatorRight The mark that tabs are cut off on the right.
    \value SE_PushButtonBevel The bevel of a push button.
    \value SE_CustomBase The first value for sub-elements of a style of the application's own.
    \omitvalue SE_TabBarTearIndicator
*/

/*!
    \enum QStyle::ComplexControl

    This enum names the controls of several parts that a style draws with drawComplexControl().

    \value CC_SpinBox A spin box: QStyleOptionSpinBox.
    \value CC_ComboBox A combo box: QStyleOptionComboBox.
    \value CC_ScrollBar A scroll bar: QStyleOptionSlider.
    \value CC_Slider A slider: QStyleOptionSlider.
    \value CC_ToolButton A tool button: QStyleOptionToolButton.
    \value CC_TitleBar A title bar: QStyleOptionTitleBar.
    \value CC_Dial A dial: QStyleOptionSlider.
    \value CC_GroupBox A group box: QStyleOptionGroupBox.
    \value CC_MdiControls The buttons of a maximized MDI sub-window: QStyleOptionComplex.
    \value CC_CustomBase The first value for complex controls of a style of the application's own.
*/

/*!
    \enum QStyle::SubControl

    This enum names the parts of a complex control. Each control has its own values, so the values of
    different controls overlap. The SubControls type is a typedef for \c{QFlags<SubControl>}.

    \value SC_None No sub-control.
    \value SC_ScrollBarAddLine The button of a scroll bar that scrolls forward.
    \value SC_ScrollBarSubLine The button of a scroll bar that scrolls back.
    \value SC_ScrollBarAddPage The part of the groove after the handle.
    \value SC_ScrollBarSubPage The part of the groove before the handle.
    \value SC_ScrollBarFirst The button that scrolls to the start.
    \value SC_ScrollBarLast The button that scrolls to the end.
    \value SC_ScrollBarSlider The handle of a scroll bar.
    \value SC_ScrollBarGroove The groove of a scroll bar.
    \value SC_SpinBoxUp The button of a spin box that steps up.
    \value SC_SpinBoxDown The button of a spin box that steps down.
    \value SC_SpinBoxFrame The frame of a spin box.
    \value SC_SpinBoxEditField The part of a spin box in which the text is edited.
    \value SC_ComboBoxFrame The frame of a combo box.
    \value SC_ComboBoxEditField The text of a combo box.
    \value SC_ComboBoxArrow The arrow of a combo box.
    \value SC_ComboBoxListBoxPopup Where the popup of a combo box opens.
    \value SC_SliderGroove The groove of a slider.
    \value SC_SliderHandle The handle of a slider.
    \value SC_SliderTickmarks The tick marks of a slider.
    \value SC_ToolButton The button of a tool button.
    \value SC_ToolButtonMenu The part of a tool button that opens its menu.
    \value SC_TitleBarSysMenu The system menu of a title bar.
    \value SC_TitleBarMinButton The minimize button of a title bar.
    \value SC_TitleBarMaxButton The maximize button of a title bar.
    \value SC_TitleBarCloseButton The close button of a title bar.
    \value SC_TitleBarNormalButton The button of a title bar that restores the window.
    \value SC_TitleBarShadeButton The shade button of a title bar.
    \value SC_TitleBarUnshadeButton The unshade button of a title bar.
    \value SC_TitleBarContextHelpButton The help button of a title bar.
    \value SC_TitleBarLabel The text of a title bar.
    \value SC_DialGroove The groove of a dial.
    \value SC_DialHandle The handle of a dial.
    \value SC_DialTickmarks The notches of a dial.
    \value SC_GroupBoxCheckBox The check box of a group box.
    \value SC_GroupBoxLabel The title of a group box.
    \value SC_GroupBoxContents The area inside the frame of a group box.
    \value SC_GroupBoxFrame The frame of a group box.
    \value SC_MdiMinButton The minimize button of a maximized MDI sub-window.
    \value SC_MdiNormalButton The restore button of a maximized MDI sub-window.
    \value SC_MdiCloseButton The close button of a maximized MDI sub-window.
    \value SC_CustomBase The first value for sub-controls of a style of the application's own.
    \value SC_All All sub-controls.
*/

/*!
    \enum QStyle::StyleHint

    This enum names the questions about behavior that styleHint() answers. The answers of the default
    style are given with each value.

    \value SH_EtchDisabledText Whether disabled text is drawn etched. The default is \c false.
    \value SH_DitherDisabledText Whether disabled text is drawn dithered. The default is \c false.
    \value SH_ScrollBar_MiddleClickAbsolutePosition Whether a middle click on a scroll bar moves the handle there. The default is \c true.
    \value SH_ScrollBar_ScrollWhenPointerLeavesControl Whether a scroll bar keeps scrolling while the pressed mouse is outside it. The default is \c true.
    \value SH_TabBar_SelectMouseType The mouse event that selects a tab: QEvent::MouseButtonPress by default.
    \value SH_TabBar_Alignment Where the tabs of a tab widget are, along its edge: Qt::AlignLeft by default.
    \value SH_Header_ArrowAlignment Where the sort arrow of a header section is.
    \value SH_Slider_SnapToValue Whether the handle of a slider snaps to the values. The default is \c true.
    \value SH_Slider_SloppyKeyEvents Whether a slider takes the arrow keys of the other direction too. The default is \c false.
    \value SH_ProgressDialog_CenterCancelButton Whether the cancel button of a progress dialog is centered. The default is \c false.
    \value SH_ProgressDialog_TextLabelAlignment The alignment of the text of a progress dialog: centered by default.
    \value SH_PrintDialog_RightAlignButtons Whether the buttons of a print dialog are on the right. The default is \c true.
    \value SH_MainWindow_SpaceBelowMenuBar Whether there is room between the menu bar and the tool bars. The default is \c false.
    \value SH_FontDialog_SelectAssociatedText Whether a font dialog selects the text of a field that gets the focus. The default is \c true.
    \value SH_Menu_AllowActiveAndDisabled Whether a disabled menu entry can be highlighted. The default is \c false.
    \value SH_Menu_SpaceActivatesItem Whether Space triggers the highlighted menu entry. The default is \c true.
    \value SH_Menu_SubMenuPopupDelay The milliseconds before a submenu opens under the mouse: 225 by default.
    \value SH_ScrollView_FrameOnlyAroundContents Whether the frame of a scroll area leaves its scroll bars out. The default is \c false.
    \value SH_MenuBar_AltKeyNavigation Whether Alt alone moves the focus to the menu bar. The default is \c true.
    \value SH_ComboBox_ListMouseTracking Whether the popup of a combo box highlights the item under the mouse. The default is \c true.
    \value SH_Menu_MouseTracking Whether a menu highlights the entry under the mouse. The default is \c true.
    \value SH_MenuBar_MouseTracking Whether a menu bar highlights the title under the mouse. The default is \c true.
    \value SH_ItemView_ChangeHighlightOnFocus Whether selected items are drawn grayed when the view loses the focus. The default is \c false.
    \value SH_Widget_ShareActivation Whether a tool window shares the activation of its parent. The default is \c true.
    \value SH_Workspace_FillSpaceOnMaximize Whether a maximized MDI sub-window fills the area. The default is \c true.
    \value SH_ComboBox_Popup Whether the popup of a combo box opens over the box, as a menu does. The default is \c false.
    \value SH_TitleBar_NoBorder Whether a title bar has no border. The default is \c false.
    \value SH_Slider_StopMouseOverSlider Whether a pressed groove stops scrolling under the mouse. The default is \c false.
    \value SH_BlinkCursorWhenTextSelected Whether the text cursor blinks while text is selected. The default is \c true.
    \value SH_RichText_FullWidthSelection Whether the selection of rich text goes to the right edge. The default is \c true.
    \value SH_Menu_Scrollable Whether a menu that is too high scrolls. The default is \c false: Nt's menus do not scroll.
    \value SH_GroupBox_TextLabelVerticalAlignment How the title of a group box lines up with its frame: Qt::AlignVCenter by default.
    \value SH_GroupBox_TextLabelColor The color of the title of a group box, as a QRgb: the window text color of the option's palette.
    \value SH_Menu_SloppySubMenus Whether a submenu stays open while the mouse crosses to it over other entries. The default is \c true.
    \value SH_Table_GridLineColor The color of the grid of a table, as a QRgb: a little darker than the base color of the option's palette, or lighter if it is dark.
    \value SH_LineEdit_PasswordCharacter The character that a password line edit shows for each character: U+25CF, a black circle.
    \value SH_DialogButtons_DefaultButton The role of the default button of a dialog: QDialogButtonBox::AcceptRole.
    \value SH_ToolBox_SelectedPageTitleBold Whether the title of the open page of a tool box is bold. The default is \c true.
    \value SH_TabBar_PreferNoArrows Whether a tab bar grows instead of showing scroll buttons. The default is \c false.
    \value SH_ScrollBar_LeftClickAbsolutePosition Whether a left click on a scroll bar moves the handle there. The default is \c false.
    \value SH_ListViewExpand_SelectMouseType The mouse event that opens an item of a tree: QEvent::MouseButtonPress by default.
    \value SH_UnderlineShortcut Whether mnemonics are underlined. The default is \c true.
    \value SH_SpinBox_AnimateButton Whether the buttons of a spin box are drawn pressed while they repeat. The default is \c false.
    \value SH_SpinBox_KeyPressAutoRepeatRate The milliseconds between the steps of a spin box while a key is held: 75 by default.
    \value SH_SpinBox_ClickAutoRepeatRate The milliseconds between the steps of a spin box while a button is held: 150 by default.
    \value SH_Menu_FillScreenWithScroll Whether a scrolling menu fills the height of the screen. The default is \c false.
    \value SH_ToolTipLabel_Opacity The opacity of tool tips, from 0 to 255: 242 by default.
    \value SH_DrawMenuBarSeparator Whether a line separates the menu bar from the window. The default is \c false.
    \value SH_TitleBar_ModifyNotification Whether the title bar shows that the document has changed. The default is \c true.
    \value SH_Button_FocusPolicy The focus policy of buttons: Qt::StrongFocus by default.
    \value SH_MessageBox_UseBorderForButtonSpacing Whether the buttons of a message box are spaced with the border. The default is \c false.
    \value SH_TitleBar_AutoRaise Whether the buttons of a title bar are raised under the mouse. The default is \c false.
    \value SH_ToolButton_PopupDelay The milliseconds a tool button must be held before its menu opens: 600 by default.
    \value SH_FocusFrame_Mask Whether a focus frame has a mask, given in a QStyleHintReturnMask. The default is \c false.
    \value SH_RubberBand_Mask Whether a rubber band has a mask, given in a QStyleHintReturnMask. The default is \c false.
    \value SH_WindowFrame_Mask Whether a window frame has a mask, given in a QStyleHintReturnMask. The default is \c false.
    \value SH_SpinControls_DisableOnBounds Whether the button of a spin box is disabled at the end of the range. The default is \c true.
    \value SH_Dial_BackgroundRole The palette role of the background of a dial: QPalette::Window.
    \value SH_ComboBox_LayoutDirection The layout direction of a combo box: that of the option.
    \value SH_ItemView_EllipsisLocation Where the text of an item of a view gets its ellipsis: at the end, Qt::AlignRight.
    \value SH_ItemView_ShowDecorationSelected Whether the whole item, decoration too, shows the selection. The default is \c false.
    \value SH_ItemView_ActivateItemOnSingleClick Whether a single click activates an item of a view. The default is \c false.
    \value SH_ScrollBar_ContextMenu Whether a scroll bar has a context menu. The default is \c true.
    \value SH_ScrollBar_RollBetweenButtons Whether the mouse can roll between the buttons of a scroll bar. The default is \c false.
    \value SH_Slider_AbsoluteSetButtons The mouse buttons that move the handle of a slider to where they are pressed: Qt::MiddleButton.
    \value SH_Slider_PageSetButtons The mouse buttons that move the handle of a slider a page: Qt::LeftButton.
    \value SH_Menu_KeyboardSearch Whether typing in a menu finds an entry by its text. The default is \c false.
    \value SH_TabBar_ElideMode Where the text of a tab that is too long gets its ellipsis: Qt::ElideNone by default.
    \value SH_DialogButtonLayout The order of the buttons of a dialog: QDialogButtonBox::KdeLayout.
    \value SH_ComboBox_PopupFrameStyle The frame style of the popup of a combo box: QFrame::StyledPanel with QFrame::Plain.
    \value SH_MessageBox_TextInteractionFlags How the user can work with the text of a message box: links can be clicked.
    \value SH_DialogButtonBox_ButtonsHaveIcons Whether the buttons of a dialog have icons. The default is \c false.
    \value SH_MessageBox_CenterButtons Whether the buttons of a message box are centered. The default is \c false.
    \value SH_Menu_SelectionWrap Whether the highlight of a menu wraps from the last entry to the first. The default is \c true.
    \value SH_ItemView_MovementWithoutUpdatingSelection Whether Control and an arrow key move the current item without selecting. The default is \c true.
    \value SH_ToolTip_Mask Whether a tool tip has a mask, given in a QStyleHintReturnMask. The default is \c false.
    \value SH_FocusFrame_AboveWidget Whether a focus frame is drawn above its widget. The default is \c false.
    \value SH_TextControl_FocusIndicatorTextCharFormat The text format of the focus of a link in rich text, given in a QStyleHintReturnVariant. The default is \c false: none.
    \value SH_WizardStyle The look of a wizard: QWizard::ClassicStyle.
    \value SH_ItemView_ArrowKeysNavigateIntoChildren Whether Right goes to the first child of an open item of a tree. The default is \c false.
    \value SH_Menu_Mask Whether a menu has a mask, given in a QStyleHintReturnMask. The default is \c false.
    \value SH_Menu_FlashTriggeredItem Whether a triggered menu entry flashes. The default is \c false.
    \value SH_Menu_FadeOutOnHide Whether a menu fades out when it closes. The default is \c false.
    \value SH_SpinBox_ClickAutoRepeatThreshold The milliseconds a spin box button must be held before it repeats: 500 by default.
    \value SH_ItemView_PaintAlternatingRowColorsForEmptyArea Whether a view draws alternating rows below its last item. The default is \c false.
    \value SH_FormLayoutWrapPolicy How a form layout wraps its rows: QFormLayout::DontWrapRows.
    \value SH_TabWidget_DefaultTabPosition Where the tabs of a tab widget are: QTabWidget::North.
    \value SH_ToolBar_Movable Whether tool bars can be moved. The default is \c true.
    \value SH_FormLayoutFieldGrowthPolicy How the fields of a form layout grow: QFormLayout::AllNonFixedFieldsGrow.
    \value SH_FormLayoutFormAlignment Where a form layout puts its rows: at the top left.
    \value SH_FormLayoutLabelAlignment How the labels of a form layout are aligned: on the left.
    \value SH_ItemView_DrawDelegateFrame Whether a view draws a frame around an item that is edited. The default is \c false.
    \value SH_TabBar_CloseButtonPosition Where the close button of a tab is: QTabBar::RightSide.
    \value SH_DockWidget_ButtonsHaveFrame Whether the buttons of a dock widget have a frame. The default is \c true.
    \value SH_ToolButtonStyle The default style of tool buttons: Qt::ToolButtonIconOnly.
    \value SH_RequestSoftwareInputPanel When a widget asks for an on-screen keyboard: when it is clicked and has the focus already.
    \value SH_ScrollBar_Transient Whether scroll bars show only while scrolling. The default is \c false.
    \value SH_Menu_SupportsSections Whether menus draw the titles of sections. The default is \c false.
    \value SH_ToolTip_WakeUpDelay The milliseconds the mouse rests before a tool tip shows: 700 by default.
    \value SH_ToolTip_FallAsleepDelay The milliseconds after which the next tool tip waits again: 2000 by default.
    \value SH_Widget_Animate Whether widgets animate. The default is \c false: Nt does not animate.
    \value SH_Splitter_OpaqueResize Whether a splitter resizes its panes while the handle is dragged. The default is \c true.
    \value SH_ComboBox_UseNativePopup Whether a combo box uses a popup of the platform. The default is \c false.
    \value SH_LineEdit_PasswordMaskDelay The milliseconds a typed password character shows before it is masked: 0 by default.
    \value SH_TabBar_ChangeCurrentDelay The milliseconds a drag must rest on a tab before it becomes current: 500 by default.
    \value SH_Menu_SubMenuUniDirection Whether a submenu opens only on the side where there is room. The default is \c false.
    \value SH_Menu_SubMenuUniDirectionFailCount How often the mouse may move away from a submenu before it closes: 1.
    \value SH_Menu_SubMenuSloppySelectOtherActions Whether other entries are highlighted while a submenu is open sloppily. The default is \c true.
    \value SH_Menu_SubMenuSloppyCloseTimeout The milliseconds before a sloppy submenu closes: 1000 by default.
    \value SH_Menu_SubMenuResetWhenReenteringParent Whether the sloppy state resets when the mouse comes back to the parent. The default is \c false.
    \value SH_Menu_SubMenuDontStartSloppyOnLeave Whether leaving a menu does not start the sloppy state. The default is \c false.
    \value SH_ItemView_ScrollMode How views scroll: QAbstractItemView::ScrollPerPixel, as Nt's views do.
    \value SH_TitleBar_ShowToolTipsOnButtons Whether the buttons of a title bar have tool tips. The default is \c true.
    \value SH_Widget_Animation_Duration The milliseconds an animation of a widget takes: 0, as Nt does not animate.
    \value SH_ComboBox_AllowWheelScrolling Whether the wheel changes the item of a combo box. The default is \c true.
    \value SH_SpinBox_ButtonsInsideFrame Whether the buttons of a spin box are inside its frame. The default is \c true.
    \value SH_SpinBox_StepModifier The modifier key that makes a spin box step ten times as far: Qt::ControlModifier.
    \value SH_TabBar_AllowWheelScrolling Whether the wheel changes the current tab. The default is \c true.
    \value SH_Table_AlwaysDrawLeftTopGridLines Whether a table draws the grid lines on its left and top edges. The default is \c false.
    \value SH_SpinBox_SelectOnStep Whether a spin box selects its text when it steps. The default is \c true.
    \value SH_CustomBase The first value for hints of a style of the application's own.
*/

/*!
    \enum QStyle::StandardPixmap

    This enum names the standard pixmaps of a style; see standardPixmap().

    \value SP_TitleBarMenuButton The menu button of a title bar.
    \value SP_TitleBarMinButton The minimize button of a title bar.
    \value SP_TitleBarMaxButton The maximize button of a title bar.
    \value SP_TitleBarCloseButton The close button of a title bar.
    \value SP_TitleBarNormalButton The restore button of a title bar.
    \value SP_TitleBarShadeButton The shade button of a title bar.
    \value SP_TitleBarUnshadeButton The unshade button of a title bar.
    \value SP_TitleBarContextHelpButton The help button of a title bar.
    \value SP_DockWidgetCloseButton The close button of a dock widget.
    \value SP_MessageBoxInformation The icon of an information message.
    \value SP_MessageBoxWarning The icon of a warning.
    \value SP_MessageBoxCritical The icon of a critical error.
    \value SP_MessageBoxQuestion The icon of a question.
    \value SP_DesktopIcon The desktop.
    \value SP_TrashIcon The trash.
    \value SP_ComputerIcon The computer.
    \value SP_DriveFDIcon A floppy disk drive.
    \value SP_DriveHDIcon A hard disk drive.
    \value SP_DriveCDIcon A CD drive.
    \value SP_DriveDVDIcon A DVD drive.
    \value SP_DriveNetIcon A network drive.
    \value SP_DirOpenIcon An open folder.
    \value SP_DirClosedIcon A closed folder.
    \value SP_DirLinkIcon A link to a folder.
    \value SP_DirLinkOpenIcon A link to an open folder.
    \value SP_FileIcon A file.
    \value SP_FileLinkIcon A link to a file.
    \value SP_ToolBarHorizontalExtensionButton The extension button of a horizontal tool bar.
    \value SP_ToolBarVerticalExtensionButton The extension button of a vertical tool bar.
    \value SP_FileDialogStart The start of a file dialog's history.
    \value SP_FileDialogEnd The end of a file dialog's history.
    \value SP_FileDialogToParent The parent folder, in a file dialog.
    \value SP_FileDialogNewFolder A new folder, in a file dialog.
    \value SP_FileDialogDetailedView The detailed view of a file dialog.
    \value SP_FileDialogInfoView The information view of a file dialog.
    \value SP_FileDialogContentsView The contents view of a file dialog.
    \value SP_FileDialogListView The list view of a file dialog.
    \value SP_FileDialogBack Back, in a file dialog.
    \value SP_DirIcon A folder.
    \value SP_DialogOkButton The OK button of a dialog.
    \value SP_DialogCancelButton The Cancel button of a dialog.
    \value SP_DialogHelpButton The Help button of a dialog.
    \value SP_DialogOpenButton The Open button of a dialog.
    \value SP_DialogSaveButton The Save button of a dialog.
    \value SP_DialogCloseButton The Close button of a dialog.
    \value SP_DialogApplyButton The Apply button of a dialog.
    \value SP_DialogResetButton The Reset button of a dialog.
    \value SP_DialogDiscardButton The Discard button of a dialog.
    \value SP_DialogYesButton The Yes button of a dialog.
    \value SP_DialogNoButton The No button of a dialog.
    \value SP_ArrowUp An arrow up.
    \value SP_ArrowDown An arrow down.
    \value SP_ArrowLeft An arrow to the left.
    \value SP_ArrowRight An arrow to the right.
    \value SP_ArrowBack An arrow back.
    \value SP_ArrowForward An arrow forward.
    \value SP_DirHomeIcon The home folder.
    \value SP_CommandLink The arrow of a command link button.
    \value SP_VistaShield The shield of an action that needs more rights.
    \value SP_BrowserReload Reload.
    \value SP_BrowserStop Stop loading.
    \value SP_MediaPlay Play.
    \value SP_MediaStop Stop.
    \value SP_MediaPause Pause.
    \value SP_MediaSkipForward Skip forward.
    \value SP_MediaSkipBackward Skip back.
    \value SP_MediaSeekForward Seek forward.
    \value SP_MediaSeekBackward Seek back.
    \value SP_MediaVolume The volume.
    \value SP_MediaVolumeMuted The volume, muted.
    \value SP_LineEditClearButton The clear button of a line edit.
    \value SP_DialogYesToAllButton The Yes to All button of a dialog.
    \value SP_DialogNoToAllButton The No to All button of a dialog.
    \value SP_DialogSaveAllButton The Save All button of a dialog.
    \value SP_DialogAbortButton The Abort button of a dialog.
    \value SP_DialogRetryButton The Retry button of a dialog.
    \value SP_DialogIgnoreButton The Ignore button of a dialog.
    \value SP_RestoreDefaultsButton The Restore Defaults button of a dialog.
    \value SP_TabCloseButton The close button of a tab.
    \value SP_CustomBase The first value for pixmaps of a style of the application's own.
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
    \value PM_ButtonDefaultIndicator The width of the frame of a default button.
    \value PM_MenuButtonIndicator The width of the menu indicator of a button with a menu.
    \value PM_ButtonShiftHorizontal How far the label of a pressed button moves to the right.
    \value PM_ButtonShiftVertical How far the label of a pressed button moves down.
    \value PM_SpinBoxFrameWidth The width of the frame of a spin box.
    \value PM_ComboBoxFrameWidth The width of the frame of a combo box.
    \value PM_MaximumDragDistance How far the mouse may leave a scroll bar while it drags the handle.
    \value PM_SliderControlThickness The thickness of the handle of a slider.
    \value PM_SliderTickmarkOffset The distance between the tick marks and the groove of a slider.
    \value PM_SliderSpaceAvailable The room that the handle of a slider moves in.
    \value PM_DockWidgetSeparatorExtent The width of the separator between dock widgets.
    \value PM_DockWidgetHandleExtent The width of the handle of a dock widget.
    \value PM_DockWidgetFrameWidth The width of the frame of a dock widget.
    \value PM_TabBarTabOverlap How far tabs overlap.
    \value PM_TabBarTabHSpace The room that a tab adds to the width of its label.
    \value PM_TabBarTabVSpace The room that a tab adds to the height of its label.
    \value PM_TabBarBaseHeight The height of the line under a row of tabs.
    \value PM_TabBarBaseOverlap How far the line under the tabs overlaps them.
    \value PM_ProgressBarChunkWidth The width of a chunk of a progress bar.
    \value PM_TitleBarHeight The height of the title bar of a sub-window or a dock widget.
    \value PM_MenuScrollerHeight The height of the scroller of a menu.
    \value PM_MenuHMargin The left and right margins of a menu.
    \value PM_MenuVMargin The top and bottom margins of a menu.
    \value PM_MenuPanelWidth The width of the frame of a menu.
    \value PM_MenuTearoffHeight The height of the tear-off entry of a menu.
    \value PM_MenuDesktopFrameWidth The room a menu keeps from the edges of the screen.
    \value PM_MenuBarPanelWidth The width of the frame of a menu bar.
    \value PM_MenuBarVMargin The top and bottom margins of a menu bar.
    \value PM_MenuBarHMargin The left and right margins of a menu bar.
    \value PM_DialogButtonsSeparator The space between the buttons of a dialog.
    \value PM_DialogButtonsButtonWidth The smallest width of a button of a dialog.
    \value PM_DialogButtonsButtonHeight The smallest height of a button of a dialog.
    \value PM_MdiSubWindowFrameWidth The width of the frame of an MDI sub-window.
    \value PM_MdiSubWindowMinimizedWidth The width of a minimized MDI sub-window.
    \value PM_HeaderMarkSize The size of the sort arrow of a header.
    \value PM_HeaderGripMargin The size of the area at the edge of a header section that resizes it.
    \value PM_TabBarTabShiftHorizontal How far the label of the selected tab moves to the right.
    \value PM_TabBarTabShiftVertical How far the label of the selected tab moves down.
    \value PM_TabBarScrollButtonWidth The width of the buttons that scroll a tab bar.
    \value PM_ToolBarFrameWidth The width of the frame of a tool bar.
    \value PM_ToolBarHandleExtent The width of the handle of a tool bar.
    \value PM_ToolBarItemSpacing The space between the items of a tool bar.
    \value PM_ToolBarItemMargin The margin around the items of a tool bar.
    \value PM_ToolBarSeparatorExtent The width of a separator of a tool bar.
    \value PM_ToolBarExtensionExtent The width of the extension button of a tool bar.
    \value PM_SpinBoxSliderHeight The height of the slider of a spin box that has one.
    \value PM_ToolBarIconSize The size of the icons of a tool bar.
    \value PM_ListViewIconSize The size of the icons of a list view.
    \value PM_IconViewIconSize The size of the icons in the icon mode of a list view.
    \value PM_SmallIconSize The size of a small icon.
    \value PM_LargeIconSize The size of a large icon.
    \value PM_FocusFrameVMargin The top and bottom margins of a focus frame.
    \value PM_FocusFrameHMargin The left and right margins of a focus frame.
    \value PM_ToolTipLabelFrameWidth The width of the frame of a tool tip.
    \value PM_TabBarIconSize The size of the icons of tabs.
    \value PM_SizeGripSize The size of a size grip.
    \value PM_DockWidgetTitleMargin The margin of the title of a dock widget.
    \value PM_MessageBoxIconSize The size of the icon of a message box.
    \value PM_ButtonIconSize The size of the icon of a button.
    \value PM_DockWidgetTitleBarButtonMargin The margin of the buttons in the title bar of a dock widget.
    \value PM_RadioButtonLabelSpacing The space between the indicator and the label of a radio button.
    \value PM_TabBar_ScrollButtonOverlap How far the scroll buttons of a tab bar overlap.
    \value PM_TextCursorWidth The width of the text cursor.
    \value PM_TabCloseIndicatorWidth The width of the close button of a tab.
    \value PM_TabCloseIndicatorHeight The height of the close button of a tab.
    \value PM_ScrollView_ScrollBarSpacing The space between a scroll bar and the viewport.
    \value PM_ScrollView_ScrollBarOverlap How far a scroll bar overlaps the viewport.
    \value PM_SubMenuOverlap How far a submenu overlaps its parent menu.
    \value PM_TreeViewIndentation The indentation of a level of a tree view.
    \value PM_TitleBarButtonIconSize The size of the icons on the buttons of a title bar.
    \value PM_TitleBarButtonSize The size of the buttons of a title bar.
    \value PM_LineEditIconSize The size of an icon in a line edit.
    \value PM_LineEditIconMargin The margin around an icon in a line edit.
    \value PM_CustomBase The first value for metrics of a style of the application's own.
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
    \value CT_Splitter A splitter handle.
    \value CT_MenuBar A menu bar.
    \value CT_Menu A menu.
    \value CT_Slider A slider.
    \value CT_ScrollBar A scroll bar.
    \value CT_SizeGrip A size grip.
    \value CT_TabWidget A tab widget.
    \value CT_DialogButtons The buttons of a dialog.
    \value CT_HeaderSection A header section.
    \value CT_GroupBox A group box.
    \value CT_MdiControls The buttons of a maximized MDI sub-window in a menu bar.
    \value CT_ItemViewItem An item of an item view.
    \value CT_CustomBase The first value for contents of a style of the application's own.
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
    Returns the style that the style calls for its own elements and metrics: the outermost QProxyStyle that it is
    the base of, or the style itself. A style draws through proxy(), so that what a proxy style changes also changes how
    the base style draws.

    \sa QProxyStyle
*/
const QStyle *QStyle::proxy() const { return m_proxy ? m_proxy->proxy() : this; }

/*!
    Returns the rectangle that \a text takes when it is drawn with drawItemText() in \a rectangle, aligned as
    \a alignment says, with the font of \a metrics. \a enabled is not used.

    \sa drawItemText()
*/
QRect QStyle::itemTextRect(const QFontMetrics &metrics, const QRect &rectangle, int alignment, bool enabled,
                           const QString &text) const {
    static_cast<void>(enabled);
    return metrics.boundingRect(rectangle, alignment, text).intersected(rectangle);
}

/*!
    Returns the rectangle that \a pixmap takes when it is drawn with drawItemPixmap() in \a rectangle, aligned
    as \a alignment says. A pixmap of a high-resolution screen takes its size in device-independent pixels.

    \sa drawItemPixmap()
*/
QRect QStyle::itemPixmapRect(const QRect &rectangle, int alignment, const QPixmap &pixmap) const {
    const qreal ratio = pixmap.devicePixelRatio() > 0 ? pixmap.devicePixelRatio() : 1.0;
    const QSize size(static_cast<int>(pixmap.width() / ratio), static_cast<int>(pixmap.height() / ratio));
    Qt::Alignment a(static_cast<Qt::AlignmentFlag>(alignment));
    if (!(a & kHorizontal))
        a |= Qt::AlignHCenter;
    if (!(a & kVertical))
        a |= Qt::AlignVCenter;
    return alignedRect(Qt::LeftToRight, a, size, rectangle);
}

/*!
    Draws \a text with \a painter in \a rectangle, aligned as \a alignment says, in the color of the role
    \a textRole of \a palette, or in the pen of \a painter if \a textRole is QPalette::NoRole. If \a enabled
    is \c false, the color is taken from the disabled group of \a palette.

    \sa drawItemPixmap(), itemTextRect()
*/
void QStyle::drawItemText(QPainter *painter, const QRect &rectangle, int alignment, const QPalette &palette,
                          bool enabled, const QString &text, QPalette::ColorRole textRole) const {
    if (!painter || text.isEmpty())
        return;
    painter->save();
    if (textRole != QPalette::NoRole)
        painter->setPen(palette.color(enabled ? QPalette::Active : QPalette::Disabled, textRole));
    painter->drawText(rectangle, alignment, text);
    painter->restore();
}

/*!
    Draws \a pixmap with \a painter in \a rectangle, aligned as \a alignment says.

    \sa drawItemText(), itemPixmapRect()
*/
void QStyle::drawItemPixmap(QPainter *painter, const QRect &rectangle, int alignment, const QPixmap &pixmap) const {
    if (!painter || pixmap.isNull())
        return;
    painter->drawPixmap(itemPixmapRect(rectangle, alignment, pixmap), pixmap);
}

/*!
    Returns \a logicalRectangle, a rectangle in \a boundingRectangle, mirrored if \a direction is
    Qt::RightToLeft, so that what is on the left in a left-to-right layout is on the right.

    \sa visualPos(), visualAlignment()
*/
QRect QStyle::visualRect(Qt::LayoutDirection direction, const QRect &boundingRectangle, const QRect &logicalRectangle) {
    if (direction != Qt::RightToLeft)
        return logicalRectangle;
    QRect mirrored = logicalRectangle;
    mirrored.moveLeft(boundingRectangle.right() - logicalRectangle.right() + boundingRectangle.left());
    return mirrored;
}

/*!
    Returns \a logicalPosition, a point in \a boundingRectangle, mirrored if \a direction is Qt::RightToLeft.

    \sa visualRect()
*/
QPoint QStyle::visualPos(Qt::LayoutDirection direction, const QRect &boundingRectangle, const QPoint &logicalPosition) {
    if (direction != Qt::RightToLeft)
        return logicalPosition;
    return QPoint(boundingRectangle.right() - logicalPosition.x() + boundingRectangle.left(), logicalPosition.y());
}

/*!
    Returns \a alignment with Qt::AlignLeft and Qt::AlignRight swapped if \a direction is Qt::RightToLeft.

    \sa visualRect()
*/
Qt::Alignment QStyle::visualAlignment(Qt::LayoutDirection direction, Qt::Alignment alignment) {
    if (direction != Qt::RightToLeft || (alignment & Qt::AlignHCenter))
        return alignment;
    const Qt::Alignment rest = alignment & ~kHorizontal;
    if (alignment & Qt::AlignRight)
        return rest | Qt::AlignLeft;
    if (alignment & Qt::AlignLeft)
        return rest | Qt::AlignRight;
    return alignment;
}

/*!
    Returns a rectangle of \a size in \a rectangle, aligned as \a alignment says, for the layout direction
    \a direction.

    \code
    const QRect icon = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignRight | Qt::AlignVCenter, QSize(16, 16), rect());
    \endcode
*/
QRect QStyle::alignedRect(Qt::LayoutDirection direction, Qt::Alignment alignment, const QSize &size,
                          const QRect &rectangle) {
    const Qt::Alignment a = visualAlignment(direction, alignment);
    int x = rectangle.x(), y = rectangle.y();
    const int w = size.width(), h = size.height();
    if (a & Qt::AlignVCenter)
        y += rectangle.height() / 2 - h / 2;
    else if (a & Qt::AlignBottom)
        y += rectangle.height() - h;
    if (a & Qt::AlignRight)
        x += rectangle.width() - w;
    else if (a & Qt::AlignHCenter)
        x += rectangle.width() / 2 - w / 2;
    return QRect(x, y, w, h);
}

/*!
    Returns the position, from 0 to \a span, of the value \a logicalValue of the range from \a min to \a max:
    where the handle of a slider of \a span pixels is. With \a upsideDown the position counts from the other end.

    \sa sliderValueFromPosition()
*/
int QStyle::sliderPositionFromValue(int min, int max, int logicalValue, int span, bool upsideDown) {
    if (span <= 0 || logicalValue < min || max <= min)
        return 0;
    if (logicalValue > max)
        return upsideDown ? 0 : span;
    const auto range = static_cast<long long>(max) - min;
    const auto position = (static_cast<long long>(logicalValue) - min) * span / range;
    const int p = static_cast<int>(std::clamp<long long>(position, 0, span));
    return upsideDown ? span - p : p;
}

/*!
    Returns the value of the range from \a min to \a max at the position \a position, from 0 to \a span: the
    value of a slider of \a span pixels whose handle is there. With \a upsideDown the position counts from the
    other end. The value is rounded to the nearest.

    \sa sliderPositionFromValue()
*/
int QStyle::sliderValueFromPosition(int min, int max, int position, int span, bool upsideDown) {
    if (span <= 0 || position <= 0)
        return upsideDown ? max : min;
    if (position >= span)
        return upsideDown ? min : max;
    const auto range = static_cast<long long>(max) - min;
    long long value = (range * position + span / 2) / span;
    if (upsideDown)
        value = range - value;
    return static_cast<int>(min + value);
}

/*!
    \fn QStyle::QStyle()

    Constructs a style.
*/

namespace {

// The color of the grid of a table: a little darker than the base, or lighter if the base is dark.
int gridColor(const QStyleOption *option) {
    const QColor base = option ? option->palette.color(QPalette::Base) : QColor(255, 255, 255);
    const bool dark = base.red() + base.green() + base.blue() < 3 * 128;
    return static_cast<int>((dark ? base.lighter(170) : base.darker(125)).rgba());
}

// The sub-controls of each complex control, in the order a hit test tries them: the small parts first.
QList<QStyle::SubControl> subControlsOf(QStyle::ComplexControl control) {
    switch (control) {
    case QStyle::CC_ScrollBar:
        return {QStyle::SC_ScrollBarSlider,  QStyle::SC_ScrollBarAddLine, QStyle::SC_ScrollBarSubLine,
                QStyle::SC_ScrollBarFirst,   QStyle::SC_ScrollBarLast,    QStyle::SC_ScrollBarSubPage,
                QStyle::SC_ScrollBarAddPage, QStyle::SC_ScrollBarGroove};
    case QStyle::CC_SpinBox:
        return {QStyle::SC_SpinBoxUp, QStyle::SC_SpinBoxDown, QStyle::SC_SpinBoxEditField, QStyle::SC_SpinBoxFrame};
    case QStyle::CC_ComboBox:
        return {QStyle::SC_ComboBoxArrow, QStyle::SC_ComboBoxEditField, QStyle::SC_ComboBoxFrame};
    case QStyle::CC_Slider:
        return {QStyle::SC_SliderHandle, QStyle::SC_SliderGroove, QStyle::SC_SliderTickmarks};
    case QStyle::CC_ToolButton:
        return {QStyle::SC_ToolButtonMenu, QStyle::SC_ToolButton};
    case QStyle::CC_TitleBar:
        return {QStyle::SC_TitleBarCloseButton, QStyle::SC_TitleBarMaxButton, QStyle::SC_TitleBarNormalButton,
                QStyle::SC_TitleBarMinButton,   QStyle::SC_TitleBarSysMenu,   QStyle::SC_TitleBarLabel};
    case QStyle::CC_Dial:
        return {QStyle::SC_DialHandle, QStyle::SC_DialGroove};
    case QStyle::CC_GroupBox:
        return {QStyle::SC_GroupBoxCheckBox, QStyle::SC_GroupBoxLabel, QStyle::SC_GroupBoxContents,
                QStyle::SC_GroupBoxFrame};
    case QStyle::CC_MdiControls:
        return {QStyle::SC_MdiCloseButton, QStyle::SC_MdiNormalButton, QStyle::SC_MdiMinButton};
    default:
        return {};
    }
}

} // namespace

/*!
    Draws the complex control \a control, described by \a option, with \a painter; \a widget may be \c nullptr.
    The default draws the slider, scroll bar, combo box, spin box, group box and dial with the control element of
    Nt's style that draws them (\c CE_Slider and so on), and draws simple tool buttons and title bars itself.

    \sa subControlRect(), hitTestComplexControl()
*/
void QStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                                const QWidget *widget) const {
    if (!option || !painter)
        return;
    switch (control) {
    case CC_Slider:
        proxy()->drawControl(CE_Slider, option, painter, widget);
        return;
    case CC_ScrollBar:
        proxy()->drawControl(CE_ScrollBar, option, painter, widget);
        return;
    case CC_ComboBox:
        proxy()->drawControl(CE_ComboBox, option, painter, widget);
        return;
    case CC_SpinBox:
        proxy()->drawControl(CE_SpinBox, option, painter, widget);
        return;
    case CC_GroupBox:
        proxy()->drawControl(CE_GroupBox, option, painter, widget);
        return;
    case CC_Dial:
        proxy()->drawControl(CE_Dial, option, painter, widget);
        return;
    case CC_ToolButton:
        if (const auto *tool = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            const bool raised = tool->state & (State_Sunken | State_On | State_MouseOver | State_Raised);
            if (raised) {
                QStyleOption bevel(*option);
                bevel.rect = proxy()->subControlRect(control, option, SC_ToolButton, widget);
                proxy()->drawPrimitive(PE_PanelButtonCommand, &bevel, painter, widget);
            }
            const QRect label = proxy()->subControlRect(control, option, SC_ToolButton, widget);
            if (tool->features & QStyleOptionToolButton::Arrow) {
                QStyleOption arrow(*option);
                const int size = std::min(label.width(), label.height()) / 2;
                arrow.rect = alignedRect(tool->direction, Qt::AlignCenter, QSize(size, size), label);
                arrow.palette.setColor(QPalette::ButtonText, option->palette.color(QPalette::ButtonText));
                const PrimitiveElement pe = tool->arrowType == Qt::UpArrow      ? PE_IndicatorArrowUp
                                            : tool->arrowType == Qt::LeftArrow  ? PE_IndicatorArrowLeft
                                            : tool->arrowType == Qt::RightArrow ? PE_IndicatorArrowRight
                                                                                : PE_IndicatorArrowDown;
                proxy()->drawPrimitive(pe, &arrow, painter, widget);
            } else if (!tool->text.isEmpty()) {
                painter->save();
                painter->setFont(tool->font);
                proxy()->drawItemText(painter, label, Qt::AlignCenter, tool->palette, tool->state & State_Enabled,
                                      tool->text, QPalette::ButtonText);
                painter->restore();
            }
            if (tool->features & QStyleOptionToolButton::MenuButtonPopup) {
                QStyleOption arrow(*option);
                const QRect menu = proxy()->subControlRect(control, option, SC_ToolButtonMenu, widget);
                arrow.rect = alignedRect(tool->direction, Qt::AlignCenter, QSize(7, 7), menu);
                proxy()->drawPrimitive(PE_IndicatorArrowDown, &arrow, painter, widget);
            }
        }
        return;
    case CC_TitleBar:
        if (const auto *bar = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
            const bool active = bar->state & State_Active;
            painter->fillRect(bar->rect, bar->palette.color(active ? QPalette::Highlight : QPalette::Window));
            const QRect label = proxy()->subControlRect(control, option, SC_TitleBarLabel, widget);
            proxy()->drawItemText(painter, label, Qt::AlignLeft | Qt::AlignVCenter, bar->palette, true,
                                  bar->fontMetrics.elidedText(bar->text, Qt::ElideRight, label.width()),
                                  active ? QPalette::HighlightedText : QPalette::WindowText);
            const QRect close = proxy()->subControlRect(control, option, SC_TitleBarCloseButton, widget);
            if (!close.isEmpty()) {
                painter->save();
                painter->setPen(
                    QPen(bar->palette.color(active ? QPalette::HighlightedText : QPalette::WindowText), 1.5));
                const QRect x = close.adjusted(4, 4, -4, -4);
                painter->drawLine(x.topLeft(), x.bottomRight());
                painter->drawLine(x.topRight(), x.bottomLeft());
                painter->restore();
            }
        }
        return;
    default:
        return;
    }
}

/*!
    Returns the sub-control of \a control, described by \a option, at \a position, or \c SC_None if there is none
    there. \a widget may be \c nullptr. The default asks subControlRect() for the parts of the control, the small
    ones first.

    \sa subControlRect()
*/
QStyle::SubControl QStyle::hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                                 const QPoint &position, const QWidget *widget) const {
    if (!option)
        return SC_None;
    for (const SubControl part : subControlsOf(control)) {
        if (!(option->subControls & part))
            continue;
        if (proxy()->subControlRect(control, option, part, widget).contains(position))
            return part;
    }
    return SC_None;
}

/*!
    Returns the rectangle of the part \a subControl of the complex control \a control, described by \a option, in
    the coordinates of the widget; \a widget may be \c nullptr. The default finds the parts of the slider, scroll
    bar, combo box, spin box and group box with subElementRect(), and lays out tool buttons and title bars
    itself. It returns an empty rectangle for a part that the control does not have.

    \sa hitTestComplexControl(), drawComplexControl()
*/
QRect QStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
                             const QWidget *widget) const {
    if (!option)
        return QRect();
    const QRect r = option->rect;
    switch (control) {
    case CC_Slider:
        switch (subControl) {
        case SC_SliderGroove:
            return proxy()->subElementRect(SE_SliderGroove, option, widget);
        case SC_SliderHandle:
            return proxy()->subElementRect(SE_SliderHandle, option, widget);
        case SC_SliderTickmarks:
            return r;
        default:
            return QRect();
        }
    case CC_ScrollBar: {
        const QRect slider = proxy()->subElementRect(SE_ScrollBarSlider, option, widget);
        const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option);
        const bool horizontal = !bar || bar->orientation == Qt::Horizontal;
        switch (subControl) {
        case SC_ScrollBarSlider:
            return slider;
        case SC_ScrollBarGroove:
            return r;
        case SC_ScrollBarSubPage:
            return horizontal ? QRect(r.left(), r.top(), slider.left() - r.left(), r.height())
                              : QRect(r.left(), r.top(), r.width(), slider.top() - r.top());
        case SC_ScrollBarAddPage:
            return horizontal ? QRect(slider.right() + 1, r.top(), r.right() - slider.right(), r.height())
                              : QRect(r.left(), slider.bottom() + 1, r.width(), r.bottom() - slider.bottom());
        default:
            return QRect(); // Nt's scroll bars have no buttons
        }
    }
    case CC_ComboBox:
        switch (subControl) {
        case SC_ComboBoxArrow:
            return proxy()->subElementRect(SE_ComboBoxArrow, option, widget);
        case SC_ComboBoxEditField: {
            const QRect arrow = proxy()->subElementRect(SE_ComboBoxArrow, option, widget);
            const int frame = proxy()->pixelMetric(PM_DefaultFrameWidth, option, widget);
            return QRect(r.left() + frame + 2, r.top() + frame, std::max(0, arrow.left() - r.left() - frame - 2),
                         std::max(0, r.height() - 2 * frame));
        }
        case SC_ComboBoxFrame:
        case SC_ComboBoxListBoxPopup:
            return r;
        default:
            return QRect();
        }
    case CC_SpinBox:
        switch (subControl) {
        case SC_SpinBoxUp:
            return proxy()->subElementRect(SE_SpinBoxUp, option, widget);
        case SC_SpinBoxDown:
            return proxy()->subElementRect(SE_SpinBoxDown, option, widget);
        case SC_SpinBoxEditField:
            return proxy()->subElementRect(SE_SpinBoxEditField, option, widget);
        case SC_SpinBoxFrame:
            return r;
        default:
            return QRect();
        }
    case CC_GroupBox:
        switch (subControl) {
        case SC_GroupBoxCheckBox: {
            const auto *box = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
            return box && box->checkable ? proxy()->subElementRect(SE_GroupBoxIndicator, option, widget) : QRect();
        }
        case SC_GroupBoxLabel:
            return proxy()->subElementRect(SE_GroupBoxTitle, option, widget);
        case SC_GroupBoxContents:
            return proxy()->subElementRect(SE_GroupBoxContents, option, widget);
        case SC_GroupBoxFrame:
            return r;
        default:
            return QRect();
        }
    case CC_Dial:
        return subControl == SC_DialGroove || subControl == SC_DialHandle || subControl == SC_DialTickmarks ? r
                                                                                                            : QRect();
    case CC_ToolButton: {
        const auto *tool = qstyleoption_cast<const QStyleOptionToolButton *>(option);
        const int menu = tool && (tool->features & QStyleOptionToolButton::MenuButtonPopup)
                             ? proxy()->pixelMetric(PM_MenuButtonIndicator, option, widget)
                             : 0;
        if (subControl == SC_ToolButton)
            return QRect(r.left(), r.top(), r.width() - menu, r.height());
        if (subControl == SC_ToolButtonMenu)
            return menu > 0 ? QRect(r.right() + 1 - menu, r.top(), menu, r.height()) : QRect();
        return QRect();
    }
    case CC_TitleBar: {
        // Square buttons on the right, close at the far end; the text takes the rest.
        const auto *bar = qstyleoption_cast<const QStyleOptionTitleBar *>(option);
        const Qt::WindowFlags flags = bar ? bar->titleBarFlags : Qt::WindowFlags();
        const int size = std::max(0, r.height() - 4);
        int right = r.right() - 1;
        const auto next = [&right, size, r]() {
            const QRect button(right + 1 - size, r.top() + 2, size, size);
            right -= size + 2;
            return button;
        };
        const QRect close = (flags & Qt::WindowCloseButtonHint) || !bar ? next() : QRect();
        const QRect max = flags & Qt::WindowMaximizeButtonHint ? next() : QRect();
        const QRect min = flags & Qt::WindowMinimizeButtonHint ? next() : QRect();
        switch (subControl) {
        case SC_TitleBarCloseButton:
            return close;
        case SC_TitleBarMaxButton:
        case SC_TitleBarNormalButton:
            return max;
        case SC_TitleBarMinButton:
            return min;
        case SC_TitleBarLabel:
            return QRect(r.left() + 6, r.top(), std::max(0, right - r.left() - 6), r.height());
        default:
            return QRect();
        }
    }
    default:
        return QRect();
    }
}

/*!
    Returns the answer of the style to the question \a hint about how widgets behave; see StyleHint for the
    answers of the default style. \a option, \a widget and \a returnData may be \c nullptr; a hint that answers
    with more than a number fills \a returnData. A style reimplements this to change what its widgets do.
*/
int QStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                      QStyleHintReturn *returnData) const {
    static_cast<void>(widget);
    static_cast<void>(returnData);
    switch (hint) {
    case SH_EtchDisabledText:
    case SH_DitherDisabledText:
    case SH_Slider_SloppyKeyEvents:
    case SH_ProgressDialog_CenterCancelButton:
    case SH_MainWindow_SpaceBelowMenuBar:
    case SH_Menu_AllowActiveAndDisabled:
    case SH_ScrollView_FrameOnlyAroundContents:
    case SH_ItemView_ChangeHighlightOnFocus:
    case SH_ComboBox_Popup:
    case SH_TitleBar_NoBorder:
    case SH_Slider_StopMouseOverSlider:
    case SH_Menu_Scrollable:
    case SH_DialogButtons_DefaultButton:
    case SH_TabBar_PreferNoArrows:
    case SH_ScrollBar_LeftClickAbsolutePosition:
    case SH_SpinBox_AnimateButton:
    case SH_Menu_FillScreenWithScroll:
    case SH_DrawMenuBarSeparator:
    case SH_MessageBox_UseBorderForButtonSpacing:
    case SH_TitleBar_AutoRaise:
    case SH_FocusFrame_Mask:
    case SH_RubberBand_Mask:
    case SH_WindowFrame_Mask:
    case SH_ItemView_ShowDecorationSelected:
    case SH_ItemView_ActivateItemOnSingleClick:
    case SH_ScrollBar_RollBetweenButtons:
    case SH_Menu_KeyboardSearch:
    case SH_DialogButtonBox_ButtonsHaveIcons:
    case SH_MessageBox_CenterButtons:
    case SH_ToolTip_Mask:
    case SH_FocusFrame_AboveWidget:
    case SH_TextControl_FocusIndicatorTextCharFormat:
    case SH_WizardStyle:
    case SH_ItemView_ArrowKeysNavigateIntoChildren:
    case SH_Menu_Mask:
    case SH_Menu_FlashTriggeredItem:
    case SH_Menu_FadeOutOnHide:
    case SH_ItemView_PaintAlternatingRowColorsForEmptyArea:
    case SH_FormLayoutWrapPolicy:
    case SH_TabWidget_DefaultTabPosition:
    case SH_ItemView_DrawDelegateFrame:
    case SH_RequestSoftwareInputPanel:
    case SH_ScrollBar_Transient:
    case SH_Menu_SupportsSections:
    case SH_Widget_Animate:
    case SH_ComboBox_UseNativePopup:
    case SH_LineEdit_PasswordMaskDelay:
    case SH_Menu_SubMenuUniDirection:
    case SH_Menu_SubMenuResetWhenReenteringParent:
    case SH_Menu_SubMenuDontStartSloppyOnLeave:
    case SH_Widget_Animation_Duration:
    case SH_Table_AlwaysDrawLeftTopGridLines:
        return static_cast<int>(0);
    case SH_ScrollBar_MiddleClickAbsolutePosition:
    case SH_ScrollBar_ScrollWhenPointerLeavesControl:
    case SH_Slider_SnapToValue:
    case SH_PrintDialog_RightAlignButtons:
    case SH_FontDialog_SelectAssociatedText:
    case SH_Menu_SpaceActivatesItem:
    case SH_MenuBar_AltKeyNavigation:
    case SH_ComboBox_ListMouseTracking:
    case SH_Menu_MouseTracking:
    case SH_MenuBar_MouseTracking:
    case SH_Widget_ShareActivation:
    case SH_Workspace_FillSpaceOnMaximize:
    case SH_BlinkCursorWhenTextSelected:
    case SH_RichText_FullWidthSelection:
    case SH_Menu_SloppySubMenus:
    case SH_ToolBox_SelectedPageTitleBold:
    case SH_UnderlineShortcut:
    case SH_TitleBar_ModifyNotification:
    case SH_SpinControls_DisableOnBounds:
    case SH_ScrollBar_ContextMenu:
    case SH_Menu_SelectionWrap:
    case SH_ItemView_MovementWithoutUpdatingSelection:
    case SH_ToolBar_Movable:
    case SH_TabBar_CloseButtonPosition:
    case SH_DockWidget_ButtonsHaveFrame:
    case SH_Splitter_OpaqueResize:
    case SH_Menu_SubMenuUniDirectionFailCount:
    case SH_Menu_SubMenuSloppySelectOtherActions:
    case SH_ItemView_ScrollMode:
    case SH_TitleBar_ShowToolTipsOnButtons:
    case SH_ComboBox_AllowWheelScrolling:
    case SH_SpinBox_ButtonsInsideFrame:
    case SH_TabBar_AllowWheelScrolling:
    case SH_SpinBox_SelectOnStep:
        return static_cast<int>(1);
    case SH_TabBar_SelectMouseType:
    case SH_ListViewExpand_SelectMouseType:
        return static_cast<int>(QEvent::MouseButtonPress);
    case SH_TabBar_Alignment:
        return static_cast<int>(Qt::AlignLeft);
    case SH_Header_ArrowAlignment:
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    case SH_ProgressDialog_TextLabelAlignment:
        return static_cast<int>(Qt::AlignCenter);
    case SH_Menu_SubMenuPopupDelay:
        return static_cast<int>(225);
    case SH_GroupBox_TextLabelVerticalAlignment:
        return static_cast<int>(Qt::AlignVCenter);
    case SH_GroupBox_TextLabelColor:
        return static_cast<int>(option ? option->palette.color(QPalette::WindowText).rgba() : 0xff000000u);
    case SH_Table_GridLineColor:
        return gridColor(option);
    case SH_LineEdit_PasswordCharacter:
        return static_cast<int>(0x25CF);
    case SH_SpinBox_KeyPressAutoRepeatRate:
        return static_cast<int>(75);
    case SH_SpinBox_ClickAutoRepeatRate:
        return static_cast<int>(150);
    case SH_ToolTipLabel_Opacity:
        return static_cast<int>(242);
    case SH_Button_FocusPolicy:
        return static_cast<int>(Qt::StrongFocus);
    case SH_ToolButton_PopupDelay:
        return static_cast<int>(600);
    case SH_Dial_BackgroundRole:
        return static_cast<int>(QPalette::Window);
    case SH_ComboBox_LayoutDirection:
        return option ? option->direction : Qt::LeftToRight;
    case SH_ItemView_EllipsisLocation:
        return static_cast<int>(Qt::AlignRight);
    case SH_Slider_AbsoluteSetButtons:
        return static_cast<int>(Qt::MiddleButton);
    case SH_Slider_PageSetButtons:
        return static_cast<int>(Qt::LeftButton);
    case SH_TabBar_ElideMode:
        return static_cast<int>(Qt::ElideNone);
    case SH_DialogButtonLayout:
    case SH_FormLayoutFieldGrowthPolicy:
        return static_cast<int>(2);
    case SH_ComboBox_PopupFrameStyle:
        return static_cast<int>(0x0016);
    case SH_MessageBox_TextInteractionFlags:
        return static_cast<int>(0x0004);
    case SH_SpinBox_ClickAutoRepeatThreshold:
    case SH_TabBar_ChangeCurrentDelay:
        return static_cast<int>(500);
    case SH_FormLayoutFormAlignment:
        return static_cast<int>(Qt::AlignLeft | Qt::AlignTop);
    case SH_FormLayoutLabelAlignment:
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    case SH_ToolButtonStyle:
        return static_cast<int>(Qt::ToolButtonIconOnly);
    case SH_ToolTip_WakeUpDelay:
        return static_cast<int>(700);
    case SH_ToolTip_FallAsleepDelay:
        return static_cast<int>(2000);
    case SH_Menu_SubMenuSloppyCloseTimeout:
        return static_cast<int>(1000);
    case SH_SpinBox_StepModifier:
        return static_cast<int>(Qt::ControlModifier);
    case SH_CustomBase:
        break;
    }
    return 0;
}

namespace {

QPixmap drawnPixmap(int size, const std::function<void(QPainter &, const QRectF &)> &draw) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        draw(painter, QRectF(0, 0, size, size));
    }
    return QPixmap::fromImage(image);
}

// The round icon of a message box: a disc of `color` with `letter` on it.
QPixmap messageIcon(int size, const QColor &color, const QString &letter) {
    return drawnPixmap(size, [&](QPainter &p, const QRectF &r) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(r.adjusted(1, 1, -1, -1));
        QFont font;
        font.setPixelSize(static_cast<int>(size * 0.6));
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, letter);
    });
}

QPixmap arrowIcon(int size, const QColor &color, int degrees) {
    return drawnPixmap(size, [&](QPainter &p, const QRectF &r) {
        p.translate(r.center());
        p.rotate(degrees);
        const qreal s = size * 0.3;
        QPainterPath path;
        path.moveTo(0, -s);
        path.lineTo(s, s * 0.6);
        path.lineTo(-s, s * 0.6);
        path.closeSubpath();
        p.fillPath(path, color);
    });
}

} // namespace

namespace {

// The icon theme names of the standard pixmaps, from the freedesktop.org icon naming specification.
const char *themeNameOf(QStyle::StandardPixmap sp) {
    switch (sp) {
    case QStyle::SP_TitleBarCloseButton:
    case QStyle::SP_DockWidgetCloseButton:
    case QStyle::SP_DialogCloseButton:
        return "window-close";
    case QStyle::SP_TitleBarMinButton:
        return "window-minimize";
    case QStyle::SP_TitleBarMaxButton:
        return "window-maximize";
    case QStyle::SP_TitleBarNormalButton:
        return "window-restore";
    case QStyle::SP_MessageBoxInformation:
        return "dialog-information";
    case QStyle::SP_MessageBoxWarning:
        return "dialog-warning";
    case QStyle::SP_MessageBoxCritical:
        return "dialog-error";
    case QStyle::SP_MessageBoxQuestion:
        return "dialog-question";
    case QStyle::SP_DesktopIcon:
        return "user-desktop";
    case QStyle::SP_TrashIcon:
        return "user-trash";
    case QStyle::SP_ComputerIcon:
        return "computer";
    case QStyle::SP_DriveFDIcon:
        return "media-floppy";
    case QStyle::SP_DriveHDIcon:
        return "drive-harddisk";
    case QStyle::SP_DriveCDIcon:
    case QStyle::SP_DriveDVDIcon:
        return "drive-optical";
    case QStyle::SP_DriveNetIcon:
        return "network-server";
    case QStyle::SP_DirOpenIcon:
    case QStyle::SP_DirLinkOpenIcon:
        return "folder-open";
    case QStyle::SP_DirClosedIcon:
    case QStyle::SP_DirIcon:
    case QStyle::SP_DirLinkIcon:
        return "folder";
    case QStyle::SP_FileIcon:
    case QStyle::SP_FileLinkIcon:
        return "text-x-generic";
    case QStyle::SP_FileDialogNewFolder:
        return "folder-new";
    case QStyle::SP_FileDialogToParent:
    case QStyle::SP_ArrowUp:
        return "go-up";
    case QStyle::SP_ArrowDown:
        return "go-down";
    case QStyle::SP_ArrowLeft:
        return "go-previous";
    case QStyle::SP_ArrowRight:
        return "go-next";
    case QStyle::SP_ArrowBack:
    case QStyle::SP_FileDialogBack:
        return "go-previous";
    case QStyle::SP_ArrowForward:
        return "go-next";
    case QStyle::SP_DirHomeIcon:
        return "go-home";
    case QStyle::SP_DialogOkButton:
    case QStyle::SP_DialogApplyButton:
    case QStyle::SP_DialogYesButton:
        return "dialog-ok";
    case QStyle::SP_DialogCancelButton:
    case QStyle::SP_DialogNoButton:
        return "dialog-cancel";
    case QStyle::SP_DialogHelpButton:
        return "help-contents";
    case QStyle::SP_DialogOpenButton:
        return "document-open";
    case QStyle::SP_DialogSaveButton:
        return "document-save";
    case QStyle::SP_DialogResetButton:
        return "edit-clear";
    case QStyle::SP_DialogDiscardButton:
        return "edit-delete";
    case QStyle::SP_BrowserReload:
        return "view-refresh";
    case QStyle::SP_BrowserStop:
        return "process-stop";
    case QStyle::SP_MediaPlay:
        return "media-playback-start";
    case QStyle::SP_MediaStop:
        return "media-playback-stop";
    case QStyle::SP_MediaPause:
        return "media-playback-pause";
    case QStyle::SP_MediaSkipForward:
        return "media-skip-forward";
    case QStyle::SP_MediaSkipBackward:
        return "media-skip-backward";
    case QStyle::SP_MediaSeekForward:
        return "media-seek-forward";
    case QStyle::SP_MediaSeekBackward:
        return "media-seek-backward";
    case QStyle::SP_MediaVolume:
        return "audio-volume-high";
    case QStyle::SP_MediaVolumeMuted:
        return "audio-volume-muted";
    default:
        return nullptr;
    }
}

} // namespace

/*!
    \since 6.0

    Returns the standard icon \a standardIcon: the icon of the same meaning in the icon theme, as QIcon::fromTheme()
    finds it, or else the standardPixmap() of the style; \a option and \a widget may be \c nullptr. Returns a null
    icon if neither has one.
*/
QIcon QStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const {
    if (const char *name = themeNameOf(standardIcon)) {
        const QIcon themed = QIcon::fromTheme(QString::fromLatin1(name));
        if (!themed.isNull())
            return themed;
    }
    const QPixmap pixmap = proxy()->standardPixmap(standardIcon, option, widget);
    return pixmap.isNull() ? QIcon() : QIcon(pixmap);
}

/*!
    Returns a copy of \a pixmap for the icon mode \a iconMode: grayed out and lighter for QIcon::Disabled, tinted
    with the highlight color of the palette of \a option for QIcon::Selected, and unchanged for the other modes.
*/
QPixmap QStyle::generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *option) const {
    if (pixmap.isNull() || iconMode == QIcon::Normal || iconMode == QIcon::Active)
        return pixmap;
    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    const QPalette palette = option ? option->palette : standardPalette();
    const QColor tint = palette.color(QPalette::Highlight);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QColor c = image.pixelColor(x, y);
            if (iconMode == QIcon::Disabled) {
                const int gray = qGray(c.red(), c.green(), c.blue());
                const int lighter = 128 + gray / 2;
                c.setRgb(lighter, lighter, lighter, c.alpha() * 6 / 10);
            } else {
                c.setRgb((c.red() + tint.red()) / 2, (c.green() + tint.green()) / 2, (c.blue() + tint.blue()) / 2,
                         c.alpha());
            }
            image.setPixelColor(x, y, c);
        }
    }
    return QPixmap::fromImage(image);
}

/*!
    Returns the space in pixels between a widget of the type \a control1 and one of the type \a control2 in a layout
    of the given \a orientation: the layout spacing of the style for that orientation, or 6 if it has none. \a option
    and \a widget are passed to pixelMetric().

    \sa combinedLayoutSpacing()
*/
int QStyle::layoutSpacing(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2,
                          Qt::Orientation orientation, const QStyleOption *option, const QWidget *widget) const {
    (void)control1;
    (void)control2;
    const int spacing = pixelMetric(
        orientation == Qt::Horizontal ? PM_LayoutHorizontalSpacing : PM_LayoutVerticalSpacing, option, widget);
    return spacing >= 0 ? spacing : 6;
}

/*!
    Returns the space in pixels between the widgets of the types \a controls1 and those of the types \a controls2:
    the largest layoutSpacing() of any pair, or 0 if a set is empty. A set with no bit set counts as the default type.
    \a orientation, \a option and \a widget are passed to layoutSpacing().

    \sa layoutSpacing()
*/
int QStyle::combinedLayoutSpacing(QSizePolicy::ControlTypes controls1, QSizePolicy::ControlTypes controls2,
                                  Qt::Orientation orientation, QStyleOption *option, const QWidget *widget) const {
    int spacing = 0;
    bool any = false;
    for (int a = 1; a <= 0x4000; a <<= 1) {
        if (!(controls1.toInt() & a) && !(a == 1 && controls1.toInt() == 0))
            continue;
        for (int b = 1; b <= 0x4000; b <<= 1) {
            if (!(controls2.toInt() & b) && !(b == 1 && controls2.toInt() == 0))
                continue;
            spacing =
                std::max(spacing, layoutSpacing(static_cast<QSizePolicy::ControlType>(a),
                                                static_cast<QSizePolicy::ControlType>(b), orientation, option, widget));
            any = true;
        }
    }
    return any ? spacing : 0;
}

/*!
    Returns the standard pixmap \a standardPixmap of the style, in the size that suits it; \a option and \a widget
    may be \c nullptr. The default draws the message box icons, the arrows, the close, minimize and maximize
    buttons, the media symbols and plain folder, file and drive icons in the colors of the palette of \a option,
    and returns a null pixmap for the others.
*/
QPixmap QStyle::standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget) const {
    const QPalette palette = option ? option->palette : QPalette();
    const QColor ink = palette.color(QPalette::WindowText);
    const int small = proxy()->pixelMetric(PM_SmallIconSize, option, widget);
    const int large = proxy()->pixelMetric(PM_MessageBoxIconSize, option, widget);
    const auto cross = [&](int size) {
        return drawnPixmap(size, [&](QPainter &p, const QRectF &r) {
            p.setPen(QPen(ink, 1.6));
            const QRectF x = r.adjusted(size * 0.25, size * 0.25, -size * 0.25, -size * 0.25);
            p.drawLine(x.topLeft(), x.bottomRight());
            p.drawLine(x.topRight(), x.bottomLeft());
        });
    };
    switch (standardPixmap) {
    case SP_MessageBoxInformation:
        return messageIcon(large, QColor(41, 128, 185), QStringLiteral("i"));
    case SP_MessageBoxWarning:
        return messageIcon(large, QColor(230, 160, 20), QStringLiteral("!"));
    case SP_MessageBoxCritical:
        return messageIcon(large, QColor(200, 40, 40), QStringLiteral("×"));
    case SP_MessageBoxQuestion:
        return messageIcon(large, QColor(41, 128, 185), QStringLiteral("?"));
    case SP_ArrowUp:
        return arrowIcon(small, ink, 0);
    case SP_ArrowRight:
    case SP_ArrowForward:
    case SP_MediaPlay:
        return arrowIcon(small, ink, 90);
    case SP_ArrowDown:
        return arrowIcon(small, ink, 180);
    case SP_ArrowLeft:
    case SP_ArrowBack:
        return arrowIcon(small, ink, 270);
    case SP_TitleBarCloseButton:
    case SP_DockWidgetCloseButton:
    case SP_TabCloseButton:
    case SP_LineEditClearButton:
    case SP_BrowserStop:
        return cross(small);
    case SP_TitleBarMinButton:
        return drawnPixmap(small, [&](QPainter &p, const QRectF &r) {
            p.fillRect(QRectF(r.width() * 0.25, r.height() * 0.65, r.width() * 0.5, 1.6), ink);
        });
    case SP_TitleBarMaxButton:
    case SP_MediaStop:
        return drawnPixmap(small, [&](QPainter &p, const QRectF &r) {
            p.setPen(QPen(ink, 1.4));
            p.drawRect(r.adjusted(small * 0.25, small * 0.25, -small * 0.25, -small * 0.25));
        });
    case SP_TitleBarNormalButton:
        return drawnPixmap(small, [&](QPainter &p, const QRectF &r) {
            p.setPen(QPen(ink, 1.2));
            p.drawRect(r.adjusted(small * 0.2, small * 0.35, -small * 0.35, -small * 0.2));
            p.drawRect(r.adjusted(small * 0.35, small * 0.2, -small * 0.2, -small * 0.35));
        });
    case SP_MediaPause:
        return drawnPixmap(small, [&](QPainter &p, const QRectF &r) {
            p.fillRect(QRectF(r.width() * 0.28, r.height() * 0.25, r.width() * 0.14, r.height() * 0.5), ink);
            p.fillRect(QRectF(r.width() * 0.58, r.height() * 0.25, r.width() * 0.14, r.height() * 0.5), ink);
        });
    case SP_DirIcon:
    case SP_DirOpenIcon:
    case SP_DirClosedIcon:
    case SP_DirLinkIcon:
    case SP_DirLinkOpenIcon:
    case SP_DirHomeIcon:
    case SP_DesktopIcon:
    case SP_FileDialogNewFolder:
        return drawnPixmap(small, [&](QPainter &p, const QRectF &r) {
            // A folder: a tab on the back edge and a body in front.
            const qreal w = r.width();
            const qreal h = r.height();
            p.setPen(QPen(ink, 1.0));
            p.setBrush(QColor(230, 185, 80));
            QPainterPath folder;
            folder.moveTo(w * 0.1, h * 0.25);
            folder.lineTo(w * 0.4, h * 0.25);
            folder.lineTo(w * 0.5, h * 0.35);
            folder.lineTo(w * 0.9, h * 0.35);
            folder.lineTo(w * 0.9, h * 0.8);
            folder.lineTo(w * 0.1, h * 0.8);
            folder.closeSubpath();
            p.drawPath(folder);
        });
    case SP_FileIcon:
    case SP_FileLinkIcon:
    case SP_TrashIcon:
        return drawnPixmap(small, [&](QPainter &p, const QRectF &r) {
            // A sheet of paper with a folded corner.
            const qreal w = r.width();
            const qreal h = r.height();
            p.setPen(QPen(ink, 1.0));
            p.setBrush(Qt::white);
            QPainterPath sheet;
            sheet.moveTo(w * 0.22, h * 0.1);
            sheet.lineTo(w * 0.6, h * 0.1);
            sheet.lineTo(w * 0.8, h * 0.3);
            sheet.lineTo(w * 0.8, h * 0.9);
            sheet.lineTo(w * 0.22, h * 0.9);
            sheet.closeSubpath();
            p.drawPath(sheet);
            p.drawLine(QPointF(w * 0.6, h * 0.1), QPointF(w * 0.6, h * 0.3));
            p.drawLine(QPointF(w * 0.6, h * 0.3), QPointF(w * 0.8, h * 0.3));
        });
    case SP_ComputerIcon:
    case SP_DriveHDIcon:
    case SP_DriveNetIcon:
    case SP_DriveCDIcon:
    case SP_DriveDVDIcon:
    case SP_DriveFDIcon:
        return drawnPixmap(small, [&](QPainter &p, const QRectF &r) {
            // A drive: a flat box with a light.
            p.setPen(QPen(ink, 1.0));
            p.setBrush(QColor(190, 190, 195));
            p.drawRect(QRectF(r.width() * 0.1, r.height() * 0.35, r.width() * 0.8, r.height() * 0.35));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(60, 170, 80));
            p.drawEllipse(QRectF(r.width() * 0.7, r.height() * 0.48, r.width() * 0.1, r.height() * 0.1));
        });
    default:
        return QPixmap();
    }
}

/*!
    Prepares \a widget for the style, before it is shown for the first time or when the style becomes its style:
    a style that animates or reacts to the mouse in its own way sets attributes or installs event filters here.
    The default does nothing.

    \sa unpolish()
*/
void QStyle::polish(QWidget *widget) { static_cast<void>(widget); }

/*!
    Takes back what polish() did to \a widget, when the style stops being its style. The default does nothing.

    \sa polish()
*/
void QStyle::unpolish(QWidget *widget) { static_cast<void>(widget); }

/*!
    \overload

    Prepares \a application for the style when the style becomes the application's style. The default does
    nothing.
*/
void QStyle::polish(QApplication *application) { static_cast<void>(application); }

/*!
    \overload

    Takes back what polish() did to \a application. The default does nothing.
*/
void QStyle::unpolish(QApplication *application) { static_cast<void>(application); }

/*!
    \overload

    Changes \a palette, the palette that the application is about to use, to suit the style. The default leaves it
    as it is.
*/
void QStyle::polish(QPalette &palette) { static_cast<void>(palette); }

/*!
    \fn QString QStyle::name() const

    Returns the name that QStyleFactory made the style with, such as \c "Fusion", or an empty string for a style
    that was made otherwise.

    \sa QStyleFactory::create()
*/

#include "moc_qstyle.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
