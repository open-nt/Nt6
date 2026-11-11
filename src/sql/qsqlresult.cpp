// MIT License - see LICENSE
#include <QtSql/qsqlresult.h>

#include <QtCore/qpointer.h>
#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlrecord.h>

/*!
    \class QSqlResult
    \brief The QSqlResult class provides an abstract interface for accessing data from specific SQL databases.

    A QSqlResult runs one statement and walks its rows. QSqlQuery does this
    for applications; QSqlResult is there to be subclassed by a driver,
    which returns an object of its subclass from
    QSqlDriver::createResult().

    A subclass implements data(), isNull(), reset(), fetch(), fetchFirst(),
    fetchLast(), size() and numRowsAffected(). A driver that prepares
    statements also implements prepare() and exec(), and most drivers
    implement record() and lastInsertId().

    The base class keeps the values that are bound to the placeholders of a
    prepared statement. savePrepare() reads the statement: it finds the
    placeholders outside of quoted text and comments, and rewrites named
    placeholders as \c{?}, so that a driver only has to bind by position.
    executedQuery() is the rewritten statement and boundValues() the values
    in the order of the placeholders. A name that appears several times has
    one value, which is bound at each of its positions.

    \differences
    Every statement with placeholders is passed to the driver with
    positional \c{?} placeholders, also if the driver has named ones. There
    is no emulation of prepared statements by writing the values into the
    statement, so prepare() of the base class fails: a driver has to prepare
    statements itself.
    \enddifferences

    \sa QSqlDriver, QSqlQuery
*/

class QSqlResultPrivate {
public:
    struct Holder {
        QString name;  // empty for a positional placeholder
        qsizetype pos; // in the executed query
    };

    QPointer<QSqlDriver> driver; // null once the driver is gone, as after QSqlDatabase::removeDatabase()
    QString sql;
    QString executed;
    QSqlError error;
    QList<Holder> holders;
    QVariantList values;
    QList<QSql::ParamType> types;
    int at = QSql::BeforeFirstRow;
    int bindCount = 0;
    QSql::NumericalPrecisionPolicy policy = QSql::LowPrecisionDouble;
    QSqlResult::BindingSyntax syntax = QSqlResult::PositionalBinding;
    bool active = false;
    bool select = false;
    bool forwardOnly = false;

    void grow(int index) {
        while (values.size() <= index) {
            values.append(QVariant());
            types.append(QSql::In);
        }
    }
    void parse(const QString &query);
};

// Finds the placeholders of query and writes it with a '?' for each of them. Text between quotes, bracketed
// names and comments are copied as they are.
void QSqlResultPrivate::parse(const QString &query) {
    holders.clear();
    executed.clear();
    syntax = QSqlResult::PositionalBinding;
    const qsizetype n = query.size();
    const auto isNameChar = [](QChar c) { return c.isLetterOrNumber() || c == QChar('_'); };
    qsizetype i = 0;
    while (i < n) {
        const QChar c = query.at(i);
        if (c == QChar('\'') || c == QChar('"') || c == QChar('`')) {
            // A doubled quote inside the text ends one piece and starts the next, which comes to the same.
            qsizetype end = query.indexOf(c, i + 1);
            end = end < 0 ? n : end + 1;
            executed += query.mid(i, end - i);
            i = end;
        } else if (c == QChar('[')) {
            qsizetype end = query.indexOf(QChar(']'), i + 1);
            end = end < 0 ? n : end + 1;
            executed += query.mid(i, end - i);
            i = end;
        } else if (c == QChar('-') && i + 1 < n && query.at(i + 1) == QChar('-')) {
            qsizetype end = query.indexOf(QChar('\n'), i);
            end = end < 0 ? n : end;
            executed += query.mid(i, end - i);
            i = end;
        } else if (c == QChar('/') && i + 1 < n && query.at(i + 1) == QChar('*')) {
            qsizetype end = query.indexOf(QStringLiteral("*/"), i + 2);
            end = end < 0 ? n : end + 2;
            executed += query.mid(i, end - i);
            i = end;
        } else if (c == QChar('?')) {
            holders.append(Holder{QString(), executed.size()});
            executed += c;
            ++i;
        } else if (c == QChar(':') && i + 1 < n && isNameChar(query.at(i + 1)) &&
                   (i == 0 || query.at(i - 1) != QChar(':'))) {
            qsizetype end = i + 1;
            while (end < n && isNameChar(query.at(end)))
                ++end;
            holders.append(Holder{query.mid(i, end - i), executed.size()});
            executed += QChar('?');
            syntax = QSqlResult::NamedBinding;
            i = end;
        } else if (c == QChar(':') && i + 1 < n && query.at(i + 1) == QChar(':')) {
            executed += QStringLiteral("::"); // a cast, as in PostgreSQL
            i += 2;
        } else {
            executed += c;
            ++i;
        }
    }
}

/*!
    Constructs a result for the driver \a db. The result does not own the
    driver, which has to outlive it.
*/
QSqlResult::QSqlResult(const QSqlDriver *db) : d(std::make_unique<QSqlResultPrivate>()) {
    d->driver = const_cast<QSqlDriver *>(db); // NOLINT(cppcoreguidelines-pro-type-const-cast): QPointer needs it
    if (db)
        d->policy = db->numericalPrecisionPolicy();
}

/*!
    Destroys the result.
*/
QSqlResult::~QSqlResult() = default;

/*!
    Returns the handle of the statement in the client library of the
    database, for code that calls the library directly. The base
    implementation returns an invalid QVariant.
*/
QVariant QSqlResult::handle() const { return QVariant(); }

/*!
    Returns the position of the current row, counted from 0, or
    QSql::BeforeFirstRow or QSql::AfterLastRow.

    \sa setAt(), isValid()
*/
int QSqlResult::at() const { return d->at; }

/*!
    Returns the text of the statement, as it was given to prepare() or
    reset().

    \sa setQuery(), executedQuery()
*/
QString QSqlResult::lastQuery() const { return d->sql; }

/*!
    Returns the last error of the result.

    \sa setLastError()
*/
QSqlError QSqlResult::lastError() const { return d->error; }

/*!
    Returns \c true if the result is positioned on a row; otherwise
    returns \c false.

    \sa at()
*/
bool QSqlResult::isValid() const { return d->at != QSql::BeforeFirstRow && d->at != QSql::AfterLastRow; }

/*!
    Returns \c true if a statement was run with success and is not
    finished; otherwise returns \c false.

    \sa setActive()
*/
bool QSqlResult::isActive() const { return d->active; }

/*!
    Returns \c true if the statement returns rows; otherwise returns
    \c false.

    \sa setSelect()
*/
bool QSqlResult::isSelect() const { return d->select; }

/*!
    Returns \c true if the rows are only read forwards; otherwise returns
    \c false.

    \sa setForwardOnly()
*/
bool QSqlResult::isForwardOnly() const { return d->forwardOnly; }

/*!
    Returns the driver of the result, or \c nullptr if the driver has been
    destroyed.
*/
const QSqlDriver *QSqlResult::driver() const { return d->driver.data(); }

/*!
    Sets the position of the current row to \a index. Called by subclasses
    from fetch() and its relatives.

    \sa at()
*/
void QSqlResult::setAt(int index) { d->at = index; }

/*!
    Sets whether a statement is running to \a active. Called by subclasses.

    \sa isActive()
*/
void QSqlResult::setActive(bool active) { d->active = active; }

/*!
    Sets the last error of the result to \a error. Called by subclasses.

    \sa lastError()
*/
void QSqlResult::setLastError(const QSqlError &error) { d->error = error; }

/*!
    Sets the text of the statement to \a query. Called by QSqlQuery before
    it runs a statement.

    \sa lastQuery()
*/
void QSqlResult::setQuery(const QString &query) { d->sql = query; }

/*!
    Sets whether the statement returns rows to \a select. Called by
    subclasses.

    \sa isSelect()
*/
void QSqlResult::setSelect(bool select) { d->select = select; }

/*!
    Sets whether the rows are only read forwards to \a forward. A driver
    may then keep less of the rows in memory.

    \sa isForwardOnly()
*/
void QSqlResult::setForwardOnly(bool forward) { d->forwardOnly = forward; }

/*!
    Runs the prepared statement with the bound values and returns \c true
    on success. The base implementation fails with an error, as it does not
    emulate prepared statements.

    \sa prepare()
*/
bool QSqlResult::exec() {
    setLastError(
        QSqlError(QStringLiteral("The driver does not run prepared statements"), QString(), QSqlError::StatementError));
    return false;
}

/*!
    Prepares the statement \a query, which may have placeholders, and
    returns \c true on success. A driver implements this by giving
    executedQuery() to the database; the statement has been read by
    savePrepare() before. The base implementation fails with an error.

    \sa exec(), savePrepare()
*/
bool QSqlResult::prepare(const QString &query) {
    static_cast<void>(query);
    setLastError(
        QSqlError(QStringLiteral("The driver does not prepare statements"), QString(), QSqlError::StatementError));
    return false;
}

/*!
    Reads the statement \a sqlquery: remembers its text, finds its
    placeholders, discards the bound values of the statement before, and
    then calls prepare(). Returns what prepare() returns.

    \sa prepare(), executedQuery(), boundValueNames()
*/
bool QSqlResult::savePrepare(const QString &sqlquery) {
    d->sql = sqlquery;
    d->parse(sqlquery);
    d->values.clear();
    d->types.clear();
    d->bindCount = 0;
    return prepare(sqlquery);
}

/*!
    Binds the value \a val of the kind \a type to the placeholder at
    position \a index, counted from 0.

    \sa addBindValue(), boundValue()
*/
void QSqlResult::bindValue(int index, const QVariant &val, QSql::ParamType type) {
    if (index < 0)
        return;
    d->grow(index);
    d->values[index] = val;
    d->types[index] = type;
}

/*!
    \overload
    Binds the value \a val of the kind \a type to every placeholder called
    \a placeholder, which is written with its colon, as in \c{":id"}.
    Nothing happens if the statement has no such placeholder.
*/
void QSqlResult::bindValue(const QString &placeholder, const QVariant &val, QSql::ParamType type) {
    for (int i = 0; i < d->holders.size(); ++i) {
        if (d->holders.at(i).name == placeholder)
            bindValue(i, val, type);
    }
}

/*!
    Binds the value \a val of the kind \a type to the next placeholder:
    the first one after prepare(), then the second, and so on.

    \sa bindValue(), resetBindCount()
*/
void QSqlResult::addBindValue(const QVariant &val, QSql::ParamType type) {
    bindValue(d->bindCount, val, type);
    ++d->bindCount;
}

/*!
    Returns the value that is bound to the placeholder called
    \a placeholder, or an invalid QVariant if there is none.
*/
QVariant QSqlResult::boundValue(const QString &placeholder) const {
    for (int i = 0; i < d->holders.size(); ++i) {
        if (d->holders.at(i).name == placeholder)
            return boundValue(i);
    }
    return QVariant();
}

/*!
    \overload
    Returns the value that is bound to the placeholder at position
    \a index.
*/
QVariant QSqlResult::boundValue(int index) const {
    return index >= 0 && index < d->values.size() ? d->values.at(index) : QVariant();
}

/*!
    Returns the kind of the value that is bound to the placeholder called
    \a placeholder.
*/
QSql::ParamType QSqlResult::bindValueType(const QString &placeholder) const {
    for (int i = 0; i < d->holders.size(); ++i) {
        if (d->holders.at(i).name == placeholder)
            return bindValueType(i);
    }
    return QSql::In;
}

/*!
    \overload
    Returns the kind of the value that is bound to the placeholder at
    position \a index.
*/
QSql::ParamType QSqlResult::bindValueType(int index) const {
    return index >= 0 && index < d->types.size() ? d->types.at(index) : QSql::ParamType(QSql::In);
}

/*!
    Returns the number of bound values.

    \sa boundValues()
*/
int QSqlResult::boundValueCount() const { return static_cast<int>(d->values.size()); }

/*!
    Returns the bound values in the order of the placeholders in the
    statement.

    \sa boundValueCount(), boundValueNames()
*/
QVariantList QSqlResult::boundValues() const { return d->values; }

/*!
    Returns the statement as it is given to the database: the text of
    lastQuery() with a \c{?} in the place of each named placeholder.

    \sa lastQuery()
*/
QString QSqlResult::executedQuery() const { return d->executed; }

/*!
    Returns the names of the placeholders in the order in which they appear
    in the statement. A positional placeholder has an empty name.

    \sa boundValueName()
*/
QStringList QSqlResult::boundValueNames() const {
    QStringList names;
    for (const QSqlResultPrivate::Holder &h : d->holders)
        names.append(h.name);
    return names;
}

/*!
    Returns the name of the placeholder at position \a index, or an empty
    string if it has none.

    \sa boundValueNames()
*/
QString QSqlResult::boundValueName(int index) const {
    return index >= 0 && index < d->holders.size() ? d->holders.at(index).name : QString();
}

/*!
    Forgets the statement, its placeholders, the bound values and the last
    error, and marks the result as inactive.
*/
void QSqlResult::clear() {
    d->sql.clear();
    d->executed.clear();
    d->holders.clear();
    d->values.clear();
    d->types.clear();
    d->bindCount = 0;
    d->error = QSqlError();
    d->at = QSql::BeforeFirstRow;
    d->active = false;
    d->select = false;
}

/*!
    Returns \c true if one of the bound values is an output value;
    otherwise returns \c false.
*/
bool QSqlResult::hasOutValues() const {
    for (const QSql::ParamType type : d->types) {
        if (type & QSql::Out)
            return true;
    }
    return false;
}

/*!
    Returns the way the placeholders of the prepared statement were
    written. A statement with both kinds counts as one with names.
*/
QSqlResult::BindingSyntax QSqlResult::bindingSyntax() const { return d->syntax; }

/*!
    Moves to the row after the current one and returns \c true if there is
    one. The base implementation calls fetch().
*/
bool QSqlResult::fetchNext() { return fetch(d->at == QSql::BeforeFirstRow ? 0 : d->at + 1); }

/*!
    Moves to the row before the current one and returns \c true if there
    is one. The base implementation calls fetch().
*/
bool QSqlResult::fetchPrevious() { return d->at > 0 && fetch(d->at - 1); }

/*!
    Returns the fields of the rows of the statement, without values. The
    base implementation returns an empty record.
*/
QSqlRecord QSqlResult::record() const { return QSqlRecord(); }

/*!
    Returns the identifier that the database gave the row that the last
    statement inserted. The base implementation returns an invalid
    QVariant.
*/
QVariant QSqlResult::lastInsertId() const { return QVariant(); }

/*!
    Runs the prepared statement once for each row of a batch: every bound
    value is a QVariantList, and the statement is run with the first items
    of the lists, then with the second, and so on. Returns \c true if every
    run succeeded. \a arrayBind is ignored. Afterwards, the bound values are
    the lists again.

    \sa QSqlQuery::execBatch()
*/
bool QSqlResult::execBatch(bool arrayBind) {
    static_cast<void>(arrayBind);
    const QVariantList batch = d->values;
    if (batch.isEmpty())
        return false;
    for (const QVariant &column : batch) {
        if (column.typeId() != QMetaType::QVariantList) {
            setLastError(QSqlError(QStringLiteral("Every bound value of a batch has to be a list"), QString(),
                                   QSqlError::StatementError));
            return false;
        }
    }
    const qsizetype rows = batch.at(0).toList().size();
    bool ok = true;
    for (qsizetype row = 0; row < rows && ok; ++row) {
        for (int i = 0; i < batch.size(); ++i) {
            const QVariantList column = batch.at(i).toList();
            d->values[i] = row < column.size() ? column.at(row) : QVariant();
        }
        ok = exec();
    }
    d->values = batch;
    return ok;
}

/*!
    Releases the rows of the statement, so that the database is free for
    other statements, and keeps the prepared statement. The base
    implementation does nothing.

    \sa QSqlQuery::finish()
*/
void QSqlResult::detachFromResultSet() {}

/*!
    Sets how numbers of high precision are returned to \a policy.

    \sa numericalPrecisionPolicy()
*/
void QSqlResult::setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy policy) { d->policy = policy; }

/*!
    Returns how numbers of high precision are returned.

    \sa setNumericalPrecisionPolicy()
*/
QSql::NumericalPrecisionPolicy QSqlResult::numericalPrecisionPolicy() const { return d->policy; }

/*!
    Moves to the next result set of a statement that has several, and
    returns \c true if there is one. The base implementation returns
    \c false.
*/
bool QSqlResult::nextResult() { return false; }

/*!
    Makes the next addBindValue() bind the first placeholder again.
*/
void QSqlResult::resetBindCount() { d->bindCount = 0; }
