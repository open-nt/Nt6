// MIT License - see LICENSE
#include <QtWidgets/qtabwidget.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include "qwidgetutil_p.h"

#include <algorithm>

// The row of tabs of a QTabWidget, a child of it.
class QTabBarWidget : public QWidget {
public:
    explicit QTabBarWidget(QTabWidget *owner) : QWidget(owner), m_owner(owner) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

    QSize tabSize(int index) const {
        QStyleOptionTab option;
        option.initFrom(this);
        option.text = m_owner->m_tabs[static_cast<std::size_t>(index)].text;
        const QFontMetrics fm = fontMetrics();
        return style()->sizeFromContents(QStyle::CT_TabBarTab, &option, fm.size(Qt::TextSingleLine, option.text), this);
    }
    int barHeight() const {
        QStyleOptionTab option;
        option.initFrom(this);
        const QFontMetrics fm = fontMetrics();
        return style()->sizeFromContents(QStyle::CT_TabBarTab, &option, QSize(0, fm.height()), this).height();
    }
    int tabsWidth() const {
        int width = 0;
        for (int i = 0; i < m_owner->count(); ++i)
            width += tabSize(i).width();
        return width;
    }
    QRect tabRect(int index) const {
        int x = 0;
        for (int i = 0; i < index; ++i)
            x += tabSize(i).width();
        return QRect(x, 0, tabSize(index).width(), height());
    }
    int tabAt(const QPoint &pos) const {
        for (int i = 0; i < m_owner->count(); ++i)
            if (tabRect(i).contains(pos))
                return i;
        return -1;
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setFont(font());
        // The selected tab is drawn last so that it overlaps its neighbours.
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < m_owner->count(); ++i) {
                const bool selected = i == m_owner->currentIndex();
                if (selected != (pass == 1))
                    continue;
                QStyleOptionTab option;
                option.initFrom(this);
                option.rect = tabRect(i);
                option.text = m_owner->m_tabs[static_cast<std::size_t>(i)].text;
                option.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_Enabled);
                if (m_owner->m_tabs[static_cast<std::size_t>(i)].enabled && isEnabled())
                    option.state |= QStyle::State_Enabled;
                if (selected) {
                    option.state |= QStyle::State_Selected;
                    if (hasFocus())
                        option.state |= QStyle::State_HasFocus;
                }
                if (i == m_hover)
                    option.state |= QStyle::State_MouseOver;
                style()->drawControl(QStyle::CE_TabBarTab, &option, &painter, this);
            }
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        const int index = tabAt(event->pos());
        if (event->button() != Qt::LeftButton || index < 0) {
            event->ignore();
            return;
        }
        if (m_owner->isTabEnabled(index))
            m_owner->setCurrentIndex(index);
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        const int index = tabAt(event->pos());
        if (index != m_hover) {
            m_hover = index;
            update();
        }
        event->accept();
    }
    void leaveEvent(QEvent *event) override {
        m_hover = -1;
        update();
        QWidget::leaveEvent(event);
    }
    void keyPressEvent(QKeyEvent *event) override {
        const int key = event->key();
        if (key != Qt::Key_Left && key != Qt::Key_Right) {
            event->ignore();
            return;
        }
        const int step = key == Qt::Key_Right ? 1 : -1;
        for (int i = m_owner->currentIndex() + step; i >= 0 && i < m_owner->count(); i += step) {
            if (m_owner->isTabEnabled(i)) {
                m_owner->setCurrentIndex(i);
                break;
            }
        }
        event->accept();
    }

private:
    QTabWidget *m_owner;
    int m_hover = -1;
};

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
    There is no \c QTabBar class, and the tabs are always above the pages: no \c setTabPosition(), \c setTabShape(),
    \c setMovable(), \c setTabsClosable(), \c setDocumentMode(), tab icons, tool tips, corner widgets
    (\c setCornerWidget()) or \c setUsesScrollButtons(); tabs that do not fit are cut off. Ctrl+Tab does not switch
    tabs. A change of the size hint of a page does not update the layout the tab widget is in.
    \enddifferences

    \sa QGroupBox, QStyle
*/

/*!
    Constructs an empty tab widget with the given \a parent.
*/
QTabWidget::QTabWidget(QWidget *parent) : QWidget(parent) {
    m_bar = new QTabBarWidget(this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

/*!
    Destroys the tab widget and the pages.
*/
QTabWidget::~QTabWidget() {
    // The pages are deleted as children of the widget, after this object is gone.
    for (const Tab &tab : m_tabs)
        QObject::disconnect(tab.page, &QObject::destroyed, this, nullptr);
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
    m_tabs.insert(m_tabs.begin() + at, Tab{page, label, true});
    QObject::connect(page, &QObject::destroyed, this, [this](QObject *gone) { pageGone(gone); });
    if (m_current < 0) {
        select(at);
    } else if (at <= m_current) {
        ++m_current;
    }
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
    QWidget *page = m_tabs[static_cast<std::size_t>(index)].page;
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
    for (std::size_t i = 0; i < m_tabs.size(); ++i) {
        if (static_cast<QObject *>(m_tabs[i].page) != page)
            continue;
        const int index = static_cast<int>(i);
        const bool wasCurrent = index == m_current;
        m_tabs.erase(m_tabs.begin() + index);
        if (index < m_current) {
            --m_current;
        } else if (wasCurrent) {
            m_current = -1;
            if (count() > 0)
                select(std::min(index, count() - 1));
            else
                emit currentChanged(-1);
        }
        updateGeometry();
        m_bar->update();
        update();
        return;
    }
}

/*!
    \fn int QTabWidget::count() const

    Returns the number of tabs.
*/

/*!
    \fn int QTabWidget::currentIndex() const

    Returns the index of the current tab, or -1 if there are none.

    \sa setCurrentIndex()
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
    return index >= 0 && index < count() ? m_tabs[static_cast<std::size_t>(index)].page : nullptr;
}

/*!
    Returns the index of the tab that has \a page, or -1 if there is none.

    \sa widget()
*/
int QTabWidget::indexOf(const QWidget *page) const {
    for (int i = 0; i < count(); ++i)
        if (m_tabs[static_cast<std::size_t>(i)].page == page)
            return i;
    return -1;
}

/*!
    Returns the text of the tab at \a index, or an empty string if there is no such tab.

    \sa setTabText()
*/
QString QTabWidget::tabText(int index) const {
    return index >= 0 && index < count() ? m_tabs[static_cast<std::size_t>(index)].text : QString();
}

/*!
    Sets the text of the tab at \a index to \a text.

    \sa tabText()
*/
void QTabWidget::setTabText(int index, const QString &text) {
    if (index < 0 || index >= count())
        return;
    m_tabs[static_cast<std::size_t>(index)].text = text;
    layoutChildren();
    m_bar->update();
}

/*!
    Returns whether the tab at \a index can be chosen. The tabs are enabled at first.

    \sa setTabEnabled()
*/
bool QTabWidget::isTabEnabled(int index) const {
    return index >= 0 && index < count() && m_tabs[static_cast<std::size_t>(index)].enabled;
}

/*!
    Sets whether the tab at \a index can be chosen to \a enabled. A disabled tab is drawn grayed out and the
    user cannot switch to it. A tab that is current stays so.

    \sa isTabEnabled()
*/
void QTabWidget::setTabEnabled(int index, bool enabled) {
    if (index < 0 || index >= count())
        return;
    m_tabs[static_cast<std::size_t>(index)].enabled = enabled;
    m_bar->update();
}

bool QTabWidget::focusIn(const QWidget *page) const {
    for (const QWidget *w = focusWidget(); w; w = w->parentWidget())
        if (w == page)
            return true;
    return false;
}

// Makes the tab at `index` current: the page of the old one is hidden, the page of the new one is shown.
void QTabWidget::select(int index) {
    if (index == m_current)
        return;
    QWidget *old = widget(m_current);
    const bool hadFocus = old && focusIn(old);
    if (old)
        old->hide();
    m_current = index;
    if (QWidget *page = widget(index)) {
        page->show();
        if (hadFocus)
            m_bar->setFocus();
    }
    m_bar->update();
    update();
    emit currentChanged(index);
}

/*!
    Makes the tab at \a index the current one and shows its page. An \a index that is not a tab is ignored.
    Emits currentChanged() if the tab changes.

    \sa currentIndex()
*/
void QTabWidget::setCurrentIndex(int index) {
    if (index < 0 || index >= count())
        return;
    select(index);
}

/*!
    Makes the tab of \a page the current one. Nothing happens if \a page is not in a tab.

    \sa currentWidget(), setCurrentIndex()
*/
void QTabWidget::setCurrentWidget(QWidget *page) { setCurrentIndex(indexOf(page)); }

void QTabWidget::layoutChildren() {
    const int bar = m_bar->barHeight();
    m_bar->setGeometry(0, 0, width(), bar);
    // The frame of the pages is 1 pixel wide, and its top edge is hidden behind the current tab.
    const QRect pages(1, bar, std::max(0, width() - 2), std::max(0, height() - bar - 1));
    for (const Tab &tab : m_tabs)
        tab.page->setGeometry(pages);
}

/*!
    \reimp
*/
QSize QTabWidget::sizeHint() const {
    QSize pages(0, 0);
    for (const Tab &tab : m_tabs)
        pages = QtPrivate::expandedTo(pages, tab.page->sizeHint().isValid() ? tab.page->sizeHint() : QSize(0, 0));
    return QSize(std::max(m_bar->tabsWidth(), pages.width()) + 2, m_bar->barHeight() + pages.height() + 1);
}

/*!
    \reimp
*/
QSize QTabWidget::minimumSizeHint() const {
    QSize pages(0, 0);
    for (const Tab &tab : m_tabs) {
        const QSize min = tab.page->minimumSizeHint();
        pages = QtPrivate::expandedTo(pages, min.isValid() ? min : QSize(0, 0));
    }
    return QSize(std::max(m_bar->tabsWidth(), pages.width()) + 2, m_bar->barHeight() + pages.height() + 1);
}

/*!
    \reimp
*/
void QTabWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    const int bar = m_bar->barHeight();
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
