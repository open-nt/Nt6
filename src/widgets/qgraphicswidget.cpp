// MIT License - see LICENSE
#include <QtWidgets/qgraphicswidget.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qgraphicslinearlayout.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicssceneevent.h>
#include <QtWidgets/qlayoutitem.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <functional>
#include <vector>

namespace {

bool instantInvalidate = false;

qreal styleMetric(QStyle::PixelMetric metric, qreal fallback) {
    QStyle *style = qApp ? QApplication::style() : nullptr;
    const int value = style ? style->pixelMetric(metric) : -1;
    return value >= 0 ? value : fallback;
}

// A component of a size along an orientation.
qreal along(const QSizeF &size, Qt::Orientation orientation) {
    return orientation == Qt::Horizontal ? size.width() : size.height();
}

} // namespace

// ---- QGraphicsLayoutItem ----

/*!
    \class QGraphicsLayoutItem
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsLayoutItem class is something that a QGraphicsLayout lays out.

    A layout item has size hints, a size policy and a geometry. QGraphicsWidget and QGraphicsLayout are layout
    items; a subclass reimplements sizeHint() and setGeometry().

    The sizes that setMinimumSize(), setPreferredSize() and setMaximumSize() set take the place of those of
    sizeHint(); effectiveSizeHint() combines the two.

    \differences
    There is no height-for-width, and \c MinimumDescent is not used.
    \enddifferences

    \sa QGraphicsLayout, QGraphicsWidget
*/

/*!
    Constructs a layout item with the parent \a parent; \a isLayout is \c true for a QGraphicsLayout.
*/
QGraphicsLayoutItem::QGraphicsLayoutItem(QGraphicsLayoutItem *parent, bool isLayout)
    : m_parent(parent), m_isLayout(isLayout) {}

/*!
    Destroys the layout item.
*/
QGraphicsLayoutItem::~QGraphicsLayoutItem() = default;

/*!
    Sets the size policy of the item to \a policy.
*/
void QGraphicsLayoutItem::setSizePolicy(const QSizePolicy &policy) {
    m_sizePolicy = policy;
    updateGeometry();
}

/*!
    \overload

    Sets the size policy of the item to \a hPolicy across and \a vPolicy down.
*/
void QGraphicsLayoutItem::setSizePolicy(QSizePolicy::Policy hPolicy, QSizePolicy::Policy vPolicy) {
    setSizePolicy(QSizePolicy(hPolicy, vPolicy));
}

/*!
    \fn QSizePolicy QGraphicsLayoutItem::sizePolicy() const

    Returns the size policy of the item; QSizePolicy::Preferred both ways by default.
*/

void QGraphicsLayoutItem::setUserSize(Qt::SizeHint which, Qt::Orientations orientations, const QSizeF &size) {
    QSizeF &user = m_userSizes[which];
    if (orientations & Qt::Horizontal)
        user.setWidth(size.width());
    if (orientations & Qt::Vertical)
        user.setHeight(size.height());
    updateGeometry();
}

/*!
    Sets the smallest size of the item to \a size, in place of the minimum size hint.
*/
void QGraphicsLayoutItem::setMinimumSize(const QSizeF &size) {
    setUserSize(Qt::MinimumSize, Qt::Horizontal | Qt::Vertical, size);
}

/*!
    Sets the smallest width of the item to \a width.
*/
void QGraphicsLayoutItem::setMinimumWidth(qreal width) {
    setUserSize(Qt::MinimumSize, Qt::Horizontal, QSizeF(width, 0));
}

/*!
    Sets the smallest height of the item to \a height.
*/
void QGraphicsLayoutItem::setMinimumHeight(qreal height) {
    setUserSize(Qt::MinimumSize, Qt::Vertical, QSizeF(0, height));
}

/*!
    Sets the best size of the item to \a size, in place of the preferred size hint.
*/
void QGraphicsLayoutItem::setPreferredSize(const QSizeF &size) {
    setUserSize(Qt::PreferredSize, Qt::Horizontal | Qt::Vertical, size);
}

/*!
    Sets the best width of the item to \a width.
*/
void QGraphicsLayoutItem::setPreferredWidth(qreal width) {
    setUserSize(Qt::PreferredSize, Qt::Horizontal, QSizeF(width, 0));
}

/*!
    Sets the best height of the item to \a height.
*/
void QGraphicsLayoutItem::setPreferredHeight(qreal height) {
    setUserSize(Qt::PreferredSize, Qt::Vertical, QSizeF(0, height));
}

/*!
    Sets the largest size of the item to \a size, in place of the maximum size hint.
*/
void QGraphicsLayoutItem::setMaximumSize(const QSizeF &size) {
    setUserSize(Qt::MaximumSize, Qt::Horizontal | Qt::Vertical, size);
}

/*!
    Sets the largest width of the item to \a width.
*/
void QGraphicsLayoutItem::setMaximumWidth(qreal width) {
    setUserSize(Qt::MaximumSize, Qt::Horizontal, QSizeF(width, 0));
}

/*!
    Sets the largest height of the item to \a height.
*/
void QGraphicsLayoutItem::setMaximumHeight(qreal height) {
    setUserSize(Qt::MaximumSize, Qt::Vertical, QSizeF(0, height));
}

/*!
    \fn void QGraphicsLayoutItem::setMinimumSize(qreal w, qreal h)
    \overload

    Sets the smallest size of the item to \a w by \a h.
*/

/*!
    \fn QSizeF QGraphicsLayoutItem::minimumSize() const

    Returns the smallest size of the item, the effective minimum size hint.
*/

/*!
    \fn qreal QGraphicsLayoutItem::minimumWidth() const

    Returns the smallest width of the item.
*/

/*!
    \fn qreal QGraphicsLayoutItem::minimumHeight() const

    Returns the smallest height of the item.
*/

/*!
    \fn void QGraphicsLayoutItem::setPreferredSize(qreal w, qreal h)
    \overload

    Sets the best size of the item to \a w by \a h.
*/

/*!
    \fn QSizeF QGraphicsLayoutItem::preferredSize() const

    Returns the best size of the item, the effective preferred size hint.
*/

/*!
    \fn qreal QGraphicsLayoutItem::preferredWidth() const

    Returns the best width of the item.
*/

/*!
    \fn qreal QGraphicsLayoutItem::preferredHeight() const

    Returns the best height of the item.
*/

/*!
    \fn void QGraphicsLayoutItem::setMaximumSize(qreal w, qreal h)
    \overload

    Sets the largest size of the item to \a w by \a h.
*/

/*!
    \fn QSizeF QGraphicsLayoutItem::maximumSize() const

    Returns the largest size of the item, the effective maximum size hint.
*/

/*!
    \fn qreal QGraphicsLayoutItem::maximumWidth() const

    Returns the largest width of the item.
*/

/*!
    \fn qreal QGraphicsLayoutItem::maximumHeight() const

    Returns the largest height of the item.
*/

/*!
    Sets the geometry of the item to \a rect, in the coordinates of its parent item, kept between its smallest and
    largest size. A subclass reimplements it to place what it holds, and calls this one.
*/
void QGraphicsLayoutItem::setGeometry(const QRectF &rect) {
    const QSizeF minimum = effectiveSizeHint(Qt::MinimumSize);
    const QSizeF maximum = effectiveSizeHint(Qt::MaximumSize);
    m_geometry = QRectF(rect.topLeft(), QSizeF(std::clamp(rect.width(), minimum.width(), maximum.width()),
                                               std::clamp(rect.height(), minimum.height(), maximum.height())));
}

/*!
    \fn QRectF QGraphicsLayoutItem::geometry() const

    Returns the geometry of the item.
*/

/*!
    Sets \a left, \a top, \a right and \a bottom to the margins of the item: 0 unless a subclass has margins.
*/
void QGraphicsLayoutItem::getContentsMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const {
    if (left)
        *left = 0;
    if (top)
        *top = 0;
    if (right)
        *right = 0;
    if (bottom)
        *bottom = 0;
}

/*!
    Returns the rectangle inside the margins of the item, in its own coordinates.
*/
QRectF QGraphicsLayoutItem::contentsRect() const {
    qreal left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    return QRectF(QPointF(), m_geometry.size()).adjusted(left, top, -right, -bottom);
}

/*!
    Returns the size hint \a which of the item for \a constraint: the size that was set for it, and else what
    sizeHint() says, with the smallest no larger than the best and the largest no smaller.
*/
QSizeF QGraphicsLayoutItem::effectiveSizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    if (which == Qt::MinimumDescent || which == Qt::NSizeHints)
        return sizeHint(which, constraint);
    QSizeF hints[3];
    for (int i = Qt::MinimumSize; i <= Qt::MaximumSize; ++i) {
        const auto kind = static_cast<Qt::SizeHint>(i);
        QSizeF hint = sizeHint(kind, constraint);
        const qreal unset = kind == Qt::MaximumSize ? QLAYOUTSIZE_MAX : 0;
        if (hint.width() < 0)
            hint.setWidth(unset);
        if (hint.height() < 0)
            hint.setHeight(unset);
        if (m_userSizes[i].width() >= 0)
            hint.setWidth(m_userSizes[i].width());
        if (m_userSizes[i].height() >= 0)
            hint.setHeight(m_userSizes[i].height());
        hints[i] = hint;
    }
    QSizeF &minimum = hints[Qt::MinimumSize];
    QSizeF &maximum = hints[Qt::MaximumSize];
    QSizeF &preferred = hints[Qt::PreferredSize];
    maximum = maximum.expandedTo(minimum);
    preferred = preferred.expandedTo(minimum).boundedTo(maximum);
    return hints[which];
}

/*!
    Tells the item that its size hints changed. The default implementation does nothing; QGraphicsWidget and
    QGraphicsLayout tell their parent layout.
*/
void QGraphicsLayoutItem::updateGeometry() {}

/*!
    Returns \c true if the item takes no room: if its graphics item was hidden itself, not only with its parent.
*/
bool QGraphicsLayoutItem::isEmpty() const {
    const QGraphicsItem *item = graphicsItem();
    return item && !isLayout() && !item->isVisibleTo(item->parentItem());
}

/*!
    \fn QGraphicsLayoutItem *QGraphicsLayoutItem::parentLayoutItem() const

    Returns the layout, or the widget for a top layout, that the item is in; \c nullptr if it is in none.
*/

/*!
    \fn void QGraphicsLayoutItem::setParentLayoutItem(QGraphicsLayoutItem *parent)

    Sets the layout item that this item is in to \a parent.
*/

/*!
    \fn bool QGraphicsLayoutItem::isLayout() const

    Returns \c true if the item is a QGraphicsLayout.
*/

/*!
    \fn QGraphicsItem *QGraphicsLayoutItem::graphicsItem() const

    Returns the graphics item of the layout item, or \c nullptr for a layout.
*/

/*!
    \fn bool QGraphicsLayoutItem::ownedByLayout() const

    Returns \c true if the layout that holds the item deletes it.
*/

/*!
    \fn QSizeF QGraphicsLayoutItem::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const

    Returns the size hint \a which of the item for \a constraint; a negative width or height means none.
*/

/*!
    \fn void QGraphicsLayoutItem::setGraphicsItem(QGraphicsItem *item)

    Sets the graphics item of the layout item to \a item.
*/

/*!
    \fn void QGraphicsLayoutItem::setOwnedByLayout(bool ownedByLayout)

    Sets whether the layout that holds the item deletes it to \a ownedByLayout.
*/

// ---- QGraphicsLayout ----

/*!
    \class QGraphicsLayout
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsLayout class is the base of the layouts of a QGraphicsWidget.

    A layout places the items it holds inside the widget that QGraphicsWidget::setLayout() gave it, and is laid
    out again when the widget is resized or an item changes. The items become children of the widget.

    \differences
    There is no QGraphicsGridLayout or QGraphicsAnchorLayout; QGraphicsLinearLayout is the only layout.
    \enddifferences

    \sa QGraphicsLinearLayout
*/

/*!
    Constructs a layout in \a parent, a layout or a widget.
*/
QGraphicsLayout::QGraphicsLayout(QGraphicsLayoutItem *parent) : QGraphicsLayoutItem(parent, true) {}

/*!
    Destroys the layout. The items it holds stay, but nested layouts are deleted.
*/
QGraphicsLayout::~QGraphicsLayout() {
    if (QGraphicsWidget *widget = parentWidget(); widget && widget->m_layout == this)
        widget->m_layout = nullptr;
}

/*!
    Sets the margins of the layout to \a left, \a top, \a right and \a bottom. By default they come from the style.
*/
void QGraphicsLayout::setContentsMargins(qreal left, qreal top, qreal right, qreal bottom) {
    m_margins[0] = left;
    m_margins[1] = top;
    m_margins[2] = right;
    m_margins[3] = bottom;
    invalidate();
}

/*!
    \reimp
*/
void QGraphicsLayout::getContentsMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const {
    // A nested layout has no margins of the style.
    const bool nested = parentLayoutItem() && parentLayoutItem()->isLayout();
    const auto margin = [&](int i, QStyle::PixelMetric metric) {
        return m_margins[i] >= 0 ? m_margins[i] : nested ? 0 : styleMetric(metric, 9);
    };
    if (left)
        *left = margin(0, QStyle::PM_LayoutLeftMargin);
    if (top)
        *top = margin(1, QStyle::PM_LayoutTopMargin);
    if (right)
        *right = margin(2, QStyle::PM_LayoutRightMargin);
    if (bottom)
        *bottom = margin(3, QStyle::PM_LayoutBottomMargin);
}

QGraphicsWidget *QGraphicsLayout::parentWidget() const {
    const QGraphicsLayoutItem *item = this;
    while (item && item->isLayout())
        item = item->parentLayoutItem();
    // The item that is no layout is the widget; dynamic_cast finds it across the two bases of QGraphicsWidget.
    return dynamic_cast<QGraphicsWidget *>(const_cast<QGraphicsLayoutItem *>(item));
}

/*!
    Lays the items out at once in the contents of the widget of the layout.
*/
void QGraphicsLayout::activate() {
    // The item above the layouts is the widget; the top layout is the one whose parent it is.
    const QGraphicsLayoutItem *top = parentLayoutItem();
    while (top && top->isLayout())
        top = top->parentLayoutItem();
    if (!top)
        return;
    m_activated = true;
    setGeometry(parentLayoutItem() == top ? top->contentsRect() : geometry());
}

/*!
    \fn bool QGraphicsLayout::isActivated() const

    Returns \c true if the layout was laid out since it last changed.
*/

/*!
    Marks the layout as changed, so that it is laid out again soon.
*/
void QGraphicsLayout::invalidate() {
    m_activated = false;
    if (QGraphicsLayoutItem *parent = parentLayoutItem(); parent && parent->isLayout())
        static_cast<QGraphicsLayout *>(parent)->invalidate();
    else if (QGraphicsWidget *widget = parentWidget())
        widget->requestLayout();
}

/*!
    \reimp
*/
void QGraphicsLayout::updateGeometry() {
    invalidate();
    if (QGraphicsLayoutItem *parent = parentLayoutItem(); parent && !parent->isLayout())
        parent->updateGeometry();
}

/*!
    Is called by the widget of the layout with its event \a e. The default implementation does nothing.
*/
void QGraphicsLayout::widgetEvent(QEvent *e) { (void)e; }

/*!
    \fn int QGraphicsLayout::count() const

    Returns the number of items in the layout.
*/

/*!
    \fn QGraphicsLayoutItem *QGraphicsLayout::itemAt(int i) const

    Returns the item at \a i.
*/

/*!
    \fn void QGraphicsLayout::removeAt(int index)

    Removes the item at \a index from the layout without deleting it.
*/

/*!
    Sets whether a change of an item lays the layouts out at once, rather than soon, to \a enable. Nt lays them out
    soon in either case.
*/
void QGraphicsLayout::setInstantInvalidatePropagation(bool enable) { instantInvalidate = enable; }

/*!
    Returns what setInstantInvalidatePropagation() set.
*/
bool QGraphicsLayout::instantInvalidatePropagation() { return instantInvalidate; }

/*!
    Puts \a layoutItem in this layout: makes the layout its parent, and its graphics item a child of the widget of
    the layout.
*/
void QGraphicsLayout::addChildLayoutItem(QGraphicsLayoutItem *layoutItem) {
    if (!layoutItem)
        return;
    layoutItem->setParentLayoutItem(this);
    QGraphicsWidget *widget = parentWidget();
    if (!widget)
        return;
    std::vector<QGraphicsLayoutItem *> pending{layoutItem};
    // The items of a nested layout move to the widget too.
    while (!pending.empty()) {
        QGraphicsLayoutItem *item = pending.back();
        pending.pop_back();
        if (item->isLayout()) {
            auto *layout = static_cast<QGraphicsLayout *>(item);
            for (int i = 0; i < layout->count(); ++i)
                pending.push_back(layout->itemAt(i));
        } else if (QGraphicsItem *graphics = item->graphicsItem(); graphics && graphics->parentItem() != widget) {
            graphics->setParentItem(widget);
        }
    }
}

// ---- QGraphicsLinearLayout ----

/*!
    \class QGraphicsLinearLayout
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsLinearLayout class lays graphics widgets out in a row or a column.

    \code
    auto *container = new QGraphicsWidget;
    auto *layout = new QGraphicsLinearLayout(Qt::Horizontal, container);
    layout->setSpacing(12);
    layout->addItem(textProxy);
    layout->addItem(imageProxy);
    layout->setAlignment(imageProxy, Qt::AlignVCenter);
    container->adjustSize();
    \endcode

    The items get their preferred size, and the room that is left goes to those that can grow, as their stretch
    factors say; with too little room they shrink towards their smallest size.

    \differences
    There is no height-for-width.
    \enddifferences

    \sa QGraphicsWidget
*/

/*!
    Constructs a horizontal layout in \a parent.
*/
QGraphicsLinearLayout::QGraphicsLinearLayout(QGraphicsLayoutItem *parent)
    : QGraphicsLinearLayout(Qt::Horizontal, parent) {}

/*!
    Constructs a layout in \a parent that lays its items out along \a orientation.
*/
QGraphicsLinearLayout::QGraphicsLinearLayout(Qt::Orientation orientation, QGraphicsLayoutItem *parent)
    : QGraphicsLayout(parent), m_orientation(orientation) {
    if (auto *widget = parent && !parent->isLayout() ? dynamic_cast<QGraphicsWidget *>(parent) : nullptr)
        widget->setLayout(this);
}

/*!
    Destroys the layout and the nested layouts it holds; the widgets stay.
*/
QGraphicsLinearLayout::~QGraphicsLinearLayout() {
    for (const Entry &entry : std::as_const(m_entries)) {
        if (!entry.item)
            continue;
        entry.item->setParentLayoutItem(nullptr);
        if (entry.item->ownedByLayout() || entry.item->isLayout())
            delete entry.item;
    }
}

/*!
    Sets the direction that the items are laid out in to \a orientation.
*/
void QGraphicsLinearLayout::setOrientation(Qt::Orientation orientation) {
    if (orientation == m_orientation)
        return;
    m_orientation = orientation;
    invalidate();
}

/*!
    \fn Qt::Orientation QGraphicsLinearLayout::orientation() const

    Returns the direction that the items are laid out in.
*/

/*!
    \fn void QGraphicsLinearLayout::addItem(QGraphicsLayoutItem *item)

    Adds \a item at the end of the layout.
*/

/*!
    \fn void QGraphicsLinearLayout::addStretch(int stretch)

    Adds a stretch with the factor \a stretch at the end of the layout: room that grows with the layout.
*/

/*!
    Inserts \a item at \a index among the items, or at the end if \a index is out of range. The graphics item of
    \a item becomes a child of the widget of the layout.
*/
void QGraphicsLinearLayout::insertItem(int index, QGraphicsLayoutItem *item) {
    if (!item || item == this || entryOf(item) >= 0)
        return;
    if (QGraphicsLayoutItem *old = item->parentLayoutItem(); old && old->isLayout()) {
        auto *oldLayout = static_cast<QGraphicsLayout *>(old);
        for (int i = 0; i < oldLayout->count(); ++i) {
            if (oldLayout->itemAt(i) == item) {
                oldLayout->removeAt(i);
                break;
            }
        }
    }
    const int at = index < 0 || index >= count() ? static_cast<int>(m_entries.size()) : entryOfItem(index);
    m_entries.insert(at, Entry{item, 0, Qt::Alignment(), -1});
    addChildLayoutItem(item);
    invalidate();
}

/*!
    Inserts a stretch with the factor \a stretch before the item at \a index, or at the end if \a index is out of
    range.
*/
void QGraphicsLinearLayout::insertStretch(int index, int stretch) {
    const int at = index < 0 || index >= count() ? static_cast<int>(m_entries.size()) : entryOfItem(index);
    m_entries.insert(at, Entry{nullptr, stretch, Qt::Alignment(), -1});
    invalidate();
}

/*!
    Removes \a item from the layout without deleting it.
*/
void QGraphicsLinearLayout::removeItem(QGraphicsLayoutItem *item) {
    const int entry = entryOf(item);
    if (entry < 0)
        return;
    m_entries.removeAt(entry);
    item->setParentLayoutItem(nullptr);
    invalidate();
}

/*!
    \reimp
*/
void QGraphicsLinearLayout::removeAt(int index) {
    if (index >= 0 && index < count())
        removeItem(itemAt(index));
}

/*!
    Sets the room between the items to \a spacing.
*/
void QGraphicsLinearLayout::setSpacing(qreal spacing) {
    m_spacing = spacing;
    for (Entry &entry : m_entries)
        entry.spacing = -1;
    invalidate();
}

/*!
    Returns the room between the items; by default it comes from the style.
*/
qreal QGraphicsLinearLayout::spacing() const {
    if (m_spacing >= 0)
        return m_spacing;
    return styleMetric(
        m_orientation == Qt::Horizontal ? QStyle::PM_LayoutHorizontalSpacing : QStyle::PM_LayoutVerticalSpacing, 6);
}

/*!
    Sets the room after the item at \a index to \a spacing; a negative \a spacing gives it spacing() again.
*/
void QGraphicsLinearLayout::setItemSpacing(int index, qreal spacing) {
    const int entry = entryOfItem(index);
    if (entry < 0)
        return;
    m_entries[entry].spacing = spacing;
    invalidate();
}

/*!
    Returns the room after the item at \a index.
*/
qreal QGraphicsLinearLayout::itemSpacing(int index) const {
    const int entry = entryOfItem(index);
    return entry < 0 ? 0 : spacingAfter(entry);
}

/*!
    Sets the stretch factor of \a item to \a stretch.
*/
void QGraphicsLinearLayout::setStretchFactor(QGraphicsLayoutItem *item, int stretch) {
    const int entry = entryOf(item);
    if (entry < 0)
        return;
    m_entries[entry].stretch = stretch;
    invalidate();
}

/*!
    Returns the stretch factor of \a item, 0 by default.
*/
int QGraphicsLinearLayout::stretchFactor(QGraphicsLayoutItem *item) const {
    const int entry = entryOf(item);
    return entry < 0 ? 0 : m_entries.at(entry).stretch;
}

/*!
    Sets the alignment of \a item in the room it gets to \a alignment.
*/
void QGraphicsLinearLayout::setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment) {
    const int entry = entryOf(item);
    if (entry < 0)
        return;
    m_entries[entry].alignment = alignment;
    invalidate();
}

/*!
    Returns the alignment of \a item, 0 by default: the top left of its room.
*/
Qt::Alignment QGraphicsLinearLayout::alignment(QGraphicsLayoutItem *item) const {
    const int entry = entryOf(item);
    return entry < 0 ? Qt::Alignment() : m_entries.at(entry).alignment;
}

/*!
    \reimp
*/
int QGraphicsLinearLayout::count() const {
    return static_cast<int>(
        std::count_if(m_entries.cbegin(), m_entries.cend(), [](const Entry &entry) { return entry.item != nullptr; }));
}

/*!
    \reimp
*/
QGraphicsLayoutItem *QGraphicsLinearLayout::itemAt(int index) const {
    const int entry = entryOfItem(index);
    return entry < 0 ? nullptr : m_entries.at(entry).item;
}

/*!
    \reimp
*/
void QGraphicsLinearLayout::invalidate() { QGraphicsLayout::invalidate(); }

int QGraphicsLinearLayout::entryOf(const QGraphicsLayoutItem *item) const {
    for (qsizetype i = 0; i < m_entries.size(); ++i)
        if (item && m_entries.at(i).item == item)
            return static_cast<int>(i);
    return -1;
}

int QGraphicsLinearLayout::entryOfItem(int index) const {
    if (index < 0)
        return -1;
    for (qsizetype i = 0; i < m_entries.size(); ++i)
        if (m_entries.at(i).item && index-- == 0)
            return static_cast<int>(i);
    return -1;
}

qreal QGraphicsLinearLayout::spacingAfter(int entry) const {
    const qreal own = m_entries.at(entry).spacing;
    return own >= 0 ? own : spacing();
}

/*!
    \reimp
*/
QSizeF QGraphicsLinearLayout::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    (void)constraint;
    qreal left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const Qt::Orientation cross = m_orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal;
    qreal main = 0;
    qreal across = 0;
    bool first = true;
    bool anyStretch = false;
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        const Entry &entry = m_entries.at(i);
        if (!entry.item) {
            anyStretch = true;
            continue;
        }
        if (entry.item->isEmpty())
            continue;
        if (!first)
            main += spacingAfter(static_cast<int>(i) - 1);
        first = false;
        const QSizeF hint = entry.item->effectiveSizeHint(which);
        main += along(hint, m_orientation);
        across = std::max(across, along(hint, cross));
    }
    if (which == Qt::MaximumSize && anyStretch)
        main = QLAYOUTSIZE_MAX;
    main = std::min<qreal>(main, QLAYOUTSIZE_MAX);
    across = std::min<qreal>(across, QLAYOUTSIZE_MAX);
    const QSizeF size = m_orientation == Qt::Horizontal ? QSizeF(main, across) : QSizeF(across, main);
    return size + QSizeF(left + right, top + bottom);
}

/*!
    \reimp
*/
void QGraphicsLinearLayout::setGeometry(const QRectF &rect) {
    QGraphicsLayout::setGeometry(rect);
    qreal left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRectF inner = geometry().adjusted(left, top, -right, -bottom);
    const bool horizontal = m_orientation == Qt::Horizontal;
    const Qt::Orientation cross = horizontal ? Qt::Vertical : Qt::Horizontal;

    struct Slot {
        int entry;
        qreal minimum, preferred, maximum, size;
        int stretch;
        bool grows;
    };
    std::vector<Slot> cells;
    qreal spacings = 0;
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        const Entry &entry = m_entries.at(i);
        if (entry.item && entry.item->isEmpty())
            continue;
        if (!cells.empty() && m_entries.at(cells.back().entry).item && entry.item)
            spacings += spacingAfter(cells.back().entry);
        if (!entry.item) {
            cells.push_back(Slot{static_cast<int>(i), 0, 0, QLAYOUTSIZE_MAX, 0, std::max(1, entry.stretch), true});
            continue;
        }
        const QSizePolicy policy = entry.item->sizePolicy();
        const QSizePolicy::Policy p = horizontal ? policy.horizontalPolicy() : policy.verticalPolicy();
        qreal minimum = along(entry.item->effectiveSizeHint(Qt::MinimumSize), m_orientation);
        const qreal preferred = along(entry.item->effectiveSizeHint(Qt::PreferredSize), m_orientation);
        qreal maximum = along(entry.item->effectiveSizeHint(Qt::MaximumSize), m_orientation);
        if (!(static_cast<int>(p) & QSizePolicy::ShrinkFlag))
            minimum = preferred;
        if (!(static_cast<int>(p) & QSizePolicy::GrowFlag))
            maximum = preferred;
        cells.push_back(
            Slot{static_cast<int>(i), minimum, preferred, maximum, preferred, entry.stretch, maximum > preferred});
    }
    // Spacing between an item and a stretch is kept too, as between items.
    const qreal available = along(inner.size(), m_orientation) - spacings;
    qreal total = 0;
    for (const Slot &s : cells)
        total += s.preferred;
    if (available < total) {
        qreal shrinkable = 0;
        for (const Slot &s : cells)
            shrinkable += s.preferred - s.minimum;
        const qreal factor = shrinkable > 0 ? std::min<qreal>(1.0, (total - available) / shrinkable) : 0;
        for (Slot &s : cells)
            s.size = s.preferred - (s.preferred - s.minimum) * factor;
    } else {
        // The room that is left goes to the cells that can grow, by stretch factor; those with a factor of 0 get
        // some only if no slot has one. A slot that reaches its largest size gives the rest back.
        qreal extra = available - total;
        bool anyStretch = false;
        for (const Slot &s : cells)
            anyStretch = anyStretch || (s.grows && s.stretch > 0);
        while (extra > 0.5) {
            qreal weights = 0;
            for (const Slot &s : cells)
                if (s.grows && s.size < s.maximum)
                    weights += anyStretch ? s.stretch : 1;
            if (weights <= 0)
                break;
            qreal given = 0;
            for (Slot &s : cells) {
                if (!s.grows || s.size >= s.maximum)
                    continue;
                const qreal weight = anyStretch ? s.stretch : 1;
                const qreal share = std::min(extra * weight / weights, s.maximum - s.size);
                s.size += share;
                given += share;
            }
            extra -= given;
            if (given <= 0)
                break;
        }
    }
    qreal pos = horizontal ? inner.left() : inner.top();
    const qreal crossAvailable = along(inner.size(), cross);
    for (std::size_t k = 0; k < cells.size(); ++k) {
        const Slot &s = cells[k];
        const Entry &entry = m_entries.at(s.entry);
        if (entry.item) {
            const QSizePolicy policy = entry.item->sizePolicy();
            const QSizePolicy::Policy p = horizontal ? policy.verticalPolicy() : policy.horizontalPolicy();
            const qreal crossMin = along(entry.item->effectiveSizeHint(Qt::MinimumSize), cross);
            const qreal crossPref = along(entry.item->effectiveSizeHint(Qt::PreferredSize), cross);
            const qreal crossMax = along(entry.item->effectiveSizeHint(Qt::MaximumSize), cross);
            qreal size = (static_cast<int>(p) & QSizePolicy::GrowFlag) ? std::min(crossAvailable, crossMax)
                                                                       : std::min(crossAvailable, crossPref);
            size = std::max(size, crossMin);
            const Qt::Alignment a = entry.alignment;
            const qreal room = crossAvailable - size;
            qreal offset = 0;
            if (horizontal)
                offset = (a & Qt::AlignVCenter) ? room / 2 : (a & Qt::AlignBottom) ? room : 0;
            else
                offset = (a & Qt::AlignHCenter) ? room / 2 : (a & Qt::AlignRight) ? room : 0;
            const qreal crossStart = (horizontal ? inner.top() : inner.left()) + offset;
            entry.item->setGeometry(horizontal ? QRectF(pos, crossStart, s.size, size)
                                               : QRectF(crossStart, pos, size, s.size));
        }
        pos += s.size;
        if (k + 1 < cells.size() && entry.item && m_entries.at(cells[k + 1].entry).item)
            pos += spacingAfter(s.entry);
    }
}

// ---- QGraphicsWidget ----

/*!
    \class QGraphicsWidget
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsWidget class is a graphics item with a geometry, a layout, a font and a palette.

    A graphics widget has a size as well as a position, and can lay its children out with a QGraphicsLayout:

    \code
    auto *container = new QGraphicsWidget;
    scene->addItem(container);
    auto *layout = new QGraphicsLinearLayout(Qt::Horizontal, container);
    layout->addItem(new QGraphicsProxyWidget);
    container->adjustSize();
    \endcode

    QGraphicsProxyWidget is a graphics widget, so widgets can be put in layouts.

    \differences
    The focus policy, \c setTabOrder(), the shortcut functions, the attributes and the window frame margins are kept
    but have no effect, and the window frame is never drawn. The style is only handed out, nothing is drawn with it.
    The actions are kept but not shown. The font and palette are not passed on to the children.
    \enddifferences

    \sa QGraphicsLinearLayout, QGraphicsProxyWidget
*/

/*!
    Constructs an empty widget with the given \a parent and the window flags \a wFlags.
*/
QGraphicsWidget::QGraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags wFlags)
    : QGraphicsObject(parent), QGraphicsLayoutItem(nullptr, false), m_windowFlags(wFlags) {
    setGraphicsItem(this);
    // A move, by setPos() or setGeometry(), sends a move event.
    const auto moved = [this] {
        if (pos() == m_lastPos)
            return;
        QGraphicsSceneMoveEvent move;
        move.setOldPos(m_lastPos);
        move.setNewPos(pos());
        m_lastPos = pos();
        QCoreApplication::sendEvent(this, &move);
        if (!m_settingGeometry) // setGeometry() emits it once for the move and the resize
            emit geometryChanged();
    };
    QObject::connect(this, &QGraphicsObject::xChanged, this, moved);
    QObject::connect(this, &QGraphicsObject::yChanged, this, moved);
    if (qApp) {
        m_font = QApplication::font();
        m_palette = QApplication::palette();
    }
}

/*!
    Destroys the widget, its layout and its children.
*/
QGraphicsWidget::~QGraphicsWidget() {
    if (QGraphicsLayoutItem *parent = parentLayoutItem(); parent && parent->isLayout()) {
        auto *layout = static_cast<QGraphicsLayout *>(parent);
        for (int i = 0; i < layout->count(); ++i) {
            if (layout->itemAt(i) == this) {
                layout->removeAt(i);
                break;
            }
        }
    }
    delete m_layout;
}

/*!
    Sets the layout of the widget to \a layout, and deletes the layout it had. The widget takes ownership of
    \a layout, and the items in it become its children.
*/
void QGraphicsWidget::setLayout(QGraphicsLayout *layout) {
    if (layout == m_layout)
        return;
    QGraphicsLayout *old = m_layout;
    m_layout = layout;
    if (old) {
        old->setParentLayoutItem(nullptr);
        delete old;
    }
    if (layout) {
        layout->setParentLayoutItem(this);
        for (int i = 0; i < layout->count(); ++i)
            layout->addChildLayoutItem(layout->itemAt(i));
        layout->invalidate();
    }
    emit layoutChanged();
}

/*!
    \fn QGraphicsLayout *QGraphicsWidget::layout() const

    Returns the layout of the widget, or \c nullptr.
*/

/*!
    Resizes the widget to its preferred size, which its layout gives if it has one.
*/
void QGraphicsWidget::adjustSize() {
    const QSizeF preferred = effectiveSizeHint(Qt::PreferredSize);
    if (preferred.isValid())
        resize(preferred);
}

/*!
    \fn QRectF QGraphicsWidget::geometry() const

    Returns the position and the size of the widget, in the coordinates of its parent.
*/

/*!
    \fn QSizeF QGraphicsWidget::size() const

    Returns the size of the widget.
*/

/*!
    Resizes the widget to \a size, kept between its smallest and largest size.
*/
void QGraphicsWidget::resize(const QSizeF &size) { setGeometry(QRectF(pos(), size)); }

/*!
    \fn void QGraphicsWidget::resize(qreal w, qreal h)
    \overload

    Resizes the widget to \a w by \a h.
*/

/*!
    \fn QRectF QGraphicsWidget::rect() const

    Returns the rectangle of the widget in its own coordinates: from 0, 0 to its size.
*/

/*!
    Moves and resizes the widget to \a rect, in the coordinates of its parent; the size is kept between its smallest
    and largest size. The layout is laid out again.
*/
void QGraphicsWidget::setGeometry(const QRectF &requested) {
    const QRectF rect = propertyChange(QStringLiteral("geometry"), QVariant(requested)).toRectF();
    const QRectF old = geometry();
    const QSizeF minimum = effectiveSizeHint(Qt::MinimumSize);
    const QSizeF maximum = effectiveSizeHint(Qt::MaximumSize);
    const QSizeF size(std::clamp(rect.width(), minimum.width(), maximum.width()),
                      std::clamp(rect.height(), minimum.height(), maximum.height()));
    if (size != old.size())
        prepareGeometryChange();
    QGraphicsLayoutItem::setGeometry(QRectF(rect.topLeft(), size));
    m_settingGeometry = true;
    setPos(rect.topLeft());
    m_settingGeometry = false;
    if (m_layout)
        m_layout->activate();
    if (size != old.size()) {
        QGraphicsSceneResizeEvent resize;
        resize.setOldSize(old.size());
        resize.setNewSize(size);
        QCoreApplication::sendEvent(this, &resize);
    }
    if (geometry() != old) {
        emit geometryChanged();
        if (size.width() != old.width())
            emit widthChanged();
        if (size.height() != old.height())
            emit heightChanged();
    }
}

/*!
    \fn void QGraphicsWidget::setGeometry(qreal x, qreal y, qreal w, qreal h)
    \overload

    Moves the widget to \a x, \a y and resizes it to \a w by \a h.
*/

/*!
    Sets the margins of the widget, around its layout, to \a left, \a top, \a right and \a bottom.
*/
void QGraphicsWidget::setContentsMargins(qreal left, qreal top, qreal right, qreal bottom) {
    m_margins[0] = left;
    m_margins[1] = top;
    m_margins[2] = right;
    m_margins[3] = bottom;
    updateGeometry();
    if (m_layout)
        m_layout->invalidate();
}

/*!
    \reimp
*/
void QGraphicsWidget::getContentsMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const {
    if (left)
        *left = m_margins[0];
    if (top)
        *top = m_margins[1];
    if (right)
        *right = m_margins[2];
    if (bottom)
        *bottom = m_margins[3];
}

/*!
    \fn QFont QGraphicsWidget::font() const

    Returns the font of the widget, the font of the application by default.
*/

/*!
    Sets the font of the widget to \a font.
*/
void QGraphicsWidget::setFont(const QFont &font) {
    m_font = font;
    update();
}

/*!
    \fn QPalette QGraphicsWidget::palette() const

    Returns the palette of the widget, the palette of the application by default.
*/

/*!
    Sets the palette of the widget to \a palette.
*/
void QGraphicsWidget::setPalette(const QPalette &palette) {
    m_palette = palette;
    update();
}

/*!
    \fn bool QGraphicsWidget::autoFillBackground() const

    Returns \c true if the widget fills its rectangle with the window color of its palette.
*/

/*!
    Makes the widget fill its rectangle with the window color of its palette if \a enabled is \c true.
*/
void QGraphicsWidget::setAutoFillBackground(bool enabled) {
    m_autoFill = enabled;
    update();
}

/*!
    \fn Qt::WindowFlags QGraphicsWidget::windowFlags() const

    Returns the window flags of the widget. Nt keeps them but gives graphics widgets no window frame.
*/

/*!
    \fn void QGraphicsWidget::setWindowFlags(Qt::WindowFlags wFlags)

    Sets the window flags of the widget to \a wFlags.
*/

/*!
    \reimp
*/
void QGraphicsWidget::updateGeometry() {
    if (QGraphicsLayoutItem *parent = parentLayoutItem(); parent && parent->isLayout())
        static_cast<QGraphicsLayout *>(parent)->updateGeometry();
}

/*!
    \reimp
*/
QRectF QGraphicsWidget::boundingRect() const { return rect(); }

/*!
    \reimp
*/
QPainterPath QGraphicsWidget::shape() const {
    QPainterPath path;
    path.addRect(rect());
    return path;
}

/*!
    \reimp

    Fills the rectangle of the widget if autoFillBackground() is \c true; otherwise draws nothing.
*/
void QGraphicsWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)option;
    (void)widget;
    if (m_autoFill)
        painter->fillRect(rect(), m_palette.color(QPalette::Window));
}

/*!
    \reimp
*/
int QGraphicsWidget::type() const { return Type; }

/*!
    \reimp

    Returns the size hint of the layout and the margins if the widget has a layout; otherwise 0 by 0 as the
    smallest, 50 by 50 as the preferred and no largest size.
*/
QSizeF QGraphicsWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    if (m_layout) {
        const QSizeF hint = m_layout->effectiveSizeHint(which, constraint);
        if (which == Qt::MaximumSize)
            return hint;
        return hint + QSizeF(m_margins[0] + m_margins[2], m_margins[1] + m_margins[3]);
    }
    switch (which) {
    case Qt::MinimumSize:
        return QSizeF(0, 0);
    case Qt::PreferredSize:
        return QSizeF(50, 50);
    case Qt::MaximumSize:
        return QSizeF(QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX);
    default:
        return QSizeF(-1, -1);
    }
}

/*!
    \since 6.0

    Is called with \a event when the widget was resized. The default implementation does nothing; the layout is
    laid out by setGeometry() already.
*/
void QGraphicsWidget::resizeEvent(QGraphicsSceneResizeEvent *event) { (void)event; }

/*!
    \since 6.0

    Is called with \a event when the widget moved. The default implementation does nothing.
*/
void QGraphicsWidget::moveEvent(QGraphicsSceneMoveEvent *event) { (void)event; }

void QGraphicsWidget::requestLayout() {
    if (m_layoutRequested)
        return;
    m_layoutRequested = true;
    QCoreApplication::postEvent(this, new QEvent(QEvent::LayoutRequest));
}

/*!
    \reimp
*/
bool QGraphicsWidget::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::Close:
        closeEvent(static_cast<QCloseEvent *>(event));
        return true;
    case QEvent::Polish:
        polishEvent();
        return true;
    case QEvent::Show:
        showEvent(static_cast<QShowEvent *>(event));
        return true;
    case QEvent::Hide:
        hideEvent(static_cast<QHideEvent *>(event));
        return true;
    case QEvent::FontChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::EnabledChange:
        changeEvent(event);
        return true;
    default:
        break;
    }
    if (event->type() == QEvent::GraphicsSceneResize) {
        resizeEvent(static_cast<QGraphicsSceneResizeEvent *>(event));
        return true;
    }
    if (event->type() == QEvent::GraphicsSceneMove) {
        moveEvent(static_cast<QGraphicsSceneMoveEvent *>(event));
        return true;
    }
    if (event->type() == QEvent::LayoutRequest) {
        m_layoutRequested = false;
        if (m_layout && !m_layout->isActivated())
            m_layout->activate();
        return true;
    }
    if (m_layout)
        m_layout->widgetEvent(event);
    return QGraphicsObject::event(event);
}

/*!
    \fn Qt::WindowType QGraphicsWidget::windowType() const

    Returns the window type from the window flags.
*/

/*!
    \fn QString QGraphicsWidget::windowTitle() const

    Returns the title of the window; only kept.
*/

/*!
    \fn void QGraphicsWidget::setWindowTitle(const QString &title)

    Sets the title of the window to \a title; only kept.
*/

/*!
    Returns \c true if the widget is in the active scene.
*/
bool QGraphicsWidget::isActiveWindow() const { return scene() && scene()->isActive(); }

/*!
    \fn Qt::FocusPolicy QGraphicsWidget::focusPolicy() const

    Returns how the widget gets focus; Qt::NoFocus by default. Only kept.
*/

/*!
    \fn void QGraphicsWidget::setFocusPolicy(Qt::FocusPolicy policy)

    Sets how the widget gets focus to \a policy; only kept.
*/

/*!
    Returns the widget below this one, or this one, that has the focus, or \c nullptr.
*/
QGraphicsWidget *QGraphicsWidget::focusWidget() const {
    if (!scene())
        return nullptr;
    QGraphicsItem *focus = scene()->focusItem();
    if (focus != this && !isAncestorOf(focus))
        return nullptr;
    while (focus) {
        if (auto *widget = dynamic_cast<QGraphicsWidget *>(focus))
            return widget;
        focus = focus->parentItem();
    }
    return nullptr;
}

/*!
    Does nothing: the tab order of \a first and \a second is not kept.
*/
void QGraphicsWidget::setTabOrder(QGraphicsWidget *first, QGraphicsWidget *second) {
    (void)first;
    (void)second;
}

/*!
    Returns the style that was set, or the style of the scene, or the style of the application.
*/
QStyle *QGraphicsWidget::style() const {
    if (m_style)
        return m_style.data();
    return scene() ? scene()->style() : QApplication::style();
}

/*!
    \fn void QGraphicsWidget::setStyle(QStyle *style)

    Sets the style of the widget to \a style, which the widget does not own. \c nullptr brings back the style of the
    scene.
*/

/*!
    \fn Qt::LayoutDirection QGraphicsWidget::layoutDirection() const

    Returns the direction in which the layouts of the widget place items, Qt::LeftToRight by default.
*/

/*!
    Sets the layout direction to \a direction and sends a QEvent::LayoutDirectionChange to the widget.
*/
void QGraphicsWidget::setLayoutDirection(Qt::LayoutDirection direction) {
    m_layoutDirectionSet = true;
    if (m_layoutDirection == direction)
        return;
    m_layoutDirection = direction;
    QEvent change(QEvent::LayoutDirectionChange);
    QCoreApplication::sendEvent(this, &change);
    if (m_layout)
        m_layout->invalidate();
}

/*!
    Brings back the layout direction of the application.
*/
void QGraphicsWidget::unsetLayoutDirection() {
    m_layoutDirectionSet = false;
    const Qt::LayoutDirection direction = QApplication::layoutDirection();
    if (m_layoutDirection == direction)
        return;
    m_layoutDirection = direction;
    QEvent change(QEvent::LayoutDirectionChange);
    QCoreApplication::sendEvent(this, &change);
}

/*!
    Sets the \a attribute to \a on. The attributes are kept but have no effect.
*/
void QGraphicsWidget::setAttribute(Qt::WidgetAttribute attribute, bool on) {
    if (attribute >= 0 && attribute < Qt::WA_AttributeCount)
        m_attributes.set(static_cast<std::size_t>(attribute), on);
}

/*!
    Returns whether the \a attribute is set.
*/
bool QGraphicsWidget::testAttribute(Qt::WidgetAttribute attribute) const {
    return attribute >= 0 && attribute < Qt::WA_AttributeCount &&
           m_attributes.test(static_cast<std::size_t>(attribute));
}

/*!
    Does nothing: the shortcut \a id is not kept, \a enabled is ignored.
*/
void QGraphicsWidget::setShortcutEnabled(int id, bool enabled) {
    (void)id;
    (void)enabled;
}

/*!
    Does nothing: the shortcut \a id is not kept, \a enabled is ignored.
*/
void QGraphicsWidget::setShortcutAutoRepeat(int id, bool enabled) {
    (void)id;
    (void)enabled;
}

/*!
    Returns a new shortcut number for the key \a sequence and the \a context. The shortcut never fires.
*/
int QGraphicsWidget::grabShortcut(const QKeySequence &sequence, Qt::ShortcutContext context) {
    (void)sequence;
    (void)context;
    return m_nextShortcut++;
}

/*!
    Does nothing: the shortcut \a id is not kept.
*/
void QGraphicsWidget::releaseShortcut(int id) { (void)id; }

/*!
    Sets the margins of the window frame to \a left, \a top, \a right and \a bottom. The frame is not drawn.
*/
void QGraphicsWidget::setWindowFrameMargins(qreal left, qreal top, qreal right, qreal bottom) {
    m_frameMargins[0] = left;
    m_frameMargins[1] = top;
    m_frameMargins[2] = right;
    m_frameMargins[3] = bottom;
    m_frameMarginsSet = true;
}

/*!
    Gets the margins of the window frame into \a left, \a top, \a right and \a bottom. Any of them may be \c nullptr.
*/
void QGraphicsWidget::getWindowFrameMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const {
    if (left)
        *left = m_frameMargins[0];
    if (top)
        *top = m_frameMargins[1];
    if (right)
        *right = m_frameMargins[2];
    if (bottom)
        *bottom = m_frameMargins[3];
}

/*!
    Sets the margins of the window frame to 0.
*/
void QGraphicsWidget::unsetWindowFrameMargins() {
    for (qreal &margin : m_frameMargins)
        margin = 0;
    m_frameMarginsSet = false;
}

/*!
    Returns the rectangle of the widget with its window frame, in the coordinates of the parent.
*/
QRectF QGraphicsWidget::windowFrameGeometry() const { return windowFrameRect().translated(pos()); }

/*!
    Returns the rectangle of the widget with its window frame, in its own coordinates.
*/
QRectF QGraphicsWidget::windowFrameRect() const {
    return rect().adjusted(-m_frameMargins[0], -m_frameMargins[1], m_frameMargins[2], m_frameMargins[3]);
}

/*!
    Returns the part of the window frame at \a pos, in the coordinates of the widget: a corner or an edge within the
    frame margins, TitleBarArea for the top margin between the corners, and Qt::NoSection inside the widget or
    outside the frame.
*/
Qt::WindowFrameSection QGraphicsWidget::windowFrameSectionAt(const QPointF &pos) const {
    const QRectF frame = windowFrameRect();
    if (!frame.contains(pos) || rect().contains(pos))
        return Qt::NoSection;
    const bool left = pos.x() < rect().left();
    const bool right = pos.x() > rect().right();
    const bool top = pos.y() < rect().top();
    const bool bottom = pos.y() > rect().bottom();
    if (top)
        return left ? Qt::TopLeftSection : right ? Qt::TopRightSection : Qt::TitleBarArea;
    if (bottom)
        return left ? Qt::BottomLeftSection : right ? Qt::BottomRightSection : Qt::BottomSection;
    return left ? Qt::LeftSection : Qt::RightSection;
}

/*!
    \fn QList<QAction *> QGraphicsWidget::actions() const

    Returns the actions that were added to the widget, in order.
*/

/*!
    \fn void QGraphicsWidget::addAction(QAction *action)

    Adds the \a action at the end of the list of actions.
*/

/*!
    \fn void QGraphicsWidget::addActions(const QList<QAction *> &actions)

    Adds the \a actions at the end of the list of actions.
*/

/*!
    Inserts the \a action before the action \a before, or at the end if \a before is \c nullptr or not in the list.
    An action that is in the list already moves.
*/
void QGraphicsWidget::insertAction(QAction *before, QAction *action) {
    if (!action)
        return;
    m_actions.removeAll(action);
    const qsizetype index = before ? m_actions.indexOf(before) : -1;
    if (index >= 0)
        m_actions.insert(index, action);
    else
        m_actions.append(action);
}

/*!
    Inserts the \a actions before the action \a before, in order.

    \sa insertAction()
*/
void QGraphicsWidget::insertActions(QAction *before, const QList<QAction *> &actions) {
    for (QAction *action : actions)
        insertAction(before, action);
}

/*!
    Removes the \a action from the list of actions.
*/
void QGraphicsWidget::removeAction(QAction *action) { m_actions.removeAll(action); }

/*!
    \reimp

    Passes the grab and ungrab events to grabMouseEvent() and the functions like it.
*/
bool QGraphicsWidget::sceneEvent(QEvent *event) {
    switch (event->type()) {
    case QEvent::GrabMouse:
        grabMouseEvent(event);
        return true;
    case QEvent::UngrabMouse:
        ungrabMouseEvent(event);
        return true;
    case QEvent::GrabKeyboard:
        grabKeyboardEvent(event);
        return true;
    case QEvent::UngrabKeyboard:
        ungrabKeyboardEvent(event);
        return true;
    default:
        return QGraphicsObject::sceneEvent(event);
    }
}

/*!
    Handles the event \a event that tells that the widget grabbed the mouse; does nothing.
*/
void QGraphicsWidget::grabMouseEvent(QEvent *event) { (void)event; }

/*!
    Handles the event \a event that tells that the widget let go of the mouse; does nothing.
*/
void QGraphicsWidget::ungrabMouseEvent(QEvent *event) { (void)event; }

/*!
    Handles the event \a event that tells that the widget grabbed the keyboard; does nothing.
*/
void QGraphicsWidget::grabKeyboardEvent(QEvent *event) { (void)event; }

/*!
    Handles the event \a event that tells that the widget let go of the keyboard; does nothing.
*/
void QGraphicsWidget::ungrabKeyboardEvent(QEvent *event) { (void)event; }

/*!
    Handles the polish event, which tells that the style and the font are ready, before the widget is shown the first time.
    Nt does not send it; an application may. Does nothing by default.
*/
void QGraphicsWidget::polishEvent() {}

/*!
    Handles the event \a event of the window frame. Returns \c true if it was handled; the default does not draw or handle a
    frame, and returns \c false.
*/
bool QGraphicsWidget::windowFrameEvent(QEvent *event) {
    (void)event;
    return false;
}

/*!
    Is called with the name \a propertyName and the new \a value before a property changes, to let the widget change the
    value: the function returns the value that is used. Nt calls it for \c "geometry", with a QRectF. The default returns
    \a value.
*/
QVariant QGraphicsWidget::propertyChange(const QString &propertyName, const QVariant &value) {
    (void)propertyName;
    return value;
}

/*!
    Moves the focus to the next focusable item inside the widget, itself included, if \a next is \c true, and to the previous
    one otherwise, wrapping at the ends. Returns \c true if the focus moved.
*/
bool QGraphicsWidget::focusNextPrevChild(bool next) {
    if (!scene())
        return false;
    QList<QGraphicsItem *> chain;
    const std::function<void(QGraphicsItem *)> collect = [&](QGraphicsItem *item) {
        if ((item->flags() & QGraphicsItem::ItemIsFocusable) && item->isVisible() && item->isEnabled())
            chain.append(item);
        for (QGraphicsItem *child : item->childItems())
            collect(child);
    };
    collect(this);
    if (chain.isEmpty())
        return false;
    const qsizetype current = chain.indexOf(scene()->focusItem());
    qsizetype target = 0;
    if (current >= 0)
        target = next ? (current + 1) % chain.size() : (current + chain.size() - 1) % chain.size();
    else if (!next)
        target = chain.size() - 1;
    if (target == current)
        return false;
    scene()->setFocusItem(chain.at(target), next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
    return true;
}

/*!
    Prints the focusable items inside the widget, in the order of focusNextPrevChild(), with qDebug().
*/
void QGraphicsWidget::dumpFocusChain() const {
    const std::function<void(const QGraphicsItem *, int)> dump = [&](const QGraphicsItem *item, int depth) {
        if (item->flags() & QGraphicsItem::ItemIsFocusable)
            qDebug("%*s%p%s", depth * 2, "", static_cast<const void *>(item), item->hasFocus() ? " (focus)" : "");
        for (const QGraphicsItem *child : item->childItems())
            dump(child, depth + 1);
    };
    dump(this, 0);
}

/*!
    Sends a close event to the widget. If it accepts it (the default), the widget is hidden and \c true is returned.
*/
bool QGraphicsWidget::close() {
    QCloseEvent closing;
    QCoreApplication::sendEvent(this, &closing);
    if (!closing.isAccepted())
        return false;
    hide();
    return true;
}

/*!
    Handles the close \a event: accepts it. Reimplement it to ignore the event and keep the widget open.
*/
void QGraphicsWidget::closeEvent(QCloseEvent *event) { event->accept(); }

/*!
    Handles the show \a event; does nothing.
*/
void QGraphicsWidget::showEvent(QShowEvent *event) { (void)event; }

/*!
    Handles the hide \a event; does nothing.
*/
void QGraphicsWidget::hideEvent(QHideEvent *event) { (void)event; }

/*!
    Handles the change \a event, for the font, palette, style, layout direction and enabled state: repaints the widget.
*/
void QGraphicsWidget::changeEvent(QEvent *event) {
    (void)event;
    update();
}

/*!
    Fills \a option with the state of the widget: its rectangle, palette, layout direction and whether it is enabled,
    active or has the focus.
*/
void QGraphicsWidget::initStyleOption(QStyleOption *option) const {
    if (!option)
        return;
    option->state = QStyle::State_None;
    if (isEnabled())
        option->state |= QStyle::State_Enabled;
    if (hasFocus())
        option->state |= QStyle::State_HasFocus;
    if (isActiveWindow())
        option->state |= QStyle::State_Active;
    option->direction = m_layoutDirection;
    option->rect = rect().toRect();
    option->palette = m_palette;
    option->fontMetrics = QFontMetrics(m_font);
}

/*!
    Draws the window frame with the \a painter. The frame is not drawn by default, and the default implementation
    does nothing; \a option and \a widget are the arguments of paint().
*/
void QGraphicsWidget::paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)painter;
    (void)option;
    (void)widget;
}

#include "moc_qgraphicswidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
