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
#include <QtWidgets/qlistview.h>

#include <vector>

class QListWidget;
class QMimeData;

class QListWidgetItem {
public:
    enum ItemType { Type = 0, UserType = 1000 };

    explicit QListWidgetItem(QListWidget *listview = nullptr, int type = Type);
    explicit QListWidgetItem(const QString &text, QListWidget *listview = nullptr, int type = Type);
    QListWidgetItem(const QIcon &icon, const QString &text, QListWidget *listview = nullptr, int type = Type);
    QListWidgetItem(const QListWidgetItem &) = delete;
    QListWidgetItem &operator=(const QListWidgetItem &) = delete;
    virtual ~QListWidgetItem();

    QListWidget *listWidget() const { return m_list; }
    QString text() const { return m_text; }
    void setText(const QString &text);
    QIcon icon() const { return m_icon; }
    void setIcon(const QIcon &icon);
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
    bool isSelected() const { return m_selected; }
    void setSelected(bool select);
    QSize sizeHint() const { return m_sizeHint; }
    void setSizeHint(const QSize &size);
    bool isHidden() const { return m_hidden; }
    void setHidden(bool hide);
    int type() const { return m_type; }
    virtual QVariant data(int role) const;
    virtual void setData(int role, const QVariant &value);

private:
    friend class QListWidget;
    void changed();

    QListWidget *m_list = nullptr;
    QString m_text;
    QString m_toolTip;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    QBrush m_foreground;
    QBrush m_background;
    QFont m_font;
    bool m_hasFont = false;
    bool m_selected = false;
    Qt::ItemFlags m_flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    QSize m_sizeHint;
    QIcon m_icon;
    bool m_hidden = false;
    int m_type = Type;
    QMap<int, QVariant> m_data; // the roles that have no member of their own
    QPointer<QWidget> m_widget; // shown over the item, owned by the list
};

class QListWidget : public QAbstractItemView {
    Q_OBJECT
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged)
    Q_PROPERTY(bool sortingEnabled READ isSortingEnabled WRITE setSortingEnabled)
public:
    explicit QListWidget(QWidget *parent = nullptr);
    ~QListWidget() override;

    int count() const { return static_cast<int>(m_items.size()); }
    QListWidgetItem *item(int row) const;
    int row(const QListWidgetItem *item) const;
    void addItem(QListWidgetItem *item);
    void addItem(const QString &text);
    void addItems(const QStringList &labels);
    void insertItem(int row, QListWidgetItem *item);
    void insertItem(int row, const QString &text);
    void insertItems(int row, const QStringList &labels);
    QListWidgetItem *takeItem(int row);
    void sortItems(Qt::SortOrder order = Qt::AscendingOrder);

    QListWidgetItem *currentItem() const;
    void setCurrentItem(QListWidgetItem *item);
    int currentRow() const { return m_current; }
    QList<QListWidgetItem *> selectedItems() const;
    QListWidgetItem *itemAt(const QPoint &pos) const;
    QListWidgetItem *itemAt(int x, int y) const { return itemAt(QPoint(x, y)); }
    QRect visualItemRect(const QListWidgetItem *item) const;
    void scrollToItem(const QListWidgetItem *item);
    QWidget *itemWidget(QListWidgetItem *item) const;
    void setItemWidget(QListWidgetItem *item, QWidget *widget);
    void removeItemWidget(QListWidgetItem *item) { setItemWidget(item, nullptr); }
    int rowHeight() const;

    QListView::ViewMode viewMode() const { return m_viewMode; }
    void setViewMode(QListView::ViewMode mode);
    QListView::Movement movement() const { return m_movement; }
    void setMovement(QListView::Movement movement) { m_movement = movement; }
    QListView::ResizeMode resizeMode() const { return m_resizeMode; }
    void setResizeMode(QListView::ResizeMode mode) { m_resizeMode = mode; }
    QListView::Flow flow() const { return m_flow; }
    void setFlow(QListView::Flow flow);
    bool isWrapping() const { return m_wrapping; }
    void setWrapping(bool enable);
    int spacing() const { return m_spacing; }
    void setSpacing(int space);
    QSize gridSize() const { return m_gridSize; }
    void setGridSize(const QSize &size);
    bool uniformItemSizes() const { return m_uniformItemSizes; }
    void setUniformItemSizes(bool enable) { m_uniformItemSizes = enable; }
    bool wordWrap() const { return m_wordWrap; }
    void setWordWrap(bool on);
    bool isRowHidden(int row) const;
    void setRowHidden(int row, bool hide);
    bool isSortingEnabled() const { return m_sorting; }
    void setSortingEnabled(bool enable);
    QList<QListWidgetItem *> findItems(const QString &text, Qt::MatchFlags flags) const;
    void editItem(QListWidgetItem *item);
    QModelIndex indexFromItem(const QListWidgetItem *item) const;
    QListWidgetItem *itemFromIndex(const QModelIndex &index) const;
    Qt::DropActions supportedDragActions() const { return m_dragActions; }
    void setSupportedDragActions(Qt::DropActions actions) { m_dragActions = actions; }

public slots:
    void setCurrentRow(int row);
    void clear();
    void selectAll();
    void clearSelection();

signals:
    /*!
        This signal is emitted when the current item changes from \a previous to \a current, either of which may be
        \c nullptr.
    */
    void currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous) {
        emitSignal(&QListWidget::currentItemChanged, current, previous);
    }
    /*!
        This signal is emitted when the current row changes to \a currentRow, or -1 if there is none.
    */
    void currentRowChanged(int currentRow) { emitSignal(&QListWidget::currentRowChanged, currentRow); }
    /*!
        This signal is emitted when the current item changes, with the \a currentText of the new one, which is empty
        if there is none.
    */
    void currentTextChanged(const QString &currentText) { emitSignal(&QListWidget::currentTextChanged, currentText); }
    /*!
        This signal is emitted when the user clicks \a item.
    */
    void itemClicked(QListWidgetItem *item) { emitSignal(&QListWidget::itemClicked, item); }
    /*!
        This signal is emitted when the user double-clicks \a item.
    */
    void itemDoubleClicked(QListWidgetItem *item) { emitSignal(&QListWidget::itemDoubleClicked, item); }
    /*!
        This signal is emitted when the user activates \a item: double-clicks it, or presses Return or Enter while it
        is the current item.
    */
    void itemActivated(QListWidgetItem *item) { emitSignal(&QListWidget::itemActivated, item); }
    /*!
        This signal is emitted when the selection changes.
    */
    void itemSelectionChanged() { emitSignal(&QListWidget::itemSelectionChanged); }
    /*!
        \since 6.0
        This signal is emitted when the data of \a item changes, its text for example.
    */
    void itemChanged(QListWidgetItem *item) { emitSignal(&QListWidget::itemChanged, item); }
    /*!
        \since 6.0
        This signal is emitted when the user presses a mouse button on \a item.
    */
    void itemPressed(QListWidgetItem *item) { emitSignal(&QListWidget::itemPressed, item); }
    /*!
        \since 6.0
        This signal is emitted when the mouse enters \a item, while mouse tracking is on.
    */
    void itemEntered(QListWidgetItem *item) { emitSignal(&QListWidget::itemEntered, item); }

protected:
    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QList<QListWidgetItem *> &items) const;
    virtual bool dropMimeData(int index, const QMimeData *data, Qt::DropAction action);
    virtual Qt::DropActions supportedDropActions() const;
    QList<QListWidgetItem *> items(const QMimeData *data) const;
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

private:
    friend class QListWidgetItem;
    void adopt(QListWidgetItem *item);
    void updateScrollBars();
    void itemDidChange(QListWidgetItem *item);
    void closeItemEditor(bool commit);
    void forget(QListWidgetItem *item);
    bool selectable(int row) const;
    bool enabled(int row) const;
    void moveCurrent(int row);
    void select(int row, Qt::KeyboardModifiers modifiers);
    void setRowSelected(int row, bool selected);
    bool clearSelection(int except);
    void scrollToRow(int row);
    // The row at `pos` in the viewport, or -1.
    int rowAt(const QPoint &pos) const;
    // Lays the items out, if they changed: their rectangles in the contents, which scroll down.
    void layoutItems() const;
    QRect itemRect(int row) const; // in the contents; empty for a hidden item
    int contentHeight() const;
    QSize iconCell() const; // the size of an item in icon mode
    void invalidateLayout();
    // Puts the item widgets over their items.
    void placeItemWidgets();
    void navigate(int target, Qt::KeyboardModifiers modifiers);
    void finishSelection();
    void search(const QString &text);

    std::vector<QListWidgetItem *> m_items;
    int m_current = -1;
    int m_anchor = -1;
    int m_pressed = -1;
    int m_scrollRow = -1;
    bool m_selectionDirty = false;
    bool m_updating = false;
    bool m_updateAgain = false;
    QListView::ViewMode m_viewMode = QListView::ListMode;
    QListView::Movement m_movement = QListView::Static;
    QListView::ResizeMode m_resizeMode = QListView::Fixed;
    QListView::Flow m_flow = QListView::TopToBottom;
    bool m_wrapping = false;
    int m_spacing = 0;
    QSize m_gridSize;
    bool m_uniformItemSizes = false;
    bool m_wordWrap = false;
    bool m_sorting = false;
    Qt::DropActions m_dragActions = Qt::CopyAction;
    QPointer<QWidget> m_editor;
    QListWidgetItem *m_editing = nullptr;
    int m_hover = -1; // the row the mouse is over, for itemEntered()
    mutable std::vector<QRect> m_rects; // of the items, row by row, as layoutItems() placed them
    mutable int m_contentHeight = 0;
    mutable bool m_layoutValid = false;
    mutable int m_layoutWidth = -1;
};
