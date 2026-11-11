// MIT License - see LICENSE
#pragma once

#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlrecord.h>
#include <QtSql/qsqlresult.h>

#include <memory>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;
class QSQLiteResult;

// The handles that QSqlDriver::handle() and QSqlResult::handle() return. An application that wants them
// declares the same two types, and gets the pointers out with QVariant::value().
Q_DECLARE_METATYPE(sqlite3 *)
Q_DECLARE_METATYPE(sqlite3_stmt *)

// What a driver and its results share: the database handle, and the results that have statements on it. A
// result can outlive the driver object, so the connection is what both of them hold on to.
struct QSQLiteConnection {
    sqlite3 *db = nullptr;
    std::vector<QSQLiteResult *> results;
};

// The QSQLITE driver, on the SQLite library of the system.
class QSQLiteDriver : public QSqlDriver {
public:
    explicit QSQLiteDriver(QObject *parent = nullptr);
    ~QSQLiteDriver() override;

    bool hasFeature(DriverFeature f) const override;
    bool open(const QString &db, const QString &user, const QString &password, const QString &host, int port,
              const QString &connOpts) override;
    void close() override;
    QSqlResult *createResult() const override;
    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;
    QStringList tables(QSql::TableType type) const override;
    QSqlRecord record(const QString &tableName) const override;
    QSqlIndex primaryIndex(const QString &tableName) const override;
    QString escapeIdentifier(const QString &identifier, IdentifierType type) const override;
    bool isIdentifierEscaped(const QString &identifier, IdentifierType type) const override;
    QString stripDelimiters(const QString &identifier, IdentifierType type) const override;
    // The sqlite3 * of the connection, or an invalid QVariant if it is closed.
    QVariant handle() const override;
    // A notification has the name of a table, and is delivered when a row of it is inserted, updated or
    // deleted through this connection, with the row id as its payload.
    bool subscribeToNotification(const QString &name) override;
    bool unsubscribeFromNotification(const QString &name) override;
    QStringList subscribedToNotifications() const override;

private:
    static void updateHook(void *driver, int operation, const char *database, const char *table, long long rowId);
    bool run(const char *sql, const QString &what);
    QSqlIndex tableInfo(const QString &tableName, bool onlyPrimaryKey) const;

    std::shared_ptr<QSQLiteConnection> m_connection;
    QStringList m_notifications;
};

class QSQLiteResult : public QSqlResult {
public:
    QSQLiteResult(const QSQLiteDriver *driver, std::shared_ptr<QSQLiteConnection> connection);
    ~QSQLiteResult() override;

    // Releases the statement: called when the connection is closed under the result.
    void finalize();
    // The sqlite3_stmt * of the prepared statement, or an invalid QVariant if there is none.
    QVariant handle() const override;

protected:
    QVariant data(int i) override;
    bool isNull(int i) override;
    bool reset(const QString &query) override;
    bool prepare(const QString &query) override;
    bool exec() override;
    bool fetch(int i) override;
    bool fetchNext() override;
    bool fetchFirst() override;
    bool fetchLast() override;
    int size() override;
    int numRowsAffected() override;
    QSqlRecord record() const override;
    QVariant lastInsertId() const override;
    void detachFromResultSet() override;

private:
    enum class Step { Row, Done, Error };
    Step step();
    bool fetchUpTo(int row);
    void readRow();
    void readColumns();
    void fail(const QString &what, QSqlError::ErrorType type);

    std::shared_ptr<QSQLiteConnection> m_connection;
    sqlite3_stmt *m_stmt = nullptr;
    QSqlRecord m_record;
    std::vector<QVariantList> m_rows; // the rows read so far; only the current one if forward-only
    std::vector<QByteArray> m_bound;  // the bytes of the bound text and blobs, alive while the statement runs
    int m_firstRow = 0;               // the position of m_rows[0]
    int m_changes = 0;
    qint64 m_lastInsertId = 0;
    bool m_hasInsertId = false;
    bool m_done = true;        // no more rows to step to
    bool m_pendingRow = false; // exec() stepped to the first row, which is not read yet
};
