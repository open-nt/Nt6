// MIT License - see LICENSE
#include <QtWidgets/qtableview.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

/*!
    \class QTableView
    \brief The QTableView class provides a default model/view implementation of a table view.

    A QTableView shows the items of a model as a table: the rows and columns of the model are the rows and
    columns of the table, with a header above the columns and one left of the rows. It is the model-based
    counterpart of QTableWidget.

    \code
    auto *model = new NutTableModel(this); // a QAbstractTableModel
    auto *view = new QTableView;
    view->setModel(model);
    view->setSortingEnabled(true);
    view->resizeColumnsToContents();
    view->hideColumn(1);
    \endcode

    horizontalHeader() and verticalHeader() are the headers; their sections are the columns and the rows, which
    the user resizes by dragging their edges. setColumnWidth(), setRowHeight(), resizeColumnToContents() and
    resizeRowToContents() size them from the program, and hideColumn() and hideRow() take them out of view.

    A click on a cell makes it current and selects it, dragging selects a block of cells, and Shift and Control
    extend the selection as in the other views. A click on a row header selects the row, a click on a column
    header the column, and the corner button selects everything. The arrow keys move between the cells that
    show, the page keys a page at a time, Home and End to the first and last column, and with Control to the
    first and last cell.

    With setSortingEnabled(), a click on a column header sorts the model by that column instead of selecting
    it; a second click turns the order round. The model does the sorting in QAbstractItemModel::sort(), so put a
    QSortFilterProxyModel between the view and a model that cannot sort.

    Items are drawn and edited by the delegate; see QAbstractItemView and QStyledItemDelegate.

    \differences
    Cells cannot be spanned (no \c setSpan()), and rows and columns cannot be moved. The headers cannot be
    replaced (no \c setHorizontalHeader()). wordWrap() asks the delegate to wrap the text, but
    QStyledItemDelegate draws one line. There is no drag and drop and no automatic scrolling while the mouse
    selects.
    \enddifferences

    \sa QTableWidget, QTreeView, QListView, QHeaderView, QAbstractItemView
*/

namespace {

// The button in the corner between the headers, which selects everything.
class CornerButton : public QWidget {
public:
    explicit CornerButton(QTableView *table) : QWidget(table), m_table(table) {}

protected:
    void paintEvent(QPaintEvent *event) override {
        static_cast<void>(event);
        QPainter painter(this);
        QStyleOptionHeader option;
        option.initFrom(this);
        option.lastSection = true;
        if (m_down)
            option.state |= QStyle::State_Sunken;
        style()->drawControl(QStyle::CE_Header, &option, &painter, this);
    }
    // A press is taken even when the button is disabled: it must not reach the table as a press on a cell.
    void mousePressEvent(QMouseEvent *event) override {
        event->accept();
        if (event->button() != Qt::LeftButton || !m_table->isCornerButtonEnabled())
            return;
        m_down = true;
        update();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (!m_down)
            return;
        m_down = false;
        update();
        if (rect().contains(event->pos()))
            m_table->selectAll();
    }

private:
    QTableView *m_table;
    bool m_down = false;
};

// The color of the lines between the cells: a little darker than the cells, or lighter if they are dark, as
// in QTableWidget.
QColor gridColor(const QColor &base) {
    const bool dark = base.red() + base.green() + base.blue() < 3 * 128;
    return dark ? base.lighter(170) : base.darker(125);
}

// The section of `header` at `position` along its contents, from the start of the first section: -1 if none.
int sectionAt(const QHeaderView *header, int position) {
    if (position < 0)
        return -1;
    int start = 0;
    for (int i = 0; i < header->count(); ++i) {
        const int end = start + header->sectionSize(i);
        if (position < end && end > start)
            return i;
        start = end;
    }
    return -1;
}

} // namespace

/*!
    Constructs a table view with the given \a parent. Its headers have clickable sections.
*/
QTableView::QTableView(QWidget *parent)
    : QAbstractItemView(parent), m_hheader(new QHeaderView(Qt::Horizontal, this)),
      m_vheader(new QHeaderView(Qt::Vertical, this)), m_corner(new CornerButton(this)) {
    setFocusPolicy(Qt::StrongFocus);
    m_hheader->setSectionsClickable(true);
    m_vheader->setSectionsClickable(true);
    for (QHeaderView *header : {m_hheader, m_vheader}) {
        connect(header, &QHeaderView::sectionResized, this, [this](int, int, int) {
            updateGeometries();
            viewport()->update();
        });
    }
    connect(m_hheader, &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
        if (m_sorting && model() && section >= 0)
            model()->sort(section, order);
    });
    connect(m_hheader, &QHeaderView::sectionClicked, this, [this](int column) {
        if (!m_sorting)
            selectColumn(column);
    });
    connect(m_vheader, &QHeaderView::sectionClicked, this, [this](int row) { selectRow(row); });
    QTableView::updateGeometries(); // not virtually: the object is still being made
}

/*!
    Destroys the view.
*/
QTableView::~QTableView() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
}

/*!
    \reimp

    Makes the view show \a model. Every row and column shows, in the default size.
*/
void QTableView::setModel(QAbstractItemModel *model) {
    if (model == this->model())
        return;
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    m_anchor = QPersistentModelIndex();
    QAbstractItemView::setModel(model);
    if (model) {
        const auto removed = [this](QHeaderView *header, const QModelIndex &parent, int first, int last) {
            if (parent != rootIndex())
                return;
            header->removeSections(first, last - first + 1);
            syncLabels(header->orientation());
            updateGeometries();
        };
        const auto again = [this] {
            syncHeaders();
            updateGeometries();
        };
        m_connections = {
            connect(model, &QAbstractItemModel::rowsRemoved, this,
                    [this, removed](const QModelIndex &parent, int first, int last) {
                        removed(m_vheader, parent, first, last);
                    }),
            connect(model, &QAbstractItemModel::columnsRemoved, this,
                    [this, removed](const QModelIndex &parent, int first, int last) {
                        removed(m_hheader, parent, first, last);
                    }),
            connect(model, &QAbstractItemModel::columnsInserted, this,
                    [this](const QModelIndex &parent, int first, int last) {
                        if (parent != rootIndex())
                            return;
                        m_hheader->insertSections(first, last - first + 1);
                        syncLabels(Qt::Horizontal);
                        updateGeometries();
                    }),
            connect(model, &QAbstractItemModel::rowsMoved, this,
                    [again](const QModelIndex &, int, int, const QModelIndex &, int) { again(); }),
            connect(model, &QAbstractItemModel::columnsMoved, this,
                    [again](const QModelIndex &, int, int, const QModelIndex &, int) { again(); }),
            connect(model, &QAbstractItemModel::layoutChanged, this,
                    [again](const QList<QPersistentModelIndex> &, QAbstractItemModel::LayoutChangeHint) { again(); }),
            connect(model, &QAbstractItemModel::headerDataChanged, this,
                    [this](Qt::Orientation orientation, int, int) { syncLabels(orientation); }),
        };
    }
    syncHeaders();
    updateGeometries();
}

/*!
    \reimp

    Shows the children of \a index as the rows and columns of the table.
*/
void QTableView::setRootIndex(const QModelIndex &index) {
    QAbstractItemView::setRootIndex(index);
    m_hheader->setCount(0);
    m_vheader->setCount(0);
    syncHeaders();
    updateGeometries();
}

// Gives the headers as many sections as the model has rows and columns, and their labels.
void QTableView::syncHeaders() {
    const int rows = model() ? model()->rowCount(rootIndex()) : 0;
    const int columns = model() ? model()->columnCount(rootIndex()) : 0;
    if (m_vheader->count() != rows)
        m_vheader->setCount(rows);
    if (m_hheader->count() != columns)
        m_hheader->setCount(columns);
    syncLabels(Qt::Horizontal);
    syncLabels(Qt::Vertical);
}

void QTableView::syncLabels(Qt::Orientation orientation) {
    QHeaderView *header = orientation == Qt::Horizontal ? m_hheader : m_vheader;
    QStringList labels;
    for (int i = 0; model() && i < header->count(); ++i) {
        const QVariant label = model()->headerData(i, orientation, Qt::DisplayRole);
        labels.append(label.isValid() ? label.toString() : QString()); // the header numbers sections without one
    }
    header->setLabels(labels);
}

/*!
    \fn QHeaderView *QTableView::horizontalHeader() const

    Returns the header above the columns.

    \sa verticalHeader()
*/

/*!
    \fn QHeaderView *QTableView::verticalHeader() const

    Returns the header left of the rows.

    \sa horizontalHeader()
*/

/*!
    \fn bool QTableView::showGrid() const

    Returns whether lines are drawn between the cells. The default is \c true.

    \sa setShowGrid(), gridStyle()
*/

/*!
    Sets whether lines are drawn between the cells to \a show.

    \sa showGrid()
*/
void QTableView::setShowGrid(bool show) {
    if (m_showGrid == show)
        return;
    m_showGrid = show;
    viewport()->update();
}

/*!
    \fn Qt::PenStyle QTableView::gridStyle() const

    Returns the style of the lines between the cells. The default is Qt::SolidLine.

    \sa setGridStyle()
*/

/*!
    Draws the lines between the cells in the style \a style.

    \sa gridStyle()
*/
void QTableView::setGridStyle(Qt::PenStyle style) {
    if (m_gridStyle == style)
        return;
    m_gridStyle = style;
    viewport()->update();
}

/*!
    \fn bool QTableView::isSortingEnabled() const

    Returns whether the user can sort the rows by clicking on a column header. The default is \c false.

    \sa setSortingEnabled()
*/

/*!
    Sets whether the user can sort the rows to \a enable. When it is on, the horizontal header shows which
    column the rows are sorted by and in which order, and a click on a column header sorts by that column
    instead of selecting it. If the header says that a column is sorted by already, the model is sorted by it
    at once.

    \sa isSortingEnabled(), sortByColumn()
*/
void QTableView::setSortingEnabled(bool enable) {
    m_sorting = enable;
    m_hheader->setSortIndicatorShown(enable);
    if (enable && model() && m_hheader->sortIndicatorSection() >= 0)
        model()->sort(m_hheader->sortIndicatorSection(), m_hheader->sortIndicatorOrder());
}

/*!
    Sorts the model by the column \a column in the order \a order, and has the horizontal header show it.

    \sa setSortingEnabled(), QAbstractItemModel::sort()
*/
void QTableView::sortByColumn(int column, Qt::SortOrder order) {
    const bool same = m_hheader->sortIndicatorSection() == column && m_hheader->sortIndicatorOrder() == order;
    m_hheader->setSortIndicator(column, order);
    // The header tells the model itself when the indicator changes and sorting is on.
    if (model() && column >= 0 && (!m_sorting || same))
        model()->sort(column, order);
}

/*!
    \fn bool QTableView::wordWrap() const

    Returns whether the text of a cell may be broken into lines. The default is \c true.

    \sa setWordWrap()
*/

/*!
    Sets whether the text of a cell may be broken into lines to \a on. The view tells the delegate through
    QStyleOptionViewItem::WrapText; QStyledItemDelegate draws one line.
*/
void QTableView::setWordWrap(bool on) {
    m_wordWrap = on;
    viewport()->update();
}

/*!
    \fn bool QTableView::isCornerButtonEnabled() const

    Returns whether a click on the button in the corner between the headers selects all cells. The default is
    \c true.

    \sa setCornerButtonEnabled()
*/

/*!
    Sets whether the corner button selects all cells to \a enable.

    \sa isCornerButtonEnabled()
*/
void QTableView::setCornerButtonEnabled(bool enable) { m_cornerEnabled = enable; }

/*!
    Returns the position, on the viewport, of the top of the row \a row; it is negative for a row that is
    scrolled out at the top.

    \sa rowAt(), rowHeight()
*/
int QTableView::rowViewportPosition(int row) const {
    return m_vheader->sectionPosition(row) - verticalScrollBar()->value();
}

/*!
    Returns the row at the position \a y of the viewport, or -1 if there is none.

    \sa rowViewportPosition()
*/
int QTableView::rowAt(int y) const { return sectionAt(m_vheader, y + verticalScrollBar()->value()); }

/*!
    Returns the height of the row \a row; 0 if it is hidden.

    \sa setRowHeight(), resizeRowToContents()
*/
int QTableView::rowHeight(int row) const { return m_vheader->sectionSize(row); }

/*!
    Sets the height of the row \a row to \a height.

    \sa rowHeight()
*/
void QTableView::setRowHeight(int row, int height) { m_vheader->resizeSection(row, height); }

/*!
    Returns the position, on the viewport, of the left edge of the column \a column; it is negative for a
    column that is scrolled out to the left.

    \sa columnAt(), columnWidth()
*/
int QTableView::columnViewportPosition(int column) const {
    return m_hheader->sectionPosition(column) - horizontalScrollBar()->value();
}

/*!
    Returns the column at the position \a x of the viewport, or -1 if there is none.

    \sa columnViewportPosition()
*/
int QTableView::columnAt(int x) const { return sectionAt(m_hheader, x + horizontalScrollBar()->value()); }

/*!
    Returns the width of the column \a column; 0 if it is hidden.

    \sa setColumnWidth(), resizeColumnToContents()
*/
int QTableView::columnWidth(int column) const { return m_hheader->sectionSize(column); }

/*!
    Sets the width of the column \a column to \a width.

    \sa columnWidth()
*/
void QTableView::setColumnWidth(int column, int width) { m_hheader->resizeSection(column, width); }

/*!
    Returns \c true if the row \a row is hidden; otherwise returns \c false.

    \sa setRowHidden(), hideRow()
*/
bool QTableView::isRowHidden(int row) const { return m_vheader->isSectionHidden(row); }

/*!
    Hides the row \a row if \a hide is \c true, and shows it again otherwise.

    \sa hideRow(), showRow(), isRowHidden()
*/
void QTableView::setRowHidden(int row, bool hide) { m_vheader->setSectionHidden(row, hide); }

/*!
    Returns \c true if the column \a column is hidden; otherwise returns \c false.

    \sa setColumnHidden(), hideColumn()
*/
bool QTableView::isColumnHidden(int column) const { return m_hheader->isSectionHidden(column); }

/*!
    Hides the column \a column if \a hide is \c true, and shows it again otherwise.

    \sa hideColumn(), showColumn(), isColumnHidden()
*/
void QTableView::setColumnHidden(int column, bool hide) { m_hheader->setSectionHidden(column, hide); }

/*!
    \fn void QTableView::hideRow(int row)

    Hides the row \a row.

    \sa showRow(), setRowHidden()
*/

/*!
    \fn void QTableView::hideColumn(int column)

    Hides the column \a column.

    \sa showColumn(), setColumnHidden()
*/

/*!
    \fn void QTableView::showRow(int row)

    Shows the row \a row again.

    \sa hideRow()
*/

/*!
    \fn void QTableView::showColumn(int column)

    Shows the column \a column again.

    \sa hideColumn()
*/

/*!
    Makes the row \a row as high as its tallest cell, and its header label, need.

    \sa resizeRowsToContents(), setRowHeight()
*/
void QTableView::resizeRowToContents(int row) {
    if (!model() || row < 0 || row >= m_vheader->count())
        return;
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    int height = QFontMetrics(m_vheader->font()).height() +
                 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, m_vheader);
    for (int c = 0; c < m_hheader->count(); ++c) {
        if (isColumnHidden(c))
            continue;
        const QModelIndex index = model()->index(row, c, rootIndex());
        height = std::max(height, itemDelegateForIndex(index)->sizeHint(option, index).height());
    }
    m_vheader->resizeSection(row, height + (m_showGrid ? 1 : 0));
}

/*!
    Makes every row as high as its contents need.

    \sa resizeRowToContents()
*/
void QTableView::resizeRowsToContents() {
    for (int r = 0; r < m_vheader->count(); ++r)
        resizeRowToContents(r);
}

/*!
    Makes the column \a column as wide as its widest cell, and its header label, need.

    \sa resizeColumnsToContents(), setColumnWidth()
*/
void QTableView::resizeColumnToContents(int column) {
    if (!model() || column < 0 || column >= m_hheader->count())
        return;
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    const int margin = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, m_hheader);
    // Room for the label and, with sorting, the arrow of the sort indicator.
    int width = QFontMetrics(m_hheader->font()).horizontalAdvance(m_hheader->labelAt(column)) + 2 * margin +
                (m_sorting ? 16 : 0);
    for (int r = 0; r < m_vheader->count(); ++r) {
        if (isRowHidden(r))
            continue;
        const QModelIndex index = model()->index(r, column, rootIndex());
        width = std::max(width, itemDelegateForIndex(index)->sizeHint(option, index).width());
    }
    m_hheader->resizeSection(column, width + (m_showGrid ? 1 : 0));
}

/*!
    Makes every column as wide as its contents need.

    \sa resizeColumnToContents()
*/
void QTableView::resizeColumnsToContents() {
    for (int c = 0; c < m_hheader->count(); ++c)
        resizeColumnToContents(c);
}

/*!
    \reimp

    Places the headers left of and above the viewport and the corner button between them, and sets the ranges
    of the scroll bars for the rows and columns.
*/
void QTableView::updateGeometries() {
    if (m_inLayout) {
        m_layoutAgain = true;
        return;
    }
    m_inLayout = true;
    QScrollBar *hbar = horizontalScrollBar(), *vbar = verticalScrollBar();
    for (int pass = 0; pass < 6; ++pass) {
        m_layoutAgain = false;
        const int top = m_hheader->isHidden() ? 0 : m_hheader->sizeHint().height();
        const int left = m_vheader->isHidden() ? 0 : m_vheader->sizeHint().width();
        setViewportMargins(left, top, 0, 0);
        const QRect inner = contentsRect();
        const QSize view = viewport()->size();
        m_hheader->setGeometry(inner.left() + left, inner.top(), view.width(), top);
        m_vheader->setGeometry(inner.left(), inner.top() + top, left, view.height());
        m_corner->setGeometry(inner.left(), inner.top(), left, top);
        m_corner->setVisible(left > 0 && top > 0);
        hbar->setPageStep(std::max(1, view.width()));
        hbar->setSingleStep(20);
        hbar->setRange(0, std::max(0, m_hheader->length() - view.width()));
        vbar->setPageStep(std::max(1, view.height()));
        vbar->setSingleStep(std::max(1, m_vheader->defaultSectionSize()));
        vbar->setRange(0, std::max(0, m_vheader->length() - view.height()));
        m_hheader->setOffset(hbar->value());
        m_vheader->setOffset(vbar->value());
        if (!m_layoutAgain)
            break;
    }
    m_inLayout = false;
    updateEditorGeometries();
}

/*!
    \reimp

    Returns the rectangle of the cell \a index on the viewport, or an empty one if its row or column is hidden
    or it is not a cell of the table.
*/
QRect QTableView::visualRect(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != model() || index.parent() != rootIndex())
        return QRect();
    const int width = columnWidth(index.column()), height = rowHeight(index.row());
    if (width <= 0 || height <= 0)
        return QRect();
    return QRect(columnViewportPosition(index.column()), rowViewportPosition(index.row()), width, height);
}

/*!
    \reimp

    Scrolls so that the cell \a index shows, where \a hint says for its row; sideways, as little as it takes.
*/
void QTableView::scrollTo(const QModelIndex &index, ScrollHint hint) {
    if (!index.isValid() || index.model() != model() || index.parent() != rootIndex())
        return;
    const int height = rowHeight(index.row()), width = columnWidth(index.column());
    if (height <= 0 || width <= 0)
        return;
    const int top = m_vheader->sectionPosition(index.row());
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
    QScrollBar *hbar = horizontalScrollBar();
    const int left = m_hheader->sectionPosition(index.column());
    if (left < hbar->value() || width > viewport()->width())
        hbar->setValue(left);
    else if (left + width > hbar->value() + viewport()->width())
        hbar->setValue(left + width - viewport()->width());
    viewport()->update();
}

/*!
    \reimp

    Returns the cell at \a point of the viewport, or an invalid index if there is none there.
*/
QModelIndex QTableView::indexAt(const QPoint &point) const {
    if (!model())
        return QModelIndex();
    const int row = rowAt(point.y()), column = columnAt(point.x());
    if (row < 0 || column < 0)
        return QModelIndex();
    return model()->index(row, column, rootIndex());
}

/*!
    \reimp

    Lays the rows and columns of the model out anew, in the default sizes, with none hidden.
*/
void QTableView::reset() {
    m_anchor = QPersistentModelIndex();
    m_pressed = QPersistentModelIndex();
    QAbstractItemView::reset();
    m_hheader->setCount(0);
    m_vheader->setCount(0);
    syncHeaders();
    updateGeometries();
}

/*!
    \reimp

    Selects all cells, if the selection mode allows several.
*/
void QTableView::selectAll() {
    if (!model() || !selectionModel() || m_vheader->count() == 0 || m_hheader->count() == 0)
        return;
    if (selectionMode() == NoSelection || selectionMode() == SingleSelection)
        return;
    const QModelIndex root = rootIndex();
    QItemSelection all;
    all.select(model()->index(0, 0, root), model()->index(m_vheader->count() - 1, m_hheader->count() - 1, root));
    selectionModel()->select(all, QItemSelectionModel::ClearAndSelect);
}

/*!
    \reimp

    Returns a size for a view of a few rows and columns.
*/
QSize QTableView::sizeHint() const { return QSize(256, 192); }

/*!
    \reimp

    Fills \a option as the base class does, and asks for wrapped text if wordWrap() is on.
*/
void QTableView::initViewItemOption(QStyleOptionViewItem *option) const {
    QAbstractItemView::initViewItemOption(option);
    if (m_wordWrap)
        option->features |= QStyleOptionViewItem::WrapText;
}

QItemSelectionModel::SelectionFlags QTableView::behaviorFlags() const {
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

// Makes the block of cells between `from` and `to` the selection, with whole rows or columns if the selection
// behavior says so.
void QTableView::selectRect(const QModelIndex &from, const QModelIndex &to) {
    if (!selectionModel() || !from.isValid() || !to.isValid())
        return;
    int top = std::min(from.row(), to.row()), bottom = std::max(from.row(), to.row());
    int left = std::min(from.column(), to.column()), right = std::max(from.column(), to.column());
    if (selectionBehavior() == SelectRows) {
        left = 0;
        right = m_hheader->count() - 1;
    } else if (selectionBehavior() == SelectColumns) {
        top = 0;
        bottom = m_vheader->count() - 1;
    }
    const QModelIndex root = rootIndex();
    QItemSelection selection;
    selection.select(model()->index(top, left, root), model()->index(bottom, right, root));
    selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
}

// Makes `index` the current cell and changes the selection the way the selection mode and the modifier keys
// say: for a click, or for a move with the keyboard.
void QTableView::selectWith(const QModelIndex &index, Qt::KeyboardModifiers modifiers, bool keyboard) {
    QItemSelectionModel *selection = selectionModel();
    if (!selection || !index.isValid())
        return;
    const bool shift = modifiers & Qt::ShiftModifier;
    const bool control = modifiers & Qt::ControlModifier;
    const SelectionMode mode = selectionMode();
    const QItemSelectionModel::SelectionFlags lines = behaviorFlags();
    switch (mode) {
    case NoSelection:
        selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
    case SingleSelection:
        selection->setCurrentIndex(index,
                                   QItemSelectionModel::SelectionFlags(QItemSelectionModel::ClearAndSelect) | lines);
        m_anchor = QPersistentModelIndex(index);
        return;
    case MultiSelection:
        selection->setCurrentIndex(index,
                                   keyboard ? QItemSelectionModel::SelectionFlags(QItemSelectionModel::NoUpdate)
                                            : QItemSelectionModel::SelectionFlags(QItemSelectionModel::Toggle) | lines);
        m_anchor = QPersistentModelIndex(index);
        return;
    case ExtendedSelection:
    case ContiguousSelection:
        if (shift && m_anchor.isValid()) {
            selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            selectRect(m_anchor, index);
            return;
        }
        if (control && mode == ExtendedSelection) {
            selection->setCurrentIndex(
                index, keyboard ? QItemSelectionModel::SelectionFlags(QItemSelectionModel::NoUpdate)
                                : QItemSelectionModel::SelectionFlags(QItemSelectionModel::Toggle) | lines);
            m_anchor = QPersistentModelIndex(index);
            return;
        }
        selection->setCurrentIndex(index,
                                   QItemSelectionModel::SelectionFlags(QItemSelectionModel::ClearAndSelect) | lines);
        m_anchor = QPersistentModelIndex(index);
        return;
    }
}

/*!
    Selects the row \a row and makes its cell in the current column the current item, if the selection mode
    and behavior allow rows to be selected: not with \c SelectColumns, and with \c SingleSelection only when
    rows are selected.

    \sa selectColumn()
*/
void QTableView::selectRow(int row) {
    if (!model() || !selectionModel() || row < 0 || row >= m_vheader->count() || isRowHidden(row))
        return;
    const SelectionMode mode = selectionMode();
    if (mode == NoSelection || selectionBehavior() == SelectColumns ||
        (mode == SingleSelection && selectionBehavior() != SelectRows))
        return;
    const int column = currentIndex().isValid() ? currentIndex().column() : firstVisible(m_hheader);
    const QModelIndex index = model()->index(row, std::max(0, column), rootIndex());
    const QItemSelectionModel::SelectionFlags command =
        mode == MultiSelection ? QItemSelectionModel::Toggle : QItemSelectionModel::ClearAndSelect;
    selectionModel()->setCurrentIndex(index, command | QItemSelectionModel::Rows);
    m_anchor = QPersistentModelIndex(index);
}

/*!
    Selects the column \a column and makes its cell in the current row the current item, if the selection mode
    and behavior allow columns to be selected: not with \c SelectRows, and with \c SingleSelection only when
    columns are selected.

    \sa selectRow()
*/
void QTableView::selectColumn(int column) {
    if (!model() || !selectionModel() || column < 0 || column >= m_hheader->count() || isColumnHidden(column))
        return;
    const SelectionMode mode = selectionMode();
    if (mode == NoSelection || selectionBehavior() == SelectRows ||
        (mode == SingleSelection && selectionBehavior() != SelectColumns))
        return;
    const int row = currentIndex().isValid() ? currentIndex().row() : firstVisible(m_vheader);
    const QModelIndex index = model()->index(std::max(0, row), column, rootIndex());
    const QItemSelectionModel::SelectionFlags command =
        mode == MultiSelection ? QItemSelectionModel::Toggle : QItemSelectionModel::ClearAndSelect;
    selectionModel()->setCurrentIndex(index, command | QItemSelectionModel::Columns);
    m_anchor = QPersistentModelIndex(index);
}

/*!
    \reimp
*/
void QTableView::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    const QRect area = event->rect();
    QStyleOptionViewItem options;
    initViewItemOption(&options);
    const QPalette::ColorGroup group = !isEnabled()                             ? QPalette::Disabled
                                       : (options.state & QStyle::State_Active) ? QPalette::Active
                                                                                : QPalette::Inactive;
    const QColor base = options.palette.color(group, QPalette::Base);
    painter.fillRect(area, base);
    if (!model() || m_vheader->count() == 0 || m_hheader->count() == 0)
        return;
    const int firstRow = rowAt(std::max(0, area.top())), firstColumn = columnAt(std::max(0, area.left()));
    if (firstRow < 0 || firstColumn < 0)
        return;
    const QItemSelectionModel *selection = selectionModel();
    const QModelIndex current = currentIndex();
    const QModelIndex root = rootIndex();
    const int grid = m_showGrid ? 1 : 0;
    QPen gridPen(gridColor(base));
    gridPen.setStyle(m_gridStyle);
    const int rows = m_vheader->count(), columns = m_hheader->count();
    int y = rowViewportPosition(firstRow);
    for (int r = firstRow; r < rows && y <= area.bottom(); ++r) {
        const int height = rowHeight(r);
        if (height <= 0)
            continue;
        const bool alternate = alternatingRowColors() && (r % 2) == 1;
        int x = columnViewportPosition(firstColumn);
        for (int c = firstColumn; c < columns && x <= area.right(); ++c) {
            const int width = columnWidth(c);
            if (width <= 0)
                continue;
            const QRect cell(x, y, width, height);
            const QModelIndex index = model()->index(r, c, root);
            QStyleOptionViewItem option = options;
            option.rect = cell.adjusted(0, 0, -grid, -grid);
            if (alternate) {
                option.features |= QStyleOptionViewItem::Alternate;
                painter.fillRect(option.rect, options.palette.color(group, QPalette::AlternateBase));
            }
            if (selection && selection->isSelected(index))
                option.state |= QStyle::State_Selected;
            if (!(model()->flags(index) & Qt::ItemIsEnabled))
                option.state &= ~QStyle::State(QStyle::State_Enabled);
            const bool isCurrent = index == current && hasFocus();
            if (isCurrent)
                option.state |= QStyle::State_HasFocus;
            itemDelegateForIndex(index)->paint(&painter, option, index);
            if (isCurrent) {
                QStyleOption focus;
                focus.initFrom(this);
                focus.rect = option.rect;
                style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
            }
            if (m_showGrid) {
                painter.setPen(gridPen);
                painter.drawLine(cell.right(), cell.top(), cell.right(), cell.bottom());
                painter.drawLine(cell.left(), cell.bottom(), cell.right(), cell.bottom());
            }
            x += width;
        }
        y += height;
    }
}

/*!
    \reimp

    A press on a cell makes it the current cell and selects it the way the selection mode and the Shift and
    Control keys say; a press beside the cells takes the selection away.
*/
void QTableView::mousePressEvent(QMouseEvent *event) {
    m_pressed = QPersistentModelIndex();
    m_dragging = false;
    if (!model()) {
        event->ignore();
        return;
    }
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        if (event->button() == Qt::LeftButton && selectionModel() && selectionMode() != NoSelection &&
            !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            selectionModel()->clearSelection();
        return;
    }
    if (!(model()->flags(index) & Qt::ItemIsEnabled))
        return;
    m_pressed = QPersistentModelIndex(index);
    m_pressedWasSelected = selectionModel() && selectionModel()->isSelected(index) && currentIndex() == index;
    emit pressed(index);
    if (event->button() == Qt::LeftButton || !(selectionModel() && selectionModel()->isSelected(index)))
        selectWith(index, event->modifiers(), false);
    m_dragging = event->button() == Qt::LeftButton;
}

/*!
    \reimp

    In the extended and contiguous selection modes, dragging with the left button selects the block of cells
    from the one that was pressed to the one under the mouse.
*/
void QTableView::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging || !(event->buttons() & Qt::LeftButton) || !selectionModel() || !m_anchor.isValid() ||
        (selectionMode() != ExtendedSelection && selectionMode() != ContiguousSelection)) {
        event->ignore();
        return;
    }
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || index == currentIndex())
        return;
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    selectRect(m_anchor, index);
}

/*!
    \reimp

    A release on the cell that was pressed is a click: clicked() is emitted, the delegate gets the event, for
    a check box, and with the \c SelectedClicked trigger a cell that was selected already is edited.
*/
void QTableView::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    const QModelIndex pressedIndex = m_pressed;
    m_pressed = QPersistentModelIndex();
    if (!model() || !pressedIndex.isValid() || indexAt(event->pos()) != pressedIndex ||
        event->button() != Qt::LeftButton)
        return;
    emit clicked(pressedIndex);
    const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
    edit(pressedIndex, m_pressedWasSelected && plain ? SelectedClicked : NoEditTriggers, event);
}

/*!
    \reimp

    A double click on a cell emits doubleClicked(), and edits the cell if \c DoubleClicked is one of the edit
    triggers and the cell can be edited; otherwise it emits activated().
*/
void QTableView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!model() || event->button() != Qt::LeftButton)
        return;
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || !(model()->flags(index) & Qt::ItemIsEnabled))
        return;
    const QPersistentModelIndex keep(index);
    emit doubleClicked(index);
    if (!keep.isValid() || edit(keep, DoubleClicked, event))
        return;
    emit activated(keep);
}

// The first section of `header` after `from`, going by `step`, that is not hidden; `from` if there is none.
int QTableView::visibleNeighbour(const QHeaderView *header, int from, int step) const {
    for (int i = from + step; i >= 0 && i < header->count(); i += step) {
        if (!header->isSectionHidden(i))
            return i;
    }
    return from;
}

int QTableView::firstVisible(const QHeaderView *header) const { return visibleNeighbour(header, -1, 1); }

int QTableView::lastVisible(const QHeaderView *header) const { return visibleNeighbour(header, header->count(), -1); }

/*!
    \reimp

    Returns the cell that \a cursorAction leads to from the current cell, over the rows and columns that are
    hidden. Home and End go to the first and last column of the row, and with Control in \a modifiers to the
    first and last cell of the table; \c MoveNext and \c MovePrevious go on into the next or previous row.
*/
QModelIndex QTableView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) {
    if (!model() || m_vheader->count() == 0 || m_hheader->count() == 0)
        return QModelIndex();
    const QModelIndex root = rootIndex();
    const int firstRow = firstVisible(m_vheader), firstColumn = firstVisible(m_hheader);
    if (firstRow < 0 || firstColumn < 0)
        return QModelIndex();
    const QModelIndex current = currentIndex();
    if (!current.isValid() || current.parent() != root)
        return model()->index(firstRow, firstColumn, root);
    int row = current.row(), column = current.column();
    const bool control = modifiers & Qt::ControlModifier;
    switch (cursorAction) {
    case MoveUp:
        row = visibleNeighbour(m_vheader, row, -1);
        break;
    case MoveDown:
        row = visibleNeighbour(m_vheader, row, 1);
        break;
    case MoveLeft:
        column = visibleNeighbour(m_hheader, column, -1);
        break;
    case MoveRight:
        column = visibleNeighbour(m_hheader, column, 1);
        break;
    case MoveHome:
        column = firstColumn;
        if (control)
            row = firstRow;
        break;
    case MoveEnd:
        column = lastVisible(m_hheader);
        if (control)
            row = lastVisible(m_vheader);
        break;
    case MovePageUp:
    case MovePageDown: {
        // As many rows as fit in the viewport.
        const int step = cursorAction == MovePageDown ? 1 : -1;
        int travelled = 0;
        for (int next = visibleNeighbour(m_vheader, row, step); next != row;
             next = visibleNeighbour(m_vheader, row, step)) {
            travelled += rowHeight(next);
            if (travelled > viewport()->height())
                break;
            row = next;
        }
        if (row == current.row())
            row = visibleNeighbour(m_vheader, row, step);
        break;
    }
    case MoveNext: {
        const int next = visibleNeighbour(m_hheader, column, 1);
        if (next != column) {
            column = next;
        } else if (const int below = visibleNeighbour(m_vheader, row, 1); below != row) {
            row = below;
            column = firstColumn;
        }
        break;
    }
    case MovePrevious: {
        const int previous = visibleNeighbour(m_hheader, column, -1);
        if (previous != column) {
            column = previous;
        } else if (const int above = visibleNeighbour(m_vheader, row, -1); above != row) {
            row = above;
            column = lastVisible(m_hheader);
        }
        break;
    }
    }
    return model()->index(row, column, root);
}

/*!
    \reimp

    The arrow, Home, End and page keys move the current cell; Return and Enter activate it; F2 edits it, as
    typing does with the \c AnyKeyPressed trigger; Space toggles a check box or the selection; Control+A selects
    everything.
*/
void QTableView::keyPressEvent(QKeyEvent *event) {
    if (!model() || m_vheader->count() == 0 || m_hheader->count() == 0) {
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
        // Control goes with Home and End; it does not toggle the selection there.
        Qt::KeyboardModifiers selecting = event->modifiers();
        if (action == MoveHome || action == MoveEnd)
            selecting.setFlag(Qt::ControlModifier, false);
        if (target.isValid() && target != current)
            selectWith(target, selecting, true);
        else if (target.isValid() && !current.isValid())
            selectWith(target, Qt::NoModifier, true);
        event->accept();
        return;
    }
    switch (event->key()) {
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
            selectionModel()->select(current,
                                     QItemSelectionModel::SelectionFlags(selectionMode() == SingleSelection
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
        const QString text = event->text();
        const bool printable =
            !text.isEmpty() && text.at(0).isPrint() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier));
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
void QTableView::resizeEvent(QResizeEvent *event) {
    static_cast<void>(event);
    updateGeometries();
}

/*!
    \reimp
*/
void QTableView::showEvent(QShowEvent *event) {
    static_cast<void>(event);
    updateGeometries();
}

/*!
    \reimp

    Moves the headers with the cells, and asks a model that hands out its rows in steps for more when the end
    is reached; \a dx and \a dy are not used.
*/
void QTableView::scrollContentsBy(int dx, int dy) {
    static_cast<void>(dx);
    static_cast<void>(dy);
    m_hheader->setOffset(horizontalScrollBar()->value());
    m_vheader->setOffset(verticalScrollBar()->value());
    viewport()->update();
    updateEditorGeometries();
    QScrollBar *vbar = verticalScrollBar();
    if (model() && vbar->value() >= vbar->maximum() && model()->canFetchMore(rootIndex()))
        model()->fetchMore(rootIndex());
}

/*!
    \reimp

    Shows the tool tip of the cell under the mouse, which the model returns for Qt::ToolTipRole; other
    \a event are handled as in the base class.
*/
bool QTableView::viewportEvent(QEvent *event) {
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

    Gives the vertical header sections for the rows \a start to \a end that were inserted under \a parent.
*/
void QTableView::rowsInserted(const QModelIndex &parent, int start, int end) {
    if (parent == rootIndex()) {
        m_vheader->insertSections(start, end - start + 1);
        syncLabels(Qt::Vertical);
        updateGeometries();
    }
    QAbstractItemView::rowsInserted(parent, start, end);
}

/*!
    \reimp

    Moves the current cell off the rows \a start to \a end of \a parent before they are removed: to the row
    after them, or else the row before them.
*/
void QTableView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) {
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
    const QModelIndex current = currentIndex();
    if (parent != rootIndex() || !current.isValid() || current.parent() != parent || current.row() < start ||
        current.row() > end)
        return;
    QModelIndex next;
    if (end + 1 < model()->rowCount(parent))
        next = model()->index(end + 1, current.column(), parent);
    else if (start > 0)
        next = model()->index(start - 1, current.column(), parent);
    if (selectionModel())
        selectionModel()->setCurrentIndex(next, QItemSelectionModel::NoUpdate);
}

#include "moc_qtableview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
