// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qitemselectionmodel.h>
#include <QtCore/qmap.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtWidgets/qabstractitemdelegate.h>
#include <QtWidgets/qabstractscrollarea.h>

#include <vector>

class QAbstractItemView : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode)
    Q_PROPERTY(SelectionBehavior selectionBehavior READ selectionBehavior WRITE setSelectionBehavior)
    Q_PROPERTY(bool alternatingRowColors READ alternatingRowColors WRITE setAlternatingRowColors)
    Q_PROPERTY(EditTriggers editTriggers READ editTriggers WRITE setEditTriggers)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(bool dragEnabled READ dragEnabled WRITE setDragEnabled)
    Q_PROPERTY(DragDropMode dragDropMode READ dragDropMode WRITE setDragDropMode)
    Q_PROPERTY(Qt::DropAction defaultDropAction READ defaultDropAction WRITE setDefaultDropAction)
    Q_PROPERTY(bool autoScroll READ hasAutoScroll WRITE setAutoScroll)
    Q_PROPERTY(int autoScrollMargin READ autoScrollMargin WRITE setAutoScrollMargin)
    Q_PROPERTY(bool tabKeyNavigation READ tabKeyNavigation WRITE setTabKeyNavigation)
    Q_PROPERTY(bool showDropIndicator READ showDropIndicator WRITE setDropIndicatorShown)
    Q_PROPERTY(bool dragDropOverwriteMode READ dragDropOverwriteMode WRITE setDragDropOverwriteMode)
    Q_PROPERTY(Qt::TextElideMode textElideMode READ textElideMode WRITE setTextElideMode)
    Q_PROPERTY(
        ScrollMode verticalScrollMode READ verticalScrollMode WRITE setVerticalScrollMode RESET resetVerticalScrollMode)
    Q_PROPERTY(ScrollMode horizontalScrollMode READ horizontalScrollMode WRITE setHorizontalScrollMode RESET
                   resetHorizontalScrollMode)
public:
    enum DragDropMode { NoDragDrop, DragOnly, DropOnly, DragDrop, InternalMove };
    Q_ENUM(DragDropMode)
    enum ScrollMode { ScrollPerItem, ScrollPerPixel };
    Q_ENUM(ScrollMode)
    enum SelectionMode { NoSelection, SingleSelection, MultiSelection, ExtendedSelection, ContiguousSelection };
    Q_ENUM(SelectionMode)
    enum SelectionBehavior { SelectItems, SelectRows, SelectColumns };
    Q_ENUM(SelectionBehavior)
    enum ScrollHint { EnsureVisible, PositionAtTop, PositionAtBottom, PositionAtCenter };
    Q_ENUM(ScrollHint)
    enum EditTrigger {
        NoEditTriggers = 0,
        CurrentChanged = 1,
        DoubleClicked = 2,
        SelectedClicked = 4,
        EditKeyPressed = 8,
        AnyKeyPressed = 16,
        AllEditTriggers = 31
    };
    Q_DECLARE_FLAGS(EditTriggers, EditTrigger)
    Q_FLAG(EditTriggers)

    explicit QAbstractItemView(QWidget *parent = nullptr);
    ~QAbstractItemView() override;

    virtual void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;
    virtual void setSelectionModel(QItemSelectionModel *selectionModel);
    QItemSelectionModel *selectionModel() const;

    void setItemDelegate(QAbstractItemDelegate *delegate);
    QAbstractItemDelegate *itemDelegate() const;
    void setItemDelegateForRow(int row, QAbstractItemDelegate *delegate);
    QAbstractItemDelegate *itemDelegateForRow(int row) const;
    void setItemDelegateForColumn(int column, QAbstractItemDelegate *delegate);
    QAbstractItemDelegate *itemDelegateForColumn(int column) const;
    virtual QAbstractItemDelegate *itemDelegateForIndex(const QModelIndex &index) const;

    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode);
    SelectionBehavior selectionBehavior() const { return m_selectionBehavior; }
    void setSelectionBehavior(SelectionBehavior behavior);
    bool alternatingRowColors() const { return m_alternating; }
    void setAlternatingRowColors(bool enable);
    EditTriggers editTriggers() const { return m_triggers; }
    void setEditTriggers(EditTriggers triggers);
    Qt::TextElideMode textElideMode() const { return m_elide; }
    void setTextElideMode(Qt::TextElideMode mode);
    QSize iconSize() const { return m_iconSize; }
    void setIconSize(const QSize &size);
    bool dragEnabled() const { return m_dragEnabled; }
    void setDragEnabled(bool enable) { m_dragEnabled = enable; }
    DragDropMode dragDropMode() const;
    void setVerticalScrollMode(ScrollMode mode) { m_verticalScrollMode = mode; }
    ScrollMode verticalScrollMode() const { return m_verticalScrollMode; }
    void setHorizontalScrollMode(ScrollMode mode) { m_horizontalScrollMode = mode; }
    ScrollMode horizontalScrollMode() const { return m_horizontalScrollMode; }
    void resetVerticalScrollMode() { m_verticalScrollMode = ScrollPerPixel; }
    void resetHorizontalScrollMode() { m_horizontalScrollMode = ScrollPerPixel; }
    bool hasAutoScroll() const { return m_autoScroll; }
    void setAutoScroll(bool enable) { m_autoScroll = enable; }
    int autoScrollMargin() const { return m_autoScrollMargin; }
    void setAutoScrollMargin(int margin) { m_autoScrollMargin = margin; }
    bool tabKeyNavigation() const { return m_tabKeyNavigation; }
    void setTabKeyNavigation(bool enable) { m_tabKeyNavigation = enable; }
    bool dragDropOverwriteMode() const { return m_overwrite; }
    void setDragDropOverwriteMode(bool overwrite) { m_overwrite = overwrite; }
    void openPersistentEditor(const QModelIndex &index);
    void closePersistentEditor(const QModelIndex &index);
    bool isPersistentEditorOpen(const QModelIndex &index) const;
    virtual void keyboardSearch(const QString &search);
    virtual int sizeHintForRow(int row) const;
    virtual int sizeHintForColumn(int column) const;
    int updateThreshold() const { return m_updateThreshold; }
    void setUpdateThreshold(int threshold) { m_updateThreshold = threshold; }
    void setDragDropMode(DragDropMode behavior);
    Qt::DropAction defaultDropAction() const { return m_defaultDropAction; }
    void setDefaultDropAction(Qt::DropAction dropAction) { m_defaultDropAction = dropAction; }
    bool showDropIndicator() const { return m_dropIndicator; }
    void setDropIndicatorShown(bool enable) { m_dropIndicator = enable; }

    QModelIndex currentIndex() const;
    QModelIndex rootIndex() const;

    virtual QRect visualRect(const QModelIndex &index) const;
    void setIndexWidget(const QModelIndex &index, QWidget *widget);
    QWidget *indexWidget(const QModelIndex &index) const;
    virtual void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible);
    virtual QModelIndex indexAt(const QPoint &point) const;
    QSize sizeHintForIndex(const QModelIndex &index) const;

public slots:
    virtual void reset();
    virtual void doItemsLayout();
    virtual void setRootIndex(const QModelIndex &index);
    virtual void selectAll();
    void edit(const QModelIndex &index);
    void clearSelection();
    void setCurrentIndex(const QModelIndex &index);
    void scrollToTop();
    void scrollToBottom();
    void update(const QModelIndex &index);

signals:
    /*!
        This signal is emitted when a mouse button is pressed on the item \a index.
    */
    void pressed(const QModelIndex &index) { emitSignal(&QAbstractItemView::pressed, index); }
    /*!
        This signal is emitted when the left mouse button is clicked on the item \a index.
    */
    void clicked(const QModelIndex &index) { emitSignal(&QAbstractItemView::clicked, index); }
    /*!
        This signal is emitted when the item \a index is double-clicked.
    */
    void doubleClicked(const QModelIndex &index) { emitSignal(&QAbstractItemView::doubleClicked, index); }
    /*!
        This signal is emitted when the user activates the item \a index: with a double click, or with
        Return or Enter while it is the current item.
    */
    void activated(const QModelIndex &index) { emitSignal(&QAbstractItemView::activated, index); }
    /*!
        This signal is emitted when the size of the decorations of the items changes to \a size.

        \sa setIconSize()
    */
    void iconSizeChanged(const QSize &size) { emitSignal(&QAbstractItemView::iconSizeChanged, size); }
    /*!
        \since 6.0
        This signal is emitted when the mouse enters the item \a index, while mouse tracking is on.
    */
    void entered(const QModelIndex &index) { emitSignal(&QAbstractItemView::entered, index); }
    /*!
        \since 6.0
        This signal is emitted when the mouse moves from an item to the empty part of the viewport, while mouse
        tracking is on.
    */
    void viewportEntered() { emitSignal(&QAbstractItemView::viewportEntered); }

protected:
    enum CursorAction {
        MoveUp,
        MoveDown,
        MoveLeft,
        MoveRight,
        MoveHome,
        MoveEnd,
        MovePageUp,
        MovePageDown,
        MoveNext,
        MovePrevious
    };
    enum State {
        NoState,
        DraggingState,
        DragSelectingState,
        EditingState,
        ExpandingState,
        CollapsingState,
        AnimatingState
    };
    enum DropIndicatorPosition { OnItem, AboveItem, BelowItem, OnViewport };

    virtual void selectionModeChanged();
    virtual void startDrag(Qt::DropActions supportedActions);
    // Called by the subclasses: a left press at `pos` may start a drag when the mouse then moves far enough.
    void setDragStart(const QPoint &pos, bool onItem);
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    virtual QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
    virtual bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event);
    virtual void initViewItemOption(QStyleOptionViewItem *option) const;
    State state() const;
    void setState(State state) { m_state = state; }
    virtual QModelIndexList selectedIndexes() const;
    virtual QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex &index,
                                                                 const QEvent *event = nullptr) const;
    virtual void setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command);
    virtual QRegion visualRegionForSelection(const QItemSelection &selection) const;
    virtual int horizontalOffset() const;
    virtual int verticalOffset() const;
    virtual bool isIndexHidden(const QModelIndex &index) const;
    DropIndicatorPosition dropIndicatorPosition() const { return m_dropIndicatorPosition; }
    void scheduleDelayedItemsLayout();
    void executeDelayedItemsLayout();
    void setDirtyRegion(const QRegion &region);
    void scrollDirtyRegion(int dx, int dy);
    QPoint dirtyRegionOffset() const { return QPoint(); }
    void startAutoScroll();
    void stopAutoScroll();
    void doAutoScroll();
    void keyPressEvent(QKeyEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void updateEditorGeometries();
    virtual void updateGeometries();
    bool viewportEvent(QEvent *event) override;
    using QWidget::update;

protected slots:
    virtual void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                             const QList<int> &roles = QList<int>());
    virtual void rowsInserted(const QModelIndex &parent, int start, int end);
    virtual void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    virtual void currentChanged(const QModelIndex &current, const QModelIndex &previous);
    virtual void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint);
    virtual void commitData(QWidget *editor);
    virtual void editorDestroyed(QObject *editor);
    virtual void updateEditorData();
    virtual void horizontalScrollbarAction(int action);
    virtual void verticalScrollbarAction(int action);
    virtual void horizontalScrollbarValueChanged(int value);
    virtual void verticalScrollbarValueChanged(int value);

private:
    void watchDelegate(QAbstractItemDelegate *delegate);
    void disconnectAll(std::vector<QMetaObject::Connection> &connections);

    SelectionMode m_selectionMode = SingleSelection;
    SelectionBehavior m_selectionBehavior = SelectItems;
    EditTriggers m_triggers = EditTriggers(DoubleClicked) | EditKeyPressed | AnyKeyPressed;
    Qt::TextElideMode m_elide = Qt::ElideRight;
    QSize m_iconSize;
    bool m_alternating = false;
    bool m_dragEnabled = false;
    bool m_dropIndicator = true;
    DragDropMode m_dragDropMode = NoDragDrop;
    ScrollMode m_verticalScrollMode = ScrollPerPixel;
    ScrollMode m_horizontalScrollMode = ScrollPerPixel;
    Qt::DropAction m_defaultDropAction = Qt::IgnoreAction;
    QPoint m_dragStart;       // where the left button went down on an item
    bool m_dragArmed = false; // a drag may start from m_dragStart
    QPointer<QAbstractItemModel> m_model;
    QPointer<QItemSelectionModel> m_selection;
    mutable QPointer<QAbstractItemDelegate> m_delegate;
    QMap<int, QPointer<QAbstractItemDelegate>> m_rowDelegates;
    QMap<int, QPointer<QAbstractItemDelegate>> m_columnDelegates;
    std::vector<QObject *> m_watched; // the delegates whose signals the view listens to
    QPersistentModelIndex m_root;
    QPointer<QWidget> m_editor;
    QPersistentModelIndex m_editIndex;
    std::vector<QMetaObject::Connection> m_modelConnections;
    std::vector<QMetaObject::Connection> m_selectionConnections;
    struct IndexWidget {
        QPersistentModelIndex index;
        QPointer<QWidget> widget; // a child of the viewport
    };
    std::vector<IndexWidget> m_indexWidgets;
    void placeIndexWidgets();
    void placePersistentEditors();
    QModelIndex persistentIndexOf(const QWidget *editor) const;
    struct PersistentEditor {
        QPersistentModelIndex index;
        QPointer<QWidget> editor;
    };
    std::vector<PersistentEditor> m_persistent;
    bool m_autoScroll = true;
    int m_autoScrollMargin = 16;
    bool m_tabKeyNavigation = false;
    bool m_overwrite = false;
    int m_updateThreshold = 200;
    State m_state = NoState;
    DropIndicatorPosition m_dropIndicatorPosition = OnViewport;
    bool m_layoutScheduled = false;
    QTimer m_autoScrollTimer;
    QString m_search;
    QElapsedTimer m_searchTimer;
    QPersistentModelIndex m_hover; // the item the mouse is over, for entered()
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractItemView::EditTriggers)
