// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qstyle.h>

class QStyleOption {
public:
    enum OptionType { SO_Default, SO_Button, SO_Frame };

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

Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionButton::ButtonFeatures)
