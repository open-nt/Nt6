// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractproxymodel.h>

#include <memory>

/*!
    \class QIdentityProxyModel
    \since 4.8
    \brief The QIdentityProxyModel class proxies its source model unmodified.

    A QIdentityProxyModel presents the source model's structure unchanged:
    each proxy index corresponds to the source index at the same row and
    column. It is the base for proxies that change only the data, by
    reimplementing data():

    \code
    class UpperCase : public QIdentityProxyModel
    {
    public:
        QVariant data(const QModelIndex &index, int role) const override
        {
            const QVariant v = QIdentityProxyModel::data(index, role);
            return role == Qt::DisplayRole ? QVariant(v.toString().toUpper()) : v;
        }
    };
    \endcode

    \differences
    There are no \c handleSourceLayoutChanges() and
    \c handleSourceDataChanges() settings; changes are always forwarded.
    \enddifferences

    \sa QAbstractProxyModel, QSortFilterProxyModel
*/
class QIdentityProxyModel : public QAbstractProxyModel {
    Q_OBJECT
public:
    explicit QIdentityProxyModel(QObject *parent = nullptr);
    ~QIdentityProxyModel() override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    QItemSelection mapSelectionFromSource(const QItemSelection &selection) const override;
    QItemSelection mapSelectionToSource(const QItemSelection &selection) const override;
    QModelIndexList match(const QModelIndex &start, int role, const QVariant &value, int hits = 1,
                          Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap)) const override;
    void setSourceModel(QAbstractItemModel *sourceModel) override;
    bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent,
                  int destinationChild) override;
    bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count, const QModelIndex &destinationParent,
                     int destinationChild) override;

    bool handleSourceDataChanges() const;
    bool handleSourceLayoutChanges() const;

protected:
    void setHandleSourceDataChanges(bool b);
    void setHandleSourceLayoutChanges(bool b);

private:
    QList<QMetaObject::Connection> m_connections;
    bool m_handleData = true;
    bool m_handleLayout = true;
    std::shared_ptr<QList<std::pair<QPersistentModelIndex, QPersistentModelIndex>>> m_layoutSaved;
};
