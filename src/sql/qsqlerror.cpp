// MIT License - see LICENSE
#include <QtSql/qsqlerror.h>

#include <QtCore/qdebug.h>

#include <utility>

/*!
    \class QSqlError
    \brief The QSqlError class provides SQL database error information.

    A QSqlError tells what went wrong with a connection or a statement. It
    has a type(), the message of the driver (driverText()), the message of
    the database itself (databaseText()) and the error code of the database
    (nativeErrorCode()). text() joins the two messages.

    \code
    QSqlQuery query;
    if (!query.exec("SELECT nme FROM people"))
        qWarning() << query.lastError().text();
    \endcode

    \differences
    None.
    \enddifferences

    \sa QSqlDatabase::lastError(), QSqlQuery::lastError()
*/

/*!
    Constructs an error with the message of the driver \a driverText, the
    message of the database \a databaseText, the \a type and the error code
    of the database \a nativeErrorCode.
*/
QSqlError::QSqlError(const QString &driverText, const QString &databaseText, ErrorType type,
                     const QString &nativeErrorCode)
    : m_driverText(driverText), m_databaseText(databaseText), m_code(nativeErrorCode), m_type(type) {}

/*!
    Returns the message of the driver. It says which operation failed.

    \sa databaseText(), text()
*/
QString QSqlError::driverText() const { return m_driverText; }

/*!
    Returns the message of the database. It says why the operation failed.

    \sa driverText(), text()
*/
QString QSqlError::databaseText() const { return m_databaseText; }

/*!
    Returns the kind of the error, or \c NoError if there is none.
*/
QSqlError::ErrorType QSqlError::type() const { return m_type; }

/*!
    Returns the error code of the database, or an empty string if it gave
    none. For SQLite this is the extended result code as a number.
*/
QString QSqlError::nativeErrorCode() const { return m_code; }

/*!
    Returns the message of the database and the message of the driver,
    joined by a space.

    \sa driverText(), databaseText()
*/
QString QSqlError::text() const {
    if (m_databaseText.isEmpty() || m_driverText.isEmpty())
        return m_databaseText + m_driverText;
    return m_databaseText + QChar(' ') + m_driverText;
}

/*!
    Returns \c true if an error is set, that is if type() is not \c NoError;
    otherwise returns \c false.
*/
bool QSqlError::isValid() const { return m_type != NoError; }

/*!
    Swaps this error with \a other.
*/
void QSqlError::swap(QSqlError &other) noexcept {
    m_driverText.swap(other.m_driverText);
    m_databaseText.swap(other.m_databaseText);
    m_code.swap(other.m_code);
    std::swap(m_type, other.m_type);
}

QDebug operator<<(QDebug dbg, const QSqlError &error) {
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QSqlError(" << error.nativeErrorCode() << ", " << error.driverText() << ", "
                  << error.databaseText() << ")";
    return dbg;
}
