// MIT License - see LICENSE
#include <QtWidgets/qsplitter.h>
#include <QtWidgets/qrubberband.h>

#include <QtCore/qiodevice.h>

#include <QtCore/qdatastream.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qlayoutitem.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

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
    The widgets always follow the handle: setOpaqueResize() is kept but there is no rubber band. handle(0), the hidden
    handle before the first widget in Qt, is \c nullptr. saveState() writes a format of Nt's own.
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
QSplitter::QSplitter(Qt::Orientation orientation, QWidget *parent) : QFrame(parent), m_orientation(orientation) {
    init();
}

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
        QSplitterHandle *handle = createHandle();
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
    for (QSplitterHandle *handle : m_handles)
        handle->setOrientation(orientation);
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

bool QSplitter::canCollapse(const Item &item) const {
    return item.collapsible < 0 ? m_collapsible : item.collapsible != 0;
}

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
            QSplitterHandle *handle = m_handles[i];
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

/*!
    \since 6.0

    Makes the handle that goes between two widgets. Reimplement it to give the splitter handles of your own, a
    subclass of QSplitterHandle.
*/
QSplitterHandle *QSplitter::createHandle() { return new QSplitterHandle(m_orientation, this); }

/*!
    \since 6.0

    Returns the handle before the widget at \a index, which the user drags to resize it and the widget before it,
    or \c nullptr for the first widget, which has none, and for an index out of range.
*/
QSplitterHandle *QSplitter::handle(int index) const {
    if (index < 1 || index >= count())
        return nullptr;
    return m_handles[static_cast<std::size_t>(index) - 1];
}

/*!
    \since 6.0

    Puts \a widget at \a index in place of the widget there, which is hidden and given back without a parent; the
    caller owns it. Returns \c nullptr, and does nothing, if \a index is out of range or \a widget is in the
    splitter already.
*/
QWidget *QSplitter::replaceWidget(int index, QWidget *widget) {
    if (!widget || index < 0 || index >= count() || indexOf(widget) >= 0)
        return nullptr;
    Item &item = m_items[static_cast<std::size_t>(index)];
    QWidget *old = item.widget;
    // The old widget leaves without taking its handle with it.
    QObject::disconnect(old, &QObject::destroyed, this, nullptr);
    old->hide();
    old->setParent(nullptr);
    item.widget = widget;
    widget->setParent(this);
    widget->show();
    QObject::connect(widget, &QObject::destroyed, this, [this, alive = m_alive](QObject *w) {
        if (*alive)
            removeItem(w);
    });
    layoutChildren();
    return old;
}

/*!
    \fn bool QSplitter::opaqueResize() const
    \since 6.0

    Returns whether the widgets follow the handle while it is dragged; \c true.
*/

/*!
    \fn void QSplitter::setOpaqueResize(bool opaque)
    \since 6.0

    Sets whether the widgets follow the handle while it is dragged to \a opaque. Nt keeps the value; the widgets
    always follow.
*/

/*!
    \since 6.0

    Returns the sizes of the widgets and the settings of the splitter, for restoreState() to bring back, such as from
    QSettings in a later run.

    \code
    settings.setValue("split", splitter->saveState());
    ...
    splitter->restoreState(settings.value("split").toByteArray());
    \endcode
*/
QByteArray QSplitter::saveState() const {
    QByteArray state;
    QDataStream out(&state, QIODevice::WriteOnly);
    out << quint32(0x4e745350) << quint32(1) << qint32(count());
    for (const int size : sizes())
        out << qint32(size);
    out << qint32(handleWidth()) << m_collapsible << qint32(m_orientation);
    return state;
}

/*!
    \since 6.0

    Brings back the sizes and the settings that saveState() returned, \a state. Returns \c false, and changes
    nothing, if \a state is not one of a splitter.
*/
bool QSplitter::restoreState(const QByteArray &state) {
    QDataStream in(state);
    quint32 magic = 0, version = 0;
    qint32 n = 0;
    in >> magic >> version >> n;
    if (magic != 0x4e745350 || version != 1 || n < 0 || n > 10000)
        return false;
    QList<int> list;
    for (qint32 i = 0; i < n; ++i) {
        qint32 size = 0;
        in >> size;
        list.append(size);
    }
    qint32 width = -1, orientation = 0;
    bool collapsible = true;
    in >> width >> collapsible >> orientation;
    if (in.status() != QDataStream::Ok)
        return false;
    setHandleWidth(width);
    setChildrenCollapsible(collapsible);
    setOrientation(static_cast<Qt::Orientation>(orientation));
    setSizes(list);
    return true;
}

/*!
    \since 6.0

    Lays the widgets out again, as after a change of their size hints.
*/
void QSplitter::refresh() {
    m_sized = false;
    layoutChildren();
}

/*!
    \since 6.0

    Returns the position nearest to \a pos that the handle before the widget at \a index can be moved to.
*/
int QSplitter::closestLegalPosition(int pos, int index) {
    if (index < 1 || index >= count())
        return pos;
    const Item &left = m_items[static_cast<std::size_t>(index) - 1];
    const Item &right = m_items[static_cast<std::size_t>(index)];
    const int start = startOf(index - 1);
    const int total = left.size + right.size;
    const int low = canCollapse(left) ? start : start + std::min(minimumOf(left), total);
    const int high = canCollapse(right) ? start + total : start + std::max(0, total - minimumOf(right));
    return std::clamp(pos, low, std::max(low, high));
}

/*!
    \since 6.0

    Moves the handle before the widget at \a index to \a pos, as the user does, and emits splitterMoved().
*/
void QSplitter::moveSplitter(int pos, int index) { moveHandle(index - 1, pos); }

/*!
    \class QSplitterHandle
    \inmodule NtWidgets
    \since 6.0
    \brief The QSplitterHandle class is the bar between two widgets of a QSplitter.

    Subclass it, and QSplitter::createHandle(), to draw handles of your own:

    \code
    class Grip : public QSplitterHandle {
    public:
        using QSplitterHandle::QSplitterHandle;
    protected:
        void paintEvent(QPaintEvent *) override { QPainter(this).fillRect(rect(), Qt::darkGray); }
    };
    \endcode

    The handles between the panes of two QSplitter widgets, one inside the other.

    \image widgets/qsplitter.png QSplitter in the light color scheme

    \differences
    The cursor does not change over a handle.
    \enddifferences

    \sa QSplitter
*/

/*!
    Constructs a handle of \a orientation for the splitter \a parent.
*/
QSplitterHandle::QSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QWidget(parent), m_splitter(parent), m_orientation(orientation) {}

/*!
    Destroys the handle.
*/
QSplitterHandle::~QSplitterHandle() = default;

/*!
    Sets the orientation of the handle to \a orientation; the splitter sets it to its own.
*/
void QSplitterHandle::setOrientation(Qt::Orientation orientation) {
    m_orientation = orientation;
    update();
}

/*!
    \fn Qt::Orientation QSplitterHandle::orientation() const

    Returns the orientation of the handle.
*/

/*!
    Returns QSplitter::opaqueResize() of the splitter.
*/
bool QSplitterHandle::opaqueResize() const { return m_splitter && m_splitter->opaqueResize(); }

/*!
    \fn QSplitter *QSplitterHandle::splitter() const

    Returns the splitter of the handle.
*/

/*!
    \reimp
*/
QSize QSplitterHandle::sizeHint() const {
    const int width = m_splitter ? m_splitter->handleWidth() : 4;
    return m_orientation == Qt::Horizontal ? QSize(width, 0) : QSize(0, width);
}

int QSplitterHandle::index() const {
    if (!m_splitter)
        return -1;
    for (std::size_t i = 0; i < m_splitter->m_handles.size(); ++i)
        if (m_splitter->m_handles[i] == this)
            return static_cast<int>(i);
    return -1;
}

/*!
    \reimp
*/
void QSplitterHandle::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    if (m_orientation == Qt::Horizontal)
        option.state |= QStyle::State_Horizontal;
    style()->drawControl(QStyle::CE_Splitter, &option, &painter, this);
}

/*!
    \reimp
*/
void QSplitterHandle::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_grab = m_orientation == Qt::Horizontal ? event->pos().x() : event->pos().y();
    m_dragging = true;
    event->accept();
}

/*!
    \reimp
*/
void QSplitterHandle::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging) {
        event->ignore();
        return;
    }
    const QPoint p = mapToParent(event->pos());
    moveSplitter((m_orientation == Qt::Horizontal ? p.x() : p.y()) - m_grab);
    event->accept();
}

/*!
    \reimp
*/
void QSplitterHandle::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    event->accept();
}

/*!
    Moves the handle to \a pos in the splitter, as a drag does.
*/
void QSplitterHandle::moveSplitter(int pos) {
    if (m_splitter)
        m_splitter->moveHandle(index(), pos);
}

/*!
    Returns the position nearest to \a pos that the handle can be moved to.
*/
int QSplitterHandle::closestLegalPosition(int pos) {
    return m_splitter ? m_splitter->closestLegalPosition(pos, index() + 1) : pos;
}

/*!
    \since 6.0

    Puts in \a min and \a max the range that the handle before the widget at \a index can be moved in, either of
    which may be \c nullptr.

    \sa closestLegalPosition()
*/
void QSplitter::getRange(int index, int *min, int *max) const {
    int low = 0;
    int high = 0;
    if (index >= 1 && index < count()) {
        const Item &left = m_items[static_cast<std::size_t>(index) - 1];
        const Item &right = m_items[static_cast<std::size_t>(index)];
        const int start = startOf(index - 1);
        const int total = left.size + right.size;
        low = canCollapse(left) ? start : start + std::min(minimumOf(left), total);
        high = std::max(low, canCollapse(right) ? start + total : start + std::max(0, total - minimumOf(right)));
    }
    if (min)
        *min = low;
    if (max)
        *max = high;
}

/*!
    \since 6.0

    Shows a rubber band across the splitter at \a position, where a handle would go while the user drags it without
    opaque resizing; a negative \a position hides it.
*/
void QSplitter::setRubberBand(int position) {
    if (position < 0) {
        if (m_rubberBand)
            m_rubberBand->hide();
        return;
    }
    if (!m_rubberBand)
        m_rubberBand = new QRubberBand(QRubberBand::Line, this);
    const int thickness = std::max(1, handleWidth());
    if (m_orientation == Qt::Horizontal)
        m_rubberBand->setGeometry(position, 0, thickness, height());
    else
        m_rubberBand->setGeometry(0, position, width(), thickness);
    m_rubberBand->raise();
    m_rubberBand->show();
}

#include "moc_qsplitter.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
