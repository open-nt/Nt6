// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractproxymodel.h>

#include <memory>

/*!
    \class QTransposeProxyModel
    \since 5.13
    \brief The QTransposeProxyModel class provides a proxy that swaps the rows and columns of its source model.

    The item at row \c r and column \c c of the proxy is the item at row
    \c c and column \c r of the source, at every level of a tree, and the
    horizontal and vertical headers are swapped too.

    \differences
    None.
    \enddifferences

    \sa QAbstractProxyModel
*/
class QTransposeProxyModel : public QAbstractProxyModel {
    Q_OBJECT
public:
    explicit QTransposeProxyModel(QObject *parent = nullptr);
    ~QTransposeProxyModel() override;

    void setSourceModel(QAbstractItemModel *newSourceModel) override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
                       int role = Qt::EditRole) override;
    QSize span(const QModelIndex &index) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent,
                  int destinationChild) override;
    bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count, const QModelIndex &destinationParent,
                     int destinationChild) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

private:
    QList<QMetaObject::Connection> m_connections;
    std::shared_ptr<QList<std::pair<QPersistentModelIndex, QPersistentModelIndex>>> m_layoutSaved;
};
