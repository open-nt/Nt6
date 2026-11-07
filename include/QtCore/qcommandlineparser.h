// MIT License - see LICENSE
#pragma once

#include <QtCore/qcommandlineoption.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>

class QCoreApplication;

class QCommandLineParser {
public:
    /*!
        \enum QCommandLineParser::SingleDashWordOptionMode

        Controls how an argument with a single dash and several characters,
        such as \c{-abc} or \c{-name}, is parsed.

        \value ParseAsCompactedShortOptions Each character is a short option, and the rest of the word after an option that takes a value is its value.
        \value ParseAsLongOptions The word is one long option name, so \c{-name} is the same as \c{--name}.
    */
    enum SingleDashWordOptionMode { ParseAsCompactedShortOptions, ParseAsLongOptions };
    /*!
        \enum QCommandLineParser::OptionsAfterPositionalArgumentsMode

        Controls how arguments that look like options are treated after the
        first positional argument.

        \value ParseAsOptions They are still parsed as options.
        \value ParseAsPositionalArguments They are positional arguments, as if they followed \c{--}.
    */
    enum OptionsAfterPositionalArgumentsMode { ParseAsOptions, ParseAsPositionalArguments };

    QCommandLineParser();
    ~QCommandLineParser();
    QCommandLineParser(const QCommandLineParser &) = delete;
    QCommandLineParser &operator=(const QCommandLineParser &) = delete;

    /*!
        Sets how single-dash words are parsed to \a mode. The default is
        \c ParseAsCompactedShortOptions.

        \sa SingleDashWordOptionMode
    */
    void setSingleDashWordOptionMode(SingleDashWordOptionMode mode) { m_dashMode = mode; }
    /*!
        Sets how arguments after the first positional argument are parsed to
        \a mode. The default is \c ParseAsOptions.

        \sa OptionsAfterPositionalArgumentsMode
    */
    void setOptionsAfterPositionalArgumentsMode(OptionsAfterPositionalArgumentsMode mode) { m_afterMode = mode; }
    bool addOption(const QCommandLineOption &option);
    bool addOptions(const QList<QCommandLineOption> &options);
    QCommandLineOption addVersionOption();
    QCommandLineOption addHelpOption();
    /*!
        Sets the description of the application shown in the help text to
        \a description.

        \sa applicationDescription()
    */
    void setApplicationDescription(const QString &description) { m_description = description; }
    /*!
        Returns the description of the application shown in the help text.

        \sa setApplicationDescription()
    */
    QString applicationDescription() const { return m_description; }
    void addPositionalArgument(const QString &name, const QString &description, const QString &syntax = QString());
    /*!
        Removes the positional arguments described with
        addPositionalArgument(). Parsed positional arguments are not
        affected.
    */
    void clearPositionalArguments() { m_positionalHelp.clear(); }

    // Exits the process on --help, --version or a parse error. The first argument is the program name.
    void process(const QStringList &arguments);
    void process(const QCoreApplication &app);
    bool parse(const QStringList &arguments);
    /*!
        Returns the error text of the last failed parse(), or an empty string
        if it succeeded.

        \sa parse()
    */
    QString errorText() const { return m_error; }

    bool isSet(const QString &name) const;
    QString value(const QString &name) const;
    QStringList values(const QString &name) const;
    /*!
        \overload
        Returns \c true if \a option was given, looking it up by its first
        name.
    */
    bool isSet(const QCommandLineOption &option) const { return !option.names().isEmpty() && isSet(option.names().first()); }
    /*!
        \overload
        Returns the last value of \a option, looking it up by its first name.
    */
    QString value(const QCommandLineOption &option) const { return option.names().isEmpty() ? QString() : value(option.names().first()); }
    /*!
        \overload
        Returns every value of \a option, looking it up by its first name.
    */
    QStringList values(const QCommandLineOption &option) const { return option.names().isEmpty() ? QStringList() : values(option.names().first()); }
    /*!
        Returns the positional arguments found by the last parse(), in order.
        The program name is not included.

        \sa addPositionalArgument()
    */
    QStringList positionalArguments() const { return m_positional; }
    /*!
        Returns the names of the options found by the last parse(), in the
        order given and as written on the command line, with repeats.

        \sa isSet()
    */
    QStringList optionNames() const { return m_seenNames; }
    /*!
        Returns the names of options found by the last parse() that are not
        defined, without leading dashes.

        \sa errorText()
    */
    QStringList unknownOptionNames() const { return m_unknown; }
    QString helpText() const;
    [[noreturn]] void showVersion();
    [[noreturn]] void showHelp(int exitCode = 0);

private:
    struct Positional {
        QString name, description, syntax;
    };
    int indexOf(const QString &name) const;
    void record(int option, const QString &name, const QString *value);

    QList<QCommandLineOption> m_options;
    QHash<QString, int> m_nameToOption;
    QHash<int, QStringList> m_values;
    QList<Positional> m_positionalHelp;
    QStringList m_positional;
    QStringList m_seenNames;
    QStringList m_unknown;
    QString m_description;
    QString m_error;
    QString m_exeName;
    SingleDashWordOptionMode m_dashMode = ParseAsCompactedShortOptions;
    OptionsAfterPositionalArgumentsMode m_afterMode = ParseAsOptions;
    bool m_parsed = false;
    int m_helpOption = -1;
    int m_versionOption = -1;
};
