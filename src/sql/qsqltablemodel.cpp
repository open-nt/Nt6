// MIT License - see LICENSE
#include <QtSql/qsqltablemodel.h>

#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlfield.h>

#include <limits>
#include <utility>

/*!
    \class QSqlTableModel
    \brief The QSqlTableModel class provides an editable data model for a single database table.

    A QSqlTableModel reads the rows of one table and writes back the items
    that are changed, the rows that are inserted and the rows that are
    removed, without any SQL in the application:

    \code
    QSqlTableModel model;
    model.setTable("employee");
    model.setFilter("salary > 50000");
    model.setSort(model.fieldIndex("name"), Qt::AscendingOrder);
    model.setEditStrategy(QSqlTableModel::OnManualSubmit);
    model.select();

    model.setData(model.index(0, model.fieldIndex("salary")), 62000);
    if (!model.submitAll())
        qWarning() << model.lastError().text();
    \endcode

    setTable() picks the table and reads its fields; select() reads the
    rows, the ones that setFilter() lets through, in the order of setSort().
    The edit strategy says when changes go to the database. Until they are
    written they are kept in the model: isDirty() tells that there are
    some, and the vertical header shows \c{*} for a row that waits to be
    inserted and \c{!} for one that waits to be deleted. revertAll() drops
    them.

    A row is found again in the table by its primary key. For a table
    without one, all the fields of the row are compared, so two rows that
    are the same are changed together.

    The filter is a piece of SQL that is put into the statement as it is.
    Do not build it from text that comes from outside the program.

    With the strategies \c OnFieldChange and \c OnRowChange, a row that was
    written is read again by itself: the model is not reset, so a view keeps
    its selection and its place, and the row shows what the database made of
    it, such as a key that it assigned or a default value. The row stays
    where it is, even if the sort order or the filter would now put it
    elsewhere, until the next select(). submitAll() with \c OnManualSubmit,
    and deleting a row, read the whole table again.

    \differences
    Rows are read in steps, as in QSqlQueryModel. setRecord() submits the
    row at once only with \c OnFieldChange; insertRecord() also with
    \c OnRowChange. A row that was inserted is read again by its primary
    key, which for a key that the database assigns is taken from the row id
    of the insert; a table without a primary key gets it back by the values
    that were written.
    \enddifferences

    \sa QSqlQueryModel, QSqlDatabase, {Using Nt SQL}
*/

/*!
    Constructs an empty model with the given \a parent, on the connection
    \a db, or on the default connection if \a db is not valid.
*/
QSqlTableModel::QSqlTableModel(QObject *parent, const QSqlDatabase &db)
    : QSqlQueryModel(parent),
      m_db(db.isValid() ? db : QSqlDatabase::database(QLatin1StringView(QSqlDatabase::defaultConnection), false)) {}

/*!
    Destroys the model. Changes that were not submitted are lost.
*/
QSqlTableModel::~QSqlTableModel() = default;

/*!
    Makes the model work on the table \a tableName and reads the fields of
    the table, but not its rows: call select() for those. The filter, the
    sort order and pending changes are dropped. lastError() tells if the
    connection has no such table.

    \sa select(), tableName()
*/
void QSqlTableModel::setTable(const QString &tableName) {
    clear();
    m_table = tableName;
    m_baseRec = m_db.record(tableName);
    m_primaryIndex = m_db.primaryIndex(tableName);
    beginResetModel();
    initColumns(m_baseRec);
    endResetModel();
    if (m_baseRec.isEmpty()) {
        setLastError(
            QSqlError(QStringLiteral("Unable to find table ") + tableName, QString(), QSqlError::StatementError));
    }
}

/*!
    Returns the name of the table of the model.

    \sa setTable()
*/
QString QSqlTableModel::tableName() const { return m_table; }

/*!
    Returns the flags of the item at \a index: those of the base class,
    and Qt::ItemIsEditable unless the row waits to be deleted or the field
    is read-only.
*/
Qt::ItemFlags QSqlTableModel::flags(const QModelIndex &index) const {
    if (!index.isValid() || index.column() >= m_baseRec.count() || index.row() < 0)
        return QSqlQueryModel::flags(index);
    Qt::ItemFlags f = QSqlQueryModel::flags(index);
    const auto it = m_cache.constFind(index.row());
    const bool deleted = it != m_cache.constEnd() && it.value().op == Delete;
    if (!deleted && !m_baseRec.field(index.column()).isReadOnly())
        f |= Qt::ItemIsEditable;
    return f;
}

/*!
    Returns the fields of the table, without values: an empty record
    before setTable().
*/
QSqlRecord QSqlTableModel::record() const { return QSqlQueryModel::record(); }

/*!
    \overload
    Returns the fields of the model with the values of the row \a row,
    pending changes included. A field is marked as generated if its value
    was changed and not submitted yet. The values are null if there is no
    such row.
*/
QSqlRecord QSqlTableModel::record(int row) const {
    const auto it = m_cache.constFind(row);
    if (it != m_cache.constEnd())
        return it.value().rec;
    QSqlRecord rec = baseRecord(row);
    for (int i = 0; i < rec.count(); ++i)
        rec.setGenerated(i, false);
    return rec;
}

/*!
    Returns the value of the item at \a idx for the display role or the
    edit role: the pending value if the item was changed, and what was read
    from the table otherwise. Returns an invalid QVariant for another
    \a role.

    \sa setData()
*/
QVariant QSqlTableModel::data(const QModelIndex &idx, int role) const {
    if (!idx.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole))
        return QVariant();
    const auto it = m_cache.constFind(idx.row());
    if (it != m_cache.constEnd())
        return it.value().rec.value(idx.column());
    return QSqlQueryModel::data(idx, role);
}

/*!
    Sets the value of the item at \a index to \a value and returns \c true
    on success. \a role has to be Qt::EditRole. With the strategy
    \c OnFieldChange the change is written to the database at once, and the
    result tells whether that worked; with \c OnRowChange, a pending change
    of another row is written first; with \c OnManualSubmit nothing is
    written.

    \sa data(), submit(), submitAll(), isDirty()
*/
bool QSqlTableModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (role != Qt::EditRole || !index.isValid() || index.row() >= rowCount() || index.column() >= m_baseRec.count())
        return false;
    if (!(flags(index) & Qt::ItemIsEditable))
        return false;
    if (m_strategy != OnManualSubmit && dirtyOtherThan(index.row())) {
        // Another row has pending changes, which are written first.
        if (!submitPending())
            return false;
        if (index.row() >= rowCount())
            return false;
    }
    ModifiedRow &row = touch(index.row());
    if (row.op == None)
        row.op = Update;
    row.rec.setValue(index.column(), value);
    row.rec.setGenerated(index.column(), true);
    const QModelIndex changed = createIndex(index.row(), index.column());
    dataChanged(changed, changed, QList<int>());
    if (m_strategy == OnFieldChange)
        return submitPending();
    return true;
}

/*!
    Sets the value of the item at \a index to null and returns \c true on
    success, as setData() does with a null value.
*/
bool QSqlTableModel::clearItemData(const QModelIndex &index) { return setData(index, QVariant(), Qt::EditRole); }

/*!
    Returns the header data for \a section, \a orientation and \a role.
    The vertical header shows \c{*} for a row that waits to be inserted and
    \c{!} for a row that waits to be deleted; everything else comes from
    QSqlQueryModel.
*/
QVariant QSqlTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        const auto it = m_cache.constFind(section);
        if (it != m_cache.constEnd() && it.value().op == Insert)
            return QStringLiteral("*");
        if (it != m_cache.constEnd() && it.value().op == Delete)
            return QStringLiteral("!");
    }
    return QSqlQueryModel::headerData(section, orientation, role);
}

/*!
    Returns \c true if the model has changes that were not written to the
    database; otherwise returns \c false.

    \sa submitAll(), revertAll()
*/
bool QSqlTableModel::isDirty() const {
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        if (it.value().op != None)
            return true;
    }
    return false;
}

/*!
    \overload
    Returns \c true if the item at \a index has a value that was not
    written to the database, or is in a row that waits to be inserted or
    deleted; otherwise returns \c false.
*/
bool QSqlTableModel::isDirty(const QModelIndex &index) const {
    if (!index.isValid())
        return false;
    const auto it = m_cache.constFind(index.row());
    if (it == m_cache.constEnd() || it.value().op == None)
        return false;
    return it.value().op != Update || it.value().rec.isGenerated(index.column());
}

/*!
    Empties the model: forgets the table, the filter, the sort order, the
    rows and all pending changes. The edit strategy is kept.
*/
void QSqlTableModel::clear() {
    m_table.clear();
    m_filter.clear();
    m_baseRec.clear();
    m_primaryIndex = QSqlIndex();
    m_cache.clear();
    m_nonKeyColumns.clear();
    m_sortColumn = -1;
    m_sortOrder = Qt::AscendingOrder;
    QSqlQueryModel::clear();
}

/*!
    Sets when changes are written to the database to \a strategy. Pending
    changes are dropped first.

    \sa editStrategy(), revertAll()
*/
void QSqlTableModel::setEditStrategy(EditStrategy strategy) {
    revertAll();
    m_strategy = strategy;
}

/*!
    Returns when changes are written to the database.

    \sa setEditStrategy()
*/
QSqlTableModel::EditStrategy QSqlTableModel::editStrategy() const { return m_strategy; }

/*!
    Returns the primary key of the table, or an empty index if the table
    has none or no table is set.

    \sa setPrimaryKey()
*/
QSqlIndex QSqlTableModel::primaryKey() const { return m_primaryIndex; }

/*!
    Returns the connection of the model.
*/
QSqlDatabase QSqlTableModel::database() const { return m_db; }

/*!
    Returns the column of the field called \a fieldName, or -1 if the
    model has no such field.
*/
int QSqlTableModel::fieldIndex(const QString &fieldName) const { return QSqlQueryModel::record().indexOf(fieldName); }

/*!
    Sorts the rows by the column \a column in the order \a order, by
    reading the table again with select(). Pending changes are lost.

    \sa setSort()
*/
void QSqlTableModel::sort(int column, Qt::SortOrder order) {
    setSort(column, order);
    select();
}

/*!
    Sets the column and the order that the next select() sorts the rows
    by, to \a column and \a order. A negative \a column means no order.

    \sa sort(), orderByClause()
*/
void QSqlTableModel::setSort(int column, Qt::SortOrder order) {
    m_sortColumn = column;
    m_sortOrder = order;
}

/*!
    Returns the filter of the model.

    \sa setFilter()
*/
QString QSqlTableModel::filter() const { return m_filter; }

/*!
    Sets the filter to \a filter: the condition of an SQL \c WHERE clause,
    without the word \c WHERE, such as \c{"age > 30"}. Only the rows that
    meet it are read. If the rows have been read already, they are read
    again at once; otherwise the filter is used by the next select().

    The filter becomes part of the statement as it is written.

    \sa filter(), select()
*/
void QSqlTableModel::setFilter(const QString &filter) {
    m_filter = filter;
    if (query().isActive())
        select();
}

/*!
    Returns the number of rows of the model: the rows that were read from
    the table, and the rows that wait to be inserted. Returns 0 for a valid
    \a parent.
*/
int QSqlTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return QSqlQueryModel::rowCount() + insertedBefore(std::numeric_limits<int>::max());
}

/*!
    Removes \a count columns starting with the column \a column from the
    model and returns \c true on success. \a parent has to be invalid. The
    table of the database keeps its columns; the model no longer reads or
    writes them. Pending changes are dropped.
*/
bool QSqlTableModel::removeColumns(int column, int count, const QModelIndex &parent) {
    if (parent.isValid() || column < 0 || count <= 0 || column + count > m_baseRec.count())
        return false;
    revertAll();
    for (int i = 0; i < count; ++i)
        m_baseRec.remove(column);
    if (query().isActive())
        return select();
    m_cache.clear();
    beginRemoveColumns(parent, column, column + count - 1);
    initColumns(m_baseRec);
    endRemoveColumns();
    return true;
}

/*!
    Removes \a count rows starting with the row \a row and returns \c true
    on success. \a parent has to be invalid. A row that waits to be
    inserted is dropped. Another row is deleted from the table: at once,
    unless the strategy is \c OnManualSubmit, with which the row stays in
    the model, marked with \c{!} in the vertical header, until submitAll()
    deletes it or revertAll() keeps it.

    \sa insertRows(), deleteRowFromTable(), beforeDelete()
*/
bool QSqlTableModel::removeRows(int row, int count, const QModelIndex &parent) {
    if (parent.isValid() || row < 0 || count <= 0 || row + count > rowCount())
        return false;
    if (m_strategy != OnManualSubmit) {
        if (count > 1)
            return false;
        const auto it = m_cache.constFind(row);
        if (it == m_cache.constEnd() || it.value().op != Insert) {
            // Write what waits in other rows first, and drop what was changed in this one.
            revertRow(row);
            if (isDirty() && !submitPending())
                return false;
            if (row >= rowCount())
                return false;
        }
    }
    for (int r = row + count - 1; r >= row; --r) {
        const auto it = m_cache.constFind(r);
        if (it != m_cache.constEnd() && it.value().op == Insert) {
            revertRow(r);
            continue;
        }
        ModifiedRow &entry = touch(r);
        entry.op = Delete;
        headerDataChanged(Qt::Vertical, r, r);
    }
    if (m_strategy != OnManualSubmit)
        return submitPending();
    return true;
}

/*!
    Inserts \a count empty rows before the row \a row and returns \c true
    on success. \a parent has to be invalid, and with the strategies
    \c OnFieldChange and \c OnRowChange only one row can be inserted at a
    time, after pending changes of other rows were written. The rows are
    not in the table of the database yet: primeInsert() is emitted for each
    of them, setData() or setRecord() fill them in, and submit() or
    submitAll() insert them.

    \sa insertRecord(), removeRows(), primeInsert()
*/
bool QSqlTableModel::insertRows(int row, int count, const QModelIndex &parent) {
    if (parent.isValid() || row < 0 || count <= 0 || row > rowCount() || m_baseRec.isEmpty())
        return false;
    if (m_strategy != OnManualSubmit) {
        if (count != 1)
            return false;
        if (isDirty()) {
            if (!submitPending())
                return false;
            if (row > rowCount())
                row = rowCount();
        }
    }
    beginInsertRows(parent, row, row + count - 1);
    // The rows from here on move down.
    QMap<int, ModifiedRow> moved;
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it)
        moved.insert(it.key() >= row ? it.key() + count : it.key(), it.value());
    m_cache = moved;
    for (int i = 0; i < count; ++i) {
        ModifiedRow entry;
        entry.op = Insert;
        entry.inserted = true;
        entry.rec = m_baseRec;
        entry.rec.clearValues();
        for (int f = 0; f < entry.rec.count(); ++f)
            entry.rec.setGenerated(f, false);
        primeInsert(row + i, entry.rec);
        m_cache.insert(row + i, entry);
    }
    endInsertRows();
    return true;
}

/*!
    Inserts a row before the row \a row, or at the end if \a row is
    negative, gives it the values of \a record, and returns \c true on
    success. With the strategies \c OnFieldChange and \c OnRowChange the
    row is inserted into the table at once.

    \sa insertRows(), setRecord()
*/
bool QSqlTableModel::insertRecord(int row, const QSqlRecord &record) {
    if (row < 0)
        row = rowCount();
    if (!insertRows(row, 1))
        return false;
    const EditStrategy strategy = m_strategy;
    m_strategy = OnManualSubmit; // fill the row in before anything is written
    const bool ok = setRecord(row, record);
    m_strategy = strategy;
    if (!ok) {
        revertRow(row);
        return false;
    }
    if (m_strategy != OnManualSubmit)
        return submitPending();
    return true;
}

/*!
    Gives the row \a row the values of the fields of \a record that are
    marked as generated, and returns \c true if every one of them could be
    set. Fields are matched by name; a field of \a record that the model
    does not have makes the function return \c false, after the others were
    set. With the strategy \c OnFieldChange the row is written to the
    database at once.

    \sa record(), insertRecord(), setData()
*/
bool QSqlTableModel::setRecord(int row, const QSqlRecord &record) {
    if (row < 0 || row >= rowCount())
        return false;
    if (m_strategy != OnManualSubmit && dirtyOtherThan(row)) {
        if (!submitPending() || row >= rowCount())
            return false;
    }
    bool all = true;
    int first = -1;
    int last = -1;
    for (int i = 0; i < record.count(); ++i) {
        if (!record.isGenerated(i))
            continue;
        const int column = fieldIndex(record.fieldName(i));
        if (column < 0 || !(flags(createIndex(row, column)) & Qt::ItemIsEditable)) {
            all = false;
            continue;
        }
        ModifiedRow &entry = touch(row);
        if (entry.op == None)
            entry.op = Update;
        entry.rec.setValue(column, record.value(i));
        entry.rec.setGenerated(column, true);
        first = first < 0 || column < first ? column : first;
        last = column > last ? column : last;
    }
    if (first >= 0)
        dataChanged(createIndex(row, first), createIndex(row, last), QList<int>());
    if (m_strategy == OnFieldChange)
        return submitPending() && all;
    return all;
}

/*!
    Drops the pending changes of the row \a row: a row that waits to be
    inserted is removed from the model, and another row gets back the
    values and the state that it has in the table.

    \sa revertAll(), revert()
*/
void QSqlTableModel::revertRow(int row) {
    const auto it = m_cache.constFind(row);
    if (it == m_cache.constEnd())
        return;
    if (it.value().op == Insert) {
        beginRemoveRows(QModelIndex(), row, row);
        QMap<int, ModifiedRow> moved;
        for (auto c = m_cache.constBegin(); c != m_cache.constEnd(); ++c) {
            if (c.key() != row)
                moved.insert(c.key() > row ? c.key() - 1 : c.key(), c.value());
        }
        m_cache = moved;
        endRemoveRows();
        return;
    }
    if (it.value().op == None)
        return;
    if (it.value().inserted || it.value().refreshed) {
        // The query does not have the row as it is in the table: keep what the model knows.
        ModifiedRow &entry = m_cache[row];
        entry.op = None;
        entry.rec = entry.dbRec;
    } else {
        m_cache.remove(row);
    }
    rowChanged(row);
}

// Tells the views that the row at position row has other values, or another state.
void QSqlTableModel::rowChanged(int row) {
    if (columnCount() > 0)
        dataChanged(createIndex(row, 0), createIndex(row, columnCount() - 1), QList<int>());
    headerDataChanged(Qt::Vertical, row, row);
}

/*!
    Reads the rows of the table again, with the filter and the sort order
    that are set, and returns \c true on success. The model is reset and
    pending changes are lost. lastError() tells what went wrong.

    \sa setTable(), setFilter(), setSort(), selectStatement()
*/
bool QSqlTableModel::select() {
    const QString statement = selectStatement();
    if (statement.isEmpty())
        return false;
    m_cache.clear();
    const QSqlRecord columns = m_baseRec;
    setQuery(QSqlQuery(statement, m_db));
    if (!query().isActive() || lastError().isValid()) {
        // Keep the columns of the table, so that the model still has its shape.
        beginResetModel();
        initColumns(columns);
        endResetModel();
        return false;
    }
    return true;
}

/*!
    Reads the row \a row from the table again and returns \c true on
    success. Pending changes of the row are dropped; the other rows are
    left alone, and the model is not reset. Returns \c false if there is no
    such row, if it waits to be inserted, or if it is no longer in the
    table.

    \sa select()
*/
bool QSqlTableModel::selectRow(int row) {
    if (row < 0 || row >= rowCount())
        return false;
    const auto it = m_cache.constFind(row);
    if (it != m_cache.constEnd() && it.value().op == Insert)
        return false;
    ModifiedRow &entry = touch(row);
    const bool found = refreshRow(entry);
    entry.op = None;
    entry.rec = entry.dbRec;
    rowChanged(row);
    return found;
}

/*!
    Writes the pending changes to the database if the strategy is
    \c OnRowChange or \c OnFieldChange, and returns \c true on success.
    With \c OnManualSubmit it does nothing and returns \c true: use
    submitAll(). Views call this when the current row changes.

    \sa submitAll(), revert(), lastError()
*/
bool QSqlTableModel::submit() {
    if (m_strategy == OnRowChange || m_strategy == OnFieldChange)
        return submitAll();
    return true;
}

/*!
    Drops the pending changes if the strategy is \c OnRowChange or
    \c OnFieldChange. With \c OnManualSubmit it does nothing: use
    revertAll().

    \sa revertAll(), submit()
*/
void QSqlTableModel::revert() {
    if (m_strategy == OnRowChange || m_strategy == OnFieldChange)
        revertAll();
}

/*!
    Writes all pending changes to the database and returns \c true on
    success. With the strategy \c OnManualSubmit, and whenever a row was
    deleted, the table is then read again, which resets the model;
    otherwise only the rows that were written are read again. On failure it
    stops at the change that failed, returns \c false and leaves the changes
    that remain pending; lastError() tells what went wrong. Changes that
    were written before the failure stay written: wrap submitAll() in a
    transaction of the connection to make it all or nothing.

    \sa revertAll(), submit(), QSqlDatabase::transaction(), lastError()
*/
bool QSqlTableModel::submitAll() {
    const QList<int> rows = m_cache.keys();
    bool wrote = false;
    bool selectAll = m_strategy == OnManualSubmit;
    QList<int> refreshed;
    for (const int row : rows) {
        ModifiedRow &entry = m_cache[row];
        if (entry.op == None)
            continue;
        bool ok = false;
        switch (entry.op) {
        case Insert: {
            QSqlRecord rec = entry.rec;
            beforeInsert(rec);
            m_insertId = QVariant();
            ok = insertRowInTable(rec);
            if (ok) {
                // What finds the new row: its key, with the row id for a key that the database assigned.
                for (int i = 0; i < rec.count(); ++i)
                    rec.setGenerated(i, !m_nonKeyColumns.contains(i));
                QSqlRecord key = m_primaryIndex.isEmpty() ? rec : rec.keyValues(m_primaryIndex);
                for (int i = 0; i < key.count(); ++i) {
                    if (key.isNull(i) && m_baseRec.field(key.fieldName(i)).isAutoValue())
                        key.setValue(i, m_insertId);
                }
                entry.primaryValues = key;
                entry.dbRec = rec;
            }
            break;
        }
        case Update: {
            QSqlRecord rec = entry.rec;
            beforeUpdate(row, rec);
            ok = updateRowInTable(row, rec);
            if (ok) {
                // A key that was changed finds the row under its new value.
                for (int i = 0; i < entry.primaryValues.count(); ++i) {
                    const int field = rec.indexOf(entry.primaryValues.fieldName(i));
                    if (field >= 0 && rec.isGenerated(field))
                        entry.primaryValues.setValue(i, rec.value(field));
                }
                entry.dbRec = rec;
            }
            break;
        }
        case Delete:
            beforeDelete(row);
            ok = deleteRowFromTable(row);
            selectAll = selectAll || ok; // the rows behind it move up
            break;
        case None:
            break;
        }
        if (!ok) {
            // What was written so far is shown as it is now in the table.
            for (const int done : refreshed)
                rowChanged(done);
            return false;
        }
        wrote = true;
        const bool wasDelete = entry.op == Delete;
        entry.op = None;
        if (!wasDelete) {
            for (int i = 0; i < entry.dbRec.count(); ++i)
                entry.dbRec.setGenerated(i, false);
            refreshRow(entry);
            entry.rec = entry.dbRec;
            refreshed.append(row);
        }
    }
    if (!wrote)
        return true;
    if (selectAll)
        return select();
    for (const int row : refreshed)
        rowChanged(row);
    return true;
}

bool QSqlTableModel::submitPending() { return submitAll(); }

/*!
    Drops all pending changes: rows that wait to be inserted are removed,
    and the other rows are shown as they are in the table.

    \sa submitAll(), revertRow()
*/
void QSqlTableModel::revertAll() {
    const QList<int> rows = m_cache.keys();
    for (qsizetype i = rows.size() - 1; i >= 0; --i)
        revertRow(rows.at(i));
}

// Runs a generated statement: binds the generated fields of values and then the generated fields of
// whereValues that are not null, in that order, which is the order of the placeholders.
bool QSqlTableModel::exec(const QString &statement, const QSqlRecord &values, const QSqlRecord &whereValues,
                          QVariant *insertId) {
    if (statement.isEmpty())
        return false;
    QSqlQuery q(m_db);
    if (!q.prepare(statement)) {
        setLastError(q.lastError());
        return false;
    }
    for (int i = 0; i < values.count(); ++i) {
        if (values.isGenerated(i))
            q.addBindValue(values.value(i));
    }
    for (int i = 0; i < whereValues.count(); ++i) {
        if (whereValues.isGenerated(i) && !whereValues.isNull(i))
            q.addBindValue(whereValues.value(i));
    }
    if (!q.exec()) {
        setLastError(q.lastError());
        return false;
    }
    if (insertId)
        *insertId = q.lastInsertId();
    return true;
}

// True if a row other than the one at position row has changes that wait to be written.
bool QSqlTableModel::dirtyOtherThan(int row) const {
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        if (it.key() != row && it.value().op != None)
            return true;
    }
    return false;
}

// The condition that finds the row with the values of key, written with literals and with the names of the
// fields qualified by the table, so that it can be the filter of selectStatement().
QString QSqlTableModel::rowCondition(const QSqlRecord &key) const {
    const QSqlDriver *driver = m_db.driver();
    const QString table = driver->isIdentifierEscaped(m_table, QSqlDriver::TableName)
                              ? m_table
                              : driver->escapeIdentifier(m_table, QSqlDriver::TableName);
    QString condition;
    for (int i = 0; i < key.count(); ++i) {
        if (!key.isGenerated(i))
            continue;
        const QString name = key.fieldName(i);
        if (!condition.isEmpty())
            condition += QStringLiteral(" AND ");
        condition += table + QChar('.') +
                     (driver->isIdentifierEscaped(name, QSqlDriver::FieldName)
                          ? name
                          : driver->escapeIdentifier(name, QSqlDriver::FieldName));
        condition +=
            key.isNull(i) ? QStringLiteral(" IS NULL") : QStringLiteral(" = ") + driver->formatValue(key.field(i));
    }
    return condition;
}

// Reads the row of entry from the table into entry.dbRec, with the statement that select() uses, so that a
// subclass shows the row the way it shows the others. Returns false if the row was not found.
bool QSqlTableModel::refreshRow(ModifiedRow &entry) {
    const QString condition = rowCondition(entry.primaryValues);
    if (condition.isEmpty())
        return false;
    const QString filter = m_filter;
    const int sortColumn = m_sortColumn;
    m_filter = condition;
    m_sortColumn = -1;
    const QString statement = selectStatement();
    m_filter = filter;
    m_sortColumn = sortColumn;
    QSqlQuery q(m_db);
    if (statement.isEmpty() || !q.exec(statement) || !q.next())
        return false;
    QSqlRecord rec = m_baseRec;
    for (int i = 0; i < rec.count(); ++i) {
        rec.setValue(i, q.value(i));
        rec.setGenerated(i, false);
    }
    entry.dbRec = rec;
    entry.refreshed = true;
    // The key as it is now, which a trigger may have changed.
    for (int i = 0; i < entry.primaryValues.count(); ++i) {
        const int field = rec.indexOf(entry.primaryValues.fieldName(i));
        if (field >= 0 && !m_nonKeyColumns.contains(field))
            entry.primaryValues.setValue(i, rec.value(field));
    }
    return true;
}

/*!
    Updates the row \a row in the table of the database with the fields of
    \a values that are marked as generated, and returns \c true on success.
    The row is found by primaryValues(). Called by submitAll(); a subclass
    can write the row in another way.

    \sa insertRowInTable(), deleteRowFromTable(), beforeUpdate()
*/
bool QSqlTableModel::updateRowInTable(int row, const QSqlRecord &values) {
    const QSqlDriver *driver = m_db.driver();
    const QSqlRecord where = primaryValues(row);
    const QString update = driver->sqlStatement(QSqlDriver::UpdateStatement, m_table, values, true);
    if (update.isEmpty())
        return true; // no field was changed
    const QString condition = driver->sqlStatement(QSqlDriver::WhereStatement, m_table, where, true);
    if (condition.isEmpty()) {
        setLastError(QSqlError(QStringLiteral("No Fields to update"), QString(), QSqlError::StatementError));
        return false;
    }
    return exec(update + QChar(' ') + condition, values, where);
}

/*!
    Inserts a row with the fields of \a values that are marked as
    generated into the table of the database, and returns \c true on
    success. A row without such fields is inserted with the default values
    of the table. Called by submitAll().

    \sa updateRowInTable(), deleteRowFromTable(), beforeInsert()
*/
bool QSqlTableModel::insertRowInTable(const QSqlRecord &values) {
    const QSqlDriver *driver = m_db.driver();
    QString insert = driver->sqlStatement(QSqlDriver::InsertStatement, m_table, values, true);
    if (insert.isEmpty()) {
        const QString table = driver->isIdentifierEscaped(m_table, QSqlDriver::TableName)
                                  ? m_table
                                  : driver->escapeIdentifier(m_table, QSqlDriver::TableName);
        insert = QStringLiteral("INSERT INTO ") + table + QStringLiteral(" DEFAULT VALUES");
    }
    return exec(insert, values, QSqlRecord(), &m_insertId);
}

/*!
    Deletes the row \a row from the table of the database and returns
    \c true on success. The row is found by primaryValues(). Called by
    submitAll().

    \sa updateRowInTable(), insertRowInTable(), beforeDelete()
*/
bool QSqlTableModel::deleteRowFromTable(int row) {
    const QSqlDriver *driver = m_db.driver();
    const QSqlRecord where = primaryValues(row);
    const QString condition = driver->sqlStatement(QSqlDriver::WhereStatement, m_table, where, true);
    if (condition.isEmpty()) {
        setLastError(QSqlError(QStringLiteral("Unable to delete row"), QString(), QSqlError::StatementError));
        return false;
    }
    const QString statement =
        driver->sqlStatement(QSqlDriver::DeleteStatement, m_table, QSqlRecord(), true) + QChar(' ') + condition;
    return exec(statement, QSqlRecord(), where);
}

/*!
    Returns the \c{ORDER BY} clause for the column and the order of
    setSort(), or an empty string if no sort column is set.

    \sa setSort(), selectStatement()
*/
QString QSqlTableModel::orderByClause() const {
    if (m_sortColumn < 0 || m_sortColumn >= m_baseRec.count())
        return QString();
    const QSqlDriver *driver = m_db.driver();
    const auto escaped = [driver](const QString &name, QSqlDriver::IdentifierType type) {
        return driver->isIdentifierEscaped(name, type) ? name : driver->escapeIdentifier(name, type);
    };
    return QStringLiteral("ORDER BY ") + escaped(m_table, QSqlDriver::TableName) + QChar('.') +
           escaped(m_baseRec.fieldName(m_sortColumn), QSqlDriver::FieldName) +
           (m_sortOrder == Qt::AscendingOrder ? QStringLiteral(" ASC") : QStringLiteral(" DESC"));
}

/*!
    Returns the \c SELECT statement that select() runs: the fields of the
    model from the table, with the filter and the sort order. Returns an
    empty string, and sets lastError(), if no table is set or the table
    has no fields.

    \sa select(), orderByClause(), setFilter()
*/
QString QSqlTableModel::selectStatement() const {
    auto *self = const_cast<QSqlTableModel *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): reports errors
    if (m_table.isEmpty()) {
        self->setLastError(QSqlError(QStringLiteral("No table name given"), QString(), QSqlError::StatementError));
        return QString();
    }
    if (m_baseRec.isEmpty()) {
        self->setLastError(
            QSqlError(QStringLiteral("Unable to find table ") + m_table, QString(), QSqlError::StatementError));
        return QString();
    }
    QString statement = m_db.driver()->sqlStatement(QSqlDriver::SelectStatement, m_table, m_baseRec, false);
    if (statement.isEmpty()) {
        self->setLastError(QSqlError(QStringLiteral("Unable to select fields from table ") + m_table, QString(),
                                     QSqlError::StatementError));
        return statement;
    }
    if (!m_filter.isEmpty())
        statement += QStringLiteral(" WHERE ") + m_filter;
    const QString order = orderByClause();
    if (!order.isEmpty())
        statement += QChar(' ') + order;
    return statement;
}

/*!
    Sets the fields that find a row in the table to \a key. This is for a
    table whose primary key the driver does not report, such as a view.

    \sa primaryKey(), primaryValues()
*/
void QSqlTableModel::setPrimaryKey(const QSqlIndex &key) { m_primaryIndex = key; }

/*!
    Returns the fields that find the row \a row in the table, with the
    values that the row has there, whatever was changed in the model: the
    fields of the primary key, or all the fields if the table has none. The
    record is empty for a row that is not in the table yet.

    \sa primaryKey(), updateRowInTable(), deleteRowFromTable()
*/
QSqlRecord QSqlTableModel::primaryValues(int row) const {
    const auto it = m_cache.constFind(row);
    if (it != m_cache.constEnd())
        return it.value().primaryValues;
    QSqlRecord rec = baseRecord(row);
    for (int i = 0; i < rec.count(); ++i)
        rec.setGenerated(i, !m_nonKeyColumns.contains(i));
    return m_primaryIndex.isEmpty() ? rec : rec.keyValues(m_primaryIndex);
}

/*!
    Returns the index in the query of the item \a item of the model: the
    rows that wait to be inserted are not in the query, so the rows after
    them have a smaller row number there. Returns an invalid index for an
    item in such a row.
*/
QModelIndex QSqlTableModel::indexInQuery(const QModelIndex &item) const {
    if (!item.isValid())
        return QModelIndex();
    const auto it = m_cache.constFind(item.row());
    if (it != m_cache.constEnd() && it.value().inserted)
        return QModelIndex();
    return QSqlQueryModel::indexInQuery(createIndex(baseRow(item.row()), item.column(), item.internalPointer()));
}

// The number of rows before the row at position row that wait to be inserted.
int QSqlTableModel::insertedBefore(int row) const {
    int n = 0;
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd() && it.key() < row; ++it) {
        if (it.value().inserted)
            ++n;
    }
    return n;
}

// The position in the query of the row at position row of the model.
int QSqlTableModel::baseRow(int row) const { return row - insertedBefore(row); }

// The values that the row at position row has in the table.
QSqlRecord QSqlTableModel::baseRecord(int row) const {
    QSqlRecord rec = m_baseRec;
    const auto it = m_cache.constFind(row);
    if (row < 0 || (it != m_cache.constEnd() && it.value().inserted))
        return rec;
    for (int i = 0; i < rec.count(); ++i)
        rec.setValue(i, QSqlQueryModel::data(createIndex(row, i), Qt::EditRole));
    return rec;
}

// The pending change of the row at position row, which is made if there is none.
QSqlTableModel::ModifiedRow &QSqlTableModel::touch(int row) {
    auto it = m_cache.find(row);
    if (it != m_cache.end())
        return it.value();
    ModifiedRow entry;
    entry.primaryValues = primaryValues(row);
    entry.rec = baseRecord(row);
    for (int i = 0; i < entry.rec.count(); ++i)
        entry.rec.setGenerated(i, false);
    entry.dbRec = entry.rec;
    return m_cache.insert(row, entry).value();
}

#include "moc_qsqltablemodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
