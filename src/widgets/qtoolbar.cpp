// MIT License - see LICENSE
#include <QtWidgets/qtoolbar.h>
#include <QtWidgets/qstyle.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qmenu.h>
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
    action is reported by actionTriggered(). When there is not room for all of them, those at the end make way for
    an extension button, which opens a menu of them. toggleViewAction() is a checkable action that shows and hides
    the tool bar, for a menu of the tool bars of a window.

    A tool bar with actions, a separator, a checked and a disabled action, and a combo box.

    \image widgets/qtoolbar.png QToolBar in the light color scheme

    \differences
    A tool bar cannot be dragged to another side of the window or float (\c isFloating() is always \c false). When its
    actions do not fit, the extension button at its end opens a menu of those that did not, where Qt shows them in an
    expanded tool bar; a widget that does not fit (one given to addWidget()) is hidden and has no entry in the menu. A
    movable tool bar shows a grip that does nothing.
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
    delete m_extensionMenu.data(); // it went to the window when it was shown
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
    m_extension = new QToolButton(this);
    m_extension->setObjectName(QStringLiteral("qt_toolbar_ext_button"));
    m_extension->setAutoRaise(true);
    m_extension->hide();
    QObject::connect(m_extension, &QAbstractButton::clicked, this, [this] { showExtensionMenu(); });
    m_toggleView = new QAction(title, this);
    m_toggleView->setCheckable(true);
    m_toggleView->setChecked(true);
    QObject::connect(m_toggleView, &QAction::triggered, this, [this](bool checked) { setVisible(checked); });
    QObject::connect(this, &QWidget::windowTitleChanged, m_toggleView, &QAction::setText);
    // A top app bar of the Material style shows the title.
    QObject::connect(this, &QWidget::windowTitleChanged, this, [this] { update(); });
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
    \overload
    \since 6.0

    Adds an action with \a icon and \a text, and returns it.
*/
QAction *QToolBar::addAction(const QIcon &icon, const QString &text) {
    auto *action = new QAction(icon, text, this);
    addAction(action);
    return action;
}

/*!
    \fn template <typename Functor> QAction *QToolBar::addAction(const QIcon &icon, const QString &text, Functor functor)
    \overload
    \since 6.0

    Adds an action with \a icon and \a text, connects its triggered() signal to \a functor, and returns it.
*/

/*!
    \since 6.0

    Returns the size of the icons of the tool buttons: what setIconSize() set, or the tool bar icon size of the
    style.
*/
QSize QToolBar::iconSize() const {
    if (m_iconSize.isValid())
        return m_iconSize;
    const int extent = style()->pixelMetric(QStyle::PM_ToolBarIconSize, nullptr, this);
    return QSize(extent > 0 ? extent : 24, extent > 0 ? extent : 24);
}

/*!
    \since 6.0

    Sets the size of the icons of the tool buttons to \a iconSize.
*/
void QToolBar::setIconSize(const QSize &iconSize) {
    if (m_iconSize == iconSize)
        return;
    m_iconSize = iconSize;
    for (const Item &item : m_items)
        if (auto *button = dynamic_cast<QToolButton *>(item.widget); button && item.owned)
            button->setIconSize(this->iconSize());
    emit iconSizeChanged(this->iconSize());
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
QSize QToolBar::sizeHint() const {
    // As long as all of the actions need, also while some of them are in the extension menu.
    QSize size = m_overflow.empty() ? m_layout->sizeHint() : m_fullHint;
    // A top app bar or a docked toolbar is as high as the scrolling of what is under it leaves it.
    if (const QMaterialStyle *material = scrollingStyle())
        size.setHeight(material->appBarExtent(this, size.height()));
    return size;
}

/*!
    \reimp
*/
QSize QToolBar::minimumSizeHint() const {
    QSize size = m_layout->minimumSize();
    // One whose actions can go to the extension menu needs room for no more than the extension button (and the FAB at
    // the end of a docked toolbar).
    if (canExtend() && !m_items.empty()) {
        const QMargins margins = m_layout->contentsMargins();
        const bool horizontal = m_orientation == Qt::Horizontal;
        int along = (horizontal ? margins.left() + margins.right() : margins.top() + margins.bottom()) +
                    alongOf(m_extension->sizeHint());
        if (m_dockedFab)
            along += alongOf(m_dockedFab->sizeHint()) + 4;
        if (horizontal)
            size.setWidth(std::min(size.width(), along));
        else
            size.setHeight(std::min(size.height(), along));
    }
    // One that scrolls away needs no more than what is left of it.
    if (scrollingStyle())
        size.setHeight(std::min(size.height(), sizeHint().height()));
    return size;
}

// The Material style of a horizontal top app bar or docked toolbar, whose height follows what scrolls under it; null
// for another toolbar.
const QMaterialStyle *QToolBar::scrollingStyle() const {
    const auto *material = qobject_cast<const QMaterialStyle *>(style());
    return material && m_orientation == Qt::Horizontal &&
                   (QMaterialStyle::isAppBar(this) || QMaterialStyle::isDockedToolBar(this))
               ? material
               : nullptr;
}

/*!
    \reimp
*/
bool QToolBar::event(QEvent *event) {
    const bool done = QWidget::event(event);
    if (event->type() != QEvent::Resize && event->type() != QEvent::LayoutRequest)
        return done;
    // At another length, more or fewer of the actions may fit.
    if (event->type() == QEvent::Resize && canExtend() && (!m_overflow.empty() || m_fullMinimum > alongOf(size())))
        relayout();
    const QMaterialStyle *material = scrollingStyle();
    if (!material)
        return done;
    // While part of it has scrolled away, its buttons are laid out at the whole height and go with the edge that
    // leaves: those of a top app bar up, those of a docked toolbar down.
    const int natural = m_layout->sizeHint().height();
    const int rest = material->toolBarRest(this, natural);
    if (material->toolBarAway(this) > 0.0 && height() < rest)
        m_layout->setGeometry(QRect(0, QMaterialStyle::isAppBar(this) ? height() - rest : 0, width(), rest));
    // The title of a medium or large one may take another line, or one fewer, at a new width.
    if (event->type() == QEvent::Resize && QMaterialStyle::isFlexibleAppBar(this) &&
        material->appBarExtent(this, natural) != height())
        updateGeometry();
    return done;
}

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
    button->setIconSize(iconSize());
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
    case QEvent::ActionChanged: {
        // The spacing of a docked toolbar is between the buttons that are shown, and it keeps its separators hidden.
        bool refit = qobject_cast<const QMaterialStyle *>(style()) && QMaterialStyle::isDockedToolBar(this);
        const bool overflowed = std::find(m_overflow.begin(), m_overflow.end(), action) != m_overflow.end();
        for (const Item &item : m_items) {
            if (item.action != action)
                continue;
            // A button that comes or goes, or one in the extension menu, changes what fits.
            if (overflowed || item.widget->isHidden() == action->isVisible())
                refit = refit || canExtend();
            if (!overflowed)
                syncVisibility(item);
        }
        if (refit)
            relayout();
        break;
    }
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

// True for the floating toolbar of the Material style, which asks for it with a property.
bool QToolBar::isFloatingForStyle() const {
    return qobject_cast<const QMaterialStyle *>(style()) && QMaterialStyle::isFloatingToolBar(this);
}

// True for a tool bar whose actions go to the extension menu when they do not fit: any but a top app bar.
bool QToolBar::canExtend() const {
    return !(qobject_cast<const QMaterialStyle *>(style()) && QMaterialStyle::isAppBar(this));
}

// The length of `size` along the tool bar.
int QToolBar::alongOf(const QSize &size) const {
    return m_orientation == Qt::Horizontal ? size.width() : size.height();
}

// Puts the widgets in the layout in the order of the actions, the first `kept` of those that can go to the extension
// menu (all of them if it is negative) and the extension button if any did. Returns how many can go.
int QToolBar::fillLayout(int kept, bool horizontal, bool floating, bool docked, bool appBar) {
    while (QLayoutItem *item = m_layout->takeAt(0))
        delete item;
    // What did not fit before may now.
    for (QAction *action : m_overflow)
        if (QWidget *widget = widgetForAction(action);
            widget && action->isVisible() && !(docked && action->isSeparator()))
            widget->show();
    m_overflow.clear();
    m_dockedFab = nullptr;
    const QList<QAction *> order = actions();
    // A medium or large app bar keeps its buttons in the top row, above its title.
    const Qt::Alignment align = appBar && QMaterialStyle::isFlexibleAppBar(this) ? Qt::AlignTop : Qt::Alignment();
    bool gap = false;
    if (appBar && std::none_of(order.begin(), order.end(), [](QAction *a) { return a->isSeparator(); })) {
        m_layout->addStretch();
        gap = true;
    }
    const auto space = [&](int extent, QSizePolicy::Policy policy) {
        m_layout->addItem(horizontal ? new QSpacerItem(extent, 0, policy, QSizePolicy::Minimum)
                                     : new QSpacerItem(0, extent, QSizePolicy::Minimum, policy));
    };
    int entries = 0;
    // Whether the next of those that can go to the extension menu stays; one that does not is hidden.
    const auto fits = [&](QAction *action, QWidget *widget) {
        const bool stays = kept < 0 || entries < kept;
        ++entries;
        if (!stays) {
            m_overflow.push_back(action);
            widget->hide();
        }
        return stays;
    };
    if (docked)
        m_layout->addStretch();
    bool first = true;
    for (QAction *action : order) {
        QWidget *widget = widgetForAction(action);
        if (!widget)
            continue;
        if (docked) {
            if (action->isSeparator())
                widget->hide(); // the spacing is what separates the buttons
            if (action->isSeparator() || !action->isVisible())
                continue;
            // A FAB goes at the end, 16 in as the buttons are, and stays there.
            if (!m_dockedFab && QMaterialStyle::isFloatingActionButton(widget)) {
                m_dockedFab = widget;
                continue;
            }
            if (!fits(action, widget))
                continue;
            if (!first) {
                space(4, QSizePolicy::Fixed);
                space(28, QSizePolicy::Maximum);
            }
            first = false;
            m_layout->addWidget(widget);
            continue;
        }
        if (appBar && action->isSeparator() && !gap) {
            m_layout->addStretch();
            widget->hide();
            gap = true;
            continue;
        }
        if (action->isSeparator() && widget->isHidden() && action->isVisible())
            widget->show(); // it was the title's place in an app bar
        if (!fits(action, widget))
            continue;
        m_layout->addWidget(widget, 0, align);
    }
    // The extension button is the last of the buttons of a docked toolbar, and at the end of another tool bar.
    const bool overflow = !m_overflow.empty();
    if (docked && overflow) {
        if (!first) {
            space(4, QSizePolicy::Fixed);
            space(28, QSizePolicy::Maximum);
        }
        m_layout->addWidget(m_extension);
    }
    if (!floating && !appBar)
        m_layout->addStretch();
    if (!docked && overflow)
        m_layout->addWidget(m_extension);
    if (m_dockedFab) {
        space(4, QSizePolicy::Fixed);
        m_layout->addWidget(m_dockedFab);
    }
    m_extension->setVisible(overflow);
    return entries;
}

// Lays the tool bar out for its kind, with as many of its actions as fit.
void QToolBar::relayout() {
    // Buttons that are shown or hidden may lay the window out at once, which resizes the tool bar and asks for this
    // again: that waits, and the tool bar is laid out again if its size changed meanwhile.
    if (m_relayingOut)
        return;
    m_relayingOut = true;
    for (int round = 0; round < 3; ++round) {
        const QSize before = size();
        layOutItems();
        if (size() == before)
            break;
    }
    m_relayingOut = false;
}

void QToolBar::layOutItems() {
    const auto *material = qobject_cast<const QMaterialStyle *>(style());
    const bool floating = isFloatingForStyle();
    // A docked toolbar spreads its buttons out along it, from 4 to 32 apart (DockedToolbarTokens), and centres them
    // where 32 leaves room at the ends.
    const bool docked = material && QMaterialStyle::isDockedToolBar(this);
    // A top app bar has its title where the first separator is, or before every action if there is none.
    const bool appBar = material && QMaterialStyle::isAppBar(this);
    const bool horizontal = m_orientation == Qt::Horizontal;
    // The extension button: under the Material style a standard icon button, or an item of a Material toolbar, with
    // the symbol of the style.
    m_extension->setIconSize(iconSize());
    m_extension->setIcon(style()->standardIcon(horizontal ? QStyle::SP_ToolBarHorizontalExtensionButton
                                                          : QStyle::SP_ToolBarVerticalExtensionButton,
                                               nullptr, m_extension));
    const QVariant iconButton = material && !floating && !docked ? QVariant(QStringLiteral("standard")) : QVariant();
    if (m_extension->property("materialIconButton") != iconButton)
        m_extension->setProperty("materialIconButton", iconButton);
    const int entries = fillLayout(-1, horizontal, floating, docked, appBar);
    m_layout->setSpacing(floating || appBar || docked ? 0 : 2);
    // A floating toolbar is as long as its buttons need; another stretches along its side.
    // A top app bar and a docked toolbar go across all of their room.
    const QSizePolicy::Policy along = floating           ? QSizePolicy::Maximum
                                      : appBar || docked ? QSizePolicy::Expanding
                                                         : QSizePolicy::Preferred;
    setSizePolicy(horizontal ? QSizePolicy(along, QSizePolicy::Fixed) : QSizePolicy(QSizePolicy::Fixed, along));
    for (const Item &item : m_items)
        if (item.owned)
            item.widget->updateGeometry(); // its size depends on the toolbar that it is in
    if (appBar)
        m_layout->setContentsMargins(4, 8, 4, 8); // 64 high with buttons of 48
    else if (docked && m_dockedFab)               // 64 across with a FAB of 56
        m_layout->setContentsMargins(horizontal ? QMargins(16, 4, 16, 4) : QMargins(4, 16, 4, 16));
    else if (docked) // 64 across with buttons of 48, and 16 at the ends
        m_layout->setContentsMargins(horizontal ? QMargins(16, 8, 16, 8) : QMargins(8, 16, 8, 16));
    else if (floating)
        // 64 across with buttons of 48, and room below for the shadow.
        m_layout->setContentsMargins(m_orientation == Qt::Horizontal ? QMargins(4, 8, 4, 10) : QMargins(8, 4, 8, 6));
    else if (m_orientation == Qt::Horizontal)
        m_layout->setContentsMargins(2 + handleExtent(), 2, 2, 2);
    else
        m_layout->setContentsMargins(2, 2 + handleExtent(), 2, 2);
    m_fullHint = m_layout->sizeHint();
    m_fullMinimum = alongOf(m_layout->minimumSize());
    // What does not fit goes to the extension menu, from the end: as many stay as leave room for its button.
    if (canExtend() && m_fullMinimum > alongOf(size())) {
        for (int kept = entries - 1; kept >= 0; --kept) {
            fillLayout(kept, horizontal, floating, docked, appBar);
            if (alongOf(m_layout->minimumSize()) <= alongOf(size()))
                break;
        }
    }
    updateGeometry();
    update();
}

// Opens the menu of the actions that did not fit, from the extension button: below it, or above it at the bottom of
// the window, and beside a vertical tool bar.
void QToolBar::showExtensionMenu() {
    if (!m_extensionMenu) {
        // A choice from it is reported by the button of its action, which follows the action.
        auto *created = new QMenu(this);
        m_extensionMenu = created;
        QObject::connect(created, &QMenu::aboutToHide, m_extension, [this] { m_extension->setDown(false); });
    }
    QMenu *menu = m_extensionMenu;
    menu->clear();
    for (QAction *action : m_overflow) {
        if (action->isSeparator()) {
            menu->addSeparator();
            continue;
        }
        // The buttons of actions; a widget has no entry.
        const auto it =
            std::find_if(m_items.begin(), m_items.end(), [action](const Item &item) { return item.action == action; });
        if (it != m_items.end() && it->owned)
            menu->addAction(action);
    }
    if (menu->isEmpty())
        return;
    menu->ensurePolished(); // the style may give it another font, which its size is measured with
    const QSize size = menu->sizeHint();
    QWidget *window = this->window();
    const QRect button(m_extension->mapTo(window, QPoint(0, 0)), m_extension->size());
    QPoint at;
    if (m_orientation == Qt::Horizontal) {
        at.setX(button.right() + 1 - size.width());
        at.setY(button.bottom() + 1 + size.height() <= window->height() ? button.bottom() + 1
                                                                        : button.top() - size.height());
    } else {
        at.setX(button.right() + 1 + size.width() <= window->width() ? button.right() + 1
                                                                     : button.left() - size.width());
        at.setY(button.top());
    }
    m_extension->setDown(true);
    menu->popup(window->mapToGlobal(at));
}

/*!
    \reimp
*/
void QToolBar::paintEvent(QPaintEvent *event) {
    (void)event;
    if (const auto *material = qobject_cast<const QMaterialStyle *>(style()); material && material->isAppBar(this)) {
        QPainter painter(this);
        material->drawAppBar(&painter, this);
        // A medium or large one, or one that scrolls away, changes its height along a spring: each frame asks for the
        // new one.
        if (material->appBarExtent(this, m_layout->sizeHint().height()) != height())
            updateGeometry();
        return;
    }
    if (const auto *material = qobject_cast<const QMaterialStyle *>(style());
        material && QMaterialStyle::isDockedToolBar(this)) {
        QPainter painter(this);
        material->drawDockedToolBar(&painter, this);
        // One that scrolls away changes its height along a spring, as an app bar does.
        if (scrollingStyle() && material->appBarExtent(this, m_layout->sizeHint().height()) != height())
            updateGeometry();
        return;
    }
    if (isFloatingForStyle()) {
        QPainter painter(this);
        static_cast<const QMaterialStyle *>(style())->drawFloatingToolBar(&painter, this);
        return;
    }
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
    \since 6.0

    Fills \a option with the state of the tool bar: whether it is movable and its orientation as an area.
*/
void QToolBar::initStyleOption(QStyleOptionToolBar *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->features = m_movable ? QStyleOptionToolBar::Movable : QStyleOptionToolBar::None;
    option->toolBarArea = m_orientation == Qt::Horizontal ? Qt::TopToolBarArea : Qt::LeftToolBarArea;
    option->positionOfLine = QStyleOptionToolBar::OnlyOne;
    option->positionWithinLine = QStyleOptionToolBar::OnlyOne;
}

/*!
    \reimp
*/
void QToolBar::changeEvent(QEvent *event) {
    if (event->type() == QEvent::StyleChange)
        relayout();
    QWidget::changeEvent(event);
}

/*!
    \reimp
*/
void QToolBar::showEvent(QShowEvent *event) {
    // A property that makes the toolbar float may have been set since the actions were added, under another style.
    relayout();
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
