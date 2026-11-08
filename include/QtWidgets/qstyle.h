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
        PE_FrameLineEdit
    };
    enum ControlElement { CE_PushButton, CE_PushButtonBevel, CE_PushButtonLabel, CE_CheckBox, CE_CheckBoxLabel };
    enum SubElement { SE_CheckBoxIndicator, SE_CheckBoxContents, SE_PushButtonContents };
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
        PM_LayoutVerticalSpacing
    };
    enum ContentsType { CT_PushButton, CT_CheckBox, CT_LineEdit };

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
