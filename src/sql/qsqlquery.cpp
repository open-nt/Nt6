// MIT License - see LICENSE
#include <QtSql/qsqlquery.h>

#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlrecord.h>
#include <QtSql/qsqlresult.h>

#include "qsqlnulldriver_p.h"

#include <cstdio>

/*!
    \class QSqlQuery
    \brief The QSqlQuery class provides a means of executing and manipulating SQL statements.

    A QSqlQuery runs SQL statements on a connection and walks the rows that
    a \c SELECT returns. Without a QSqlDatabase it uses the default
    connection.

    \code
    QSqlQuery query("SELECT name, age FROM people ORDER BY name");
    while (query.next()) {
        QString name = query.value(0).toString();
        int age = query.value(1).toInt();
    }
    \endcode

    After a successful exec() the query is active (isActive()) and
    positioned before the first row: next() moves to the first row and then
    on, and returns \c false after the last one. previous(), first(),
    last() and seek() move around freely, unless the query is forward-only.
    value() reads a field of the current row by position, or by name, which
    is slower; record() tells the names and positions.

    \section1 Binding Values

    Values belong in placeholders, not in the text of the statement: this
    keeps them apart from the SQL, whatever characters they contain, and
    the statement is prepared only once. Placeholders have names or are
    written \c{?}:

    \code
    QSqlQuery query;
    query.prepare("INSERT INTO people (name, age) VALUES (:name, :age)");
    query.bindValue(":name", "Ada");
    query.bindValue(":age", 36);
    query.exec();

    query.prepare("SELECT name FROM people WHERE age > ?");
    query.addBindValue(30);
    query.exec();
    \endcode

    The values stay bound: binding one of them again and calling exec()
    runs the statement with the others unchanged.

    \differences
    A query can be moved but not copied; copying is deprecated in Qt 6. A
    statement with placeholders is always given to the driver with \c{?}
    placeholders, so executedQuery() shows those; bound values are not
    written into its text. \c setPositionalBindingEnabled() is missing.
    execBatch() runs the statement once for each row with every driver.
    With the \c QSQLITE driver, size() is -1 for a \c SELECT, as in Qt, and
    a statement text that holds several statements is an error.
    \enddifferences

    \sa QSqlDatabase, QSqlRecord, QSqlQueryModel, {Using Nt SQL}
*/

/*!
    Constructs a query that uses \a result, which a driver made, and takes
    ownership of it.
*/
QSqlQuery::QSqlQuery(QSqlResult *result) : m_result(result) {
    if (!m_result)
        m_result.reset(QSqlDatabase().driver()->createResult());
}

/*!
    Constructs a query on the connection \a db, or on the default
    connection if \a db is not valid. If \a query is not empty, it is run
    at once, as by exec().

    \sa QSqlDatabase::database()
*/
QSqlQuery::QSqlQuery(const QString &query, const QSqlDatabase &db) {
    QSqlDatabase database = db;
    if (!database.isValid())
        database = QSqlDatabase::database(QLatin1StringView(QSqlDatabase::defaultConnection), false);
    m_result.reset(database.driver()->createResult());
    if (!query.isEmpty())
        exec(query);
}

/*!
    \overload
    Constructs a query on the connection \a db.
*/
QSqlQuery::QSqlQuery(const QSqlDatabase &db) : QSqlQuery(QString(), db) {}

/*!
    Constructs a query that takes over the statement and the rows of
    \a other. \a other can only be assigned to or destroyed afterwards.
*/
QSqlQuery::QSqlQuery(QSqlQuery &&other) noexcept = default;

/*!
    Takes over the statement and the rows of \a other and returns a
    reference to this query.
*/
QSqlQuery &QSqlQuery::operator=(QSqlQuery &&other) noexcept = default;

/*!
    Destroys the query and releases what it holds of the database.
*/
QSqlQuery::~QSqlQuery() = default;

/*!
    Returns \c true if the query is positioned on a row; otherwise returns
    \c false.

    \sa at(), next()
*/
bool QSqlQuery::isValid() const { return m_result->isValid(); }

/*!
    Returns \c true if a statement was run with success and is not
    finished; otherwise returns \c false.

    \sa exec(), finish()
*/
bool QSqlQuery::isActive() const { return m_result->isActive(); }

/*!
    Returns \c true if the field at position \a field of the current row is
    null, if the query is not on a row, or if there is no such field;
    otherwise returns \c false.

    \sa value()
*/
bool QSqlQuery::isNull(int field) const {
    return !(m_result->isActive() && m_result->isValid() && field >= 0 && field < m_result->record().count()) ||
           m_result->isNull(field);
}

/*!
    \overload
    Returns \c true if the field called \a name of the current row is null.
*/
bool QSqlQuery::isNull(QAnyStringView name) const {
    const int index = m_result->record().indexOf(name);
    if (index < 0) {
        std::fprintf(stderr, "QSqlQuery::isNull: unknown field name '%s'\n", name.toString().toUtf8().constData());
        return true;
    }
    return isNull(index);
}

/*!
    Returns the position of the current row, counted from 0, or
    QSql::BeforeFirstRow or QSql::AfterLastRow.

    \sa isValid(), seek()
*/
int QSqlQuery::at() const { return m_result->at(); }

/*!
    Returns the text of the statement that was run or prepared last, as it
    was written.

    \sa executedQuery()
*/
QString QSqlQuery::lastQuery() const { return m_result->lastQuery(); }

/*!
    Returns the number of rows that the last statement changed, or -1 if
    that is not known or the query is not active. For a \c SELECT, use
    size().

    \sa size()
*/
int QSqlQuery::numRowsAffected() const { return m_result->isActive() ? m_result->numRowsAffected() : -1; }

/*!
    Returns what went wrong with the last statement, if anything did.

    \sa QSqlError
*/
QSqlError QSqlQuery::lastError() const { return m_result->lastError(); }

/*!
    Returns \c true if the statement returns rows, as a \c SELECT does;
    otherwise returns \c false.
*/
bool QSqlQuery::isSelect() const { return m_result->isSelect(); }

/*!
    Returns the number of rows of a \c SELECT, or -1 if the query is not an
    active \c SELECT or if the driver does not know the number; see
    QSqlDriver::QuerySize. Count the rows with next(), or with last() and
    at().

    \sa numRowsAffected()
*/
int QSqlQuery::size() const {
    const QSqlDriver *drv = m_result->driver();
    if (m_result->isActive() && m_result->isSelect() && drv && drv->hasFeature(QSqlDriver::QuerySize))
        return m_result->size();
    return -1;
}

/*!
    Returns the driver of the connection of the query, or \c nullptr if the
    connection has been removed and its driver destroyed.
*/
const QSqlDriver *QSqlQuery::driver() const { return m_result->driver(); }

/*!
    Returns the result that the driver made for this query.
*/
const QSqlResult *QSqlQuery::result() const { return m_result.get(); }

/*!
    Returns \c true if the rows can only be read forwards, with next() and
    seek() to a later row; otherwise returns \c false.

    \sa setForwardOnly()
*/
bool QSqlQuery::isForwardOnly() const { return m_result->isForwardOnly(); }

/*!
    Returns the fields of the query. On a row, the fields have the values
    of the row; otherwise their values are null. An inactive query gives an
    empty record.

    \sa value()
*/
QSqlRecord QSqlQuery::record() const {
    QSqlRecord rec = m_result->record();
    if (isValid()) {
        for (int i = 0; i < rec.count(); ++i)
            rec.setValue(i, value(i));
    }
    return rec;
}

/*!
    Sets whether the rows are only read forwards to \a forward. A
    forward-only query needs less memory, as the driver does not have to
    keep the rows that were read; previous(), first() once the first row
    has been passed, and seek() to an earlier row then fail. Call this
    before the statement is run.

    \sa isForwardOnly(), next()
*/
void QSqlQuery::setForwardOnly(bool forward) { m_result->setForwardOnly(forward); }

/*!
    Runs the SQL statement \a query and returns \c true on success. The
    query is then active, and for a \c SELECT it is positioned before the
    first row. On failure, lastError() tells why.

    \a query has to be one statement in the SQL of the database. It is run
    as it is: do not build it from values that come from outside, bind them
    with prepare() and bindValue() instead.

    \sa prepare(), next(), lastError()
*/
bool QSqlQuery::exec(const QString &query) {
    m_result->detachFromResultSet();
    m_result->setActive(false);
    m_result->setLastError(QSqlError());
    m_result->setAt(QSql::BeforeFirstRow);
    const QSqlDriver *drv = m_result->driver();
    if (!drv || !drv->isOpen() || drv->isOpenError()) {
        std::fputs("QSqlQuery::exec: database not open\n", stderr);
        m_result->setLastError(QSqlError(QStringLiteral("Driver not loaded"), QStringLiteral("Database not open"),
                                         QSqlError::ConnectionError));
        return false;
    }
    if (query.isEmpty()) {
        std::fputs("QSqlQuery::exec: empty query\n", stderr);
        return false;
    }
    const QString statement = query.trimmed();
    m_result->clear();
    m_result->setQuery(statement);
    return m_result->reset(statement);
}

/*!
    Returns the value of the field at position \a i of the current row.
    Fields are counted from 0, in the order of the \c SELECT; with
    \c{SELECT *} that order is up to the database, so name the columns.

    Returns an invalid QVariant, with a warning, if the query is not on a
    row or there is no such field.

    \sa record(), isNull(), next()
*/
QVariant QSqlQuery::value(int i) const {
    if (m_result->isActive() && m_result->isValid() && i >= 0 && i < m_result->record().count())
        return m_result->data(i);
    std::fputs("QSqlQuery::value: not positioned on a valid record\n", stderr);
    return QVariant();
}

/*!
    \overload
    Returns the value of the field called \a name of the current row. This
    looks the name up each time; in a loop over many rows, get the position
    from record() once.
*/
QVariant QSqlQuery::value(QAnyStringView name) const {
    const int index = m_result->record().indexOf(name);
    if (index >= 0)
        return value(index);
    std::fprintf(stderr, "QSqlQuery::value: unknown field name '%s'\n", name.toString().toUtf8().constData());
    return QVariant();
}

/*!
    Sets how numbers of high precision are returned to \a precisionPolicy,
    from the next statement on.

    \sa numericalPrecisionPolicy()
*/
void QSqlQuery::setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy precisionPolicy) {
    m_result->setNumericalPrecisionPolicy(precisionPolicy);
}

/*!
    Returns how numbers of high precision are returned.

    \sa setNumericalPrecisionPolicy()
*/
QSql::NumericalPrecisionPolicy QSqlQuery::numericalPrecisionPolicy() const {
    return m_result->numericalPrecisionPolicy();
}

/*!
    Moves to the row at position \a i, counted from 0, and returns \c true
    if there is such a row. If \a relative is \c true, \a i is counted from
    the current row: a negative number goes back. If there is no such row,
    the query is left before the first or after the last row.

    A forward-only query can only seek forwards.

    \sa next(), previous(), first(), last(), at()
*/
bool QSqlQuery::seek(int i, bool relative) {
    if (!isSelect() || !isActive())
        return false;
    int target = i;
    if (relative) {
        switch (at()) {
        case QSql::BeforeFirstRow:
            if (i <= 0)
                return false;
            target = i - 1;
            break;
        case QSql::AfterLastRow:
            if (i >= 0)
                return false;
            if (isForwardOnly()) {
                std::fputs("QSqlQuery::seek: cannot seek backwards in a forward only query\n", stderr);
                return false;
            }
            if (!m_result->fetchLast()) {
                m_result->setAt(QSql::BeforeFirstRow);
                return false;
            }
            target = at() + i + 1;
            break;
        default:
            if (at() + i < 0) {
                m_result->setAt(QSql::BeforeFirstRow);
                return false;
            }
            target = at() + i;
            break;
        }
    } else if (i < 0) {
        m_result->setAt(QSql::BeforeFirstRow);
        return false;
    }
    if (isForwardOnly() && at() != QSql::BeforeFirstRow && (at() == QSql::AfterLastRow || target < at())) {
        std::fputs("QSqlQuery::seek: cannot seek backwards in a forward only query\n", stderr);
        return false;
    }
    if (target < 0) {
        m_result->setAt(QSql::BeforeFirstRow);
        return false;
    }
    if (target == at() + 1 && at() != QSql::BeforeFirstRow) {
        if (m_result->fetchNext())
            return true;
        m_result->setAt(QSql::AfterLastRow);
        return false;
    }
    if (m_result->fetch(target))
        return true;
    m_result->setAt(QSql::AfterLastRow);
    return false;
}

/*!
    Moves to the next row and returns \c true if there is one: to the first
    row if the query is before it. Returns \c false, leaving the query
    after the last row, when the rows are used up.

    \sa previous(), seek(), isValid()
*/
bool QSqlQuery::next() {
    if (!isSelect() || !isActive())
        return false;
    bool ok = false;
    switch (at()) {
    case QSql::BeforeFirstRow:
        ok = m_result->fetchFirst();
        break;
    case QSql::AfterLastRow:
        return false;
    default:
        ok = m_result->fetchNext();
        break;
    }
    if (!ok)
        m_result->setAt(QSql::AfterLastRow);
    return ok;
}

/*!
    Moves to the row before the current one and returns \c true if there
    is one: to the last row if the query is after it. Returns \c false,
    leaving the query before the first row, at the start. Fails for a
    forward-only query.

    \sa next(), seek()
*/
bool QSqlQuery::previous() {
    if (!isSelect() || !isActive())
        return false;
    if (isForwardOnly()) {
        std::fputs("QSqlQuery::seek: cannot seek backwards in a forward only query\n", stderr);
        return false;
    }
    bool ok = false;
    switch (at()) {
    case QSql::BeforeFirstRow:
        return false;
    case QSql::AfterLastRow:
        ok = m_result->fetchLast();
        break;
    default:
        ok = m_result->fetchPrevious();
        break;
    }
    if (!ok)
        m_result->setAt(QSql::BeforeFirstRow);
    return ok;
}

/*!
    Moves to the first row and returns \c true if there is one. Fails for
    a forward-only query that has left the first row.

    \sa last(), next()
*/
bool QSqlQuery::first() {
    if (!isSelect() || !isActive())
        return false;
    if (isForwardOnly() && at() > QSql::BeforeFirstRow) {
        std::fputs("QSqlQuery::seek: cannot seek backwards in a forward only query\n", stderr);
        return false;
    }
    return m_result->fetchFirst();
}

/*!
    Moves to the last row and returns \c true if there is one. at() then
    tells the number of rows, less one.

    \sa first(), previous()
*/
bool QSqlQuery::last() {
    if (!isSelect() || !isActive())
        return false;
    return m_result->fetchLast();
}

/*!
    Releases the rows and the statement and makes the query inactive, with
    no error and no bound values. It keeps its connection, and whether it
    is forward-only.

    \sa finish()
*/
void QSqlQuery::clear() {
    m_result->detachFromResultSet();
    m_result->clear();
}

/*!
    \overload
    Runs the statement that was prepared with prepare(), with the values
    that are bound to its placeholders, and returns \c true on success.

    \sa prepare(), bindValue(), addBindValue()
*/
bool QSqlQuery::exec() {
    m_result->resetBindCount();
    if (m_result->lastError().isValid())
        m_result->setLastError(QSqlError());
    const QSqlDriver *drv = m_result->driver();
    if (!drv || !drv->isOpen() || drv->isOpenError()) {
        std::fputs("QSqlQuery::exec: database not open\n", stderr);
        m_result->setLastError(QSqlError(QStringLiteral("Driver not loaded"), QStringLiteral("Database not open"),
                                         QSqlError::ConnectionError));
        return false;
    }
    return m_result->exec();
}

/*!
    Runs the prepared statement once for each row of a batch and returns
    \c true if every run succeeded. Each bound value has to be a
    QVariantList with the values of its placeholder, one for each run; a
    null QVariant in a list binds a null value.

    \code
    QSqlQuery query;
    query.prepare("INSERT INTO people (name, age) VALUES (?, ?)");
    query.addBindValue(QVariantList{"Ada", "Grace"});
    query.addBindValue(QVariantList{36, 45});
    query.execBatch();
    \endcode

    Wrap the batch in a transaction, to make it one change and much faster.
    \a mode other than \c ValuesAsRows is not supported and fails.

    \sa prepare(), addBindValue(), QSqlDatabase::transaction()
*/
bool QSqlQuery::execBatch(BatchExecutionMode mode) {
    m_result->resetBindCount();
    if (mode != ValuesAsRows) {
        m_result->setLastError(QSqlError(QStringLiteral("Binding lists as columns is not supported"), QString(),
                                         QSqlError::StatementError));
        return false;
    }
    const QSqlDriver *drv = m_result->driver();
    if (!drv || !drv->isOpen() || drv->isOpenError()) {
        std::fputs("QSqlQuery::execBatch: database not open\n", stderr);
        return false;
    }
    return m_result->execBatch(false);
}

/*!
    Prepares the statement \a query to be run with exec(), and returns
    \c true on success. The statement may have placeholders, with names
    such as \c{:id} or written \c{?}, for values that are bound with
    bindValue() or addBindValue().

    With most databases this fails if the statement has an error or names
    a table that does not exist; lastError() tells.

    \sa exec(), bindValue(), addBindValue()
*/
bool QSqlQuery::prepare(const QString &query) {
    m_result->detachFromResultSet();
    m_result->setActive(false);
    m_result->setLastError(QSqlError());
    m_result->setAt(QSql::BeforeFirstRow);
    const QSqlDriver *drv = m_result->driver();
    if (!drv || !drv->isOpen() || drv->isOpenError()) {
        std::fputs("QSqlQuery::prepare: database not open\n", stderr);
        return false;
    }
    if (query.isEmpty()) {
        std::fputs("QSqlQuery::prepare: empty query\n", stderr);
        return false;
    }
    return m_result->savePrepare(query);
}

/*!
    Binds the value \a val to the placeholder called \a placeholder in the
    prepared statement; the name is written with its colon, as in
    \c{":id"}. A placeholder that appears several times gets the value each
    time. A null QVariant binds a null value. \a type tells the kind of the
    value; add QSql::Binary for binary data that is not a QByteArray.

    Nothing happens if the statement has no such placeholder.

    \sa addBindValue(), prepare(), exec(), boundValue()
*/
void QSqlQuery::bindValue(const QString &placeholder, const QVariant &val, QSql::ParamType type) {
    m_result->bindValue(placeholder, val, type);
}

/*!
    \overload
    Binds the value \a val of the kind \a type to the placeholder at
    position \a pos, counted from 0 in the order of the statement.
*/
void QSqlQuery::bindValue(int pos, const QVariant &val, QSql::ParamType type) { m_result->bindValue(pos, val, type); }

/*!
    Binds the value \a val of the kind \a type to the next placeholder: the
    first call after prepare() or exec() binds the first one.

    \sa bindValue(), prepare(), exec()
*/
void QSqlQuery::addBindValue(const QVariant &val, QSql::ParamType type) { m_result->addBindValue(val, type); }

/*!
    Returns the value that is bound to the placeholder called
    \a placeholder, or an invalid QVariant if there is none.

    \sa boundValues(), bindValue()
*/
QVariant QSqlQuery::boundValue(const QString &placeholder) const { return m_result->boundValue(placeholder); }

/*!
    \overload
    Returns the value that is bound to the placeholder at position \a pos.
*/
QVariant QSqlQuery::boundValue(int pos) const { return m_result->boundValue(pos); }

/*!
    Returns the bound values, in the order of the placeholders in the
    statement.

    \sa boundValueNames(), boundValue()
*/
QVariantList QSqlQuery::boundValues() const { return m_result->boundValues(); }

/*!
    \since 6.6
    Returns the names of the placeholders, in the order of the statement.
    A placeholder that is written \c{?} has an empty name.

    \sa boundValues(), boundValueName()
*/
QStringList QSqlQuery::boundValueNames() const { return m_result->boundValueNames(); }

/*!
    \since 6.6
    Returns the name of the placeholder at position \a pos, or an empty
    string if it has none.

    \sa boundValueNames()
*/
QString QSqlQuery::boundValueName(int pos) const { return m_result->boundValueName(pos); }

/*!
    Returns the statement as it was given to the database: for a prepared
    statement, with a \c{?} for each placeholder; otherwise the same as
    lastQuery().

    \sa lastQuery()
*/
QString QSqlQuery::executedQuery() const {
    const QString executed = m_result->executedQuery();
    return executed.isEmpty() ? m_result->lastQuery() : executed;
}

/*!
    Returns the identifier that the database gave the row that this query
    inserted last, or an invalid QVariant if there is none or the driver
    does not tell; see QSqlDriver::LastInsertId.
*/
QVariant QSqlQuery::lastInsertId() const { return m_result->lastInsertId(); }

/*!
    Releases the rows of the statement and makes the query inactive, so
    that the database is free of it: with SQLite, an unfinished \c SELECT
    keeps a table locked and stops a transaction from being committed. The
    prepared statement and its bound values are kept, so exec() runs it
    again.

    There is no need to call this for a query whose rows were all read or
    that is about to be destroyed.

    \sa clear(), prepare(), exec()
*/
void QSqlQuery::finish() {
    if (!isActive())
        return;
    m_result->setLastError(QSqlError());
    m_result->setAt(QSql::BeforeFirstRow);
    m_result->detachFromResultSet();
    m_result->setActive(false);
}

/*!
    Moves to the next result set of a statement that returns several, and
    returns \c true if there is one; see QSqlDriver::MultipleResultSets.
    The \c QSQLITE driver has none.
*/
bool QSqlQuery::nextResult() { return isActive() && m_result->nextResult(); }

/*!
    Swaps this query with \a other.
*/
void QSqlQuery::swap(QSqlQuery &other) noexcept { m_result.swap(other.m_result); }
