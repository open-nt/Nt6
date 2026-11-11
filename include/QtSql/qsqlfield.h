// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtSql/qtsqlglobal.h>

class QDebug;

class QSqlField {
public:
    /*!
        \enum QSqlField::RequiredStatus

        Whether a field must have a value when a row is inserted.

        \value Unknown The database does not tell.
        \value Optional The field may be left out.
        \value Required The field needs a value.
    */
    enum RequiredStatus { Unknown = -1, Optional = 0, Required = 1 };

    explicit QSqlField(const QString &fieldName = QString(), QMetaType type = QMetaType(),
                       const QString &tableName = QString());

    void setValue(const QVariant &value);
    QVariant value() const;
    void setName(const QString &name);
    QString name() const;
    void setTableName(const QString &tableName);
    QString tableName() const;
    bool isNull() const;
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;
    void clear();
    QMetaType metaType() const;
    void setMetaType(QMetaType type);
    bool isAutoValue() const;
    void setAutoValue(bool autoVal);
    void setRequiredStatus(RequiredStatus status);
    void setRequired(bool required);
    RequiredStatus requiredStatus() const;
    void setLength(int fieldLength);
    int length() const;
    void setPrecision(int precision);
    int precision() const;
    void setDefaultValue(const QVariant &value);
    QVariant defaultValue() const;
    void setGenerated(bool gen);
    bool isGenerated() const;
    bool isValid() const;
    void swap(QSqlField &other) noexcept;

    bool operator==(const QSqlField &other) const;
    /*!
        Returns \c true if this field differs from \a other; otherwise
        returns \c false.
    */
    bool operator!=(const QSqlField &other) const { return !operator==(other); }

private:
    QString m_name;
    QString m_table;
    QVariant m_value;
    QVariant m_default;
    QMetaType m_type;
    RequiredStatus m_required = Unknown;
    int m_length = -1;
    int m_precision = -1;
    bool m_readOnly = false;
    bool m_generated = true;
    bool m_auto = false;
};

QDebug operator<<(QDebug dbg, const QSqlField &field);
