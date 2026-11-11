// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qanystringview.h>
#include <QtCore/qvariant.h>
#include <QtSql/qsqldatabase.h>
#include <QtSql/qsqlerror.h>
#include <QtSql/qtsqlglobal.h>

#include <memory>

class QSqlDriver;
class QSqlRecord;
class QSqlResult;

class QSqlQuery {
public:
    /*!
        \enum QSqlQuery::BatchExecutionMode

        How execBatch() reads the bound lists.

        \value ValuesAsRows Each list holds the values of one placeholder, one item for each run of the statement.
        \value ValuesAsColumns Each list is bound as a single value, for databases that take arrays. No driver of Nt does.
    */
    enum BatchExecutionMode { ValuesAsRows, ValuesAsColumns };

    explicit QSqlQuery(QSqlResult *result);
    explicit QSqlQuery(const QString &query = QString(), const QSqlDatabase &db = QSqlDatabase());
    explicit QSqlQuery(const QSqlDatabase &db);
    QSqlQuery(const QSqlQuery &) = delete;
    QSqlQuery &operator=(const QSqlQuery &) = delete;
    QSqlQuery(QSqlQuery &&other) noexcept;
    QSqlQuery &operator=(QSqlQuery &&other) noexcept;
    ~QSqlQuery();

    bool isValid() const;
    bool isActive() const;
    bool isNull(int field) const;
    bool isNull(QAnyStringView name) const;
    int at() const;
    QString lastQuery() const;
    int numRowsAffected() const;
    QSqlError lastError() const;
    bool isSelect() const;
    int size() const;
    const QSqlDriver *driver() const;
    const QSqlResult *result() const;
    bool isForwardOnly() const;
    QSqlRecord record() const;

    void setForwardOnly(bool forward);
    bool exec(const QString &query);
    QVariant value(int i) const;
    QVariant value(QAnyStringView name) const;

    void setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy precisionPolicy);
    QSql::NumericalPrecisionPolicy numericalPrecisionPolicy() const;

    bool seek(int i, bool relative = false);
    bool next();
    bool previous();
    bool first();
    bool last();

    void clear();

    bool exec();
    bool execBatch(BatchExecutionMode mode = ValuesAsRows);
    bool prepare(const QString &query);
    void bindValue(const QString &placeholder, const QVariant &val, QSql::ParamType type = QSql::In);
    void bindValue(int pos, const QVariant &val, QSql::ParamType type = QSql::In);
    void addBindValue(const QVariant &val, QSql::ParamType type = QSql::In);
    QVariant boundValue(const QString &placeholder) const;
    QVariant boundValue(int pos) const;
    QVariantList boundValues() const;
    QStringList boundValueNames() const;
    QString boundValueName(int pos) const;
    QString executedQuery() const;
    QVariant lastInsertId() const;
    void finish();
    bool nextResult();
    void swap(QSqlQuery &other) noexcept;

private:
    std::unique_ptr<QSqlResult> m_result;
};
