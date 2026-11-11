// MIT License - see LICENSE
#include <QtSql/qsqlquerymodel.h>

#include <QtSql/qsqldriver.h>

#include <utility>

/*!
    \class QSqlQueryModel
    \brief The QSqlQueryModel class provides a read-only data model for SQL result sets.

    A QSqlQueryModel shows the rows of a \c SELECT as a table model, for a
    view or for code that reads items by row and column:

    \code
    QSqlQueryModel model;
    model.setQuery("SELECT name, salary FROM employee");
    model.setHeaderData(0, Qt::Horizontal, "Name");
    for (int row = 0; row < model.rowCount(); ++row)
        qDebug() << model.data(model.index(row, 0)).toString();
    \endcode

    The columns are the fields of the query, and the horizontal header has
    their names until setHeaderData() gives others. The items cannot be
    changed; subclass the model, or use QSqlTableModel, for that.

    With a driver that does not know the number of rows of a query, such as
    \c QSQLITE, the model reads the rows in steps: rowCount() counts the
    rows read so far and grows with fetchMore(), which views call as they
    scroll. To have all the rows at once:

    \code
    while (model.canFetchMore())
        model.fetchMore();
    \endcode

    \differences
    \c setQuery() takes the query by moving it, or as text; the overload
    that copies a QSqlQuery, deprecated in Qt 6, is missing. The first step
    of rows is read inside the model reset of setQuery(), so it is not
    announced with \c rowsInserted().
    \enddifferences

    \sa QSqlTableModel, QSqlQuery, {Using Nt SQL}
*/

namespace {
constexpr int FetchStep = 255; // the number of rows that fetchMore() adds
}

/*!
    Constructs an empty model with the given \a parent.
*/
QSqlQueryModel::QSqlQueryModel(QObject *parent)
    : QAbstractTableModel(parent), m_query(static_cast<QSqlResult *>(nullptr)) {}

/*!
    Destroys the model and its query.
*/
QSqlQueryModel::~QSqlQueryModel() = default;

/*!
    Returns the number of rows that have been read from the query; see
    fetchMore(). Returns 0 for a valid \a parent, as a table has no child
    items.

    \sa canFetchMore(), columnCount()
*/
int QSqlQueryModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_bottom + 1; }

/*!
    Returns the number of columns: the fields of the query and the columns
    added with insertColumns(). Returns 0 for a valid \a parent.

    \sa rowCount()
*/
int QSqlQueryModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rec.count(); }

/*!
    Returns the fields of the model with the values of the row \a row. The
    values are null if there is no such row.

    \sa data()
*/
QSqlRecord QSqlQueryModel::record(int row) const {
    QSqlRecord rec = m_rec;
    if (row < 0)
        return rec;
    for (int i = 0; i < rec.count(); ++i)
        rec.setValue(i, data(createIndex(row, i), Qt::EditRole));
    return rec;
}

/*!
    \overload
    Returns the fields of the model, without values: an empty record if no
    query is set.
*/
QSqlRecord QSqlQueryModel::record() const { return m_rec; }

/*!
    Returns the value of the field at \a item for the display role or the
    edit role, and an invalid QVariant for another \a role, an invalid
    index or a column that is not in the query. A row that has not been
    read yet is read first.

    \sa record(), lastError()
*/
QVariant QSqlQueryModel::data(const QModelIndex &item, int role) const {
    if (!item.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole))
        return QVariant();
    const QModelIndex inQuery = indexInQuery(item);
    if (!inQuery.isValid())
        return QVariant();
    auto *self = const_cast<QSqlQueryModel *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): reads rows lazily
    if (inQuery.row() > m_bottom)
        self->prefetch(inQuery.row());
    if (!m_query.seek(inQuery.row())) {
        self->m_error = m_query.lastError();
        return QVariant();
    }
    return m_query.value(inQuery.column());
}

/*!
    Returns the header data of the column \a section for \a role if
    \a orientation is horizontal: what setHeaderData() set, and otherwise,
    for the display role, the name of the field. A vertical header has the
    row numbers of the base class.

    \sa setHeaderData()
*/
QVariant QSqlQueryModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_headers.size()) {
            QVariant value = m_headers.at(section).value(role);
            if (role == Qt::DisplayRole && !value.isValid())
                value = m_headers.at(section).value(Qt::EditRole);
            if (value.isValid())
                return value;
        }
        if (role == Qt::DisplayRole && section >= 0 && section < m_rec.count())
            return m_rec.fieldName(section);
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

/*!
    Sets the header data of the column \a section for \a role to \a value
    and returns \c true, if \a orientation is horizontal and there is such
    a column; otherwise returns \c false. The data is forgotten when a new
    query is set.

    \sa headerData()
*/
bool QSqlQueryModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) {
    if (orientation != Qt::Horizontal || section < 0 || section >= columnCount())
        return false;
    while (m_headers.size() <= section)
        m_headers.append(QHash<int, QVariant>());
    m_headers[section].insert(role, value);
    headerDataChanged(orientation, section, section);
    return true;
}

/*!
    Inserts \a count columns before the column \a column and returns
    \c true on success. \a parent has to be invalid. The new columns are
    not part of the query: their items are empty until a subclass returns
    something for them from data().

    \sa removeColumns()
*/
bool QSqlQueryModel::insertColumns(int column, int count, const QModelIndex &parent) {
    if (count <= 0 || parent.isValid() || column < 0 || column > m_rec.count())
        return false;
    beginInsertColumns(parent, column, column + count - 1);
    for (int i = 0; i < count; ++i) {
        m_rec.insert(column, QSqlField());
        m_columnInQuery.insert(column, -1);
        if (column < m_headers.size())
            m_headers.insert(column, QHash<int, QVariant>());
    }
    endInsertColumns();
    return true;
}

/*!
    Removes \a count columns starting with the column \a column and
    returns \c true on success. \a parent has to be invalid. The query is
    not changed: the model only stops showing the columns.

    \sa insertColumns()
*/
bool QSqlQueryModel::removeColumns(int column, int count, const QModelIndex &parent) {
    if (count <= 0 || parent.isValid() || column < 0 || column + count > m_rec.count())
        return false;
    beginRemoveColumns(parent, column, column + count - 1);
    for (int i = 0; i < count; ++i) {
        m_rec.remove(column);
        m_columnInQuery.removeAt(column);
        if (column < m_headers.size())
            m_headers.removeAt(column);
    }
    endRemoveColumns();
    return true;
}

void QSqlQueryModel::initColumns(const QSqlRecord &rec) {
    m_rec = rec;
    m_columnInQuery.clear();
    for (int i = 0; i < rec.count(); ++i)
        m_columnInQuery.append(i);
    m_headers.clear();
}

/*!
    Makes the model show the rows of \a query, which has to be active, and
    takes the query over. The model is reset. lastError() tells if the
    query is not active or is forward-only, which a model cannot use; the
    model is then empty.

    \sa query(), lastError(), queryChange()
*/
void QSqlQueryModel::setQuery(QSqlQuery &&query) {
    beginResetModel();
    m_query = std::move(query);
    initColumns(m_query.record());
    m_bottom = -1;
    m_atEnd = true;
    m_error = m_query.lastError();
    if (m_query.isActive() && m_query.isForwardOnly()) {
        m_error = QSqlError(QStringLiteral("Forward-only queries cannot be used in a data model"), QString(),
                            QSqlError::ConnectionError);
    } else if (m_query.isActive() && m_query.isSelect()) {
        const QSqlDriver *driver = m_query.driver();
        if (driver && driver->hasFeature(QSqlDriver::QuerySize) && m_query.size() >= 0) {
            m_bottom = m_query.size() - 1;
        } else {
            // The first step of rows comes with the reset, without a signal of its own.
            m_atEnd = false;
            if (m_query.seek(FetchStep - 1)) {
                m_bottom = FetchStep - 1;
            } else {
                m_bottom = m_query.last() ? m_query.at() : -1;
                m_atEnd = true;
            }
        }
    }
    queryChange();
    endResetModel();
}

/*!
    \overload
    Runs the statement \a query on the connection \a db, or on the default
    connection, and shows its rows. lastError() tells if it failed.
*/
void QSqlQueryModel::setQuery(const QString &query, const QSqlDatabase &db) { setQuery(QSqlQuery(query, db)); }

/*!
    Returns the query of the model. Its position changes as the model
    reads from it.

    \sa setQuery()
*/
const QSqlQuery &QSqlQueryModel::query() const { return m_query; }

/*!
    Empties the model: forgets the query, its rows, the header data and
    the last error.
*/
void QSqlQueryModel::clear() {
    beginResetModel();
    m_query = QSqlQuery(static_cast<QSqlResult *>(nullptr));
    m_error = QSqlError();
    m_bottom = -1;
    m_atEnd = true;
    initColumns(QSqlRecord());
    endResetModel();
}

/*!
    Returns the last error of the model: what went wrong with the query.

    \sa setLastError(), QSqlQuery::lastError()
*/
QSqlError QSqlQueryModel::lastError() const { return m_error; }

// Reads the rows up to the row at position row, and announces them.
void QSqlQueryModel::prefetch(int row) {
    if (m_atEnd || row <= m_bottom)
        return;
    int newBottom = row;
    if (!m_query.seek(row)) {
        newBottom = m_query.last() ? m_query.at() : -1;
        m_atEnd = true;
    }
    if (newBottom > m_bottom) {
        beginInsertRows(QModelIndex(), m_bottom + 1, newBottom);
        m_bottom = newBottom;
        endInsertRows();
    }
}

/*!
    Reads up to 255 more rows from the query and adds them to the model.
    Nothing happens for a valid \a parent, or if all rows have been read.

    \sa canFetchMore()
*/
void QSqlQueryModel::fetchMore(const QModelIndex &parent) {
    if (parent.isValid())
        return;
    prefetch(m_bottom + FetchStep);
}

/*!
    Returns \c true if the query may have rows that the model has not read
    yet; otherwise returns \c false. It is always \c false for a valid
    \a parent, and for a driver that knows the number of rows, as all of
    them are in the model from the start.

    \sa fetchMore()
*/
bool QSqlQueryModel::canFetchMore(const QModelIndex &parent) const { return !parent.isValid() && !m_atEnd; }

/*!
    Returns the names of the roles of the model: only \c display, for
    Qt::DisplayRole.
*/
QHash<int, QByteArray> QSqlQueryModel::roleNames() const {
    QHash<int, QByteArray> names;
    names.insert(Qt::DisplayRole, QByteArrayLiteral("display"));
    return names;
}

/*!
    Called when the query of the model has changed, inside the model
    reset. The base implementation does nothing; a subclass can adapt
    itself to the new fields here.

    \sa setQuery()
*/
void QSqlQueryModel::queryChange() {}

/*!
    Returns the index in the query of the item \a item of the model. The
    row is the same; the column differs if columns were inserted or removed.
    Returns an invalid index for a column that is not in the query.

    \sa insertColumns(), removeColumns()
*/
QModelIndex QSqlQueryModel::indexInQuery(const QModelIndex &item) const {
    if (!item.isValid() || item.column() < 0 || item.column() >= m_columnInQuery.size() ||
        m_columnInQuery.at(item.column()) < 0)
        return QModelIndex();
    return createIndex(item.row(), m_columnInQuery.at(item.column()), item.internalPointer());
}

/*!
    Sets the last error of the model to \a error. Called by subclasses.

    \sa lastError()
*/
void QSqlQueryModel::setLastError(const QSqlError &error) { m_error = error; }

#include "moc_qsqlquerymodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
