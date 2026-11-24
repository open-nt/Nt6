// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractitemview.h>

#include <vector>

class QListView : public QAbstractItemView {
    Q_OBJECT
    Q_PROPERTY(Movement movement READ movement WRITE setMovement)
    Q_PROPERTY(Flow flow READ flow WRITE setFlow)
    Q_PROPERTY(bool isWrapping READ isWrapping WRITE setWrapping)
    Q_PROPERTY(ResizeMode resizeMode READ resizeMode WRITE setResizeMode)
    Q_PROPERTY(LayoutMode layoutMode READ layoutMode WRITE setLayoutMode)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing)
    Q_PROPERTY(QSize gridSize READ gridSize WRITE setGridSize)
    Q_PROPERTY(ViewMode viewMode READ viewMode WRITE setViewMode)
    Q_PROPERTY(int modelColumn READ modelColumn WRITE setModelColumn)
    Q_PROPERTY(bool uniformItemSizes READ uniformItemSizes WRITE setUniformItemSizes)
    Q_PROPERTY(int batchSize READ batchSize WRITE setBatchSize)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(bool selectionRectVisible READ isSelectionRectVisible WRITE setSelectionRectVisible)
    Q_PROPERTY(Qt::Alignment itemAlignment READ itemAlignment WRITE setItemAlignment)
public:
    enum Movement { Static, Free, Snap };
    Q_ENUM(Movement)
    enum Flow { LeftToRight, TopToBottom };
    Q_ENUM(Flow)
    enum ResizeMode { Fixed, Adjust };
    Q_ENUM(ResizeMode)
    enum LayoutMode { SinglePass, Batched };
    Q_ENUM(LayoutMode)
    enum ViewMode { ListMode, IconMode };
    Q_ENUM(ViewMode)

    explicit QListView(QWidget *parent = nullptr);
    ~QListView() override;

    void setModel(QAbstractItemModel *model) override;
    void setRootIndex(const QModelIndex &index) override;

    Movement movement() const { return m_movement; }
    void setMovement(Movement movement);
    Flow flow() const { return m_flow; }
    void setFlow(Flow flow);
    bool isWrapping() const { return m_wrapping; }
    void setWrapping(bool enable);
    ResizeMode resizeMode() const { return m_resizeMode; }
    void setResizeMode(ResizeMode mode);
    LayoutMode layoutMode() const { return m_layoutMode; }
    void setLayoutMode(LayoutMode mode);
    int spacing() const { return m_spacing; }
    void setSpacing(int space);
    int batchSize() const { return m_batchSize; }
    void setBatchSize(int batchSize);
    QSize gridSize() const { return m_gridSize; }
    void setGridSize(const QSize &size);
    ViewMode viewMode() const { return m_viewMode; }
    void setViewMode(ViewMode mode);
    int modelColumn() const { return m_modelColumn; }
    void setModelColumn(int column);
    bool uniformItemSizes() const { return m_uniform; }
    void setUniformItemSizes(bool enable);
    bool wordWrap() const { return m_wordWrap; }
    void setWordWrap(bool on);
    bool isSelectionRectVisible() const { return m_selectionRectVisible; }
    void setSelectionRectVisible(bool show);
    Qt::Alignment itemAlignment() const { return m_itemAlignment; }
    void setItemAlignment(Qt::Alignment alignment);

    bool isRowHidden(int row) const;
    void setRowHidden(int row, bool hide);
    void clearPropertyFlags();

    QRect visualRect(const QModelIndex &index) const override;
    void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint &point) const override;

    void reset() override;
    void selectAll() override;
    QSize sizeHint() const override;

protected:
    QSize contentsSize() const;
    QRect rectForIndex(const QModelIndex &index) const;
    void setPositionForIndex(const QPoint &position, const QModelIndex &index);
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
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                     const QList<int> &roles = QList<int>()) override;
    void rowsInserted(const QModelIndex &parent, int start, int end) override;
    void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;

private:
    struct Item {
        QModelIndex index; // laid out again whenever rows come, go or move
        QRect rect;        // in the coordinates of the contents, before scrolling
    };
    void doLayout();
    void setRanges();
    bool scrollsVertically() const;
    int itemAt(const QPoint &contentsPoint) const;
    int positionOf(const QModelIndex &index) const;
    QModelIndex itemIndex(int position) const;
    QModelIndex nearest(int from, CursorAction direction) const;
    void selectWith(const QModelIndex &index, Qt::KeyboardModifiers modifiers, bool keyboard);
    void selectRange(int fromPosition, int toPosition);
    QPoint offset() const;

    std::vector<Item> m_items; // the items that show, in the order of their rows
    QList<QPersistentModelIndex> m_hidden;
    std::vector<std::pair<QPersistentModelIndex, QPoint>> m_placed; // items put where the application wanted
    std::vector<QMetaObject::Connection> m_connections;
    QPersistentModelIndex m_anchor;
    QPersistentModelIndex m_pressed;
    QSize m_contents;
    QSize m_gridSize;
    Movement m_movement = Static;
    Flow m_flow = TopToBottom;
    ResizeMode m_resizeMode = Fixed;
    LayoutMode m_layoutMode = SinglePass;
    ViewMode m_viewMode = ListMode;
    Qt::Alignment m_itemAlignment;
    int m_spacing = 0;
    int m_batchSize = 100;
    int m_modelColumn = 0;
    bool m_wrapping = false;
    bool m_uniform = false;
    bool m_wordWrap = false;
    bool m_selectionRectVisible = false;
    bool m_pressedWasSelected = false;
    bool m_dragging = false;
    bool m_inLayout = false;
    bool m_layoutAgain = false;
};
