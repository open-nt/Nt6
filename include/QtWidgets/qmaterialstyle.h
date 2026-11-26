// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qpointer.h>
#include <QtGui/qcolor.h>
#include <QtGui/qicon.h>
#include <QtWidgets/qcommonstyle.h>

#include <memory>

namespace QtPrivate {
class Animator;
struct SpringSpec;
} // namespace QtPrivate

class QCalendarWidget;
class QFrame;
class QLineEdit;
class QListWidget;
class QMainWindow;
class QStatusBar;
class QToolBar;
class QStyleOptionButton;
class QMenu;
class QPushButton;
class QToolButton;
class QPainterPath;
class QStyledItemDelegate;
class QTabBar;

// The Material 3 Expressive look. Nt only: Qt Widgets has no such style.
class QMaterialStyle : public QCommonStyle {
    Q_OBJECT
public:
    // The colour roles of Material 3.
    enum class Role {
        Primary,
        OnPrimary,
        PrimaryContainer,
        OnPrimaryContainer,
        Secondary,
        OnSecondary,
        SecondaryContainer,
        OnSecondaryContainer,
        Tertiary,
        OnTertiary,
        TertiaryContainer,
        OnTertiaryContainer,
        Error,
        OnError,
        ErrorContainer,
        OnErrorContainer,
        Surface,
        OnSurface,
        SurfaceVariant,
        OnSurfaceVariant,
        SurfaceContainerLowest,
        SurfaceContainerLow,
        SurfaceContainer,
        SurfaceContainerHigh,
        SurfaceContainerHighest,
        Outline,
        OutlineVariant,
        InverseSurface,
        InverseOnSurface,
        InversePrimary
    };
    Q_ENUM(Role)

    QMaterialStyle();
    explicit QMaterialStyle(const QColor &seedColor);
    ~QMaterialStyle() override;

    // The color that the system's own theme is made from, on Android 12 and later; invalid elsewhere.
    static QColor systemSeedColor();
    QColor seedColor() const { return m_seed; }
    void setSeedColor(const QColor &color);
    Qt::ColorScheme colorScheme() const { return m_scheme; }
    void setColorScheme(Qt::ColorScheme scheme);
    bool isDark() const;
    QColor color(Role role) const;

    bool isMotionEnabled() const { return m_motion; }
    void setMotionEnabled(bool enabled);

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                           const QWidget *widget = nullptr) const override;
    QPalette standardPalette() const override;
    using QCommonStyle::polish;
    using QCommonStyle::unpolish;
    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    void polish(QApplication *application) override;
    void unpolish(QApplication *application) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class ButtonKind {
        Filled,
        Tonal,
        Outlined,
        Text,
        Elevated,
        Fab,
        ToolBarItem,
        IconStandard,
        IconFilled,
        IconTonal,
        IconOutlined,
        Chip
    };
    static ButtonKind buttonKind(const QStyleOption *option, const QWidget *widget);
    void drawButtonContainer(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    // The container of a button, or of one half of a split button, with its corners at the left and the right.
    void drawButtonShape(const QStyleOption *option, QPainter *painter, const QWidget *widget, double left,
                         double right) const;
    void drawButtonLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawFabLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawChipLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    // A QToolButton with MenuButtonPopup is a split button. It paints its label itself, in the color returned.
    friend class QToolButton;
    QColor drawSplitButton(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget,
                           const QRect &trailing, bool open) const;
    // Opens the menu of a floating action button as Material's FAB menu, above it. False for another button, whose
    // menu is left to open as menus do.
    friend class QPushButton;
    bool popupFabMenu(QPushButton *button, QMenu *menu) const;
    // A floating toolbar. QToolBar lays itself out for one and asks for its container; its buttons ask for the color
    // of their labels (as does another tool button with a kind or a size); for any other button the color is invalid.
    // A text field: its container, label and the line under it. QLineEdit draws its placeholder itself, and asks if
    // the resting label stands where it would.
    friend class QLineEdit;
    bool hidesPlaceholder(const QLineEdit *field) const;
    void drawTextFieldText(QPainter *painter, const QStyleOption *option, const QWidget *widget) const;
    // A property of the style's that a widget was given, or lost, while it is shown.
    void propertyChanged(QWidget *widget);
    // A day of a calendar, as the date picker of Material shows it: QCalendarWidget::paintCell() asks for it. The
    // text is in `ink` if the program formatted the day with a color.
    friend class QCalendarWidget;
    void drawCalendarDay(QPainter *painter, const QRect &cell, const QCalendarWidget *calendar, int day, bool selected,
                         bool today, bool usable, const QColor &ink) const;
    // A frame that is a card, or a line that is a divider: QFrame asks for it. False for another frame.
    friend class QFrame;
    bool drawMaterialFrame(QPainter *painter, const QFrame *frame) const;
    friend class QToolBar;
    static bool isFloatingToolBar(const QWidget *widget);
    // A top app bar: QToolBar lays itself out for one and asks for its background and title.
    static bool isAppBar(const QWidget *widget);
    void drawAppBar(QPainter *painter, const QToolBar *bar) const;
    void drawFloatingToolBar(QPainter *painter, const QToolBar *bar) const;
    QColor toolBarItemColor(const QStyleOption *option, const QWidget *widget) const;
    // An icon in `ink`, if it has one color of its own; one with several is drawn as it is.
    static void paintIcon(QPainter *painter, const QRect &rect, const QIcon &icon, QIcon::Mode mode, QIcon::State state,
                          const QColor &ink);
    // The width and height of the icon of an icon button; 0 for another button.
    static int iconButtonIconSize(const QWidget *widget);
    void drawSwitch(const QStyleOption *option, const QRect &rect, QPainter *painter, const QWidget *widget) const;
    void drawSlider(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawProgressBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawLoadingIndicator(const QStyleOption *option, QPainter *painter, const QWidget *widget,
                              bool contained) const;
    void drawTab(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawComboBox(const QStyleOption *option, QPainter *painter) const;
    void drawMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *menu) const;
    void drawFabMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *menu) const;
    QColor buttonLabelColor(const QStyleOption *option, const QWidget *widget) const;
    bool keyboardFocus(const QStyleOption *option) const;
    double stateLayer(const QStyleOption *option) const;
    // Where a value of a widget is on its way to `target`, along a spring; the target itself without motion.
    double animated(const QWidget *widget, int channel, double target, const QtPrivate::SpringSpec &spring,
                    double precision) const;
    void drawRipple(QPainter *painter, const QWidget *widget, const QPainterPath &outline, const QRectF &shape,
                    const QPointF &origin, bool held, const QColor &ink) const;
    // What a press, the pointer or the focus shows around the control of a check box, a radio button or a switch.
    void drawChoicePress(const QStyleOption *option, QPainter *painter, const QWidget *widget, const QPointF &centre,
                         const QColor &ink) const;
    // The background of a row of a list: its selection, and the press on it. The views that paint rows ask for it.
    friend class QListWidget;
    friend class QStyledItemDelegate;
    void drawListRow(QPainter *painter, const QRect &row, bool selected, bool enabled, const QBrush &background,
                     const QWidget *view) const;
    // The line under the current tab of a horizontal tab bar. QTabBar paints a tab at a time, so it asks for the line,
    // which slides from tab to tab, once they are all painted.
    friend class QTabBar;
    void drawTabIndicator(QPainter *painter, const QTabBar *bar, const QRect &tab, bool below) const;
    // A status bar that is a snackbar: QMainWindow floats it over the content, QStatusBar asks for it to be drawn and
    // lays its actions out.
    friend class QMainWindow;
    friend class QStatusBar;
    static bool isSnackbar(const QWidget *widget);
    static int snackbarHeight(const QStatusBar *bar, int width);
    void drawSnackbar(QPainter *painter, const QStatusBar *bar) const;
    void drawCircularProgress(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    // A bottom navigation bar: QTabBar gives its tabs equal widths and asks for the whole of it to be drawn.
    static bool isNavigationBar(const QWidget *widget);
    void drawNavigationBar(QPainter *painter, const QTabBar *bar) const;

    QColor m_seed;
    Qt::ColorScheme m_scheme = Qt::ColorScheme::Unknown; // Unknown follows the system
    bool m_keyboard = false;                             // the focus was last moved with the keyboard
    bool m_motion = true;
    std::unique_ptr<QtPrivate::Animator> m_animator;
    QPointer<QWidget> m_rippleWidget; // the button that was pressed last, and where
    QPointF m_rippleOrigin;
    bool m_rowPressed = false; // a row of a list, a tab or a row of a menu is held down
};
