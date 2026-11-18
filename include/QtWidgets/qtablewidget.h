// MIT License - see LICENSE
#pragma once

#include <QtCore/qvariant.h>

#include <QtCore/qpointer.h>

#include <QtCore/qmap.h>
#include <QtGui/qicon.h>

#include <QtCore/qlist.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtWidgets/qabstractitemview.h>

#include <vector>

class QHeaderView;
class QTableWidget;

class QTableWidgetSelectionRange {
public:
    QTableWidgetSelectionRange() = default;
    QTableWidgetSelectionRange(int top, int left, int bottom, int right)
        : m_top(top), m_left(left), m_bottom(bottom), m_right(right) {}

    int topRow() const { return m_top; }
    int bottomRow() const { return m_bottom; }
    int leftColumn() const { return m_left; }
    int rightColumn() const { return m_right; }
    int rowCount() const { return m_top < 0 ? 0 : m_bottom - m_top + 1; }
    int columnCount() const { return m_left < 0 ? 0 : m_right - m_left + 1; }

    friend bool operator==(const QTableWidgetSelectionRange &a, const QTableWidgetSelectionRange &b) {
        return a.m_top == b.m_top && a.m_left == b.m_left && a.m_bottom == b.m_bottom && a.m_right == b.m_right;
    }
    friend bool operator!=(const QTableWidgetSelectionRange &a, const QTableWidgetSelectionRange &b) {
        return !(a == b);
    }

private:
    int m_top = -1;
    int m_left = -1;
    int m_bottom = -2;
    int m_right = -2;
};

class QTableWidgetItem {
public:
    QTableWidgetItem();
    explicit QTableWidgetItem(const QString &text);
    QTableWidgetItem(const QIcon &icon, const QString &text);
    QTableWidgetItem(const QTableWidgetItem &) = delete;
    QTableWidgetItem &operator=(const QTableWidgetItem &) = delete;
    virtual ~QTableWidgetItem();

    QTableWidget *tableWidget() const { return m_table; }
    int row() const { return m_row; }
    int column() const { return m_column; }
    QString text() const { return m_text; }
    void setText(const QString &text);
    QString toolTip() const { return m_toolTip; }
    void setToolTip(const QString &toolTip) { m_toolTip = toolTip; }
    Qt::Alignment textAlignment() const { return m_alignment; }
    void setTextAlignment(Qt::Alignment alignment);
    QBrush foreground() const { return m_foreground; }
    void setForeground(const QBrush &brush);
    QBrush background() const { return m_background; }
    void setBackground(const QBrush &brush);
    QFont font() const;
    void setFont(const QFont &font);
    Qt::ItemFlags flags() const { return m_flags; }
    void setFlags(Qt::ItemFlags flags);
    bool isSelected() const;
    void setSelected(bool select);
    QIcon icon() const { return m_icon; }
    void setIcon(const QIcon &icon);
    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);
    virtual QVariant data(int role) const;
    virtual void setData(int role, const QVariant &value);
    virtual bool operator<(const QTableWidgetItem &other) const;

private:
    friend class QTableWidget;
    void changed();

    QTableWidget *m_table = nullptr;
    int m_row = -1;
    int m_column = -1;
    QString m_text;
    QString m_toolTip;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    QBrush m_foreground;
    QBrush m_background;
    QFont m_font;
    bool m_hasFont = false;
    Qt::ItemFlags m_flags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    int m_checkState = -1;      // a Qt::CheckState once one is set; -1 for an item without a check box
    QMap<int, QVariant> m_data; // the roles that have no member of their own
    QIcon m_icon;
    Qt::Orientation m_header = {}; // for a header item: the header it labels
    bool m_isHeader = false;
};

class QTableWidget : public QAbstractItemView {
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount WRITE setRowCount)
    Q_PROPERTY(int columnCount READ columnCount WRITE setColumnCount)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid)
public:
    explicit QTableWidget(QWidget *parent = nullptr);
    QTableWidget(int rows, int columns, QWidget *parent = nullptr);
    ~QTableWidget() override;

    int rowCount() const;
    void setRowCount(int rows);
    int columnCount() const;
    void setColumnCount(int columns);
    void insertRow(int row);
    void removeRow(int row);
    void insertColumn(int column);
    void removeColumn(int column);

    QTableWidgetItem *item(int row, int column) const;
    void setItem(int row, int column, QTableWidgetItem *item);
    QTableWidgetItem *takeItem(int row, int column);
    QTableWidgetItem *itemAt(const QPoint &pos) const;
    QTableWidgetItem *itemAt(int x, int y) const { return itemAt(QPoint(x, y)); }
    QRect visualItemRect(const QTableWidgetItem *item) const;
    int rowAt(int y) const;
    int columnAt(int x) const;

    void setHorizontalHeaderLabels(const QStringList &labels);
    void setVerticalHeaderLabels(const QStringList &labels);
    QTableWidgetItem *horizontalHeaderItem(int column) const;
    void setHorizontalHeaderItem(int column, QTableWidgetItem *item);
    QTableWidgetItem *verticalHeaderItem(int row) const;
    void setVerticalHeaderItem(int row, QTableWidgetItem *item);
    QWidget *cellWidget(int row, int column) const;
    void setCellWidget(int row, int column, QWidget *widget);
    void removeCellWidget(int row, int column) { setCellWidget(row, column, nullptr); }
    void editItem(QTableWidgetItem *item);
    void openPersistentEditor(QTableWidgetItem *item) { editItem(item); }
    QHeaderView *horizontalHeader() const { return m_hheader; }
    QHeaderView *verticalHeader() const { return m_vheader; }
    int rowHeight(int row) const;
    void setRowHeight(int row, int height);
    int columnWidth(int column) const;
    void setColumnWidth(int column, int width);
    void resizeColumnToContents(int column);
    void resizeColumnsToContents();
    void resizeRowToContents(int row);
    void resizeRowsToContents();
    bool isSortingEnabled() const { return m_sortingEnabled; }
    void setSortingEnabled(bool enable);
    void sortItems(int column, Qt::SortOrder order = Qt::AscendingOrder);
    void sortByColumn(int column, Qt::SortOrder order) { sortItems(column, order); }
    bool wordWrap() const { return m_wordWrap; }
    void setWordWrap(bool on);
    bool isRowHidden(int row) const;
    void setRowHidden(int row, bool hide);
    bool isColumnHidden(int column) const;
    void setColumnHidden(int column, bool hide);

    int currentRow() const { return m_currentRow; }
    int currentColumn() const { return m_currentColumn; }
    QTableWidgetItem *currentItem() const;
    void setCurrentCell(int row, int column);
    void setCurrentItem(QTableWidgetItem *item);
    QList<QTableWidgetItem *> selectedItems() const;
    QList<QTableWidgetSelectionRange> selectedRanges() const;
    void setRangeSelected(const QTableWidgetSelectionRange &range, bool select);
    void scrollToItem(const QTableWidgetItem *item);
    int row(const QTableWidgetItem *item) const;
    int column(const QTableWidgetItem *item) const;
    int visualRow(int logicalRow) const;
    int visualColumn(int logicalColumn) const;
    QList<QTableWidgetItem *> findItems(const QString &text, Qt::MatchFlags flags) const;
    const QTableWidgetItem *itemPrototype() const { return m_prototype; }
    void setItemPrototype(const QTableWidgetItem *item);
    QTableWidgetItem *takeHorizontalHeaderItem(int column);
    QTableWidgetItem *takeVerticalHeaderItem(int row);
    QModelIndex indexFromItem(const QTableWidgetItem *item) const;
    QTableWidgetItem *itemFromIndex(const QModelIndex &index) const;
    Qt::DropActions supportedDragActions() const { return m_dragActions; }
    void setSupportedDragActions(Qt::DropActions actions) { m_dragActions = actions; }

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);

public slots:
    void clear();
    void clearContents();
    void selectRow(int row);
    void selectColumn(int column);
    void hideRow(int row) { setRowHidden(row, true); }
    void showRow(int row) { setRowHidden(row, false); }
    void hideColumn(int column) { setColumnHidden(column, true); }
    void showColumn(int column) { setColumnHidden(column, false); }

signals:
    /*!
        This signal is emitted when the user clicks the cell at \a row and \a column.
    */
    void cellClicked(int row, int column) { emitSignal(&QTableWidget::cellClicked, row, column); }
    /*!
        This signal is emitted when the user double-clicks the cell at \a row and \a column.
    */
    void cellDoubleClicked(int row, int column) { emitSignal(&QTableWidget::cellDoubleClicked, row, column); }
    /*!
        This signal is emitted when the current cell changes from (\a previousRow, \a previousColumn) to
        (\a currentRow, \a currentColumn). A cell that does not exist is -1.
    */
    void currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn) {
        emitSignal(&QTableWidget::currentCellChanged, currentRow, currentColumn, previousRow, previousColumn);
    }
    /*!
        This signal is emitted when the user clicks \a item.
    */
    void itemClicked(QTableWidgetItem *item) { emitSignal(&QTableWidget::itemClicked, item); }
    /*!
        This signal is emitted when the selection changes.
    */
    void itemSelectionChanged() { emitSignal(&QTableWidget::itemSelectionChanged); }
    /*!
        \since 6.0
        This signal is emitted when the data of \a item changes: its text, its check state, or another role.
    */
    void itemChanged(QTableWidgetItem *item) { emitSignal(&QTableWidget::itemChanged, item); }
    /*!
        \since 6.0
        This signal is emitted when the data of the item in the cell at \a row and \a column changes.
    */
    void cellChanged(int row, int column) { emitSignal(&QTableWidget::cellChanged, row, column); }
    /*!
        \since 6.0
        This signal is emitted when the user presses a mouse button on the cell at \a row and \a column.
    */
    void cellPressed(int row, int column) { emitSignal(&QTableWidget::cellPressed, row, column); }
    /*!
        \since 6.0
        This signal is emitted when the mouse enters the cell at \a row and \a column, while mouse tracking is on.
    */
    void cellEntered(int row, int column) { emitSignal(&QTableWidget::cellEntered, row, column); }
    /*!
        \since 6.0
        This signal is emitted when the user activates the cell at \a row and \a column: double-clicks it, or
        presses Return or Enter while it is current.
    */
    void cellActivated(int row, int column) { emitSignal(&QTableWidget::cellActivated, row, column); }
    /*!
        \since 6.0
        This signal is emitted when the user presses a mouse button on \a item.
    */
    void itemPressed(QTableWidgetItem *item) { emitSignal(&QTableWidget::itemPressed, item); }
    /*!
        \since 6.0
        This signal is emitted when the mouse enters \a item, while mouse tracking is on.
    */
    void itemEntered(QTableWidgetItem *item) { emitSignal(&QTableWidget::itemEntered, item); }
    /*!
        \since 6.0
        This signal is emitted when the user activates \a item.
    */
    void itemActivated(QTableWidgetItem *item) { emitSignal(&QTableWidget::itemActivated, item); }
    /*!
        \since 6.0
        This signal is emitted when the user double-clicks \a item.
    */
    void itemDoubleClicked(QTableWidgetItem *item) { emitSignal(&QTableWidget::itemDoubleClicked, item); }
    /*!
        \since 6.0
        This signal is emitted when the current item changes from \a previous to \a current, either of which may be
        \c nullptr.
    */
    void currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous) {
        emitSignal(&QTableWidget::currentItemChanged, current, previous);
    }

protected:
    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QList<QTableWidgetItem *> &items) const;
    virtual bool dropMimeData(int row, int column, const QMimeData *data, Qt::DropAction action);
    virtual Qt::DropActions supportedDropActions() const;
    QList<QTableWidgetItem *> items(const QMimeData *data) const;
    bool viewportEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    void selectionModeChanged() override;
    QSize viewportSizeHint() const override;

private:
    friend class QTableWidgetItem;
    void init(int rows, int columns);
    void reindex();
    void updateLayout();
    void updateScrollBars();
    void refreshItem(QTableWidgetItem *item);
    void forget(QTableWidgetItem *item);
    // Cell widgets keep their cell when rows or columns are inserted or removed before them.
    struct CellWidget {
        int row;
        int column;
        QPointer<QWidget> widget;
    };
    void shiftCellWidgets(bool rows, int from, int delta);
    void placeCellWidgets();
    std::vector<QTableWidgetItem *> &headerItems(Qt::Orientation orientation);
    QTableWidgetItem *headerItem(Qt::Orientation orientation, int section) const;
    void setHeaderItem(Qt::Orientation orientation, int section, QTableWidgetItem *item);
    void headerItemChanged(QTableWidgetItem *item);
    QRect checkRect(int row, int column) const;
    void closeItemEditor(bool commit);
    void moveCurrent(int row, int column);
    bool selectable(int row, int column) const;
    void scrollToCell(int row, int column);
    QRect cellRect(int row, int column) const;

    std::vector<std::vector<QTableWidgetItem *>> m_cells;
    QHeaderView *m_hheader = nullptr;
    QHeaderView *m_vheader = nullptr;
    QWidget *m_corner = nullptr;
    int m_rows = 0;
    int m_columns = 0;
    int m_currentRow = -1;
    int m_currentColumn = -1;
    int m_scrollRow = -1;
    int m_scrollColumn = -1;
    int m_pressedRow = -1;
    int m_pressedColumn = -1;
    bool m_showGrid = true;
    bool m_sortingEnabled = false;
    bool m_wordWrap = false;
    bool m_inLayout = false;
    bool m_layoutAgain = false;
    std::vector<CellWidget> m_cellWidgets;
    std::vector<QTableWidgetItem *> m_hItems; // header items, owned; null for sections without one
    std::vector<QTableWidgetItem *> m_vItems;
    QPointer<QWidget> m_editor;
    QTableWidgetItem *m_editing = nullptr;
    const QTableWidgetItem *m_prototype = nullptr; // owned
    Qt::DropActions m_dragActions = Qt::CopyAction;
    int m_hoverRow = -1; // the cell the mouse is over, for cellEntered()
    int m_hoverColumn = -1;
    QTableWidgetItem *takeHeaderItem(Qt::Orientation orientation, int section);
};
