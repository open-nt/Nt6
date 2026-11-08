// MIT License - see LICENSE
#include <QtWidgets/qtabbar.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

namespace {

constexpr int kCloseSize = 14; // the close button of a tab
constexpr int kDragDistance = 4;

} // namespace

/*!
    \class QTabBar
    \brief The QTabBar class provides a row of tabs.

    A tab bar shows tabs with a text, one of which is current. QTabWidget uses one above its pages; a QTabBar on its
    own suits a view that changes with the tab:

    \code
    QTabBar *bar = new QTabBar(window);
    bar->addTab("Day");
    bar->addTab("Week");
    connect(bar, &QTabBar::currentChanged, [](int index) { showRange(index); });
    \endcode

    The user clicks a tab, turns the mouse wheel over the bar, or uses Left, Right, Home and End while it has the focus;
    disabled tabs are skipped. With setTabsClosable() each tab has a close button that emits tabCloseRequested(), and a
    movable tab bar lets the user drag the tabs into another order.

    \differences
    The tabs are always at the top and rounded: there is no \c setShape(), no icons, no \c setTabButton(),
    \c setElideMode(), \c setExpanding(), \c setUsesScrollButtons(), \c setDrawBase() or \c setDocumentMode(), and no
    tab colors. Tabs that do not fit are cut off. The close button is drawn as an "x".
    \enddifferences

    \sa QTabWidget
*/

/*!
    \enum QTabBar::SelectionBehavior

    Which tab becomes current when the current tab is removed.

    \value SelectLeftTab The tab at its left.
    \value SelectRightTab The tab at its right, the default.
    \value SelectPreviousTab The tab that was current before it.
*/

/*!
    Constructs an empty tab bar with the given \a parent.
*/
QTabBar::QTabBar(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::TabFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

/*!
    Destroys the tab bar.
*/
QTabBar::~QTabBar() = default;

/*!
    Adds a tab with the given \a text at the end and returns its index. The first tab becomes current.

    \sa insertTab()
*/
int QTabBar::addTab(const QString &text) { return insertTab(count(), text); }

/*!
    Inserts a tab with the given \a text at \a index, or at the end if \a index is out of range, and returns its
    index.

    \sa removeTab()
*/
int QTabBar::insertTab(int index, const QString &text) {
    const int at = valid(index) ? index : count();
    Tab tab;
    tab.text = text;
    m_tabs.insert(m_tabs.begin() + at, tab);
    for (int &h : m_history)
        if (h >= at)
            ++h;
    if (m_current >= at)
        ++m_current;
    tabInserted(at);
    if (m_current < 0)
        setCurrentIndex(at);
    changed();
    return at;
}

/*!
    Removes the tab at \a index. If it was current, another tab becomes current as selectionBehaviorOnRemove() says.

    \sa insertTab()
*/
void QTabBar::removeTab(int index) {
    if (!valid(index))
        return;
    const bool wasCurrent = index == m_current;
    m_tabs.erase(m_tabs.begin() + index);
    m_history.erase(std::remove(m_history.begin(), m_history.end(), index), m_history.end());
    for (int &h : m_history)
        if (h > index)
            --h;
    m_hover = -1;
    if (index < m_current) {
        --m_current;
    } else if (wasCurrent) {
        m_current = -1;
        int next = -1;
        if (count() > 0) {
            switch (m_onRemove) {
            case SelectLeftTab:
                next = std::max(0, index - 1);
                break;
            case SelectRightTab:
                next = std::min(index, count() - 1);
                break;
            case SelectPreviousTab:
                next = m_history.empty() ? std::min(index, count() - 1) : m_history.back();
                break;
            }
        }
        if (next >= 0) {
            setCurrentIndex(next);
        } else {
            emit currentChanged(-1);
        }
    }
    tabRemoved(index);
    changed();
}

/*!
    Moves the tab at \a from to \a to and emits tabMoved(). The current tab stays current.
*/
void QTabBar::moveTab(int from, int to) {
    if (!valid(from) || !valid(to) || from == to)
        return;
    const Tab tab = m_tabs[static_cast<std::size_t>(from)];
    m_tabs.erase(m_tabs.begin() + from);
    m_tabs.insert(m_tabs.begin() + to, tab);
    const auto remap = [from, to](int i) {
        if (i == from)
            return to;
        if (from < to && i > from && i <= to)
            return i - 1;
        if (from > to && i >= to && i < from)
            return i + 1;
        return i;
    };
    m_current = remap(m_current);
    for (int &h : m_history)
        h = remap(h);
    emit tabMoved(from, to);
    changed();
}

/*!
    Returns whether the tab at \a index is enabled. A disabled tab cannot be made current by the user.

    \sa setTabEnabled()
*/
bool QTabBar::isTabEnabled(int index) const { return valid(index) && m_tabs[static_cast<std::size_t>(index)].enabled; }

/*!
    Enables the tab at \a index if \a enabled is \c true, and disables it otherwise.
*/
void QTabBar::setTabEnabled(int index, bool enabled) {
    if (!valid(index))
        return;
    m_tabs[static_cast<std::size_t>(index)].enabled = enabled;
    update();
}

/*!
    Returns the text of the tab at \a index.

    \sa setTabText()
*/
QString QTabBar::tabText(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].text : QString();
}

/*!
    Sets the text of the tab at \a index to \a text.
*/
void QTabBar::setTabText(int index, const QString &text) {
    if (!valid(index))
        return;
    m_tabs[static_cast<std::size_t>(index)].text = text;
    changed();
}

/*!
    Returns the tool tip of the tab at \a index.

    \sa setTabToolTip()
*/
QString QTabBar::tabToolTip(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].toolTip : QString();
}

/*!
    Sets the tool tip of the tab at \a index to \a tip.
*/
void QTabBar::setTabToolTip(int index, const QString &tip) {
    if (valid(index))
        m_tabs[static_cast<std::size_t>(index)].toolTip = tip;
}

/*!
    Returns the data of the tab at \a index.

    \sa setTabData()
*/
QVariant QTabBar::tabData(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].data : QVariant();
}

/*!
    Sets the data of the tab at \a index to \a data, for the application's own use.
*/
void QTabBar::setTabData(int index, const QVariant &data) {
    if (valid(index))
        m_tabs[static_cast<std::size_t>(index)].data = data;
}

/*!
    Returns the rectangle of the tab at \a index in the tab bar, or an empty rectangle if there is no such tab.
*/
QRect QTabBar::tabRect(int index) const {
    if (!valid(index))
        return QRect();
    int x = 0;
    for (int i = 0; i < index; ++i)
        x += tabSizeHint(i).width();
    return QRect(x, 0, tabSizeHint(index).width(), height());
}

/*!
    Returns the index of the tab at \a position, or -1 if there is none.
*/
int QTabBar::tabAt(const QPoint &position) const {
    for (int i = 0; i < count(); ++i)
        if (tabRect(i).contains(position))
            return i;
    return -1;
}

/*!
    \fn int QTabBar::currentIndex() const

    Returns the index of the current tab, or -1 if there are no tabs.
*/

/*!
    \fn int QTabBar::count() const

    Returns the number of tabs.
*/

/*!
    \fn bool QTabBar::tabsClosable() const

    Returns whether the tabs have close buttons. The default is \c false.

    \sa setTabsClosable()
*/

/*!
    Sets whether the tabs have close buttons to \a closable.

    \sa tabCloseRequested()
*/
void QTabBar::setTabsClosable(bool closable) {
    if (m_closable == closable)
        return;
    m_closable = closable;
    changed();
}

/*!
    \fn bool QTabBar::isMovable() const

    Returns whether the user can drag the tabs into another order. The default is \c false.
*/

/*!
    \fn void QTabBar::setMovable(bool movable)

    Sets whether the user can drag the tabs into another order to \a movable.
*/

/*!
    \fn QTabBar::SelectionBehavior QTabBar::selectionBehaviorOnRemove() const

    Returns which tab becomes current when the current one is removed, SelectRightTab by default.
*/

/*!
    \fn void QTabBar::setSelectionBehaviorOnRemove(SelectionBehavior behavior)

    Sets which tab becomes current when the current one is removed to \a behavior.
*/

/*!
    \reimp
*/
QSize QTabBar::sizeHint() const {
    int width = 0;
    int height = 0;
    for (int i = 0; i < count(); ++i) {
        const QSize s = tabSizeHint(i);
        width += s.width();
        height = std::max(height, s.height());
    }
    if (count() == 0) {
        QStyleOptionTab option;
        option.initFrom(this);
        height =
            style()->sizeFromContents(QStyle::CT_TabBarTab, &option, QSize(0, fontMetrics().height()), this).height();
    }
    return QSize(width, height);
}

/*!
    \reimp
*/
QSize QTabBar::minimumSizeHint() const { return QSize(0, sizeHint().height()); }

/*!
    Makes the tab at \a index current and emits currentChanged(), if it is another tab.

    \sa currentIndex()
*/
void QTabBar::setCurrentIndex(int index) {
    if (!valid(index) || index == m_current)
        return;
    if (m_current >= 0) {
        m_history.erase(std::remove(m_history.begin(), m_history.end(), m_current), m_history.end());
        m_history.push_back(m_current);
    }
    m_current = index;
    update();
    emit currentChanged(index);
}

/*!
    Returns the size of the tab at \a index. Reimplement it for tabs of another size.
*/
QSize QTabBar::tabSizeHint(int index) const {
    QStyleOptionTab option;
    option.initFrom(this);
    option.text = tabText(index);
    QSize text = fontMetrics().size(Qt::TextSingleLine, option.text);
    if (m_closable)
        text.setWidth(text.width() + kCloseSize + 4);
    return style()->sizeFromContents(QStyle::CT_TabBarTab, &option, text, this);
}

/*!
    Is called after a tab is inserted at \a index. The default implementation does nothing.
*/
void QTabBar::tabInserted(int index) { (void)index; }

/*!
    Is called after the tab at \a index is removed. The default implementation does nothing.
*/
void QTabBar::tabRemoved(int index) { (void)index; }

/*!
    Is called when the sizes of the tabs change. The default implementation does nothing.
*/
void QTabBar::tabLayoutChange() {}

void QTabBar::changed() {
    tabLayoutChange();
    updateGeometry();
    update();
}

QRect QTabBar::closeRect(int index) const {
    if (!m_closable || !valid(index))
        return QRect();
    const QRect tab = tabRect(index);
    return QRect(tab.right() - kCloseSize - 4, tab.top() + (tab.height() - kCloseSize) / 2, kCloseSize, kCloseSize);
}

// Makes the next enabled tab in `direction` current.
void QTabBar::step(int direction) {
    for (int i = m_current + direction; valid(i); i += direction) {
        if (isTabEnabled(i)) {
            setCurrentIndex(i);
            return;
        }
    }
}

/*!
    \reimp

    A tool tip event over a tab shows the tool tip of the tab.
*/
bool QTabBar::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int index = tabAt(help->pos());
        if (index >= 0 && !tabToolTip(index).isEmpty()) {
            QToolTip::showText(help->globalPos(), tabToolTip(index), this, tabRect(index));
            return true;
        }
        event->ignore();
        return true;
    }
    return QWidget::event(event);
}

/*!
    \reimp
*/
void QTabBar::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    // The current tab, and a tab that is dragged, are drawn last so that they overlap their neighbours.
    const auto drawTab = [&](int i) {
        QStyleOptionTab option;
        option.initFrom(this);
        option.rect = tabRect(i).translated(i == m_dragging ? m_dragOffset : 0, 0);
        option.text = tabText(i);
        option.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_Enabled);
        if (isTabEnabled(i) && isEnabled())
            option.state |= QStyle::State_Enabled;
        if (i == m_current) {
            option.state |= QStyle::State_Selected;
            if (hasFocus())
                option.state |= QStyle::State_HasFocus;
        }
        if (i == m_hover)
            option.state |= QStyle::State_MouseOver;
        if (m_closable) {
            // The tab without its text, then the text beside the close button.
            QStyleOptionTab shape = option;
            shape.text.clear();
            style()->drawControl(QStyle::CE_TabBarTab, &shape, &painter, this);
            const QPalette::ColorGroup textGroup =
                (option.state & QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled;
            painter.setPen(palette().color(textGroup, QPalette::WindowText));
            painter.drawText(option.rect.adjusted(4, 0, -(kCloseSize + 6), 0), Qt::AlignCenter | Qt::TextSingleLine,
                             option.text);
        } else {
            style()->drawControl(QStyle::CE_TabBarTab, &option, &painter, this);
        }
        if (m_closable) {
            const QRect close = closeRect(i).translated(i == m_dragging ? m_dragOffset : 0, 0);
            const QPalette::ColorGroup group =
                (option.state & QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled;
            if (i == m_pressedClose)
                painter.fillRect(close, palette().color(group, QPalette::Mid));
            painter.setPen(palette().color(group, QPalette::ButtonText));
            painter.drawLine(close.left() + 3, close.top() + 3, close.right() - 3, close.bottom() - 3);
            painter.drawLine(close.right() - 3, close.top() + 3, close.left() + 3, close.bottom() - 3);
        }
    };
    for (int i = 0; i < count(); ++i)
        if (i != m_current && i != m_dragging)
            drawTab(i);
    if (m_current >= 0 && m_current != m_dragging)
        drawTab(m_current);
    if (m_dragging >= 0)
        drawTab(m_dragging);
}

/*!
    \reimp
*/
void QTabBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int index = tabAt(event->pos());
    emit tabBarClicked(index);
    if (index < 0) {
        event->accept();
        return;
    }
    if (closeRect(index).contains(event->pos())) {
        m_pressedClose = index;
        update();
        event->accept();
        return;
    }
    if (isTabEnabled(index))
        setCurrentIndex(index);
    if (m_movable) {
        m_dragging = index;
        m_pressX = event->pos().x();
        m_dragOffset = 0;
    }
    event->accept();
}

/*!
    \reimp

    Dragging a tab of a movable tab bar past the middle of its neighbour swaps them.
*/
void QTabBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging >= 0 && (event->buttons() & Qt::LeftButton)) {
        int offset = event->pos().x() - m_pressX;
        if (std::abs(offset) < kDragDistance && m_dragOffset == 0) {
            event->accept();
            return;
        }
        while (offset > 0 && valid(m_dragging + 1) && offset > tabRect(m_dragging + 1).width() / 2) {
            const int width = tabRect(m_dragging + 1).width();
            moveTab(m_dragging, m_dragging + 1);
            ++m_dragging;
            m_pressX += width;
            offset -= width;
        }
        while (offset < 0 && valid(m_dragging - 1) && -offset > tabRect(m_dragging - 1).width() / 2) {
            const int width = tabRect(m_dragging - 1).width();
            moveTab(m_dragging, m_dragging - 1);
            --m_dragging;
            m_pressX -= width;
            offset += width;
        }
        m_dragOffset = offset;
        update();
        event->accept();
        return;
    }
    const int index = tabAt(event->pos());
    if (index != m_hover) {
        m_hover = index;
        update();
    }
    event->accept();
}

/*!
    \reimp

    Releasing the mouse on the close button that was pressed emits tabCloseRequested().
*/
void QTabBar::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    if (m_pressedClose >= 0) {
        const int index = m_pressedClose;
        m_pressedClose = -1;
        update();
        if (closeRect(index).contains(event->pos()))
            emit tabCloseRequested(index);
    }
    if (m_dragging >= 0) {
        m_dragging = -1;
        m_dragOffset = 0;
        update();
    }
    event->accept();
}

/*!
    \reimp
*/
void QTabBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    emit tabBarDoubleClicked(tabAt(event->pos()));
    event->accept();
}

/*!
    \reimp

    Left and Right move to the previous and next enabled tab, Home and End to the first and last.
*/
void QTabBar::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Left:
        step(-1);
        break;
    case Qt::Key_Right:
        step(1);
        break;
    case Qt::Key_Home:
        for (int i = 0; i < count(); ++i)
            if (isTabEnabled(i)) {
                setCurrentIndex(i);
                break;
            }
        break;
    case Qt::Key_End:
        for (int i = count() - 1; i >= 0; --i)
            if (isTabEnabled(i)) {
                setCurrentIndex(i);
                break;
            }
        break;
    default:
        event->ignore();
        return;
    }
    event->accept();
}

/*!
    \reimp

    The wheel moves to the previous or next enabled tab.
*/
void QTabBar::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0) {
        event->ignore();
        return;
    }
    step(notches > 0 ? -1 : 1);
    event->accept();
}

/*!
    \reimp
*/
void QTabBar::leaveEvent(QEvent *event) {
    m_hover = -1;
    update();
    QWidget::leaveEvent(event);
}

/*!
    \reimp
*/
void QTabBar::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        changed();
    QWidget::changeEvent(event);
}

#include "moc_qtabbar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
