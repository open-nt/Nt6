// MIT License - see LICENSE
#include <QtCore>
#include <QtSql>

#include <cstdio>
#include <thread>

// The handles of the QSQLITE driver, as an application declares them to get them out of a QVariant.
struct sqlite3;
struct sqlite3_stmt;
Q_DECLARE_METATYPE(sqlite3 *)
Q_DECLARE_METATYPE(sqlite3_stmt *)

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static QString s(const char *text) { return QString::fromUtf8(text); }

// Value classes: no database needed.

static void testError() {
    QSqlError none;
    CHECK(!none.isValid() && none.type() == QSqlError::NoError && none.text().isEmpty());
    QSqlError e(s("driver"), s("database"), QSqlError::StatementError, s("19"));
    CHECK(e.isValid() && e.type() == QSqlError::StatementError);
    CHECK(e.driverText() == s("driver") && e.databaseText() == s("database") && e.nativeErrorCode() == s("19"));
    CHECK(e.text() == s("database driver"));
    CHECK(QSqlError(s("only"), QString(), QSqlError::UnknownError).text() == s("only"));
    CHECK(e == QSqlError(s("x"), s("y"), QSqlError::StatementError, s("19")));
    CHECK(e != QSqlError(s("driver"), s("database"), QSqlError::StatementError, s("20")));
    CHECK(e != none);
    none.swap(e);
    CHECK(none.isValid() && !e.isValid());
    CHECK(QDebug::toString(none).contains(s("database")));
}

static void testField() {
    QSqlField f(s("age"), QMetaType::fromType<int>(), s("people"));
    CHECK(f.name() == s("age") && f.tableName() == s("people") && f.metaType().id() == QMetaType::Int);
    CHECK(f.isValid() && f.isNull() && f.isGenerated() && !f.isReadOnly() && !f.isAutoValue());
    CHECK(f.requiredStatus() == QSqlField::Unknown && f.length() == -1 && f.precision() == -1);
    CHECK(!QSqlField().isValid());
    f.setValue(42);
    CHECK(!f.isNull() && f.value().toInt() == 42);
    f.setReadOnly(true);
    f.setValue(43);
    f.clear();
    CHECK(f.value().toInt() == 42 && f.isReadOnly());
    f.setReadOnly(false);
    f.clear();
    CHECK(f.isNull() && f.value().typeId() == QMetaType::Int);
    f.setName(s("years"));
    f.setTableName(s("t"));
    f.setRequired(true);
    CHECK(f.requiredStatus() == QSqlField::Required);
    f.setRequired(false);
    CHECK(f.requiredStatus() == QSqlField::Optional);
    f.setRequiredStatus(QSqlField::Unknown);
    f.setLength(10);
    f.setPrecision(2);
    f.setDefaultValue(7);
    f.setGenerated(false);
    f.setAutoValue(true);
    CHECK(f.name() == s("years") && f.tableName() == s("t") && f.length() == 10 && f.precision() == 2);
    CHECK(f.defaultValue().toInt() == 7 && !f.isGenerated() && f.isAutoValue());
    f.setMetaType(QMetaType::fromType<QString>());
    CHECK(f.metaType().id() == QMetaType::QString && f.value().typeId() == QMetaType::QString && f.isNull());
    QSqlField copy = f;
    CHECK(copy == f);
    copy.setValue(s("x"));
    CHECK(copy != f);
    copy.swap(f);
    CHECK(f.value().toString() == s("x") && copy.isNull());
    CHECK(QDebug::toString(f).contains(s("years")));
}

static void testRecord() {
    QSqlRecord r;
    CHECK(r.isEmpty() && r.count() == 0);
    r.append(QSqlField(s("id"), QMetaType::fromType<int>(), s("people")));
    r.append(QSqlField(s("Name"), QMetaType::fromType<QString>(), s("people")));
    r.insert(1, QSqlField(s("age"), QMetaType::fromType<int>()));
    CHECK(r.count() == 3 && r.fieldName(0) == s("id") && r.fieldName(1) == s("age") && r.fieldName(2) == s("Name"));
    CHECK(r.indexOf("name") == 2 && r.indexOf(s("NAME")) == 2 && r.indexOf("people.name") == 2);
    CHECK(r.indexOf("other.name") == -1 && r.indexOf("missing") == -1 && r.indexOf("people.age") == -1);
    CHECK(r.contains("ID") && !r.contains("nope"));
    CHECK(r.isNull(0) && r.isNull("name") && r.isNull(17) && r.isNull("nope"));
    r.setValue(0, 5);
    r.setValue("name", s("Ada"));
    r.setValue(17, 1);
    r.setValue("nope", 1);
    CHECK(r.value(0).toInt() == 5 && r.value("NAME").toString() == s("Ada"));
    CHECK(!r.value(17).isValid() && !r.value("nope").isValid() && r.fieldName(17).isEmpty());
    CHECK(r.field(2).name() == s("Name") && r.field("id").value().toInt() == 5 && !r.field(9).isValid());
    CHECK(r.isGenerated(0) && r.isGenerated("name") && !r.isGenerated(9));
    r.setGenerated(0, false);
    r.setGenerated("name", false);
    CHECK(!r.isGenerated("id") && !r.isGenerated(2) && r.isGenerated(1));
    r.setNull("name");
    CHECK(r.isNull(2));
    r.setValue(2, s("Bo"));
    r.setNull(2);
    CHECK(r.isNull("name") && r.value(2).typeId() == QMetaType::QString);

    QSqlRecord key;
    key.append(QSqlField(s("ID")));
    key.append(QSqlField(s("ghost")));
    const QSqlRecord values = r.keyValues(key);
    CHECK(values.count() == 1 && values.fieldName(0) == s("id") && values.value(0).toInt() == 5);

    QSqlRecord copy = r;
    CHECK(copy == r);
    copy.setValue(1, 30);
    CHECK(copy != r);
    copy.replace(1, QSqlField(s("years")));
    copy.replace(9, QSqlField(s("no")));
    CHECK(copy.fieldName(1) == s("years") && copy.count() == 3);
    copy.remove(1);
    copy.remove(9);
    CHECK(copy.count() == 2 && copy.fieldName(1) == s("Name"));
    copy.insert(-4, QSqlField(s("first")));
    copy.insert(99, QSqlField(s("last")));
    CHECK(copy.fieldName(0) == s("first") && copy.fieldName(3) == s("last"));
    copy.clearValues();
    CHECK(copy.isNull("id") && copy.count() == 4);
    copy.swap(r);
    CHECK(r.count() == 4 && copy.count() == 3);
    r.clear();
    CHECK(r.isEmpty());
    QSqlRecord moved = std::move(copy);
    CHECK(moved.count() == 3);
    CHECK(QDebug::toString(moved).contains(s("QSqlRecord(3)")));

    QSqlIndex index(s("people"), s("pk"));
    CHECK(index.cursorName() == s("people") && index.name() == s("pk") && index.isEmpty());
    index.append(QSqlField(s("id")));
    index.append(QSqlField(s("when")), true);
    CHECK(index.count() == 2 && !index.isDescending(0) && index.isDescending(1) && !index.isDescending(5));
    index.setDescending(0, true);
    index.setDescending(1, false);
    index.setDescending(7, true);
    CHECK(index.isDescending(0) && !index.isDescending(1));
    index.setCursorName(s("t"));
    index.setName(s("n"));
    CHECK(index.cursorName() == s("t") && index.name() == s("n"));
    QSqlIndex other;
    other.swap(index);
    CHECK(index.isEmpty() && other.count() == 2 && other.name() == s("n") && other.isDescending(0));
    const QSqlRecord &asRecord = other;
    CHECK(asRecord.fieldName(1) == s("when"));
}

// A driver of the application, to test what does not depend on SQLite.

class MemoryResult : public QSqlResult {
public:
    explicit MemoryResult(const QSqlDriver *driver) : QSqlResult(driver) {}
    QStringList log;
    using QSqlResult::bindingSyntax;
    using QSqlResult::bindValueType;
    using QSqlResult::boundValueCount;
    using QSqlResult::hasOutValues;
    using QSqlResult::NamedBinding;
    using QSqlResult::PositionalBinding;

protected:
    QVariant data(int i) override { return i == 0 ? QVariant(at()) : QVariant(s("row") + QString::number(at())); }
    bool isNull(int) override { return false; }
    bool reset(const QString &query) override {
        log.append(s("reset:") + query);
        const bool select = query.startsWith(s("SELECT"));
        setSelect(select);
        setActive(true);
        return true;
    }
    bool prepare(const QString &query) override {
        log.append(s("prepare:") + query + s("=>") + executedQuery());
        return true;
    }
    bool exec() override {
        QStringList values;
        for (const QVariant &v : boundValues())
            values.append(v.isNull() ? s("NULL") : v.toString());
        log.append(s("exec:") + values.join(QChar(',')));
        setSelect(lastQuery().startsWith(s("SELECT")));
        setActive(true);
        return !lastQuery().contains(s("FAIL"));
    }
    bool fetch(int i) override {
        if (i < 0 || i >= 3)
            return false;
        setAt(i);
        return true;
    }
    bool fetchFirst() override { return fetch(0); }
    bool fetchLast() override { return fetch(2); }
    int size() override { return 3; }
    int numRowsAffected() override { return 7; }
    QSqlRecord record() const override {
        QSqlRecord r;
        r.append(QSqlField(s("n"), QMetaType::fromType<int>()));
        r.append(QSqlField(s("label"), QMetaType::fromType<QString>()));
        return r;
    }
};

class MemoryDriver : public QSqlDriver {
public:
    static int instances;
    MemoryDriver() { ++instances; }
    ~MemoryDriver() override { --instances; }
    bool hasFeature(DriverFeature f) const override { return f == QuerySize || f == PreparedQueries || transactions; }
    bool open(const QString &db, const QString &user, const QString &password, const QString &host, int port,
              const QString &options) override {
        opened = db + QChar('|') + user + QChar('|') + password + QChar('|') + host + QChar('|') +
                 QString::number(port) + QChar('|') + options;
        if (db == s("bad")) {
            setLastError(QSqlError(s("cannot open"), s("bad name"), QSqlError::ConnectionError, s("1")));
            setOpenError(true);
            return false;
        }
        setOpen(true);
        setOpenError(false);
        return true;
    }
    void close() override {
        setOpen(false);
        setOpenError(false);
    }
    QSqlResult *createResult() const override { return new MemoryResult(this); }
    bool beginTransaction() override { return ++begun > 0; }
    bool commitTransaction() override { return true; }
    bool rollbackTransaction() override { return true; }
    using QSqlDriver::setDbmsType;
    QString opened;
    int begun = 0;
    bool transactions = false;
};
int MemoryDriver::instances = 0;

static void testDriverBase() {
    MemoryDriver d;
    CHECK(!d.isOpen() && !d.isOpenError() && d.dbmsType() == QSqlDriver::UnknownDbms);
    d.setDbmsType(QSqlDriver::PostgreSQL);
    CHECK(d.dbmsType() == QSqlDriver::PostgreSQL);
    CHECK(d.tables(QSql::AllTables).isEmpty() && d.record(s("t")).isEmpty() && d.primaryIndex(s("t")).isEmpty());
    CHECK(!d.handle().isValid() && !d.cancelQuery());
    CHECK(!d.subscribeToNotification(s("n")) && !d.unsubscribeFromNotification(s("n")));
    CHECK(d.subscribedToNotifications().isEmpty());
    CHECK(d.maximumIdentifierLength(QSqlDriver::TableName) > 1000);
    CHECK(d.numericalPrecisionPolicy() == QSql::LowPrecisionDouble);
    d.setNumericalPrecisionPolicy(QSql::HighPrecision);
    CHECK(d.numericalPrecisionPolicy() == QSql::HighPrecision);
    CHECK(d.escapeIdentifier(s("a b"), QSqlDriver::FieldName) == s("a b"));
    CHECK(d.isIdentifierEscaped(s("\"a\""), QSqlDriver::FieldName) &&
          !d.isIdentifierEscaped(s("a"), QSqlDriver::FieldName));
    CHECK(d.stripDelimiters(s("\"a b\""), QSqlDriver::TableName) == s("a b"));
    CHECK(d.stripDelimiters(s("a"), QSqlDriver::TableName) == s("a"));

    int notified = 0;
    QObject::connect(&d, &QSqlDriver::notification,
                     [&](const QString &name, QSqlDriver::NotificationSource source, const QVariant &payload) {
                         if (name == s("ping") && source == QSqlDriver::OtherSource && payload.toInt() == 3)
                             ++notified;
                     });
    d.notification(s("ping"), QSqlDriver::OtherSource, 3);
    CHECK(notified == 1);

    const auto format = [&d](const QVariant &v, bool trim = false) {
        QSqlField f(s("f"));
        f.setValue(v);
        return d.formatValue(f, trim);
    };
    CHECK(format(QVariant()) == s("NULL"));
    CHECK(format(42) == s("42") && format(-7LL) == s("-7") && format(1.5) == s("1.5"));
    CHECK(format(true) == s("1") && format(false) == s("0"));
    CHECK(format(s("it's")) == s("'it''s'"));
    CHECK(format(s("pad  ")) == s("'pad  '") && format(s("pad  "), true) == s("'pad'"));
    CHECK(format(s("'; DROP TABLE t; --")) == s("'''; DROP TABLE t; --'"));
    CHECK(format(QByteArray("\x01\xff", 2)) == s("X'01ff'"));
    CHECK(format(QDate(2024, 2, 29)) == s("'2024-02-29'"));
    CHECK(format(QTime(1, 2, 3, 4)) == s("'01:02:03.004'"));
    CHECK(format(QDateTime(QDate(2024, 2, 29), QTime(1, 2, 3, 4), QTimeZone::UTC))
              .startsWith(s("'2024-02-29T01:02:03.004")));
    CHECK(format(QDate()) == s("NULL"));

    QSqlRecord rec;
    rec.append(QSqlField(s("id"), QMetaType::fromType<int>()));
    rec.append(QSqlField(s("name"), QMetaType::fromType<QString>()));
    rec.append(QSqlField(s("note"), QMetaType::fromType<QString>()));
    rec.setValue(0, 3);
    rec.setValue(1, s("O'Hara"));
    rec.setGenerated(2, false);
    CHECK(d.sqlStatement(QSqlDriver::SelectStatement, s("t"), rec, false) == s("SELECT id, name FROM t"));
    CHECK(d.sqlStatement(QSqlDriver::InsertStatement, s("t"), rec, true) ==
          s("INSERT INTO t (id, name) VALUES (?, ?)"));
    CHECK(d.sqlStatement(QSqlDriver::InsertStatement, s("t"), rec, false) ==
          s("INSERT INTO t (id, name) VALUES (3, 'O''Hara')"));
    CHECK(d.sqlStatement(QSqlDriver::UpdateStatement, s("t"), rec, true) == s("UPDATE t SET id = ?, name = ?"));
    CHECK(d.sqlStatement(QSqlDriver::UpdateStatement, s("t"), rec, false) ==
          s("UPDATE t SET id = 3, name = 'O''Hara'"));
    CHECK(d.sqlStatement(QSqlDriver::DeleteStatement, s("t"), rec, true) == s("DELETE FROM t"));
    CHECK(d.sqlStatement(QSqlDriver::WhereStatement, s("t"), rec, true) == s("WHERE id = ? AND name = ?"));
    rec.setNull(1);
    CHECK(d.sqlStatement(QSqlDriver::WhereStatement, s("t"), rec, true) == s("WHERE id = ? AND name IS NULL"));
    CHECK(d.sqlStatement(QSqlDriver::WhereStatement, s("t"), rec, false) == s("WHERE id = 3 AND name IS NULL"));
    rec.setGenerated(0, false);
    rec.setGenerated(1, false);
    CHECK(d.sqlStatement(QSqlDriver::SelectStatement, s("t"), rec, false).isEmpty());
    CHECK(d.sqlStatement(QSqlDriver::InsertStatement, s("t"), rec, true).isEmpty());
    CHECK(d.sqlStatement(QSqlDriver::UpdateStatement, s("t"), rec, true).isEmpty());
    CHECK(d.sqlStatement(QSqlDriver::WhereStatement, s("t"), rec, true).isEmpty());
}

static void testConnections() {
    CHECK(!QSqlDatabase().isValid() && !QSqlDatabase().isOpen() && QSqlDatabase().driver() != nullptr);
    CHECK(QSqlDatabase().connectionName().isEmpty() && QSqlDatabase().port() == -1);
    CHECK(!QSqlDatabase().open() && !QSqlDatabase().transaction() && QSqlDatabase().tables().isEmpty());
    CHECK(!QSqlDatabase::contains() && !QSqlDatabase::contains(s("mem")));
    CHECK(!QSqlDatabase::database(s("mem")).isValid());
    CHECK(!QSqlDatabase::isDriverAvailable(s("QMEMORY")));
    {
        // An unknown kind of database gives a handle that is in the list but not valid.
        QSqlDatabase nope = QSqlDatabase::addDatabase(s("QNOPE"), s("nope"));
        CHECK(!nope.isValid() && QSqlDatabase::contains(s("nope")) && !nope.open());
        CHECK(nope.lastError().isValid() && nope.driverName() == s("QNOPE"));
        QSqlQuery q(nope);
        CHECK(!q.exec(s("SELECT 1")) && q.lastError().isValid() && !q.isActive());
    }
    QSqlDatabase::removeDatabase(s("nope"));
    CHECK(!QSqlDatabase::contains(s("nope")));

    QSqlDatabase::registerSqlDriver(s("QMEMORY"), new QSqlDriverCreator<MemoryDriver>);
    CHECK(QSqlDatabase::isDriverAvailable(s("QMEMORY")) && QSqlDatabase::drivers().contains(s("QMEMORY")));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(s("QMEMORY"), s("mem"));
        CHECK(db.isValid() && !db.isOpen() && db.driverName() == s("QMEMORY") && db.connectionName() == s("mem"));
        CHECK(MemoryDriver::instances == 1);
        CHECK(QSqlDatabase::contains(s("mem")) && QSqlDatabase::connectionNames().contains(s("mem")));
        db.setDatabaseName(s("name"));
        db.setUserName(s("user"));
        db.setPassword(s("secret"));
        db.setHostName(s("host"));
        db.setPort(5432);
        db.setConnectOptions(s("A=1;B"));
        CHECK(db.databaseName() == s("name") && db.userName() == s("user") && db.password() == s("secret"));
        CHECK(db.hostName() == s("host") && db.port() == 5432 && db.connectOptions() == s("A=1;B"));
        db.setNumericalPrecisionPolicy(QSql::LowPrecisionInt64);
        CHECK(db.numericalPrecisionPolicy() == QSql::LowPrecisionInt64);
        CHECK(db.open());
        auto *driver = static_cast<MemoryDriver *>(db.driver());
        CHECK(driver->opened == s("name|user|secret|host|5432|A=1;B"));
        CHECK(driver->numericalPrecisionPolicy() == QSql::LowPrecisionInt64);
        CHECK(db.isOpen() && !db.isOpenError() && !db.lastError().isValid());
        CHECK(db.open(s("other"), s("pw")) && driver->opened == s("name|other|pw|host|5432|A=1;B"));
        CHECK(db.userName() == s("other") && db.password() == s("secret"));

        // The driver has no transactions until it says so.
        CHECK(!db.transaction() && !db.commit() && !db.rollback() && driver->begun == 0);
        driver->transactions = true;
        CHECK(db.transaction() && db.commit() && db.rollback() && driver->begun == 1);

        // Copies are handles to the same connection, and database() returns one.
        QSqlDatabase same = QSqlDatabase::database(s("mem"));
        CHECK(same.isOpen() && same.driver() == db.driver());
        db.close();
        CHECK(!same.isOpen());
        // database() opens a closed connection, unless told not to.
        CHECK(!QSqlDatabase::database(s("mem"), false).isOpen());
        CHECK(QSqlDatabase::database(s("mem")).isOpen());

        db.setDatabaseName(s("bad"));
        CHECK(!db.open() && db.isOpenError() && !db.isOpen());
        CHECK(db.lastError().type() == QSqlError::ConnectionError && db.lastError().nativeErrorCode() == s("1"));
        CHECK(QDebug::toString(db).contains(s("QMEMORY")));
        db.setDatabaseName(s("name"));

        QSqlDatabase clone = QSqlDatabase::cloneDatabase(db, s("mem2"));
        CHECK(clone.isValid() && !clone.isOpen() && clone.driver() != db.driver() && MemoryDriver::instances == 2);
        CHECK(clone.databaseName() == s("name") && clone.port() == 5432 && clone.connectionName() == s("mem2"));
        CHECK(clone.numericalPrecisionPolicy() == QSql::LowPrecisionInt64);
        QSqlDatabase clone2 = QSqlDatabase::cloneDatabase(s("mem"), s("mem3"));
        CHECK(clone2.isValid() && clone2.hostName() == s("host") && MemoryDriver::instances == 3);
        CHECK(!QSqlDatabase::cloneDatabase(QSqlDatabase(), s("x")).isValid() && !QSqlDatabase::contains(s("x")));
    }
    QSqlDatabase::removeDatabase(s("mem3"));
    QSqlDatabase::removeDatabase(s("mem2"));
    CHECK(MemoryDriver::instances == 1);

    // A connection with a driver object of the application.
    {
        QSqlDatabase own = QSqlDatabase::addDatabase(new MemoryDriver, s("own"));
        CHECK(own.isValid() && own.driverName().isEmpty() && MemoryDriver::instances == 2);
        CHECK(!QSqlDatabase::cloneDatabase(own, s("own2")).isValid());
    }
    QSqlDatabase::removeDatabase(s("own"));
    QSqlDatabase::removeDatabase(s("not there"));
    CHECK(MemoryDriver::instances == 1);
}

static void testQueryWithDriver() {
    QSqlDatabase db = QSqlDatabase::database(s("mem"), false);
    db.setDatabaseName(s("name"));
    {
        QSqlQuery closed(db);
        db.close();
        CHECK(!closed.exec(s("SELECT 1")) && closed.lastError().type() == QSqlError::ConnectionError);
        CHECK(!closed.prepare(s("SELECT 1")) && !closed.exec() && !closed.execBatch());
    }
    CHECK(db.open());
    QSqlQuery q(db);
    auto *result = const_cast<MemoryResult *>(static_cast<const MemoryResult *>(q.result()));
    CHECK(q.driver() == db.driver() && !q.isActive() && !q.isValid() && q.at() == QSql::BeforeFirstRow);
    CHECK(!q.next() && !q.first() && !q.last() && !q.previous() && !q.seek(0) && q.size() == -1);
    CHECK(q.numRowsAffected() == -1 && !q.value(0).isValid() && q.isNull(0));
    CHECK(!q.exec(QString()));

    CHECK(q.exec(s("  SELECT n, label FROM t  ")));
    CHECK(q.isActive() && q.isSelect() && q.lastQuery() == s("SELECT n, label FROM t"));
    CHECK(q.executedQuery() == q.lastQuery());
    CHECK(q.size() == 3 && q.numRowsAffected() == 7 && q.record().count() == 2 && q.record().isNull(0));
    CHECK(!q.value(0).isValid() && q.isNull(0));
    // Walking the three rows.
    CHECK(q.next() && q.at() == 0 && q.isValid() && q.value(0).toInt() == 0);
    CHECK(q.value("LABEL").toString() == s("row0") && !q.value("nope").isValid() && !q.value(2).isValid());
    CHECK(!q.isNull(0) && !q.isNull("label") && q.isNull("nope") && q.isNull(5));
    CHECK(q.record().value(1).toString() == s("row0"));
    CHECK(q.next() && q.next() && q.at() == 2 && !q.next() && q.at() == QSql::AfterLastRow && !q.isValid());
    CHECK(!q.next() && q.previous() && q.at() == 2 && q.previous() && q.previous() && q.at() == 0);
    CHECK(!q.previous() && q.at() == QSql::BeforeFirstRow && !q.previous());
    CHECK(q.last() && q.at() == 2 && q.first() && q.at() == 0);
    CHECK(q.seek(1) && q.at() == 1 && q.seek(1, true) && q.at() == 2 && q.seek(-2, true) && q.at() == 0);
    CHECK(q.seek(0, true) && q.at() == 0);
    CHECK(!q.seek(-1, true) && q.at() == QSql::BeforeFirstRow);
    CHECK(!q.seek(0, true) && !q.seek(-1, true) && q.seek(2, true) && q.at() == 1);
    CHECK(!q.seek(7) && q.at() == QSql::AfterLastRow && !q.seek(1, true) && !q.seek(0, true));
    CHECK(q.seek(-1, true) && q.at() == 2 && q.seek(-3, true) == false);
    CHECK(!q.seek(-1) && q.at() == QSql::BeforeFirstRow);

    // Forward only.
    q.setForwardOnly(true);
    CHECK(q.isForwardOnly() && q.exec(s("SELECT 1")));
    CHECK(q.first() && q.next() && q.at() == 1 && !q.previous() && !q.first() && !q.seek(0) && q.at() == 1);
    CHECK(!q.seek(-1, true) && q.seek(2) && q.at() == 2 && !q.next() && !q.seek(-1, true));
    q.setForwardOnly(false);

    // A statement that returns no rows.
    CHECK(q.exec(s("UPDATE t SET n = 1")) && q.isActive() && !q.isSelect());
    CHECK(!q.next() && !q.first() && q.size() == -1 && q.numRowsAffected() == 7);

    // Placeholders.
    result->log.clear();
    CHECK(q.prepare(s("SELECT a FROM t WHERE x = :x AND y = ? AND z = :x AND s = ':no ?' AND w = :w_1 -- :c ?\n")));
    CHECK(result->log.last() ==
          s("prepare:SELECT a FROM t WHERE x = :x AND y = ? AND z = :x AND s = ':no ?' AND w = :w_1 -- :c ?\n=>"
            "SELECT a FROM t WHERE x = ? AND y = ? AND z = ? AND s = ':no ?' AND w = ? -- :c ?\n"));
    CHECK(q.boundValueNames() == QStringList({s(":x"), QString(), s(":x"), s(":w_1")}));
    CHECK(q.boundValueName(3) == s(":w_1") && q.boundValueName(1).isEmpty() && q.boundValueName(9).isEmpty());
    CHECK(q.executedQuery().count(QChar('?')) == 6 && q.lastQuery().contains(s(":w_1")));
    CHECK(result->bindingSyntax() == MemoryResult::NamedBinding && q.boundValues().isEmpty());
    q.bindValue(s(":x"), 1);
    q.bindValue(1, s("two"));
    q.bindValue(s(":w_1"), QVariant(), QSql::InOut);
    q.bindValue(s(":ghost"), 9);
    CHECK(q.boundValues().size() == 4 && result->boundValueCount() == 4);
    CHECK(q.boundValue(s(":x")).toInt() == 1 && q.boundValue(2).toInt() == 1 && q.boundValue(1).toString() == s("two"));
    CHECK(!q.boundValue(s(":ghost")).isValid() && !q.boundValue(9).isValid() && q.boundValue(3).isNull());
    CHECK(result->bindValueType(3) == QSql::InOut && result->bindValueType(s(":x")) == QSql::In &&
          result->hasOutValues());
    CHECK(result->bindValueType(s(":ghost")) == QSql::In && result->bindValueType(44) == QSql::In);
    CHECK(q.exec() && result->log.last() == s("exec:1,two,1,NULL"));
    // The values stay bound for the next run.
    q.bindValue(s(":x"), 5);
    CHECK(q.exec() && result->log.last() == s("exec:5,two,5,NULL"));
    // addBindValue() starts again with the first placeholder after exec().
    q.addBindValue(s("a"));
    q.addBindValue(s("b"));
    CHECK(q.exec() && result->log.last() == s("exec:a,b,5,NULL"));

    CHECK(q.prepare(s("INSERT INTO t VALUES (?, ?)")) && q.boundValues().isEmpty());
    CHECK(result->bindingSyntax() == MemoryResult::PositionalBinding && !result->hasOutValues());
    // Text in quotes, names in brackets and comments are not searched, and a cast is not a name.
    CHECK(q.prepare(s("SELECT \"a:b?\", [c:d?], `e:f?`, x::int, 'it''s :n' /* :m ? */ FROM t WHERE a = :a")));
    CHECK(q.boundValueNames() == QStringList({s(":a")}));
    CHECK(q.executedQuery() == s("SELECT \"a:b?\", [c:d?], `e:f?`, x::int, 'it''s :n' /* :m ? */ FROM t WHERE a = ?"));
    CHECK(q.prepare(s("SELECT ':unfinished ? FROM t")) && q.boundValueNames().isEmpty());
    CHECK(!q.prepare(QString()));

    // A batch runs the statement for each row.
    CHECK(q.prepare(s("INSERT INTO t VALUES (?, ?)")));
    q.addBindValue(QVariantList{1, 2, 3});
    q.addBindValue(QVariantList{s("a"), QVariant(), s("c")});
    result->log.clear();
    CHECK(q.execBatch());
    CHECK(result->log == QStringList({s("exec:1,a"), s("exec:2,NULL"), s("exec:3,c")}));
    CHECK(q.boundValue(0).toList().size() == 3);
    CHECK(!q.execBatch(QSqlQuery::ValuesAsColumns) && q.lastError().isValid());
    q.bindValue(1, 5);
    CHECK(!q.execBatch() && q.lastError().isValid());
    CHECK(q.prepare(s("INSERT INTO t VALUES (?)")) && !q.execBatch());
    CHECK(q.prepare(s("INSERT FAIL (?)")));
    q.addBindValue(QVariantList{1, 2});
    result->log.clear();
    CHECK(!q.execBatch() && result->log.size() == 1);

    // finish() and clear().
    CHECK(q.exec(s("SELECT 1")) && q.next());
    q.finish();
    CHECK(!q.isActive() && q.at() == QSql::BeforeFirstRow && q.lastQuery() == s("SELECT 1"));
    q.finish();
    CHECK(q.exec(s("SELECT 2")));
    q.clear();
    CHECK(!q.isActive() && q.lastQuery().isEmpty() && !q.lastError().isValid() && q.boundValues().isEmpty());
    CHECK(!q.nextResult() && !q.lastInsertId().isValid() && !q.result()->handle().isValid());
    q.setNumericalPrecisionPolicy(QSql::HighPrecision);
    CHECK(q.numericalPrecisionPolicy() == QSql::HighPrecision);

    // Moving and swapping.
    CHECK(q.exec(s("SELECT 3")) && q.next());
    QSqlQuery moved = std::move(q);
    CHECK(moved.isActive() && moved.at() == 0 && moved.lastQuery() == s("SELECT 3"));
    QSqlQuery other(db);
    other.swap(moved);
    CHECK(other.isActive() && !moved.isActive());
    moved = std::move(other);
    CHECK(moved.isActive());

    // A query that takes a result of the driver, and one without a result.
    QSqlQuery direct(db.driver()->createResult());
    CHECK(direct.exec(s("SELECT 4")) && direct.driver() == db.driver());
    QSqlQuery none(static_cast<QSqlResult *>(nullptr));
    CHECK(!none.exec(s("SELECT 1")) && !none.isActive() && none.lastError().isValid());

    // The driver of a query is gone when its connection is.
    {
        QSqlDatabase temp = QSqlDatabase::addDatabase(s("QMEMORY"), s("temp"));
        CHECK(temp.open());
    }
    QSqlQuery orphan(QSqlDatabase::database(s("temp")));
    CHECK(orphan.exec(s("SELECT 1")) && orphan.driver() != nullptr);
    QSqlDatabase::removeDatabase(s("temp"));
    CHECK(orphan.driver() == nullptr && !orphan.exec(s("SELECT 1")) && !orphan.prepare(s("SELECT 1")));
    CHECK(orphan.size() == -1);
}

// SQLite.

static bool run(QSqlQuery &q, const char *sql) {
    if (q.exec(s(sql)))
        return true;
    std::printf("SQL failed: %s: %s\n", sql, q.lastError().text().toUtf8().constData());
    return false;
}

static void testSqliteBasics(const QString &dir) {
    CHECK(QSqlDatabase::isDriverAvailable(s("QSQLITE")));
    QSqlDatabase db = QSqlDatabase::addDatabase(s("QSQLITE"));
    CHECK(db.isValid() && db.connectionName() == QLatin1StringView(QSqlDatabase::defaultConnection));
    CHECK(db.driver()->dbmsType() == QSqlDriver::SQLite);
    CHECK(db.driver()->hasFeature(QSqlDriver::Transactions) && db.driver()->hasFeature(QSqlDriver::PreparedQueries));
    CHECK(db.driver()->hasFeature(QSqlDriver::LastInsertId) && db.driver()->hasFeature(QSqlDriver::BLOB));
    CHECK(!db.driver()->hasFeature(QSqlDriver::QuerySize) && !db.driver()->hasFeature(QSqlDriver::BatchOperations));
    CHECK(db.tables().isEmpty() && !db.transaction());

    // A file that cannot be made.
    db.setDatabaseName(dir + s("/no/such/dir/x.db"));
    CHECK(!db.open() && db.isOpenError() && db.lastError().type() == QSqlError::ConnectionError);
    CHECK(!db.lastError().nativeErrorCode().isEmpty() && !db.lastError().databaseText().isEmpty());

    db.setDatabaseName(s(":memory:"));
    CHECK(db.open() && db.isOpen() && !db.isOpenError() && !db.lastError().isValid());

    QSqlQuery q; // the default connection
    CHECK(run(q, "CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT NOT NULL, age INT, score REAL DEFAULT 1.5, "
                 "photo BLOB, born DATE, note VARCHAR(20) DEFAULT 'none', active BOOLEAN, seen DATETIME, big BIGINT)"));
    CHECK(q.isActive() && !q.isSelect() && !q.next() && q.size() == -1);
    CHECK(run(q, "CREATE VIEW adults AS SELECT name FROM people WHERE age >= 18"));
    CHECK(db.tables() == QStringList({s("people")}));
    CHECK(db.tables(QSql::Views) == QStringList({s("adults")}));
    CHECK(db.tables(QSql::SystemTables) == QStringList({s("sqlite_master")}));
    CHECK(db.tables(QSql::AllTables).size() == 3);

    const QSqlRecord rec = db.record(s("people"));
    CHECK(rec.count() == 10 && rec.fieldName(0) == s("id") && rec.fieldName(9) == s("big"));
    CHECK(rec.field(0).metaType().id() == QMetaType::Int && rec.field(0).isAutoValue());
    CHECK(rec.field(1).metaType().id() == QMetaType::QString && rec.field(1).requiredStatus() == QSqlField::Required);
    CHECK(rec.field(2).metaType().id() == QMetaType::Int && rec.field(2).requiredStatus() == QSqlField::Optional);
    CHECK(rec.field(3).metaType().id() == QMetaType::Double && rec.field(3).defaultValue().toString() == s("1.5"));
    CHECK(rec.field(4).metaType().id() == QMetaType::QByteArray && rec.field(5).metaType().id() == QMetaType::QDate);
    CHECK(rec.field(6).metaType().id() == QMetaType::QString && rec.field(6).defaultValue().toString() == s("none"));
    CHECK(rec.field(7).metaType().id() == QMetaType::Bool && rec.field(8).metaType().id() == QMetaType::QDateTime);
    CHECK(rec.field(9).metaType().id() == QMetaType::LongLong && !rec.field(1).isAutoValue());
    CHECK(rec.field(1).tableName() == s("people") && rec.isNull(1));
    CHECK(db.record(s("\"people\"")).count() == 10 && db.record(s("main.people")).count() == 10);
    CHECK(db.record(s("nope")).isEmpty() && db.record(s("x\"); DROP TABLE people; --")).isEmpty());
    CHECK(db.record(s("adults")).count() == 1);
    const QSqlIndex pk = db.primaryIndex(s("people"));
    CHECK(pk.count() == 1 && pk.fieldName(0) == s("id") && pk.cursorName() == s("people"));
    CHECK(db.primaryIndex(s("adults")).isEmpty() && db.primaryIndex(s("nope")).isEmpty());

    const QSqlDriver *driver = db.driver();
    CHECK(driver->escapeIdentifier(s("my \"table\""), QSqlDriver::TableName) == s("\"my \"\"table\"\"\""));
    CHECK(driver->escapeIdentifier(s("\"done\""), QSqlDriver::TableName) == s("\"done\""));
    CHECK(driver->isIdentifierEscaped(s("[a]"), QSqlDriver::FieldName) &&
          driver->isIdentifierEscaped(s("`a`"), QSqlDriver::FieldName));
    CHECK(!driver->isIdentifierEscaped(s("a"), QSqlDriver::FieldName) &&
          !driver->isIdentifierEscaped(s("\""), QSqlDriver::FieldName));
    CHECK(driver->stripDelimiters(s("\"a \"\"b\"\"\""), QSqlDriver::FieldName) == s("a \"b\""));
    CHECK(driver->stripDelimiters(s("[a b]"), QSqlDriver::FieldName) == s("a b"));

    // Inserting with every kind of value.
    const QByteArray photo("\x00\x01\xfe\xff binary", 11);
    const QDateTime seen(QDate(2024, 2, 29), QTime(13, 14, 15, 160), QTimeZone::UTC);
    CHECK(q.prepare(s("INSERT INTO people (name, age, score, photo, born, note, active, seen, big) "
                      "VALUES (:name, :age, :score, :photo, :born, :note, :active, :seen, :big)")));
    q.bindValue(s(":name"), s("Ada Ünïcödé 'quoted' \"x\"; DROP TABLE people; --"));
    q.bindValue(s(":age"), 36);
    q.bindValue(s(":score"), 9.25);
    q.bindValue(s(":photo"), photo);
    q.bindValue(s(":born"), QDate(1815, 12, 10));
    q.bindValue(s(":note"), QVariant());
    q.bindValue(s(":active"), true);
    q.bindValue(s(":seen"), seen);
    q.bindValue(s(":big"), Q_INT64_C(9007199254740993));
    CHECK(q.exec() && q.isActive() && !q.isSelect() && q.numRowsAffected() == 1);
    CHECK(q.lastInsertId().toLongLong() == 1);
    q.bindValue(s(":name"), s("Grace"));
    q.bindValue(s(":age"), QVariant(QMetaType::fromType<int>()));
    q.bindValue(s(":photo"), QByteArray("", 0));
    q.bindValue(s(":note"), QString::fromUtf8(""));
    q.bindValue(s(":active"), false);
    q.bindValue(s(":big"), std::numeric_limits<quint64>::max());
    CHECK(q.exec() && q.lastInsertId().toLongLong() == 2);

    CHECK(run(q, "SELECT id, name, age, score, photo, born, note, active, seen, big, age + 1 AS next, NULL FROM people "
                 "ORDER BY id"));
    CHECK(q.isSelect() && q.isActive() && q.size() == -1 && q.at() == QSql::BeforeFirstRow &&
          !q.lastInsertId().isValid());
    const QSqlRecord row = q.record();
    CHECK(row.count() == 12 && row.fieldName(1) == s("name") && row.fieldName(10) == s("next") && row.isNull(1));
    CHECK(row.field(0).metaType().id() == QMetaType::Int && row.field(4).metaType().id() == QMetaType::QByteArray);
    CHECK(row.field(10).metaType().id() == QMetaType::LongLong);
    CHECK(q.next() && q.at() == 0);
    CHECK(q.value(0).typeId() == QMetaType::LongLong && q.value(0).toInt() == 1);
    CHECK(q.value(1).toString() == s("Ada Ünïcödé 'quoted' \"x\"; DROP TABLE people; --"));
    CHECK(q.value("age").toInt() == 36 && q.value(3).typeId() == QMetaType::Double && q.value(3).toDouble() == 9.25);
    CHECK(q.value(4).typeId() == QMetaType::QByteArray && q.value(4).toByteArray() == photo);
    CHECK(q.value(5).toString() == s("1815-12-10") && q.value(5).toDate() == QDate(1815, 12, 10));
    CHECK(q.isNull(6) && q.isNull("note") && q.value(6).isNull() && q.value(6).typeId() == QMetaType::QString);
    CHECK(q.value(7).toInt() == 1 && q.value(7).toBool());
    CHECK(q.value(8).toString() == s("2024-02-29T13:14:15.160Z") && q.value(8).toDateTime() == seen);
    CHECK(q.value(9).toLongLong() == Q_INT64_C(9007199254740993));
    CHECK(q.value(10).toInt() == 37 && q.isNull(11) && !q.value(11).isValid());
    CHECK(q.record().value("name").toString().startsWith(s("Ada")));
    CHECK(q.next() && q.value(1).toString() == s("Grace") && q.isNull(2) && q.value(2).typeId() == QMetaType::Int);
    CHECK(!q.isNull(4) && q.value(4).toByteArray().isEmpty() && !q.isNull(6) && q.value(6).toString().isEmpty());
    CHECK(!q.value(7).toBool() && q.value(9).toDouble() > 1.8e19);
    CHECK(!q.next() && q.at() == QSql::AfterLastRow && !q.isValid() && !q.next());
    // The rows that were read can be visited again.
    CHECK(q.previous() && q.value(1).toString() == s("Grace") && q.first() && q.value(0).toInt() == 1);
    CHECK(q.last() && q.at() == 1 && q.seek(0) && q.seek(1, true) && q.at() == 1 && !q.seek(2));
}

static void testSqliteStatements() {
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec(s("DROP VIEW IF EXISTS adults"));
    q.exec(s("DROP TABLE IF EXISTS people"));
    CHECK(run(q, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT UNIQUE, n INT)"));

    // Errors.
    CHECK(!q.exec(s("SELEC nonsense")) && !q.isActive());
    CHECK(q.lastError().type() == QSqlError::StatementError && q.lastError().databaseText().contains(s("syntax")));
    CHECK(!q.lastError().nativeErrorCode().isEmpty() && q.lastQuery() == s("SELEC nonsense"));
    CHECK(!q.exec(s("SELECT * FROM missing")) && q.lastError().databaseText().contains(s("missing")));
    CHECK(!q.prepare(s("INSERT INTO missing VALUES (?)")) && q.lastError().isValid() && !q.exec());
    CHECK(!q.exec(s("INSERT INTO t (name) VALUES ('a'); INSERT INTO t (name) VALUES ('b')")));
    CHECK(q.lastError().databaseText().contains(s("multiple statements")));
    CHECK(run(q, "SELECT count(*) FROM t") && q.next() && q.value(0).toInt() == 0);
    CHECK(q.exec(s("INSERT INTO t (name) VALUES ('a') ; -- trailing comment\n /* and another */ ")));
    CHECK(!q.exec(s("   ;  ")) && !q.exec(s("-- only a comment")) && q.lastError().isValid());
    CHECK(q.exec(s("SELECT 1")) && !q.lastError().isValid());
    // A constraint that fails when the statement runs.
    CHECK(!q.exec(s("INSERT INTO t (name) VALUES ('a')")) && !q.isActive());
    CHECK(q.lastError().databaseText().contains(s("UNIQUE")) && q.lastError().nativeErrorCode() == s("2067"));
    CHECK(q.prepare(s("INSERT INTO t (name, n) VALUES (?, ?)")));
    q.addBindValue(s("a"));
    q.addBindValue(1);
    CHECK(!q.exec() && q.lastError().isValid());
    q.addBindValue(s("b"));
    q.addBindValue(2);
    CHECK(q.exec() && !q.lastError().isValid() && q.lastInsertId().toInt() == 2);
    // Too few or too many values.
    q.addBindValue(s("c"));
    q.bindValue(2, 1);
    CHECK(!q.exec() && q.lastError().driverText().contains(s("count mismatch")));
    CHECK(q.prepare(s("INSERT INTO t (name, n) VALUES (?, ?)")));
    q.addBindValue(s("c"));
    CHECK(!q.exec() && q.lastError().driverText().contains(s("count mismatch")));
    CHECK(run(q, "SELECT count(*) FROM t") && q.next() && q.value(0).toInt() == 2);

    // Values are data, whatever they look like; a name used twice is bound twice.
    CHECK(q.prepare(s("SELECT :v, :v || 'x', ?, :w")));
    q.bindValue(s(":v"), s("'); DROP TABLE t; --"));
    q.bindValue(2, s("?"));
    q.bindValue(s(":w"), s(":v"));
    CHECK(q.exec() && q.next() && q.value(0).toString() == s("'); DROP TABLE t; --"));
    CHECK(q.value(1).toString() == s("'); DROP TABLE t; --x") && q.value(2).toString() == s("?"));
    CHECK(q.value(3).toString() == s(":v") && q.record().count() == 4);
    CHECK(db.tables().contains(s("t")));
    CHECK(q.prepare(s("SELECT ':a', \"name\", ? FROM t WHERE name = :n -- :c\n")));
    q.bindValue(0, 5);
    q.bindValue(s(":n"), s("b"));
    CHECK(q.exec() && q.next() && q.value(0).toString() == s(":a") && q.value(1).toString() == s("b") && !q.next());

    // Running a prepared statement again, with other values.
    CHECK(q.prepare(s("SELECT name FROM t WHERE n = :n")));
    q.bindValue(s(":n"), 2);
    CHECK(q.exec() && q.next() && q.value(0).toString() == s("b") && !q.next());
    q.bindValue(s(":n"), 77);
    CHECK(q.exec() && !q.next() && q.isActive());
    q.bindValue(s(":n"), 2);
    CHECK(q.exec() && q.first() && q.value(0).toString() == s("b"));
    q.finish();
    CHECK(!q.isActive() && !q.next() && !q.value(0).isValid());
    CHECK(q.exec() && q.next() && q.value(0).toString() == s("b"));

    // UPDATE and DELETE tell the number of rows.
    CHECK(run(q, "UPDATE t SET n = 5") && q.numRowsAffected() == 2 && !q.isSelect());
    CHECK(run(q, "DELETE FROM t WHERE name = 'zzz'") && q.numRowsAffected() == 0);
    CHECK(run(q, "DELETE FROM t") && q.numRowsAffected() == 2);

    // A batch, in a transaction.
    CHECK(db.transaction());
    CHECK(q.prepare(s("INSERT INTO t (name, n) VALUES (?, ?)")));
    QVariantList names;
    QVariantList numbers;
    for (int i = 0; i < 600; ++i) {
        names.append(s("name") + QString::number(i).rightJustified(3, QChar('0')));
        numbers.append(i % 7 == 0 ? QVariant() : QVariant(i));
    }
    q.addBindValue(names);
    q.addBindValue(numbers);
    CHECK(q.execBatch());
    CHECK(db.commit());
    CHECK(run(q, "SELECT count(*), count(n) FROM t") && q.next() && q.value(0).toInt() == 600 &&
          q.value(1).toInt() == 514);

    // Transactions.
    CHECK(db.transaction() && run(q, "DELETE FROM t WHERE n < 100") && db.rollback());
    CHECK(run(q, "SELECT count(*) FROM t") && q.next() && q.value(0).toInt() == 600);
    CHECK(!db.commit() && db.lastError().type() == QSqlError::TransactionError); // none is running
    CHECK(!db.rollback() && db.transaction() && !db.transaction() && db.rollback());

    // Walking many rows, forwards and back, without reading them all first.
    CHECK(run(q, "SELECT name, n FROM t ORDER BY name"));
    CHECK(q.seek(10) && q.value(0).toString() == s("name010") && q.seek(3) && q.value(0).toString() == s("name003"));
    CHECK(q.seek(-2, true) && q.at() == 1 && q.seek(20, true) && q.at() == 21 && q.isNull(1));
    CHECK(q.last() && q.at() == 599 && q.value(0).toString() == s("name599") && !q.next());
    CHECK(q.first() && q.at() == 0 && q.previous() == false && q.at() == QSql::BeforeFirstRow);
    CHECK(!q.seek(600) && q.at() == QSql::AfterLastRow && q.seek(-1, true) && q.at() == 599);
    int count = 0;
    CHECK(run(q, "SELECT name FROM t ORDER BY name"));
    while (q.next())
        ++count;
    CHECK(count == 600 && q.at() == QSql::AfterLastRow);

    // Forward only: one row at a time.
    QSqlQuery forward(db);
    forward.setForwardOnly(true);
    CHECK(forward.exec(s("SELECT name FROM t ORDER BY name")) && forward.isForwardOnly());
    CHECK(forward.next() && forward.value(0).toString() == s("name000"));
    CHECK(forward.seek(5) && forward.value(0).toString() == s("name005") && !forward.previous() && !forward.first());
    CHECK(!forward.seek(2) && forward.at() == 5 && forward.next() && forward.value(0).toString() == s("name006"));
    CHECK(forward.last() && forward.at() == 599 && forward.value(0).toString() == s("name599") && !forward.next());
    CHECK(forward.exec(s("SELECT name FROM t WHERE n = -1")) && !forward.next() && !forward.last());

    // An empty result still has its fields.
    CHECK(run(q, "SELECT name, n FROM t WHERE n = -1"));
    CHECK(q.record().count() == 2 && !q.next() && !q.first() && !q.last() && !q.seek(0) && q.isActive());
    q.clear();
    CHECK(q.record().isEmpty() && !q.isActive());

    // Two queries at the same time on one connection.
    QSqlQuery outer(db);
    QSqlQuery inner(db);
    CHECK(outer.exec(s("SELECT id FROM t WHERE n IN (1, 2, 3) ORDER BY id")));
    CHECK(inner.prepare(s("UPDATE t SET n = n + 1000 WHERE id = ?")));
    int updated = 0;
    while (outer.next()) {
        inner.addBindValue(outer.value(0));
        CHECK(inner.exec() && inner.numRowsAffected() == 1);
        ++updated;
    }
    CHECK(updated == 3);
    CHECK(run(q, "SELECT count(*) FROM t WHERE n > 1000") && q.next() && q.value(0).toInt() == 3);

    // Other types go in as text.
    CHECK(q.prepare(s("SELECT ?, ?, ?, ?, typeof(?), typeof(?), typeof(?)")));
    q.addBindValue(QTime(7, 8, 9, 10));
    q.addBindValue(QUuid(QStringLiteral("{12345678-1234-5678-1234-567812345678}")));
    q.addBindValue(QChar('x'));
    q.addBindValue(1.0f);
    q.addBindValue(static_cast<short>(3));
    q.addBindValue(2.5f);
    q.addBindValue(QStringList({s("a")}));
    CHECK(q.exec() && q.next() && q.value(0).toString() == s("07:08:09.010"));
    CHECK(q.value(1).toString().contains(s("12345678-1234-5678-1234-567812345678")) && q.value(2).toString() == s("x"));
    CHECK(q.value(3).toDouble() == 1.0 && q.value(4).toString() == s("integer") && q.value(5).toString() == s("real"));
    CHECK(q.value(6).toString() == s("text"));
}

static void testSqliteConnections(const QString &dir) {
    const QString file = dir + s("/shared.db");
    {
        QSqlDatabase a = QSqlDatabase::addDatabase(s("QSQLITE"), s("a"));
        a.setDatabaseName(file);
        CHECK(a.open());
        QSqlQuery q(a);
        CHECK(run(q, "CREATE TABLE log (id INTEGER PRIMARY KEY, who TEXT)"));
        CHECK(run(q, "INSERT INTO log (who) VALUES ('a')"));

        // A second connection to the same file sees what the first one wrote.
        QSqlDatabase b = QSqlDatabase::cloneDatabase(a, s("b"));
        CHECK(b.isValid() && !b.isOpen() && b.databaseName() == file && b.open());
        QSqlQuery qb(b);
        CHECK(run(qb, "SELECT who FROM log") && qb.next() && qb.value(0).toString() == s("a") && !qb.next());

        // A lock held by one connection makes the other one fail, after its timeout, instead of waiting for ever.
        b.close();
        b.setConnectOptions(s("QSQLITE_BUSY_TIMEOUT=50"));
        CHECK(b.open());
        QSqlQuery qb2(b);
        CHECK(a.transaction() && run(q, "INSERT INTO log (who) VALUES ('held')"));
        CHECK(!qb2.exec(s("INSERT INTO log (who) VALUES ('b')")));
        CHECK(qb2.lastError().isValid() && qb2.lastError().databaseText().contains(s("locked")));
        CHECK(a.commit());
        CHECK(qb2.exec(s("INSERT INTO log (who) VALUES ('b')")));

        // Read only.
        QSqlDatabase ro = QSqlDatabase::addDatabase(s("QSQLITE"), s("ro"));
        ro.setDatabaseName(file);
        ro.setConnectOptions(s("QSQLITE_OPEN_READONLY; QSQLITE_BUSY_TIMEOUT=10"));
        CHECK(ro.open());
        QSqlQuery qro(ro);
        CHECK(run(qro, "SELECT count(*) FROM log") && qro.next() && qro.value(0).toInt() == 3);
        CHECK(!qro.exec(s("INSERT INTO log (who) VALUES ('ro')")) &&
              qro.lastError().databaseText().contains(s("readonly")));
        QSqlDatabase missing = QSqlDatabase::addDatabase(s("QSQLITE"), s("missing"));
        missing.setDatabaseName(dir + s("/not-there.db"));
        missing.setConnectOptions(s("QSQLITE_OPEN_READONLY"));
        CHECK(!missing.open() && missing.isOpenError());
        CHECK(!QFile::exists(dir + s("/not-there.db")));

        // A URI name.
        QSqlDatabase uri = QSqlDatabase::addDatabase(s("QSQLITE"), s("uri"));
        uri.setDatabaseName(s("file:") + file + s("?mode=ro"));
        uri.setConnectOptions(s("QSQLITE_OPEN_URI"));
        CHECK(uri.open());
        QSqlQuery quri(uri);
        CHECK(run(quri, "SELECT count(*) FROM log") && !quri.exec(s("DELETE FROM log")));

        // Each thread has its own connection.
        int seen = -1;
        std::thread worker([&] {
            {
                QSqlDatabase mine = QSqlDatabase::cloneDatabase(s("a"), s("worker"));
                if (mine.open()) {
                    QSqlQuery wq(mine);
                    if (wq.exec(s("INSERT INTO log (who) VALUES ('worker')")) &&
                        wq.exec(s("SELECT count(*) FROM log")) && wq.next())
                        seen = wq.value(0).toInt();
                }
            }
            QSqlDatabase::removeDatabase(s("worker"));
        });
        worker.join();
        CHECK(seen == 4 && !QSqlDatabase::contains(s("worker")));

        // Closing a connection under a query makes the query inactive, not broken.
        CHECK(run(qb, "SELECT who FROM log") && qb.next());
        QSqlQuery prepared(b);
        CHECK(prepared.prepare(s("SELECT ?")));
        b.close();
        CHECK(!b.isOpen() && !qb.isActive() && !qb.next() && !qb.value(0).isValid());
        prepared.addBindValue(1);
        CHECK(!prepared.exec() && prepared.lastError().isValid());
        CHECK(!qb.exec(s("SELECT 1")) && b.tables().isEmpty() && b.record(s("log")).isEmpty() && !b.transaction());
        CHECK(b.open() && qb.exec(s("SELECT count(*) FROM log")) && qb.next() && qb.value(0).toInt() == 4);
        CHECK(prepared.prepare(s("SELECT ?")));
        prepared.addBindValue(8);
        CHECK(prepared.exec() && prepared.next() && prepared.value(0).toInt() == 8);
    }
    for (const char *name : {"a", "b", "ro", "missing", "uri"})
        QSqlDatabase::removeDatabase(s(name));

    // A query that outlives its connection.
    QSqlQuery *orphan = nullptr;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(s("QSQLITE"), s("short"));
        db.setDatabaseName(s(":memory:"));
        CHECK(db.open());
        orphan = new QSqlQuery(db);
        CHECK(orphan->exec(s("SELECT 1, 2")) && orphan->next());
    }
    QSqlDatabase::removeDatabase(s("short")); // warns: the query is still there
    CHECK(orphan->driver() == nullptr && !orphan->isActive() && !orphan->next());
    CHECK(!orphan->exec(s("SELECT 1")) && !orphan->prepare(s("SELECT 1")) && orphan->record().isEmpty());
    delete orphan;

    // Replacing a connection of the same name closes the old one.
    {
        QSqlDatabase first = QSqlDatabase::addDatabase(s("QSQLITE"), s("dup"));
        first.setDatabaseName(s(":memory:"));
        CHECK(first.open());
        QSqlDatabase second = QSqlDatabase::addDatabase(s("QSQLITE"), s("dup"));
        CHECK(!first.isOpen() && first.connectionName().isEmpty() && second.connectionName() == s("dup"));
        CHECK(QSqlDatabase::database(s("dup"), false).driver() == second.driver());
    }
    QSqlDatabase::removeDatabase(s("dup"));

    // An empty name is a private temporary database.
    {
        QSqlDatabase temp = QSqlDatabase::addDatabase(s("QSQLITE"), s("temp"));
        CHECK(temp.open());
        QSqlQuery q(temp);
        CHECK(run(q, "CREATE TABLE x (a)") && run(q, "CREATE TEMP TABLE y (b)"));
        CHECK(temp.tables().size() == 2 && temp.tables().contains(s("y")));
    }
    QSqlDatabase::removeDatabase(s("temp"));
}

static void testSqliteExtras(const QString &dir) {
    const QString file = dir + s("/extras.db");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(s("QSQLITE"), s("extras"));
        db.setDatabaseName(file);
        CHECK(!db.driver()->handle().isValid());
        CHECK(db.open());
        QSqlQuery q(db);
        CHECK(run(q, "CREATE TABLE log (id INTEGER PRIMARY KEY, who TEXT UNIQUE)"));
        CHECK(run(q, "CREATE TABLE other (x REAL)"));

        // The handles of the client library.
        const QVariant handle = db.driver()->handle();
        CHECK(handle.isValid() && QByteArray(handle.typeName()) == "sqlite3*" && handle.value<sqlite3 *>() != nullptr);
        QSqlQuery prepared(db);
        CHECK(!prepared.result()->handle().isValid());
        CHECK(prepared.prepare(s("SELECT who FROM log WHERE id = ?")));
        CHECK(prepared.result()->handle().value<sqlite3_stmt *>() != nullptr);
        prepared.clear();
        CHECK(prepared.result()->handle().isValid()); // clear() keeps the statement of the driver until the next one

        // The table that a field of a query comes from, if this SQLite tells.
        CHECK(run(q, "SELECT who, 1 + 1 FROM log"));
        const QString table = q.record().field(0).tableName();
        CHECK((table.isEmpty() || table == s("log")) && q.record().field(1).tableName().isEmpty());

        // Notifications: a table that changes.
        QSqlDriver *driver = db.driver();
        CHECK(driver->hasFeature(QSqlDriver::EventNotifications));
        QStringList seen;
        QObject::connect(driver, &QSqlDriver::notification,
                         [&](const QString &name, QSqlDriver::NotificationSource source, const QVariant &payload) {
                             seen.append(name + QChar(':') + payload.toString() + QChar(':') + QString::number(source));
                         });
        CHECK(driver->subscribedToNotifications().isEmpty() && !driver->unsubscribeFromNotification(s("log")));
        CHECK(driver->subscribeToNotification(s("log")) && !driver->subscribeToNotification(s("log")));
        CHECK(!driver->subscribeToNotification(QString()) &&
              driver->subscribedToNotifications() == QStringList({s("log")}));
        CHECK(run(q, "INSERT INTO log (who) VALUES ('a')") && run(q, "INSERT INTO other VALUES (2.75)"));
        CHECK(seen.isEmpty()); // delivered from the event loop, not from inside the statement
        QCoreApplication::processEvents();
        CHECK(seen == QStringList({s("log:1:1")}));
        CHECK(run(q, "UPDATE log SET who = 'b' WHERE id = 1") && run(q, "DELETE FROM log WHERE id = 1"));
        QCoreApplication::processEvents();
        CHECK(seen.size() == 3 && seen.last() == s("log:1:1"));
        // A notification that is on its way when the subscription ends is dropped.
        CHECK(run(q, "INSERT INTO log (who) VALUES ('c')") && driver->unsubscribeFromNotification(s("log")));
        QCoreApplication::processEvents();
        CHECK(seen.size() == 3 && driver->subscribedToNotifications().isEmpty());
        CHECK(run(q, "INSERT INTO log (who) VALUES ('d')"));
        QCoreApplication::processEvents();
        CHECK(seen.size() == 3);
        // Subscriptions outlive a close and a new open.
        CHECK(driver->subscribeToNotification(s("other")));
        db.close();
        CHECK(!driver->subscribeToNotification(s("log")) && !driver->unsubscribeFromNotification(s("other")));
        CHECK(db.open() && q.exec(s("INSERT INTO other VALUES (1.5)")));
        QCoreApplication::processEvents();
        CHECK(seen.size() == 4 && seen.last().startsWith(s("other:")));
        CHECK(driver->unsubscribeFromNotification(s("other")));

        // Extended result codes are the default.
        CHECK(!q.exec(s("INSERT INTO log (who) VALUES ('c')")) && q.lastError().nativeErrorCode() == s("2067"));
        // No REGEXP unless it is asked for.
        CHECK(!q.exec(s("SELECT 'abc' REGEXP 'b+'")));

        // How real numbers are returned.
        CHECK(run(q, "SELECT x FROM other ORDER BY x DESC") && q.next() && q.value(0).typeId() == QMetaType::Double);
        q.setNumericalPrecisionPolicy(QSql::LowPrecisionInt32);
        CHECK(run(q, "SELECT x FROM other ORDER BY x DESC") && q.next() && q.value(0).typeId() == QMetaType::Int);
        CHECK(q.value(0).toInt() == 2);
        q.setNumericalPrecisionPolicy(QSql::LowPrecisionInt64);
        CHECK(run(q, "SELECT x FROM other ORDER BY x DESC") && q.next() && q.value(0).typeId() == QMetaType::LongLong);
        q.setNumericalPrecisionPolicy(QSql::HighPrecision);
        CHECK(run(q, "SELECT x FROM other ORDER BY x DESC") && q.next() && q.value(0).toDouble() == 2.75);
        db.setNumericalPrecisionPolicy(QSql::LowPrecisionInt32);
        QSqlQuery fresh(db);
        CHECK(fresh.numericalPrecisionPolicy() == QSql::LowPrecisionInt32);
        CHECK(run(fresh, "SELECT x FROM other ORDER BY x DESC") && fresh.next() &&
              fresh.value(0).typeId() == QMetaType::Int);
    }
    QSqlDatabase::removeDatabase(s("extras"));

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(s("QSQLITE"), s("options"));
        db.setDatabaseName(file);
        db.setConnectOptions(s("QSQLITE_ENABLE_REGEXP=2;QSQLITE_NO_USE_EXTENDED_RESULT_CODES"));
        CHECK(db.open());
        QSqlQuery q(db);
        CHECK(!q.exec(s("INSERT INTO log (who) VALUES ('c')")) && q.lastError().nativeErrorCode() == s("19"));
        CHECK(run(q, "SELECT 'abc' REGEXP 'b+', 'abc' REGEXP '^b', 'x' REGEXP NULL, NULL REGEXP 'x'") && q.next());
        CHECK(q.value(0).toInt() == 1 && q.value(1).toInt() == 0 && q.isNull(2) && q.isNull(3));
        CHECK(run(q, "SELECT count(*) FROM log WHERE who REGEXP '^[cd]$'") && q.next() && q.value(0).toInt() == 2);
        // More patterns than the cache holds.
        for (const char *pattern : {"a", "b", "c", "d", "a", "^c$"}) {
            CHECK(q.prepare(s("SELECT count(*) FROM log WHERE who REGEXP ?")));
            q.addBindValue(s(pattern));
            CHECK(q.exec() && q.next() &&
                  q.value(0).toInt() == (s(pattern).contains(QChar('c')) || s(pattern) == s("d") ? 1 : 0));
        }
        CHECK(!q.exec(s("SELECT 'abc' REGEXP '(unclosed'")) &&
              q.lastError().databaseText().contains(s("regular expression")));
        CHECK(q.exec(s("SELECT 1")));
    }
    QSqlDatabase::removeDatabase(s("options"));

    // A symbolic link to the database is followed, unless the connection says no.
    const QString link = dir + s("/link.db");
    if (QFile::link(file, link)) {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(s("QSQLITE"), s("link"));
            db.setDatabaseName(link);
            CHECK(db.open() && db.tables().contains(s("log")));
            db.close();
            db.setConnectOptions(s("QSQLITE_OPEN_NOFOLLOW"));
            CHECK(!db.open() && db.isOpenError() && db.lastError().isValid());
        }
        QSqlDatabase::removeDatabase(s("link"));
    }
}

// Models.

struct ModelSpy {
    explicit ModelSpy(QAbstractItemModel *model) {
        QObject::connect(model, &QAbstractItemModel::modelReset, [this] { ++resets; });
        QObject::connect(model, &QAbstractItemModel::rowsInserted,
                         [this](const QModelIndex &, int first, int last) { inserted += last - first + 1; });
        QObject::connect(model, &QAbstractItemModel::rowsRemoved,
                         [this](const QModelIndex &, int first, int last) { removed += last - first + 1; });
        QObject::connect(model, &QAbstractItemModel::dataChanged,
                         [this](const QModelIndex &, const QModelIndex &, const QList<int> &) { ++changed; });
    }
    int resets = 0;
    int inserted = 0;
    int removed = 0;
    int changed = 0;
};

static void testQueryModel() {
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec(s("DROP TABLE IF EXISTS t"));
    CHECK(run(q, "CREATE TABLE items (id INTEGER PRIMARY KEY, label TEXT, price REAL)"));
    CHECK(db.transaction() && q.prepare(s("INSERT INTO items (label, price) VALUES (?, ?)")));
    for (int i = 0; i < 600; ++i) {
        q.addBindValue(s("item") + QString::number(i));
        q.addBindValue(i % 10 == 0 ? QVariant() : QVariant(i * 0.5));
        CHECK(q.exec());
    }
    CHECK(db.commit());

    QSqlQueryModel model;
    ModelSpy spy(&model);
    CHECK(model.rowCount() == 0 && model.columnCount() == 0 && !model.canFetchMore() && model.record().isEmpty());
    CHECK(!model.data(model.index(0, 0)).isValid() && !model.query().isActive());

    model.setQuery(s("SELECT id, label, price FROM items ORDER BY id")); // the default connection
    CHECK(spy.resets == 1 && !model.lastError().isValid() && model.query().isActive());
    CHECK(model.columnCount() == 3 && model.rowCount() == 255 && model.canFetchMore());
    CHECK(model.rowCount(model.index(0, 0)) == 0 && model.columnCount(model.index(0, 0)) == 0);
    CHECK(!model.canFetchMore(model.index(0, 0)));
    CHECK(model.data(model.index(0, 0)).toInt() == 1 && model.data(model.index(254, 1)).toString() == s("item254"));
    CHECK(model.data(model.index(1, 2), Qt::EditRole).toDouble() == 0.5 && model.data(model.index(0, 2)).isNull());
    CHECK(!model.data(model.index(0, 0), Qt::DecorationRole).isValid() && !model.data(QModelIndex()).isValid());
    CHECK(model.headerData(1, Qt::Horizontal).toString() == s("label") &&
          model.headerData(0, Qt::Vertical).toInt() == 1);
    CHECK(!model.headerData(1, Qt::Horizontal, Qt::DecorationRole).isValid());
    CHECK(model.setHeaderData(1, Qt::Horizontal, s("Label")) &&
          model.headerData(1, Qt::Horizontal).toString() == s("Label"));
    CHECK(model.headerData(1, Qt::Horizontal, Qt::EditRole).toString() == s("Label"));
    CHECK(model.setHeaderData(2, Qt::Horizontal, 7, Qt::UserRole) &&
          model.headerData(2, Qt::Horizontal, Qt::UserRole).toInt() == 7);
    CHECK(model.headerData(2, Qt::Horizontal).toString() == s("price"));
    CHECK(!model.setHeaderData(9, Qt::Horizontal, s("x")) && !model.setHeaderData(0, Qt::Vertical, s("x")));
    CHECK(model.record().count() == 3 && model.record().isNull(0) &&
          model.record(3).value("label").toString() == s("item3"));
    CHECK(model.record(-1).isNull("label") && model.roleNames().value(Qt::DisplayRole) == "display");
    CHECK(!(model.flags(model.index(0, 0)) & Qt::ItemIsEditable));

    model.fetchMore(model.index(0, 0));
    CHECK(model.rowCount() == 255);
    model.fetchMore();
    CHECK(model.rowCount() == 510 && spy.inserted == 255 && model.canFetchMore());
    model.fetchMore();
    CHECK(model.rowCount() == 600 && spy.inserted == 345 && !model.canFetchMore());
    model.fetchMore();
    CHECK(model.rowCount() == 600 && spy.inserted == 345 && spy.resets == 1);
    CHECK(model.data(model.index(599, 1)).toString() == s("item599") && !model.data(model.index(600, 1)).isValid());

    // Columns that are not in the query, and columns that are hidden.
    CHECK(model.insertColumns(1, 2) && model.columnCount() == 5);
    CHECK(model.data(model.index(0, 0)).toInt() == 1 && !model.data(model.index(0, 1)).isValid());
    CHECK(model.data(model.index(0, 3)).toString() == s("item0") &&
          model.headerData(3, Qt::Horizontal).toString() == s("Label"));
    CHECK(model.removeColumns(0, 3) && model.columnCount() == 2 &&
          model.data(model.index(4, 0)).toString() == s("item4"));
    CHECK(model.data(model.index(1, 1)).toDouble() == 0.5 &&
          model.headerData(0, Qt::Horizontal).toString() == s("Label"));
    CHECK(!model.insertColumns(9, 1) && !model.removeColumns(1, 2) && !model.insertColumns(0, 0));
    CHECK(!model.insertColumns(0, 1, model.index(0, 0)) && !model.removeColumns(0, 1, model.index(0, 0)));

    // A new query replaces the old one, with its headers.
    QSqlQuery small(db);
    CHECK(small.exec(s("SELECT label FROM items WHERE id <= 3 ORDER BY id DESC")));
    model.setQuery(std::move(small));
    CHECK(spy.resets == 2 && model.rowCount() == 3 && model.columnCount() == 1 && !model.canFetchMore());
    CHECK(model.data(model.index(0, 0)).toString() == s("item2") &&
          model.headerData(0, Qt::Horizontal).toString() == s("label"));
    model.setQuery(s("SELECT label FROM items WHERE id < 0"));
    CHECK(model.rowCount() == 0 && model.columnCount() == 1 && !model.canFetchMore() && !model.lastError().isValid());

    // Errors.
    model.setQuery(s("SELECT nope FROM items"));
    CHECK(model.lastError().isValid() && model.rowCount() == 0 && model.columnCount() == 0 && !model.canFetchMore());
    model.setQuery(s("UPDATE items SET price = price"));
    CHECK(!model.lastError().isValid() && model.rowCount() == 0 && model.columnCount() == 0);
    QSqlQuery forward(db);
    forward.setForwardOnly(true);
    CHECK(forward.exec(s("SELECT label FROM items")));
    model.setQuery(std::move(forward));
    CHECK(model.lastError().isValid() && model.rowCount() == 0);

    model.setQuery(s("SELECT id FROM items"), db);
    CHECK(model.rowCount() == 255);
    model.clear();
    CHECK(model.rowCount() == 0 && model.columnCount() == 0 && !model.lastError().isValid() &&
          !model.query().isActive());
}

static QString column(const QSqlTableModel &model, int col) {
    QStringList values;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariant v = model.data(model.index(row, col));
        values.append(v.isNull() ? s("NULL") : v.toString());
    }
    return values.join(QChar(','));
}

static QString stored(const char *sql) {
    QSqlQuery q(s(sql));
    QStringList values;
    while (q.next())
        values.append(q.value(0).isNull() ? s("NULL") : q.value(0).toString());
    return values.join(QChar(','));
}

static void testTableModel() {
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    CHECK(run(
        q, "CREATE TABLE staff (id INTEGER PRIMARY KEY, name TEXT NOT NULL, \"pay grade\" INT DEFAULT 1, note TEXT)"));
    CHECK(run(q,
              "INSERT INTO staff (name, \"pay grade\", note) VALUES ('Cy', 3, NULL), ('Ada', 5, 'x'), ('Bo', 4, 'y')"));

    QSqlTableModel model; // the default connection
    ModelSpy spy(&model);
    CHECK(model.database().driver() == db.driver() && model.editStrategy() == QSqlTableModel::OnRowChange);
    CHECK(model.tableName().isEmpty() && !model.select() && model.lastError().isValid());
    CHECK(model.rowCount() == 0 && model.columnCount() == 0 && !model.isDirty() && model.primaryKey().isEmpty());
    CHECK(!model.insertRows(0, 1));

    model.setTable(s("nope"));
    CHECK(model.lastError().isValid() && !model.select() && model.columnCount() == 0);

    model.setTable(s("staff"));
    CHECK(model.tableName() == s("staff") && !model.lastError().isValid());
    CHECK(model.columnCount() == 4 && model.rowCount() == 0 && model.record().count() == 4);
    CHECK(model.fieldIndex(s("name")) == 1 && model.fieldIndex(s("pay grade")) == 2 &&
          model.fieldIndex(s("nope")) == -1);
    CHECK(model.primaryKey().count() == 1 && model.primaryKey().fieldName(0) == s("id"));
    CHECK(model.headerData(2, Qt::Horizontal).toString() == s("pay grade"));
    CHECK(model.select() && model.rowCount() == 3 && column(model, 1) == s("Cy,Ada,Bo"));
    CHECK(model.query().lastQuery() == s("SELECT \"id\", \"name\", \"pay grade\", \"note\" FROM \"staff\""));

    // Sorting and filtering.
    model.setSort(1, Qt::AscendingOrder);
    CHECK(column(model, 1) == s("Cy,Ada,Bo") && model.select() && column(model, 1) == s("Ada,Bo,Cy"));
    model.sort(2, Qt::DescendingOrder);
    CHECK(column(model, 1) == s("Ada,Bo,Cy") && column(model, 2) == s("5,4,3"));
    CHECK(model.query().lastQuery().endsWith(s("ORDER BY \"staff\".\"pay grade\" DESC")));
    model.setFilter(s("\"pay grade\" > 3"));
    CHECK(model.filter() == s("\"pay grade\" > 3") && model.rowCount() == 2 && column(model, 1) == s("Ada,Bo"));
    model.setFilter(s("this is not sql"));
    CHECK(model.lastError().isValid() && model.rowCount() == 0 && model.columnCount() == 4);
    model.setFilter(QString());
    model.setSort(0, Qt::AscendingOrder);
    CHECK(model.select() && !model.lastError().isValid() && column(model, 1) == s("Cy,Ada,Bo"));
    CHECK(model.record(1).value("name").toString() == s("Ada") && !model.record(1).isGenerated("name"));
    CHECK(model.flags(model.index(0, 1)) & Qt::ItemIsEditable);
    CHECK(!(model.flags(QModelIndex()) & Qt::ItemIsEditable));

    // OnManualSubmit: changes wait in the model.
    model.setEditStrategy(QSqlTableModel::OnManualSubmit);
    CHECK(model.editStrategy() == QSqlTableModel::OnManualSubmit);
    const int resets = spy.resets;
    CHECK(model.setData(model.index(1, 1), s("Ada L.")) && spy.changed == 1);
    CHECK(model.isDirty() && model.isDirty(model.index(1, 1)) && !model.isDirty(model.index(1, 2)));
    CHECK(!model.isDirty(model.index(0, 1)) && !model.isDirty(QModelIndex()));
    CHECK(model.data(model.index(1, 1)).toString() == s("Ada L.") && model.data(model.index(1, 2)).toInt() == 5);
    CHECK(model.record(1).isGenerated("name") && !model.record(1).isGenerated("note"));
    CHECK(model.record(1).value("name").toString() == s("Ada L."));
    CHECK(stored("SELECT name FROM staff ORDER BY id") == s("Cy,Ada,Bo"));
    CHECK(!model.setData(model.index(1, 1), s("x"), Qt::DisplayRole) && !model.setData(model.index(9, 1), s("x")));
    CHECK(!model.setData(QModelIndex(), s("x")) && !model.setData(model.index(1, 9), s("x")));
    CHECK(model.submit() && stored("SELECT name FROM staff ORDER BY id") == s("Cy,Ada,Bo")); // not with this strategy
    model.revert();
    CHECK(model.isDirty());
    model.revertRow(1);
    CHECK(!model.isDirty() && model.data(model.index(1, 1)).toString() == s("Ada"));
    model.revertRow(1);
    model.revertRow(77);

    // Updating, inserting and deleting in one go.
    int primed = 0;
    int before = 0;
    QObject::connect(&model, &QSqlTableModel::primeInsert, [&](int row, QSqlRecord &record) {
        ++primed;
        record.setValue(s("note"), s("primed") + QString::number(row));
        record.setGenerated(s("note"), true);
    });
    QObject::connect(&model, &QSqlTableModel::beforeInsert, [&](QSqlRecord &record) {
        ++before;
        if (record.value(s("name")).toString() == s("Eve"))
            record.setValue(s("name"), s("Eve!"));
    });
    QObject::connect(&model, &QSqlTableModel::beforeUpdate, [&](int, QSqlRecord &) { before += 10; });
    QObject::connect(&model, &QSqlTableModel::beforeDelete, [&](int) { before += 100; });

    CHECK(model.setData(model.index(0, 2), 9) && model.clearItemData(model.index(1, 3)));
    CHECK(model.insertRows(1, 2) && model.rowCount() == 5 && spy.inserted == 2 && primed == 2);
    CHECK(model.headerData(1, Qt::Vertical).toString() == s("*") && model.headerData(0, Qt::Vertical).toInt() == 1);
    CHECK(model.data(model.index(1, 3)).toString() == s("primed1") && model.data(model.index(1, 1)).isNull());
    // The rows behind the new ones moved down, with their pending changes.
    CHECK(column(model, 1) == s("Cy,NULL,NULL,Ada,Bo") && model.data(model.index(3, 3)).isNull());
    CHECK(model.isDirty(model.index(3, 3)) && model.isDirty(model.index(1, 0)) && model.isDirty(model.index(0, 2)));
    CHECK(model.setData(model.index(1, 1), s("Dee")) && model.setData(model.index(2, 1), s("Eve")));
    CHECK(model.setData(model.index(2, 2), 2));
    CHECK(model.removeRows(4, 1) && model.rowCount() == 5 && model.headerData(4, Qt::Vertical).toString() == s("!"));
    CHECK(!(model.flags(model.index(4, 1)) & Qt::ItemIsEditable) && !model.setData(model.index(4, 1), s("x")));
    CHECK(model.data(model.index(4, 1)).toString() == s("Bo") && model.isDirty(model.index(4, 0)));
    CHECK(model.primaryKey().count() == 1);
    CHECK(stored("SELECT count(*) FROM staff") == s("3"));
    CHECK(spy.resets == resets);
    CHECK(model.submitAll() && !model.isDirty() && spy.resets == resets + 1 && before == 122);
    CHECK(stored("SELECT name FROM staff ORDER BY id") == s("Cy,Ada,Dee,Eve!"));
    CHECK(stored("SELECT \"pay grade\" FROM staff ORDER BY id") == s("9,5,1,2"));
    CHECK(stored("SELECT note FROM staff ORDER BY id") == s("NULL,NULL,primed1,primed2"));
    CHECK(model.rowCount() == 4 && column(model, 1) == s("Cy,Ada,Dee,Eve!"));
    CHECK(model.submitAll() && spy.resets == resets + 1); // nothing to do

    // Reverting everything.
    CHECK(model.insertRows(0, 1) && model.insertRows(model.rowCount(), 1) && model.removeRows(2, 1));
    CHECK(model.setData(model.index(3, 1), s("changed")) && model.rowCount() == 6);
    CHECK(column(model, 1) == s("NULL,Cy,Ada,changed,Eve!,NULL"));
    CHECK(model.removeRows(0, 1) && model.rowCount() == 5 && column(model, 1) == s("Cy,Ada,changed,Eve!,NULL"));
    CHECK(model.headerData(1, Qt::Vertical).toString() == s("!"));
    model.revertAll();
    CHECK(!model.isDirty() && model.rowCount() == 4 && column(model, 1) == s("Cy,Ada,Dee,Eve!"));
    CHECK(model.headerData(1, Qt::Vertical).toInt() == 2);
    CHECK(stored("SELECT count(*) FROM staff") == s("4"));
    CHECK(!model.insertRows(9, 1) && !model.insertRows(0, 0) && !model.insertRows(0, 1, model.index(0, 0)));
    CHECK(!model.removeRows(4, 1) && !model.removeRows(0, 0) && !model.removeRows(3, 2) && !model.removeRows(-1, 1));

    // A change that the database refuses: the others before it stay written, the rest stays pending.
    CHECK(model.setData(model.index(0, 1), s("Cy 2")) && model.setData(model.index(1, 1), QVariant()));
    CHECK(model.setData(model.index(2, 1), s("Dee 2")));
    CHECK(!model.submitAll() && model.lastError().isValid() &&
          model.lastError().databaseText().contains(s("NOT NULL")));
    CHECK(model.isDirty() && !model.isDirty(model.index(0, 1)) && model.isDirty(model.index(2, 1)));
    CHECK(stored("SELECT name FROM staff ORDER BY id") == s("Cy 2,Ada,Dee,Eve!"));
    CHECK(model.setData(model.index(1, 1), s("Ada 2")) && model.submitAll());
    CHECK(stored("SELECT name FROM staff ORDER BY id") == s("Cy 2,Ada 2,Dee 2,Eve!"));
    // In a transaction, a refused change takes the others back with it.
    CHECK(model.setData(model.index(0, 1), s("Cy 3")) && model.setData(model.index(1, 1), QVariant()));
    CHECK(db.transaction() && !model.submitAll() && db.rollback());
    model.revertAll();
    CHECK(model.select() && column(model, 1) == s("Cy 2,Ada 2,Dee 2,Eve!"));

    // Records.
    QSqlRecord rec = model.record();
    rec.setValue(s("name"), s("Fay"));
    rec.setValue(s("pay grade"), 7);
    rec.setGenerated(s("id"), false);
    rec.setGenerated(s("note"), false);
    CHECK(model.insertRecord(-1, rec) && model.rowCount() == 5 && model.data(model.index(4, 1)).toString() == s("Fay"));
    QSqlRecord change;
    change.append(QSqlField(s("note")));
    change.append(QSqlField(s("ghost")));
    change.setValue(0, s("noted"));
    change.setValue(1, 1);
    CHECK(!model.setRecord(0, change) && model.data(model.index(0, 3)).toString() == s("noted")); // "ghost" is unknown
    CHECK(!model.setRecord(9, change) && !model.setRecord(-1, change));
    CHECK(model.submitAll() && stored("SELECT name || '/' || \"pay grade\" FROM staff WHERE id = 6") == s("Fay/7"));
    CHECK(stored("SELECT note FROM staff WHERE id = 1") == s("noted"));
    // A row without values gets the defaults of the table.
    CHECK(model.insertRows(0, 1) && model.setData(model.index(0, 1), s("Gil")) && model.submitAll());
    CHECK(stored("SELECT \"pay grade\" FROM staff WHERE name = 'Gil'") == s("1"));

    // OnRowChange: a row is written when another row is touched, or with submit().
    model.setEditStrategy(QSqlTableModel::OnRowChange);
    CHECK(model.select() && model.rowCount() == 6);
    const int rowResets = spy.resets;
    CHECK(model.setData(model.index(0, 3), s("a")) && model.setData(model.index(0, 2), 8) && model.isDirty());
    CHECK(stored("SELECT note FROM staff WHERE id = 1") == s("noted"));
    CHECK(model.setData(model.index(1, 3), s("b")));
    CHECK(stored("SELECT note || \"pay grade\" FROM staff WHERE id = 1") == s("a8"));
    CHECK(stored("SELECT note FROM staff WHERE id = 2") == s("NULL") && model.isDirty(model.index(1, 3)));
    CHECK(model.submit() && !model.isDirty() && stored("SELECT note FROM staff WHERE id = 2") == s("b"));
    // The rows that were written were read again by themselves: the model was not reset.
    CHECK(spy.resets == rowResets && model.data(model.index(0, 3)).toString() == s("a") &&
          model.data(model.index(0, 2)).toInt() == 8);
    CHECK(!model.isDirty(model.index(0, 3)) && !model.record(0).isGenerated(3) &&
          model.headerData(0, Qt::Vertical).toInt() == 1);
    // A row that was written can be changed again, and a change of its key is followed.
    CHECK(model.setData(model.index(1, 0), 20) && model.submit() &&
          stored("SELECT name FROM staff WHERE id = 20") == s("Ada 2"));
    CHECK(model.data(model.index(1, 0)).toInt() == 20 && model.setData(model.index(1, 0), 2) && model.submit());
    CHECK(stored("SELECT name FROM staff WHERE id = 2") == s("Ada 2") && spy.resets == rowResets);
    // selectRow() shows what somebody else wrote, and leaves the other rows alone.
    CHECK(run(q, "UPDATE staff SET note = 'behind' WHERE id = 4"));
    CHECK(model.data(model.index(2, 3)).toString() == s("primed1") && model.setData(model.index(0, 3), s("pending")));
    CHECK(model.selectRow(2) && model.data(model.index(2, 3)).toString() == s("behind") &&
          model.isDirty(model.index(0, 3)));
    CHECK(model.selectRow(0) && !model.isDirty() && model.data(model.index(0, 3)).toString() == s("a"));
    CHECK(run(q, "UPDATE staff SET note = 'primed1' WHERE id = 4") && model.selectRow(2) && spy.resets == rowResets);
    CHECK(model.setData(model.index(2, 3), s("c")));
    model.revert();
    CHECK(!model.isDirty() && stored("SELECT note FROM staff WHERE id = 4") == s("primed1"));
    CHECK(!model.insertRows(0, 2) && !model.removeRows(0, 2));
    CHECK(model.setData(model.index(2, 3), s("d")) && model.removeRows(0, 1));
    CHECK(stored("SELECT note FROM staff WHERE id = 4") == s("d") &&
          stored("SELECT count(*) FROM staff WHERE id = 1") == s("0"));
    CHECK(model.rowCount() == 5 && !model.isDirty());
    const int insertResets = spy.resets;
    CHECK(model.insertRecord(0, rec) && !model.isDirty() &&
          stored("SELECT count(*) FROM staff WHERE name = 'Fay'") == s("2"));
    // The new row stays where it was put, and shows the key and the default that the database gave it.
    CHECK(spy.resets == insertResets && model.rowCount() == 6 && model.data(model.index(0, 1)).toString() == s("Fay"));
    CHECK(model.data(model.index(0, 0)).toInt() == 8 && model.data(model.index(0, 3)).toString() == s("primed0"));
    CHECK(model.headerData(0, Qt::Vertical).toString() != s("*") && model.data(model.index(1, 0)).toInt() == 2);
    CHECK(model.setData(model.index(0, 3), s("again")) && model.submit() &&
          stored("SELECT note FROM staff WHERE id = 8") == s("again"));
    CHECK(model.setData(model.index(0, 3), QVariant()) && model.submit() && spy.resets == insertResets);
    CHECK(model.insertRows(0, 1) && model.isDirty() && model.removeRows(0, 1) && !model.isDirty() &&
          model.rowCount() == 6);
    // A new row that the database refuses stays in the model.
    CHECK(model.insertRows(0, 1) && !model.submit() && model.isDirty() && model.rowCount() == 7);
    CHECK(!model.setData(model.index(3, 3), s("blocked")));
    model.revertAll();
    CHECK(model.rowCount() == 6);

    // OnFieldChange: every item is written at once.
    model.setEditStrategy(QSqlTableModel::OnFieldChange);
    CHECK(model.select() && model.data(model.index(5, 0)).toInt() == 8);
    CHECK(model.setData(model.index(0, 3), s("now")) && !model.isDirty());
    CHECK(stored("SELECT note FROM staff WHERE id = 2") == s("now"));
    CHECK(!model.setData(model.index(0, 1), QVariant()) && model.lastError().isValid() && model.isDirty());
    model.revertAll();
    CHECK(model.setRecord(1, change) == false && stored("SELECT note FROM staff WHERE id = 4") == s("noted"));
    CHECK(model.selectRow(0) && !model.selectRow(99) && model.rowCount() == 6);

    // Values with quotes, and names that need quoting, are safe.
    model.setEditStrategy(QSqlTableModel::OnManualSubmit);
    CHECK(model.setData(model.index(0, 1), s("O'Neil\"; DROP TABLE staff; --")) && model.submitAll());
    CHECK(stored("SELECT name FROM staff WHERE id = 2") == s("O'Neil\"; DROP TABLE staff; --"));
    CHECK(db.tables().contains(s("staff")));

    // Hiding a column: it is neither read nor written.
    CHECK(model.removeColumns(3, 1) && model.columnCount() == 3 && model.rowCount() == 6);
    CHECK(model.insertRows(0, 1) && model.setData(model.index(0, 1), s("Hal")) && model.submitAll());
    CHECK(stored("SELECT note FROM staff WHERE name = 'Hal'") == s("NULL"));
    CHECK(!model.removeColumns(2, 2) && !model.removeColumns(0, 1, model.index(0, 0)));

    // A table without a primary key: the whole row finds it.
    CHECK(run(q, "CREATE TABLE pairs (a TEXT, b INT)"));
    CHECK(run(q, "INSERT INTO pairs VALUES ('x', 1), ('y', NULL), ('z', 3)"));
    QSqlTableModel pairs(nullptr, db);
    pairs.setEditStrategy(QSqlTableModel::OnManualSubmit);
    pairs.setTable(s("pairs"));
    CHECK(pairs.primaryKey().isEmpty() && pairs.select() && pairs.rowCount() == 3);
    CHECK(pairs.setData(pairs.index(1, 0), s("why")) && pairs.setData(pairs.index(0, 1), 10) && pairs.removeRows(2, 1));
    CHECK(pairs.submitAll());
    CHECK(stored("SELECT a || ':' || ifnull(b, 'null') FROM pairs ORDER BY rowid") == s("x:10,why:null"));

    // A view cannot be written.
    CHECK(run(q, "CREATE VIEW names AS SELECT name FROM staff"));
    QSqlTableModel view(nullptr, db);
    view.setEditStrategy(QSqlTableModel::OnManualSubmit);
    view.setTable(s("names"));
    CHECK(view.select() && view.rowCount() == 7 && view.setData(view.index(0, 0), s("no")));
    CHECK(!view.submitAll() && view.lastError().isValid() && view.isDirty());

    // Many rows: the model reads them in steps, and new rows count on top.
    CHECK(run(q, "CREATE TABLE big (id INTEGER PRIMARY KEY, v INT)"));
    CHECK(db.transaction() &&
          run(q, "WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM c WHERE x < 400) "
                 "INSERT INTO big (v) SELECT x FROM c") &&
          db.commit());
    QSqlTableModel big(nullptr, db);
    big.setEditStrategy(QSqlTableModel::OnManualSubmit);
    big.setTable(s("big"));
    CHECK(big.select() && big.rowCount() == 255 && big.canFetchMore());
    CHECK(big.insertRows(0, 1) && big.rowCount() == 256 && big.data(big.index(1, 1)).toInt() == 1);
    big.fetchMore();
    CHECK(big.rowCount() == 401 && !big.canFetchMore() && big.data(big.index(400, 1)).toInt() == 400);
    CHECK(big.setData(big.index(0, 1), 0) && big.setData(big.index(400, 1), -400) && big.submitAll());
    CHECK(stored("SELECT count(*) || '/' || min(v) || '/' || max(v) FROM big") == s("401/-400/399"));

    model.clear();
    CHECK(model.tableName().isEmpty() && model.columnCount() == 0 && model.rowCount() == 0 && model.filter().isEmpty());
    CHECK(model.editStrategy() == QSqlTableModel::OnManualSubmit);
}

static void testRelationalModel() {
    QSqlRelation none;
    QSqlRelation rel(s("city"), s("id"), s("name"));
    CHECK(!none.isValid() && rel.isValid() && !QSqlRelation(s("city"), QString(), s("name")).isValid());
    CHECK(rel.tableName() == s("city") && rel.indexColumn() == s("id") && rel.displayColumn() == s("name"));
    none.swap(rel);
    CHECK(none.isValid() && !rel.isValid() && none.tableName() == s("city"));

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    CHECK(run(q, "CREATE TABLE city (id INTEGER PRIMARY KEY, name TEXT)"));
    CHECK(run(q, "CREATE TABLE country (code TEXT PRIMARY KEY, name TEXT)"));
    CHECK(run(q, "CREATE TABLE person (id INTEGER PRIMARY KEY, name TEXT, city INT, country TEXT)"));
    CHECK(q.prepare(s("INSERT INTO city VALUES (?, ?)")));
    q.addBindValue(QVariantList{1, 2, 3});
    q.addBindValue(QVariantList{s("Oslo"), s("Rome"), s("Zürich")});
    CHECK(q.execBatch());
    CHECK(run(q, "INSERT INTO country VALUES ('no', 'Norway'), ('it', 'Italy')"));
    CHECK(run(q, "INSERT INTO person VALUES (1, 'Ann', 1, 'no'), (2, 'Bob', 2, 'it'), (3, 'Cat', NULL, 'no'), "
                 "(4, 'Dan', 9, 'it')"));

    QSqlRelationalTableModel model(nullptr, db);
    ModelSpy spy(&model);
    model.setEditStrategy(QSqlTableModel::OnManualSubmit);
    model.setTable(s("person"));
    CHECK(!model.relation(2).isValid() && model.relationModel(2) == nullptr);
    // Without relations it is a table model.
    CHECK(model.select() && model.rowCount() == 4 && model.data(model.index(0, 2)).toInt() == 1);
    model.setRelation(2, QSqlRelation(s("city"), s("id"), s("name")));
    model.setRelation(3, QSqlRelation(s("country"), s("code"), s("name")));
    model.setRelation(-1, QSqlRelation(s("city"), s("id"), s("name")));
    CHECK(model.relation(2).tableName() == s("city") && model.relation(3).indexColumn() == s("code"));
    CHECK(!model.relation(1).isValid() && !model.relation(99).isValid() && !model.relation(-1).isValid());

    // An inner join leaves out the rows whose keys find nothing.
    CHECK(model.select() && model.rowCount() == 2 && model.columnCount() == 4);
    CHECK(column(model, 1) == s("Ann,Bob") && column(model, 2) == s("Oslo,Rome") &&
          column(model, 3) == s("Norway,Italy"));
    // Three columns would be called "name": the two of the relations get names of their own.
    CHECK(model.record().fieldName(1) == s("name") && model.record().fieldName(2) == s("city_name_2"));
    CHECK(model.headerData(3, Qt::Horizontal).toString() == s("country_name_3"));
    CHECK(model.query().lastQuery().contains(
        s("INNER JOIN \"city\" relTblAl_2 ON (\"person\".\"city\" = relTblAl_2.\"id\")")));

    model.setJoinMode(QSqlRelationalTableModel::LeftJoin);
    CHECK(model.select() && model.rowCount() == 4 && column(model, 2) == s("Oslo,Rome,NULL,NULL"));
    CHECK(column(model, 3) == s("Norway,Italy,Norway,Italy"));

    // Sorting by a relation sorts by what is shown; the filter can name the other table.
    model.setSort(2, Qt::DescendingOrder);
    CHECK(model.select() && column(model, 2).startsWith(s("Rome,Oslo")));
    model.setSort(1, Qt::DescendingOrder);
    CHECK(model.select() && column(model, 1) == s("Dan,Cat,Bob,Ann"));
    model.setSort(0, Qt::AscendingOrder);
    model.setFilter(s("relTblAl_2.name = 'Oslo' OR relTblAl_3.name = 'Italy'"));
    CHECK(column(model, 1) == s("Ann,Bob,Dan"));
    model.setFilter(QString());
    CHECK(model.rowCount() == 4);

    // The other table, to choose a key from.
    QSqlTableModel *cities = model.relationModel(2);
    CHECK(cities && cities->rowCount() == 3 && cities->tableName() == s("city") &&
          static_cast<QObject *>(cities)->parent() == &model);
    CHECK(model.relationModel(2) == cities && model.relationModel(1) == nullptr && model.relationModel(42) == nullptr);

    // A foreign key is set as a key, and shown as what it stands for.
    const int resets = spy.resets;
    CHECK(model.setData(model.index(0, 2), 3) && model.isDirty(model.index(0, 2)));
    CHECK(model.data(model.index(0, 2)).toString() == s("Zürich") &&
          model.data(model.index(0, 3)).toString() == s("Norway"));
    CHECK(!model.setData(model.index(0, 2), 77) && model.data(model.index(0, 2)).toString() == s("Zürich"));
    CHECK(!model.setData(model.index(0, 3), s("xx")) && model.setData(model.index(0, 3), s("it")));
    CHECK(model.data(model.index(0, 3)).toString() == s("Italy") && model.setData(model.index(1, 2), QVariant()));
    CHECK(model.data(model.index(1, 2)).isNull() && model.setData(model.index(0, 1), s("Anne")));
    CHECK(stored("SELECT city FROM person WHERE id = 1") == s("1") && spy.resets == resets);
    model.revertRow(1);
    CHECK(model.data(model.index(1, 2)).toString() == s("Rome"));
    CHECK(model.submitAll() && !model.isDirty());
    CHECK(stored("SELECT name || city || country FROM person WHERE id = 1") == s("Anne3it"));
    CHECK(column(model, 2) == s("Zürich,Rome,NULL,NULL") && column(model, 3) == s("Italy,Italy,Norway,Italy"));

    // A new row.
    CHECK(model.insertRows(4, 1) && model.setData(model.index(4, 1), s("Eli")) && model.setData(model.index(4, 2), 2));
    CHECK(model.setData(model.index(4, 3), s("no")) && model.data(model.index(4, 2)).toString() == s("Rome"));
    CHECK(model.submitAll() && model.rowCount() == 5 && model.data(model.index(4, 3)).toString() == s("Norway"));
    CHECK(stored("SELECT city || country FROM person WHERE name = 'Eli'") == s("2no"));
    CHECK(model.removeRows(4, 1) && model.submitAll() && stored("SELECT count(*) FROM person") == s("4"));

    // Written at once, the row is read again through the join, without a reset.
    model.setEditStrategy(QSqlTableModel::OnFieldChange);
    const int fieldResets = spy.resets;
    CHECK(model.setData(model.index(1, 3), s("no")) && !model.isDirty() && spy.resets == fieldResets);
    CHECK(model.data(model.index(1, 3)).toString() == s("Norway") &&
          stored("SELECT country FROM person WHERE id = 2") == s("no"));
    CHECK(model.setData(model.index(1, 2), 1) && model.data(model.index(1, 2)).toString() == s("Oslo"));
    CHECK(model.setData(model.index(1, 1), s("Rob")) && model.data(model.index(1, 2)).toString() == s("Oslo"));
    CHECK(stored("SELECT name || city FROM person WHERE id = 2") == s("Rob1") && spy.resets == fieldResets);
    // A key that finds nothing any more: the row stays, and shows the key.
    CHECK(model.setData(model.index(2, 2), QVariant()) && spy.resets == fieldResets);

    // A new city is known after the next select().
    CHECK(run(q, "INSERT INTO city VALUES (4, 'Bern')") && !model.setData(model.index(0, 2), 4));
    CHECK(model.select() && model.setData(model.index(0, 2), 4) &&
          model.data(model.index(0, 2)).toString() == s("Bern"));

    // Removing a column moves the relations behind it.
    model.setEditStrategy(QSqlTableModel::OnManualSubmit);
    CHECK(model.removeColumns(2, 1) && model.columnCount() == 3 && model.relation(2).tableName() == s("country"));
    CHECK(!model.relation(3).isValid() && model.rowCount() == 4 && column(model, 2) == s("Italy,Norway,Norway,Italy"));
    CHECK(model.record().fieldName(2) == s("country_name_2"));
    CHECK(model.setData(model.index(3, 2), s("no")) && model.submitAll());
    CHECK(stored("SELECT country || ifnull(city, '-') FROM person WHERE id = 4") == s("no9"));
    CHECK(!model.removeColumns(2, 5) && !model.removeColumns(0, 1, model.index(0, 0)));

    model.clear();
    CHECK(!model.relation(2).isValid() && model.tableName().isEmpty() && model.rowCount() == 0);

    // A table without a primary key is found by its columns that have no relation.
    CHECK(run(q, "CREATE TABLE visit (who TEXT, city INT)") && run(q, "INSERT INTO visit VALUES ('x', 1), ('y', 2)"));
    QSqlRelationalTableModel visits(nullptr, db);
    visits.setEditStrategy(QSqlTableModel::OnManualSubmit);
    visits.setTable(s("visit"));
    visits.setRelation(1, QSqlRelation(s("city"), s("id"), s("name")));
    CHECK(visits.select() && visits.rowCount() == 2 && visits.record().fieldName(1) == s("name"));
    CHECK(visits.setData(visits.index(1, 1), 3) && visits.removeRows(0, 1) && visits.submitAll());
    CHECK(stored("SELECT who || city FROM visit") == s("y3"));
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testError();
    testField();
    testRecord();
    testDriverBase();
    testConnections();
    testQueryWithDriver();
    QSqlDatabase::removeDatabase(s("mem"));
    CHECK(MemoryDriver::instances == 0);

    if (QSqlDatabase::isDriverAvailable(s("QSQLITE"))) {
        QTemporaryDir dir;
        CHECK(dir.isValid());
        testSqliteBasics(dir.path());
        testSqliteStatements();
        testSqliteConnections(dir.path());
        testSqliteExtras(dir.path());
        testQueryModel();
        testTableModel();
        testRelationalModel();
        QSqlDatabase::removeDatabase(QLatin1StringView(QSqlDatabase::defaultConnection));
        CHECK(QSqlDatabase::connectionNames().isEmpty());
    } else {
        std::puts("Nt was built without SQLite: the QSQLITE tests are skipped");
        CHECK(QSqlDatabase::drivers() == QStringList({s("QMEMORY")}));
    }

    if (g_failures) {
        std::printf("%d sql test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all sql tests passed");
    return 0;
}
