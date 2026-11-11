// MIT License - see LICENSE
#pragma once

#include <QtCore/qset.h>
#include <QtWidgets/qabstractitemview.h>

#include <vector>

class QHeaderView;

class QTreeView : public QAbstractItemView {
    Q_OBJECT
    Q_PROPERTY(int indentation READ indentation WRITE setIndentation RESET resetIndentation)
    Q_PROPERTY(bool rootIsDecorated READ rootIsDecorated WRITE setRootIsDecorated)
    Q_PROPERTY(bool uniformRowHeights READ uniformRowHeights WRITE setUniformRowHeights)
    Q_PROPERTY(bool itemsExpandable READ itemsExpandable WRITE setItemsExpandable)
    Q_PROPERTY(bool sortingEnabled READ isSortingEnabled WRITE setSortingEnabled)
    Q_PROPERTY(bool headerHidden READ isHeaderHidden WRITE setHeaderHidden)
    Q_PROPERTY(bool expandsOnDoubleClick READ expandsOnDoubleClick WRITE setExpandsOnDoubleClick)
public:
    explicit QTreeView(QWidget *parent = nullptr);
    ~QTreeView() override;

    void setModel(QAbstractItemModel *model) override;
    void setRootIndex(const QModelIndex &index) override;

    QHeaderView *header() const { return m_header; }

    int indentation() const { return m_indentation; }
    void setIndentation(int i);
    void resetIndentation();
    bool rootIsDecorated() const { return m_rootDecorated; }
    void setRootIsDecorated(bool show);
    bool uniformRowHeights() const { return m_uniform; }
    void setUniformRowHeights(bool uniform);
    bool itemsExpandable() const { return m_expandable; }
    void setItemsExpandable(bool enable);
    bool expandsOnDoubleClick() const { return m_expandOnDoubleClick; }
    void setExpandsOnDoubleClick(bool enable);
    bool isHeaderHidden() const;
    void setHeaderHidden(bool hide);
    bool isSortingEnabled() const { return m_sorting; }
    void setSortingEnabled(bool enable);

    int columnViewportPosition(int column) const;
    int columnWidth(int column) const;
    void setColumnWidth(int column, int width);
    int columnAt(int x) const;

    bool isExpanded(const QModelIndex &index) const;
    void setExpanded(const QModelIndex &index, bool expand);

    QRect visualRect(const QModelIndex &index) const override;
    void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint &point) const override;
    QModelIndex indexAbove(const QModelIndex &index) const;
    QModelIndex indexBelow(const QModelIndex &index) const;

    void reset() override;
    void selectAll() override;
    void sortByColumn(int column, Qt::SortOrder order);

    QSize sizeHint() const override;

public slots:
    void expand(const QModelIndex &index);
    void collapse(const QModelIndex &index);
    void expandAll();
    void collapseAll();
    void expandToDepth(int depth);
    void expandRecursively(const QModelIndex &index, int depth = -1);
    void resizeColumnToContents(int column);

signals:
    /*!
        This signal is emitted when the item \a index was expanded, by the user or by the program.
    */
    void expanded(const QModelIndex &index) { emitSignal(&QTreeView::expanded, index); }
    /*!
        This signal is emitted when the item \a index was collapsed.
    */
    void collapsed(const QModelIndex &index) { emitSignal(&QTreeView::collapsed, index); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    bool viewportEvent(QEvent *event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    void rowsInserted(const QModelIndex &parent, int start, int end) override;
    void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;
    int rowHeight(const QModelIndex &index) const;
    virtual void drawRow(QPainter *painter, const QStyleOptionViewItem &options, const QModelIndex &index) const;
    virtual void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const;

private:
    struct Row {
        QPersistentModelIndex index; // in column 0
        int level = 0;
        bool hasChildren = false;
        bool expanded = false;
    };
    void rebuild();
    void appendRows(const QModelIndex &parent, int level, const QSet<QModelIndex> &open);
    bool isOpen(const QModelIndex &first) const;
    bool close(const QModelIndex &first);
    void updateGeometries();
    void syncHeader();
    int rowOf(const QModelIndex &index) const;
    int rowAtY(int y) const;
    int itemHeight() const;
    int indentOf(int level) const;
    int contentWidth() const;
    void toggle(int row);
    void selectWith(const QModelIndex &index, Qt::KeyboardModifiers modifiers, bool keyboard);
    void selectRange(int fromRow, int toRow);
    QItemSelectionModel::SelectionFlags behaviorFlags() const;

    QHeaderView *m_header;
    std::vector<Row> m_rows; // the items that are laid out, from top to bottom
    // Not a set: a persistent index changes, and with it its hash, when rows move.
    QList<QPersistentModelIndex> m_expanded;
    std::vector<QMetaObject::Connection> m_connections;
    QPersistentModelIndex m_anchor;  // where a range selection starts
    QPersistentModelIndex m_pressed; // the item under the press that is going on
    int m_indentation = 20;
    bool m_rootDecorated = true;
    bool m_uniform = false;
    bool m_expandable = true;
    bool m_expandOnDoubleClick = true;
    bool m_sorting = false;
    bool m_pressedWasSelected = false;
    bool m_inLayout = false;
    bool m_layoutAgain = false;
};
