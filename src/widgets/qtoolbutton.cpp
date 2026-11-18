// MIT License - see LICENSE
#include <QtWidgets/qtoolbutton.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qstyleoption.h>

/*!
    \class QToolButton
    \brief The QToolButton class is a small button for a command, usually with a short text or an arrow.

    A tool button is for a place where a push button would be too big: a row of commands, or a step in a spin control.
    It shows its text or an arrow, and is a push button, a toggle button if it is checkable, and with setAutoRaise() it is
    flat until the mouse is over it:

    \code
    QToolButton *open = new QToolButton(window);
    open->setText("Open");
    open->setAutoRaise(true);
    connect(open, &QToolButton::clicked, [] { openFile(); });
    \endcode

    Tool buttons: a text button, a pressed one, a checked one, a disabled one, one with automatic raise, and the four arrows.

    \image widgets/qtoolbutton.png QToolButton in the light color scheme

    \differences
    Qt::ToolButtonFollowStyle shows the icon only, as Qt::ToolButtonIconOnly. A button without an icon shows its
    text whatever the style, or an arrow if \c arrowType() is not \c Qt::NoArrow. There is no menu (\c setMenu(),
    \c setPopupMode()) and no auto-repeat. The button can be reached with the Tab key, but does not take the focus when
    clicked, as in Qt.
    \enddifferences

    \sa QPushButton, QAbstractButton
*/

/*!
    Constructs a tool button with the given \a parent. It has no text.
*/
QToolButton::QToolButton(QWidget *parent) : QAbstractButton(parent) {
    setFocusPolicy(Qt::TabFocus);
    m_popupTimer.setSingleShot(true);
    m_popupTimer.setInterval(600);
    QObject::connect(&m_popupTimer, &QTimer::timeout, this, [this] {
        if (isDown())
            showMenu();
    });
    QObject::connect(this, &QAbstractButton::released, this, [this] { m_popupTimer.stop(); });
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // A click triggers the default action, which then sets the checked state of the button.
    QObject::connect(this, &QAbstractButton::clicked, this, [this] {
        if (m_defaultAction && m_defaultAction->isEnabled())
            m_defaultAction->trigger();
    });
}

/*!
    Destroys the button.
*/
QToolButton::~QToolButton() = default;

/*!
    \fn bool QToolButton::autoRaise() const

    Returns whether the button looks flat until the mouse is over it, or it is pressed. The default is \c false.

    \sa setAutoRaise()
*/

/*!
    Sets whether the button looks flat until the mouse is over it to \a enable.

    \sa autoRaise()
*/
void QToolButton::setAutoRaise(bool enable) {
    if (m_autoRaise == enable)
        return;
    m_autoRaise = enable;
    update();
}

/*!
    \fn Qt::ArrowType QToolButton::arrowType() const

    Returns the arrow that the button shows in place of its text. The default is \c Qt::NoArrow.

    \sa setArrowType()
*/

/*!
    \fn Qt::ToolButtonStyle QToolButton::toolButtonStyle() const

    Returns whether the button shows its icon, its text or both. The default is Qt::ToolButtonIconOnly.

    \sa setToolButtonStyle()
*/

/*!
    Sets whether the button shows its icon, its text or both to \a style. Buttons have no icons in Nt, so they show
    their text whatever the style.

    \sa toolButtonStyle()
*/
void QToolButton::setToolButtonStyle(Qt::ToolButtonStyle style) {
    if (m_style == style)
        return;
    m_style = style;
    updateGeometry();
    update();
}

/*!
    \fn QAction *QToolButton::defaultAction() const

    Returns the default action of the button, or \c nullptr.

    \sa setDefaultAction()
*/

/*!
    Makes \a action the default action of the button: the button takes the text, tool tip, checkable and checked state
    and enabled state of the action and follows their changes, and clicking the button triggers the
    action. The action is added to the actions of the button.

    \sa defaultAction(), triggered()
*/
void QToolButton::setDefaultAction(QAction *action) {
    if (m_defaultAction == action)
        return;
    QObject::disconnect(m_triggered);
    m_defaultAction = action;
    if (!action)
        return;
    if (!actions().contains(action))
        addAction(action);
    m_triggered = QObject::connect(action, &QAction::triggered, this, [this, action] { emit triggered(action); });
    syncAction();
}

void QToolButton::syncAction() {
    QAction *action = m_defaultAction;
    if (!action)
        return;
    setText(action->iconText());
    setIcon(action->icon());
    setToolTip(action->toolTip());
    setCheckable(action->isCheckable());
    if (action->isCheckable() && isChecked() != action->isChecked()) {
        // Follow the action without the rules of exclusive groups: the action group decides.
        const bool exclusive = autoExclusive();
        setAutoExclusive(false);
        setChecked(action->isChecked());
        setAutoExclusive(exclusive);
    }
    setEnabled(action->isEnabled());
}

/*!
    \reimp
*/
void QToolButton::actionEvent(QActionEvent *event) {
    if (event->type() == QEvent::ActionChanged && event->action() == m_defaultAction)
        syncAction();
    if (event->type() == QEvent::ActionRemoved && event->action() == m_defaultAction) {
        QObject::disconnect(m_triggered);
        m_defaultAction = nullptr;
    }
    QAbstractButton::actionEvent(event);
}

/*!
    Sets the arrow that the button shows in place of its text to \a type.

    \sa arrowType()
*/
void QToolButton::setArrowType(Qt::ArrowType type) {
    if (m_arrow == type)
        return;
    m_arrow = type;
    updateGeometry();
    update();
}

/*!
    \reimp
*/
QSize QToolButton::sizeHint() const {
    QStyleOptionButton option;
    option.initFrom(this);
    QSize contents =
        m_arrow != Qt::NoArrow ? QSize(10, 16) : QSize(fontMetrics().horizontalAdvance(text()), fontMetrics().height());
    if (m_arrow == Qt::NoArrow && showsIcon()) {
        const QSize icon = iconSize();
        const QSize text = showsText() ? contents : QSize(0, 0);
        if (m_style == Qt::ToolButtonTextUnderIcon)
            contents =
                QSize(std::max(icon.width(), text.width()), icon.height() + (showsText() ? text.height() + 2 : 0));
        else
            contents =
                QSize(icon.width() + (showsText() ? text.width() + 4 : 0), std::max(icon.height(), text.height()));
    }
    return style()->sizeFromContents(QStyle::CT_ToolButton, &option, contents, this);
}

/*!
    \reimp
*/
void QToolButton::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    QStyleOptionButton option;
    option.initFrom(this);
    option.text = text();
    if (m_autoRaise)
        option.features |= QStyleOptionButton::Flat;
    option.state |= isDown() ? QStyle::State_Sunken : QStyle::State_Raised;
    if (isChecked())
        option.state |= QStyle::State_On;
    style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, &painter, this);
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const bool pressed = option.state & (QStyle::State_Sunken | QStyle::State_On);
    QRect content = rect();
    if (activeMenu() && m_popupMode == MenuButtonPopup) {
        // The part at the right that opens the menu, with its arrow.
        QStyleOption arrow = option;
        arrow.rect = menuArrowRect().adjusted(2, 0, -2, 0);
        style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrow, &painter, this);
        content.setRight(menuArrowRect().left() - 1);
    }
    if (pressed)
        content.translate(0, 1);
    if (m_arrow != Qt::NoArrow) {
        QStyleOption arrow = option;
        arrow.rect = content;
        QStyle::PrimitiveElement element = QStyle::PE_IndicatorArrowDown;
        switch (m_arrow) {
        case Qt::UpArrow:
            element = QStyle::PE_IndicatorArrowUp;
            break;
        case Qt::LeftArrow:
            element = QStyle::PE_IndicatorArrowLeft;
            break;
        case Qt::RightArrow:
            element = QStyle::PE_IndicatorArrowRight;
            break;
        default:
            break;
        }
        style()->drawPrimitive(element, &arrow, &painter, this);
    } else if (showsIcon()) {
        const QSize size = iconSize();
        const QIcon::Mode mode = !isEnabled() ? QIcon::Disabled : QIcon::Normal;
        const QIcon::State state = isChecked() ? QIcon::On : QIcon::Off;
        painter.setPen(palette().color(group, QPalette::ButtonText));
        const int textWidth = showsText() ? fontMetrics().horizontalAdvance(text()) : 0;
        if (!showsText()) {
            icon().paint(&painter, content, Qt::AlignCenter, mode, state);
        } else if (m_style == Qt::ToolButtonTextUnderIcon) {
            const int total = size.height() + 2 + fontMetrics().height();
            const QRect iconRect(content.left(), content.top() + (content.height() - total) / 2, content.width(),
                                 size.height());
            icon().paint(&painter, iconRect, Qt::AlignCenter, mode, state);
            painter.drawText(QRect(content.left(), iconRect.bottom() + 3, content.width(), fontMetrics().height()),
                             Qt::AlignCenter | Qt::TextSingleLine, text());
        } else {
            const int left = content.left() + (content.width() - size.width() - 4 - textWidth) / 2;
            icon().paint(&painter, QRect(left, content.top(), size.width(), content.height()), Qt::AlignCenter, mode,
                         state);
            painter.drawText(QRect(left + size.width() + 4, content.top(), textWidth + 1, content.height()),
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
        }
    } else if (!text().isEmpty()) {
        painter.setPen(palette().color(group, QPalette::ButtonText));
        painter.drawText(content, Qt::AlignCenter | Qt::TextSingleLine, text());
    }
}

/*!
    \enum QToolButton::ToolButtonPopupMode
    \since 6.0

    How the button opens its menu.

    \value DelayedPopup When the button is held down for a moment; a click triggers the button.
    \value MenuButtonPopup With an arrow at the right of the button, which opens the menu.
    \value InstantPopup When the button is pressed; the button is not triggered.
*/

QMenu *QToolButton::activeMenu() const {
    if (m_menu)
        return static_cast<QMenu *>(m_menu.data());
    return m_defaultAction ? m_defaultAction->menu<QMenu *>() : nullptr;
}

QRect QToolButton::menuArrowRect() const {
    const int width = std::min(14, this->width() / 2);
    return QRect(this->width() - width, 0, width, height());
}

/*!
    \since 6.0

    Returns the menu of the button, or \c nullptr.
*/
QMenu *QToolButton::menu() const { return static_cast<QMenu *>(m_menu.data()); }

/*!
    \since 6.0

    Makes the button open \a menu as popupMode() says; \c nullptr removes it. The button does not take ownership of
    the menu. Without a menu of its own, the button opens the menu of its default action.
*/
void QToolButton::setMenu(QMenu *menu) {
    m_menu = menu;
    updateGeometry();
    update();
}

/*!
    \fn QToolButton::ToolButtonPopupMode QToolButton::popupMode() const
    \since 6.0

    Returns how the button opens its menu, DelayedPopup by default.
*/

/*!
    \since 6.0

    Sets how the button opens its menu to \a mode.
*/
void QToolButton::setPopupMode(ToolButtonPopupMode mode) {
    m_popupMode = mode;
    updateGeometry();
    update();
}

/*!
    \since 6.0

    Opens the menu of the button below it. The button stays down while the menu is open.
*/
void QToolButton::showMenu() {
    QMenu *m = activeMenu();
    if (!m)
        return;
    m_popupTimer.stop();
    setDown(true);
    QPointer<QToolButton> guard(this);
    QObject::connect(
        m, &QMenu::aboutToHide, this,
        [guard] {
            if (guard)
                guard->setDown(false);
        },
        Qt::SingleShotConnection);
    QObject::connect(
        m, &QMenu::triggered, this, [this](QAction *action) { emit triggered(action); }, Qt::SingleShotConnection);
    m->popup(mapToGlobal(QPoint(0, height())));
}

/*!
    \reimp

    With a menu, InstantPopup opens it at once, MenuButtonPopup when the press is on the arrow, and DelayedPopup
    when the button is held down.
*/
void QToolButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && activeMenu()) {
        if (m_popupMode == InstantPopup || (m_popupMode == MenuButtonPopup && menuArrowRect().contains(event->pos()))) {
            showMenu();
            event->accept();
            return;
        }
        if (m_popupMode == DelayedPopup)
            m_popupTimer.start();
    }
    QAbstractButton::mousePressEvent(event);
}

/*!
    \since 6.0

    Fills \a option with the state of the button, for the style to draw it.
*/
void QToolButton::initStyleOption(QStyleOptionToolButton *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->text = text();
    option->icon = icon();
    option->iconSize = iconSize();
    option->arrowType = m_arrow;
    option->toolButtonStyle = m_style;
    option->font = font();
    if (m_arrow != Qt::NoArrow)
        option->features |= QStyleOptionToolButton::Arrow;
    if (activeMenu()) {
        option->features |= QStyleOptionToolButton::HasMenu;
        if (m_popupMode == MenuButtonPopup)
            option->features |= QStyleOptionToolButton::MenuButtonPopup;
        else if (m_popupMode == DelayedPopup)
            option->features |= QStyleOptionToolButton::PopupDelay;
    }
    option->state |= isDown() ? QStyle::State_Sunken : QStyle::State_Raised;
    if (isChecked())
        option->state |= QStyle::State_On;
    if (m_autoRaise)
        option->state |= QStyle::State_AutoRaise;
}

#include "moc_qtoolbutton.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
