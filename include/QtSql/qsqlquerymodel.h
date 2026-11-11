// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtSql/qsqldatabase.h>
#include <QtSql/qsqlerror.h>
#include <QtSql/qsqlquery.h>
#include <QtSql/qsqlrecord.h>

class QSqlQueryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit QSqlQueryModel(QObject *parent = nullptr);
    ~QSqlQueryModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QSqlRecord record(int row) const;
    QSqlRecord record() const;

    QVariant data(const QModelIndex &item, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
                       int role = Qt::EditRole) override;

    bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;

    void setQuery(QSqlQuery &&query);
    void setQuery(const QString &query, const QSqlDatabase &db = QSqlDatabase());
    const QSqlQuery &query() const;

    virtual void clear();

    QSqlError lastError() const;

    void fetchMore(const QModelIndex &parent = QModelIndex()) override;
    bool canFetchMore(const QModelIndex &parent = QModelIndex()) const override;

    QHash<int, QByteArray> roleNames() const override;

protected:
    virtual void queryChange();
    virtual QModelIndex indexInQuery(const QModelIndex &item) const;
    void setLastError(const QSqlError &error);

private:
    friend class QSqlTableModel;
    void prefetch(int row);
    void initColumns(const QSqlRecord &rec);

    mutable QSqlQuery m_query;
    QSqlError m_error;
    QSqlRecord m_rec;
    QList<int> m_columnInQuery;            // for each model column: its position in the query, or -1
    QList<QHash<int, QVariant>> m_headers; // for each model column: the header data that was set, by role
    int m_bottom = -1;                     // the last row that was fetched
    bool m_atEnd = true;
};
