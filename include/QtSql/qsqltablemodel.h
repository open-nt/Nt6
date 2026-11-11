// MIT License - see LICENSE
#pragma once

#include <QtCore/qmap.h>
#include <QtSql/qsqldatabase.h>
#include <QtSql/qsqlindex.h>
#include <QtSql/qsqlquerymodel.h>
#include <QtSql/qsqlrecord.h>

class QSqlTableModel : public QSqlQueryModel {
    Q_OBJECT
public:
    /*!
        \enum QSqlTableModel::EditStrategy

        When the changes that are made to the model are written to the
        database.

        \value OnFieldChange At once, with each item that is changed.
        \value OnRowChange When another row is changed, inserted or removed, or submit() is called. This is the default.
        \value OnManualSubmit Only when submitAll() is called. Until then the changes are kept in the model, and revertAll() drops them.
    */
    enum EditStrategy { OnFieldChange, OnRowChange, OnManualSubmit };

    explicit QSqlTableModel(QObject *parent = nullptr, const QSqlDatabase &db = QSqlDatabase());
    ~QSqlTableModel() override;

    virtual void setTable(const QString &tableName);
    QString tableName() const;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QSqlRecord record() const;
    QSqlRecord record(int row) const;
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool clearItemData(const QModelIndex &index) override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool isDirty() const;
    bool isDirty(const QModelIndex &index) const;

    void clear() override;

    virtual void setEditStrategy(EditStrategy strategy);
    EditStrategy editStrategy() const;

    QSqlIndex primaryKey() const;
    QSqlDatabase database() const;
    int fieldIndex(const QString &fieldName) const;

    void sort(int column, Qt::SortOrder order) override;
    virtual void setSort(int column, Qt::SortOrder order);

    QString filter() const;
    virtual void setFilter(const QString &filter);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    bool insertRecord(int row, const QSqlRecord &record);
    bool setRecord(int row, const QSqlRecord &record);

    virtual void revertRow(int row);

public slots:
    virtual bool select();
    virtual bool selectRow(int row);

    bool submit() override;
    void revert() override;

    bool submitAll();
    void revertAll();

signals:
    /*!
        This signal is emitted by insertRows() for each new row, with its
        position \a row and its empty \a record. A slot can give the fields
        of \a record their first values; a field that it sets is written
        when the row is inserted into the table.
    */
    void primeInsert(int row, QSqlRecord &record) { emitSignal(&QSqlTableModel::primeInsert, row, record); }
    /*!
        This signal is emitted before a new row is inserted into the table
        of the database, with the values that are about to be written in
        \a record. A slot can still change them.
    */
    void beforeInsert(QSqlRecord &record) { emitSignal(&QSqlTableModel::beforeInsert, record); }
    /*!
        This signal is emitted before the row \a row is updated in the
        table of the database, with the values that are about to be written
        in \a record: the fields that are marked as generated. A slot can
        still change them.
    */
    void beforeUpdate(int row, QSqlRecord &record) { emitSignal(&QSqlTableModel::beforeUpdate, row, record); }
    /*!
        This signal is emitted before the row \a row is deleted from the
        table of the database.
    */
    void beforeDelete(int row) { emitSignal(&QSqlTableModel::beforeDelete, row); }

protected:
    virtual bool updateRowInTable(int row, const QSqlRecord &values);
    virtual bool insertRowInTable(const QSqlRecord &values);
    virtual bool deleteRowFromTable(int row);
    virtual QString orderByClause() const;
    virtual QString selectStatement() const;

    void setPrimaryKey(const QSqlIndex &key);
    QSqlRecord primaryValues(int row) const;
    QModelIndex indexInQuery(const QModelIndex &item) const override;

private:
    friend class QSqlRelationalTableModel;
    // What waits to be done to a row. None: nothing; the row is kept because the model knows it better than
    // the query does, having read it again or inserted it since the query ran.
    enum Op { None, Insert, Update, Delete };
    struct ModifiedRow {
        Op op = None;
        QSqlRecord rec;           // the values of the row; a field with a pending change is marked as generated
        QSqlRecord dbRec;         // the values that the row has in the table
        QSqlRecord primaryValues; // what finds the row in the table; empty for a row that is not there yet
        bool inserted = false;    // the row is not one of the rows of the query
        bool refreshed = false;   // dbRec was read after the query ran
    };

    int baseRow(int row) const;
    int insertedBefore(int row) const;
    QSqlRecord baseRecord(int row) const;
    ModifiedRow &touch(int row);
    bool exec(const QString &statement, const QSqlRecord &values, const QSqlRecord &whereValues,
              QVariant *insertId = nullptr);
    bool submitPending();
    bool dirtyOtherThan(int row) const;
    bool refreshRow(ModifiedRow &entry);
    QString rowCondition(const QSqlRecord &key) const;
    void rowChanged(int row);

    QList<int> m_nonKeyColumns; // columns whose values in the model are not those of the table
    QVariant m_insertId;        // the row id of the last insertRowInTable()

    QSqlDatabase m_db;
    QString m_table;
    QString m_filter;
    QSqlRecord m_baseRec;
    QSqlIndex m_primaryIndex;
    QMap<int, ModifiedRow> m_cache;
    EditStrategy m_strategy = OnRowChange;
    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};
