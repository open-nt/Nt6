// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlerror.h>
#include <QtSql/qtsqlglobal.h>

#include <memory>

class QDebug;
class QSqlIndex;
class QSqlRecord;
class QSqlDatabasePrivate;

class QSqlDatabase {
public:
    QSqlDatabase();
    QSqlDatabase(const QSqlDatabase &other);
    QSqlDatabase &operator=(const QSqlDatabase &other);
    ~QSqlDatabase();

    bool open();
    bool open(const QString &user, const QString &password);
    void close();
    bool isOpen() const;
    bool isOpenError() const;
    QStringList tables(QSql::TableType type = QSql::Tables) const;
    QSqlIndex primaryIndex(const QString &tablename) const;
    QSqlRecord record(const QString &tablename) const;
    QSqlError lastError() const;
    bool isValid() const;

    bool transaction();
    bool commit();
    bool rollback();

    void setDatabaseName(const QString &name);
    void setUserName(const QString &name);
    void setPassword(const QString &password);
    void setHostName(const QString &host);
    void setPort(int p);
    void setConnectOptions(const QString &options = QString());
    QString databaseName() const;
    QString userName() const;
    QString password() const;
    QString hostName() const;
    QString driverName() const;
    int port() const;
    QString connectOptions() const;
    QString connectionName() const;
    void setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy precisionPolicy);
    QSql::NumericalPrecisionPolicy numericalPrecisionPolicy() const;

    QSqlDriver *driver() const;

    static const char *const defaultConnection;

    static QSqlDatabase addDatabase(const QString &type,
                                    const QString &connectionName = QLatin1StringView(defaultConnection));
    static QSqlDatabase addDatabase(QSqlDriver *driver,
                                    const QString &connectionName = QLatin1StringView(defaultConnection));
    static QSqlDatabase cloneDatabase(const QSqlDatabase &other, const QString &connectionName);
    static QSqlDatabase cloneDatabase(const QString &other, const QString &connectionName);
    static QSqlDatabase database(const QString &connectionName = QLatin1StringView(defaultConnection),
                                 bool open = true);
    static void removeDatabase(const QString &connectionName);
    static bool contains(const QString &connectionName = QLatin1StringView(defaultConnection));
    static QStringList drivers();
    static QStringList connectionNames();
    static void registerSqlDriver(const QString &name, QSqlDriverCreatorBase *creator);
    static bool isDriverAvailable(const QString &name);

private:
    friend class QSqlDatabasePrivate;
    std::shared_ptr<QSqlDatabasePrivate> d;
};

QDebug operator<<(QDebug dbg, const QSqlDatabase &db);
