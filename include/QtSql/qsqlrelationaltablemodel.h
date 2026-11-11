// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtSql/qsqltablemodel.h>

class QSqlRelation {
public:
    /*!
        Constructs an invalid relation, which stands for no relation.

        \sa isValid()
    */
    QSqlRelation() = default;
    /*!
        Constructs a relation to the table \a tableName: a foreign key
        holds a value of the column \a indexColumn of that table, and is
        shown as the value that the column \a displayColumn has in the same
        row.
    */
    QSqlRelation(const QString &tableName, const QString &indexColumn, const QString &displayColumn)
        : m_table(tableName), m_index(indexColumn), m_display(displayColumn) {}

    /*!
        Swaps this relation with \a other.
    */
    void swap(QSqlRelation &other) noexcept {
        m_table.swap(other.m_table);
        m_index.swap(other.m_index);
        m_display.swap(other.m_display);
    }

    /*!
        Returns the name of the table that the foreign key refers to.
    */
    QString tableName() const { return m_table; }
    /*!
        Returns the name of the column of tableName() that the foreign key
        holds a value of.
    */
    QString indexColumn() const { return m_index; }
    /*!
        Returns the name of the column of tableName() that is shown in the
        place of the foreign key.
    */
    QString displayColumn() const { return m_display; }
    /*!
        Returns \c true if the relation has a table, an index column and a
        display column; otherwise returns \c false.
    */
    bool isValid() const { return !m_table.isEmpty() && !m_index.isEmpty() && !m_display.isEmpty(); }

private:
    QString m_table;
    QString m_index;
    QString m_display;
};

class QSqlRelationalTableModel : public QSqlTableModel {
    Q_OBJECT
public:
    /*!
        \enum QSqlRelationalTableModel::JoinMode

        Which rows the model has when a foreign key finds no row in the
        table that it refers to.

        \value InnerJoin Only the rows whose foreign keys all find a row. This is the default.
        \value LeftJoin All the rows; a foreign key that is null or finds no row is shown as null.
    */
    enum JoinMode { InnerJoin, LeftJoin };

    explicit QSqlRelationalTableModel(QObject *parent = nullptr, const QSqlDatabase &db = QSqlDatabase());
    ~QSqlRelationalTableModel() override;

    QVariant data(const QModelIndex &item, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &item, const QVariant &value, int role = Qt::EditRole) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;

    void clear() override;
    bool select() override;

    void setTable(const QString &tableName) override;
    virtual void setRelation(int column, const QSqlRelation &relation);
    QSqlRelation relation(int column) const;
    virtual QSqlTableModel *relationModel(int column) const;
    void setJoinMode(JoinMode joinMode);

public slots:
    void revertRow(int row) override;

protected:
    QString selectStatement() const override;
    QString orderByClause() const override;

private:
    struct Related {
        QSqlRelation relation;
        QSqlTableModel *model = nullptr;  // made by relationModel(); a child of this model
        QHash<QString, QVariant> display; // the display value for each key, as text
        bool loaded = false;
    };
    bool hasRelation(int column) const;
    const QHash<QString, QVariant> &dictionary(int column) const;
    QString relationAlias(int column) const;
    QString displayAlias(int column) const;

    mutable QList<Related> m_relations; // by column
    JoinMode m_joinMode = InnerJoin;
};
