// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>
#include <QtCore/qversionnumber.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace QTestPrivate {

struct TestTable;

// One value of a row. The column says which type it must have.
struct Cell {
    const std::type_info *type;
    std::shared_ptr<const void> value;
};

void addColumn(const std::type_info &type, const char *name);
const void *fetchData(const char *name, const std::type_info &type, bool global);

char *boolToString(bool value);
char *charToString(char value);
char *integerToString(long long value);
char *unsignedToString(unsigned long long value);
char *realToString(double value);
char *realToString(float value);
char *pointerToString(const void *value);
char *sequenceToString(const char *open, const std::vector<std::string> &items, const char *close);

bool fuzzyEqual(double a, double b);
bool fuzzyEqual(float a, float b);

// Both return false if the test function must stop.
bool reportPass(const char *file, int line);
bool reportFailure(const char *summary, const char *actualValue, const char *expectedValue,
                   const char *actual, const char *expected, const char *actualLabel,
                   const char *expectedLabel, const char *file, int line);

template <typename T>
inline constexpr bool isCString =
    std::is_same_v<std::decay_t<T>, const char *> || std::is_same_v<std::decay_t<T>, char *>;
template <typename T>
inline constexpr bool isInteger = std::is_integral_v<T> && !std::is_same_v<T, bool>;

enum class ComparisonOp { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

// Compares integers by value, whatever their signedness.
template <typename A, typename B>
constexpr bool integerLess(A a, B b) noexcept {
    if constexpr (std::is_signed_v<A> == std::is_signed_v<B>)
        return a < b;
    else if constexpr (std::is_signed_v<A>)
        return a < 0 || static_cast<std::make_unsigned_t<A>>(a) < b;
    else
        return b >= 0 && a < static_cast<std::make_unsigned_t<B>>(b);
}

template <typename A, typename B>
bool valuesEqual(const A &a, const B &b) {
    if constexpr (isCString<A> && isCString<B>) {
        const char *x = a;
        const char *y = b;
        return (x && y) ? std::strcmp(x, y) == 0 : x == y;
    } else if constexpr (isInteger<A> && isInteger<B>) {
        return !integerLess(a, b) && !integerLess(b, a);
    } else if constexpr (std::is_arithmetic_v<A> && std::is_arithmetic_v<B> &&
                         (std::is_floating_point_v<A> || std::is_floating_point_v<B>)) {
        if constexpr (std::is_same_v<A, float> || std::is_same_v<B, float>)
            return fuzzyEqual(static_cast<float>(a), static_cast<float>(b));
        else
            return fuzzyEqual(static_cast<double>(a), static_cast<double>(b));
    } else {
        return a == b;
    }
}

template <typename A, typename B>
bool valuesLess(const A &a, const B &b) {
    if constexpr (isInteger<A> && isInteger<B>)
        return integerLess(a, b);
    else
        return a < b;
}

template <ComparisonOp Op, typename A, typename B>
bool holds(const A &a, const B &b) {
    if constexpr (Op == ComparisonOp::Equal)
        return valuesEqual(a, b);
    else if constexpr (Op == ComparisonOp::NotEqual)
        return !valuesEqual(a, b);
    else if constexpr (Op == ComparisonOp::Less)
        return valuesLess(a, b);
    else if constexpr (Op == ComparisonOp::LessEqual)
        return !valuesLess(b, a);
    else if constexpr (Op == ComparisonOp::Greater)
        return valuesLess(b, a);
    else
        return !valuesLess(a, b);
}

const char *comparisonFailure(ComparisonOp op);

} // namespace QTestPrivate

class QTestData {
public:
    QTestData(const QTestData &) = delete;
    QTestData &operator=(const QTestData &) = delete;

    /*!
        Appends \a value as the next cell of this row and returns a reference
        to the row. \c T must be exactly the type given to
        QTest::addColumn() for that column.
    */
    template <typename T>
    QTestData &operator<<(const T &value) {
        append(typeid(T), std::make_shared<const T>(value));
        return *this;
    }
    QTestData &operator<<(const char *value);

    const char *dataTag() const;

private:
    friend struct QTestPrivate::TestTable;
    QTestData(const char *tag, QTestPrivate::TestTable *table);

    void append(const std::type_info &type, std::shared_ptr<const void> value);

    std::string m_tag;
    QTestPrivate::TestTable *m_table;
    std::vector<QTestPrivate::Cell> m_cells;
};

namespace QTest {

/*!
    \enum QTest::TestFailMode

    This enum says what happens after a failure announced by \c QEXPECT_FAIL.

    \value Abort The test function stops, as it does after an unexpected failure.
    \value Continue The test function goes on with the statement after the failure.
*/
enum TestFailMode { Abort = 1, Continue = 2 };

/*!
    Runs the test functions of \a testObject and returns the number of
    failures, up to 127. \a argc and \a argv hold the command line described
    in \l{Writing Tests with Nt Test}.

    Returns 1 without running anything if the arguments are wrong or \c ntmoc
    has not seen the class of \a testObject. It may be called more than once,
    but not while another call is running.
*/
int qExec(QObject *testObject, int argc = 0, char **argv = nullptr);
/*!
    \overload

    Runs \a testObject with the command line in \a arguments, the first item
    of which is the program name.
*/
int qExec(QObject *testObject, const QStringList &arguments);

/*!
    Adds a column of type \c T called \a name to the test data table. Call it
    from a \c _data function, before the first row. QFETCH() reads the column
    by this name in the test function.

    \sa newRow(), addRow()
*/
template <typename T>
void addColumn(const char *name) {
    QTestPrivate::addColumn(typeid(T), name);
}

/*!
    Adds a row called \a dataTag to the test data table and returns it, to
    fill in with \c{operator<<}. Call it from a \c _data function, after the
    columns. The tag names the row in the log and on the command line.

    \sa addRow(), addColumn()
*/
QTestData &newRow(const char *dataTag);
/*!
    Adds a row to the test data table, with a tag built from the \c printf
    style \a format and the arguments after it, and returns the row.

    \code
    for (int i = 0; i < 4; ++i)
        QTest::addRow("size-%d", i) << i;
    \endcode

    \sa newRow()
*/
QTestData &addRow(const char *format, ...);

/*!
    Returns the file name of the test program, or the class name if qExec()
    was not given a command line. Returns an empty string before the first
    call of qExec().
*/
const char *currentAppName();
/*!
    Returns the name of the running test function, or \c nullptr outside qExec().
*/
const char *currentTestFunction();
/*!
    Returns the tag of the current data row, or \c nullptr if there is none.

    \sa currentGlobalDataTag()
*/
const char *currentDataTag();
/*!
    Returns the tag of the current row of \c initTestCase_data(), or
    \c nullptr if there is none.

    \sa currentDataTag()
*/
const char *currentGlobalDataTag();
/*!
    Returns \c true if a check has failed in the current data row. Use it
    after calling a helper function that uses QVERIFY() or QCOMPARE(), as they
    return only from the helper.
*/
bool currentTestFailed();

/*!
    Expects the next message of the given \a type to be \a message, and keeps
    it out of the log. Call it again for each further message. The current
    data row fails if the message does not arrive before it ends.

    \a message must match the whole text.
*/
void ignoreMessage(QtMsgType type, const char *message);

/*!
    Blocks for \a ms milliseconds without processing events.

    \sa qWait()
*/
void qSleep(int ms);
/*!
    Waits for \a ms milliseconds, processing events meanwhile. Without a
    QCoreApplication it only sleeps.

    \sa qWaitFor(), qSleep()
*/
void qWait(int ms);
/*!
    \overload

    Waits for \a time, processing events meanwhile.
*/
void qWait(std::chrono::milliseconds time);

/*!
    Calls \a predicate repeatedly, processing events in between, until it
    returns \c true or \a timeout milliseconds have passed. Returns the last
    value it returned. The default timeout is five seconds.

    \sa qWait()
*/
template <typename Functor>
bool qWaitFor(Functor predicate, int timeout = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (true) {
        if (predicate())
            return true;
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0)
            return predicate();
        qWait(left.count() < 10 ? static_cast<int>(left.count()) : 10);
    }
}

/*!
    \overload

    Waits for \a predicate for at most \a timeout.
*/
template <typename Functor>
bool qWaitFor(Functor predicate, std::chrono::milliseconds timeout) {
    return qWaitFor(std::move(predicate), static_cast<int>(timeout.count()));
}

/*!
    Returns the absolute path of the test data file \a base, or an empty
    string, after a warning, if it does not exist. Looks next to the test
    program, in the current directory and in the directory of the source file
    \a file, in that order. QFINDTESTDATA() passes \a file and \a line, and
    \a line is only there for the warning.
*/
QString qFindTestData(const QString &base, const char *file = nullptr, int line = 0);

/*!
    \overload

    Returns \a value in double quotes, with quotes and control characters escaped, or \c nullptr as text for a null pointer.
*/
char *toString(const char *value);
/*!
    \overload

    Returns the text \c nullptr.
*/
char *toString(std::nullptr_t);
/*!
    \overload

    Returns \c true or \c false as text.
*/
char *toString(bool value);
/*!
    \overload

    Returns the character \a value in double quotes, escaped when needed.
*/
char *toString(char value);
/*!
    \overload

    Returns \a value in double quotes, escaped when needed, as UTF-8.
*/
char *toString(const QString &value);
/*!
    \overload

    Returns \a value in double quotes, escaped when needed, as UTF-8.
*/
char *toString(QStringView value);
/*!
    \overload

    Returns \a value in double quotes, escaped when needed.
*/
char *toString(QLatin1StringView value);
/*!
    \overload

    Returns the strings of \a value as \c{QStringList("a", "b")}.
*/
char *toString(const QStringList &value);
/*!
    \overload

    Returns \a value in double quotes, with bytes that are not printable written as \c{\xNN}.
*/
char *toString(const QByteArray &value);
/*!
    \overload

    Returns the character \a value in double quotes, escaped when needed.
*/
char *toString(QChar value);
/*!
    \overload

    Returns \a value in ISO 8601 form, or \c{Invalid QDate}.
*/
char *toString(const QDate &value);
/*!
    \overload

    Returns \a value as \c{hh:mm:ss.zzz}, or \c{Invalid QTime}.
*/
char *toString(const QTime &value);
/*!
    \overload

    Returns \a value in ISO 8601 form, or \c{Invalid QDateTime}.
*/
char *toString(const QDateTime &value);
/*!
    \overload

    Returns the encoded form of \a value in double quotes.
*/
char *toString(const QUrl &value);
/*!
    \overload

    Returns \a value in the form \c{{00000000-0000-0000-0000-000000000000}}.
*/
char *toString(const QUuid &value);
/*!
    \overload

    Returns \a value with its segments separated by dots.
*/
char *toString(const QVersionNumber &value);

/*!
    Returns a text form of \a value for a failure message, as an array
    allocated with \c{new char[]} that the caller deletes. Numbers, enums and
    pointers are covered here, and so are the Nt classes with an overload of
    this function. For anything else it returns \c nullptr and the failure
    message says that there is no text form.

    To cover your own type, specialize this template, or better, add an
    overload next to the type, in its namespace:

    \code
    char *toString(const Point &p)
    {
        return qstrdup(QByteArray::number(p.x) + "," + QByteArray::number(p.y));
    }
    \endcode
*/
template <typename T>
char *toString(const T &value) {
    if constexpr (std::is_enum_v<T>)
        return QTestPrivate::integerToString(static_cast<long long>(value));
    else if constexpr (std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>)
        return QTestPrivate::integerToString(static_cast<long long>(value));
    else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
        return QTestPrivate::integerToString(static_cast<long long>(value));
    else if constexpr (std::is_integral_v<T>)
        return QTestPrivate::unsignedToString(static_cast<unsigned long long>(value));
    else if constexpr (std::is_floating_point_v<T>)
        return QTestPrivate::realToString(value);
    else if constexpr (std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>)
        return QTestPrivate::pointerToString(value);
    else
        return nullptr;
}

} // namespace QTest

namespace QTestPrivate {

template <typename T>
std::string itemToString(const T &value) {
    using QTest::toString;
    std::unique_ptr<char[]> text(toString(value));
    return text ? std::string(text.get()) : std::string("?");
}

template <typename Tuple, std::size_t... I>
std::vector<std::string> tupleItems(const Tuple &value, std::index_sequence<I...>) {
    return {itemToString(std::get<I>(value))...};
}

} // namespace QTestPrivate

namespace QTest {

/*!
    Returns a text form of \a value, with each item converted by toString().
*/
template <typename T>
char *toString(const QList<T> &value) {
    std::vector<std::string> items;
    for (const T &item : value)
        items.push_back(QTestPrivate::itemToString(item));
    return QTestPrivate::sequenceToString("QList(", items, ")");
}

/*!
    Returns a text form of \a value, with both members converted by toString().
*/
template <typename T1, typename T2>
char *toString(const std::pair<T1, T2> &value) {
    return QTestPrivate::sequenceToString(
        "std::pair(", {QTestPrivate::itemToString(value.first), QTestPrivate::itemToString(value.second)}, ")");
}

/*!
    Returns a text form of \a value, with each member converted by toString().
*/
template <typename... Types>
char *toString(const std::tuple<Types...> &value) {
    return QTestPrivate::sequenceToString(
        "std::tuple(", QTestPrivate::tupleItems(value, std::index_sequence_for<Types...>{}), ")");
}

/*!
    Reports \a statement, the result of the check \a statementText at \a line
    of \a file, for the current test function, with \a description added to a
    failure. Returns \c false if the test function must stop. QVERIFY() and
    QVERIFY2() call it.
*/
bool qVerify(bool statement, const char *statementText, const char *description, const char *file, int line);
/*!
    Reports a failure of the current test function with \a message, at
    \a line of \a file. QFAIL() calls it and then returns.
*/
void qFail(const char *message, const char *file, int line);
/*!
    Marks the current data row as skipped, because of \a message, at \a line
    of \a file. QSKIP() calls it and then returns.
*/
void qSkip(const char *message, const char *file, int line);
/*!
    Announces that the next check fails for the row \a dataTag, or for all
    rows if it is empty, because of \a comment. \a mode says what happens
    after it. Returns \c false if the test function must stop, which is when
    a failure is already expected at \a line of \a file. QEXPECT_FAIL() calls it.
*/
bool qExpectFail(const char *dataTag, const char *comment, TestFailMode mode, const char *file, int line);

/*!
    Compares \a t1 with \a t2 and reports the result for the current test
    function; \a actual and \a expected are their source text, and \a file
    and \a line the place of the comparison. Returns \c false if the test
    function must stop. QCOMPARE() calls it.

    C strings are compared by content, integers by value whatever their
    signedness, and floating point numbers with a tolerance, treating two
    NaNs as equal. Everything else uses \c{operator==}.
*/
template <typename T1, typename T2>
bool qCompare(const T1 &t1, const T2 &t2, const char *actual, const char *expected, const char *file,
              int line) {
    if (QTestPrivate::valuesEqual(t1, t2))
        return QTestPrivate::reportPass(file, line);
    std::unique_ptr<char[]> actualValue(toString(t1));
    std::unique_ptr<char[]> expectedValue(toString(t2));
    return QTestPrivate::reportFailure("Compared values are not the same", actualValue.get(),
                                       expectedValue.get(), actual, expected, "Actual", "Expected",
                                       file, line);
}

/*!
    \overload

    Compares the lists \a t1 and \a t2 item by item, so that the failure
    message names the first item that differs. \a actual, \a expected,
    \a file and \a line are as for the general function.
*/
template <typename T>
bool qCompare(const QList<T> &t1, const QList<T> &t2, const char *actual, const char *expected,
              const char *file, int line) {
    if (t1.size() != t2.size()) {
        const std::string summary = "Compared lists have different sizes: " +
                                    std::to_string(t1.size()) + " and " + std::to_string(t2.size());
        std::unique_ptr<char[]> actualValue(toString(t1));
        std::unique_ptr<char[]> expectedValue(toString(t2));
        return QTestPrivate::reportFailure(summary.c_str(), actualValue.get(), expectedValue.get(), actual,
                                           expected, "Actual", "Expected", file, line);
    }
    for (qsizetype i = 0; i < t1.size(); ++i) {
        if (QTestPrivate::valuesEqual(t1.at(i), t2.at(i)))
            continue;
        const std::string summary = "Compared lists differ at index " + std::to_string(i);
        std::unique_ptr<char[]> actualValue(toString(t1.at(i)));
        std::unique_ptr<char[]> expectedValue(toString(t2.at(i)));
        return QTestPrivate::reportFailure(summary.c_str(), actualValue.get(), expectedValue.get(), actual,
                                           expected, "Actual", "Expected", file, line);
    }
    return QTestPrivate::reportPass(file, line);
}

/*!
    Compares \a actual with the cell called \a elementName of the current
    data row and reports the result. \a file and \a line give the place of
    the comparison, \a actualText is the source text of \a actual and
    \a expected the text shown for the cell. Returns \c false if the test
    function must stop. QTEST() calls it.
*/
template <typename T>
bool qTest(const T &actual, const char *elementName, const char *actualText, const char *expected,
           const char *file, int line) {
    const void *cell = QTestPrivate::fetchData(elementName, typeid(T), false);
    return qCompare(actual, *static_cast<const T *>(cell), actualText, expected, file, line);
}

} // namespace QTest

namespace QTestPrivate {

template <ComparisonOp Op, typename T1, typename T2>
bool compareOp(const T1 &lhs, const T2 &rhs, const char *lhsText, const char *rhsText, const char *file,
               int line) {
    if (holds<Op>(lhs, rhs))
        return reportPass(file, line);
    using QTest::toString;
    std::unique_ptr<char[]> left(toString(lhs));
    std::unique_ptr<char[]> right(toString(rhs));
    return reportFailure(comparisonFailure(Op), left.get(), right.get(), lhsText, rhsText, "Computed",
                         "Baseline", file, line);
}

} // namespace QTestPrivate

#define QVERIFY(statement)                                                                         \
    do {                                                                                           \
        if (!QTest::qVerify(static_cast<bool>(statement), #statement, "", __FILE__, __LINE__))     \
            return;                                                                                \
    } while (false)

#define QVERIFY2(statement, description)                                                           \
    do {                                                                                           \
        if (!QTest::qVerify(static_cast<bool>(statement), #statement, (description), __FILE__,     \
                            __LINE__))                                                             \
            return;                                                                                \
    } while (false)

#define QCOMPARE(actual, expected)                                                                 \
    do {                                                                                           \
        if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))            \
            return;                                                                                \
    } while (false)

#define QTEST_COMPARE_OP_IMPL(lhs, rhs, op)                                                        \
    do {                                                                                           \
        if (!QTestPrivate::compareOp<QTestPrivate::ComparisonOp::op>(lhs, rhs, #lhs, #rhs,         \
                                                                     __FILE__, __LINE__))          \
            return;                                                                                \
    } while (false)

#define QCOMPARE_EQ(computed, baseline) QTEST_COMPARE_OP_IMPL(computed, baseline, Equal)
#define QCOMPARE_NE(computed, baseline) QTEST_COMPARE_OP_IMPL(computed, baseline, NotEqual)
#define QCOMPARE_LT(computed, baseline) QTEST_COMPARE_OP_IMPL(computed, baseline, Less)
#define QCOMPARE_LE(computed, baseline) QTEST_COMPARE_OP_IMPL(computed, baseline, LessEqual)
#define QCOMPARE_GT(computed, baseline) QTEST_COMPARE_OP_IMPL(computed, baseline, Greater)
#define QCOMPARE_GE(computed, baseline) QTEST_COMPARE_OP_IMPL(computed, baseline, GreaterEqual)

#define QFAIL(message)                                                                             \
    do {                                                                                           \
        QTest::qFail(message, __FILE__, __LINE__);                                                 \
        return;                                                                                    \
    } while (false)

#define QSKIP(message)                                                                             \
    do {                                                                                           \
        QTest::qSkip(message, __FILE__, __LINE__);                                                 \
        return;                                                                                    \
    } while (false)

#define QEXPECT_FAIL(dataTag, comment, mode)                                                       \
    do {                                                                                           \
        if (!QTest::qExpectFail(dataTag, comment, QTest::mode, __FILE__, __LINE__))                \
            return;                                                                                \
    } while (false)

#define QFETCH(Type, name)                                                                         \
    Type name = *static_cast<const std::remove_cv_t<Type> *>(                                      \
        QTestPrivate::fetchData(#name, typeid(Type), false))

#define QFETCH_GLOBAL(Type, name)                                                                  \
    Type name = *static_cast<const std::remove_cv_t<Type> *>(                                      \
        QTestPrivate::fetchData(#name, typeid(Type), true))

#define QTEST(actual, testElement)                                                                 \
    do {                                                                                           \
        if (!QTest::qTest(actual, testElement, #actual, testElement, __FILE__, __LINE__))          \
            return;                                                                                \
    } while (false)

#define QTRY_VERIFY2_WITH_TIMEOUT(expression, description, timeout)                                \
    do {                                                                                           \
        (void)QTest::qWaitFor([&] { return static_cast<bool>(expression); }, timeout);             \
        QVERIFY2(expression, description);                                                         \
    } while (false)
#define QTRY_VERIFY_WITH_TIMEOUT(expression, timeout)                                              \
    QTRY_VERIFY2_WITH_TIMEOUT(expression, "", timeout)
#define QTRY_VERIFY2(expression, description)                                                      \
    QTRY_VERIFY2_WITH_TIMEOUT(expression, description, 5000)
#define QTRY_VERIFY(expression) QTRY_VERIFY2_WITH_TIMEOUT(expression, "", 5000)

#define QTRY_COMPARE_WITH_TIMEOUT(actual, expected, timeout)                                       \
    do {                                                                                           \
        (void)QTest::qWaitFor([&] { return QTestPrivate::valuesEqual(actual, expected); },         \
                              timeout);                                                            \
        QCOMPARE(actual, expected);                                                                \
    } while (false)
#define QTRY_COMPARE(actual, expected) QTRY_COMPARE_WITH_TIMEOUT(actual, expected, 5000)

#define QVERIFY_THROWS_EXCEPTION(exceptionType, ...)                                               \
    do {                                                                                           \
        const int qtestThrown = [&]() -> int {                                                     \
            try {                                                                                  \
                __VA_ARGS__;                                                                       \
            } catch (const exceptionType &) {                                                      \
                return 1;                                                                          \
            } catch (...) {                                                                        \
                return 2;                                                                          \
            }                                                                                      \
            return 0;                                                                              \
        }();                                                                                       \
        if (qtestThrown == 0)                                                                      \
            QFAIL("Expected exception of type " #exceptionType " to be thrown, but none was");     \
        if (qtestThrown == 2)                                                                      \
            QFAIL("Expected exception of type " #exceptionType ", but caught another type");       \
    } while (false)

#define QVERIFY_THROWS_NO_EXCEPTION(...)                                                           \
    do {                                                                                           \
        const bool qtestThrown = [&]() -> bool {                                                   \
            try {                                                                                  \
                __VA_ARGS__;                                                                       \
            } catch (...) {                                                                        \
                return true;                                                                       \
            }                                                                                      \
            return false;                                                                          \
        }();                                                                                       \
        if (qtestThrown)                                                                           \
            QFAIL("Expected no exception to be thrown, but caught one");                           \
    } while (false)

#define QFINDTESTDATA(basepath) QTest::qFindTestData(basepath, __FILE__, __LINE__)

#define QTEST_APPLESS_MAIN(TestObject)                                                             \
    int main(int argc, char *argv[]) {                                                             \
        TestObject testObject;                                                                     \
        return QTest::qExec(&testObject, argc, argv);                                              \
    }

#define QTEST_GUILESS_MAIN(TestObject)                                                             \
    int main(int argc, char *argv[]) {                                                             \
        QCoreApplication app(argc, argv);                                                          \
        TestObject testObject;                                                                     \
        return QTest::qExec(&testObject, argc, argv);                                              \
    }

// Nt has no GUI module yet, so this is the same as QTEST_GUILESS_MAIN.
#define QTEST_MAIN(TestObject) QTEST_GUILESS_MAIN(TestObject)
