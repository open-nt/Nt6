// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtSql/qtsqlglobal.h>

class QDebug;

class QSqlError {
public:
    /*!
        \enum QSqlError::ErrorType

        The kind of an error.

        \value NoError No error happened.
        \value ConnectionError The connection could not be opened, or was lost.
        \value StatementError An SQL statement had a syntax error or could not be run.
        \value TransactionError A transaction could not be started, committed or rolled back.
        \value UnknownError Another error.
    */
    enum ErrorType { NoError, ConnectionError, StatementError, TransactionError, UnknownError };

    explicit QSqlError(const QString &driverText = QString(), const QString &databaseText = QString(),
                       ErrorType type = NoError, const QString &nativeErrorCode = QString());

    QString driverText() const;
    QString databaseText() const;
    ErrorType type() const;
    QString nativeErrorCode() const;
    QString text() const;
    bool isValid() const;
    void swap(QSqlError &other) noexcept;

    /*!
        Returns \c true if \a lhs and \a rhs have the same type and the same
        native error code; otherwise returns \c false. The texts are not
        compared.
    */
    friend bool operator==(const QSqlError &lhs, const QSqlError &rhs) {
        return lhs.m_type == rhs.m_type && lhs.m_code == rhs.m_code;
    }
    /*!
        Returns \c true if \a lhs and \a rhs differ in their type or their
        native error code; otherwise returns \c false.
    */
    friend bool operator!=(const QSqlError &lhs, const QSqlError &rhs) { return !(lhs == rhs); }

private:
    QString m_driverText;
    QString m_databaseText;
    QString m_code;
    ErrorType m_type = NoError;
};

QDebug operator<<(QDebug dbg, const QSqlError &error);
