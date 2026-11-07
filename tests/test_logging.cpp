// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qbuffer.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qtemporaryfile.h>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

int noOutputCompiledOut(int *evaluated);

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

struct Record {
    QtMsgType type;
    std::string file;
    int line;
    std::string function;
    std::string category;
    std::string text;
};

static std::vector<Record> g_records;

static void collect(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    g_records.push_back({type, context.file ? context.file : "", context.line, context.function ? context.function : "",
                         context.category ? context.category : "", message.toStdString()});
}

// Runs a body with the collecting handler installed and returns what it logged.
template <typename F>
static std::vector<Record> capture(F body) {
    g_records.clear();
    const QtMessageHandler old = qInstallMessageHandler(collect);
    body();
    qInstallMessageHandler(old);
    return g_records;
}

Q_DECLARE_LOGGING_CATEGORY(lcTest)
Q_DECLARE_LOGGING_CATEGORY(lcWarnOnly)
Q_DECLARE_LOGGING_CATEGORY(lcCritOnly)
Q_DECLARE_LOGGING_CATEGORY(lcOther)
Q_LOGGING_CATEGORY(lcTest, "test.cat")
Q_LOGGING_CATEGORY(lcWarnOnly, "test.warnonly", QtWarningMsg)
Q_LOGGING_CATEGORY(lcCritOnly, "test.critonly", QtCriticalMsg)
Q_LOGGING_CATEGORY(lcOther, "test.other")

static int g_evaluated = 0;
static int evaluate() { return ++g_evaluated; }

static void testMacroContext() {
    const int line = __LINE__ + 1;
    auto records = capture([] { qDebug("hello %d", 7); });
    CHECK(records.size() == 1);
    if (records.size() != 1)
        return;
    CHECK(records[0].type == QtDebugMsg);
    CHECK(records[0].text == "hello 7");
    CHECK(records[0].line == line);
    CHECK(records[0].file.find("test_logging.cpp") != std::string::npos);
    CHECK(records[0].function.find("testMacroContext") != std::string::npos);
    CHECK(records[0].category == "default");

    records = capture([] {
        qDebug() << "a" << 1;
        qInfo() << "b";
        qWarning() << "c";
        qCritical() << "d";
        qInfo("e %s", "x");
        qWarning("f");
        qCritical("g");
    });
    CHECK(records.size() == 7);
    if (records.size() == 7) {
        CHECK(records[0].type == QtDebugMsg && records[0].text == "a 1");
        CHECK(records[1].type == QtInfoMsg && records[1].text == "b");
        CHECK(records[2].type == QtWarningMsg && records[2].text == "c");
        CHECK(records[3].type == QtCriticalMsg && records[3].text == "d");
        CHECK(records[4].type == QtInfoMsg && records[4].text == "e x");
        CHECK(records[5].type == QtWarningMsg && records[5].text == "f");
        CHECK(records[6].type == QtCriticalMsg && records[6].text == "g");
    }
    // The statement can be an unbraced branch of an if.
    records = capture([] {
        if (g_evaluated >= 0)
            qDebug() << "yes";
        else
            qDebug() << "no";
    });
    CHECK(records.size() == 1 && records[0].text == "yes");
}

static void testLongMessages() {
    const std::string big(5000, 'x');
    auto records = capture([&] { qDebug("%s", big.c_str()); });
    CHECK(records.size() == 1 && records[0].text == big);
}

static void testCategoryBasics() {
    CHECK(std::strcmp(lcTest().categoryName(), "test.cat") == 0);
    CHECK(&lcTest() == &lcTest());
    CHECK(lcTest().isDebugEnabled() && lcTest().isInfoEnabled() && lcTest().isWarningEnabled() && lcTest().isCriticalEnabled());
    CHECK(lcTest().isEnabled(QtDebugMsg) && lcTest().isEnabled(QtFatalMsg));
    // The severity level decides what is on to begin with.
    CHECK(!lcWarnOnly().isDebugEnabled() && !lcWarnOnly().isInfoEnabled());
    CHECK(lcWarnOnly().isWarningEnabled() && lcWarnOnly().isCriticalEnabled());
    CHECK(!lcCritOnly().isWarningEnabled() && lcCritOnly().isCriticalEnabled());
    QLoggingCategory info("test.info", QtInfoMsg);
    CHECK(!info.isDebugEnabled() && info.isInfoEnabled());
    // A category works wherever a category function does.
    CHECK(&info() == &info);
    const QLoggingCategory *def = QLoggingCategory::defaultCategory();
    CHECK(def && std::strcmp(def->categoryName(), "default") == 0);
    CHECK(QLoggingCategory::defaultCategory() == def);
}

static void testCategoryMacros() {
    auto records = capture([] {
        qCDebug(lcTest) << "stream" << 1;
        qCInfo(lcTest, "printf %d", 2);
        qCWarning(lcTest) << "w";
        qCCritical(lcTest, "c");
    });
    CHECK(records.size() == 4);
    if (records.size() == 4) {
        CHECK(records[0].category == "test.cat" && records[0].text == "stream 1" && records[0].type == QtDebugMsg);
        CHECK(records[1].category == "test.cat" && records[1].text == "printf 2" && records[1].type == QtInfoMsg);
        CHECK(records[2].category == "test.cat" && records[2].type == QtWarningMsg);
        CHECK(records[3].category == "test.cat" && records[3].type == QtCriticalMsg);
        CHECK(records[0].file.find("test_logging.cpp") != std::string::npos && records[0].line > 0);
    }
    // A category object, not only a function, can be the argument.
    QLoggingCategory local("test.local");
    records = capture([&] { qCDebug(local) << "obj"; });
    CHECK(records.size() == 1 && records[0].category == "test.local");

    // A disabled message does not evaluate its operands.
    g_evaluated = 0;
    records = capture([] {
        qCDebug(lcWarnOnly) << evaluate();
        qCInfo(lcWarnOnly, "%d", evaluate());
        qCWarning(lcWarnOnly) << "shown";
    });
    CHECK(g_evaluated == 0);
    CHECK(records.size() == 1 && records[0].text == "shown");

    // Directly through the logger, a disabled category still writes nothing.
    records = capture([] {
        QMessageLogger().debug(lcWarnOnly(), "no");
        QMessageLogger().debug(lcWarnOnly) << "no";
        QMessageLogger("f.cpp", 12, "fn", "x.y").warning(lcTest) << "yes";
    });
    CHECK(records.size() == 1);
    if (records.size() == 1)
        CHECK(records[0].file == "f.cpp" && records[0].line == 12 && records[0].function == "fn" && records[0].category == "test.cat");
}

static void testRules() {
    QLoggingCategory::setFilterRules("test.cat.debug=false");
    CHECK(!lcTest().isDebugEnabled() && lcTest().isInfoEnabled() && lcTest().isWarningEnabled());
    CHECK(lcOther().isDebugEnabled());

    // Rules replace the earlier ones, and categories created later see them too.
    QLoggingCategory::setFilterRules("test.cat=false");
    CHECK(!lcTest().isDebugEnabled() && !lcTest().isInfoEnabled() && !lcTest().isWarningEnabled() && !lcTest().isCriticalEnabled());
    CHECK(lcTest().isEnabled(QtFatalMsg));
    {
        QLoggingCategory late("test.cat");
        CHECK(!late.isDebugEnabled() && !late.isCriticalEnabled());
    }

    // Wildcards.
    QLoggingCategory::setFilterRules("*.debug=false");
    CHECK(!lcTest().isDebugEnabled() && !lcOther().isDebugEnabled() && lcOther().isInfoEnabled());
    QLoggingCategory::setFilterRules("test.*=false");
    CHECK(!lcTest().isWarningEnabled() && !lcOther().isWarningEnabled());
    CHECK(QLoggingCategory::defaultCategory()->isWarningEnabled());
    QLoggingCategory::setFilterRules("*.other.warning=false");
    CHECK(!lcOther().isWarningEnabled() && lcTest().isWarningEnabled());
    QLoggingCategory::setFilterRules("*st*=false");
    CHECK(!lcTest().isInfoEnabled() && !lcOther().isInfoEnabled());
    QLoggingCategory::setFilterRules("*");  // no '=' so ignored
    CHECK(lcTest().isInfoEnabled());
    QLoggingCategory::setFilterRules("*=false");
    CHECK(!lcTest().isCriticalEnabled() && !QLoggingCategory::defaultCategory()->isCriticalEnabled());

    // A later rule wins; rules are separated by ';' or newlines, spaces and case do not matter.
    QLoggingCategory::setFilterRules("*=false ; test.cat.debug = TRUE\n# a comment\ntest.cat.info=true");
    CHECK(lcTest().isDebugEnabled() && lcTest().isInfoEnabled() && !lcTest().isWarningEnabled());
    QLoggingCategory::setFilterRules("test.cat.debug=true;test.cat.debug=false");
    CHECK(!lcTest().isDebugEnabled());

    // Malformed rules are ignored.
    QLoggingCategory::setFilterRules("a*b=false;=true;novalue;test.other.debug=false");
    CHECK(lcTest().isDebugEnabled() && !lcOther().isDebugEnabled());

    // Rules take the severity level into account: rules win over it.
    QLoggingCategory::setFilterRules("test.warnonly.debug=true");
    CHECK(lcWarnOnly().isDebugEnabled() && !lcWarnOnly().isInfoEnabled());

    QLoggingCategory::setFilterRules(QString());
    CHECK(lcTest().isDebugEnabled() && lcOther().isDebugEnabled() && !lcWarnOnly().isDebugEnabled());

    // Rules change what is written.
    QLoggingCategory::setFilterRules("test.cat.debug=false");
    auto records = capture([] {
        qCDebug(lcTest) << "off";
        qCWarning(lcTest) << "on";
    });
    CHECK(records.size() == 1 && records[0].text == "on");
    QLoggingCategory::setFilterRules(QString());
}

static void testSetEnabledAndFilter() {
    // Q_LOGGING_CATEGORY hands out a const reference; setEnabled() is non-const.
    QLoggingCategory &cat = const_cast<QLoggingCategory &>(lcTest());
    cat.setEnabled(QtDebugMsg, false);
    CHECK(!lcTest().isDebugEnabled());
    cat.setEnabled(QtDebugMsg, true);
    CHECK(lcTest().isDebugEnabled());
    cat.setEnabled(QtFatalMsg, false);
    CHECK(lcTest().isEnabled(QtFatalMsg));

    static int calls = 0;
    const QLoggingCategory::CategoryFilter old = QLoggingCategory::installFilter(+[](QLoggingCategory *category) {
        ++calls;
        if (std::strcmp(category->categoryName(), "test.other") == 0)
            category->setEnabled(QtWarningMsg, false);
    });
    CHECK(old == nullptr);
    CHECK(calls > 0);
    CHECK(!lcOther().isWarningEnabled() && lcTest().isWarningEnabled());
    const int before = calls;
    QLoggingCategory fresh("test.fresh");
    CHECK(calls == before + 1);
    // The filter has the last word, after rules.
    QLoggingCategory::setFilterRules("test.other.warning=true");
    CHECK(!lcOther().isWarningEnabled());
    CHECK(QLoggingCategory::installFilter(nullptr) != nullptr);
    CHECK(lcOther().isWarningEnabled());
    QLoggingCategory::setFilterRules(QString());
}

static void testDefaultCategory() {
    QLoggingCategory *def = QLoggingCategory::defaultCategory();
    def->setEnabled(QtDebugMsg, false);
    auto records = capture([] {
        qDebug() << "dropped";
        qDebug("dropped");
        qInfo() << "kept";
    });
    CHECK(records.size() == 1 && records[0].text == "kept");
    def->setEnabled(QtDebugMsg, true);
    // Categorised messages are not affected by it.
    def->setEnabled(QtWarningMsg, false);
    records = capture([] {
        qCWarning(lcTest) << "kept";
        qWarning() << "dropped";
    });
    CHECK(records.size() == 1 && records[0].text == "kept");
    def->setEnabled(QtWarningMsg, true);
    // Rules reach it as well.
    QLoggingCategory::setFilterRules("default.info=false");
    records = capture([] { qInfo() << "dropped"; });
    CHECK(records.empty());
    QLoggingCategory::setFilterRules(QString());
    records = capture([] { qInfo() << "back"; });
    CHECK(records.size() == 1);
}

static void testHandler() {
    CHECK(qInstallMessageHandler(nullptr) == nullptr);
    CHECK(qInstallMessageHandler(collect) == nullptr);
    CHECK(qInstallMessageHandler(nullptr) == collect);

    // A message logged from inside the handler does not recurse into it.
    static int depth = 0;
    static int calls = 0;
    qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext &, const QString &) {
        ++calls;
        ++depth;
        qCDebug(lcTest) << "inside the handler";
        --depth;
    });
    // The nested message goes to the default handler, which writes to stderr.
    int saved = dup(2);
    int devnull = open("/dev/null", 1);
    dup2(devnull, 2);
    qDebug() << "outer";
    dup2(saved, 2);
    close(saved);
    close(devnull);
    qInstallMessageHandler(nullptr);
    CHECK(calls == 1 && depth == 0);

    // Messages from several threads all arrive.
    static std::atomic<int> count{0};
    qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext &, const QString &) { count.fetch_add(1); });
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([] {
            for (int i = 0; i < 200; ++i)
                qCInfo(lcTest) << "message" << i;
        });
    for (std::thread &t : threads)
        t.join();
    qInstallMessageHandler(nullptr);
    CHECK(count == 800);
}

static QString format(QtMsgType type, const char *category, const char *text, const char *file = "f.cpp", int line = 9,
                      const char *function = "void ns::Klass::method(int) const") {
    const QMessageLogContext context(file, line, function, category);
    return qFormatLogMessage(type, context, QString::fromUtf8(text));
}

static void testPattern() {
    // The default pattern leaves out the default category.
    qSetMessagePattern(QString());
    CHECK(format(QtDebugMsg, "default", "m") == "m");
    CHECK(format(QtDebugMsg, "net", "m") == "net: m");

    qSetMessagePattern("[%{type}] %{message}");
    CHECK(format(QtDebugMsg, "x", "a") == "[debug] a");
    CHECK(format(QtInfoMsg, "x", "a") == "[info] a");
    CHECK(format(QtWarningMsg, "x", "a") == "[warning] a");
    CHECK(format(QtCriticalMsg, "x", "a") == "[critical] a");
    CHECK(format(QtFatalMsg, "x", "a") == "[fatal] a");

    qSetMessagePattern("%{file}:%{line} %{function} %{category}|%{message}");
    CHECK(format(QtDebugMsg, "c", "m") == "f.cpp:9 ns::Klass::method c|m");
    CHECK(format(QtDebugMsg, "c", "m", nullptr, 0, nullptr) == "unknown:0 unknown c|m");
    CHECK(format(QtDebugMsg, "c", "m", "f", 1, "int main(int, char**)") == "f:1 main c|m");
    CHECK(format(QtDebugMsg, "c", "m", "f", 1, "std::vector<int>::size_type ns::f<std::pair<int, int>>(int)") == "f:1 ns::f<std::pair<int, int>> c|m");
    CHECK(format(QtDebugMsg, "c", "m", "f", 1, "auto ns::g()::<lambda(int)>::operator()") == "f:1 ns::g");

    qSetMessagePattern("%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}");
    CHECK(format(QtDebugMsg, "x", "") == "D" && format(QtInfoMsg, "x", "") == "I" && format(QtWarningMsg, "x", "") == "W");
    CHECK(format(QtCriticalMsg, "x", "") == "C" && format(QtFatalMsg, "x", "") == "F");

    qSetMessagePattern("%{if-category}<%{category}>%{endif}%{if-warning}!%{if-category}?%{endif}%{endif} %{message}");
    CHECK(format(QtWarningMsg, "x", "m") == "<x>!? m");
    CHECK(format(QtWarningMsg, "default", "m") == "! m");
    CHECK(format(QtDebugMsg, "x", "m") == "<x> m");

    qSetMessagePattern("%{pid}/%{threadid}/%{appname}");
    const QString parts = format(QtDebugMsg, "x", "");
    CHECK(parts.startsWith(QString::number(getpid()) + "/"));
    CHECK(parts.split('/').size() == 3 && parts.split('/')[1].toLongLong() > 0);
    QCoreApplication::setApplicationName("logtest");
    CHECK(format(QtDebugMsg, "x", "").endsWith("/logtest"));

    qSetMessagePattern("%{qthreadptr}");
    CHECK(format(QtDebugMsg, "x", "").startsWith("0x"));

    qSetMessagePattern("%{time process}|%{time boot}|%{time yyyy}|%{time}");
    const QStringList times = format(QtDebugMsg, "x", "").split('|');
    CHECK(times.size() == 4);
    if (times.size() == 4) {
        CHECK(times[0].contains('.') && times[0].toDouble() >= 0.0);
        CHECK(times[1].contains('.') && times[1].toDouble() > 0.0);
        CHECK(times[2] == QDate::currentDate().toString("yyyy"));
        CHECK(QDateTime::fromString(times[3], Qt::ISODate).isValid());
    }

    // Unknown placeholders, an unterminated one, and plain percent signs are kept as text.
    int saved = dup(2);
    int devnull = open("/dev/null", 1);
    dup2(devnull, 2);
    qSetMessagePattern("100%% %{bogus} %{message} %{unterminated");
    dup2(saved, 2);
    close(saved);
    close(devnull);
    CHECK(format(QtDebugMsg, "x", "m") == "100%% %{bogus} m %{unterminated");
    qSetMessagePattern("%{backtrace}[%{message}]");
    CHECK(format(QtDebugMsg, "x", "m") == "[m]");

    qSetMessagePattern(QString());
    CHECK(format(QtDebugMsg, "x", "m") == "x: m");
}

// Runs the default handler with stderr redirected to a file, and returns what it wrote.
static QByteArray captureStderr(void (*body)()) {
    QTemporaryFile file;
    if (!file.open())
        return {};
    std::fflush(stderr);
    const int saved = dup(2);
    dup2(file.handle(), 2);
    body();
    std::fflush(stderr);
    dup2(saved, 2);
    close(saved);
    file.seek(0);
    return file.readAll();
}

static void testDefaultHandler() {
    qSetMessagePattern(QString());
    CHECK(captureStderr([] { qWarning("hello %s", "there"); }) == "hello there\n");
    CHECK(captureStderr([] { qInfo() << "stream" << 3; }) == "stream 3\n");
    CHECK(captureStderr([] { qCCritical(lcTest) << "oops"; }) == "test.cat: oops\n");
    CHECK(captureStderr([] { qCDebug(lcTest) << "multi\nline"; }) == "test.cat: \"multi\\nline\"\n");
    qSetMessagePattern("%{type}|%{category}|%{message}");
    CHECK(captureStderr([] { qWarning() << "w"; }) == "warning|default|\"w\"\n" || captureStderr([] { qWarning() << "w"; }) == "warning|default|w\n");
    qSetMessagePattern(QString());
    // A custom handler can reuse the format.
    qInstallMessageHandler(+[](QtMsgType type, const QMessageLogContext &context, const QString &message) {
        g_records.push_back({type, "", 0, "", "", qFormatLogMessage(type, context, message).toStdString()});
    });
    g_records.clear();
    qCInfo(lcTest) << "x";
    qInstallMessageHandler(nullptr);
    CHECK(g_records.size() == 1 && g_records[0].text == "test.cat: x");
}

static void testDebugTargets() {
    QString text;
    {
        QDebug(&text) << "a" << 1 << QString("b");
    }
    CHECK(text == "a 1 \"b\" ");
    {
        QDebug d(&text);
        d.noquote().nospace() << "|" << QString("c");
    }
    CHECK(text == "a 1 \"b\" |c");

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    {
        QDebug(&buffer).noquote() << QString("héllo") << 2;
    }
    CHECK(buffer.data() == QByteArray("héllo 2 "));

    // Nothing is emitted to the handler for these.
    auto records = capture([&] { QDebug(&text) << "x"; });
    CHECK(records.empty());

    // The state saver restores spacing and quoting.
    QString out;
    {
        QDebug d(&out);
        {
            QDebugStateSaver saver(d);
            d.nospace().noquote() << "a" << QString("b");
        }
        d << "c";
        CHECK(d.autoInsertSpaces());
    }
    CHECK(out == "abc \"\"" || out == "ab\"c\" " || out == "abc ");
}

static void testFatalAndEnvironment(const char *self) {
    // A fatal message aborts, after the handler ran.
    pid_t pid = fork();
    if (pid == 0) {
        qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext &, const QString &) { _exit(42); });
        // The handler exits first: the abort comes only after a handler that returns.
        qFatal("boom");
    }
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 42);

    pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", 1);
        dup2(devnull, 2);
        qFatal("boom %d", 1);
    }
    waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);

    pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", 1);
        dup2(devnull, 2);
        qCFatal(lcTest) << "stream";
    }
    waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);

    // Children that read the environment, which is looked at when logging first starts.
    struct Case {
        const char *name;
        const char *var;
        const char *value;
        int expected; // exit status, or -SIGABRT
    };
    const Case cases[] = {
        {"fatal-warnings", "QT_FATAL_WARNINGS", "1", -SIGABRT},
        {"fatal-criticals", "QT_FATAL_CRITICALS", "1", -SIGABRT},
        {"fatal-criticals-warning-ok", "QT_FATAL_CRITICALS", "1", 0},
        {"env-rules", "QT_LOGGING_RULES", "test.cat.debug=false;test.other.warning=false", 0},
        {"env-wins", "QT_LOGGING_RULES", "test.cat.debug=false", 0},
        {"env-pattern", "QT_MESSAGE_PATTERN", "<%{type}:%{message}>", 0},
    };
    for (const Case &c : cases) {
        pid = fork();
        if (pid == 0) {
            setenv(c.var, c.value, 1);
            execl(self, self, "--child", c.name, nullptr);
            _exit(99);
        }
        waitpid(pid, &status, 0);
        const bool ok = c.expected < 0 ? WIFSIGNALED(status) && WTERMSIG(status) == -c.expected
                                       : WIFEXITED(status) && WEXITSTATUS(status) == c.expected;
        if (!ok)
            std::printf("child %s: status %d\n", c.name, status);
        CHECK(ok);
    }

    // QT_LOGGING_CONF names a file with a [Rules] section.
    QTemporaryFile conf;
    CHECK(conf.open());
    conf.write("[Other]\ntest.other.debug=false\n[Rules]\n# comment\ntest.cat.debug=false\ntest.cat.warning=false\n[More]\ntest.cat.info=false\n");
    conf.flush();
    pid = fork();
    if (pid == 0) {
        setenv("QT_LOGGING_CONF", conf.fileName().toUtf8().constData(), 1);
        execl(self, self, "--child", "conf", nullptr);
        _exit(99);
    }
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// Exit status 0 means the child saw what it should.
static int runChild(const std::string &name) {
    if (name == "fatal-warnings") {
        qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext &, const QString &) {});
        qWarning("this aborts");
        return 0;
    }
    if (name == "fatal-criticals") {
        qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext &, const QString &) {});
        qCritical("this aborts");
        return 0;
    }
    if (name == "fatal-criticals-warning-ok") {
        qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext &, const QString &) {});
        qWarning("this does not");
        return 0;
    }
    if (name == "env-rules") {
        QLoggingCategory::setFilterRules("test.cat.debug=true;test.other.warning=true");
        // The environment takes precedence over rules set in the program.
        return !lcTest().isDebugEnabled() && !lcOther().isWarningEnabled() && lcTest().isInfoEnabled() ? 0 : 1;
    }
    if (name == "env-wins")
        return !lcTest().isDebugEnabled() ? 0 : 1;
    if (name == "env-pattern") {
        qSetMessagePattern("ignored");
        return format(QtInfoMsg, "x", "m") == "<info:m>" ? 0 : 1;
    }
    if (name == "conf") {
        const bool before = !lcTest().isDebugEnabled() && !lcTest().isWarningEnabled() && lcTest().isInfoEnabled() && lcOther().isDebugEnabled();
        // Rules set in the program rank above the file.
        QLoggingCategory::setFilterRules("test.cat.warning=true");
        return before && lcTest().isWarningEnabled() && !lcTest().isDebugEnabled() ? 0 : 1;
    }
    return 2;
}

static void testCompiledOut() {
    int evaluated = 0;
    CHECK(noOutputCompiledOut(&evaluated) == 0);
    CHECK(evaluated == 1);
}

int main(int argc, char **argv) {
    if (argc == 3 && std::strcmp(argv[1], "--child") == 0)
        return runChild(argv[2]);
    QCoreApplication app(argc, argv);
    testMacroContext();
    testLongMessages();
    testCategoryBasics();
    testCategoryMacros();
    testRules();
    testSetEnabledAndFilter();
    testDefaultCategory();
    testHandler();
    testPattern();
    testDefaultHandler();
    testDebugTargets();
    testCompiledOut();
    testFatalAndEnvironment(argv[0]);
    if (g_failures == 0)
        std::puts("all logging tests passed");
    return g_failures == 0 ? 0 : 1;
}
