// MIT License - see LICENSE
#include <QtWidgets/qsplitter.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qlayoutitem.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace QtPrivate {

// The bar between two widgets of a splitter, which the user drags.
class SplitterHandle : public QWidget {
public:
    SplitterHandle(QSplitter *splitter, int index) : QWidget(splitter), m_splitter(splitter), m_index(index) {}
    void setIndex(int index) { m_index = index; }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        QStyleOption option;
        option.initFrom(this);
        if (m_splitter->orientation() == Qt::Horizontal)
            option.state |= QStyle::State_Horizontal;
        style()->drawControl(QStyle::CE_Splitter, &option, &painter, this);
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            event->ignore();
            return;
        }
        m_grab = m_splitter->orientation() == Qt::Horizontal ? event->pos().x() : event->pos().y();
        m_dragging = true;
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (!m_dragging) {
            event->ignore();
            return;
        }
        const QPoint p = mapToParent(event->pos());
        const int along = m_splitter->orientation() == Qt::Horizontal ? p.x() : p.y();
        m_splitter->moveHandle(m_index, along - m_grab);
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        m_dragging = false;
        event->accept();
    }

private:
    QSplitter *m_splitter;
    int m_index;
    int m_grab = 0;
    bool m_dragging = false;
};

} // namespace QtPrivate

/*!
    \class QSplitter
    \brief The QSplitter class lets the user change the sizes of the widgets in it by dragging the bars between them.

    A splitter puts its widgets side by side (\c Qt::Horizontal, the default) or one above the other, with a handle between
    each two. The user drags a handle to give one widget more room and the next less. When the splitter is resized, the
    widgets keep their share of its size, or the ones with a stretch factor take all the change:

    \code
    QSplitter *split = new QSplitter(window);
    split->addWidget(list);
    split->addWidget(editor);
    split->setSizes({150, 450});
    split->setStretchFactor(1, 1);
    \endcode

    A widget that is dragged to less than half of its minimum size collapses to nothing, unless it is not collapsible.

    A horizontal splitter with a list and an editor, and a vertical one with two panes.

    \image widgets/qsplitter.png QSplitter in the light color scheme

    \differences
    There is no \c QSplitterHandle class and no \c handle(), \c replaceWidget(), \c saveState() or \c restoreState(), and no
    \c setOpaqueResize(): the widgets always follow the handle. A splitter has no \c refresh(). The cursor does not change
    over a handle.
    \enddifferences

    \sa QScrollArea, QTabWidget
*/

/*!
    Constructs a horizontal splitter with the given \a parent.
*/
QSplitter::QSplitter(QWidget *parent) : QFrame(parent) { init(); }

/*!
    Constructs a splitter with the given \a orientation and \a parent.
*/
QSplitter::QSplitter(Qt::Orientation orientation, QWidget *parent) : QFrame(parent), m_orientation(orientation) { init(); }

void QSplitter::init() { setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); }

/*!
    Destroys the splitter, and the widgets in it.
*/
QSplitter::~QSplitter() { *m_alive = false; }

/*!
    Adds \a widget after the last one, and makes it a child of the splitter.

    \sa insertWidget()
*/
void QSplitter::addWidget(QWidget *widget) { insertWidget(count(), widget); }

/*!
    Inserts \a widget at \a index, which is brought into the range 0 to count(). A widget that is already in the splitter
    moves. The splitter makes \a widget a child of its own and shows it.

    \sa addWidget(), indexOf()
*/
void QSplitter::insertWidget(int index, QWidget *widget) {
    if (!widget)
        return;
    const int existing = indexOf(widget);
    if (existing >= 0) {
        if (existing == index || existing + 1 == index)
            return;
        Item item = m_items[static_cast<std::size_t>(existing)];
        m_items.erase(m_items.begin() + existing);
        if (existing < index)
            --index;
        m_items.insert(m_items.begin() + std::clamp(index, 0, count()), item);
        layoutChildren();
        return;
    }
    index = std::clamp(index, 0, count());
    widget->setParent(this);
    widget->show();
    m_items.insert(m_items.begin() + index, Item{widget, 0, 0, -1});
    if (count() > 1) {
        auto *handle = new QtPrivate::SplitterHandle(this, 0);
        handle->show();
        m_handles.push_back(handle);
    }
    // When the widget is deleted, it leaves the splitter.
    // (Not while the splitter itself is destroyed, and its widgets go with it.)
    QObject::connect(widget, &QObject::destroyed, this, [this, alive = m_alive](QObject *w) {
        if (*alive)
            removeItem(w);
    });
    // A new widget has no size of its own: the sizes are worked out again.
    m_sized = false;
    layoutChildren();
}

void QSplitter::removeItem(QObject *widget) {
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].widget != widget)
            continue;
        m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(i));
        if (!m_handles.empty()) {
            const std::size_t handle = std::min(i, m_handles.size() - 1);
            delete m_handles[handle];
            m_handles.erase(m_handles.begin() + static_cast<std::ptrdiff_t>(handle));
        }
        m_sized = false;
        layoutChildren();
        return;
    }
}

/*!
    Returns the index of \a widget, or -1 if it is not in the splitter.

    \sa widget()
*/
int QSplitter::indexOf(const QWidget *widget) const {
    for (int i = 0; i < count(); ++i)
        if (m_items[static_cast<std::size_t>(i)].widget == widget)
            return i;
    return -1;
}

/*!
    Returns the widget at \a index, or \c nullptr if there is none.

    \sa indexOf()
*/
QWidget *QSplitter::widget(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].widget : nullptr;
}

/*!
    \fn int QSplitter::count() const

    Returns the number of widgets.
*/

/*!
    \fn Qt::Orientation QSplitter::orientation() const

    Returns whether the widgets are side by side (\c Qt::Horizontal) or one above the other (\c Qt::Vertical).

    \sa setOrientation()
*/

/*!
    Sets whether the widgets are side by side or one above the other to \a orientation.

    \sa orientation()
*/
void QSplitter::setOrientation(Qt::Orientation orientation) {
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    m_sized = false;
    updateGeometry();
    layoutChildren();
}

/*!
    \fn bool QSplitter::childrenCollapsible() const

    Returns whether the user can shrink a widget to nothing. The default is \c true.

    \sa setChildrenCollapsible(), setCollapsible()
*/

/*!
    Sets whether the user can shrink a widget to nothing to \a collapse. A widget that was set with setCollapsible()
    keeps what it was set to.

    \sa childrenCollapsible()
*/
void QSplitter::setChildrenCollapsible(bool collapse) { m_collapsible = collapse; }

/*!
    Sets whether the user can shrink the widget at \a index to nothing to \a collapse.

    \sa isCollapsible()
*/
void QSplitter::setCollapsible(int index, bool collapse) {
    if (index >= 0 && index < count())
        m_items[static_cast<std::size_t>(index)].collapsible = collapse ? 1 : 0;
}

/*!
    Returns whether the user can shrink the widget at \a index to nothing.

    \sa setCollapsible()
*/
bool QSplitter::isCollapsible(int index) const {
    return index >= 0 && index < count() && canCollapse(m_items[static_cast<std::size_t>(index)]);
}

bool QSplitter::canCollapse(const Item &item) const { return item.collapsible < 0 ? m_collapsible : item.collapsible != 0; }

/*!
    Returns the width of the handles: the one that setHandleWidth() set, or else the width that the style has.

    \sa setHandleWidth()
*/
int QSplitter::handleWidth() const {
    return m_handleWidth >= 0 ? m_handleWidth : style()->pixelMetric(QStyle::PM_SplitterWidth, nullptr, this);
}

/*!
    Sets the width of the handles to \a width pixels.

    \sa handleWidth()
*/
void QSplitter::setHandleWidth(int width) {
    m_handleWidth = std::max(0, width);
    updateGeometry();
    layoutChildren();
}

/*!
    Gives the widget at \a index a stretch factor of \a stretch. When the splitter is resized, the widgets that have a
    stretch factor share the change in proportion to it, and the others keep their size. Without any, all widgets
    keep their share.
*/
void QSplitter::setStretchFactor(int index, int stretch) {
    if (index >= 0 && index < count())
        m_items[static_cast<std::size_t>(index)].stretch = std::max(0, stretch);
}

// The room for the widgets: the length of the splitter, less the handles.
int QSplitter::available() const {
    const int length = m_orientation == Qt::Horizontal ? width() : height();
    return std::max(0, length - std::max(0, count() - 1) * handleWidth());
}

int QSplitter::minimumOf(const Item &item) const {
    const QSize mn = QWidgetItem(item.widget).minimumSize();
    return m_orientation == Qt::Horizontal ? mn.width() : mn.height();
}

/*!
    Returns the sizes of the widgets along the orientation of the splitter, in pixels.

    \sa setSizes()
*/
QList<int> QSplitter::sizes() const {
    QList<int> list;
    for (const Item &item : m_items)
        list.append(item.size);
    return list;
}

/*!
    Sets the sizes of the widgets to \a list. The sizes are scaled to the room there is, so that only their proportions
    count. A widget that has no size in the list gets 0.

    \sa sizes()
*/
void QSplitter::setSizes(const QList<int> &list) {
    for (int i = 0; i < count(); ++i)
        m_items[static_cast<std::size_t>(i)].size = i < list.size() ? std::max(0, list.at(i)) : 0;
    m_sized = true;
    layoutChildren();
}

// Brings the sizes to the room that there is: by the stretch factors if there are any, and else in proportion.
void QSplitter::fit() {
    const int room = available();
    if (m_items.empty())
        return;
    if (!m_sized) {
        // Without sizes yet, the size hints say what each widget wants; widgets with none share equally.
        int total = 0;
        for (Item &item : m_items) {
            const QSize hint = item.widget->sizeHint();
            item.size = std::max(0, m_orientation == Qt::Horizontal ? hint.width() : hint.height());
            total += item.size;
        }
        if (total <= 0)
            for (Item &item : m_items)
                item.size = 1;
        m_sized = true;
    }
    int sum = 0, stretchSum = 0;
    for (const Item &item : m_items) {
        sum += item.size;
        stretchSum += item.stretch;
    }
    if (sum == room)
        return;
    std::vector<qint64> weight;
    if (stretchSum > 0 && sum > 0) {
        // The widgets with a stretch factor take the change.
        int fixedTotal = 0;
        for (const Item &item : m_items)
            if (item.stretch == 0)
                fixedTotal += item.size;
        const int flexible = std::max(0, room - fixedTotal);
        int distributed = 0;
        int lastFlexible = -1;
        for (int i = 0; i < count(); ++i)
            if (m_items[static_cast<std::size_t>(i)].stretch > 0)
                lastFlexible = i;
        for (int i = 0; i < count(); ++i) {
            Item &item = m_items[static_cast<std::size_t>(i)];
            if (item.stretch == 0)
                continue;
            item.size = i == lastFlexible ? flexible - distributed : flexible * item.stretch / stretchSum;
            distributed += item.size;
        }
    } else {
        int assigned = 0;
        for (int i = 0; i < count(); ++i) {
            Item &item = m_items[static_cast<std::size_t>(i)];
            const int size = i + 1 == count() ? room - assigned
                                              : sum > 0 ? static_cast<int>(std::lround(static_cast<double>(item.size) * room / sum))
                                                        : room / count();
            item.size = std::max(0, size);
            assigned += item.size;
        }
    }
    // The sizes of the widgets stay above their minimum, with what they need taken from the largest.
    for (int pass = 0; pass < 8; ++pass) {
        int deficit = 0;
        for (const Item &item : m_items)
            deficit += std::max(0, minimumOf(item) - item.size);
        if (deficit == 0)
            break;
        for (Item &item : m_items)
            item.size = std::max(item.size, minimumOf(item));
        while (deficit > 0) {
            auto largest = std::max_element(m_items.begin(), m_items.end(), [this](const Item &a, const Item &b) {
                return a.size - minimumOf(a) < b.size - minimumOf(b);
            });
            const int give = std::min(deficit, largest->size - minimumOf(*largest));
            if (give <= 0)
                break;
            largest->size -= give;
            deficit -= give;
        }
    }
}

void QSplitter::layoutChildren() {
    if (m_laying || m_items.empty())
        return;
    // Before it is shown a splitter keeps what it is told and lays out at its size when it is shown: its size before is
    // not the one that counts.
    if (!m_shown)
        return;
    m_laying = true;
    fit();
    const bool horizontal = m_orientation == Qt::Horizontal;
    const int hw = handleWidth();
    int pos = 0;
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items[i];
        item.widget->setGeometry(horizontal ? QRect(pos, 0, item.size, height()) : QRect(0, pos, width(), item.size));
        pos += item.size;
        if (i + 1 < m_items.size()) {
            QtPrivate::SplitterHandle *handle = m_handles[i];
            handle->setIndex(static_cast<int>(i));
            handle->setGeometry(horizontal ? QRect(pos, 0, hw, height()) : QRect(0, pos, width(), hw));
            handle->update();
            pos += hw;
        }
    }
    m_laying = false;
}

// Where the widget at `index` starts along the splitter.
int QSplitter::startOf(int index) const {
    int pos = 0;
    for (int i = 0; i < index; ++i)
        pos += m_items[static_cast<std::size_t>(i)].size + handleWidth();
    return pos;
}

// Moves the handle after the widget at `index` so that it starts at `pos`; the two widgets next to it share their room.
void QSplitter::moveHandle(int index, int pos) {
    if (index < 0 || index + 1 >= count())
        return;
    Item &left = m_items[static_cast<std::size_t>(index)];
    Item &right = m_items[static_cast<std::size_t>(index) + 1];
    const int total = left.size + right.size;
    const int minLeft = minimumOf(left), minRight = minimumOf(right);
    int wanted = pos - startOf(index);
    if (wanted < minLeft)
        wanted = canCollapse(left) && wanted < minLeft / 2 ? 0 : std::min(minLeft, total);
    if (total - wanted < minRight)
        wanted = canCollapse(right) && total - wanted < minRight / 2 ? total : std::max(0, total - minRight);
    wanted = std::clamp(wanted, 0, total);
    if (wanted == left.size)
        return;
    left.size = wanted;
    right.size = total - wanted;
    layoutChildren();
    emit splitterMoved(startOf(index) + left.size, index + 1);
}

/*!
    \reimp
*/
QSize QSplitter::sizeHint() const {
    int along = std::max(0, count() - 1) * handleWidth(), across = 0;
    for (const Item &item : m_items) {
        const QSize hint = item.widget->sizeHint().expandedTo(QSize(0, 0));
        along += m_orientation == Qt::Horizontal ? hint.width() : hint.height();
        across = std::max(across, m_orientation == Qt::Horizontal ? hint.height() : hint.width());
    }
    return m_orientation == Qt::Horizontal ? QSize(along, across) : QSize(across, along);
}

/*!
    \reimp
*/
QSize QSplitter::minimumSizeHint() const {
    int along = std::max(0, count() - 1) * handleWidth(), across = 0;
    for (const Item &item : m_items) {
        const QSize mn = QWidgetItem(item.widget).minimumSize();
        along += m_orientation == Qt::Horizontal ? mn.width() : mn.height();
        across = std::max(across, m_orientation == Qt::Horizontal ? mn.height() : mn.width());
    }
    return m_orientation == Qt::Horizontal ? QSize(along, across) : QSize(across, along);
}

/*!
    \reimp
*/
void QSplitter::resizeEvent(QResizeEvent *event) {
    (void)event;
    layoutChildren();
}

/*!
    \reimp
*/
void QSplitter::showEvent(QShowEvent *event) {
    (void)event;
    m_shown = true;
    layoutChildren();
}

/*!
    \reimp
*/
void QSplitter::changeEvent(QEvent *event) {
    layoutChildren();
    QFrame::changeEvent(event);
}

#include "moc_qsplitter.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
