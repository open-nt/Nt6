// MIT License - see LICENSE
#pragma once

#include <QtCore/qcommandlineoption.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>

class QCoreApplication;

class QCommandLineParser {
public:
    enum SingleDashWordOptionMode { ParseAsCompactedShortOptions, ParseAsLongOptions };
    enum OptionsAfterPositionalArgumentsMode { ParseAsOptions, ParseAsPositionalArguments };

    QCommandLineParser();
    ~QCommandLineParser();
    QCommandLineParser(const QCommandLineParser &) = delete;
    QCommandLineParser &operator=(const QCommandLineParser &) = delete;

    void setSingleDashWordOptionMode(SingleDashWordOptionMode mode) { m_dashMode = mode; }
    void setOptionsAfterPositionalArgumentsMode(OptionsAfterPositionalArgumentsMode mode) { m_afterMode = mode; }
    bool addOption(const QCommandLineOption &option);
    bool addOptions(const QList<QCommandLineOption> &options);
    QCommandLineOption addVersionOption();
    QCommandLineOption addHelpOption();
    void setApplicationDescription(const QString &description) { m_description = description; }
    QString applicationDescription() const { return m_description; }
    void addPositionalArgument(const QString &name, const QString &description, const QString &syntax = QString());
    void clearPositionalArguments() { m_positionalHelp.clear(); }

    // Exits the process on --help, --version or a parse error.
    void process(const QStringList &arguments);
    void process(const QCoreApplication &app);
    bool parse(const QStringList &arguments);
    QString errorText() const { return m_error; }

    bool isSet(const QString &name) const;
    QString value(const QString &name) const;
    QStringList values(const QString &name) const;
    bool isSet(const QCommandLineOption &option) const { return isSet(option.names().first()); }
    QString value(const QCommandLineOption &option) const { return value(option.names().first()); }
    QStringList values(const QCommandLineOption &option) const { return values(option.names().first()); }
    QStringList positionalArguments() const { return m_positional; }
    QStringList optionNames() const { return m_seenNames; }
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
    SingleDashWordOptionMode m_dashMode = ParseAsCompactedShortOptions;
    OptionsAfterPositionalArgumentsMode m_afterMode = ParseAsOptions;
    bool m_parsed = false;
    int m_helpOption = -1;
    int m_versionOption = -1;
};
