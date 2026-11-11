// MIT License - see LICENSE
#include <QtWidgets/qtreeview.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

/*!
    \class QTreeView
    \brief The QTreeView class provides a default model/view implementation of a tree view.

    A QTreeView shows the items of a model as a tree with columns: each row is an item, an item with children
    has an arrow that opens and closes it, and a header names the columns. With a model that has no hierarchy
    it is a table with rows that are selected as a whole.

    \code
    auto *view = new QTreeView;
    view->setModel(model);
    view->setSortingEnabled(true);
    view->expandToDepth(0);
    connect(view, &QTreeView::activated, this, &Window::open);
    \endcode

    The user opens an item by clicking its arrow, by double-clicking it, or with the Right key; expand(),
    collapse(), expandAll() and their relatives do it from the program. Up, Down, Home, End and the page keys
    move the current item, and Left goes to the parent. Items are selected a row at a time by default; see
    setSelectionBehavior() and setSelectionMode().

    header() is the header: its sections are the columns, which the user resizes by dragging their borders.
    With setSortingEnabled(), a click on a section sorts the model by that column, and a second click turns
    the order round; the model does the sorting, in QAbstractItemModel::sort(), so put a
    QSortFilterProxyModel between the view and a model that cannot sort.

    Items are drawn and edited by the delegate; see QAbstractItemView and QStyledItemDelegate. A model that
    hands out its rows in steps, as QSqlQueryModel does, is asked for more when the view is scrolled to its
    end.

    \differences
    All rows have the same height: that of a line of text of the font of the view, or more if the first item
    wants more; uniformRowHeights() is kept but makes no difference. Columns cannot be hidden, moved or
    spanned, and rows cannot be hidden. The tree is always in the first column. There is no animation, no word
    wrap, no drag and drop, and the branch lines of some styles are not drawn: an item with children has an
    arrow.
    \enddifferences

    \sa QTreeWidget, QAbstractItemView, QHeaderView, QStyledItemDelegate
*/

namespace {
constexpr int kMinimumRowHeight = 20;
}

/*!
    Constructs a tree view with the given \a parent. It selects rows, and its header stretches the last column.
*/
QTreeView::QTreeView(QWidget *parent) : QAbstractItemView(parent), m_header(new QHeaderView(Qt::Horizontal, this)) {
    setSelectionBehavior(SelectRows);
    setFocusPolicy(Qt::StrongFocus);
    m_header->setStretchLastSection(true);
    connect(m_header, &QHeaderView::sectionResized, this, [this](int, int, int) {
        updateGeometries();
        viewport()->update();
    });
    connect(m_header, &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
        if (m_sorting && model() && section >= 0)
            model()->sort(section, order);
    });
    updateGeometries();
}

/*!
    Destroys the view.
*/
QTreeView::~QTreeView() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
}

/*!
    \reimp

    Makes the view show \a model. Nothing is expanded.
*/
void QTreeView::setModel(QAbstractItemModel *model) {
    if (model == this->model())
        return;
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    m_expanded.clear();
    m_rows.clear();
    m_anchor = QPersistentModelIndex();
    QAbstractItemView::setModel(model);
    if (model) {
        const auto relayout = [this] {
            rebuild();
            viewport()->update();
        };
        m_connections = {
            connect(model, &QAbstractItemModel::rowsRemoved, this, [relayout](const QModelIndex &, int, int) { relayout(); }),
            connect(model, &QAbstractItemModel::rowsMoved, this,
                    [relayout](const QModelIndex &, int, int, const QModelIndex &, int) { relayout(); }),
            connect(model, &QAbstractItemModel::layoutChanged, this,
                    [relayout](const QList<QPersistentModelIndex> &, QAbstractItemModel::LayoutChangeHint) { relayout(); }),
            connect(model, &QAbstractItemModel::columnsInserted, this, [relayout](const QModelIndex &, int, int) { relayout(); }),
            connect(model, &QAbstractItemModel::columnsRemoved, this, [relayout](const QModelIndex &, int, int) { relayout(); }),
            connect(model, &QAbstractItemModel::headerDataChanged, this, [this](Qt::Orientation orientation, int, int) {
                if (orientation == Qt::Horizontal)
                    syncHeader();
            }),
        };
    }
    rebuild();
}

/*!
    \reimp

    Shows the children of \a index as the top items of the view.
*/
void QTreeView::setRootIndex(const QModelIndex &index) {
    QAbstractItemView::setRootIndex(index);
    rebuild();
}

/*!
    \fn QHeaderView *QTreeView::header() const

    Returns the header of the view, whose sections are the columns.

    \sa setHeaderHidden(), setSortingEnabled()
*/

/*!
    \fn int QTreeView::indentation() const

    Returns how far, in pixels, the items of each level are moved to the right of their parents. The default
    is 20.

    \sa setIndentation()
*/

/*!
    Sets the indentation of each level of the tree to \a i pixels.

    \sa indentation(), resetIndentation()
*/
void QTreeView::setIndentation(int i) {
    m_indentation = std::max(0, i);
    viewport()->update();
    updateEditorGeometries();
}

/*!
    Gives the view its default indentation again.

    \sa setIndentation()
*/
void QTreeView::resetIndentation() { setIndentation(20); }

/*!
    \fn bool QTreeView::rootIsDecorated() const

    Returns whether the top items have arrows for opening and closing them, and the room for them. The default
    is \c true.

    \sa setRootIsDecorated()
*/

/*!
    Sets whether the top items have arrows for opening and closing them to \a show. Turn it off for a model
    that is a plain list or table, so that its first column starts at the left edge.

    \sa rootIsDecorated()
*/
void QTreeView::setRootIsDecorated(bool show) {
    m_rootDecorated = show;
    viewport()->update();
    updateEditorGeometries();
}

/*!
    \fn bool QTreeView::uniformRowHeights() const

    Returns the value that setUniformRowHeights() set. All rows of the view have the same height either way.
*/

/*!
    Keeps \a uniform for uniformRowHeights(). In Qt this promises that all rows have the same height, so that
    the view can lay them out faster; in Nt they always have.
*/
void QTreeView::setUniformRowHeights(bool uniform) { m_uniform = uniform; }

/*!
    \fn bool QTreeView::itemsExpandable() const

    Returns whether the user can open and close items. The default is \c true.

    \sa setItemsExpandable()
*/

/*!
    Sets whether the user can open and close items to \a enable. The program still can.

    \sa itemsExpandable()
*/
void QTreeView::setItemsExpandable(bool enable) { m_expandable = enable; }

/*!
    \fn bool QTreeView::expandsOnDoubleClick() const

    Returns whether a double click on an item with children opens or closes it. The default is \c true.

    \sa setExpandsOnDoubleClick()
*/

/*!
    Sets whether a double click on an item with children opens or closes it to \a enable. A double click that
    starts an editor does not.

    \sa expandsOnDoubleClick()
*/
void QTreeView::setExpandsOnDoubleClick(bool enable) { m_expandOnDoubleClick = enable; }

/*!
    Returns whether the header is hidden. The default is \c false.

    \sa setHeaderHidden()
*/
bool QTreeView::isHeaderHidden() const { return m_header->isHidden(); }

/*!
    Hides the header if \a hide is \c true, and shows it otherwise.

    \sa isHeaderHidden(), header()
*/
void QTreeView::setHeaderHidden(bool hide) {
    m_header->setVisible(!hide);
    updateGeometries();
}

/*!
    \fn bool QTreeView::isSortingEnabled() const

    Returns whether the user can sort the items by clicking on the header. The default is \c false.

    \sa setSortingEnabled()
*/

/*!
    Sets whether the user can sort the items to \a enable. When it is on, the header shows which column the
    items are sorted by and in which order, a click on a section sorts by that column, and a click on the
    section that is sorted by turns the order round. If the header says that a column is sorted by already,
    the model is sorted by it at once.

    The model does the sorting; one that does not implement QAbstractItemModel::sort() stays as it is.

    \sa isSortingEnabled(), sortByColumn()
*/
void QTreeView::setSortingEnabled(bool enable) {
    m_sorting = enable;
    m_header->setSortIndicatorShown(enable);
    m_header->setSectionsClickable(enable);
    if (enable && model() && m_header->sortIndicatorSection() >= 0)
        model()->sort(m_header->sortIndicatorSection(), m_header->sortIndicatorOrder());
}

/*!
    Sorts the model by the column \a column in the order \a order, and has the header show it. A \a column of
    -1 only takes the sort indicator away.

    \sa setSortingEnabled(), QAbstractItemModel::sort()
*/
void QTreeView::sortByColumn(int column, Qt::SortOrder order) {
    const bool same = m_header->sortIndicatorSection() == column && m_header->sortIndicatorOrder() == order;
    m_header->setSortIndicator(column, order);
    // The header tells the model itself when the indicator changes and sorting is on.
    if (model() && column >= 0 && (!m_sorting || same))
        model()->sort(column, order);
}

/*!
    Returns the position, on the viewport, of the left edge of the column \a column; it is negative for a
    column that is scrolled out to the left.

    \sa columnAt(), columnWidth()
*/
int QTreeView::columnViewportPosition(int column) const {
    return m_header->sectionPosition(column) - horizontalScrollBar()->value();
}

/*!
    Returns the width of the column \a column.

    \sa setColumnWidth()
*/
int QTreeView::columnWidth(int column) const { return m_header->sectionSize(column); }

/*!
    Sets the width of the column \a column to \a width.

    \sa columnWidth(), resizeColumnToContents()
*/
void QTreeView::setColumnWidth(int column, int width) { m_header->resizeSection(column, width); }

/*!
    Returns the column at the position \a x of the viewport, or -1 if there is none.

    \sa columnViewportPosition()
*/
int QTreeView::columnAt(int x) const { return m_header->logicalIndexAt(x + horizontalScrollBar()->value()); }

/*!
    Returns \c true if the item \a index is expanded, so that its children show; otherwise returns \c false.

    \sa expand(), setExpanded()
*/
bool QTreeView::isExpanded(const QModelIndex &index) const {
    if (!index.isValid())
        return false;
    return isOpen(index.sibling(index.row(), 0));
}

/*!
    Expands the item \a index if \a expand is \c true, and collapses it otherwise.

    \sa expand(), collapse(), isExpanded()
*/
void QTreeView::setExpanded(const QModelIndex &index, bool expand) {
    if (expand)
        this->expand(index);
    else
        collapse(index);
}

/*!
    Expands the item \a index, so that its children show, and emits expanded(). Nothing happens if it is
    expanded already or has no children.

    \sa collapse(), expandAll(), isExpanded()
*/
void QTreeView::expand(const QModelIndex &index) {
    if (!model() || !index.isValid() || index.model() != model())
        return;
    const QModelIndex first = index.sibling(index.row(), 0);
    if (isOpen(first) || !model()->hasChildren(first))
        return;
    if (model()->canFetchMore(first))
        model()->fetchMore(first);
    m_expanded.append(QPersistentModelIndex(first));
    rebuild();
    viewport()->update();
    emit expanded(first);
}

/*!
    Collapses the item \a index, so that its children no longer show, and emits collapsed(). If the current
    item was one of them, the item becomes the current item.

    \sa expand(), collapseAll()
*/
void QTreeView::collapse(const QModelIndex &index) {
    if (!model() || !index.isValid() || index.model() != model())
        return;
    const QModelIndex first = index.sibling(index.row(), 0);
    if (!close(first))
        return;
    // The current item must not be one that does not show.
    for (QModelIndex i = currentIndex().parent(); i.isValid(); i = i.parent()) {
        if (i == first) {
            setCurrentIndex(first);
            break;
        }
    }
    rebuild();
    viewport()->update();
    emit collapsed(first);
}

/*!
    Expands all items that have children.

    \sa collapseAll(), expandToDepth()
*/
void QTreeView::expandAll() { expandToDepth(-1); }

/*!
    Collapses all items.

    \sa expandAll()
*/
void QTreeView::collapseAll() {
    if (m_expanded.isEmpty())
        return;
    const QList<QPersistentModelIndex> were = m_expanded;
    m_expanded.clear();
    // The current item goes up to its top-level ancestor.
    QModelIndex current = currentIndex();
    while (current.isValid() && current.parent().isValid() && current.parent() != rootIndex())
        current = current.parent();
    if (current.isValid() && current != currentIndex())
        setCurrentIndex(current);
    rebuild();
    viewport()->update();
    for (const QPersistentModelIndex &index : were) {
        if (index.isValid())
            emit collapsed(index);
    }
}

/*!
    Expands the items down to the level \a depth and collapses nothing: with 0 only the top items are
    expanded, with 1 their children too, and with -1 every item.

    \sa expandAll(), expandRecursively()
*/
void QTreeView::expandToDepth(int depth) { expandRecursively(rootIndex(), depth < 0 ? -1 : depth + 1); }

/*!
    Expands the item \a index and its descendants down to \a depth levels below it: with 0 only the item, with
    -1 all of them. An invalid \a index stands for the top of the model.

    \sa expandAll(), expandToDepth()
*/
void QTreeView::expandRecursively(const QModelIndex &index, int depth) {
    if (!model())
        return;
    QList<QModelIndex> added;
    QSet<QModelIndex> open;
    for (const QPersistentModelIndex &i : m_expanded)
        open.insert(QModelIndex(i));
    // Level by level, without recursion: a model can be deep.
    struct Node {
        QModelIndex index;
        int depth;
    };
    std::vector<Node> pending{{index.isValid() ? index.sibling(index.row(), 0) : QModelIndex(index), 0}};
    while (!pending.empty()) {
        const Node node = pending.back();
        pending.pop_back();
        const bool isRoot = !node.index.isValid() || node.index == rootIndex();
        if (!isRoot || index.isValid()) {
            if (node.index.isValid() && model()->hasChildren(node.index)) {
                if (model()->canFetchMore(node.index))
                    model()->fetchMore(node.index);
                if (!open.contains(node.index)) {
                    open.insert(node.index);
                    m_expanded.append(QPersistentModelIndex(node.index));
                    added.append(node.index);
                }
            }
        }
        if (depth >= 0 && node.depth >= depth)
            continue;
        const int rows = model()->rowCount(node.index);
        for (int r = 0; r < rows; ++r) {
            const QModelIndex child = model()->index(r, 0, node.index);
            if (model()->hasChildren(child))
                pending.push_back({child, node.depth + 1});
        }
    }
    if (added.isEmpty())
        return;
    rebuild();
    viewport()->update();
    for (const QModelIndex &i : added)
        emit expanded(i);
}

/*!
    Makes the column \a column as wide as the widest of its items that are laid out, and its header, need.

    \sa setColumnWidth()
*/
void QTreeView::resizeColumnToContents(int column) {
    if (!model() || column < 0 || column >= m_header->count())
        return;
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    const QString label = model()->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
    int width = QFontMetrics(m_header->font()).horizontalAdvance(label) + 24;
    for (const Row &row : m_rows) {
        const QModelIndex index = QModelIndex(row.index).sibling(row.index.row(), column);
        int w = itemDelegateForIndex(index)->sizeHint(option, index).width();
        if (column == 0)
            w += indentOf(row.level);
        width = std::max(width, w);
    }
    m_header->resizeSection(column, width);
}

// The height of every row.
int QTreeView::itemHeight() const {
    int height = std::max(kMinimumRowHeight, QFontMetrics(font()).height() + 6);
    if (!m_rows.empty() && m_rows.front().index.isValid())
        height = std::max(height, sizeHintForIndex(m_rows.front().index).height());
    return height;
}

/*!
    Returns the height of the row of the item \a index, or 0 if \a index is not valid. All rows have the same
    height.
*/
int QTreeView::rowHeight(const QModelIndex &index) const { return index.isValid() ? itemHeight() : 0; }

// How far the items of a level start right of the left edge of the first column.
int QTreeView::indentOf(int level) const { return (level + (m_rootDecorated ? 1 : 0)) * m_indentation; }

int QTreeView::contentWidth() const { return m_header->length(); }

// The position in m_rows of the row of `index`, or -1 if it is not laid out.
int QTreeView::rowOf(const QModelIndex &index) const {
    if (!index.isValid())
        return -1;
    const QModelIndex first = index.sibling(index.row(), 0);
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].index == first)
            return static_cast<int>(i);
    }
    return -1;
}

int QTreeView::rowAtY(int y) const {
    const int absolute = y + verticalScrollBar()->value();
    if (absolute < 0)
        return -1;
    const int row = absolute / itemHeight();
    return row < static_cast<int>(m_rows.size()) ? row : -1;
}

bool QTreeView::isOpen(const QModelIndex &first) const {
    for (const QPersistentModelIndex &i : m_expanded) {
        if (i == first)
            return true;
    }
    return false;
}

bool QTreeView::close(const QModelIndex &first) {
    for (qsizetype i = 0; i < m_expanded.size(); ++i) {
        if (m_expanded.at(i) == first) {
            m_expanded.removeAt(i);
            return true;
        }
    }
    return false;
}

void QTreeView::appendRows(const QModelIndex &parent, int level, const QSet<QModelIndex> &open) {
    QAbstractItemModel *m = model();
    const int rows = m->rowCount(parent);
    for (int r = 0; r < rows; ++r) {
        const QModelIndex index = m->index(r, 0, parent);
        Row row;
        row.index = QPersistentModelIndex(index);
        row.level = level;
        row.hasChildren = m->hasChildren(index);
        row.expanded = row.hasChildren && open.contains(index);
        m_rows.push_back(row);
        if (row.expanded)
            appendRows(index, level + 1, open);
    }
}

// Lays the items out again: the rows that show, from the model and from what is expanded.
void QTreeView::rebuild() {
    m_rows.clear();
    if (model()) {
        // Items that are gone are no longer expanded. The indexes hold still while the rows are laid out, so
        // a set of them is good for that long.
        QSet<QModelIndex> open;
        for (qsizetype i = m_expanded.size() - 1; i >= 0; --i) {
            if (m_expanded.at(i).isValid())
                open.insert(QModelIndex(m_expanded.at(i)));
            else
                m_expanded.removeAt(i);
        }
        appendRows(rootIndex(), 0, open);
    }
    syncHeader();
    updateGeometries();
    updateEditorGeometries();
}

// Gives the header the columns of the model and their names.
void QTreeView::syncHeader() {
    const int columns = model() ? model()->columnCount(rootIndex()) : 0;
    if (m_header->count() != columns)
        m_header->setCount(columns);
    QStringList labels;
    for (int c = 0; c < columns; ++c) {
        const QVariant label = model()->headerData(c, Qt::Horizontal, Qt::DisplayRole);
        labels.append(label.isValid() ? label.toString() : QString::number(c + 1));
    }
    m_header->setLabels(labels);
    m_header->update();
}

void QTreeView::updateGeometries() {
    if (m_inLayout) {
        m_layoutAgain = true;
        return;
    }
    m_inLayout = true;
    QScrollBar *hbar = horizontalScrollBar(), *vbar = verticalScrollBar();
    for (int pass = 0; pass < 6; ++pass) {
        m_layoutAgain = false;
        const int headerHeight = m_header->isHidden() ? 0 : m_header->sizeHint().height();
        setViewportMargins(0, headerHeight, 0, 0);
        const QRect inner = contentsRect();
        const QSize view = viewport()->size();
        m_header->setGeometry(inner.left(), inner.top(), view.width(), headerHeight);
        const int height = itemHeight();
        hbar->setPageStep(std::max(1, view.width()));
        hbar->setSingleStep(20);
        hbar->setRange(0, std::max(0, contentWidth() - view.width()));
        vbar->setPageStep(std::max(1, view.height()));
        vbar->setSingleStep(height);
        vbar->setRange(0, std::max(0, static_cast<int>(m_rows.size()) * height - view.height()));
        m_header->setOffset(hbar->value());
        if (!m_layoutAgain)
            break;
    }
    m_inLayout = false;
}

/*!
    \reimp

    Returns the rectangle of the item \a index on the viewport: its cell, which in the first column starts
    after the indentation of the item. It is empty if the item is not laid out, because a parent of it is
    collapsed.
*/
QRect QTreeView::visualRect(const QModelIndex &index) const {
    const int row = rowOf(index);
    if (row < 0 || index.column() < 0 || index.column() >= m_header->count())
        return QRect();
    const int height = itemHeight();
    int x = columnViewportPosition(index.column());
    int width = m_header->sectionSize(index.column());
    if (index.column() == 0) {
        const int indent = std::min(indentOf(m_rows[static_cast<std::size_t>(row)].level), width);
        x += indent;
        width -= indent;
    }
    return QRect(x, row * height - verticalScrollBar()->value(), width, height);
}

/*!
    \reimp

    Scrolls so that the item \a index shows, where \a hint says, after expanding the items above it that are
    collapsed.
*/
void QTreeView::scrollTo(const QModelIndex &index, ScrollHint hint) {
    if (!model() || !index.isValid() || index.model() != model())
        return;
    bool opened = false;
    for (QModelIndex parent = index.parent(); parent.isValid() && parent != rootIndex(); parent = parent.parent()) {
        const QModelIndex p = parent.sibling(parent.row(), 0);
        if (!isOpen(p)) {
            m_expanded.append(QPersistentModelIndex(p));
            opened = true;
        }
    }
    if (opened)
        rebuild();
    const int row = rowOf(index);
    if (row < 0)
        return;
    const int height = itemHeight();
    const int top = row * height;
    const int view = viewport()->height();
    QScrollBar *vbar = verticalScrollBar();
    switch (hint) {
    case PositionAtTop:
        vbar->setValue(top);
        break;
    case PositionAtBottom:
        vbar->setValue(top + height - view);
        break;
    case PositionAtCenter:
        vbar->setValue(top - (view - height) / 2);
        break;
    case EnsureVisible:
        if (top < vbar->value())
            vbar->setValue(top);
        else if (top + height > vbar->value() + view)
            vbar->setValue(top + height - view);
        break;
    }
    // Sideways only as far as it takes to show the start of the cell.
    QScrollBar *hbar = horizontalScrollBar();
    const int left = m_header->sectionPosition(index.column());
    const int right = left + m_header->sectionSize(index.column());
    if (left < hbar->value() || right - left > viewport()->width())
        hbar->setValue(left);
    else if (right > hbar->value() + viewport()->width())
        hbar->setValue(right - viewport()->width());
    viewport()->update();
}

/*!
    \reimp

    Returns the item at \a point of the viewport: the one of the row and the column there, also where the
    indentation and the arrow of the row are.
*/
QModelIndex QTreeView::indexAt(const QPoint &point) const {
    if (!model())
        return QModelIndex();
    const int row = rowAtY(point.y());
    const int column = columnAt(point.x());
    if (row < 0 || column < 0)
        return QModelIndex();
    const QModelIndex first = m_rows[static_cast<std::size_t>(row)].index;
    return first.sibling(first.row(), column);
}

/*!
    Returns the item that is laid out above the item \a index, in the same column, or an invalid index if
    there is none.

    \sa indexBelow()
*/
QModelIndex QTreeView::indexAbove(const QModelIndex &index) const {
    const int row = rowOf(index);
    if (row <= 0)
        return QModelIndex();
    const QModelIndex first = m_rows[static_cast<std::size_t>(row - 1)].index;
    return first.sibling(first.row(), index.column());
}

/*!
    Returns the item that is laid out below the item \a index, in the same column, or an invalid index if
    there is none.

    \sa indexAbove()
*/
QModelIndex QTreeView::indexBelow(const QModelIndex &index) const {
    const int row = rowOf(index);
    if (row < 0 || row + 1 >= static_cast<int>(m_rows.size()))
        return QModelIndex();
    const QModelIndex first = m_rows[static_cast<std::size_t>(row) + 1].index;
    return first.sibling(first.row(), index.column());
}

/*!
    \reimp

    Collapses everything and lays the items of the model out anew.
*/
void QTreeView::reset() {
    m_expanded.clear();
    m_anchor = QPersistentModelIndex();
    m_pressed = QPersistentModelIndex();
    QAbstractItemView::reset();
    rebuild();
}

/*!
    \reimp

    Selects all the items that are laid out, if the selection mode allows several: the items under a collapsed
    item are not selected.
*/
void QTreeView::selectAll() {
    if (!selectionModel() || m_rows.empty())
        return;
    const SelectionMode mode = selectionMode();
    if (mode == NoSelection || mode == SingleSelection)
        return;
    selectRange(0, static_cast<int>(m_rows.size()) - 1);
}

QItemSelectionModel::SelectionFlags QTreeView::behaviorFlags() const {
    switch (selectionBehavior()) {
    case SelectRows:
        return QItemSelectionModel::Rows;
    case SelectColumns:
        return QItemSelectionModel::Columns;
    case SelectItems:
        break;
    }
    return QItemSelectionModel::NoUpdate;
}

// Makes the rows from `fromRow` to `toRow` of the layout the selection.
void QTreeView::selectRange(int fromRow, int toRow) {
    if (!selectionModel() || !model())
        return;
    const int first = std::max(0, std::min(fromRow, toRow));
    const int last = std::min(static_cast<int>(m_rows.size()) - 1, std::max(fromRow, toRow));
    const int lastColumn = std::max(0, m_header->count() - 1);
    QItemSelection selection;
    for (int r = first; r <= last; ++r) {
        const QModelIndex left = m_rows[static_cast<std::size_t>(r)].index;
        selection.select(left, left.sibling(left.row(), lastColumn));
    }
    selectionModel()->select(selection, QItemSelectionModel::SelectionFlags(QItemSelectionModel::ClearAndSelect));
}

// Makes `index` the current item and changes the selection the way the selection mode and the modifier keys
// say: for a click, or for a move with the keyboard.
void QTreeView::selectWith(const QModelIndex &index, Qt::KeyboardModifiers modifiers, bool keyboard) {
    QItemSelectionModel *selection = selectionModel();
    if (!selection || !index.isValid())
        return;
    const bool shift = modifiers & Qt::ShiftModifier;
    const bool control = modifiers & Qt::ControlModifier;
    const SelectionMode mode = selectionMode();
    const QItemSelectionModel::SelectionFlags rows = behaviorFlags();
    switch (mode) {
    case NoSelection:
        selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
    case SingleSelection:
        selection->setCurrentIndex(index, QItemSelectionModel::SelectionFlags(QItemSelectionModel::ClearAndSelect) | rows);
        m_anchor = QPersistentModelIndex(index);
        return;
    case MultiSelection:
        // The keyboard moves the current item; a click, or Space, toggles the item.
        selection->setCurrentIndex(index, keyboard ? QItemSelectionModel::SelectionFlags(QItemSelectionModel::NoUpdate)
                                                   : QItemSelectionModel::SelectionFlags(QItemSelectionModel::Toggle) | rows);
        m_anchor = QPersistentModelIndex(index);
        return;
    case ExtendedSelection:
    case ContiguousSelection:
        if (shift && m_anchor.isValid() && rowOf(m_anchor) >= 0) {
            selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            selectRange(rowOf(m_anchor), rowOf(index));
            return;
        }
        if (control && mode == ExtendedSelection) {
            selection->setCurrentIndex(index, keyboard ? QItemSelectionModel::SelectionFlags(QItemSelectionModel::NoUpdate)
                                                       : QItemSelectionModel::SelectionFlags(QItemSelectionModel::Toggle) | rows);
            m_anchor = QPersistentModelIndex(index);
            return;
        }
        selection->setCurrentIndex(index, QItemSelectionModel::SelectionFlags(QItemSelectionModel::ClearAndSelect) | rows);
        m_anchor = QPersistentModelIndex(index);
        return;
    }
}

void QTreeView::toggle(int row) {
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return;
    const Row &r = m_rows[static_cast<std::size_t>(row)];
    if (!r.hasChildren)
        return;
    const QModelIndex index = r.index;
    if (r.expanded)
        collapse(index);
    else
        expand(index);
}

/*!
    \reimp

    Returns a size for a view of a few columns and rows.
*/
QSize QTreeView::sizeHint() const { return QSize(256, 192); }

/*!
    Draws the arrow of the item \a index with \a painter, in \a rect: the room that the indentation leaves in
    front of the item. The arrow points right for an item that can be expanded and down for one that is.
    Nothing is drawn for an item without children. A subclass draws its own branches here.
*/
void QTreeView::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const {
    const int row = rowOf(index);
    if (row < 0 || rect.isEmpty())
        return;
    const Row &r = m_rows[static_cast<std::size_t>(row)];
    if (!r.hasChildren || m_indentation <= 0)
        return;
    QStyleOption option;
    option.initFrom(this);
    // The slot of the arrow is the last indentation step before the item.
    option.rect = QRect(rect.right() + 1 - m_indentation, rect.top(), m_indentation, rect.height());
    if (selectionModel() && selectionModel()->isSelected(index) && selectionBehavior() == SelectRows)
        option.palette.setColor(QPalette::ButtonText, option.palette.color(QPalette::HighlightedText));
    else
        option.palette.setColor(QPalette::ButtonText, option.palette.color(QPalette::Text));
    style()->drawPrimitive(r.expanded ? QStyle::PE_IndicatorArrowDown : QStyle::PE_IndicatorArrowRight, &option, painter,
                           this);
}

/*!
    Draws the row of the item \a index with \a painter: its background, its arrow, and its item in each
    column, through the delegates. \a options has the palette, the font and the state that all items share,
    and in its rectangle the whole row. A subclass that draws rows in its own way reimplements this.
*/
void QTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &options, const QModelIndex &index) const {
    const int row = rowOf(index);
    if (row < 0)
        return;
    const Row &r = m_rows[static_cast<std::size_t>(row)];
    const QRect rowRect = options.rect;
    const QPalette::ColorGroup group = !isEnabled() ? QPalette::Disabled
                                       : (options.state & QStyle::State_Active) ? QPalette::Active
                                                                                : QPalette::Inactive;
    const bool alternate = alternatingRowColors() && (row % 2) == 1;
    if (alternate)
        painter->fillRect(rowRect, options.palette.color(group, QPalette::AlternateBase));
    const QItemSelectionModel *selection = selectionModel();
    const QModelIndex current = currentIndex();
    const bool rowSelected =
        selection && selectionBehavior() == SelectRows && selection->isSelected(QModelIndex(r.index));
    const int columns = m_header->count();
    for (int c = 0; c < columns; ++c) {
        const int width = m_header->sectionSize(c);
        if (width <= 0)
            continue;
        const int x = columnViewportPosition(c);
        if (x + width <= 0 || x >= viewport()->width())
            continue;
        const QModelIndex cell = QModelIndex(r.index).sibling(r.index.row(), c);
        QStyleOptionViewItem option = options;
        option.rect = QRect(x, rowRect.top(), width, rowRect.height());
        if (alternate)
            option.features |= QStyleOptionViewItem::Alternate;
        if (selection && selection->isSelected(cell))
            option.state |= QStyle::State_Selected;
        if (!(model()->flags(cell) & Qt::ItemIsEnabled))
            option.state &= ~QStyle::State(QStyle::State_Enabled);
        if (c == 0) {
            const int indent = std::min(indentOf(r.level), width);
            const QRect branch(x, rowRect.top(), indent, rowRect.height());
            // A selected row is highlighted from its left edge, under the arrow too.
            if (rowSelected)
                painter->fillRect(branch, options.palette.color(group, QPalette::Highlight));
            painter->save();
            painter->setClipRect(option.rect);
            drawBranches(painter, branch, cell);
            painter->restore();
            option.rect.setLeft(x + indent);
        }
        if (cell == current && hasFocus())
            option.state |= QStyle::State_HasFocus;
        itemDelegateForIndex(cell)->paint(painter, option, cell);
    }
    // The current item has a frame while the view has the focus: around the row, if rows are selected.
    if (hasFocus() && current.isValid() && rowOf(current) == row) {
        QStyleOption focus;
        focus.initFrom(this);
        focus.rect = selectionBehavior() == SelectRows
                         ? QRect(-horizontalScrollBar()->value(), rowRect.top(), std::max(contentWidth(), 1), rowRect.height())
                         : visualRect(current);
        if (!focus.rect.isEmpty())
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, this);
    }
}

/*!
    \reimp
*/
void QTreeView::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    const QRect area = event->rect();
    QStyleOptionViewItem options;
    initViewItemOption(&options);
    const QPalette::ColorGroup group = !isEnabled() ? QPalette::Disabled
                                       : (options.state & QStyle::State_Active) ? QPalette::Active
                                                                                : QPalette::Inactive;
    painter.fillRect(area, options.palette.color(group, QPalette::Base));
    if (!model() || m_rows.empty())
        return;
    const int height = itemHeight();
    const int offset = verticalScrollBar()->value();
    const int first = std::max(0, (area.top() + offset) / height);
    const int last = std::min(static_cast<int>(m_rows.size()) - 1, (area.bottom() + offset) / height);
    for (int row = first; row <= last; ++row) {
        options.rect = QRect(0, row * height - offset, viewport()->width(), height);
        drawRow(&painter, options, m_rows[static_cast<std::size_t>(row)].index);
    }
}

/*!
    \reimp

    A press on the arrow of an item opens or closes the item; a press on an item makes it the current item
    and selects it the way the selection mode and the Shift and Control keys say.
*/
void QTreeView::mousePressEvent(QMouseEvent *event) {
    m_pressed = QPersistentModelIndex();
    if (!model()) {
        event->ignore();
        return;
    }
    const int row = rowAtY(event->pos().y());
    const QModelIndex index = indexAt(event->pos());
    if (row < 0 || !index.isValid()) {
        // A click below the items takes the selection away, as in a file manager.
        if (event->button() == Qt::LeftButton && selectionModel() && selectionMode() != NoSelection &&
            !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            selectionModel()->clearSelection();
        return;
    }
    const Row &r = m_rows[static_cast<std::size_t>(row)];
    if (event->button() == Qt::LeftButton && index.column() == 0 && r.hasChildren && m_expandable) {
        const int x = event->pos().x() - columnViewportPosition(0);
        const int end = indentOf(r.level);
        if (x >= end - m_indentation && x < end) {
            toggle(row);
            return;
        }
    }
    if (!(model()->flags(index) & Qt::ItemIsEnabled))
        return;
    m_pressed = QPersistentModelIndex(index);
    m_pressedWasSelected = selectionModel() && selectionModel()->isSelected(index) && currentIndex() == index;
    emit pressed(index);
    if (event->button() == Qt::LeftButton || !(selectionModel() && selectionModel()->isSelected(index)))
        selectWith(index, event->modifiers(), false);
}

/*!
    \reimp

    A release on the item that was pressed is a click: clicked() is emitted, the delegate gets the event, for
    a check box, and with the \c SelectedClicked trigger an item that was selected already is edited.
*/
void QTreeView::mouseReleaseEvent(QMouseEvent *event) {
    const QModelIndex pressedIndex = m_pressed;
    m_pressed = QPersistentModelIndex();
    if (!model() || !pressedIndex.isValid() || indexAt(event->pos()) != pressedIndex)
        return;
    if (event->button() != Qt::LeftButton)
        return;
    emit clicked(pressedIndex);
    const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
    edit(pressedIndex, m_pressedWasSelected && plain ? SelectedClicked : NoEditTriggers, event);
}

/*!
    \reimp

    A double click on an item emits doubleClicked() and edits the item if \c DoubleClicked is one of the edit
    triggers and the item can be edited; otherwise it emits activated() and, if expandsOnDoubleClick() is on,
    opens or closes the item.
*/
void QTreeView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!model() || event->button() != Qt::LeftButton)
        return;
    const QModelIndex index = indexAt(event->pos());
    const int row = rowAtY(event->pos().y());
    if (!index.isValid() || row < 0 || !(model()->flags(index) & Qt::ItemIsEnabled))
        return;
    const QPersistentModelIndex keep(index);
    emit doubleClicked(index);
    if (!keep.isValid())
        return;
    if (edit(keep, DoubleClicked, event))
        return;
    emit activated(keep);
    if (keep.isValid() && m_expandOnDoubleClick && m_expandable)
        toggle(rowOf(keep));
}

/*!
    \reimp

    Returns the item that \a cursorAction leads to from the current item, in the rows as they are laid out.
    Left closes an open item or goes to the parent; Right opens an item or goes to its first child.
    \a modifiers is not used.
*/
QModelIndex QTreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) {
    static_cast<void>(modifiers);
    if (!model() || m_rows.empty())
        return QModelIndex();
    const QModelIndex current = currentIndex();
    const int count = static_cast<int>(m_rows.size());
    const int row = rowOf(current);
    const int column = current.isValid() ? current.column() : 0;
    const auto at = [this, column](int r) {
        const QModelIndex first = m_rows[static_cast<std::size_t>(r)].index;
        return first.sibling(first.row(), column);
    };
    if (row < 0)
        return at(0);
    const int page = std::max(1, viewport()->height() / itemHeight() - 1);
    switch (cursorAction) {
    case MoveUp:
        return at(std::max(0, row - 1));
    case MoveDown:
        return at(std::min(count - 1, row + 1));
    case MoveHome:
        return at(0);
    case MoveEnd:
        return at(count - 1);
    case MovePageUp:
        return at(std::max(0, row - page));
    case MovePageDown:
        return at(std::min(count - 1, row + page));
    case MoveLeft: {
        const Row &r = m_rows[static_cast<std::size_t>(row)];
        if (r.expanded && m_expandable) {
            collapse(r.index);
            return current;
        }
        const QModelIndex parent = QModelIndex(r.index).parent();
        if (parent.isValid() && parent != rootIndex())
            return parent.sibling(parent.row(), column);
        return current;
    }
    case MoveRight: {
        const Row &r = m_rows[static_cast<std::size_t>(row)];
        if (r.hasChildren && !r.expanded && m_expandable) {
            expand(r.index);
            return current;
        }
        if (r.expanded && row + 1 < count)
            return at(row + 1);
        return current;
    }
    case MoveNext:
        if (column + 1 < m_header->count())
            return current.sibling(current.row(), column + 1);
        if (row + 1 < count)
            return QModelIndex(m_rows[static_cast<std::size_t>(row) + 1].index);
        return current;
    case MovePrevious:
        if (column > 0)
            return current.sibling(current.row(), column - 1);
        if (row > 0) {
            const QModelIndex first = m_rows[static_cast<std::size_t>(row - 1)].index;
            return first.sibling(first.row(), std::max(0, m_header->count() - 1));
        }
        return current;
    }
    return current;
}

/*!
    \reimp

    The arrow, Home, End and page keys move the current item; Plus, Minus and the asterisk open and close
    items; Return and Enter activate the current item; F2 edits it, as typing does with the \c AnyKeyPressed
    trigger; Space toggles a check box or the selection; Control+A selects everything.
*/
void QTreeView::keyPressEvent(QKeyEvent *event) {
    if (!model() || m_rows.empty()) {
        QAbstractItemView::keyPressEvent(event);
        return;
    }
    const QModelIndex current = currentIndex();
    CursorAction action = MoveDown;
    bool moves = true;
    switch (event->key()) {
    case Qt::Key_Up:
        action = MoveUp;
        break;
    case Qt::Key_Down:
        action = MoveDown;
        break;
    case Qt::Key_Left:
        action = MoveLeft;
        break;
    case Qt::Key_Right:
        action = MoveRight;
        break;
    case Qt::Key_Home:
        action = MoveHome;
        break;
    case Qt::Key_End:
        action = MoveEnd;
        break;
    case Qt::Key_PageUp:
        action = MovePageUp;
        break;
    case Qt::Key_PageDown:
        action = MovePageDown;
        break;
    default:
        moves = false;
        break;
    }
    if (moves) {
        const QModelIndex target = moveCursor(action, event->modifiers());
        if (target.isValid() && target != current)
            selectWith(target, event->modifiers(), true);
        else if (target.isValid() && !current.isValid())
            selectWith(target, Qt::NoModifier, true);
        event->accept();
        return;
    }
    if (event->text() == QStringLiteral("*") && current.isValid()) {
        expandRecursively(current);
        event->accept();
        return;
    }
    switch (event->key()) {
    case Qt::Key_Plus:
        expand(current);
        break;
    case Qt::Key_Minus:
        collapse(current);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (current.isValid() && (model()->flags(current) & Qt::ItemIsEnabled))
            emit activated(current);
        break;
    case Qt::Key_F2:
        if (!edit(current, EditKeyPressed, event))
            event->ignore();
        return;
    case Qt::Key_Space:
        if (current.isValid() && !edit(current, NoEditTriggers, event) && selectionModel() &&
            selectionMode() != NoSelection) {
            selectionModel()->select(current, QItemSelectionModel::SelectionFlags(selectionMode() == SingleSelection
                                                                                      ? QItemSelectionModel::ClearAndSelect
                                                                                      : QItemSelectionModel::Toggle) |
                                                  behaviorFlags());
        }
        break;
    case Qt::Key_A:
        if (event->modifiers() & Qt::ControlModifier) {
            selectAll();
            break;
        }
        [[fallthrough]];
    default: {
        // Typing starts an editor, which gets the character that was typed.
        const QString text = event->text();
        const bool printable = !text.isEmpty() && text.at(0).isPrint() &&
                               !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier));
        if (printable && current.isValid() && edit(current, AnyKeyPressed, event) && state() == EditingState) {
            if (QWidget *editor = QApplication::focusWidget(); editor && editor != this)
                QCoreApplication::sendEvent(editor, event);
            event->accept();
            return;
        }
        QAbstractItemView::keyPressEvent(event);
        return;
    }
    }
    event->accept();
}

/*!
    \reimp
*/
void QTreeView::resizeEvent(QResizeEvent *event) {
    static_cast<void>(event);
    updateGeometries();
    updateEditorGeometries();
}

/*!
    \reimp
*/
void QTreeView::showEvent(QShowEvent *event) {
    static_cast<void>(event);
    updateGeometries();
}

/*!
    \reimp

    Moves the header with the columns, and asks a model that hands out its rows in steps for more when the
    end is reached; \a dx and \a dy are not used.
*/
void QTreeView::scrollContentsBy(int dx, int dy) {
    static_cast<void>(dx);
    static_cast<void>(dy);
    m_header->setOffset(horizontalScrollBar()->value());
    viewport()->update();
    updateEditorGeometries();
    QScrollBar *vbar = verticalScrollBar();
    if (model() && vbar->value() >= vbar->maximum() && model()->canFetchMore(rootIndex()))
        model()->fetchMore(rootIndex());
}

/*!
    \reimp

    Shows the tool tip of the item under the mouse, which the model returns for Qt::ToolTipRole; other
    \a event are handled as in the base class.
*/
bool QTreeView::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::ToolTip && model()) {
        auto *help = static_cast<QHelpEvent *>(event);
        const QModelIndex index = indexAt(help->pos());
        const QString tip = index.isValid() ? index.data(Qt::ToolTipRole).toString() : QString();
        if (!tip.isEmpty()) {
            QToolTip::showText(help->globalPos(), tip, viewport(), visualRect(index));
            event->accept();
            return true;
        }
    }
    return QAbstractItemView::viewportEvent(event);
}

/*!
    \reimp

    Lays the items out again after the rows \a start to \a end were inserted under \a parent.
*/
void QTreeView::rowsInserted(const QModelIndex &parent, int start, int end) {
    rebuild();
    QAbstractItemView::rowsInserted(parent, start, end);
}

/*!
    \reimp

    Moves the current item off the rows \a start to \a end of \a parent before they are removed: to the row
    after them, the row before them, or their parent.
*/
void QTreeView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) {
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
    const QModelIndex current = currentIndex();
    for (QModelIndex i = current; i.isValid(); i = i.parent()) {
        if (i.parent() == parent && i.row() >= start && i.row() <= end) {
            QModelIndex next;
            if (end + 1 < model()->rowCount(parent))
                next = model()->index(end + 1, current.column(), parent);
            else if (start > 0)
                next = model()->index(start - 1, current.column(), parent);
            else if (parent.isValid() && parent != rootIndex())
                next = parent;
            if (selectionModel())
                selectionModel()->setCurrentIndex(next, QItemSelectionModel::NoUpdate);
            break;
        }
    }
}

/*!
    \reimp

    Handles the change of the current item from \a previous to \a current as the base class does.
*/
void QTreeView::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    QAbstractItemView::currentChanged(current, previous);
}

#include "moc_qtreeview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
