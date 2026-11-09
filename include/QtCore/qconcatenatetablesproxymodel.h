// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>

/*!
    \class QConcatenateTablesProxyModel
    \since 5.13
    \brief The QConcatenateTablesProxyModel class proxies multiple source models, concatenating their rows.

    The rows of the first source model come first, followed by those of the
    second, and so on. The proxy has as many columns as the source with the
    fewest. The source models must be flat: their items have no children.

    \code
    QConcatenateTablesProxyModel all;
    all.addSourceModel(&inbox);
    all.addSourceModel(&archive);
    \endcode

    \differences
    A change of the column count of a source resets the proxy.
    \enddifferences

    \sa QAbstractItemModel
*/
class QConcatenateTablesProxyModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit QConcatenateTablesProxyModel(QObject *parent = nullptr);
    ~QConcatenateTablesProxyModel() override;

    QList<QAbstractItemModel *> sourceModels() const;
    void addSourceModel(QAbstractItemModel *sourceModel);
    void removeSourceModel(QAbstractItemModel *sourceModel);

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QMap<int, QVariant> itemData(const QModelIndex &proxyIndex) const override;
    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;
    QSize span(const QModelIndex &index) const override;

private:
    struct Source {
        QAbstractItemModel *model = nullptr;
        QList<QMetaObject::Connection> connections;
    };
    int offsetOf(const QAbstractItemModel *model) const;
    int sourceForRow(int row, int *localRow) const;
    void disconnectSource(Source &s);
    QList<Source> m_sources;
    int m_columns = 0;
};
