// MIT License - see LICENSE
#include "qsqlitedriver_p.h"

#include <QtCore/qcache.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qregularexpression.h>
#include <QtSql/qsqlfield.h>
#include <QtSql/qsqlindex.h>

#include <sqlite3.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace {

QString fromUtf8(const char *text) { return text ? QString::fromUtf8(text) : QString(); }

QSqlError makeError(sqlite3 *db, const QString &what, QSqlError::ErrorType type) {
    const int code = db ? sqlite3_errcode(db) : SQLITE_CANTOPEN; // extended, if the connection has that on
    return QSqlError(what, db ? fromUtf8(sqlite3_errmsg(db)) : QStringLiteral("The database is not open"), type,
                     QString::number(code));
}

// The type that a column of the declared type decl holds, by the affinity rules of SQLite, with the names
// of dates and booleans, which SQLite stores as text and numbers, kept apart.
QMetaType typeOfDeclaration(const QString &decl) {
    const QString t = decl.toLower();
    if (t.isEmpty())
        return QMetaType();
    if (t == QStringLiteral("bool") || t == QStringLiteral("boolean"))
        return QMetaType(QMetaType::Bool);
    if (t == QStringLiteral("date"))
        return QMetaType(QMetaType::QDate);
    if (t == QStringLiteral("time"))
        return QMetaType(QMetaType::QTime);
    if (t == QStringLiteral("datetime") || t == QStringLiteral("timestamp"))
        return QMetaType(QMetaType::QDateTime);
    if (t.contains(QStringLiteral("int")))
        return QMetaType(t == QStringLiteral("int") || t == QStringLiteral("integer") ? QMetaType::Int
                                                                                      : QMetaType::LongLong);
    if (t.contains(QStringLiteral("char")) || t.contains(QStringLiteral("clob")) || t.contains(QStringLiteral("text")))
        return QMetaType(QMetaType::QString);
    if (t.contains(QStringLiteral("blob")))
        return QMetaType(QMetaType::QByteArray);
    if (t.contains(QStringLiteral("real")) || t.contains(QStringLiteral("floa")) ||
        t.contains(QStringLiteral("doub")) || t.startsWith(QStringLiteral("numeric")) ||
        t.startsWith(QStringLiteral("decimal")))
        return QMetaType(QMetaType::Double);
    return QMetaType(QMetaType::QString);
}

QMetaType typeOfValue(int sqliteType) {
    switch (sqliteType) {
    case SQLITE_INTEGER:
        return QMetaType(QMetaType::LongLong);
    case SQLITE_FLOAT:
        return QMetaType(QMetaType::Double);
    case SQLITE_BLOB:
        return QMetaType(QMetaType::QByteArray);
    case SQLITE_TEXT:
        return QMetaType(QMetaType::QString);
    default:
        return QMetaType();
    }
}

// The SQL function behind "subject REGEXP pattern", which SQLite leaves to the application. Compiled
// patterns are kept in a cache that belongs to the connection.
using RegexpCache = QCache<QString, QRegularExpression>;

void regexpFunction(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc != 2) {
        sqlite3_result_error(context, "regexp() takes a pattern and a text", -1);
        return;
    }
    const auto *patternText = reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const auto *subjectText = reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    if (!patternText || !subjectText) {
        sqlite3_result_null(context); // NULL matches nothing and nothing matches NULL, as with LIKE
        return;
    }
    try {
        const QString pattern = QString::fromUtf8(patternText, sqlite3_value_bytes(argv[0]));
        const QString subject = QString::fromUtf8(subjectText, sqlite3_value_bytes(argv[1]));
        auto *cache = static_cast<RegexpCache *>(sqlite3_user_data(context));
        const QRegularExpression *regex = cache->object(pattern);
        if (!regex) {
            auto compiled = std::make_unique<QRegularExpression>(pattern);
            if (!compiled->isValid()) {
                sqlite3_result_error(context, "invalid regular expression", -1);
                return;
            }
            // The cache may refuse the object and delete it, so match with it first.
            const bool matched = compiled->match(subject).hasMatch();
            cache->insert(pattern, compiled.release());
            sqlite3_result_int(context, matched ? 1 : 0);
            return;
        }
        sqlite3_result_int(context, regex->match(subject).hasMatch() ? 1 : 0);
    } catch (const std::bad_alloc &) {
        sqlite3_result_error_nomem(context);
    } catch (...) {
        sqlite3_result_error(context, "regexp() failed", -1);
    }
}

void destroyRegexpCache(void *cache) { delete static_cast<RegexpCache *>(cache); }

} // namespace

QSQLiteDriver::QSQLiteDriver(QObject *parent)
    : QSqlDriver(parent), m_connection(std::make_shared<QSQLiteConnection>()) {
    setDbmsType(SQLite);
}

QSQLiteDriver::~QSQLiteDriver() { QSQLiteDriver::close(); }

bool QSQLiteDriver::hasFeature(DriverFeature f) const {
    switch (f) {
    case BLOB:
    case Transactions:
    case Unicode:
    case LastInsertId:
    case PreparedQueries:
    case PositionalPlaceholders:
    case NamedPlaceholders:
    case SimpleLocking:
    case FinishQuery:
    case LowPrecisionNumbers:
    case EventNotifications:
        return true;
    case QuerySize:
    case BatchOperations:
    case MultipleResultSets:
    case CancelQuery:
        return false;
    }
    return false;
}

bool QSQLiteDriver::open(const QString &db, const QString &user, const QString &password, const QString &host, int port,
                         const QString &connOpts) {
    static_cast<void>(user);
    static_cast<void>(password);
    static_cast<void>(host);
    static_cast<void>(port);
    if (isOpen())
        close();

    int timeout = 5000;
    bool readOnly = false;
    bool uri = false;
    bool sharedCache = false;
    bool noFollow = false;
    bool extendedCodes = true;
    int regexpCache = -1; // negative: no REGEXP function
    for (const QString &part : connOpts.split(QChar(';'))) {
        const QString option = part.trimmed();
        if (option.isEmpty())
            continue;
        const qsizetype eq = option.indexOf(QChar('='));
        const QString name = (eq < 0 ? option : option.left(eq)).trimmed();
        const QString value = eq < 0 ? QString() : option.mid(eq + 1).trimmed();
        if (name == QStringLiteral("QSQLITE_BUSY_TIMEOUT")) {
            bool ok = false;
            const int n = value.toInt(&ok);
            if (ok && n >= 0)
                timeout = n;
        } else if (name == QStringLiteral("QSQLITE_OPEN_READONLY")) {
            readOnly = true;
        } else if (name == QStringLiteral("QSQLITE_OPEN_URI")) {
            uri = true;
        } else if (name == QStringLiteral("QSQLITE_ENABLE_SHARED_CACHE")) {
            sharedCache = true;
        } else if (name == QStringLiteral("QSQLITE_OPEN_NOFOLLOW")) {
            noFollow = true;
        } else if (name == QStringLiteral("QSQLITE_NO_USE_EXTENDED_RESULT_CODES")) {
            extendedCodes = false;
        } else if (name == QStringLiteral("QSQLITE_ENABLE_REGEXP")) {
            bool ok = false;
            const int n = value.toInt(&ok);
            regexpCache = ok && n > 0 ? n : 25;
        }
    }
    int flags = readOnly ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    flags |= sharedCache ? SQLITE_OPEN_SHAREDCACHE : SQLITE_OPEN_PRIVATECACHE;
    if (uri)
        flags |= SQLITE_OPEN_URI;
#ifdef SQLITE_OPEN_NOFOLLOW
    if (noFollow)
        flags |= SQLITE_OPEN_NOFOLLOW;
#else
    if (noFollow) {
        setLastError(QSqlError(QStringLiteral("Error opening database"),
                               QStringLiteral("This SQLite cannot refuse symbolic links"), QSqlError::ConnectionError));
        setOpenError(true);
        return false;
    }
#endif
    // Each connection is used by one thread at a time, but different connections by different threads.
    flags |= SQLITE_OPEN_NOMUTEX;

    sqlite3 *handle = nullptr;
    const int rc = sqlite3_open_v2(db.toUtf8().constData(), &handle, flags, nullptr);
    if (rc != SQLITE_OK) {
        setLastError(makeError(handle, QStringLiteral("Error opening database"), QSqlError::ConnectionError));
        setOpenError(true);
        if (handle)
            sqlite3_close(handle);
        return false;
    }
    sqlite3_busy_timeout(handle, timeout);
    sqlite3_extended_result_codes(handle, extendedCodes ? 1 : 0);
    if (regexpCache > 0) {
        // SQLite calls destroyRegexpCache() also when it cannot add the function.
        auto *cache = new RegexpCache(regexpCache);
        if (sqlite3_create_function_v2(handle, "regexp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, cache, &regexpFunction,
                                       nullptr, nullptr, &destroyRegexpCache) != SQLITE_OK) {
            setLastError(makeError(handle, QStringLiteral("Error opening database"), QSqlError::ConnectionError));
            setOpenError(true);
            sqlite3_close(handle);
            return false;
        }
    }
    m_connection->db = handle;
    if (!m_notifications.isEmpty())
        sqlite3_update_hook(handle, &QSQLiteDriver::updateHook, this);
    setLastError(QSqlError());
    setOpen(true);
    setOpenError(false);
    return true;
}

void QSQLiteDriver::close() {
    // The statements have to go before the database can be closed; their results stay behind, inactive.
    const std::vector<QSQLiteResult *> results = m_connection->results;
    for (QSQLiteResult *result : results)
        result->finalize();
    if (m_connection->db) {
        sqlite3_update_hook(m_connection->db, nullptr, nullptr);
        if (sqlite3_close(m_connection->db) != SQLITE_OK)
            setLastError(
                makeError(m_connection->db, QStringLiteral("Error closing database"), QSqlError::ConnectionError));
        m_connection->db = nullptr;
    }
    setOpen(false);
    setOpenError(false);
}

QSqlResult *QSQLiteDriver::createResult() const { return new QSQLiteResult(this, m_connection); }

QVariant QSQLiteDriver::handle() const { return m_connection->db ? QVariant::fromValue(m_connection->db) : QVariant(); }

// Called by SQLite while it changes a row. The database must not be used from here, so the signal is
// emitted later, from the event loop of the thread of the driver.
void QSQLiteDriver::updateHook(void *driver, int operation, const char *database, const char *table, long long rowId) {
    static_cast<void>(operation);
    static_cast<void>(database);
    auto *self = static_cast<QSQLiteDriver *>(driver);
    try {
        const QString name = fromUtf8(table);
        if (!self->m_notifications.contains(name))
            return;
        QMetaObject::invokeMethod(
            self,
            [self, name, rowId] {
                if (self->m_notifications.contains(name))
                    self->notification(name, QSqlDriver::SelfSource, QVariant(static_cast<qlonglong>(rowId)));
            },
            Qt::QueuedConnection);
    } catch (...) { // NOLINT(bugprone-empty-catch): an exception must not pass through SQLite; the notification is lost
    }
}

bool QSQLiteDriver::subscribeToNotification(const QString &name) {
    if (!isOpen() || !m_connection->db || name.isEmpty() || m_notifications.contains(name))
        return false;
    m_notifications.append(name);
    if (m_notifications.size() == 1)
        sqlite3_update_hook(m_connection->db, &QSQLiteDriver::updateHook, this);
    return true;
}

bool QSQLiteDriver::unsubscribeFromNotification(const QString &name) {
    if (!isOpen() || !m_connection->db || !m_notifications.contains(name))
        return false;
    m_notifications.removeAll(name);
    if (m_notifications.isEmpty())
        sqlite3_update_hook(m_connection->db, nullptr, nullptr);
    return true;
}

QStringList QSQLiteDriver::subscribedToNotifications() const { return m_notifications; }

bool QSQLiteDriver::run(const char *sql, const QString &what) {
    if (!isOpen() || !m_connection->db)
        return false;
    if (sqlite3_exec(m_connection->db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        setLastError(makeError(m_connection->db, what, QSqlError::TransactionError));
        return false;
    }
    return true;
}

bool QSQLiteDriver::beginTransaction() { return run("BEGIN", QStringLiteral("Unable to begin transaction")); }
bool QSQLiteDriver::commitTransaction() { return run("COMMIT", QStringLiteral("Unable to commit transaction")); }
bool QSQLiteDriver::rollbackTransaction() { return run("ROLLBACK", QStringLiteral("Unable to rollback transaction")); }

QStringList QSQLiteDriver::tables(QSql::TableType type) const {
    QStringList names;
    if (!isOpen() || !m_connection->db)
        return names;
    if (type & (QSql::Tables | QSql::Views)) {
        const char *sql = (type & QSql::Tables) && (type & QSql::Views)
                              ? "SELECT name FROM sqlite_master WHERE type IN ('table','view') AND name NOT LIKE "
                                "'sqlite!_%' ESCAPE '!' UNION ALL SELECT name FROM sqlite_temp_master WHERE type IN "
                                "('table','view')"
                          : (type & QSql::Tables)
                              ? "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE "
                                "'sqlite!_%' ESCAPE '!' UNION ALL SELECT name FROM sqlite_temp_master WHERE "
                                "type = 'table'"
                              : "SELECT name FROM sqlite_master WHERE type = 'view' UNION ALL SELECT name FROM "
                                "sqlite_temp_master WHERE type = 'view'";
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_connection->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW)
                names.append(fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))));
        }
        sqlite3_finalize(stmt);
    }
    if (type & QSql::SystemTables)
        names.append(QStringLiteral("sqlite_master")); // the only system table that can always be read
    return names;
}

// The columns of a table, or only those of its primary key, from PRAGMA table_info.
QSqlIndex QSQLiteDriver::tableInfo(const QString &tableName, bool onlyPrimaryKey) const {
    QSqlIndex index(tableName);
    if (!isOpen() || !m_connection->db)
        return index;
    QString table = stripDelimiters(tableName, TableName);
    QString schema;
    const qsizetype dot = table.indexOf(QChar('.'));
    if (dot > 0 && !isIdentifierEscaped(tableName, TableName)) {
        schema = table.left(dot);
        table = table.mid(dot + 1);
    }
    // PRAGMA takes no bound values, so the name goes into the statement, quoted as an identifier.
    QString sql = QStringLiteral("PRAGMA ");
    if (!schema.isEmpty())
        sql += escapeIdentifier(schema, TableName) + QChar('.');
    sql += QStringLiteral("table_info(") + escapeIdentifier(table, TableName) + QChar(')');
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_connection->db, sql.toUtf8().constData(), -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return index;
    }
    struct Column {
        QSqlField field;
        int pk;
    };
    std::vector<Column> columns;
    int keyColumns = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const QString name = fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        const QString decl = fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        const bool notNull = sqlite3_column_int(stmt, 3) != 0;
        const int pk = sqlite3_column_int(stmt, 5);
        QSqlField field(name, typeOfDeclaration(decl), table);
        field.setRequired(notNull);
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            QString def = fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
            if (def.size() >= 2 && def.startsWith(QChar('\'')) && def.endsWith(QChar('\'')))
                def = def.mid(1, def.size() - 2).replace(QStringLiteral("''"), QStringLiteral("'"));
            field.setDefaultValue(def);
        }
        // An INTEGER PRIMARY KEY column is the row id, which SQLite fills in.
        if (pk == 1 && decl.compare(QStringLiteral("integer"), Qt::CaseInsensitive) == 0)
            field.setAutoValue(true);
        if (pk > 0)
            ++keyColumns;
        columns.push_back(Column{field, pk});
    }
    sqlite3_finalize(stmt);
    if (onlyPrimaryKey) {
        std::stable_sort(columns.begin(), columns.end(), [](const Column &a, const Column &b) { return a.pk < b.pk; });
    }
    for (Column &c : columns) {
        if (keyColumns != 1)
            c.field.setAutoValue(false); // only a key of one column is the row id
        if (!onlyPrimaryKey || c.pk > 0)
            index.append(c.field);
    }
    return index;
}

QSqlRecord QSQLiteDriver::record(const QString &tableName) const { return tableInfo(tableName, false); }

QSqlIndex QSQLiteDriver::primaryIndex(const QString &tableName) const { return tableInfo(tableName, true); }

QString QSQLiteDriver::escapeIdentifier(const QString &identifier, IdentifierType type) const {
    static_cast<void>(type);
    if (identifier.isEmpty() || isIdentifierEscaped(identifier, type))
        return identifier;
    QString escaped = identifier;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QChar('"') + escaped + QChar('"');
}

bool QSQLiteDriver::isIdentifierEscaped(const QString &identifier, IdentifierType type) const {
    static_cast<void>(type);
    if (identifier.size() < 2)
        return false;
    const QChar first = identifier.at(0);
    const QChar last = identifier.at(identifier.size() - 1);
    return (first == QChar('"') && last == QChar('"')) || (first == QChar('`') && last == QChar('`')) ||
           (first == QChar('[') && last == QChar(']'));
}

QString QSQLiteDriver::stripDelimiters(const QString &identifier, IdentifierType type) const {
    if (!isIdentifierEscaped(identifier, type))
        return identifier;
    const QChar quote = identifier.at(0);
    QString inner = identifier.mid(1, identifier.size() - 2);
    if (quote != QChar('['))
        inner.replace(QString(2, quote), QString(1, quote));
    return inner;
}

QSQLiteResult::QSQLiteResult(const QSQLiteDriver *driver, std::shared_ptr<QSQLiteConnection> connection)
    : QSqlResult(driver), m_connection(std::move(connection)) {
    m_connection->results.push_back(this);
}

QSQLiteResult::~QSQLiteResult() {
    finalize();
    auto &results = m_connection->results;
    results.erase(std::remove(results.begin(), results.end(), this), results.end());
}

void QSQLiteResult::finalize() {
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
        m_stmt = nullptr;
    }
    m_rows.clear();
    m_bound.clear();
    m_record.clear();
    m_done = true;
    m_pendingRow = false;
    setActive(false);
    setAt(QSql::BeforeFirstRow);
}

QVariant QSQLiteResult::handle() const { return m_stmt ? QVariant::fromValue(m_stmt) : QVariant(); }

void QSQLiteResult::fail(const QString &what, QSqlError::ErrorType type) {
    setLastError(makeError(m_connection->db, what, type));
}

bool QSQLiteResult::reset(const QString &query) { return savePrepare(query) && exec(); }

bool QSQLiteResult::prepare(const QString &query) {
    static_cast<void>(query);
    finalize();
    setSelect(false);
    if (!m_connection->db) {
        fail(QStringLiteral("Unable to execute statement"), QSqlError::ConnectionError);
        return false;
    }
    const QByteArray sql = executedQuery().toUtf8();
    if (sql.size() > std::numeric_limits<int>::max()) {
        setLastError(QSqlError(QStringLiteral("Unable to execute statement"),
                               QStringLiteral("The statement is too long"), QSqlError::StatementError));
        return false;
    }
    const char *tail = nullptr;
    if (sqlite3_prepare_v2(m_connection->db, sql.constData(), static_cast<int>(sql.size()), &m_stmt, &tail) !=
        SQLITE_OK) {
        fail(QStringLiteral("Unable to execute statement"), QSqlError::StatementError);
        finalize();
        return false;
    }
    // What follows the first statement may only be white space, semicolons and comments.
    bool extra = false;
    for (const char *p = tail; p && *p != '\0' && !extra;) {
        const char c = *p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';') {
            ++p;
        } else if (c == '-' && p[1] == '-') {
            while (*p != '\0' && *p != '\n')
                ++p;
        } else if (c == '/' && p[1] == '*') {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/'))
                ++p;
            if (*p != '\0')
                p += 2;
        } else {
            extra = true;
        }
    }
    if (extra || !m_stmt) {
        setLastError(QSqlError(QStringLiteral("Unable to execute statement"),
                               extra ? QStringLiteral("Unable to execute multiple statements at a time")
                                     : QStringLiteral("The statement is empty"),
                               QSqlError::StatementError));
        finalize();
        return false;
    }
    return true;
}

bool QSQLiteResult::exec() {
    if (!m_stmt || !m_connection->db) {
        if (!lastError().isValid())
            fail(QStringLiteral("Unable to execute statement"), QSqlError::StatementError);
        return false;
    }
    sqlite3_reset(m_stmt);
    sqlite3_clear_bindings(m_stmt);
    m_rows.clear();
    m_bound.clear();
    m_firstRow = 0;
    m_done = true;
    m_pendingRow = false;
    m_hasInsertId = false;
    setActive(false);
    setAt(QSql::BeforeFirstRow);
    setLastError(QSqlError());

    const QVariantList values = boundValues();
    const int wanted = sqlite3_bind_parameter_count(m_stmt);
    if (values.size() != wanted) {
        setLastError(QSqlError(QStringLiteral("Parameter count mismatch"), QString(), QSqlError::StatementError));
        return false;
    }
    m_bound.reserve(static_cast<std::size_t>(wanted)); // so that the buffers do not move while they are bound
    for (int i = 0; i < wanted; ++i) {
        const QVariant &value = values.at(i);
        int rc = SQLITE_OK;
        if (value.isNull()) {
            rc = sqlite3_bind_null(m_stmt, i + 1);
        } else {
            switch (value.typeId()) {
            case QMetaType::QByteArray: {
                m_bound.push_back(value.toByteArray());
                const QByteArray &bytes = m_bound.back();
                // A null pointer would bind NULL, so an empty array still passes a pointer.
                rc = sqlite3_bind_blob64(m_stmt, i + 1, bytes.isEmpty() ? "" : bytes.constData(),
                                         static_cast<sqlite3_uint64>(bytes.size()), nullptr);
                break;
            }
            case QMetaType::Bool:
                rc = sqlite3_bind_int(m_stmt, i + 1, value.toBool() ? 1 : 0);
                break;
            case QMetaType::Int:
            case QMetaType::UInt:
            case QMetaType::Short:
            case QMetaType::UShort:
            case QMetaType::Char:
            case QMetaType::SChar:
            case QMetaType::UChar:
            case QMetaType::Long:
            case QMetaType::LongLong:
                rc = sqlite3_bind_int64(m_stmt, i + 1, value.toLongLong());
                break;
            case QMetaType::ULong:
            case QMetaType::ULongLong: {
                // SQLite integers are signed: a larger number goes in as text, which keeps its digits in a
                // text column; a numeric column makes it a real.
                const qulonglong u = value.toULongLong();
                if (u <= static_cast<qulonglong>(std::numeric_limits<qint64>::max())) {
                    rc = sqlite3_bind_int64(m_stmt, i + 1, static_cast<sqlite3_int64>(u));
                } else {
                    m_bound.push_back(QByteArray::number(u));
                    rc = sqlite3_bind_text64(m_stmt, i + 1, m_bound.back().constData(),
                                             static_cast<sqlite3_uint64>(m_bound.back().size()), nullptr, SQLITE_UTF8);
                }
                break;
            }
            case QMetaType::Double:
            case QMetaType::Float:
                rc = sqlite3_bind_double(m_stmt, i + 1, value.toDouble());
                break;
            default: {
                QString text;
                if (value.typeId() == QMetaType::QDateTime)
                    text = value.toDateTime().toString(Qt::ISODateWithMs);
                else if (value.typeId() == QMetaType::QTime)
                    text = value.toTime().toString(Qt::ISODateWithMs);
                else if (value.typeId() == QMetaType::QDate)
                    text = value.toDate().toString(Qt::ISODate);
                else
                    text = value.toString();
                m_bound.push_back(text.toUtf8());
                const QByteArray &bytes = m_bound.back();
                rc = sqlite3_bind_text64(m_stmt, i + 1, bytes.isEmpty() ? "" : bytes.constData(),
                                         static_cast<sqlite3_uint64>(bytes.size()), nullptr, SQLITE_UTF8);
                break;
            }
            }
        }
        if (rc != SQLITE_OK) {
            fail(QStringLiteral("Unable to bind parameters"), QSqlError::StatementError);
            return false;
        }
    }

    const int columns = sqlite3_column_count(m_stmt);
    setSelect(columns > 0);
    m_done = false;
    const Step first = step();
    if (first == Step::Error) {
        fail(QStringLiteral("Unable to fetch row"), QSqlError::StatementError);
        sqlite3_reset(m_stmt);
        m_done = true;
        return false;
    }
    m_pendingRow = first == Step::Row;
    readColumns();
    m_changes = sqlite3_changes(m_connection->db);
    if (!isSelect() && sqlite3_stmt_readonly(m_stmt) == 0) {
        m_lastInsertId = sqlite3_last_insert_rowid(m_connection->db);
        m_hasInsertId = true;
    }
    if (first == Step::Done)
        sqlite3_reset(m_stmt); // nothing more to read: let go of the locks at once
    setActive(true);
    return true;
}

QSQLiteResult::Step QSQLiteResult::step() {
    if (m_done || !m_stmt)
        return Step::Done;
    const int rc = sqlite3_step(m_stmt);
    if (rc == SQLITE_ROW)
        return Step::Row;
    m_done = true;
    return rc == SQLITE_DONE ? Step::Done : Step::Error;
}

// The fields of the rows: their names, and their types from the declarations of the columns, or, for an
// expression, from the value in the first row.
void QSQLiteResult::readColumns() {
    m_record.clear();
    const int columns = sqlite3_column_count(m_stmt);
    for (int i = 0; i < columns; ++i) {
        const QString name = fromUtf8(sqlite3_column_name(m_stmt, i));
        const QString decl = fromUtf8(sqlite3_column_decltype(m_stmt, i));
        QMetaType type = typeOfDeclaration(decl);
        if (!type.isValid() && m_pendingRow)
            type = typeOfValue(sqlite3_column_type(m_stmt, i));
#ifdef NT_SQLITE_COLUMN_METADATA
        m_record.append(QSqlField(name, type, fromUtf8(sqlite3_column_table_name(m_stmt, i))));
#else
        m_record.append(QSqlField(name, type));
#endif
    }
}

// Copies the row that the statement stands on to the end of m_rows.
void QSQLiteResult::readRow() {
    const int columns = sqlite3_column_count(m_stmt);
    QVariantList row;
    row.reserve(columns);
    for (int i = 0; i < columns; ++i) {
        switch (sqlite3_column_type(m_stmt, i)) {
        case SQLITE_INTEGER:
            row.append(QVariant(static_cast<qlonglong>(sqlite3_column_int64(m_stmt, i))));
            break;
        case SQLITE_FLOAT:
            // SQLite converts the number itself when an integer is asked for.
            switch (numericalPrecisionPolicy()) {
            case QSql::LowPrecisionInt32:
                row.append(QVariant(sqlite3_column_int(m_stmt, i)));
                break;
            case QSql::LowPrecisionInt64:
                row.append(QVariant(static_cast<qlonglong>(sqlite3_column_int64(m_stmt, i))));
                break;
            case QSql::LowPrecisionDouble:
            case QSql::HighPrecision:
                row.append(QVariant(sqlite3_column_double(m_stmt, i)));
                break;
            }
            break;
        case SQLITE_BLOB: {
            const void *bytes = sqlite3_column_blob(m_stmt, i);
            const int size = sqlite3_column_bytes(m_stmt, i);
            row.append(QVariant(bytes ? QByteArray(static_cast<const char *>(bytes), size) : QByteArray("", 0)));
            break;
        }
        case SQLITE_NULL:
            row.append(QVariant(m_record.field(i).metaType()));
            break;
        default: {
            const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(m_stmt, i));
            const int size = sqlite3_column_bytes(m_stmt, i);
            row.append(QVariant(text ? QString::fromUtf8(text, size) : QStringLiteral("")));
            break;
        }
        }
    }
    if (isForwardOnly()) {
        m_firstRow += static_cast<int>(m_rows.size());
        m_rows.clear();
    }
    m_rows.push_back(std::move(row));
}

// Reads rows until the row at position row is in m_rows, or the rows are used up.
bool QSQLiteResult::fetchUpTo(int row) {
    for (;;) {
        const int have = m_firstRow + static_cast<int>(m_rows.size());
        if (row < have)
            return row >= m_firstRow;
        if (m_pendingRow) {
            m_pendingRow = false;
            readRow();
            continue;
        }
        switch (step()) {
        case Step::Row:
            readRow();
            break;
        case Step::Error:
            fail(QStringLiteral("Unable to fetch row"), QSqlError::ConnectionError);
            sqlite3_reset(m_stmt);
            return false;
        case Step::Done:
            if (m_stmt)
                sqlite3_reset(m_stmt);
            return false;
        }
    }
}

bool QSQLiteResult::fetch(int i) {
    if (i < 0 || !isActive() || !fetchUpTo(i))
        return false;
    setAt(i);
    return true;
}

bool QSQLiteResult::fetchNext() { return fetch(at() == QSql::BeforeFirstRow ? 0 : at() + 1); }

bool QSQLiteResult::fetchFirst() { return fetch(0); }

bool QSQLiteResult::fetchLast() {
    if (!isActive())
        return false;
    fetchUpTo(std::numeric_limits<int>::max() - 1);
    const int count = m_firstRow + static_cast<int>(m_rows.size());
    if (count == 0 || m_rows.empty())
        return false;
    setAt(count - 1);
    return true;
}

QVariant QSQLiteResult::data(int i) {
    const int row = at() - m_firstRow;
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return QVariant();
    const QVariantList &values = m_rows[static_cast<std::size_t>(row)];
    return i >= 0 && i < values.size() ? values.at(i) : QVariant();
}

bool QSQLiteResult::isNull(int i) { return data(i).isNull(); }

int QSQLiteResult::size() { return -1; }

int QSQLiteResult::numRowsAffected() { return m_changes; }

QSqlRecord QSQLiteResult::record() const { return isActive() && isSelect() ? m_record : QSqlRecord(); }

QVariant QSQLiteResult::lastInsertId() const {
    return isActive() && m_hasInsertId ? QVariant(static_cast<qlonglong>(m_lastInsertId)) : QVariant();
}

void QSQLiteResult::detachFromResultSet() {
    if (m_stmt)
        sqlite3_reset(m_stmt);
    m_rows.clear();
    m_firstRow = 0;
    m_done = true;
    m_pendingRow = false;
}
