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

    setCornerWidget() puts a widget, such as a button or a label, at the left or right end of the bar.

    \differences
    A bar is not a native menu bar, and there is no extension button for titles that do not fit: they are cut off.
    F10 and Alt alone do not move the focus to the bar, the titles have no icons, and separators take no room.
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
    const int left = m_leftCorner && !m_leftCorner->isHidden() ? cornerSize(m_leftCorner).width() : 0;
    const int right = m_rightCorner && !m_rightCorner->isHidden() ? cornerSize(m_rightCorner).width() : 0;
    int x = left + spacing, height = 0;
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
    m_size = m_items.empty() ? QSize(left, 0) : QSize(x - spacing, height);
    m_size.setWidth(m_size.width() + right);
    for (const QWidget *corner : {m_leftCorner.data(), m_rightCorner.data()})
        if (corner && !corner->isHidden())
            m_size.setHeight(std::max(m_size.height(), cornerSize(corner).height()));
    // The titles take the height of the bar, so that they line up with the corner widgets.
    for (Item &item : m_items)
        item.rect.setHeight(m_size.height());
    m_dirty = false;
}

// The size of a corner widget: its size hint, or its size if it has none.
QSize QMenuBar::cornerSize(const QWidget *corner) const {
    return corner->sizeHint().isValid() ? corner->sizeHint() : corner->size();
}

// Puts the corner widgets at the ends of the bar, as high as the bar and centred down.
void QMenuBar::placeCorners() {
    if (QWidget *corner = m_leftCorner.data()) {
        const QSize size = cornerSize(corner);
        const int h = std::min(size.height(), height());
        corner->setGeometry(0, (height() - h) / 2, size.width(), h);
    }
    if (QWidget *corner = m_rightCorner.data()) {
        const QSize size = cornerSize(corner);
        const int h = std::min(size.height(), height());
        corner->setGeometry(width() - size.width(), (height() - h) / 2, size.width(), h);
    }
}

/*!
    \since 6.0

    Returns the widget at the left end of the bar for Qt::TopLeftCorner or Qt::BottomLeftCorner, and at the right
    end for the other \a corner; \c nullptr if there is none.

    \sa setCornerWidget()
*/
QWidget *QMenuBar::cornerWidget(Qt::Corner corner) const {
    return (corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner) ? m_leftCorner.data()
                                                                           : m_rightCorner.data();
}

/*!
    \since 6.0

    Puts \a widget at the left end of the bar for Qt::TopLeftCorner or Qt::BottomLeftCorner, and at the right end
    for the other \a corner. The bar becomes the parent of \a widget; the widget that was there is hidden, not
    deleted. \c nullptr removes the corner widget.

    \code
    auto *status = new QLabel("Connected");
    menuBar()->setCornerWidget(status, Qt::TopRightCorner);
    \endcode

    \sa cornerWidget()
*/
void QMenuBar::setCornerWidget(QWidget *widget, Qt::Corner corner) {
    QPointer<QWidget> &slot =
        (corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner) ? m_leftCorner : m_rightCorner;
    if (slot == widget)
        return;
    if (slot && slot->parentWidget() == this)
        slot->hide();
    slot = widget;
    if (widget) {
        widget->setParent(this);
        widget->show();
    }
    m_dirty = true;
    updateGeometry();
    placeCorners();
    update();
}

/*!
    \reimp
*/
void QMenuBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    placeCorners();
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
    if (m_defaultUp)
        menu->popup(mapToGlobal(QPoint(item.left() + 2, item.top() - menu->sizeHint().height() - 2)));
    else
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
        initStyleOption(&option, item.action);
        option.rect = item.rect;
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

/*!
    \since 6.0

    Fills \a option with the state of the title of \a action, for the style to draw it.
*/
void QMenuBar::initStyleOption(QStyleOptionMenuItem *option, const QAction *action) const {
    if (!option || !action)
        return;
    option->initFrom(this);
    option->rect = actionGeometry(const_cast<QAction *>(action)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    option->text = action->text();
    option->font = font();
    option->state = QStyle::State();
    if (action->isEnabled() && isEnabled())
        option->state |= QStyle::State_Enabled;
    if (action == m_active.data())
        option->state |= QStyle::State_Selected;
}

/*!
    \since 6.0

    Adds a separator to the end of the bar and returns its action. The bar shows no line for it; it only parts
    the titles in actions().
*/
QAction *QMenuBar::addSeparator() { return insertSeparator(nullptr); }

/*!
    \since 6.0

    Inserts a separator before \a before, or at the end if it is \c nullptr, and returns its action.
*/
QAction *QMenuBar::insertSeparator(QAction *before) {
    auto *action = new QAction(this);
    action->setSeparator(true);
    insertAction(before, action);
    return action;
}

/*!
    \fn bool QMenuBar::isDefaultUp() const
    \since 6.0

    Returns whether the menus open upwards, above the bar. The default is \c false.
*/

/*!
    \fn void QMenuBar::setDefaultUp(bool up)
    \since 6.0

    Makes the menus open above the bar if \a up is \c true, as for a bar at the bottom of a window.
*/

/*!
    \fn bool QMenuBar::isNativeMenuBar() const
    \since 6.0

    Returns \c false: an Nt menu bar is drawn in its window, not by the platform.
*/

/*!
    \fn void QMenuBar::setNativeMenuBar(bool nativeMenuBar)
    \since 6.0

    Would make the bar the menu bar of the platform if \a nativeMenuBar is \c true. Nt has none, so the bar stays
    in its window.
*/

#include "moc_qmenubar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
