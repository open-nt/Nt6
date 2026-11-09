// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qitemselectionmodel.h>

/*!
    \class QAbstractProxyModel
    \since 4.1
    \brief The QAbstractProxyModel class provides a base class for proxy item models that can do sorting, filtering or other data processing tasks.

    A proxy model presents the items of a source model in another form.
    Subclasses implement mapToSource() and mapFromSource(); the data,
    header, flag and MIME functions are forwarded through them.

    \differences
    sourceModelChanged() has no \c QPrivateSignal argument, and there are no
    bindable properties.
    \enddifferences

    \sa QIdentityProxyModel, QSortFilterProxyModel
*/
class QAbstractProxyModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit QAbstractProxyModel(QObject *parent = nullptr);
    ~QAbstractProxyModel() override;

    virtual void setSourceModel(QAbstractItemModel *sourceModel);
    QAbstractItemModel *sourceModel() const;
    /*!
        Returns the source index of the item at \a proxyIndex, or an invalid
        index.
    */
    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const = 0;
    /*!
        Returns the proxy index of the source item at \a sourceIndex, or an
        invalid index if the proxy does not show it.
    */
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const = 0;
    virtual QItemSelection mapSelectionToSource(const QItemSelection &selection) const;
    virtual QItemSelection mapSelectionFromSource(const QItemSelection &selection) const;

    QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QMap<int, QVariant> itemData(const QModelIndex &index) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
                       int role = Qt::EditRole) override;
    bool clearItemData(const QModelIndex &index) override;
    QModelIndex buddy(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    QSize span(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;
    QStringList mimeTypes() const override;
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    bool submit() override;
    void revert() override;

signals:
    /*!
        \since 5.1
        This signal is emitted when the source model changes.
    */
    void sourceModelChanged() { emitSignal(&QAbstractProxyModel::sourceModelChanged); }

protected:
    QModelIndex createSourceIndex(int row, int col, void *internalPtr) const;
    // Maps drop coordinates to the source: sets *sourceRow, *sourceColumn and *sourceParent.
    bool mapDropCoordinatesToSource(int row, int column, const QModelIndex &parent, int *sourceRow, int *sourceColumn,
                                    QModelIndex *sourceParent) const;

private:
    QAbstractItemModel *m_source = nullptr;
    QMetaObject::Connection m_destroyed;
};
