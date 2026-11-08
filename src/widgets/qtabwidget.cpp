// MIT License - see LICENSE
#include <QtWidgets/qtabwidget.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
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

    \differences
    The tabs are always above the pages: there is no \c setTabPosition(), \c setTabShape(), \c setDocumentMode(),
    tab icons, corner widgets (\c setCornerWidget()), \c setTabBar() or \c setUsesScrollButtons(); tabs that do not
    fit are cut off. Ctrl+Tab does not switch
    tabs. A change of the size hint of a page does not update the layout the tab widget is in.
    \enddifferences

    \sa QGroupBox, QStyle
*/

/*!
    Constructs an empty tab widget with the given \a parent.
*/
QTabWidget::QTabWidget(QWidget *parent) : QWidget(parent) {
    m_bar = new QTabBar(this);
    m_bar->setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
        updateGeometry();
        update();
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

void QTabWidget::layoutChildren() {
    const int bar = m_bar->sizeHint().height();
    m_bar->setGeometry(0, 0, width(), bar);
    // The frame of the pages is 1 pixel wide, and its top edge is hidden behind the current tab.
    const QRect pages(1, bar, std::max(0, width() - 2), std::max(0, height() - bar - 1));
    for (QWidget *page : m_pages)
        page->setGeometry(pages);
}

/*!
    \reimp
*/
QSize QTabWidget::sizeHint() const {
    QSize pages(0, 0);
    for (QWidget *page : m_pages)
        pages = QtPrivate::expandedTo(pages, page->sizeHint().isValid() ? page->sizeHint() : QSize(0, 0));
    const QSize bar = m_bar->sizeHint();
    return QSize(std::max(bar.width(), pages.width()) + 2, bar.height() + pages.height() + 1);
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
    const QSize bar = m_bar->sizeHint();
    return QSize(std::max(bar.width(), pages.width()) + 2, bar.height() + pages.height() + 1);
}

/*!
    \reimp
*/
void QTabWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    const int bar = m_bar->sizeHint().height();
    option.rect = QRect(0, bar - 1, width(), height() - bar + 1);
    style()->drawPrimitive(QStyle::PE_FrameTabWidget, &option, &painter, this);
}

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

#include "moc_qtabwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
