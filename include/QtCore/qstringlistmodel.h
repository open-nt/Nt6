// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qstringlist.h>

/*!
    \class QStringListModel
    \brief The QStringListModel class provides a model that supplies strings to views.

    A QStringListModel is an editable list model whose items are the strings
    of a QStringList, available under Qt::DisplayRole and Qt::EditRole.

    \code
    QStringListModel model({u"apple"_s, u"banana"_s});
    model.insertRows(1, 1);
    model.setData(model.index(1), u"cherry"_s);
    model.stringList(); // apple, cherry, banana
    \endcode

    \differences
    Only the display and edit roles are stored; data for other roles, such as
    Qt::CheckStateRole, cannot be set.
    \enddifferences

    \sa QAbstractListModel, QStringList
*/
class QStringListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit QStringListModel(QObject *parent = nullptr);
    explicit QStringListModel(const QStringList &strings, QObject *parent = nullptr);
    ~QStringListModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool clearItemData(const QModelIndex &index) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent,
                  int destinationChild) override;
    QMap<int, QVariant> itemData(const QModelIndex &index) const override;
    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    Qt::DropActions supportedDropActions() const override;

    QStringList stringList() const;
    void setStringList(const QStringList &strings);

private:
    QStringList m_strings;
};
