// MIT License - see LICENSE
#include <QtSql/qsqldatabase.h>

#include <QtCore/qdebug.h>
#include <QtCore/qhash.h>
#include <QtCore/qmap.h>
#include <QtSql/qsqlindex.h>
#include <QtSql/qsqlrecord.h>
#include <QtSql/qsqlresult.h>

#include "qsqlnulldriver_p.h"
#ifdef NT_HAVE_SQLITE
#include "qsqlitedriver_p.h"
#endif

#include <cstdio>
#include <mutex>

/*!
    \module NtSql
    \title Nt SQL C++ Classes
    \brief Classes for working with SQL databases.
    \cmaketarget Nt::Sql

    Nt SQL connects to SQL databases, runs statements with bound values and
    shows tables as item models. It comes with a driver for SQLite, and
    takes drivers of the application for other databases.

    See \l{Using Nt SQL} for a walk through, and
    \c{examples/sql/contacts.cpp} for a complete program.
*/

/*!
    \class QSqlDatabase
    \brief The QSqlDatabase class handles a connection to a database.

    A QSqlDatabase is a handle to a connection. Connections are kept in a
    list under their names: addDatabase() makes one with the driver for a
    kind of database, database() returns it from anywhere in the program,
    and removeDatabase() takes it out of the list again. A connection that
    is added without a name is the default connection, which QSqlQuery and
    the models use unless they are given another one.

    \code
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("contacts.db");
    if (!db.open())
        qFatal("%s", qPrintable(db.lastError().text()));
    \endcode

    The handle is a value: copies of it stand for the same connection.
    Remove a connection only when its handles and queries are gone:

    \code
    {
        QSqlDatabase db = QSqlDatabase::database("import");
        QSqlQuery query("SELECT name FROM people", db);
        // ...
    }
    QSqlDatabase::removeDatabase("import");
    \endcode

    The name given to setDatabaseName() means what the driver makes of it.
    For \c QSQLITE it is the path of the database file, \c{:memory:} for a
    database that lives in memory, or an empty name for a temporary
    database.

    \section1 Threads

    The list of connections can be used from any thread. A connection
    itself, with its queries, belongs to the thread that uses it: do not
    use one connection from two threads at the same time. Give each thread
    its own connection, with cloneDatabase() for example.

    \differences
    The only driver that comes with Nt is \c QSQLITE, and it is there if Nt
    was built with SQLite; drivers() tells. There are no driver plugins:
    other drivers are added with registerSqlDriver() or passed to
    addDatabase(). There is no \c exec(), which is deprecated in Qt 6: use
    QSqlQuery. \c thread() and \c moveToThread() are missing, and the thread
    that uses a connection is not checked. The \c QSQLITE driver knows the
    connection options \c QSQLITE_BUSY_TIMEOUT, \c QSQLITE_OPEN_READONLY,
    \c QSQLITE_OPEN_URI, \c QSQLITE_ENABLE_SHARED_CACHE,
    \c QSQLITE_OPEN_NOFOLLOW, \c QSQLITE_NO_USE_EXTENDED_RESULT_CODES and
    \c QSQLITE_ENABLE_REGEXP, whose regular expressions are those of
    QRegularExpression; it reports extended result codes unless told not to.
    \enddifferences

    \sa QSqlQuery, QSqlDriver, {Using Nt SQL}
*/

class QSqlDatabasePrivate {
public:
    ~QSqlDatabasePrivate() {
        if (driver) {
            if (driver->isOpen())
                driver->close();
            delete driver;
        }
    }

    QSqlDriver *driver = nullptr; // owned; never null for a handle that came from addDatabase()
    QString driverName;
    QString connectionName;
    QString databaseName;
    QString userName;
    QString password;
    QString hostName;
    QString options;
    int port = -1;
    QSql::NumericalPrecisionPolicy policy = QSql::LowPrecisionDouble;

    struct Registry {
        std::mutex mutex;
        QMap<QString, QSqlDatabase> connections;
        QMap<QString, std::shared_ptr<QSqlDriverCreatorBase>> creators;
    };
    static Registry &registry() {
        static Registry *r = new Registry; // never destroyed: connections may be used while the program exits
        return *r;
    }
    static QSqlDriver *sharedNullDriver() {
        static QSqlDriver *driver = new QSqlNullDriver;
        return driver;
    }
    static QSqlDriver *createDriver(const QString &type) {
        std::shared_ptr<QSqlDriverCreatorBase> creator;
        {
            Registry &r = registry();
            const std::lock_guard<std::mutex> lock(r.mutex);
            creator = r.creators.value(type);
        }
        if (creator)
            return creator->createObject();
#ifdef NT_HAVE_SQLITE
        if (type == QStringLiteral("QSQLITE"))
            return new QSQLiteDriver;
#endif
        return nullptr;
    }
    static QSqlDatabase make(QSqlDriver *driver, const QString &type, const QString &connectionName) {
        QSqlDatabase db;
        db.d = std::make_shared<QSqlDatabasePrivate>();
        db.d->driver = driver ? driver : new QSqlNullDriver;
        db.d->driverName = type;
        db.d->connectionName = connectionName;
        return db;
    }
    // Puts db into the list, in the place of a connection of the same name.
    static void add(const QSqlDatabase &db, const QString &connectionName) {
        QSqlDatabase old;
        {
            Registry &r = registry();
            const std::lock_guard<std::mutex> lock(r.mutex);
            old = r.connections.value(connectionName);
            r.connections.insert(connectionName, db);
        }
        if (old.d) {
            std::fprintf(stderr,
                         "QSqlDatabasePrivate::addDatabase: duplicate connection name '%s', old connection removed.\n",
                         connectionName.toUtf8().constData());
            invalidate(old);
        }
    }
    // Closes a connection that left the list. Handles and queries that are still around stop working.
    static void invalidate(QSqlDatabase &db) {
        if (db.d.use_count() > 1) {
            std::fprintf(stderr,
                         "QSqlDatabasePrivate::removeDatabase: connection '%s' is still in use, all queries will "
                         "cease to work.\n",
                         db.d->connectionName.toUtf8().constData());
        }
        if (db.d->driver->isOpen())
            db.d->driver->close();
        db.d->connectionName.clear();
    }
};

/*!
    \variable QSqlDatabase::defaultConnection

    The name of the default connection: the one that addDatabase() makes
    and database() returns when they are called without a name.
*/
const char *const QSqlDatabase::defaultConnection = "qt_sql_default_connection";

/*!
    Constructs an invalid handle, which stands for no connection. Use
    addDatabase() or database() to get a valid one.

    \sa isValid()
*/
QSqlDatabase::QSqlDatabase() = default;

/*!
    Constructs a handle to the connection of \a other.
*/
QSqlDatabase::QSqlDatabase(const QSqlDatabase &other) = default;

/*!
    Makes this a handle to the connection of \a other and returns a
    reference to it.
*/
QSqlDatabase &QSqlDatabase::operator=(const QSqlDatabase &other) = default;

/*!
    Destroys the handle. The connection is closed when its last handle is
    destroyed, which for a connection in the list is the one that
    removeDatabase() drops.
*/
QSqlDatabase::~QSqlDatabase() = default;

/*!
    Opens the connection with the database name, user name, password, host
    name, port and options that were set, and returns \c true on success.
    A connection that is open is closed first. On failure, lastError()
    tells why.

    \sa close(), isOpen(), lastError()
*/
bool QSqlDatabase::open() {
    if (!d)
        return false;
    if (d->driver->isOpen())
        d->driver->close();
    d->driver->setNumericalPrecisionPolicy(d->policy);
    return d->driver->open(d->databaseName, d->userName, d->password, d->hostName, d->port, d->options);
}

/*!
    \overload
    Opens the connection for the user \a user with the password
    \a password. The user name is kept as with setUserName(); the password
    is not kept.
*/
bool QSqlDatabase::open(const QString &user, const QString &password) {
    if (!d)
        return false;
    setUserName(user);
    if (d->driver->isOpen())
        d->driver->close();
    d->driver->setNumericalPrecisionPolicy(d->policy);
    return d->driver->open(d->databaseName, user, password, d->hostName, d->port, d->options);
}

/*!
    Closes the connection. The queries on it become inactive and release
    what they hold of the database.

    \sa open()
*/
void QSqlDatabase::close() {
    if (d)
        d->driver->close();
}

/*!
    Returns \c true if the connection is open; otherwise returns \c false.

    \sa open()
*/
bool QSqlDatabase::isOpen() const { return d && d->driver->isOpen(); }

/*!
    Returns \c true if the last attempt to open the connection failed;
    otherwise returns \c false.

    \sa lastError()
*/
bool QSqlDatabase::isOpenError() const { return d && d->driver->isOpenError(); }

/*!
    Returns the names of the tables of the kinds \a type in the database.
    The connection has to be open.

    \sa record(), primaryIndex()
*/
QStringList QSqlDatabase::tables(QSql::TableType type) const { return d ? d->driver->tables(type) : QStringList(); }

/*!
    Returns the primary key of the table \a tablename, or an empty index if
    it has none.

    \sa tables(), record()
*/
QSqlIndex QSqlDatabase::primaryIndex(const QString &tablename) const {
    return d ? d->driver->primaryIndex(tablename) : QSqlIndex();
}

/*!
    Returns the fields of the table or view \a tablename, in the order of
    its columns and without values, or an empty record if there is no such
    table.

    \sa tables(), primaryIndex()
*/
QSqlRecord QSqlDatabase::record(const QString &tablename) const {
    return d ? d->driver->record(tablename) : QSqlRecord();
}

/*!
    Returns the last error of the connection. Errors of a statement are
    found in QSqlQuery::lastError().
*/
QSqlError QSqlDatabase::lastError() const { return d ? d->driver->lastError() : QSqlError(); }

/*!
    Returns \c true if the handle stands for a connection that has a
    driver; otherwise returns \c false. A handle from addDatabase() with a
    kind of database that has no driver is not valid.
*/
bool QSqlDatabase::isValid() const { return d && dynamic_cast<QSqlNullDriver *>(d->driver) == nullptr; }

/*!
    Starts a transaction and returns \c true on success. Returns \c false
    if the driver has no transactions.

    \sa commit(), rollback(), QSqlDriver::hasFeature()
*/
bool QSqlDatabase::transaction() {
    return d && d->driver->hasFeature(QSqlDriver::Transactions) && d->driver->beginTransaction();
}

/*!
    Commits the running transaction and returns \c true on success. With
    some databases this fails while a \c SELECT statement is active: finish
    the query first.

    \sa transaction(), rollback(), QSqlQuery::finish()
*/
bool QSqlDatabase::commit() {
    return d && d->driver->hasFeature(QSqlDriver::Transactions) && d->driver->commitTransaction();
}

/*!
    Rolls the running transaction back and returns \c true on success.

    \sa transaction(), commit()
*/
bool QSqlDatabase::rollback() {
    return d && d->driver->hasFeature(QSqlDriver::Transactions) && d->driver->rollbackTransaction();
}

/*!
    Sets the name of the database to \a name. This has to happen before
    the connection is opened; what the name means depends on the driver.

    \sa databaseName(), open()
*/
void QSqlDatabase::setDatabaseName(const QString &name) {
    if (d)
        d->databaseName = name;
}

/*!
    Sets the name of the user to \a name, for the next open().

    \sa userName()
*/
void QSqlDatabase::setUserName(const QString &name) {
    if (d)
        d->userName = name;
}

/*!
    Sets the password to \a password, for the next open(). The password is
    kept in memory as plain text; use open() with a password to avoid that.

    \sa password()
*/
void QSqlDatabase::setPassword(const QString &password) {
    if (d)
        d->password = password;
}

/*!
    Sets the name of the host of the database to \a host, for the next
    open().

    \sa hostName()
*/
void QSqlDatabase::setHostName(const QString &host) {
    if (d)
        d->hostName = host;
}

/*!
    Sets the port of the database to \a p, for the next open().

    \sa port()
*/
void QSqlDatabase::setPort(int p) {
    if (d)
        d->port = p;
}

/*!
    Sets the options of the connection to \a options, for the next open():
    a list of names, or names with values, separated by semicolons, as in
    \c{"QSQLITE_BUSY_TIMEOUT=2000;QSQLITE_OPEN_READONLY"}. The names are
    those of the driver.

    \sa connectOptions()
*/
void QSqlDatabase::setConnectOptions(const QString &options) {
    if (d)
        d->options = options;
}

/*!
    Returns the name of the database.

    \sa setDatabaseName()
*/
QString QSqlDatabase::databaseName() const { return d ? d->databaseName : QString(); }

/*!
    Returns the name of the user.

    \sa setUserName()
*/
QString QSqlDatabase::userName() const { return d ? d->userName : QString(); }

/*!
    Returns the password that was set with setPassword().

    \sa setPassword()
*/
QString QSqlDatabase::password() const { return d ? d->password : QString(); }

/*!
    Returns the name of the host of the database.

    \sa setHostName()
*/
QString QSqlDatabase::hostName() const { return d ? d->hostName : QString(); }

/*!
    Returns the name of the driver of the connection, as it was given to
    addDatabase(). It is empty for a connection that was added with a
    driver object.
*/
QString QSqlDatabase::driverName() const { return d ? d->driverName : QString(); }

/*!
    Returns the port of the database, or -1 if none was set.

    \sa setPort()
*/
int QSqlDatabase::port() const { return d ? d->port : -1; }

/*!
    Returns the options of the connection.

    \sa setConnectOptions()
*/
QString QSqlDatabase::connectOptions() const { return d ? d->options : QString(); }

/*!
    Returns the name of the connection in the list of connections. It is
    empty for an invalid handle and for a connection that was removed.

    \sa addDatabase()
*/
QString QSqlDatabase::connectionName() const { return d ? d->connectionName : QString(); }

/*!
    Sets how the queries that are made on this connection from now on
    return numbers of high precision to \a precisionPolicy.

    \sa numericalPrecisionPolicy(), QSqlQuery::setNumericalPrecisionPolicy()
*/
void QSqlDatabase::setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy precisionPolicy) {
    if (!d)
        return;
    d->policy = precisionPolicy;
    d->driver->setNumericalPrecisionPolicy(precisionPolicy);
}

/*!
    Returns how queries on this connection return numbers of high
    precision.

    \sa setNumericalPrecisionPolicy()
*/
QSql::NumericalPrecisionPolicy QSqlDatabase::numericalPrecisionPolicy() const {
    return d ? d->policy : QSql::LowPrecisionDouble;
}

/*!
    Returns the driver of the connection. It is never \c nullptr: a handle
    without a connection has a driver that does nothing.
*/
QSqlDriver *QSqlDatabase::driver() const { return d ? d->driver : QSqlDatabasePrivate::sharedNullDriver(); }

/*!
    Adds a connection called \a connectionName to the list, with the driver
    for the kind of database \a type, and returns a handle to it. Without a
    name, it becomes the default connection. A connection of the same name
    that is in the list already is removed first.

    If there is no driver for \a type, a warning is printed and the handle
    that is returned is not valid. The connection is closed: set the name
    of the database and call open().

    \sa database(), removeDatabase(), drivers()
*/
QSqlDatabase QSqlDatabase::addDatabase(const QString &type, const QString &connectionName) {
    QSqlDriver *driver = QSqlDatabasePrivate::createDriver(type);
    if (!driver) {
        std::fprintf(stderr, "QSqlDatabase: %s driver not loaded\nQSqlDatabase: available drivers: %s\n",
                     type.toUtf8().constData(), drivers().join(QChar(' ')).toUtf8().constData());
    }
    QSqlDatabase db = QSqlDatabasePrivate::make(driver, type, connectionName);
    QSqlDatabasePrivate::add(db, connectionName);
    return db;
}

/*!
    \overload
    Adds a connection called \a connectionName that uses \a driver, an
    object of a driver class of the application, and returns a handle to
    it. The connection takes ownership of \a driver.
*/
QSqlDatabase QSqlDatabase::addDatabase(QSqlDriver *driver, const QString &connectionName) {
    QSqlDatabase db = QSqlDatabasePrivate::make(driver, QString(), connectionName);
    QSqlDatabasePrivate::add(db, connectionName);
    return db;
}

/*!
    Adds a connection called \a connectionName that is set up like the
    connection \a other: with a new driver of the same kind and the same
    database name, user name, password, host name, port and options.
    Returns a handle to it, which is closed. Returns an invalid handle if
    \a other is not valid.

    This gives another thread its own connection to the same database.

    \sa addDatabase()
*/
QSqlDatabase QSqlDatabase::cloneDatabase(const QSqlDatabase &other, const QString &connectionName) {
    if (!other.isValid())
        return QSqlDatabase();
    QSqlDriver *driver = QSqlDatabasePrivate::createDriver(other.d->driverName);
    if (!driver)
        return QSqlDatabase(); // the connection was added with a driver object, which cannot be made again
    QSqlDatabase db = QSqlDatabasePrivate::make(driver, other.d->driverName, connectionName);
    db.d->databaseName = other.d->databaseName;
    db.d->userName = other.d->userName;
    db.d->password = other.d->password;
    db.d->hostName = other.d->hostName;
    db.d->options = other.d->options;
    db.d->port = other.d->port;
    db.d->policy = other.d->policy;
    QSqlDatabasePrivate::add(db, connectionName);
    return db;
}

/*!
    \overload
    Clones the connection that is called \a other in the list, as
    \a connectionName. Unlike database(), this does not open \a other, so
    it may be called from another thread than the one that uses \a other.
*/
QSqlDatabase QSqlDatabase::cloneDatabase(const QString &other, const QString &connectionName) {
    return cloneDatabase(database(other, false), connectionName);
}

/*!
    Returns a handle to the connection called \a connectionName, or to the
    default connection. If \a open is \c true and the connection is closed,
    it is opened; lastError() of the handle tells if that failed. Returns an
    invalid handle if there is no such connection.

    \sa addDatabase(), contains()
*/
QSqlDatabase QSqlDatabase::database(const QString &connectionName, bool open) {
    QSqlDatabase db;
    {
        QSqlDatabasePrivate::Registry &r = QSqlDatabasePrivate::registry();
        const std::lock_guard<std::mutex> lock(r.mutex);
        db = r.connections.value(connectionName);
    }
    if (open && db.isValid() && !db.isOpen()) {
        if (!db.open()) {
            std::fprintf(stderr, "QSqlDatabasePrivate::database: unable to open database: %s\n",
                         db.lastError().text().toUtf8().constData());
        }
    }
    return db;
}

/*!
    Removes the connection called \a connectionName from the list and
    closes it. Nothing happens if there is no such connection.

    If handles or queries of the connection are still around, a warning is
    printed and they stop working. Let them go out of scope first.

    \sa addDatabase(), database()
*/
void QSqlDatabase::removeDatabase(const QString &connectionName) {
    QSqlDatabase db;
    {
        QSqlDatabasePrivate::Registry &r = QSqlDatabasePrivate::registry();
        const std::lock_guard<std::mutex> lock(r.mutex);
        db = r.connections.take(connectionName);
    }
    if (db.d)
        QSqlDatabasePrivate::invalidate(db);
}

/*!
    Returns \c true if the list has a connection called \a connectionName,
    or a default connection; otherwise returns \c false.

    \sa connectionNames()
*/
bool QSqlDatabase::contains(const QString &connectionName) {
    QSqlDatabasePrivate::Registry &r = QSqlDatabasePrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    return r.connections.contains(connectionName);
}

/*!
    Returns the names of the kinds of databases that have a driver: the
    drivers that Nt was built with and those added with
    registerSqlDriver().

    \sa isDriverAvailable()
*/
QStringList QSqlDatabase::drivers() {
    QStringList list;
#ifdef NT_HAVE_SQLITE
    list.append(QStringLiteral("QSQLITE"));
#endif
    QSqlDatabasePrivate::Registry &r = QSqlDatabasePrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    for (const QString &name : r.creators.keys()) {
        if (!list.contains(name))
            list.append(name);
    }
    return list;
}

/*!
    Returns the names of the connections in the list.

    \sa contains(), database()
*/
QStringList QSqlDatabase::connectionNames() {
    QSqlDatabasePrivate::Registry &r = QSqlDatabasePrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    return r.connections.keys();
}

/*!
    Makes a driver of the application available under \a name, so that
    addDatabase() can be called with it. \a creator makes the driver
    objects; the list of drivers takes ownership of it. A driver that is
    registered under the name of one that is there already, a built-in one
    included, takes its place.

    \sa QSqlDriverCreator, drivers()
*/
void QSqlDatabase::registerSqlDriver(const QString &name, QSqlDriverCreatorBase *creator) {
    std::shared_ptr<QSqlDriverCreatorBase> owned(creator);
    QSqlDatabasePrivate::Registry &r = QSqlDatabasePrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    if (owned)
        r.creators.insert(name, owned);
    else
        r.creators.remove(name);
}

/*!
    Returns \c true if there is a driver for the kind of database \a name;
    otherwise returns \c false.

    \sa drivers()
*/
bool QSqlDatabase::isDriverAvailable(const QString &name) { return drivers().contains(name); }

QDebug operator<<(QDebug dbg, const QSqlDatabase &db) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    if (!db.isValid()) {
        dbg << "QSqlDatabase(invalid)";
        return dbg;
    }
    dbg << "QSqlDatabase(driver=\"" << db.driverName().toUtf8().constData() << "\", database=\""
        << db.databaseName().toUtf8().constData() << "\", host=\"" << db.hostName().toUtf8().constData()
        << "\", port=" << db.port() << ", user=\"" << db.userName().toUtf8().constData() << "\", open=" << db.isOpen()
        << ")";
    return dbg;
}
