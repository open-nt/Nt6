// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtSql/qsqlerror.h>
#include <QtSql/qtsqlglobal.h>

#include <memory>

class QSqlDriver;
class QSqlRecord;
class QSqlResultPrivate;

class QSqlResult {
public:
    virtual ~QSqlResult();
    QSqlResult(const QSqlResult &) = delete;
    QSqlResult &operator=(const QSqlResult &) = delete;

    virtual QVariant handle() const;

protected:
    /*!
        \enum QSqlResult::BindingSyntax

        The way the placeholders of a prepared statement were written.

        \value PositionalBinding With \c{?}.
        \value NamedBinding With names, such as \c{:id}.
    */
    enum BindingSyntax { PositionalBinding, NamedBinding };

    explicit QSqlResult(const QSqlDriver *db);

    int at() const;
    QString lastQuery() const;
    QSqlError lastError() const;
    bool isValid() const;
    bool isActive() const;
    bool isSelect() const;
    bool isForwardOnly() const;
    const QSqlDriver *driver() const;
    virtual void setAt(int index);
    virtual void setActive(bool active);
    virtual void setLastError(const QSqlError &error);
    virtual void setQuery(const QString &query);
    virtual void setSelect(bool select);
    virtual void setForwardOnly(bool forward);

    virtual bool exec();
    virtual bool prepare(const QString &query);
    virtual bool savePrepare(const QString &sqlquery);
    virtual void bindValue(int index, const QVariant &val, QSql::ParamType type);
    virtual void bindValue(const QString &placeholder, const QVariant &val, QSql::ParamType type);
    void addBindValue(const QVariant &val, QSql::ParamType type);
    QVariant boundValue(const QString &placeholder) const;
    QVariant boundValue(int index) const;
    QSql::ParamType bindValueType(const QString &placeholder) const;
    QSql::ParamType bindValueType(int index) const;
    int boundValueCount() const;
    QVariantList boundValues() const;
    QString executedQuery() const;
    QStringList boundValueNames() const;
    QString boundValueName(int index) const;
    void clear();
    bool hasOutValues() const;

    BindingSyntax bindingSyntax() const;

    /*!
        Returns the value of the field at position \a i of the current row.
        Called only for a valid row and a position inside the record.
    */
    virtual QVariant data(int i) = 0;
    /*!
        Returns \c true if the field at position \a i of the current row is
        null; otherwise returns \c false.
    */
    virtual bool isNull(int i) = 0;
    /*!
        Runs the SQL statement \a sqlquery, without preparing it first, and
        returns \c true on success. An implementation calls setActive() and
        setSelect(), and setLastError() on failure.
    */
    virtual bool reset(const QString &sqlquery) = 0;
    /*!
        Moves to the row at position \a i, counted from 0, and returns
        \c true if there is such a row. An implementation calls setAt().
    */
    virtual bool fetch(int i) = 0;
    virtual bool fetchNext();
    virtual bool fetchPrevious();
    /*!
        Moves to the first row and returns \c true if there is one.
    */
    virtual bool fetchFirst() = 0;
    /*!
        Moves to the last row and returns \c true if there is one.
    */
    virtual bool fetchLast() = 0;
    /*!
        Returns the number of rows of a \c SELECT statement, or -1 if it is
        not known.
    */
    virtual int size() = 0;
    /*!
        Returns the number of rows that the last statement changed, or -1
        if it is not known.
    */
    virtual int numRowsAffected() = 0;
    virtual QSqlRecord record() const;
    virtual QVariant lastInsertId() const;

    virtual bool execBatch(bool arrayBind = false);
    virtual void detachFromResultSet();
    virtual void setNumericalPrecisionPolicy(QSql::NumericalPrecisionPolicy policy);
    QSql::NumericalPrecisionPolicy numericalPrecisionPolicy() const;
    virtual bool nextResult();
    void resetBindCount();

private:
    friend class QSqlQuery;
    friend class QSqlResultPrivate;
    std::unique_ptr<QSqlResultPrivate> d;
};
