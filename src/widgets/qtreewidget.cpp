// MIT License - see LICENSE
#include <QtWidgets/qtreewidget.h>

#include <QtCore/qregularexpression.h>
#include <QtWidgets/qheaderview.h>

#include <algorithm>

/*!
    \class QTreeWidgetItem
    \brief The QTreeWidgetItem class provides an item for use with the QTreeWidget convenience class.

    An item is a row of a QTreeWidget: it has a text and other data for each column, and child items below it.
    An item that is constructed with a tree or with another item as its parent is added to it at once:

    \code
    auto *fruit = new QTreeWidgetItem(tree, {"Fruit", "2"});
    new QTreeWidgetItem(fruit, {"Apple", "red"});
    new QTreeWidgetItem(fruit, {"Pear", "green"});
    fruit->setExpanded(true);
    \endcode

    setText(), setCheckState(), setFont(), setForeground() and their relatives set the data of a column, and
    setData() any role of it. setFlags() says what the user can do with the item: an item is edited in place
    only if it has Qt::ItemIsEditable. A parent owns its children, and the tree its top-level items: deleting an
    item deletes the items below it and takes it out of the tree.

    Subclass the item to keep your own data with it, or to sort in your own way by reimplementing operator<().

    \differences
    There is no icon, as Nt has no QIcon: give Qt::DecorationRole a QPixmap or a QColor with setData(). There
    are no \c statusTip(), \c whatsThis() and \c sizeHint() functions (use setData()), an item cannot be
    hidden or span the columns, and there are no stream operators. A check state is not propagated to the
    parent or the children by Qt::ItemIsAutoTristate.
    \enddifferences

    \sa QTreeWidget
*/

/*!
    \class QTreeWidget
    \brief The QTreeWidget class provides a tree view that uses a predefined tree model.

    A QTreeWidget is a QTreeView that keeps its own items, QTreeWidgetItem objects, so that a tree can be
    filled without writing a model:

    \code
    auto *tree = new QTreeWidget;
    tree->setHeaderLabels({"Name", "Size"});
    auto *folder = new QTreeWidgetItem(tree, {"Documents", ""});
    new QTreeWidgetItem(folder, {"notes.txt", "2 KiB"});
    tree->expandAll();
    connect(tree, &QTreeWidget::itemActivated, this, &Window::open);
    \endcode

    Everything that QTreeView does is there: columns with a header, sorting by a click on the header with
    setSortingEnabled(), selection, and editing in place through the delegate for the items that are
    Qt::ItemIsEditable. The signals tell about items instead of model indexes, and itemChanged() is emitted
    for every change of an item.

    Use QTreeView with a model of your own when the data is already somewhere else, or is large.

    \differences
    Items are not sorted as they are inserted while sorting is enabled: they are sorted when the user clicks
    the header or sortItems() is called. There are no item widgets (\c setItemWidget()), no persistent
    editors, no drag and drop, and no \c headerItem() that can be replaced: setHeaderLabels() sets the header.
    \enddifferences

    \sa QTreeWidgetItem, QTreeView
*/

namespace {
// The column that operator<() of the items compares while sortChildren() runs; -1 otherwise.
thread_local int t_sortColumn = -1;

bool isNumber(const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Double:
    case QMetaType::Float:
        return true;
    default:
        return false;
    }
}
} // namespace

namespace QtPrivate {

// The model of a QTreeWidget: an index holds a pointer to its item.
class TreeModel : public QAbstractItemModel {
public:
    explicit TreeModel(QTreeWidget *view) : QAbstractItemModel(view), m_view(view) {}

    QTreeWidgetItem *item(const QModelIndex &index) const {
        return index.isValid() ? static_cast<QTreeWidgetItem *>(index.internalPointer()) : m_view->m_root;
    }
    QModelIndex indexOf(const QTreeWidgetItem *item, int column) const {
        if (!item || item == m_view->m_root || item->m_view != m_view || !item->m_parent || column < 0 ||
            column >= columnCount())
            return QModelIndex();
        const int row = item->m_parent->indexOfChild(const_cast<QTreeWidgetItem *>(item)); // NOLINT
        return row < 0 ? QModelIndex() : createIndex(row, column, item);
    }

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        if (row < 0 || column < 0 || column >= columnCount() || parent.column() > 0)
            return QModelIndex();
        const QTreeWidgetItem *p = item(parent);
        return row < p->childCount() ? createIndex(row, column, p->m_children[static_cast<std::size_t>(row)]) : QModelIndex();
    }
    QModelIndex parent(const QModelIndex &child) const override {
        if (!child.isValid())
            return QModelIndex();
        return indexOf(item(child)->m_parent, 0);
    }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.column() > 0 ? 0 : item(parent)->childCount();
    }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        static_cast<void>(parent);
        return m_view->m_headerItem->columnCount();
    }
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.column() > 0)
            return false;
        const QTreeWidgetItem *p = item(parent);
        if (parent.isValid() && p->m_policy == QTreeWidgetItem::ShowIndicator)
            return true;
        if (parent.isValid() && p->m_policy == QTreeWidgetItem::DontShowIndicator)
            return false;
        return p->childCount() > 0;
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        return index.isValid() ? item(index)->data(index.column(), role) : QVariant();
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
        if (!index.isValid())
            return false;
        item(index)->setData(index.column(), role, value);
        return true;
    }
    Qt::ItemFlags flags(const QModelIndex &index) const override {
        return index.isValid() ? item(index)->flags() : Qt::ItemFlags();
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation != Qt::Horizontal || section < 0 || section >= columnCount())
            return QVariant();
        QVariant value = m_view->m_headerItem->data(section, role);
        if (!value.isValid() && role == Qt::DisplayRole)
            return QString::number(section + 1);
        return value;
    }
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override { sortBelow(m_view->m_root, column, order); }

    // Sorts the items below `top`, and keeps the indexes that others hold pointing at their items.
    void sortBelow(QTreeWidgetItem *top, int column, Qt::SortOrder order) {
        if (column < 0 || column >= columnCount())
            return;
        emit layoutAboutToBeChanged();
        const QModelIndexList before = persistentIndexList();
        top->sortRecursively(column, order);
        QModelIndexList after;
        for (const QModelIndex &old : before)
            after.append(indexOf(static_cast<QTreeWidgetItem *>(old.internalPointer()), old.column()));
        changePersistentIndexList(before, after);
        emit layoutChanged();
    }

    void beginInsert(QTreeWidgetItem *parent, int first, int last) { beginInsertRows(indexOf(parent, 0), first, last); }
    void endInsert() { endInsertRows(); }
    void beginRemove(QTreeWidgetItem *parent, int first, int last) { beginRemoveRows(indexOf(parent, 0), first, last); }
    void endRemove() { endRemoveRows(); }
    void beginReset() { beginResetModel(); }
    void endReset() { endResetModel(); }
    void itemDataChanged(QTreeWidgetItem *item, int column) {
        const QModelIndex index = indexOf(item, column);
        if (index.isValid())
            emit dataChanged(index, index, QList<int>());
    }
    void itemChangedInAllColumns(QTreeWidgetItem *item) {
        const QModelIndex first = indexOf(item, 0);
        if (first.isValid())
            emit dataChanged(first, first.sibling(first.row(), columnCount() - 1), QList<int>());
    }
    void headerChanged(int column) { emit headerDataChanged(Qt::Horizontal, column, column); }
    void setColumns(int columns) {
        const int old = columnCount();
        if (columns == old)
            return;
        if (columns > old) {
            beginInsertColumns(QModelIndex(), old, columns - 1);
            m_view->m_headerItem->m_values.resize(static_cast<std::size_t>(columns));
            endInsertColumns();
        } else {
            beginRemoveColumns(QModelIndex(), columns, old - 1);
            m_view->m_headerItem->m_values.resize(static_cast<std::size_t>(columns));
            endRemoveColumns();
        }
    }

private:
    QTreeWidget *m_view;
};

} // namespace QtPrivate

using QtPrivate::TreeModel;

/*!
    Constructs an item of the type \a type that is in no tree yet.

    \sa type()
*/
QTreeWidgetItem::QTreeWidgetItem(int type) : m_type(type) {}

/*!
    \overload

    Constructs an item of the type \a type with a text for each column from \a strings.
*/
QTreeWidgetItem::QTreeWidgetItem(const QStringList &strings, int type) : m_type(type) {
    for (int i = 0; i < strings.size(); ++i)
        QTreeWidgetItem::setData(i, Qt::DisplayRole, strings.at(i)); // not virtual in a constructor
}

/*!
    \overload

    Constructs an item of the type \a type and adds it to \a treeview as its last top-level item.
*/
QTreeWidgetItem::QTreeWidgetItem(QTreeWidget *treeview, int type) : m_type(type) {
    if (treeview)
        treeview->addTopLevelItem(this);
}

/*!
    \overload

    Constructs an item of the type \a type with the texts \a strings and adds it to \a treeview as its last
    top-level item.
*/
QTreeWidgetItem::QTreeWidgetItem(QTreeWidget *treeview, const QStringList &strings, int type) : m_type(type) {
    for (int i = 0; i < strings.size(); ++i)
        QTreeWidgetItem::setData(i, Qt::DisplayRole, strings.at(i)); // not virtual in a constructor
    if (treeview)
        treeview->addTopLevelItem(this);
}

/*!
    \overload

    Constructs an item of the type \a type and adds it to \a parent as its last child.
*/
QTreeWidgetItem::QTreeWidgetItem(QTreeWidgetItem *parent, int type) : m_type(type) {
    if (parent)
        parent->addChild(this);
}

/*!
    \overload

    Constructs an item of the type \a type with the texts \a strings and adds it to \a parent as its last
    child.
*/
QTreeWidgetItem::QTreeWidgetItem(QTreeWidgetItem *parent, const QStringList &strings, int type) : m_type(type) {
    for (int i = 0; i < strings.size(); ++i)
        QTreeWidgetItem::setData(i, Qt::DisplayRole, strings.at(i)); // not virtual in a constructor
    if (parent)
        parent->addChild(this);
}

/*!
    Constructs a copy of \a other: its data, flags and type, but not its children, its parent or its tree.

    \sa clone()
*/
QTreeWidgetItem::QTreeWidgetItem(const QTreeWidgetItem &other)
    : m_values(other.m_values), m_flags(other.m_flags), m_policy(other.m_policy), m_type(other.m_type) {}

/*!
    Destroys the item and its children, and takes it out of its parent or its tree.
*/
QTreeWidgetItem::~QTreeWidgetItem() {
    if (m_parent) {
        const int index = m_parent->indexOfChild(this);
        if (index >= 0)
            m_parent->takeChild(index);
    }
    // The children are no longer part of a tree: nobody is told about each of them going.
    std::vector<QTreeWidgetItem *> children;
    children.swap(m_children);
    for (QTreeWidgetItem *child : children) {
        child->m_parent = nullptr;
        child->setView(nullptr);
        delete child;
    }
}

/*!
    Returns a new item with a copy of the data of this item and copies of all the items below it. It is in no
    tree.
*/
QTreeWidgetItem *QTreeWidgetItem::clone() const {
    auto *copy = new QTreeWidgetItem(*this);
    for (const QTreeWidgetItem *child : m_children) {
        QTreeWidgetItem *c = child->clone();
        c->m_parent = copy;
        copy->m_children.push_back(c);
    }
    return copy;
}

/*!
    Gives this item the data and the flags of \a other, and returns a reference to it. Its children, its
    parent, its tree and its type stay as they are.
*/
QTreeWidgetItem &QTreeWidgetItem::operator=(const QTreeWidgetItem &other) {
    if (this == &other)
        return *this;
    m_values = other.m_values;
    m_flags = other.m_flags;
    m_policy = other.m_policy;
    if (m_view)
        m_view->treeModel()->itemChangedInAllColumns(this);
    return *this;
}

void QTreeWidgetItem::setView(QTreeWidget *view) {
    // Without recursion: a tree can be deep.
    std::vector<QTreeWidgetItem *> pending{this};
    while (!pending.empty()) {
        QTreeWidgetItem *item = pending.back();
        pending.pop_back();
        item->m_view = view;
        pending.insert(pending.end(), item->m_children.begin(), item->m_children.end());
    }
}

/*!
    \fn QTreeWidget *QTreeWidgetItem::treeWidget() const

    Returns the tree that the item is in, or \c nullptr.
*/

/*!
    Returns the parent of the item, or \c nullptr for a top-level item and for an item that has none.

    \sa child()
*/
QTreeWidgetItem *QTreeWidgetItem::parent() const {
    return m_view && m_parent == m_view->invisibleRootItem() ? nullptr : m_parent;
}

/*!
    Returns the child at position \a index, or \c nullptr if there is none.

    \sa childCount(), indexOfChild()
*/
QTreeWidgetItem *QTreeWidgetItem::child(int index) const {
    return index >= 0 && index < childCount() ? m_children[static_cast<std::size_t>(index)] : nullptr;
}

/*!
    \fn int QTreeWidgetItem::childCount() const

    Returns the number of children of the item.
*/

/*!
    Returns the position of \a child among the children of the item, or -1 if it is not one of them.
*/
int QTreeWidgetItem::indexOfChild(QTreeWidgetItem *child) const {
    const auto it = std::find(m_children.begin(), m_children.end(), child);
    return it == m_children.end() ? -1 : static_cast<int>(it - m_children.begin());
}

/*!
    \fn int QTreeWidgetItem::columnCount() const

    Returns the number of columns that the item has data for.
*/

/*!
    Adds \a child as the last child of the item, which takes ownership of it. Nothing happens if \a child is
    \c nullptr or has a parent already.

    \sa insertChild(), takeChild()
*/
void QTreeWidgetItem::addChild(QTreeWidgetItem *child) { insertChild(childCount(), child); }

/*!
    Inserts \a child at position \a index of the children of the item, which takes ownership of it. Nothing
    happens if \a child is \c nullptr, has a parent or is in a tree already, or if \a index is out of range.

    \sa addChild()
*/
void QTreeWidgetItem::insertChild(int index, QTreeWidgetItem *child) {
    if (!child || child == this || child->m_parent || child->m_view || index < 0 || index > childCount())
        return;
    // An item cannot become a child of one of its own descendants.
    for (const QTreeWidgetItem *p = this; p; p = p->m_parent) {
        if (p == child)
            return;
    }
    if (m_view)
        m_view->treeModel()->beginInsert(this, index, index);
    m_children.insert(m_children.begin() + index, child);
    child->m_parent = this;
    child->setView(m_view);
    if (m_view)
        m_view->treeModel()->endInsert();
}

/*!
    Removes \a child from the children of the item without deleting it: the caller owns it afterwards.

    \sa takeChild()
*/
void QTreeWidgetItem::removeChild(QTreeWidgetItem *child) { takeChild(indexOfChild(child)); }

/*!
    Removes the child at position \a index and returns it, or \c nullptr if there is none. The caller owns the
    item, which keeps its own children.

    \sa removeChild(), takeChildren()
*/
QTreeWidgetItem *QTreeWidgetItem::takeChild(int index) {
    if (index < 0 || index >= childCount())
        return nullptr;
    QTreeWidgetItem *child = m_children[static_cast<std::size_t>(index)];
    if (m_view)
        m_view->treeModel()->beginRemove(this, index, index);
    m_children.erase(m_children.begin() + index);
    child->m_parent = nullptr;
    child->setView(nullptr);
    if (m_view)
        m_view->treeModel()->endRemove();
    return child;
}

/*!
    Adds the items in \a children as the last children of the item.

    \sa addChild()
*/
void QTreeWidgetItem::addChildren(const QList<QTreeWidgetItem *> &children) { insertChildren(childCount(), children); }

/*!
    Inserts the items in \a children at position \a index of the children of the item, in their order.

    \sa insertChild()
*/
void QTreeWidgetItem::insertChildren(int index, const QList<QTreeWidgetItem *> &children) {
    if (index < 0 || index > childCount())
        return;
    for (QTreeWidgetItem *child : children) {
        const int before = childCount();
        insertChild(index, child);
        if (childCount() > before)
            ++index;
    }
}

/*!
    Removes all children of the item and returns them. The caller owns them.

    \sa takeChild()
*/
QList<QTreeWidgetItem *> QTreeWidgetItem::takeChildren() {
    QList<QTreeWidgetItem *> taken;
    if (m_children.empty())
        return taken;
    if (m_view)
        m_view->treeModel()->beginRemove(this, 0, childCount() - 1);
    for (QTreeWidgetItem *child : m_children) {
        child->m_parent = nullptr;
        child->setView(nullptr);
        taken.append(child);
    }
    m_children.clear();
    if (m_view)
        m_view->treeModel()->endRemove();
    return taken;
}

/*!
    \fn Qt::ItemFlags QTreeWidgetItem::flags() const

    Returns what the user can do with the item. A new item is enabled, selectable and checkable.

    \sa setFlags()
*/

/*!
    Sets what the user can do with the item to \a flags. Add Qt::ItemIsEditable to let the user edit it:

    \code
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    \endcode

    \sa flags()
*/
void QTreeWidgetItem::setFlags(Qt::ItemFlags flags) {
    if (m_flags == flags)
        return;
    m_flags = flags;
    if (m_view)
        m_view->treeModel()->itemChangedInAllColumns(this);
}

/*!
    \fn bool QTreeWidgetItem::isDisabled() const

    Returns \c true if the item is not enabled: the user cannot select or change it.

    \sa setDisabled()
*/

/*!
    Disables the item if \a disabled is \c true, and enables it otherwise. Its children are not changed.

    \sa isDisabled(), setFlags()
*/
void QTreeWidgetItem::setDisabled(bool disabled) {
    setFlags(disabled ? m_flags & ~Qt::ItemFlags(Qt::ItemIsEnabled) : m_flags | Qt::ItemIsEnabled);
}

/*!
    Returns \c true if the item is selected in its tree; otherwise returns \c false.

    \sa setSelected()
*/
bool QTreeWidgetItem::isSelected() const {
    return m_view && m_view->selectionModel() && m_view->selectionModel()->isSelected(m_view->indexFromItem(this));
}

/*!
    Selects the item in its tree if \a select is \c true, and deselects it otherwise. Nothing happens for an
    item that is in no tree.

    \sa isSelected()
*/
void QTreeWidgetItem::setSelected(bool select) {
    if (!m_view || !m_view->selectionModel())
        return;
    m_view->selectionModel()->select(m_view->indexFromItem(this),
                                     QItemSelectionModel::SelectionFlags(select ? QItemSelectionModel::Select
                                                                                : QItemSelectionModel::Deselect) |
                                         QItemSelectionModel::Rows);
}

/*!
    Returns \c true if the item is expanded in its tree; otherwise returns \c false.

    \sa setExpanded()
*/
bool QTreeWidgetItem::isExpanded() const { return m_view && m_view->isExpanded(m_view->indexFromItem(this)); }

/*!
    Expands the item in its tree if \a expand is \c true, and collapses it otherwise. Nothing happens for an
    item that is in no tree, or that has no children.

    \sa isExpanded()
*/
void QTreeWidgetItem::setExpanded(bool expand) {
    if (m_view)
        m_view->setExpanded(m_view->indexFromItem(this), expand);
}

/*!
    \fn QTreeWidgetItem::ChildIndicatorPolicy QTreeWidgetItem::childIndicatorPolicy() const

    Returns when the item has an arrow for opening it.

    \sa setChildIndicatorPolicy()
*/

/*!
    Sets when the item has an arrow for opening it to \a policy.

    \sa childIndicatorPolicy()
*/
void QTreeWidgetItem::setChildIndicatorPolicy(ChildIndicatorPolicy policy) {
    if (m_policy == policy)
        return;
    m_policy = policy;
    if (m_view)
        m_view->treeModel()->itemChangedInAllColumns(this);
}

/*!
    \fn QString QTreeWidgetItem::text(int column) const

    Returns the text of the item in the column \a column.

    \sa setText()
*/

/*!
    \fn void QTreeWidgetItem::setText(int column, const QString &text)

    Sets the text of the item in the column \a column to \a text.

    \sa text()
*/

/*!
    \fn QString QTreeWidgetItem::toolTip(int column) const

    Returns the tool tip of the item in the column \a column.

    \sa setToolTip()
*/

/*!
    \fn void QTreeWidgetItem::setToolTip(int column, const QString &toolTip)

    Sets the tool tip of the item in the column \a column to \a toolTip.

    \sa toolTip()
*/

/*!
    Returns the font of the text in the column \a column: a default font unless setFont() set one.

    \sa setFont()
*/
QFont QTreeWidgetItem::font(int column) const {
    const QVariant value = data(column, Qt::FontRole);
    return value.isValid() ? value.value<QFont>() : QFont();
}

/*!
    Sets the font of the text in the column \a column to \a font.

    \sa font()
*/
void QTreeWidgetItem::setFont(int column, const QFont &font) { setData(column, Qt::FontRole, QVariant::fromValue(font)); }

/*!
    \fn int QTreeWidgetItem::textAlignment(int column) const

    Returns the alignment of the text in the column \a column, a combination of Qt::AlignmentFlag values, or 0
    if none was set.

    \sa setTextAlignment()
*/

/*!
    \fn void QTreeWidgetItem::setTextAlignment(int column, int alignment)

    Sets the alignment of the text in the column \a column to \a alignment, a combination of
    Qt::AlignmentFlag values.

    \sa textAlignment()
*/

/*!
    Returns the background of the item in the column \a column: an empty brush unless setBackground() set one.

    \sa setBackground()
*/
QBrush QTreeWidgetItem::background(int column) const {
    const QVariant value = data(column, Qt::BackgroundRole);
    return value.isValid() ? value.value<QBrush>() : QBrush();
}

/*!
    Sets the background of the item in the column \a column to \a brush.

    \sa background()
*/
void QTreeWidgetItem::setBackground(int column, const QBrush &brush) {
    setData(column, Qt::BackgroundRole, brush.style() == Qt::NoBrush ? QVariant() : QVariant::fromValue(brush));
}

/*!
    Returns the brush that the text in the column \a column is drawn with: an empty brush unless
    setForeground() set one.

    \sa setForeground()
*/
QBrush QTreeWidgetItem::foreground(int column) const {
    const QVariant value = data(column, Qt::ForegroundRole);
    return value.isValid() ? value.value<QBrush>() : QBrush();
}

/*!
    Sets the brush that the text in the column \a column is drawn with to \a brush.

    \sa foreground()
*/
void QTreeWidgetItem::setForeground(int column, const QBrush &brush) {
    setData(column, Qt::ForegroundRole, brush.style() == Qt::NoBrush ? QVariant() : QVariant::fromValue(brush));
}

/*!
    \fn Qt::CheckState QTreeWidgetItem::checkState(int column) const

    Returns the check state of the item in the column \a column.

    \sa setCheckState()
*/

/*!
    \fn void QTreeWidgetItem::setCheckState(int column, Qt::CheckState state)

    Sets the check state of the item in the column \a column to \a state, which gives the item a check box
    there. The user can toggle it if the item is Qt::ItemIsUserCheckable.

    \sa checkState()
*/

/*!
    Returns the data of the item in the column \a column for the role \a role, or an invalid QVariant if there
    is none. The edit role and the display role are the same data.

    \sa setData()
*/
QVariant QTreeWidgetItem::data(int column, int role) const {
    if (column < 0 || column >= columnCount())
        return QVariant();
    return m_values[static_cast<std::size_t>(column)].value(role == Qt::EditRole ? int(Qt::DisplayRole) : role);
}

/*!
    Sets the data of the item in the column \a column for the role \a role to \a value. The tree is told, and
    emits QTreeWidget::itemChanged(), if this changes anything. The item gets more columns if \a column is
    beyond the ones that it has, and so does its tree.

    \sa data()
*/
void QTreeWidgetItem::setData(int column, int role, const QVariant &value) {
    if (column < 0)
        return;
    if (role == Qt::EditRole)
        role = Qt::DisplayRole;
    const bool header = m_view && this == m_view->headerItem();
    if (column >= columnCount()) {
        if (header)
            return; // the header has the columns of the tree
        m_values.resize(static_cast<std::size_t>(column) + 1);
    }
    if (m_view && !header && column >= m_view->columnCount())
        m_view->setColumnCount(column + 1);
    QMap<int, QVariant> &values = m_values[static_cast<std::size_t>(column)];
    const QVariant old = values.value(role);
    // The same value of another type is a change: 9 and "9" sort differently.
    if (old.isValid() == value.isValid() && old.typeId() == value.typeId() && old == value)
        return;
    if (value.isValid())
        values.insert(role, value);
    else
        values.remove(role);
    if (!m_view)
        return;
    if (header) {
        m_view->treeModel()->headerChanged(column);
    } else if (this != m_view->invisibleRootItem()) {
        m_view->treeModel()->itemDataChanged(this, column);
        emit m_view->itemChanged(this, column);
    }
}

/*!
    Sorts the children of the item, and theirs, by the column \a column in the order \a order, with
    operator<().

    \sa QTreeWidget::sortItems()
*/
void QTreeWidgetItem::sortChildren(int column, Qt::SortOrder order) {
    if (m_view)
        m_view->treeModel()->sortBelow(this, column, order);
    else
        sortRecursively(column, order);
}

void QTreeWidgetItem::sortRecursively(int column, Qt::SortOrder order) {
    const int outer = t_sortColumn;
    std::vector<QTreeWidgetItem *> pending{this};
    while (!pending.empty()) {
        QTreeWidgetItem *item = pending.back();
        pending.pop_back();
        t_sortColumn = column;
        std::stable_sort(item->m_children.begin(), item->m_children.end(),
                         [order](const QTreeWidgetItem *a, const QTreeWidgetItem *b) {
                             return order == Qt::AscendingOrder ? *a < *b : *b < *a;
                         });
        pending.insert(pending.end(), item->m_children.begin(), item->m_children.end());
    }
    t_sortColumn = outer;
}

/*!
    Returns \c true if this item goes before \a other when items are sorted in ascending order. The data of
    the display role in the column that is sorted by is compared: as numbers if both are numbers, and as text
    otherwise. A subclass reimplements this to sort in its own way.

    \sa sortChildren(), QTreeWidget::sortItems()
*/
bool QTreeWidgetItem::operator<(const QTreeWidgetItem &other) const {
    int column = t_sortColumn;
    if (column < 0)
        column = m_view ? std::max(0, m_view->sortColumn()) : 0;
    const QVariant a = data(column, Qt::DisplayRole);
    const QVariant b = other.data(column, Qt::DisplayRole);
    if (isNumber(a) && isNumber(b))
        return a.toDouble() < b.toDouble();
    return a.toString().compare(b.toString()) < 0;
}

/*!
    \fn int QTreeWidgetItem::type() const

    Returns the type that was given to the constructor: \c Type for a plain item.
*/

/*!
    Constructs an empty tree with one column and the given \a parent.
*/
QTreeWidget::QTreeWidget(QWidget *parent)
    : QTreeView(parent), m_model(nullptr), m_root(new QTreeWidgetItem), m_headerItem(new QTreeWidgetItem) {
    m_root->m_view = this;
    m_headerItem->m_values.resize(1);
    m_headerItem->m_view = this;
    m_model = new TreeModel(this);
    QTreeView::setModel(m_model);
    connect(this, &QAbstractItemView::pressed, this,
            [this](const QModelIndex &index) { emit itemPressed(itemFromIndex(index), index.column()); });
    connect(this, &QAbstractItemView::clicked, this,
            [this](const QModelIndex &index) { emit itemClicked(itemFromIndex(index), index.column()); });
    connect(this, &QAbstractItemView::doubleClicked, this,
            [this](const QModelIndex &index) { emit itemDoubleClicked(itemFromIndex(index), index.column()); });
    connect(this, &QAbstractItemView::activated, this,
            [this](const QModelIndex &index) { emit itemActivated(itemFromIndex(index), index.column()); });
    connect(this, &QTreeView::expanded, this, [this](const QModelIndex &index) { emit itemExpanded(itemFromIndex(index)); });
    connect(this, &QTreeView::collapsed, this,
            [this](const QModelIndex &index) { emit itemCollapsed(itemFromIndex(index)); });
}

/*!
    Destroys the tree and all its items.
*/
QTreeWidget::~QTreeWidget() {
    // The items go without telling the model about each of them.
    m_root->setView(nullptr);
    delete m_root;
    m_headerItem->m_view = nullptr;
    delete m_headerItem;
}

// A tree widget has its own model.
void QTreeWidget::setModel(QAbstractItemModel *model) { static_cast<void>(model); }

/*!
    \reimp

    Sets the selection model to \a selectionModel, and tells about its changes with currentItemChanged() and
    itemSelectionChanged().
*/
void QTreeWidget::setSelectionModel(QItemSelectionModel *selectionModel) {
    QTreeView::setSelectionModel(selectionModel);
    if (!selectionModel)
        return;
    connect(selectionModel, &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &previous) {
                QTreeWidgetItem *now = itemFromIndex(current);
                QTreeWidgetItem *before = itemFromIndex(previous);
                if (now != before)
                    emit currentItemChanged(now, before);
            });
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection &, const QItemSelection &) { emit itemSelectionChanged(); });
}

/*!
    Returns the number of columns of the tree.

    \sa setColumnCount()
*/
int QTreeWidget::columnCount() const { return m_headerItem->columnCount(); }

/*!
    Sets the number of columns of the tree to \a columns. Items keep the data that they have for columns that
    are taken away.

    \sa columnCount(), setHeaderLabels()
*/
void QTreeWidget::setColumnCount(int columns) {
    if (columns >= 0)
        m_model->setColumns(columns);
}

/*!
    \fn QTreeWidgetItem *QTreeWidget::invisibleRootItem() const

    Returns the item that the top-level items are the children of. It does not show; with it, code that walks
    the tree treats the top level like every other level.
*/

/*!
    Returns the top-level item at position \a index, or \c nullptr if there is none.

    \sa topLevelItemCount()
*/
QTreeWidgetItem *QTreeWidget::topLevelItem(int index) const { return m_root->child(index); }

/*!
    Returns the number of top-level items.

    \sa topLevelItem()
*/
int QTreeWidget::topLevelItemCount() const { return m_root->childCount(); }

/*!
    Inserts \a item at position \a index of the top-level items. The tree takes ownership of it. Nothing
    happens if the item is in a tree or has a parent already.

    \sa addTopLevelItem()
*/
void QTreeWidget::insertTopLevelItem(int index, QTreeWidgetItem *item) { m_root->insertChild(index, item); }

/*!
    Adds \a item as the last top-level item. The tree takes ownership of it.

    \sa insertTopLevelItem()
*/
void QTreeWidget::addTopLevelItem(QTreeWidgetItem *item) { m_root->addChild(item); }

/*!
    Removes the top-level item at position \a index and returns it, or \c nullptr if there is none. The caller
    owns it.
*/
QTreeWidgetItem *QTreeWidget::takeTopLevelItem(int index) { return m_root->takeChild(index); }

/*!
    Returns the position of \a item among the top-level items, or -1 if it is not one of them.
*/
int QTreeWidget::indexOfTopLevelItem(QTreeWidgetItem *item) const { return m_root->indexOfChild(item); }

/*!
    Inserts \a items at position \a index of the top-level items, in their order.
*/
void QTreeWidget::insertTopLevelItems(int index, const QList<QTreeWidgetItem *> &items) {
    m_root->insertChildren(index, items);
}

/*!
    Adds \a items as the last top-level items.
*/
void QTreeWidget::addTopLevelItems(const QList<QTreeWidgetItem *> &items) { m_root->addChildren(items); }

/*!
    \fn QTreeWidgetItem *QTreeWidget::headerItem() const

    Returns the item whose texts are the labels of the header. The tree owns it.

    \sa setHeaderLabels()
*/

/*!
    Sets the labels of the header to \a labels, one for each column from the first. The tree gets more columns
    if \a labels has more than it has.

    \sa setHeaderLabel(), setColumnCount()
*/
void QTreeWidget::setHeaderLabels(const QStringList &labels) {
    if (labels.size() > columnCount())
        setColumnCount(static_cast<int>(labels.size()));
    for (int i = 0; i < labels.size(); ++i)
        m_headerItem->setText(i, labels.at(i));
}

/*!
    \fn void QTreeWidget::setHeaderLabel(const QString &label)

    Sets the label of the first column of the header to \a label.

    \sa setHeaderLabels()
*/

/*!
    Returns the current item, or \c nullptr if there is none.

    \sa setCurrentItem(), currentColumn()
*/
QTreeWidgetItem *QTreeWidget::currentItem() const { return itemFromIndex(currentIndex()); }

/*!
    Returns the column of the current item that is current, or -1 if there is no current item.
*/
int QTreeWidget::currentColumn() const { return currentIndex().isValid() ? currentIndex().column() : -1; }

/*!
    Makes \a item the current item, and selects it unless the selection mode is \c NoSelection.

    \sa currentItem()
*/
void QTreeWidget::setCurrentItem(QTreeWidgetItem *item) { setCurrentItem(item, 0); }

/*!
    \overload

    Makes the column \a column of \a item current.
*/
void QTreeWidget::setCurrentItem(QTreeWidgetItem *item, int column) { setCurrentIndex(indexFromItem(item, column)); }

/*!
    Returns the item at the position \a p of the viewport, or \c nullptr if there is none.
*/
QTreeWidgetItem *QTreeWidget::itemAt(const QPoint &p) const { return itemFromIndex(indexAt(p)); }

/*!
    \fn QTreeWidgetItem *QTreeWidget::itemAt(int x, int y) const
    \overload

    Returns the item at the position (\a x, \a y) of the viewport.
*/

/*!
    Returns the rectangle that \a item takes on the viewport, over all the columns, or an empty rectangle if
    it is not laid out.
*/
QRect QTreeWidget::visualItemRect(const QTreeWidgetItem *item) const {
    const QModelIndex first = indexFromItem(item, 0);
    if (!first.isValid())
        return QRect();
    const QRect left = visualRect(first);
    if (left.isEmpty() || columnCount() < 2)
        return left;
    return left.united(visualRect(first.sibling(first.row(), columnCount() - 1)));
}

/*!
    Returns the column that the items are sorted by, as the header shows it, or -1 if they are not sorted.

    \sa sortItems()
*/
int QTreeWidget::sortColumn() const { return header()->sortIndicatorSection(); }

/*!
    Sorts the items of all levels by the column \a column in the order \a order.

    \sa sortColumn(), QTreeWidgetItem::sortChildren()
*/
void QTreeWidget::sortItems(int column, Qt::SortOrder order) { sortByColumn(column, order); }

/*!
    Starts editing \a item in the column \a column, if the item is enabled and Qt::ItemIsEditable.
*/
void QTreeWidget::editItem(QTreeWidgetItem *item, int column) { edit(indexFromItem(item, column)); }

/*!
    Returns the items that are selected, each of them once.

    \sa QTreeWidgetItem::isSelected(), itemSelectionChanged()
*/
QList<QTreeWidgetItem *> QTreeWidget::selectedItems() const {
    QList<QTreeWidgetItem *> items;
    if (!selectionModel())
        return items;
    for (const QModelIndex &index : selectionModel()->selectedIndexes()) {
        QTreeWidgetItem *item = itemFromIndex(index);
        if (item && !items.contains(item))
            items.append(item);
    }
    return items;
}

/*!
    Returns the items whose text in the column \a column matches \a text the way \a flags says: the top-level
    items, or with Qt::MatchRecursive the items of all levels, in the order of the tree. Matching ignores case
    unless \a flags has Qt::MatchCaseSensitive or is Qt::MatchExactly.
*/
QList<QTreeWidgetItem *> QTreeWidget::findItems(const QString &text, Qt::MatchFlags flags, int column) const {
    QList<QTreeWidgetItem *> found;
    const int type = static_cast<int>(flags) & 0x0f;
    const Qt::CaseSensitivity cs =
        (flags & Qt::MatchCaseSensitive) || type == Qt::MatchExactly ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QRegularExpression regex;
    if (type == Qt::MatchRegularExpression || type == Qt::MatchWildcard) {
        // A wildcard has to match the whole text; a regular expression matches anywhere unless it is anchored.
        const QString pattern = type == Qt::MatchWildcard ? QRegularExpression::wildcardToRegularExpression(text) : text;
        regex = QRegularExpression(pattern, cs == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption
                                                                      : QRegularExpression::NoPatternOption);
    }
    const auto matches = [&](const QString &candidate) {
        switch (type) {
        case Qt::MatchContains:
            return candidate.contains(text, cs);
        case Qt::MatchStartsWith:
            return candidate.startsWith(text, cs);
        case Qt::MatchEndsWith:
            return candidate.endsWith(text, cs);
        case Qt::MatchRegularExpression:
        case Qt::MatchWildcard:
            return regex.match(candidate).hasMatch();
        default: // MatchExactly and MatchFixedString
            return candidate.compare(text, cs) == 0;
        }
    };
    // In the order of the tree: an item, then what is below it.
    std::vector<QTreeWidgetItem *> pending(m_root->m_children.rbegin(), m_root->m_children.rend());
    while (!pending.empty()) {
        QTreeWidgetItem *item = pending.back();
        pending.pop_back();
        if (matches(item->text(column)))
            found.append(item);
        if (flags & Qt::MatchRecursive)
            pending.insert(pending.end(), item->m_children.rbegin(), item->m_children.rend());
    }
    return found;
}

/*!
    Returns the item that is laid out above \a item, or \c nullptr.

    \sa itemBelow()
*/
QTreeWidgetItem *QTreeWidget::itemAbove(const QTreeWidgetItem *item) const {
    return itemFromIndex(indexAbove(indexFromItem(item)));
}

/*!
    Returns the item that is laid out below \a item, or \c nullptr.

    \sa itemAbove()
*/
QTreeWidgetItem *QTreeWidget::itemBelow(const QTreeWidgetItem *item) const {
    return itemFromIndex(indexBelow(indexFromItem(item)));
}

/*!
    Returns the model index of \a item in the column \a column, or an invalid index if the item is not in this
    tree.

    \sa itemFromIndex()
*/
QModelIndex QTreeWidget::indexFromItem(const QTreeWidgetItem *item, int column) const {
    return m_model->indexOf(item, column);
}

/*!
    Returns the item of the model index \a index, or \c nullptr if \a index is not valid.

    \sa indexFromItem()
*/
QTreeWidgetItem *QTreeWidget::itemFromIndex(const QModelIndex &index) const {
    return index.isValid() && index.model() == m_model ? m_model->item(index) : nullptr;
}

/*!
    Scrolls so that \a item shows, where \a hint says, after expanding the items above it.
*/
void QTreeWidget::scrollToItem(const QTreeWidgetItem *item, QAbstractItemView::ScrollHint hint) {
    scrollTo(indexFromItem(item), hint);
}

/*!
    Expands \a item.

    \sa collapseItem(), QTreeWidgetItem::setExpanded()
*/
void QTreeWidget::expandItem(const QTreeWidgetItem *item) { expand(indexFromItem(item)); }

/*!
    Collapses \a item.

    \sa expandItem()
*/
void QTreeWidget::collapseItem(const QTreeWidgetItem *item) { collapse(indexFromItem(item)); }

/*!
    Removes and deletes all the items of the tree. The columns and the header stay.
*/
void QTreeWidget::clear() {
    m_model->beginReset();
    std::vector<QTreeWidgetItem *> children;
    children.swap(m_root->m_children);
    for (QTreeWidgetItem *child : children) {
        child->m_parent = nullptr;
        child->setView(nullptr);
        delete child;
    }
    m_model->endReset();
}

#include "moc_qtreewidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
