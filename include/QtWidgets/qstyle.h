// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtGui/qpalette.h>

class QPainter;
class QStyleOption;
class QWidget;

class QStyle : public QObject {
    Q_OBJECT
public:
    enum StateFlag {
        State_None = 0x00000000,
        State_Enabled = 0x00000001,
        State_Raised = 0x00000002,
        State_Sunken = 0x00000004,
        State_Off = 0x00000008,
        State_On = 0x00000020,
        State_HasFocus = 0x00000100,
        State_Horizontal = 0x00000080,
        State_Selected = 0x00000200,
        State_MouseOver = 0x00002000,
        State_Active = 0x00010000
    };
    Q_DECLARE_FLAGS(State, StateFlag)

    enum PrimitiveElement {
        PE_Frame,
        PE_FrameFocusRect,
        PE_PanelButtonCommand,
        PE_IndicatorCheckBox,
        PE_PanelLineEdit,
        PE_FrameLineEdit,
        PE_IndicatorRadioButton,
        PE_IndicatorArrowUp,
        PE_IndicatorArrowDown,
        PE_FrameGroupBox,
        PE_FrameTabWidget,
        PE_PanelTipLabel,
        PE_IndicatorArrowLeft,
        PE_IndicatorArrowRight,
        PE_PanelStatusBar,
        PE_PanelMenu
    };
    enum ControlElement {
        CE_PushButton,
        CE_PushButtonBevel,
        CE_PushButtonLabel,
        CE_CheckBox,
        CE_CheckBoxLabel,
        CE_RadioButton,
        CE_RadioButtonLabel,
        CE_Slider,
        CE_ProgressBar,
        CE_TabBarTab,
        CE_ComboBox,
        CE_SpinBox,
        CE_GroupBox,
        CE_ScrollBar,
        CE_Header,
        CE_Dial,
        CE_Splitter,
        CE_MenuItem,
        CE_MenuBarItem,
        CE_MenuBarEmptyArea
    };
    enum SubElement {
        SE_CheckBoxIndicator,
        SE_CheckBoxContents,
        SE_PushButtonContents,
        SE_RadioButtonIndicator,
        SE_RadioButtonContents,
        SE_LineEditContents,
        SE_SliderGroove,
        SE_SliderHandle,
        SE_ProgressBarContents,
        SE_ComboBoxArrow,
        SE_SpinBoxEditField,
        SE_SpinBoxUp,
        SE_SpinBoxDown,
        SE_GroupBoxIndicator,
        SE_GroupBoxTitle,
        SE_GroupBoxContents,
        SE_ScrollBarSlider
    };
    enum PixelMetric {
        PM_ButtonMargin,
        PM_DefaultFrameWidth,
        PM_IndicatorWidth,
        PM_IndicatorHeight,
        PM_CheckBoxLabelSpacing,
        PM_LayoutLeftMargin,
        PM_LayoutTopMargin,
        PM_LayoutRightMargin,
        PM_LayoutBottomMargin,
        PM_LayoutHorizontalSpacing,
        PM_LayoutVerticalSpacing,
        PM_ExclusiveIndicatorWidth,
        PM_ExclusiveIndicatorHeight,
        PM_SliderThickness,
        PM_SliderLength,
        PM_SpinBoxButtonWidth,
        PM_ComboBoxArrowWidth,
        PM_ScrollBarExtent,
        PM_ScrollBarSliderMin,
        PM_HeaderMargin,
        PM_HeaderDefaultSectionSizeHorizontal,
        PM_HeaderDefaultSectionSizeVertical,
        PM_SplitterWidth,
        PM_MenuBarItemSpacing
    };
    enum ContentsType {
        CT_PushButton,
        CT_CheckBox,
        CT_LineEdit,
        CT_RadioButton,
        CT_ProgressBar,
        CT_TabBarTab,
        CT_ComboBox,
        CT_SpinBox,
        CT_ToolButton,
        CT_MenuItem,
        CT_MenuBarItem
    };

    ~QStyle() override;

    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                               const QWidget *widget = nullptr) const = 0;
    virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                             const QWidget *widget = nullptr) const = 0;
    virtual QRect subElementRect(SubElement element, const QStyleOption *option,
                                 const QWidget *widget = nullptr) const = 0;
    virtual int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                            const QWidget *widget = nullptr) const = 0;
    virtual QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                                   const QWidget *widget = nullptr) const = 0;
    virtual QPalette standardPalette() const;

protected:
    QStyle() = default;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QStyle::State)
