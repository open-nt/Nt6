// MIT License - see LICENSE
#include <QtCore/qstringlistmodel.h>

#include <algorithm>
#include <numeric>
#include <vector>

/*!
    Constructs an empty model with the given \a parent.
*/
QStringListModel::QStringListModel(QObject *parent) : QAbstractListModel(parent) {}

/*!
    Constructs a model with the given \a parent that holds \a strings.
*/
QStringListModel::QStringListModel(const QStringList &strings, QObject *parent)
    : QAbstractListModel(parent), m_strings(strings) {}

/*!
    Destroys the model.
*/
QStringListModel::~QStringListModel() = default;

/*!
    \reimp
    Returns the number of strings, or 0 for a valid \a parent.
*/
int QStringListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_strings.size());
}

/*!
    \reimp
    Returns the index at \a row and \a column next to \a idx.
*/
QModelIndex QStringListModel::sibling(int row, int column, const QModelIndex &idx) const {
    if (!idx.isValid() || column != 0 || row < 0 || row >= m_strings.size())
        return QModelIndex();
    return createIndex(row, 0);
}

/*!
    \reimp
    Returns the string at \a index for Qt::DisplayRole and Qt::EditRole, and
    an invalid QVariant for other values of \a role.
*/
QVariant QStringListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_strings.size())
        return QVariant();
    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return m_strings.at(index.row());
    return QVariant();
}

/*!
    \reimp
    Sets the string at \a index to \a value for Qt::DisplayRole or
    Qt::EditRole (\a role) and emits dataChanged(). Returns \c false for
    other roles or an invalid index.
*/
bool QStringListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_strings.size() ||
        (role != Qt::EditRole && role != Qt::DisplayRole))
        return false;
    const QString text = value.toString();
    if (m_strings.at(index.row()) == text)
        return true;
    m_strings[index.row()] = text;
    dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

/*!
    \reimp
    \since 6.0
    Makes the string at \a index empty.
*/
bool QStringListModel::clearItemData(const QModelIndex &index) { return setData(index, QVariant()); }

/*!
    \reimp
    Returns the flags of the item at \a index: items are editable and can be
    dragged; the root accepts drops.
*/
Qt::ItemFlags QStringListModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

/*!
    \reimp
    Inserts \a count empty strings before \a row. \a parent must be invalid.
*/
bool QStringListModel::insertRows(int row, int count, const QModelIndex &parent) {
    if (count < 1 || row < 0 || row > rowCount(parent) || parent.isValid())
        return false;
    beginInsertRows(QModelIndex(), row, row + count - 1);
    for (int k = 0; k < count; ++k)
        m_strings.insert(row, QString());
    endInsertRows();
    return true;
}

/*!
    \reimp
    Removes \a count strings from \a row. \a parent must be invalid.
*/
bool QStringListModel::removeRows(int row, int count, const QModelIndex &parent) {
    if (count <= 0 || row < 0 || row + count > rowCount(parent) || parent.isValid())
        return false;
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    m_strings.remove(row, count);
    endRemoveRows();
    return true;
}

/*!
    \reimp
    \since 6.0
    Moves \a count strings from \a sourceRow to before \a destinationChild.
    Both parents, \a sourceParent and \a destinationParent, must be invalid.
*/
bool QStringListModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                                const QModelIndex &destinationParent, int destinationChild) {
    if (sourceParent.isValid() || destinationParent.isValid() || count <= 0 || sourceRow < 0 ||
        sourceRow + count > m_strings.size() || destinationChild < 0 || destinationChild > m_strings.size())
        return false;
    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1, QModelIndex(), destinationChild))
        return false;
    const QStringList moved = m_strings.mid(sourceRow, count);
    m_strings.remove(sourceRow, count);
    const int target = destinationChild > sourceRow ? destinationChild - count : destinationChild;
    for (int k = 0; k < count; ++k)
        m_strings.insert(target + k, moved.at(k));
    endMoveRows();
    return true;
}

/*!
    \reimp
    Returns the display and edit data of the item at \a index.
*/
QMap<int, QVariant> QStringListModel::itemData(const QModelIndex &index) const {
    QMap<int, QVariant> r;
    if (!index.isValid() || index.row() >= m_strings.size())
        return r;
    r.insert(Qt::DisplayRole, m_strings.at(index.row()));
    r.insert(Qt::EditRole, m_strings.at(index.row()));
    return r;
}

/*!
    \reimp
    Sets the string at \a index from the Qt::EditRole value in \a roles, or
    else the Qt::DisplayRole value.
*/
bool QStringListModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) {
    if (roles.contains(Qt::EditRole))
        return setData(index, roles.value(Qt::EditRole), Qt::EditRole);
    if (roles.contains(Qt::DisplayRole))
        return setData(index, roles.value(Qt::DisplayRole), Qt::EditRole);
    return false;
}

/*!
    \reimp
    Sorts the strings in \a order; \a column is ignored. Persistent indexes
    follow their strings.
*/
void QStringListModel::sort(int column, Qt::SortOrder order) {
    Q_UNUSED(column)
    layoutAboutToBeChanged({}, VerticalSortHint);
    std::vector<int> perm(static_cast<std::size_t>(m_strings.size()));
    std::iota(perm.begin(), perm.end(), 0);
    std::stable_sort(perm.begin(), perm.end(), [&](int a, int b) {
        const int c = m_strings.at(a).compare(m_strings.at(b));
        return order == Qt::AscendingOrder ? c < 0 : c > 0;
    });
    QStringList sorted;
    QModelIndexList from, to;
    for (std::size_t newRow = 0; newRow < perm.size(); ++newRow) {
        sorted.append(m_strings.at(perm[newRow]));
        from.append(createIndex(perm[newRow], 0));
        to.append(createIndex(static_cast<int>(newRow), 0));
    }
    m_strings = sorted;
    changePersistentIndexList(from, to);
    layoutChanged({}, VerticalSortHint);
}

/*!
    \reimp
    Returns Qt::CopyAction and Qt::MoveAction.
*/
Qt::DropActions QStringListModel::supportedDropActions() const { return Qt::CopyAction | Qt::MoveAction; }

/*!
    Returns the strings of the model.

    \sa setStringList()
*/
QStringList QStringListModel::stringList() const { return m_strings; }

/*!
    Replaces the strings of the model with \a strings, resetting the model.

    \sa stringList()
*/
void QStringListModel::setStringList(const QStringList &strings) {
    beginResetModel();
    m_strings = strings;
    endResetModel();
}

#include "moc_qstringlistmodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
