// MIT License - see LICENSE
#include <QtSql/qsqlrecord.h>

#include <QtCore/qdebug.h>
#include <QtSql/qsqlindex.h>

#include <utility>

/*!
    \class QSqlRecord
    \brief The QSqlRecord class encapsulates a database record.

    A QSqlRecord is a list of fields with their values: one row of a table,
    a view or a query. QSqlQuery::record() gives the current row of a
    query, and QSqlDatabase::record() the columns of a table without
    values.

    \code
    QSqlRecord row = query.record();
    int name = row.indexOf("name");
    while (query.next())
        qDebug() << query.value(name).toString();
    \endcode

    Fields are found by position or by name; names are compared without
    regard to case, and a name of the form \c table.field also names the
    field \c field of the table \c table. A position or a name that does not
    exist is ignored by the setters and gives a default value in the
    getters.

    \differences
    A record is a plain value: copies are not implicitly shared.
    \enddifferences

    \sa QSqlField, QSqlQuery::record()
*/

/*!
    Constructs an empty record.

    \sa isEmpty(), append()
*/
QSqlRecord::QSqlRecord() = default;

/*!
    Constructs a copy of \a other.
*/
QSqlRecord::QSqlRecord(const QSqlRecord &other) = default;

/*!
    Constructs a record with the fields of \a other, which is left empty.
*/
QSqlRecord::QSqlRecord(QSqlRecord &&other) noexcept = default;

/*!
    Makes this record a copy of \a other and returns a reference to it.
*/
QSqlRecord &QSqlRecord::operator=(const QSqlRecord &other) = default;

/*!
    Moves the fields of \a other to this record and returns a reference to
    it.
*/
QSqlRecord &QSqlRecord::operator=(QSqlRecord &&other) noexcept = default;

/*!
    Destroys the record.
*/
QSqlRecord::~QSqlRecord() = default;

/*!
    Returns \c true if this record has the same fields with the same values
    as \a other; otherwise returns \c false.
*/
bool QSqlRecord::operator==(const QSqlRecord &other) const { return m_fields == other.m_fields; }

/*!
    Returns the value of the field at position \a i, or an invalid QVariant
    if there is no such field.

    \sa fieldName(), isNull()
*/
QVariant QSqlRecord::value(int i) const { return i >= 0 && i < count() ? m_fields.at(i).value() : QVariant(); }

/*!
    \overload
    Returns the value of the field called \a name, or an invalid QVariant
    if there is no such field.
*/
QVariant QSqlRecord::value(QAnyStringView name) const { return value(indexOf(name)); }

/*!
    Sets the value of the field at position \a i to \a val. Nothing happens
    if there is no such field, or if it is read-only.

    \sa setNull()
*/
void QSqlRecord::setValue(int i, const QVariant &val) {
    if (i >= 0 && i < count())
        m_fields[i].setValue(val);
}

/*!
    \overload
    Sets the value of the field called \a name to \a val.
*/
void QSqlRecord::setValue(QAnyStringView name, const QVariant &val) { setValue(indexOf(name), val); }

/*!
    Sets the value of the field at position \a i to null. Nothing happens
    if there is no such field.

    \sa setValue()
*/
void QSqlRecord::setNull(int i) {
    if (i >= 0 && i < count())
        m_fields[i].clear();
}

/*!
    \overload
    Sets the value of the field called \a name to null.
*/
void QSqlRecord::setNull(QAnyStringView name) { setNull(indexOf(name)); }

/*!
    Returns \c true if the value of the field at position \a i is null or
    if there is no such field; otherwise returns \c false.

    \sa setNull()
*/
bool QSqlRecord::isNull(int i) const { return i < 0 || i >= count() || m_fields.at(i).isNull(); }

/*!
    \overload
    Returns \c true if the value of the field called \a name is null or if
    there is no such field; otherwise returns \c false.
*/
bool QSqlRecord::isNull(QAnyStringView name) const { return isNull(indexOf(name)); }

/*!
    Returns the position of the field called \a name, or -1 if there is no
    such field. Case does not matter, and \a name may have the form
    \c table.field.

    \sa fieldName()
*/
int QSqlRecord::indexOf(QAnyStringView name) const {
    const QString wanted = name.toString();
    QString table;
    QString field = wanted;
    const qsizetype dot = wanted.indexOf(QChar('.'));
    if (dot >= 0) {
        table = wanted.left(dot);
        field = wanted.mid(dot + 1);
    }
    for (int i = 0; i < count(); ++i) {
        const QSqlField &f = m_fields.at(i);
        // A field may itself be called "a.b", as the column of a join is in some databases.
        if (f.name().compare(wanted, Qt::CaseInsensitive) == 0)
            return i;
        if (dot >= 0 && f.name().compare(field, Qt::CaseInsensitive) == 0 &&
            f.tableName().compare(table, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

/*!
    Returns the name of the field at position \a i, or an empty string if
    there is no such field.

    \sa indexOf()
*/
QString QSqlRecord::fieldName(int i) const { return i >= 0 && i < count() ? m_fields.at(i).name() : QString(); }

/*!
    Returns the field at position \a i, or a default-constructed field if
    there is no such field.
*/
QSqlField QSqlRecord::field(int i) const { return i >= 0 && i < count() ? m_fields.at(i) : QSqlField(); }

/*!
    \overload
    Returns the field called \a name.
*/
QSqlField QSqlRecord::field(QAnyStringView name) const { return field(indexOf(name)); }

/*!
    Returns \c true if the field at position \a i takes part in generated
    SQL statements; returns \c false if it does not or if there is no such
    field.

    \sa setGenerated()
*/
bool QSqlRecord::isGenerated(int i) const { return i >= 0 && i < count() && m_fields.at(i).isGenerated(); }

/*!
    \overload
    Returns \c true if the field called \a name takes part in generated SQL
    statements.
*/
bool QSqlRecord::isGenerated(QAnyStringView name) const { return isGenerated(indexOf(name)); }

/*!
    \overload
    Sets whether the field called \a name takes part in generated SQL
    statements to \a generated.
*/
void QSqlRecord::setGenerated(QAnyStringView name, bool generated) { setGenerated(indexOf(name), generated); }

/*!
    Sets whether the field at position \a i takes part in generated SQL
    statements to \a generated. Nothing happens if there is no such field.

    \sa isGenerated()
*/
void QSqlRecord::setGenerated(int i, bool generated) {
    if (i >= 0 && i < count())
        m_fields[i].setGenerated(generated);
}

/*!
    Adds a copy of \a field to the end of the record.

    \sa insert(), replace(), remove()
*/
void QSqlRecord::append(const QSqlField &field) { m_fields.append(field); }

/*!
    Replaces the field at position \a pos with a copy of \a field. Nothing
    happens if there is no such field.

    \sa append(), insert()
*/
void QSqlRecord::replace(int pos, const QSqlField &field) {
    if (pos >= 0 && pos < count())
        m_fields[pos] = field;
}

/*!
    Inserts a copy of \a field at position \a pos. A position that is out
    of range is moved to the nearest end of the record.

    \sa append(), remove()
*/
void QSqlRecord::insert(int pos, const QSqlField &field) {
    m_fields.insert(pos < 0 ? 0 : (pos > count() ? count() : pos), field);
}

/*!
    Removes the field at position \a pos. Nothing happens if there is no
    such field.

    \sa append(), clear()
*/
void QSqlRecord::remove(int pos) {
    if (pos >= 0 && pos < count())
        m_fields.removeAt(pos);
}

/*!
    Returns \c true if the record has no fields; otherwise returns
    \c false.

    \sa count()
*/
bool QSqlRecord::isEmpty() const { return m_fields.isEmpty(); }

/*!
    Returns \c true if the record has a field called \a name; otherwise
    returns \c false.

    \sa indexOf()
*/
bool QSqlRecord::contains(QAnyStringView name) const { return indexOf(name) >= 0; }

/*!
    Removes all the fields of the record.

    \sa clearValues(), isEmpty()
*/
void QSqlRecord::clear() { m_fields.clear(); }

/*!
    Sets the value of every field to null. Read-only fields keep their
    values.

    \sa clear(), setNull()
*/
void QSqlRecord::clearValues() {
    for (QSqlField &f : m_fields)
        f.clear();
}

/*!
    Returns the number of fields in the record.

    \sa isEmpty()
*/
int QSqlRecord::count() const { return static_cast<int>(m_fields.size()); }

/*!
    Returns a record with the fields of \a keyFields, by name, and the
    values that these fields have in this record. Fields of \a keyFields
    that this record does not have are left out.
*/
QSqlRecord QSqlRecord::keyValues(const QSqlRecord &keyFields) const {
    QSqlRecord result;
    for (int i = 0; i < keyFields.count(); ++i) {
        const int at = indexOf(keyFields.fieldName(i));
        if (at >= 0)
            result.append(m_fields.at(at));
    }
    return result;
}

/*!
    Swaps this record with \a other.
*/
void QSqlRecord::swap(QSqlRecord &other) noexcept { m_fields.swap(other.m_fields); }

QDebug operator<<(QDebug dbg, const QSqlRecord &record) {
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QSqlRecord(" << record.count() << ")";
    for (int i = 0; i < record.count(); ++i)
        dbg << "\n " << i << ": " << record.field(i) << " " << record.value(i);
    return dbg;
}

/*!
    \class QSqlIndex
    \brief The QSqlIndex class provides functions to manipulate and describe database indexes.

    A QSqlIndex is a record whose fields are the columns of an index, each
    with a sort order. QSqlDatabase::primaryIndex() returns the primary key
    of a table as one.

    \differences
    None.
    \enddifferences

    \sa QSqlDatabase::primaryIndex(), QSqlRecord
*/

/*!
    Constructs an empty index called \a name for the table or view
    \a cursorName.
*/
QSqlIndex::QSqlIndex(const QString &cursorName, const QString &name) : m_cursor(cursorName), m_name(name) {}

/*!
    Constructs a copy of \a other.
*/
QSqlIndex::QSqlIndex(const QSqlIndex &other) = default;

/*!
    Constructs an index with the contents of \a other, which is left empty.
*/
QSqlIndex::QSqlIndex(QSqlIndex &&other) noexcept = default;

/*!
    Makes this index a copy of \a other and returns a reference to it.
*/
QSqlIndex &QSqlIndex::operator=(const QSqlIndex &other) = default;

/*!
    Moves the contents of \a other to this index and returns a reference to
    it.
*/
QSqlIndex &QSqlIndex::operator=(QSqlIndex &&other) noexcept = default;

/*!
    Destroys the index.
*/
QSqlIndex::~QSqlIndex() = default;

/*!
    Sets the name of the table or view that the index belongs to, to
    \a cursorName.

    \sa cursorName()
*/
void QSqlIndex::setCursorName(const QString &cursorName) { m_cursor = cursorName; }

/*!
    Returns the name of the table or view that the index belongs to.

    \sa setCursorName()
*/
QString QSqlIndex::cursorName() const { return m_cursor; }

/*!
    Sets the name of the index to \a name.

    \sa name()
*/
void QSqlIndex::setName(const QString &name) { m_name = name; }

/*!
    Returns the name of the index.

    \sa setName()
*/
QString QSqlIndex::name() const { return m_name; }

/*!
    Adds \a field to the index, sorted in ascending order.
*/
void QSqlIndex::append(const QSqlField &field) { append(field, false); }

/*!
    \overload
    Adds \a field to the index, sorted in descending order if \a desc is
    \c true.
*/
void QSqlIndex::append(const QSqlField &field, bool desc) {
    // The record may have got fields through the QSqlRecord interface, which have no order of their own.
    while (m_descending.size() < count())
        m_descending.append(0);
    QSqlRecord::append(field);
    m_descending.append(desc ? 1 : 0);
}

/*!
    Returns \c true if the field at position \a i is sorted in descending
    order; otherwise returns \c false.

    \sa setDescending()
*/
bool QSqlIndex::isDescending(int i) const { return i >= 0 && i < m_descending.size() && m_descending.at(i) != 0; }

/*!
    Sets the sort order of the field at position \a i to descending if
    \a desc is \c true, and to ascending otherwise. Nothing happens if
    there is no such field.

    \sa isDescending()
*/
void QSqlIndex::setDescending(int i, bool desc) {
    if (i < 0 || i >= count())
        return;
    while (m_descending.size() < count())
        m_descending.append(0);
    m_descending[i] = desc ? 1 : 0;
}

/*!
    Swaps this index with \a other.
*/
void QSqlIndex::swap(QSqlIndex &other) noexcept {
    QSqlRecord::swap(other);
    m_cursor.swap(other.m_cursor);
    m_name.swap(other.m_name);
    m_descending.swap(other.m_descending);
}
