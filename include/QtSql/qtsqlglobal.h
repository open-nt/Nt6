// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>

/*!
    \namespace QSql
    \brief The QSql namespace holds identifiers that are used throughout Nt SQL.

    \differences
    None.
    \enddifferences
*/
namespace QSql {

/*!
    \enum QSql::Location

    The special positions of a query, as returned by QSqlQuery::at().

    \value BeforeFirstRow The query is positioned before the first row.
    \value AfterLastRow The query is positioned after the last row.
*/
enum Location { BeforeFirstRow = -1, AfterLastRow = -2 };

/*!
    \enum QSql::ParamTypeFlag

    The kind of a value that is bound to a placeholder.

    \value In The value is passed to the database.
    \value Out The database writes the value. No driver of Nt has output parameters.
    \value InOut The value is passed to the database, which may overwrite it.
    \value Binary The value is binary data. This can be combined with the other values.
*/
enum ParamTypeFlag { In = 0x00000001, Out = 0x00000002, InOut = In | Out, Binary = 0x00000004 };
Q_DECLARE_FLAGS(ParamType, ParamTypeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ParamType)

/*!
    \enum QSql::TableType

    The kinds of tables that QSqlDatabase::tables() lists.

    \value Tables The tables that the user can see.
    \value SystemTables The tables that the database uses internally.
    \value Views The views that the user can see.
    \value AllTables All of the above.
*/
enum TableType { Tables = 0x01, SystemTables = 0x02, Views = 0x04, AllTables = 0xff };

/*!
    \enum QSql::NumericalPrecisionPolicy

    How numbers that do not fit a \c double without loss are returned. The
    SQLite driver stores what SQLite stores, a 64-bit integer or a
    \c double, so it accepts the policy and ignores it.

    \value LowPrecisionInt32 Return a 32-bit integer.
    \value LowPrecisionInt64 Return a 64-bit integer.
    \value LowPrecisionDouble Return a \c double. This is the default.
    \value HighPrecision Return a string that keeps every digit.
*/
enum NumericalPrecisionPolicy {
    LowPrecisionInt32 = 0x01,
    LowPrecisionInt64 = 0x02,
    LowPrecisionDouble = 0x04,
    HighPrecision = 0
};

} // namespace QSql
