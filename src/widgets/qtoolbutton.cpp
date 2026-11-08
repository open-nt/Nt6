// MIT License - see LICENSE
#include <QtWidgets/qtoolbutton.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
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
    There is no icon, so there is no \c setIcon() or \c setIconSize(), and toolButtonStyle() changes nothing; a button
    shows a text, or an arrow if \c arrowType() is not \c Qt::NoArrow. There is no menu (\c setMenu(),
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
    const QSize contents =
        m_arrow != Qt::NoArrow ? QSize(10, 16) : QSize(fontMetrics().horizontalAdvance(text()), fontMetrics().height());
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
    } else if (!text().isEmpty()) {
        painter.setPen(palette().color(group, QPalette::ButtonText));
        painter.drawText(content, Qt::AlignCenter | Qt::TextSingleLine, text());
    }
}

#include "moc_qtoolbutton.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
