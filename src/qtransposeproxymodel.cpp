// MIT License - see LICENSE
#include <QtCore/qtransposeproxymodel.h>

namespace {
Qt::Orientation flip(Qt::Orientation o) { return o == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal; }
} // namespace

/*!
    Constructs a transposing proxy with the given \a parent.
*/
QTransposeProxyModel::QTransposeProxyModel(QObject *parent)
    : QAbstractProxyModel(parent),
      m_layoutSaved(std::make_shared<QList<std::pair<QPersistentModelIndex, QPersistentModelIndex>>>()) {}

/*!
    Destroys the proxy.
*/
QTransposeProxyModel::~QTransposeProxyModel() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
}

/*!
    \reimp
    Sets the source model to \a newSourceModel, resetting the proxy. Row
    changes of the source become column changes of the proxy and the other
    way round.
*/
void QTransposeProxyModel::setSourceModel(QAbstractItemModel *newSourceModel) {
    beginResetModel();
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    QAbstractProxyModel::setSourceModel(newSourceModel);
    if (QAbstractItemModel *s = newSourceModel) {
        using M = QAbstractItemModel;
        auto map = [this](const QModelIndex &i) { return mapFromSource(i); };
        m_connections << connect(s, &M::rowsAboutToBeInserted, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginInsertColumns(map(p), f, l); });
        m_connections << connect(s, &M::rowsInserted, this, [this] { endInsertColumns(); });
        m_connections << connect(s, &M::rowsAboutToBeRemoved, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginRemoveColumns(map(p), f, l); });
        m_connections << connect(s, &M::rowsRemoved, this, [this] { endRemoveColumns(); });
        m_connections << connect(s, &M::rowsAboutToBeMoved, this,
                                 [this, map](const QModelIndex &sp, int f, int l, const QModelIndex &dp, int d) {
                                     beginMoveColumns(map(sp), f, l, map(dp), d);
                                 });
        m_connections << connect(s, &M::rowsMoved, this, [this] { endMoveColumns(); });
        m_connections << connect(s, &M::columnsAboutToBeInserted, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginInsertRows(map(p), f, l); });
        m_connections << connect(s, &M::columnsInserted, this, [this] { endInsertRows(); });
        m_connections << connect(s, &M::columnsAboutToBeRemoved, this,
                                 [this, map](const QModelIndex &p, int f, int l) { beginRemoveRows(map(p), f, l); });
        m_connections << connect(s, &M::columnsRemoved, this, [this] { endRemoveRows(); });
        m_connections << connect(s, &M::columnsAboutToBeMoved, this,
                                 [this, map](const QModelIndex &sp, int f, int l, const QModelIndex &dp, int d) {
                                     beginMoveRows(map(sp), f, l, map(dp), d);
                                 });
        m_connections << connect(s, &M::columnsMoved, this, [this] { endMoveRows(); });
        m_connections << connect(s, &M::dataChanged, this,
                                 [this, map](const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles) {
                                     dataChanged(map(tl), map(br), roles);
                                 });
        m_connections << connect(s, &M::headerDataChanged, this,
                                 [this](Qt::Orientation o, int f, int l) { headerDataChanged(flip(o), f, l); });
        m_connections << connect(s, &M::modelAboutToBeReset, this, [this] { beginResetModel(); });
        m_connections << connect(s, &M::modelReset, this, [this] { endResetModel(); });
        auto saved = m_layoutSaved;
        m_connections << connect(
            s, &M::layoutAboutToBeChanged, this,
            [this, map, saved](const QList<QPersistentModelIndex> &parents, M::LayoutChangeHint hint) {
                QList<QPersistentModelIndex> proxyParents;
                for (const QPersistentModelIndex &p : parents)
                    proxyParents.append(map(p));
                const M::LayoutChangeHint flipped = hint == M::VerticalSortHint     ? M::HorizontalSortHint
                                                    : hint == M::HorizontalSortHint ? M::VerticalSortHint
                                                                                    : hint;
                layoutAboutToBeChanged(proxyParents, flipped);
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
                const M::LayoutChangeHint flipped = hint == M::VerticalSortHint     ? M::HorizontalSortHint
                                                    : hint == M::HorizontalSortHint ? M::VerticalSortHint
                                                                                    : hint;
                layoutChanged(proxyParents, flipped);
            });
    }
    endResetModel();
}

/*!
    \reimp
    Returns the number of columns of the source under \a parent.
*/
int QTransposeProxyModel::rowCount(const QModelIndex &parent) const {
    return sourceModel() ? sourceModel()->columnCount(mapToSource(parent)) : 0;
}

/*!
    \reimp
    Returns the number of rows of the source under \a parent.
*/
int QTransposeProxyModel::columnCount(const QModelIndex &parent) const {
    return sourceModel() ? sourceModel()->rowCount(mapToSource(parent)) : 0;
}

/*!
    \reimp
    Returns the source header data of \a section in the other orientation
    than \a orientation, for \a role.
*/
QVariant QTransposeProxyModel::headerData(int section, Qt::Orientation orientation, int role) const {
    return sourceModel() ? sourceModel()->headerData(section, flip(orientation), role) : QVariant();
}

/*!
    \reimp
    Sets the source header data of \a section in the other orientation than
    \a orientation, for \a role, to \a value.
*/
bool QTransposeProxyModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) {
    return sourceModel() && sourceModel()->setHeaderData(section, flip(orientation), value, role);
}

/*!
    \reimp
    Returns the span of the source item of \a index, transposed.
*/
QSize QTransposeProxyModel::span(const QModelIndex &index) const {
    if (!sourceModel())
        return QSize(1, 1);
    const QSize s = sourceModel()->span(mapToSource(index));
    return QSize(s.height(), s.width());
}

/*!
    \reimp
    Returns the proxy index of \a sourceIndex, with its row and column
    swapped.
*/
QModelIndex QTransposeProxyModel::mapFromSource(const QModelIndex &sourceIndex) const {
    if (!sourceModel() || !sourceIndex.isValid() || sourceIndex.model() != sourceModel())
        return QModelIndex();
    return createIndex(sourceIndex.column(), sourceIndex.row(), sourceIndex.internalPointer());
}

/*!
    \reimp
    Returns the source index of \a proxyIndex, with its row and column
    swapped.
*/
QModelIndex QTransposeProxyModel::mapToSource(const QModelIndex &proxyIndex) const {
    if (!sourceModel() || !proxyIndex.isValid())
        return QModelIndex();
    return createSourceIndex(proxyIndex.column(), proxyIndex.row(), proxyIndex.internalPointer());
}

/*!
    \reimp
    Returns the parent of \a index.
*/
QModelIndex QTransposeProxyModel::parent(const QModelIndex &index) const {
    return mapFromSource(mapToSource(index).parent());
}

/*!
    \reimp
    Returns the proxy index for \a row and \a column under \a parent.
*/
QModelIndex QTransposeProxyModel::index(int row, int column, const QModelIndex &parent) const {
    if (!sourceModel())
        return QModelIndex();
    return mapFromSource(sourceModel()->index(column, row, mapToSource(parent)));
}

/*!
    \reimp
    Inserts \a count columns before column \a row under \a parent in the
    source.
*/
bool QTransposeProxyModel::insertRows(int row, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->insertColumns(row, count, mapToSource(parent));
}

/*!
    \reimp
    Removes \a count columns from column \a row under \a parent in the source.
*/
bool QTransposeProxyModel::removeRows(int row, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->removeColumns(row, count, mapToSource(parent));
}

/*!
    \reimp
    Moves \a count source columns from \a sourceRow under \a sourceParent to
    before \a destinationChild under \a destinationParent.
*/
bool QTransposeProxyModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                                    const QModelIndex &destinationParent, int destinationChild) {
    return sourceModel() && sourceModel()->moveColumns(mapToSource(sourceParent), sourceRow, count,
                                                       mapToSource(destinationParent), destinationChild);
}

/*!
    \reimp
    Inserts \a count rows before row \a column under \a parent in the source.
*/
bool QTransposeProxyModel::insertColumns(int column, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->insertRows(column, count, mapToSource(parent));
}

/*!
    \reimp
    Removes \a count rows from row \a column under \a parent in the source.
*/
bool QTransposeProxyModel::removeColumns(int column, int count, const QModelIndex &parent) {
    return sourceModel() && sourceModel()->removeRows(column, count, mapToSource(parent));
}

/*!
    \reimp
    Moves \a count source rows from \a sourceColumn under \a sourceParent to
    before \a destinationChild under \a destinationParent.
*/
bool QTransposeProxyModel::moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count,
                                       const QModelIndex &destinationParent, int destinationChild) {
    return sourceModel() && sourceModel()->moveRows(mapToSource(sourceParent), sourceColumn, count,
                                                    mapToSource(destinationParent), destinationChild);
}

/*!
    \reimp
    Sorts the source by \a column in \a order. Since rows and columns are
    swapped, sorting a transposed table sorts the source's rows by the
    source column \a column.
*/
void QTransposeProxyModel::sort(int column, Qt::SortOrder order) {
    if (sourceModel())
        sourceModel()->sort(column, order);
}

#include "moc_qtransposeproxymodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
