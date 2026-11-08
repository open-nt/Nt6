// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtWidgets/qabstractitemview.h>

#include <vector>

class QHeaderView;
class QTableWidget;

class QTableWidgetItem {
public:
    QTableWidgetItem();
    explicit QTableWidgetItem(const QString &text);
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
    Qt::ItemFlags m_flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
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
    QHeaderView *horizontalHeader() const { return m_hheader; }
    QHeaderView *verticalHeader() const { return m_vheader; }
    int rowHeight(int row) const;
    void setRowHeight(int row, int height);
    int columnWidth(int column) const;
    void setColumnWidth(int column, int width);
    void resizeColumnToContents(int column);
    void resizeColumnsToContents();

    int currentRow() const { return m_currentRow; }
    int currentColumn() const { return m_currentColumn; }
    QTableWidgetItem *currentItem() const;
    void setCurrentCell(int row, int column);
    void setCurrentItem(QTableWidgetItem *item);
    QList<QTableWidgetItem *> selectedItems() const;
    void scrollToItem(const QTableWidgetItem *item);

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);

public slots:
    void clear();
    void clearContents();

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

protected:
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
    void itemChanged(QTableWidgetItem *item);
    void forget(QTableWidgetItem *item);
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
    bool m_inLayout = false;
    bool m_layoutAgain = false;
};
