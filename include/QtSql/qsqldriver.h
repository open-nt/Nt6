// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtSql/qsqlerror.h>
#include <QtSql/qtsqlglobal.h>

#include <memory>

class QSqlDatabase;
class QSqlField;
class QSqlIndex;
class QSqlRecord;
class QSqlResult;

class QSqlDriver : public QObject {
    Q_OBJECT
    Q_PROPERTY(QSql::NumericalPrecisionPolicy numericalPrecisionPolicy READ numericalPrecisionPolicy WRITE
                   setNumericalPrecisionPolicy)
public:
    /*!
        \enum QSqlDriver::DriverFeature

        What a driver can do, as told by hasFeature().

        \value Transactions The driver has transactions.
        \value QuerySize The driver knows the number of rows of a query before fetching them.
        \value BLOB The driver stores binary data.
        \value Unicode The driver stores Unicode text.
        \value PreparedQueries The driver prepares statements.
        \value NamedPlaceholders The driver has placeholders with names, such as \c{:id}.
        \value PositionalPlaceholders The driver has placeholders that are written \c{?}.
        \value LastInsertId The driver tells the identifier of the row that was inserted last.
        \value BatchOperations The driver runs a statement for a batch of values in one call.
        \value SimpleLocking The driver locks a whole table while it is read, so that writing to it has to wait.
        \value LowPrecisionNumbers The driver can return numbers with less precision.
        \value EventNotifications The driver delivers notifications of the database.
        \value FinishQuery The driver releases the resources of a query that is finished.
        \value MultipleResultSets The driver returns several result sets for one statement.
        \value CancelQuery The driver can cancel a running query.
    */
    enum DriverFeature {
        Transactions,
        QuerySize,
        BLOB,
        Unicode,
        PreparedQueries,
        NamedPlaceholders,
        PositionalPlaceholders,
        LastInsertId,
        BatchOperations,
        SimpleLocking,
        LowPrecisionNumbers,
        EventNotifications,
        FinishQuery,
        MultipleResultSets,
        CancelQuery
    };
    Q_ENUM(DriverFeature)

    /*!
        \enum QSqlDriver::StatementType

        The kinds of statements that sqlStatement() writes.

        \value WhereStatement A \c WHERE clause that selects the row with the values of a record.
        \value SelectStatement A \c SELECT statement for the fields of a record.
        \value UpdateStatement An \c UPDATE statement that sets the fields of a record, without a \c WHERE clause.
        \value InsertStatement An \c INSERT statement for the fields of a record.
        \value DeleteStatement A \c DELETE statement, without a \c WHERE clause.
    */
    enum StatementType { WhereStatement, SelectStatement, UpdateStatement, InsertStatement, DeleteStatement };

    /*!
        \enum QSqlDriver::IdentifierType

        The kinds of identifiers that escapeIdentifier() quotes.

        \value FieldName The name of a field.
        \value TableName The name of a table.
    */
    enum IdentifierType { FieldName, TableName };

    /*!
        \enum QSqlDriver::NotificationSource

        Where a notification of the database comes from.

        \value UnknownSource The source is not known.
        \value SelfSource The notification comes from this connection.
        \value OtherSource The notification comes from another connection.
    */
    enum NotificationSource { UnknownSource, SelfSource, OtherSource };

    /*!
        \enum QSqlDriver::DbmsType

        The database systems that a driver can be connected to.

        \value UnknownDbms An unknown system.
        \value MSSqlServer Microsoft SQL Server.
        \value MySqlServer MySQL or MariaDB.
        \value PostgreSQL PostgreSQL.
        \value Oracle Oracle Database.
        \value Sybase Sybase.
        \value SQLite SQLite.
        \value Interbase Interbase or Firebird.
        \value DB2 IBM Db2.
        \value MimerSQL Mimer SQL.
    */
    enum DbmsType {
        UnknownDbms,
        MSSqlServer,
        MySqlServer,
        PostgreSQL,
        Oracle,
        Sybase,
        SQLite,
        Interbase,
        DB2,
        MimerSQL
    };

    explicit QSqlDriver(QObject *parent = nullptr);
    ~QSqlDriver() override;

    virtual bool isOpen() const;
    bool isOpenError() const;

    virtual bool beginTransaction();
    virtual bool commitTransaction();
    virtual bool rollbackTransaction();
    virtual QStringList tables(QSql::TableType tableType) const;
    virtual QSqlIndex primaryIndex(const QString &tableName) const;
    virtual QSqlRecord record(const QString &tableName) const;
    virtual QString formatValue(const QSqlField &field, bool trimStrings = false) const;

    virtual QString escapeIdentifier(const QString &identifier, IdentifierType type) const;
    virtual QString sqlStatement(StatementType type, const QString &tableName, const QSqlRecord &rec,
                                 bool preparedStatement) const;

    QSqlError lastError() const;

    virtual QVariant handle() const;
    /*!
        Returns \c true if the driver has the feature \a f; otherwise
        returns \c false.
    */
    virtual bool hasFeature(DriverFeature f) const = 0;
    /*!
        Closes the connection to the database. An implementation calls
        setOpen() and setOpenError() with \c false.

        \sa open()
    */
    virtual void close() = 0;
    /*!
        Returns a new result for a query on this connection. The caller
        owns it; a QSqlQuery takes it over.
    */
    virtual QSqlResult *createResult() const = 0;
    /*!
        Opens a connection to the database \a db for the user \a user with
        the password \a password, on the host \a host and the port \a port,
        with the options \a connOpts. An implementation returns \c true and
        calls setOpen() on success, and calls setLastError() and
        setOpenError() otherwise.

        \sa close(), QSqlDatabase::open()
    */
    virtual bool open(const QString &db, const QString &user = QString(), const QString &password = QString(),
                      const QString &host = QString(), int port = -1, const QString &connOpts = QString()) = 0;

    virtual bool subscribeToNotification(const QString &name);
    virtual bool unsubscribeFromNotification(const QString &name);
    virtual QStringList subscribedToNotifications() const;

    virtual bool isIdentifierEscaped(const QString &identifier, IdentifierType type) const;
    virtual QString stripDelimiters(const QString &identifier, IdentifierType type) const;
    virtual int maximumIdentifierLength(IdentifierType type) const;

    void setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy precisionPolicy);
    QSql::NumericalPrecisionPolicy numericalPrecisionPolicy() const;

    DbmsType dbmsType() const;

public slots:
    virtual bool cancelQuery();

signals:
    /*!
        This signal is emitted when the database delivers the notification
        \a name that the driver subscribed to, from \a source and with the
        data \a payload.

        \sa subscribeToNotification()
    */
    void notification(const QString &name, QSqlDriver::NotificationSource source, const QVariant &payload) {
        emitSignal(&QSqlDriver::notification, name, source, payload);
    }

protected:
    virtual void setOpen(bool open);
    virtual void setOpenError(bool error);
    virtual void setLastError(const QSqlError &error);
    void setDbmsType(DbmsType type);

private:
    QSqlError m_error;
    QSql::NumericalPrecisionPolicy m_policy = QSql::LowPrecisionDouble;
    DbmsType m_dbms = UnknownDbms;
    bool m_open = false;
    bool m_openError = false;
};

class QSqlDriverCreatorBase {
public:
    /*!
        Destroys the creator.
    */
    virtual ~QSqlDriverCreatorBase() = default;
    /*!
        Returns a new driver. The caller owns it.
    */
    virtual QSqlDriver *createObject() const = 0;
};

template <typename T> class QSqlDriverCreator : public QSqlDriverCreatorBase {
public:
    /*!
        Returns a new, default-constructed driver of the class \c T. The
        caller owns it.
    */
    QSqlDriver *createObject() const override { return new T; }
};
