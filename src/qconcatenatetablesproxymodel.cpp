// MIT License - see LICENSE
#include <QtCore/qconcatenatetablesproxymodel.h>

#include <QtCore/qmimedata.h>

#include <algorithm>
#include <memory>

/*!
    Constructs a proxy with the given \a parent and no source models.
*/
QConcatenateTablesProxyModel::QConcatenateTablesProxyModel(QObject *parent) : QAbstractItemModel(parent) {}

/*!
    Destroys the proxy.
*/
QConcatenateTablesProxyModel::~QConcatenateTablesProxyModel() {
    for (Source &s : m_sources)
        disconnectSource(s);
}

void QConcatenateTablesProxyModel::disconnectSource(Source &s) {
    for (const QMetaObject::Connection &c : s.connections)
        QObject::disconnect(c);
    s.connections.clear();
}

/*!
    Returns the source models, in the order their rows appear.
*/
QList<QAbstractItemModel *> QConcatenateTablesProxyModel::sourceModels() const {
    QList<QAbstractItemModel *> r;
    for (const Source &s : m_sources)
        if (s.model)
            r.append(s.model);
    return r;
}

int QConcatenateTablesProxyModel::offsetOf(const QAbstractItemModel *model) const {
    int offset = 0;
    for (const Source &s : m_sources) {
        if (s.model == model)
            return offset;
        if (s.model)
            offset += s.model->rowCount();
    }
    return -1;
}

int QConcatenateTablesProxyModel::sourceForRow(int row, int *localRow) const {
    int offset = 0;
    for (int k = 0; k < static_cast<int>(m_sources.size()); ++k) {
        const QAbstractItemModel *m = m_sources.at(k).model;
        if (!m)
            continue;
        const int rows = m->rowCount();
        if (row < offset + rows) {
            *localRow = row - offset;
            return k;
        }
        offset += rows;
    }
    return -1;
}

namespace {
int minimumColumns(const QList<QAbstractItemModel *> &models) {
    if (models.isEmpty())
        return 0;
    int c = models.at(0)->columnCount();
    for (const QAbstractItemModel *m : models)
        c = std::min(c, m->columnCount());
    return c;
}
} // namespace

/*!
    Appends the rows of \a sourceModel, which must be a flat model. The
    proxy follows the changes of the source from then on.

    \sa removeSourceModel()
*/
void QConcatenateTablesProxyModel::addSourceModel(QAbstractItemModel *sourceModel) {
    if (!sourceModel || sourceModels().contains(sourceModel))
        return;
    QList<QAbstractItemModel *> models = sourceModels();
    models.append(sourceModel);
    const int newColumns = minimumColumns(models);
    const int first = rowCount();
    const int rows = sourceModel->rowCount();
    const bool reset = newColumns != m_columns;
    if (reset)
        beginResetModel();
    else if (rows > 0)
        beginInsertRows(QModelIndex(), first, first + rows - 1);

    Source s;
    s.model = sourceModel;
    using M = QAbstractItemModel;
    QAbstractItemModel *m = sourceModel;
    auto resetBegin = [this] { beginResetModel(); };
    auto resetEnd = [this] {
        m_columns = minimumColumns(sourceModels());
        endResetModel();
    };
    s.connections << connect(m, &M::rowsAboutToBeInserted, this, [this, m](const QModelIndex &p, int f, int l) {
        if (!p.isValid())
            beginInsertRows(QModelIndex(), offsetOf(m) + f, offsetOf(m) + l);
    });
    s.connections << connect(m, &M::rowsInserted, this, [this](const QModelIndex &p) {
        if (!p.isValid())
            endInsertRows();
    });
    s.connections << connect(m, &M::rowsAboutToBeRemoved, this, [this, m](const QModelIndex &p, int f, int l) {
        if (!p.isValid())
            beginRemoveRows(QModelIndex(), offsetOf(m) + f, offsetOf(m) + l);
    });
    s.connections << connect(m, &M::rowsRemoved, this, [this](const QModelIndex &p) {
        if (!p.isValid())
            endRemoveRows();
    });
    s.connections << connect(m, &M::rowsAboutToBeMoved, this,
                             [this, m](const QModelIndex &, int f, int l, const QModelIndex &, int d) {
                                 const int o = offsetOf(m);
                                 beginMoveRows(QModelIndex(), o + f, o + l, QModelIndex(), o + d);
                             });
    s.connections << connect(m, &M::rowsMoved, this, [this] { endMoveRows(); });
    s.connections << connect(m, &M::columnsAboutToBeInserted, this, resetBegin)
                  << connect(m, &M::columnsInserted, this, resetEnd);
    s.connections << connect(m, &M::columnsAboutToBeRemoved, this, resetBegin)
                  << connect(m, &M::columnsRemoved, this, resetEnd);
    s.connections << connect(m, &M::columnsAboutToBeMoved, this, resetBegin)
                  << connect(m, &M::columnsMoved, this, resetEnd);
    s.connections << connect(m, &M::modelAboutToBeReset, this, resetBegin)
                  << connect(m, &M::modelReset, this, resetEnd);
    auto saved = std::make_shared<QList<std::pair<QPersistentModelIndex, QPersistentModelIndex>>>();
    s.connections << connect(m, &M::layoutAboutToBeChanged, this, [this, saved] {
        layoutAboutToBeChanged();
        saved->clear();
        for (const QModelIndex &proxy : persistentIndexList())
            saved->append({proxy, mapToSource(proxy)});
    });
    s.connections << connect(m, &M::layoutChanged, this, [this, saved] {
        QModelIndexList from, to;
        for (const auto &entry : *saved) {
            from.append(entry.first);
            to.append(mapFromSource(entry.second));
        }
        saved->clear();
        changePersistentIndexList(from, to);
        layoutChanged();
    });
    s.connections << connect(
        m, &M::dataChanged, this, [this](const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles) {
            if (m_columns == 0 || tl.column() >= m_columns)
                return;
            const QModelIndex topLeft = mapFromSource(tl);
            const QModelIndex bottomRight = mapFromSource(br.siblingAtColumn(std::min(br.column(), m_columns - 1)));
            if (topLeft.isValid() && bottomRight.isValid())
                dataChanged(topLeft, bottomRight, roles);
        });
    s.connections << connect(m, &M::headerDataChanged, this, [this, m](Qt::Orientation o, int f, int l) {
        if (o == Qt::Vertical)
            headerDataChanged(o, offsetOf(m) + f, offsetOf(m) + l);
        else if (!m_sources.isEmpty() && m_sources.first().model == m)
            headerDataChanged(o, f, std::min(l, m_columns - 1));
    });
    // The source is half destroyed when destroyed() is emitted, so it is
    // dropped with a reset instead of asking it for its row count.
    s.connections << connect(m, &QObject::destroyed, this, [this, m] {
        auto dead = std::find_if(m_sources.begin(), m_sources.end(), [m](const Source &x) { return x.model == m; });
        if (dead == m_sources.end())
            return;
        beginResetModel();
        disconnectSource(*dead);
        m_sources.erase(dead);
        m_columns = minimumColumns(sourceModels());
        endResetModel();
    });
    m_sources.append(s);

    if (reset) {
        m_columns = newColumns;
        endResetModel();
    } else if (rows > 0) {
        endInsertRows();
    }
}

/*!
    Removes the rows of \a sourceModel from the proxy.

    \sa addSourceModel()
*/
void QConcatenateTablesProxyModel::removeSourceModel(QAbstractItemModel *sourceModel) {
    auto it = std::find_if(m_sources.begin(), m_sources.end(), [&](const Source &s) { return s.model == sourceModel; });
    if (it == m_sources.end())
        return;
    QList<QAbstractItemModel *> remaining = sourceModels();
    remaining.removeAll(sourceModel);
    const int newColumns = minimumColumns(remaining);
    const bool reset = newColumns != m_columns;
    const int first = offsetOf(it->model);
    const int rows = it->model->rowCount();
    if (reset)
        beginResetModel();
    else if (rows > 0)
        beginRemoveRows(QModelIndex(), first, first + rows - 1);
    disconnectSource(*it);
    m_sources.erase(it);
    if (reset) {
        m_columns = newColumns;
        endResetModel();
    } else if (rows > 0) {
        endRemoveRows();
    }
}

/*!
    Returns the proxy index of \a sourceIndex, an index of one of the source
    models, or an invalid index.
*/
QModelIndex QConcatenateTablesProxyModel::mapFromSource(const QModelIndex &sourceIndex) const {
    if (!sourceIndex.isValid() || sourceIndex.column() >= m_columns)
        return QModelIndex();
    const int offset = offsetOf(sourceIndex.model());
    return offset < 0 ? QModelIndex() : createIndex(offset + sourceIndex.row(), sourceIndex.column());
}

/*!
    Returns the source index of \a proxyIndex, or an invalid index.
*/
QModelIndex QConcatenateTablesProxyModel::mapToSource(const QModelIndex &proxyIndex) const {
    if (!proxyIndex.isValid() || proxyIndex.model() != this)
        return QModelIndex();
    int local = 0;
    const int k = sourceForRow(proxyIndex.row(), &local);
    return k < 0 ? QModelIndex() : m_sources.at(k).model->index(local, proxyIndex.column());
}

/*!
    \reimp
    Returns the data of the source item of \a index for \a role.
*/
QVariant QConcatenateTablesProxyModel::data(const QModelIndex &index, int role) const {
    return mapToSource(index).data(role);
}

/*!
    \reimp
    Sets the data of the source item of \a index for \a role to \a value.
*/
bool QConcatenateTablesProxyModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    const QModelIndex s = mapToSource(index);
    return s.isValid() && const_cast<QAbstractItemModel *>(s.model())->setData(s, value, role);
}

/*!
    \reimp
    Returns the item data of the source item of \a proxyIndex.
*/
QMap<int, QVariant> QConcatenateTablesProxyModel::itemData(const QModelIndex &proxyIndex) const {
    const QModelIndex s = mapToSource(proxyIndex);
    return s.isValid() ? s.model()->itemData(s) : QMap<int, QVariant>();
}

/*!
    \reimp
    Sets the item data of the source item of \a index to \a roles.
*/
bool QConcatenateTablesProxyModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) {
    const QModelIndex s = mapToSource(index);
    return s.isValid() && const_cast<QAbstractItemModel *>(s.model())->setItemData(s, roles);
}

/*!
    \reimp
    Returns the flags of the source item of \a index, or none for an invalid
    index.
*/
Qt::ItemFlags QConcatenateTablesProxyModel::flags(const QModelIndex &index) const {
    const QModelIndex s = mapToSource(index);
    return s.isValid() ? s.flags() : Qt::ItemFlags();
}

/*!
    \reimp
    Returns the index at \a row and \a column; \a parent must be invalid.
*/
QModelIndex QConcatenateTablesProxyModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column) : QModelIndex();
}

/*!
    \reimp
    Returns an invalid index: the proxy is flat. \a index is not used.
*/
QModelIndex QConcatenateTablesProxyModel::parent(const QModelIndex &index) const {
    Q_UNUSED(index)
    return QModelIndex();
}

/*!
    \reimp
    Returns the total number of rows of the source models, or 0 for a valid
    \a parent.
*/
int QConcatenateTablesProxyModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    int rows = 0;
    for (const Source &s : m_sources)
        if (s.model)
            rows += s.model->rowCount();
    return rows;
}

/*!
    \reimp
    Returns the header data of \a section in \a orientation for \a role: the
    horizontal headers come from the first source model, and the vertical
    headers from the source of each row.
*/
QVariant QConcatenateTablesProxyModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal)
        return !m_sources.isEmpty() && m_sources.first().model
                   ? m_sources.first().model->headerData(section, orientation, role)
                   : QVariant();
    int local = 0;
    const int k = sourceForRow(section, &local);
    return k < 0 ? QVariant() : m_sources.at(k).model->headerData(local, orientation, role);
}

/*!
    \reimp
    Returns the smallest column count of the source models, or 0 for a valid
    \a parent.
*/
int QConcatenateTablesProxyModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_columns;
}

/*!
    \reimp
    Returns the MIME types that all source models support.
*/
QStringList QConcatenateTablesProxyModel::mimeTypes() const {
    const QList<QAbstractItemModel *> models = sourceModels();
    if (models.isEmpty())
        return QAbstractItemModel::mimeTypes();
    QStringList r = models.first()->mimeTypes();
    for (const QAbstractItemModel *m : models) {
        const QStringList types = m->mimeTypes();
        r.removeIf([&](const QString &t) { return !types.contains(t); });
    }
    return r;
}

/*!
    \reimp
    Returns the MIME data of the source items of \a indexes, which must all
    belong to the same source model; otherwise returns \c nullptr.
*/
QMimeData *QConcatenateTablesProxyModel::mimeData(const QModelIndexList &indexes) const {
    QModelIndexList source;
    const QAbstractItemModel *model = nullptr;
    for (const QModelIndex &index : indexes) {
        const QModelIndex s = mapToSource(index);
        if (!s.isValid())
            continue;
        if (model && s.model() != model)
            return nullptr;
        model = s.model();
        source.append(s);
    }
    return model ? model->mimeData(source) : nullptr;
}

/*!
    \reimp
    Returns whether the source model at \a row accepts \a data dropped with
    \a action at \a column under \a parent.
*/
bool QConcatenateTablesProxyModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                                                   const QModelIndex &parent) const {
    if (parent.isValid()) {
        const QModelIndex s = mapToSource(parent);
        return s.isValid() && s.model()->canDropMimeData(data, action, -1, -1, s);
    }
    if (m_sources.isEmpty())
        return false;
    int local = 0;
    int k = row < 0 ? static_cast<int>(m_sources.size()) - 1 : sourceForRow(row, &local);
    if (k < 0) {
        k = static_cast<int>(m_sources.size()) - 1;
        local = m_sources.at(k).model ? m_sources.at(k).model->rowCount() : 0;
    }
    return m_sources.at(k).model &&
           m_sources.at(k).model->canDropMimeData(data, action, row < 0 ? -1 : local, column, QModelIndex());
}

/*!
    \reimp
    Drops \a data with \a action into the source model at \a row and
    \a column under \a parent.
*/
bool QConcatenateTablesProxyModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                                                const QModelIndex &parent) {
    if (parent.isValid()) {
        const QModelIndex s = mapToSource(parent);
        return s.isValid() && const_cast<QAbstractItemModel *>(s.model())->dropMimeData(data, action, -1, -1, s);
    }
    if (m_sources.isEmpty())
        return false;
    int local = 0;
    int k = row < 0 ? static_cast<int>(m_sources.size()) - 1 : sourceForRow(row, &local);
    if (k < 0) {
        k = static_cast<int>(m_sources.size()) - 1;
        local = m_sources.at(k).model ? m_sources.at(k).model->rowCount() : 0;
    }
    return m_sources.at(k).model &&
           m_sources.at(k).model->dropMimeData(data, action, row < 0 ? -1 : local, column, QModelIndex());
}

/*!
    \reimp
    Returns the span of the source item of \a index.
*/
QSize QConcatenateTablesProxyModel::span(const QModelIndex &index) const {
    const QModelIndex s = mapToSource(index);
    return s.isValid() ? s.model()->span(s) : QSize(1, 1);
}

#include "moc_qconcatenatetablesproxymodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
