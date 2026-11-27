// MIT License - see LICENSE
#include <QtWidgets/qgridlayout.h>

#include "qlayoutcalc_p.h"

#include <QtCore/qdebug.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

using QtPrivate::Cell;
using QtPrivate::calculate;

/*!
    \class QGridLayout
    \brief The QGridLayout class lays out widgets in a grid of rows and columns.

    Each widget goes in a cell, or in a block of cells that it spans. A column is as wide as the widest widget
    in it needs, and a row as high as its highest widget; the space that is left over is shared out as in QBoxLayout:
    rows and columns with a stretch factor, or that hold widgets that want to expand, get it. Spacing is put between
    the rows and between the columns, and the margins around the grid.

    \code
    QGridLayout *grid = new QGridLayout(window);
    grid->addWidget(new QLabel("Name"), 0, 0);
    grid->addWidget(new QLineEdit, 0, 1);
    grid->addWidget(new QLabel("Notes"), 1, 0, Qt::AlignTop);
    grid->addWidget(new QLineEdit, 1, 1);
    grid->addWidget(okButton, 2, 0, 1, 2); // spans both columns
    grid->setColumnStretch(1, 1);
    \endcode

    A form in a grid: labels in the first column, fields that span two columns, and two buttons in the last row.
    The second and third columns have the same stretch factor.

    \image widgets/qgridlayout.png QGridLayout in the light color scheme

    \differences
    Empty rows and columns have no spacing next to them.
    \enddifferences

    \sa QBoxLayout, QLayout
*/

/*!
    Constructs a grid layout for \a parent, with no rows and columns. Rows and columns appear when widgets are added.
*/
QGridLayout::QGridLayout(QWidget *parent) : QLayout(parent) {}

/*!
    Destroys the layout. The widgets that are in it stay.
*/
QGridLayout::~QGridLayout() {
    // A layout that is deleted leaves the layout it is in.
    if (auto *parentLayout = dynamic_cast<QLayout *>(parent()))
        parentLayout->removeItem(this);
    for (Entry &e : m_items)
        if (e.owned)
            delete e.item;
}

void QGridLayout::insert(QLayoutItem *item, bool owned, int row, int column, int rowSpan, int columnSpan) {
    row = std::max(0, row);
    column = std::max(0, column);
    rowSpan = std::max(1, rowSpan);
    columnSpan = std::max(1, columnSpan);
    m_items.push_back(Entry{item, owned, row, column, rowSpan, columnSpan});
    m_rows = std::max(m_rows, row + rowSpan);
    m_columns = std::max(m_columns, column + columnSpan);
    invalidate();
}

/*!
    Adds \a widget in the cell at \a row and \a column, with the given \a alignment: without it, the widget fills the
    cell as far as its size limits allow. The layout makes \a widget a child of the widget it manages.

    \sa addLayout(), addItem()
*/
void QGridLayout::addWidget(QWidget *widget, int row, int column, Qt::Alignment alignment) {
    addWidget(widget, row, column, 1, 1, alignment);
}

/*!
    \overload

    Adds \a widget so that it covers \a rowSpan rows and \a columnSpan columns, from the cell at \a row and
    \a column.
*/
void QGridLayout::addWidget(QWidget *widget, int row, int column, int rowSpan, int columnSpan, Qt::Alignment alignment) {
    if (!widget)
        return;
    addChildWidget(widget);
    auto *item = new QWidgetItem(widget);
    item->setAlignment(alignment);
    insert(item, true, row, column, rowSpan, columnSpan);
}

/*!
    Adds \a layout in the cell at \a row and \a column, with the given \a alignment. It becomes a child of this layout.

    \sa addWidget()
*/
void QGridLayout::addLayout(QLayout *layout, int row, int column, Qt::Alignment alignment) {
    addLayout(layout, row, column, 1, 1, alignment);
}

/*!
    \overload

    Adds \a layout so that it covers \a rowSpan rows and \a columnSpan columns, from the cell at \a row and
    \a column.
*/
void QGridLayout::addLayout(QLayout *layout, int row, int column, int rowSpan, int columnSpan, Qt::Alignment alignment) {
    if (!layout)
        return;
    if (layout->parent() && layout->parent() != this) {
        qWarning("QGridLayout::addLayout: the layout already has a parent");
        return;
    }
    layout->QObject::setParent(this);
    layout->reparentChildWidgets();
    layout->setAlignment(alignment);
    insert(layout, false, row, column, rowSpan, columnSpan);
}

/*!
    Adds \a item so that it covers \a rowSpan rows and \a columnSpan columns, from the cell at \a row and
    \a column, with the given \a alignment. The layout takes ownership of the item.
*/
void QGridLayout::addItem(QLayoutItem *item, int row, int column, int rowSpan, int columnSpan, Qt::Alignment alignment) {
    if (!item)
        return;
    item->setAlignment(alignment);
    insert(item, item->layout() == nullptr, row, column, rowSpan, columnSpan);
}

/*!
    \overload

    Adds \a item in the first free cell of the last row, or in a new row if that one is full.
*/
void QGridLayout::addItem(QLayoutItem *item) {
    if (m_defaultCount > 0) {
        // setDefaultPositioning(): the next cell of rows (or columns) of m_defaultCount cells.
        const int n = static_cast<int>(m_items.size());
        if (m_defaultOrientation == Qt::Horizontal)
            addItem(item, n / m_defaultCount, n % m_defaultCount);
        else
            addItem(item, n % m_defaultCount, n / m_defaultCount);
        return;
    }
    // The next cell, going right, and down at the end of the row; the width is that of the grid so far, or one column.
    int row = std::max(0, m_rows - 1);
    int column = 0;
    for (const Entry &e : m_items)
        if (e.row <= row && e.row + e.rowSpan > row)
            column = std::max(column, e.column + e.columnSpan);
    if (column >= std::max(1, m_columns)) {
        ++row;
        column = 0;
    }
    addItem(item, row, column);
}

/*!
    Returns the item that covers the cell at \a row and \a column, or \c nullptr if there is none.

    \sa getItemPosition()
*/
QLayoutItem *QGridLayout::itemAtPosition(int row, int column) const {
    for (const Entry &e : m_items)
        if (row >= e.row && row < e.row + e.rowSpan && column >= e.column && column < e.column + e.columnSpan)
            return e.item;
    return nullptr;
}

/*!
    Finds the position of the item at \a index and stores its \a row, \a column, \a rowSpan and \a columnSpan. Nothing
    is stored for an index that is not an item.

    \sa itemAtPosition()
*/
void QGridLayout::getItemPosition(int index, int *row, int *column, int *rowSpan, int *columnSpan) const {
    if (index < 0 || index >= count())
        return;
    const Entry &e = m_items[static_cast<std::size_t>(index)];
    if (row)
        *row = e.row;
    if (column)
        *column = e.column;
    if (rowSpan)
        *rowSpan = e.rowSpan;
    if (columnSpan)
        *columnSpan = e.columnSpan;
}

/*!
    \fn int QGridLayout::rowCount() const

    Returns the number of rows: one more than the last row that has an item.
*/

/*!
    \fn int QGridLayout::columnCount() const

    Returns the number of columns: one more than the last column that has an item.
*/

/*!
    Sets the stretch factor of \a row to \a stretch. Rows with a stretch factor share the space that is left over in
    proportion to the factors.

    \sa rowStretch(), setColumnStretch()
*/
void QGridLayout::setRowStretch(int row, int stretch) {
    if (row < 0)
        return;
    m_rowStretch[row] = std::max(0, stretch);
    m_rows = std::max(m_rows, row + 1);
    invalidate();
}

/*!
    Returns the stretch factor of \a row, 0 by default.

    \sa setRowStretch()
*/
int QGridLayout::rowStretch(int row) const {
    const auto it = m_rowStretch.find(row);
    return it == m_rowStretch.end() ? 0 : it->second;
}

/*!
    Sets the stretch factor of \a column to \a stretch.

    \sa columnStretch(), setRowStretch()
*/
void QGridLayout::setColumnStretch(int column, int stretch) {
    if (column < 0)
        return;
    m_columnStretch[column] = std::max(0, stretch);
    m_columns = std::max(m_columns, column + 1);
    invalidate();
}

/*!
    Returns the stretch factor of \a column, 0 by default.

    \sa setColumnStretch()
*/
int QGridLayout::columnStretch(int column) const {
    const auto it = m_columnStretch.find(column);
    return it == m_columnStretch.end() ? 0 : it->second;
}

/*!
    Makes \a row at least \a height pixels high, even if it is empty.

    \sa rowMinimumHeight()
*/
void QGridLayout::setRowMinimumHeight(int row, int height) {
    if (row < 0)
        return;
    m_rowMinimum[row] = std::max(0, height);
    m_rows = std::max(m_rows, row + 1);
    invalidate();
}

/*!
    Returns the smallest height of \a row that was set, 0 by default.

    \sa setRowMinimumHeight()
*/
int QGridLayout::rowMinimumHeight(int row) const {
    const auto it = m_rowMinimum.find(row);
    return it == m_rowMinimum.end() ? 0 : it->second;
}

/*!
    Makes \a column at least \a width pixels wide, even if it is empty.

    \sa columnMinimumWidth()
*/
void QGridLayout::setColumnMinimumWidth(int column, int width) {
    if (column < 0)
        return;
    m_columnMinimum[column] = std::max(0, width);
    m_columns = std::max(m_columns, column + 1);
    invalidate();
}

/*!
    Returns the smallest width of \a column that was set, 0 by default.

    \sa setColumnMinimumWidth()
*/
int QGridLayout::columnMinimumWidth(int column) const {
    const auto it = m_columnMinimum.find(column);
    return it == m_columnMinimum.end() ? 0 : it->second;
}

/*!
    Returns the space between the columns: the one that setHorizontalSpacing() set, or else spacing().

    \sa setHorizontalSpacing(), verticalSpacing()
*/
int QGridLayout::horizontalSpacing() const { return m_horizontalSpacing >= 0 ? m_horizontalSpacing : spacing(); }

/*!
    Sets the space between the columns to \a spacing.

    \sa horizontalSpacing()
*/
void QGridLayout::setHorizontalSpacing(int spacing) {
    m_horizontalSpacing = spacing;
    invalidate();
}

/*!
    Returns the space between the rows: the one that setVerticalSpacing() set, or else spacing().

    \sa setVerticalSpacing(), horizontalSpacing()
*/
int QGridLayout::verticalSpacing() const { return m_verticalSpacing >= 0 ? m_verticalSpacing : spacing(); }

/*!
    Sets the space between the rows to \a spacing.

    \sa verticalSpacing()
*/
void QGridLayout::setVerticalSpacing(int spacing) {
    m_verticalSpacing = spacing;
    invalidate();
}

/*!
    \reimp
*/
QLayoutItem *QGridLayout::itemAt(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].item : nullptr;
}

/*!
    \reimp
*/
QLayoutItem *QGridLayout::takeAt(int index) {
    if (index < 0 || index >= count())
        return nullptr;
    QLayoutItem *item = m_items[static_cast<std::size_t>(index)].item;
    m_items.erase(m_items.begin() + index);
    return item;
}

int QGridLayout::gap(bool columns) const { return columns ? horizontalSpacing() : verticalSpacing(); }

// The room of `span` cells from `start`, which have their sizes, with the spacing between them.
int QGridLayout::spanSize(const std::vector<Cell> &cells, int start, int span) const {
    int size = 0, spaced = 0;
    for (int i = start; i < start + span && i < static_cast<int>(cells.size()); ++i) {
        const Cell &c = cells[static_cast<std::size_t>(i)];
        size += c.size;
        spaced += c.empty ? 0 : 1;
    }
    return size + std::max(0, spaced - 1) * horizontalSpacing();
}

/*!
    \reimp

    Returns \c true if the height of any item of the grid depends on its width.
*/
bool QGridLayout::hasHeightForWidth() const {
    return std::any_of(m_items.begin(), m_items.end(),
                       [](const Entry &e) { return !e.item->isEmpty() && e.item->hasHeightForWidth(); });
}

int QGridLayout::heightAtWidth(int w, bool minimum) const {
    if (!hasHeightForWidth())
        return -1;
    const QMargins m = contentsMargins();
    std::vector<Cell> columns = axisCells(true);
    calculate(columns, std::max(0, w - m.left() - m.right()), horizontalSpacing());
    int height = 0, used = 0;
    for (const Cell &c : axisCells(false, &columns)) {
        height += minimum ? c.min : c.hint;
        used += c.empty ? 0 : 1;
    }
    return height + std::max(0, used - 1) * verticalSpacing() + m.top() + m.bottom();
}

/*!
    \reimp

    Returns the height that the grid would like to have if it is \a w wide, or -1 if no item's height depends on its
    width.
*/
int QGridLayout::heightForWidth(int w) const { return heightAtWidth(w, false); }

/*!
    \reimp

    Returns the least height of the grid if it is \a w wide, or -1 if no item's height depends on its width.
*/
int QGridLayout::minimumHeightForWidth(int w) const { return heightAtWidth(w, true); }

// The cells of the columns, or of the rows: how much each needs and wants, and whether it wants more.
std::vector<Cell> QGridLayout::axisCells(bool columns, const std::vector<Cell> *widths) const {
    // The sizes of an item along the axis, its height that for the width of its columns if the one depends on the other.
    const auto sizes = [&](const Entry &e) {
        std::pair<QSize, QSize> s(e.item->sizeHint(), e.item->minimumSize());
        if (!columns && widths && e.item->hasHeightForWidth()) {
            const int width = QtPrivate::widthIn(e.item, spanSize(*widths, e.column, e.columnSpan));
            s.second.setHeight(QtPrivate::heightAt(e.item, width, true));
            s.first.setHeight(std::max(s.second.height(), QtPrivate::heightAt(e.item, width, false)));
        }
        return s;
    };
    const int n = columns ? m_columns : m_rows;
    std::vector<Cell> cells(static_cast<std::size_t>(n));
    std::vector<bool> used(static_cast<std::size_t>(n), false), hasItems(static_cast<std::size_t>(n), false);
    for (int i = 0; i < n; ++i) {
        Cell &c = cells[static_cast<std::size_t>(i)];
        c.item = nullptr;
        c.stretch = columns ? columnStretch(i) : rowStretch(i);
        c.min = c.hint = columns ? columnMinimumWidth(i) : rowMinimumHeight(i);
        c.max = QLAYOUTSIZE_MAX; // the smallest maximum of the items that are in it
        c.expansive = c.stretch > 0;
        c.empty = true;
        used[static_cast<std::size_t>(i)] = c.min > 0; // a row that only has a stretch factor has no spacing next to it
    }
    const int spacing = gap(columns);
    // Items that cover one cell first.
    for (const Entry &e : m_items) {
        if (e.item->isEmpty() && !e.item->spacerItem())
            continue;
        const int start = columns ? e.column : e.row;
        const int span = columns ? e.columnSpan : e.rowSpan;
        const auto [hint, mn] = sizes(e);
        const QSize mx = e.item->maximumSize();
        const bool expands = e.item->expandingDirections().testFlag(columns ? Qt::Horizontal : Qt::Vertical);
        for (int i = start; i < start + span; ++i) {
            Cell &c = cells[static_cast<std::size_t>(i)];
            used[static_cast<std::size_t>(i)] = true;
            hasItems[static_cast<std::size_t>(i)] = true;
            c.expansive = c.expansive || expands;
            if (span == 1) {
                c.min = std::max(c.min, columns ? mn.width() : mn.height());
                c.hint = std::max(c.hint, columns ? hint.width() : hint.height());
                // As in Qt, an alignment lifts the limit: the item keeps its size, and the cell may grow around it.
                const Qt::Alignment aligned = e.item->alignment();
                const bool free = columns ? bool(aligned & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter))
                                          : bool(aligned & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter));
                c.max = std::min(c.max, free ? QLAYOUTSIZE_MAX : (columns ? mx.width() : mx.height()));
            }
        }
    }
    // Items that cover several cells: if they need more than the cells have, every cell gets a share of what is missing.
    for (const Entry &e : m_items) {
        const int span = columns ? e.columnSpan : e.rowSpan;
        if (span == 1 || (e.item->isEmpty() && !e.item->spacerItem()))
            continue;
        const int start = columns ? e.column : e.row;
        const auto [hint, mn] = sizes(e);
        for (int pass = 0; pass < 2; ++pass) {
            const int needed = pass == 0 ? (columns ? hint.width() : hint.height()) : (columns ? mn.width() : mn.height());
            int have = (span - 1) * spacing;
            for (int i = start; i < start + span; ++i) {
                const Cell &c = cells[static_cast<std::size_t>(i)];
                have += pass == 0 ? c.hint : c.min;
            }
            if (needed > have) {
                const int more = needed - have;
                for (int i = start; i < start + span; ++i) {
                    Cell &c = cells[static_cast<std::size_t>(i)];
                    const int share = more / span + (i - start < more % span ? 1 : 0);
                    (pass == 0 ? c.hint : c.min) += share;
                }
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        Cell &c = cells[static_cast<std::size_t>(i)];
        c.empty = !used[static_cast<std::size_t>(i)];
        c.hint = std::max(c.hint, c.min);
        // A cell cannot grow past the smallest maximum of its items; one that only has a stretch factor has no limit.
        if (!hasItems[static_cast<std::size_t>(i)])
            c.max = c.stretch > 0 ? QLAYOUTSIZE_MAX : c.hint;
        c.max = std::max(c.max, c.hint);
    }
    return cells;
}

/*!
    \reimp
*/
QSize QGridLayout::sizeHint() const {
    int width = 0, height = 0, columnsUsed = 0, rowsUsed = 0;
    for (const Cell &c : axisCells(true)) {
        width += c.hint;
        columnsUsed += c.empty ? 0 : 1;
    }
    for (const Cell &c : axisCells(false)) {
        height += c.hint;
        rowsUsed += c.empty ? 0 : 1;
    }
    width += std::max(0, columnsUsed - 1) * horizontalSpacing();
    height += std::max(0, rowsUsed - 1) * verticalSpacing();
    const QMargins m = contentsMargins();
    return QSize(width + m.left() + m.right(), height + m.top() + m.bottom());
}

/*!
    \reimp
*/
QSize QGridLayout::minimumSize() const {
    int width = 0, height = 0, columnsUsed = 0, rowsUsed = 0;
    for (const Cell &c : axisCells(true)) {
        width += c.min;
        columnsUsed += c.empty ? 0 : 1;
    }
    for (const Cell &c : axisCells(false)) {
        height += c.min;
        rowsUsed += c.empty ? 0 : 1;
    }
    width += std::max(0, columnsUsed - 1) * horizontalSpacing();
    height += std::max(0, rowsUsed - 1) * verticalSpacing();
    const QMargins m = contentsMargins();
    return QSize(width + m.left() + m.right(), height + m.top() + m.bottom());
}

/*!
    \reimp
*/
Qt::Orientations QGridLayout::expandingDirections() const {
    Qt::Orientations directions;
    for (const Cell &c : axisCells(true))
        if (c.expansive)
            directions |= Qt::Horizontal;
    for (const Cell &c : axisCells(false))
        if (c.expansive)
            directions |= Qt::Vertical;
    return directions;
}

/*!
    \reimp
*/
void QGridLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    if (m_items.empty())
        return;
    const QRect r = rect.marginsRemoved(contentsMargins());
    std::vector<Cell> columns = axisCells(true);
    const int horizontalGap = horizontalSpacing(), verticalGap = verticalSpacing();
    const int extraX = calculate(columns, r.width(), horizontalGap);
    std::vector<Cell> rows = axisCells(false, &columns); // after the columns, whose widths heights may depend on
    const int extraY = calculate(rows, r.height(), verticalGap);

    // Where each column and row starts; the space that nobody could take goes between them, as in a box layout.
    auto starts = [](const std::vector<Cell> &cells, int first, int gapSize, int extra) {
        std::vector<int> out;
        int pos = first + extra;
        for (const Cell &c : cells) {
            out.push_back(pos);
            pos += c.size + (c.empty ? 0 : gapSize + extra);
        }
        return out;
    };
    std::vector<int> x = starts(columns, r.left(), horizontalGap, extraX);
    std::vector<int> y = starts(rows, r.top(), verticalGap, extraY);
    // An origin at the right or the bottom mirrors the columns or the rows.
    const bool mirrorX = m_origin == Qt::TopRightCorner || m_origin == Qt::BottomRightCorner;
    const bool mirrorY = m_origin == Qt::BottomLeftCorner || m_origin == Qt::BottomRightCorner;
    if (mirrorX)
        for (std::size_t i = 0; i < x.size(); ++i)
            x[i] = r.left() + r.right() + 1 - x[i] - columns[i].size;
    if (mirrorY)
        for (std::size_t i = 0; i < y.size(); ++i)
            y[i] = r.top() + r.bottom() + 1 - y[i] - rows[i].size;
    m_columnRects.clear();
    m_rowRects.clear();
    for (std::size_t i = 0; i < x.size(); ++i)
        m_columnRects.emplace_back(x[i], 0, columns[i].size, 0);
    for (std::size_t i = 0; i < y.size(); ++i)
        m_rowRects.emplace_back(0, y[i], 0, rows[i].size);

    for (const Entry &e : m_items) {
        const int right = e.column + e.columnSpan - 1, bottom = e.row + e.rowSpan - 1;
        const int x0 = std::min(x[static_cast<std::size_t>(e.column)], x[static_cast<std::size_t>(right)]);
        const int x1 =
            std::max(x[static_cast<std::size_t>(e.column)] + columns[static_cast<std::size_t>(e.column)].size,
                     x[static_cast<std::size_t>(right)] + columns[static_cast<std::size_t>(right)].size);
        const int y0 = std::min(y[static_cast<std::size_t>(e.row)], y[static_cast<std::size_t>(bottom)]);
        const int y1 = std::max(y[static_cast<std::size_t>(e.row)] + rows[static_cast<std::size_t>(e.row)].size,
                                y[static_cast<std::size_t>(bottom)] + rows[static_cast<std::size_t>(bottom)].size);
        const QRect cell(x0, y0, x1 - x0, y1 - y0);
        const QSize mn = e.item->minimumSize(), mx = e.item->maximumSize();
        QSize hint = e.item->sizeHint();
        if (e.item->hasHeightForWidth())
            hint.setHeight(QtPrivate::heightAt(e.item, QtPrivate::widthIn(e.item, cell.width()), false));
        const Qt::Alignment a = e.item->alignment();
        QSize size(std::clamp(cell.width(), mn.width(), std::max(mn.width(), mx.width())),
                   std::clamp(cell.height(), mn.height(), std::max(mn.height(), mx.height())));
        int left = cell.left(), top = cell.top();
        // An alignment shrinks the item to its hint; without one it fills the cell, and is centered if it cannot.
        if (a & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter))
            size.setWidth(std::min(size.width(), std::max(mn.width(), hint.width())));
        if (a & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter))
            size.setHeight(std::min(size.height(), std::max(mn.height(), hint.height())));
        const int roomX = cell.width() - size.width(), roomY = cell.height() - size.height();
        if (a & Qt::AlignRight)
            left += roomX;
        else if (a & Qt::AlignHCenter || !(a & Qt::AlignLeft))
            left += roomX / 2;
        if (a & Qt::AlignBottom)
            top += roomY;
        else if (a & Qt::AlignVCenter || !(a & Qt::AlignTop))
            top += roomY / 2;
        e.item->setGeometry(QRect(QPoint(left, top), size));
    }
}

/*!
    \since 6.0

    Returns the rectangle of the cell at \a row and \a column as the layout last placed it, or an invalid rectangle
    if the layout has not been laid out or there is no such cell.
*/
QRect QGridLayout::cellRect(int row, int column) const {
    if (row < 0 || column < 0 || static_cast<std::size_t>(row) >= m_rowRects.size() ||
        static_cast<std::size_t>(column) >= m_columnRects.size())
        return QRect();
    const QRect &c = m_columnRects[static_cast<std::size_t>(column)];
    const QRect &r = m_rowRects[static_cast<std::size_t>(row)];
    return QRect(c.x(), r.y(), c.width(), r.height());
}

/*!
    \fn Qt::Corner QGridLayout::originCorner() const
    \since 6.0

    Returns the corner where row 0 and column 0 are, Qt::TopLeftCorner by default.
*/

/*!
    \since 6.0

    Puts row 0 and column 0 at \a corner: a right corner puts the columns from right to left, a bottom corner the
    rows from bottom to top.
*/
void QGridLayout::setOriginCorner(Qt::Corner corner) {
    m_origin = corner;
    invalidate();
}

/*!
    \since 6.0

    Makes addWidget() and addItem() without a position fill rows of \a n cells for Qt::Horizontal \a orient, or
    columns of \a n cells for Qt::Vertical. A non-positive \a n goes back to filling the rows of the grid so far.
*/
void QGridLayout::setDefaultPositioning(int n, Qt::Orientation orient) {
    m_defaultCount = std::max(0, n);
    m_defaultOrientation = orient;
}

QLayoutItem *QGridLayout::replaceAt(int index, QLayoutItem *item) {
    if (index < 0 || index >= count() || !item)
        return nullptr;
    Entry &e = m_items[static_cast<std::size_t>(index)];
    QLayoutItem *old = e.item;
    e.item = item;
    e.owned = true;
    return old;
}

/*!
    \fn void QGridLayout::addWidget(QWidget *widget)
    \since 6.0
    \overload

    Adds \a widget in the next cell, as addItem() places it.
*/

#include "moc_qgridlayout.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
