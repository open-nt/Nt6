// MIT License - see LICENSE
#include <QtWidgets/qstackedlayout.h>

#include "qwidgetutil_p.h"

#include <QtCore/qdebug.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

/*!
    \class QStackedLayout
    \brief The QStackedLayout class lays out widgets so that only one of them is visible at a time.

    All the widgets of a stacked layout have the size of the layout, and the current one is shown while the others
    are hidden. A QStackedWidget has one of these, and a QComboBox or a list can choose the page:

    \code
    QStackedLayout *pages = new QStackedLayout(window);
    pages->addWidget(generalPage);
    pages->addWidget(advancedPage);
    connect(combo, &QComboBox::currentIndexChanged, pages, &QStackedLayout::setCurrentIndex);
    \endcode

    The size hint of the layout is the largest hint of its widgets, so the window has room for any page.

    \differences
    There is no \c stackingMode(): only the current widget is shown, as in Qt's default \c StackOne. A stacked layout
    can only hold widgets: \c addItem() takes the widget of the item. It has no contents margins by default.
    \enddifferences

    \sa QStackedWidget, QTabWidget, QLayout
*/

/*!
    Constructs a stacked layout for \a parent, with no widgets.
*/
QStackedLayout::QStackedLayout(QWidget *parent) : QLayout(parent) { setContentsMargins(0, 0, 0, 0); }

/*!
    Destroys the layout. The widgets that are in it stay.
*/
QStackedLayout::~QStackedLayout() {
    if (auto *parentLayout = dynamic_cast<QLayout *>(parent()))
        parentLayout->removeItem(this);
    for (QLayoutItem *item : m_items)
        delete item;
}

/*!
    Adds \a widget at the end of the layout and returns its index. If it is the first widget, it becomes the current one.

    \sa insertWidget()
*/
int QStackedLayout::addWidget(QWidget *widget) { return insertWidget(count(), widget); }

/*!
    Inserts \a widget at \a index, which is brought into the range 0 to count(), and returns the index it got. It
    becomes the current widget if there is none; otherwise it is hidden. The layout makes it a child of the widget it
    manages.

    \sa addWidget()
*/
int QStackedLayout::insertWidget(int index, QWidget *widget) {
    if (!widget)
        return -1;
    index = std::clamp(index, 0, count());
    addChildWidget(widget);
    m_items.insert(m_items.begin() + index, new QWidgetItem(widget));
    if (m_current < 0) {
        m_current = index;
        widget->show();
        emit currentChanged(m_current);
    } else {
        if (index <= m_current)
            ++m_current;
        widget->hide();
    }
    invalidate();
    return index;
}

/*!
    Returns the widget at \a index, or \c nullptr if there is none.
*/
QWidget *QStackedLayout::widget(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)]->widget() : nullptr;
}

/*!
    Returns the current widget, or \c nullptr if there is none.

    \sa currentIndex()
*/
QWidget *QStackedLayout::currentWidget() const { return widget(m_current); }

/*!
    \fn int QStackedLayout::currentIndex() const

    Returns the index of the current widget, or -1 if there is none.

    \sa setCurrentIndex()
*/

/*!
    Makes the widget at \a index the current one: it is shown and the one before is hidden. Does nothing if there is
    no widget at \a index. Emits currentChanged().

    \sa currentIndex()
*/
void QStackedLayout::setCurrentIndex(int index) {
    if (index < 0 || index >= count() || index == m_current)
        return;
    QWidget *previous = currentWidget();
    m_current = index;
    QWidget *next = currentWidget();
    if (previous)
        previous->hide();
    // The new page has its size before it is shown.
    if (m_rect.isValid())
        m_items[static_cast<std::size_t>(index)]->setGeometry(m_rect.marginsRemoved(contentsMargins()));
    next->show();
    next->raise();
    emit currentChanged(m_current);
}

/*!
    Makes \a widget the current one. Does nothing if it is not in the layout.

    \sa setCurrentIndex()
*/
void QStackedLayout::setCurrentWidget(QWidget *widget) { setCurrentIndex(indexOf(widget)); }

/*!
    \reimp

    A stacked layout holds widgets only: the widget of \a item is added, and \a item is deleted. An item that is not a
    widget is ignored.
*/
void QStackedLayout::addItem(QLayoutItem *item) {
    if (!item)
        return;
    QWidget *w = item->widget();
    if (!w)
        qWarning("QStackedLayout::addItem: only widgets can be added");
    delete item;
    if (w)
        addWidget(w);
}

/*!
    \reimp
*/
QLayoutItem *QStackedLayout::itemAt(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)] : nullptr;
}

/*!
    \reimp

    If the widget was the current one, the next widget becomes the current one, or the one before if it was the
    last. Emits widgetRemoved(), and currentChanged() if the current widget was the one removed.
*/
QLayoutItem *QStackedLayout::takeAt(int index) {
    if (index < 0 || index >= count())
        return nullptr;
    QLayoutItem *item = m_items[static_cast<std::size_t>(index)];
    m_items.erase(m_items.begin() + index);
    const int old = m_current;
    if (m_items.empty()) {
        m_current = -1;
    } else if (index < m_current) {
        --m_current;
    } else if (index == m_current) {
        m_current = std::min(index, count() - 1);
        if (QWidget *next = currentWidget()) {
            if (m_rect.isValid())
                m_items[static_cast<std::size_t>(m_current)]->setGeometry(m_rect.marginsRemoved(contentsMargins()));
            next->show();
        }
    }
    emit widgetRemoved(index);
    if (index == old)
        emit currentChanged(m_current);
    invalidate();
    return item;
}

/*!
    \reimp
*/
QSize QStackedLayout::sizeHint() const {
    QSize size(0, 0);
    for (const QLayoutItem *item : m_items)
        size = QtPrivate::expandedTo(size, item->sizeHint());
    const QMargins m = contentsMargins();
    return QSize(size.width() + m.left() + m.right(), size.height() + m.top() + m.bottom());
}

/*!
    \reimp
*/
QSize QStackedLayout::minimumSize() const {
    QSize size(0, 0);
    for (const QLayoutItem *item : m_items)
        size = QtPrivate::expandedTo(size, item->minimumSize());
    const QMargins m = contentsMargins();
    return QSize(size.width() + m.left() + m.right(), size.height() + m.top() + m.bottom());
}

/*!
    \reimp
*/
Qt::Orientations QStackedLayout::expandingDirections() const { return Qt::Horizontal | Qt::Vertical; }

/*!
    \reimp

    Gives the current widget the whole rectangle, within the margins.
*/
void QStackedLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    if (m_current >= 0)
        m_items[static_cast<std::size_t>(m_current)]->setGeometry(rect.marginsRemoved(contentsMargins()));
}

#include "moc_qstackedlayout.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
