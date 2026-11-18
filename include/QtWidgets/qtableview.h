// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractitemview.h>

#include <vector>

class QHeaderView;

class QTableView : public QAbstractItemView {
    Q_OBJECT
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid)
    Q_PROPERTY(Qt::PenStyle gridStyle READ gridStyle WRITE setGridStyle)
    Q_PROPERTY(bool sortingEnabled READ isSortingEnabled WRITE setSortingEnabled)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(bool cornerButtonEnabled READ isCornerButtonEnabled WRITE setCornerButtonEnabled)
public:
    explicit QTableView(QWidget *parent = nullptr);
    ~QTableView() override;

    void setModel(QAbstractItemModel *model) override;
    void setRootIndex(const QModelIndex &index) override;

    QHeaderView *horizontalHeader() const { return m_hheader; }
    QHeaderView *verticalHeader() const { return m_vheader; }
    void setHorizontalHeader(QHeaderView *header);
    void setVerticalHeader(QHeaderView *header);
    void setSpan(int row, int column, int rowSpan, int columnSpan);
    int rowSpan(int row, int column) const;
    int columnSpan(int row, int column) const;
    void clearSpans();

    bool showGrid() const { return m_showGrid; }
    Qt::PenStyle gridStyle() const { return m_gridStyle; }
    void setGridStyle(Qt::PenStyle style);
    bool isSortingEnabled() const { return m_sorting; }
    void setSortingEnabled(bool enable);
    bool wordWrap() const { return m_wordWrap; }
    void setWordWrap(bool on);
    bool isCornerButtonEnabled() const { return m_cornerEnabled; }
    void setCornerButtonEnabled(bool enable);

    int rowViewportPosition(int row) const;
    int rowAt(int y) const;
    int rowHeight(int row) const;
    void setRowHeight(int row, int height);
    int columnViewportPosition(int column) const;
    int columnAt(int x) const;
    int columnWidth(int column) const;
    void setColumnWidth(int column, int width);

    bool isRowHidden(int row) const;
    void setRowHidden(int row, bool hide);
    bool isColumnHidden(int column) const;
    void setColumnHidden(int column, bool hide);

    QRect visualRect(const QModelIndex &index) const override;
    void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint &point) const override;

    void reset() override;
    void selectAll() override;
    QSize sizeHint() const override;

public slots:
    void setShowGrid(bool show);
    void selectRow(int row);
    void selectColumn(int column);
    void hideRow(int row) { setRowHidden(row, true); }
    void hideColumn(int column) { setColumnHidden(column, true); }
    void showRow(int row) { setRowHidden(row, false); }
    void showColumn(int column) { setColumnHidden(column, false); }
    void resizeRowToContents(int row);
    void resizeRowsToContents();
    void resizeColumnToContents(int column);
    void resizeColumnsToContents();
    void sortByColumn(int column, Qt::SortOrder order);

protected slots:
    void rowMoved(int row, int oldIndex, int newIndex);
    void columnMoved(int column, int oldIndex, int newIndex);
    void rowResized(int row, int oldHeight, int newHeight);
    void columnResized(int column, int oldWidth, int newWidth);
    void rowCountChanged(int oldCount, int newCount);
    void columnCountChanged(int oldCount, int newCount);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    bool viewportEvent(QEvent *event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    void initViewItemOption(QStyleOptionViewItem *option) const override;
    void updateGeometries() override;
    void rowsInserted(const QModelIndex &parent, int start, int end) override;
    void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;

private:
    void syncHeaders();
    void connectHeader(QHeaderView *header);
    struct Span {
        int row;
        int column;
        int rowSpan;
        int columnSpan;
    };
    std::vector<Span> m_spans;
    const Span *spanAt(int row, int column) const; // the span that covers the cell, or null
    QRect spanRect(const Span &span) const;        // on the viewport
    void syncLabels(Qt::Orientation orientation);
    int visibleNeighbour(const QHeaderView *header, int from, int step) const;
    int firstVisible(const QHeaderView *header) const;
    int lastVisible(const QHeaderView *header) const;
    void selectWith(const QModelIndex &index, Qt::KeyboardModifiers modifiers, bool keyboard);
    void selectRect(const QModelIndex &from, const QModelIndex &to);
    QItemSelectionModel::SelectionFlags behaviorFlags() const;

    QHeaderView *m_hheader;
    QHeaderView *m_vheader;
    QWidget *m_corner;
    std::vector<QMetaObject::Connection> m_connections;
    QPersistentModelIndex m_anchor;
    QPersistentModelIndex m_pressed;
    Qt::PenStyle m_gridStyle = Qt::SolidLine;
    bool m_showGrid = true;
    bool m_sorting = false;
    bool m_wordWrap = true;
    bool m_cornerEnabled = true;
    bool m_pressedWasSelected = false;
    bool m_dragging = false;
    bool m_inLayout = false;
    bool m_layoutAgain = false;
};
