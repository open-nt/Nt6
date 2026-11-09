// MIT License - see LICENSE
#include <QtCore/qabstractproxymodel.h>

#include <QtCore/qmimedata.h>

/*!
    Constructs a proxy model with the given \a parent and no source model.
*/
QAbstractProxyModel::QAbstractProxyModel(QObject *parent) : QAbstractItemModel(parent) {}

/*!
    Destroys the proxy model.
*/
QAbstractProxyModel::~QAbstractProxyModel() { QObject::disconnect(m_destroyed); }

/*!
    Sets the source model to \a sourceModel and emits sourceModelChanged().
    Subclasses that reimplement it connect to the signals of the source and
    call this implementation. When the source model is destroyed, the proxy
    is left without one.

    \sa sourceModel()
*/
void QAbstractProxyModel::setSourceModel(QAbstractItemModel *sourceModel) {
    if (m_source == sourceModel)
        return;
    QObject::disconnect(m_destroyed);
    m_source = sourceModel;
    if (sourceModel)
        m_destroyed = connect(sourceModel, &QObject::destroyed, this, [this] { setSourceModel(nullptr); });
    sourceModelChanged();
}

/*!
    Returns the source model, or \c nullptr.
*/
QAbstractItemModel *QAbstractProxyModel::sourceModel() const { return m_source; }

/*!
    Returns \a selection with each range mapped to the source model.
*/
QItemSelection QAbstractProxyModel::mapSelectionToSource(const QItemSelection &selection) const {
    QItemSelection r;
    for (const QModelIndex &index : selection.indexes()) {
        const QModelIndex s = mapToSource(index);
        if (s.isValid())
            r.append(QItemSelectionRange(s));
    }
    return r;
}

/*!
    Returns \a selection with each range mapped from the source model.
*/
QItemSelection QAbstractProxyModel::mapSelectionFromSource(const QItemSelection &selection) const {
    QItemSelection r;
    for (const QModelIndex &index : selection.indexes()) {
        const QModelIndex p = mapFromSource(index);
        if (p.isValid())
            r.append(QItemSelectionRange(p));
    }
    return r;
}

/*!
    \reimp
    Returns the data of the source item of \a proxyIndex for \a role.
*/
QVariant QAbstractProxyModel::data(const QModelIndex &proxyIndex, int role) const {
    return m_source ? m_source->data(mapToSource(proxyIndex), role) : QVariant();
}

/*!
    \reimp
    Returns the header data of \a section in \a orientation for \a role,
    taken from the source section that the proxy section shows.
*/
QVariant QAbstractProxyModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (!m_source)
        return QAbstractItemModel::headerData(section, orientation, role);
    const QModelIndex proxy = orientation == Qt::Horizontal ? index(0, section) : index(section, 0);
    const QModelIndex source = mapToSource(proxy);
    if (!source.isValid())
        return QAbstractItemModel::headerData(section, orientation, role);
    return m_source->headerData(orientation == Qt::Horizontal ? source.column() : source.row(), orientation, role);
}

/*!
    \reimp
    Returns the item data of the source item of \a index.
*/
QMap<int, QVariant> QAbstractProxyModel::itemData(const QModelIndex &index) const {
    return m_source ? m_source->itemData(mapToSource(index)) : QMap<int, QVariant>();
}

/*!
    \reimp
    Returns the flags of the source item of \a index.
*/
Qt::ItemFlags QAbstractProxyModel::flags(const QModelIndex &index) const {
    return m_source ? m_source->flags(mapToSource(index)) : Qt::ItemFlags();
}

/*!
    \reimp
    Sets the data of the source item of \a index for \a role to \a value.
*/
bool QAbstractProxyModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    return m_source && m_source->setData(mapToSource(index), value, role);
}

/*!
    \reimp
    Sets the data of the source item of \a index for the roles in \a roles.
*/
bool QAbstractProxyModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) {
    return m_source && m_source->setItemData(mapToSource(index), roles);
}

/*!
    \reimp
    Sets the header data of the source section shown as \a section in
    \a orientation for \a role to \a value.
*/
bool QAbstractProxyModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) {
    if (!m_source)
        return false;
    const QModelIndex proxy = orientation == Qt::Horizontal ? index(0, section) : index(section, 0);
    const QModelIndex source = mapToSource(proxy);
    if (!source.isValid())
        return false;
    return m_source->setHeaderData(orientation == Qt::Horizontal ? source.column() : source.row(), orientation, value,
                                   role);
}

/*!
    \reimp
    Clears the item data of the source item of \a index.
*/
bool QAbstractProxyModel::clearItemData(const QModelIndex &index) {
    return m_source && m_source->clearItemData(mapToSource(index));
}

/*!
    \reimp
    Returns the proxy index of the buddy of the source item of \a index.
*/
QModelIndex QAbstractProxyModel::buddy(const QModelIndex &index) const {
    return m_source ? mapFromSource(m_source->buddy(mapToSource(index))) : QModelIndex();
}

/*!
    \reimp
    Returns whether the source model can fetch more items under the source
    item of \a parent.
*/
bool QAbstractProxyModel::canFetchMore(const QModelIndex &parent) const {
    return m_source && m_source->canFetchMore(mapToSource(parent));
}

/*!
    \reimp
    Fetches more items under the source item of \a parent.
*/
void QAbstractProxyModel::fetchMore(const QModelIndex &parent) {
    if (m_source)
        m_source->fetchMore(mapToSource(parent));
}

/*!
    \reimp
    Sorts the source model by \a column in \a order.
*/
void QAbstractProxyModel::sort(int column, Qt::SortOrder order) {
    if (m_source)
        m_source->sort(column, order);
}

/*!
    \reimp
    Returns the span of the source item of \a index.
*/
QSize QAbstractProxyModel::span(const QModelIndex &index) const {
    return m_source ? m_source->span(mapToSource(index)) : QSize(1, 1);
}

/*!
    \reimp
    Returns whether the source item of \a parent has children.
*/
bool QAbstractProxyModel::hasChildren(const QModelIndex &parent) const {
    return m_source && m_source->hasChildren(mapToSource(parent));
}

/*!
    \reimp
    Returns the index at \a row and \a column next to \a idx.
*/
QModelIndex QAbstractProxyModel::sibling(int row, int column, const QModelIndex &idx) const {
    return index(row, column, idx.parent());
}

/*!
    \reimp
    Returns the MIME data of the source items of \a indexes.
*/
QMimeData *QAbstractProxyModel::mimeData(const QModelIndexList &indexes) const {
    if (!m_source)
        return nullptr;
    QModelIndexList source;
    for (const QModelIndex &index : indexes)
        source.append(mapToSource(index));
    return m_source->mimeData(source);
}

/*!
    \internal
    Maps the drop position \a row, \a column under \a parent to the source:
    sets \a sourceRow, \a sourceColumn and \a sourceParent and returns
    \c true on success.
*/
bool QAbstractProxyModel::mapDropCoordinatesToSource(int row, int column, const QModelIndex &parent, int *sourceRow,
                                                     int *sourceColumn, QModelIndex *sourceParent) const {
    if (!m_source)
        return false;
    *sourceRow = -1;
    *sourceColumn = -1;
    if (row == -1 && column == -1) {
        *sourceParent = mapToSource(parent);
    } else if (row == rowCount(parent)) {
        *sourceParent = mapToSource(parent);
        *sourceRow = m_source->rowCount(*sourceParent);
    } else {
        const QModelIndex proxy = index(row, column, parent);
        const QModelIndex source = mapToSource(proxy);
        if (!source.isValid())
            return false;
        *sourceRow = source.row();
        *sourceColumn = source.column();
        *sourceParent = source.parent();
    }
    return true;
}

/*!
    \reimp
    Returns whether the source model accepts \a data dropped with \a action
    at the source position of \a row and \a column under \a parent.
*/
bool QAbstractProxyModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                                          const QModelIndex &parent) const {
    int sr = 0, sc = 0;
    QModelIndex sp;
    return mapDropCoordinatesToSource(row, column, parent, &sr, &sc, &sp) &&
           m_source->canDropMimeData(data, action, sr, sc, sp);
}

/*!
    \reimp
    Drops \a data with \a action into the source model at the source position
    of \a row and \a column under \a parent.
*/
bool QAbstractProxyModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                                       const QModelIndex &parent) {
    int sr = 0, sc = 0;
    QModelIndex sp;
    return mapDropCoordinatesToSource(row, column, parent, &sr, &sc, &sp) &&
           m_source->dropMimeData(data, action, sr, sc, sp);
}

/*!
    \reimp
    Returns the MIME types of the source model.
*/
QStringList QAbstractProxyModel::mimeTypes() const {
    return m_source ? m_source->mimeTypes() : QAbstractItemModel::mimeTypes();
}

/*!
    \reimp
    Returns the drag actions of the source model.
*/
Qt::DropActions QAbstractProxyModel::supportedDragActions() const {
    return m_source ? m_source->supportedDragActions() : QAbstractItemModel::supportedDragActions();
}

/*!
    \reimp
    Returns the drop actions of the source model.
*/
Qt::DropActions QAbstractProxyModel::supportedDropActions() const {
    return m_source ? m_source->supportedDropActions() : QAbstractItemModel::supportedDropActions();
}

/*!
    \reimp
    Returns the role names of the source model.
*/
QHash<int, QByteArray> QAbstractProxyModel::roleNames() const {
    return m_source ? m_source->roleNames() : QAbstractItemModel::roleNames();
}

/*!
    \reimp
    Calls submit() on the source model.
*/
bool QAbstractProxyModel::submit() { return m_source ? m_source->submit() : false; }

/*!
    \reimp
    Calls revert() on the source model.
*/
void QAbstractProxyModel::revert() {
    if (m_source)
        m_source->revert();
}

/*!
    \since 5.8
    Creates an index of the source model for \a row and \a col that stores
    \a internalPtr, for proxies whose indexes mirror those of the source.
*/
QModelIndex QAbstractProxyModel::createSourceIndex(int row, int col, void *internalPtr) const {
    return m_source ? m_source->createIndex(row, col, internalPtr) : QModelIndex();
}

#include "moc_qabstractproxymodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
