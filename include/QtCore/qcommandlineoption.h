// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QCommandLineOption {
public:
    enum Flag { HiddenFromHelp = 0x1, ShortOptionStyle = 0x2 };
    Q_DECLARE_FLAGS(Flags, Flag)

    explicit QCommandLineOption(const QString &name) : QCommandLineOption(QStringList(name)) {}
    explicit QCommandLineOption(const QStringList &names);
    QCommandLineOption(const QString &name, const QString &description, const QString &valueName = QString(), const QString &defaultValue = QString())
        : QCommandLineOption(QStringList(name), description, valueName, defaultValue) {}
    QCommandLineOption(const QStringList &names, const QString &description, const QString &valueName = QString(), const QString &defaultValue = QString());

    QStringList names() const { return m_names; }
    void setValueName(const QString &name) { m_valueName = name; }
    QString valueName() const { return m_valueName; }
    void setDescription(const QString &description) { m_description = description; }
    QString description() const { return m_description; }
    void setDefaultValue(const QString &value) { m_defaults = value.isEmpty() ? QStringList() : QStringList(value); }
    void setDefaultValues(const QStringList &values) { m_defaults = values; }
    QStringList defaultValues() const { return m_defaults; }
    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

private:
    QStringList m_names;
    QString m_valueName;
    QString m_description;
    QStringList m_defaults;
    Flags m_flags;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCommandLineOption::Flags)
