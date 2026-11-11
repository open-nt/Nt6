// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtSql/qsqlrecord.h>

class QSqlIndex : public QSqlRecord {
public:
    explicit QSqlIndex(const QString &cursorName = QString(), const QString &name = QString());
    QSqlIndex(const QSqlIndex &other);
    QSqlIndex(QSqlIndex &&other) noexcept;
    QSqlIndex &operator=(const QSqlIndex &other);
    QSqlIndex &operator=(QSqlIndex &&other) noexcept;
    ~QSqlIndex() override;

    void setCursorName(const QString &cursorName);
    QString cursorName() const;
    void setName(const QString &name);
    QString name() const;

    void append(const QSqlField &field);
    void append(const QSqlField &field, bool desc);

    bool isDescending(int i) const;
    void setDescending(int i, bool desc);
    void swap(QSqlIndex &other) noexcept;

private:
    QString m_cursor;
    QString m_name;
    QList<int> m_descending;
};
