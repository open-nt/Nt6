// MIT License - see LICENSE
#include <QtCore/qcommandlineparser.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

/*
    Differences from Qt:
    - Messages are not translatable.
    - --help-all and Qt-specific options are not offered; the built-in help option is -h/--help.
    - Option names that start with '-' or contain '=' are dropped (Qt warns and drops them too).
    - Descriptions are wrapped at 79 columns regardless of the terminal width.
    - Error messages name a short option as "-o" and a long one as "--name"; unknown options are
      reported without dashes, as in Qt.
*/

namespace {

bool isValidName(const QString &name) {
    return !name.isEmpty() && !name.startsWith(QChar(u'-')) && !name.contains(QChar(u'='));
}

QStringList validNames(const QStringList &names) {
    QStringList r;
    for (const QString &n : names) {
        if (isValidName(n))
            r.append(n);
        else
            qWarning("QCommandLineOption: Option names cannot be empty, start with '-' or contain '=': \"%s\"",
                     n.toUtf8().constData());
    }
    return r;
}

QString utf8(const char *s) { return QString::fromUtf8(s); }

// "  names  description", with the description wrapped to 79 columns and aligned.
QString wrapText(const QString &names, qsizetype longest, const QString &description) {
    const qsizetype indent = 2 + longest + 2;
    const qsizetype width = std::max<qsizetype>(79 - indent, 20);
    QString out = QString(2, QChar(u' ')) + names.leftJustified(longest) + QString(2, QChar(u' '));
    QStringList lines;
    for (const QString &para : description.split(QChar(u'\n'))) {
        QString line;
        for (const QString &word : para.split(QChar(u' '), Qt::SkipEmptyParts)) {
            if (!line.isEmpty() && line.size() + 1 + word.size() > width) {
                lines.append(line);
                line.clear();
            }
            if (!line.isEmpty())
                line += QChar(u' ');
            line += word;
        }
        lines.append(line);
    }
    const QString pad(indent, QChar(u' '));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (i > 0)
            out += pad;
        out += lines.at(i);
        out += QChar(u'\n');
    }
    // No description: drop the column padding.
    while (out.endsWith(QString::fromLatin1(" \n")))
        out.replace(out.size() - 2, 2, QString::fromLatin1("\n"));
    return out;
}

} // namespace

/*!
    \class QCommandLineOption
    \brief The QCommandLineOption class defines a possible command-line option.

    An option has one or more names, a description for the help text and
    optionally a value. Add options to a QCommandLineParser, then query the
    parser after parsing:

    \code
    QCommandLineParser parser;
    QCommandLineOption output({"o", "output"}, "Write to <file>.", "file");
    parser.addOption(output);
    parser.process(QCoreApplication::arguments());
    QString path = parser.value(output);
    \endcode

    Names that are empty, start with \c - or contain \c = are not valid; they
    are dropped with a warning.

    \differences
    There is no \c swap() and no \c setHidden() or \c isHidden(); use the
    \c HiddenFromHelp flag instead.
    \enddifferences

    \sa QCommandLineParser
*/

/*!
    Constructs a command line option with the names \a names. Names that are
    not valid are dropped. The option takes no value.
*/
QCommandLineOption::QCommandLineOption(const QStringList &names) : m_names(validNames(names)) {}

/*!
    \overload
    Constructs a command line option with the names \a names and the help
    text \a description. If \a valueName is not empty the option takes a
    value with that name, and \a defaultValue is used when it is not given.
    Names that are not valid are dropped.
*/
QCommandLineOption::QCommandLineOption(const QStringList &names, const QString &description, const QString &valueName,
                                       const QString &defaultValue)
    : m_names(validNames(names)), m_valueName(valueName), m_description(description) {
    setDefaultValue(defaultValue);
}

/*!
    \class QCommandLineParser
    \brief The QCommandLineParser class provides a means for handling the command line options.

    Describe the options with QCommandLineOption, add them with addOption(),
    then call process() or parse() with the arguments. Afterwards use
    isSet(), value(), values() and positionalArguments() to read the result.

    \code
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Copies files");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("source", "File to copy.");
    QCommandLineOption force({"f", "force"}, "Overwrite existing files.");
    parser.addOption(force);

    parser.process(app);
    bool overwrite = parser.isSet(force);
    \endcode

    Supported forms are \c{--name}, \c{--name=value}, \c{--name value},
    \c{-n}, \c{-nvalue}, \c{-n value} and compacted short options such as
    \c{-abc}. A \c{--} argument ends option processing.

    \differences
    Messages are not translatable. \c --help-all and the Qt-specific
    options are not offered; the built-in help option is \c{-h/--help}.
    Option names that start with \c - or contain \c = are dropped. Help text
    descriptions are wrapped at 79 columns regardless of the terminal width.
    Error messages name a short option as \c{-o} and a long one as
    \c{--name}; unknown options are reported without dashes, as in Qt.
    \enddifferences

    \sa QCommandLineOption, QCoreApplication
*/

/*!
    Constructs a command line parser.
*/
QCommandLineParser::QCommandLineParser() = default;
/*!
    Destroys the command line parser.
*/
QCommandLineParser::~QCommandLineParser() = default;

/*!
    Adds \a option to the parser and returns \c true on success. Returns
    \c false, without adding it, if the option has no valid names or one of
    its names is already defined.

    \sa addOptions(), addHelpOption(), addVersionOption()
*/
bool QCommandLineParser::addOption(const QCommandLineOption &option) {
    const QStringList names = option.names();
    if (names.isEmpty())
        return false;
    for (const QString &n : names)
        if (m_nameToOption.contains(n))
            return false;
    const int index = static_cast<int>(m_options.size());
    m_options.append(option);
    for (const QString &n : names)
        m_nameToOption.insert(n, index);
    return true;
}

/*!
    Adds each option in \a options and returns \c true if all were added.
    Options that cannot be added are skipped.

    \sa addOption()
*/
bool QCommandLineParser::addOptions(const QList<QCommandLineOption> &options) {
    bool ok = true;
    for (const QCommandLineOption &o : options)
        ok = addOption(o) && ok;
    return ok;
}

/*!
    Adds the option \c{-v, --version} that displays the application name and
    version, set with QCoreApplication::setApplicationVersion(), and returns
    it. The option is acted on by process().

    \sa addHelpOption(), showVersion()
*/
QCommandLineOption QCommandLineParser::addVersionOption() {
    QCommandLineOption opt(QStringList{utf8("v"), utf8("version")}, utf8("Displays version information."));
    if (addOption(opt))
        m_versionOption = static_cast<int>(m_options.size()) - 1;
    return opt;
}

/*!
    Adds the option \c{-h, --help} that displays the help text and returns
    it. The option is acted on by process().

    \sa addVersionOption(), showHelp()
*/
QCommandLineOption QCommandLineParser::addHelpOption() {
    QCommandLineOption opt(QStringList{utf8("h"), utf8("help")}, utf8("Displays help on commandline options."));
    if (addOption(opt))
        m_helpOption = static_cast<int>(m_options.size()) - 1;
    return opt;
}

/*!
    Describes a positional argument called \a name with the help text
    \a description. The argument is shown in the usage line as \a syntax, or
    as \a name if \a syntax is empty. This only affects helpText(); positional
    arguments are not validated.

    \sa positionalArguments(), clearPositionalArguments()
*/
void QCommandLineParser::addPositionalArgument(const QString &name, const QString &description, const QString &syntax) {
    m_positionalHelp.append(Positional{name, description, syntax});
}

int QCommandLineParser::indexOf(const QString &name) const { return m_nameToOption.value(name, -1); }

void QCommandLineParser::record(int option, const QString &name, const QString *value) {
    m_seenNames.append(name);
    QStringList &list = m_values[option];
    if (value)
        list.append(*value);
}

/*!
    Parses \a arguments, whose first entry is the program name, and stores
    the result. Returns \c true on success; returns \c false if an option is
    unknown, a value is missing or unexpected, and sets errorText().

    Unknown options are listed by unknownOptionNames(). Anything after
    \c{--}, and arguments that are not options, are positional.

    \sa process(), errorText()
*/
bool QCommandLineParser::parse(const QStringList &arguments) {
    m_values.clear();
    m_positional.clear();
    m_seenNames.clear();
    m_unknown.clear();
    m_error.clear();
    m_parsed = true;
    m_exeName = arguments.isEmpty() ? QString() : arguments.first();

    bool optionsEnded = false;
    // Handles one option by name; `inlineValue` is non-null for --name=value forms.
    auto handle = [&](const QString &name, const QString &shown, const QString *inlineValue, qsizetype &i) {
        const int idx = indexOf(name);
        if (idx < 0) {
            m_unknown.append(name);
            return true;
        }
        const QCommandLineOption &opt = m_options.at(idx);
        if (opt.valueName().isEmpty()) {
            if (inlineValue) {
                m_error = QString::fromLatin1("Unexpected value after '%1'.").arg(shown);
                return false;
            }
            record(idx, name, nullptr);
            return true;
        }
        QString value;
        if (inlineValue) {
            value = *inlineValue;
        } else if (i + 1 < arguments.size()) {
            value = arguments.at(++i);
        } else {
            m_error = QString::fromLatin1("Missing value after '%1'.").arg(shown);
            return false;
        }
        record(idx, name, &value);
        return true;
    };

    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const QString &arg = arguments.at(i);
        if (optionsEnded) {
            m_positional.append(arg);
        } else if (arg == QString::fromLatin1("--")) {
            optionsEnded = true;
        } else if (arg.size() > 2 && arg.startsWith(QString::fromLatin1("--"))) {
            const QString body = arg.mid(2);
            const qsizetype eq = body.indexOf(QChar(u'='));
            const QString name = eq < 0 ? body : body.left(eq);
            const QString value = eq < 0 ? QString() : body.mid(eq + 1);
            if (!handle(name, QString::fromLatin1("--") + name, eq < 0 ? nullptr : &value, i))
                return false;
        } else if (arg.size() > 1 && arg.at(0) == u'-') {
            const QString body = arg.mid(1);
            if (m_dashMode == ParseAsLongOptions) {
                const qsizetype eq = body.indexOf(QChar(u'='));
                const QString name = eq < 0 ? body : body.left(eq);
                const QString value = eq < 0 ? QString() : body.mid(eq + 1);
                if (!handle(name, QString(QChar(u'-')) + name, eq < 0 ? nullptr : &value, i))
                    return false;
                continue;
            }
            for (qsizetype pos = 0; pos < body.size(); ++pos) {
                const QString name = body.mid(pos, 1);
                const int idx = indexOf(name);
                if (idx < 0) {
                    m_unknown.append(name);
                    break;
                }
                if (m_options.at(idx).valueName().isEmpty()) {
                    record(idx, name, nullptr);
                    continue;
                }
                // The rest of the word (after an optional '=') is the value; otherwise the next argument.
                QString rest = body.mid(pos + 1);
                if (rest.startsWith(QChar(u'=')))
                    rest.remove(0, 1);
                const bool inlineValue = pos + 1 < body.size();
                if (!handle(name, QString(QChar(u'-')) + name, inlineValue ? &rest : nullptr, i))
                    return false;
                break;
            }
        } else {
            m_positional.append(arg);
            if (m_afterMode == ParseAsPositionalArguments)
                optionsEnded = true;
        }
    }

    if (!m_unknown.isEmpty()) {
        m_error = m_unknown.size() == 1
            ? QString::fromLatin1("Unknown option '%1'.").arg(m_unknown.first())
            : QString::fromLatin1("Unknown options: %1.").arg(m_unknown.join(QString::fromLatin1(", ")));
        return false;
    }
    return true;
}

/*!
    Parses \a arguments, whose first entry is the program name, and handles
    the common cases. On a parse error it prints the application name and
    errorText() to standard error and exits the process with a failure
    status. If the version or help option added with addVersionOption() or
    addHelpOption() was given, it prints that text and exits successfully.

    \sa parse(), showHelp(), showVersion()
*/
void QCommandLineParser::process(const QStringList &arguments) {
    if (!parse(arguments)) {
        std::fprintf(stderr, "%s: %s\n", QCoreApplication::applicationName().toUtf8().constData(),
                     m_error.toUtf8().constData());
        std::fflush(stderr);
        std::exit(EXIT_FAILURE);
    }
    if (m_versionOption >= 0 && m_values.contains(m_versionOption))
        showVersion();
    if (m_helpOption >= 0 && m_values.contains(m_helpOption))
        showHelp(0);
}

/*!
    \overload
    Processes the command line of \a app, as returned by
    QCoreApplication::arguments().
*/
void QCommandLineParser::process(const QCoreApplication &app) {
    Q_UNUSED(app)
    process(QCoreApplication::arguments());
}

/*!
    Returns \c true if the option called \a name was given on the command
    line; otherwise returns \c false. Prints a warning and returns \c false
    if no option has that name.

    \sa value(), values()
*/
bool QCommandLineParser::isSet(const QString &name) const {
    const int idx = indexOf(name);
    if (idx < 0) {
        qWarning("QCommandLineParser: option not defined: \"%s\"", name.toUtf8().constData());
        return false;
    }
    return m_values.contains(idx);
}

/*!
    Returns every value given for the option called \a name, in command-line
    order, or the option's default values if it was not given. Prints a
    warning and returns an empty list if no option has that name.

    \sa value(), isSet()
*/
QStringList QCommandLineParser::values(const QString &name) const {
    const int idx = indexOf(name);
    if (idx < 0) {
        qWarning("QCommandLineParser: option not defined: \"%s\"", name.toUtf8().constData());
        return QStringList();
    }
    const QStringList given = m_values.value(idx);
    return given.isEmpty() ? m_options.at(idx).defaultValues() : given;
}

/*!
    Returns the last value given for the option called \a name, or the last
    of its default values if it was not given. Returns an empty string if
    there is none.

    \sa values(), isSet()
*/
QString QCommandLineParser::value(const QString &name) const {
    const QStringList v = values(name);
    return v.isEmpty() ? QString() : v.last();
}

/*!
    Returns the help text: a usage line, the application description, the
    options that are not hidden and the positional arguments added with
    addPositionalArgument(). Descriptions are wrapped at 79 columns.

    \sa showHelp(), QCommandLineOption::HiddenFromHelp
*/
QString QCommandLineParser::helpText() const {
    QString exe = m_exeName;
    if (exe.isEmpty()) {
        const QStringList args = QCoreApplication::arguments();
        exe = args.isEmpty() ? QCoreApplication::applicationName() : args.first();
    }
    const QChar nl(u'\n');
    QString usage = QString::fromLatin1("Usage: ") + exe;
    if (!m_options.isEmpty())
        usage += QString::fromLatin1(" [options]");
    for (const Positional &p : m_positionalHelp)
        usage += QChar(u' ') + (p.syntax.isEmpty() ? p.name : p.syntax);

    QString text = usage + nl;
    if (!m_description.isEmpty())
        text += m_description + nl;
    text += nl;

    if (!m_options.isEmpty()) {
        text += QString::fromLatin1("Options:") + nl;
        QStringList nameColumn;
        QStringList descColumn;
        qsizetype longest = 0;
        for (const QCommandLineOption &opt : m_options) {
            if (opt.flags().testFlag(QCommandLineOption::HiddenFromHelp))
                continue;
            QStringList shown;
            for (const QString &n : opt.names()) {
                const bool single = n.size() == 1 || opt.flags().testFlag(QCommandLineOption::ShortOptionStyle);
                shown.append((single ? QString(QChar(u'-')) : QString::fromLatin1("--")) + n);
            }
            QString label = shown.join(QString::fromLatin1(", "));
            if (!opt.valueName().isEmpty())
                label += QString::fromLatin1(" <") + opt.valueName() + QChar(u'>');
            longest = std::max(longest, label.size());
            nameColumn.append(label);
            descColumn.append(opt.description());
        }
        for (qsizetype i = 0; i < nameColumn.size(); ++i)
            text += wrapText(nameColumn.at(i), longest, descColumn.at(i));
    }

    if (!m_positionalHelp.isEmpty()) {
        if (!m_options.isEmpty())
            text += nl;
        text += QString::fromLatin1("Arguments:") + nl;
        qsizetype longest = 0;
        for (const Positional &p : m_positionalHelp)
            longest = std::max(longest, p.name.size());
        for (const Positional &p : m_positionalHelp)
            text += wrapText(p.name, longest, p.description);
    }
    return text;
}

/*!
    \since 6.9
    Writes \a message, followed by a line feed if it has none, to standard output for an \c Information message
    or to standard error for an \c Error as \a type says, and exits the process with \a exitCode. Programs use it to
    report what is wrong with their arguments in the way that showHelp() and showVersion() do.

    \code
    if (parser.positionalArguments().isEmpty())
        QCommandLineParser::showMessageAndExit(QCommandLineParser::MessageType::Error,
                                               QStringLiteral("No file given."), EXIT_FAILURE);
    \endcode

    \sa showHelp()
*/
void QCommandLineParser::showMessageAndExit(MessageType type, const QString &message, int exitCode) {
    QString text = message;
    if (!text.endsWith(QChar(u'\n')))
        text.append(QChar(u'\n'));
    std::FILE *out = type == MessageType::Information ? stdout : stderr;
    std::fputs(text.toUtf8().constData(), out);
    std::fflush(out);
    std::exit(exitCode);
}

/*!
    Prints the application name and version to standard output and exits
    the process with a success status. This function does not return.

    \sa addVersionOption(), QCoreApplication::setApplicationVersion()
*/
void QCommandLineParser::showVersion() {
    const QString line = QCoreApplication::applicationName() + QChar(u' ') + QCoreApplication::applicationVersion() + QChar(u'\n');
    std::fputs(line.toUtf8().constData(), stdout);
    std::fflush(stdout);
    std::exit(EXIT_SUCCESS);
}

/*!
    Prints helpText() and exits the process with \a exitCode. The text goes to
    standard output if \a exitCode is 0, otherwise to standard error. This
    function does not return.

    \sa helpText(), addHelpOption()
*/
void QCommandLineParser::showHelp(int exitCode) {
    std::fputs(helpText().toUtf8().constData(), exitCode == 0 ? stdout : stderr);
    std::fflush(exitCode == 0 ? stdout : stderr);
    std::exit(exitCode);
}
