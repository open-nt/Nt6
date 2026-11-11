// MIT License - see LICENSE
#include <QtSql/qsqlrelationaltablemodel.h>

#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlfield.h>
#include <QtSql/qsqlquery.h>

/*!
    \class QSqlRelation
    \brief The QSqlRelation class stores information about an SQL foreign key.

    A QSqlRelation tells QSqlRelationalTableModel::setRelation() what a
    foreign key column refers to: the table, the column of that table whose
    values the key holds, and the column of that table that is shown to the
    user instead.

    \code
    // employee.city holds a value of city.id, and is shown as city.name
    model.setRelation(2, QSqlRelation("city", "id", "name"));
    \endcode

    \differences
    None.
    \enddifferences

    \sa QSqlRelationalTableModel
*/

/*!
    \class QSqlRelationalTableModel
    \brief The QSqlRelationalTableModel class provides an editable data model for a single database table, with foreign key support.

    A QSqlRelationalTableModel is a QSqlTableModel that resolves foreign
    keys: a column that holds the key of a row in another table shows a
    value from that row, a name in the place of a number.

    \code
    QSqlRelationalTableModel model;
    model.setTable("employee");
    model.setRelation(2, QSqlRelation("city", "id", "name"));
    model.setRelation(3, QSqlRelation("country", "id", "name"));
    model.select();
    model.data(model.index(0, 2)); // "Oslo", not 5
    \endcode

    Set the relations after setTable() and before select(). The rows come
    from a join of the table with the tables that it refers to; with the
    default join mode, a row whose foreign key is null or finds nothing is
    left out, and setJoinMode() with \c LeftJoin keeps it.

    A column with a relation is changed by giving it a key: setData() takes
    a value of the index column of the relation, writes it to the table, and
    the item shows the display value that goes with it. relationModel()
    gives the rows of the other table to choose from.

    The column of a relation is named after the display column in record()
    and in the header: \c name in the example above, or, if the model
    would then have two fields of the same name, the name of the other
    table, the display column and the number of the column, joined by
    underscores, as in \c country_name_3.

    \differences
    The join is always written as \c{INNER JOIN} or \c{LEFT JOIN}. The
    display values of a relation are read when they are first needed and
    again after each select(). A row is found in the table by its primary
    key; in a table without one, by its columns that have no relation.
    \c updateRowInTable() and \c insertRowInTable() are not reimplemented,
    as the model keeps the fields of its own table for writing.
    \c QSqlRelationalDelegate is not provided.
    \enddifferences

    \sa QSqlRelation, QSqlTableModel, {Using Nt SQL}
*/

namespace {
QString escaped(const QSqlDriver *driver, const QString &name, QSqlDriver::IdentifierType type) {
    return driver->isIdentifierEscaped(name, type) ? name : driver->escapeIdentifier(name, type);
}
} // namespace

/*!
    Constructs an empty model with the given \a parent, on the connection
    \a db, or on the default connection if \a db is not valid.
*/
QSqlRelationalTableModel::QSqlRelationalTableModel(QObject *parent, const QSqlDatabase &db)
    : QSqlTableModel(parent, db) {}

/*!
    Destroys the model, with the models of its relations.
*/
QSqlRelationalTableModel::~QSqlRelationalTableModel() = default;

bool QSqlRelationalTableModel::hasRelation(int column) const {
    return column >= 0 && column < m_relations.size() && m_relations.at(column).relation.isValid();
}

// The name that the table of the relation of a column has in the statement.
QString QSqlRelationalTableModel::relationAlias(int column) const {
    return QStringLiteral("relTblAl_") + QString::number(column);
}

// The name that the display column of a relation has in the rows, if it needs another one than its own:
// when the model would otherwise have two fields of the same name.
QString QSqlRelationalTableModel::displayAlias(int column) const {
    const QString name = m_relations.at(column).relation.displayColumn();
    int uses = 0;
    for (int i = 0; i < m_baseRec.count(); ++i) {
        const QString other = hasRelation(i) ? m_relations.at(i).relation.displayColumn() : m_baseRec.fieldName(i);
        if (other.compare(name, Qt::CaseInsensitive) == 0)
            ++uses;
    }
    if (uses < 2)
        return QString();
    return m_relations.at(column).relation.tableName() + QChar('_') + name + QChar('_') + QString::number(column);
}

// The display values of the relation of a column, by key. Keys are compared as text.
const QHash<QString, QVariant> &QSqlRelationalTableModel::dictionary(int column) const {
    Related &related = m_relations[column];
    if (related.loaded)
        return related.display;
    related.loaded = true;
    related.display.clear();
    const QSqlDriver *driver = database().driver();
    const QString statement =
        QStringLiteral("SELECT ") + escaped(driver, related.relation.indexColumn(), QSqlDriver::FieldName) +
        QStringLiteral(", ") + escaped(driver, related.relation.displayColumn(), QSqlDriver::FieldName) +
        QStringLiteral(" FROM ") + escaped(driver, related.relation.tableName(), QSqlDriver::TableName);
    QSqlQuery q(database());
    q.setForwardOnly(true);
    if (q.exec(statement)) {
        while (q.next())
            related.display.insert(q.value(0).toString(), q.value(1));
    }
    return related.display;
}

/*!
    Returns the value of the item at \a item for the display role or the
    edit role. In a column with a relation this is the display value of the
    relation: the one that the row has in the database, or, for an item that
    was given another key with setData(), the one that goes with that key.

    \sa setData(), setRelation()
*/
QVariant QSqlRelationalTableModel::data(const QModelIndex &item, int role) const {
    if ((role == Qt::DisplayRole || role == Qt::EditRole) && item.isValid() && hasRelation(item.column())) {
        const auto it = m_cache.constFind(item.row());
        if (it != m_cache.constEnd() && it.value().rec.isGenerated(item.column())) {
            // The row holds the key that was set, which has not been written yet.
            const QVariant key = it.value().rec.value(item.column());
            if (key.isNull())
                return key;
            const QHash<QString, QVariant> &display = dictionary(item.column());
            const auto found = display.constFind(key.toString());
            return found != display.constEnd() ? found.value() : key;
        }
    }
    return QSqlTableModel::data(item, role);
}

/*!
    Sets the value of the item at \a item to \a value and returns \c true
    on success, as QSqlTableModel::setData() does. For a column with a
    relation, \a value is a key: a value of the index column of the
    relation. It is refused, and \c false returned, if the other table has
    no row with that key; a null value is accepted.

    \sa data(), relationModel()
*/
bool QSqlRelationalTableModel::setData(const QModelIndex &item, const QVariant &value, int role) {
    if (role == Qt::EditRole && item.isValid() && hasRelation(item.column()) && !value.isNull()) {
        if (!dictionary(item.column()).contains(value.toString()))
            return false;
    }
    return QSqlTableModel::setData(item, value, role);
}

/*!
    Removes \a count columns starting with the column \a column from the
    model, with their relations, and returns \c true on success. \a parent
    has to be invalid.
*/
bool QSqlRelationalTableModel::removeColumns(int column, int count, const QModelIndex &parent) {
    if (parent.isValid() || column < 0 || count <= 0 || column + count > m_baseRec.count())
        return false;
    // The relations move with their columns before the rows are read again.
    for (int i = 0; i < count && column < m_relations.size(); ++i) {
        delete m_relations.at(column).model;
        m_relations.removeAt(column);
    }
    const bool ok = QSqlTableModel::removeColumns(column, count, parent);
    m_nonKeyColumns.clear();
    for (int i = 0; i < m_relations.size(); ++i) {
        if (m_relations.at(i).relation.isValid())
            m_nonKeyColumns.append(i);
    }
    return ok;
}

/*!
    Empties the model: forgets the table, the relations, the rows and all
    pending changes. The join mode is kept.
*/
void QSqlRelationalTableModel::clear() {
    for (const Related &related : m_relations)
        delete related.model;
    m_relations.clear();
    QSqlTableModel::clear();
}

/*!
    Reads the rows of the table again, joined with the tables of the
    relations, and returns \c true on success. The display values of the
    relations are read again when they are next needed.

    \sa setRelation(), selectStatement()
*/
bool QSqlRelationalTableModel::select() {
    for (Related &related : m_relations) {
        related.loaded = false;
        related.display.clear();
    }
    return QSqlTableModel::select();
}

/*!
    Makes the model work on the table \a tableName, without relations. Set
    them with setRelation() afterwards.
*/
void QSqlRelationalTableModel::setTable(const QString &tableName) { QSqlTableModel::setTable(tableName); }

/*!
    Makes the column \a column a foreign key as \a relation describes. An
    invalid relation removes the relation of the column. Call this after
    setTable() and before select().

    \sa relation(), relationModel()
*/
void QSqlRelationalTableModel::setRelation(int column, const QSqlRelation &relation) {
    if (column < 0)
        return;
    while (m_relations.size() <= column)
        m_relations.append(Related());
    Related &related = m_relations[column];
    delete related.model;
    related = Related();
    related.relation = relation;
    m_nonKeyColumns.removeAll(column);
    if (relation.isValid())
        m_nonKeyColumns.append(column);
}

/*!
    Returns the relation of the column \a column, or an invalid relation if
    it has none.

    \sa setRelation()
*/
QSqlRelation QSqlRelationalTableModel::relation(int column) const {
    return column >= 0 && column < m_relations.size() ? m_relations.at(column).relation : QSqlRelation();
}

/*!
    Returns a model of the table that the column \a column refers to, with
    its rows read, or \c nullptr if the column has no relation. The model
    belongs to this one. It is meant for choosing a key, in a combo box for
    example; changes that are made through it are not seen by this model
    until its next select().

    \sa setRelation()
*/
QSqlTableModel *QSqlRelationalTableModel::relationModel(int column) const {
    if (!hasRelation(column))
        return nullptr;
    Related &related = m_relations[column];
    if (!related.model) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): the child is made when it is first asked for
        related.model = new QSqlTableModel(const_cast<QSqlRelationalTableModel *>(this), database());
        related.model->setTable(related.relation.tableName());
        related.model->select();
    }
    return related.model;
}

/*!
    Sets which rows the model has when a foreign key finds no row to
    \a joinMode, for the next select().
*/
void QSqlRelationalTableModel::setJoinMode(JoinMode joinMode) { m_joinMode = joinMode; }

/*!
    Drops the pending changes of the row \a row, as
    QSqlTableModel::revertRow() does.
*/
void QSqlRelationalTableModel::revertRow(int row) { QSqlTableModel::revertRow(row); }

/*!
    Returns the \c SELECT statement that select() runs: the fields of the
    table, with the display column of the other table in the place of each
    foreign key, from the table joined with the tables of the relations,
    with the filter and the sort order. In the filter, a column of another
    table is written with the name that the table has in the statement,
    \c relTblAl_ followed by the number of the column of the relation, as in
    \c{relTblAl_2.name = 'Oslo'}.

    \sa select(), setJoinMode(), setFilter()
*/
QString QSqlRelationalTableModel::selectStatement() const {
    bool any = false;
    for (int i = 0; i < m_baseRec.count() && !any; ++i)
        any = hasRelation(i) && m_baseRec.isGenerated(i);
    if (!any || m_table.isEmpty())
        return QSqlTableModel::selectStatement();

    const QSqlDriver *driver = database().driver();
    const QString table = escaped(driver, m_table, QSqlDriver::TableName);
    QString fields;
    QString joins;
    for (int i = 0; i < m_baseRec.count(); ++i) {
        if (!m_baseRec.isGenerated(i))
            continue;
        if (!fields.isEmpty())
            fields += QStringLiteral(", ");
        const QString field = table + QChar('.') + escaped(driver, m_baseRec.fieldName(i), QSqlDriver::FieldName);
        if (!hasRelation(i)) {
            fields += field;
            continue;
        }
        const QSqlRelation &rel = m_relations.at(i).relation;
        const QString alias = relationAlias(i);
        fields += alias + QChar('.') + escaped(driver, rel.displayColumn(), QSqlDriver::FieldName);
        const QString displayName = displayAlias(i);
        if (!displayName.isEmpty())
            fields += QStringLiteral(" AS ") + driver->escapeIdentifier(displayName, QSqlDriver::FieldName);
        joins += (m_joinMode == LeftJoin ? QStringLiteral(" LEFT JOIN ") : QStringLiteral(" INNER JOIN ")) +
                 escaped(driver, rel.tableName(), QSqlDriver::TableName) + QChar(' ') + alias +
                 QStringLiteral(" ON (") + field + QStringLiteral(" = ") + alias + QChar('.') +
                 escaped(driver, rel.indexColumn(), QSqlDriver::FieldName) + QChar(')');
    }
    QString statement = QStringLiteral("SELECT ") + fields + QStringLiteral(" FROM ") + table + joins;
    if (!m_filter.isEmpty())
        statement += QStringLiteral(" WHERE (") + m_filter + QChar(')');
    const QString order = orderByClause();
    if (!order.isEmpty())
        statement += QChar(' ') + order;
    return statement;
}

/*!
    Returns the \c{ORDER BY} clause for the column and the order of
    setSort(). A column with a relation is sorted by its display values.
*/
QString QSqlRelationalTableModel::orderByClause() const {
    if (!hasRelation(m_sortColumn) || m_sortColumn >= m_baseRec.count())
        return QSqlTableModel::orderByClause();
    const QSqlDriver *driver = database().driver();
    return QStringLiteral("ORDER BY ") + relationAlias(m_sortColumn) + QChar('.') +
           escaped(driver, m_relations.at(m_sortColumn).relation.displayColumn(), QSqlDriver::FieldName) +
           (m_sortOrder == Qt::AscendingOrder ? QStringLiteral(" ASC") : QStringLiteral(" DESC"));
}

#include "moc_qsqlrelationaltablemodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
