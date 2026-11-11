// MIT License - see LICENSE
#include <QtSql/qsqlfield.h>

#include <QtCore/qdebug.h>

#include <utility>

/*!
    \class QSqlField
    \brief The QSqlField class manipulates the fields of SQL database tables and views.

    A QSqlField stands for one column: it has a name(), a type
    (metaType()), a value() and what the database tells about the column,
    such as requiredStatus(), length() and defaultValue(). Fields are mostly
    met as the parts of a QSqlRecord.

    \code
    QSqlField field("age", QMetaType::fromType<int>());
    field.setValue(42);
    field.isNull();   // false
    field.clear();
    field.isNull();   // true
    \endcode

    A read-only field ignores setValue() and clear(). setGenerated() tells
    QSqlTableModel and QSqlDriver::sqlStatement() whether the field takes
    part in the SQL they generate.

    \differences
    A field is a plain value: copies are not implicitly shared. A value
    that is set keeps its own type and is not converted to metaType().
    \enddifferences

    \sa QSqlRecord
*/

/*!
    Constructs a field named \a fieldName of the type \a type that belongs
    to the table \a tableName. The value of the field is null.
*/
QSqlField::QSqlField(const QString &fieldName, QMetaType type, const QString &tableName)
    : m_name(fieldName), m_table(tableName), m_value(type), m_type(type) {}

/*!
    Sets the value of the field to \a value. Nothing happens if the field
    is read-only.

    \sa value(), isReadOnly(), clear()
*/
void QSqlField::setValue(const QVariant &value) {
    if (m_readOnly)
        return;
    m_value = value;
}

/*!
    Returns the value of the field. Use isNull() to tell a null value.

    \sa setValue()
*/
QVariant QSqlField::value() const { return m_value; }

/*!
    Sets the name of the field to \a name.

    \sa name()
*/
void QSqlField::setName(const QString &name) { m_name = name; }

/*!
    Returns the name of the field.

    \sa setName()
*/
QString QSqlField::name() const { return m_name; }

/*!
    Sets the name of the table that the field belongs to, to \a tableName.

    \sa tableName()
*/
void QSqlField::setTableName(const QString &tableName) { m_table = tableName; }

/*!
    Returns the name of the table that the field belongs to, which may be
    empty.

    \sa setTableName()
*/
QString QSqlField::tableName() const { return m_table; }

/*!
    Returns \c true if the value of the field is null; otherwise returns
    \c false.

    \sa clear()
*/
bool QSqlField::isNull() const { return m_value.isNull(); }

/*!
    Makes the field read-only if \a readOnly is \c true, and writable
    otherwise. The value of a read-only field cannot be changed.

    \sa isReadOnly()
*/
void QSqlField::setReadOnly(bool readOnly) { m_readOnly = readOnly; }

/*!
    Returns \c true if the value of the field cannot be changed; otherwise
    returns \c false.

    \sa setReadOnly()
*/
bool QSqlField::isReadOnly() const { return m_readOnly; }

/*!
    Sets the value of the field to a null value of the type of the field.
    Nothing happens if the field is read-only.

    \sa isNull(), setValue()
*/
void QSqlField::clear() {
    if (m_readOnly)
        return;
    m_value = QVariant(m_type);
}

/*!
    Returns the type of the field in the database.

    \sa setMetaType()
*/
QMetaType QSqlField::metaType() const { return m_type; }

/*!
    Sets the type of the field to \a type. A null value takes the new type;
    another value stays as it is.

    \sa metaType()
*/
void QSqlField::setMetaType(QMetaType type) {
    m_type = type;
    if (m_value.isNull())
        m_value = QVariant(type);
}

/*!
    Returns \c true if the database gives the field its value, as for an
    auto-incremented key; otherwise returns \c false.

    \sa setAutoValue()
*/
bool QSqlField::isAutoValue() const { return m_auto; }

/*!
    Marks the field as one that the database gives its value if \a autoVal
    is \c true.

    \sa isAutoValue()
*/
void QSqlField::setAutoValue(bool autoVal) { m_auto = autoVal; }

/*!
    Sets whether the field must have a value to \a status.

    \sa requiredStatus(), setRequired()
*/
void QSqlField::setRequiredStatus(RequiredStatus status) { m_required = status; }

/*!
    Sets the required status of the field to \c Required if \a required is
    \c true, and to \c Optional otherwise.

    \sa setRequiredStatus()
*/
void QSqlField::setRequired(bool required) { m_required = required ? Required : Optional; }

/*!
    Returns whether the field must have a value when a row is inserted.

    \sa setRequiredStatus()
*/
QSqlField::RequiredStatus QSqlField::requiredStatus() const { return m_required; }

/*!
    Sets the length of the field to \a fieldLength: the number of
    characters of a string, for example.

    \sa length()
*/
void QSqlField::setLength(int fieldLength) { m_length = fieldLength; }

/*!
    Returns the length of the field, or a negative number if the database
    does not tell.

    \sa setLength()
*/
int QSqlField::length() const { return m_length; }

/*!
    Sets the precision of the field to \a precision: the number of digits
    after the decimal point of a number.

    \sa precision()
*/
void QSqlField::setPrecision(int precision) { m_precision = precision; }

/*!
    Returns the precision of the field, or a negative number if the
    database does not tell.

    \sa setPrecision()
*/
int QSqlField::precision() const { return m_precision; }

/*!
    Sets the default value of the field to \a value.

    \sa defaultValue()
*/
void QSqlField::setDefaultValue(const QVariant &value) { m_default = value; }

/*!
    Returns the value that the database gives the field when a row is
    inserted without one.

    \sa setDefaultValue()
*/
QVariant QSqlField::defaultValue() const { return m_default; }

/*!
    Sets whether the field takes part in generated SQL statements to
    \a gen.

    \sa isGenerated()
*/
void QSqlField::setGenerated(bool gen) { m_generated = gen; }

/*!
    Returns \c true if the field takes part in generated SQL statements,
    which is the default; otherwise returns \c false.

    \sa setGenerated()
*/
bool QSqlField::isGenerated() const { return m_generated; }

/*!
    Returns \c true if the field has a valid type; otherwise returns
    \c false.
*/
bool QSqlField::isValid() const { return m_type.isValid(); }

/*!
    Swaps this field with \a other.
*/
void QSqlField::swap(QSqlField &other) noexcept { std::swap(*this, other); }

/*!
    Returns \c true if this field equals \a other in every respect, the
    value included; otherwise returns \c false.
*/
bool QSqlField::operator==(const QSqlField &other) const {
    return m_name == other.m_name && m_table == other.m_table && m_type == other.m_type &&
           m_value.isNull() == other.m_value.isNull() && (m_value.isNull() || m_value == other.m_value) &&
           m_default == other.m_default && m_required == other.m_required && m_length == other.m_length &&
           m_precision == other.m_precision && m_readOnly == other.m_readOnly && m_generated == other.m_generated &&
           m_auto == other.m_auto;
}

QDebug operator<<(QDebug dbg, const QSqlField &field) {
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QSqlField(" << field.name() << ", " << field.metaType().name();
    if (!field.tableName().isEmpty())
        dbg << ", tableName: " << field.tableName();
    if (field.length() >= 0)
        dbg << ", length: " << field.length();
    if (field.precision() >= 0)
        dbg << ", precision: " << field.precision();
    if (field.requiredStatus() != QSqlField::Unknown)
        dbg << ", required: " << (field.requiredStatus() == QSqlField::Required ? "yes" : "no");
    dbg << ", generated: " << (field.isGenerated() ? "yes" : "no");
    if (!field.defaultValue().isNull())
        dbg << ", defaultValue: " << field.defaultValue();
    if (field.isAutoValue())
        dbg << ", auto-value: yes";
    dbg << ")";
    return dbg;
}
