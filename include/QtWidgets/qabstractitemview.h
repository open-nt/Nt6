// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qitemselectionmodel.h>
#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>
#include <QtWidgets/qabstractitemdelegate.h>
#include <QtWidgets/qabstractscrollarea.h>

#include <vector>

class QAbstractItemView : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode)
    Q_PROPERTY(SelectionBehavior selectionBehavior READ selectionBehavior WRITE setSelectionBehavior)
    Q_PROPERTY(bool alternatingRowColors READ alternatingRowColors WRITE setAlternatingRowColors)
    Q_PROPERTY(EditTriggers editTriggers READ editTriggers WRITE setEditTriggers)
public:
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

    QModelIndex currentIndex() const;
    QModelIndex rootIndex() const;

    virtual QRect visualRect(const QModelIndex &index) const;
    virtual void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible);
    virtual QModelIndex indexAt(const QPoint &point) const;
    QSize sizeHintForIndex(const QModelIndex &index) const;

public slots:
    virtual void reset();
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
    enum State { NoState, EditingState };

    virtual void selectionModeChanged();
    virtual QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
    virtual bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event);
    virtual void initViewItemOption(QStyleOptionViewItem *option) const;
    State state() const;
    void updateEditorGeometries();
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

private:
    void watchDelegate(QAbstractItemDelegate *delegate);
    void disconnectAll(std::vector<QMetaObject::Connection> &connections);

    SelectionMode m_selectionMode = SingleSelection;
    SelectionBehavior m_selectionBehavior = SelectItems;
    EditTriggers m_triggers = EditTriggers(DoubleClicked) | EditKeyPressed | AnyKeyPressed;
    Qt::TextElideMode m_elide = Qt::ElideRight;
    bool m_alternating = false;
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
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractItemView::EditTriggers)
