// MIT License - see LICENSE
#include <QtWidgets/qmenu.h>

#include "qmaterialmotion_p.h"
#include "qwidget_p.h"

#include <QtCore/qeventloop.h>
#include <QtGui/qactiongroup.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>
#include <climits>

using QtPrivate::WidgetAccess;

namespace {

constexpr int kSubMenuDelay = 225;

// The text of an action as a menu shows it: the mnemonic marks are kept, and the shortcut follows after a tab.
QString textWithShortcut(const QAction *action) {
    QString text = action->text();
    if (!text.contains('\t') && !action->shortcut().isEmpty())
        text += '\t' + action->shortcut().toString(QKeySequence::NativeText);
    return text;
}

// The text without the mnemonic marks and the shortcut, which is what the width of an entry is measured on.
QString plainLabel(const QString &text) {
    const qsizetype tab = text.indexOf('\t');
    const QString label = tab < 0 ? text : text.left(tab);
    QString plain;
    for (qsizetype i = 0; i < label.size(); ++i) {
        if (label.at(i) == '&' && i + 1 < label.size())
            ++i;
        plain += label.at(i);
    }
    return plain;
}

} // namespace

/*!
    \class QMenu
    \brief The QMenu class provides a menu: a list of actions to choose from.

    A menu holds actions, with separators between groups of them, and submenus. It is shown at a place with popup() or
    exec(), or from a QMenuBar or a menu that has it as a submenu, and it closes when the user has chosen an action or
    clicked outside of it. Choosing an action triggers it:

    \code
    QMenu *menu = new QMenu(window);
    QAction *open = menu->addAction("&Open...");
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, window, &Editor::openFile);
    menu->addSeparator();
    QMenu *recent = menu->addMenu("Open &Recent");
    recent->addAction("notes.txt");
    QAction *wrap = menu->addAction("&Wrap lines");
    wrap->setCheckable(true);
    menu->popup(button->mapToGlobal(QPoint(0, button->height())));
    \endcode

    The menu follows its actions: a disabled action is dimmed and cannot be chosen, a hidden one is left out, and a
    checkable one has a check box. The up and down keys move through the entries, Right opens a submenu and Left or Escape
    closes it, Return and Space choose, and the letter after a \c{&} in the text of an action chooses it.

    Menus open and close with the mouse, too: moving over an entry highlights it, moving over a submenu opens it after a
    short delay, and releasing the button over an entry chooses it.

    The menu is a widget inside the window of the widget that it belongs to, not a window of its own, so it never reaches
    outside the window and it is clipped to it. It opens at the place that popup() is given, or to the left of it or above
    it if there is not room.

    \image widgets/qmenu.png QMenu in the light color scheme

    \differences
    There are no tear-off menus (isTearOffEnabled() is only kept), no scrolling of a menu that is taller than the
    window, no \c QMenu::exec() with a list of actions, and no platform menu. Sections are drawn as a label in
    bold, which Qt's Fusion style does not show. The menu is a child of a window, not a window that can reach beyond it:
    popup() reparents the menu to the window of its parent, or to the active window if it has no parent. Shortcuts of the
    actions are shown but are triggered by the window, not by the menu. The menu is not a style-sheet or a platform menu.
    Under QMaterialStyle, a menu whose Nt-only property \c materialMenu is \c "vertical" opens its submenus in its own
    place, on a click, Right or Return, under a row with an arrow and the submenu's title that goes back, as do Left
    and Escape; the submenu emits aboutToShow() and aboutToHide() but is not shown itself.
    \enddifferences

    \sa QMenuBar, QAction, QActionGroup
*/

/*!
    Constructs a menu with the given \a parent. The menu is hidden until it is popped up.
*/
QMenu::QMenu(QWidget *parent) : QWidget(parent) { init(); }

/*!
    Constructs a menu with the given \a title and \a parent. The title is what a menu bar or the menu that has this one
    as a submenu shows.
*/
QMenu::QMenu(const QString &title, QWidget *parent) : QWidget(parent), m_title(title) { init(); }

void QMenu::init() {
    hide();
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    m_menuAction = new QAction(m_title, this);
    m_menuAction->setMenu(this);
    m_subMenuTimer.setSingleShot(true);
    m_subMenuTimer.setInterval(kSubMenuDelay);
    QObject::connect(&m_subMenuTimer, &QTimer::timeout, this, [this] {
        if (m_pendingSubMenu && m_pendingSubMenu == m_active)
            openSubMenu(m_pendingSubMenu.data(), false);
    });
}

/*!
    Destroys the menu.
*/
QMenu::~QMenu() { m_subMenuTimer.stop(); }

/*!
    \fn QString QMenu::title() const

    Returns the title of the menu.

    \sa setTitle()
*/

/*!
    Sets the title of the menu to \a title.

    \sa title()
*/
void QMenu::setTitle(const QString &title) {
    m_title = title;
    m_menuAction->setText(title);
}

/*!
    \fn QAction *QMenu::menuAction() const

    Returns the action that stands for the menu: its text is the title, and its menu is this one. A menu bar or another
    menu that has this menu has this action.

    \sa addMenu()
*/

/*!
    Creates an action with the given \a text, adds it to the end of the menu, and returns it. The menu owns the action.

    \sa addSeparator(), addMenu()
*/
QAction *QMenu::addAction(const QString &text) {
    auto *action = new QAction(text, this);
    QWidget::addAction(action);
    return action;
}

/*!
    \overload
    \since 6.0

    Adds an action with \a icon and \a text to the end of the menu, and returns it.
*/
QAction *QMenu::addAction(const QIcon &icon, const QString &text) {
    QAction *action = addAction(text);
    action->setIcon(icon);
    return action;
}

/*!
    \fn template <typename Functor> QAction *QMenu::addAction(const QIcon &icon, const QString &text, Functor functor)
    \overload
    \since 6.0

    Adds an action with \a icon and \a text, connects its triggered() signal to \a functor, and returns it.
*/

/*!
    \overload
    \since 6.0

    Adds an action with \a text and the \a shortcut, and connects its triggered() signal to the slot \a member of
    \a receiver, written with SLOT(). Returns the action.
*/
QAction *QMenu::addAction(const QString &text, const QObject *receiver, const char *member,
                          const QKeySequence &shortcut) {
    QAction *action = addAction(text);
    action->setShortcut(shortcut);
    QObject::connect(action, SIGNAL(triggered(bool)), receiver, member);
    return action;
}

/*!
    \fn template <typename Receiver, typename Functor, typename> QAction *QMenu::addAction(const QString &text, const Receiver *receiver, Functor functor, const QKeySequence &shortcut)
    \overload
    \since 6.0

    Adds an action with \a text and the \a shortcut, and connects its triggered() signal to \a functor, a slot of
    \a receiver or a callable that runs while \a receiver exists. Returns the action.
*/

/*!
    \fn template <typename Functor> QAction *QMenu::addAction(const QString &text, Functor functor)

    Creates an action with the given \a text, connects its triggered() signal to \a functor, adds the action to the
    end of the menu, and returns it.
*/

/*!
    Adds \a menu as a submenu at the end of this menu, and returns the action of the submenu, which is the same as
    \c{menu->menuAction()}. The menu is not owned by this one.

    \sa insertMenu(), addSeparator()
*/
QAction *QMenu::addMenu(QMenu *menu) {
    if (!menu)
        return nullptr;
    QWidget::addAction(menu->menuAction());
    return menu->menuAction();
}

/*!
    Creates a menu with the given \a title, adds it as a submenu at the end of this menu, and returns it. This menu owns the
    new one.

    \sa addMenu(QMenu *)
*/
QMenu *QMenu::addMenu(const QString &title) {
    auto *menu = new QMenu(title, this);
    QWidget::addAction(menu->menuAction());
    return menu;
}

/*!
    \overload
    \since 6.1

    Creates a menu with the given \a icon and \a title, adds it as a submenu, and returns it.
*/
QMenu *QMenu::addMenu(const QIcon &icon, const QString &title) {
    QMenu *menu = addMenu(title);
    menu->setIcon(icon);
    return menu;
}

/*!
    Adds a separator, a line between groups of entries, at the end of the menu, and returns its action. A separator at the
    start or the end of a menu, and one next to another, is not shown.

    \sa insertSeparator()
*/
QAction *QMenu::addSeparator() { return insertSeparator(nullptr); }

/*!
    Adds \a menu as a submenu in front of the action \a before, and returns the action of the submenu.

    \sa addMenu()
*/
QAction *QMenu::insertMenu(QAction *before, QMenu *menu) {
    if (!menu)
        return nullptr;
    insertAction(before, menu->menuAction());
    return menu->menuAction();
}

/*!
    Adds a separator in front of the action \a before, and returns its action.

    \sa addSeparator()
*/
QAction *QMenu::insertSeparator(QAction *before) {
    auto *action = new QAction(this);
    action->setSeparator(true);
    insertAction(before, action);
    return action;
}

/*!
    Removes all the actions of the menu. The actions that the menu owns and that no other widget has, and the submenus that
    it owns, are deleted.

    \sa isEmpty()
*/
void QMenu::clear() {
    for (QAction *action : actions()) {
        removeAction(action);
        QMenu *submenu = action->menu<QMenu *>();
        if (submenu && submenu->parent() == this)
            delete submenu;
        else if (action->parent() == this && action->associatedObjects().isEmpty())
            delete action;
    }
}

/*!
    Returns \c true if the menu has no entries that can be chosen: no visible actions, apart from separators; otherwise
    returns \c false.
*/
bool QMenu::isEmpty() const {
    for (QAction *action : actions())
        if (action->isVisible() && !action->isSeparator())
            return false;
    return true;
}

// The entries: the visible actions, with the separators at the start and at the end and the doubled ones left out.
void QMenu::layoutItems() const {
    if (!m_dirty)
        return;
    m_items.clear();
    std::vector<QAction *> visible;
    if (!m_inPlace.empty() && m_backAction)
        visible.push_back(m_backAction);
    bool afterSeparator = true;
    for (QAction *action : shownActions()) {
        if (!action->isVisible())
            continue;
        const bool section = action->isSeparator() && !action->text().isEmpty();
        if (action->isSeparator() && !section) {
            if (afterSeparator && m_collapsible)
                continue;
            afterSeparator = true;
        } else if (section) {
            afterSeparator = true;
        } else {
            afterSeparator = false;
        }
        visible.push_back(action);
    }
    while (m_collapsible && !visible.empty() && visible.back()->isSeparator() && visible.back()->text().isEmpty())
        visible.pop_back();

    const QFontMetrics menuMetrics(font());
    int tab = 0, widest = 0;
    std::vector<QSize> sizes;
    for (QAction *action : visible) {
        QStyleOptionMenuItem option;
        option.text = textWithShortcut(action);
        option.icon = action->isIconVisibleInMenu() ? action->icon() : QIcon();
        option.font = action->font() == QFont() ? font() : action->font();
        option.reservedShortcutWidth = tab;
        QSize size;
        if (action->isSeparator() && !action->text().isEmpty()) {
            QFont bold = option.font;
            bold.setBold(true);
            const QFontMetrics metrics(bold);
            size = style()->sizeFromContents(QStyle::CT_MenuItem, &option,
                                             QSize(metrics.horizontalAdvance(action->text()), metrics.height()), this);
        } else if (action->isSeparator()) {
            option.menuItemType = QStyleOptionMenuItem::Separator;
            size = style()->sizeFromContents(QStyle::CT_MenuItem, &option, QSize(0, 2), this);
        } else {
            if (isSubMenuAction(action))
                option.menuItemType = QStyleOptionMenuItem::SubMenu;
            const QFontMetrics metrics(option.font);
            size = style()->sizeFromContents(
                QStyle::CT_MenuItem, &option,
                QSize(metrics.horizontalAdvance(plainLabel(option.text)), metrics.height()), this);
            const qsizetype at = option.text.indexOf('\t');
            if (at >= 0)
                tab = std::max(tab, menuMetrics.horizontalAdvance(option.text.mid(at + 1)));
        }
        widest = std::max(widest, size.width());
        sizes.push_back(size);
    }
    const int width = widest + tab;
    // The style keeps the entries in from the edges of the menu, as Qt's QMenu asks it to.
    const int hmargin = style()->pixelMetric(QStyle::PM_MenuHMargin, nullptr, this);
    const int vmargin = style()->pixelMetric(QStyle::PM_MenuVMargin, nullptr, this);
    int y = vmargin;
    for (std::size_t i = 0; i < visible.size(); ++i) {
        m_items.push_back({visible[i], QRect(hmargin, y, width, sizes[i].height())});
        y += sizes[i].height();
    }
    m_size = m_items.empty() ? QSize() : QSize(width + 2 * hmargin, y + vmargin);
    m_dirty = false;
}

// The part of its window that the menu, `size` large, is kept in: the window less the margins that the style asks
// for, or the whole window along a side where the menu does not fit inside them.
QRect QMenu::windowArea(const QWidget *window, QSize size) const {
    const QMargins margins = QMaterialStyle::menuWindowMargins(this);
    QRect area = window->rect();
    if (size.width() <= area.width() - margins.left() - margins.right())
        area.adjust(margins.left(), 0, -margins.right(), 0);
    if (size.height() <= area.height() - margins.top() - margins.bottom())
        area.adjust(0, margins.top(), 0, -margins.bottom());
    return area;
}

bool QMenu::isSubMenuAction(const QAction *action) const { return action->menu<QMenu *>() != nullptr; }

bool QMenu::opensSubMenusInPlace() const { return QMaterialStyle::opensSubMenusInPlace(this); }

QList<QAction *> QMenu::shownActions() const {
    for (auto it = m_inPlace.rbegin(); it != m_inPlace.rend(); ++it)
        if (*it)
            return (*it)->actions();
    return actions();
}

// Shows the items of `menu` in the place of this menu's, under a row with an arrow and the title of the menu that
// goes back. The submenu is told that it shows and hides as if it opened on its own.
void QMenu::openInPlace(QMenu *menu, bool selectFirst) {
    m_subMenuTimer.stop();
    m_pendingSubMenu = nullptr;
    m_inPlace.emplace_back(menu);
    menu->m_inPlaceHost = this;
    if (!m_backAction)
        m_backAction = new QAction(this);
    m_backAction->setText(menu->title());
    m_backAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack, nullptr, this));
    emit menu->aboutToShow();
    m_active = nullptr;
    const QImage before = rowsPicture();
    relayout();
    slideLevel(before, 1);
    setCurrent(selectFirst ? step(m_backAction, 1) : nullptr, false);
}

// Goes back from the submenu shown in place to what was shown before it, with the submenu's row current.
void QMenu::goBack() {
    if (m_inPlace.empty())
        return;
    const QPointer<QMenu> menu = m_inPlace.back();
    m_inPlace.pop_back();
    if (!m_inPlace.empty() && m_inPlace.back())
        m_backAction->setText(m_inPlace.back()->title());
    m_active = nullptr;
    const QImage before = rowsPicture();
    relayout();
    slideLevel(before, -1);
    if (menu) {
        menu->m_inPlaceHost = nullptr;
        setCurrent(menu->menuAction(), false);
        emit menu->aboutToHide();
    }
}

void QMenu::relayout() {
    m_dirty = true;
    layoutItems();
    if (m_active && std::none_of(m_items.begin(), m_items.end(),
                                 [this](const Item &item) { return item.action == m_active.data(); }))
        m_active = nullptr;
    if (isVisible() && !m_size.isEmpty()) {
        resize(m_size);
        // What grew past the edge of the window goes back into it.
        if (const QWidget *window = parentWidget()) {
            const QRect area = windowArea(window, size());
            const int x = std::max(area.left(), std::min(pos().x(), area.left() + area.width() - width()));
            const int y = std::max(area.top(), std::min(pos().y(), area.top() + area.height() - height()));
            move(x, y);
        }
    }
    update();
}

// The rows as they are now, without the container, to slide out of the menu as another level comes in.
QImage QMenu::rowsPicture() const {
    const auto *material = qobject_cast<const QMaterialStyle *>(style());
    if (!material || !material->isMotionEnabled() || !isVisible() || size().isEmpty())
        return QImage();
    const qreal ratio = devicePixelRatio() > 0 ? devicePixelRatio() : 1.0;
    QImage picture(QSize(qRound(width() * ratio), qRound(height() * ratio)), QImage::Format_ARGB32_Premultiplied);
    picture.setDevicePixelRatio(ratio);
    picture.fill(Qt::transparent);
    QPainter painter(&picture);
    painter.setFont(font());
    // Drawn in the slide's own frame: the level that goes is no longer the menu's.
    for (const Item &item : m_items) {
        QStyleOptionMenuItem option;
        initStyleOption(&option, item.action);
        option.rect = item.rect;
        style()->drawControl(QStyle::CE_MenuItem, &option, &painter, this);
    }
    return picture;
}

// The new level comes in along the fast spatial spring, and the old one goes out ahead of it.
void QMenu::slideLevel(const QImage &before, int direction) {
    QtPrivate::Transition::stop(this, QStringLiteral("_nt_menuLevel"));
    if (before.isNull()) {
        m_levelDirection = 0;
        m_levelPicture = QImage();
        return;
    }
    m_levelPicture = before;
    m_levelDirection = isRightToLeft() ? -direction : direction;
    m_levelProgress = 0;
    QPointer<QMenu> guard(this);
    new QtPrivate::Transition(this, QStringLiteral("_nt_menuLevel"), QtPrivate::fastSpatialSpring,
                              [guard](double progress) {
                                  if (!guard)
                                      return false;
                                  guard->m_levelProgress = progress;
                                  if (progress >= 1.0 || !guard->isVisible()) {
                                      guard->m_levelDirection = 0;
                                      guard->m_levelPicture = QImage();
                                      guard->update();
                                      return false;
                                  }
                                  guard->update();
                                  return true;
                              });
}

QMenu *QMenu::openMenu() const { return static_cast<QMenu *>(m_open.data()); }

QMenuBar *QMenu::menuBar() const { return static_cast<QMenuBar *>(m_bar.data()); }

/*!
    \reimp
*/
QSize QMenu::sizeHint() const {
    layoutItems();
    return m_size;
}

/*!
    Returns the rectangle of \a action in the menu; an empty rectangle if the action is not shown.

    \sa actionAt()
*/
QRect QMenu::actionGeometry(QAction *action) const {
    layoutItems();
    for (const Item &item : m_items)
        if (item.action == action)
            return item.rect;
    return QRect();
}

/*!
    Returns the action at \a point in the menu, which can be a separator; \c nullptr if there is none there.

    \sa actionGeometry()
*/
QAction *QMenu::actionAt(const QPoint &point) const {
    layoutItems();
    for (const Item &item : m_items)
        if (item.rect.contains(point))
            return item.action;
    return nullptr;
}

/*!
    Sets the action that is current, the one that is highlighted and that Return chooses, to \a action; \c nullptr for
    none. This emits hovered(). It does not open a submenu.

    \sa activeAction()
*/
void QMenu::setActiveAction(QAction *action) { setCurrent(action, false); }

/*!
    \fn QAction *QMenu::activeAction() const

    Returns the action that is highlighted; \c nullptr if there is none.

    \sa setActiveAction()
*/

void QMenu::setCurrent(QAction *action, bool openSubMenu) {
    if (action && action->isSeparator())
        action = nullptr;
    if (action != m_active.data()) {
        if (m_open && (!action || openMenu()->menuAction() != action))
            closeSubMenu();
        m_subMenuTimer.stop();
        m_active = action;
        update();
        if (action) {
            emit hovered(action);
            if (QMenu *root = rootMenu(); root->menuBar())
                emit root->menuBar()->hovered(action);
            action->hover();
        }
    }
    // A submenu that opens in place waits for a click: it would take the rows from under the pointer.
    if (action && openSubMenu && action->isEnabled() && isSubMenuAction(action) && !opensSubMenusInPlace()) {
        m_pendingSubMenu = action;
        m_subMenuTimer.start();
    }
}

QMenu *QMenu::rootMenu() {
    QMenu *root = this;
    while (auto *parent = dynamic_cast<QMenu *>(WidgetAccess::data(root).popupParent.data()))
        root = parent;
    return root;
}

QWidget *QMenu::host() const {
    if (QWidget *parent = parentWidget())
        return parent->window();
    return QApplication::activeWindow();
}

// Shows the menu with its top left corner at `position` in the window `host`, or above or to the left if it does not fit.
// `leftEdge` is where the right edge goes when the menu does not fit on the right, for a submenu; -1 to keep it in the window.
void QMenu::showAt(QWidget *host, const QPoint &position, QAction *at, int leftEdge) {
    if (!host)
        return;
    if (parentWidget() != host)
        setParent(host);
    // The style may give the menu another font, which its size must be measured with, the first time it opens too.
    ensurePolished();
    layoutItems();
    if (m_items.empty())
        return;
    const QSize size = sizeHint();
    resize(size);
    QPoint p = position;
    if (at)
        p.setY(p.y() - actionGeometry(at).top());
    const QRect area = windowArea(host, size);
    const int right = area.left() + area.width();
    const int bottom = area.top() + area.height();
    if (p.x() + size.width() > right)
        p.setX(leftEdge >= 0 ? leftEdge - size.width() : right - size.width());
    if (p.y() + size.height() > bottom)
        p.setY(p.y() - size.height() >= area.top() && !at ? std::min(position.y(), bottom) - size.height()
                                                          : bottom - size.height());
    move(std::max(area.left(), p.x()), std::max(area.top(), p.y()));
    m_active = at;
    m_triggered = nullptr;
    slideLevel(QImage(), 0); // it opens at rest
    emit aboutToShow();
    raise();
    show();
    if (!WidgetAccess::data(this).popupParent)
        m_previousFocus = host->focusWidget();
    setFocus();
    if (!WidgetAccess::data(this).popupParent)
        WidgetAccess::setPopup(this, this);
    update();
}

/*!
    Shows the menu with its top left corner at \a pos, a position on the screen, and returns at once. If \a at is an
    action of the menu, the menu is placed so that the action is at \a pos, and it is the current action. The menu is
    moved if it would reach outside the window; under QMaterialStyle it keeps 8 pixels from the window's edges, where it
    fits.

    \sa exec()
*/
void QMenu::popup(const QPoint &pos, QAction *at) {
    QWidget *window = host();
    if (!window)
        return;
    WidgetAccess::data(this).popupParent = nullptr;
    showAt(window, window->mapFromGlobal(pos), at);
}

/*!
    Shows the menu at the position that the menu has in its window, and waits until it is closed.
    Returns the action that the user chose; \c nullptr if the menu was closed without one.

    \sa popup()
*/
QAction *QMenu::exec() { return exec(parentWidget() ? parentWidget()->mapToGlobal(pos()) : pos()); }

/*!
    Shows the menu at \a pos like popup(), and waits until it is closed, running the event loop meanwhile. Returns the
    action that the user chose, after it has been triggered; \c nullptr if the menu was closed without one. \a at is the
    action that is put at \a pos.

    \sa popup()
*/
QAction *QMenu::exec(const QPoint &pos, QAction *at) {
    popup(pos, at);
    if (!isVisible())
        return nullptr;
    QEventLoop loop;
    QObject::connect(this, &QMenu::aboutToHide, &loop, &QEventLoop::quit);
    loop.exec();
    return m_triggered.data();
}

void QMenu::openSubMenu(QAction *action, bool selectFirst) {
    auto *sub = action ? action->menu<QMenu *>() : nullptr;
    if (!sub || sub->isEmpty() || !action->isEnabled())
        return;
    if (opensSubMenusInPlace()) {
        openInPlace(sub, selectFirst);
        return;
    }
    if (m_open.data() == sub && sub->isVisible()) {
        if (selectFirst)
            sub->setCurrent(sub->step(nullptr, 1), false);
        return;
    }
    closeSubMenu();
    layoutItems();
    const QRect item = actionGeometry(action);
    WidgetAccess::data(sub).popupParent = this;
    QWidget *window = parentWidget();
    const QPoint position = pos() + QPoint(width() - 2, item.top());
    // A submenu that does not fit on the right goes to the left of this menu
    sub->showAt(window, position, nullptr, x() + 2);
    m_open = sub;
    if (selectFirst)
        sub->setCurrent(sub->step(nullptr, 1), false);
}

void QMenu::closeSubMenu() {
    m_pendingSubMenu = nullptr;
    m_subMenuTimer.stop();
    if (!m_open)
        return;
    QMenu *sub = openMenu();
    m_open = nullptr;
    sub->hide();
}

void QMenu::closeAll() { rootMenu()->hide(); }

// The entry after (direction 1) or before (-1) `from` that can be chosen, going around; the first or last one if
// `from` is null.
QAction *QMenu::step(QAction *from, int direction) const {
    layoutItems();
    std::vector<QAction *> choices;
    int at = -1;
    for (const Item &item : m_items) {
        if (item.action->isSeparator() || !item.action->isEnabled())
            continue;
        if (item.action == from)
            at = static_cast<int>(choices.size());
        choices.push_back(item.action);
    }
    if (choices.empty())
        return nullptr;
    const int count = static_cast<int>(choices.size());
    if (at < 0) {
        // `from` is not among them (or none): a disabled current entry still has a place among the visible ones
        if (from) {
            bool passed = false;
            QAction *found = nullptr;
            for (const Item &item : m_items) {
                if (item.action == from) {
                    passed = true;
                    if (direction < 0)
                        break;
                    continue;
                }
                if (item.action->isSeparator() || !item.action->isEnabled())
                    continue;
                if (direction > 0 && passed)
                    return item.action;
                if (direction < 0 && !passed)
                    found = item.action;
            }
            if (found)
                return found;
        }
        return direction > 0 ? choices.front() : choices.back();
    }
    return choices[static_cast<std::size_t>((at + direction + count) % count)];
}

QAction *QMenu::mnemonicTarget(int key, bool *several) const {
    layoutItems();
    QAction *first = nullptr, *next = nullptr;
    bool passedCurrent = m_active.isNull();
    int found = 0;
    for (const Item &item : m_items) {
        QAction *action = item.action;
        if (action->isSeparator() || !action->isEnabled())
            continue;
        if (QKeySequence::mnemonic(action->text())[0] != (Qt::ALT | key))
            continue;
        ++found;
        if (!first)
            first = action;
        if (passedCurrent && !next)
            next = action;
        if (action == m_active.data())
            passedCurrent = true;
    }
    if (several)
        *several = found > 1;
    return found > 1 && next ? next : first;
}

void QMenu::activateItem(QAction *action) {
    if (!action || !action->isEnabled() || action->isSeparator())
        return;
    if (action == m_backAction && !m_inPlace.empty()) {
        goBack();
        return;
    }
    // The menus from this one to the first, which all report the action, after those shown in this one's place
    std::vector<QPointer<QMenu>> chain(m_inPlace.rbegin(), m_inPlace.rend());
    for (QMenu *menu = this; menu; menu = dynamic_cast<QMenu *>(WidgetAccess::data(menu).popupParent.data()))
        chain.emplace_back(menu);
    QMenu *root = rootMenu();
    QPointer<QWidget> bar = root->m_bar;
    root->m_triggered = action;
    QPointer<QAction> guard(action);
    root->hide();
    if (guard)
        action->trigger();
    for (const QPointer<QMenu> &menu : chain)
        if (menu)
            emit menu->triggered(action);
    if (bar && guard)
        emit static_cast<QMenuBar *>(bar.data())->triggered(action);
}

/*!
    \reimp
*/
void QMenu::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOption panel;
    panel.initFrom(this);
    panel.rect = rect();
    style()->drawPrimitive(QStyle::PE_PanelMenu, &panel, &painter, this);
    layoutItems();
    // Between the levels of a vertical menu the rows slide, inside the container.
    if (m_levelDirection != 0) {
        const double across = width();
        const double shift = m_levelDirection * across * (1.0 - m_levelProgress);
        painter.setClipRect(rect());
        painter.drawImage(QPointF(shift - m_levelDirection * across, 0), m_levelPicture);
        painter.translate(shift, 0);
    }
    for (const Item &item : m_items) {
        QStyleOptionMenuItem option;
        initStyleOption(&option, item.action);
        option.rect = item.rect;
        style()->drawControl(QStyle::CE_MenuItem, &option, &painter, this);
    }
}

/*!
    \reimp
*/
void QMenu::mouseMoveEvent(QMouseEvent *event) {
    setCurrent(actionAt(event->pos()), true);
    event->accept();
}

/*!
    \reimp
*/
void QMenu::mousePressEvent(QMouseEvent *event) {
    QAction *action = actionAt(event->pos());
    if (action && !action->isSeparator()) {
        setCurrent(action, false);
        if (isSubMenuAction(action) && !opensSubMenusInPlace())
            openSubMenu(action, false);
    }
    event->accept();
}

/*!
    \reimp
*/
void QMenu::mouseReleaseEvent(QMouseEvent *event) {
    QAction *action = actionAt(event->pos());
    if (action && !action->isSeparator() && action->isEnabled()) {
        // A submenu that opens in place does on the release, so that the release does not choose what takes its place.
        if (!isSubMenuAction(action))
            activateItem(action);
        else if (opensSubMenusInPlace())
            openSubMenu(action, false);
    }
    event->accept();
}

/*!
    \reimp
*/
void QMenu::leaveEvent(QEvent *event) {
    (void)event;
    m_subMenuTimer.stop();
    if (!m_open && m_active) {
        m_active = nullptr;
        update();
    }
}

/*!
    \reimp
*/
void QMenu::keyPressEvent(QKeyEvent *event) {
    QAction *current = m_active.data();
    const bool hasSubMenu = current && current->isEnabled() && isSubMenuAction(current);
    QMenu *parentMenu = dynamic_cast<QMenu *>(WidgetAccess::data(this).popupParent.data());
    switch (event->key()) {
    case Qt::Key_Down:
        setCurrent(step(current, 1), false);
        break;
    case Qt::Key_Up:
        setCurrent(step(current, -1), false);
        break;
    case Qt::Key_Home:
        setCurrent(step(nullptr, 1), false);
        break;
    case Qt::Key_End:
        setCurrent(step(nullptr, -1), false);
        break;
    case Qt::Key_Right:
        if (hasSubMenu)
            openSubMenu(current, true);
        else if (QMenu *root = rootMenu(); root->menuBar())
            root->menuBar()->navigate(root, 1);
        break;
    case Qt::Key_Left:
        if (!m_inPlace.empty())
            goBack();
        else if (parentMenu)
            hide();
        else if (menuBar())
            menuBar()->navigate(this, -1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (hasSubMenu)
            openSubMenu(current, true);
        else if (current)
            activateItem(current);
        break;
    case Qt::Key_Escape:
        if (!m_inPlace.empty())
            goBack();
        else if (parentMenu)
            hide();
        else
            closeAll();
        break;
    default: {
        const int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z && (event->modifiers() & ~Qt::ShiftModifier) == Qt::NoModifier) {
            bool several = false;
            if (QAction *target = mnemonicTarget(key, &several)) {
                if (several) {
                    setCurrent(target, false);
                } else if (isSubMenuAction(target)) {
                    setCurrent(target, false);
                    openSubMenu(target, true);
                } else {
                    activateItem(target);
                }
                event->accept();
                return;
            }
        }
        event->ignore();
        return;
    }
    }
    event->accept();
}

/*!
    \reimp
*/
void QMenu::actionEvent(QActionEvent *event) {
    (void)event;
    m_dirty = true;
    // A menu shown in another's place changes the rows of that one.
    if (m_inPlaceHost && isHidden())
        m_inPlaceHost->relayout();
    if (m_active && m_active.data() != m_backAction) {
        bool present = false;
        for (QAction *action : shownActions())
            present = present || action == m_active.data();
        if (!present)
            m_active = nullptr;
    }
    if (isVisible() && !m_closing) {
        layoutItems();
        if (!m_size.isEmpty())
            resize(m_size);
    }
    update();
}

/*!
    \reimp
*/
void QMenu::hideEvent(QHideEvent *event) {
    (void)event;
    m_closing = true;
    closeSubMenu();
    m_active = nullptr;
    // The submenus shown in place close with it, and it opens again with its own items.
    std::vector<QPointer<QMenu>> inPlace;
    inPlace.swap(m_inPlace);
    m_dirty = m_dirty || !inPlace.empty();
    for (auto it = inPlace.rbegin(); it != inPlace.rend(); ++it) {
        if (*it) {
            (*it)->m_inPlaceHost = nullptr;
            emit(*it)->aboutToHide();
        }
    }
    auto *parentMenu = dynamic_cast<QMenu *>(WidgetAccess::data(this).popupParent.data());
    if (parentMenu) {
        WidgetAccess::data(this).popupParent = nullptr;
        if (parentMenu->m_open.data() == this)
            parentMenu->m_open = nullptr;
        parentMenu->setFocus();
    } else {
        WidgetAccess::setPopup(this, nullptr);
        if (menuBar())
            menuBar()->menuClosed(this);
        if (m_previousFocus)
            m_previousFocus->setFocus();
        m_previousFocus = nullptr;
    }
    m_closing = false;
    emit aboutToHide();
}

/*!
    \reimp
*/
void QMenu::changeEvent(QEvent *event) {
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
        m_dirty = true;
        if (isVisible()) {
            layoutItems();
            if (!m_size.isEmpty())
                resize(m_size);
        }
        update();
        break;
    default:
        break;
    }
    QWidget::changeEvent(event);
}

/*!
    \fn template <typename Receiver, typename Functor, typename> QAction *QMenu::addAction(const QString &text, const QKeySequence &shortcut, const Receiver *receiver, Functor functor, Qt::ConnectionType type)
    \overload
    \since 6.0

    Adds an action with \a text and \a shortcut whose triggered() signal calls \a functor, a member function of
    \a receiver or a function, with the connection \a type, and returns it.
*/

/*!
    \fn template <typename Functor, typename> QAction *QMenu::addAction(const QString &text, const QKeySequence &shortcut, Functor functor)
    \overload
    \since 6.0

    Adds an action with \a text and \a shortcut whose triggered() signal calls \a functor, and returns it.
*/

/*!
    \overload
    \since 6.0

    Adds an action with \a text and \a shortcut to the end of the menu, and returns it.
*/
QAction *QMenu::addAction(const QString &text, const QKeySequence &shortcut) {
    QAction *action = addAction(text);
    action->setShortcut(shortcut);
    return action;
}

/*!
    \overload
    \since 6.0

    Adds an action with \a icon, \a text and \a shortcut to the end of the menu, and returns it.
*/
QAction *QMenu::addAction(const QIcon &icon, const QString &text, const QKeySequence &shortcut) {
    QAction *action = addAction(icon, text);
    action->setShortcut(shortcut);
    return action;
}

/*!
    \overload
    \since 6.0

    Adds an action with \a text and \a shortcut whose triggered() signal is connected to the slot \a member of
    \a receiver with the connection \a type, and returns it.
*/
QAction *QMenu::addAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver,
                          const char *member, Qt::ConnectionType type) {
    QAction *action = addAction(text, shortcut);
    QObject::connect(action, SIGNAL(triggered(bool)), receiver, member, type);
    return action;
}

/*!
    \since 6.0

    Fills \a option with the state of the entry of \a action, for the style to draw it.
*/
void QMenu::initStyleOption(QStyleOptionMenuItem *option, const QAction *action) const {
    if (!option || !action)
        return;
    option->initFrom(this);
    option->rect = actionGeometry(const_cast<QAction *>(action)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    option->text = textWithShortcut(action);
    option->icon = action->isIconVisibleInMenu() ? action->icon() : QIcon();
    option->font = action->font() == QFont() ? font() : action->font();
    option->state = QStyle::State();
    const bool section = action->isSeparator() && !action->text().isEmpty();
    if (action->isEnabled() && isEnabled() && !section)
        option->state |= QStyle::State_Enabled;
    if (action == m_active.data())
        option->state |= QStyle::State_Selected;
    if (section || action == m_default.data())
        option->font.setBold(true);
    if (action->isSeparator() && !section)
        option->menuItemType = QStyleOptionMenuItem::Separator;
    else if (isSubMenuAction(action))
        option->menuItemType = QStyleOptionMenuItem::SubMenu;
    if (action->isCheckable()) {
        option->checkType = action->actionGroup() && action->actionGroup()->isExclusive()
                                ? QStyleOptionMenuItem::Exclusive
                                : QStyleOptionMenuItem::NonExclusive;
        option->checked = action->isChecked();
    }
}

/*!
    \since 6.0

    Adds a section with the title \a text to the end of the menu and returns its action: a separator that shows
    the title, to head the entries after it.

    \code
    menu->addSection(tr("Recent Files"));
    \endcode
*/
QAction *QMenu::addSection(const QString &text) { return insertSection(nullptr, QIcon(), text); }

/*!
    \since 6.0
    \overload

    Adds a section with \a icon and the title \a text to the end of the menu and returns its action.
*/
QAction *QMenu::addSection(const QIcon &icon, const QString &text) { return insertSection(nullptr, icon, text); }

/*!
    \since 6.0

    Inserts a section with the title \a text before the action \a before, or at the end if it is \c nullptr, and
    returns its action.
*/
QAction *QMenu::insertSection(QAction *before, const QString &text) { return insertSection(before, QIcon(), text); }

/*!
    \since 6.0
    \overload

    Inserts a section with \a icon and the title \a text before the action \a before and returns its action.
*/
QAction *QMenu::insertSection(QAction *before, const QIcon &icon, const QString &text) {
    auto *action = new QAction(icon, text, this);
    action->setSeparator(true);
    insertAction(before, action);
    return action;
}

/*!
    \fn QAction *QMenu::defaultAction() const
    \since 6.0

    Returns the default action of the menu, or \c nullptr.
*/

/*!
    \since 6.0

    Makes \a act the default action of the menu, which is drawn in bold.
*/
void QMenu::setDefaultAction(QAction *act) {
    m_default = act;
    update();
}

/*!
    \since 6.0

    Returns the icon of the menu, which its menuAction() has.
*/
QIcon QMenu::icon() const { return m_menuAction->icon(); }

/*!
    \since 6.0

    Sets the icon of the menu, shown where the menu is an entry of another one, to \a icon.
*/
void QMenu::setIcon(const QIcon &icon) { m_menuAction->setIcon(icon); }

/*!
    \fn bool QMenu::separatorsCollapsible() const
    \since 6.0

    Returns whether consecutive separators show as one, and separators at the ends are not shown. The default is
    \c true.
*/

/*!
    \since 6.0

    Sets whether consecutive separators show as one, and separators at the ends are hidden, to \a collapse.
*/
void QMenu::setSeparatorsCollapsible(bool collapse) {
    m_collapsible = collapse;
    m_dirty = true;
    update();
}

/*!
    \fn bool QMenu::toolTipsVisible() const
    \since 6.0

    Returns whether the tool tips of the actions show when the mouse rests on them. The default is \c false.
*/

/*!
    \fn void QMenu::setToolTipsVisible(bool visible)
    \since 6.0

    Sets whether the tool tips of the actions show to \a visible.
*/

/*!
    \fn bool QMenu::isTearOffEnabled() const
    \since 6.0

    Returns whether the menu can be torn off. Nt keeps the value.
*/

/*!
    \fn void QMenu::setTearOffEnabled(bool enable)
    \since 6.0

    Sets whether the menu can be torn off to \a enable. Nt has no tear-off menus.
*/

/*!
    \fn bool QMenu::isTearOffMenuVisible() const
    \since 6.0

    Returns \c false: Nt shows no torn-off copy of a menu.
*/

/*!
    \since 6.0

    Would show a torn-off copy of the menu. Nt has none, so this does nothing.
*/
void QMenu::showTearOffMenu() {}

/*!
    \since 6.0
    \overload

    Would show a torn-off copy of the menu at \a pos. Does nothing.
*/
void QMenu::showTearOffMenu(const QPoint &pos) { (void)pos; }

/*!
    \since 6.0

    Would hide the torn-off copy of the menu. Does nothing.
*/
void QMenu::hideTearOffMenu() {}

/*!
    \since 6.0

    Would keep the click that closes the menu from reaching \a widget. A click outside an Nt menu only closes it,
    so this does nothing.
*/
void QMenu::setNoReplayFor(QWidget *widget) { (void)widget; }

/*!
    \since 6.0

    Returns the menu that \a action opens, or \c nullptr.
*/
QMenu *QMenu::menuInAction(const QAction *action) { return action ? action->menu<QMenu *>() : nullptr; }

/*!
    \fn int QMenu::columnCount() const
    \since 6.0

    Returns the number of columns of the menu: 1, as an Nt menu does not wrap into columns.
*/

/*!
    \reimp

    With toolTipsVisible(), a tool tip event over an entry shows the tool tip of its action.
*/
bool QMenu::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        QAction *action = actionAt(help->pos());
        if (m_toolTipsVisible && action && !action->isSeparator() && !action->toolTip().isEmpty() &&
            action->toolTip() != plainLabel(action->text())) {
            QToolTip::showText(help->globalPos(), action->toolTip(), this, actionGeometry(action));
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

#include "moc_qmenu.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
