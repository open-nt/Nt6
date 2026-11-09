// MIT License - see LICENSE
#include <QtCore/qidentityproxymodel.h>

/*!
    Constructs an identity proxy with the given \a parent.
*/
QIdentityProxyModel::QIdentityProxyModel(QObject *parent)
    : QAbstractProxyModel(parent),
      m_layoutSaved(std::make_shared<QList<std::pair<QPersistentModelIndex, QPersistentModelIndex>>>()) {}

/*!
    Destroys the proxy.
*/
QIdentityProxyModel::~QIdentityProxyModel() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
}

/*!
    \reimp
    Returns the number of columns of the source under \a parent.
*/
int QIdentityProxyModel::columnCount(const QModelIndex &parent) const {
    return sourceModel() ? sourceModel()->columnCount(mapToSource(parent)) : 0;
}

/*!
    \reimp
    Returns the proxy index for \a row and \a column under \a parent.
*/
QModelIndex QIdentityProxyModel::index(int row, int column, const QModelIndex &parent) const {
    if (!sourceModel())
        return QModelIndex();
    return mapFromSource(sourceModel()->index(row, column, mapToSource(parent)));
}

/*!
    \reimp
    Returns the proxy index at the same position as \a sourceIndex.
*/
QModelIndex QIdentityProxyModel::mapFromSource(const QModelIndex &sourceIndex) const {
    if (!sourceModel() || !sourceIndex.isValid() || sourceIndex.model() != sourceModel())
        return QModelIndex();
    return createIndex(sourceIndex.row(), sourceIndex.column(), sourceIndex.internalPointer());
}

/*!
    \reimp
    Returns the source index at the same position as \a proxyIndex.
*/
QModelIndex QIdentityProxyModel::mapToSource(const QModelIndex &proxyIndex) const {
    if (!sourceModel() || !proxyIndex.isValid())
        return QModelIndex();
    return createSourceIndex(proxyIndex.row(), proxyIndex.column(), proxyIndex.internalPointer());
}

/*!
    \reimp
    Returns the parent of \a child.
*/
QModelIndex QIdentityProxyModel::parent(const QModelIndex &child) const {
    return mapFromSource(mapToSource(child).parent());
}

/*!
    \reimp
    Returns the number of rows of the source under \a parent.
*/
int QIdentityProxyModel::rowCount(const QModelIndex &parent) const {
    return sourceModel() ? sourceModel()->rowCount(mapToSource(parent)) : 0;
}

/*!
    \reimp
    Returns the source header data of \a section in \a orientation for
    \a role.
*/
QVariant QIdentityProxyModel::headerData(int section, Qt::Orientation orientation, int role) const {
    return sourceModel() ? sourceModel()->headerData(section, orientation, role) : QVariant();
}

/*!
    \reimp
    Returns the index at \a row and \a column next to \a idx.
*/
QModelIndex QIdentityProxyModel::sibling(int row, int column, const QModelIndex &idx) const {
    return sourceModel() ? mapFromSource(sourceModel()->sibling(row, column, mapToSource(idx))) : QModelIndex();
}

/*!
    \reimp
    Returns \a selection mapped from the source, range by range.
*/
QItemSelection QIdentityProxyModel::mapSelectionFromSource(const QItemSelection &selection) const {
    QItemSelection r;
    for (const QItemSelectionRange &range : selection)
        r.append(QItemSelectionRange(mapFromSource(range.topLeft()), mapFromSource(range.bottomRight())));
    return r;
}

/*!
    \reimp
    Returns \a selection mapped to the source, range by range.
*/
QItemSelection QIdentityProxyModel::mapSelectionToSource(const QItemSelection &selection) const {
    QItemSelection r;
    for (const QItemSelectionRange &range : selection)
        r.append(QItemSelectionRange(mapToSource(range.topLeft()), mapToSource(range.bottomRight())));
    return r;
}

/*!
    \reimp
    Searches the source for items matching \a value under \a role from
    \a start, with \a hits and \a flags as for QAbstractItemModel::match(),
    and returns the proxy indexes of the matches.
*/
QModelIndexList QIdentityProxyModel::match(const QModelIndex &start, int role, const QVariant &value, int hits,
                                           Qt::MatchFlags flags) const {
    QModelIndexList r;
    if (!sourceModel())
        return r;
    for (const QModelIndex &index : sourceModel()->match(mapToSource(start), role, value, hits, flags))
        r.append(mapFromSource(index));
    return r;
}

/*!
    \reimp
    Sets the source model to \a sourceModel, resetting the proxy, and
    forwards the changes of the source from then on.
*/
void QIdentityProxyModel::setSourceModel(QAbstractItemModel *sourceModel) {
    beginResetModel();
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    QAbstractProxyModel::setSourceModel(sourceModel);
    if (QAbstractItemModel *s = sourceModel) {
        using M = QAbstractItemModel;
        auto map = [this](const QModelIndex &i) { return mapFromSource(i); };
        m_connections << connect(s, &M::rowsAboutToBeInserted, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginInsertRows(map(p), f, l); });
        m_connections << connect(s, &M::rowsInserted, this, [this] { endInsertRows(); });
        m_connections << connect(s, &M::rowsAboutToBeRemoved, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginRemoveRows(map(p), f, l); });
        m_connections << connect(s, &M::rowsRemoved, this, [this] { endRemoveRows(); });
        m_connections << connect(s, &M::rowsAboutToBeMoved, this,
                                 [this, map](const QModelIndex &sp, int f, int l, const QModelIndex &dp, int d) {
                                     beginMoveRows(map(sp), f, l, map(dp), d);
                                 });
        m_connections << connect(s, &M::rowsMoved, this, [this] { endMoveRows(); });
        m_connections << connect(s, &M::columnsAboutToBeInserted, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginInsertColumns(map(p), f, l); });
        m_connections << connect(s, &M::columnsInserted, this, [this] { endInsertColumns(); });
        m_connections << connect(s, &M::columnsAboutToBeRemoved, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginRemoveColumns(map(p), f, l); });
        m_connections << connect(s, &M::columnsRemoved, this, [this] { endRemoveColumns(); });
        m_connections << connect(s, &M::columnsAboutToBeMoved, this,
                                 [this, map](const QModelIndex &sp, int f, int l, const QModelIndex &dp, int d) {
                                     beginMoveColumns(map(sp), f, l, map(dp), d);
                                 });
        m_connections << connect(s, &M::columnsMoved, this, [this] { endMoveColumns(); });
        m_connections << connect(s, &M::dataChanged, this,
                                 [this, map](const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles) {
                                     dataChanged(map(tl), map(br), roles);
                                 });
        m_connections << connect(s, &M::headerDataChanged, this,
                                 [this](Qt::Orientation o, int f, int l) { headerDataChanged(o, f, l); });
        m_connections << connect(s, &M::modelAboutToBeReset, this, [this] { beginResetModel(); });
        m_connections << connect(s, &M::modelReset, this, [this] { endResetModel(); });
        auto saved = m_layoutSaved;
        m_connections << connect(
            s, &M::layoutAboutToBeChanged, this,
            [this, map, saved](const QList<QPersistentModelIndex> &parents, M::LayoutChangeHint hint) {
                QList<QPersistentModelIndex> proxyParents;
                for (const QPersistentModelIndex &p : parents)
                    proxyParents.append(map(p));
                layoutAboutToBeChanged(proxyParents, hint);
                saved->clear();
                for (const QModelIndex &proxy : persistentIndexList())
                    saved->append({proxy, mapToSource(proxy)});
            });
        m_connections << connect(
            s, &M::layoutChanged, this,
            [this, map, saved](const QList<QPersistentModelIndex> &parents, M::LayoutChangeHint hint) {
                QModelIndexList from, to;
                for (const auto &entry : *saved) {
                    from.append(entry.first);
                    to.append(map(entry.second));
                }
                saved->clear();
                changePersistentIndexList(from, to);
                QList<QPersistentModelIndex> proxyParents;
                for (const QPersistentModelIndex &p : parents)
                    proxyParents.append(map(p));
                layoutChanged(proxyParents, hint);
            });
    }
    endResetModel();
}

/*!
    \reimp
    Inserts \a count columns before \a column under \a parent in the source.
*/
bool QIdentityProxyModel::insertColumns(int column, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->insertColumns(column, count, mapToSource(parent));
}

/*!
    \reimp
    Inserts \a count rows before \a row under \a parent in the source.
*/
bool QIdentityProxyModel::insertRows(int row, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->insertRows(row, count, mapToSource(parent));
}

/*!
    \reimp
    Removes \a count columns from \a column under \a parent in the source.
*/
bool QIdentityProxyModel::removeColumns(int column, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->removeColumns(column, count, mapToSource(parent));
}

/*!
    \reimp
    Removes \a count rows from \a row under \a parent in the source.
*/
bool QIdentityProxyModel::removeRows(int row, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->removeRows(row, count, mapToSource(parent));
}

/*!
    \reimp
    Moves \a count rows from \a sourceRow under \a sourceParent to before
    \a destinationChild under \a destinationParent in the source.
*/
bool QIdentityProxyModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                                   const QModelIndex &destinationParent, int destinationChild) {
    return sourceModel() && sourceModel()->moveRows(mapToSource(sourceParent), sourceRow, count,
                                                    mapToSource(destinationParent), destinationChild);
}

/*!
    \reimp
    Moves \a count columns from \a sourceColumn under \a sourceParent to
    before \a destinationChild under \a destinationParent in the source.
*/
bool QIdentityProxyModel::moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count,
                                      const QModelIndex &destinationParent, int destinationChild) {
    return sourceModel() && sourceModel()->moveColumns(mapToSource(sourceParent), sourceColumn, count,
                                                       mapToSource(destinationParent), destinationChild);
}

#include "moc_qidentityproxymodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
