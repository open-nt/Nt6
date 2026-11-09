// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qsize.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <memory>

class QAbstractItemModel;
class QMimeData;
struct QPersistentModelIndexData;
struct QAbstractItemModelPrivate;

/*!
    \class QModelIndex
    \brief The QModelIndex class is used to locate data in a data model.

    A model index refers to an item of a QAbstractItemModel by its row, its
    column and its parent. Models create indexes with
    QAbstractItemModel::createIndex(); views and delegates use them to read
    and change data. An index is only valid until the model's structure
    changes; use QPersistentModelIndex to keep one.

    \code
    const QModelIndex index = model->index(2, 0);
    const QString text = index.data().toString();
    \endcode

    \differences
    There is no \c multiData() and no QModelRoleDataSpan.
    \enddifferences

    \sa QAbstractItemModel, QPersistentModelIndex
*/
class QModelIndex {
public:
    /*!
        Constructs an invalid index, which refers to the root of a model.
    */
    constexpr QModelIndex() noexcept = default;

    /*!
        Returns the row of the index, or -1 if it is invalid.
    */
    constexpr int row() const noexcept { return r; }
    /*!
        Returns the column of the index, or -1 if it is invalid.
    */
    constexpr int column() const noexcept { return c; }
    /*!
        Returns the integer the model stored in the index.

        \sa internalPointer()
    */
    constexpr quintptr internalId() const noexcept { return i; }
    /*!
        Returns the pointer the model stored in the index.

        \sa internalId()
    */
    void *internalPointer() const noexcept {
        return reinterpret_cast<void *>(i); // NOLINT(performance-no-int-to-ptr): the id holds the model's pointer
    }
    /*!
        \since 6.0
        Returns the pointer the model stored in the index, as a pointer to
        const.
    */
    const void *constInternalPointer() const noexcept {
        return reinterpret_cast<const void *>(i); // NOLINT(performance-no-int-to-ptr): the id holds the model's pointer
    }
    QModelIndex parent() const;
    QModelIndex sibling(int row, int column) const;
    QModelIndex siblingAtColumn(int column) const;
    QModelIndex siblingAtRow(int row) const;
    QVariant data(int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags() const;
    /*!
        Returns the model that the index belongs to, or \c nullptr for an
        invalid index.
    */
    constexpr const QAbstractItemModel *model() const noexcept { return m; }
    /*!
        Returns \c true if the index refers to an item; otherwise returns
        \c false.
    */
    constexpr bool isValid() const noexcept { return r >= 0 && c >= 0 && m != nullptr; }

    friend constexpr bool operator==(const QModelIndex &a, const QModelIndex &b) noexcept {
        return a.r == b.r && a.i == b.i && a.c == b.c && a.m == b.m;
    }
    friend constexpr bool operator!=(const QModelIndex &a, const QModelIndex &b) noexcept { return !(a == b); }
    friend bool operator<(const QModelIndex &a, const QModelIndex &b) noexcept {
        if (a.r != b.r)
            return a.r < b.r;
        if (a.c != b.c)
            return a.c < b.c;
        if (a.i != b.i)
            return a.i < b.i;
        return std::less<const QAbstractItemModel *>()(a.m, b.m);
    }

private:
    friend class QAbstractItemModel;
    constexpr QModelIndex(int row, int column, quintptr id, const QAbstractItemModel *model) noexcept : r(row), c(column), i(id), m(model) {}
    int r = -1;
    int c = -1;
    quintptr i = 0;
    const QAbstractItemModel *m = nullptr;
};

// A list of model indexes.
using QModelIndexList = QList<QModelIndex>;

inline std::size_t qHash(const QModelIndex &index, std::size_t seed = 0) noexcept {
    return (static_cast<std::size_t>(index.row()) << 4) + static_cast<std::size_t>(index.column()) + index.internalId() + seed;
}

/*!
    \class QPersistentModelIndex
    \brief The QPersistentModelIndex class is used to locate data in a data model, and stays valid when the model changes.

    A persistent index follows its item when rows and columns are inserted,
    removed or moved, and becomes invalid when the item is removed or the
    model is reset. Models that rearrange their items (for example when
    sorting) update persistent indexes with changePersistentIndex().

    \differences
    Each persistent index made from a QModelIndex has its own record in the
    model, which copies share; Qt shares one record per item.
    \enddifferences

    \sa QModelIndex, QAbstractItemModel
*/
class QPersistentModelIndex {
public:
    QPersistentModelIndex();
    QPersistentModelIndex(const QModelIndex &index);
    QPersistentModelIndex(const QPersistentModelIndex &other);
    QPersistentModelIndex(QPersistentModelIndex &&other) noexcept;
    ~QPersistentModelIndex();
    QPersistentModelIndex &operator=(const QPersistentModelIndex &other);
    QPersistentModelIndex &operator=(QPersistentModelIndex &&other) noexcept;
    QPersistentModelIndex &operator=(const QModelIndex &other);
    /*!
        Swaps this index with \a other.
    */
    void swap(QPersistentModelIndex &other) noexcept { d.swap(other.d); }

    operator QModelIndex() const;
    int row() const;
    int column() const;
    void *internalPointer() const;
    const void *constInternalPointer() const;
    quintptr internalId() const;
    QModelIndex parent() const;
    QModelIndex sibling(int row, int column) const;
    QVariant data(int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags() const;
    const QAbstractItemModel *model() const;
    bool isValid() const;

    friend bool operator==(const QPersistentModelIndex &a, const QPersistentModelIndex &b) noexcept {
        return QModelIndex(a) == QModelIndex(b);
    }
    friend bool operator!=(const QPersistentModelIndex &a, const QPersistentModelIndex &b) noexcept { return !(a == b); }
    friend bool operator==(const QPersistentModelIndex &a, const QModelIndex &b) noexcept { return QModelIndex(a) == b; }
    friend bool operator!=(const QPersistentModelIndex &a, const QModelIndex &b) noexcept { return QModelIndex(a) != b; }
    friend bool operator==(const QModelIndex &a, const QPersistentModelIndex &b) noexcept { return a == QModelIndex(b); }
    friend bool operator!=(const QModelIndex &a, const QPersistentModelIndex &b) noexcept { return a != QModelIndex(b); }
    friend bool operator<(const QPersistentModelIndex &a, const QPersistentModelIndex &b) noexcept { return QModelIndex(a) < QModelIndex(b); }

private:
    friend class QAbstractItemModel;
    std::shared_ptr<QPersistentModelIndexData> d;
};

inline std::size_t qHash(const QPersistentModelIndex &index, std::size_t seed = 0) noexcept { return qHash(QModelIndex(index), seed); }

/*!
    \class QAbstractItemModel
    \brief The QAbstractItemModel class provides the abstract interface for item model classes.

    An item model presents data as a table of items, where each item can have
    a table of child items. Subclasses implement index(), parent(),
    rowCount(), columnCount() and data(); editable models also implement
    setData() and flags(), and resizable models insertRows() and
    removeRows().

    Changes are announced with the begin and end functions, such as
    beginInsertRows() and endInsertRows(), which emit the matching signals
    and keep persistent indexes up to date:

    \code
    bool MyModel::insertRows(int row, int count, const QModelIndex &parent)
    {
        beginInsertRows(parent, row, row + count - 1);
        m_items.insert(row, count, QString());
        endInsertRows();
        return true;
    }
    \endcode

    \differences
    The signals are ordinary public signals, without Qt's \c QPrivateSignal
    argument. There is no \c multiData(), \c encodeData() or \c decodeData();
    the default mimeData() and dropMimeData() use their own encoding of the
    item data under the Qt MIME type. Item data in that encoding is limited to
    the types that QDataStream can store in a QVariant.
    \enddifferences

    \sa QModelIndex, QAbstractListModel, QAbstractTableModel, QSortFilterProxyModel
*/
class QAbstractItemModel : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QAbstractItemModel::LayoutChangeHint

        \value NoLayoutChangeHint No hint is available.
        \value VerticalSortHint Rows are being sorted.
        \value HorizontalSortHint Columns are being sorted.
    */
    enum LayoutChangeHint { NoLayoutChangeHint, VerticalSortHint, HorizontalSortHint };

    /*!
        \enum QAbstractItemModel::CheckIndexOption
        \since 5.11

        \value NoOption No check option is set.
        \value IndexIsValid The index must be valid.
        \value DoNotUseParent The parent of the index is not checked.
        \value ParentIsInvalid The parent of the index must be invalid: the index is at the top level.
    */
    enum class CheckIndexOption { NoOption = 0x0000, IndexIsValid = 0x0001, DoNotUseParent = 0x0002, ParentIsInvalid = 0x0004 };
    Q_DECLARE_FLAGS(CheckIndexOptions, CheckIndexOption)

    explicit QAbstractItemModel(QObject *parent = nullptr);
    ~QAbstractItemModel() override;

    bool hasIndex(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    /*!
        Returns the index of the item at \a row and \a column under
        \a parent. Subclasses implement it with createIndex().

        \sa parent(), createIndex()
    */
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const = 0;
    /*!
        Returns the parent of the item at \a child, or an invalid index for
        a top-level item.
    */
    virtual QModelIndex parent(const QModelIndex &child) const = 0;
    virtual QModelIndex sibling(int row, int column, const QModelIndex &idx) const;
    /*!
        Returns the number of rows under \a parent.
    */
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const = 0;
    /*!
        Returns the number of columns under \a parent.
    */
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const = 0;
    virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
    /*!
        Returns the data of the item at \a index for \a role, or an invalid
        QVariant.

        \sa setData(), Qt::ItemDataRole
    */
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const = 0;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    virtual bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);
    virtual QMap<int, QVariant> itemData(const QModelIndex &index) const;
    virtual bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles);
    virtual bool clearItemData(const QModelIndex &index);
    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const;
    virtual bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const;
    virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    virtual Qt::DropActions supportedDropActions() const;
    virtual Qt::DropActions supportedDragActions() const;
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
    virtual bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex());
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    virtual bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex());
    virtual bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild);
    virtual bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count, const QModelIndex &destinationParent,
                             int destinationChild);
    bool insertRow(int row, const QModelIndex &parent = QModelIndex());
    bool insertColumn(int column, const QModelIndex &parent = QModelIndex());
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());
    bool removeColumn(int column, const QModelIndex &parent = QModelIndex());
    bool moveRow(const QModelIndex &sourceParent, int sourceRow, const QModelIndex &destinationParent, int destinationChild);
    bool moveColumn(const QModelIndex &sourceParent, int sourceColumn, const QModelIndex &destinationParent, int destinationChild);
    virtual void fetchMore(const QModelIndex &parent);
    virtual bool canFetchMore(const QModelIndex &parent) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
    virtual QModelIndex buddy(const QModelIndex &index) const;
    virtual QModelIndexList match(const QModelIndex &start, int role, const QVariant &value, int hits = 1,
                                  Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap)) const;
    virtual QSize span(const QModelIndex &index) const;
    virtual QHash<int, QByteArray> roleNames() const;
    bool checkIndex(const QModelIndex &index, CheckIndexOptions options = CheckIndexOption::NoOption) const;

public slots:
    virtual bool submit();
    virtual void revert();

signals:
    /*!
        This signal is emitted when the data of the items from \a topLeft to
        \a bottomRight changes. \a roles lists the roles that changed; an
        empty list means all of them.
    */
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles = QList<int>()) {
        emitSignal(&QAbstractItemModel::dataChanged, topLeft, bottomRight, roles);
    }
    /*!
        This signal is emitted when the header sections \a first to \a last
        of \a orientation change.
    */
    void headerDataChanged(Qt::Orientation orientation, int first, int last) {
        emitSignal(&QAbstractItemModel::headerDataChanged, orientation, first, last);
    }
    /*!
        This signal is emitted after the items under \a parents (all items if
        empty) were rearranged; \a hint describes how.

        \sa layoutAboutToBeChanged()
    */
    void layoutChanged(const QList<QPersistentModelIndex> &parents = QList<QPersistentModelIndex>(),
                       QAbstractItemModel::LayoutChangeHint hint = QAbstractItemModel::NoLayoutChangeHint) {
        emitSignal(&QAbstractItemModel::layoutChanged, parents, hint);
    }
    /*!
        This signal is emitted before the items under \a parents (all items if
        empty) are rearranged; \a hint describes how.
    */
    void layoutAboutToBeChanged(const QList<QPersistentModelIndex> &parents = QList<QPersistentModelIndex>(),
                                QAbstractItemModel::LayoutChangeHint hint = QAbstractItemModel::NoLayoutChangeHint) {
        emitSignal(&QAbstractItemModel::layoutAboutToBeChanged, parents, hint);
    }
    /*!
        This signal is emitted before rows \a first to \a last are inserted
        under \a parent.
    */
    void rowsAboutToBeInserted(const QModelIndex &parent, int first, int last) {
        emitSignal(&QAbstractItemModel::rowsAboutToBeInserted, parent, first, last);
    }
    /*!
        This signal is emitted after rows \a first to \a last were inserted
        under \a parent.
    */
    void rowsInserted(const QModelIndex &parent, int first, int last) { emitSignal(&QAbstractItemModel::rowsInserted, parent, first, last); }
    /*!
        This signal is emitted before rows \a first to \a last are removed
        from under \a parent.
    */
    void rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last) {
        emitSignal(&QAbstractItemModel::rowsAboutToBeRemoved, parent, first, last);
    }
    /*!
        This signal is emitted after rows \a first to \a last were removed
        from under \a parent.
    */
    void rowsRemoved(const QModelIndex &parent, int first, int last) { emitSignal(&QAbstractItemModel::rowsRemoved, parent, first, last); }
    /*!
        This signal is emitted before columns \a first to \a last are
        inserted under \a parent.
    */
    void columnsAboutToBeInserted(const QModelIndex &parent, int first, int last) {
        emitSignal(&QAbstractItemModel::columnsAboutToBeInserted, parent, first, last);
    }
    /*!
        This signal is emitted after columns \a first to \a last were
        inserted under \a parent.
    */
    void columnsInserted(const QModelIndex &parent, int first, int last) {
        emitSignal(&QAbstractItemModel::columnsInserted, parent, first, last);
    }
    /*!
        This signal is emitted before columns \a first to \a last are removed
        from under \a parent.
    */
    void columnsAboutToBeRemoved(const QModelIndex &parent, int first, int last) {
        emitSignal(&QAbstractItemModel::columnsAboutToBeRemoved, parent, first, last);
    }
    /*!
        This signal is emitted after columns \a first to \a last were removed
        from under \a parent.
    */
    void columnsRemoved(const QModelIndex &parent, int first, int last) {
        emitSignal(&QAbstractItemModel::columnsRemoved, parent, first, last);
    }
    /*!
        This signal is emitted when the model is about to be reset.
    */
    void modelAboutToBeReset() { emitSignal(&QAbstractItemModel::modelAboutToBeReset); }
    /*!
        This signal is emitted after the model was reset.
    */
    void modelReset() { emitSignal(&QAbstractItemModel::modelReset); }
    /*!
        This signal is emitted before rows \a sourceStart to \a sourceEnd
        under \a sourceParent move to before row \a destinationRow under
        \a destinationParent.
    */
    void rowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent,
                            int destinationRow) {
        emitSignal(&QAbstractItemModel::rowsAboutToBeMoved, sourceParent, sourceStart, sourceEnd, destinationParent, destinationRow);
    }
    /*!
        This signal is emitted after rows \a start to \a end under \a parent
        moved to before row \a row under \a destination.
    */
    void rowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row) {
        emitSignal(&QAbstractItemModel::rowsMoved, parent, start, end, destination, row);
    }
    /*!
        This signal is emitted before columns \a sourceStart to \a sourceEnd
        under \a sourceParent move to before column \a destinationColumn
        under \a destinationParent.
    */
    void columnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent,
                               int destinationColumn) {
        emitSignal(&QAbstractItemModel::columnsAboutToBeMoved, sourceParent, sourceStart, sourceEnd, destinationParent, destinationColumn);
    }
    /*!
        This signal is emitted after columns \a start to \a end under
        \a parent moved to before column \a column under \a destination.
    */
    void columnsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int column) {
        emitSignal(&QAbstractItemModel::columnsMoved, parent, start, end, destination, column);
    }

protected:
    /*!
        Creates an index of this model for \a row and \a column that stores
        the pointer \a ptr.
    */
    QModelIndex createIndex(int row, int column, const void *ptr = nullptr) const {
        return QModelIndex(row, column, reinterpret_cast<quintptr>(ptr), this);
    }
    /*!
        \overload
        Creates an index of this model for \a row and \a column that stores
        the integer \a id.
    */
    QModelIndex createIndex(int row, int column, quintptr id) const { return QModelIndex(row, column, id, this); }

    void beginInsertRows(const QModelIndex &parent, int first, int last);
    void endInsertRows();
    void beginRemoveRows(const QModelIndex &parent, int first, int last);
    void endRemoveRows();
    bool beginMoveRows(const QModelIndex &sourceParent, int sourceFirst, int sourceLast, const QModelIndex &destinationParent,
                       int destinationRow);
    void endMoveRows();
    void beginInsertColumns(const QModelIndex &parent, int first, int last);
    void endInsertColumns();
    void beginRemoveColumns(const QModelIndex &parent, int first, int last);
    void endRemoveColumns();
    bool beginMoveColumns(const QModelIndex &sourceParent, int sourceFirst, int sourceLast, const QModelIndex &destinationParent,
                          int destinationColumn);
    void endMoveColumns();
    void beginResetModel();
    void endResetModel();
    void changePersistentIndex(const QModelIndex &from, const QModelIndex &to);
    void changePersistentIndexList(const QModelIndexList &from, const QModelIndexList &to);
    QModelIndexList persistentIndexList() const;

protected slots:
    virtual void resetInternalData();

private:
    friend class QPersistentModelIndex;
    friend struct QPersistentModelIndexData;
    friend class QAbstractProxyModel;
    friend struct QAbstractItemModelPrivate;
    std::unique_ptr<QAbstractItemModelPrivate> d_ptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractItemModel::CheckIndexOptions)

/*!
    \class QAbstractTableModel
    \brief The QAbstractTableModel class provides an abstract model that can be subclassed to create table models.

    A table model has rows and columns but no child items. Subclasses
    implement rowCount(), columnCount() and data(); index() and parent() are
    provided.

    \differences
    None.
    \enddifferences

    \sa QAbstractListModel, QAbstractItemModel
*/
class QAbstractTableModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit QAbstractTableModel(QObject *parent = nullptr);
    ~QAbstractTableModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QModelIndex parent(const QModelIndex &child) const override;
    bool hasChildren(const QModelIndex &parent) const override;
};

/*!
    \class QAbstractListModel
    \brief The QAbstractListModel class provides an abstract model that can be subclassed to create one-dimensional list models.

    A list model has one column of items without children. Subclasses
    implement rowCount() and data().

    \differences
    None.
    \enddifferences

    \sa QStringListModel, QAbstractItemModel
*/
class QAbstractListModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit QAbstractListModel(QObject *parent = nullptr);
    ~QAbstractListModel() override;

    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    int columnCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    bool hasChildren(const QModelIndex &parent) const override;
};
