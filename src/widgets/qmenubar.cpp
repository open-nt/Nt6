// MIT License - see LICENSE
#include <QtWidgets/qmenubar.h>

#include "qwidget_p.h"

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

using QtPrivate::WidgetAccess;

namespace {

QString plainTitle(const QString &text) {
    QString plain;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == '&' && i + 1 < text.size())
            ++i;
        plain += text.at(i);
    }
    return plain;
}

} // namespace

/*!
    \class QMenuBar
    \brief The QMenuBar class provides a row of menu titles, usually at the top of a window.

    A menu bar shows the title of each of its menus. Clicking a title opens the menu below it; while one is open, moving
    over another title opens that one instead, and Left and Right in a menu go to the one next to it. A title that has
    no menu is a button that triggers its action. Put the bar at the top of the layout of the window:

    \code
    QMenuBar *bar = new QMenuBar(window);
    QMenu *file = bar->addMenu("&File");
    file->addAction("&Open...", [] { openFile(); });
    file->addSeparator();
    file->addAction("&Quit", [] { QApplication::quit(); });
    bar->addMenu("&Edit")->addAction("&Copy");
    layout->addWidget(bar);
    \endcode

    Alt and the letter after a \c{&} in a title opens its menu.

    \image widgets/qmenubar.png QMenuBar in the light color scheme

    \differences
    The bar does not become the menu bar of a window by itself, as the bar of a \c QMainWindow does: there is no
    \c QMainWindow, and no \c QLayout::setMenuBar(), so the bar is added to a layout like a widget. A bar is not a native
    menu bar, and there is no extension button for titles that do not fit: they are cut off. F10 and Alt alone do not move
    the focus to the bar, the titles have no icons, and the bar cannot be \c setCornerWidget()ed or \c setDefaultUp().
    \enddifferences

    \sa QMenu, QAction
*/

/*!
    Constructs a menu bar with the given \a parent.
*/
QMenuBar::QMenuBar(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
}

/*!
    Destroys the menu bar.
*/
QMenuBar::~QMenuBar() = default;

/*!
    Creates an action with the given \a text, adds it at the end of the bar, and returns it. The bar owns the action. An
    action with no menu is a title that triggers when it is clicked.

    \sa addMenu()
*/
QAction *QMenuBar::addAction(const QString &text) {
    auto *action = new QAction(text, this);
    QWidget::addAction(action);
    return action;
}

/*!
    \fn template <typename Functor> QAction *QMenuBar::addAction(const QString &text, Functor functor)

    Creates an action with the given \a text, connects its triggered() signal to \a functor, adds the action at the end of
    the bar, and returns it.
*/

/*!
    Adds \a menu at the end of the bar, and returns its action. The title of the menu is the text of the title in the bar.
    The bar does not own the menu.

    \sa insertMenu()
*/
QAction *QMenuBar::addMenu(QMenu *menu) { return insertMenu(nullptr, menu); }

/*!
    Creates a menu with the given \a title, adds it at the end of the bar, and returns it. The bar owns the menu.

    \sa addMenu(QMenu *)
*/
QMenu *QMenuBar::addMenu(const QString &title) {
    auto *menu = new QMenu(title, this);
    QWidget::addAction(menu->menuAction());
    return menu;
}

/*!
    Adds \a menu in front of the action \a before, and returns its action.

    \sa addMenu()
*/
QAction *QMenuBar::insertMenu(QAction *before, QMenu *menu) {
    if (!menu)
        return nullptr;
    insertAction(before, menu->menuAction());
    return menu->menuAction();
}

/*!
    Removes all the actions of the bar. The actions and the menus that the bar owns are deleted.
*/
void QMenuBar::clear() {
    closeMenu();
    for (QAction *action : actions()) {
        removeAction(action);
        QMenu *menu = action->menu<QMenu *>();
        if (menu && menu->parent() == this)
            delete menu;
        else if (action->parent() == this && action->associatedObjects().isEmpty())
            delete action;
    }
}

// The titles: the visible actions, left to right with the space of the style between them and before the first.
QMenu *QMenuBar::openMenu() const { return static_cast<QMenu *>(m_open.data()); }

void QMenuBar::layoutItems() const {
    if (!m_dirty)
        return;
    m_items.clear();
    const QFontMetrics metrics(font());
    const int spacing = style()->pixelMetric(QStyle::PM_MenuBarItemSpacing, nullptr, this);
    int x = spacing, height = 0;
    for (QAction *action : actions()) {
        if (!action->isVisible() || action->isSeparator())
            continue;
        QStyleOptionMenuItem option;
        option.text = action->text();
        option.font = font();
        const QSize size = style()->sizeFromContents(
            QStyle::CT_MenuBarItem, &option,
            QSize(metrics.horizontalAdvance(plainTitle(action->text())), metrics.height()), this);
        m_items.push_back({action, QRect(x, 0, size.width(), size.height())});
        height = size.height();
        x += size.width() + spacing;
    }
    m_size = m_items.empty() ? QSize(0, 0) : QSize(x - spacing, height);
    m_dirty = false;
}

/*!
    \reimp
*/
QSize QMenuBar::sizeHint() const {
    layoutItems();
    return m_size;
}

/*!
    \reimp
*/
QSize QMenuBar::minimumSizeHint() const {
    layoutItems();
    int narrowest = 0;
    for (const Item &item : m_items)
        narrowest = narrowest == 0 ? item.rect.width() : std::min(narrowest, item.rect.width());
    return QSize(narrowest, m_size.height());
}

/*!
    Returns the rectangle of the title of \a action in the bar; an empty rectangle if the action is not shown.

    \sa actionAt()
*/
QRect QMenuBar::actionGeometry(QAction *action) const {
    layoutItems();
    for (const Item &item : m_items)
        if (item.action == action)
            return item.rect;
    return QRect();
}

/*!
    Returns the action whose title is at \a point in the bar; \c nullptr if there is none.

    \sa actionGeometry()
*/
QAction *QMenuBar::actionAt(const QPoint &point) const {
    layoutItems();
    for (const Item &item : m_items)
        if (item.rect.contains(point))
            return item.action;
    return nullptr;
}

/*!
    Highlights the title of \a action, the one that has a menu open or that the mouse is on; \c nullptr for none. This emits
    hovered() but does not open the menu.

    \sa activeAction()
*/
void QMenuBar::setActiveAction(QAction *action) {
    if (action == m_active.data())
        return;
    m_active = action;
    update();
    if (action) {
        emit hovered(action);
        action->hover();
    }
}

/*!
    \fn QAction *QMenuBar::activeAction() const

    Returns the action whose title is highlighted; \c nullptr if none is.

    \sa setActiveAction()
*/

void QMenuBar::popupMenu(QAction *action, bool selectFirst) {
    QMenu *menu = action ? action->menu<QMenu *>() : nullptr;
    if (!menu || !action->isEnabled() || menu->isEmpty())
        return;
    if (m_open.data() == menu && menu->isVisible())
        return;
    closeMenu();
    m_open = menu;
    setActiveAction(action);
    menu->m_bar = this;
    WidgetAccess::data(menu).popupAnchor = this;
    WidgetAccess::data(menu).popupParent = nullptr;
    const QRect item = actionGeometry(action);
    menu->popup(mapToGlobal(QPoint(item.left() + 2, item.top() + item.height() + 2)));
    if (selectFirst && menu->isVisible())
        menu->setCurrent(menu->step(nullptr, 1), false);
}

void QMenuBar::closeMenu() {
    QMenu *menu = openMenu();
    if (menu && menu->isVisible())
        menu->hide();
    m_open = nullptr;
    if (m_active) {
        m_active = nullptr;
        update();
    }
}

// A menu of the bar was hidden: the bar has none open, and no title highlighted.
void QMenuBar::menuClosed(QMenu *menu) {
    if (m_open.data() != menu)
        return;
    m_open = nullptr;
    m_active = nullptr;
    update();
}

// Opens the menu of the next (direction 1) or previous (-1) title, going around, with its first entry current.
void QMenuBar::navigate(QMenu *from, int direction) {
    layoutItems();
    std::vector<QAction *> choices;
    int at = -1;
    for (const Item &item : m_items) {
        if (!item.action->isEnabled() || !item.action->menu<QMenu *>())
            continue;
        if (item.action == from->menuAction())
            at = static_cast<int>(choices.size());
        choices.push_back(item.action);
    }
    if (choices.size() < 2 || at < 0)
        return;
    const int count = static_cast<int>(choices.size());
    popupMenu(choices[static_cast<std::size_t>((at + direction + count) % count)], true);
}

// Opens the menu whose title has the mnemonic Alt and `key`.
bool QMenuBar::activateMnemonic(int key) {
    layoutItems();
    for (const Item &item : m_items)
        if (item.action->isEnabled() && QKeySequence::mnemonic(item.action->text())[0] == (Qt::ALT | key)) {
            if (item.action->menu<QMenu *>())
                popupMenu(item.action, true);
            else
                item.action->trigger();
            return true;
        }
    return false;
}

namespace QtPrivate {

// Alt and a letter open the menu of a menu bar of the window that has the mnemonic.
bool WidgetAccess::activateMnemonic(QWidget *top, int key) {
    if (!top)
        return false;
    if (auto *bar = dynamic_cast<QMenuBar *>(top))
        if (!bar->isHidden() && bar->isEnabled() && bar->activateMnemonic(key))
            return true;
    for (QObject *child : top->children())
        if (auto *widget = dynamic_cast<QWidget *>(child))
            if (!widget->isWindow() && !widget->isHidden() && activateMnemonic(widget, key))
                return true;
    return false;
}

} // namespace QtPrivate

/*!
    \reimp
*/
void QMenuBar::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOption area;
    area.initFrom(this);
    area.rect = rect();
    style()->drawControl(QStyle::CE_MenuBarEmptyArea, &area, &painter, this);
    layoutItems();
    for (const Item &item : m_items) {
        QStyleOptionMenuItem option;
        option.initFrom(this);
        option.rect = item.rect;
        option.text = item.action->text();
        option.font = font();
        option.state = QStyle::State();
        if (item.action->isEnabled() && isEnabled())
            option.state |= QStyle::State_Enabled;
        if (item.action == m_active.data())
            option.state |= QStyle::State_Selected;
        style()->drawControl(QStyle::CE_MenuBarItem, &option, &painter, this);
    }
}

/*!
    \reimp
*/
void QMenuBar::mouseMoveEvent(QMouseEvent *event) {
    QAction *action = actionAt(event->pos());
    if (m_open) {
        // With a menu open, moving to another title opens that menu
        if (action && action != m_active.data() && action->menu<QMenu *>())
            popupMenu(action, false);
    } else {
        setActiveAction(action);
    }
    event->accept();
}

/*!
    \reimp
*/
void QMenuBar::mousePressEvent(QMouseEvent *event) {
    QAction *action = actionAt(event->pos());
    if (!action || !action->isEnabled()) {
        closeMenu();
        m_active = nullptr;
        update();
        event->accept();
        return;
    }
    if (action->menu<QMenu *>()) {
        if (m_open && m_active == action)
            closeMenu();
        else
            popupMenu(action, false);
    } else {
        closeMenu();
        setActiveAction(action);
        action->trigger();
        emit triggered(action);
    }
    event->accept();
}

/*!
    \reimp
*/
void QMenuBar::leaveEvent(QEvent *event) {
    (void)event;
    if (!m_open && m_active) {
        m_active = nullptr;
        update();
    }
}

/*!
    \reimp
*/
void QMenuBar::actionEvent(QActionEvent *event) {
    (void)event;
    m_dirty = true;
    if (m_active) {
        bool present = false;
        for (QAction *action : actions())
            present = present || action == m_active.data();
        if (!present)
            m_active = nullptr;
    }
    updateGeometry();
    update();
}

/*!
    \reimp
*/
void QMenuBar::changeEvent(QEvent *event) {
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
        m_dirty = true;
        updateGeometry();
        update();
        break;
    case QEvent::PaletteChange:
    case QEvent::EnabledChange:
        update();
        break;
    default:
        break;
    }
    QWidget::changeEvent(event);
}

#include "moc_qmenubar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
