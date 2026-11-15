// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qlocale.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qtransform.h>
#include <QtWidgets/qstyle.h>

#include <type_traits>

class QStyleOption {
public:
    enum OptionType {
        SO_Default,
        SO_FocusRect,
        SO_Button,
        SO_Tab,
        SO_MenuItem,
        SO_Frame,
        SO_ProgressBar,
        SO_ToolBox,
        SO_Header,
        SO_DockWidget,
        SO_ViewItem,
        SO_TabWidgetFrame,
        SO_TabBarBase,
        SO_RubberBand,
        SO_ToolBar,
        SO_GraphicsItem,
        SO_Complex = 0xf0000,
        SO_Slider,
        SO_SpinBox,
        SO_ToolButton,
        SO_ComboBox,
        SO_TitleBar,
        SO_GroupBox,
        SO_SizeGrip,
        SO_CustomBase = 0xf00,
        SO_ComplexCustomBase = 0xf000000
    };
    enum StyleOptionType { Type = SO_Default };
    enum StyleOptionVersion { Version = 1 };

    explicit QStyleOption(int optionType = SO_Default) : type(optionType), fontMetrics(QFont()) {}
    QStyleOption(int optionVersion, int optionType) : type(optionType), version(optionVersion), fontMetrics(QFont()) {}
    QStyleOption(const QStyleOption &other) = default;
    QStyleOption &operator=(const QStyleOption &other) = default;
    virtual ~QStyleOption();
    void initFrom(const QWidget *widget);

    int type;
    int version = Version;
    QStyle::State state;
    Qt::LayoutDirection direction = Qt::LeftToRight;
    QRect rect;
    QPalette palette;
    QFontMetrics fontMetrics;
    QObject *styleObject = nullptr;
};

class QStyleOptionFocusRect : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_FocusRect };
    QStyleOptionFocusRect() : QStyleOption(SO_FocusRect) {}

    QColor backgroundColor;
};

class QStyleOptionButton : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_Button };
    enum ButtonFeature {
        None = 0x00,
        Flat = 0x01,
        HasMenu = 0x02,
        DefaultButton = 0x04,
        AutoDefaultButton = 0x08,
        CommandLinkButton = 0x10
    };
    Q_DECLARE_FLAGS(ButtonFeatures, ButtonFeature)

    QStyleOptionButton() : QStyleOption(SO_Button) {}

    ButtonFeatures features;
    QString text;
    QSize iconSize;
};

class QStyleOptionFrame : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_Frame };
    enum FrameFeature { None = 0x00, Flat = 0x01, Rounded = 0x02 };
    Q_DECLARE_FLAGS(FrameFeatures, FrameFeature)

    QStyleOptionFrame() : QStyleOption(SO_Frame) {}

    int lineWidth = 1;
    int midLineWidth = 0;
    FrameFeatures features;
    int frameShape = 0; // a QFrame::Shape
};

class QStyleOptionTabWidgetFrame : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_TabWidgetFrame };
    QStyleOptionTabWidgetFrame() : QStyleOption(SO_TabWidgetFrame) {}

    int lineWidth = 0;
    int midLineWidth = 0;
    int shape = 0; // a QTabBar::Shape
    QSize tabBarSize;
    QSize rightCornerWidgetSize;
    QSize leftCornerWidgetSize;
    QRect tabBarRect;
    QRect selectedTabRect;
};

class QStyleOptionTabBarBase : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_TabBarBase };
    QStyleOptionTabBarBase() : QStyleOption(SO_TabBarBase) {}

    int shape = 0; // a QTabBar::Shape
    QRect tabBarRect;
    QRect selectedTabRect;
    bool documentMode = false;
};

class QStyleOptionComplex : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_Complex };
    explicit QStyleOptionComplex(int optionType = SO_Complex) : QStyleOption(optionType) {}

    QStyle::SubControls subControls = QStyle::SC_All;
    QStyle::SubControls activeSubControls = QStyle::SC_None;
};

class QStyleOptionSlider : public QStyleOptionComplex {
public:
    enum StyleOptionType { Type = SO_Slider };
    QStyleOptionSlider() : QStyleOptionComplex(SO_Slider) {}

    Qt::Orientation orientation = Qt::Horizontal;
    int minimum = 0;
    int maximum = 100;
    int tickPosition = 0; // a QSlider::TickPosition
    int tickInterval = 0;
    bool upsideDown = false;
    int sliderPosition = 0;
    int sliderValue = 0;
    int singleStep = 1;
    int pageStep = 0;
    qreal notchTarget = 3.7;
    bool dialWrapping = false;
    bool tickMarks = false;
    Qt::KeyboardModifiers keyboardModifiers;
};

class QStyleOptionProgressBar : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_ProgressBar };
    QStyleOptionProgressBar() : QStyleOption(SO_ProgressBar) {}

    int minimum = 0;
    int maximum = 100;
    int progress = 0;
    QString text;
    Qt::Alignment textAlignment = Qt::AlignLeft;
    bool textVisible = true;
    bool invertedAppearance = false;
    bool bottomToTop = false;
};

class QStyleOptionTab : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_Tab };
    enum TabPosition { Beginning, Middle, End, OnlyOneTab, Moving };
    enum SelectedPosition { NotAdjacent, NextIsSelected, PreviousIsSelected };
    enum CornerWidget { NoCornerWidgets = 0x00, LeftCornerWidget = 0x01, RightCornerWidget = 0x02 };
    Q_DECLARE_FLAGS(CornerWidgets, CornerWidget)
    enum TabFeature { None = 0x00, HasFrame = 0x01, MinimumSizeHint = 0x02 };
    Q_DECLARE_FLAGS(TabFeatures, TabFeature)

    QStyleOptionTab() : QStyleOption(SO_Tab) {}

    int shape = 0; // a QTabBar::Shape
    QString text;
    int row = 0;
    TabPosition position = Beginning;
    SelectedPosition selectedPosition = NotAdjacent;
    CornerWidgets cornerWidgets;
    QSize iconSize;
    bool documentMode = false;
    QSize leftButtonSize;
    QSize rightButtonSize;
    TabFeatures features;
    int tabIndex = -1;
};

class QStyleOptionToolBar : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_ToolBar };
    enum ToolBarPosition { Beginning, Middle, End, OnlyOne };
    enum ToolBarFeature { None = 0x0, Movable = 0x1 };
    Q_DECLARE_FLAGS(ToolBarFeatures, ToolBarFeature)

    QStyleOptionToolBar() : QStyleOption(SO_ToolBar) {}

    ToolBarPosition positionOfLine = OnlyOne;
    ToolBarPosition positionWithinLine = OnlyOne;
    Qt::ToolBarArea toolBarArea = Qt::TopToolBarArea;
    ToolBarFeatures features;
    int lineWidth = 1;
    int midLineWidth = 0;
};

class QStyleOptionToolBox : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_ToolBox };
    enum TabPosition { Beginning, Middle, End, OnlyOneTab };
    enum SelectedPosition { NotAdjacent, NextIsSelected, PreviousIsSelected };

    QStyleOptionToolBox() : QStyleOption(SO_ToolBox) {}

    QString text;
    TabPosition position = Beginning;
    SelectedPosition selectedPosition = NotAdjacent;
};

class QStyleOptionDockWidget : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_DockWidget };
    QStyleOptionDockWidget() : QStyleOption(SO_DockWidget) {}

    QString title;
    bool closable = false;
    bool movable = false;
    bool floatable = false;
    bool verticalTitleBar = false;
};

class QStyleOptionRubberBand : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_RubberBand };
    QStyleOptionRubberBand() : QStyleOption(SO_RubberBand) {}

    int shape = 0; // a QRubberBand::Shape
    bool opaque = false;
};

class QStyleOptionComboBox : public QStyleOptionComplex {
public:
    enum StyleOptionType { Type = SO_ComboBox };
    QStyleOptionComboBox() : QStyleOptionComplex(SO_ComboBox) {}

    bool editable = false;
    QRect popupRect;
    bool frame = true;
    QString currentText;
    QSize iconSize;
    Qt::Alignment textAlignment = Qt::AlignLeft | Qt::AlignVCenter;
};

class QStyleOptionSpinBox : public QStyleOptionComplex {
public:
    enum StyleOptionType { Type = SO_SpinBox };
    QStyleOptionSpinBox() : QStyleOptionComplex(SO_SpinBox) {}

    int buttonSymbols = 0; // a QAbstractSpinBox::ButtonSymbols
    bool frame = true;
    bool upEnabled = true;
    bool downEnabled = true;
    bool upPressed = false;
    bool downPressed = false;
};

class QStyleOptionToolButton : public QStyleOptionComplex {
public:
    enum StyleOptionType { Type = SO_ToolButton };
    enum ToolButtonFeature {
        None = 0x00,
        Arrow = 0x01,
        Menu = 0x04,
        MenuButtonPopup = Menu,
        PopupDelay = 0x08,
        HasMenu = 0x10
    };
    Q_DECLARE_FLAGS(ToolButtonFeatures, ToolButtonFeature)

    QStyleOptionToolButton() : QStyleOptionComplex(SO_ToolButton) {}

    ToolButtonFeatures features;
    QString text;
    QSize iconSize;
    QFont font;
    Qt::ArrowType arrowType = Qt::DownArrow;
    Qt::ToolButtonStyle toolButtonStyle = Qt::ToolButtonIconOnly;
    QPoint pos;
};

class QStyleOptionTitleBar : public QStyleOptionComplex {
public:
    enum StyleOptionType { Type = SO_TitleBar };
    QStyleOptionTitleBar() : QStyleOptionComplex(SO_TitleBar) {}

    QString text;
    int titleBarState = 0;
    Qt::WindowFlags titleBarFlags;
};

class QStyleOptionGroupBox : public QStyleOptionComplex {
public:
    enum StyleOptionType { Type = SO_GroupBox };
    QStyleOptionGroupBox() : QStyleOptionComplex(SO_GroupBox) {}

    QStyleOptionFrame::FrameFeatures features;
    QString text;
    Qt::Alignment textAlignment = Qt::AlignLeft;
    QColor textColor;
    int lineWidth = 1;
    int midLineWidth = 0;
    bool checkable = false;
};

class QStyleOptionSizeGrip : public QStyleOptionComplex {
public:
    enum StyleOptionType { Type = SO_SizeGrip };
    QStyleOptionSizeGrip() : QStyleOptionComplex(SO_SizeGrip) {}

    int corner = 3; // a Qt::Corner: Qt::BottomRightCorner
};

class QStyleOptionHeader : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_Header };
    enum SectionPosition { Beginning, Middle, End, OnlyOneSection };
    enum SelectedPosition { NotAdjacent, NextIsSelected, PreviousIsSelected, NextAndPreviousAreSelected };

    QStyleOptionHeader() : QStyleOption(SO_Header) {}

    Qt::Orientation orientation = Qt::Horizontal;
    int section = 0;
    QString text;
    Qt::Alignment textAlignment = Qt::AlignCenter;
    Qt::Alignment iconAlignment = Qt::AlignLeft;
    SectionPosition position = Beginning;
    SelectedPosition selectedPosition = NotAdjacent;
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
Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionFrame::FrameFeatures)
Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionTab::CornerWidgets)
Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionTab::TabFeatures)
Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionToolBar::ToolBarFeatures)
Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionToolButton::ToolButtonFeatures)

class QStyleOptionMenuItem : public QStyleOption {
public:
    enum StyleOptionType { Type = SO_MenuItem };
    enum MenuItemType { Normal, DefaultItem, Separator, SubMenu, Scroller, TearOff, Margin, EmptyArea };
    enum CheckType { NotCheckable, Exclusive, NonExclusive };

    QStyleOptionMenuItem() : QStyleOption(SO_MenuItem) {}

    MenuItemType menuItemType = Normal;
    CheckType checkType = NotCheckable;
    bool checked = false;
    bool menuHasCheckableItems = true;
    QRect menuRect;
    QString text;
    int maxIconWidth = 0;
    int reservedShortcutWidth = 0;
    QFont font;
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
    \c icon, as Nt has no QIcon. There is no \c viewItemPosition.
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

    /*!
        \enum QStyleOptionViewItem::Position

        Where the decoration of an item is, next to its text.

        \value Left Left of the text, as in a list or a table.
        \value Right Right of the text.
        \value Top Above the text, as in the icon mode of QListView.
        \value Bottom Below the text.
    */
    enum Position { Left, Right, Top, Bottom };

    /*!
        \enum QStyleOptionViewItem::ViewItemPosition

        Where an item is in its row, for a style that draws a row as one piece.

        \value Invalid Not known.
        \value Beginning The first item of the row.
        \value Middle An item between others.
        \value End The last item of the row.
        \value OnlyOne The only item of the row.
    */
    enum ViewItemPosition { Invalid, Beginning, Middle, End, OnlyOne };
    /*!
        \enum QStyleOptionViewItem::StyleOptionType
        \value Type The option type, \c SO_ViewItem.
    */
    enum StyleOptionType { Type = SO_ViewItem };

    /*! Constructs an option for an item, with the type \c SO_ViewItem. */
    QStyleOptionViewItem() : QStyleOption(SO_ViewItem) {}

    /*! How the text is aligned in its part of the item; left and vertically centered by default. */
    Qt::Alignment displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    /*! Where the text gets its ellipsis when it does not fit; at the end by default. */
    Qt::TextElideMode textElideMode = Qt::ElideRight;
    /*! The size that the decoration is drawn in; the size of \c decoration if it is empty. */
    QSize decorationSize;
    /*! Where the decoration is, next to the text; on the left by default. */
    Position decorationPosition = Left;
    /*! How the decoration is aligned in its part of the item; centered by default. */
    Qt::Alignment decorationAlignment = Qt::AlignCenter;
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
    /*! The decoration of the item, drawn next to its text where \c decorationPosition says; null if it has none. */
    QPixmap decoration;
    /*! The text of the item. */
    QString text;
    /*! The background of the item, if the model gives it one. */
    QBrush backgroundBrush;
    /*! Where the item is in its row. */
    ViewItemPosition viewItemPosition = Invalid;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QStyleOptionViewItem::ViewItemFeatures)

class QStyleOptionGraphicsItem : public QStyleOption {
public:
    /*!
        \enum QStyleOptionGraphicsItem::StyleOptionType
        \value Type The option type, \c SO_GraphicsItem.
    */
    enum StyleOptionType { Type = SO_GraphicsItem };
    /*! Constructs an option for a graphics item, with the type \c SO_GraphicsItem. */
    QStyleOptionGraphicsItem() : QStyleOption(SO_GraphicsItem) {}
    static qreal levelOfDetailFromTransform(const QTransform &worldTransform);
    /*! The part of the item that needs painting, in item coordinates. */
    QRectF exposedRect;
};

template <typename T> T qstyleoption_cast(const QStyleOption *option) {
    using Option = std::remove_cv_t<std::remove_pointer_t<T>>;
    if (option && (int(Option::Type) == QStyleOption::SO_Default || option->type == int(Option::Type) ||
                   (int(Option::Type) == QStyleOption::SO_Complex && option->type > QStyleOption::SO_Complex)))
        return static_cast<T>(option);
    return nullptr;
}

template <typename T> T qstyleoption_cast(QStyleOption *option) {
    using Option = std::remove_cv_t<std::remove_pointer_t<T>>;
    if (option && (int(Option::Type) == QStyleOption::SO_Default || option->type == int(Option::Type) ||
                   (int(Option::Type) == QStyleOption::SO_Complex && option->type > QStyleOption::SO_Complex)))
        return static_cast<T>(option);
    return nullptr;
}
