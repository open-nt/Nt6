// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qlocale.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpixmap.h>
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
        SO_GroupBox,
        SO_Header,
        SO_MenuItem,
        SO_ViewItem
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
    int pageStep = 0;
    bool tickMarks = false;
    bool wrapping = false;
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

class QStyleOptionHeader : public QStyleOption {
public:
    QStyleOptionHeader() : QStyleOption(SO_Header) {}

    Qt::Orientation orientation = Qt::Horizontal;
    QString text;
    Qt::Alignment textAlignment = Qt::AlignCenter;
    bool lastSection = false;
    /*!
        \enum QStyleOptionHeader::SortIndicator

        The arrow that a section shows for the order that its column is
        sorted in.

        \value None No arrow: the column is not the one that is sorted by.
        \value SortUp The arrow of the descending order.
        \value SortDown The arrow of the ascending order.
    */
    enum SortIndicator { None, SortUp, SortDown };
    /*! The arrow of the sort order that the section shows; \c None by default. */
    SortIndicator sortIndicator = None;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionButton::ButtonFeatures)

class QStyleOptionMenuItem : public QStyleOption {
public:
    enum MenuItemType { Normal, Separator, SubMenu };
    enum CheckType { NotCheckable, Exclusive, NonExclusive };

    QStyleOptionMenuItem() : QStyleOption(SO_MenuItem) {}

    MenuItemType menuItemType = Normal;
    CheckType checkType = NotCheckable;
    bool checked = false;
    QString text;
    QFont font;
    int tabWidth = 0;
};

/*!
    \class QStyleOptionViewItem
    \brief The QStyleOptionViewItem class describes how an item of an item view is drawn.

    A view fills one of these in for each item that it draws and passes it
    to its delegate; QStyledItemDelegate::initStyleOption() adds what the
    model says about the item: its text, check state, decoration, font and
    colors.

    \differences
    The decoration is a QPixmap in \c decoration, where Qt has a QIcon in
    \c icon, as Nt has no QIcon. There is no \c viewItemPosition and no
    \c decorationPosition: the decoration is on the left.
    \enddifferences

    \sa QStyledItemDelegate, QAbstractItemView
*/
class QStyleOptionViewItem : public QStyleOption {
public:
    /*!
        \enum QStyleOptionViewItem::ViewItemFeature

        What an item has and how it is drawn.

        \value None Nothing special.
        \value WrapText The text may be broken into lines. Nt draws one line.
        \value Alternate The item is in a row that has the alternate background.
        \value HasCheckIndicator The item has a check box.
        \value HasDisplay The item has text.
        \value HasDecoration The item has a decoration.
    */
    enum ViewItemFeature {
        None = 0x00,
        WrapText = 0x01,
        Alternate = 0x02,
        HasCheckIndicator = 0x04,
        HasDisplay = 0x08,
        HasDecoration = 0x10
    };
    Q_DECLARE_FLAGS(ViewItemFeatures, ViewItemFeature)

    /*! Constructs an option for an item, with the type \c SO_ViewItem. */
    QStyleOptionViewItem() : QStyleOption(SO_ViewItem) {}

    /*! How the text is aligned in its part of the item; left and vertically centered by default. */
    Qt::Alignment displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    /*! Where the text gets its ellipsis when it does not fit; at the end by default. */
    Qt::TextElideMode textElideMode = Qt::ElideRight;
    /*! The size that the decoration is drawn in; the size of \c decoration if it is empty. */
    QSize decorationSize;
    /*! The font of the text. */
    QFont font;
    /*! Whether the decoration is part of what shows the selection. */
    bool showDecorationSelected = false;
    /*! What the item has; see ViewItemFeature. */
    ViewItemFeatures features;
    /*! The locale that numbers and dates are written with. */
    QLocale locale;
    /*! The widget that the item is drawn on: the view, or \c nullptr. */
    const QWidget *widget = nullptr;
    /*! The model index of the item. */
    QModelIndex index;
    /*! The check state of the item, if it has a check box. */
    Qt::CheckState checkState = Qt::Unchecked;
    /*! The decoration of the item, drawn left of its text; null if it has none. */
    QPixmap decoration;
    /*! The text of the item. */
    QString text;
    /*! The background of the item, if the model gives it one. */
    QBrush backgroundBrush;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionViewItem::ViewItemFeatures)
