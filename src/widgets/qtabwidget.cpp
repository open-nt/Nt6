// MIT License - see LICENSE
#include <QtWidgets/qtabwidget.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtabbar.h>

#include "qwidgetutil_p.h"

#include <algorithm>

/*!
    \class QTabWidget
    \brief The QTabWidget class is a stack of pages with a row of tabs to choose the page.

    Each page is a widget, usually with a layout of its own, that is shown when its tab is current. The user
    clicks a tab, or uses the Left and Right keys when the tabs have the focus, to switch:

    \code
    QTabWidget *tabs = new QTabWidget(window);
    tabs->addTab(buttonsPage, "Buttons");
    tabs->addTab(inputPage, "Input");
    connect(tabs, &QTabWidget::currentChanged, [](int index) { qDebug() << index; });
    \endcode

    The tab widget makes the pages its children and sizes them to fill the area below the tabs. It does not own
    them in any other way than a parent owns its children: a page that is removed with removeTab() is not deleted,
    and deleting a page removes its tab.

    A tab widget with the first tab current and the last one disabled.

    \image widgets/qtabwidget.png QTabWidget in the light color scheme

    setTabPosition() puts the tabs below the pages or at their side, and setCornerWidget() puts a widget, such as a
    button that opens a new tab, at an end of the row of tabs.

    Under QMaterialStyle, with the tab bar a navigation rail (\c materialTabBar \c "navigationRail") at the west, the
    corner widget at Qt::TopLeftCorner (Qt::TopRightCorner for a rail at the east) goes at the top of the rail, 44
    down and as wide as the rail, and the destinations start 40 under it: the place for a menu button and a floating
    action button. With \c materialTabBar \c "navigationRailModal" the rail is the modal expanded rail: it lies over
    the pages, which keep the room of the collapsed rail, with a scrim over them. A click on the scrim, Escape or the
    choice of a destination sets \c materialTabBar back to \c "navigationRail", which collapses it; a menu button in
    the corner widget opens it by setting the property:

    \code
    auto *menu = new QToolButton;
    menu->setIcon(QIcon::fromTheme("open-menu"));
    tabs->setCornerWidget(menu, Qt::TopLeftCorner);
    QObject::connect(menu, &QToolButton::clicked, [tabs] {
        tabs->tabBar()->setProperty("materialTabBar", "navigationRailModal");
    });
    \endcode

    \differences
    Corner widgets are placed only while the tabs are above or below the pages, or at the top of a navigation rail of
    QMaterialStyle. Triangular tabs are drawn rounded,
    and there are no scroll buttons; tabs that do not fit are cut off or, with elideMode(), shortened. Ctrl+Tab
    does not switch tabs. A change of the size hint of a page does not update the layout the tab widget is in.
    \enddifferences

    \sa QGroupBox, QStyle
*/

/*!
    \enum QTabWidget::TabPosition
    \since 6.0

    Where the tabs are.

    \value North Above the pages, the default.
    \value South Below the pages.
    \value West At the left of the pages.
    \value East At the right of the pages.
*/

/*!
    \enum QTabWidget::TabShape
    \since 6.0

    The shape of the tabs.

    \value Rounded Rounded tabs, the default.
    \value Triangular Triangular tabs; Nt draws them rounded.
*/

/*!
    Constructs an empty tab widget with the given \a parent.
*/
QTabWidget::QTabWidget(QWidget *parent) : QWidget(parent) {
    m_bar = new QTabBar(this);
    m_bar->setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connectBar();
}

void QTabWidget::connectBar() {
    QObject::connect(m_bar, &QTabBar::currentChanged, this, [this](int index) { select(index); });
    QObject::connect(m_bar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *page = m_pages[static_cast<std::size_t>(from)];
        m_pages.erase(m_pages.begin() + from);
        m_pages.insert(m_pages.begin() + to, page);
    });
    QObject::connect(m_bar, &QTabBar::tabCloseRequested, this, &QTabWidget::tabCloseRequested);
    QObject::connect(m_bar, &QTabBar::tabBarClicked, this, &QTabWidget::tabBarClicked);
    QObject::connect(m_bar, &QTabBar::tabBarDoubleClicked, this, &QTabWidget::tabBarDoubleClicked);
}

/*!
    Destroys the tab widget and the pages.
*/
QTabWidget::~QTabWidget() {
    // The pages are deleted as children of the widget, after this object is gone.
    for (QWidget *page : m_pages)
        QObject::disconnect(page, &QObject::destroyed, this, nullptr);
    QObject::disconnect(m_bar, nullptr, this, nullptr);
}

/*!
    Adds \a page as a tab with the text \a label, after the other tabs, and returns its index. The tab widget
    becomes the parent of \a page. The first tab that is added becomes the current one.

    \sa insertTab()
*/
int QTabWidget::addTab(QWidget *page, const QString &label) { return insertTab(count(), page, label); }

/*!
    \since 6.0
    \overload

    Adds \a page as a tab with the given \a icon and \a label and returns its index.
*/
int QTabWidget::addTab(QWidget *page, const QIcon &icon, const QString &label) {
    return insertTab(count(), page, icon, label);
}

/*!
    \since 6.0
    \overload

    Inserts \a page as a tab with the given \a icon and \a label at \a index and returns its index.
*/
int QTabWidget::insertTab(int index, QWidget *page, const QIcon &icon, const QString &label) {
    const int at = insertTab(index, page, label);
    if (at >= 0)
        m_bar->setTabIcon(at, icon);
    return at;
}

/*!
    Inserts \a page as a tab with the text \a label at \a index, which is brought into the range of the tabs, and returns the
    index of the new tab. The tab widget becomes the parent of \a page. The current tab stays the same one, except
    that the first tab that is added becomes the current one.

    \sa addTab(), removeTab()
*/
int QTabWidget::insertTab(int index, QWidget *page, const QString &label) {
    if (!page)
        return -1;
    const int at = std::clamp(index, 0, count());
    page->setParent(this);
    page->hide();
    m_pages.insert(m_pages.begin() + at, page);
    QObject::connect(page, &QObject::destroyed, this, [this](QObject *gone) { pageGone(gone); });
    m_bar->insertTab(at, label);
    layoutChildren();
    updateGeometry();
    update();
    tabInserted(at);
    return at;
}

/*!
    Removes the tab at \a index. The page is hidden but not deleted, and stays a child of the tab widget. If the
    tab was the current one, a neighbouring tab becomes current.

    \sa clear(), insertTab()
*/
void QTabWidget::removeTab(int index) {
    if (index < 0 || index >= count())
        return;
    QWidget *page = m_pages[static_cast<std::size_t>(index)];
    QObject::disconnect(page, &QObject::destroyed, this, nullptr);
    pageGone(page);
    page->hide();
}

/*!
    Removes all tabs. The pages are hidden but not deleted.

    \sa removeTab()
*/
void QTabWidget::clear() {
    while (count() > 0)
        removeTab(count() - 1);
}

// Takes the tab of `page` out, after the page is deleted or removed.
void QTabWidget::pageGone(QObject *page) {
    for (std::size_t i = 0; i < m_pages.size(); ++i) {
        if (static_cast<QObject *>(m_pages[i]) != page)
            continue;
        m_pages.erase(m_pages.begin() + static_cast<std::ptrdiff_t>(i));
        m_bar->removeTab(static_cast<int>(i));
        layoutChildren();
        updateGeometry();
        update();
        tabRemoved(static_cast<int>(i));
        return;
    }
}

/*!
    \fn int QTabWidget::count() const

    Returns the number of tabs.
*/

/*!
    \fn QWidget *QTabWidget::currentWidget() const

    Returns the page of the current tab, or \c nullptr if there are none.

    \sa setCurrentWidget()
*/

/*!
    Returns the page of the tab at \a index, or \c nullptr if there is no such tab.

    \sa indexOf()
*/
QWidget *QTabWidget::widget(int index) const {
    return index >= 0 && index < count() ? m_pages[static_cast<std::size_t>(index)] : nullptr;
}

/*!
    Returns the index of the tab that has \a page, or -1 if there is none.

    \sa widget()
*/
int QTabWidget::indexOf(const QWidget *page) const {
    for (int i = 0; i < count(); ++i)
        if (m_pages[static_cast<std::size_t>(i)] == page)
            return i;
    return -1;
}

/*!
    Returns the text of the tab at \a index, or an empty string if there is no such tab.

    \sa setTabText()
*/
QString QTabWidget::tabText(int index) const { return m_bar->tabText(index); }

/*!
    Sets the text of the tab at \a index to \a text.

    \sa tabText()
*/
void QTabWidget::setTabText(int index, const QString &text) {
    m_bar->setTabText(index, text);
    layoutChildren();
}

/*!
    Returns whether the tab at \a index can be chosen. The tabs are enabled at first.

    \sa setTabEnabled()
*/
bool QTabWidget::isTabEnabled(int index) const { return m_bar->isTabEnabled(index); }

/*!
    Sets whether the tab at \a index can be chosen to \a enabled. A disabled tab is drawn grayed out and the
    user cannot switch to it. A tab that is current stays so.

    \sa isTabEnabled()
*/
void QTabWidget::setTabEnabled(int index, bool enabled) { m_bar->setTabEnabled(index, enabled); }

/*!
    Returns the index of the current tab, or -1 if there are none.

    \sa setCurrentIndex()
*/
int QTabWidget::currentIndex() const { return m_bar->currentIndex(); }

/*!
    Returns the tool tip of the tab at \a index.

    \sa setTabToolTip()
*/
QString QTabWidget::tabToolTip(int index) const { return m_bar->tabToolTip(index); }

/*!
    Sets the tool tip of the tab at \a index to \a tip.
*/
void QTabWidget::setTabToolTip(int index, const QString &tip) { m_bar->setTabToolTip(index, tip); }

/*!
    Returns whether the tabs have close buttons. The default is \c false.

    \sa setTabsClosable(), tabCloseRequested()
*/
bool QTabWidget::tabsClosable() const { return m_bar->tabsClosable(); }

/*!
    Sets whether the tabs have close buttons to \a closable. A click on one emits tabCloseRequested(); the tab widget
    does not remove the tab itself.
*/
void QTabWidget::setTabsClosable(bool closable) {
    m_bar->setTabsClosable(closable);
    layoutChildren();
}

/*!
    Returns whether the user can drag the tabs into another order. The default is \c false.

    \sa setMovable()
*/
bool QTabWidget::isMovable() const { return m_bar->isMovable(); }

/*!
    Sets whether the user can drag the tabs into another order to \a movable. The pages move with their tabs.
*/
void QTabWidget::setMovable(bool movable) { m_bar->setMovable(movable); }

/*!
    \fn QTabBar *QTabWidget::tabBar() const

    Returns the tab bar of the tab widget.
*/

bool QTabWidget::focusIn(const QWidget *page) const {
    for (const QWidget *w = focusWidget(); w; w = w->parentWidget())
        if (w == page)
            return true;
    return false;
}

// Makes the tab at `index` current: the page of the old one is hidden, the page of the new one is shown.
void QTabWidget::select(int index) {
    QWidget *old = m_shown;
    QWidget *page = widget(index);
    if (page == old && page) {
        emit currentChanged(index);
        return;
    }
    const bool hadFocus = old && focusIn(old);
    if (old)
        old->hide();
    m_shown = page;
    if (page) {
        page->show();
        if (hadFocus)
            m_bar->setFocus();
    }
    update();
    emit currentChanged(index);
}

/*!
    Makes the tab at \a index the current one and shows its page. An \a index that is not a tab is ignored.
    Emits currentChanged() if the tab changes.

    \sa currentIndex()
*/
void QTabWidget::setCurrentIndex(int index) { m_bar->setCurrentIndex(index); }

/*!
    Makes the tab of \a page the current one. Nothing happens if \a page is not in a tab.

    \sa currentWidget(), setCurrentIndex()
*/
void QTabWidget::setCurrentWidget(QWidget *page) { setCurrentIndex(indexOf(page)); }

namespace {
// The size of a corner widget: its size hint, or its size if it has none.
QSize cornerSize(const QWidget *corner) { return corner->sizeHint().isValid() ? corner->sizeHint() : corner->size(); }

constexpr int kRailHeaderTop = 44; // a corner widget at the top of a navigation rail is this far down

bool isModalKind(const QTabBar *bar) {
    return bar->property("materialTabBar").toString() == QLatin1String("navigationRailModal");
}

// The scrim over the pages behind a modal expanded rail. A press on it, Escape on the rail, or a destination chosen
// collapses the rail.
class RailScrim : public QWidget {
public:
    RailScrim(QTabWidget *tabs, QTabBar *bar) : QWidget(tabs), m_bar(bar) {
        bar->installEventFilter(this);
        QObject::connect(bar, &QTabBar::currentChanged, this, [bar] { collapse(bar); });
    }
    static void collapse(QTabBar *bar) {
        if (isModalKind(bar))
            bar->setProperty("materialTabBar", "navigationRail");
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 82)); // black at 32 percent
    }
    void mousePressEvent(QMouseEvent *event) override {
        event->accept();
        if (m_bar)
            collapse(m_bar);
    }
    void mouseReleaseEvent(QMouseEvent *event) override { event->accept(); }
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched != m_bar || !isModalKind(m_bar) || m_bar->isHidden())
            return false;
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            collapse(m_bar);
            return true;
        }
        // The destination that is shown chosen again: one that is not changes the current tab, which collapses it.
        if (event->type() == QEvent::MouseButtonRelease) {
            const int index = m_bar->tabAt(static_cast<QMouseEvent *>(event)->pos());
            if (index >= 0 && index == m_bar->currentIndex())
                collapse(m_bar);
        }
        return false;
    }

private:
    QPointer<QTabBar> m_bar;
};
} // namespace

bool QTabWidget::isMaterialRail() const {
    return qobject_cast<const QMaterialStyle *>(m_bar->style()) && QMaterialStyle::isNavigationRail(m_bar);
}

bool QTabWidget::isOpenModalRail() const {
    return isMaterialRail() && QMaterialStyle::isModalRail(m_bar) && !m_bar->isHidden();
}

int QTabWidget::barThickness() const {
    if (m_bar->isHidden())
        return 0;
    // The pages keep the room of the collapsed rail beside a modal rail, which lies over them.
    if (verticalBar() && isOpenModalRail())
        return QMaterialStyle::collapsedRailWidth();
    const QSize hint = m_bar->sizeHint();
    int thick = verticalBar() ? hint.width() : hint.height();
    if (!verticalBar()) {
        for (const QWidget *corner : {m_leftCorner.data(), m_rightCorner.data()})
            if (corner && !corner->isHidden())
                thick = std::max(thick, cornerSize(corner).height());
    }
    return thick;
}

// Where the frame of the pages is: all but the bar, overlapping it by one pixel.
QRect QTabWidget::frameRect() const {
    const int thick = barThickness();
    const int overlap = thick > 0 ? 1 : 0;
    switch (m_position) {
    case South:
        return QRect(0, 0, width(), height() - thick + overlap);
    case West:
        return QRect(thick - overlap, 0, width() - thick + overlap, height());
    case East:
        return QRect(0, 0, width() - thick + overlap, height());
    case North:
        break;
    }
    return QRect(0, thick - overlap, width(), height() - thick + overlap);
}

void QTabWidget::layoutChildren() {
    const int thick = barThickness();
    const int w = width();
    const int h = height();
    auto *scrim = static_cast<RailScrim *>(m_scrim.data());
    if (verticalBar()) {
        const bool modal = isOpenModalRail();
        const int across = modal ? m_bar->sizeHint().width() : thick;
        m_bar->setGeometry(m_position == West ? 0 : w - across, 0, across, h);
        // A rail of the Material style has the corner widget at its top; otherwise the corners have no room.
        QWidget *top = nullptr;
        if (isMaterialRail())
            top = m_position == West ? m_leftCorner.data() : m_rightCorner.data();
        for (QWidget *corner : {m_leftCorner.data(), m_rightCorner.data()})
            if (corner && corner != top)
                corner->setGeometry(QRect());
        if (modal) {
            if (!scrim) {
                scrim = new RailScrim(this, m_bar);
                m_scrim = scrim;
            }
            scrim->setGeometry(rect());
            scrim->show();
            scrim->raise();
            m_bar->raise();
        } else if (scrim) {
            scrim->hide();
        }
        if (top && !top->isHidden()) {
            top->setGeometry(m_bar->x(), kRailHeaderTop, across, cornerSize(top).height());
            top->raise();
        }
    } else {
        if (scrim)
            scrim->hide();
        int left = 0;
        int right = w;
        const int y = m_position == North ? 0 : h - thick;
        if (QWidget *corner = m_leftCorner.data(); corner && !corner->isHidden()) {
            const int cw = cornerSize(corner).width();
            corner->setGeometry(0, y, cw, thick);
            left += cw;
        }
        if (QWidget *corner = m_rightCorner.data(); corner && !corner->isHidden()) {
            const int cw = cornerSize(corner).width();
            corner->setGeometry(w - cw, y, cw, thick);
            right -= cw;
        }
        const int barHeight = m_bar->sizeHint().height();
        m_bar->setGeometry(left, m_position == North ? thick - barHeight : y, std::max(0, right - left), barHeight);
    }
    // The frame of the pages is 1 pixel wide.
    const QRect pages = frameRect().adjusted(1, 1, -1, -1);
    for (QWidget *page : m_pages)
        page->setGeometry(QRect(pages.topLeft(), pages.size().expandedTo(QSize(0, 0))));
}

/*!
    \reimp
*/
QSize QTabWidget::sizeHint() const {
    QSize pages(0, 0);
    for (QWidget *page : m_pages)
        pages = QtPrivate::expandedTo(pages, page->sizeHint().isValid() ? page->sizeHint() : QSize(0, 0));
    const QSize bar = m_bar->isHidden() ? QSize(0, 0) : m_bar->sizeHint();
    const int thick = barThickness();
    if (verticalBar())
        return QSize(thick + pages.width() + 1, std::max(bar.height(), pages.height()) + 2);
    int corners = 0;
    for (const QWidget *corner : {m_leftCorner.data(), m_rightCorner.data()})
        if (corner && !corner->isHidden())
            corners += cornerSize(corner).width();
    return QSize(std::max(bar.width() + corners, pages.width()) + 2, thick + pages.height() + 1);
}

/*!
    \reimp
*/
QSize QTabWidget::minimumSizeHint() const {
    QSize pages(0, 0);
    for (QWidget *page : m_pages) {
        const QSize min = page->minimumSizeHint();
        pages = QtPrivate::expandedTo(pages, min.isValid() ? min : QSize(0, 0));
    }
    const QSize bar = m_bar->isHidden() ? QSize(0, 0) : m_bar->sizeHint();
    const int thick = barThickness();
    if (verticalBar())
        return QSize(thick + pages.width() + 1, std::max(bar.height(), pages.height()) + 2);
    return QSize(std::max(bar.width(), pages.width()) + 2, thick + pages.height() + 1);
}

/*!
    \reimp
*/
void QTabWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionTabWidgetFrame option;
    initStyleOption(&option);
    style()->drawPrimitive(QStyle::PE_FrameTabWidget, &option, &painter, this);
}

/*!
    \since 6.0

    Fills \a option with the state of the tab widget, for the style to draw the frame of its pages.
*/
void QTabWidget::initStyleOption(QStyleOptionTabWidgetFrame *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->rect = frameRect();
    option->lineWidth = 1;
    option->shape = m_bar->shape();
    option->tabBarSize = m_bar->isHidden() ? QSize(0, 0) : m_bar->sizeHint();
    option->tabBarRect = m_bar->geometry();
    option->selectedTabRect = m_bar->tabRect(m_bar->currentIndex()).translated(m_bar->pos());
    option->leftCornerWidgetSize = m_leftCorner ? m_leftCorner->size() : QSize(0, 0);
    option->rightCornerWidgetSize = m_rightCorner ? m_rightCorner->size() : QSize(0, 0);
}

/*!
    \since 6.0

    Returns the icon of the tab at \a index.
*/
QIcon QTabWidget::tabIcon(int index) const { return m_bar->tabIcon(index); }

/*!
    \since 6.0

    Sets the icon of the tab at \a index to \a icon.
*/
void QTabWidget::setTabIcon(int index, const QIcon &icon) {
    m_bar->setTabIcon(index, icon);
    layoutChildren();
    updateGeometry();
}

/*!
    \since 6.0

    Returns \c true if the tab at \a index is shown.
*/
bool QTabWidget::isTabVisible(int index) const { return m_bar->isTabVisible(index); }

/*!
    \since 6.0

    Shows the tab at \a index if \a visible is \c true, and hides it otherwise. If it was current, another tab
    becomes current.
*/
void QTabWidget::setTabVisible(int index, bool visible) {
    m_bar->setTabVisible(index, visible);
    layoutChildren();
}

/*!
    \since 6.0

    Returns the "What's This?" help of the tab at \a index.
*/
QString QTabWidget::tabWhatsThis(int index) const { return m_bar->tabWhatsThis(index); }

/*!
    \since 6.0

    Sets the "What's This?" help of the tab at \a index to \a text.
*/
void QTabWidget::setTabWhatsThis(int index, const QString &text) { m_bar->setTabWhatsThis(index, text); }

/*!
    \fn QTabWidget::TabPosition QTabWidget::tabPosition() const
    \since 6.0

    Returns where the tabs are, North by default.
*/

/*!
    \since 6.0

    Puts the tabs at \a position.
*/
void QTabWidget::setTabPosition(TabPosition position) {
    if (m_position == position)
        return;
    m_position = position;
    applyShape();
}

/*!
    \fn QTabWidget::TabShape QTabWidget::tabShape() const
    \since 6.0

    Returns the shape of the tabs.
*/

/*!
    \since 6.0

    Sets the shape of the tabs to \a s.
*/
void QTabWidget::setTabShape(TabShape s) {
    if (m_shape == s)
        return;
    m_shape = s;
    applyShape();
}

void QTabWidget::applyShape() {
    m_bar->setShape(static_cast<QTabBar::Shape>((m_shape == Triangular ? 4 : 0) + static_cast<int>(m_position)));
    layoutChildren();
    updateGeometry();
    update();
}

/*!
    \since 6.0

    Returns the widget at the \a corner of the row of tabs, or \c nullptr.
*/
QWidget *QTabWidget::cornerWidget(Qt::Corner corner) const {
    return (corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner) ? m_leftCorner.data()
                                                                           : m_rightCorner.data();
}

/*!
    \since 6.0

    Puts \a widget at the end of the row of tabs that \a corner names: a left corner before the first tab, a
    right corner after the space of the tabs. The tab widget becomes the parent of \a widget. The widget that was
    there is not deleted; \c nullptr removes it.

    \code
    auto *add = new QToolButton;
    add->setText("+");
    tabs->setCornerWidget(add, Qt::TopRightCorner);
    \endcode
*/
void QTabWidget::setCornerWidget(QWidget *widget, Qt::Corner corner) {
    QPointer<QWidget> &slot =
        (corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner) ? m_leftCorner : m_rightCorner;
    if (slot == widget)
        return;
    if (slot)
        slot->hide();
    slot = widget;
    if (widget) {
        widget->setParent(this);
        widget->show();
    }
    layoutChildren();
    updateGeometry();
}

/*!
    \since 6.0

    Returns the size of the icons of the tabs.
*/
QSize QTabWidget::iconSize() const { return m_bar->iconSize(); }

/*!
    \since 6.0

    Sets the size of the icons of the tabs to \a size.
*/
void QTabWidget::setIconSize(const QSize &size) {
    m_bar->setIconSize(size);
    layoutChildren();
    updateGeometry();
}

/*!
    \since 6.0

    Returns how the text of tabs that do not fit is shortened.
*/
Qt::TextElideMode QTabWidget::elideMode() const { return m_bar->elideMode(); }

/*!
    \since 6.0

    Sets how the text of tabs that do not fit is shortened to \a mode.
*/
void QTabWidget::setElideMode(Qt::TextElideMode mode) { m_bar->setElideMode(mode); }

/*!
    \since 6.0

    Returns whether buttons would scroll tabs that do not fit; see QTabBar::usesScrollButtons().
*/
bool QTabWidget::usesScrollButtons() const { return m_bar->usesScrollButtons(); }

/*!
    \since 6.0

    Sets whether buttons scroll tabs that do not fit to \a useButtons.
*/
void QTabWidget::setUsesScrollButtons(bool useButtons) { m_bar->setUsesScrollButtons(useButtons); }

/*!
    \since 6.0

    Returns \c true if the tabs are hidden while there are fewer than two.
*/
bool QTabWidget::tabBarAutoHide() const { return m_bar->autoHide(); }

/*!
    \since 6.0

    Hides the tabs while there are fewer than two if \a enabled is \c true.
*/
void QTabWidget::setTabBarAutoHide(bool enabled) {
    m_bar->setAutoHide(enabled);
    layoutChildren();
    updateGeometry();
}

/*!
    \reimp
*/
bool QTabWidget::hasHeightForWidth() const {
    return std::any_of(m_pages.begin(), m_pages.end(), [](const QWidget *page) { return page->hasHeightForWidth(); });
}

/*!
    \reimp
*/
int QTabWidget::heightForWidth(int width) const {
    int pages = -1;
    const int across = verticalBar() ? width - barThickness() - 1 : width - 2;
    for (const QWidget *page : m_pages)
        if (page->hasHeightForWidth())
            pages = std::max(pages, page->heightForWidth(across));
    if (pages < 0)
        return -1;
    return verticalBar() ? pages + 2 : pages + barThickness() + 1;
}

/*!
    \since 6.0

    Replaces the tab bar with \a tb, which takes over the tabs. The tab widget takes ownership of \a tb and deletes
    the old bar.
*/
void QTabWidget::setTabBar(QTabBar *tb) {
    if (!tb || tb == m_bar)
        return;
    QTabBar *old = m_bar;
    QObject::disconnect(old, nullptr, this, nullptr);
    delete m_scrim.data(); // that of the old bar, if it was a modal rail
    while (tb->count() > 0)
        tb->removeTab(tb->count() - 1);
    for (int i = 0; i < old->count(); ++i) {
        tb->addTab(old->tabIcon(i), old->tabText(i));
        tb->setTabEnabled(i, old->isTabEnabled(i));
        tb->setTabToolTip(i, old->tabToolTip(i));
        tb->setTabWhatsThis(i, old->tabWhatsThis(i));
        tb->setTabVisible(i, old->isTabVisible(i));
    }
    tb->setCurrentIndex(old->currentIndex());
    tb->setTabsClosable(old->tabsClosable());
    tb->setMovable(old->isMovable());
    tb->setDocumentMode(old->documentMode());
    tb->setShape(old->shape());
    tb->setParent(this);
    tb->setFocusPolicy(Qt::StrongFocus);
    m_bar = tb;
    connectBar();
    delete old;
    tb->show();
    layoutChildren();
    updateGeometry();
}

/*!
    \since 6.0

    Is called after a tab is inserted at \a index. The default does nothing.
*/
void QTabWidget::tabInserted(int index) { (void)index; }

/*!
    \since 6.0

    Is called after the tab at \a index is removed. The default does nothing.
*/
void QTabWidget::tabRemoved(int index) { (void)index; }

/*!
    \reimp
*/
void QTabWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    layoutChildren();
}

/*!
    \reimp
*/
void QTabWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        layoutChildren();
        updateGeometry();
    }
    update();
    QWidget::changeEvent(event);
}

/*!
    \since 6.0

    Returns whether the tab widget is drawn as for the documents of a main window.
*/
bool QTabWidget::documentMode() const { return m_bar->documentMode(); }

/*!
    \since 6.0

    Sets whether the tab widget is drawn as for documents to \a set, as QTabBar::setDocumentMode() does.
*/
void QTabWidget::setDocumentMode(bool set) { m_bar->setDocumentMode(set); }

#include "moc_qtabwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
