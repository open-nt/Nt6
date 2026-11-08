// MIT License - see LICENSE
#include <QtWidgets/qtoolbar.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qwidgetaction.h>

#include <algorithm>

namespace {

constexpr int kHandleExtent = 8; // the grip of a movable tool bar
constexpr int kSeparatorExtent = 7;

// The line between groups of actions.
class ToolBarSeparator : public QWidget {
public:
    explicit ToolBarSeparator(QToolBar *bar) : QWidget(bar), m_bar(bar) {}

    QSize sizeHint() const override {
        return m_bar->orientation() == Qt::Horizontal ? QSize(kSeparatorExtent, 16) : QSize(16, kSeparatorExtent);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        painter.setPen(palette().color(group, QPalette::Mid));
        if (m_bar->orientation() == Qt::Horizontal)
            painter.drawLine(width() / 2, 3, width() / 2, height() - 4);
        else
            painter.drawLine(3, height() / 2, width() - 4, height() / 2);
    }

private:
    QToolBar *m_bar;
};

} // namespace

/*!
    \class QToolBar
    \brief The QToolBar class provides a bar of buttons for actions.

    A tool bar shows its actions as tool buttons, in a row or a column, usually at the top of a QMainWindow under the
    menu bar. Add actions, separators and other widgets:

    \code
    QToolBar *file = window->addToolBar("File");
    file->addAction(newAction);
    file->addAction(openAction);
    file->addSeparator();
    file->addWidget(new QComboBox);
    \endcode

    Each action gets a QToolButton that follows its text, checked state, enabled state and visibility. A triggered
    action is reported by actionTriggered(). toggleViewAction() is a checkable action that shows and hides the tool
    bar, for a menu of the tool bars of a window.

    \differences
    A tool bar cannot be dragged to another side of the window or float (\c isFloating() is always \c false), and it
    has no extension button for the actions that do not fit. Buttons have no icons, so \c iconSize() is missing and
    they show their text whatever the style. A movable tool bar shows a grip that does nothing.
    \enddifferences

    \sa QMainWindow, QToolButton, QAction
*/

/*!
    Constructs a tool bar with the given window \a title, which names it in toggleViewAction(), and \a parent.
*/
QToolBar::QToolBar(const QString &title, QWidget *parent) : QWidget(parent) { init(title); }

/*!
    Constructs a tool bar with the given \a parent.
*/
QToolBar::QToolBar(QWidget *parent) : QWidget(parent) { init(QString()); }

/*!
    Destroys the tool bar.
*/
QToolBar::~QToolBar() {
    // The widgets of widget actions go back to their actions; the other items go with the children.
    for (const Item &item : m_items)
        if (item.fromAction)
            static_cast<QWidgetAction *>(item.action)->releaseWidget(item.widget);
    m_items.clear();
}

void QToolBar::init(const QString &title) {
    setWindowTitle(title);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    m_layout->setContentsMargins(2 + handleExtent(), 2, 2, 2);
    m_layout->setSpacing(2);
    m_toggleView = new QAction(title, this);
    m_toggleView->setCheckable(true);
    m_toggleView->setChecked(true);
    QObject::connect(m_toggleView, &QAction::triggered, this, [this](bool checked) { setVisible(checked); });
    QObject::connect(this, &QWidget::windowTitleChanged, m_toggleView, &QAction::setText);
}

int QToolBar::handleExtent() const { return m_movable ? kHandleExtent : 0; }

/*!
    Sets whether the user may move the tool bar to \a movable. A movable tool bar shows a grip at its start.

    \sa isMovable()
*/
void QToolBar::setMovable(bool movable) {
    if (m_movable == movable)
        return;
    m_movable = movable;
    relayout();
    emit movableChanged(movable);
}

/*!
    \fn bool QToolBar::isMovable() const

    Returns whether the user may move the tool bar. The default is \c true.

    \sa setMovable()
*/

/*!
    Sets the areas of a QMainWindow that the tool bar may go in to \a areas.

    \sa allowedAreas(), isAreaAllowed()
*/
void QToolBar::setAllowedAreas(Qt::ToolBarAreas areas) {
    if (m_allowedAreas == areas)
        return;
    m_allowedAreas = areas;
    emit allowedAreasChanged(areas);
}

/*!
    \fn Qt::ToolBarAreas QToolBar::allowedAreas() const

    Returns the areas of a QMainWindow that the tool bar may go in, Qt::AllToolBarAreas by default.

    \sa setAllowedAreas()
*/

/*!
    \fn bool QToolBar::isAreaAllowed(Qt::ToolBarArea area) const

    Returns whether the tool bar may go in \a area.
*/

/*!
    Sets whether the buttons are in a row or a column to \a orientation. QMainWindow sets it from the area the tool
    bar is in.

    \sa orientation()
*/
void QToolBar::setOrientation(Qt::Orientation orientation) {
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    m_layout->setDirection(orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred));
    relayout();
    emit orientationChanged(orientation);
}

/*!
    \fn Qt::Orientation QToolBar::orientation() const

    Returns whether the buttons are in a row or a column, Qt::Horizontal by default.

    \sa setOrientation()
*/

/*!
    Removes all the actions of the tool bar.
*/
void QToolBar::clear() {
    const QList<QAction *> all = actions();
    for (QAction *action : all)
        removeAction(action);
}

/*!
    Creates an action with the given \a text, adds it to the tool bar and returns it. The tool bar owns the action.
*/
QAction *QToolBar::addAction(const QString &text) {
    auto *action = new QAction(text, this);
    addAction(action);
    return action;
}

/*!
    \fn template <typename Functor> QAction *QToolBar::addAction(const QString &text, Functor functor)

    \overload

    Creates an action with the given \a text that calls \a functor when it is triggered, adds it and returns it.
*/

/*!
    Adds a separator after the actions and returns its action.

    \sa insertSeparator()
*/
QAction *QToolBar::addSeparator() { return insertSeparator(nullptr); }

/*!
    Inserts a separator before the action \a before, or at the end if it is \c nullptr, and returns its action.
*/
QAction *QToolBar::insertSeparator(QAction *before) {
    auto *action = new QAction(this);
    action->setSeparator(true);
    insertAction(before, action);
    return action;
}

/*!
    Adds \a widget, such as a combo box, after the actions and returns the action that stands for it. Hiding the
    action hides the widget. The tool bar takes the widget as a child.

    \sa insertWidget(), widgetForAction()
*/
QAction *QToolBar::addWidget(QWidget *widget) { return insertWidget(nullptr, widget); }

/*!
    Inserts \a widget before the action \a before, or at the end if it is \c nullptr, and returns the action that
    stands for it.
*/
QAction *QToolBar::insertWidget(QAction *before, QWidget *widget) {
    auto *action = new QAction(this);
    m_widgetActions.emplace_back(action, widget);
    insertAction(before, action);
    return action;
}

/*!
    Returns the geometry of the widget of \a action in the tool bar, or an empty rectangle if it has none.
*/
QRect QToolBar::actionGeometry(QAction *action) const {
    QWidget *widget = widgetForAction(action);
    return widget ? widget->geometry() : QRect();
}

/*!
    Returns the action whose widget is at \a p, a point in the tool bar, or \c nullptr.
*/
QAction *QToolBar::actionAt(const QPoint &p) const {
    for (const Item &item : m_items)
        if (!item.widget->isHidden() && item.widget->geometry().contains(p))
            return item.action;
    return nullptr;
}

/*!
    \fn QAction *QToolBar::actionAt(int x, int y) const

    \overload

    Returns the action whose widget is at \a x, \a y.
*/

/*!
    \fn QAction *QToolBar::toggleViewAction() const

    Returns a checkable action that shows and hides the tool bar, with the window title of the tool bar as its text.
*/

/*!
    \fn Qt::ToolButtonStyle QToolBar::toolButtonStyle() const

    Returns the style of the buttons, Qt::ToolButtonIconOnly by default.

    \sa setToolButtonStyle()
*/

/*!
    Returns the widget of \a action: its tool button, the widget given to addWidget(), or its separator. Returns
    \c nullptr for an action that is not in the tool bar.
*/
QWidget *QToolBar::widgetForAction(QAction *action) const {
    for (const Item &item : m_items)
        if (item.action == action)
            return item.widget;
    return nullptr;
}

/*!
    \fn bool QToolBar::isFloatable() const

    Returns whether the tool bar may float. The default is \c true, but tool bars do not float in Nt.

    \sa setFloatable()
*/

/*!
    \fn void QToolBar::setFloatable(bool floatable)

    Sets whether the tool bar may float to \a floatable.
*/

/*!
    \fn bool QToolBar::isFloating() const

    Returns whether the tool bar floats; always \c false in Nt.
*/

/*!
    Sets the style of the buttons to \a toolButtonStyle. A QMainWindow passes its own style on to its tool bars.

    \sa toolButtonStyle()
*/
void QToolBar::setToolButtonStyle(Qt::ToolButtonStyle toolButtonStyle) {
    if (m_style == toolButtonStyle)
        return;
    m_style = toolButtonStyle;
    for (const Item &item : m_items)
        if (auto *button = dynamic_cast<QToolButton *>(item.widget); button && item.owned)
            button->setToolButtonStyle(toolButtonStyle);
    emit toolButtonStyleChanged(toolButtonStyle);
}

/*!
    \reimp
*/
QSize QToolBar::sizeHint() const { return m_layout->sizeHint(); }

/*!
    \reimp
*/
QSize QToolBar::minimumSizeHint() const { return m_layout->minimumSize(); }

QWidget *QToolBar::createItemWidget(QAction *action, bool *fromAction) {
    *fromAction = false;
    for (const auto &pair : m_widgetActions)
        if (pair.first == action && pair.second)
            return pair.second;
    if (action->isSeparator())
        return new ToolBarSeparator(this);
    if (auto *widgetAction = dynamic_cast<QWidgetAction *>(action))
        if (QWidget *w = widgetAction->requestWidget(this)) {
            *fromAction = true;
            return w;
        }
    auto *button = new QToolButton(this);
    button->setAutoRaise(true);
    button->setToolButtonStyle(m_style);
    button->setDefaultAction(action);
    QObject::connect(button, &QToolButton::triggered, this, [this](QAction *a) { emit actionTriggered(a); });
    return button;
}

void QToolBar::syncVisibility(const Item &item) {
    if (item.action->isVisible())
        item.widget->show();
    else
        item.widget->hide();
}

/*!
    \reimp

    The tool bar makes a button, a separator or a widget for each action, in the order of actions().
*/
void QToolBar::actionEvent(QActionEvent *event) {
    QAction *action = event->action();
    if (!action) {
        QWidget::actionEvent(event);
        return;
    }
    switch (event->type()) {
    case QEvent::ActionAdded: {
        auto it = std::find_if(m_items.begin(), m_items.end(), [action](const Item &i) { return i.action == action; });
        if (it == m_items.end()) {
            bool fromAction = false;
            QWidget *widget = createItemWidget(action, &fromAction);
            const bool owned = !fromAction && std::none_of(m_widgetActions.begin(), m_widgetActions.end(),
                                                           [action](const auto &pair) { return pair.first == action; });
            if (!owned)
                widget->setParent(this);
            m_items.push_back({action, widget, owned, fromAction});
            syncVisibility(m_items.back());
        }
        relayout();
        break;
    }
    case QEvent::ActionChanged:
        for (const Item &item : m_items)
            if (item.action == action)
                syncVisibility(item);
        break;
    case QEvent::ActionRemoved: {
        auto it = std::find_if(m_items.begin(), m_items.end(), [action](const Item &i) { return i.action == action; });
        if (it != m_items.end()) {
            const Item item = *it;
            m_items.erase(it);
            m_layout->removeWidget(item.widget);
            if (item.owned) {
                delete item.widget;
            } else if (item.fromAction) {
                static_cast<QWidgetAction *>(action)->releaseWidget(item.widget);
            } else {
                item.widget->hide();
                item.widget->setParent(nullptr);
            }
        }
        m_widgetActions.erase(
            std::remove_if(m_widgetActions.begin(), m_widgetActions.end(),
                           [action](const auto &pair) { return pair.first == action || !pair.first; }),
            m_widgetActions.end());
        relayout();
        break;
    }
    default:
        break;
    }
    QWidget::actionEvent(event);
}

// Puts the widgets in the layout in the order of the actions.
void QToolBar::relayout() {
    while (QLayoutItem *item = m_layout->takeAt(0))
        delete item;
    const QList<QAction *> order = actions();
    for (QAction *action : order)
        if (QWidget *widget = widgetForAction(action))
            m_layout->addWidget(widget);
    m_layout->addStretch();
    if (m_orientation == Qt::Horizontal)
        m_layout->setContentsMargins(2 + handleExtent(), 2, 2, 2);
    else
        m_layout->setContentsMargins(2, 2 + handleExtent(), 2, 2);
    updateGeometry();
    update();
}

/*!
    \reimp
*/
void QToolBar::paintEvent(QPaintEvent *event) {
    (void)event;
    if (!m_movable)
        return;
    // The grip: two columns of dots at the start.
    QPainter painter(this);
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    painter.setPen(palette().color(group, QPalette::Mid));
    if (m_orientation == Qt::Horizontal) {
        for (int y = 5; y < height() - 4; y += 3) {
            painter.drawPoint(3, y);
            painter.drawPoint(6, y);
        }
    } else {
        for (int x = 5; x < width() - 4; x += 3) {
            painter.drawPoint(x, 3);
            painter.drawPoint(x, 6);
        }
    }
}

/*!
    \reimp
*/
void QToolBar::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    m_toggleView->setChecked(true);
    emit visibilityChanged(true);
}

/*!
    \reimp
*/
void QToolBar::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (isHidden())
        m_toggleView->setChecked(false);
    emit visibilityChanged(false);
}

#include "moc_qtoolbar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
