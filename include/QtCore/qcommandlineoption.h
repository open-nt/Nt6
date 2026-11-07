// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QCommandLineOption {
public:
    /*!
        \enum QCommandLineOption::Flag

        Flags that change how an option is shown in the help text.

        \value HiddenFromHelp The option is left out of QCommandLineParser::helpText().
        \value ShortOptionStyle The option's names are all shown with a single dash, even the long ones.
    */
    enum Flag { HiddenFromHelp = 0x1, ShortOptionStyle = 0x2 };
    Q_DECLARE_FLAGS(Flags, Flag)

    /*!
        Constructs a command line option with the single name \a name. The
        option takes no value.
    */
    explicit QCommandLineOption(const QString &name) : QCommandLineOption(QStringList(name)) {}
    explicit QCommandLineOption(const QStringList &names);
    /*!
        \overload
        Constructs a command line option with the single name \a name, the
        help text \a description and, if \a valueName is not empty, a value
        with that name and the default \a defaultValue.
    */
    QCommandLineOption(const QString &name, const QString &description, const QString &valueName = QString(), const QString &defaultValue = QString())
        : QCommandLineOption(QStringList(name), description, valueName, defaultValue) {}
    QCommandLineOption(const QStringList &names, const QString &description, const QString &valueName = QString(), const QString &defaultValue = QString());

    /*!
        Returns the names of the option. A name of one character is a short
        option, a longer name a long option.
    */
    QStringList names() const { return m_names; }
    /*!
        Sets the name of the option's value to \a name. An option with a
        non-empty value name takes a value; the name is shown in the help
        text, as in \c{--output <name>}.

        \sa valueName()
    */
    void setValueName(const QString &name) { m_valueName = name; }
    /*!
        Returns the name of the option's value, or an empty string if the
        option takes no value.

        \sa setValueName()
    */
    QString valueName() const { return m_valueName; }
    /*!
        Sets the description shown in the help text to \a description.

        \sa description()
    */
    void setDescription(const QString &description) { m_description = description; }
    /*!
        Returns the description shown in the help text.

        \sa setDescription()
    */
    QString description() const { return m_description; }
    /*!
        Sets the default value of the option to \a value, which is used when
        the option is not given. An empty \a value clears the defaults.

        \sa setDefaultValues(), defaultValues()
    */
    void setDefaultValue(const QString &value) { m_defaults = value.isEmpty() ? QStringList() : QStringList(value); }
    /*!
        Sets the default values of the option to \a values.

        \sa setDefaultValue(), defaultValues()
    */
    void setDefaultValues(const QStringList &values) { m_defaults = values; }
    /*!
        Returns the default values of the option, which are empty unless set.

        \sa setDefaultValues()
    */
    QStringList defaultValues() const { return m_defaults; }
    /*!
        Returns the flags of the option.

        \sa setFlags()
    */
    Flags flags() const { return m_flags; }
    /*!
        Sets the flags of the option to \a f.

        \sa flags()
    */
    void setFlags(Flags f) { m_flags = f; }

private:
    QStringList m_names;
    QString m_valueName;
    QString m_description;
    QStringList m_defaults;
    Flags m_flags;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCommandLineOption::Flags)
