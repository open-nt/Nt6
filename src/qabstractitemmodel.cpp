// MIT License - see LICENSE
#include <QtCore/qabstractitemmodel.h>

#include <QtCore/qbuffer.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qlogging.h>
#include <QtCore/qmimedata.h>
#include <QtCore/qregularexpression.h>

#include <algorithm>
#include <vector>

// A persistent index's record. The model updates the index when its structure changes and
// clears it when the item goes away.
struct QPersistentModelIndexData {
    QModelIndex index;
    QAbstractItemModel *model = nullptr;

    explicit QPersistentModelIndexData(const QModelIndex &i);
    ~QPersistentModelIndexData();
    QPersistentModelIndexData(const QPersistentModelIndexData &) = delete;
    QPersistentModelIndexData &operator=(const QPersistentModelIndexData &) = delete;
};

struct QAbstractItemModelPrivate {
    // A pending structural change, recorded by a begin function and applied by the end function.
    struct Change {
        enum Kind { InsertRows, RemoveRows, MoveRows, InsertColumns, RemoveColumns, MoveColumns } kind;
        QModelIndex parent;
        int first, last;
        QModelIndex destination;
        int destinationChild = 0;
        // The persistent indexes the change affects, and their new row or column (-1: removed).
        std::vector<std::pair<QPersistentModelIndexData *, int>> updates;
    };

    std::vector<QPersistentModelIndexData *> persistent;
    std::vector<Change> changes;
    bool resetting = false;

    static QAbstractItemModelPrivate *get(QAbstractItemModel *m) { return m->d_ptr.get(); }

    // Returns true if index is a descendant of the rows (or columns) first..last under parent.
    static bool isDescendantOf(const QModelIndex &index, const QModelIndex &parent, int first, int last, bool rows) {
        QModelIndex p = index.parent();
        QModelIndex child = index;
        while (child.isValid()) {
            if (p == parent) {
                const int pos = rows ? child.row() : child.column();
                return pos >= first && pos <= last && child != index;
            }
            child = p;
            p = p.parent();
        }
        return false;
    }
};

QPersistentModelIndexData::QPersistentModelIndexData(const QModelIndex &i) : index(i), model(const_cast<QAbstractItemModel *>(i.model())) {
    if (model)
        QAbstractItemModelPrivate::get(model)->persistent.push_back(this);
}

QPersistentModelIndexData::~QPersistentModelIndexData() {
    if (!model)
        return;
    auto &list = QAbstractItemModelPrivate::get(model)->persistent;
    list.erase(std::remove(list.begin(), list.end(), this), list.end());
}

namespace {
void invalidate(QPersistentModelIndexData *d) {
    if (!d->model)
        return;
    auto &list = QAbstractItemModelPrivate::get(d->model)->persistent;
    list.erase(std::remove(list.begin(), list.end(), d), list.end());
    d->index = QModelIndex();
    d->model = nullptr;
}

const char kModelMime[] = "application/x-qabstractitemmodeldatalist";
} // namespace

// ------------------------------------------------------------------ QModelIndex

/*!
    Returns the parent of the item, or an invalid index for a top-level item
    or an invalid index.
*/
QModelIndex QModelIndex::parent() const { return m ? m->parent(*this) : QModelIndex(); }

/*!
    Returns the index at \a row and \a column under the same parent, or an
    invalid index.
*/
QModelIndex QModelIndex::sibling(int row, int column) const { return m ? (r == row && c == column ? *this : m->sibling(row, column, *this)) : QModelIndex(); }

/*!
    \since 5.11
    Returns the index in \a column of the same row.
*/
QModelIndex QModelIndex::siblingAtColumn(int column) const { return sibling(r, column); }

/*!
    \since 5.11
    Returns the index in \a row of the same column.
*/
QModelIndex QModelIndex::siblingAtRow(int row) const { return sibling(row, c); }

/*!
    Returns the data of the item for \a role, or an invalid QVariant.
*/
QVariant QModelIndex::data(int role) const { return m ? m->data(*this, role) : QVariant(); }

/*!
    Returns the flags of the item.
*/
Qt::ItemFlags QModelIndex::flags() const { return m ? m->flags(*this) : Qt::ItemFlags(); }

// ------------------------------------------------------------------ QPersistentModelIndex

/*!
    Constructs an invalid persistent index.
*/
QPersistentModelIndex::QPersistentModelIndex() = default;

/*!
    Constructs a persistent index that follows the item at \a index.
*/
QPersistentModelIndex::QPersistentModelIndex(const QModelIndex &index) {
    if (index.isValid())
        d = std::make_shared<QPersistentModelIndexData>(index);
}

/*!
    Constructs a copy of \a other, which follows the same item.
*/
QPersistentModelIndex::QPersistentModelIndex(const QPersistentModelIndex &other) = default;

/*!
    Move-constructs a persistent index from \a other.
*/
QPersistentModelIndex::QPersistentModelIndex(QPersistentModelIndex &&other) noexcept = default;

/*!
    Destroys the persistent index.
*/
QPersistentModelIndex::~QPersistentModelIndex() = default;

/*!
    Assigns \a other to this persistent index and returns a reference to it.
*/
QPersistentModelIndex &QPersistentModelIndex::operator=(const QPersistentModelIndex &other) = default;

/*!
    Move-assigns \a other to this persistent index and returns a reference to
    it.
*/
QPersistentModelIndex &QPersistentModelIndex::operator=(QPersistentModelIndex &&other) noexcept = default;

/*!
    Makes this persistent index follow the item at \a other and returns a
    reference to it.
*/
QPersistentModelIndex &QPersistentModelIndex::operator=(const QModelIndex &other) {
    *this = QPersistentModelIndex(other);
    return *this;
}

/*!
    Returns the current index of the item, or an invalid index.
*/
QPersistentModelIndex::operator QModelIndex() const { return d ? d->index : QModelIndex(); }

/*!
    Returns the current row of the item, or -1.
*/
int QPersistentModelIndex::row() const { return QModelIndex(*this).row(); }

/*!
    Returns the current column of the item, or -1.
*/
int QPersistentModelIndex::column() const { return QModelIndex(*this).column(); }

/*!
    Returns the pointer the model stored in the index.
*/
void *QPersistentModelIndex::internalPointer() const { return QModelIndex(*this).internalPointer(); }

/*!
    \since 6.0
    Returns the pointer the model stored in the index, as a pointer to const.
*/
const void *QPersistentModelIndex::constInternalPointer() const { return QModelIndex(*this).constInternalPointer(); }

/*!
    Returns the integer the model stored in the index.
*/
quintptr QPersistentModelIndex::internalId() const { return QModelIndex(*this).internalId(); }

/*!
    Returns the parent of the item.
*/
QModelIndex QPersistentModelIndex::parent() const { return QModelIndex(*this).parent(); }

/*!
    Returns the index at \a row and \a column under the same parent.
*/
QModelIndex QPersistentModelIndex::sibling(int row, int column) const { return QModelIndex(*this).sibling(row, column); }

/*!
    Returns the data of the item for \a role.
*/
QVariant QPersistentModelIndex::data(int role) const { return QModelIndex(*this).data(role); }

/*!
    Returns the flags of the item.
*/
Qt::ItemFlags QPersistentModelIndex::flags() const { return QModelIndex(*this).flags(); }

/*!
    Returns the model of the item, or \c nullptr.
*/
const QAbstractItemModel *QPersistentModelIndex::model() const { return d ? d->model : nullptr; }

/*!
    Returns \c true if the item still exists; otherwise returns \c false.
*/
bool QPersistentModelIndex::isValid() const { return d && d->index.isValid(); }

// ------------------------------------------------------------------ QAbstractItemModel

/*!
    Constructs an item model with the given \a parent.
*/
QAbstractItemModel::QAbstractItemModel(QObject *parent) : QObject(parent), d_ptr(std::make_unique<QAbstractItemModelPrivate>()) {}

/*!
    Destroys the model. Persistent indexes of the model become invalid.
*/
QAbstractItemModel::~QAbstractItemModel() {
    for (QPersistentModelIndexData *p : std::vector<QPersistentModelIndexData *>(d_ptr->persistent)) {
        p->index = QModelIndex();
        p->model = nullptr;
    }
    d_ptr->persistent.clear();
}

/*!
    Returns \c true if \a row and \a column are inside the table under
    \a parent; otherwise returns \c false.
*/
bool QAbstractItemModel::hasIndex(int row, int column, const QModelIndex &parent) const {
    return row >= 0 && column >= 0 && row < rowCount(parent) && column < columnCount(parent);
}

/*!
    Returns the index at \a row and \a column under the parent of \a idx.
*/
QModelIndex QAbstractItemModel::sibling(int row, int column, const QModelIndex &idx) const {
    if (row == idx.row() && column == idx.column())
        return idx;
    return index(row, column, parent(idx));
}

/*!
    Returns \c true if \a parent has child items; otherwise returns
    \c false. The default asks rowCount() and columnCount().
*/
bool QAbstractItemModel::hasChildren(const QModelIndex &parent) const {
    if (parent.isValid() && parent.model() != this)
        return false;
    return rowCount(parent) > 0 && columnCount(parent) > 0;
}

/*!
    Sets the data of the item at \a index for \a role to \a value and returns
    \c true on success. The default does nothing and returns \c false;
    editable models emit dataChanged() when they change data.
*/
bool QAbstractItemModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    Q_UNUSED(index)
    Q_UNUSED(value)
    Q_UNUSED(role)
    return false;
}

/*!
    Returns the header data of \a section in \a orientation for \a role. The
    default returns the section number counted from 1 for Qt::DisplayRole.
*/
QVariant QAbstractItemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    Q_UNUSED(orientation)
    return role == Qt::DisplayRole ? QVariant(section + 1) : QVariant();
}

/*!
    Sets the header data of \a section in \a orientation for \a role to
    \a value and returns \c true on success. The default returns \c false.
*/
bool QAbstractItemModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) {
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    Q_UNUSED(value)
    Q_UNUSED(role)
    return false;
}

/*!
    Returns the data of the item at \a index for all predefined roles that
    have data.
*/
QMap<int, QVariant> QAbstractItemModel::itemData(const QModelIndex &index) const {
    QMap<int, QVariant> r;
    for (int role = 0; role < Qt::UserRole; ++role) {
        const QVariant v = data(index, role);
        if (v.isValid())
            r.insert(role, v);
    }
    return r;
}

/*!
    Sets the data of the item at \a index for each role in \a roles and
    returns \c true if all of them succeeded.
*/
bool QAbstractItemModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) {
    if (roles.isEmpty())
        return false;
    bool ok = true;
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        ok = setData(index, it.value(), it.key()) && ok;
    return ok;
}

/*!
    \since 6.0
    Removes the data of all roles of the item at \a index and returns
    \c true on success. The default returns \c false.
*/
bool QAbstractItemModel::clearItemData(const QModelIndex &index) {
    Q_UNUSED(index)
    return false;
}

/*!
    Returns the MIME types that mimeData() produces; the default is the Qt
    item list type \c{application/x-qabstractitemmodeldatalist}.
*/
QStringList QAbstractItemModel::mimeTypes() const { return {QString::fromLatin1(kModelMime)}; }

/*!
    Returns a new QMimeData object with the data of the items at \a indexes,
    or \c nullptr if \a indexes is empty. The caller owns the object.
*/
QMimeData *QAbstractItemModel::mimeData(const QModelIndexList &indexes) const {
    if (indexes.isEmpty())
        return nullptr;
    const QStringList types = mimeTypes();
    if (types.isEmpty())
        return nullptr;
    QByteArray encoded;
    {
        QDataStream out(&encoded, QIODevice::WriteOnly);
        for (const QModelIndex &index : indexes) {
            const QMap<int, QVariant> data = itemData(index);
            out << qint32(index.row()) << qint32(index.column()) << qint32(data.size());
            for (auto it = data.cbegin(); it != data.cend(); ++it)
                out << qint32(it.key()) << it.value();
        }
    }
    auto *mime = new QMimeData;
    mime->setData(types.at(0), encoded);
    return mime;
}

/*!
    Returns \c true if \a data can be dropped with \a action at \a row and
    \a column under \a parent; otherwise returns \c false. The default checks
    the format and the action.
*/
bool QAbstractItemModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const {
    Q_UNUSED(row)
    Q_UNUSED(column)
    Q_UNUSED(parent)
    if (!data || !(supportedDropActions() & action))
        return false;
    const QStringList types = mimeTypes();
    return std::any_of(types.begin(), types.end(), [&](const QString &t) { return data->hasFormat(t); });
}

/*!
    Inserts the items in \a data, dropped with \a action, at \a row and
    \a column under \a parent and returns \c true on success. The default
    understands the format of mimeData(): it inserts rows and sets the item
    data.
*/
bool QAbstractItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    if (!data || !(action == Qt::CopyAction || action == Qt::MoveAction))
        return false;
    if (action == Qt::IgnoreAction)
        return true;
    const QByteArray encoded = data->data(QString::fromLatin1(kModelMime));
    if (encoded.isEmpty())
        return false;
    QDataStream in(encoded);
    struct Item {
        int row, column;
        QMap<int, QVariant> data;
    };
    std::vector<Item> items;
    int top = std::numeric_limits<int>::max(), left = std::numeric_limits<int>::max(), bottom = 0, right = 0;
    while (!in.atEnd()) {
        qint32 r = 0, c = 0, n = 0;
        in >> r >> c >> n;
        if (in.status() != QDataStream::Ok || n < 0)
            return false;
        Item item{r, c, {}};
        for (qint32 k = 0; k < n; ++k) {
            qint32 role = 0;
            QVariant v;
            in >> role >> v;
            item.data.insert(role, v);
        }
        top = std::min<int>(top, r);
        left = std::min<int>(left, c);
        bottom = std::max<int>(bottom, r);
        right = std::max<int>(right, c);
        items.push_back(std::move(item));
    }
    if (items.empty())
        return false;
    if (row < 0)
        row = rowCount(parent);
    if (column < 0)
        column = 0;
    const int rows = bottom - top + 1;
    if (!insertRows(row, rows, parent))
        return false;
    const int cols = std::max(1, columnCount(parent));
    for (const Item &item : items) {
        const int c = column + item.column - left;
        if (c >= cols)
            continue;
        setItemData(index(row + item.row - top, c, parent), item.data);
    }
    Q_UNUSED(right)
    return true;
}

/*!
    Returns the drop actions the model supports; the default is
    Qt::CopyAction.
*/
Qt::DropActions QAbstractItemModel::supportedDropActions() const { return Qt::CopyAction; }

/*!
    Returns the drag actions the model supports; the default is
    supportedDropActions().
*/
Qt::DropActions QAbstractItemModel::supportedDragActions() const { return supportedDropActions(); }

/*!
    Inserts \a count rows before \a row under \a parent and returns \c true
    on success. The default returns \c false; resizable models implement it
    with beginInsertRows() and endInsertRows().
*/
bool QAbstractItemModel::insertRows(int row, int count, const QModelIndex &parent) {
    Q_UNUSED(row)
    Q_UNUSED(count)
    Q_UNUSED(parent)
    return false;
}

/*!
    Inserts \a count columns before \a column under \a parent and returns
    \c true on success. The default returns \c false.
*/
bool QAbstractItemModel::insertColumns(int column, int count, const QModelIndex &parent) {
    Q_UNUSED(column)
    Q_UNUSED(count)
    Q_UNUSED(parent)
    return false;
}

/*!
    Removes \a count rows from \a row under \a parent and returns \c true on
    success. The default returns \c false.
*/
bool QAbstractItemModel::removeRows(int row, int count, const QModelIndex &parent) {
    Q_UNUSED(row)
    Q_UNUSED(count)
    Q_UNUSED(parent)
    return false;
}

/*!
    Removes \a count columns from \a column under \a parent and returns
    \c true on success. The default returns \c false.
*/
bool QAbstractItemModel::removeColumns(int column, int count, const QModelIndex &parent) {
    Q_UNUSED(column)
    Q_UNUSED(count)
    Q_UNUSED(parent)
    return false;
}

/*!
    \since 5.11
    Moves \a count rows from \a sourceRow under \a sourceParent to before
    \a destinationChild under \a destinationParent and returns \c true on
    success. The default returns \c false.
*/
bool QAbstractItemModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent,
                                  int destinationChild) {
    Q_UNUSED(sourceParent)
    Q_UNUSED(sourceRow)
    Q_UNUSED(count)
    Q_UNUSED(destinationParent)
    Q_UNUSED(destinationChild)
    return false;
}

/*!
    \since 5.11
    Moves \a count columns from \a sourceColumn under \a sourceParent to
    before \a destinationChild under \a destinationParent and returns
    \c true on success. The default returns \c false.
*/
bool QAbstractItemModel::moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count, const QModelIndex &destinationParent,
                                     int destinationChild) {
    Q_UNUSED(sourceParent)
    Q_UNUSED(sourceColumn)
    Q_UNUSED(count)
    Q_UNUSED(destinationParent)
    Q_UNUSED(destinationChild)
    return false;
}

/*!
    Inserts one row before \a row under \a parent.
*/
bool QAbstractItemModel::insertRow(int row, const QModelIndex &parent) { return insertRows(row, 1, parent); }

/*!
    Inserts one column before \a column under \a parent.
*/
bool QAbstractItemModel::insertColumn(int column, const QModelIndex &parent) { return insertColumns(column, 1, parent); }

/*!
    Removes the row \a row under \a parent.
*/
bool QAbstractItemModel::removeRow(int row, const QModelIndex &parent) { return removeRows(row, 1, parent); }

/*!
    Removes the column \a column under \a parent.
*/
bool QAbstractItemModel::removeColumn(int column, const QModelIndex &parent) { return removeColumns(column, 1, parent); }

/*!
    \since 5.11
    Moves the row \a sourceRow under \a sourceParent to before
    \a destinationChild under \a destinationParent.
*/
bool QAbstractItemModel::moveRow(const QModelIndex &sourceParent, int sourceRow, const QModelIndex &destinationParent, int destinationChild) {
    return moveRows(sourceParent, sourceRow, 1, destinationParent, destinationChild);
}

/*!
    \since 5.11
    Moves the column \a sourceColumn under \a sourceParent to before
    \a destinationChild under \a destinationParent.
*/
bool QAbstractItemModel::moveColumn(const QModelIndex &sourceParent, int sourceColumn, const QModelIndex &destinationParent,
                                    int destinationChild) {
    return moveColumns(sourceParent, sourceColumn, 1, destinationParent, destinationChild);
}

/*!
    Fetches more items under \a parent, for models that load data
    incrementally. The default does nothing.
*/
void QAbstractItemModel::fetchMore(const QModelIndex &parent) { Q_UNUSED(parent) }

/*!
    Returns \c true if more items can be fetched under \a parent; the default
    returns \c false.
*/
bool QAbstractItemModel::canFetchMore(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return false;
}

/*!
    Returns the flags of the item at \a index: Qt::ItemIsSelectable and
    Qt::ItemIsEnabled for a valid index, and none for an invalid one.
*/
Qt::ItemFlags QAbstractItemModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

/*!
    Sorts the model by \a column in \a order. The default does nothing.
*/
void QAbstractItemModel::sort(int column, Qt::SortOrder order) {
    Q_UNUSED(column)
    Q_UNUSED(order)
}

/*!
    Returns the item that is edited in place of the item at \a index; the
    default returns \a index.
*/
QModelIndex QAbstractItemModel::buddy(const QModelIndex &index) const { return index; }

/*!
    Returns up to \a hits indexes of items in the column of \a start, under
    its parent and starting at its row, whose data for \a role matches
    \a value according to \a flags. A \a hits of -1 returns all matches.
    With Qt::MatchWrap the search wraps to the first row, and with
    Qt::MatchRecursive child items are searched too.
*/
QModelIndexList QAbstractItemModel::match(const QModelIndex &start, int role, const QVariant &value, int hits, Qt::MatchFlags flags) const {
    QModelIndexList result;
    const uint type = static_cast<uint>(flags.toInt()) & Qt::MatchTypeMask;
    const Qt::CaseSensitivity cs = flags.testFlag(Qt::MatchCaseSensitive) ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QString text = value.toString();
    QRegularExpression re;
    if (type == Qt::MatchRegularExpression) {
        re = value.typeId() == QMetaType::QRegularExpression ? value.value<QRegularExpression>() : QRegularExpression(text);
        if (cs == Qt::CaseInsensitive)
            re.setPatternOptions(re.patternOptions() | QRegularExpression::CaseInsensitiveOption);
    } else if (type == Qt::MatchWildcard) {
        re = QRegularExpression::fromWildcard(text, cs, QRegularExpression::NonPathWildcardConversion);
    }
    auto matches = [&](const QVariant &v) {
        if (type == Qt::MatchExactly)
            return v == value;
        const QString s = v.toString();
        switch (type) {
        case Qt::MatchRegularExpression:
        case Qt::MatchWildcard:
            return re.match(s).hasMatch();
        case Qt::MatchStartsWith:
            return s.startsWith(text, cs);
        case Qt::MatchEndsWith:
            return s.endsWith(text, cs);
        case Qt::MatchFixedString:
            return s.compare(text, cs) == 0;
        default:
            return s.contains(text, cs);
        }
    };
    const QModelIndex parentIndex = start.parent();
    const int column = start.column();
    const int rows = rowCount(parentIndex);
    std::function<void(const QModelIndex &, int, int)> search = [&](const QModelIndex &parent, int from, int to) {
        for (int r = from; r < to && (hits == -1 || result.size() < hits); ++r) {
            const QModelIndex idx = index(r, column, parent);
            if (!idx.isValid())
                continue;
            if (matches(data(idx, role)))
                result.append(idx);
            if (flags.testFlag(Qt::MatchRecursive)) {
                const QModelIndex first = index(r, 0, parent);
                if (hasChildren(first))
                    search(first, 0, rowCount(first));
            }
        }
    };
    search(parentIndex, std::max(0, start.row()), rows);
    if (flags.testFlag(Qt::MatchWrap) && start.row() > 0)
        search(parentIndex, 0, start.row());
    return result;
}

/*!
    Returns the number of rows and columns the item at \a index spans; the
    default is one of each.
*/
QSize QAbstractItemModel::span(const QModelIndex &index) const {
    Q_UNUSED(index)
    return QSize(1, 1);
}

/*!
    \since 4.6
    Returns the names of the roles, as used by declarative frontends. The
    defaults are "display", "decoration", "edit", "toolTip", "statusTip" and
    "whatsThis".
*/
QHash<int, QByteArray> QAbstractItemModel::roleNames() const {
    QHash<int, QByteArray> r;
    r.insert(Qt::DisplayRole, "display");
    r.insert(Qt::DecorationRole, "decoration");
    r.insert(Qt::EditRole, "edit");
    r.insert(Qt::ToolTipRole, "toolTip");
    r.insert(Qt::StatusTipRole, "statusTip");
    r.insert(Qt::WhatsThisRole, "whatsThis");
    return r;
}

/*!
    \since 5.11
    Returns \c true if \a index is a valid index of this model, checked
    according to \a options; otherwise prints a warning and returns
    \c false. An invalid index passes unless IndexIsValid is set.
*/
bool QAbstractItemModel::checkIndex(const QModelIndex &index, CheckIndexOptions options) const {
    if (!index.isValid()) {
        if (options.testFlag(CheckIndexOption::IndexIsValid)) {
            qWarning("QAbstractItemModel::checkIndex: Index is not valid (expected valid)");
            return false;
        }
        return true;
    }
    if (index.model() != this) {
        qWarning("QAbstractItemModel::checkIndex: Index is for a different model");
        return false;
    }
    if (index.row() < 0 || index.column() < 0) {
        qWarning("QAbstractItemModel::checkIndex: Index has negative coordinates");
        return false;
    }
    if (options.testFlag(CheckIndexOption::DoNotUseParent))
        return true;
    const QModelIndex p = parent(index);
    if (options.testFlag(CheckIndexOption::ParentIsInvalid) && p.isValid()) {
        qWarning("QAbstractItemModel::checkIndex: Index has a valid parent (expected an invalid parent)");
        return false;
    }
    if (index.row() >= rowCount(p) || index.column() >= columnCount(p)) {
        qWarning("QAbstractItemModel::checkIndex: Index is out of range");
        return false;
    }
    return true;
}

/*!
    Lets the model write cached changes to its storage and returns \c true on
    success. The default returns \c true.
*/
bool QAbstractItemModel::submit() { return true; }

/*!
    Lets the model discard cached changes. The default does nothing.
*/
void QAbstractItemModel::revert() {}

/*!
    \since 5.0
    Called when the model is reset, after modelAboutToBeReset() and before
    modelReset(), so that subclasses can clear their internal data. The
    default does nothing.
*/
void QAbstractItemModel::resetInternalData() {}

/*!
    Begins inserting rows \a first to \a last under \a parent: emits
    rowsAboutToBeInserted(). Call endInsertRows() after inserting the rows.
*/
void QAbstractItemModel::beginInsertRows(const QModelIndex &parent, int first, int last) {
    QAbstractItemModelPrivate::Change ch{QAbstractItemModelPrivate::Change::InsertRows, parent, first, last, {}, 0, {}};
    for (QPersistentModelIndexData *p : d_ptr->persistent)
        if (p->index.parent() == parent && p->index.row() >= first)
            ch.updates.emplace_back(p, p->index.row() + last - first + 1);
    d_ptr->changes.push_back(std::move(ch));
    rowsAboutToBeInserted(parent, first, last);
}

/*!
    Ends inserting rows: updates persistent indexes and emits rowsInserted().
*/
void QAbstractItemModel::endInsertRows() {
    if (d_ptr->changes.empty())
        return;
    const auto ch = std::move(d_ptr->changes.back());
    d_ptr->changes.pop_back();
    for (const auto &u : ch.updates)
        u.first->index = createIndex(u.second, u.first->index.column(), u.first->index.internalId());
    rowsInserted(ch.parent, ch.first, ch.last);
}

/*!
    Begins removing rows \a first to \a last under \a parent: emits
    rowsAboutToBeRemoved(). Call endRemoveRows() after removing the rows.
*/
void QAbstractItemModel::beginRemoveRows(const QModelIndex &parent, int first, int last) {
    rowsAboutToBeRemoved(parent, first, last);
    QAbstractItemModelPrivate::Change ch{QAbstractItemModelPrivate::Change::RemoveRows, parent, first, last, {}, 0, {}};
    for (QPersistentModelIndexData *p : d_ptr->persistent) {
        if (p->index.parent() == parent) {
            const int row = p->index.row();
            if (row >= first && row <= last)
                ch.updates.emplace_back(p, -1);
            else if (row > last)
                ch.updates.emplace_back(p, row - (last - first + 1));
        } else if (QAbstractItemModelPrivate::isDescendantOf(p->index, parent, first, last, true)) {
            ch.updates.emplace_back(p, -1);
        }
    }
    d_ptr->changes.push_back(std::move(ch));
}

/*!
    Ends removing rows: invalidates the persistent indexes of removed items,
    updates the others and emits rowsRemoved().
*/
void QAbstractItemModel::endRemoveRows() {
    if (d_ptr->changes.empty())
        return;
    const auto ch = std::move(d_ptr->changes.back());
    d_ptr->changes.pop_back();
    for (const auto &u : ch.updates) {
        if (u.second < 0)
            invalidate(u.first);
        else
            u.first->index = createIndex(u.second, u.first->index.column(), u.first->index.internalId());
    }
    rowsRemoved(ch.parent, ch.first, ch.last);
}

namespace {
// Returns true if parent is one of the rows (or columns) first..last under sourceParent, or below
// one of them.
bool isWithin(const QModelIndex &parent, const QModelIndex &sourceParent, int first, int last, bool rows) {
    for (QModelIndex p = parent; p.isValid(); p = p.parent()) {
        if (p.parent() == sourceParent) {
            const int pos = rows ? p.row() : p.column();
            if (pos >= first && pos <= last)
                return true;
        }
    }
    return false;
}
} // namespace

/*!
    Begins moving rows \a sourceFirst to \a sourceLast under \a sourceParent
    to before row \a destinationRow under \a destinationParent, and emits
    rowsAboutToBeMoved(). Returns \c false, doing nothing, if the move is not
    possible (the destination is inside the moved rows, or the move would
    not change anything); otherwise call endMoveRows() after moving the rows.
*/
bool QAbstractItemModel::beginMoveRows(const QModelIndex &sourceParent, int sourceFirst, int sourceLast, const QModelIndex &destinationParent,
                                       int destinationRow) {
    if (sourceFirst < 0 || sourceLast < sourceFirst)
        return false;
    if (sourceParent == destinationParent && destinationRow >= sourceFirst && destinationRow <= sourceLast + 1)
        return false;
    if (isWithin(destinationParent, sourceParent, sourceFirst, sourceLast, true))
        return false;
    const int count = sourceLast - sourceFirst + 1;
    QAbstractItemModelPrivate::Change ch{QAbstractItemModelPrivate::Change::MoveRows, sourceParent, sourceFirst, sourceLast,
                                         destinationParent, destinationRow, {}};
    const bool sameParent = sourceParent == destinationParent;
    for (QPersistentModelIndexData *p : d_ptr->persistent) {
        const QModelIndex parent = p->index.parent();
        const int row = p->index.row();
        if (parent == sourceParent && row >= sourceFirst && row <= sourceLast) {
            int target = destinationRow + (row - sourceFirst);
            if (sameParent && destinationRow > sourceLast)
                target -= count;
            ch.updates.emplace_back(p, target);
        } else if (sameParent && parent == sourceParent) {
            if (destinationRow > sourceLast && row > sourceLast && row < destinationRow)
                ch.updates.emplace_back(p, row - count);
            else if (destinationRow < sourceFirst && row >= destinationRow && row < sourceFirst)
                ch.updates.emplace_back(p, row + count);
        } else if (parent == sourceParent && row > sourceLast) {
            ch.updates.emplace_back(p, row - count);
        } else if (parent == destinationParent && row >= destinationRow) {
            ch.updates.emplace_back(p, row + count);
        }
    }
    d_ptr->changes.push_back(std::move(ch));
    rowsAboutToBeMoved(sourceParent, sourceFirst, sourceLast, destinationParent, destinationRow);
    return true;
}

/*!
    Ends moving rows: updates persistent indexes and emits rowsMoved().
*/
void QAbstractItemModel::endMoveRows() {
    if (d_ptr->changes.empty())
        return;
    const auto ch = std::move(d_ptr->changes.back());
    d_ptr->changes.pop_back();
    for (const auto &u : ch.updates)
        u.first->index = createIndex(u.second, u.first->index.column(), u.first->index.internalId());
    rowsMoved(ch.parent, ch.first, ch.last, ch.destination, ch.destinationChild);
}

/*!
    Begins inserting columns \a first to \a last under \a parent: emits
    columnsAboutToBeInserted(). Call endInsertColumns() afterwards.
*/
void QAbstractItemModel::beginInsertColumns(const QModelIndex &parent, int first, int last) {
    QAbstractItemModelPrivate::Change ch{QAbstractItemModelPrivate::Change::InsertColumns, parent, first, last, {}, 0, {}};
    for (QPersistentModelIndexData *p : d_ptr->persistent)
        if (p->index.parent() == parent && p->index.column() >= first)
            ch.updates.emplace_back(p, p->index.column() + last - first + 1);
    d_ptr->changes.push_back(std::move(ch));
    columnsAboutToBeInserted(parent, first, last);
}

/*!
    Ends inserting columns: updates persistent indexes and emits
    columnsInserted().
*/
void QAbstractItemModel::endInsertColumns() {
    if (d_ptr->changes.empty())
        return;
    const auto ch = std::move(d_ptr->changes.back());
    d_ptr->changes.pop_back();
    for (const auto &u : ch.updates)
        u.first->index = createIndex(u.first->index.row(), u.second, u.first->index.internalId());
    columnsInserted(ch.parent, ch.first, ch.last);
}

/*!
    Begins removing columns \a first to \a last under \a parent: emits
    columnsAboutToBeRemoved(). Call endRemoveColumns() afterwards.
*/
void QAbstractItemModel::beginRemoveColumns(const QModelIndex &parent, int first, int last) {
    columnsAboutToBeRemoved(parent, first, last);
    QAbstractItemModelPrivate::Change ch{QAbstractItemModelPrivate::Change::RemoveColumns, parent, first, last, {}, 0, {}};
    for (QPersistentModelIndexData *p : d_ptr->persistent) {
        if (p->index.parent() == parent) {
            const int column = p->index.column();
            if (column >= first && column <= last)
                ch.updates.emplace_back(p, -1);
            else if (column > last)
                ch.updates.emplace_back(p, column - (last - first + 1));
        } else if (QAbstractItemModelPrivate::isDescendantOf(p->index, parent, first, last, false)) {
            ch.updates.emplace_back(p, -1);
        }
    }
    d_ptr->changes.push_back(std::move(ch));
}

/*!
    Ends removing columns: invalidates or updates persistent indexes and
    emits columnsRemoved().
*/
void QAbstractItemModel::endRemoveColumns() {
    if (d_ptr->changes.empty())
        return;
    const auto ch = std::move(d_ptr->changes.back());
    d_ptr->changes.pop_back();
    for (const auto &u : ch.updates) {
        if (u.second < 0)
            invalidate(u.first);
        else
            u.first->index = createIndex(u.first->index.row(), u.second, u.first->index.internalId());
    }
    columnsRemoved(ch.parent, ch.first, ch.last);
}

/*!
    Begins moving columns \a sourceFirst to \a sourceLast under
    \a sourceParent to before column \a destinationColumn under
    \a destinationParent, and emits columnsAboutToBeMoved(). Returns
    \c false, doing nothing, if the move is not possible; otherwise call
    endMoveColumns() afterwards.
*/
bool QAbstractItemModel::beginMoveColumns(const QModelIndex &sourceParent, int sourceFirst, int sourceLast, const QModelIndex &destinationParent,
                                          int destinationColumn) {
    if (sourceFirst < 0 || sourceLast < sourceFirst)
        return false;
    if (sourceParent == destinationParent && destinationColumn >= sourceFirst && destinationColumn <= sourceLast + 1)
        return false;
    if (isWithin(destinationParent, sourceParent, sourceFirst, sourceLast, false))
        return false;
    const int count = sourceLast - sourceFirst + 1;
    QAbstractItemModelPrivate::Change ch{QAbstractItemModelPrivate::Change::MoveColumns, sourceParent, sourceFirst, sourceLast,
                                         destinationParent, destinationColumn, {}};
    const bool sameParent = sourceParent == destinationParent;
    for (QPersistentModelIndexData *p : d_ptr->persistent) {
        const QModelIndex parent = p->index.parent();
        const int column = p->index.column();
        if (parent == sourceParent && column >= sourceFirst && column <= sourceLast) {
            int target = destinationColumn + (column - sourceFirst);
            if (sameParent && destinationColumn > sourceLast)
                target -= count;
            ch.updates.emplace_back(p, target);
        } else if (sameParent && parent == sourceParent) {
            if (destinationColumn > sourceLast && column > sourceLast && column < destinationColumn)
                ch.updates.emplace_back(p, column - count);
            else if (destinationColumn < sourceFirst && column >= destinationColumn && column < sourceFirst)
                ch.updates.emplace_back(p, column + count);
        } else if (parent == sourceParent && column > sourceLast) {
            ch.updates.emplace_back(p, column - count);
        } else if (parent == destinationParent && column >= destinationColumn) {
            ch.updates.emplace_back(p, column + count);
        }
    }
    d_ptr->changes.push_back(std::move(ch));
    columnsAboutToBeMoved(sourceParent, sourceFirst, sourceLast, destinationParent, destinationColumn);
    return true;
}

/*!
    Ends moving columns: updates persistent indexes and emits
    columnsMoved().
*/
void QAbstractItemModel::endMoveColumns() {
    if (d_ptr->changes.empty())
        return;
    const auto ch = std::move(d_ptr->changes.back());
    d_ptr->changes.pop_back();
    for (const auto &u : ch.updates)
        u.first->index = createIndex(u.first->index.row(), u.second, u.first->index.internalId());
    columnsMoved(ch.parent, ch.first, ch.last, ch.destination, ch.destinationChild);
}

/*!
    Begins resetting the model: emits modelAboutToBeReset(). Call
    endResetModel() after changing the data.
*/
void QAbstractItemModel::beginResetModel() {
    d_ptr->resetting = true;
    modelAboutToBeReset();
}

/*!
    Ends resetting the model: invalidates all persistent indexes, calls
    resetInternalData() and emits modelReset().
*/
void QAbstractItemModel::endResetModel() {
    for (QPersistentModelIndexData *p : std::vector<QPersistentModelIndexData *>(d_ptr->persistent))
        invalidate(p);
    d_ptr->resetting = false;
    resetInternalData();
    modelReset();
}

/*!
    Changes the persistent indexes that refer to \a from so that they refer
    to \a to. An invalid \a to invalidates them. Models call this between
    layoutAboutToBeChanged() and layoutChanged().
*/
void QAbstractItemModel::changePersistentIndex(const QModelIndex &from, const QModelIndex &to) {
    for (QPersistentModelIndexData *p : std::vector<QPersistentModelIndexData *>(d_ptr->persistent)) {
        if (p->index != from)
            continue;
        if (to.isValid())
            p->index = to;
        else
            invalidate(p);
    }
}

/*!
    \since 4.1
    Changes the persistent indexes that refer to the indexes in \a from to
    the corresponding indexes in \a to.
*/
void QAbstractItemModel::changePersistentIndexList(const QModelIndexList &from, const QModelIndexList &to) {
    // Look up every persistent index before changing any, so that chains such as a->b, b->c work.
    std::vector<std::pair<QPersistentModelIndexData *, QModelIndex>> changes;
    for (QPersistentModelIndexData *p : d_ptr->persistent) {
        const qsizetype k = from.indexOf(p->index);
        if (k >= 0 && k < to.size())
            changes.emplace_back(p, to.at(k));
    }
    for (const auto &c : changes) {
        if (c.second.isValid())
            c.first->index = c.second;
        else
            invalidate(c.first);
    }
}

/*!
    \since 4.2
    Returns the indexes currently held by persistent indexes of the model.
*/
QModelIndexList QAbstractItemModel::persistentIndexList() const {
    QModelIndexList r;
    for (const QPersistentModelIndexData *p : d_ptr->persistent)
        if (!r.contains(p->index))
            r.append(p->index);
    return r;
}

// ------------------------------------------------------------------ QAbstractTableModel

/*!
    Constructs a table model with the given \a parent.
*/
QAbstractTableModel::QAbstractTableModel(QObject *parent) : QAbstractItemModel(parent) {}

/*!
    Destroys the table model.
*/
QAbstractTableModel::~QAbstractTableModel() = default;

/*!
    \reimp
    Returns the index at \a row and \a column, or an invalid index if it is
    out of range or \a parent is valid.
*/
QModelIndex QAbstractTableModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column) : QModelIndex();
}

/*!
    \reimp
    Returns the index at \a row and \a column next to \a idx.
*/
QModelIndex QAbstractTableModel::sibling(int row, int column, const QModelIndex &idx) const { return index(row, column, idx.parent()); }

/*!
    \reimp
    Drops \a data with \a action at \a row and \a column under \a parent.
*/
bool QAbstractTableModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    return QAbstractItemModel::dropMimeData(data, action, row, column, parent);
}

/*!
    \reimp
    Returns the flags of the item at \a index, with Qt::ItemNeverHasChildren.
*/
Qt::ItemFlags QAbstractTableModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemNeverHasChildren;
    return f;
}

QModelIndex QAbstractTableModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)
    return QModelIndex();
}

bool QAbstractTableModel::hasChildren(const QModelIndex &parent) const {
    if (parent.isValid())
        return false;
    return rowCount(parent) > 0 && columnCount(parent) > 0;
}

// ------------------------------------------------------------------ QAbstractListModel

/*!
    Constructs a list model with the given \a parent.
*/
QAbstractListModel::QAbstractListModel(QObject *parent) : QAbstractItemModel(parent) {}

/*!
    Destroys the list model.
*/
QAbstractListModel::~QAbstractListModel() = default;

/*!
    \reimp
    Returns the index of \a row in \a column 0, or an invalid index if it is
    out of range or \a parent is valid.
*/
QModelIndex QAbstractListModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column) : QModelIndex();
}

/*!
    \reimp
    Returns the index at \a row and \a column next to \a idx.
*/
QModelIndex QAbstractListModel::sibling(int row, int column, const QModelIndex &idx) const {
    Q_UNUSED(idx)
    return index(row, column);
}

/*!
    \reimp
    Drops \a data with \a action at \a row and \a column under \a parent.
*/
bool QAbstractListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    return QAbstractItemModel::dropMimeData(data, action, row, column, parent);
}

/*!
    \reimp
    Returns the flags of the item at \a index, with Qt::ItemNeverHasChildren.
*/
Qt::ItemFlags QAbstractListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemNeverHasChildren;
    return f;
}

int QAbstractListModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : 1; }

QModelIndex QAbstractListModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)
    return QModelIndex();
}

bool QAbstractListModel::hasChildren(const QModelIndex &parent) const { return parent.isValid() ? false : rowCount(parent) > 0; }

#include "moc_qabstractitemmodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
