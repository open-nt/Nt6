// MIT License - see LICENSE
//
// Overview page about Nt SQL for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-sql.html
    \title Using Nt SQL
    \brief How to connect to an SQL database, run statements with bound values, and show tables in models.

    Nt SQL gives access to SQL databases: QSqlDatabase stands for a connection, QSqlQuery runs
    statements and reads their rows, and QSqlQueryModel and QSqlTableModel show them as item models. The
    library is \c Nt::Sql and needs only Nt Core:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(contacts main.cpp)
    target_link_libraries(contacts PRIVATE Nt::Sql)
    \endcode

    \section1 Drivers

    A driver connects Nt SQL to one kind of database. Nt comes with one, \c QSQLITE, for SQLite: a database
    in a single file, or in memory, with nothing to install or to run. It uses the SQLite library of the
    system and is there if Nt was built with it (the CMake option \c NT_USE_SQLITE, which is on by default);
    QSqlDatabase::drivers() tells at run time.

    There are no driver plugins. A driver for another database is a subclass of QSqlDriver and QSqlResult
    in the application, made known with QSqlDatabase::registerSqlDriver().

    \section1 Connecting

    A connection is added once and then found by name from anywhere; the one without a name is the default
    connection, which queries and models use when they are given none:

    \snippet sql/contacts.cpp open

    For \c QSQLITE the database name is the path of the file, which is created if it does not exist,
    \c{:memory:} for a database that lives in memory and is gone when the connection is closed, or an empty
    name for a temporary one. QSqlDatabase::setConnectOptions() takes these options:

    \list
    \li \c QSQLITE_BUSY_TIMEOUT: how long, in milliseconds, to wait for a lock of another connection;
        5000 by default.
    \li \c QSQLITE_OPEN_READONLY: open the file for reading only; it has to exist.
    \li \c QSQLITE_OPEN_URI: the database name is a \c{file:} URI.
    \li \c QSQLITE_OPEN_NOFOLLOW: refuse a database name that is a symbolic link.
    \li \c QSQLITE_ENABLE_SHARED_CACHE: share the cache with the other connections of the process.
    \li \c QSQLITE_ENABLE_REGEXP: make the SQL operator \c REGEXP work, with the patterns of
        QRegularExpression. A number after it, as in \c{QSQLITE_ENABLE_REGEXP=50}, is how many compiled
        patterns are kept; 25 by default.
    \li \c QSQLITE_NO_USE_EXTENDED_RESULT_CODES: report the plain result codes of SQLite in
        QSqlError::nativeErrorCode(), 19 for any constraint and not 2067 for a unique one.
    \endlist

    A connection, with its queries, is for one thread at a time. A thread that needs the database gets its
    own connection, with QSqlDatabase::cloneDatabase() for example.

    \section1 Running Statements

    QSqlQuery::exec() runs a statement. A statement that takes values is prepared with placeholders, and
    the values are bound to them:

    \snippet sql/contacts.cpp insert

    Bind values, do not paste them into the text of the statement: a bound value is data whatever
    characters it has, so a name with a quote in it, or one written to break the statement, does no harm.
    Placeholders have names, as above, or are written \c{?} and bound in order:

    \snippet sql/contacts.cpp select

    After exec() a \c SELECT is positioned before its first row. QSqlQuery::next() moves through the rows
    and QSqlQuery::value() reads a field of the current one, as a QVariant of the type that the database
    stored: with SQLite a 64-bit integer, a \c double, a QString or a QByteArray, or a null QVariant. A date
    is stored as ISO 8601 text, which QVariant::toDate() and QVariant::toDateTime() read back.

    When something fails, the function returns \c false and QSqlQuery::lastError(), or
    QSqlDatabase::lastError() for the connection, tells why.

    \section1 Transactions

    Several statements become one change, which is written completely or not at all, between
    QSqlDatabase::transaction() and QSqlDatabase::commit(); QSqlDatabase::rollback() gives it up. With
    SQLite a transaction also makes many inserts much faster, as the file is synchronized once and not for
    each row:

    \code
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QSqlQuery query;
    query.prepare("INSERT INTO samples (at, value) VALUES (?, ?)");
    for (const Sample &sample : samples) {
        query.addBindValue(sample.at);
        query.addBindValue(sample.value);
        query.exec();
    }
    db.commit();
    \endcode

    \section1 Models

    QSqlQueryModel shows the rows of any \c SELECT, read-only. QSqlTableModel works on one table and writes
    changes back: items that are set, rows that are inserted and rows that are removed, at once or when
    QSqlTableModel::submitAll() is called, as its edit strategy says:

    \snippet sql/contacts.cpp model

    Both are item models like any other, so they fit the views of Nt Widgets and the proxy models of Nt
    Core, such as QSortFilterProxyModel. With SQLite they read their rows in steps of 255, as a view asks
    for them; see QSqlQueryModel::fetchMore().

    QSqlRelationalTableModel adds foreign keys to that: a column that holds the key of a row in another
    table shows a value from that row, and is changed by giving it another key:

    \code
    QSqlRelationalTableModel model;
    model.setTable("employee");
    model.setRelation(model.fieldIndex("city"), QSqlRelation("city", "id", "name"));
    model.select();
    \endcode

    With the edit strategies that write at once, a row that was written is read again by itself, so the
    model is not reset and a view keeps its selection; QSqlTableModel::selectRow() does the same on
    request.

    \section1 Notifications and Handles

    The \c QSQLITE driver tells when a table changes through its own connection. Subscribe to the name of
    the table with QSqlDriver::subscribeToNotification(); QSqlDriver::notification() is then emitted from
    the event loop for each row that is inserted, updated or deleted, with the row id as its payload.
    Changes that another connection or another process makes are not seen.

    QSqlDriver::handle() and QSqlResult::handle() give the \c{sqlite3 *} of a connection and the
    \c{sqlite3_stmt *} of a prepared statement, for code that calls SQLite directly:

    \code
    Q_DECLARE_METATYPE(sqlite3 *)

    sqlite3 *handle = db.driver()->handle().value<sqlite3 *>();
    \endcode

    \section1 What Is Missing

    Drivers for other databases than SQLite, driver plugins, \c QSqlRelationalDelegate, and the
    deprecated \c QSqlDatabase::exec(). The class pages list what differs from Qt in each class.

    \sa QSqlDatabase, QSqlQuery, QSqlTableModel, QSqlRelationalTableModel
*/
