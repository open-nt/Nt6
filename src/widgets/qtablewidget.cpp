// MIT License - see LICENSE
#include <QtWidgets/qtablewidget.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qmimedata.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtooltip.h>

#include "qitemtextmatch_p.h"

#include <algorithm>
#include <functional>

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
    There is no \c setSizeHint(), \c type() or \c clone(), and the copy of an item is not allowed.
    \enddifferences

    \sa QTableWidget
*/

/*!
    Constructs an item with no text, of the given \a type.
*/
QTableWidgetItem::QTableWidgetItem(int type) : m_type(type) {}

/*!
    Constructs an item with the given \a text and \a type.
*/
QTableWidgetItem::QTableWidgetItem(const QString &text, int type) : m_type(type), m_text(text) {}

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
    if (!m_table)
        return;
    if (m_isHeader)
        m_table->headerItemChanged(this);
    else
        m_table->refreshItem(this);
}

/*!
    \since 6.0

    Constructs an item with \a icon and \a text.
    The item is of the given \a type.
*/
QTableWidgetItem::QTableWidgetItem(const QIcon &icon, const QString &text, int type) : QTableWidgetItem(text, type) {
    m_icon = icon;
}

/*!
    \fn QIcon QTableWidgetItem::icon() const
    \since 6.0

    Returns the icon of the item, shown before its text; a null icon if it has none.
*/

/*!
    \since 6.0

    Sets the icon of the item to \a icon, which the table shows in its icon size.
*/
void QTableWidgetItem::setIcon(const QIcon &icon) {
    m_icon = icon;
    changed();
}

/*!
    \since 6.0

    Returns the check state of the item: Qt::Unchecked for an item that has no check box.

    \sa setCheckState()
*/
Qt::CheckState QTableWidgetItem::checkState() const {
    return m_checkState < 0 ? Qt::Unchecked : static_cast<Qt::CheckState>(m_checkState);
}

/*!
    \since 6.0

    Sets the check state of the item to \a state, which gives it a check box before its text. With
    Qt::ItemIsUserCheckable in its flags, a click on the box, or Space while it is the current item, toggles it.

    \sa checkState()
*/
void QTableWidgetItem::setCheckState(Qt::CheckState state) {
    if (m_checkState == static_cast<int>(state))
        return;
    m_checkState = static_cast<int>(state);
    changed();
}

/*!
    \since 6.0

    Returns the data of the item for \a role: the text for Qt::DisplayRole and Qt::EditRole, the check state, tool
    tip, alignment, brushes and font for their roles, and what setData() stored for the other roles. Returns an
    invalid QVariant for a role that has nothing.

    \sa setData()
*/
QVariant QTableWidgetItem::data(int role) const {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return m_text;
    case Qt::CheckStateRole:
        return m_checkState < 0 ? QVariant() : QVariant(m_checkState);
    case Qt::DecorationRole:
        return m_icon.isNull() ? QVariant() : QVariant::fromValue(m_icon);
    case Qt::ToolTipRole:
        return m_toolTip.isEmpty() ? QVariant() : QVariant(m_toolTip);
    case Qt::TextAlignmentRole:
        return QVariant(static_cast<int>(m_alignment));
    case Qt::ForegroundRole:
        return m_foreground.style() == Qt::NoBrush ? QVariant() : QVariant::fromValue(m_foreground);
    case Qt::BackgroundRole:
        return m_background.style() == Qt::NoBrush ? QVariant() : QVariant::fromValue(m_background);
    case Qt::FontRole:
        return m_hasFont ? QVariant::fromValue(m_font) : QVariant();
    default:
        return m_data.value(role);
    }
}

/*!
    \since 6.0

    Sets the data of the item for \a role to \a value. The roles that the item shows change it; the other roles,
    such as Qt::UserRole, are stored for data() to return. The table emits itemChanged().
*/
void QTableWidgetItem::setData(int role, const QVariant &value) {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        setText(value.toString());
        return;
    case Qt::CheckStateRole:
        if (value.isValid()) {
            setCheckState(static_cast<Qt::CheckState>(value.toInt()));
        } else if (m_checkState >= 0) {
            m_checkState = -1;
            changed();
        }
        return;
    case Qt::ToolTipRole:
        m_toolTip = value.toString();
        changed();
        return;
    case Qt::DecorationRole:
        setIcon(value.value<QIcon>());
        return;
    case Qt::TextAlignmentRole:
        setTextAlignment(Qt::Alignment::fromInt(value.toInt()));
        return;
    case Qt::ForegroundRole:
        setForeground(value.value<QBrush>());
        return;
    case Qt::BackgroundRole:
        setBackground(value.value<QBrush>());
        return;
    case Qt::FontRole:
        setFont(value.value<QFont>());
        return;
    default:
        if (value.isValid())
            m_data.insert(role, value);
        else
            m_data.remove(role);
        changed();
    }
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
    \since 6.0

    Returns \c true if this item sorts before \a other: by their Qt::DisplayRole data, numbers by value and anything
    else as text. Reimplement it to sort by something else.
*/
bool QTableWidgetItem::operator<(const QTableWidgetItem &other) const {
    const QVariant a = data(Qt::DisplayRole);
    const QVariant b = other.data(Qt::DisplayRole);
    bool okA = false, okB = false;
    const double x = a.toString().toDouble(&okA);
    const double y = b.toString().toDouble(&okB);
    if (okA && okB)
        return x < y;
    return QString::localeAwareCompare(a.toString(), b.toString()) < 0;
}

/*!
    Returns whether the item is selected. A table has one selected cell, its current cell.

    \sa setSelected()
*/
bool QTableWidgetItem::isSelected() const {
    return m_table && m_table->selectionMode() != QAbstractItemView::NoSelection && m_table->currentRow() == m_row &&
           m_table->currentColumn() == m_column;
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
    There is no model and no \c QTableView: the table keeps its items itself. Editing is with editItem() only (no
    \c setEditTriggers()), and there is no drag and drop and no spans. A single cell is selected, so
    \c selectedItems() has at most one item: every selection mode but \c NoSelection selects the current cell, and
    \c ExtendedSelection is the default, as in Qt; selectRow() makes a cell of the row current. There is no
    selection model. Sorting happens when it is turned on and when a header is clicked, not when an item changes.
    The table scrolls by pixels, and the user cannot move the sections of the headers. Items are not created by the
    table: put them there with setItem().
    \enddifferences

    \sa QTableWidgetItem, QHeaderView, QScrollArea
*/

/*!
    Constructs an empty table with the given \a parent.
*/
QTableWidget::QTableWidget(QWidget *parent) : QAbstractItemView(parent) { init(0, 0); }

/*!
    Constructs a table of \a rows by \a columns empty cells with the given \a parent.
*/
QTableWidget::QTableWidget(int rows, int columns, QWidget *parent) : QAbstractItemView(parent) { init(rows, columns); }

void QTableWidget::init(int rows, int columns) {
    setFocusPolicy(Qt::StrongFocus);
    // As in Qt, but only one cell is ever selected: see the differences from Qt.
    setSelectionMode(ExtendedSelection);
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
    // A click on a column header, which turns its sort indicator round, sorts the rows while sorting is on.
    QObject::connect(m_hheader, &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) {
        if (m_sortingEnabled)
            sortItems(column, order);
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
    m_editing = nullptr;
    delete m_prototype;
    for (std::vector<QTableWidgetItem *> *items : {&m_hItems, &m_vItems})
        for (QTableWidgetItem *&item : *items) {
            if (item)
                item->m_table = nullptr;
            delete item;
            item = nullptr;
        }
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
    if (rows < m_rows)
        shiftCellWidgets(true, rows, -(m_rows - rows));
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
    if (columns < m_columns)
        shiftCellWidgets(false, columns, -(m_columns - columns));
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
    shiftCellWidgets(true, row, 1);
    if (static_cast<std::size_t>(row) < m_vItems.size())
        m_vItems.insert(m_vItems.begin() + row, nullptr);
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
    shiftCellWidgets(true, row, -1);
    if (static_cast<std::size_t>(row) < m_vItems.size()) {
        delete m_vItems[static_cast<std::size_t>(row)];
        m_vItems.erase(m_vItems.begin() + row);
    }
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
    shiftCellWidgets(false, column, 1);
    if (static_cast<std::size_t>(column) < m_hItems.size())
        m_hItems.insert(m_hItems.begin() + column, nullptr);
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
    shiftCellWidgets(false, column, -1);
    if (static_cast<std::size_t>(column) < m_hItems.size()) {
        delete m_hItems[static_cast<std::size_t>(column)];
        m_hItems.erase(m_hItems.begin() + column);
    }
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
    if (item->m_isHeader) {
        for (std::vector<QTableWidgetItem *> *items : {&m_hItems, &m_vItems})
            std::replace(items->begin(), items->end(), item, static_cast<QTableWidgetItem *>(nullptr));
        item->m_table = nullptr;
        return;
    }
    if (item == m_editing)
        closeItemEditor(false);
    const int row = item->m_row, column = item->m_column;
    if (row >= 0 && row < m_rows && column >= 0 && column < m_columns &&
        m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] == item) {
        m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = nullptr;
        viewport()->update(cellRect(row, column));
    }
    item->m_table = nullptr;
    item->m_row = item->m_column = -1;
}

void QTableWidget::refreshItem(QTableWidgetItem *item) {
    viewport()->update(cellRect(item->m_row, item->m_column));
    emit itemChanged(item);
    emit cellChanged(item->m_row, item->m_column);
}

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
    for (qsizetype i = 0; i < labels.size() && i < static_cast<qsizetype>(m_hItems.size()); ++i)
        if (QTableWidgetItem *it = m_hItems[static_cast<std::size_t>(i)])
            it->m_text = labels.at(i);
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
    for (qsizetype i = 0; i < labels.size() && i < static_cast<qsizetype>(m_vItems.size()); ++i)
        if (QTableWidgetItem *it = m_vItems[static_cast<std::size_t>(i)])
            it->m_text = labels.at(i);
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

/*!
    \since 6.0

    Makes \a row as high as its highest text: with wordWrap(), the text wrapped to the width of its column.

    \sa resizeRowsToContents()
*/
void QTableWidget::resizeRowToContents(int row) {
    if (row < 0 || row >= m_rows)
        return;
    int height = m_vheader->minimumSectionSize();
    for (int c = 0; c < m_columns; ++c) {
        const QTableWidgetItem *it = item(row, c);
        if (!it)
            continue;
        const QFontMetrics fm(it->font());
        const int width = std::max(1, columnWidth(c) - 2 * textMargin);
        const int textHeight = m_wordWrap ? fm.boundingRect(QRect(0, 0, width, 100000), Qt::TextWordWrap, it->text()).height()
                                          : fm.height();
        height = std::max(height, textHeight + 6);
    }
    m_vheader->resizeSection(row, height);
}

/*!
    \since 6.0

    Makes every row as high as its highest text.

    \sa resizeRowToContents()
*/
void QTableWidget::resizeRowsToContents() {
    for (int r = 0; r < m_rows; ++r)
        resizeRowToContents(r);
}

/*!
    \fn bool QTableWidget::isSortingEnabled() const
    \since 6.0

    Returns \c true if a click on a column header sorts the rows.
*/

/*!
    \since 6.0

    Turns sorting on if \a enable is \c true: the column headers show the sort indicator, the rows are sorted by
    its column now, and a click on a header sorts them by that column, the other way round on a second click. A
    change of an item does not sort the rows again.
*/
void QTableWidget::setSortingEnabled(bool enable) {
    m_sortingEnabled = enable;
    m_hheader->setSortIndicatorShown(enable);
    if (enable) {
        m_hheader->setSectionsClickable(true);
        if (m_hheader->sortIndicatorSection() >= 0)
            sortItems(m_hheader->sortIndicatorSection(), m_hheader->sortIndicatorOrder());
    }
}

/*!
    \since 6.0

    Sorts the rows by their items in \a column, in \a order, as QTableWidgetItem::operator<() compares them. Rows
    without an item in the column go last. Cell widgets and the current cell move with their rows.

    \sa setSortingEnabled()
*/
void QTableWidget::sortItems(int column, Qt::SortOrder order) {
    if (column < 0 || column >= m_columns || m_rows < 2)
        return;
    closeItemEditor(true);
    std::vector<int> rows(static_cast<std::size_t>(m_rows));
    for (int r = 0; r < m_rows; ++r)
        rows[static_cast<std::size_t>(r)] = r;
    const auto cellAt = [this, column](int r) { return item(r, column); };
    std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
        const QTableWidgetItem *x = cellAt(a);
        const QTableWidgetItem *y = cellAt(b);
        if (!x || !y)
            return x != nullptr;
        return order == Qt::AscendingOrder ? *x < *y : *y < *x;
    });
    std::vector<int> newRowOf(static_cast<std::size_t>(m_rows));
    std::vector<std::vector<QTableWidgetItem *>> cells;
    cells.reserve(static_cast<std::size_t>(m_rows));
    for (int k = 0; k < m_rows; ++k) {
        const int from = rows[static_cast<std::size_t>(k)];
        cells.push_back(m_cells[static_cast<std::size_t>(from)]);
        newRowOf[static_cast<std::size_t>(from)] = k;
    }
    m_cells = std::move(cells);
    reindex();
    for (CellWidget &w : m_cellWidgets)
        if (w.row >= 0 && w.row < m_rows)
            w.row = newRowOf[static_cast<std::size_t>(w.row)];
    if (m_currentRow >= 0 && m_currentRow < m_rows)
        m_currentRow = newRowOf[static_cast<std::size_t>(m_currentRow)];
    if (m_hheader->sortIndicatorSection() != column || m_hheader->sortIndicatorOrder() != order) {
        const bool blocked = m_hheader->blockSignals(true);
        m_hheader->setSortIndicator(column, order);
        m_hheader->blockSignals(blocked);
    }
    placeCellWidgets();
    viewport()->update();
}

/*!
    \fn void QTableWidget::sortByColumn(int column, Qt::SortOrder order)
    \since 6.0

    Sorts the rows by \a column in \a order, as sortItems() does.
*/

/*!
    \fn bool QTableWidget::wordWrap() const
    \since 6.0

    Returns whether the texts of the cells wrap; \c false by default.
*/

/*!
    \since 6.0

    Sets whether the texts of the cells wrap within their cells to \a on. resizeRowsToContents() makes the rows high
    enough for them.
*/
void QTableWidget::setWordWrap(bool on) {
    m_wordWrap = on;
    viewport()->update();
}

/*!
    \since 6.0

    Returns \c true if \a row is hidden.
*/
bool QTableWidget::isRowHidden(int row) const { return m_vheader->isSectionHidden(row); }

/*!
    \since 6.0

    Hides \a row if \a hide is \c true, or shows it.
*/
void QTableWidget::setRowHidden(int row, bool hide) {
    m_vheader->setSectionHidden(row, hide);
    updateLayout();
    viewport()->update();
}

/*!
    \since 6.0

    Returns \c true if \a column is hidden.
*/
bool QTableWidget::isColumnHidden(int column) const { return m_hheader->isSectionHidden(column); }

/*!
    \since 6.0

    Hides \a column if \a hide is \c true, or shows it.
*/
void QTableWidget::setColumnHidden(int column, bool hide) {
    m_hheader->setSectionHidden(column, hide);
    updateLayout();
    viewport()->update();
}

/*!
    \since 6.0

    Makes a cell of \a row the current, selected cell: the one in the current column, or the first column.
*/
void QTableWidget::selectRow(int row) { setCurrentCell(row, std::max(0, m_currentColumn)); }

/*!
    \since 6.0

    Makes a cell of \a column the current, selected cell: the one in the current row, or the first row.
*/
void QTableWidget::selectColumn(int column) { setCurrentCell(std::max(0, m_currentRow), column); }

/*!
    \fn void QTableWidget::hideRow(int row)
    \since 6.0

    Hides \a row.
*/

/*!
    \fn void QTableWidget::showRow(int row)
    \since 6.0

    Shows \a row.
*/

/*!
    \fn void QTableWidget::hideColumn(int column)
    \since 6.0

    Hides \a column.
*/

/*!
    \fn void QTableWidget::showColumn(int column)
    \since 6.0

    Shows \a column.
*/

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
    QTableWidgetItem *now = item(row, column);
    QTableWidgetItem *before = item(oldRow, oldColumn);
    if (now != before)
        emit currentItemChanged(now, before);
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
        if (selectionMode() != NoSelection)
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
    for (int column : m_hheader->contentsSections())
        resizeColumnToContents(column);
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
    placeCellWidgets();
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
    placeCellWidgets();
    if (m_editor && m_editing)
        m_editor->setGeometry(cellRect(m_editing->m_row, m_editing->m_column));
    viewport()->update();
}

/*!
    \reimp
*/
void QTableWidget::changeEvent(QEvent *event) {
    updateLayout();
    viewport()->update();
    QAbstractItemView::changeEvent(event);
}

/*!
    \reimp
*/
QSize QTableWidget::viewportSizeHint() const { return QSize(); }

/*!
    \reimp

    The current cell is selected unless the mode is \c NoSelection.
*/
void QTableWidget::selectionModeChanged() {
    if (m_currentRow >= 0)
        emit itemSelectionChanged();
}

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
            const bool isCurrent = r == m_currentRow && c == m_currentColumn;
            const bool current = isCurrent && selectionMode() != NoSelection;
            if (current)
                painter.fillRect(cell, highlight);
            else if (it && it->background().style() != Qt::NoBrush)
                painter.fillRect(cell, it->background().color());
            else if (alternatingRowColors() && (r & 1))
                painter.fillRect(cell, pal.color(group, QPalette::AlternateBase));
            if (m_showGrid) {
                painter.setPen(lines);
                painter.drawLine(cell.right(), cell.top(), cell.right(), cell.bottom());
                painter.drawLine(cell.left(), cell.bottom(), cell.right(), cell.bottom());
            }
            int checkWidth = 0;
            if (it && it->m_checkState >= 0) {
                QStyleOptionButton box;
                box.rect = checkRect(r, c);
                box.palette = pal;
                box.state = QStyle::State_None;
                if (isEnabled() && (it->flags() & Qt::ItemIsEnabled))
                    box.state |= QStyle::State_Enabled;
                box.state |= it->checkState() == Qt::Unchecked ? QStyle::State_Off : QStyle::State_On;
                style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &box, &painter, this);
                checkWidth = box.rect.right() + 1 - cell.left();
            }
            if (it && !it->icon().isNull()) {
                const QSize size = iconSize().isValid() ? iconSize() : QSize(16, 16);
                const bool on = isEnabled() && (it->flags() & Qt::ItemIsEnabled);
                const QRect iconRect(cell.left() + std::max(textMargin, checkWidth + textMargin / 2), cell.top(),
                                     size.width(), cell.height());
                it->icon().paint(&painter, iconRect, Qt::AlignCenter, on ? QIcon::Normal : QIcon::Disabled);
                checkWidth = iconRect.right() + 1 - cell.left();
            }
            if (it && !it->text().isEmpty()) {
                const bool enabled = isEnabled() && (it->flags() & Qt::ItemIsEnabled);
                QColor color = pal.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Text);
                if (current)
                    color = pal.color(group, QPalette::HighlightedText);
                else if (it->foreground().style() != Qt::NoBrush && enabled)
                    color = it->foreground().color();
                const int grid = m_showGrid ? 1 : 0;
                const QRect textRect =
                    cell.adjusted(std::max(textMargin, checkWidth + textMargin / 2), 0, -textMargin - grid, -grid);
                const QFont font = it->font();
                painter.setFont(font);
                painter.setPen(color);
                painter.setClipRect(cell.adjusted(0, 0, -grid, -grid));
                if (m_wordWrap)
                    painter.drawText(textRect, static_cast<int>(it->textAlignment()) | Qt::TextWordWrap, it->text());
                else
                    painter.drawText(textRect, static_cast<int>(it->textAlignment()) | Qt::TextSingleLine,
                                     QFontMetrics(font).elidedText(it->text(), Qt::ElideRight, textRect.width()));
                painter.setClipping(false);
            }
            if (isCurrent && hasFocus()) {
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
    if (event->type() == QEvent::MouseMove && (viewport()->hasMouseTracking() || hasMouseTracking())) {
        const QPoint pos = static_cast<QMouseEvent *>(event)->pos();
        const int row = rowAt(pos.y()), column = columnAt(pos.x());
        if (row != m_hoverRow || column != m_hoverColumn) {
            m_hoverRow = row;
            m_hoverColumn = column;
            if (row >= 0 && column >= 0) {
                emit cellEntered(row, column);
                if (QTableWidgetItem *it = item(row, column))
                    emit itemEntered(it);
            } else {
                emit viewportEntered();
            }
        }
    } else if (event->type() == QEvent::Leave) {
        m_hoverRow = m_hoverColumn = -1;
    }
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
    return QAbstractItemView::viewportEvent(event);
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
    if (row >= 0 && column >= 0) {
        emit cellPressed(row, column);
        if (QTableWidgetItem *it = item(row, column))
            emit itemPressed(it);
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
        QTableWidgetItem *hit = item(row, column);
        if (hit && hit->m_checkState >= 0 && (hit->flags() & Qt::ItemIsUserCheckable) &&
            (hit->flags() & Qt::ItemIsEnabled) && checkRect(row, column).adjusted(-2, -2, 2, 2).contains(event->pos()))
            hit->setCheckState(hit->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
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
        QTableWidgetItem *it = item(row, column);
        if (it)
            emit itemDoubleClicked(it);
        emit cellActivated(row, column);
        if (it)
            emit itemActivated(it);
        if (it && (it->flags() & Qt::ItemIsEditable) && (it->flags() & Qt::ItemIsEnabled) &&
            !checkRect(row, column).adjusted(-2, -2, 2, 2).contains(event->pos()))
            editItem(it);
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
        QAbstractItemView::keyPressEvent(event);
        return;
    }
    int dRow = 0, dColumn = 0;
    int targetRow = m_currentRow, targetColumn = m_currentColumn;
    const bool control = event->modifiers() & Qt::ControlModifier;
    const int pageRows = std::max(1, viewport()->height() / std::max(1, m_vheader->defaultSectionSize()) - 1);
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_currentRow >= 0 &&
        m_currentColumn >= 0 && !m_editor) {
        emit cellActivated(m_currentRow, m_currentColumn);
        if (QTableWidgetItem *it = currentItem())
            emit itemActivated(it);
        event->ignore(); // as in Qt, so that a dialog can click its default button
        return;
    }
    if (QTableWidgetItem *it = currentItem(); it && (it->flags() & Qt::ItemIsEnabled)) {
        if (event->key() == Qt::Key_F2 && (it->flags() & Qt::ItemIsEditable)) {
            editItem(it);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Space && it->m_checkState >= 0 && (it->flags() & Qt::ItemIsUserCheckable)) {
            it->setCheckState(it->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            event->accept();
            return;
        }
    }
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

namespace {

// Closes the editor of a table without keeping its text when Escape is pressed in it.
class EscapeFilter : public QObject {
public:
    explicit EscapeFilter(std::function<void()> cancel, QObject *parent)
        : QObject(parent), m_cancel(std::move(cancel)) {}
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_cancel();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_cancel;
};

} // namespace

/*!
    \since 6.0

    Returns the item that labels column \a column, or \c nullptr if there is none. setHorizontalHeaderLabels() gives
    each labelled column one, made when it is first asked for; change its text or tool tip to change the header.

    \sa setHorizontalHeaderItem(), verticalHeaderItem()
*/
QTableWidgetItem *QTableWidget::horizontalHeaderItem(int column) const { return headerItem(Qt::Horizontal, column); }

/*!
    \since 6.0

    Makes \a item label column \a column, and takes ownership of it. The item it replaces is deleted.
*/
void QTableWidget::setHorizontalHeaderItem(int column, QTableWidgetItem *item) {
    setHeaderItem(Qt::Horizontal, column, item);
}

/*!
    \since 6.0

    Returns the item that labels row \a row, or \c nullptr if there is none.

    \sa setVerticalHeaderItem(), horizontalHeaderItem()
*/
QTableWidgetItem *QTableWidget::verticalHeaderItem(int row) const { return headerItem(Qt::Vertical, row); }

/*!
    \since 6.0

    Makes \a item label row \a row, and takes ownership of it. The item it replaces is deleted.
*/
void QTableWidget::setVerticalHeaderItem(int row, QTableWidgetItem *item) { setHeaderItem(Qt::Vertical, row, item); }

std::vector<QTableWidgetItem *> &QTableWidget::headerItems(Qt::Orientation orientation) {
    return orientation == Qt::Horizontal ? m_hItems : m_vItems;
}

QTableWidgetItem *QTableWidget::headerItem(Qt::Orientation orientation, int section) const {
    const bool horizontal = orientation == Qt::Horizontal;
    QHeaderView *header = horizontal ? m_hheader : m_vheader;
    if (section < 0 || section >= header->count())
        return nullptr;
    auto *self = const_cast<QTableWidget *>(this);
    std::vector<QTableWidgetItem *> &items = self->headerItems(orientation);
    if (static_cast<std::size_t>(section) < items.size() && items[static_cast<std::size_t>(section)])
        return items[static_cast<std::size_t>(section)];
    // A section with a label has an item, as setHorizontalHeaderLabels() makes them in Qt.
    if (section >= header->m_labels.size() || header->m_labels.at(section).isEmpty())
        return nullptr;
    auto *item = new QTableWidgetItem(header->m_labels.at(section));
    self->setHeaderItem(orientation, section, item);
    return item;
}

void QTableWidget::setHeaderItem(Qt::Orientation orientation, int section, QTableWidgetItem *item) {
    QHeaderView *header = orientation == Qt::Horizontal ? m_hheader : m_vheader;
    if (section < 0 || section >= header->count())
        return;
    std::vector<QTableWidgetItem *> &items = headerItems(orientation);
    if (items.size() < static_cast<std::size_t>(header->count()))
        items.resize(static_cast<std::size_t>(header->count()), nullptr);
    QTableWidgetItem *&slot = items[static_cast<std::size_t>(section)];
    if (slot == item)
        return;
    if (slot) {
        slot->m_table = nullptr;
        delete slot;
    }
    slot = item;
    if (item) {
        if (item->m_table && item->m_table != this)
            item->m_table->forget(item);
        item->m_table = this;
        item->m_isHeader = true;
        item->m_header = orientation;
        item->m_row = orientation == Qt::Vertical ? section : -1;
        item->m_column = orientation == Qt::Horizontal ? section : -1;
        headerItemChanged(item);
    }
}

void QTableWidget::headerItemChanged(QTableWidgetItem *item) {
    QHeaderView *header = item->m_header == Qt::Horizontal ? m_hheader : m_vheader;
    const int section = item->m_header == Qt::Horizontal ? item->m_column : item->m_row;
    if (section < 0)
        return;
    QStringList labels = header->m_labels;
    while (labels.size() <= section)
        labels.append(QString());
    labels[section] = item->text();
    header->setLabels(labels);
    while (header->m_toolTips.size() <= section)
        header->m_toolTips.append(QString());
    header->m_toolTips[section] = item->toolTip();
    updateLayout();
}

/*!
    \fn void QTableWidget::removeCellWidget(int row, int column)
    \since 6.0

    Removes and deletes the widget shown in the cell at \a row and \a column.
*/

/*!
    \fn void QTableWidget::openPersistentEditor(QTableWidgetItem *item)
    \since 6.0

    Opens an editor over \a item, as editItem() does.
*/

/*!
    \since 6.0

    Returns the widget shown in the cell at \a row and \a column, or \c nullptr if it has none.

    \sa setCellWidget()
*/
QWidget *QTableWidget::cellWidget(int row, int column) const {
    for (const CellWidget &w : m_cellWidgets)
        if (w.row == row && w.column == column)
            return w.widget.data();
    return nullptr;
}

/*!
    \since 6.0

    Shows \a widget in the cell at \a row and \a column, over the item there, and takes ownership of it. A widget
    the cell had before is deleted. The widget stays in its cell when rows and columns are inserted or removed, and
    is deleted with its row or column. A \c nullptr \a widget removes the one the cell has.

    \sa cellWidget(), removeCellWidget()
*/
void QTableWidget::setCellWidget(int row, int column, QWidget *widget) {
    if (row < 0 || row >= m_rows || column < 0 || column >= m_columns)
        return;
    for (auto it = m_cellWidgets.begin(); it != m_cellWidgets.end(); ++it) {
        if (it->row == row && it->column == column) {
            if (it->widget && it->widget != widget)
                it->widget->deleteLater();
            m_cellWidgets.erase(it);
            break;
        }
    }
    if (!widget)
        return;
    widget->setParent(viewport());
    m_cellWidgets.push_back({row, column, widget});
    placeCellWidgets();
}

void QTableWidget::shiftCellWidgets(bool rows, int from, int delta) {
    for (auto it = m_cellWidgets.begin(); it != m_cellWidgets.end();) {
        int &at = rows ? it->row : it->column;
        if (delta < 0 && at >= from && at < from - delta) {
            if (it->widget)
                it->widget->deleteLater();
            it = m_cellWidgets.erase(it);
            continue;
        }
        if (at >= from)
            at += delta;
        ++it;
    }
    placeCellWidgets();
}

void QTableWidget::placeCellWidgets() {
    for (const CellWidget &w : m_cellWidgets) {
        if (!w.widget)
            continue;
        w.widget->setGeometry(cellRect(w.row, w.column).adjusted(0, 0, m_showGrid ? -1 : 0, m_showGrid ? -1 : 0));
        w.widget->show();
    }
}

QRect QTableWidget::checkRect(int row, int column) const {
    const QRect cell = cellRect(row, column);
    const int size = std::min(style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, this), cell.height() - 2);
    return QRect(cell.left() + textMargin, cell.top() + (cell.height() - size) / 2, size, size);
}

/*!
    \since 6.0

    Opens an editor over \a item, if it is in this table: a line edit with its text, which the item gets when
    Return is pressed or the editor loses the focus. Escape closes the editor without changing the item. A double
    click on an item with Qt::ItemIsEditable, or F2 on the current one, opens the editor too.
*/
void QTableWidget::editItem(QTableWidgetItem *item) {
    if (!item || item->m_table != this || item->m_isHeader)
        return;
    closeItemEditor(true);
    scrollToItem(item);
    auto *editor = new QLineEdit(viewport());
    editor->setText(item->text());
    editor->setFrame(false);
    editor->setGeometry(cellRect(item->m_row, item->m_column));
    editor->installEventFilter(new EscapeFilter([this] { closeItemEditor(false); }, editor));
    QObject::connect(editor, &QLineEdit::editingFinished, this, [this] { closeItemEditor(true); });
    m_editor = editor;
    m_editing = item;
    editor->show();
    editor->selectAll();
    editor->setFocus();
}

void QTableWidget::closeItemEditor(bool commit) {
    QPointer<QWidget> editor = m_editor;
    QTableWidgetItem *item = m_editing;
    m_editor = nullptr;
    m_editing = nullptr;
    if (!editor)
        return;
    if (commit && item)
        if (auto *line = qobject_cast<QLineEdit *>(editor.data()))
            item->setText(line->text());
    editor->hide();
    editor->deleteLater();
    if (hasFocus() || !QApplication::focusWidget() || QApplication::focusWidget() == editor)
        setFocus();
}

/*!
    \class QTableWidgetSelectionRange
    \inmodule NtWidgets
    \since 6.0
    \brief The QTableWidgetSelectionRange class is a block of cells of a QTableWidget.

    \differences
    A table selects one cell at a time, so QTableWidget::selectedRanges() has at most one range of one cell.
    \enddifferences

    \sa QTableWidget::selectedRanges()
*/

/*!
    \fn QTableWidgetSelectionRange::QTableWidgetSelectionRange()

    Constructs an empty range.
*/

/*!
    \fn QTableWidgetSelectionRange::QTableWidgetSelectionRange(int top, int left, int bottom, int right)

    Constructs the range of the cells from \a top to \a bottom and from \a left to \a right.
*/

/*!
    \fn int QTableWidgetSelectionRange::topRow() const

    Returns the first row.
*/

/*!
    \fn int QTableWidgetSelectionRange::bottomRow() const

    Returns the last row.
*/

/*!
    \fn int QTableWidgetSelectionRange::leftColumn() const

    Returns the first column.
*/

/*!
    \fn int QTableWidgetSelectionRange::rightColumn() const

    Returns the last column.
*/

/*!
    \fn int QTableWidgetSelectionRange::rowCount() const

    Returns the number of rows.
*/

/*!
    \fn int QTableWidgetSelectionRange::columnCount() const

    Returns the number of columns.
*/

/*!
    \since 6.0

    Returns the selected blocks of cells: the selected cell, as a block of one, or none.
*/
QList<QTableWidgetSelectionRange> QTableWidget::selectedRanges() const {
    if (selectionMode() == NoSelection || m_currentRow < 0 || m_currentColumn < 0)
        return {};
    return {QTableWidgetSelectionRange(m_currentRow, m_currentColumn, m_currentRow, m_currentColumn)};
}

/*!
    \since 6.0

    Selects the block of cells \a range if \a select is \c true: as one cell is selected at a time, its top left cell
    becomes the current cell. With \a select \c false the selection is cleared if it is in \a range.
*/
void QTableWidget::setRangeSelected(const QTableWidgetSelectionRange &range, bool select) {
    if (range.rowCount() <= 0 || range.columnCount() <= 0)
        return;
    if (select) {
        setCurrentCell(range.topRow(), range.leftColumn());
        return;
    }
    if (m_currentRow >= range.topRow() && m_currentRow <= range.bottomRow() && m_currentColumn >= range.leftColumn() &&
        m_currentColumn <= range.rightColumn())
        moveCurrent(-1, -1);
}

/*!
    \since 6.0

    Returns the row of \a item, or -1 if it is not in the table.
*/
int QTableWidget::row(const QTableWidgetItem *item) const {
    return item && item->m_table == this && !item->m_isHeader ? item->m_row : -1;
}

/*!
    \since 6.0

    Returns the column of \a item, or -1 if it is not in the table.
*/
int QTableWidget::column(const QTableWidgetItem *item) const {
    return item && item->m_table == this && !item->m_isHeader ? item->m_column : -1;
}

/*!
    \since 6.0

    Returns where the row \a logicalRow is shown, as the vertical header orders the rows.
*/
int QTableWidget::visualRow(int logicalRow) const { return m_vheader->visualIndex(logicalRow); }

/*!
    \since 6.0

    Returns where the column \a logicalColumn is shown, as the horizontal header orders the columns.
*/
int QTableWidget::visualColumn(int logicalColumn) const { return m_hheader->visualIndex(logicalColumn); }

/*!
    \since 6.0

    Returns the items whose text matches \a text the way \a flags says, row by row.
*/
QList<QTableWidgetItem *> QTableWidget::findItems(const QString &text, Qt::MatchFlags flags) const {
    const QtPrivate::ItemTextMatcher matches(text, flags);
    QList<QTableWidgetItem *> found;
    for (int r = 0; r < m_rows; ++r)
        for (int c = 0; c < m_columns; ++c)
            if (QTableWidgetItem *it = item(r, c); it && matches(it->text()))
                found.append(it);
    return found;
}

/*!
    \fn const QTableWidgetItem *QTableWidget::itemPrototype() const
    \since 6.0

    Returns the item that new items are made like, or \c nullptr.
*/

/*!
    \since 6.0

    Makes \a item the prototype of new items, and takes ownership of it; the old prototype is deleted. Nt keeps it,
    as the table does not make items itself.
*/
void QTableWidget::setItemPrototype(const QTableWidgetItem *item) {
    if (item == m_prototype)
        return;
    delete m_prototype;
    m_prototype = item;
}

QTableWidgetItem *QTableWidget::takeHeaderItem(Qt::Orientation orientation, int section) {
    std::vector<QTableWidgetItem *> &items = headerItems(orientation);
    if (section < 0 || static_cast<std::size_t>(section) >= items.size())
        return nullptr;
    QTableWidgetItem *item = items[static_cast<std::size_t>(section)];
    if (!item)
        return nullptr;
    items[static_cast<std::size_t>(section)] = nullptr;
    item->m_table = nullptr;
    QHeaderView *header = orientation == Qt::Horizontal ? m_hheader : m_vheader;
    QStringList labels = header->m_labels;
    if (section < labels.size()) {
        labels[section] = QString();
        header->setLabels(labels);
    }
    updateLayout();
    return item;
}

/*!
    \since 6.0

    Takes the header item of the column \a column out of the table and returns it, or \c nullptr. The caller owns it.
*/
QTableWidgetItem *QTableWidget::takeHorizontalHeaderItem(int column) { return takeHeaderItem(Qt::Horizontal, column); }

/*!
    \since 6.0

    Takes the header item of the row \a row out of the table and returns it, or \c nullptr. The caller owns it.
*/
QTableWidgetItem *QTableWidget::takeVerticalHeaderItem(int row) { return takeHeaderItem(Qt::Vertical, row); }

/*!
    \since 6.0

    Returns an invalid index: the table keeps its items without a model, so \a item has no model index.
*/
QModelIndex QTableWidget::indexFromItem(const QTableWidgetItem *item) const {
    (void)item;
    return QModelIndex();
}

/*!
    \since 6.0

    Returns the item at the row and column of \a index, or \c nullptr. The table has no model, so only the row and
    column of \a index are looked at.
*/
QTableWidgetItem *QTableWidget::itemFromIndex(const QModelIndex &index) const {
    return index.isValid() ? item(index.row(), index.column()) : nullptr;
}

/*!
    \fn Qt::DropActions QTableWidget::supportedDragActions() const
    \since 6.0

    Returns the actions a drag from the table allows.
*/

/*!
    \fn void QTableWidget::setSupportedDragActions(Qt::DropActions actions)
    \since 6.0

    Sets the actions a drag from the table allows to \a actions. Nt keeps the value.
*/

/*!
    \since 6.0

    Returns the MIME types that mimeData() makes.
*/
QStringList QTableWidget::mimeTypes() const { return {QStringLiteral("application/x-qabstractitemmodeldatalist")}; }

/*!
    \since 6.0

    Returns new data that stands for \a items: their texts, one per line, as plain text. The caller owns it.
*/
QMimeData *QTableWidget::mimeData(const QList<QTableWidgetItem *> &items) const {
    auto *data = new QMimeData;
    QStringList texts;
    for (const QTableWidgetItem *it : items)
        if (it)
            texts.append(it->text());
    data->setText(texts.join(QChar('\n')));
    return data;
}

/*!
    \since 6.0

    Handles \a data dropped with \a action on the cell at \a row and \a column, and returns \c true if it was taken.
    The default takes nothing; reimplement it to accept drops.
*/
bool QTableWidget::dropMimeData(int row, int column, const QMimeData *data, Qt::DropAction action) {
    (void)row;
    (void)column;
    (void)data;
    (void)action;
    return false;
}

/*!
    \since 6.0

    Returns the actions that drops on the table may take: Qt::CopyAction | Qt::MoveAction by default.
*/
Qt::DropActions QTableWidget::supportedDropActions() const { return Qt::CopyAction | Qt::MoveAction; }

/*!
    \since 6.0

    Returns the items of the table that \a data was made from; empty, as the items are not kept in MIME data.
*/
QList<QTableWidgetItem *> QTableWidget::items(const QMimeData *data) const {
    (void)data;
    return {};
}

/*!
    \enum QTableWidgetItem::ItemType
    \since 6.0

    The type of an item, which a subclass can set to tell its items apart.

    \value Type The type of a plain item.
    \value UserType The first type for items of an application.
*/

/*!
    \fn int QTableWidgetItem::type() const
    \since 6.0

    Returns the type the item was made with.
*/

/*!
    \fn QString QTableWidgetItem::statusTip() const
    \since 6.0

    Returns the status tip of the item.
*/

/*!
    \fn void QTableWidgetItem::setStatusTip(const QString &statusTip)
    \since 6.0

    Sets the status tip of the item to \a statusTip.
*/

/*!
    \fn QString QTableWidgetItem::whatsThis() const
    \since 6.0

    Returns the "What's This?" help of the item.
*/

/*!
    \fn void QTableWidgetItem::setWhatsThis(const QString &whatsThis)
    \since 6.0

    Sets the "What's This?" help of the item to \a whatsThis.
*/

/*!
    \fn QSize QTableWidgetItem::sizeHint() const
    \since 6.0

    Returns the size hint of the item, or an invalid size if it has none.
*/

/*!
    \fn void QTableWidgetItem::setSizeHint(const QSize &size)
    \since 6.0

    Sets the size hint of the item to \a size.
*/

/*!
    \since 6.0

    Returns a new item, without a table, with the same data and flags as this one.
*/
QTableWidgetItem *QTableWidgetItem::clone() const {
    auto *copy = new QTableWidgetItem(m_icon, m_text, m_type);
    copy->m_toolTip = m_toolTip;
    copy->m_alignment = m_alignment;
    copy->m_foreground = m_foreground;
    copy->m_background = m_background;
    copy->m_font = m_font;
    copy->m_hasFont = m_hasFont;
    copy->m_flags = m_flags;
    copy->m_checkState = m_checkState;
    copy->m_data = m_data;
    return copy;
}

/*!
    \since 6.0

    Reads the data of the item from \a in, as write() wrote it.
*/
void QTableWidgetItem::read(QDataStream &in) {
    quint32 count = 0;
    in >> count;
    for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        qint32 role = 0;
        QString value;
        in >> role >> value;
        setData(role, value);
    }
}

/*!
    \since 6.0

    Writes the data of the item to \a out: the text, the tool tip and the roles whose value is a string. Other
    values, such as icons, are not written.
*/
void QTableWidgetItem::write(QDataStream &out) const {
    QList<std::pair<int, QString>> values;
    if (!m_text.isEmpty())
        values.append({Qt::DisplayRole, m_text});
    if (!m_toolTip.isEmpty())
        values.append({Qt::ToolTipRole, m_toolTip});
    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it)
        if (it.value().typeId() == QMetaType::QString)
            values.append({it.key(), it.value().toString()});
    out << static_cast<quint32>(values.size());
    for (const auto &[role, value] : values)
        out << static_cast<qint32>(role) << value;
}

#include "moc_qtablewidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
