// MIT License - see LICENSE
#include <QtWidgets/qabstractitemview.h>
#include <QtGui/qdrag.h>
#include <QtCore/qmimedata.h>

#include <QtGui/qevent.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyleditemdelegate.h>

#include <algorithm>

/*!
    \class QAbstractItemView
    \brief The QAbstractItemView class provides the basic functionality for item view classes.

    An item view shows the items of a model: a QAbstractItemModel holds the data, the view shows it and lets the
    user work with it, a QItemSelectionModel knows which items are selected and which one is current, and a
    delegate draws each item and provides the widget that it is edited with. Several views can show one model,
    and share one selection.

    \code
    auto *view = new QTreeView;
    view->setModel(model);
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, &Window::showDetails);
    \endcode

    This class has what views share: the model, the selection model and the delegates, how items are selected
    (selectionMode() and selectionBehavior()), when they are edited (editTriggers()), and the signals for what
    the user does with an item: pressed(), clicked(), doubleClicked() and activated(). A subclass lays the
    items out: it implements visualRect(), indexAt(), scrollTo() and moveCursor(), and draws the items on
    viewport(). QListView, QTableView and QTreeView are the views of Nt Widgets that do; QListWidget and
    QTableWidget derive from this class too, but keep their own items and use none of what has to do with a model.

    \section1 Editing

    edit() opens an editor on an item, if the item is Qt::ItemIsEditable. The user starts one in the ways that
    editTriggers() allows: by default with a double click, with F2, or by starting to type. The delegate makes
    the editor and writes its value to the model when the user is done; see QStyledItemDelegate.

    \differences
    visualRect(), indexAt(), scrollTo(), moveCursor(), horizontalOffset(), verticalOffset(), isIndexHidden(),
    setSelection() and visualRegionForSelection() have defaults, where Qt has pure virtual functions, so that
    QListWidget and QTableWidget, which have no model, need not implement them. updateThreshold() and the dirty
    region functions are kept for compatibility; the view repaints what changed.
    \enddifferences

    \sa QListView, QTableView, QTreeView, QStyledItemDelegate, QItemSelectionModel
*/

/*!
    \enum QAbstractItemView::SelectionMode

    This enum says how many items the user can select, and how.

    \value NoSelection Items cannot be selected.
    \value SingleSelection One item can be selected; selecting another deselects it.
    \value MultiSelection Clicking an item toggles its selection, and the others stay as they are.
    \value ExtendedSelection Clicking selects one item; Control toggles an item and Shift selects the items from the
           last one to the one clicked.
    \value ContiguousSelection Clicking selects one item, and Shift selects the items from the last one to the
           one clicked; Control does nothing special.
*/

/*!
    \enum QAbstractItemView::SelectionBehavior

    This enum says what is selected when the user selects an item.

    \value SelectItems The item itself.
    \value SelectRows The whole row of the item.
    \value SelectColumns The whole column of the item.
*/

/*!
    \enum QAbstractItemView::ScrollHint

    This enum says where scrollTo() puts an item.

    \value EnsureVisible Anywhere, as long as it shows: the view scrolls as little as it can.
    \value PositionAtTop At the top of the viewport.
    \value PositionAtBottom At the bottom of the viewport.
    \value PositionAtCenter In the middle of the viewport.
*/

/*!
    \enum QAbstractItemView::EditTrigger

    This enum says what the user does to start editing an item. The EditTriggers type is a typedef for
    \c{QFlags<EditTrigger>}.

    \value NoEditTriggers Nothing: items are edited only when the program calls edit().
    \value CurrentChanged An item becoming the current item.
    \value DoubleClicked A double click on the item.
    \value SelectedClicked A click on an item that is selected already.
    \value EditKeyPressed The edit key, F2, on the current item.
    \value AnyKeyPressed Typing a character on the current item.
    \value AllEditTriggers All of the above.
*/

/*!
    \enum QAbstractItemView::CursorAction

    This enum says which way moveCursor() goes from the current item.

    \value MoveUp To the item above.
    \value MoveDown To the item below.
    \value MoveLeft To the item on the left; in a tree, to the parent or by collapsing the item.
    \value MoveRight To the item on the right; in a tree, to the first child or by expanding the item.
    \value MoveHome To the first item.
    \value MoveEnd To the last item.
    \value MovePageUp One page up.
    \value MovePageDown One page down.
    \value MoveNext To the item after the current one, as Tab does in an editor.
    \value MovePrevious To the item before the current one.
*/

/*!
    \enum QAbstractItemView::State

    This enum says what the view is doing.

    \value NoState Nothing in particular.
    \value DraggingState Items are being dragged.
    \value DragSelectingState Items are being selected by dragging the mouse.
    \value EditingState An editor is open.
    \value ExpandingState An item is being expanded.
    \value CollapsingState An item is being collapsed.
    \value AnimatingState The view is animating.

    Nt views set only EditingState themselves; a subclass sets the others with setState().
*/

/*!
    Constructs an item view with the given \a parent. It allows one selected item.
*/
QAbstractItemView::QAbstractItemView(QWidget *parent) : QAbstractScrollArea(parent) {
    m_autoScrollTimer.setInterval(50);
    QObject::connect(&m_autoScrollTimer, &QTimer::timeout, this, [this] { doAutoScroll(); });
}

/*!
    Destroys the view. The model and a selection model that was given to it are left alone.
*/
QAbstractItemView::~QAbstractItemView() {
    disconnectAll(m_modelConnections);
    disconnectAll(m_selectionConnections);
}

void QAbstractItemView::disconnectAll(std::vector<QMetaObject::Connection> &connections) {
    for (const QMetaObject::Connection &c : connections)
        QObject::disconnect(c);
    connections.clear();
}

/*!
    Sets the model that the view shows to \a model, and gives the view a new selection model for it. The view
    does not own the model: several views can show one. A \c nullptr leaves the view without items.

    \sa model(), setSelectionModel()
*/
void QAbstractItemView::setModel(QAbstractItemModel *model) {
    if (m_model == model)
        return;
    if (m_editor)
        closeEditor(m_editor, QAbstractItemDelegate::RevertModelCache);
    disconnectAll(m_modelConnections);
    m_model = model;
    m_root = QPersistentModelIndex();
    if (model) {
        m_modelConnections = {
            connect(model, &QAbstractItemModel::dataChanged, this,
                    [this](const QModelIndex &a, const QModelIndex &b, const QList<int> &roles) {
                        dataChanged(a, b, roles);
                    }),
            connect(model, &QAbstractItemModel::rowsInserted, this,
                    [this](const QModelIndex &parent, int first, int last) { rowsInserted(parent, first, last); }),
            connect(
                model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                [this](const QModelIndex &parent, int first, int last) { rowsAboutToBeRemoved(parent, first, last); }),
            connect(model, &QAbstractItemModel::modelReset, this, [this] { reset(); }),
            connect(model, &QObject::destroyed, this,
                    [this] {
                        // The model is half destroyed: the view must not ask it anything from here on.
                        m_modelConnections.clear();
                        m_model = nullptr;
                        m_root = QPersistentModelIndex();
                        QItemSelectionModel *old = m_selection;
                        setSelectionModel(nullptr);
                        if (old && old->parent() == this)
                            old->deleteLater();
                        reset();
                    }),
        };
    }
    // The selection of the old model means nothing for the new one.
    QItemSelectionModel *old = m_selection;
    setSelectionModel(model ? new QItemSelectionModel(model, this) : nullptr);
    if (old && old->parent() == this)
        old->deleteLater();
    reset();
}

/*!
    Returns the model that the view shows, or \c nullptr.

    \sa setModel()
*/
QAbstractItemModel *QAbstractItemView::model() const { return m_model; }

/*!
    Sets the selection model of the view to \a selectionModel, which has to be one for the model of the view.
    Giving two views the same selection model makes them show the same selection and the same current item.

    \sa selectionModel(), setModel()
*/
void QAbstractItemView::setSelectionModel(QItemSelectionModel *selectionModel) {
    if (m_selection == selectionModel)
        return;
    disconnectAll(m_selectionConnections);
    m_selection = selectionModel;
    if (selectionModel) {
        m_selectionConnections = {
            connect(selectionModel, &QItemSelectionModel::selectionChanged, this,
                    [this](const QItemSelection &selected, const QItemSelection &deselected) {
                        selectionChanged(selected, deselected);
                    }),
            connect(
                selectionModel, &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex &current, const QModelIndex &previous) { currentChanged(current, previous); }),
        };
    }
    viewport()->update();
}

/*!
    Returns the selection model of the view: the one that setModel() made, or the one that setSelectionModel()
    was given. It is \c nullptr while the view has no model.

    \sa setSelectionModel()
*/
QItemSelectionModel *QAbstractItemView::selectionModel() const { return m_selection; }

void QAbstractItemView::watchDelegate(QAbstractItemDelegate *delegate) {
    if (!delegate || std::find(m_watched.begin(), m_watched.end(), delegate) != m_watched.end())
        return;
    m_watched.push_back(delegate);
    connect(delegate, &QAbstractItemDelegate::commitData, this, [this](QWidget *editor) { commitData(editor); });
    connect(delegate, &QAbstractItemDelegate::closeEditor, this,
            [this](QWidget *editor, QAbstractItemDelegate::EndEditHint hint) { closeEditor(editor, hint); });
    connect(delegate, &QAbstractItemDelegate::sizeHintChanged, this,
            [this](const QModelIndex &) { viewport()->update(); });
    connect(delegate, &QObject::destroyed, this, [this, delegate] {
        m_watched.erase(std::remove(m_watched.begin(), m_watched.end(), static_cast<QObject *>(delegate)),
                        m_watched.end());
    });
}

/*!
    Sets the delegate that draws and edits the items of the view to \a delegate. The view does not own it, so
    several views can share one. A \c nullptr gives the view its own QStyledItemDelegate again.

    \sa itemDelegate(), setItemDelegateForColumn()
*/
void QAbstractItemView::setItemDelegate(QAbstractItemDelegate *delegate) {
    if (m_delegate == delegate)
        return;
    if (m_delegate && m_delegate->parent() == this)
        m_delegate->deleteLater();
    m_delegate = delegate;
    watchDelegate(delegate);
    viewport()->update();
}

/*!
    Returns the delegate of the view: the one that setItemDelegate() set, or a QStyledItemDelegate of the view.

    \sa setItemDelegate(), itemDelegateForIndex()
*/
QAbstractItemDelegate *QAbstractItemView::itemDelegate() const {
    if (!m_delegate) {
        auto *self =
            const_cast<QAbstractItemView *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): made lazily
        m_delegate = new QStyledItemDelegate(self);
        self->watchDelegate(m_delegate);
    }
    return m_delegate;
}

/*!
    Sets the delegate for the items of the row \a row to \a delegate, or takes it away if \a delegate is
    \c nullptr. A delegate for a row goes before one for a column.

    \sa itemDelegateForRow(), setItemDelegateForColumn()
*/
void QAbstractItemView::setItemDelegateForRow(int row, QAbstractItemDelegate *delegate) {
    if (delegate)
        m_rowDelegates.insert(row, delegate);
    else
        m_rowDelegates.remove(row);
    watchDelegate(delegate);
    viewport()->update();
}

/*!
    Returns the delegate that was set for the row \a row, or \c nullptr.

    \sa setItemDelegateForRow()
*/
QAbstractItemDelegate *QAbstractItemView::itemDelegateForRow(int row) const { return m_rowDelegates.value(row); }

/*!
    Sets the delegate for the items of the column \a column to \a delegate, or takes it away if \a delegate is
    \c nullptr: a column of dates or of ratings is drawn and edited in its own way.

    \sa itemDelegateForColumn(), setItemDelegateForRow()
*/
void QAbstractItemView::setItemDelegateForColumn(int column, QAbstractItemDelegate *delegate) {
    if (delegate)
        m_columnDelegates.insert(column, delegate);
    else
        m_columnDelegates.remove(column);
    watchDelegate(delegate);
    viewport()->update();
}

/*!
    Returns the delegate that was set for the column \a column, or \c nullptr.

    \sa setItemDelegateForColumn()
*/
QAbstractItemDelegate *QAbstractItemView::itemDelegateForColumn(int column) const {
    return m_columnDelegates.value(column);
}

/*!
    Returns the delegate that draws and edits the item \a index: the one of its row, else the one of its
    column, else itemDelegate().

    \sa setItemDelegate()
*/
QAbstractItemDelegate *QAbstractItemView::itemDelegateForIndex(const QModelIndex &index) const {
    if (QAbstractItemDelegate *row = m_rowDelegates.value(index.row()))
        return row;
    if (QAbstractItemDelegate *column = m_columnDelegates.value(index.column()))
        return column;
    return itemDelegate();
}

/*!
    \fn QAbstractItemView::SelectionMode QAbstractItemView::selectionMode() const

    Returns how items are selected.

    \sa setSelectionMode()
*/

/*!
    Sets how items are selected to \a mode. A selection that the new mode does not allow is cleared.

    \sa selectionMode()
*/
void QAbstractItemView::setSelectionMode(SelectionMode mode) {
    if (m_selectionMode == mode)
        return;
    m_selectionMode = mode;
    if (m_selection && (mode == NoSelection || mode == SingleSelection)) {
        const QModelIndex current = m_selection->currentIndex();
        m_selection->clearSelection();
        if (mode == SingleSelection && current.isValid()) {
            m_selection->select(current, QItemSelectionModel::SelectionFlags(QItemSelectionModel::Select) |
                                             (m_selectionBehavior == SelectRows ? QItemSelectionModel::Rows
                                                                                : QItemSelectionModel::NoUpdate));
        }
    }
    selectionModeChanged();
    viewport()->update();
}

/*!
    Called when the selection mode changes: a subclass brings its selection in line with it. The default does nothing.
*/
void QAbstractItemView::selectionModeChanged() {}

/*!
    \fn QAbstractItemView::SelectionBehavior QAbstractItemView::selectionBehavior() const

    Returns what is selected when the user selects an item: the item, its row or its column.

    \sa setSelectionBehavior()
*/

/*!
    Sets what is selected when the user selects an item to \a behavior.

    \sa selectionBehavior()
*/
void QAbstractItemView::setSelectionBehavior(SelectionBehavior behavior) { m_selectionBehavior = behavior; }

/*!
    \fn bool QAbstractItemView::alternatingRowColors() const

    Returns whether every other row has a different background. The default is \c false.

    \sa setAlternatingRowColors()
*/

/*!
    Sets whether every other row has the alternate base color of the palette as its background to \a enable.

    \sa alternatingRowColors()
*/
void QAbstractItemView::setAlternatingRowColors(bool enable) {
    if (m_alternating == enable)
        return;
    m_alternating = enable;
    viewport()->update();
}

/*!
    \fn QAbstractItemView::EditTriggers QAbstractItemView::editTriggers() const

    Returns what the user does to start editing an item: by default, a double click, F2, or typing.

    \sa setEditTriggers()
*/

/*!
    Sets what the user does to start editing an item to \a triggers. With \c NoEditTriggers the user cannot
    edit; edit() still can.

    \sa editTriggers(), edit()
*/
void QAbstractItemView::setEditTriggers(EditTriggers triggers) { m_triggers = triggers; }

/*!
    \fn Qt::TextElideMode QAbstractItemView::textElideMode() const

    Returns where the text of an item gets its ellipsis when it does not fit. The default is at the end.

    \sa setTextElideMode()
*/

/*!
    Sets where the text of an item gets its ellipsis when it does not fit to \a mode.

    \sa textElideMode()
*/
void QAbstractItemView::setTextElideMode(Qt::TextElideMode mode) {
    m_elide = mode;
    viewport()->update();
}

/*!
    \fn QSize QAbstractItemView::iconSize() const

    Returns the size that the decorations of the items are drawn in. It is invalid by default, and then each
    decoration has its own size; the icon mode of QListView makes it 32 by 32 pixels.

    \sa setIconSize()
*/

/*!
    Sets the size that the decorations of the items are drawn in to \a size, lays the items out again and emits
    iconSizeChanged(). An invalid \a size draws each decoration in its own size.

    \sa iconSize()
*/
void QAbstractItemView::setIconSize(const QSize &size) {
    if (m_iconSize == size)
        return;
    m_iconSize = size;
    updateGeometries();
    viewport()->update();
    emit iconSizeChanged(size);
}

/*!
    Returns the current item: the one that the keyboard works on. It is invalid if there is none.

    \sa setCurrentIndex(), QItemSelectionModel::currentIndex()
*/
QModelIndex QAbstractItemView::currentIndex() const {
    return m_selection ? m_selection->currentIndex() : QModelIndex();
}

/*!
    Returns the item whose children the view shows: an invalid index, for the top of the model, unless
    setRootIndex() set another one.

    \sa setRootIndex()
*/
QModelIndex QAbstractItemView::rootIndex() const { return m_root; }

/*!
    Returns the rectangle that the item \a index takes on the viewport, which is empty if the item is not laid
    out, because a parent of it is collapsed for example. The default returns an empty rectangle.

    \sa indexAt(), scrollTo()
*/
QRect QAbstractItemView::visualRect(const QModelIndex &index) const {
    static_cast<void>(index);
    return QRect();
}

/*!
    Scrolls the view so that the item \a index shows, at the place that \a hint says. The default does nothing.

    \sa visualRect()
*/
void QAbstractItemView::scrollTo(const QModelIndex &index, ScrollHint hint) {
    static_cast<void>(index);
    static_cast<void>(hint);
}

/*!
    Returns the item at the position \a point of the viewport, or an invalid index if there is none. The
    default returns an invalid index.

    \sa visualRect()
*/
QModelIndex QAbstractItemView::indexAt(const QPoint &point) const {
    static_cast<void>(point);
    return QModelIndex();
}

/*!
    Returns the size that the delegate of the item \a index wants for it, or an invalid size if \a index is
    not valid.
*/
QSize QAbstractItemView::sizeHintForIndex(const QModelIndex &index) const {
    if (!index.isValid())
        return QSize();
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    return itemDelegateForIndex(index)->sizeHint(option, index);
}

/*!
    \enum QAbstractItemView::DragDropMode
    \since 6.0

    This enum tells what drags the view starts and takes.

    \value NoDragDrop None. This is the default.
    \value DragOnly The items can be dragged from the view.
    \value DropOnly Drags can be dropped on the view.
    \value DragDrop Both.
    \value InternalMove Items are moved within the view only.
*/

/*!
    \enum QAbstractItemView::ScrollMode
    \since 6.0

    This enum tells how the scroll bars of a view move.

    \value ScrollPerItem An item at a time.
    \value ScrollPerPixel A pixel at a time.
*/

/*!
    \fn void QAbstractItemView::setVerticalScrollMode(ScrollMode mode)
    \since 6.0

    Sets how the view scrolls down to \a mode. Nt views always scroll by pixels; the mode is kept for
    verticalScrollMode().
*/

/*!
    \fn QAbstractItemView::ScrollMode QAbstractItemView::verticalScrollMode() const
    \since 6.0

    Returns how the view scrolls down; ScrollPerPixel by default.
*/

/*!
    \fn void QAbstractItemView::setHorizontalScrollMode(ScrollMode mode)
    \since 6.0

    Sets how the view scrolls across to \a mode. Nt views always scroll by pixels; the mode is kept for
    horizontalScrollMode().
*/

/*!
    \fn QAbstractItemView::ScrollMode QAbstractItemView::horizontalScrollMode() const
    \since 6.0

    Returns how the view scrolls across; ScrollPerPixel by default.
*/

/*!
    \fn bool QAbstractItemView::dragEnabled() const
    \since 6.0

    Returns \c true if the selected items can be dragged from the view.

    \sa setDragEnabled(), startDrag()
*/

/*!
    \fn void QAbstractItemView::setDragEnabled(bool enable)
    \since 6.0

    Lets the selected items be dragged from the view if \a enable is \c true.
*/

/*!
    \since 6.0

    Returns the drag and drop mode of the view, from what was set with setDragDropMode(), or from dragEnabled() and
    acceptDrops().
*/
QAbstractItemView::DragDropMode QAbstractItemView::dragDropMode() const {
    if (m_dragDropMode == InternalMove)
        return InternalMove;
    if (m_dragEnabled && acceptDrops())
        return DragDrop;
    if (m_dragEnabled)
        return DragOnly;
    if (acceptDrops())
        return DropOnly;
    return NoDragDrop;
}

/*!
    \since 6.0

    Sets the drag and drop mode of the view to \a behavior, which sets dragEnabled() and acceptDrops() to match.
*/
void QAbstractItemView::setDragDropMode(DragDropMode behavior) {
    m_dragDropMode = behavior;
    setDragEnabled(behavior == DragOnly || behavior == DragDrop || behavior == InternalMove);
    setAcceptDrops(behavior == DropOnly || behavior == DragDrop || behavior == InternalMove);
}

/*!
    \fn Qt::DropAction QAbstractItemView::defaultDropAction() const
    \since 6.0

    Returns the action a drag from the view proposes when no key asks for another; Qt::IgnoreAction, the default,
    lets the drag choose.
*/

/*!
    \fn void QAbstractItemView::setDefaultDropAction(Qt::DropAction dropAction)
    \since 6.0

    Sets the action a drag from the view proposes to \a dropAction.
*/

/*!
    \fn bool QAbstractItemView::showDropIndicator() const
    \since 6.0

    Returns \c true if a drop indicator is to be shown; Nt keeps the setting but draws none.
*/

/*!
    \fn void QAbstractItemView::setDropIndicatorShown(bool enable)
    \since 6.0

    Sets whether a drop indicator is shown to \a enable.
*/

/*!
    \since 6.0

    Starts a drag of the selected items that can be dragged, with the data that the model makes for them in
    QAbstractItemModel::mimeData(), allowing \a supportedActions. If the items are moved to another place, they are
    removed from the model.
*/
void QAbstractItemView::startDrag(Qt::DropActions supportedActions) {
    if (!model() || !selectionModel())
        return;
    QModelIndexList indexes;
    for (const QModelIndex &index : selectionModel()->selectedIndexes())
        if (model()->flags(index) & Qt::ItemIsDragEnabled)
            indexes.append(index);
    if (indexes.isEmpty())
        return;
    QMimeData *data = model()->mimeData(indexes);
    if (!data)
        return;
    auto *drag = new QDrag(this);
    drag->setMimeData(data);
    const Qt::DropAction result = drag->exec(supportedActions, m_defaultDropAction);
    if (result == Qt::MoveAction && dragDropMode() != InternalMove && drag->target() != window()->windowHandle()) {
        // Rows from the bottom up, so the indexes of the others stay valid.
        QList<QPersistentModelIndex> rows;
        for (const QModelIndex &index : indexes)
            if (index.column() == 0)
                rows.append(QPersistentModelIndex(index));
        for (qsizetype i = rows.size() - 1; i >= 0; --i)
            if (rows.at(i).isValid())
                model()->removeRow(rows.at(i).row(), rows.at(i).parent());
    }
    drag->deleteLater();
}

/*!
    \since 6.0

    Records that the left button went down at \a pos in the viewport, on an item if \a onItem is \c true, where a
    drag may start once the mouse moves far enough. The views call it from their mouse press handlers.
*/
void QAbstractItemView::setDragStart(const QPoint &pos, bool onItem) {
    m_dragStart = pos;
    m_dragArmed = onItem && m_dragEnabled;
}

/*!
    \reimp
    \since 6.0

    Starts a drag of the selected items once the mouse has moved startDragDistance() with the left button held from
    an item, if dragEnabled() is \c true; \a event is otherwise ignored.
*/
void QAbstractItemView::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragArmed && (event->buttons() & Qt::LeftButton) && model() &&
        (event->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragArmed = false;
        startDrag(model()->supportedDragActions());
        event->accept();
        return;
    }
    if (!(event->buttons() & Qt::LeftButton))
        m_dragArmed = false;
    event->ignore();
}

/*!
    \reimp
    \since 6.0

    Accepts a drag that the model can take, for a view that accepts drops: one whose formats are among
    QAbstractItemModel::mimeTypes(). \a event is ignored otherwise.
*/
void QAbstractItemView::dragEnterEvent(QDragEnterEvent *event) {
    if (!model() || !acceptDrops()) {
        event->ignore();
        return;
    }
    for (const QString &type : model()->mimeTypes()) {
        if (event->mimeData()->hasFormat(type)) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

/*!
    \reimp
    \since 6.0

    Accepts the drag \a event where the model can take it: on the item under it, or after the last item.
*/
void QAbstractItemView::dragMoveEvent(QDragMoveEvent *event) {
    if (!model()) {
        event->ignore();
        return;
    }
    const QModelIndex index = indexAt(event->position().toPoint());
    const bool can = model()->canDropMimeData(event->mimeData(), event->dropAction(), -1, -1, index);
    event->setAccepted(can);
}

/*!
    \reimp
    \since 6.0

    Gives the data of the drop \a event to the model, with QAbstractItemModel::dropMimeData(), on the item under it
    or after the last item; accepts the event if the model took the data.
*/
void QAbstractItemView::dropEvent(QDropEvent *event) {
    if (!model()) {
        event->ignore();
        return;
    }
    const QModelIndex index = indexAt(event->position().toPoint());
    if (model()->dropMimeData(event->mimeData(), event->dropAction(), -1, -1, index))
        event->accept();
    else
        event->ignore();
}

/*!
    \since 6.0

    Lays the items out again at once, rather than from the event loop. The default updates the geometries and
    repaints the viewport.
*/
void QAbstractItemView::doItemsLayout() {
    updateGeometries();
    viewport()->update();
}

/*!
    Brings the view back to its state for a model that is new or was reset: closes the editor and lays the
    items out again. A subclass that keeps something about the items, such as which are expanded, forgets it
    here and calls this implementation.
*/
void QAbstractItemView::reset() {
    if (m_editor)
        closeEditor(m_editor, QAbstractItemDelegate::RevertModelCache);
    viewport()->update();
}

/*!
    Makes the item \a index the root of the view: the view shows its children as its top items. An invalid
    index shows the whole model.

    \sa rootIndex()
*/
void QAbstractItemView::setRootIndex(const QModelIndex &index) {
    if (index.isValid() && index.model() != m_model)
        return;
    m_root = QPersistentModelIndex(index);
    viewport()->update();
}

/*!
    Selects all the items that the view shows, if the selection mode allows more than one. The default does
    nothing: it is the subclass that knows its items.
*/
void QAbstractItemView::selectAll() {}

/*!
    Opens an editor on the item \a index, if the item is enabled and Qt::ItemIsEditable. This works whatever
    editTriggers() is. The editor that is open on another item is closed first, and its value stored.

    \sa setEditTriggers(), QAbstractItemDelegate::createEditor()
*/
void QAbstractItemView::edit(const QModelIndex &index) { edit(index, AllEditTriggers, nullptr); }

/*!
    Deselects all items. The current item stays.

    \sa selectAll()
*/
void QAbstractItemView::clearSelection() {
    if (m_selection)
        m_selection->clearSelection();
}

/*!
    Makes the item \a index the current item and, unless the selection mode is \c NoSelection, the selected
    one. The view scrolls to it.

    \sa currentIndex(), QItemSelectionModel::setCurrentIndex()
*/
void QAbstractItemView::setCurrentIndex(const QModelIndex &index) {
    if (!m_selection || (index.isValid() && index.model() != m_model))
        return;
    QItemSelectionModel::SelectionFlags command = QItemSelectionModel::NoUpdate;
    if (m_selectionMode != NoSelection && index.isValid()) {
        command = QItemSelectionModel::ClearAndSelect;
        if (m_selectionBehavior == SelectRows)
            command |= QItemSelectionModel::Rows;
        else if (m_selectionBehavior == SelectColumns)
            command |= QItemSelectionModel::Columns;
    }
    m_selection->setCurrentIndex(index, command);
}

/*!
    Scrolls the view to its top.

    \sa scrollToBottom(), scrollTo()
*/
void QAbstractItemView::scrollToTop() { verticalScrollBar()->setValue(verticalScrollBar()->minimum()); }

/*!
    Scrolls the view to its bottom.

    \sa scrollToTop(), scrollTo()
*/
void QAbstractItemView::scrollToBottom() { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); }

/*!
    Repaints the item \a index.
*/
void QAbstractItemView::update(const QModelIndex &index) {
    const QRect rect = visualRect(index);
    if (!rect.isEmpty())
        viewport()->update(rect);
}

/*!
    Returns the item that \a cursorAction leads to from the current item, with the keys of \a modifiers held;
    the view makes it the current item. A subclass knows how its items are laid out and implements this. The
    default returns an invalid index, for no move.
*/
QModelIndex QAbstractItemView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) {
    static_cast<void>(cursorAction);
    static_cast<void>(modifiers);
    return QModelIndex();
}

/*!
    Fills \a option with what is the same for all items of the view: its palette, font, state and the widget.
    A view passes it to the delegate for each item, with the rectangle and the state of that item added.
*/
void QAbstractItemView::initViewItemOption(QStyleOptionViewItem *option) const {
    option->initFrom(this);
    option->state &= ~QStyle::State(QStyle::State_MouseOver | QStyle::State_HasFocus);
    option->font = font();
    option->fontMetrics = QFontMetrics(font());
    option->textElideMode = m_elide;
    if (m_iconSize.isValid())
        option->decorationSize = m_iconSize;
    option->widget = this;
}

/*!
    Returns \c EditingState while an editor is open.
*/
QAbstractItemView::State QAbstractItemView::state() const { return m_editor ? EditingState : m_state; }

/*!
    Lays out the parts of the view around its items again: its headers and the ranges of its scroll bars. A
    subclass reimplements this; the default does nothing.
*/
void QAbstractItemView::updateGeometries() {}

/*!
    Places the editor of the view over its item again. A subclass calls this when it has scrolled or laid its
    items out anew.
*/
void QAbstractItemView::updateEditorGeometries() {
    if (!m_editor || !m_editIndex.isValid())
        return;
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(m_editIndex);
    if (option.rect.isEmpty())
        m_editor->hide(); // its item is no longer laid out; the editor comes back with it
    else
        itemDelegateForIndex(m_editIndex)->updateEditorGeometry(m_editor, option, m_editIndex);
}

/*!
    \overload

    Starts editing the item \a index because of \a trigger, and returns \c true if the view is now editing it
    or the delegate handled \a event. The delegate sees \a event first, which is how a click toggles a check
    box. Then, if \a trigger is one of editTriggers(), or is \c AllEditTriggers as from edit(), and the item is
    enabled and editable, the delegate makes an editor for it. A subclass calls this from its event handlers.
*/
bool QAbstractItemView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) {
    if (!m_model || !index.isValid() || index.model() != m_model)
        return false;
    QAbstractItemDelegate *delegate = itemDelegateForIndex(index);
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    if (event && delegate->editorEvent(event, m_model, option, index))
        return true;
    if (m_editor && m_editIndex == index)
        return trigger == AllEditTriggers;
    if (trigger != AllEditTriggers && !(m_triggers & trigger))
        return false;
    const Qt::ItemFlags flags = m_model->flags(index);
    if (!(flags & Qt::ItemIsEditable) || !(flags & Qt::ItemIsEnabled) || !isEnabled())
        return false;
    if (m_editor) {
        // One editor at a time: the other one stores its value and goes.
        QWidget *other = m_editor;
        commitData(other);
        closeEditor(other, QAbstractItemDelegate::NoHint);
    }
    scrollTo(index);
    option.rect = visualRect(index);
    QWidget *editor = delegate->createEditor(viewport(), option, index);
    if (!editor)
        return false;
    m_editor = editor;
    m_editIndex = QPersistentModelIndex(index);
    editor->installEventFilter(delegate);
    delegate->setEditorData(editor, index);
    delegate->updateEditorGeometry(editor, option, index);
    editor->show();
    editor->setFocus();
    return true;
}

/*!
    Called when the model changed the data of the items from \a topLeft to \a bottomRight, for the roles in
    \a roles, or for all roles if \a roles is empty. The default repaints the viewport, and gives an open
    editor of one of these items the new value.
*/
void QAbstractItemView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                                    const QList<int> &roles) {
    static_cast<void>(roles);
    if (m_editor && m_editIndex.isValid() && m_editIndex.parent() == topLeft.parent() &&
        m_editIndex.row() >= topLeft.row() && m_editIndex.row() <= bottomRight.row() &&
        m_editIndex.column() >= topLeft.column() && m_editIndex.column() <= bottomRight.column() &&
        !m_editor->hasFocus())
        itemDelegateForIndex(m_editIndex)->setEditorData(m_editor, m_editIndex);
    for (const PersistentEditor &p : m_persistent) {
        const QModelIndex i = p.index;
        if (p.editor && i.isValid() && i.parent() == topLeft.parent() && i.row() >= topLeft.row() &&
            i.row() <= bottomRight.row() && i.column() >= topLeft.column() && i.column() <= bottomRight.column() &&
            !p.editor->hasFocus())
            itemDelegateForIndex(i)->setEditorData(p.editor, i);
    }
    viewport()->update();
}

/*!
    Called when the rows \a start to \a end were inserted under \a parent. The default repaints the viewport.
*/
void QAbstractItemView::rowsInserted(const QModelIndex &parent, int start, int end) {
    static_cast<void>(parent);
    static_cast<void>(start);
    static_cast<void>(end);
    viewport()->update();
}

/*!
    Called before the rows \a start to \a end are removed from under \a parent. The default closes the editor
    if it is on one of them or on one of their descendants, without storing its value.
*/
void QAbstractItemView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) {
    // Persistent editors of the rows that go are destroyed.
    for (auto it = m_persistent.begin(); it != m_persistent.end();) {
        bool gone = false;
        for (QModelIndex i = it->index; i.isValid(); i = i.parent())
            gone = gone || (i.parent() == parent && i.row() >= start && i.row() <= end);
        if (gone && it->editor) {
            QWidget *editor = it->editor;
            const QModelIndex index = it->index;
            it = m_persistent.erase(it);
            QObject::disconnect(editor, &QObject::destroyed, this, nullptr);
            itemDelegateForIndex(index)->destroyEditor(editor, index);
            continue;
        }
        ++it;
    }
    if (m_editor && m_editIndex.isValid()) {
        for (QModelIndex i = m_editIndex; i.isValid(); i = i.parent()) {
            if (i.parent() == parent && i.row() >= start && i.row() <= end) {
                closeEditor(m_editor, QAbstractItemDelegate::RevertModelCache);
                break;
            }
        }
    }
    viewport()->update();
}

/*!
    Called when the selection changed: \a selected has the items that are selected now and were not before, and
    \a deselected those that are no longer selected. The default repaints the viewport.
*/
void QAbstractItemView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {
    static_cast<void>(selected);
    static_cast<void>(deselected);
    viewport()->update();
}

/*!
    Called when the current item changed from \a previous to \a current. The default stores the value of an
    editor that is open on another item and closes it, scrolls to the new item, and starts editing it if
    editTriggers() has \c CurrentChanged.
*/
void QAbstractItemView::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    static_cast<void>(previous);
    if (m_editor && m_editIndex != current) {
        QWidget *editor = m_editor;
        commitData(editor);
        closeEditor(editor, QAbstractItemDelegate::NoHint);
    }
    if (current.isValid()) {
        scrollTo(current);
        if (m_triggers & CurrentChanged)
            edit(current, CurrentChanged, nullptr);
    }
    viewport()->update();
}

/*!
    Closes \a editor, the editor of the view, and does what \a hint asks for: with \c EditNextItem or
    \c EditPreviousItem, the item after or before the one that was edited becomes the current item and gets an
    editor. The value of the editor is not stored here; see commitData(). Called when the delegate emits
    QAbstractItemDelegate::closeEditor().
*/
void QAbstractItemView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) {
    if (!editor || editor != m_editor)
        return;
    const QModelIndex index = m_editIndex;
    const bool hadFocus = editor->hasFocus() || QApplication::focusWidget() == editor;
    m_editor = nullptr;
    m_editIndex = QPersistentModelIndex();
    QAbstractItemDelegate *delegate = index.isValid() ? itemDelegateForIndex(index) : itemDelegate();
    editor->removeEventFilter(delegate);
    editor->hide();
    delegate->destroyEditor(editor, index);
    if (hadFocus)
        setFocus();
    viewport()->update();
    if (hint == QAbstractItemDelegate::EditNextItem || hint == QAbstractItemDelegate::EditPreviousItem) {
        const QModelIndex next =
            moveCursor(hint == QAbstractItemDelegate::EditNextItem ? MoveNext : MovePrevious, Qt::NoModifier);
        if (next.isValid() && next != index) {
            setCurrentIndex(next);
            edit(next, AllEditTriggers, nullptr);
        }
    }
}

/*!
    Stores the value of \a editor, the editor of the view, in the model, through the delegate. Called when the
    delegate emits QAbstractItemDelegate::commitData().
*/
void QAbstractItemView::commitData(QWidget *editor) {
    if (!editor || !m_model)
        return;
    const QModelIndex index = editor == m_editor ? QModelIndex(m_editIndex) : persistentIndexOf(editor);
    if (!index.isValid())
        return;
    itemDelegateForIndex(index)->setModelData(editor, m_model, index);
}

/*!
    \since 6.0

    Shows \a widget over the item at \a index, in place of how the item is drawn, and takes ownership of it. A widget
    that the item had is deleted; \c nullptr removes it. The widget follows the item as the view scrolls, and is
    deleted when its item goes.

    \code
    view->setIndexWidget(model->index(0, 1), new QProgressBar);
    \endcode
*/
void QAbstractItemView::setIndexWidget(const QModelIndex &index, QWidget *widget) {
    if (!index.isValid())
        return;
    for (auto it = m_indexWidgets.begin(); it != m_indexWidgets.end(); ++it) {
        if (it->index == index) {
            if (it->widget && it->widget != widget)
                it->widget->deleteLater();
            m_indexWidgets.erase(it);
            break;
        }
    }
    if (widget) {
        widget->setParent(viewport());
        m_indexWidgets.push_back({QPersistentModelIndex(index), widget});
    }
    placeIndexWidgets();
    viewport()->update();
}

/*!
    \since 6.0

    Returns the widget shown over the item at \a index, or \c nullptr.
*/
QWidget *QAbstractItemView::indexWidget(const QModelIndex &index) const {
    for (const IndexWidget &w : m_indexWidgets)
        if (w.index == index)
            return w.widget.data();
    return nullptr;
}

// Puts the index widgets over their items; drops those whose item went.
void QAbstractItemView::placeIndexWidgets() {
    for (auto it = m_indexWidgets.begin(); it != m_indexWidgets.end();) {
        if (!it->widget) {
            it = m_indexWidgets.erase(it);
            continue;
        }
        if (!it->index.isValid()) {
            it->widget->deleteLater();
            it = m_indexWidgets.erase(it);
            continue;
        }
        const QRect rect = visualRect(it->index);
        if (rect.isEmpty() || !rect.intersects(viewport()->rect())) {
            it->widget->hide();
        } else {
            if (it->widget->geometry() != rect)
                it->widget->setGeometry(rect);
            it->widget->show();
        }
        ++it;
    }
}

/*!
    \reimp

    Puts the index widgets over their items before the viewport is painted.
*/
bool QAbstractItemView::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::Paint && !m_indexWidgets.empty())
        placeIndexWidgets();
    if (event->type() == QEvent::Paint && !m_persistent.empty())
        placePersistentEditors();
    if (event->type() == QEvent::MouseMove) {
        const auto *move = static_cast<QMouseEvent *>(event);
        // entered() and viewportEntered() while mouse tracking is on.
        if (viewport()->hasMouseTracking() || hasMouseTracking()) {
            const QModelIndex under = indexAt(move->pos());
            if (under != QModelIndex(m_hover)) {
                m_hover = QPersistentModelIndex(under);
                if (under.isValid())
                    emit entered(under);
                else
                    emit viewportEntered();
            }
        }
        // Scrolling while the mouse is dragged at an edge.
        if (m_autoScroll && (move->buttons() & Qt::LeftButton)) {
            const QRect inner = viewport()->rect().adjusted(m_autoScrollMargin, m_autoScrollMargin, -m_autoScrollMargin,
                                                            -m_autoScrollMargin);
            if (!inner.contains(move->pos()))
                startAutoScroll();
        }
    }
    if (event->type() == QEvent::Leave)
        m_hover = QPersistentModelIndex();
    return QAbstractScrollArea::viewportEvent(event);
}

/*!
    \enum QAbstractItemView::DropIndicatorPosition
    \since 6.0

    Where a drop would go, relative to the item under the mouse.

    \value OnItem On the item.
    \value AboveItem Above the item.
    \value BelowItem Below the item.
    \value OnViewport On the empty part of the viewport.
*/

/*!
    \fn bool QAbstractItemView::hasAutoScroll() const
    \since 6.0

    Returns whether the view scrolls when the mouse is dragged near its edges. The default is \c true.
*/

/*!
    \fn void QAbstractItemView::setAutoScroll(bool enable)
    \since 6.0

    Sets whether the view scrolls when the mouse is dragged near its edges to \a enable.
*/

/*!
    \fn int QAbstractItemView::autoScrollMargin() const
    \since 6.0

    Returns how near the edge, in pixels, the mouse must be for the view to scroll. The default is 16.
*/

/*!
    \fn void QAbstractItemView::setAutoScrollMargin(int margin)
    \since 6.0

    Sets how near the edge the mouse must be for the view to scroll to \a margin pixels.
*/

/*!
    \fn bool QAbstractItemView::tabKeyNavigation() const
    \since 6.0

    Returns whether Tab and Shift+Tab move the current item instead of the focus. The default is \c false.
*/

/*!
    \fn void QAbstractItemView::setTabKeyNavigation(bool enable)
    \since 6.0

    Sets whether Tab and Shift+Tab move the current item to \a enable.
*/

/*!
    \fn bool QAbstractItemView::dragDropOverwriteMode() const
    \since 6.0

    Returns whether a drop takes the place of the item it is dropped on. Nt keeps the value.
*/

/*!
    \fn void QAbstractItemView::setDragDropOverwriteMode(bool overwrite)
    \since 6.0

    Sets whether a drop takes the place of the item it is dropped on to \a overwrite.
*/

/*!
    \fn void QAbstractItemView::resetVerticalScrollMode()
    \since 6.0

    Sets the vertical scroll mode back to its default, ScrollPerPixel.
*/

/*!
    \fn void QAbstractItemView::resetHorizontalScrollMode()
    \since 6.0

    Sets the horizontal scroll mode back to its default, ScrollPerPixel.
*/

/*!
    \fn int QAbstractItemView::updateThreshold() const
    \since 6.0

    Returns how many changed items the view repaints one by one before it repaints all. Nt keeps the value.
*/

/*!
    \fn void QAbstractItemView::setUpdateThreshold(int threshold)
    \since 6.0

    Sets how many changed items are repainted one by one to \a threshold.
*/

/*!
    \fn void QAbstractItemView::setState(State state)
    \since 6.0

    Sets the state of the view to \a state; while an editor is open the state is EditingState whatever was set.
*/

/*!
    \fn QAbstractItemView::DropIndicatorPosition QAbstractItemView::dropIndicatorPosition() const
    \since 6.0

    Returns where a drop would go; OnViewport, as Nt does not track it.
*/

/*!
    \fn QPoint QAbstractItemView::dirtyRegionOffset() const
    \since 6.0

    Returns how far the region to repaint was scrolled: none, as the view repaints at once.
*/

/*!
    \since 6.0

    Opens an editor on the item at \a index that stays open, unlike that of edit(): for a column of check boxes
    or combo boxes that is always shown. The editor writes its value to the model as the delegate says, and is
    destroyed by closePersistentEditor() or when its item goes.

    \sa closePersistentEditor(), isPersistentEditorOpen()
*/
void QAbstractItemView::openPersistentEditor(const QModelIndex &index) {
    if (!m_model || !index.isValid() || index.model() != m_model || isPersistentEditorOpen(index))
        return;
    QAbstractItemDelegate *delegate = itemDelegateForIndex(index);
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    QWidget *editor = delegate->createEditor(viewport(), option, index);
    if (!editor)
        return;
    editor->installEventFilter(delegate);
    QObject::connect(editor, &QObject::destroyed, this, &QAbstractItemView::editorDestroyed);
    delegate->setEditorData(editor, index);
    delegate->updateEditorGeometry(editor, option, index);
    m_persistent.push_back({QPersistentModelIndex(index), editor});
    editor->show();
}

/*!
    \since 6.0

    Closes the persistent editor of the item at \a index, without storing its value.

    \sa openPersistentEditor()
*/
void QAbstractItemView::closePersistentEditor(const QModelIndex &index) {
    for (auto it = m_persistent.begin(); it != m_persistent.end(); ++it) {
        if (it->index != index)
            continue;
        QWidget *editor = it->editor;
        m_persistent.erase(it);
        if (editor) {
            QObject::disconnect(editor, &QObject::destroyed, this, nullptr);
            QAbstractItemDelegate *delegate = itemDelegateForIndex(index);
            editor->removeEventFilter(delegate);
            editor->hide();
            delegate->destroyEditor(editor, index);
        }
        viewport()->update();
        return;
    }
}

/*!
    \since 6.0

    Returns \c true if the item at \a index has a persistent editor open.
*/
bool QAbstractItemView::isPersistentEditorOpen(const QModelIndex &index) const {
    return index.isValid() && std::any_of(m_persistent.begin(), m_persistent.end(),
                                          [&](const PersistentEditor &p) { return p.editor && p.index == index; });
}

QModelIndex QAbstractItemView::persistentIndexOf(const QWidget *editor) const {
    for (const PersistentEditor &p : m_persistent)
        if (editor && p.editor == editor)
            return p.index;
    return QModelIndex();
}

// Puts the persistent editors over their items; hides those whose item is not laid out.
void QAbstractItemView::placePersistentEditors() {
    for (const PersistentEditor &p : m_persistent) {
        if (!p.editor || !p.index.isValid())
            continue;
        QStyleOptionViewItem option;
        initViewItemOption(&option);
        option.rect = visualRect(p.index);
        if (option.rect.isEmpty() || !option.rect.intersects(viewport()->rect())) {
            p.editor->hide();
            continue;
        }
        itemDelegateForIndex(p.index)->updateEditorGeometry(p.editor, option, p.index);
        p.editor->show();
    }
}

/*!
    \since 6.0

    Forgets \a editor, an editor that was destroyed.
*/
void QAbstractItemView::editorDestroyed(QObject *editor) {
    m_persistent.erase(std::remove_if(m_persistent.begin(), m_persistent.end(),
                                      [editor](const PersistentEditor &p) {
                                          return !p.editor || static_cast<QObject *>(p.editor.data()) == editor;
                                      }),
                       m_persistent.end());
    if (static_cast<QObject *>(m_editor.data()) == editor) {
        m_editor = nullptr;
        m_editIndex = QPersistentModelIndex();
    }
}

/*!
    \since 6.0

    Gives the open editors the values of their items from the model.
*/
void QAbstractItemView::updateEditorData() {
    if (m_editor && m_editIndex.isValid())
        itemDelegateForIndex(m_editIndex)->setEditorData(m_editor, m_editIndex);
    for (const PersistentEditor &p : m_persistent)
        if (p.editor && p.index.isValid())
            itemDelegateForIndex(p.index)->setEditorData(p.editor, p.index);
}

/*!
    \since 6.0

    Makes the next item of the root of the view whose text starts with \a search current, from the current one.
    Key presses that come within QApplication::keyboardInputInterval() of each other add to the text searched
    for; the same letter typed again moves on to the next item that starts with it. An empty \a search starts
    again.
*/
void QAbstractItemView::keyboardSearch(const QString &search) {
    if (!m_model || search.isEmpty()) {
        m_search.clear();
        return;
    }
    const bool continuing =
        m_searchTimer.isValid() && m_searchTimer.elapsed() < QGuiApplication::styleHints()->keyboardInputInterval();
    m_searchTimer.start();
    const bool repeated = continuing && m_search.size() == 1 && search == m_search;
    m_search = continuing && !repeated ? m_search + search : search;
    const int rows = m_model->rowCount(m_root);
    if (rows == 0)
        return;
    const QModelIndex current = currentIndex();
    const int column = current.isValid() ? current.column() : 0;
    int start = current.isValid() && current.parent() == QModelIndex(m_root) ? current.row() : 0;
    if (current.isValid() && (!continuing || repeated))
        start = (start + 1) % rows;
    for (int n = 0; n < rows; ++n) {
        const QModelIndex index = m_model->index((start + n) % rows, column, m_root);
        if (isIndexHidden(index) || !(m_model->flags(index) & Qt::ItemIsEnabled))
            continue;
        if (m_model->data(index, Qt::DisplayRole).toString().startsWith(m_search, Qt::CaseInsensitive)) {
            setCurrentIndex(index);
            return;
        }
    }
}

/*!
    \since 6.0

    Returns the height that the row \a row of the root of the view wants: the largest size hint of its items.
    Returns -1 if there is no model or no such row.
*/
int QAbstractItemView::sizeHintForRow(int row) const {
    if (!m_model || row < 0 || row >= m_model->rowCount(m_root))
        return -1;
    int height = 0;
    for (int column = 0; column < m_model->columnCount(m_root); ++column) {
        const QModelIndex index = m_model->index(row, column, m_root);
        if (!isIndexHidden(index))
            height = std::max(height, sizeHintForIndex(index).height());
    }
    return height;
}

/*!
    \since 6.0

    Returns the width that the column \a column of the root of the view wants: the largest size hint of its
    items. Returns -1 if there is no model or no such column.
*/
int QAbstractItemView::sizeHintForColumn(int column) const {
    if (!m_model || column < 0 || column >= m_model->columnCount(m_root))
        return -1;
    int width = 0;
    for (int row = 0; row < m_model->rowCount(m_root); ++row) {
        const QModelIndex index = m_model->index(row, column, m_root);
        if (!isIndexHidden(index))
            width = std::max(width, sizeHintForIndex(index).width());
    }
    return width;
}

/*!
    \since 6.0

    Returns the selected items, from the selection model.
*/
QModelIndexList QAbstractItemView::selectedIndexes() const {
    return m_selection ? m_selection->selectedIndexes() : QModelIndexList();
}

/*!
    \since 6.0

    Returns how a click or key press \a event on the item at \a index changes the selection, as selectionMode()
    and selectionBehavior() say: with ExtendedSelection, Control toggles and Shift extends.
*/
QItemSelectionModel::SelectionFlags QAbstractItemView::selectionCommand(const QModelIndex &index,
                                                                        const QEvent *event) const {
    QItemSelectionModel::SelectionFlags behavior;
    if (m_selectionBehavior == SelectRows)
        behavior = QItemSelectionModel::Rows;
    else if (m_selectionBehavior == SelectColumns)
        behavior = QItemSelectionModel::Columns;
    Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
    if (event && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
                  event->type() == QEvent::MouseMove))
        modifiers = static_cast<const QMouseEvent *>(event)->modifiers();
    else if (event && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease))
        modifiers = static_cast<const QKeyEvent *>(event)->modifiers();
    switch (m_selectionMode) {
    case NoSelection:
        return QItemSelectionModel::NoUpdate;
    case SingleSelection:
        return QItemSelectionModel::ClearAndSelect | behavior;
    case MultiSelection:
        return index.isValid() ? QItemSelectionModel::Toggle | behavior
                               : QItemSelectionModel::SelectionFlags(QItemSelectionModel::NoUpdate);
    case ExtendedSelection:
        if (modifiers & Qt::ControlModifier)
            return QItemSelectionModel::Toggle | behavior;
        if (modifiers & Qt::ShiftModifier)
            return QItemSelectionModel::SelectCurrent | behavior;
        return QItemSelectionModel::ClearAndSelect | behavior;
    case ContiguousSelection:
        if (modifiers & Qt::ShiftModifier)
            return QItemSelectionModel::SelectCurrent | behavior;
        return QItemSelectionModel::ClearAndSelect | behavior;
    }
    return QItemSelectionModel::NoUpdate;
}

/*!
    \since 6.0

    Selects the items of the root of the view whose rectangles, from visualRect(), meet \a rect, in the viewport,
    as \a command says.
*/
void QAbstractItemView::setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command) {
    if (!m_model || !m_selection)
        return;
    QItemSelection selection;
    for (int row = 0; row < m_model->rowCount(m_root); ++row) {
        for (int column = 0; column < m_model->columnCount(m_root); ++column) {
            const QModelIndex index = m_model->index(row, column, m_root);
            if (!isIndexHidden(index) && visualRect(index).intersects(rect))
                selection.select(index, index);
        }
    }
    m_selection->select(selection, command);
}

/*!
    \since 6.0

    Returns the region of the viewport that the items of \a selection cover.
*/
QRegion QAbstractItemView::visualRegionForSelection(const QItemSelection &selection) const {
    QRegion region;
    for (const QModelIndex &index : selection.indexes())
        region += visualRect(index);
    return region;
}

/*!
    \since 6.0

    Returns how far the view is scrolled to the right, in pixels.
*/
int QAbstractItemView::horizontalOffset() const { return horizontalScrollBar()->value(); }

/*!
    \since 6.0

    Returns how far the view is scrolled down, in pixels.
*/
int QAbstractItemView::verticalOffset() const { return verticalScrollBar()->value(); }

/*!
    \since 6.0

    Returns \c true if the item at \a index is hidden in the view. The default returns \c false.
*/
bool QAbstractItemView::isIndexHidden(const QModelIndex &index) const {
    (void)index;
    return false;
}

/*!
    \since 6.0

    Lays the items out again with doItemsLayout() once control returns to the event loop.
*/
void QAbstractItemView::scheduleDelayedItemsLayout() {
    if (m_layoutScheduled)
        return;
    m_layoutScheduled = true;
    QTimer::singleShot(0, this, [this] { executeDelayedItemsLayout(); });
}

/*!
    \since 6.0

    Lays the items out now if scheduleDelayedItemsLayout() asked for it.
*/
void QAbstractItemView::executeDelayedItemsLayout() {
    if (!m_layoutScheduled)
        return;
    m_layoutScheduled = false;
    doItemsLayout();
}

/*!
    \since 6.0

    Repaints \a region of the viewport.
*/
void QAbstractItemView::setDirtyRegion(const QRegion &region) { viewport()->update(region); }

/*!
    \since 6.0

    Would move the region to repaint by \a dx and \a dy as the view scrolls; the view repaints at once, so this
    does nothing.
*/
void QAbstractItemView::scrollDirtyRegion(int dx, int dy) {
    (void)dx;
    (void)dy;
}

/*!
    \since 6.0

    Starts scrolling the view while the mouse is held near an edge; doAutoScroll() does each step.
*/
void QAbstractItemView::startAutoScroll() {
    if (!m_autoScrollTimer.isActive())
        m_autoScrollTimer.start();
}

/*!
    \since 6.0

    Stops the scrolling that startAutoScroll() started.
*/
void QAbstractItemView::stopAutoScroll() { m_autoScrollTimer.stop(); }

/*!
    \since 6.0

    Scrolls the view one step towards the edge that the mouse is near, while the left button is held; stops when
    the button is released or the mouse moves away from the edges.
*/
void QAbstractItemView::doAutoScroll() {
    if (!(QGuiApplication::mouseButtons() & Qt::LeftButton)) {
        stopAutoScroll();
        return;
    }
    const QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    const QRect area = viewport()->rect();
    QScrollBar *v = verticalScrollBar();
    QScrollBar *h = horizontalScrollBar();
    bool moved = false;
    if (pos.y() < area.top() + m_autoScrollMargin) {
        v->setValue(v->value() - v->singleStep());
        moved = true;
    } else if (pos.y() > area.bottom() - m_autoScrollMargin) {
        v->setValue(v->value() + v->singleStep());
        moved = true;
    }
    if (pos.x() < area.left() + m_autoScrollMargin) {
        h->setValue(h->value() - h->singleStep());
        moved = true;
    } else if (pos.x() > area.right() - m_autoScrollMargin) {
        h->setValue(h->value() + h->singleStep());
        moved = true;
    }
    if (!moved)
        stopAutoScroll();
}

/*!
    \reimp
    \since 6.0

    A printable key that no subclass took goes to keyboardSearch().
*/
void QAbstractItemView::keyPressEvent(QKeyEvent *event) {
    const QString text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) &&
        m_model) {
        keyboardSearch(text);
        event->accept();
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

/*!
    \reimp
    \since 6.0

    With tabKeyNavigation(), Tab and Shift+Tab move the current item instead of the focus.
*/
bool QAbstractItemView::focusNextPrevChild(bool next) {
    if (m_tabKeyNavigation && isEnabled() && m_model && state() != EditingState) {
        const QModelIndex index = moveCursor(next ? MoveNext : MovePrevious, Qt::NoModifier);
        if (index.isValid()) {
            setCurrentIndex(index);
            return true;
        }
    }
    return QAbstractScrollArea::focusNextPrevChild(next);
}

/*!
    \since 6.0

    Called when the user acts on the horizontal scroll bar with \a action. The default does nothing.
*/
void QAbstractItemView::horizontalScrollbarAction(int action) { (void)action; }

/*!
    \since 6.0

    Called when the user acts on the vertical scroll bar with \a action. The default does nothing.
*/
void QAbstractItemView::verticalScrollbarAction(int action) { (void)action; }

/*!
    \since 6.0

    Called when the horizontal scroll bar moves to \a value. The default repaints the viewport.
*/
void QAbstractItemView::horizontalScrollbarValueChanged(int value) {
    (void)value;
    viewport()->update();
}

/*!
    \since 6.0

    Called when the vertical scroll bar moves to \a value. The default repaints the viewport.
*/
void QAbstractItemView::verticalScrollbarValueChanged(int value) {
    (void)value;
    viewport()->update();
}

#include "moc_qabstractitemview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
