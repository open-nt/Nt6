// MIT License - see LICENSE
#include <QtSql/qsqldriver.h>

#include <QtCore/qdatetime.h>
#include <QtSql/qsqlfield.h>
#include <QtSql/qsqlindex.h>
#include <QtSql/qsqlrecord.h>

#include <limits>

/*!
    \class QSqlDriver
    \brief The QSqlDriver class is an abstract base class for accessing specific SQL databases.

    A driver connects Nt SQL to one kind of database. Applications use
    QSqlDatabase and QSqlQuery and rarely meet the driver; it is there to be
    subclassed, in order to add a database. A subclass implements open(),
    close(), hasFeature() and createResult(), together with a QSqlResult
    subclass that runs the statements, and usually tables(), record() and
    primaryIndex(). It is made available with
    QSqlDatabase::registerSqlDriver(), or handed to
    QSqlDatabase::addDatabase() directly.

    The driver also writes the SQL of QSqlTableModel: sqlStatement() makes
    the statements, escapeIdentifier() quotes names and formatValue() writes
    values as literals.

    \differences
    Drivers are compiled in or registered with
    QSqlDatabase::registerSqlDriver(); there are no driver plugins. The only
    driver that comes with Nt is \c QSQLITE.
    \enddifferences

    \sa QSqlDatabase, QSqlResult
*/

/*!
    \class QSqlDriverCreatorBase
    \brief The QSqlDriverCreatorBase class is the base class for SQL driver factories.

    A subclass returns a new driver from createObject(). An object of it is
    given to QSqlDatabase::registerSqlDriver(), which makes the driver
    available under a name. QSqlDriverCreator does this for a driver class
    that has a default constructor.

    \differences
    None.
    \enddifferences

    \sa QSqlDriverCreator, QSqlDatabase::registerSqlDriver()
*/

/*!
    \class QSqlDriverCreator
    \brief The QSqlDriverCreator class is a template class that provides a SQL driver factory for a specific driver type.

    \code
    QSqlDatabase::registerSqlDriver("MYDRIVER", new QSqlDriverCreator<MyDriver>);
    QSqlDatabase db = QSqlDatabase::addDatabase("MYDRIVER");
    \endcode

    \differences
    None.
    \enddifferences

    \sa QSqlDriverCreatorBase, QSqlDatabase::registerSqlDriver()
*/

/*!
    Constructs a driver with the given \a parent.
*/
QSqlDriver::QSqlDriver(QObject *parent) : QObject(parent) {}

/*!
    Destroys the driver. A subclass closes its connection in its own
    destructor.
*/
QSqlDriver::~QSqlDriver() = default;

/*!
    Returns \c true if the connection to the database is open; otherwise
    returns \c false.

    \sa setOpen(), isOpenError()
*/
bool QSqlDriver::isOpen() const { return m_open; }

/*!
    Returns \c true if the last attempt to open the connection failed;
    otherwise returns \c false.

    \sa setOpenError(), lastError()
*/
bool QSqlDriver::isOpenError() const { return m_openError; }

/*!
    Starts a transaction and returns \c true on success. The base
    implementation does nothing and returns \c false.

    \sa commitTransaction(), rollbackTransaction()
*/
bool QSqlDriver::beginTransaction() { return false; }

/*!
    Commits the running transaction and returns \c true on success. The
    base implementation does nothing and returns \c false.

    \sa beginTransaction()
*/
bool QSqlDriver::commitTransaction() { return false; }

/*!
    Rolls the running transaction back and returns \c true on success. The
    base implementation does nothing and returns \c false.

    \sa beginTransaction()
*/
bool QSqlDriver::rollbackTransaction() { return false; }

/*!
    Returns the names of the tables of the kinds \a tableType in the
    database. The base implementation returns an empty list.
*/
QStringList QSqlDriver::tables(QSql::TableType tableType) const {
    static_cast<void>(tableType);
    return QStringList();
}

/*!
    Returns the primary key of the table \a tableName, or an empty index if
    it has none. The base implementation returns an empty index.
*/
QSqlIndex QSqlDriver::primaryIndex(const QString &tableName) const {
    static_cast<void>(tableName);
    return QSqlIndex();
}

/*!
    Returns the fields of the table \a tableName, without values, or an
    empty record if there is no such table. The base implementation returns
    an empty record.
*/
QSqlRecord QSqlDriver::record(const QString &tableName) const {
    static_cast<void>(tableName);
    return QSqlRecord();
}

/*!
    Returns the value of \a field written as a literal that can be put into
    an SQL statement. A null value gives \c NULL; text is put between
    single quotes, with the quotes inside it doubled, and loses the spaces
    at its end if \a trimStrings is \c true; a date, a time or a date and
    time is written in ISO 8601 between quotes; a boolean is written as 1 or
    0; a byte array is written as a hexadecimal \c{X'...'} literal.
*/
QString QSqlDriver::formatValue(const QSqlField &field, bool trimStrings) const {
    const QVariant value = field.value();
    if (value.isNull())
        return QStringLiteral("NULL");
    const auto quoted = [](QString text) {
        text.replace(QStringLiteral("'"), QStringLiteral("''"));
        return QChar('\'') + text + QChar('\'');
    };
    switch (value.typeId()) {
    case QMetaType::Bool:
        return value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Double:
    case QMetaType::Float:
        return value.toString();
    case QMetaType::QDate:
        return value.toDate().isValid() ? quoted(value.toDate().toString(Qt::ISODate)) : QStringLiteral("NULL");
    case QMetaType::QTime:
        return value.toTime().isValid() ? quoted(value.toTime().toString(Qt::ISODateWithMs)) : QStringLiteral("NULL");
    case QMetaType::QDateTime:
        return value.toDateTime().isValid() ? quoted(value.toDateTime().toString(Qt::ISODateWithMs))
                                            : QStringLiteral("NULL");
    case QMetaType::QByteArray:
        return QStringLiteral("X'") + QString::fromLatin1(value.toByteArray().toHex()) + QChar('\'');
    default: {
        QString text = value.toString();
        if (trimStrings) {
            qsizetype end = text.size();
            while (end > 0 && text.at(end - 1).isSpace())
                --end;
            text.truncate(end);
        }
        return quoted(text);
    }
    }
}

/*!
    Returns \a identifier quoted the way the database wants the name of a
    field or a table, as \a type tells, to be quoted. The base
    implementation returns \a identifier unchanged.

    \sa isIdentifierEscaped(), stripDelimiters()
*/
QString QSqlDriver::escapeIdentifier(const QString &identifier, IdentifierType type) const {
    static_cast<void>(type);
    return identifier;
}

/*!
    Returns an SQL statement of the kind \a type for the table \a tableName
    and the generated fields of \a rec. With \a preparedStatement the values
    are written as \c{?} placeholders, to be bound in the order of the
    fields; without it they are written as literals with formatValue(). A
    \c WHERE clause compares a null value with \c{IS NULL} in both forms, so
    a null value has no placeholder. Returns an empty string if there is
    nothing to write.

    \sa formatValue(), escapeIdentifier()
*/
QString QSqlDriver::sqlStatement(StatementType type, const QString &tableName, const QSqlRecord &rec,
                                 bool preparedStatement) const {
    const auto fieldName = [this, &rec](int i) {
        const QString name = rec.fieldName(i);
        return isIdentifierEscaped(name, FieldName) ? name : escapeIdentifier(name, FieldName);
    };
    const QString table =
        isIdentifierEscaped(tableName, TableName) ? tableName : escapeIdentifier(tableName, TableName);
    QString s;
    switch (type) {
    case SelectStatement:
        for (int i = 0; i < rec.count(); ++i) {
            if (rec.isGenerated(i))
                s += (s.isEmpty() ? QString() : QStringLiteral(", ")) + fieldName(i);
        }
        if (s.isEmpty())
            return s;
        return QStringLiteral("SELECT ") + s + QStringLiteral(" FROM ") + table;
    case WhereStatement:
        for (int i = 0; i < rec.count(); ++i) {
            if (!rec.isGenerated(i))
                continue;
            s += s.isEmpty() ? QStringLiteral("WHERE ") : QStringLiteral(" AND ");
            s += fieldName(i);
            if (rec.isNull(i))
                s += QStringLiteral(" IS NULL");
            else if (preparedStatement)
                s += QStringLiteral(" = ?");
            else
                s += QStringLiteral(" = ") + formatValue(rec.field(i));
        }
        return s;
    case UpdateStatement:
        for (int i = 0; i < rec.count(); ++i) {
            if (!rec.isGenerated(i))
                continue;
            s += (s.isEmpty() ? QString() : QStringLiteral(", ")) + fieldName(i) + QStringLiteral(" = ");
            s += preparedStatement ? QStringLiteral("?") : formatValue(rec.field(i));
        }
        if (s.isEmpty())
            return s;
        return QStringLiteral("UPDATE ") + table + QStringLiteral(" SET ") + s;
    case DeleteStatement:
        return QStringLiteral("DELETE FROM ") + table;
    case InsertStatement: {
        QString values;
        for (int i = 0; i < rec.count(); ++i) {
            if (!rec.isGenerated(i))
                continue;
            if (!s.isEmpty()) {
                s += QStringLiteral(", ");
                values += QStringLiteral(", ");
            }
            s += fieldName(i);
            values += preparedStatement ? QStringLiteral("?") : formatValue(rec.field(i));
        }
        if (s.isEmpty())
            return s;
        return QStringLiteral("INSERT INTO ") + table + QStringLiteral(" (") + s + QStringLiteral(") VALUES (") +
               values + QChar(')');
    }
    }
    return s;
}

/*!
    Returns the last error of the connection.

    \sa setLastError()
*/
QSqlError QSqlDriver::lastError() const { return m_error; }

/*!
    Returns the handle of the connection in the client library of the
    database, for code that calls the library directly. The base
    implementation returns an invalid QVariant.
*/
QVariant QSqlDriver::handle() const { return QVariant(); }

/*!
    Subscribes to the notification \a name of the database and returns
    \c true on success. The base implementation returns \c false.

    \sa notification(), unsubscribeFromNotification()
*/
bool QSqlDriver::subscribeToNotification(const QString &name) {
    static_cast<void>(name);
    return false;
}

/*!
    Ends the subscription to the notification \a name and returns \c true
    on success. The base implementation returns \c false.

    \sa subscribeToNotification()
*/
bool QSqlDriver::unsubscribeFromNotification(const QString &name) {
    static_cast<void>(name);
    return false;
}

/*!
    Returns the names of the notifications that the driver is subscribed
    to. The base implementation returns an empty list.

    \sa subscribeToNotification()
*/
QStringList QSqlDriver::subscribedToNotifications() const { return QStringList(); }

/*!
    Returns \c true if \a identifier is quoted the way escapeIdentifier()
    quotes a name of the kind \a type; otherwise returns \c false. The base
    implementation tests for a name between double quotes.

    \sa stripDelimiters()
*/
bool QSqlDriver::isIdentifierEscaped(const QString &identifier, IdentifierType type) const {
    static_cast<void>(type);
    return identifier.size() > 2 && identifier.startsWith(QChar('"')) && identifier.endsWith(QChar('"'));
}

/*!
    Returns \a identifier without the quotes around it, if it is quoted as
    a name of the kind \a type; otherwise returns it unchanged.

    \sa isIdentifierEscaped()
*/
QString QSqlDriver::stripDelimiters(const QString &identifier, IdentifierType type) const {
    if (!isIdentifierEscaped(identifier, type))
        return identifier;
    return identifier.mid(1, identifier.size() - 2);
}

/*!
    Returns the largest length that the database allows for a name of the
    kind \a type. The base implementation returns the largest \c int.
*/
int QSqlDriver::maximumIdentifierLength(IdentifierType type) const {
    static_cast<void>(type);
    return std::numeric_limits<int>::max();
}

/*!
    Sets how the results created by this driver return numbers of high
    precision to \a precisionPolicy.

    \sa numericalPrecisionPolicy()
*/
void QSqlDriver::setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy precisionPolicy) {
    m_policy = precisionPolicy;
}

/*!
    Returns how the results created by this driver return numbers of high
    precision.

    \sa setNumericalPrecisionPolicy()
*/
QSql::NumericalPrecisionPolicy QSqlDriver::numericalPrecisionPolicy() const { return m_policy; }

/*!
    Returns the database system that the driver is connected to.

    \sa setDbmsType()
*/
QSqlDriver::DbmsType QSqlDriver::dbmsType() const { return m_dbms; }

/*!
    Tries to cancel the query that is running and returns \c true on
    success. The base implementation returns \c false.
*/
bool QSqlDriver::cancelQuery() { return false; }

/*!
    Sets whether the connection is open to \a open. Called by subclasses.

    \sa isOpen()
*/
void QSqlDriver::setOpen(bool open) { m_open = open; }

/*!
    Sets whether opening the connection failed to \a error. An error also
    marks the connection as closed. Called by subclasses.

    \sa isOpenError()
*/
void QSqlDriver::setOpenError(bool error) {
    m_openError = error;
    if (error)
        m_open = false;
}

/*!
    Sets the last error of the connection to \a error. Called by
    subclasses.

    \sa lastError()
*/
void QSqlDriver::setLastError(const QSqlError &error) { m_error = error; }

/*!
    Sets the database system that the driver is connected to, to \a type.
    Called by subclasses.

    \sa dbmsType()
*/
void QSqlDriver::setDbmsType(DbmsType type) { m_dbms = type; }

#include "moc_qsqldriver.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
