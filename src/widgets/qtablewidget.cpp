// MIT License - see LICENSE
#include <QtWidgets/qtablewidget.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

namespace {

// The square where the two headers of a table meet.
class CornerWidget : public QWidget {
public:
    explicit CornerWidget(QWidget *parent) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        QStyleOptionHeader option;
        option.initFrom(this);
        option.lastSection = true;
        style()->drawControl(QStyle::CE_Header, &option, &painter, this);
    }
};

// The color of the lines between the cells: a little darker than the cells, or lighter if they are dark.
QColor gridColor(const QColor &base) {
    const bool dark = base.red() + base.green() + base.blue() < 3 * 128;
    return dark ? base.lighter(170) : base.darker(125);
}

constexpr int textMargin = 3;

} // namespace

/*!
    \class QTableWidgetItem
    \brief The QTableWidgetItem class is a cell of a QTableWidget.

    An item has a text, and how it is drawn: its alignment, colors and font. Create an item, set what it should look
    like, and give it to a table with QTableWidget::setItem(), which takes it over:

    \code
    QTableWidgetItem *name = new QTableWidgetItem("Bruce");
    name->setForeground(QBrush(QColor(0, 90, 160)));
    name->setToolTip("The name of the user");
    table->setItem(0, 0, name);
    \endcode

    Deleting an item takes it out of its table.

    \differences
    An item holds a text, not a \c QVariant: there is no \c data(), \c setData(), \c setCheckState(), \c setIcon() or
    \c setSizeHint(), and no \c type() or \c clone(). An item cannot be edited, so there is no \c ItemIsEditable flag.
    The copy of an item is not allowed.
    \enddifferences

    \sa QTableWidget
*/

/*!
    Constructs an item with no text.
*/
QTableWidgetItem::QTableWidgetItem() = default;

/*!
    Constructs an item with the given \a text.
*/
QTableWidgetItem::QTableWidgetItem(const QString &text) : m_text(text) {}

/*!
    Destroys the item, and removes it from its table.
*/
QTableWidgetItem::~QTableWidgetItem() {
    if (m_table)
        m_table->forget(this);
}

/*!
    \fn QTableWidget *QTableWidgetItem::tableWidget() const

    Returns the table that has the item, or \c nullptr if it is in none.
*/

/*!
    \fn int QTableWidgetItem::row() const

    Returns the row of the item in its table, or -1 if it is in none.

    \sa column()
*/

/*!
    \fn int QTableWidgetItem::column() const

    Returns the column of the item in its table, or -1 if it is in none.

    \sa row()
*/

// Tells the table that the item looks different.
void QTableWidgetItem::changed() {
    if (m_table)
        m_table->itemChanged(this);
}

/*!
    \fn QString QTableWidgetItem::text() const

    Returns the text of the item.

    \sa setText()
*/

/*!
    Sets the text of the item to \a text.

    \sa text()
*/
void QTableWidgetItem::setText(const QString &text) {
    if (m_text == text)
        return;
    m_text = text;
    changed();
}

/*!
    \fn QString QTableWidgetItem::toolTip() const

    Returns the tool tip of the item, which the table shows when the mouse rests over the cell.

    \sa setToolTip()
*/

/*!
    \fn void QTableWidgetItem::setToolTip(const QString &toolTip)

    Sets the tool tip of the item to \a toolTip. Without one, the cell shows the tool tip of the table.

    \sa toolTip()
*/

/*!
    \fn Qt::Alignment QTableWidgetItem::textAlignment() const

    Returns how the text is placed in the cell. The default is \c{Qt::AlignLeft | Qt::AlignVCenter}.

    \sa setTextAlignment()
*/

/*!
    Sets how the text is placed in the cell to \a alignment.

    \sa textAlignment()
*/
void QTableWidgetItem::setTextAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
    changed();
}

/*!
    \fn QBrush QTableWidgetItem::foreground() const

    Returns the brush of the text. A brush with no style (the default) stands for the text color of the palette.

    \sa setForeground()
*/

/*!
    Sets the brush of the text to \a brush. Only its color is used.

    \sa foreground()
*/
void QTableWidgetItem::setForeground(const QBrush &brush) {
    m_foreground = brush;
    changed();
}

/*!
    \fn QBrush QTableWidgetItem::background() const

    Returns the brush that fills the cell. A brush with no style (the default) leaves it to the table.

    \sa setBackground()
*/

/*!
    Sets the brush that fills the cell to \a brush. Only its color is used.

    \sa background()
*/
void QTableWidgetItem::setBackground(const QBrush &brush) {
    m_background = brush;
    changed();
}

/*!
    Returns the font of the text: the one that was set, or the font of the table.

    \sa setFont()
*/
QFont QTableWidgetItem::font() const { return m_hasFont || !m_table ? m_font : m_table->font(); }

/*!
    Sets the font of the text to \a font.

    \sa font()
*/
void QTableWidgetItem::setFont(const QFont &font) {
    m_font = font;
    m_hasFont = true;
    changed();
}

/*!
    \fn Qt::ItemFlags QTableWidgetItem::flags() const

    Returns what the user can do with the item. The default is that it is enabled and selectable.

    \sa setFlags()
*/

/*!
    Sets what the user can do with the item to \a flags.

    \sa flags()
*/
void QTableWidgetItem::setFlags(Qt::ItemFlags flags) {
    m_flags = flags;
    changed();
}

/*!
    Returns whether the item is selected. A table has one selected cell, its current cell.

    \sa setSelected()
*/
bool QTableWidgetItem::isSelected() const {
    return m_table && m_table->currentRow() == m_row && m_table->currentColumn() == m_column;
}

/*!
    Selects the item if \a select is \c true, which makes it the current item of the table; otherwise clears the
    selection if the item was selected.

    \sa isSelected()
*/
void QTableWidgetItem::setSelected(bool select) {
    if (!m_table)
        return;
    if (select)
        m_table->setCurrentCell(m_row, m_column);
    else if (isSelected())
        m_table->moveCurrent(-1, -1);
}

/*!
    \class QTableWidget
    \brief The QTableWidget class provides a table of items.

    A table has rows and columns of cells, with a header for each: the horizontal header names the columns and the
    vertical one numbers the rows. A cell is empty, or has a QTableWidgetItem. The user clicks a cell, or moves with the
    arrow keys, to select it. The table scrolls when it is larger than its viewport:

    \code
    QTableWidget *table = new QTableWidget(3, 2, window);
    table->setHorizontalHeaderLabels({"Name", "Role"});
    table->setItem(0, 0, new QTableWidgetItem("Ada"));
    table->setItem(0, 1, new QTableWidgetItem("Engineer"));
    connect(table, &QTableWidget::cellClicked, [](int row, int column) { qDebug() << row << column; });
    \endcode

    The width of a column and the height of a row are the sizes of the sections of the headers: change them with
    setColumnWidth() and setRowHeight(), or let the user drag the edge of a header. An item can have a tool tip, which
    the table shows over its cell.

    A table in the light color scheme with alternating row colors, a selected cell, a colored item and a wide column.

    \image widgets/qtablewidget.png QTableWidget in the light color scheme

    \differences
    There is no model and no \c QTableView: the table keeps its items itself. There is no editing (\c setEditTriggers(),
    \c editItem()), no sorting, no drag and drop, no spans, and no cell widgets. A single cell is selected, so
    \c selectedItems() has at most one item and there is no \c selectionMode, selection model or \c selectRow(). The
    table scrolls by pixels, and the user cannot move or hide the sections of the headers. Items are not created
    by the table: put them there with setItem().
    \enddifferences

    \sa QTableWidgetItem, QHeaderView, QScrollArea
*/

/*!
    Constructs an empty table with the given \a parent.
*/
QTableWidget::QTableWidget(QWidget *parent) : QAbstractScrollArea(parent) { init(0, 0); }

/*!
    Constructs a table of \a rows by \a columns empty cells with the given \a parent.
*/
QTableWidget::QTableWidget(int rows, int columns, QWidget *parent) : QAbstractScrollArea(parent) {
    init(rows, columns);
}

void QTableWidget::init(int rows, int columns) {
    setFocusPolicy(Qt::StrongFocus);
    m_hheader = new QHeaderView(Qt::Horizontal, this);
    m_vheader = new QHeaderView(Qt::Vertical, this);
    m_hheader->setSectionsClickable(true);
    m_vheader->setSectionsClickable(true);
    m_corner = new CornerWidget(this);
    QObject::connect(m_hheader, &QHeaderView::sectionResized, [this](int, int, int) {
        updateLayout();
        viewport()->update();
    });
    QObject::connect(m_vheader, &QHeaderView::sectionResized, [this](int, int, int) {
        updateLayout();
        viewport()->update();
    });
    verticalScrollBar()->setSingleStep(m_vheader->defaultSectionSize());
    horizontalScrollBar()->setSingleStep(20);
    setRowCount(rows);
    setColumnCount(columns);
    updateLayout();
}

/*!
    Destroys the table, and the items in it.
*/
QTableWidget::~QTableWidget() {
    for (auto &row : m_cells)
        for (QTableWidgetItem *&item : row) {
            if (item)
                item->m_table = nullptr;
            delete item;
            item = nullptr;
        }
}

/*!
    \fn QHeaderView *QTableWidget::horizontalHeader() const

    Returns the header of the columns.

    \sa verticalHeader()
*/

/*!
    \fn QHeaderView *QTableWidget::verticalHeader() const

    Returns the header of the rows.

    \sa horizontalHeader()
*/

/*!
    Returns the number of rows.

    \sa setRowCount()
*/
int QTableWidget::rowCount() const { return m_rows; }

/*!
    Returns the number of columns.

    \sa setColumnCount()
*/
int QTableWidget::columnCount() const { return m_columns; }

// Brings the numbers into the cells, after rows or columns were added or removed.
void QTableWidget::reindex() {
    for (int r = 0; r < m_rows; ++r)
        for (int c = 0; c < m_columns; ++c)
            if (QTableWidgetItem *it = m_cells[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]) {
                it->m_row = r;
                it->m_column = c;
            }
}

/*!
    Sets the number of rows to \a rows. New rows are empty; the items of rows that go are deleted.

    \sa rowCount(), insertRow(), removeRow()
*/
void QTableWidget::setRowCount(int rows) {
    rows = std::max(0, rows);
    if (rows == m_rows)
        return;
    for (int r = rows; r < m_rows; ++r)
        for (QTableWidgetItem *&item : m_cells[static_cast<std::size_t>(r)]) {
            if (item)
                item->m_table = nullptr;
            delete item;
        }
    m_cells.resize(static_cast<std::size_t>(rows), std::vector<QTableWidgetItem *>(static_cast<std::size_t>(m_columns)));
    m_rows = rows;
    m_vheader->setCount(rows);
    if (m_currentRow >= rows)
        moveCurrent(-1, -1);
    updateLayout();
    viewport()->update();
}

/*!
    Sets the number of columns to \a columns. New columns are empty; the items of columns that go are deleted.

    \sa columnCount(), insertColumn(), removeColumn()
*/
void QTableWidget::setColumnCount(int columns) {
    columns = std::max(0, columns);
    if (columns == m_columns)
        return;
    for (auto &row : m_cells) {
        for (int c = columns; c < m_columns; ++c) {
            QTableWidgetItem *item = row[static_cast<std::size_t>(c)];
            if (item)
                item->m_table = nullptr;
            delete item;
        }
        row.resize(static_cast<std::size_t>(columns), nullptr);
    }
    m_columns = columns;
    m_hheader->setCount(columns);
    if (m_currentColumn >= columns)
        moveCurrent(-1, -1);
    updateLayout();
    viewport()->update();
}

/*!
    Inserts an empty row at \a row, before the row that is there; rows that follow move down. \a row is brought into
    the range 0 to rowCount().

    \sa removeRow(), setRowCount()
*/
void QTableWidget::insertRow(int row) {
    row = std::clamp(row, 0, m_rows);
    m_cells.insert(m_cells.begin() + row, std::vector<QTableWidgetItem *>(static_cast<std::size_t>(m_columns)));
    ++m_rows;
    m_vheader->insertSections(row, 1);
    reindex();
    if (m_currentRow >= row) {
        ++m_currentRow;
        emit currentCellChanged(m_currentRow, m_currentColumn, m_currentRow - 1, m_currentColumn);
    }
    updateLayout();
    viewport()->update();
}

/*!
    Removes the row \a row, and deletes its items. Does nothing if there is no such row.

    \sa insertRow(), setRowCount()
*/
void QTableWidget::removeRow(int row) {
    if (row < 0 || row >= m_rows)
        return;
    for (QTableWidgetItem *&item : m_cells[static_cast<std::size_t>(row)]) {
        if (item)
            item->m_table = nullptr;
        delete item;
    }
    m_cells.erase(m_cells.begin() + row);
    --m_rows;
    m_vheader->removeSections(row, 1);
    reindex();
    if (m_currentRow == row) {
        moveCurrent(-1, -1);
    } else if (m_currentRow > row) {
        --m_currentRow;
        emit currentCellChanged(m_currentRow, m_currentColumn, m_currentRow + 1, m_currentColumn);
    }
    updateLayout();
    viewport()->update();
}

/*!
    Inserts an empty column at \a column, before the column that is there; columns that follow move to the right.
    \a column is brought into the range 0 to columnCount().

    \sa removeColumn(), setColumnCount()
*/
void QTableWidget::insertColumn(int column) {
    column = std::clamp(column, 0, m_columns);
    for (auto &row : m_cells)
        row.insert(row.begin() + column, nullptr);
    ++m_columns;
    m_hheader->insertSections(column, 1);
    reindex();
    if (m_currentColumn >= column) {
        ++m_currentColumn;
        emit currentCellChanged(m_currentRow, m_currentColumn, m_currentRow, m_currentColumn - 1);
    }
    updateLayout();
    viewport()->update();
}

/*!
    Removes the column \a column, and deletes its items. Does nothing if there is no such column.

    \sa insertColumn(), setColumnCount()
*/
void QTableWidget::removeColumn(int column) {
    if (column < 0 || column >= m_columns)
        return;
    for (auto &row : m_cells) {
        QTableWidgetItem *item = row[static_cast<std::size_t>(column)];
        if (item)
            item->m_table = nullptr;
        delete item;
        row.erase(row.begin() + column);
    }
    --m_columns;
    m_hheader->removeSections(column, 1);
    reindex();
    if (m_currentColumn == column) {
        moveCurrent(-1, -1);
    } else if (m_currentColumn > column) {
        --m_currentColumn;
        emit currentCellChanged(m_currentRow, m_currentColumn, m_currentRow, m_currentColumn + 1);
    }
    updateLayout();
    viewport()->update();
}

/*!
    Returns the item at \a row and \a column, or \c nullptr if the cell is empty or there is no such cell.

    \sa setItem()
*/
QTableWidgetItem *QTableWidget::item(int row, int column) const {
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns)
        return nullptr;
    return m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
}

/*!
    Puts \a item in the cell at \a row and \a column, and the table owns it. An item that was in the cell is deleted.
    An item that is in another cell or table moves. Does nothing, and deletes \a item, if there is no such cell.

    \sa item(), takeItem()
*/
void QTableWidget::setItem(int row, int column, QTableWidgetItem *item) {
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns) {
        delete item;
        return;
    }
    QTableWidgetItem *&slot = m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
    if (slot == item)
        return;
    if (item && item->m_table)
        item->m_table->forget(item);
    if (slot) {
        slot->m_table = nullptr;
        delete slot;
    }
    slot = item;
    if (item) {
        item->m_table = this;
        item->m_row = row;
        item->m_column = column;
    }
    viewport()->update(cellRect(row, column));
}

/*!
    Removes the item at \a row and \a column from the table and returns it; the caller owns it. Returns \c nullptr if
    the cell is empty.

    \sa setItem()
*/
QTableWidgetItem *QTableWidget::takeItem(int row, int column) {
    QTableWidgetItem *it = item(row, column);
    if (!it)
        return nullptr;
    forget(it);
    return it;
}

// Takes the item out of its cell without deleting it.
void QTableWidget::forget(QTableWidgetItem *item) {
    const int row = item->m_row, column = item->m_column;
    if (row >= 0 && row < m_rows && column >= 0 && column < m_columns &&
        m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] == item) {
        m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = nullptr;
        viewport()->update(cellRect(row, column));
    }
    item->m_table = nullptr;
    item->m_row = item->m_column = -1;
}

void QTableWidget::itemChanged(QTableWidgetItem *item) { viewport()->update(cellRect(item->m_row, item->m_column)); }

/*!
    Removes and deletes every item, and the labels of the headers, and clears the selection. The size of the table and
    of its sections stays.

    \sa clearContents()
*/
void QTableWidget::clear() {
    clearContents();
    m_hheader->setLabels(QStringList());
    m_vheader->setLabels(QStringList());
    updateLayout();
}

/*!
    Removes and deletes every item, and clears the selection. The labels of the headers stay.

    \sa clear()
*/
void QTableWidget::clearContents() {
    for (auto &row : m_cells)
        for (QTableWidgetItem *&item : row) {
            if (item)
                item->m_table = nullptr;
            delete item;
            item = nullptr;
        }
    moveCurrent(-1, -1);
    viewport()->update();
}

/*!
    Sets the labels of the columns to \a labels, and makes the table have as many columns if it had fewer. A column
    that has no label is named with its number.

    \sa setVerticalHeaderLabels()
*/
void QTableWidget::setHorizontalHeaderLabels(const QStringList &labels) {
    if (labels.size() > m_columns)
        setColumnCount(static_cast<int>(labels.size()));
    m_hheader->setLabels(labels);
}

/*!
    Sets the labels of the rows to \a labels, and makes the table have as many rows if it had fewer. A row that has no
    label is named with its number.

    \sa setHorizontalHeaderLabels()
*/
void QTableWidget::setVerticalHeaderLabels(const QStringList &labels) {
    if (labels.size() > m_rows)
        setRowCount(static_cast<int>(labels.size()));
    m_vheader->setLabels(labels);
    updateLayout();
}

/*!
    Returns the height of \a row in pixels.

    \sa setRowHeight()
*/
int QTableWidget::rowHeight(int row) const { return m_vheader->sectionSize(row); }

/*!
    Sets the height of \a row to \a height pixels.

    \sa rowHeight()
*/
void QTableWidget::setRowHeight(int row, int height) { m_vheader->resizeSection(row, height); }

/*!
    Returns the width of \a column in pixels.

    \sa setColumnWidth()
*/
int QTableWidget::columnWidth(int column) const { return m_hheader->sectionSize(column); }

/*!
    Sets the width of \a column to \a width pixels.

    \sa columnWidth()
*/
void QTableWidget::setColumnWidth(int column, int width) { m_hheader->resizeSection(column, width); }

/*!
    Makes \a column as wide as its widest text, the label in the header included.

    \sa resizeColumnsToContents()
*/
void QTableWidget::resizeColumnToContents(int column) {
    if (column < 0 || column >= m_columns)
        return;
    const int margin = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, this);
    int width = m_hheader->fontMetrics().horizontalAdvance(m_hheader->labelAt(column)) + 2 * margin;
    for (int r = 0; r < m_rows; ++r)
        if (const QTableWidgetItem *it = item(r, column))
            width = std::max(width, QFontMetrics(it->font()).horizontalAdvance(it->text()) + 2 * textMargin + 1);
    m_hheader->resizeSection(column, width);
}

/*!
    Makes every column as wide as its widest text.

    \sa resizeColumnToContents()
*/
void QTableWidget::resizeColumnsToContents() {
    for (int c = 0; c < m_columns; ++c)
        resizeColumnToContents(c);
}

// The rectangle of a cell in the viewport.
QRect QTableWidget::cellRect(int row, int column) const {
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns)
        return QRect();
    return QRect(m_hheader->sectionPosition(column) - horizontalScrollBar()->value(),
                 m_vheader->sectionPosition(row) - verticalScrollBar()->value(), m_hheader->sectionSize(column),
                 m_vheader->sectionSize(row));
}

/*!
    Returns the row at \a y, a position in the viewport, or -1 if there is none.

    \sa columnAt()
*/
int QTableWidget::rowAt(int y) const { return m_vheader->logicalIndexAt(y); }

/*!
    Returns the column at \a x, a position in the viewport, or -1 if there is none.

    \sa rowAt()
*/
int QTableWidget::columnAt(int x) const { return m_hheader->logicalIndexAt(x); }

/*!
    Returns the item at \a pos, a position in the viewport, or \c nullptr if the cell there is empty or there is none.

    \sa visualItemRect()
*/
QTableWidgetItem *QTableWidget::itemAt(const QPoint &pos) const { return item(rowAt(pos.y()), columnAt(pos.x())); }

/*!
    \fn QTableWidgetItem *QTableWidget::itemAt(int x, int y) const
    \overload

    Returns the item at the position (\a x, \a y) in the viewport.
*/

/*!
    Returns the rectangle of \a item in the viewport, whether it is in view or not. It is empty if \a item is not in
    this table.

    \sa itemAt()
*/
QRect QTableWidget::visualItemRect(const QTableWidgetItem *item) const {
    if (!item || item->m_table != this)
        return QRect();
    return cellRect(item->m_row, item->m_column);
}

/*!
    Returns the current item, or \c nullptr if the current cell is empty or there is none.

    \sa setCurrentItem(), currentRow()
*/
QTableWidgetItem *QTableWidget::currentItem() const { return item(m_currentRow, m_currentColumn); }

/*!
    \fn int QTableWidget::currentRow() const

    Returns the row of the current cell, or -1 if there is none.

    \sa currentColumn(), setCurrentCell()
*/

/*!
    \fn int QTableWidget::currentColumn() const

    Returns the column of the current cell, or -1 if there is none.

    \sa currentRow(), setCurrentCell()
*/

// A cell is selectable unless it has an item that says it is not.
bool QTableWidget::selectable(int row, int column) const {
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns)
        return false;
    const QTableWidgetItem *it = item(row, column);
    return !it || (it->flags() & Qt::ItemIsSelectable && it->flags() & Qt::ItemIsEnabled);
}

void QTableWidget::moveCurrent(int row, int column) {
    if (row == m_currentRow && column == m_currentColumn)
        return;
    const int oldRow = m_currentRow, oldColumn = m_currentColumn;
    viewport()->update(cellRect(oldRow, oldColumn));
    m_currentRow = row;
    m_currentColumn = column;
    viewport()->update(cellRect(row, column));
    emit currentCellChanged(row, column, oldRow, oldColumn);
    emit itemSelectionChanged();
}

/*!
    Makes the cell at \a row and \a column the current cell, which selects it, and scrolls it into view. Does nothing
    if there is no such cell, or if its item cannot be selected.

    \sa currentRow(), currentColumn()
*/
void QTableWidget::setCurrentCell(int row, int column) {
    if (!selectable(row, column))
        return;
    moveCurrent(row, column);
    scrollToCell(row, column);
}

/*!
    Makes \a item the current item, which selects it, and scrolls it into view.

    \sa currentItem()
*/
void QTableWidget::setCurrentItem(QTableWidgetItem *item) {
    if (item && item->m_table == this)
        setCurrentCell(item->m_row, item->m_column);
}

/*!
    Returns the selected items: the current item if it is selected, none otherwise. An empty selected cell does not
    count.

    \sa currentItem()
*/
QList<QTableWidgetItem *> QTableWidget::selectedItems() const {
    QList<QTableWidgetItem *> items;
    if (QTableWidgetItem *it = currentItem())
        items.append(it);
    return items;
}

void QTableWidget::scrollToCell(int row, int column) {
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns)
        return;
    // Before the table is shown it has no size to scroll in: the scrolling waits until it has.
    if (!isVisible()) {
        m_scrollRow = row;
        m_scrollColumn = column;
        return;
    }
    m_scrollRow = m_scrollColumn = -1;
    QScrollBar *hbar = horizontalScrollBar(), *vbar = verticalScrollBar();
    const int left = m_hheader->sectionPosition(column), right = left + m_hheader->sectionSize(column);
    const int top = m_vheader->sectionPosition(row), bottom = top + m_vheader->sectionSize(row);
    const QSize view = viewport()->size();
    if (left < hbar->value())
        hbar->setValue(left);
    else if (right > hbar->value() + view.width())
        hbar->setValue(std::min(left, right - view.width()));
    if (top < vbar->value())
        vbar->setValue(top);
    else if (bottom > vbar->value() + view.height())
        vbar->setValue(std::min(top, bottom - view.height()));
}

/*!
    Scrolls the table so that \a item is in view. Does nothing if \a item is not in this table.

    \sa setCurrentItem()
*/
void QTableWidget::scrollToItem(const QTableWidgetItem *item) {
    if (item && item->m_table == this)
        scrollToCell(item->m_row, item->m_column);
}

/*!
    \fn bool QTableWidget::showGrid() const

    Returns whether the lines between the cells are drawn. The default is \c true.

    \sa setShowGrid()
*/

/*!
    Sets whether the lines between the cells are drawn to \a show.

    \sa showGrid()
*/
void QTableWidget::setShowGrid(bool show) {
    if (m_showGrid == show)
        return;
    m_showGrid = show;
    viewport()->update();
}

/*!
    \fn bool QTableWidget::alternatingRowColors() const

    Returns whether every other row has a different background. The default is \c false.

    \sa setAlternatingRowColors()
*/

/*!
    Sets whether every other row has the alternate base color of the palette as its background to \a enable.

    \sa alternatingRowColors()
*/
void QTableWidget::setAlternatingRowColors(bool enable) {
    if (m_alternating == enable)
        return;
    m_alternating = enable;
    viewport()->update();
}

// Puts the headers around the viewport, and sets the scroll bars from the sizes of the headers. A scroll bar that comes
// or goes, or a header that changes its size, changes the viewport, so this goes on until everything agrees.
void QTableWidget::updateLayout() {
    if (!m_hheader)
        return;
    if (m_inLayout) {
        m_layoutAgain = true;
        return;
    }
    m_inLayout = true;
    QScrollBar *hbar = horizontalScrollBar(), *vbar = verticalScrollBar();
    for (int pass = 0; pass < 6; ++pass) {
        m_layoutAgain = false;
        const int headerWidth = m_vheader->isHidden() ? 0 : m_vheader->sizeHint().width();
        const int headerHeight = m_hheader->isHidden() ? 0 : m_hheader->sizeHint().height();
        setViewportMargins(headerWidth, headerHeight, 0, 0);
        const QRect inner = contentsRect();
        const QSize view = viewport()->size();
        m_hheader->setGeometry(inner.left() + headerWidth, inner.top(), view.width(), headerHeight);
        m_vheader->setGeometry(inner.left(), inner.top() + headerHeight, headerWidth, view.height());
        m_corner->setGeometry(inner.left(), inner.top(), headerWidth, headerHeight);
        m_corner->setVisible(headerWidth > 0 && headerHeight > 0);
        hbar->setPageStep(std::max(1, view.width()));
        vbar->setPageStep(std::max(1, view.height()));
        hbar->setRange(0, std::max(0, m_hheader->length() - view.width()));
        vbar->setRange(0, std::max(0, m_vheader->length() - view.height()));
        m_hheader->setOffset(hbar->value());
        m_vheader->setOffset(vbar->value());
        if (!m_layoutAgain)
            break;
    }
    m_inLayout = false;
}

/*!
    \reimp
*/
void QTableWidget::resizeEvent(QResizeEvent *event) {
    (void)event;
    updateLayout();
    if (m_scrollRow >= 0 && isVisible())
        scrollToCell(m_scrollRow, m_scrollColumn);
}

/*!
    \reimp
*/
void QTableWidget::showEvent(QShowEvent *event) {
    (void)event;
    updateLayout();
    if (m_scrollRow >= 0)
        scrollToCell(m_scrollRow, m_scrollColumn);
}

/*!
    \reimp
*/
void QTableWidget::scrollContentsBy(int dx, int dy) {
    (void)dx;
    (void)dy;
    m_hheader->setOffset(horizontalScrollBar()->value());
    m_vheader->setOffset(verticalScrollBar()->value());
    viewport()->update();
}

/*!
    \reimp
*/
void QTableWidget::changeEvent(QEvent *event) {
    updateLayout();
    viewport()->update();
    QAbstractScrollArea::changeEvent(event);
}

/*!
    \reimp
*/
QSize QTableWidget::viewportSizeHint() const { return QSize(); }

/*!
    \reimp
*/
void QTableWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(viewport());
    const QPalette &pal = palette();
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QRect all = viewport()->rect();
    const QColor base = pal.color(group, QPalette::Base);
    painter.fillRect(all, base);
    const int firstRow = std::max(0, rowAt(0)), firstColumn = std::max(0, columnAt(0));
    const QColor lines = gridColor(base);
    const QColor highlight = pal.color(group, QPalette::Highlight);
    for (int r = firstRow; r < m_rows; ++r) {
        const QRect rowRect = cellRect(r, firstColumn);
        if (rowRect.top() >= all.bottom() + 1)
            break;
        for (int c = firstColumn; c < m_columns; ++c) {
            const QRect cell = cellRect(r, c);
            if (cell.left() > all.right())
                break;
            const QTableWidgetItem *it = item(r, c);
            const bool current = r == m_currentRow && c == m_currentColumn;
            if (current)
                painter.fillRect(cell, highlight);
            else if (it && it->background().style() != Qt::NoBrush)
                painter.fillRect(cell, it->background().color());
            else if (m_alternating && (r & 1))
                painter.fillRect(cell, pal.color(group, QPalette::AlternateBase));
            if (m_showGrid) {
                painter.setPen(lines);
                painter.drawLine(cell.right(), cell.top(), cell.right(), cell.bottom());
                painter.drawLine(cell.left(), cell.bottom(), cell.right(), cell.bottom());
            }
            if (it && !it->text().isEmpty()) {
                const bool enabled = isEnabled() && (it->flags() & Qt::ItemIsEnabled);
                QColor color = pal.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Text);
                if (current)
                    color = pal.color(group, QPalette::HighlightedText);
                else if (it->foreground().style() != Qt::NoBrush && enabled)
                    color = it->foreground().color();
                const int grid = m_showGrid ? 1 : 0;
                const QRect textRect = cell.adjusted(textMargin, 0, -textMargin - grid, -grid);
                const QFont font = it->font();
                painter.setFont(font);
                painter.setPen(color);
                painter.setClipRect(cell.adjusted(0, 0, -grid, -grid));
                painter.drawText(textRect, static_cast<int>(it->textAlignment()) | Qt::TextSingleLine,
                                 QFontMetrics(font).elidedText(it->text(), Qt::ElideRight, textRect.width()));
                painter.setClipping(false);
            }
            if (current && hasFocus()) {
                painter.setPen(pal.color(group, QPalette::Base).lighter(110));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(cell.adjusted(1, 1, -2, -2));
            }
        }
    }
}

/*!
    \reimp
*/
bool QTableWidget::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int row = rowAt(help->pos().y()), column = columnAt(help->pos().x());
        const QTableWidgetItem *it = item(row, column);
        if (it && !it->toolTip().isEmpty()) {
            QToolTip::showText(help->globalPos(), it->toolTip(), viewport(), cellRect(row, column));
            event->accept();
            return true;
        }
    }
    return QAbstractScrollArea::viewportEvent(event);
}

/*!
    \reimp
*/
void QTableWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int row = rowAt(event->pos().y()), column = columnAt(event->pos().x());
    m_pressedRow = row;
    m_pressedColumn = column;
    if (selectable(row, column)) {
        moveCurrent(row, column);
        scrollToCell(row, column);
    }
    event->accept();
}

/*!
    \reimp
*/
void QTableWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int row = rowAt(event->pos().y()), column = columnAt(event->pos().x());
    const bool clicked = row >= 0 && column >= 0 && row == m_pressedRow && column == m_pressedColumn;
    m_pressedRow = m_pressedColumn = -1;
    if (clicked) {
        emit cellClicked(row, column);
        if (QTableWidgetItem *it = item(row, column))
            emit itemClicked(it);
    }
    event->accept();
}

/*!
    \reimp
*/
void QTableWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int row = rowAt(event->pos().y()), column = columnAt(event->pos().x());
    if (row >= 0 && column >= 0) {
        if (selectable(row, column))
            moveCurrent(row, column);
        emit cellDoubleClicked(row, column);
    }
    event->accept();
}

/*!
    \reimp

    The arrow keys move the current cell by one, skipping cells that cannot be selected; Page Up and Page Down by the
    rows in view; Home and End go to the first and last cell of the row, and with Control to the first and last of the
    table. If there is no current cell, any of them selects the first one.
*/
void QTableWidget::keyPressEvent(QKeyEvent *event) {
    if (m_rows == 0 || m_columns == 0) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    int dRow = 0, dColumn = 0;
    int targetRow = m_currentRow, targetColumn = m_currentColumn;
    const bool control = event->modifiers() & Qt::ControlModifier;
    const int pageRows = std::max(1, viewport()->height() / std::max(1, m_vheader->defaultSectionSize()) - 1);
    switch (event->key()) {
    case Qt::Key_Up:
        dRow = -1;
        break;
    case Qt::Key_Down:
        dRow = 1;
        break;
    case Qt::Key_Left:
        dColumn = -1;
        break;
    case Qt::Key_Right:
        dColumn = 1;
        break;
    case Qt::Key_PageUp:
        targetRow = std::max(0, m_currentRow - pageRows);
        break;
    case Qt::Key_PageDown:
        targetRow = std::min(m_rows - 1, m_currentRow + pageRows);
        break;
    case Qt::Key_Home:
        targetColumn = 0;
        if (control)
            targetRow = 0;
        break;
    case Qt::Key_End:
        targetColumn = m_columns - 1;
        if (control)
            targetRow = m_rows - 1;
        break;
    default:
        event->ignore();
        return;
    }
    event->accept();
    if (m_currentRow < 0 || m_currentColumn < 0) {
        if (selectable(0, 0))
            setCurrentCell(0, 0);
        return;
    }
    if (dRow != 0 || dColumn != 0) {
        targetRow = m_currentRow + dRow;
        targetColumn = m_currentColumn + dColumn;
        while (targetRow >= 0 && targetRow < m_rows && targetColumn >= 0 && targetColumn < m_columns &&
               !selectable(targetRow, targetColumn)) {
            targetRow += dRow;
            targetColumn += dColumn;
        }
    }
    if (selectable(targetRow, targetColumn))
        setCurrentCell(targetRow, targetColumn);
}

/*!
    \reimp
*/
void QTableWidget::focusInEvent(QFocusEvent *event) {
    (void)event;
    viewport()->update();
}

/*!
    \reimp
*/
void QTableWidget::focusOutEvent(QFocusEvent *event) {
    (void)event;
    viewport()->update();
}

#include "moc_qtablewidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
