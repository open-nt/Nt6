// MIT License - see LICENSE
#include <QtWidgets/qboxlayout.h>

#include <QtCore/qdebug.h>

#include "qwidgetutil_p.h"

#include <QtWidgets/qwidget.h>

#include <algorithm>

namespace {

struct Cell {
    QLayoutItem *item;
    int stretch;
    int min;
    int hint;
    int max;
    bool expansive;
    int size;
};

// Gives `extra` pixels to the cells that `eligible` selects, in proportion to `weight`, never past the maximum.
template <typename Eligible, typename Weight>
void grow(std::vector<Cell> &cells, int extra, Eligible eligible, Weight weight) {
    while (extra > 0) {
        long long total = 0;
        for (const Cell &s : cells)
            if (eligible(s) && s.size < s.max)
                total += weight(s);
        if (total <= 0)
            return;
        int used = 0;
        bool clamped = false;
        for (Cell &s : cells) {
            if (!eligible(s) || s.size >= s.max)
                continue;
            const int share = static_cast<int>(static_cast<long long>(extra) * weight(s) / total);
            const int give = std::min(share, s.max - s.size);
            s.size += give;
            used += give;
            if (give < share)
                clamped = true;
        }
        extra -= used;
        if (!clamped) {
            // What is left from rounding goes to the first cells, one pixel each.
            for (Cell &s : cells) {
                if (extra <= 0)
                    break;
                if (eligible(s) && s.size < s.max && weight(s) > 0) {
                    ++s.size;
                    --extra;
                }
            }
            return;
        }
    }
}

} // namespace

/*!
    \class QBoxLayout
    \brief The QBoxLayout class lines up child widgets horizontally or vertically.

    A box layout puts its items in a row or a column, as its direction says. Each item gets its size hint
    along the box, and the space that is left over (or missing) is shared out as follows. If any
    item has a stretch factor, only the items with one grow, in proportion to the factors. Otherwise the items that
    want to expand (their size policy has the Expanding flag, as a stretch item does) share the space equally, and if there
    are none, all items that can grow do. If the box is too small, items shrink down to their minimum size in proportion to
    how much they can shrink. Across the box, items fill the available space within their limits.

    \code
    QVBoxLayout *layout = new QVBoxLayout(window);
    layout->addWidget(new QLabel("Title"));
    layout->addWidget(list, 1); // takes the remaining space
    QHBoxLayout *row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(ok);
    layout->addLayout(row);
    \endcode

    \differences
    There is no height-for-width, \c addStrut(), \c setAlignment() on the layout or \c insertItem(). The alignment of an
    item applies across the box only: along the box an item always gets the space that the algorithm gives it.
    \enddifferences

    \sa QHBoxLayout, QVBoxLayout, QLayout
*/

/*!
    \enum QBoxLayout::Direction

    This enum gives the direction in which a box layout places its items.

    \value LeftToRight Horizontally from the left.
    \value RightToLeft Horizontally from the right.
    \value TopToBottom Vertically from the top.
    \value BottomToTop Vertically from the bottom.
*/

/*!
    Constructs a box layout with the given \a direction for \a parent.
*/
QBoxLayout::QBoxLayout(Direction direction, QWidget *parent) : QLayout(parent), m_direction(direction) {}

/*!
    Destroys the layout and the items in it, but not the widgets. A layout that is in another layout is removed from it.
*/
QBoxLayout::~QBoxLayout() {
    // A layout that is deleted leaves the layout it is in.
    if (auto *parentLayout = dynamic_cast<QLayout *>(parent()))
        parentLayout->removeItem(this);
    for (Entry &e : m_items)
        if (e.owned)
            delete e.item;
}

/*!
    \fn QBoxLayout::Direction QBoxLayout::direction() const

    Returns the direction of the layout.

    \sa setDirection()
*/

/*!
    Sets the direction of the layout to \a direction.

    \sa direction()
*/
void QBoxLayout::setDirection(Direction direction) {
    if (m_direction == direction)
        return;
    m_direction = direction;
    invalidate();
}

void QBoxLayout::insertEntry(int index, QLayoutItem *item, bool owned, int stretch) {
    const int n = static_cast<int>(m_items.size());
    if (index < 0 || index > n)
        index = n;
    m_items.insert(m_items.begin() + index, Entry{item, owned, stretch});
    invalidate();
}

/*!
    Adds a fixed space of \a size pixels at the end of the layout, besides the spacing.

    \sa addStretch()
*/
void QBoxLayout::addSpacing(int size) { insertSpacing(-1, size); }

/*!
    Adds stretchable space with the stretch factor \a stretch at the end of the layout.

    \sa addSpacing()
*/
void QBoxLayout::addStretch(int stretch) { insertStretch(-1, stretch); }

/*!
    Adds \a widget at the end of the layout with the stretch factor \a stretch and the \a alignment. Across the box, a widget with an
    alignment gets its size hint instead of all the space, and is placed as the alignment says.

    \sa insertWidget(), addLayout()
*/
void QBoxLayout::addWidget(QWidget *widget, int stretch, Qt::Alignment alignment) {
    insertWidget(-1, widget, stretch, alignment);
}

/*!
    Adds \a layout at the end of the layout with the stretch factor \a stretch. It becomes a child of this layout.

    \sa insertLayout()
*/
void QBoxLayout::addLayout(QLayout *layout, int stretch) { insertLayout(-1, layout, stretch); }

/*!
    Inserts a fixed space of \a size pixels at \a index. An index of -1 means the end.
*/
void QBoxLayout::insertSpacing(int index, int size) {
    QSpacerItem *item = horizontal() ? new QSpacerItem(size, 0, QSizePolicy::Fixed, QSizePolicy::Minimum)
                                     : new QSpacerItem(0, size, QSizePolicy::Minimum, QSizePolicy::Fixed);
    insertEntry(index, item, true, 0);
}

/*!
    Inserts stretchable space with the stretch factor \a stretch at \a index.
*/
void QBoxLayout::insertStretch(int index, int stretch) {
    QSpacerItem *item = horizontal() ? new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum)
                                     : new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    insertEntry(index, item, true, stretch);
}

/*!
    Inserts \a widget at \a index, with the stretch factor \a stretch and the \a alignment; see addWidget().
*/
void QBoxLayout::insertWidget(int index, QWidget *widget, int stretch, Qt::Alignment alignment) {
    if (!widget)
        return;
    addChildWidget(widget);
    auto *item = new QWidgetItem(widget);
    item->setAlignment(alignment);
    insertEntry(index, item, true, stretch);
}

/*!
    Inserts \a layout at \a index with the stretch factor \a stretch. It becomes a child of this layout.
*/
void QBoxLayout::insertLayout(int index, QLayout *layout, int stretch) {
    if (!layout)
        return;
    if (layout->parent() && layout->parent() != this) {
        qWarning("QBoxLayout::insertLayout: the layout already has a parent");
        return;
    }
    layout->QObject::setParent(this);
    layout->reparentChildWidgets();
    insertEntry(index, layout, false, stretch);
}

/*!
    Sets the stretch factor of the item at \a index to \a stretch.

    \sa stretch()
*/
void QBoxLayout::setStretch(int index, int stretch) {
    if (index >= 0 && index < count()) {
        m_items[static_cast<std::size_t>(index)].stretch = stretch;
        invalidate();
    }
}

/*!
    Returns the stretch factor of the item at \a index, or 0.

    \sa setStretch()
*/
int QBoxLayout::stretch(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].stretch : 0;
}

/*!
    Sets the stretch factor of \a widget to \a stretch and returns \c true if the widget is in this layout.
*/
bool QBoxLayout::setStretchFactor(QWidget *widget, int stretch) {
    const int index = indexOf(widget);
    if (index < 0)
        return false;
    setStretch(index, stretch);
    return true;
}

/*!
    Adds \a item at the end of the layout, which takes ownership of it.
*/
void QBoxLayout::addItem(QLayoutItem *item) { insertEntry(-1, item, item->layout() == nullptr, 0); }

/*!
    \reimp
*/
void QBoxLayout::removeItem(QLayoutItem *item) { QLayout::removeItem(item); }

/*!
    \reimp
*/
QLayoutItem *QBoxLayout::itemAt(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].item : nullptr;
}

/*!
    \reimp
*/
QLayoutItem *QBoxLayout::takeAt(int index) {
    if (index < 0 || index >= count())
        return nullptr;
    QLayoutItem *item = m_items[static_cast<std::size_t>(index)].item;
    m_items.erase(m_items.begin() + index);
    return item;
}

/*!
    \fn int QBoxLayout::count() const

    \reimp
*/

/*!
    \reimp
*/
QSize QBoxLayout::sizeHint() const {
    int along = 0, across = 0, n = 0;
    for (const Entry &e : m_items) {
        if (e.item->isEmpty() && !e.item->spacerItem())
            continue;
        const QSize s = e.item->sizeHint();
        along += horizontal() ? s.width() : s.height();
        across = std::max(across, horizontal() ? s.height() : s.width());
        ++n;
    }
    if (n > 1)
        along += (n - 1) * spacing();
    const QMargins m = contentsMargins();
    const QSize content = horizontal() ? QSize(along, across) : QSize(across, along);
    return QSize(content.width() + m.left() + m.right(), content.height() + m.top() + m.bottom());
}

/*!
    \reimp
*/
QSize QBoxLayout::minimumSize() const {
    int along = 0, across = 0, n = 0;
    for (const Entry &e : m_items) {
        if (e.item->isEmpty() && !e.item->spacerItem())
            continue;
        const QSize s = e.item->minimumSize();
        along += horizontal() ? s.width() : s.height();
        across = std::max(across, horizontal() ? s.height() : s.width());
        ++n;
    }
    if (n > 1)
        along += (n - 1) * spacing();
    const QMargins m = contentsMargins();
    const QSize content = horizontal() ? QSize(along, across) : QSize(across, along);
    return QSize(content.width() + m.left() + m.right(), content.height() + m.top() + m.bottom());
}

/*!
    \reimp
*/
void QBoxLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    const QRect r = rect.marginsRemoved(contentsMargins());
    const bool h = horizontal();

    std::vector<Cell> cells;
    bool anyStretch = false;
    for (const Entry &e : m_items) {
        if (e.item->isEmpty() && !e.item->spacerItem())
            continue;
        const QSize hint = e.item->sizeHint(), mn = e.item->minimumSize(), mx = e.item->maximumSize();
        Cell s;
        s.item = e.item;
        s.stretch = e.stretch;
        s.min = h ? mn.width() : mn.height();
        s.hint = std::max(s.min, h ? hint.width() : hint.height());
        s.max = std::max(s.hint, h ? mx.width() : mx.height());
        s.expansive = e.item->expandingDirections().testFlag(h ? Qt::Horizontal : Qt::Vertical);
        s.size = s.hint;
        anyStretch = anyStretch || e.stretch > 0;
        cells.push_back(s);
    }
    if (cells.empty())
        return;

    const int gap = spacing();
    const int available = (h ? r.width() : r.height()) - static_cast<int>(cells.size() - 1) * gap;
    int total = 0;
    for (const Cell &s : cells)
        total += s.size;

    if (anyStretch) {
        // Items with a stretch factor share what the others leave, in proportion to the factors, within their limits.
        int fixed = 0;
        for (const Cell &c : cells)
            if (c.stretch <= 0)
                fixed += c.size;
        int remaining = available - fixed;
        std::vector<Cell *> open;
        long long weights = 0;
        for (Cell &c : cells) {
            if (c.stretch > 0) {
                open.push_back(&c);
                weights += c.stretch;
            }
        }
        bool settled = false;
        while (!settled && !open.empty()) {
            settled = true;
            for (auto it = open.begin(); it != open.end();) {
                Cell &c = **it;
                const int share =
                    weights > 0 ? static_cast<int>(static_cast<long long>(remaining) * c.stretch / weights) : 0;
                if (share < c.min || share > c.max) {
                    c.size = share < c.min ? c.min : c.max;
                    remaining -= c.size;
                    weights -= c.stretch;
                    it = open.erase(it);
                    settled = false;
                } else {
                    ++it;
                }
            }
        }
        int given = 0;
        for (Cell *c : open) {
            c->size = static_cast<int>(static_cast<long long>(remaining) * c->stretch / weights);
            given += c->size;
        }
        // Pixels lost in the division go to the first items that can take them.
        for (Cell *c : open) {
            if (given >= remaining)
                break;
            if (c->size < c->max) {
                ++c->size;
                ++given;
            }
        }
    } else if (available > total) {
        const int extra = available - total;
        // Items that want to expand grow first; what they cannot take goes to everything else that can grow.
        grow(cells, extra, [](const Cell &c) { return c.expansive; }, [](const Cell &) { return 1; });
        int used = 0;
        for (const Cell &c : cells)
            used += c.size;
        if (available > used)
            grow(cells, available - used, [](const Cell &) { return true; }, [](const Cell &) { return 1; });
    } else if (available < total) {
        const int deficit = total - available;
        long long room = 0;
        for (const Cell &c : cells)
            room += c.size - c.min;
        if (room > 0) {
            const int take = static_cast<int>(std::min<long long>(deficit, room));
            int taken = 0;
            for (Cell &c : cells) {
                const int cut = static_cast<int>(static_cast<long long>(take) * (c.size - c.min) / room);
                c.size -= cut;
                taken += cut;
            }
            for (Cell &c : cells) {
                if (taken >= take)
                    break;
                if (c.size > c.min) {
                    --c.size;
                    ++taken;
                }
            }
        }
    }

    // Place the cells, from the far end if the direction is reversed.
    const bool reversed = m_direction == RightToLeft || m_direction == BottomToTop;
    int pos = h ? r.left() : r.top();
    const int count = static_cast<int>(cells.size());
    for (int n = 0; n < count; ++n) {
        const Cell &s = cells[static_cast<std::size_t>(n)];
        const QSize mn = s.item->minimumSize(), mx = s.item->maximumSize(), hint = s.item->sizeHint();
        const int crossAvailable = h ? r.height() : r.width();
        const int crossMin = h ? mn.height() : mn.width();
        const int crossMax = std::max(crossMin, h ? mx.height() : mx.width());
        int cross = std::clamp(crossAvailable, crossMin, crossMax);
        const Qt::Alignment a = s.item->alignment();
        const Qt::Alignment crossMask = h ? (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter)
                                          : (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter);
        int crossPos = h ? r.top() : r.left();
        if (a & crossMask) {
            cross = std::min(cross, std::max(crossMin, h ? hint.height() : hint.width()));
            const int room = crossAvailable - cross;
            if (a & (h ? Qt::AlignBottom : Qt::AlignRight))
                crossPos += room;
            else if (a & (h ? Qt::AlignVCenter : Qt::AlignHCenter))
                crossPos += room / 2;
        }
        int start = pos;
        if (reversed)
            start = (h ? r.left() + r.right() + 1 : r.top() + r.bottom() + 1) - pos - s.size;
        s.item->setGeometry(h ? QRect(start, crossPos, s.size, cross) : QRect(crossPos, start, cross, s.size));
        pos += s.size + gap;
    }
}

/*!
    \class QHBoxLayout
    \brief The QHBoxLayout class lines up widgets horizontally.

    \sa QBoxLayout, QVBoxLayout
*/

/*!
    Constructs a horizontal box layout for \a parent, which places its items from left to right.
*/
QHBoxLayout::QHBoxLayout(QWidget *parent) : QBoxLayout(LeftToRight, parent) {}

/*!
    Destroys the layout.
*/
QHBoxLayout::~QHBoxLayout() = default;

/*!
    \class QVBoxLayout
    \brief The QVBoxLayout class lines up widgets vertically.

    \sa QBoxLayout, QHBoxLayout
*/

/*!
    Constructs a vertical box layout for \a parent, which places its items from top to bottom.
*/
QVBoxLayout::QVBoxLayout(QWidget *parent) : QBoxLayout(TopToBottom, parent) {}

/*!
    Destroys the layout.
*/
QVBoxLayout::~QVBoxLayout() = default;

#include "moc_qboxlayout.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
