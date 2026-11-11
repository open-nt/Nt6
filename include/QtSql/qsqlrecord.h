// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qanystringview.h>
#include <QtCore/qvariant.h>
#include <QtSql/qsqlfield.h>

class QDebug;

class QSqlRecord {
public:
    QSqlRecord();
    QSqlRecord(const QSqlRecord &other);
    QSqlRecord(QSqlRecord &&other) noexcept;
    QSqlRecord &operator=(const QSqlRecord &other);
    QSqlRecord &operator=(QSqlRecord &&other) noexcept;
    virtual ~QSqlRecord();

    bool operator==(const QSqlRecord &other) const;
    /*!
        Returns \c true if this record differs from \a other; otherwise
        returns \c false.
    */
    bool operator!=(const QSqlRecord &other) const { return !operator==(other); }

    QVariant value(int i) const;
    QVariant value(QAnyStringView name) const;
    void setValue(int i, const QVariant &val);
    void setValue(QAnyStringView name, const QVariant &val);
    void setNull(int i);
    void setNull(QAnyStringView name);
    bool isNull(int i) const;
    bool isNull(QAnyStringView name) const;

    int indexOf(QAnyStringView name) const;
    QString fieldName(int i) const;
    QSqlField field(int i) const;
    QSqlField field(QAnyStringView name) const;

    bool isGenerated(int i) const;
    bool isGenerated(QAnyStringView name) const;
    void setGenerated(QAnyStringView name, bool generated);
    void setGenerated(int i, bool generated);

    void append(const QSqlField &field);
    void replace(int pos, const QSqlField &field);
    void insert(int pos, const QSqlField &field);
    void remove(int pos);

    bool isEmpty() const;
    bool contains(QAnyStringView name) const;
    void clear();
    void clearValues();
    int count() const;
    QSqlRecord keyValues(const QSqlRecord &keyFields) const;
    void swap(QSqlRecord &other) noexcept;

private:
    QList<QSqlField> m_fields;
};

QDebug operator<<(QDebug dbg, const QSqlRecord &record);
