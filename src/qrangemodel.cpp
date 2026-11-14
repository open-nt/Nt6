// MIT License - see LICENSE
#include <QtCore/qrangemodel.h>

/*!
    \class QRangeModel
    \since 6.10
    \brief The QRangeModel class implements QAbstractItemModel for any C++ range.

    QRangeModel makes a model of a C++ range, such as a \c std::vector, a QList or a \c std::array, so that views
    and other model users can show and edit it. How the model looks depends on the type of the rows:

    \list
        \li A plain value, such as an \c int or a QString, makes a list: one column, with the value as the
            \c Qt::DisplayRole and \c Qt::EditRole data.
        \li A type that implements the tuple protocol, such as \c std::tuple, \c std::pair or \c std::array, makes
            a table with one column per element.
        \li A range, such as a QList of a QList, makes a table with as many columns as its first row has.
        \li A gadget or QObject type, or a pointer to one, makes a table with one column per property, with the
            property names as the horizontal header. If RowOptions gives it the RowCategory \c MultiRoleItem, it is
            one item whose roles are its properties, by the names in roleNames().
        \li A map from roles to QVariant, such as \c{QMap<int, QVariant>} or \c{std::map<QString, QVariant>}, is
            one item with several roles.
    \endlist

    An item in a tuple or a list that is a gadget, a QObject or a map of roles is a multi-role item as well. The
    roles of the properties of a gadget or QObject item are named after the properties, from \c Qt::UserRole on,
    except for properties named like a standard role, such as \c display or \c toolTip.

    \code
    std::vector<std::pair<QString, int>> scores = {{"Ann", 3}, {"Bo", 5}};
    QRangeModel model(std::ref(scores));    // edits go to scores
    model.setData(model.index(1, 1), 7);    // scores[1].second == 7
    \endcode

    The model keeps a copy of a range that is passed by value, and works on the caller's range if it is passed as a
    pointer or with \c std::ref(). A const range, or a range of const rows, makes a read-only model. Rows can be
    inserted and removed if the range has \c insert() and \c erase(), and columns if the rows are such ranges too.
    A row that is a raw pointer is made with \c new when it is inserted and deleted when it is removed.

    \section1 Trees

    A range whose row type has \c parentRow() and \c childRows() members makes a tree. \c parentRow() returns a
    pointer to the parent row (null at the top level), and \c childRows() the range of the children, which has the
    type of the whole range and may be held in a \c std::optional or by pointer. A \c setParentRow() member lets the
    model keep the parents right when rows move in memory, as they do in a \c std::vector when rows are inserted
    or removed. The same functions can instead be members of a protocol object passed to the constructor, which
    take the row as their first argument.

    \code
    struct Node {
        QString name;
        Node *parent = nullptr;
        std::optional<std::vector<Node>> children;

        const Node *parentRow() const { return parent; }
        void setParentRow(Node *p) { parent = p; }
        std::optional<std::vector<Node>> &childRows() { return children; }
        const std::optional<std::vector<Node>> &childRows() const { return children; }
    };
    \endcode

    \differences
    Rows can only be moved within one parent. The model does not watch the rows: changes made to the range behind
    its back, and the notify signals of QObject rows, are not reported to the views. There is no \c multiData() and
    no \c sort().
    \enddifferences

    \sa QAbstractItemModel, QStringListModel
*/

/*!
    \fn template <typename Range, typename> QRangeModel::QRangeModel(Range &&range, QObject *parent)

    Constructs a model of \a range with the given \a parent. A range passed by value is copied or moved into the
    model; a pointer to a range or a \c std::reference_wrapper makes the model work on that range. If the row type
    implements the tree protocol, the model is a tree.
*/

/*!
    \fn template <typename Range, typename Protocol, typename> QRangeModel::QRangeModel(Range &&range, Protocol &&protocol, QObject *parent)

    Constructs a tree model of \a range with the given \a parent, which finds the parent and the children of a row
    through \a protocol: an object with the members \c parentRow(row), \c childRows(row) and, for a model whose rows
    move in memory, \c setParentRow(row, parent).
*/

/*!
    Destroys the model. A range that the model keeps is destroyed with it.
*/
QRangeModel::~QRangeModel() = default;

/*!
    \reimp
*/
QModelIndex QRangeModel::index(int row, int column, const QModelIndex &parent) const {
    return m_impl->index(row, column, parent);
}

/*!
    \reimp
*/
QModelIndex QRangeModel::parent(const QModelIndex &child) const { return m_impl->parent(child); }

/*!
    \reimp
*/
QModelIndex QRangeModel::sibling(int row, int column, const QModelIndex &index) const {
    if (!index.isValid() || index.model() != this)
        return QModelIndex();
    if (row == index.row() && column == index.column())
        return index;
    return this->index(row, column, parent(index));
}

/*!
    \reimp
*/
int QRangeModel::rowCount(const QModelIndex &parent) const { return m_impl->rowCount(parent); }

/*!
    \reimp
*/
int QRangeModel::columnCount(const QModelIndex &parent) const { return m_impl->columnCount(parent); }

/*!
    \reimp
*/
bool QRangeModel::hasChildren(const QModelIndex &parent) const {
    return rowCount(parent) > 0 && columnCount(parent) > 0;
}

/*!
    \reimp
    An item is enabled and selectable, and editable if the model can write it.
*/
Qt::ItemFlags QRangeModel::flags(const QModelIndex &index) const { return m_impl->flags(index); }

/*!
    \reimp
    For rows of a gadget or QObject type, the horizontal header shows the property names.
*/
QVariant QRangeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const QVariant v = m_impl->headerData(section, orientation, role);
    return v.isValid() ? v : QAbstractItemModel::headerData(section, orientation, role);
}

/*!
    \reimp
*/
QVariant QRangeModel::data(const QModelIndex &index, int role) const { return m_impl->data(index, role); }

/*!
    \reimp
    Writes \a value to the item at \a index for \a role, converting it to the type of the item, and emits
    dataChanged(). Returns \c false if the model is read-only, or the item has no such role or cannot hold
    \a value.
*/
bool QRangeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    return m_impl->setData(index, value, role);
}

/*!
    \reimp
    Returns the roles of a multi-role item that have data, or the display and edit roles of other items.
*/
QMap<int, QVariant> QRangeModel::itemData(const QModelIndex &index) const { return m_impl->itemData(index); }

/*!
    \reimp
*/
bool QRangeModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &values) {
    return m_impl->setItemData(index, values);
}

/*!
    \reimp
    Clears an item that is a map of roles. Returns \c false for other items.
*/
bool QRangeModel::clearItemData(const QModelIndex &index) { return m_impl->clearItemData(index); }

/*!
    \reimp
    Inserts \a count default rows before \a row under \a parent, if the range can grow. A new row of a table of
    lists gets as many columns as the model has.
*/
bool QRangeModel::insertRows(int row, int count, const QModelIndex &parent) {
    return m_impl->insertRows(row, count, parent);
}

/*!
    \reimp
*/
bool QRangeModel::removeRows(int row, int count, const QModelIndex &parent) {
    return m_impl->removeRows(row, count, parent);
}

/*!
    \reimp
    Moves \a count rows from \a sourceRow to before \a destinationChild. \a sourceParent and \a destinationParent
    must be the same.
*/
bool QRangeModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                           const QModelIndex &destinationParent, int destinationChild) {
    return m_impl->moveRows(sourceParent, sourceRow, count, destinationParent, destinationChild);
}

/*!
    \reimp
    Inserts columns into a table whose rows are ranges that can grow.
*/
bool QRangeModel::insertColumns(int column, int count, const QModelIndex &parent) {
    return m_impl->insertColumns(column, count, parent);
}

/*!
    \reimp
*/
bool QRangeModel::removeColumns(int column, int count, const QModelIndex &parent) {
    return m_impl->removeColumns(column, count, parent);
}

/*!
    \reimp
*/
bool QRangeModel::moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count,
                              const QModelIndex &destinationParent, int destinationChild) {
    return m_impl->moveColumns(sourceParent, sourceColumn, count, destinationParent, destinationChild);
}

/*!
    \reimp
    Returns the role names that setRoleNames() set, or else the standard ones together with the properties of a
    multi-role item type.

    \sa setRoleNames(), resetRoleNames()
*/
QHash<int, QByteArray> QRangeModel::roleNames() const { return m_roleNames; }

/*!
    Sets the role names to \a names, and emits roleNamesChanged() if they changed.

    \sa resetRoleNames()
*/
void QRangeModel::setRoleNames(const QHash<int, QByteArray> &names) {
    if (m_roleNames == names)
        return;
    m_roleNames = names;
    roleNamesChanged();
}

/*!
    Restores the role names that the model has by default, and emits roleNamesChanged() if they changed.

    \sa setRoleNames()
*/
void QRangeModel::resetRoleNames() { setRoleNames(defaultRoleNames()); }

QHash<int, QByteArray> QRangeModel::defaultRoleNames() const {
    QHash<int, QByteArray> names = QAbstractItemModel::roleNames();
    const QMetaObject *mo = m_impl ? m_impl->itemMetaObject() : nullptr;
    if (!mo)
        return names;
    int next = Qt::UserRole;
    const int first = mo->inherits(&QObject::staticMetaObject) ? QObject::staticMetaObject.propertyCount() : 0;
    for (int i = first; i < mo->propertyCount(); ++i) {
        const QByteArray name = mo->property(i).name();
        bool standard = false;
        for (auto it = names.cbegin(); it != names.cend(); ++it) {
            if (it.key() < Qt::UserRole && it.value() == name) {
                standard = true;
                break;
            }
        }
        if (!standard)
            names.insert(next++, name);
    }
    return names;
}

namespace QRangeModelPrivate {

ImplBase::~ImplBase() = default;

QHash<int, QByteArray> ImplBase::roleNames() const { return m_model->roleNames(); }

QModelIndex ImplBase::createIndex(int row, int column, const void *pointer) const {
    return m_model->createIndex(row, column, pointer);
}

bool ImplBase::ownsIndex(const QModelIndex &index) const { return index.model() == m_model; }

void ImplBase::beginInsertRows(const QModelIndex &parent, int first, int last) {
    m_model->beginInsertRows(parent, first, last);
}

void ImplBase::endInsertRows() { m_model->endInsertRows(); }

void ImplBase::beginRemoveRows(const QModelIndex &parent, int first, int last) {
    m_model->beginRemoveRows(parent, first, last);
}

void ImplBase::endRemoveRows() { m_model->endRemoveRows(); }

bool ImplBase::beginMoveRows(const QModelIndex &sourceParent, int first, int last, const QModelIndex &destinationParent,
                             int destinationChild) {
    return m_model->beginMoveRows(sourceParent, first, last, destinationParent, destinationChild);
}

void ImplBase::endMoveRows() { m_model->endMoveRows(); }

void ImplBase::beginInsertColumns(const QModelIndex &parent, int first, int last) {
    m_model->beginInsertColumns(parent, first, last);
}

void ImplBase::endInsertColumns() { m_model->endInsertColumns(); }

void ImplBase::beginRemoveColumns(const QModelIndex &parent, int first, int last) {
    m_model->beginRemoveColumns(parent, first, last);
}

void ImplBase::endRemoveColumns() { m_model->endRemoveColumns(); }

bool ImplBase::beginMoveColumns(const QModelIndex &sourceParent, int first, int last,
                                const QModelIndex &destinationParent, int destinationChild) {
    return m_model->beginMoveColumns(sourceParent, first, last, destinationParent, destinationChild);
}

void ImplBase::endMoveColumns() { m_model->endMoveColumns(); }

void ImplBase::emitDataChanged(const QModelIndex &index, int role) {
    QList<int> roles;
    if (role == Qt::DisplayRole || role == Qt::EditRole)
        roles = {Qt::DisplayRole, Qt::EditRole};
    else if (role >= 0)
        roles = {role};
    m_model->dataChanged(index, index, roles);
}

void ImplBase::repointPersistentIndexes(const std::vector<const void *> &from, const std::vector<const void *> &to) {
    QModelIndexList oldIndexes, newIndexes;
    for (const QModelIndex &index : m_model->persistentIndexList()) {
        const auto it = std::find(from.begin(), from.end(), index.internalPointer());
        if (it == from.end())
            continue;
        oldIndexes.append(index);
        newIndexes.append(createIndex(index.row(), index.column(), to[static_cast<std::size_t>(it - from.begin())]));
    }
    if (!oldIndexes.isEmpty())
        m_model->changePersistentIndexList(oldIndexes, newIndexes);
}

QVariant ImplBase::readProperty(const QMetaObject *mo, const void *gadget, const QObject *object, int property) {
    if (property < 0 || property >= mo->propertyCount())
        return QVariant();
    const QMetaProperty p = mo->property(property);
    return object ? p.read(object) : p.readOnGadget(gadget);
}

bool ImplBase::writeProperty(const QMetaObject *mo, void *gadget, QObject *object, int property,
                             const QVariant &value) {
    if (property < 0 || property >= mo->propertyCount())
        return false;
    const QMetaProperty p = mo->property(property);
    return object ? p.write(object, value) : p.writeOnGadget(gadget, value);
}

} // namespace QRangeModelPrivate

#include "moc_qrangemodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
