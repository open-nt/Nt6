// MIT License - see LICENSE
#include <QtCore/qloggingcategory.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace {

// A rule of the form "category[.type] = true|false". The category may start and end with '*'.
struct Rule {
    enum Match { All, Full, Prefix, Suffix, Contains };
    Match match = Full;
    std::string key;
    int type = -1; // QtMsgType, or -1 for every type
    bool enabled = false;

    bool matches(const std::string &name, int msgType) const {
        if (type != -1 && type != msgType)
            return false;
        switch (match) {
        case All: return true;
        case Full: return name == key;
        case Prefix: return name.compare(0, key.size(), key) == 0;
        case Suffix: return name.size() >= key.size() && name.compare(name.size() - key.size(), key.size(), key) == 0;
        case Contains: return name.find(key) != std::string::npos;
        }
        return false;
    }
};

std::string trim(const std::string &s) {
    const char *space = " \t\r\n";
    const std::size_t a = s.find_first_not_of(space);
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(space) - a + 1);
}

bool lower(const std::string &s, const char *word) {
    if (s.size() != std::strlen(word))
        return false;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) != word[i])
            return false;
    return true;
}

// Parses one "pattern = value" line into rule; returns false for anything malformed.
bool parseRule(const std::string &line, Rule &rule) {
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos)
        return false;
    std::string key = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));
    if (key.empty())
        return false;
    rule.enabled = lower(value, "true");

    static const struct {
        const char *suffix;
        QtMsgType type;
    } kTypes[] = {{".debug", QtDebugMsg}, {".info", QtInfoMsg}, {".warning", QtWarningMsg}, {".critical", QtCriticalMsg}};
    rule.type = -1;
    for (const auto &t : kTypes) {
        const std::size_t n = std::strlen(t.suffix);
        if (key.size() > n && key.compare(key.size() - n, n, t.suffix) == 0) {
            rule.type = t.type;
            key.erase(key.size() - n);
            break;
        }
    }

    const bool front = key.front() == '*';
    const bool back = key.size() > 1 && key.back() == '*';
    if (key == "*") {
        rule.match = Rule::All;
        rule.key.clear();
        return true;
    }
    if (front)
        key.erase(0, 1);
    if (back)
        key.pop_back();
    if (key.empty() || key.find('*') != std::string::npos)
        return false;
    rule.key = key;
    rule.match = front && back ? Rule::Contains : front ? Rule::Suffix : back ? Rule::Prefix : Rule::Full;
    return true;
}

std::vector<Rule> parseRules(const std::string &text) {
    std::vector<Rule> rules;
    std::string line;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n' || text[i] == ';') {
            const std::string t = trim(line);
            Rule rule;
            if (!t.empty() && t[0] != '#' && parseRule(t, rule))
                rules.push_back(rule);
            line.clear();
        } else {
            line.push_back(text[i]);
        }
    }
    return rules;
}

// The rules of a QT_LOGGING_CONF file: the lines under [Rules], or all lines if it has no sections.
std::vector<Rule> loadConfigFile(const char *path) {
    std::ifstream in(path);
    if (!in)
        return {};
    std::string line;
    std::string rules;
    bool hasSections = false;
    bool inRules = false;
    std::string all;
    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (!t.empty() && t.front() == '[') {
            hasSections = true;
            inRules = t.size() >= 7 && lower(t.substr(0, 7), "[rules]");
            continue;
        }
        all += t + "\n";
        if (inRules)
            rules += t + "\n";
    }
    return parseRules(hasSections ? rules : all);
}

} // namespace

// Keeps every category and the rules in force. It is never destroyed, so that categories
// held in static objects can still unregister while the program exits.
class QLoggingRegistry {
public:
    static QLoggingRegistry &instance() {
        alignas(QLoggingRegistry) static unsigned char storage[sizeof(QLoggingRegistry)];
        static QLoggingRegistry *registry = new (storage) QLoggingRegistry;
        return *registry;
    }

    void add(QLoggingCategory *category) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_categories.push_back(category);
        apply(category);
    }

    void remove(QLoggingCategory *category) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_categories.erase(std::remove(m_categories.begin(), m_categories.end(), category), m_categories.end());
    }

    void setApiRules(std::vector<Rule> rules) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_api = std::move(rules);
        applyAll();
    }

    QLoggingCategory::CategoryFilter installFilter(QLoggingCategory::CategoryFilter filter) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        const QLoggingCategory::CategoryFilter old = m_filter;
        m_filter = filter;
        applyAll();
        return old;
    }

private:
    QLoggingRegistry() {
        if (const char *conf = std::getenv("QT_LOGGING_CONF"))
            m_config = loadConfigFile(conf);
        if (const char *env = std::getenv("QT_LOGGING_RULES"))
            m_environment = parseRules(env);
    }

    // The order is: the severity level, the configuration file, rules set in the program,
    // the environment (so that it can override the program), and last the filter.
    void apply(QLoggingCategory *category) const {
        bool enabled[4];
        for (int i = 0; i < 4; ++i)
            enabled[i] = true;
        switch (category->m_severity) {
        case QtInfoMsg: enabled[QLoggingCategory::kDebug] = false; break;
        case QtWarningMsg: enabled[QLoggingCategory::kDebug] = enabled[QLoggingCategory::kInfo] = false; break;
        case QtCriticalMsg:
        case QtFatalMsg:
            enabled[QLoggingCategory::kDebug] = enabled[QLoggingCategory::kInfo] = enabled[QLoggingCategory::kWarning] = false;
            break;
        default: break;
        }
        static const QtMsgType kMsgTypes[4] = {QtDebugMsg, QtInfoMsg, QtWarningMsg, QtCriticalMsg};
        for (const std::vector<Rule> *set : {&m_config, &m_api, &m_environment})
            for (const Rule &rule : *set)
                for (int i = 0; i < 4; ++i)
                    if (rule.matches(category->m_name, kMsgTypes[i]))
                        enabled[i] = rule.enabled;
        for (int i = 0; i < 4; ++i)
            category->m_enabled[i].store(enabled[i]);
        if (m_filter)
            m_filter(category);
    }

    void applyAll() const {
        for (QLoggingCategory *category : m_categories)
            apply(category);
    }

    mutable std::mutex m_mutex;
    std::vector<QLoggingCategory *> m_categories;
    std::vector<Rule> m_config;
    std::vector<Rule> m_api;
    std::vector<Rule> m_environment;
    QLoggingCategory::CategoryFilter m_filter = nullptr;
};

/*!
    \class QLoggingCategory
    \brief The QLoggingCategory class represents a category, or 'area', in the logging infrastructure.

    A category names a part of a program, such as \c{myapp.network}, so that
    its messages can be turned on and off independently. Declare one in a
    header, define it in a source file, and log with the \c qCDebug(),
    \c qCInfo(), \c qCWarning(), \c qCCritical() and \c qCFatal() macros:

    \code
    // network.h
    Q_DECLARE_LOGGING_CATEGORY(lcNetwork)

    // network.cpp
    Q_LOGGING_CATEGORY(lcNetwork, "myapp.network")

    qCDebug(lcNetwork) << "connecting to" << host;
    qCWarning(lcNetwork, "timeout after %d ms", ms);
    \endcode

    The arguments of a macro are not evaluated when the category does not
    enable that type of message.

    \section1 Filter Rules

    Which types of message a category enables is set by rules of the form
    \c{category[.type] = true|false}, where \c type is \c debug, \c info,
    \c warning or \c critical, and \c category may start and end with
    \c{*}. A rule without a type applies to all four types. Rules are separated
    by newlines or semicolons, and a later rule that matches overrides an
    earlier one. For example, \c{"*.debug=false;myapp.network.debug=true"}
    turns debug messages off, except those of \c{myapp.network}.

    Rules come from these places, from the lowest precedence to the highest:

    \list 1
    \li The \c QT_LOGGING_CONF file, which has them under a \c{[Rules]} heading.
    \li setFilterRules().
    \li The \c QT_LOGGING_RULES environment variable.
    \li The filter installed with installFilter(), which has the last word.
    \endlist

    By default all types are enabled. The \c{"default"} category, which
    holds the messages written with \c qDebug() and its relatives, follows
    the same rules.

    \differences
    The configuration files that Qt looks for in standard locations
    (\c{qtlogging.ini}) are not read; only \c QT_LOGGING_CONF is. The
    \c QT_LOGGING_TO_CONSOLE variable is ignored. Qt's own \c{qt.*} categories
    do not exist. There is no category for a \c QObject.
    \enddifferences

    \sa QMessageLogger, QDebug
*/

QLoggingCategory::QLoggingCategory(const char *category, QtMsgType severityLevel)
    : m_name(category ? category : "default"), m_severity(severityLevel), m_enabled{} {
    QLoggingRegistry::instance().add(this);
}

/*!
    Destroys the category.
*/
QLoggingCategory::~QLoggingCategory() { QLoggingRegistry::instance().remove(this); }

/*!
    Returns \c true if messages of the given \a type are enabled for this
    category. Fatal messages are always enabled.

    \sa setEnabled()
*/
bool QLoggingCategory::isEnabled(QtMsgType type) const {
    switch (type) {
    case QtDebugMsg: return isDebugEnabled();
    case QtInfoMsg: return isInfoEnabled();
    case QtWarningMsg: return isWarningEnabled();
    case QtCriticalMsg: return isCriticalEnabled();
    case QtFatalMsg: return true;
    }
    return true;
}

/*!
    Changes whether messages of the given \a type are enabled to \a enable.
    Fatal messages cannot be disabled. A rule or filter applied later may
    override the change.

    \sa isEnabled()
*/
void QLoggingCategory::setEnabled(QtMsgType type, bool enable) {
    switch (type) {
    case QtDebugMsg: m_enabled[kDebug].store(enable); break;
    case QtInfoMsg: m_enabled[kInfo].store(enable); break;
    case QtWarningMsg: m_enabled[kWarning].store(enable); break;
    case QtCriticalMsg: m_enabled[kCritical].store(enable); break;
    case QtFatalMsg: break;
    }
}

/*!
    Returns the category of messages written without one, named
    \c{"default"}. The \c qDebug(), \c qInfo(), \c qWarning() and
    \c qCritical() macros are filtered by it.
*/
QLoggingCategory *QLoggingCategory::defaultCategory() {
    // Never destroyed, because messages may be logged while the program exits.
    alignas(QLoggingCategory) static unsigned char storage[sizeof(QLoggingCategory)];
    static QLoggingCategory *category = new (storage) QLoggingCategory("default");
    return category;
}

/*!
    Installs a \a filter function that is called for every category, once
    its rules have been applied, and again whenever the rules change. The
    filter can enable or disable types of messages with setEnabled(). Returns
    the previously installed filter, or \c nullptr if there was none. Pass
    \c nullptr to remove the filter.

    The filter is called with an internal lock held, so it must not create
    categories or change rules.

    \sa setFilterRules()
*/
QLoggingCategory::CategoryFilter QLoggingCategory::installFilter(CategoryFilter filter) {
    return QLoggingRegistry::instance().installFilter(filter);
}

/*!
    Sets the filter rules to \a rules, replacing those set by an earlier call.
    The format is described in the class documentation. Rules in the
    \c QT_LOGGING_RULES environment variable take precedence.
*/
void QLoggingCategory::setFilterRules(const QString &rules) {
    QLoggingRegistry::instance().setApiRules(parseRules(rules.toStdString()));
}
