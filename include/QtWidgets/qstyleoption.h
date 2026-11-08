// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qstyle.h>

class QStyleOption {
public:
    enum OptionType {
        SO_Default,
        SO_Button,
        SO_Frame,
        SO_Slider,
        SO_ProgressBar,
        SO_Tab,
        SO_ComboBox,
        SO_SpinBox,
        SO_GroupBox
    };

    explicit QStyleOption(int optionType = SO_Default) : type(optionType), fontMetrics(QFont()) {}
    virtual ~QStyleOption();
    void initFrom(const QWidget *widget);

    int type;
    QStyle::State state;
    QRect rect;
    QPalette palette;
    QFontMetrics fontMetrics;
};

class QStyleOptionButton : public QStyleOption {
public:
    enum ButtonFeature { None = 0x00, Flat = 0x01, DefaultButton = 0x02 };
    Q_DECLARE_FLAGS(ButtonFeatures, ButtonFeature)

    QStyleOptionButton() : QStyleOption(SO_Button) {}

    ButtonFeatures features;
    QString text;
};

class QStyleOptionFrame : public QStyleOption {
public:
    QStyleOptionFrame() : QStyleOption(SO_Frame) {}

    int lineWidth = 1;
};

class QStyleOptionSlider : public QStyleOption {
public:
    QStyleOptionSlider() : QStyleOption(SO_Slider) {}

    Qt::Orientation orientation = Qt::Horizontal;
    int minimum = 0;
    int maximum = 100;
    int sliderPosition = 0;
};

class QStyleOptionProgressBar : public QStyleOption {
public:
    QStyleOptionProgressBar() : QStyleOption(SO_ProgressBar) {}

    int minimum = 0;
    int maximum = 100;
    int progress = 0;
    QString text;
    bool textVisible = true;
};

class QStyleOptionTab : public QStyleOption {
public:
    QStyleOptionTab() : QStyleOption(SO_Tab) {}

    QString text;
};

class QStyleOptionComboBox : public QStyleOption {
public:
    QStyleOptionComboBox() : QStyleOption(SO_ComboBox) {}

    QString currentText;
};

class QStyleOptionSpinBox : public QStyleOption {
public:
    QStyleOptionSpinBox() : QStyleOption(SO_SpinBox) {}

    bool upEnabled = true;
    bool downEnabled = true;
    bool upPressed = false;
    bool downPressed = false;
};

class QStyleOptionGroupBox : public QStyleOption {
public:
    QStyleOptionGroupBox() : QStyleOption(SO_GroupBox) {}

    QString text;
    bool checkable = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionButton::ButtonFeatures)
