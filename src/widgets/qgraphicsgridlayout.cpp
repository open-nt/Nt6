// MIT License - see LICENSE
#include <QtWidgets/qgraphicsgridlayout.h>

#include <QtWidgets/qapplication.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicswidget.h>
#include <QtWidgets/qlayoutitem.h>
#include <QtWidgets/qstyle.h>

#include <algorithm>

namespace {

qreal along(const QSizeF &size, Qt::Orientation orientation) {
    return orientation == Qt::Horizontal ? size.width() : size.height();
}

// Spreads `available` over the lines: their preferred sizes, shrunk towards the minimum or grown, by stretch, up
// to the maximum.
std::vector<qreal> distribute(const std::vector<qreal> &minimum, const std::vector<qreal> &preferred,
                              const std::vector<qreal> &maximum, const std::vector<int> &stretch, qreal available) {
    std::vector<qreal> sizes = preferred;
    qreal total = 0;
    for (qreal p : preferred)
        total += p;
    if (available < total) {
        qreal shrinkable = 0;
        for (std::size_t i = 0; i < sizes.size(); ++i)
            shrinkable += preferred[i] - minimum[i];
        const qreal factor = shrinkable > 0 ? std::min<qreal>(1.0, (total - available) / shrinkable) : 0;
        for (std::size_t i = 0; i < sizes.size(); ++i)
            sizes[i] = preferred[i] - (preferred[i] - minimum[i]) * factor;
        return sizes;
    }
    qreal extra = available - total;
    bool anyStretch = false;
    for (std::size_t i = 0; i < sizes.size(); ++i)
        anyStretch = anyStretch || (stretch[i] > 0 && maximum[i] > preferred[i]);
    while (extra > 0.5) {
        qreal weights = 0;
        for (std::size_t i = 0; i < sizes.size(); ++i)
            if (sizes[i] < maximum[i])
                weights += anyStretch ? stretch[i] : 1;
        if (weights <= 0)
            break;
        qreal given = 0;
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            if (sizes[i] >= maximum[i])
                continue;
            const qreal weight = anyStretch ? stretch[i] : 1;
            const qreal share = std::min(extra * weight / weights, maximum[i] - sizes[i]);
            sizes[i] += share;
            given += share;
        }
        extra -= given;
        if (given <= 0)
            break;
    }
    return sizes;
}

} // namespace

/*!
    \class QGraphicsGridLayout
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsGridLayout class lays graphics widgets out in a grid.

    Each item takes a cell, or a block of cells with a row span and a column span. The rows and the columns are as
    large as their items want, and the room that is left goes to those that can grow, as their stretch factors say:

    \code
    auto *grid = new QGraphicsGridLayout(form);
    grid->addItem(nameLabel, 0, 0);
    grid->addItem(nameField, 0, 1);
    grid->addItem(notes, 1, 0, 1, 2); // across both columns
    grid->setColumnStretchFactor(1, 1);
    \endcode

    \differences
    There is no height-for-width, and an empty row or column takes no room but keeps its spacing.
    \enddifferences

    \sa QGraphicsLinearLayout, QGraphicsWidget
*/

/*!
    Constructs an empty grid in \a parent, a layout or a widget.
*/
QGraphicsGridLayout::QGraphicsGridLayout(QGraphicsLayoutItem *parent) : QGraphicsLayout(parent) {
    if (parent && !parent->isLayout())
        if (auto *widget = dynamic_cast<QGraphicsWidget *>(parent))
            widget->setLayout(this);
}

/*!
    Destroys the grid and the nested layouts it holds; the widgets stay.
*/
QGraphicsGridLayout::~QGraphicsGridLayout() {
    for (const Entry &entry : m_entries) {
        entry.item->setParentLayoutItem(nullptr);
        if (entry.item->ownedByLayout() || entry.item->isLayout())
            delete entry.item;
    }
}

/*!
    Puts \a item in the cell at \a row and \a column, across \a rowSpan rows and \a columnSpan columns, with
    \a alignment in the room it gets. Its graphics item becomes a child of the widget of the grid.
*/
void QGraphicsGridLayout::addItem(QGraphicsLayoutItem *item, int row, int column, int rowSpan, int columnSpan,
                                  Qt::Alignment alignment) {
    if (!item || item == this || row < 0 || column < 0 || rowSpan < 1 || columnSpan < 1)
        return;
    removeItem(item);
    if (QGraphicsLayoutItem *old = item->parentLayoutItem(); old && old->isLayout()) {
        auto *oldLayout = static_cast<QGraphicsLayout *>(old);
        for (int i = 0; i < oldLayout->count(); ++i) {
            if (oldLayout->itemAt(i) == item) {
                oldLayout->removeAt(i);
                break;
            }
        }
    }
    m_entries.push_back(Entry{item, row, column, rowSpan, columnSpan, alignment});
    line(Qt::Vertical, row + rowSpan - 1);
    line(Qt::Horizontal, column + columnSpan - 1);
    addChildLayoutItem(item);
    invalidate();
}

/*!
    \fn void QGraphicsGridLayout::addItem(QGraphicsLayoutItem *item, int row, int column, Qt::Alignment alignment)
    \overload

    Puts \a item in the cell at \a row and \a column with \a alignment.
*/

QGraphicsGridLayout::Line &QGraphicsGridLayout::line(Qt::Orientation orientation, int index) {
    std::vector<Line> &lines = orientation == Qt::Horizontal ? m_columns : m_rows;
    if (index >= static_cast<int>(lines.size()))
        lines.resize(static_cast<std::size_t>(index) + 1);
    return lines[static_cast<std::size_t>(index)];
}

QGraphicsGridLayout::Line QGraphicsGridLayout::lineAt(Qt::Orientation orientation, int index) const {
    const std::vector<Line> &lines = orientation == Qt::Horizontal ? m_columns : m_rows;
    return index >= 0 && index < static_cast<int>(lines.size()) ? lines[static_cast<std::size_t>(index)] : Line();
}

/*!
    Sets the room between the columns to \a spacing.
*/
void QGraphicsGridLayout::setHorizontalSpacing(qreal spacing) {
    m_hSpacing = spacing;
    invalidate();
}

/*!
    Returns the room between the columns; by default it comes from the style.
*/
qreal QGraphicsGridLayout::horizontalSpacing() const {
    if (m_hSpacing >= 0)
        return m_hSpacing;
    QStyle *style = qApp ? QApplication::style() : nullptr;
    const int value = style ? style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) : -1;
    return value >= 0 ? value : 6;
}

/*!
    Sets the room between the rows to \a spacing.
*/
void QGraphicsGridLayout::setVerticalSpacing(qreal spacing) {
    m_vSpacing = spacing;
    invalidate();
}

/*!
    Returns the room between the rows; by default it comes from the style.
*/
qreal QGraphicsGridLayout::verticalSpacing() const {
    if (m_vSpacing >= 0)
        return m_vSpacing;
    QStyle *style = qApp ? QApplication::style() : nullptr;
    const int value = style ? style->pixelMetric(QStyle::PM_LayoutVerticalSpacing) : -1;
    return value >= 0 ? value : 6;
}

/*!
    Sets the room between the rows and between the columns to \a spacing.
*/
void QGraphicsGridLayout::setSpacing(qreal spacing) {
    m_hSpacing = m_vSpacing = spacing;
    invalidate();
}

/*!
    Sets the room after \a row to \a spacing.
*/
void QGraphicsGridLayout::setRowSpacing(int row, qreal spacing) {
    if (row < 0)
        return;
    line(Qt::Vertical, row).spacing = spacing;
    invalidate();
}

/*!
    Returns the room after \a row.
*/
qreal QGraphicsGridLayout::rowSpacing(int row) const { return gap(Qt::Vertical, row); }

/*!
    Sets the room after \a column to \a spacing.
*/
void QGraphicsGridLayout::setColumnSpacing(int column, qreal spacing) {
    if (column < 0)
        return;
    line(Qt::Horizontal, column).spacing = spacing;
    invalidate();
}

/*!
    Returns the room after \a column.
*/
qreal QGraphicsGridLayout::columnSpacing(int column) const { return gap(Qt::Horizontal, column); }

qreal QGraphicsGridLayout::gap(Qt::Orientation orientation, int index) const {
    const qreal own = lineAt(orientation, index).spacing;
    if (own >= 0)
        return own;
    return orientation == Qt::Horizontal ? horizontalSpacing() : verticalSpacing();
}

/*!
    Sets the stretch factor of \a row to \a stretch.
*/
void QGraphicsGridLayout::setRowStretchFactor(int row, int stretch) {
    if (row < 0)
        return;
    line(Qt::Vertical, row).stretch = stretch;
    invalidate();
}

/*!
    Returns the stretch factor of \a row.
*/
int QGraphicsGridLayout::rowStretchFactor(int row) const { return lineAt(Qt::Vertical, row).stretch; }

/*!
    Sets the stretch factor of \a column to \a stretch.
*/
void QGraphicsGridLayout::setColumnStretchFactor(int column, int stretch) {
    if (column < 0)
        return;
    line(Qt::Horizontal, column).stretch = stretch;
    invalidate();
}

/*!
    Returns the stretch factor of \a column.
*/
int QGraphicsGridLayout::columnStretchFactor(int column) const { return lineAt(Qt::Horizontal, column).stretch; }

/*!
    Sets the smallest height of \a row to \a height.
*/
void QGraphicsGridLayout::setRowMinimumHeight(int row, qreal height) {
    if (row < 0)
        return;
    line(Qt::Vertical, row).minimum = height;
    invalidate();
}

/*!
    Returns the smallest height that was set for \a row, or -1.
*/
qreal QGraphicsGridLayout::rowMinimumHeight(int row) const { return lineAt(Qt::Vertical, row).minimum; }

/*!
    Sets the best height of \a row to \a height.
*/
void QGraphicsGridLayout::setRowPreferredHeight(int row, qreal height) {
    if (row < 0)
        return;
    line(Qt::Vertical, row).preferred = height;
    invalidate();
}

/*!
    Returns the best height that was set for \a row, or -1.
*/
qreal QGraphicsGridLayout::rowPreferredHeight(int row) const { return lineAt(Qt::Vertical, row).preferred; }

/*!
    Sets the largest height of \a row to \a height.
*/
void QGraphicsGridLayout::setRowMaximumHeight(int row, qreal height) {
    if (row < 0)
        return;
    line(Qt::Vertical, row).maximum = height;
    invalidate();
}

/*!
    Returns the largest height that was set for \a row, or -1.
*/
qreal QGraphicsGridLayout::rowMaximumHeight(int row) const { return lineAt(Qt::Vertical, row).maximum; }

/*!
    Makes \a row \a height high: its smallest, best and largest height.
*/
void QGraphicsGridLayout::setRowFixedHeight(int row, qreal height) {
    if (row < 0)
        return;
    Line &l = line(Qt::Vertical, row);
    l.minimum = l.preferred = l.maximum = height;
    invalidate();
}

/*!
    Sets the smallest width of \a column to \a width.
*/
void QGraphicsGridLayout::setColumnMinimumWidth(int column, qreal width) {
    if (column < 0)
        return;
    line(Qt::Horizontal, column).minimum = width;
    invalidate();
}

/*!
    Returns the smallest width that was set for \a column, or -1.
*/
qreal QGraphicsGridLayout::columnMinimumWidth(int column) const { return lineAt(Qt::Horizontal, column).minimum; }

/*!
    Sets the best width of \a column to \a width.
*/
void QGraphicsGridLayout::setColumnPreferredWidth(int column, qreal width) {
    if (column < 0)
        return;
    line(Qt::Horizontal, column).preferred = width;
    invalidate();
}

/*!
    Returns the best width that was set for \a column, or -1.
*/
qreal QGraphicsGridLayout::columnPreferredWidth(int column) const { return lineAt(Qt::Horizontal, column).preferred; }

/*!
    Sets the largest width of \a column to \a width.
*/
void QGraphicsGridLayout::setColumnMaximumWidth(int column, qreal width) {
    if (column < 0)
        return;
    line(Qt::Horizontal, column).maximum = width;
    invalidate();
}

/*!
    Returns the largest width that was set for \a column, or -1.
*/
qreal QGraphicsGridLayout::columnMaximumWidth(int column) const { return lineAt(Qt::Horizontal, column).maximum; }

/*!
    Makes \a column \a width wide: its smallest, best and largest width.
*/
void QGraphicsGridLayout::setColumnFixedWidth(int column, qreal width) {
    if (column < 0)
        return;
    Line &l = line(Qt::Horizontal, column);
    l.minimum = l.preferred = l.maximum = width;
    invalidate();
}

/*!
    Sets the alignment of the items of \a row that have none of their own to \a alignment.
*/
void QGraphicsGridLayout::setRowAlignment(int row, Qt::Alignment alignment) {
    if (row < 0)
        return;
    line(Qt::Vertical, row).alignment = alignment;
    invalidate();
}

/*!
    Returns the alignment of \a row.
*/
Qt::Alignment QGraphicsGridLayout::rowAlignment(int row) const { return lineAt(Qt::Vertical, row).alignment; }

/*!
    Sets the alignment of the items of \a column that have none of their own to \a alignment.
*/
void QGraphicsGridLayout::setColumnAlignment(int column, Qt::Alignment alignment) {
    if (column < 0)
        return;
    line(Qt::Horizontal, column).alignment = alignment;
    invalidate();
}

/*!
    Returns the alignment of \a column.
*/
Qt::Alignment QGraphicsGridLayout::columnAlignment(int column) const {
    return lineAt(Qt::Horizontal, column).alignment;
}

/*!
    Sets the alignment of \a item in its cells to \a alignment.
*/
void QGraphicsGridLayout::setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment) {
    for (Entry &entry : m_entries) {
        if (entry.item == item) {
            entry.alignment = alignment;
            invalidate();
        }
    }
}

/*!
    Returns the alignment of \a item in its cells.
*/
Qt::Alignment QGraphicsGridLayout::alignment(QGraphicsLayoutItem *item) const {
    for (const Entry &entry : m_entries)
        if (entry.item == item)
            return entry.alignment;
    return Qt::Alignment();
}

/*!
    Returns the number of rows.
*/
int QGraphicsGridLayout::rowCount() const { return static_cast<int>(m_rows.size()); }

/*!
    Returns the number of columns.
*/
int QGraphicsGridLayout::columnCount() const { return static_cast<int>(m_columns.size()); }

/*!
    Returns the item that takes the cell at \a row and \a column, or \c nullptr.
*/
QGraphicsLayoutItem *QGraphicsGridLayout::itemAt(int row, int column) const {
    for (const Entry &e : m_entries)
        if (row >= e.row && row < e.row + e.rowSpan && column >= e.column && column < e.column + e.columnSpan)
            return e.item;
    return nullptr;
}

/*!
    \reimp
*/
int QGraphicsGridLayout::count() const { return static_cast<int>(m_entries.size()); }

/*!
    \reimp
*/
QGraphicsLayoutItem *QGraphicsGridLayout::itemAt(int index) const {
    return index >= 0 && index < count() ? m_entries[static_cast<std::size_t>(index)].item : nullptr;
}

/*!
    \reimp
*/
void QGraphicsGridLayout::removeAt(int index) {
    if (index < 0 || index >= count())
        return;
    m_entries[static_cast<std::size_t>(index)].item->setParentLayoutItem(nullptr);
    m_entries.erase(m_entries.begin() + index);
    invalidate();
}

/*!
    Removes \a item from the grid without deleting it.
*/
void QGraphicsGridLayout::removeItem(QGraphicsLayoutItem *item) {
    for (int i = 0; i < count(); ++i) {
        if (m_entries[static_cast<std::size_t>(i)].item == item) {
            removeAt(i);
            return;
        }
    }
}

/*!
    \reimp
*/
void QGraphicsGridLayout::invalidate() { QGraphicsLayout::invalidate(); }

void QGraphicsGridLayout::lineSizes(Qt::Orientation orientation, std::vector<qreal> *minimum,
                                    std::vector<qreal> *preferred, std::vector<qreal> *maximum) const {
    const bool horizontal = orientation == Qt::Horizontal;
    const std::size_t n = (horizontal ? m_columns : m_rows).size();
    minimum->assign(n, 0);
    preferred->assign(n, 0);
    maximum->assign(n, 0);
    std::vector<bool> used(n, false);
    const auto sizesOf = [&](const Entry &e, qreal *mn, qreal *pf, qreal *mx) {
        *mn = along(e.item->effectiveSizeHint(Qt::MinimumSize), orientation);
        *pf = along(e.item->effectiveSizeHint(Qt::PreferredSize), orientation);
        *mx = along(e.item->effectiveSizeHint(Qt::MaximumSize), orientation);
        const QSizePolicy policy = e.item->sizePolicy();
        const QSizePolicy::Policy p = horizontal ? policy.horizontalPolicy() : policy.verticalPolicy();
        if (!(static_cast<int>(p) & QSizePolicy::GrowFlag))
            *mx = *pf;
        if (!(static_cast<int>(p) & QSizePolicy::ShrinkFlag))
            *mn = *pf;
    };
    // The items of one line first.
    for (const Entry &e : m_entries) {
        if (e.item->isEmpty() || (horizontal ? e.columnSpan : e.rowSpan) != 1)
            continue;
        const auto i = static_cast<std::size_t>(horizontal ? e.column : e.row);
        qreal mn = 0, pf = 0, mx = 0;
        sizesOf(e, &mn, &pf, &mx);
        (*minimum)[i] = std::max((*minimum)[i], mn);
        (*preferred)[i] = std::max((*preferred)[i], pf);
        (*maximum)[i] = std::max((*maximum)[i], mx);
        used[i] = true;
    }
    // Then the items across several, which make their lines larger where they need more.
    for (const Entry &e : m_entries) {
        const int span = horizontal ? e.columnSpan : e.rowSpan;
        if (e.item->isEmpty() || span == 1)
            continue;
        const int first = horizontal ? e.column : e.row;
        qreal mn = 0, pf = 0, mx = 0;
        sizesOf(e, &mn, &pf, &mx);
        qreal gaps = 0, haveMin = 0, havePref = 0;
        for (int k = first; k < first + span; ++k) {
            const auto i = static_cast<std::size_t>(k);
            haveMin += (*minimum)[i];
            havePref += (*preferred)[i];
            if (!used[i])
                (*maximum)[i] = QLAYOUTSIZE_MAX;
            used[i] = true;
            if (k + 1 < first + span)
                gaps += gap(orientation, k);
        }
        const qreal moreMin = std::max<qreal>(0, mn - gaps - haveMin) / span;
        const qreal morePref = std::max<qreal>(0, pf - gaps - havePref) / span;
        for (int k = first; k < first + span; ++k) {
            (*minimum)[static_cast<std::size_t>(k)] += moreMin;
            (*preferred)[static_cast<std::size_t>(k)] += morePref;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        const Line l = lineAt(orientation, static_cast<int>(i));
        if (l.minimum >= 0)
            (*minimum)[i] = l.minimum;
        if (l.maximum >= 0)
            (*maximum)[i] = l.maximum;
        else if (!used[i])
            (*maximum)[i] = 0;
        if (l.preferred >= 0)
            (*preferred)[i] = l.preferred;
        (*maximum)[i] = std::max((*maximum)[i], (*minimum)[i]);
        (*preferred)[i] = std::clamp((*preferred)[i], (*minimum)[i], (*maximum)[i]);
    }
}

/*!
    \reimp
*/
QSizeF QGraphicsGridLayout::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    (void)constraint;
    qreal left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    qreal extent[2] = {0, 0};
    for (const Qt::Orientation orientation : {Qt::Horizontal, Qt::Vertical}) {
        std::vector<qreal> mn, pf, mx;
        lineSizes(orientation, &mn, &pf, &mx);
        const std::vector<qreal> &chosen = which == Qt::MinimumSize ? mn : which == Qt::MaximumSize ? mx : pf;
        qreal total = 0;
        for (std::size_t i = 0; i < chosen.size(); ++i) {
            total += chosen[i];
            if (i + 1 < chosen.size())
                total += gap(orientation, static_cast<int>(i));
        }
        extent[orientation == Qt::Horizontal ? 0 : 1] = std::min<qreal>(total, QLAYOUTSIZE_MAX);
    }
    return QSizeF(extent[0] + left + right, extent[1] + top + bottom);
}

/*!
    \reimp
*/
void QGraphicsGridLayout::setGeometry(const QRectF &rect) {
    QGraphicsLayout::setGeometry(rect);
    qreal left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRectF inner = geometry().adjusted(left, top, -right, -bottom);
    std::vector<qreal> positions[2], sizes[2];
    for (const Qt::Orientation orientation : {Qt::Horizontal, Qt::Vertical}) {
        const int o = orientation == Qt::Horizontal ? 0 : 1;
        std::vector<qreal> mn, pf, mx;
        lineSizes(orientation, &mn, &pf, &mx);
        std::vector<int> stretch(mn.size());
        qreal gaps = 0;
        for (std::size_t i = 0; i < mn.size(); ++i) {
            stretch[i] = lineAt(orientation, static_cast<int>(i)).stretch;
            if (i + 1 < mn.size())
                gaps += gap(orientation, static_cast<int>(i));
        }
        sizes[o] = distribute(mn, pf, mx, stretch, along(inner.size(), orientation) - gaps);
        qreal at = o == 0 ? inner.left() : inner.top();
        for (std::size_t i = 0; i < sizes[o].size(); ++i) {
            positions[o].push_back(at);
            at += sizes[o][i] + (i + 1 < sizes[o].size() ? gap(orientation, static_cast<int>(i)) : 0);
        }
    }
    for (const Entry &e : m_entries) {
        if (e.item->isEmpty())
            continue;
        const auto c0 = static_cast<std::size_t>(e.column), c1 = static_cast<std::size_t>(e.column + e.columnSpan - 1);
        const auto r0 = static_cast<std::size_t>(e.row), r1 = static_cast<std::size_t>(e.row + e.rowSpan - 1);
        const QRectF cell(QPointF(positions[0][c0], positions[1][r0]),
                          QPointF(positions[0][c1] + sizes[0][c1], positions[1][r1] + sizes[1][r1]));
        const QSizePolicy policy = e.item->sizePolicy();
        const QSizeF mn = e.item->effectiveSizeHint(Qt::MinimumSize);
        const QSizeF pf = e.item->effectiveSizeHint(Qt::PreferredSize);
        const QSizeF mx = e.item->effectiveSizeHint(Qt::MaximumSize);
        const qreal w = std::max(mn.width(), (static_cast<int>(policy.horizontalPolicy()) & QSizePolicy::GrowFlag)
                                                 ? std::min(cell.width(), mx.width())
                                                 : std::min(cell.width(), pf.width()));
        const qreal h = std::max(mn.height(), (static_cast<int>(policy.verticalPolicy()) & QSizePolicy::GrowFlag)
                                                  ? std::min(cell.height(), mx.height())
                                                  : std::min(cell.height(), pf.height()));
        Qt::Alignment a = e.alignment;
        if (!(a & Qt::AlignHorizontal_Mask))
            a |= lineAt(Qt::Horizontal, e.column).alignment & Qt::AlignHorizontal_Mask;
        if (!(a & Qt::AlignVertical_Mask))
            a |= lineAt(Qt::Vertical, e.row).alignment & Qt::AlignVertical_Mask;
        qreal x = cell.left(), y = cell.top();
        if (a & Qt::AlignHCenter)
            x += (cell.width() - w) / 2;
        else if (a & Qt::AlignRight)
            x += cell.width() - w;
        if (a & Qt::AlignVCenter)
            y += (cell.height() - h) / 2;
        else if (a & Qt::AlignBottom)
            y += cell.height() - h;
        e.item->setGeometry(QRectF(x, y, w, h));
    }
}
