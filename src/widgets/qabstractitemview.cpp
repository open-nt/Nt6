// MIT License - see LICENSE
#include <QtWidgets/qabstractitemview.h>

#include <QtGui/qevent.h>
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
    viewport(). QTreeView is the view of Nt Widgets that does; QListWidget and QTableWidget derive from this
    class too, but keep their own items and use none of what has to do with a model.

    \section1 Editing

    edit() opens an editor on an item, if the item is Qt::ItemIsEditable. The user starts one in the ways that
    editTriggers() allows: by default with a double click, with F2, or by starting to type. The delegate makes
    the editor and writes its value to the model when the user is done; see QStyledItemDelegate.

    \differences
    The view has one editor at a time: there are no persistent editors and no index widgets. There is no drag
    and drop, no automatic scrolling while the mouse is dragged, and no keyboard search. visualRect(),
    indexAt(), scrollTo() and moveCursor() have defaults that do nothing, where Qt has pure virtual functions,
    so that QListWidget and QTableWidget, which have no model, need not implement them.
    \enddifferences

    \sa QTreeView, QTreeWidget, QStyledItemDelegate, QItemSelectionModel
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
    \value EditingState An editor is open.
*/

/*!
    Constructs an item view with the given \a parent. It allows one selected item.
*/
QAbstractItemView::QAbstractItemView(QWidget *parent) : QAbstractScrollArea(parent) {}

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
    option->widget = this;
}

/*!
    Returns \c EditingState while an editor is open.
*/
QAbstractItemView::State QAbstractItemView::state() const { return m_editor ? EditingState : NoState; }

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
    if (!editor || editor != m_editor || !m_model || !m_editIndex.isValid())
        return;
    const QModelIndex index = m_editIndex;
    itemDelegateForIndex(index)->setModelData(editor, m_model, index);
}

#include "moc_qabstractitemview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
