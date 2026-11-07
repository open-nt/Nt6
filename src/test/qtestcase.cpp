// MIT License - see LICENSE
#include <QTest>

#include <QtCore/qeventloop.h>
#include <QtCore/qtimer.h>

#include "qtestlog_p.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <typeindex>
#include <vector>

/*!
    \module NtTest
    \title Nt Test C++ Classes
    \brief Unit testing: test functions, data-driven tests, signal spies and result logs.
    \cmaketarget Nt::Test

    Nt Test runs the private slots of a QObject as test functions and logs the
    results. Headers keep Qt's names, so \c{#include <QTest>} and
    \c{#include <QtTest/QSignalSpy>} both work.

    See \l{Writing Tests with Nt Test} for a complete test.
*/

/*!
    \page nt-test.html
    \title Writing Tests with Nt Test
    \brief How to write, build and run a test, and the macros Nt Test provides.

    A test is a QObject with \c Q_OBJECT whose private slots are the test
    functions. \c ntmoc has to process the class: that is how Nt Test finds the
    slots. The usual form is a \c .cpp file that includes its own moc output:

    \snippet test/tst_counter.cpp test

    Link \c Nt::Test and enable moc for the target:

    \code
    add_executable(tst_counter tst_counter.cpp)
    target_link_libraries(tst_counter PRIVATE Nt::Test)
    nt_enable_automoc(tst_counter)
    add_test(NAME tst_counter COMMAND tst_counter)
    \endcode

    The executable returns the number of failed test functions, or 127 if that
    is higher.

    \section1 Test Functions

    Every private slot that returns \c void and takes no arguments runs, in the
    order of declaration, except for these special ones:

    \table
    \header \li Slot \li Runs
    \row \li \c initTestCase() \li Once, before the first test function. If it
         fails, no test function runs.
    \row \li \c cleanupTestCase() \li Once, after the last test function.
    \row \li \c init() \li Before each test function, for each data row. If it
         fails, that test function does not run.
    \row \li \c cleanup() \li After each test function, for each data row.
    \row \li \c initTestCase_data() \li Once, first. Its rows are global: every
         test function runs for every global row.
    \row \li \c name_data() \li Before \c name(). Its rows drive \c name().
    \endtable

    Slots inherited from a base class that \c ntmoc has seen run first.

    \section1 Data-Driven Tests

    A \c _data function builds a table with QTest::addColumn() and
    QTest::newRow(), and the test function reads the current row with
    \c QFETCH(). See the \c toHex test in the example above.

    \section1 Macros

    The macros that report a failure return from the calling function, so they
    only work in a function that returns \c void. To check from a helper
    function, follow the call with QTest::currentTestFailed().

    \table
    \header \li Macro \li Purpose
    \row \li \c{QVERIFY(condition)} \li Fails if \c condition is false.
    \row \li \c{QVERIFY2(condition, message)} \li The same, and adds \c message to the failure.
    \row \li \c{QCOMPARE(actual, expected)} \li Fails unless the values are equal, and prints both.
    \row \li \c{QCOMPARE_EQ}, \c{_NE}, \c{_LT}, \c{_LE}, \c{_GT}, \c{_GE}\c{(computed, baseline)}
         \li Fails unless the relation holds, and prints both values.
    \row \li \c{QFAIL(message)} \li Fails at once.
    \row \li \c{QSKIP(message)} \li Skips the current data row. In a \c _data
         function it skips the test function, and in \c initTestCase() every test function.
    \row \li \c{QEXPECT_FAIL(dataTag, comment, mode)} \li Marks the next check as
         a known failure, for the row \c dataTag or for all rows if it is
         empty. \c mode is \c Abort or \c Continue. If the check passes, the test fails.
    \row \li \c{QFETCH(Type, name)} \li Declares \c name with the value of that
         column in the current row. The row of \c initTestCase_data() is used
         if the test function has no such column.
    \row \li \c{QFETCH_GLOBAL(Type, name)} \li The same for the global row. It
         works in \c init() and \c cleanup() too.
    \row \li \c{QTEST(actual, "name")} \li Compares \c actual with that column of the row.
    \row \li \c{QTRY_VERIFY(condition)}, \c{QTRY_VERIFY2(condition, message)}
         \li Like \c QVERIFY, but first processes events until the condition is
         true or five seconds have passed.
    \row \li \c{QTRY_COMPARE(actual, expected)} \li The same for \c QCOMPARE.
    \row \li \c{QTRY_VERIFY_WITH_TIMEOUT}, \c{QTRY_VERIFY2_WITH_TIMEOUT}, \c{QTRY_COMPARE_WITH_TIMEOUT}
         \li The same, with the timeout in milliseconds as the last argument.
    \row \li \c{QVERIFY_THROWS_EXCEPTION(Type, expression)} \li Fails unless the
         expression throws a \c Type.
    \row \li \c{QVERIFY_THROWS_NO_EXCEPTION(expression)} \li Fails if the expression throws.
    \row \li \c{QFINDTESTDATA(path)} \li Returns the absolute path of a test
         data file. See QTest::qFindTestData().
    \row \li \c{QTEST_MAIN(Class)} \li Defines \c main(): a QCoreApplication and a
         run of the class. Nt has no GUI module, so it is the same as \c QTEST_GUILESS_MAIN.
    \row \li \c{QTEST_GUILESS_MAIN(Class)} \li The same.
    \row \li \c{QTEST_APPLESS_MAIN(Class)} \li The same with no application
         object, for tests that need no event loop.
    \endtable

    \section1 Running a Test

    Name test functions on the command line to run only those, and add a data
    tag to run one row: \c{./tst_counter toHex:zero}. A row of global and local
    data is \c{global:local}. The options are:

    \table
    \header \li Option \li Effect
    \row \li \c{-o file,format} \li Writes the log to \c file, or to the standard
         output if it is \c{-}. Repeat it for several logs. \c format is \c txt,
         \c junitxml or \c tap.
    \row \li \c{-txt}, \c{-junitxml}, \c{-tap} \li Sets the format of the standard
         output, or of a following \c{-o file}.
    \row \li \c{-silent} \li Shows only failures, fatal messages and the totals.
    \row \li \c{-v1} \li Shows each result. This is the default.
    \row \li \c{-v2} \li Also shows when each test function starts.
    \row \li \c{-functions} \li Lists the test functions, and runs nothing.
    \row \li \c{-help} \li Lists the options, and runs nothing.
    \endtable
*/

/*!
    \namespace QTest
    \brief The QTest namespace holds the functions behind Nt Test.

    \differences
    There are no GUI event functions, no benchmarks (\c QBENCHMARK), no
    \c failOnWarning() and no \c QTest::Throw. Regular expressions are not
    accepted by ignoreMessage(), since Nt has no \c QRegularExpression. Test
    classes must have gone through \c ntmoc, and must be outside anonymous
    namespaces and not templates. \c ntmoc does not evaluate preprocessor
    conditionals, so a private slot inside \c{#if} must exist in every
    configuration. Fatal signals and hung tests are not caught.

    Between rows, all pending events are processed to run \c deleteLater(),
    not only deferred deletes. Test macros must be used from the main thread.
    \enddifferences
*/

namespace QTestPrivate {

namespace {

struct Column {
    std::string name;
    const std::type_info *type;
};

using Clock = std::chrono::steady_clock;

qint64 millisecondsSince(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

const char *testFunctionHelp =
    "Usage: %s [options] [testfunction[:datatag]]...\n"
    "\n"
    "  -o file,format  Write the log to file, or to the standard output for '-'.\n"
    "                  Formats: txt, junitxml, tap\n"
    "  -txt, -junitxml, -tap\n"
    "                  Format of the standard output, or of a following -o file\n"
    "  -silent         Show only failures, fatal messages and the totals\n"
    "  -v1             Show each result (the default)\n"
    "  -v2             Also show when each test function starts\n"
    "  -functions      List the test functions and exit\n"
    "  -help           Show this help and exit\n";

bool isKnownFormat(const std::string &format) {
    return format == "txt" || format == "junitxml" || format == "tap";
}

// Collects the private slots of `type`, those of its bases first. A slot that a derived class
// declares again takes the place of the one in the base class.
void collectSlots(const std::type_info &type, std::set<std::type_index> &visited,
                  std::vector<QtPrivate::MetaSlot> &found) {
    const QtPrivate::MetaClass *meta = QtPrivate::findMetaClass(type);
    if (!meta || !visited.insert(std::type_index(type)).second)
        return;
    for (std::size_t i = 0; i < meta->baseCount; ++i)
        collectSlots(*meta->bases[i], visited, found);
    for (std::size_t i = 0; i < meta->privateSlotCount; ++i) {
        const QtPrivate::MetaSlot &slot = meta->privateSlots[i];
        const auto same = [&](const QtPrivate::MetaSlot &other) {
            return std::string(other.name) == slot.name;
        };
        const auto existing = std::find_if(found.begin(), found.end(), same);
        if (existing == found.end())
            found.push_back(slot);
        else
            existing->invoke = slot.invoke;
    }
}

bool endsWith(const std::string &text, const std::string &suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isSpecialSlot(const std::string &name) {
    return name == "initTestCase" || name == "cleanupTestCase" || name == "init" ||
           name == "cleanup" || endsWith(name, "_data");
}

std::string g_appName;

} // namespace

struct TestTable {
    std::vector<Column> columns;
    std::vector<std::unique_ptr<QTestData>> rows;

    QTestData &addRow(const char *tag) {
        rows.emplace_back(new QTestData(tag, this));
        return *rows.back();
    }

    int columnIndex(const char *name) const {
        for (std::size_t i = 0; i < columns.size(); ++i)
            if (columns[i].name == name)
                return static_cast<int>(i);
        return -1;
    }

    // The value of `name` in `row`, or nullptr if there is no such column. A wrong type or a
    // missing cell is a bug in the test that cannot be reported from an expression.
    const void *value(const QTestData &row, const char *name, const std::type_info &type) const {
        const int index = columnIndex(name);
        if (index < 0)
            return nullptr;
        const auto column = static_cast<std::size_t>(index);
        if (*columns[column].type != type)
            qFatal("Requested type '%s' does not match the type '%s' of column '%s'.", type.name(),
                   columns[column].type->name(), name);
        if (column >= row.m_cells.size())
            qFatal("Row '%s' has no value for column '%s'.", row.m_tag.c_str(), name);
        return row.m_cells[column].value.get();
    }

    const std::string &tag(std::size_t row) const { return rows[row]->m_tag; }
};

namespace {

struct ExpectedFailure {
    bool active = false;
    std::string comment;
    QTest::TestFailMode mode = QTest::Abort;
};

struct IgnoredMessage {
    QtMsgType type;
    std::string text;
};

struct Selection {
    std::string function;
    std::string tag;
};

struct Output {
    std::string file;
    std::string format;
};

class Run {
public:
    Run(QObject *object, std::vector<std::string> arguments)
        : m_object(object), m_arguments(std::move(arguments)) {}

    int exec();

    bool reportPass();
    bool reportFailure(const std::string &message, const char *file, int line);
    void skip(const std::string &message, const char *file, int line);
    bool expectFail(const char *tag, const char *comment, QTest::TestFailMode mode, const char *file,
                    int line);
    void ignoreMessage(QtMsgType type, const char *text);
    void handleMessage(QtMsgType type, const std::string &text);

    const void *fetch(const char *name, const std::type_info &type, bool global) const;

    TestTable *activeTable() const { return m_activeTable; }
    const char *function() const { return m_function.c_str(); }
    const char *dataTag() const { return m_localRow ? m_localRow->dataTag() : nullptr; }
    const char *globalDataTag() const { return m_globalRow ? m_globalRow->dataTag() : nullptr; }
    bool rowFailed() const { return m_rowFailed; }

private:
    enum class Parsed { Run, ExitSuccess, ExitFailure };

    Parsed parseArguments();
    bool collectTests();
    bool openLogs();
    void runAll();
    bool runFunction(const Selection &selection);
    void runRow(const QtPrivate::MetaSlot &function);
    Outcome finishRow();
    void beginRow();
    void call(const QtPrivate::MetaSlot *slot);
    void callWithTable(const QtPrivate::MetaSlot *slot, TestTable *table);
    void checkExpectedFailure();
    const QtPrivate::MetaSlot *findSlot(const std::string &name) const;
    bool matchesTag(const std::string &tag) const;
    std::string label() const;
    void event(Event event, const std::string &message, const char *file, int line);

    QObject *m_object;
    std::vector<std::string> m_arguments;

    std::string m_className;
    std::vector<QtPrivate::MetaSlot> m_slots;
    std::vector<Selection> m_selection;
    std::vector<Output> m_outputs;
    std::vector<std::unique_ptr<Logger>> m_loggers;
    Verbosity m_verbosity = Verbosity::Normal;

    Totals m_totals;
    std::string m_function;
    Clock::time_point m_rowStart;
    bool m_rowFailed = false;
    bool m_rowSkipped = false;
    ExpectedFailure m_expected;
    std::vector<IgnoredMessage> m_ignored;

    TestTable m_globalTable;
    TestTable *m_activeTable = nullptr;
    const TestTable *m_localTable = nullptr;
    const QTestData *m_localRow = nullptr;
    const QTestData *m_globalRow = nullptr;

    // Messages may come from any thread; everything else stays on the main thread.
    std::recursive_mutex m_logMutex;
};

Run *g_run = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message) {
    if (g_run)
        g_run->handleMessage(type, message.toStdString());
}

Run::Parsed Run::parseArguments() {
    if (!m_arguments.empty()) {
        g_appName = std::filesystem::path(m_arguments.front()).filename().string();
    } else {
        g_appName = m_className;
    }

    std::string format = "txt";
    bool listFunctions = false;
    for (std::size_t i = 1; i < m_arguments.size(); ++i) {
        const std::string &argument = m_arguments[i];
        if (argument == "-help" || argument == "--help" || argument == "-h") {
            std::printf(testFunctionHelp, g_appName.c_str());
            return Parsed::ExitSuccess;
        } else if (argument == "-functions") {
            listFunctions = true;
        } else if (argument == "-silent") {
            m_verbosity = Verbosity::Silent;
        } else if (argument == "-v1") {
            m_verbosity = Verbosity::Normal;
        } else if (argument == "-v2") {
            m_verbosity = Verbosity::Verbose;
        } else if (argument == "-txt" || argument == "-junitxml" || argument == "-tap") {
            format = argument.substr(1);
        } else if (argument == "-o") {
            if (++i == m_arguments.size()) {
                std::printf("-o needs an argument: file,format\n");
                return Parsed::ExitFailure;
            }
            const std::string &value = m_arguments[i];
            const std::size_t comma = value.rfind(',');
            Output output{value.substr(0, comma), comma == std::string::npos ? format : value.substr(comma + 1)};
            if (!isKnownFormat(output.format)) {
                std::printf("Unsupported log format: %s\n", output.format.c_str());
                return Parsed::ExitFailure;
            }
            m_outputs.push_back(std::move(output));
        } else if (!argument.empty() && argument[0] == '-') {
            std::printf("Unknown option: '%s'\n\n", argument.c_str());
            std::printf(testFunctionHelp, g_appName.c_str());
            return Parsed::ExitFailure;
        } else {
            const std::size_t colon = argument.find(':');
            Selection selection{argument.substr(0, colon),
                                colon == std::string::npos ? std::string() : argument.substr(colon + 1)};
            if (isSpecialSlot(selection.function) || !findSlot(selection.function)) {
                std::printf("Unknown test function: '%s'\n", selection.function.c_str());
                return Parsed::ExitFailure;
            }
            m_selection.push_back(std::move(selection));
        }
    }
    if (listFunctions) {
        for (const QtPrivate::MetaSlot &slot : m_slots)
            if (!isSpecialSlot(slot.name))
                std::printf("%s()\n", slot.name);
        return Parsed::ExitSuccess;
    }
    if (m_outputs.empty())
        m_outputs.push_back({"-", format});
    return Parsed::Run;
}

bool Run::collectTests() {
    const QObject &object = *m_object;
    const std::type_info &type = typeid(object);
    const QtPrivate::MetaClass *meta = QtPrivate::findMetaClass(type);
    if (!meta) {
        std::printf("QTest::qExec: no test functions are known for class %s. It needs Q_OBJECT, "
                    "and ntmoc has to process it; the class cannot be in an anonymous namespace.\n",
                    type.name());
        return false;
    }
    m_className = meta->className;
    std::set<std::type_index> visited;
    collectSlots(type, visited, m_slots);
    return true;
}

bool Run::openLogs() {
    for (const Output &output : m_outputs) {
        std::FILE *file = output.file == "-" ? stdout : std::fopen(output.file.c_str(), "w");
        if (!file) {
            std::printf("Cannot open the log file '%s'\n", output.file.c_str());
            return false;
        }
        m_loggers.push_back(createLogger(output.format, file, m_verbosity));
    }
    return true;
}

int Run::exec() {
    if (!collectTests())
        return 1;
    const Parsed parsed = parseArguments();
    if (parsed != Parsed::Run)
        return parsed == Parsed::ExitSuccess ? 0 : 1;
    if (!openLogs())
        return 1;

    const Clock::time_point start = Clock::now();
    for (const auto &logger : m_loggers)
        logger->begin(m_className);

    g_run = this;
    const QtMessageHandler previousHandler = qInstallMessageHandler(messageHandler);
    runAll();
    qInstallMessageHandler(previousHandler);
    g_run = nullptr;

    m_totals.elapsedMs = millisecondsSince(start);
    for (const auto &logger : m_loggers)
        logger->end(m_className, m_totals);
    return std::min(m_totals.failed, 127);
}

void Run::runAll() {
    m_function = "initTestCase";
    beginRow();
    callWithTable(findSlot("initTestCase_data"), &m_globalTable);
    if (!m_rowFailed && !m_rowSkipped)
        call(findSlot("initTestCase"));
    const Outcome initialized = finishRow();
    if (initialized == Outcome::Skip)
        return;

    if (initialized == Outcome::Pass) {
        std::vector<Selection> selection = m_selection;
        if (selection.empty())
            for (const QtPrivate::MetaSlot &slot : m_slots)
                if (!isSpecialSlot(slot.name))
                    selection.push_back({slot.name, {}});
        // The global data rows are the outer loop: every test function runs for each of them.
        std::vector<bool> matched(selection.size(), false);
        const std::size_t globalRows = std::max<std::size_t>(1, m_globalTable.rows.size());
        for (std::size_t g = 0; g < globalRows; ++g) {
            m_globalRow = m_globalTable.rows.empty() ? nullptr : m_globalTable.rows[g].get();
            for (std::size_t i = 0; i < selection.size(); ++i)
                if (runFunction(selection[i]))
                    matched[i] = true;
        }
        m_globalRow = nullptr;
        for (std::size_t i = 0; i < selection.size(); ++i) {
            if (matched[i])
                continue;
            m_function = selection[i].function;
            beginRow();
            reportFailure("Unknown data tag: '" + selection[i].tag + "'", nullptr, 0);
            finishRow();
        }
    }

    m_function = "cleanupTestCase";
    beginRow();
    call(findSlot("cleanupTestCase"));
    finishRow();
}

bool Run::matchesTag(const std::string &tag) const {
    if (tag.empty())
        return true;
    const std::string local = m_localRow ? m_localRow->dataTag() : "";
    const std::string global = m_globalRow ? m_globalRow->dataTag() : "";
    return tag == local || tag == global || tag == global + ":" + local;
}

// Runs one test function for the current global data row. Returns whether any of
// its rows matched the data tag of the selection.
bool Run::runFunction(const Selection &selection) {
    m_function = selection.function;
    const QtPrivate::MetaSlot *function = findSlot(selection.function);
    const QtPrivate::MetaSlot *dataFunction = findSlot(selection.function + "_data");
    bool matched = false;

    TestTable table;
    m_localTable = &table;
    if (dataFunction) {
        beginRow();
        callWithTable(dataFunction, &table);
        if (m_rowFailed || m_rowSkipped) {
            m_localTable = nullptr;
            finishRow();
            return true;
        }
        if (table.rows.empty()) {
            m_localTable = nullptr;
            skip("No data available for this test function", nullptr, 0);
            finishRow();
            return true;
        }
    }
    const std::size_t localRows = std::max<std::size_t>(1, table.rows.size());
    for (std::size_t l = 0; l < localRows; ++l) {
        m_localRow = table.rows.empty() ? nullptr : table.rows[l].get();
        if (matchesTag(selection.tag)) {
            matched = true;
            runRow(*function);
        }
    }
    m_localRow = nullptr;
    m_localTable = nullptr;
    return matched;
}

void Run::runRow(const QtPrivate::MetaSlot &function) {
    beginRow();
    event(Event::Note, "entering", nullptr, 0);
    call(findSlot("init"));
    if (!m_rowFailed && !m_rowSkipped)
        call(&function);
    checkExpectedFailure();
    call(findSlot("cleanup"));
    finishRow();
}

void Run::beginRow() {
    m_rowFailed = false;
    m_rowSkipped = false;
    m_expected = ExpectedFailure();
    m_ignored.clear();
    m_rowStart = Clock::now();
}

void Run::checkExpectedFailure() {
    if (m_expected.active) {
        m_expected.active = false;
        reportFailure("QEXPECT_FAIL was called without any check after it", nullptr, 0);
    }
}

Outcome Run::finishRow() {
    checkExpectedFailure();
    {
        const std::lock_guard<std::recursive_mutex> lock(m_logMutex);
        for (const IgnoredMessage &ignored : m_ignored)
            reportFailure("Did not receive the message: \"" + ignored.text + "\"", nullptr, 0);
        m_ignored.clear();
    }
    // Objects from deleteLater() must not outlive the row that made them.
    if (QCoreApplication::instance())
        QCoreApplication::processEvents();

    Outcome outcome = Outcome::Pass;
    if (m_rowFailed) {
        outcome = Outcome::Fail;
        ++m_totals.failed;
    } else if (m_rowSkipped) {
        outcome = Outcome::Skip;
        ++m_totals.skipped;
    } else {
        ++m_totals.passed;
    }
    const qint64 elapsed = millisecondsSince(m_rowStart);
    const std::string text = label();
    const std::lock_guard<std::recursive_mutex> lock(m_logMutex);
    for (const auto &logger : m_loggers)
        logger->rowFinished(outcome, text, elapsed);
    return outcome;
}

void Run::call(const QtPrivate::MetaSlot *slot) {
    if (!slot)
        return;
    try {
        slot->invoke(m_object);
    } catch (const std::exception &e) {
        reportFailure(std::string("Caught unhandled exception: ") + e.what(), nullptr, 0);
    } catch (...) {
        reportFailure("Caught unhandled exception", nullptr, 0);
    }
}

void Run::callWithTable(const QtPrivate::MetaSlot *slot, TestTable *table) {
    m_activeTable = table;
    call(slot);
    m_activeTable = nullptr;
}

const QtPrivate::MetaSlot *Run::findSlot(const std::string &name) const {
    for (const QtPrivate::MetaSlot &slot : m_slots)
        if (name == slot.name)
            return &slot;
    return nullptr;
}

std::string Run::label() const {
    std::string tag;
    if (m_globalRow)
        tag = m_globalRow->dataTag();
    if (m_globalRow && m_localRow)
        tag += ":";
    if (m_localRow)
        tag += m_localRow->dataTag();
    return m_className + "::" + m_function + "(" + tag + ")";
}

void Run::event(Event event, const std::string &message, const char *file, int line) {
    const std::string text = label();
    const std::lock_guard<std::recursive_mutex> lock(m_logMutex);
    for (const auto &logger : m_loggers)
        logger->event(event, text, message, file, line);
}

bool Run::reportPass() {
    if (!m_expected.active)
        return true;
    m_expected.active = false;
    m_rowFailed = true;
    event(Event::XPass, "Passed unexpectedly: " + m_expected.comment, nullptr, 0);
    return m_expected.mode == QTest::Continue;
}

bool Run::reportFailure(const std::string &message, const char *file, int line) {
    if (m_expected.active) {
        m_expected.active = false;
        event(Event::XFail, m_expected.comment, file, line);
        return m_expected.mode == QTest::Continue;
    }
    m_rowFailed = true;
    event(Event::Fail, message, file, line);
    return false;
}

void Run::skip(const std::string &message, const char *file, int line) {
    m_expected.active = false;
    m_rowSkipped = true;
    event(Event::Skip, message, file, line);
}

bool Run::expectFail(const char *tag, const char *comment, QTest::TestFailMode mode, const char *file,
                     int line) {
    if (tag && *tag && !matchesTag(tag))
        return true;
    if (m_expected.active) {
        // Clear it first, or the error below would be taken for the expected failure.
        m_expected.active = false;
        reportFailure("QEXPECT_FAIL was called twice without a check in between", file, line);
        return false;
    }
    m_expected.active = true;
    m_expected.comment = comment;
    m_expected.mode = mode;
    return true;
}

void Run::ignoreMessage(QtMsgType type, const char *text) {
    const std::lock_guard<std::recursive_mutex> lock(m_logMutex);
    m_ignored.push_back({type, text});
}

void Run::handleMessage(QtMsgType type, const std::string &text) {
    {
        const std::lock_guard<std::recursive_mutex> lock(m_logMutex);
        const auto ignored = std::find_if(m_ignored.begin(), m_ignored.end(), [&](const IgnoredMessage &m) {
            return m.type == type && m.text == text;
        });
        if (ignored != m_ignored.end()) {
            m_ignored.erase(ignored);
            return;
        }
    }
    switch (type) {
    case QtDebugMsg: event(Event::Debug, text, nullptr, 0); break;
    case QtInfoMsg: event(Event::Info, text, nullptr, 0); break;
    case QtWarningMsg: event(Event::Warning, text, nullptr, 0); break;
    case QtCriticalMsg: event(Event::Critical, text, nullptr, 0); break;
    case QtFatalMsg: event(Event::Fatal, text, nullptr, 0); break;
    }
}

const void *Run::fetch(const char *name, const std::type_info &type, bool global) const {
    const void *value = nullptr;
    if (!global && m_localTable && m_localRow)
        value = m_localTable->value(*m_localRow, name, type);
    if (!value && m_globalRow)
        value = m_globalTable.value(*m_globalRow, name, type);
    if (!value)
        qFatal("QFETCH: The test data '%s' is not available, check the _data function.", name);
    return value;
}

// Macros can run outside qExec(), in a helper called from an ordinary program.
Run &currentRun(const char *what) {
    if (!g_run)
        qFatal("%s can only be used while QTest::qExec() is running.", what);
    return *g_run;
}

TestTable &activeTable(const char *what) {
    TestTable *table = currentRun(what).activeTable();
    if (!table)
        qFatal("%s can only be called from a _data function.", what);
    return *table;
}

int execute(QObject *testObject, std::vector<std::string> arguments) {
    if (!testObject) {
        std::printf("QTest::qExec: the test object is null\n");
        return 1;
    }
    if (g_run) {
        std::printf("QTest::qExec: a test is already running\n");
        return 1;
    }
    Run run(testObject, std::move(arguments));
    return run.exec();
}

std::string withPadding(const char *label, std::size_t labelWidth, const char *expression, std::size_t width) {
    std::string text = std::string("   ") + label;
    if (text.size() < 3 + labelWidth)
        text.append(3 + labelWidth - text.size(), ' ');
    text += std::string(" (") + expression + ")";
    if (text.size() < width)
        text.append(width - text.size(), ' ');
    return text;
}

} // namespace

void addColumn(const std::type_info &type, const char *name) {
    TestTable &table = activeTable("QTest::addColumn()");
    if (!table.rows.empty())
        qFatal("QTest::addColumn(): the columns must be added before the first row.");
    if (table.columnIndex(name) >= 0)
        qFatal("QTest::addColumn(): there is already a column called '%s'.", name);
    table.columns.push_back({name, &type});
}

const void *fetchData(const char *name, const std::type_info &type, bool global) {
    return currentRun("QFETCH").fetch(name, type, global);
}

bool reportPass(const char *, int) { return g_run ? g_run->reportPass() : true; }

bool reportFailure(const char *summary, const char *actualValue, const char *expectedValue,
                   const char *actual, const char *expected, const char *actualLabel,
                   const char *expectedLabel, const char *file, int line) {
    const char *noText = "(no string representation)";
    // Pad the shorter label so that the expressions and the colons line up.
    const std::size_t labelWidth = std::max(std::strlen(actualLabel), std::strlen(expectedLabel));
    const std::size_t width = std::max(withPadding(actualLabel, labelWidth, actual, 0).size(),
                                       withPadding(expectedLabel, labelWidth, expected, 0).size());
    const std::string message = std::string(summary) + "\n" + withPadding(actualLabel, labelWidth, actual, width) +
                                ": " + (actualValue ? actualValue : noText) + "\n" +
                                withPadding(expectedLabel, labelWidth, expected, width) + ": " +
                                (expectedValue ? expectedValue : noText);
    return currentRun("QCOMPARE").reportFailure(message, file, line);
}

const char *comparisonFailure(ComparisonOp op) {
    switch (op) {
    case ComparisonOp::Equal: return "The computed value is expected to be equal to the baseline, but is not";
    case ComparisonOp::NotEqual: return "The computed value is expected to be different from the baseline, but is not";
    case ComparisonOp::Less: return "The computed value is expected to be less than the baseline, but is not";
    case ComparisonOp::LessEqual: return "The computed value is expected to be less than or equal to the baseline, but is not";
    case ComparisonOp::Greater: return "The computed value is expected to be greater than the baseline, but is not";
    case ComparisonOp::GreaterEqual: return "The computed value is expected to be greater than or equal to the baseline, but is not";
    }
    return "";
}

} // namespace QTestPrivate

/*!
    \class QTestData
    \brief The QTestData class is a row of test data under construction.

    QTest::newRow() returns a QTestData, and \c{operator<<} fills in one cell
    per column, in the order the columns were added.

    \code
    QTest::newRow("empty") << QString() << 0;
    \endcode

    \differences
    The class cannot be copied. A value of the wrong type stops the test with
    a fatal message, as a wrong QFETCH() type does; the type must match
    exactly, so an \c int is not accepted by a \c qsizetype column.
    \enddifferences

    \sa QTest::addColumn()
*/

QTestData::QTestData(const char *tag, QTestPrivate::TestTable *table) : m_tag(tag), m_table(table) {}

void QTestData::append(const std::type_info &type, std::shared_ptr<const void> value) {
    const std::size_t column = m_cells.size();
    if (column >= m_table->columns.size())
        qFatal("Row '%s' has more values than there are columns.", m_tag.c_str());
    if (*m_table->columns[column].type != type)
        qFatal("Row '%s': the value for column '%s' has type '%s' instead of '%s'.", m_tag.c_str(),
               m_table->columns[column].name.c_str(), type.name(), m_table->columns[column].type->name());
    m_cells.push_back({&type, std::move(value)});
}

/*!
    \overload

    Appends the string literal or C string \a value as the next cell. It may
    fill a \c QString column, a \c QByteArray column or a column of
    \c{const char *}. Returns a reference to the row.
*/
QTestData &QTestData::operator<<(const char *value) {
    const std::size_t column = m_cells.size();
    if (column < m_table->columns.size()) {
        if (*m_table->columns[column].type == typeid(QString)) {
            append(typeid(QString), std::make_shared<const QString>(QString::fromUtf8(value)));
            return *this;
        }
        if (*m_table->columns[column].type == typeid(QByteArray)) {
            append(typeid(QByteArray), std::make_shared<const QByteArray>(value));
            return *this;
        }
    }
    append(typeid(const char *), std::make_shared<const char *const>(value));
    return *this;
}

/*!
    Returns the name of this row.
*/
const char *QTestData::dataTag() const { return m_tag.c_str(); }

namespace QTest {

using QTestPrivate::g_run;

int qExec(QObject *testObject, int argc, char **argv) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc : 0));
    for (int i = 0; i < argc; ++i)
        arguments.emplace_back(argv[i]);
    return QTestPrivate::execute(testObject, std::move(arguments));
}

int qExec(QObject *testObject, const QStringList &arguments) {
    std::vector<std::string> list;
    for (const QString &argument : arguments)
        list.push_back(argument.toStdString());
    return QTestPrivate::execute(testObject, std::move(list));
}

QTestData &newRow(const char *dataTag) { return QTestPrivate::activeTable("QTest::newRow()").addRow(dataTag); }

QTestData &addRow(const char *format, ...) {
    char tag[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(tag, sizeof tag, format, args);
    va_end(args);
    return QTestPrivate::activeTable("QTest::addRow()").addRow(tag);
}

const char *currentAppName() { return QTestPrivate::g_appName.c_str(); }

const char *currentTestFunction() { return g_run ? g_run->function() : nullptr; }

const char *currentDataTag() { return g_run ? g_run->dataTag() : nullptr; }

const char *currentGlobalDataTag() { return g_run ? g_run->globalDataTag() : nullptr; }

bool currentTestFailed() { return g_run && g_run->rowFailed(); }

void ignoreMessage(QtMsgType type, const char *message) {
    QTestPrivate::currentRun("QTest::ignoreMessage()").ignoreMessage(type, message);
}

void qSleep(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void qWait(int ms) {
    if (!QCoreApplication::instance()) {
        qSleep(ms);
        return;
    }
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(ms);
    loop.exec();
}

void qWait(std::chrono::milliseconds time) { qWait(static_cast<int>(time.count())); }

QString qFindTestData(const QString &base, const char *file, int line) {
    namespace fs = std::filesystem;
    const fs::path path(base.toStdString());
    std::vector<fs::path> candidates;
    if (path.is_absolute()) {
        candidates.push_back(path);
    } else {
        if (const QStringList arguments = QCoreApplication::instance() ? QCoreApplication::arguments() : QStringList();
            !arguments.isEmpty())
            candidates.push_back(fs::absolute(arguments.first().toStdString()).parent_path() / path);
        candidates.push_back(fs::current_path() / path);
        if (file)
            candidates.push_back(fs::path(file).parent_path() / path);
    }
    std::error_code error;
    for (const fs::path &candidate : candidates)
        if (fs::exists(candidate, error))
            return QString::fromStdString(fs::absolute(candidate).lexically_normal().string());
    qWarning("QFINDTESTDATA: could not find %s (%s:%d)", base.toUtf8().constData(), file ? file : "?", line);
    return QString();
}

bool qVerify(bool statement, const char *statementText, const char *description, const char *file, int line) {
    if (statement)
        return QTestPrivate::reportPass(file, line);
    const std::string message = std::string("'") + statementText + "' returned FALSE. (" + description + ")";
    return QTestPrivate::currentRun("QVERIFY").reportFailure(message, file, line);
}

void qFail(const char *message, const char *file, int line) {
    QTestPrivate::currentRun("QFAIL").reportFailure(message, file, line);
}

void qSkip(const char *message, const char *file, int line) {
    QTestPrivate::currentRun("QSKIP").skip(message, file, line);
}

bool qExpectFail(const char *dataTag, const char *comment, TestFailMode mode, const char *file, int line) {
    return QTestPrivate::currentRun("QEXPECT_FAIL").expectFail(dataTag, comment, mode, file, line);
}

} // namespace QTest
